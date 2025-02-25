/*
 * Copyright 2017-2018 Roderick Colenbrander
 * Copyright 2022 Jacek Caban for CodeWeavers
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
#pragma makedep install
#endif

#ifndef __WINE_VULKAN_DRIVER_H
#define __WINE_VULKAN_DRIVER_H

#include <stdarg.h>
#include <stddef.h>

#include <windef.h>
#include <winbase.h>

/* Base 'class' for our Vulkan dispatchable objects such as VkDevice and VkInstance.
 * This structure MUST be the first element of a dispatchable object as the ICD
 * loader depends on it. For now only contains loader_magic, but over time more common
 * functionality is expected.
 */
struct vulkan_client_object
{
    /* Special section in each dispatchable object for use by the ICD loader for
     * storing dispatch tables. The start contains a magical value '0x01CDC0DE'.
     */
    UINT64 loader_magic;
    UINT64 unix_handle;
};

#ifdef WINE_UNIX_LIB

#include "wine/vulkan.h"
#include "wine/rbtree.h"
#include "wine/list.h"

/* Wine internal vulkan driver version, needs to be bumped upon vulkan_funcs changes. */
#define WINE_VULKAN_DRIVER_VERSION 36

struct vulkan_object
{
    UINT64 host_handle;
    UINT64 client_handle;
    struct rb_entry entry;
};

#define VULKAN_OBJECT_HEADER( type, name ) \
    union { \
        struct vulkan_object obj; \
        struct { \
            union { type name; UINT64 handle; } host; \
            union { type name; UINT64 handle; } client; \
        }; \
    }

static inline void vulkan_object_init( struct vulkan_object *obj, UINT64 host_handle )
{
    obj->host_handle = host_handle;
    obj->client_handle = (UINT_PTR)obj;
}

struct vulkan_instance
{
    VULKAN_OBJECT_HEADER( VkInstance, instance );
#define USE_VK_FUNC(x) PFN_ ## x p_ ## x;
    ALL_VK_INSTANCE_FUNCS
#undef USE_VK_FUNC
    void (*p_insert_object)( struct vulkan_instance *instance, struct vulkan_object *obj );
    void (*p_remove_object)( struct vulkan_instance *instance, struct vulkan_object *obj );
    uint32_t api_version;
};

static inline struct vulkan_instance *vulkan_instance_from_handle( VkInstance handle )
{
    struct vulkan_client_object *client = (struct vulkan_client_object *)handle;
    return (struct vulkan_instance *)(UINT_PTR)client->unix_handle;
}

struct vulkan_physical_device
{
    VULKAN_OBJECT_HEADER( VkPhysicalDevice, physical_device );
    struct vulkan_instance *instance;
    uint32_t api_version;
};

static inline struct vulkan_physical_device *vulkan_physical_device_from_handle( VkPhysicalDevice handle )
{
    struct vulkan_client_object *client = (struct vulkan_client_object *)handle;
    return (struct vulkan_physical_device *)(UINT_PTR)client->unix_handle;
}

struct local_timeline_semaphore
{
    VkSemaphore sem;
    uint64_t value;
};

struct vulkan_device
{
    VULKAN_OBJECT_HEADER( VkDevice, device );
    struct vulkan_physical_device *physical_device;
#define USE_VK_FUNC(x) PFN_ ## x p_ ## x;
    ALL_VK_DEVICE_FUNCS
#undef USE_VK_FUNC
    uint64_t queue_count;
    struct vulkan_queue *queues;
    VkQueueFamilyProperties *queue_props;

    pthread_t signaller_thread;
    pthread_mutex_t signaller_mutex;
    BOOL stop;
    struct list free_fence_ops_list;
    struct list sem_poll_list;
    struct local_timeline_semaphore sem_poll_update;
    pthread_cond_t sem_poll_updated_cond;
    uint64_t sem_poll_update_value; /* set to sem_poll_update.value by signaller thread once update is processed. */
    unsigned int allocated_fence_ops_count;

    BOOL keyed_mutexes_enabled;
};

static inline struct vulkan_device *vulkan_device_from_handle( VkDevice handle )
{
    struct vulkan_client_object *client = (struct vulkan_client_object *)handle;
    return (struct vulkan_device *)(UINT_PTR)client->unix_handle;
}

struct vulkan_queue
{
    VULKAN_OBJECT_HEADER( VkQueue, queue );
    struct vulkan_device *device;
    uint32_t family_index;
    uint32_t queue_index;
    VkDeviceQueueCreateFlags flags;
};

static inline struct vulkan_queue *vulkan_queue_from_handle( VkQueue handle )
{
    struct vulkan_client_object *client = (struct vulkan_client_object *)handle;
    return (struct vulkan_queue *)(UINT_PTR)client->unix_handle;
}

struct vulkan_surface
{
    VULKAN_OBJECT_HEADER( VkSurfaceKHR, surface );
    struct vulkan_instance *instance;
};

static inline struct vulkan_surface *vulkan_surface_from_handle( VkSurfaceKHR handle )
{
    return (struct vulkan_surface *)(UINT_PTR)handle;
}

struct vulkan_swapchain
{
    VULKAN_OBJECT_HEADER( VkSwapchainKHR, swapchain );
};

static inline struct vulkan_swapchain *vulkan_swapchain_from_handle( VkSwapchainKHR handle )
{
    return (struct vulkan_swapchain *)(UINT_PTR)handle;
}

struct vulkan_semaphore
{
    VULKAN_OBJECT_HEADER( VkSemaphore, semaphore );
    BOOL d3d12_fence;
};

static inline struct vulkan_semaphore *vulkan_semaphore_from_handle( VkSemaphore handle )
{
    return (struct vulkan_semaphore *)(UINT_PTR)handle;
}

struct vulkan_funcs
{
    /* Vulkan global functions. These are the only calls at this point a graphics driver
     * needs to provide. Other function calls will be provided indirectly by dispatch
     * tables part of dispatchable Vulkan objects such as VkInstance or vkDevice.
     */
    PFN_vkAcquireNextImage2KHR p_vkAcquireNextImage2KHR;
    PFN_vkAcquireNextImageKHR p_vkAcquireNextImageKHR;
    PFN_vkCreateSwapchainKHR p_vkCreateSwapchainKHR;
    PFN_vkCreateWin32SurfaceKHR p_vkCreateWin32SurfaceKHR;
    PFN_vkDestroySurfaceKHR p_vkDestroySurfaceKHR;
    PFN_vkDestroySwapchainKHR p_vkDestroySwapchainKHR;
    PFN_vkGetDeviceProcAddr p_vkGetDeviceProcAddr;
    PFN_vkGetInstanceProcAddr p_vkGetInstanceProcAddr;
    PFN_vkGetPhysicalDevicePresentRectanglesKHR p_vkGetPhysicalDevicePresentRectanglesKHR;
    PFN_vkGetPhysicalDeviceSurfaceCapabilities2KHR p_vkGetPhysicalDeviceSurfaceCapabilities2KHR;
    PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR p_vkGetPhysicalDeviceSurfaceCapabilitiesKHR;
    PFN_vkGetPhysicalDeviceSurfaceFormats2KHR p_vkGetPhysicalDeviceSurfaceFormats2KHR;
    PFN_vkGetPhysicalDeviceSurfaceFormatsKHR p_vkGetPhysicalDeviceSurfaceFormatsKHR;
    PFN_vkGetPhysicalDeviceWin32PresentationSupportKHR p_vkGetPhysicalDeviceWin32PresentationSupportKHR;
    PFN_vkGetSwapchainImagesKHR p_vkGetSwapchainImagesKHR;
    PFN_vkQueuePresentKHR p_vkQueuePresentKHR;

    /* winevulkan specific functions */
    const char *(*p_get_host_surface_extension)(void);
};

/* interface between win32u and the user drivers */
struct vulkan_driver_funcs
{
    VkResult (*p_vulkan_surface_create)(HWND, const struct vulkan_instance *, VkSurfaceKHR *, void **);
    void (*p_vulkan_surface_destroy)(HWND, void *);
    void (*p_vulkan_surface_detach)(HWND, void *);
    void (*p_vulkan_surface_update)(HWND, void *);
    void (*p_vulkan_surface_presented)(HWND, void *, VkResult);
    BOOL (*p_vulkan_surface_enable_fshack)(HWND, void *);

    VkBool32 (*p_vkGetPhysicalDeviceWin32PresentationSupportKHR)(VkPhysicalDevice, uint32_t);
    const char *(*p_get_host_surface_extension)(void);
};

#endif /* WINE_UNIX_LIB */

#endif /* __WINE_VULKAN_DRIVER_H */
