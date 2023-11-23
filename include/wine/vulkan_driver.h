/* Automatically generated from Vulkan vk.xml; DO NOT EDIT!
 *
 * This file is generated from Vulkan vk.xml file covered
 * by the following copyright and permission notice:
 *
 * Copyright 2015-2023 The Khronos Group Inc.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#ifndef __WINE_VULKAN_DRIVER_H
#define __WINE_VULKAN_DRIVER_H

/* Wine internal vulkan driver version, needs to be bumped upon vulkan_funcs changes. */
#define WINE_VULKAN_DRIVER_VERSION 11

struct vulkan_funcs
{
    /* Vulkan global functions. These are the only calls at this point a graphics driver
     * needs to provide. Other function calls will be provided indirectly by dispatch
     * tables part of dispatchable Vulkan objects such as VkInstance or vkDevice.
     */
    VkResult (*p_vkAcquireNextImage2KHR)(VkDevice, const VkAcquireNextImageInfoKHR *, uint32_t *);
    VkResult (*p_vkAcquireNextImageKHR)(VkDevice, VkSwapchainKHR, uint64_t, VkSemaphore, VkFence, uint32_t *);
    VkResult (*p_vkCreateInstance)(const VkInstanceCreateInfo *, const VkAllocationCallbacks *, VkInstance *);
    VkResult (*p_vkCreateSwapchainKHR)(VkDevice, const VkSwapchainCreateInfoKHR *, const VkAllocationCallbacks *, VkSwapchainKHR *);
    VkResult (*p_vkCreateWin32SurfaceKHR)(VkInstance, const VkWin32SurfaceCreateInfoKHR *, const VkAllocationCallbacks *, VkSurfaceKHR *);
    void (*p_vkDestroyInstance)(VkInstance, const VkAllocationCallbacks *);
    void (*p_vkDestroySurfaceKHR)(VkInstance, VkSurfaceKHR, const VkAllocationCallbacks *);
    void (*p_vkDestroySwapchainKHR)(VkDevice, VkSwapchainKHR, const VkAllocationCallbacks *);
    VkResult (*p_vkEnumerateInstanceExtensionProperties)(const char *, uint32_t *, VkExtensionProperties *);
    VkResult (*p_vkGetDeviceGroupSurfacePresentModesKHR)(VkDevice, VkSurfaceKHR, VkDeviceGroupPresentModeFlagsKHR *);
    void * (*p_vkGetDeviceProcAddr)(VkDevice, const char *);
    void * (*p_vkGetInstanceProcAddr)(VkInstance, const char *);
    VkResult (*p_vkGetPhysicalDevicePresentRectanglesKHR)(VkPhysicalDevice, VkSurfaceKHR, uint32_t *, VkRect2D *);
    VkResult (*p_vkGetPhysicalDeviceSurfaceCapabilities2KHR)(VkPhysicalDevice, const VkPhysicalDeviceSurfaceInfo2KHR *, VkSurfaceCapabilities2KHR *);
    VkResult (*p_vkGetPhysicalDeviceSurfaceCapabilitiesKHR)(VkPhysicalDevice, VkSurfaceKHR, VkSurfaceCapabilitiesKHR *);
    VkResult (*p_vkGetPhysicalDeviceSurfaceFormats2KHR)(VkPhysicalDevice, const VkPhysicalDeviceSurfaceInfo2KHR *, uint32_t *, VkSurfaceFormat2KHR *);
    VkResult (*p_vkGetPhysicalDeviceSurfaceFormatsKHR)(VkPhysicalDevice, VkSurfaceKHR, uint32_t *, VkSurfaceFormatKHR *);
    VkResult (*p_vkGetPhysicalDeviceSurfacePresentModesKHR)(VkPhysicalDevice, VkSurfaceKHR, uint32_t *, VkPresentModeKHR *);
    VkResult (*p_vkGetPhysicalDeviceSurfaceSupportKHR)(VkPhysicalDevice, uint32_t, VkSurfaceKHR, VkBool32 *);
    VkBool32 (*p_vkGetPhysicalDeviceWin32PresentationSupportKHR)(VkPhysicalDevice, uint32_t);
    VkResult (*p_vkGetSwapchainImagesKHR)(VkDevice, VkSwapchainKHR, uint32_t *, VkImage *);
    VkResult (*p_vkQueuePresentKHR)(VkQueue, const VkPresentInfoKHR *);

    /* winevulkan specific functions */
    VkSurfaceKHR (*p_wine_get_native_surface)(VkSurfaceKHR);
    /* Optional. Returns TRUE if FS hack is active, otherwise returns FALSE. If
     * it returns TRUE, then real_sz will contain the actual display
     * resolution; user_sz will contain the app's requested mode; and dst_blit
     * will contain the area to blit the user image to in real coordinates.
     * All parameters are optional. */
    VkBool32 (*query_fs_hack)(VkSurfaceKHR surface, VkExtent2D *real_sz, VkExtent2D *user_sz, VkRect2D *dst_blit, VkFilter *filter);
};

extern const struct vulkan_funcs * __wine_get_vulkan_driver(UINT version);

static inline void *get_vulkan_driver_device_proc_addr(
        const struct vulkan_funcs *vulkan_funcs, const char *name)
{
    if (!name || name[0] != 'v' || name[1] != 'k') return NULL;

    name += 2;

    if (!strcmp(name, "AcquireNextImage2KHR"))
        return vulkan_funcs->p_vkAcquireNextImage2KHR;
    if (!strcmp(name, "AcquireNextImageKHR"))
        return vulkan_funcs->p_vkAcquireNextImageKHR;
    if (!strcmp(name, "CreateSwapchainKHR"))
        return vulkan_funcs->p_vkCreateSwapchainKHR;
    if (!strcmp(name, "DestroySwapchainKHR"))
        return vulkan_funcs->p_vkDestroySwapchainKHR;
    if (!strcmp(name, "GetDeviceGroupSurfacePresentModesKHR"))
        return vulkan_funcs->p_vkGetDeviceGroupSurfacePresentModesKHR;
    if (!strcmp(name, "GetDeviceProcAddr"))
        return vulkan_funcs->p_vkGetDeviceProcAddr;
    if (!strcmp(name, "GetSwapchainImagesKHR"))
        return vulkan_funcs->p_vkGetSwapchainImagesKHR;
    if (!strcmp(name, "QueuePresentKHR"))
        return vulkan_funcs->p_vkQueuePresentKHR;

    return NULL;
}

static inline void *get_vulkan_driver_instance_proc_addr(
        const struct vulkan_funcs *vulkan_funcs, VkInstance instance, const char *name)
{
    if (!name || name[0] != 'v' || name[1] != 'k') return NULL;

    name += 2;

    if (!strcmp(name, "CreateInstance"))
        return vulkan_funcs->p_vkCreateInstance;
    if (!strcmp(name, "EnumerateInstanceExtensionProperties"))
        return vulkan_funcs->p_vkEnumerateInstanceExtensionProperties;

    if (!instance) return NULL;

    if (!strcmp(name, "CreateWin32SurfaceKHR"))
        return vulkan_funcs->p_vkCreateWin32SurfaceKHR;
    if (!strcmp(name, "DestroyInstance"))
        return vulkan_funcs->p_vkDestroyInstance;
    if (!strcmp(name, "DestroySurfaceKHR"))
        return vulkan_funcs->p_vkDestroySurfaceKHR;
    if (!strcmp(name, "GetInstanceProcAddr"))
        return vulkan_funcs->p_vkGetInstanceProcAddr;
    if (!strcmp(name, "GetPhysicalDevicePresentRectanglesKHR"))
        return vulkan_funcs->p_vkGetPhysicalDevicePresentRectanglesKHR;
    if (!strcmp(name, "GetPhysicalDeviceSurfaceCapabilities2KHR"))
        return vulkan_funcs->p_vkGetPhysicalDeviceSurfaceCapabilities2KHR;
    if (!strcmp(name, "GetPhysicalDeviceSurfaceCapabilitiesKHR"))
        return vulkan_funcs->p_vkGetPhysicalDeviceSurfaceCapabilitiesKHR;
    if (!strcmp(name, "GetPhysicalDeviceSurfaceFormats2KHR"))
        return vulkan_funcs->p_vkGetPhysicalDeviceSurfaceFormats2KHR;
    if (!strcmp(name, "GetPhysicalDeviceSurfaceFormatsKHR"))
        return vulkan_funcs->p_vkGetPhysicalDeviceSurfaceFormatsKHR;
    if (!strcmp(name, "GetPhysicalDeviceSurfacePresentModesKHR"))
        return vulkan_funcs->p_vkGetPhysicalDeviceSurfacePresentModesKHR;
    if (!strcmp(name, "GetPhysicalDeviceSurfaceSupportKHR"))
        return vulkan_funcs->p_vkGetPhysicalDeviceSurfaceSupportKHR;
    if (!strcmp(name, "GetPhysicalDeviceWin32PresentationSupportKHR"))
        return vulkan_funcs->p_vkGetPhysicalDeviceWin32PresentationSupportKHR;

    name -= 2;

    return get_vulkan_driver_device_proc_addr(vulkan_funcs, name);
}

typedef VkResult (WINAPI *PFN_native_vkCreateInstance)(const VkInstanceCreateInfo *, const VkAllocationCallbacks *, VkInstance *,
                                                       void * (*)(VkInstance, const char *), void *);
typedef VkResult (WINAPI *PFN_native_vkCreateDevice)(VkPhysicalDevice, const VkDeviceCreateInfo *, const VkAllocationCallbacks *, VkDevice *,
                                                     void * (*)(VkInstance, const char *), void *);

typedef struct VkCreateInfoWineDeviceCallback {
    VkStructureType             sType;
    const void*                 pNext;
    PFN_native_vkCreateDevice   native_create_callback;
    void*                       context;
} VkCreateInfoWineDeviceCallback;
#define VK_STRUCTURE_TYPE_CREATE_INFO_WINE_DEVICE_CALLBACK 2125312001
typedef struct VkCreateInfoWineInstanceCallback {
    VkStructureType             sType;
    const void*                 pNext;
    PFN_native_vkCreateInstance native_create_callback;
    void*                       context;
} VkCreateInfoWineInstanceCallback;
#define VK_STRUCTURE_TYPE_CREATE_INFO_WINE_INSTANCE_CALLBACK 2125312002
#endif /* __WINE_VULKAN_DRIVER_H */
