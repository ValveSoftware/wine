/*
 * XeSS DLL - D3D12 implementation
 * Copyright 2025 the Wine project
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

#include <stdarg.h>

#include <stdlib.h>

#include <string.h>
#include "windef.h"
#include "winbase.h"
#include "wine/debug.h"
#include "wine/list.h"
#define COBJMACROS

#include <initguid.h>

#include "xess_wine.h"
#include "unixlib.h"

#include "extern/xess_d3d12.h"
#define VKD3D_NO_WIN32_TYPES
#include <vkd3d.h>
#undef VKD3D_NO_WIN32_TYPES

#include "vkd3d-proton-interop.h"

WINE_DEFAULT_DEBUG_CHANNEL(xess);

static UINT get_mip_level_count_from_desc(const D3D12_RESOURCE_DESC *desc)
{
    // D3D12 allows creating textures with MipLevels=0 (meaning "full mip chain")
    // If MipLevels is 0, the number of mip levels is calculated based on the dimensions of the resource.
    // ref: https://github.com/microsoft/DirectXTex/wiki/CalculateMipLevels/6b633fd8fe916225d4a7ece15c3c40c2eb6da163
    // "The maximum number of mipmap levels is calculated by repeatedly halving each dimension
    // (width, height, and depth for 3D) until at least one dimension reaches 1 pixel.
    // The total number of levels includes the original base level plus all reduced levels."
    UINT64 w = desc->Width;
    UINT32 h = desc->Height;
    UINT32 d = (desc->Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D) ? desc->DepthOrArraySize : 1;
    UINT16 mips = 1;

    if (desc->MipLevels > 0)
        return desc->MipLevels;

    while (w > 1 || h > 1 || d > 1)
    {
        if (w > 1) w >>= 1;
        if (h > 1) h >>= 1;
        if (d > 1) d >>= 1;
        ++mips;
    }
    return mips;
}

static VkImageView get_vk_image_view(VkDevice vk_device, PFN_vkCreateImageView pfn_vkCreateImageView,
    VkImage vk_image, VkFormat format, const D3D12_RESOURCE_DESC *desc,
    VkImageAspectFlags aspect_mask, UINT mip_level_count)
{
    VkImageViewCreateInfo view_info;
    VkImageView image_view = VK_NULL_HANDLE;
    VkResult vk_result;

    memset(&view_info, 0, sizeof(view_info));
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = vk_image;
    view_info.viewType = (desc->Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D) ? VK_IMAGE_VIEW_TYPE_3D
        : ((desc->DepthOrArraySize > 1) ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D);
    view_info.format = format;
    view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.subresourceRange.aspectMask = aspect_mask;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = mip_level_count;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = (desc->Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D) ? 1 : desc->DepthOrArraySize;

    vk_result = pfn_vkCreateImageView(vk_device, &view_info, NULL, &image_view);
    if (vk_result != VK_SUCCESS)
    {
        WARN("Failed to create VkImageView: %d\n", vk_result);
        return VK_NULL_HANDLE;
    }

    return image_view;
}

static VkImageViewType get_vk_image_view_type_from_desc(const D3D12_RESOURCE_DESC *desc)
{
    return (desc->Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D) ? VK_IMAGE_VIEW_TYPE_3D
        : ((desc->DepthOrArraySize > 1) ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D);
}

static xess_result_t translate_heap_to_vk_memory(ID3D12Heap *heap, VkDeviceMemory *vk_memory, uint64_t *base_offset)
{
    ID3D12DXVKInteropDevice3 *interop = NULL;
    ID3D12Device *device = NULL;
    UINT64 vk_memory_u64;
    UINT64 heap_offset_u64;
    UINT32 vk_memory_type;
    HRESULT hr;

    TRACE("(%p, %p, %p)\n", heap, vk_memory, base_offset);

    if (!heap)
    {
        TRACE("No heap provided, skipping external heap handling.\n");
        *vk_memory = VK_NULL_HANDLE;
        *base_offset = 0;
        return XESS_RESULT_SUCCESS;
    }

    hr = ID3D12Heap_GetDevice(heap, &IID_ID3D12Device, (void **)&device);
    if (FAILED(hr))
    {
        WARN("Failed to get heap device: %#lx\n", hr);
        return XESS_RESULT_ERROR_UNSUPPORTED;
    }

    TRACE("Heap device: %p\n", device);

    hr = ID3D12Device_QueryInterface(device, &IID_ID3D12DXVKInteropDevice3, (void **)&interop);
    ID3D12Device_Release(device);
    if (FAILED(hr))
    {
        WARN("ID3D12DXVKInteropDevice3 unavailable (%#lx), ignoring external heap and falling back to internal XeSS allocation.\n", hr);
        *vk_memory = VK_NULL_HANDLE;
        *base_offset = 0;
        return XESS_RESULT_SUCCESS;
    }

    TRACE("Interop: %p\n", interop);

    hr = ID3D12DXVKInteropDevice3_GetVulkanHeapInfo(interop, heap, &vk_memory_u64, &heap_offset_u64, &vk_memory_type);
    ID3D12DXVKInteropDevice3_Release(interop);
    if (FAILED(hr))
    {
        WARN("GetVulkanHeapInfo failed: %#lx\n", hr);
        return XESS_RESULT_ERROR_UNSUPPORTED;
    }

    *vk_memory = (VkDeviceMemory)vk_memory_u64;
    *base_offset = heap_offset_u64;
    TRACE("Got memory handle %#I64x with offset %#I64x and memory type %u.\n", vk_memory_u64, *base_offset, vk_memory_type);
    return XESS_RESULT_SUCCESS;
}

/* Per-context state tracking helpers. */
struct xess_d3d12_state_tracker
{
    struct list entry;
    xess_context_handle_t context;
    /* for pipeline cache*/
    VkDevice vk_device;
    VkPipelineCache pipeline_cache;
    PFN_vkDestroyPipelineCache pfn_vkDestroyPipelineCache;
    /* for temporary heaps */
    ID3D12Heap *temp_buffer_heap;
    ID3D12Heap *temp_texture_heap;
    uint64_t buffer_heap_base_offset;
    uint64_t texture_heap_base_offset;
    /* Keep image views alive after command recording and reuse them across executes. */
    struct list image_view_entries;
    PFN_vkDestroyImageView pfn_vkDestroyImageView;
};

struct xess_d3d12_image_view_entry
{
    struct list entry;
    VkImage image;
    VkFormat format;
    VkImageAspectFlags aspect_mask;
    VkImageViewType view_type;
    UINT mip_level_count;
    UINT layer_count;
    VkImageView image_view;
    UINT reuse_count;
};

static struct list xess_d3d12_state_trackers = LIST_INIT(xess_d3d12_state_trackers);

static void xess_d3d12_release_state_tracker(struct xess_d3d12_state_tracker *entry)
{
    PFN_vkDeviceWaitIdle pfn_vkDeviceWaitIdle;
    PFN_vkDestroyImageView pfn_vkDestroyImageView = entry->pfn_vkDestroyImageView;
    struct xess_d3d12_image_view_entry *view_entry, *view_entry_next;
    VkResult vk_result;

    if (entry->vk_device &&
        (!list_empty(&entry->image_view_entries) || entry->pipeline_cache != VK_NULL_HANDLE))
    {
        pfn_vkDeviceWaitIdle = (PFN_vkDeviceWaitIdle)vkGetDeviceProcAddr(entry->vk_device, "vkDeviceWaitIdle");
        if (pfn_vkDeviceWaitIdle)
        {
            vk_result = pfn_vkDeviceWaitIdle(entry->vk_device);
            if (vk_result != VK_SUCCESS)
                WARN("vkDeviceWaitIdle failed while releasing context %p: %d\n", entry->context, vk_result);
        }
    }

    if (!pfn_vkDestroyImageView && entry->vk_device)
        pfn_vkDestroyImageView = (PFN_vkDestroyImageView)vkGetDeviceProcAddr(entry->vk_device, "vkDestroyImageView");

    if (!pfn_vkDestroyImageView && !list_empty(&entry->image_view_entries))
        WARN("Unable to resolve vkDestroyImageView for %p, leaking %u cached image views.\n", entry->context,
            list_count(&entry->image_view_entries));

    LIST_FOR_EACH_ENTRY_SAFE(view_entry, view_entry_next, &entry->image_view_entries,
        struct xess_d3d12_image_view_entry, entry)
    {
        if (view_entry->reuse_count)
            TRACE("Destroying cached VkImageView %#I64x (image %#I64x), reused %u times.\n",
                (UINT64)view_entry->image_view, (UINT64)view_entry->image, view_entry->reuse_count);
        list_remove(&view_entry->entry);
        if (pfn_vkDestroyImageView)
            pfn_vkDestroyImageView(entry->vk_device, view_entry->image_view, NULL);
        free(view_entry);
    }

    if (entry->pipeline_cache != VK_NULL_HANDLE && entry->pfn_vkDestroyPipelineCache)
    {
        entry->pfn_vkDestroyPipelineCache(entry->vk_device, entry->pipeline_cache, NULL);
        entry->pipeline_cache = VK_NULL_HANDLE;
    }
    entry->vk_device = VK_NULL_HANDLE;
    entry->pfn_vkDestroyPipelineCache = NULL;
    entry->pfn_vkDestroyImageView = NULL;

    if (entry->temp_texture_heap)
    {
        ID3D12Heap_Release(entry->temp_texture_heap);
        entry->temp_texture_heap = NULL;
    }
    if (entry->temp_buffer_heap)
    {
        ID3D12Heap_Release(entry->temp_buffer_heap);
        entry->temp_buffer_heap = NULL;
    }
    entry->buffer_heap_base_offset = 0;
    entry->texture_heap_base_offset = 0;
    list_init(&entry->image_view_entries);
}

static struct xess_d3d12_state_tracker *xess_d3d12_find_state_tracker(xess_context_handle_t hContext)
{
    struct xess_d3d12_state_tracker *entry;

    LIST_FOR_EACH_ENTRY(entry, &xess_d3d12_state_trackers, struct xess_d3d12_state_tracker, entry)
    {
        if (entry->context == hContext)
            return entry;
    }

    return NULL;
}

static void xess_d3d12_set_image_view_destroy_proc(xess_context_handle_t hContext,
    PFN_vkDestroyImageView pfn_vkDestroyImageView)
{
    struct xess_d3d12_state_tracker *entry;

    if ((entry = xess_d3d12_find_state_tracker(hContext)))
        entry->pfn_vkDestroyImageView = pfn_vkDestroyImageView;
}

static VkImageView xess_d3d12_get_or_create_image_view(xess_context_handle_t hContext,
    VkDevice vk_device, PFN_vkCreateImageView pfn_vkCreateImageView,
    VkImage vk_image, VkFormat format, const D3D12_RESOURCE_DESC *desc,
    VkImageAspectFlags aspect_mask, UINT mip_level_count)
{
    struct xess_d3d12_state_tracker *state_entry;
    struct xess_d3d12_image_view_entry *cache_entry;
    VkImageViewType view_type;
    UINT layer_count;
    VkImageView image_view;

    state_entry = xess_d3d12_find_state_tracker(hContext);
    if (!state_entry)
    {
        WARN("No state tracker found for context %p when creating image view.\n", hContext);
        return VK_NULL_HANDLE;
    }

    view_type = get_vk_image_view_type_from_desc(desc);
    layer_count = (desc->Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D) ? 1 : desc->DepthOrArraySize;

    LIST_FOR_EACH_ENTRY(cache_entry, &state_entry->image_view_entries, struct xess_d3d12_image_view_entry, entry)
    {
        if (cache_entry->image == vk_image &&
            cache_entry->format == format &&
            cache_entry->aspect_mask == aspect_mask &&
            cache_entry->view_type == view_type &&
            cache_entry->mip_level_count == mip_level_count &&
            cache_entry->layer_count == layer_count)
        {
            cache_entry->reuse_count++;
            if (cache_entry->reuse_count == 1)
                TRACE("Reusing cached VkImageView %#I64x for image %#I64x (format %u).\n",
                    (UINT64)cache_entry->image_view, (UINT64)cache_entry->image, cache_entry->format);
            return cache_entry->image_view;
        }
    }

    image_view = get_vk_image_view(vk_device, pfn_vkCreateImageView, vk_image, format, desc, aspect_mask, mip_level_count);
    if (image_view == VK_NULL_HANDLE)
        return VK_NULL_HANDLE;

    if (!(cache_entry = calloc(1, sizeof(*cache_entry))))
    {
        WARN("Failed to allocate image view cache entry. Falling back to uncached VkImageView.\n");
        return image_view;
    }

    cache_entry->image = vk_image;
    cache_entry->format = format;
    cache_entry->aspect_mask = aspect_mask;
    cache_entry->view_type = view_type;
    cache_entry->mip_level_count = mip_level_count;
    cache_entry->layer_count = layer_count;
    cache_entry->image_view = image_view;
    cache_entry->reuse_count = 0;
    list_add_tail(&state_entry->image_view_entries, &cache_entry->entry);
    TRACE("Cached new VkImageView %#I64x for image %#I64x (format %u, mips %u, layers %u).\n",
        (UINT64)cache_entry->image_view, (UINT64)cache_entry->image, cache_entry->format,
        cache_entry->mip_level_count, cache_entry->layer_count);
    return image_view;
}

static xess_result_t translate_texture_resource(
    xess_context_handle_t hContext,
    ID3D12DXVKInteropDevice3 *interop,
    VkDevice vk_device,
    PFN_vkCreateImageView pfn_vkCreateImageView,
    ID3D12Resource *pTexture,
    xess_vk_image_view_info *pTextureInfo,
    const char *texture_name)
{
    D3D12_RESOURCE_DESC desc;
    UINT mip_level_count;
    UINT64 vk_handle;
    UINT64 buffer_offset;
    HRESULT hr;
    VkImageAspectFlags aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT;

    TRACE("Handling %s texture...\n", texture_name);

    hr = ID3D12DXVKInteropDevice3_GetVulkanResourceInfo1(interop, pTexture,
        &vk_handle, &buffer_offset, &pTextureInfo->format);
    (void)buffer_offset; // not used for textures
    if (FAILED(hr))
    {
        WARN("Failed to get %s texture info: %#lx\n", texture_name, hr);
        return XESS_RESULT_ERROR_UNKNOWN;
    }

    desc = ID3D12Resource_GetDesc(pTexture);
    if (desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
    {
        WARN("%s resource is a buffer, expected a texture.\n", texture_name);
        return XESS_RESULT_ERROR_INVALID_ARGUMENT;
    }
    mip_level_count = get_mip_level_count_from_desc(&desc);
    TRACE("%s texture: %I64ux%u, MipLevels=%u, ArraySize=%u, Format=%u\n",
          texture_name, desc.Width, desc.Height, desc.MipLevels, desc.DepthOrArraySize, desc.Format);
    TRACE("%s texture: computed mip_level_count=%u\n",
          texture_name, mip_level_count);
    if (pTextureInfo->format == VK_FORMAT_D16_UNORM || pTextureInfo->format == VK_FORMAT_X8_D24_UNORM_PACK32 ||
        pTextureInfo->format == VK_FORMAT_D32_SFLOAT || pTextureInfo->format == VK_FORMAT_D16_UNORM_S8_UINT ||
        pTextureInfo->format == VK_FORMAT_D24_UNORM_S8_UINT || pTextureInfo->format == VK_FORMAT_D32_SFLOAT_S8_UINT)
    {
        aspect_mask = VK_IMAGE_ASPECT_DEPTH_BIT;
        if (pTextureInfo->format == VK_FORMAT_D16_UNORM_S8_UINT || pTextureInfo->format == VK_FORMAT_D24_UNORM_S8_UINT ||
            pTextureInfo->format == VK_FORMAT_D32_SFLOAT_S8_UINT)
            aspect_mask |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }

    pTextureInfo->image = (VkImage)vk_handle;
    pTextureInfo->width = (unsigned int)desc.Width;
    pTextureInfo->height = (unsigned int)desc.Height;
    pTextureInfo->subresourceRange.aspectMask = aspect_mask;
    pTextureInfo->subresourceRange.baseMipLevel = 0;
    pTextureInfo->subresourceRange.levelCount = mip_level_count;
    pTextureInfo->subresourceRange.baseArrayLayer = 0;
    pTextureInfo->subresourceRange.layerCount = (desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D) ? 1 : desc.DepthOrArraySize;

    /* Create VkImageView */
    pTextureInfo->imageView = xess_d3d12_get_or_create_image_view(hContext, vk_device, pfn_vkCreateImageView,
        pTextureInfo->image, pTextureInfo->format, &desc, aspect_mask, mip_level_count);
    if (pTextureInfo->imageView == VK_NULL_HANDLE)
    {
        WARN("Failed to create %s texture image view\n", texture_name);
        return XESS_RESULT_ERROR_UNKNOWN;
    }

    TRACE("Finished %s texture handler.\n", texture_name);
    return XESS_RESULT_SUCCESS;
}

static BOOL xess_d3d12_create_state_tracker(xess_context_handle_t hContext, VkDevice vk_device)
{
    struct xess_d3d12_state_tracker *entry;
    PFN_vkCreatePipelineCache pfn_vkCreatePipelineCache;
    VkPipelineCacheCreateInfo create_info;
    VkResult vk_result;

    if ((entry = xess_d3d12_find_state_tracker(hContext)))
    {
        xess_d3d12_release_state_tracker(entry);
    }
    else
    {
        if (!(entry = calloc(1, sizeof(*entry))))
            return FALSE;

        entry->context = hContext;
        list_init(&entry->image_view_entries);
        list_add_tail(&xess_d3d12_state_trackers, &entry->entry);
    }

    entry->vk_device = vk_device;
    pfn_vkCreatePipelineCache = (PFN_vkCreatePipelineCache)vkGetDeviceProcAddr(vk_device, "vkCreatePipelineCache");
    entry->pfn_vkDestroyPipelineCache = (PFN_vkDestroyPipelineCache)vkGetDeviceProcAddr(vk_device, "vkDestroyPipelineCache");
    if (!pfn_vkCreatePipelineCache || !entry->pfn_vkDestroyPipelineCache)
    {
        WARN("Failed to get Vulkan pipeline cache function pointers for context %p.\n", hContext);
        return FALSE;
    }

    memset(&create_info, 0, sizeof(create_info));
    create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;

    vk_result = pfn_vkCreatePipelineCache(vk_device, &create_info, NULL, &entry->pipeline_cache);
    if (vk_result != VK_SUCCESS)
    {
        WARN("Failed to create VkPipelineCache for context %p: %d\n", hContext, vk_result);
        entry->pfn_vkDestroyPipelineCache = NULL;
        entry->vk_device = VK_NULL_HANDLE;
        return FALSE;
    }

    return TRUE;
}

static xess_result_t xess_d3d12_get_context_pipeline_cache(xess_context_handle_t hContext,
    VkPipelineCache *pipeline_cache)
{
    struct xess_d3d12_state_tracker *entry;

    if (!(entry = xess_d3d12_find_state_tracker(hContext)) || entry->pipeline_cache == VK_NULL_HANDLE)
    {
        WARN("No pipeline cache is tracked for context %p.\n", hContext);
        return XESS_RESULT_ERROR_INVALID_ARGUMENT;
    }

    *pipeline_cache = entry->pipeline_cache;
    return XESS_RESULT_SUCCESS;
}

static BOOL xess_d3d12_track_state_heaps(xess_context_handle_t hContext,
    ID3D12Heap *temp_buffer_heap, ID3D12Heap *temp_texture_heap,
    uint64_t buffer_heap_base_offset, uint64_t texture_heap_base_offset)
{
    struct xess_d3d12_state_tracker *entry;

    if (temp_buffer_heap)
        ID3D12Heap_AddRef(temp_buffer_heap);
    if (temp_texture_heap)
        ID3D12Heap_AddRef(temp_texture_heap);

    entry = xess_d3d12_find_state_tracker(hContext);
    if (!entry)
    {
        if (!(entry = calloc(1, sizeof(*entry))))
        {
            if (temp_texture_heap)
                ID3D12Heap_Release(temp_texture_heap);
            if (temp_buffer_heap)
                ID3D12Heap_Release(temp_buffer_heap);
            return FALSE;
        }

        entry->context = hContext;
        list_init(&entry->image_view_entries);
        list_add_tail(&xess_d3d12_state_trackers, &entry->entry);
    }
    else
    {
        if (entry->temp_texture_heap)
            ID3D12Heap_Release(entry->temp_texture_heap);
        if (entry->temp_buffer_heap)
            ID3D12Heap_Release(entry->temp_buffer_heap);
    }

    entry->temp_buffer_heap = temp_buffer_heap;
    entry->temp_texture_heap = temp_texture_heap;
    entry->buffer_heap_base_offset = buffer_heap_base_offset;
    entry->texture_heap_base_offset = texture_heap_base_offset;
    return TRUE;
}

static void xess_d3d12_get_state_tracker_heaps(xess_context_handle_t hContext,
    ID3D12Heap **temp_buffer_heap, ID3D12Heap **temp_texture_heap,
    uint64_t *buffer_heap_base_offset, uint64_t *texture_heap_base_offset)
{
    struct xess_d3d12_state_tracker *entry;

    *temp_buffer_heap = NULL;
    *temp_texture_heap = NULL;
    *buffer_heap_base_offset = 0;
    *texture_heap_base_offset = 0;

    if ((entry = xess_d3d12_find_state_tracker(hContext)))
    {
        *temp_buffer_heap = entry->temp_buffer_heap;
        *temp_texture_heap = entry->temp_texture_heap;
        *buffer_heap_base_offset = entry->buffer_heap_base_offset;
        *texture_heap_base_offset = entry->texture_heap_base_offset;
    }
}

void xess_d3d12_destroy_state_tracker(xess_context_handle_t hContext)
{
    struct xess_d3d12_state_tracker *entry;

    if ((entry = xess_d3d12_find_state_tracker(hContext)))
    {
        list_remove(&entry->entry);
        xess_d3d12_release_state_tracker(entry);
        free(entry);
    }
}

void xess_d3d12_destroy_all_state_trackers(void)
{
    struct xess_d3d12_state_tracker *entry, *next;

    LIST_FOR_EACH_ENTRY_SAFE(entry, next, &xess_d3d12_state_trackers, struct xess_d3d12_state_tracker, entry)
    {
        list_remove(&entry->entry);
        xess_d3d12_release_state_tracker(entry);
        free(entry);
    }
}

xess_result_t CDECL xessD3D12CreateContext(ID3D12Device *pDevice, xess_context_handle_t *phContext)
{
    VkDevice vk_device;
    VkPhysicalDevice vk_physical_device;
    VkInstance vk_instance;
    ID3D12DXVKInteropDevice3 *interop = NULL;
    struct xess_vk_create_context_params unix_params;
    HRESULT hr;
    NTSTATUS status;

    TRACE("(%p, %p)\n", pDevice, phContext);

    if (!pDevice || !phContext)
        return XESS_RESULT_ERROR_INVALID_ARGUMENT;

    hr = ID3D12Device_QueryInterface(pDevice, &IID_ID3D12DXVKInteropDevice3, (void**)&interop);
    if (FAILED(hr))
    {
        ERR("Failed to query interface: %#lx\n", hr);
        return XESS_RESULT_ERROR_DEVICE;
    }

    hr = ID3D12DXVKInteropDevice3_GetVulkanHandles(interop, &vk_instance, &vk_physical_device, &vk_device);
    ID3D12DXVKInteropDevice3_Release(interop);
    if (FAILED(hr))
    {
        ERR("GetVulkanHandles failed: %#lx\n", hr);
        return XESS_RESULT_ERROR_DEVICE;
    }

    TRACE("vk_instance: %p, vk_physical_device: %p, vk_device: %p\n",
            vk_instance, vk_physical_device, vk_device);

    memset(&unix_params, 0, sizeof(unix_params));
    unix_params.instance = vk_instance;
    unix_params.physicalDevice = vk_physical_device;
    unix_params.device = vk_device;
    unix_params.phContext = phContext;
    unix_params.result = XESS_RESULT_ERROR_CANT_LOAD_LIBRARY;

    status = WINE_UNIX_CALL(unix_xessVKCreateContext, &unix_params);
    if (status)
    {
        ERR("Unix call unix_xessVKCreateContext failed, status %#lx\n", status);
        return XESS_RESULT_ERROR_CANT_LOAD_LIBRARY;
    }

    if (unix_params.result == XESS_RESULT_SUCCESS && !xess_d3d12_create_state_tracker(*phContext, vk_device))
    {
        ERR("Failed to initialize state tracker for context %p\n", *phContext);
        xessDestroyContext(*phContext);
        *phContext = NULL;
        return XESS_RESULT_ERROR_UNKNOWN;
    }

    TRACE("xessVKCreateContext result: %s (0x%x)\n", xess_result_to_string(unix_params.result), unix_params.result);
    return unix_params.result;
}

xess_result_t CDECL xessD3D12BuildPipelines(xess_context_handle_t hContext,
    ID3D12PipelineLibrary *pPipelineLibrary, bool blocking, uint32_t initFlags)
{
    struct xess_vk_build_pipelines_params unix_params;
    xess_result_t result;
    NTSTATUS status;

    TRACE("(%p, %p, %u, 0x%x)\n", hContext, pPipelineLibrary, blocking, initFlags);

    if (pPipelineLibrary)
        WARN("Ignoring ID3D12PipelineLibrary %p for context %p.\n", pPipelineLibrary, hContext);

    result = xess_d3d12_get_context_pipeline_cache(hContext, &unix_params.pipelineCache);
    if (result != XESS_RESULT_SUCCESS)
        return result;

    unix_params.hContext = hContext;
    unix_params.blocking = blocking;
    unix_params.initFlags = initFlags;
    unix_params.result = XESS_RESULT_ERROR_CANT_LOAD_LIBRARY;
    status = WINE_UNIX_CALL(unix_xessVKBuildPipelines, &unix_params);
    if (status)
    {
        ERR("Unix call unix_xessVKBuildPipelines failed, status %#lx\n", status);
        return XESS_RESULT_ERROR_CANT_LOAD_LIBRARY;
    }

    TRACE("xessVKBuildPipelines result: %s (0x%x)\n", xess_result_to_string(unix_params.result), unix_params.result);
    return unix_params.result;
}

xess_result_t CDECL xessD3D12Init(xess_context_handle_t hContext, const xess_d3d12_init_params_t *pInitParams)
{
    xess_result_t result;
    xess_vk_init_params_t vk_init_params;
    struct xess_vk_init_params unix_params;
    uint64_t buffer_heap_base_offset = 0;
    uint64_t texture_heap_base_offset = 0;
    NTSTATUS status;

    TRACE("(%p, %p)\n", hContext, pInitParams);

    if (!pInitParams)
        return XESS_RESULT_ERROR_INVALID_ARGUMENT;

    memset(&vk_init_params, 0, sizeof(vk_init_params));
    vk_init_params.outputResolution.x = pInitParams->outputResolution.x;
    vk_init_params.outputResolution.y = pInitParams->outputResolution.y;
    vk_init_params.qualitySetting = pInitParams->qualitySetting;
    vk_init_params.initFlags = pInitParams->initFlags;
    vk_init_params.creationNodeMask = pInitParams->creationNodeMask;
    vk_init_params.visibleNodeMask = pInitParams->visibleNodeMask;

    result = translate_heap_to_vk_memory(pInitParams->pTempBufferHeap,
        &vk_init_params.tempBufferHeap, &buffer_heap_base_offset);
    if (result != XESS_RESULT_SUCCESS)
        return result;

    result = translate_heap_to_vk_memory(pInitParams->pTempTextureHeap,
        &vk_init_params.tempTextureHeap, &texture_heap_base_offset);
    if (result != XESS_RESULT_SUCCESS)
        return result;

    if (vk_init_params.tempBufferHeap != VK_NULL_HANDLE)
        vk_init_params.bufferHeapOffset = pInitParams->bufferHeapOffset + buffer_heap_base_offset;
    else
        vk_init_params.bufferHeapOffset = 0;

    if (vk_init_params.tempTextureHeap != VK_NULL_HANDLE)
        vk_init_params.textureHeapOffset = pInitParams->textureHeapOffset + texture_heap_base_offset;
    else
        vk_init_params.textureHeapOffset = 0;

    result = xess_d3d12_get_context_pipeline_cache(hContext, &vk_init_params.pipelineCache);
    if (result != XESS_RESULT_SUCCESS)
        return result;

    memset(&unix_params, 0, sizeof(unix_params));
    unix_params.hContext = hContext;
    unix_params.pInitParams = &vk_init_params;
    unix_params.result = XESS_RESULT_ERROR_CANT_LOAD_LIBRARY;
    status = WINE_UNIX_CALL(unix_xessVKInit, &unix_params);
    if (status)
    {
        ERR("Unix call unix_xessVKInit failed, status %#lx\n", status);
        return XESS_RESULT_ERROR_CANT_LOAD_LIBRARY;
    }

    if (unix_params.result == XESS_RESULT_SUCCESS)
    {
        if (!xess_d3d12_track_state_heaps(hContext, pInitParams->pTempBufferHeap, pInitParams->pTempTextureHeap,
            buffer_heap_base_offset, texture_heap_base_offset))
        {
            ERR("Failed to track heaps for context %p\n", hContext);
            xessDestroyContext(hContext);
            return XESS_RESULT_ERROR_UNKNOWN;
        }
    }

    TRACE("xessVKInit result: %s (0x%x)\n", xess_result_to_string(unix_params.result), unix_params.result);
    return unix_params.result;
}

xess_result_t CDECL xessD3D12GetInitParams(xess_context_handle_t hContext, xess_d3d12_init_params_t *pInitParams)
{
    xess_vk_init_params_t vk_init_params;
    struct xess_vk_get_init_params_params unix_params;
    uint64_t buffer_heap_base_offset = 0;
    uint64_t texture_heap_base_offset = 0;
    NTSTATUS status;

    TRACE("(%p, %p)\n", hContext, pInitParams);

    if (!pInitParams)
        return XESS_RESULT_ERROR_INVALID_ARGUMENT;

    memset(&vk_init_params, 0, sizeof(vk_init_params));

    unix_params.hContext = hContext;
    unix_params.pInitParams = &vk_init_params;
    unix_params.result = XESS_RESULT_ERROR_CANT_LOAD_LIBRARY;
    status = WINE_UNIX_CALL(unix_xessVKGetInitParams, &unix_params);
    if (status)
    {
        ERR("Unix call unix_xessVKGetInitParams failed, status %#lx\n", status);
        return XESS_RESULT_ERROR_CANT_LOAD_LIBRARY;
    }
    if (unix_params.result != XESS_RESULT_SUCCESS) {
        TRACE("xessVKGetInitParams result: %s (0x%x)\n", xess_result_to_string(unix_params.result), unix_params.result);
        return unix_params.result;
    }

    memset(pInitParams, 0, sizeof(*pInitParams));
    pInitParams->outputResolution = vk_init_params.outputResolution;
    pInitParams->qualitySetting = vk_init_params.qualitySetting;
    pInitParams->initFlags = vk_init_params.initFlags;
    pInitParams->creationNodeMask = vk_init_params.creationNodeMask;
    pInitParams->visibleNodeMask = vk_init_params.visibleNodeMask;
    xess_d3d12_get_state_tracker_heaps(hContext, &pInitParams->pTempBufferHeap, &pInitParams->pTempTextureHeap,
        &buffer_heap_base_offset, &texture_heap_base_offset);

    if (vk_init_params.bufferHeapOffset >= buffer_heap_base_offset)
        pInitParams->bufferHeapOffset = vk_init_params.bufferHeapOffset - buffer_heap_base_offset;
    else
    {
        WARN("Vulkan buffer heap offset %#I64x is smaller than base %#I64x for context %p\n",
            vk_init_params.bufferHeapOffset, buffer_heap_base_offset, hContext);
        pInitParams->bufferHeapOffset = 0;
    }

    if (vk_init_params.textureHeapOffset >= texture_heap_base_offset)
        pInitParams->textureHeapOffset = vk_init_params.textureHeapOffset - texture_heap_base_offset;
    else
    {
        WARN("Vulkan texture heap offset %#I64x is smaller than base %#I64x for context %p\n",
            vk_init_params.textureHeapOffset, texture_heap_base_offset, hContext);
        pInitParams->textureHeapOffset = 0;
    }

    // pipeline libraries are optional and hard to translate
    // we use an internal VkPipelineCache instead.
    pInitParams->pPipelineLibrary = NULL;

    return XESS_RESULT_SUCCESS;
}

xess_result_t CDECL xessD3D12Execute(xess_context_handle_t hContext,
    ID3D12GraphicsCommandList *pCommandList, const xess_d3d12_execute_params_t *pExecParams)
{
    xess_result_t result;
    ID3D12Device *pDevice = NULL;
    ID3D12DXVKInteropDevice3 *interop = NULL;
    VkCommandBuffer vk_command_buffer = VK_NULL_HANDLE;
    xess_vk_execute_params_t vk_exec_params;
    struct xess_vk_execute_params unix_params;
    HRESULT hr;
    VkDevice vk_device = VK_NULL_HANDLE;
    VkPhysicalDevice vk_physical_device = VK_NULL_HANDLE;
    VkInstance vk_instance = VK_NULL_HANDLE;
    PFN_vkCreateImageView pfn_vkCreateImageView = NULL;
    NTSTATUS status;

    TRACE("(%p, %p, %p)\n", hContext, pCommandList, pExecParams);

    if (!pCommandList || !pExecParams || !pExecParams->pColorTexture || !pExecParams->pVelocityTexture || !pExecParams->pOutputTexture)
        return XESS_RESULT_ERROR_INVALID_ARGUMENT;

    // no support for external descriptor heaps at this time
    if (pExecParams->pDescriptorHeap || pExecParams->descriptorHeapOffset)
    {
        WARN("External descriptor heap parameters are not supported.\n");
        return XESS_RESULT_ERROR_NOT_IMPLEMENTED;
    }

    memset(&vk_exec_params, 0, sizeof(vk_exec_params));
    memset(&unix_params, 0, sizeof(unix_params));

    /* Get the D3D12 device from the command list */
    hr = ID3D12GraphicsCommandList_GetDevice(pCommandList, &IID_ID3D12Device, (void**)&pDevice);
    if (FAILED(hr))
    {
        WARN("Failed to get ID3D12Device from command list: %#lx\n", hr);
        return XESS_RESULT_ERROR_INVALID_ARGUMENT;
    }

    /* Get the interop interface */
    hr = ID3D12Device_QueryInterface(pDevice, &IID_ID3D12DXVKInteropDevice3, (void**)&interop);
    if (FAILED(hr))
    {
        WARN("Failed to get ID3D12DXVKInteropDevice interface: %#lx\n", hr);
        ID3D12Device_Release(pDevice);
        return XESS_RESULT_ERROR_UNSUPPORTED;
    }

    TRACE("Got DXVK interop\n");

    /* Get Vulkan handles for creating image views */
    hr = ID3D12DXVKInteropDevice3_GetVulkanHandles(interop, &vk_instance, &vk_physical_device, &vk_device);
    if (FAILED(hr))
    {
        WARN("Failed to get Vulkan handles: %#lx\n", hr);
        ID3D12DXVKInteropDevice3_Release(interop);
        ID3D12Device_Release(pDevice);
        return XESS_RESULT_ERROR_UNSUPPORTED;
    }

    TRACE("Got Vulkan handles vk_instance=%p, vk_physical_device=%p, vk_device=%p\n", vk_instance, vk_physical_device, vk_device);

    /* Get Vulkan function pointers */
    pfn_vkCreateImageView = (PFN_vkCreateImageView)vkGetDeviceProcAddr(vk_device, "vkCreateImageView");
    if (!pfn_vkCreateImageView)
    {
        WARN("Failed to get Vulkan function pointers\n");
        ID3D12DXVKInteropDevice3_Release(interop);
        ID3D12Device_Release(pDevice);
        return XESS_RESULT_ERROR_UNSUPPORTED;
    }
    xess_d3d12_set_image_view_destroy_proc(hContext,
        (PFN_vkDestroyImageView)vkGetDeviceProcAddr(vk_device, "vkDestroyImageView"));

    TRACE("Got Vulkan function pointer pfn_vkCreateImageView=%p\n", pfn_vkCreateImageView);

    /* Get Vulkan command buffer from D3D12 command list */
    hr = ID3D12DXVKInteropDevice3_BeginVkCommandBufferInterop(interop, (ID3D12CommandList*)pCommandList, &vk_command_buffer);
    if (FAILED(hr))
    {
        WARN("Failed to get VkCommandBuffer: %#lx\n", hr);
        ID3D12DXVKInteropDevice3_Release(interop);
        ID3D12Device_Release(pDevice);
        return XESS_RESULT_ERROR_UNKNOWN;
    }

    TRACE("Got Vulkan interop command buffer vk_command_buffer=%p\n", vk_command_buffer);

    /* Get color texture VkImage and format */
    if (pExecParams->pColorTexture)
    {
        result = translate_texture_resource(hContext, interop, vk_device, pfn_vkCreateImageView,
            pExecParams->pColorTexture, &vk_exec_params.colorTexture, "color");
        if (result != XESS_RESULT_SUCCESS)
            goto cleanup;
    }

    /* Get velocity texture if provided */
    if (pExecParams->pVelocityTexture)
    {
        result = translate_texture_resource(hContext, interop, vk_device, pfn_vkCreateImageView,
            pExecParams->pVelocityTexture, &vk_exec_params.velocityTexture, "velocity");
        if (result != XESS_RESULT_SUCCESS)
            goto cleanup;
    }

    /* Get depth texture if provided */
    if (pExecParams->pDepthTexture)
    {
        result = translate_texture_resource(hContext, interop, vk_device, pfn_vkCreateImageView,
            pExecParams->pDepthTexture, &vk_exec_params.depthTexture, "depth");
        if (result != XESS_RESULT_SUCCESS)
            goto cleanup;
    }

    /* Get exposure scale texture if provided */
    if (pExecParams->pExposureScaleTexture)
    {
        result = translate_texture_resource(hContext, interop, vk_device, pfn_vkCreateImageView,
            pExecParams->pExposureScaleTexture, &vk_exec_params.exposureScaleTexture, "exposure scale");
        if (result != XESS_RESULT_SUCCESS)
            goto cleanup;
    }

    /* Get responsive pixel mask texture if provided */
    if (pExecParams->pResponsivePixelMaskTexture)
    {
        result = translate_texture_resource(hContext, interop, vk_device, pfn_vkCreateImageView,
            pExecParams->pResponsivePixelMaskTexture, &vk_exec_params.responsivePixelMaskTexture, "responsive pixel mask");
        if (result != XESS_RESULT_SUCCESS)
            goto cleanup;
    }

    /* Get output texture VkImage and format */
    if (pExecParams->pOutputTexture)
    {
        result = translate_texture_resource(hContext, interop, vk_device, pfn_vkCreateImageView,
            pExecParams->pOutputTexture, &vk_exec_params.outputTexture, "output");
        if (result != XESS_RESULT_SUCCESS)
            goto cleanup;
    }

    /* Copy execution parameters */
    vk_exec_params.jitterOffsetX = pExecParams->jitterOffsetX;
    vk_exec_params.jitterOffsetY = pExecParams->jitterOffsetY;
    vk_exec_params.exposureScale = pExecParams->exposureScale;
    vk_exec_params.resetHistory = pExecParams->resetHistory;
    vk_exec_params.inputWidth = pExecParams->inputWidth;
    vk_exec_params.inputHeight = pExecParams->inputHeight;
    vk_exec_params.inputColorBase = pExecParams->inputColorBase;
    vk_exec_params.inputMotionVectorBase = pExecParams->inputMotionVectorBase;
    vk_exec_params.inputDepthBase = pExecParams->inputDepthBase;
    vk_exec_params.inputResponsiveMaskBase = pExecParams->inputResponsiveMaskBase;
    vk_exec_params.reserved0 = pExecParams->reserved0;
    vk_exec_params.outputColorBase = pExecParams->outputColorBase;

    /* Execute XeSS */
    unix_params.hContext = hContext;
    unix_params.pCommandBuffer = vk_command_buffer;
    unix_params.pExecParams = &vk_exec_params;
    unix_params.result = XESS_RESULT_ERROR_CANT_LOAD_LIBRARY;
    TRACE("Executing XeSS call...\n");
    status = WINE_UNIX_CALL(unix_xessVKExecute, &unix_params);
    if (status)
    {
        ERR("Unix call unix_xessVKExecute failed, status %#lx\n", status);
        result = XESS_RESULT_ERROR_CANT_LOAD_LIBRARY;
        goto cleanup;
    }
    result = unix_params.result;

cleanup:
    /* End Vulkan command buffer interop */
    if (vk_command_buffer != VK_NULL_HANDLE)
        ID3D12DXVKInteropDevice3_EndVkCommandBufferInterop(interop, (ID3D12CommandList*)pCommandList);

    ID3D12DXVKInteropDevice3_Release(interop);
    ID3D12Device_Release(pDevice);
    TRACE("xessVKExecute result: %s (0x%x)\n", xess_result_to_string(result), result);
    return result;
}
