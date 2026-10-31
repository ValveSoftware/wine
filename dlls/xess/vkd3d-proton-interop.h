/*
 * XeSS DLL - vkd3d-proton Interop
 * Copyright 2025 the Wine project
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#ifndef __WINE_VKD3D_PROTON_INTEROP_H
#define __WINE_VKD3D_PROTON_INTEROP_H

#include <d3d12.h>
#ifdef __WINESRC__
#include "wine/vulkan.h"
#else
#include <vulkan/vulkan.h>
#endif

typedef interface ID3D12DXVKInteropDevice3 ID3D12DXVKInteropDevice3;

static const GUID IID_ID3D12DXVKInteropDevice3 = {0x22a70184, 0xa6a4, 0x4c24, {0xbf, 0x97, 0x7d, 0x6d, 0xf9, 0xf1, 0x2d, 0x8a}};

typedef struct ID3D12DXVKInteropDevice3Vtbl {
    BEGIN_INTERFACE

    /*** IUnknown methods ***/
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(
        ID3D12DXVKInteropDevice3 *This,
        REFIID riid,
        void **object);

    ULONG (STDMETHODCALLTYPE *AddRef)(
        ID3D12DXVKInteropDevice3 *This);

    ULONG (STDMETHODCALLTYPE *Release)(
        ID3D12DXVKInteropDevice3 *This);

    /*** ID3D12DXVKInteropDevice methods ***/
    HRESULT (STDMETHODCALLTYPE *GetDXGIAdapter)(
        ID3D12DXVKInteropDevice3 *This,
        REFIID iid,
        void **object);

    HRESULT (STDMETHODCALLTYPE *GetInstanceExtensions)(
        ID3D12DXVKInteropDevice3 *This,
        UINT *extension_count,
        const char **extensions);

    HRESULT (STDMETHODCALLTYPE *GetDeviceExtensions)(
        ID3D12DXVKInteropDevice3 *This,
        UINT *extension_count,
        const char **extensions);

    HRESULT (STDMETHODCALLTYPE *GetDeviceFeatures)(
        ID3D12DXVKInteropDevice3 *This,
        const VkPhysicalDeviceFeatures2 **features);

    HRESULT (STDMETHODCALLTYPE *GetVulkanHandles)(
        ID3D12DXVKInteropDevice3 *This,
        VkInstance *vk_instance,
        VkPhysicalDevice *vk_physical_device,
        VkDevice *vk_device);

    HRESULT (STDMETHODCALLTYPE *GetVulkanQueueInfo)(
        ID3D12DXVKInteropDevice3 *This,
        ID3D12CommandQueue *queue,
        VkQueue *vk_queue,
        UINT32 *vk_queue_family);

    void (STDMETHODCALLTYPE *GetVulkanImageLayout)(
        ID3D12DXVKInteropDevice3 *This,
        ID3D12Resource *resource,
        D3D12_RESOURCE_STATES state,
        VkImageLayout *vk_layout);

    HRESULT (STDMETHODCALLTYPE *GetVulkanResourceInfo)(
        ID3D12DXVKInteropDevice3 *This,
        ID3D12Resource *resource,
        UINT64 *vk_handle,
        UINT64 *buffer_offset);

    HRESULT (STDMETHODCALLTYPE *LockCommandQueue)(
        ID3D12DXVKInteropDevice3 *This,
        ID3D12CommandQueue *queue);

    HRESULT (STDMETHODCALLTYPE *UnlockCommandQueue)(
        ID3D12DXVKInteropDevice3 *This,
        ID3D12CommandQueue *queue);

    /*** ID3D12DXVKInteropDevice1 methods ***/
    HRESULT (STDMETHODCALLTYPE *GetVulkanResourceInfo1)(
        ID3D12DXVKInteropDevice3 *This,
        ID3D12Resource *resource,
        UINT64 *vk_handle,
        UINT64 *buffer_offset,
        VkFormat *format);

    HRESULT (STDMETHODCALLTYPE *CreateInteropCommandQueue)(
        ID3D12DXVKInteropDevice3 *This,
        const D3D12_COMMAND_QUEUE_DESC *pDesc,
        UINT32 vk_queue_family_index,
        ID3D12CommandQueue **ppQueue);

    HRESULT (STDMETHODCALLTYPE *CreateInteropCommandAllocator)(
        ID3D12DXVKInteropDevice3 *This,
        D3D12_COMMAND_LIST_TYPE type,
        UINT32 vk_queue_family_index,
        ID3D12CommandAllocator **ppAllocator);

    HRESULT (STDMETHODCALLTYPE *BeginVkCommandBufferInterop)(
        ID3D12DXVKInteropDevice3 *This,
        ID3D12CommandList *pCmdList,
        VkCommandBuffer *pCommandBuffer);

    HRESULT (STDMETHODCALLTYPE *EndVkCommandBufferInterop)(
        ID3D12DXVKInteropDevice3 *This,
        ID3D12CommandList *pCmdList);

    /*** ID3D12DXVKInteropDevice2 methods ***/
    HRESULT (STDMETHODCALLTYPE *LockVulkanQueue)(
        ID3D12DXVKInteropDevice3 *This,
        ID3D12CommandQueue *queue);

    HRESULT (STDMETHODCALLTYPE *UnlockVulkanQueue)(
        ID3D12DXVKInteropDevice3 *This,
        ID3D12CommandQueue *queue);

    /*** ID3D12DXVKInteropDevice3 methods ***/
    HRESULT (STDMETHODCALLTYPE *GetVulkanHeapInfo)(
        ID3D12DXVKInteropDevice3 *This,
        ID3D12Heap *heap,
        UINT64 *vk_memory,
        UINT64 *heap_offset,
        UINT32 *vk_memory_type);

    END_INTERFACE
} ID3D12DXVKInteropDevice3Vtbl;

interface ID3D12DXVKInteropDevice3 {
    CONST_VTBL ID3D12DXVKInteropDevice3Vtbl* lpVtbl;
};

/*** IUnknown methods ***/
#define ID3D12DXVKInteropDevice3_QueryInterface(This,riid,object) (This)->lpVtbl->QueryInterface(This,riid,object)
#define ID3D12DXVKInteropDevice3_AddRef(This) (This)->lpVtbl->AddRef(This)
#define ID3D12DXVKInteropDevice3_Release(This) (This)->lpVtbl->Release(This)
/*** ID3D12DXVKInteropDevice methods ***/
#define ID3D12DXVKInteropDevice3_GetDXGIAdapter(This,iid,object) (This)->lpVtbl->GetDXGIAdapter(This,iid,object)
#define ID3D12DXVKInteropDevice3_GetInstanceExtensions(This,extension_count,extensions) (This)->lpVtbl->GetInstanceExtensions(This,extension_count,extensions)
#define ID3D12DXVKInteropDevice3_GetDeviceExtensions(This,extension_count,extensions) (This)->lpVtbl->GetDeviceExtensions(This,extension_count,extensions)
#define ID3D12DXVKInteropDevice3_GetDeviceFeatures(This,features) (This)->lpVtbl->GetDeviceFeatures(This,features)
#define ID3D12DXVKInteropDevice3_GetVulkanHandles(This,vk_instance,vk_physical_device,vk_device) (This)->lpVtbl->GetVulkanHandles(This,vk_instance,vk_physical_device,vk_device)
#define ID3D12DXVKInteropDevice3_GetVulkanQueueInfo(This,queue,vk_queue,vk_queue_family) (This)->lpVtbl->GetVulkanQueueInfo(This,queue,vk_queue,vk_queue_family)
#define ID3D12DXVKInteropDevice3_GetVulkanImageLayout(This,resource,state,vk_layout) (This)->lpVtbl->GetVulkanImageLayout(This,resource,state,vk_layout)
#define ID3D12DXVKInteropDevice3_GetVulkanResourceInfo(This,resource,vk_handle,buffer_offset) (This)->lpVtbl->GetVulkanResourceInfo(This,resource,vk_handle,buffer_offset)
#define ID3D12DXVKInteropDevice3_LockCommandQueue(This,queue) (This)->lpVtbl->LockCommandQueue(This,queue)
#define ID3D12DXVKInteropDevice3_UnlockCommandQueue(This,queue) (This)->lpVtbl->UnlockCommandQueue(This,queue)
/*** ID3D12DXVKInteropDevice1 methods ***/
#define ID3D12DXVKInteropDevice3_GetVulkanResourceInfo1(This,resource,vk_handle,buffer_offset,format) (This)->lpVtbl->GetVulkanResourceInfo1(This,resource,vk_handle,buffer_offset,format)
#define ID3D12DXVKInteropDevice3_CreateInteropCommandQueue(This,pDesc,vk_queue_family_index,ppQueue) (This)->lpVtbl->CreateInteropCommandQueue(This,pDesc,vk_queue_family_index,ppQueue)
#define ID3D12DXVKInteropDevice3_CreateInteropCommandAllocator(This,type,vk_queue_family_index,ppAllocator) (This)->lpVtbl->CreateInteropCommandAllocator(This,type,vk_queue_family_index,ppAllocator)
#define ID3D12DXVKInteropDevice3_BeginVkCommandBufferInterop(This,pCmdList,pCommandBuffer) (This)->lpVtbl->BeginVkCommandBufferInterop(This,pCmdList,pCommandBuffer)
#define ID3D12DXVKInteropDevice3_EndVkCommandBufferInterop(This,pCmdList) (This)->lpVtbl->EndVkCommandBufferInterop(This,pCmdList)

/*** ID3D12DXVKInteropDevice2 methods ***/
#define ID3D12DXVKInteropDevice3_LockVulkanQueue(This,queue) (This)->lpVtbl->LockVulkanQueue(This,queue)
#define ID3D12DXVKInteropDevice3_UnlockVulkanQueue(This,queue) (This)->lpVtbl->UnlockVulkanQueue(This,queue)

/*** ID3D12DXVKInteropDevice3 methods ***/
#define ID3D12DXVKInteropDevice3_GetVulkanHeapInfo(This,heap,vk_memory,heap_offset,vk_memory_type) (This)->lpVtbl->GetVulkanHeapInfo(This,heap,vk_memory,heap_offset,vk_memory_type)

#endif /* __WINE_VKD3D_PROTON_INTEROP_H */