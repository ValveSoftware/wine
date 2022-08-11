/* Wine Vulkan ICD private data structures
 *
 * Copyright 2017 Roderick Colenbrander
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

#ifndef __WINE_VULKAN_PRIVATE_H
#define __WINE_VULKAN_PRIVATE_H

/* Perform vulkan struct conversion on 32-bit x86 platforms. */
#if defined(__i386__)
#define USE_STRUCT_CONVERSION
#endif
#define VK_NO_PROTOTYPES

#include <pthread.h>
#include <stdbool.h>

#include "wine/list.h"

#include "vulkan_loader.h"
#include "vulkan_thunks.h"

/* Some extensions have callbacks for those we need to be able to
 * get the wine wrapper for a native handle
 */
struct wine_vk_mapping
{
    struct list link;
    uint64_t native_handle;
    uint64_t wine_wrapped_handle;
};

struct VkCommandBuffer_T
{
    struct wine_vk_base base;
    struct VkDevice_T *device; /* parent */
    VkCommandBuffer command_buffer; /* native command buffer */

    struct list pool_link;
    struct wine_vk_mapping mapping;
};

struct VkDevice_T
{
    struct wine_vk_device_base base;
    struct vulkan_device_funcs funcs;
    struct VkPhysicalDevice_T *phys_dev; /* parent */
    VkDevice device; /* native device */

    struct VkQueue_T* queues;
    uint32_t queue_count;

    VkQueueFamilyProperties *queue_props;

    struct wine_vk_mapping mapping;
};

struct fs_hack_image
{
    uint32_t cmd_queue_idx;
    VkCommandBuffer cmd;
    VkImage swapchain_image;
    VkImage blit_image;
    VkImage user_image;
    VkSemaphore blit_finished;
    VkImageView user_view, blit_view;
    VkDescriptorSet descriptor_set;
};

struct VkSwapchainKHR_T
{
    VkSwapchainKHR swapchain; /* native swapchain */

    /* fs hack data below */
    BOOL fs_hack_enabled;
    VkExtent2D user_extent;
    VkExtent2D real_extent;
    VkImageUsageFlags surface_usage;
    VkRect2D blit_dst;
    VkCommandPool *cmd_pools; /* VkCommandPool[device->queue_count] */
    VkDeviceMemory user_image_memory, blit_image_memory;
    uint32_t n_images;
    struct fs_hack_image *fs_hack_images; /* struct fs_hack_image[n_images] */
    VkFilter fs_hack_filter;
    VkSampler sampler;
    VkDescriptorPool descriptor_pool;
    VkDescriptorSetLayout descriptor_set_layout;
    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline;

    struct wine_vk_mapping mapping;
};

struct wine_debug_utils_messenger;

struct wine_debug_report_callback
{
    struct VkInstance_T *instance; /* parent */
    VkDebugReportCallbackEXT debug_callback; /* native callback object */

    /* application callback + data */
    PFN_vkDebugReportCallbackEXT user_callback;
    void *user_data;

    struct wine_vk_mapping mapping;
};

struct VkInstance_T
{
    struct wine_vk_base base;
    struct vulkan_instance_funcs funcs;
    VkInstance instance; /* native instance */

    uint32_t api_version;

    /* We cache devices as we need to wrap them as they are
     * dispatchable objects.
     */
    struct VkPhysicalDevice_T **phys_devs;
    uint32_t phys_dev_count;

    VkBool32 enable_wrapper_list;
    struct list wrappers;
    pthread_rwlock_t wrapper_lock;

    struct wine_debug_utils_messenger *utils_messengers;
    uint32_t utils_messenger_count;

    struct wine_debug_report_callback default_callback;

    unsigned int quirks;

    struct wine_vk_mapping mapping;
};

struct VkPhysicalDevice_T
{
    struct wine_vk_base base;
    struct VkInstance_T *instance; /* parent */
    VkPhysicalDevice phys_dev; /* native physical device */

    uint32_t api_version;

    VkExtensionProperties *extensions;
    uint32_t extension_count;

    bool fake_memory_priority;

    struct wine_vk_mapping mapping;
};

struct VkQueue_T
{
    struct wine_vk_base base;
    struct VkDevice_T *device; /* parent */
    VkQueue queue; /* native queue */

    uint32_t family_index;
    uint32_t queue_index;
    VkDeviceQueueCreateFlags flags;

    bool virtual_queue;
    bool processing;
    bool device_lost;

    pthread_t virtual_queue_thread;
    pthread_mutex_t submissions_mutex;
    pthread_cond_t submissions_cond;
    struct list submissions;

    pthread_t signal_thread;
    pthread_mutex_t signaller_mutex;
    pthread_cond_t signaller_cond;
    struct list signal_ops;

    bool stop;

    struct wine_vk_mapping mapping;
};

struct wine_cmd_pool
{
    VkCommandPool command_pool;

    struct list command_buffers;

    struct wine_vk_mapping mapping;
};

static inline struct wine_cmd_pool *wine_cmd_pool_from_handle(VkCommandPool handle)
{
    return (struct wine_cmd_pool *)(uintptr_t)handle;
}

static inline VkCommandPool wine_cmd_pool_to_handle(struct wine_cmd_pool *cmd_pool)
{
    return (VkCommandPool)(uintptr_t)cmd_pool;
}

struct wine_debug_utils_messenger
{
    struct VkInstance_T *instance; /* parent */
    VkDebugUtilsMessengerEXT debug_messenger; /* native messenger */

    /* application callback + data */
    PFN_vkDebugUtilsMessengerCallbackEXT user_callback;
    void *user_data;

    struct wine_vk_mapping mapping;
};

static inline struct wine_debug_utils_messenger *wine_debug_utils_messenger_from_handle(
        VkDebugUtilsMessengerEXT handle)
{
    return (struct wine_debug_utils_messenger *)(uintptr_t)handle;
}

static inline VkDebugUtilsMessengerEXT wine_debug_utils_messenger_to_handle(
        struct wine_debug_utils_messenger *debug_messenger)
{
    return (VkDebugUtilsMessengerEXT)(uintptr_t)debug_messenger;
}

static inline struct wine_debug_report_callback *wine_debug_report_callback_from_handle(
        VkDebugReportCallbackEXT handle)
{
    return (struct wine_debug_report_callback *)(uintptr_t)handle;
}

static inline VkDebugReportCallbackEXT wine_debug_report_callback_to_handle(
        struct wine_debug_report_callback *debug_messenger)
{
    return (VkDebugReportCallbackEXT)(uintptr_t)debug_messenger;
}

struct wine_surface
{
    VkSurfaceKHR surface; /* native surface */
    VkSurfaceKHR driver_surface; /* wine driver surface */

    struct wine_vk_mapping mapping;
};

static inline struct wine_surface *wine_surface_from_handle(VkSurfaceKHR handle)
{
    return (struct wine_surface *)(uintptr_t)handle;
}

static inline VkSurfaceKHR wine_surface_to_handle(struct wine_surface *surface)
{
    return (VkSurfaceKHR)(uintptr_t)surface;
}

struct wine_dev_mem
{
    VkDeviceMemory dev_mem;

    VkExternalMemoryHandleTypeFlagBits handle_types;

    BOOL inherit;
    DWORD access;

    HANDLE handle;

    struct wine_vk_mapping mapping;
};

static inline struct wine_dev_mem *wine_dev_mem_from_handle(VkDeviceMemory handle)
{
    return (struct wine_dev_mem *)(uintptr_t)handle;
}

static inline VkDeviceMemory wine_dev_mem_to_handle(struct wine_dev_mem *dev_mem)
{
    return (VkDeviceMemory)(uintptr_t)dev_mem;
}

struct wine_semaphore
{
    VkSemaphore semaphore;
    VkSemaphore fence_timeline_semaphore;

    VkExternalSemaphoreHandleTypeFlagBits export_types;

    struct wine_vk_mapping mapping;

    /* mutable members */
    VkExternalSemaphoreHandleTypeFlagBits handle_type;
    HANDLE handle;
    struct
    {
        pthread_mutex_t mutex;
        uint64_t virtual_value, physical_value, counter;

        struct pending_wait
        {
            bool present, satisfied;
            uint64_t virtual_value;
            uint64_t physical_value;
            pthread_cond_t cond;
        } pending_waits[100];

        struct pending_update
        {
            uint64_t virtual_value;
            uint64_t physical_value;
            pid_t signalling_pid;
            struct VkQueue_T *signalling_queue;
        } pending_updates[100];
        uint32_t pending_updates_count;
    } *d3d12_fence_shm;
};

static inline struct wine_semaphore *wine_semaphore_from_handle(VkSemaphore handle)
{
    return (struct wine_semaphore *)(uintptr_t)handle;
}

static inline VkSemaphore wine_semaphore_to_handle(struct wine_semaphore *semaphore)
{
    return (VkSemaphore)(uintptr_t)semaphore;
}

static inline VkSemaphore wine_semaphore_host_handle(struct wine_semaphore *semaphore)
{
    if (semaphore->handle_type == VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D12_FENCE_BIT)
        return semaphore->fence_timeline_semaphore;
    return semaphore->semaphore;
}

struct wine_fence
{
    VkFence fence;

    struct VkQueue_T *queue;
    struct VkSwapchainKHR_T *swapchain;
    bool wait_assist;
    int eventfd;
};

static inline struct wine_fence *wine_fence_from_handle(VkFence handle)
{
    return (struct wine_fence *)(uintptr_t)handle;
}

static inline VkFence wine_fence_to_handle(struct wine_fence *fence)
{
    return (VkFence)(uintptr_t)fence;
}

BOOL wine_vk_device_extension_supported(const char *name) DECLSPEC_HIDDEN;
BOOL wine_vk_instance_extension_supported(const char *name) DECLSPEC_HIDDEN;

BOOL wine_vk_is_type_wrapped(VkObjectType type) DECLSPEC_HIDDEN;
uint64_t wine_vk_unwrap_handle(VkObjectType type, uint64_t handle) DECLSPEC_HIDDEN;

NTSTATUS init_vulkan(void *args) DECLSPEC_HIDDEN;

extern const struct unix_funcs loader_funcs;

BOOL WINAPI wine_vk_is_available_instance_function(VkInstance instance, const char *name) DECLSPEC_HIDDEN;
BOOL WINAPI wine_vk_is_available_device_function(VkDevice device, const char *name) DECLSPEC_HIDDEN;

extern VkDevice WINAPI __wine_get_native_VkDevice(VkDevice device) DECLSPEC_HIDDEN;
extern VkInstance WINAPI __wine_get_native_VkInstance(VkInstance instance) DECLSPEC_HIDDEN;
extern VkPhysicalDevice WINAPI __wine_get_native_VkPhysicalDevice(VkPhysicalDevice phys_dev) DECLSPEC_HIDDEN;
extern VkQueue WINAPI __wine_get_native_VkQueue(VkQueue queue) DECLSPEC_HIDDEN;
extern VkPhysicalDevice WINAPI __wine_get_wrapped_VkPhysicalDevice(VkInstance instance, VkPhysicalDevice native_phys_dev) DECLSPEC_HIDDEN;

extern VkResult WINAPI __wine_create_vk_instance_with_callback(const VkInstanceCreateInfo *create_info, const VkAllocationCallbacks *allocator,
        VkInstance *instance, PFN_native_vkCreateInstance callback, void *context) DECLSPEC_HIDDEN;
extern VkResult WINAPI __wine_create_vk_device_with_callback(VkPhysicalDevice phys_dev, const VkDeviceCreateInfo *create_info,
        const VkAllocationCallbacks *allocator, VkDevice *device, PFN_native_vkCreateDevice callback, void *context) DECLSPEC_HIDDEN;

static inline void init_unicode_string( UNICODE_STRING *str, const WCHAR *data )
{
    str->Length = lstrlenW(data) * sizeof(WCHAR);
    str->MaximumLength = str->Length + sizeof(WCHAR);
    str->Buffer = (WCHAR *)data;
}

static inline void *memdup(const void *in, size_t number, size_t size)
{
    void *out;

    if (!in)
        return NULL;

    out = malloc(number * size);
    memcpy(out, in, number * size);
    return out;
}

#endif /* __WINE_VULKAN_PRIVATE_H */
