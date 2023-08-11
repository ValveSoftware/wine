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

#define WINE_VK_HOST
#define VK_NO_PROTOTYPES

#include <pthread.h>
#include <stdbool.h>

#include "vulkan_loader.h"
#include "vulkan_thunks.h"

/* Some extensions have callbacks for those we need to be able to
 * get the wine wrapper for a host handle
 */
struct wine_vk_mapping
{
    struct list link;
    uint64_t host_handle;
    uint64_t wine_wrapped_handle;
};

struct wine_cmd_buffer
{
    struct wine_device *device; /* parent */

    VkCommandBuffer handle; /* client command buffer */
    VkCommandBuffer host_command_buffer;

    struct wine_vk_mapping mapping;
};

static inline struct wine_cmd_buffer *wine_cmd_buffer_from_handle(VkCommandBuffer handle)
{
    return (struct wine_cmd_buffer *)(uintptr_t)handle->base.unix_handle;
}

struct wine_semaphore;

struct local_timeline_semaphore
{
    VkSemaphore sem;
    uint64_t value;
};

struct pending_d3d12_fence_op
{
    /* Vulkan native local semaphore. */
    struct local_timeline_semaphore local_sem;

    /* Operation values. */
    struct wine_vk_mapping mapping;
    struct list entry;
    uint64_t virtual_value;
    uint64_t shared_physical_value;
};

struct wine_device
{
    struct vulkan_device_funcs funcs;
    struct wine_phys_dev *phys_dev; /* parent */

    VkDevice handle; /* client device */
    VkDevice host_device;

    struct wine_queue *queues;
    uint32_t queue_count;

    VkQueueFamilyProperties *queue_props;

    struct wine_vk_mapping mapping;

    pthread_t signaller_thread;
    pthread_mutex_t signaller_mutex;
    bool stop;
    struct list free_fence_ops_list;
    struct list sem_poll_list;
    struct local_timeline_semaphore sem_poll_update;
    pthread_cond_t sem_poll_updated_cond;
    uint64_t sem_poll_update_value; /* set to sem_poll_update.value by signaller thread once update is processed. */
    unsigned int allocated_fence_ops_count;
    BOOL keyed_mutexes_enabled;
};

static inline struct wine_device *wine_device_from_handle(VkDevice handle)
{
    return (struct wine_device *)(uintptr_t)handle->base.unix_handle;
}

struct fs_hack_image
{
    uint32_t cmd_queue_idx;
    VkCommandBuffer cmd;
    VkImage swapchain_image;
    VkImage user_image;
    VkSemaphore blit_finished;
    VkImageView user_view, blit_view;
    VkDescriptorSet descriptor_set;
};

struct wine_swapchain
{
    VkSwapchainKHR host_swapchain;

    /* fs hack data below */
    BOOL fs_hack_enabled;
    VkExtent2D user_extent;
    VkExtent2D real_extent;
    VkRect2D blit_dst;
    VkCommandPool *cmd_pools; /* VkCommandPool[device->queue_count] */
    VkDeviceMemory user_image_memory;
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

static inline struct wine_swapchain *wine_swapchain_from_handle(VkSwapchainKHR handle)
{
    return (struct wine_swapchain *)(uintptr_t)handle;
}

static inline VkSwapchainKHR wine_swapchain_to_handle(struct wine_swapchain *swapchain)
{
    return (VkSwapchainKHR)(uintptr_t)swapchain;
}

struct wine_debug_utils_messenger;

struct wine_debug_report_callback
{
    struct wine_instance *instance; /* parent */
    VkDebugReportCallbackEXT host_debug_callback;

    /* application callback + data */
    PFN_vkDebugReportCallbackEXT user_callback;
    void *user_data;

    struct wine_vk_mapping mapping;
};

struct wine_instance
{
    struct vulkan_instance_funcs funcs;

    VkInstance handle; /* client instance */
    VkInstance host_instance;

    /* We cache devices as we need to wrap them as they are
     * dispatchable objects.
     */
    struct wine_phys_dev **phys_devs;
    uint32_t phys_dev_count;
    uint32_t api_version;

    VkBool32 enable_wrapper_list;
    struct list wrappers;
    pthread_rwlock_t wrapper_lock;

    struct wine_debug_utils_messenger *utils_messengers;
    uint32_t utils_messenger_count;

    struct wine_debug_report_callback default_callback;

    unsigned int quirks;

    struct wine_vk_mapping mapping;
};

static inline struct wine_instance *wine_instance_from_handle(VkInstance handle)
{
    return (struct wine_instance *)(uintptr_t)handle->base.unix_handle;
}

struct wine_phys_dev
{
    struct wine_instance *instance; /* parent */

    VkPhysicalDevice handle; /* client physical device */
    VkPhysicalDevice host_physical_device;

    VkPhysicalDeviceMemoryProperties memory_properties;
    VkExtensionProperties *extensions;
    uint32_t extension_count;
    uint32_t api_version;

    uint32_t external_memory_align;

    struct wine_vk_mapping mapping;
};

static inline struct wine_phys_dev *wine_phys_dev_from_handle(VkPhysicalDevice handle)
{
    return (struct wine_phys_dev *)(uintptr_t)handle->base.unix_handle;
}

struct wine_queue
{
    struct wine_device *device; /* parent */

    VkQueue handle; /* client queue */
    VkQueue host_queue;

    uint32_t family_index;
    uint32_t queue_index;
    VkDeviceQueueCreateFlags flags;

    struct wine_vk_mapping mapping;
};

static inline struct wine_queue *wine_queue_from_handle(VkQueue handle)
{
    return (struct wine_queue *)(uintptr_t)handle->base.unix_handle;
}

struct wine_cmd_pool
{
    VkCommandPool handle;
    VkCommandPool host_command_pool;

    struct wine_vk_mapping mapping;
};

static inline struct wine_cmd_pool *wine_cmd_pool_from_handle(VkCommandPool handle)
{
    struct vk_command_pool *client_ptr = command_pool_from_handle(handle);
    return (struct wine_cmd_pool *)(uintptr_t)client_ptr->unix_handle;
}

struct keyed_mutex_shm
{
    pthread_mutex_t mutex;
    uint64_t instance_id_counter;
    uint64_t acquired_to_instance;
    uint64_t key;
    uint64_t timeline_value;
    uint64_t timeline_queued_release;
};

struct wine_device_memory
{
    VkDeviceMemory host_memory;
    VkExternalMemoryHandleTypeFlagBits handle_types;
    BOOL inherit;
    DWORD access;
    HANDLE handle;
    void *mapping;
    struct keyed_mutex_shm *keyed_mutex_shm;
    VkSemaphore keyed_mutex_sem;
    uint64_t keyed_mutex_instance_id;
};

static inline VkDeviceMemory wine_device_memory_to_handle(struct wine_device_memory *device_memory)
{
    return (VkDeviceMemory)(uintptr_t)device_memory;
}

static inline struct wine_device_memory *wine_device_memory_from_handle(VkDeviceMemory handle)
{
    return (struct wine_device_memory *)(uintptr_t)handle;
}

struct wine_debug_utils_messenger
{
    struct wine_instance *instance; /* parent */
    VkDebugUtilsMessengerEXT host_debug_messenger;

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
    VkSurfaceKHR host_surface;
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

BOOL wine_vk_device_extension_supported(const char *name);
BOOL wine_vk_instance_extension_supported(const char *name);

BOOL wine_vk_is_type_wrapped(VkObjectType type);

NTSTATUS init_vulkan(void *args);

NTSTATUS vk_is_available_instance_function(void *arg);
NTSTATUS vk_is_available_device_function(void *arg);
NTSTATUS vk_is_available_instance_function32(void *arg);
NTSTATUS vk_is_available_device_function32(void *arg);

struct conversion_context
{
    char buffer[2048];
    uint32_t used;
    struct list alloc_entries;
};

static inline void init_conversion_context(struct conversion_context *pool)
{
    pool->used = 0;
    list_init(&pool->alloc_entries);
}

static inline void free_conversion_context(struct conversion_context *pool)
{
    struct list *entry, *next;
    LIST_FOR_EACH_SAFE(entry, next, &pool->alloc_entries)
        free(entry);
}

struct wine_semaphore
{
    VkSemaphore semaphore;

    VkExternalSemaphoreHandleTypeFlagBits export_types;

    struct wine_vk_mapping mapping;

    /* mutable members */
    VkExternalSemaphoreHandleTypeFlagBits handle_type;
    struct list poll_entry;
    struct list pending_waits;
    struct list pending_signals;
    HANDLE handle;
    struct
    {
        /* Shared mem access mutex. The non-shared parts access is guarded with device global signaller_mutex. */
        pthread_mutex_t mutex;
        uint64_t virtual_value, physical_value;
        uint64_t last_reset_physical;
        uint64_t last_dropped_reset_physical;
        struct
        {
            uint64_t physical_at_reset;
            uint64_t virtual_before_reset;
        }
        reset_backlog[16];
        uint32_t reset_backlog_count;
    } *d3d12_fence_shm;
    /* The Vulkan shared semaphore is only waited or signaled in signaller_worker(). */
    VkSemaphore fence_timeline_semaphore;
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

static inline void *conversion_context_alloc(struct conversion_context *pool, size_t size)
{
    if (pool->used + size <= sizeof(pool->buffer))
    {
        void *ret = pool->buffer + pool->used;
        pool->used += (size + sizeof(UINT64) - 1) & ~(sizeof(UINT64) - 1);
        return ret;
    }
    else
    {
        struct list *entry;
        if (!(entry = malloc(sizeof(*entry) + size)))
            return NULL;
        list_add_tail(&pool->alloc_entries, entry);
        return entry + 1;
    }
}

struct wine_deferred_operation
{
    VkDeferredOperationKHR host_deferred_operation;
    struct conversion_context ctx; /* to keep params alive. */
    struct wine_vk_mapping mapping;
};

static inline struct wine_deferred_operation *wine_deferred_operation_from_handle(
        VkDeferredOperationKHR handle)
{
    return (struct wine_deferred_operation *)(uintptr_t)handle;
}

static inline VkDeferredOperationKHR wine_deferred_operation_to_handle(
        struct wine_deferred_operation *deferred_operation)
{
    return (VkDeferredOperationKHR)(uintptr_t)deferred_operation;
}

typedef UINT32 PTR32;

typedef struct
{
    VkStructureType sType;
    PTR32 pNext;
} VkBaseInStructure32;

typedef struct
{
    VkStructureType sType;
    PTR32 pNext;
} VkBaseOutStructure32;

static inline void *find_next_struct32(void *s, VkStructureType t)
{
    VkBaseOutStructure32 *header;

    for (header = s; header; header = UlongToPtr(header->pNext))
    {
        if (header->sType == t)
            return header;
    }

    return NULL;
}

static inline void *find_next_struct(const void *s, VkStructureType t)
{
    VkBaseOutStructure *header;

    for (header = (VkBaseOutStructure *)s; header; header = header->pNext)
    {
        if (header->sType == t)
            return header;
    }

    return NULL;
}

static inline void init_unicode_string( UNICODE_STRING *str, const WCHAR *data )
{
    str->Length = lstrlenW(data) * sizeof(WCHAR);
    str->MaximumLength = str->Length + sizeof(WCHAR);
    str->Buffer = (WCHAR *)data;
}

#define MEMDUP(ctx, dst, src, count) dst = conversion_context_alloc((ctx), sizeof(*(dst)) * (count)); \
    memcpy((void *)(dst), (src), sizeof(*(dst)) * (count));
#define MEMDUP_VOID(ctx, dst, src, size) dst = conversion_context_alloc((ctx), size); \
    memcpy((void *)(dst), (src), size);

#endif /* __WINE_VULKAN_PRIVATE_H */
