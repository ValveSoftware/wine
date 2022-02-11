/* Copyright 2022 Roderick Colenbrander
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

WINE_DEFAULT_DEBUG_CHANNEL(vulkan);

/*
#version 450

layout(binding = 0) uniform sampler2D texSampler;
layout(binding = 1, rgba8) uniform writeonly image2D outImage;
layout(push_constant) uniform pushConstants {
    //both in real image coords
    vec2 offset;
    vec2 extents;
} constants;

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

void main()
{
    vec2 texcoord = (vec2(gl_GlobalInvocationID.xy) - constants.offset) / constants.extents;
    vec4 c = texture(texSampler, texcoord);
    imageStore(outImage, ivec2(gl_GlobalInvocationID.xy), c);
}
*/
static const uint32_t blit_comp_spv[] = {
    0x07230203, 0x00010000, 0x000D000A, 0x00000036, 0x00000000, 0x00020011, 0x00000001, 0x0006000B,
    0x00000001, 0x4C534C47, 0x6474732E, 0x3035342E, 0x00000000, 0x0003000E, 0x00000000, 0x00000001,
    0x0006000F, 0x00000005, 0x00000004, 0x6E69616D, 0x00000000, 0x0000000D, 0x00060010, 0x00000004,
    0x00000011, 0x00000008, 0x00000008, 0x00000001, 0x00040047, 0x0000000D, 0x0000000B, 0x0000001C,
    0x00050048, 0x00000012, 0x00000000, 0x00000023, 0x00000000, 0x00050048, 0x00000012, 0x00000001,
    0x00000023, 0x00000008, 0x00030047, 0x00000012, 0x00000002, 0x00040047, 0x00000025, 0x00000022,
    0x00000000, 0x00040047, 0x00000025, 0x00000021, 0x00000000, 0x00040047, 0x0000002C, 0x00000022,
    0x00000000, 0x00040047, 0x0000002C, 0x00000021, 0x00000001, 0x00030047, 0x0000002C, 0x00000019,
    0x00040047, 0x00000035, 0x0000000B, 0x00000019, 0x00020013, 0x00000002, 0x00030021, 0x00000003,
    0x00000002, 0x00030016, 0x00000006, 0x00000020, 0x00040017, 0x00000007, 0x00000006, 0x00000002,
    0x00040015, 0x0000000A, 0x00000020, 0x00000000, 0x00040017, 0x0000000B, 0x0000000A, 0x00000003,
    0x00040020, 0x0000000C, 0x00000001, 0x0000000B, 0x0004003B, 0x0000000C, 0x0000000D, 0x00000001,
    0x00040017, 0x0000000E, 0x0000000A, 0x00000002, 0x0004001E, 0x00000012, 0x00000007, 0x00000007,
    0x00040020, 0x00000013, 0x00000009, 0x00000012, 0x0004003B, 0x00000013, 0x00000014, 0x00000009,
    0x00040015, 0x00000015, 0x00000020, 0x00000001, 0x0004002B, 0x00000015, 0x00000016, 0x00000000,
    0x00040020, 0x00000017, 0x00000009, 0x00000007, 0x0004002B, 0x00000015, 0x0000001B, 0x00000001,
    0x00040017, 0x0000001F, 0x00000006, 0x00000004, 0x00090019, 0x00000022, 0x00000006, 0x00000001,
    0x00000000, 0x00000000, 0x00000000, 0x00000001, 0x00000000, 0x0003001B, 0x00000023, 0x00000022,
    0x00040020, 0x00000024, 0x00000000, 0x00000023, 0x0004003B, 0x00000024, 0x00000025, 0x00000000,
    0x0004002B, 0x00000006, 0x00000028, 0x00000000, 0x00090019, 0x0000002A, 0x00000006, 0x00000001,
    0x00000000, 0x00000000, 0x00000000, 0x00000002, 0x00000004, 0x00040020, 0x0000002B, 0x00000000,
    0x0000002A, 0x0004003B, 0x0000002B, 0x0000002C, 0x00000000, 0x00040017, 0x00000030, 0x00000015,
    0x00000002, 0x0004002B, 0x0000000A, 0x00000033, 0x00000008, 0x0004002B, 0x0000000A, 0x00000034,
    0x00000001, 0x0006002C, 0x0000000B, 0x00000035, 0x00000033, 0x00000033, 0x00000034, 0x00050036,
    0x00000002, 0x00000004, 0x00000000, 0x00000003, 0x000200F8, 0x00000005, 0x0004003D, 0x0000000B,
    0x0000000F, 0x0000000D, 0x0007004F, 0x0000000E, 0x00000010, 0x0000000F, 0x0000000F, 0x00000000,
    0x00000001, 0x00040070, 0x00000007, 0x00000011, 0x00000010, 0x00050041, 0x00000017, 0x00000018,
    0x00000014, 0x00000016, 0x0004003D, 0x00000007, 0x00000019, 0x00000018, 0x00050083, 0x00000007,
    0x0000001A, 0x00000011, 0x00000019, 0x00050041, 0x00000017, 0x0000001C, 0x00000014, 0x0000001B,
    0x0004003D, 0x00000007, 0x0000001D, 0x0000001C, 0x00050088, 0x00000007, 0x0000001E, 0x0000001A,
    0x0000001D, 0x0004003D, 0x00000023, 0x00000026, 0x00000025, 0x00070058, 0x0000001F, 0x00000029,
    0x00000026, 0x0000001E, 0x00000002, 0x00000028, 0x0004003D, 0x0000002A, 0x0000002D, 0x0000002C,
    0x0004007C, 0x00000030, 0x00000031, 0x00000010, 0x00040063, 0x0000002D, 0x00000031, 0x00000029,
    0x000100FD, 0x00010038
};

struct blit_hack_data
{
    VkDescriptorSet descriptor_set;
};

struct blit_data
{
    VkSampler sampler;
    VkDescriptorPool descriptor_pool;
    VkDescriptorSetLayout descriptor_set_layout;
    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline;
};

static VkResult create_descriptor_set(VkDevice device, struct VkSwapchainKHR_T *swapchain, struct fs_hack_image *hack)
{
    VkResult res;
    struct blit_data *blit_data = swapchain->upscaler_data;
#if defined(USE_STRUCT_CONVERSION)
    VkDescriptorSetAllocateInfo_host descriptorAllocInfo = {0};
    VkWriteDescriptorSet_host descriptorWrites[2] = {{0}, {0}};
    VkDescriptorImageInfo_host userDescriptorImageInfo = {0}, realDescriptorImageInfo = {0};
#else
    VkDescriptorSetAllocateInfo descriptorAllocInfo = {0};
    VkWriteDescriptorSet descriptorWrites[2] = {{0}, {0}};
    VkDescriptorImageInfo userDescriptorImageInfo = {0}, realDescriptorImageInfo = {0};
#endif

    struct blit_hack_data* blit_hack = calloc(1, sizeof(struct blit_hack_data));
    if (blit_hack == NULL){
        ERR("Failed to allocate blit hack data");
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }
    hack->upscaler_data = blit_hack;

    descriptorAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptorAllocInfo.descriptorPool = blit_data->descriptor_pool;
    descriptorAllocInfo.descriptorSetCount = 1;
    descriptorAllocInfo.pSetLayouts = &blit_data->descriptor_set_layout;

    res = device->funcs.p_vkAllocateDescriptorSets(device->device, &descriptorAllocInfo, &blit_hack->descriptor_set);
    if(res != VK_SUCCESS){
        ERR("vkAllocateDescriptorSets: %d\n", res);
        return res;
    }

    userDescriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    userDescriptorImageInfo.imageView = hack->user_view;
    userDescriptorImageInfo.sampler = blit_data->sampler;

    realDescriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    realDescriptorImageInfo.imageView = hack->blit_view;

    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = blit_hack->descriptor_set;
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pImageInfo = &userDescriptorImageInfo;

    descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[1].dstSet = blit_hack->descriptor_set;
    descriptorWrites[1].dstBinding = 1;
    descriptorWrites[1].dstArrayElement = 0;
    descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].pImageInfo = &realDescriptorImageInfo;

    device->funcs.p_vkUpdateDescriptorSets(device->device, 2, descriptorWrites, 0, NULL);

    return VK_SUCCESS;
}

static VkResult init(VkDevice device, struct VkSwapchainKHR_T *swapchain)
{
    VkResult res;
    VkSamplerCreateInfo samplerInfo = {0};
    VkDescriptorPoolSize poolSizes[2] = {{0}, {0}};
    VkDescriptorPoolCreateInfo poolInfo = {0};
    VkDescriptorSetLayoutBinding layoutBindings[2] = {{0}, {0}};
    VkDescriptorSetLayoutCreateInfo descriptorLayoutInfo = {0};
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {0};
    VkPushConstantRange pushConstants;
    VkShaderModuleCreateInfo shaderInfo = {0};
    VkShaderModule shaderModule = 0;
    struct blit_data *blit_data;
    uint32_t i;
#if defined(USE_STRUCT_CONVERSION)
    VkComputePipelineCreateInfo_host pipelineInfo = {0};
#else
    VkComputePipelineCreateInfo pipelineInfo = {0};
#endif

    blit_data = calloc(1, sizeof(struct blit_data));
    if (!blit_data)
    {
        WARN("calloc failed\n");
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    swapchain->upscaler_data = blit_data;

    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = swapchain->fs_hack_filter;
    samplerInfo.minFilter = swapchain->fs_hack_filter;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;

    res = device->funcs.p_vkCreateSampler(device->device, &samplerInfo, NULL, &blit_data->sampler);
    if(res != VK_SUCCESS)
    {
        WARN("vkCreateSampler failed, res=%d\n", res);
        return res;
    }

    poolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[0].descriptorCount = swapchain->n_images;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    poolSizes[1].descriptorCount = swapchain->n_images;

    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 2;
    poolInfo.pPoolSizes = poolSizes;
    poolInfo.maxSets = swapchain->n_images;

    res = device->funcs.p_vkCreateDescriptorPool(device->device, &poolInfo, NULL, &blit_data->descriptor_pool);
    if(res != VK_SUCCESS){
        ERR("vkCreateDescriptorPool: %d\n", res);
        goto fail;
    }

    layoutBindings[0].binding = 0;
    layoutBindings[0].descriptorCount = 1;
    layoutBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    layoutBindings[0].pImmutableSamplers = NULL;
    layoutBindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    layoutBindings[1].binding = 1;
    layoutBindings[1].descriptorCount = 1;
    layoutBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    layoutBindings[1].pImmutableSamplers = NULL;
    layoutBindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    descriptorLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorLayoutInfo.bindingCount = 2;
    descriptorLayoutInfo.pBindings = layoutBindings;

    res = device->funcs.p_vkCreateDescriptorSetLayout(device->device, &descriptorLayoutInfo, NULL, &blit_data->descriptor_set_layout);
    if(res != VK_SUCCESS){
        ERR("vkCreateDescriptorSetLayout: %d\n", res);
        goto fail;
    }

    pushConstants.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushConstants.offset = 0;
    pushConstants.size = 4 * sizeof(float); /* 2 * vec2 */

    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &blit_data->descriptor_set_layout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstants;

    res = device->funcs.p_vkCreatePipelineLayout(device->device, &pipelineLayoutInfo, NULL, &blit_data->pipeline_layout);
    if(res != VK_SUCCESS){
        ERR("vkCreatePipelineLayout: %d\n", res);
        goto fail;
    }

    shaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderInfo.codeSize = sizeof(blit_comp_spv);
    shaderInfo.pCode = blit_comp_spv;

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
    pipelineInfo.layout = blit_data->pipeline_layout;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineInfo.basePipelineIndex = -1;

    res = device->funcs.p_vkCreateComputePipelines(device->device, VK_NULL_HANDLE, 1, &pipelineInfo, NULL, &blit_data->pipeline);
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

    return VK_SUCCESS;

fail:
    for(i = 0; i < swapchain->n_images; ++i){
        struct fs_hack_image *hack = &swapchain->fs_hack_images[i];
        struct blit_hack_data* blit_hack = (struct blit_hack_data*)hack->upscaler_data;
        if (blit_hack != NULL) {
            device->funcs.p_vkFreeDescriptorSets(device->device, blit_data->descriptor_pool, 1, &blit_hack->descriptor_set);
            hack->upscaler_data = NULL;
            free(blit_hack);
        }
    }
    device->funcs.p_vkDestroyShaderModule(device->device, shaderModule, NULL);
    device->funcs.p_vkDestroyPipeline(device->device, blit_data->pipeline, NULL);
    device->funcs.p_vkDestroyPipelineLayout(device->device, blit_data->pipeline_layout, NULL);
    device->funcs.p_vkDestroyDescriptorSetLayout(device->device, blit_data->descriptor_set_layout, NULL);
    device->funcs.p_vkDestroyDescriptorPool(device->device, blit_data->descriptor_pool, NULL);
    device->funcs.p_vkDestroySampler(device->device, blit_data->sampler, NULL);
    free(blit_data);

    return res;
}

static VkResult record_graphics_cmd(VkDevice device, struct VkSwapchainKHR_T *swapchain, struct fs_hack_image *hack)
{
    VkResult result;
    VkImageBlit blitregion = {0};
    VkImageSubresourceRange range = {0};
    VkClearColorValue black = {{0.f, 0.f, 0.f}};
#if defined(USE_STRUCT_CONVERSION)
    VkImageMemoryBarrier_host barriers[2] = {{0}};
    VkCommandBufferBeginInfo_host beginInfo = {0};
#else
    VkImageMemoryBarrier barriers[2] = {{0}};
    VkCommandBufferBeginInfo beginInfo = {0};
#endif

    TRACE("recording graphics command\n");

    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

    device->funcs.p_vkBeginCommandBuffer(hack->cmd, &beginInfo);

    /* transition real image from whatever to TRANSFER_DST_OPTIMAL */
    barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barriers[0].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barriers[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[0].image = hack->swapchain_image;
    barriers[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barriers[0].subresourceRange.baseMipLevel = 0;
    barriers[0].subresourceRange.levelCount = 1;
    barriers[0].subresourceRange.baseArrayLayer = 0;
    barriers[0].subresourceRange.layerCount = 1;
    barriers[0].srcAccessMask = 0;
    barriers[0].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    /* transition user image from PRESENT_SRC to TRANSFER_SRC_OPTIMAL */
    barriers[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barriers[1].oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    barriers[1].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[1].image = hack->user_image;
    barriers[1].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barriers[1].subresourceRange.baseMipLevel = 0;
    barriers[1].subresourceRange.levelCount = 1;
    barriers[1].subresourceRange.baseArrayLayer = 0;
    barriers[1].subresourceRange.layerCount = 1;
    barriers[1].srcAccessMask = 0;
    barriers[1].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

    device->funcs.p_vkCmdPipelineBarrier(
            hack->cmd,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            0, NULL,
            0, NULL,
            2, barriers
    );

    /* clear the image */
    range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    range.baseMipLevel = 0;
    range.levelCount = 1;
    range.baseArrayLayer = 0;
    range.layerCount = 1;

    device->funcs.p_vkCmdClearColorImage(
            hack->cmd, hack->swapchain_image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            &black, 1, &range);

    /* perform blit */
    blitregion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blitregion.srcSubresource.layerCount = 1;
    blitregion.srcOffsets[0].x = 0;
    blitregion.srcOffsets[0].y = 0;
    blitregion.srcOffsets[0].z = 0;
    blitregion.srcOffsets[1].x = swapchain->user_extent.width;
    blitregion.srcOffsets[1].y = swapchain->user_extent.height;
    blitregion.srcOffsets[1].z = 1;
    blitregion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blitregion.dstSubresource.layerCount = 1;
    blitregion.dstOffsets[0].x = swapchain->blit_dst.offset.x;
    blitregion.dstOffsets[0].y = swapchain->blit_dst.offset.y;
    blitregion.dstOffsets[0].z = 0;
    blitregion.dstOffsets[1].x = swapchain->blit_dst.offset.x + swapchain->blit_dst.extent.width;
    blitregion.dstOffsets[1].y = swapchain->blit_dst.offset.y + swapchain->blit_dst.extent.height;
    blitregion.dstOffsets[1].z = 1;

    device->funcs.p_vkCmdBlitImage(hack->cmd,
            hack->user_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            hack->swapchain_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &blitregion, swapchain->fs_hack_filter);

    /* transition real image from TRANSFER_DST to PRESENT_SRC */
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

    /* transition user image from TRANSFER_SRC_OPTIMAL to back to PRESENT_SRC */
    barriers[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barriers[1].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barriers[1].newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    barriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[1].image = hack->user_image;
    barriers[1].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barriers[1].subresourceRange.baseMipLevel = 0;
    barriers[1].subresourceRange.levelCount = 1;
    barriers[1].subresourceRange.baseArrayLayer = 0;
    barriers[1].subresourceRange.layerCount = 1;
    barriers[1].srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barriers[1].dstAccessMask = 0;

    device->funcs.p_vkCmdPipelineBarrier(
            hack->cmd,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            0,
            0, NULL,
            0, NULL,
            2, barriers
    );

    result = device->funcs.p_vkEndCommandBuffer(hack->cmd);
    if(result != VK_SUCCESS){
        ERR("vkEndCommandBuffer: %d\n", result);
        return result;
    }

    return VK_SUCCESS;
}

static VkResult record_compute_cmd(VkDevice device, struct VkSwapchainKHR_T *swapchain, struct fs_hack_image *hack)
{
    VkResult result;
    VkImageCopy region = {0};
    struct blit_data *blit_data = swapchain->upscaler_data;
    struct blit_hack_data* blit_hack = hack->upscaler_data;
#if defined(USE_STRUCT_CONVERSION)
    VkImageMemoryBarrier_host barriers[3] = {{0}};
    VkCommandBufferBeginInfo_host beginInfo = {0};
#else
    VkImageMemoryBarrier barriers[3] = {{0}};
    VkCommandBufferBeginInfo beginInfo = {0};
#endif
    float constants[4];

    TRACE("recording compute command\n");

#if 0
    /* DOOM runs out of memory when allocating blit images after loading. */
    if(!swapchain->blit_image_memory){
        result = init_blit_images(device, swapchain);
        if(result != VK_SUCCESS)
            return result;
    }
#endif

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

    /* perform blit shader */
    device->funcs.p_vkCmdBindPipeline(hack->cmd,
            VK_PIPELINE_BIND_POINT_COMPUTE, blit_data->pipeline);

    device->funcs.p_vkCmdBindDescriptorSets(hack->cmd,
            VK_PIPELINE_BIND_POINT_COMPUTE, blit_data->pipeline_layout,
            0, 1, &blit_hack->descriptor_set, 0, NULL);

    /* vec2: blit dst offset in real coords */
    constants[0] = swapchain->blit_dst.offset.x;
    constants[1] = swapchain->blit_dst.offset.y;
    /* vec2: blit dst extents in real coords */
    constants[2] = swapchain->blit_dst.extent.width;
    constants[3] = swapchain->blit_dst.extent.height;
    device->funcs.p_vkCmdPushConstants(hack->cmd,
            blit_data->pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT,
            0, sizeof(constants), constants);

    /* local sizes in shader are 8 */
    device->funcs.p_vkCmdDispatch(hack->cmd, ceil(swapchain->real_extent.width / 8.),
            ceil(swapchain->real_extent.height / 8.), 1);

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

static VkResult record_cmd(VkDevice device, struct VkSwapchainKHR_T *swapchain, struct fs_hack_image *hack, uint32_t queue_idx)
{
    VkResult res;

    if(device->queue_props[queue_idx].queueFlags & VK_QUEUE_GRAPHICS_BIT)
        res = record_graphics_cmd(device, swapchain, hack);
    else if(device->queue_props[queue_idx].queueFlags & VK_QUEUE_COMPUTE_BIT)
        res = record_compute_cmd(device, swapchain, hack);
    else{
        ERR("Present queue is neither graphics nor compute queue!\n");
        res = VK_ERROR_DEVICE_LOST;
    }

    return res;
}

static void destroy(VkDevice device, struct VkSwapchainKHR_T *swapchain)
{
    uint32_t i;
    struct blit_data *blit_data = swapchain->upscaler_data;

    for(i = 0; i < swapchain->n_images; ++i) {
        struct blit_hack_data* blit_hack = (struct blit_hack_data*)swapchain->fs_hack_images[i].upscaler_data;
        device->funcs.p_vkFreeDescriptorSets(device->device, blit_data->descriptor_pool, 1, &blit_hack->descriptor_set);
        swapchain->fs_hack_images[i].upscaler_data = NULL;
        free(blit_hack);
    }
    device->funcs.p_vkDestroyPipeline(device->device, blit_data->pipeline, NULL);
    device->funcs.p_vkDestroyPipelineLayout(device->device, blit_data->pipeline_layout, NULL);
    device->funcs.p_vkDestroyDescriptorSetLayout(device->device, blit_data->descriptor_set_layout, NULL);
    device->funcs.p_vkDestroyDescriptorPool(device->device, blit_data->descriptor_pool, NULL);
    device->funcs.p_vkDestroySampler(device->device, blit_data->sampler, NULL);

    free(blit_data);
    swapchain->upscaler_data = NULL;
}

struct upscaler_implementation blit_upscaler = {
    init,
    record_cmd,
    destroy
};
