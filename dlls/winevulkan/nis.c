/*
 * Copyright 2022 NVIDIA CORPORATION & AFFILIATES. All rights reserved. 
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#if 0
#pragma makedep unix
#endif

#include <math.h>
#include <time.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>

#include "vulkan_private.h"

#include "nis.h"
#include "nis_fp32_spv.h"
#include "nis_fp16_spv.h"

WINE_DEFAULT_DEBUG_CHANNEL(vulkan);

struct nis_hack_data
{
    VkDescriptorSet descriptor_set;
};

struct nis_data
{
    VkBuffer uniform_buffer;
    VkImage scaler_coef_img;
    VkImageView scaler_coef_img_view;
    VkImage usm_coef_img;
    VkImageView usm_coef_img_view;
    VkDeviceMemory memory;
    VkSampler sampler;
    VkDescriptorPool descriptor_pool;
    VkDescriptorSetLayout descriptor_set_layout;
    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline;
    uint32_t curr_queue_index;
    VkCommandPool pool;
    BOOL use_fp16;
    float sharpness;
};

static void nis_update_config(struct nis_config* config, float sharpness,
    uint32_t inputViewportOriginX, uint32_t inputViewportOriginY,
    uint32_t inputViewportWidth, uint32_t inputViewportHeight,
    uint32_t inputTextureWidth, uint32_t inputTextureHeight,
    uint32_t outputViewportOriginX, uint32_t outputViewportOriginY,
    uint32_t outputViewportWidth, uint32_t outputViewportHeight,
    uint32_t outputTextureWidth, uint32_t outputTextureHeight)
{
    float sharpen_slider;
    float max_scale;
    float min_scale;
    float limit_scale;
    float kDetectRatio;
    float kDetectThres;
    float kMinContrastRatio;
    float kMaxContrastRatio;
    float kSharpStartY;
    float kSharpEndY;
    float kSharpStrengthMin;
    float kSharpStrengthMax;
    float kSharpLimitMin;
    float kSharpLimitMax;
    float kRatioNorm;
    float kSharpScaleY;
    float kSharpStrengthScale;
    float kSharpLimitScale;

    // adjust params based on value from sharpness slider
    sharpness = fmaxf(fminf(1.f, sharpness), 0.f);
    sharpen_slider = sharpness - 0.5f;   // Map 0 to 1 to -0.5 to +0.5

    // Different range for 0 to 50% vs 50% to 100%
    // The idea is to make sure sharpness of 0% map to no-sharpening,
    // while also ensuring that sharpness of 100% doesn't cause too much over-sharpening.
    max_scale = (sharpen_slider >= 0.0f) ? 1.25f : 1.75f;
    min_scale = (sharpen_slider >= 0.0f) ? 1.25f : 1.0f;
    limit_scale = (sharpen_slider >= 0.0f) ? 1.25f : 1.0f;

    kDetectRatio = 2 * 1127.f / 1024.f;

    // Params for SDR
    kDetectThres = 64.0f / 1024.0f;
    kMinContrastRatio = 2.0f;
    kMaxContrastRatio = 10.0f;

    kSharpStartY = 0.45f;
    kSharpEndY = 0.9f;
    kSharpStrengthMin = fmaxf(0.0f, 0.4f + sharpen_slider * min_scale * 1.2f);
    kSharpStrengthMax = 1.6f + sharpen_slider * max_scale * 1.8f;
    kSharpLimitMin = fmaxf(0.1f, 0.14f + sharpen_slider * limit_scale * 0.32f);
    kSharpLimitMax = 0.5f + sharpen_slider * limit_scale * 0.6f;

    kRatioNorm = 1.0f / (kMaxContrastRatio - kMinContrastRatio);
    kSharpScaleY = 1.0f / (kSharpEndY - kSharpStartY);
    kSharpStrengthScale = kSharpStrengthMax - kSharpStrengthMin;
    kSharpLimitScale = kSharpLimitMax - kSharpLimitMin;

    config->kInputViewportWidth = inputViewportWidth == 0 ? inputTextureWidth : inputViewportWidth;
    config->kInputViewportHeight = inputViewportHeight == 0 ? inputTextureHeight : inputViewportHeight;
    config->kOutputViewportWidth = outputViewportWidth == 0 ? outputTextureWidth : outputViewportWidth;
    config->kOutputViewportHeight = outputViewportHeight == 0 ? outputTextureHeight : outputViewportHeight;

    config->kInputViewportOriginX = inputViewportOriginX;
    config->kInputViewportOriginY = inputViewportOriginY;
    config->kOutputViewportOriginX = outputViewportOriginX;
    config->kOutputViewportOriginY = outputViewportOriginY;

    config->kSrcNormX = 1.f / inputTextureWidth;
    config->kSrcNormY = 1.f / inputTextureHeight;
    config->kDstNormX = 1.f / outputTextureWidth;
    config->kDstNormY = 1.f / outputTextureHeight;
    config->kScaleX = config->kInputViewportWidth / (float)(config->kOutputViewportWidth);
    config->kScaleY = config->kInputViewportHeight / (float)(config->kOutputViewportHeight);
    config->kDetectRatio = kDetectRatio;
    config->kDetectThres = kDetectThres;
    config->kMinContrastRatio = kMinContrastRatio;
    config->kRatioNorm = kRatioNorm;
    config->kContrastBoost = 1.0f;
    config->kEps = 1.0f / 255.0f;
    config->kSharpStartY = kSharpStartY;
    config->kSharpScaleY = kSharpScaleY;
    config->kSharpStrengthMin = kSharpStrengthMin;
    config->kSharpStrengthScale = kSharpStrengthScale;
    config->kSharpLimitMin = kSharpLimitMin;
    config->kSharpLimitScale = kSharpLimitScale;
}

static VkResult upload_nis_data(VkDevice device, struct VkSwapchainKHR_T *swapchain)
{
    VkResult res = VK_SUCCESS;
    struct nis_data *nis_data = swapchain->upscaler_data;
    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkMemoryPropertyFlags stagingMemFlags;
    VkDeviceMemory stagingMem = VK_NULL_HANDLE;
    VkCommandPool pool;
    VkCommandBuffer cmd;
    VkCommandPoolCreateInfo poolInfo = {0};
    VkSubmitInfo submitInfo = {0};
    VkQueue queue = VK_NULL_HANDLE;
    struct nis_config config = {0};
    VkDeviceSize uniform_data_offset;
    VkDeviceSize scaler_data_offset;
    VkDeviceSize usm_data_offset;
    uint32_t scaler_data_size;
    uint32_t usm_data_size;
    uint32_t memory_type = -1;
    uint32_t queue_index;
    void* staging_data;
    char* uniform_data;
    char* scaler_data;
    char* usm_data;
#if defined(USE_STRUCT_CONVERSION)
    VkPhysicalDeviceMemoryProperties_host memProperties;
    VkMemoryRequirements_host memReqs = {0};
    VkMemoryAllocateInfo_host allocInfo = {0};
    VkBufferCreateInfo_host bufferInfo = {0};
    VkCommandBufferAllocateInfo_host cmdBufferAllocInfo = {0};
    VkCommandBufferBeginInfo_host beginInfo = {0};
    VkImageMemoryBarrier_host barriers[2] = {{0}};
    VkBufferCopy_host bufferCopy = {0};
    VkBufferImageCopy_host imageCopy = {0};
#else
    VkPhysicalDeviceMemoryProperties memProperties;
    VkMemoryRequirements memReqs = {0};
    VkMemoryAllocateInfo allocInfo = {0};
    VkBufferCreateInfo bufferInfo = {0};
    VkCommandBufferAllocateInfo cmdBufferAllocInfo = {0};
    VkCommandBufferBeginInfo beginInfo = {0};
    VkImageMemoryBarrier barriers[2] = {{0}};
    VkBufferCopy bufferCopy = {0};
    VkBufferImageCopy imageCopy = {0};
#endif

    device->phys_dev->instance->funcs.p_vkGetPhysicalDeviceMemoryProperties(device->phys_dev->phys_dev, &memProperties);

    /* Create the staging buffer for upload */
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = nis_data->use_fp16 ? sizeof(struct nis_config) + sizeof(coef_scale_fp16) + sizeof(coef_usm_fp16) :
                                            sizeof(struct nis_config) + sizeof(coef_scale) + sizeof(coef_usm);
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    res = device->funcs.p_vkCreateBuffer(device->device, &bufferInfo, NULL, &stagingBuffer);
    if(res != VK_SUCCESS)
    {
        ERR("vkCreateBuffer failed for staging, res=%d\n", res);
        return res;
    }

    /* Allocate the memory for staging */
    device->funcs.p_vkGetBufferMemoryRequirements(device->device, stagingBuffer, &memReqs);

    stagingMemFlags = (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    for(uint32_t i = 0; i < memProperties.memoryTypeCount; i++){
        if((memProperties.memoryTypes[i].propertyFlags & stagingMemFlags) == stagingMemFlags) {
            if(memReqs.memoryTypeBits & (1 << i)){
                memory_type = i;
                break;
            }
        }
    }

    if(memory_type == -1){
        ERR("unable to find suitable memory type for staging buffer\n");
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = memory_type;

    res = device->funcs.p_vkAllocateMemory(device->device, &allocInfo, NULL, &stagingMem);
    if (res != VK_SUCCESS)
    {
        ERR("vkAllocateMemory failed staging memory, res=%d\n", res);
        return res;
    }

    res = device->funcs.p_vkBindBufferMemory(device->device, stagingBuffer, stagingMem, 0);
    if (res != VK_SUCCESS)
    {
        ERR("vkBindBufferMemory failed staging buffer, res=%d\n", res);
        return res;
    }

    /* Map the memory and copy the data */
    staging_data = NULL;
    device->funcs.p_vkMapMemory(device->device, stagingMem, 0, VK_WHOLE_SIZE, 0, &staging_data);
    if (staging_data == NULL)
    {
        ERR("vkMapMemory failed staging buffer\n");
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    nis_update_config(
        &config,
        nis_data->sharpness,
        0, 0, // Input Viewport Origin
        swapchain->user_extent.width, swapchain->user_extent.height, // Input Viewport Extent
        swapchain->user_extent.width, swapchain->user_extent.height, // Input Texture Extent
        0, 0, // Output Viewport Origin
        swapchain->real_extent.width, swapchain->real_extent.height, // Output Viewport Extent
        swapchain->real_extent.width, swapchain->real_extent.height // Output Texture Extent
    );

    scaler_data_size = nis_data->use_fp16 ? sizeof(coef_scale_fp16) : sizeof(coef_scale);
    usm_data_size = nis_data->use_fp16 ? sizeof(coef_usm_fp16) : sizeof(coef_usm);

    uniform_data_offset = 0;
    scaler_data_offset = sizeof(struct nis_config);
    usm_data_offset = scaler_data_offset + scaler_data_size;

    uniform_data = staging_data;
    scaler_data = uniform_data + sizeof(struct nis_config);
    usm_data = scaler_data + scaler_data_size;

    memcpy(uniform_data, &config, sizeof(struct nis_config));
    memcpy(scaler_data, nis_data->use_fp16 ? (void*)coef_scale_fp16 : (void*)coef_scale, scaler_data_size);
    memcpy(usm_data, nis_data->use_fp16 ? (void*)coef_usm_fp16 : (void*)coef_usm, usm_data_size);

    device->funcs.p_vkUnmapMemory(device->device, stagingMem);

    /* Find a queue to submit the copy on. Anything that supports transfer operations should work just fine. */
    queue_index = -1;
    for (uint32_t i = 0; i < device->queue_count; i++){
        if (device->queue_props[i].queueFlags & VK_QUEUE_TRANSFER_BIT){
            queue_index = i;
            break;
        }
    }

    if (queue_index == -1)
    {
        ERR("Failed to find queue for uploading NIS data");
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = queue_index;

    res = device->funcs.p_vkCreateCommandPool(device->device, &poolInfo, NULL, &pool);
    if(res != VK_SUCCESS){
        ERR("vkCreateCommandPool failed, res=%d\n", res);
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    /* Keep the command pool and queue index in case the resources need to transfer to another queue */
    nis_data->curr_queue_index = queue_index;
    nis_data->pool = pool;

    cmdBufferAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdBufferAllocInfo.commandPool = pool;
    cmdBufferAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdBufferAllocInfo.commandBufferCount = 1;

    res = device->funcs.p_vkAllocateCommandBuffers(device->device, &cmdBufferAllocInfo, &cmd);
    if(res != VK_SUCCESS){
        ERR("vkAllocateCommandBuffers failed, res=%d\n", res);
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    /* Record and submit the command buffer to upload the data */
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

    device->funcs.p_vkBeginCommandBuffer(cmd, &beginInfo);

    bufferCopy.size = sizeof(struct nis_config);
    bufferCopy.srcOffset = uniform_data_offset;
    bufferCopy.dstOffset = 0;

    device->funcs.p_vkCmdCopyBuffer(cmd, stagingBuffer, nis_data->uniform_buffer, 1, &bufferCopy);

    barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barriers[0].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barriers[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[0].image = nis_data->scaler_coef_img;
    barriers[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barriers[0].subresourceRange.baseMipLevel = 0;
    barriers[0].subresourceRange.levelCount = 1;
    barriers[0].subresourceRange.baseArrayLayer = 0;
    barriers[0].subresourceRange.layerCount = 1;
    barriers[0].srcAccessMask = 0;
    barriers[0].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    barriers[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barriers[1].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barriers[1].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[1].image = nis_data->usm_coef_img;
    barriers[1].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barriers[1].subresourceRange.baseMipLevel = 0;
    barriers[1].subresourceRange.levelCount = 1;
    barriers[1].subresourceRange.baseArrayLayer = 0;
    barriers[1].subresourceRange.layerCount = 1;
    barriers[1].srcAccessMask = 0;
    barriers[1].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    device->funcs.p_vkCmdPipelineBarrier(
        cmd,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0,
        0, NULL,
        0, NULL,
        2, barriers
    );

    imageCopy.bufferOffset = scaler_data_offset;
    imageCopy.imageExtent.width = kFilterSize / 4;
    imageCopy.imageExtent.height = kPhaseCount;
    imageCopy.imageExtent.depth = 1;
    imageCopy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageCopy.imageSubresource.mipLevel = 0;
    imageCopy.imageSubresource.baseArrayLayer = 0;
    imageCopy.imageSubresource.layerCount = 1;

    device->funcs.p_vkCmdCopyBufferToImage(cmd, stagingBuffer, nis_data->scaler_coef_img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageCopy);

    imageCopy.bufferOffset = usm_data_offset;

    device->funcs.p_vkCmdCopyBufferToImage(cmd, stagingBuffer, nis_data->usm_coef_img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageCopy);

    barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barriers[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barriers[0].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[0].image = nis_data->scaler_coef_img;
    barriers[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barriers[0].subresourceRange.baseMipLevel = 0;
    barriers[0].subresourceRange.levelCount = 1;
    barriers[0].subresourceRange.baseArrayLayer = 0;
    barriers[0].subresourceRange.layerCount = 1;
    barriers[0].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barriers[0].dstAccessMask = 0;

    barriers[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barriers[1].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barriers[1].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[1].image = nis_data->usm_coef_img;
    barriers[1].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barriers[1].subresourceRange.baseMipLevel = 0;
    barriers[1].subresourceRange.levelCount = 1;
    barriers[1].subresourceRange.baseArrayLayer = 0;
    barriers[1].subresourceRange.layerCount = 1;
    barriers[1].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barriers[1].dstAccessMask = 0;

    device->funcs.p_vkCmdPipelineBarrier(
        cmd,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        0,
        0, NULL,
        0, NULL,
        2, barriers
    );

    device->funcs.p_vkEndCommandBuffer(cmd);

    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;

    queue = device->queues[queue_index].queue;

    res = device->funcs.p_vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    if(res != VK_SUCCESS){
        ERR("vkQueueSubmit: %d\n", res);
        return res;
    }

    res = device->funcs.p_vkQueueWaitIdle(queue);
    if(res != VK_SUCCESS){
        ERR("vkQueueWaitIdle: %d\n", res);
        return res;
    }

    /* Once the transfer is done clean up */
    device->funcs.p_vkFreeCommandBuffers(device->device, pool, 1, &cmd);
    device->funcs.p_vkDestroyBuffer(device->device, stagingBuffer, NULL);
    device->funcs.p_vkFreeMemory(device->device, stagingMem, NULL);

    return VK_SUCCESS;
}

static VkResult create_descriptor_set(VkDevice device, struct VkSwapchainKHR_T *swapchain, struct fs_hack_image *hack)
{
    VkResult res;
    struct nis_data *nis_data;
#if defined(USE_STRUCT_CONVERSION)
    VkDescriptorSetAllocateInfo_host descriptorAllocInfo = {0};
    VkWriteDescriptorSet_host descriptorWrites[6] = {{0}, {0}, {0}, {0}, {0}, {0}};
    VkDescriptorBufferInfo_host uniformBufferDescriptorInfo = {0};
    VkDescriptorImageInfo_host samplerDescriptorImageInfo = {0};
    VkDescriptorImageInfo_host userDescriptorImageInfo = {0}; 
    VkDescriptorImageInfo_host realDescriptorImageInfo = {0};
    VkDescriptorImageInfo_host scalerCoefDescriptorImageInfo = {0};
    VkDescriptorImageInfo_host usmCoefDescriptorImageInfo = {0};
#else
    VkDescriptorSetAllocateInfo descriptorAllocInfo = {0};
    VkWriteDescriptorSet descriptorWrites[6] = {{0}, {0}, {0}, {0}, {0}, {0}};
    VkDescriptorBufferInfo uniformBufferDescriptorInfo = {0};
    VkDescriptorImageInfo samplerDescriptorImageInfo = {0};
    VkDescriptorImageInfo userDescriptorImageInfo = {0};
    VkDescriptorImageInfo realDescriptorImageInfo = {0};
    VkDescriptorImageInfo scalerCoefDescriptorImageInfo = {0};
    VkDescriptorImageInfo usmCoefDescriptorImageInfo = {0};
#endif

    struct nis_hack_data* nis_hack = calloc(1, sizeof(struct nis_hack_data));
    if (nis_hack == NULL){
        ERR("Failed to allocate nis hack data");
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }
    hack->upscaler_data = nis_hack;

    nis_data = swapchain->upscaler_data;

    descriptorAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptorAllocInfo.descriptorPool = nis_data->descriptor_pool;
    descriptorAllocInfo.descriptorSetCount = 1;
    descriptorAllocInfo.pSetLayouts = &nis_data->descriptor_set_layout;

    res = device->funcs.p_vkAllocateDescriptorSets(device->device, &descriptorAllocInfo, &nis_hack->descriptor_set);
    if(res != VK_SUCCESS){
        ERR("vkAllocateDescriptorSets: %d\n", res);
        return res;
    }

    uniformBufferDescriptorInfo.buffer = nis_data->uniform_buffer;
    uniformBufferDescriptorInfo.offset = 0;
    uniformBufferDescriptorInfo.range = VK_WHOLE_SIZE;

    samplerDescriptorImageInfo.sampler = nis_data->sampler;

    userDescriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    userDescriptorImageInfo.imageView = hack->user_view;

    realDescriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    realDescriptorImageInfo.imageView = hack->blit_view;

    scalerCoefDescriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    scalerCoefDescriptorImageInfo.imageView = nis_data->scaler_coef_img_view;

    usmCoefDescriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    usmCoefDescriptorImageInfo.imageView = nis_data->usm_coef_img_view;

    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = nis_hack->descriptor_set;
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pBufferInfo = &uniformBufferDescriptorInfo;
    
    descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[1].dstSet = nis_hack->descriptor_set;
    descriptorWrites[1].dstBinding = 1;
    descriptorWrites[1].dstArrayElement = 0;
    descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].pImageInfo = &samplerDescriptorImageInfo;

    descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[2].dstSet = nis_hack->descriptor_set;
    descriptorWrites[2].dstBinding = 2;
    descriptorWrites[2].dstArrayElement = 0;
    descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    descriptorWrites[2].descriptorCount = 1;
    descriptorWrites[2].pImageInfo = &userDescriptorImageInfo;

    descriptorWrites[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[3].dstSet = nis_hack->descriptor_set;
    descriptorWrites[3].dstBinding = 3;
    descriptorWrites[3].dstArrayElement = 0;
    descriptorWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    descriptorWrites[3].descriptorCount = 1;
    descriptorWrites[3].pImageInfo = &realDescriptorImageInfo;

    descriptorWrites[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[4].dstSet = nis_hack->descriptor_set;
    descriptorWrites[4].dstBinding = 4;
    descriptorWrites[4].dstArrayElement = 0;
    descriptorWrites[4].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    descriptorWrites[4].descriptorCount = 1;
    descriptorWrites[4].pImageInfo = &scalerCoefDescriptorImageInfo;

    descriptorWrites[5].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[5].dstSet = nis_hack->descriptor_set;
    descriptorWrites[5].dstBinding = 5;
    descriptorWrites[5].dstArrayElement = 0;
    descriptorWrites[5].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    descriptorWrites[5].descriptorCount = 1;
    descriptorWrites[5].pImageInfo = &usmCoefDescriptorImageInfo;

    device->funcs.p_vkUpdateDescriptorSets(device->device, 6, descriptorWrites, 0, NULL);

    return VK_SUCCESS;
}

static VkResult alloc_resource_memory(VkDevice device, struct VkSwapchainKHR_T *swapchain)
{
    VkResult res;
    uint32_t memory_type = -1;
    uint32_t supported_memory_types;
    VkDeviceSize memory_size, scaler_offset, usm_offset;
    struct nis_data *nis_data = swapchain->upscaler_data;
#if defined(USE_STRUCT_CONVERSION)
    VkPhysicalDeviceMemoryProperties_host memProperties;
    VkMemoryRequirements_host memReqs = {0};
    VkMemoryAllocateInfo_host allocInfo = {0};
#else
    VkPhysicalDeviceMemoryProperties memProperties;
    VkMemoryRequirements memReqs = {0};
    VkMemoryAllocateInfo allocInfo = {0};
#endif

    device->phys_dev->instance->funcs.p_vkGetPhysicalDeviceMemoryProperties(device->phys_dev->phys_dev, &memProperties);

    device->funcs.p_vkGetBufferMemoryRequirements(device->device, nis_data->uniform_buffer, &memReqs);
    supported_memory_types = memReqs.memoryTypeBits;
    memory_size = memReqs.size;

    device->funcs.p_vkGetImageMemoryRequirements(device->device, nis_data->scaler_coef_img, &memReqs);
    supported_memory_types &= memReqs.memoryTypeBits;
    scaler_offset = memory_size + (memReqs.alignment - (memory_size % memReqs.alignment));
    memory_size += memReqs.size + scaler_offset;

    device->funcs.p_vkGetImageMemoryRequirements(device->device, nis_data->usm_coef_img, &memReqs);
    supported_memory_types &= memReqs.memoryTypeBits;
    usm_offset = memory_size + (memReqs.alignment - (memory_size % memReqs.alignment));
    memory_size += memReqs.size + usm_offset;

    for(uint32_t i = 0; i < memProperties.memoryTypeCount; i++){
        if((memProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) == VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT){
            if(supported_memory_types & (1 << i)){
                memory_type = i;
                break;
            }
        }
    }

    if(memory_type == -1){
        ERR("Unable to find suitable memory type for uniform buffer\n");
        res = VK_ERROR_OUT_OF_DEVICE_MEMORY;
        return res;
    }

    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memory_size;
    allocInfo.memoryTypeIndex = memory_type;

    res = device->funcs.p_vkAllocateMemory(device->device, &allocInfo, NULL, &nis_data->memory);
    if (res != VK_SUCCESS){
        ERR("vkAllocateMemory failed NIS data, res=%d\n", res);
        return res;
    }

    res = device->funcs.p_vkBindBufferMemory(device->device, nis_data->uniform_buffer, nis_data->memory, 0);
    if (res != VK_SUCCESS){
        ERR("vkBindBufferMemory failed for uniform buffer, res=%d\n", res);
        return res;
    }

    res = device->funcs.p_vkBindImageMemory(device->device, nis_data->scaler_coef_img, nis_data->memory, scaler_offset);
    if (res != VK_SUCCESS){
        ERR("vkBindImageMemory failed scaler image, res=%d\n", res);
        return res;
    }

    res = device->funcs.p_vkBindImageMemory(device->device, nis_data->usm_coef_img, nis_data->memory, usm_offset);
    if (res != VK_SUCCESS){
        ERR("vkBindImageMemory failed usm image, res=%d\n", res);
        return res;
    }

    return VK_SUCCESS;
}

static VkResult init(VkDevice device, struct VkSwapchainKHR_T *swapchain)
{
    VkResult res;
    VkFormatProperties formatProperties = {0};
    VkPhysicalDeviceFeatures2 physDeviceFeatures = {0};
    VkPhysicalDeviceFloat16Int8FeaturesKHR fp16DeviceFeatures = {0};
    VkSamplerCreateInfo samplerInfo = {0};
    VkDescriptorPoolSize poolSizes[4] = {{0}, {0}, {0}, {0}};
    VkDescriptorPoolCreateInfo poolInfo = {0};
    VkDescriptorSetLayoutBinding layoutBindings[6] = {{0}, {0}, {0}, {0}, {0}, {0}};
    VkDescriptorSetLayoutCreateInfo descriptorLayoutInfo = {0};
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {0};
    VkShaderModuleCreateInfo shaderInfo = {0};
    VkShaderModule shaderModule = 0;
    VkImageCreateInfo imageInfo = {0};
    struct nis_data *nis_data;
    const char *force_fp32_env;
    BOOL force_fp32;
    const char* sharpness_env;
    char* sharpness_env_end;
    long sharpness;
    uint32_t i;
#if defined(USE_STRUCT_CONVERSION)
    VkImageViewCreateInfo_host viewInfo = {0};
    VkBufferCreateInfo_host bufferInfo = {0};
    VkComputePipelineCreateInfo_host pipelineInfo = {0};
#else
    VkImageViewCreateInfo viewInfo = {0};
    VkBufferCreateInfo bufferInfo = {0};
    VkComputePipelineCreateInfo pipelineInfo = {0};
#endif

    nis_data = calloc(1, sizeof(struct nis_data));
    if (!nis_data){
        WARN("calloc failed\n");
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    swapchain->upscaler_data = nis_data;

    /* Get the NIS upscaler options */
    force_fp32_env = getenv("WINE_NIS_UPSCALER_FORCE_FP32");
    force_fp32 = force_fp32_env && strcmp(force_fp32_env, "0");

    if (!force_fp32){
        fp16DeviceFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES;

        physDeviceFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        physDeviceFeatures.pNext = &fp16DeviceFeatures;
        device->phys_dev->instance->funcs.p_vkGetPhysicalDeviceFeatures2(device->phys_dev->phys_dev, &physDeviceFeatures);

        device->phys_dev->instance->funcs.p_vkGetPhysicalDeviceFormatProperties(device->phys_dev->phys_dev, VK_FORMAT_R16G16B16A16_SFLOAT, &formatProperties);

        nis_data->use_fp16 = (formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) && (fp16DeviceFeatures.shaderFloat16);
    }

    sharpness_env = getenv("WINE_NIS_UPSCALER_SHARPNESS");
    nis_data->sharpness = 0.33f;

    if (sharpness_env != NULL){
        sharpness = strtol(sharpness_env, &sharpness_env_end, 10);
        if (*sharpness_env_end == '\0'){
            nis_data->sharpness = min(max(0, sharpness), 100) / 100.0f;
        }else{
            WARN("WINE_NIS_UPSCALER_SHARPNESS is not a number defaulting to 33\n");
        }
    }

    /* Create the NIS resources */
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.minLod = -1000;
    samplerInfo.maxLod = 1000;
    samplerInfo.maxAnisotropy = 1.0f;

    res = device->funcs.p_vkCreateSampler(device->device, &samplerInfo, NULL, &nis_data->sampler);
    if(res != VK_SUCCESS){
        ERR("vkCreateSampler failed, res=%d\n", res);
        return res;
    }

    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = sizeof(struct nis_config);
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    res = device->funcs.p_vkCreateBuffer(device->device, &bufferInfo, NULL, &nis_data->uniform_buffer);
    if(res != VK_SUCCESS){
        ERR("vkCreateBuffer failed, res=%d\n", res);
        return res;
    }

    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = nis_data->use_fp16 ? VK_FORMAT_R16G16B16A16_SFLOAT : VK_FORMAT_R32G32B32A32_SFLOAT;
    imageInfo.extent.width = kFilterSize / 4;
    imageInfo.extent.height = kPhaseCount;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    res = device->funcs.p_vkCreateImage(device->device, &imageInfo, NULL, &nis_data->scaler_coef_img);
    if(res != VK_SUCCESS){
        ERR("vkCreateImage for scaler coef image failed, res=%d\n", res);
        return res;
    }

    res = device->funcs.p_vkCreateImage(device->device, &imageInfo, NULL, &nis_data->usm_coef_img);
    if(res != VK_SUCCESS){
        ERR("vkCreateImage for usm coef image failed, res=%d\n", res);
        return res;
    }

    alloc_resource_memory(device, swapchain);

    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = nis_data->scaler_coef_img;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = nis_data->use_fp16 ? VK_FORMAT_R16G16B16A16_SFLOAT : VK_FORMAT_R32G32B32A32_SFLOAT;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    res = device->funcs.p_vkCreateImageView(device->device, &viewInfo, NULL, &nis_data->scaler_coef_img_view);
    if(res != VK_SUCCESS){
        ERR("vkCreateImageView scaler coef: %d\n", res);
        goto fail;
    }

    viewInfo.image = nis_data->usm_coef_img;

    res = device->funcs.p_vkCreateImageView(device->device, &viewInfo, NULL, &nis_data->usm_coef_img_view);
    if(res != VK_SUCCESS){
        ERR("vkCreateImageView usm coef: %d\n", res);
        goto fail;
    }

    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = swapchain->n_images;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_SAMPLER;
    poolSizes[1].descriptorCount = swapchain->n_images;
    poolSizes[2].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    poolSizes[2].descriptorCount = swapchain->n_images * 3;
    poolSizes[3].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    poolSizes[3].descriptorCount = swapchain->n_images;

    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 4;
    poolInfo.pPoolSizes = poolSizes;
    poolInfo.maxSets = swapchain->n_images;

    res = device->funcs.p_vkCreateDescriptorPool(device->device, &poolInfo, NULL, &nis_data->descriptor_pool);
    if(res != VK_SUCCESS){
        ERR("vkCreateDescriptorPool: %d\n", res);
        goto fail;
    }

    layoutBindings[0].binding = 0;
    layoutBindings[0].descriptorCount = 1;
    layoutBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    layoutBindings[0].pImmutableSamplers = NULL;
    layoutBindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    layoutBindings[1].binding = 1;
    layoutBindings[1].descriptorCount = 1;
    layoutBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    layoutBindings[1].pImmutableSamplers = NULL;
    layoutBindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    layoutBindings[2].binding = 2;
    layoutBindings[2].descriptorCount = 1;
    layoutBindings[2].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    layoutBindings[2].pImmutableSamplers = NULL;
    layoutBindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    layoutBindings[3].binding = 3;
    layoutBindings[3].descriptorCount = 1;
    layoutBindings[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    layoutBindings[3].pImmutableSamplers = NULL;
    layoutBindings[3].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    layoutBindings[4].binding = 4;
    layoutBindings[4].descriptorCount = 1;
    layoutBindings[4].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    layoutBindings[4].pImmutableSamplers = NULL;
    layoutBindings[4].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    layoutBindings[5].binding = 5;
    layoutBindings[5].descriptorCount = 1;
    layoutBindings[5].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    layoutBindings[5].pImmutableSamplers = NULL;
    layoutBindings[5].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    descriptorLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorLayoutInfo.bindingCount = 6;
    descriptorLayoutInfo.pBindings = layoutBindings;

    res = device->funcs.p_vkCreateDescriptorSetLayout(device->device, &descriptorLayoutInfo, NULL, &nis_data->descriptor_set_layout);
    if(res != VK_SUCCESS){
        ERR("vkCreateDescriptorSetLayout: %d\n", res);
        goto fail;
    }

    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &nis_data->descriptor_set_layout;

    res = device->funcs.p_vkCreatePipelineLayout(device->device, &pipelineLayoutInfo, NULL, &nis_data->pipeline_layout);
    if(res != VK_SUCCESS){
        ERR("vkCreatePipelineLayout: %d\n", res);
        goto fail;
    }

    shaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderInfo.codeSize = nis_data->use_fp16 ? sizeof(nis_fp16_spv) : sizeof(nis_fp32_spv);
    shaderInfo.pCode = nis_data->use_fp16 ? nis_fp16_spv : nis_fp32_spv;

    res = device->funcs.p_vkCreateShaderModule(device->device, &shaderInfo, NULL, &shaderModule);
    if(res != VK_SUCCESS){
        ERR("vkCreateShaderModule: %d\n", res);
        goto fail;
    }

    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineInfo.stage.module = shaderModule;
    pipelineInfo.stage.pName = "main";
    pipelineInfo.layout = nis_data->pipeline_layout;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineInfo.basePipelineIndex = -1;

    res = device->funcs.p_vkCreateComputePipelines(device->device, VK_NULL_HANDLE, 1, &pipelineInfo, NULL, &nis_data->pipeline);
    if(res != VK_SUCCESS){
        ERR("vkCreateComputePipelines: %d\n", res);
        goto fail;
    }

    device->funcs.p_vkDestroyShaderModule(device->device, shaderModule, NULL);

    for(i = 0; i < swapchain->n_images; ++i){
        struct fs_hack_image *hack = &swapchain->fs_hack_images[i];
        res = create_descriptor_set(device, swapchain, hack);
        if(res != VK_SUCCESS)
            goto fail;
    }

    res = upload_nis_data(device, swapchain);
    if(res != VK_SUCCESS){
        ERR("upload_nis_data failed: %d\n", res);
        goto fail;
    }

    return VK_SUCCESS;

fail:
    for(i = 0; i < swapchain->n_images; ++i){
        struct fs_hack_image *hack = &swapchain->fs_hack_images[i];
        struct nis_hack_data* nis_hack = (struct nis_hack_data*)hack->upscaler_data;

        if (nis_hack != NULL){
            device->funcs.p_vkFreeDescriptorSets(device->device, nis_data->descriptor_pool, 1, &nis_hack->descriptor_set);
            hack->upscaler_data = NULL;
            free(nis_hack);
        }
    }
    device->funcs.p_vkDestroyShaderModule(device->device, shaderModule, NULL);
    device->funcs.p_vkDestroyBuffer(device->device, nis_data->uniform_buffer, NULL);
    device->funcs.p_vkDestroyImageView(device->device, nis_data->scaler_coef_img_view, NULL);
    device->funcs.p_vkDestroyImage(device->device, nis_data->scaler_coef_img, NULL);
    device->funcs.p_vkDestroyImageView(device->device, nis_data->usm_coef_img_view, NULL);
    device->funcs.p_vkDestroyImage(device->device, nis_data->usm_coef_img, NULL);
    device->funcs.p_vkFreeMemory(device->device, nis_data->memory, NULL);
    device->funcs.p_vkDestroyPipeline(device->device, nis_data->pipeline, NULL);
    device->funcs.p_vkDestroyPipelineLayout(device->device, nis_data->pipeline_layout, NULL);
    device->funcs.p_vkDestroyDescriptorSetLayout(device->device, nis_data->descriptor_set_layout, NULL);
    device->funcs.p_vkDestroyDescriptorPool(device->device, nis_data->descriptor_pool, NULL);
    device->funcs.p_vkDestroySampler(device->device, nis_data->sampler, NULL);
    free(swapchain->upscaler_data);

    return res;
}

static void record_resource_transfer(VkDevice device, struct VkSwapchainKHR_T *swapchain, VkCommandBuffer cmd, uint32_t dst_index)
{
    struct nis_data *nis_data = swapchain->upscaler_data;
#if defined(USE_STRUCT_CONVERSION)
    VkBufferMemoryBarrier_host bufferBarrier;
    VkImageMemoryBarrier_host imageBarriers[2] = {{0}};
#else
    VkBufferMemoryBarrier bufferBarrier;
    VkImageMemoryBarrier imageBarriers[2] = {{0}};
#endif

    imageBarriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imageBarriers[0].oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageBarriers[0].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageBarriers[0].srcQueueFamilyIndex = nis_data->curr_queue_index;
    imageBarriers[0].dstQueueFamilyIndex = dst_index;
    imageBarriers[0].image = nis_data->scaler_coef_img;
    imageBarriers[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageBarriers[0].subresourceRange.baseMipLevel = 0;
    imageBarriers[0].subresourceRange.levelCount = 1;
    imageBarriers[0].subresourceRange.baseArrayLayer = 0;
    imageBarriers[0].subresourceRange.layerCount = 1;

    imageBarriers[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imageBarriers[1].oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageBarriers[1].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageBarriers[1].srcQueueFamilyIndex = nis_data->curr_queue_index;
    imageBarriers[1].dstQueueFamilyIndex = dst_index;
    imageBarriers[1].image = nis_data->scaler_coef_img;
    imageBarriers[1].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageBarriers[1].subresourceRange.baseMipLevel = 0;
    imageBarriers[1].subresourceRange.levelCount = 1;
    imageBarriers[1].subresourceRange.baseArrayLayer = 0;
    imageBarriers[1].subresourceRange.layerCount = 1;

    bufferBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    bufferBarrier.srcQueueFamilyIndex = nis_data->curr_queue_index;
    bufferBarrier.dstQueueFamilyIndex = dst_index;
    bufferBarrier.buffer = nis_data->uniform_buffer;
    bufferBarrier.size = VK_WHOLE_SIZE;

    device->funcs.p_vkCmdPipelineBarrier(
        cmd,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        0,
        0, NULL,
        1, &bufferBarrier,
        2, imageBarriers
    );
}

static VkResult transfer_nis_resources(VkDevice device, struct VkSwapchainKHR_T *swapchain, uint32_t dst_index, struct fs_hack_image *hack)
{
    VkResult res;
    VkCommandPool pool;
    VkSemaphore semaphore;
    VkCommandBuffer releaseCmd, acquireCmd;
    VkPipelineStageFlags waitStages = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    VkSemaphoreCreateInfo semaphoreInfo = {0};
    VkCommandPoolCreateInfo poolInfo = {0};
    VkSubmitInfo submitInfo = {0};
    struct nis_data *nis_data = swapchain->upscaler_data;
#if defined(USE_STRUCT_CONVERSION)
    VkCommandBufferAllocateInfo_host cmdBufferAllocInfo = {0};
    VkCommandBufferBeginInfo_host beginInfo = {0};
#else
    VkCommandBufferAllocateInfo cmdBufferAllocInfo = {0};
    VkCommandBufferBeginInfo beginInfo = {0};
#endif

    if (nis_data->curr_queue_index == dst_index){
        WARN("No need to transfer resources to the same queue index\n");
        return VK_SUCCESS;
    }

    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    res = device->funcs.p_vkCreateSemaphore(device->device, &semaphoreInfo, NULL, &semaphore);
    if(res != VK_SUCCESS){
        ERR("vkCreateSemaphore failed, res=%d\n", res);
        return res;
    }    

    /* Record and submit the resource release from the current queue */
    cmdBufferAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdBufferAllocInfo.commandPool = nis_data->pool;
    cmdBufferAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdBufferAllocInfo.commandBufferCount = 1;

    res = device->funcs.p_vkAllocateCommandBuffers(device->device, &cmdBufferAllocInfo, &releaseCmd);
    if(res != VK_SUCCESS){
        ERR("vkAllocateCommandBuffers failed, res=%d\n", res);
        return res;
    }

    /* Record and submit the command buffer to upload the data */
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

    device->funcs.p_vkBeginCommandBuffer(releaseCmd, &beginInfo);

    record_resource_transfer(device, swapchain, releaseCmd, dst_index);

    device->funcs.p_vkEndCommandBuffer(releaseCmd);

    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &releaseCmd;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &semaphore;

    res = device->funcs.p_vkQueueSubmit(device->queues[nis_data->curr_queue_index].queue, 1, &submitInfo, VK_NULL_HANDLE);
    if(res != VK_SUCCESS){
        ERR("vkQueueSubmit failed, res=%d\n", res);
        return res;
    }

    memset(&submitInfo, 0, sizeof(submitInfo));

    /* Record and submit the resource acquire */
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = dst_index;

    res = device->funcs.p_vkCreateCommandPool(device->device, &poolInfo, NULL, &pool);
    if(res != VK_SUCCESS){
        ERR("vkCreateCommandPool failed, res=%d\n", res);
        return res;
    }

    cmdBufferAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdBufferAllocInfo.commandPool = pool;
    cmdBufferAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdBufferAllocInfo.commandBufferCount = 1;

    res = device->funcs.p_vkAllocateCommandBuffers(device->device, &cmdBufferAllocInfo, &acquireCmd);
    if(res != VK_SUCCESS){
        ERR("vkAllocateCommandBuffers failed, res=%d\n", res);
        return res;
    }

    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

    device->funcs.p_vkBeginCommandBuffer(acquireCmd, &beginInfo);

    record_resource_transfer(device, swapchain, acquireCmd, dst_index);

    device->funcs.p_vkEndCommandBuffer(acquireCmd);

    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &acquireCmd;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &semaphore;
    submitInfo.pWaitDstStageMask = &waitStages;

    res = device->funcs.p_vkQueueSubmit(device->queues[dst_index].queue, 1, &submitInfo, VK_NULL_HANDLE);
    if(res != VK_SUCCESS){
        ERR("vkQueueSubmit failed, res=%d\n", res);
        return res;
    }

    res = device->funcs.p_vkQueueWaitIdle(device->queues[dst_index].queue);
    if(res != VK_SUCCESS){
        ERR("vkQueueWaitIdle failed, res=%d\n", res);
        return res;
    }

    device->funcs.p_vkDestroySemaphore(device->device, semaphore, NULL);
    device->funcs.p_vkDestroyCommandPool(device->device, nis_data->pool, NULL);
    device->funcs.p_vkFreeCommandBuffers(device->device, pool, 1, &acquireCmd);

    /* Update the nis data to hold the information needed if resources ever need to transfer queues again */
    nis_data->curr_queue_index = dst_index;
    nis_data->pool = pool;

    return VK_SUCCESS;
}

static VkResult record_cmd(VkDevice device, struct VkSwapchainKHR_T *swapchain, struct fs_hack_image *hack, uint32_t queue_idx)
{
    VkResult result;
    VkImageCopy region = {0};
    struct nis_data *nis_data = swapchain->upscaler_data;
    struct nis_hack_data* nis_hack = hack->upscaler_data;
#if defined(USE_STRUCT_CONVERSION)
    VkImageMemoryBarrier_host barriers[2] = {{0}};
    VkCommandBufferBeginInfo_host beginInfo = {0};
#else
    VkImageMemoryBarrier barriers[2] = {{0}};
    VkCommandBufferBeginInfo beginInfo = {0};
#endif

    TRACE("recording nis command\n");

    if (queue_idx != nis_data->curr_queue_index){
        result = transfer_nis_resources(device, swapchain, queue_idx, hack);
        if(result != VK_SUCCESS){
            ERR("transfer_nis_resources: %d\n", result);
            return result;
        }
    }

    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

    device->funcs.p_vkBeginCommandBuffer(hack->cmd, &beginInfo);

    /* for the cs we run... */
    /* transition user image from PRESENT_SRC to SHADER_READ */
    barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barriers[0].oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    barriers[0].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[0].image = hack->user_image;
    barriers[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barriers[0].subresourceRange.baseMipLevel = 0;
    barriers[0].subresourceRange.levelCount = 1;
    barriers[0].subresourceRange.baseArrayLayer = 0;
    barriers[0].subresourceRange.layerCount = 1;
    barriers[0].srcAccessMask = 0;
    barriers[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    /* storage image... */
    /* transition blit image from whatever to GENERAL */
    barriers[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barriers[1].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barriers[1].newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[1].image = hack->blit_image ? hack->blit_image : hack->swapchain_image;
    barriers[1].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barriers[1].subresourceRange.baseMipLevel = 0;
    barriers[1].subresourceRange.levelCount = 1;
    barriers[1].subresourceRange.baseArrayLayer = 0;
    barriers[1].subresourceRange.layerCount = 1;
    barriers[1].srcAccessMask = 0;
    barriers[1].dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;

    device->funcs.p_vkCmdPipelineBarrier(
        hack->cmd,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0,
        0, NULL,
        0, NULL,
        2, barriers
    );

    device->funcs.p_vkCmdBindPipeline(hack->cmd,
            VK_PIPELINE_BIND_POINT_COMPUTE, nis_data->pipeline);

    device->funcs.p_vkCmdBindDescriptorSets(hack->cmd,
            VK_PIPELINE_BIND_POINT_COMPUTE, nis_data->pipeline_layout,
            0, 1, &nis_hack->descriptor_set, 0, NULL);

    /* local sizes in shader are 24 */
    device->funcs.p_vkCmdDispatch(hack->cmd, (uint32_t)ceil(swapchain->real_extent.width / 32.0f),
            (uint32_t)ceil(swapchain->real_extent.height / 24.0f), 1);

    /* transition user image from SHADER_READ back to PRESENT_SRC */
    barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barriers[0].oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barriers[0].newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[0].image = hack->user_image;
    barriers[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barriers[0].subresourceRange.baseMipLevel = 0;
    barriers[0].subresourceRange.levelCount = 1;
    barriers[0].subresourceRange.baseArrayLayer = 0;
    barriers[0].subresourceRange.layerCount = 1;
    barriers[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barriers[0].dstAccessMask = 0;

    device->funcs.p_vkCmdPipelineBarrier(
        hack->cmd,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        0,
        0, NULL,
        0, NULL,
        1, barriers
    );

    if(hack->blit_image){
        /* for the copy... */
        /* no transition, just a barrier for our access masks (w -> r) */
        barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barriers[0].oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        barriers[0].newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barriers[0].image = hack->blit_image;
        barriers[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barriers[0].subresourceRange.baseMipLevel = 0;
        barriers[0].subresourceRange.levelCount = 1;
        barriers[0].subresourceRange.baseArrayLayer = 0;
        barriers[0].subresourceRange.layerCount = 1;
        barriers[0].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barriers[0].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        /* for the copy... */
        /* transition swapchain image from whatever to TRANSFER_DST
         * we don't care about the contents... */
        barriers[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barriers[1].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barriers[1].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barriers[1].image = hack->swapchain_image;
        barriers[1].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barriers[1].subresourceRange.baseMipLevel = 0;
        barriers[1].subresourceRange.levelCount = 1;
        barriers[1].subresourceRange.baseArrayLayer = 0;
        barriers[1].subresourceRange.layerCount = 1;
        barriers[1].srcAccessMask = 0;
        barriers[1].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        device->funcs.p_vkCmdPipelineBarrier(
            hack->cmd,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            0, NULL,
            0, NULL,
            2, barriers
        );

        /* copy from blit image to swapchain image */
        region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.srcSubresource.layerCount = 1;
        region.srcOffset.x = 0;
        region.srcOffset.y = 0;
        region.srcOffset.z = 0;
        region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.dstSubresource.layerCount = 1;
        region.dstOffset.x = 0;
        region.dstOffset.y = 0;
        region.dstOffset.z = 0;
        region.extent.width = swapchain->real_extent.width;
        region.extent.height = swapchain->real_extent.height;
        region.extent.depth = 1;

        device->funcs.p_vkCmdCopyImage(hack->cmd,
                hack->blit_image, VK_IMAGE_LAYOUT_GENERAL,
                hack->swapchain_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1, &region);

        /* transition swapchain image from TRANSFER_DST_OPTIMAL to PRESENT_SRC */
        barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barriers[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barriers[0].newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barriers[0].image = hack->swapchain_image;
        barriers[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barriers[0].subresourceRange.baseMipLevel = 0;
        barriers[0].subresourceRange.levelCount = 1;
        barriers[0].subresourceRange.baseArrayLayer = 0;
        barriers[0].subresourceRange.layerCount = 1;
        barriers[0].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barriers[0].dstAccessMask = 0;

        device->funcs.p_vkCmdPipelineBarrier(
            hack->cmd,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            0, NULL,
            0, NULL,
            1, barriers
        );
    }else{
        /* transition swapchain image from GENERAL to PRESENT_SRC */
        barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barriers[0].oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        barriers[0].newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barriers[0].image = hack->swapchain_image;
        barriers[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barriers[0].subresourceRange.baseMipLevel = 0;
        barriers[0].subresourceRange.levelCount = 1;
        barriers[0].subresourceRange.baseArrayLayer = 0;
        barriers[0].subresourceRange.layerCount = 1;
        barriers[0].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barriers[0].dstAccessMask = 0;

        device->funcs.p_vkCmdPipelineBarrier(
            hack->cmd,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            0,
            0, NULL,
            0, NULL,
            1, barriers
        );
    }

    result = device->funcs.p_vkEndCommandBuffer(hack->cmd);
    if(result != VK_SUCCESS){
        ERR("vkEndCommandBuffer: %d\n", result);
        return result;
    }

    return VK_SUCCESS;
}

void destroy(VkDevice device, struct VkSwapchainKHR_T *swapchain)
{
    uint32_t i;
    struct nis_data *nis_data = swapchain->upscaler_data;

    for(i = 0; i < swapchain->n_images; ++i) {
        struct nis_hack_data* nis_hack = (struct nis_hack_data*)swapchain->fs_hack_images[i].upscaler_data;
        device->funcs.p_vkFreeDescriptorSets(device->device, nis_data->descriptor_pool, 1, &nis_hack->descriptor_set);
        swapchain->fs_hack_images[i].upscaler_data = NULL;
        free(nis_hack);
    }
    device->funcs.p_vkDestroyBuffer(device->device, nis_data->uniform_buffer, NULL);
    device->funcs.p_vkDestroyImageView(device->device, nis_data->scaler_coef_img_view, NULL);
    device->funcs.p_vkDestroyImage(device->device, nis_data->scaler_coef_img, NULL);
    device->funcs.p_vkDestroyImageView(device->device, nis_data->usm_coef_img_view, NULL);
    device->funcs.p_vkDestroyImage(device->device, nis_data->usm_coef_img, NULL);
    device->funcs.p_vkFreeMemory(device->device, nis_data->memory, NULL);
    device->funcs.p_vkDestroyPipeline(device->device, nis_data->pipeline, NULL);
    device->funcs.p_vkDestroyPipelineLayout(device->device, nis_data->pipeline_layout, NULL);
    device->funcs.p_vkDestroyDescriptorSetLayout(device->device, nis_data->descriptor_set_layout, NULL);
    device->funcs.p_vkDestroyDescriptorPool(device->device, nis_data->descriptor_pool, NULL);
    device->funcs.p_vkDestroySampler(device->device, nis_data->sampler, NULL);
    device->funcs.p_vkDestroyCommandPool(device->device, nis_data->pool, NULL);

    free(nis_data);
    swapchain->upscaler_data = NULL;
}

struct upscaler_implementation nis_upscaler = {
    init,
    record_cmd,
    destroy
};
