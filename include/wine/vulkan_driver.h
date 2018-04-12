/* Automatically generated from Vulkan vk.xml; DO NOT EDIT! */

#ifndef __WINE_VULKAN_DRIVER_H
#define __WINE_VULKAN_DRIVER_H

/* Wine internal vulkan driver version, needs to be bumped upon vulkan_funcs changes. */
#define WINE_VULKAN_DRIVER_VERSION 3

struct vulkan_funcs
{
    /* Vulkan global functions. These are the only calls at this point a graphics driver
     * needs to provide. Other function calls will be provided indirectly by dispatch
     * tables part of dispatchable Vulkan objects such as VkInstance or vkDevice.
     */
    VkResult (*p_vkAcquireNextImageKHR)(VkDevice, VkSwapchainKHR, uint64_t, VkSemaphore, VkFence, uint32_t *);
    VkResult (*p_vkCreateInstance)(const VkInstanceCreateInfo *, const VkAllocationCallbacks *, VkInstance *);
    VkResult (*p_vkCreateSwapchainKHR)(VkDevice, const VkSwapchainCreateInfoKHR *, const VkAllocationCallbacks *, VkSwapchainKHR *);
    VkResult (*p_vkCreateWin32SurfaceKHR)(VkInstance, const VkWin32SurfaceCreateInfoKHR *, const VkAllocationCallbacks *, VkSurfaceKHR *);
    void (*p_vkDestroyInstance)(VkInstance, const VkAllocationCallbacks *);
    void (*p_vkDestroySurfaceKHR)(VkInstance, VkSurfaceKHR, const VkAllocationCallbacks *);
    void (*p_vkDestroySwapchainKHR)(VkDevice, VkSwapchainKHR, const VkAllocationCallbacks *);
    VkResult (*p_vkEnumerateInstanceExtensionProperties)(const char *, uint32_t *, VkExtensionProperties *);
    void * (*p_vkGetDeviceProcAddr)(VkDevice, const char *);
    void * (*p_vkGetInstanceProcAddr)(VkInstance, const char *);
    VkResult (*p_vkGetPhysicalDeviceSurfaceCapabilitiesKHR)(VkPhysicalDevice, VkSurfaceKHR, VkSurfaceCapabilitiesKHR *);
    VkResult (*p_vkGetPhysicalDeviceSurfaceFormatsKHR)(VkPhysicalDevice, VkSurfaceKHR, uint32_t *, VkSurfaceFormatKHR *);
    VkResult (*p_vkGetPhysicalDeviceSurfacePresentModesKHR)(VkPhysicalDevice, VkSurfaceKHR, uint32_t *, VkPresentModeKHR *);
    VkResult (*p_vkGetPhysicalDeviceSurfaceSupportKHR)(VkPhysicalDevice, uint32_t, VkSurfaceKHR, VkBool32 *);
    VkBool32 (*p_vkGetPhysicalDeviceWin32PresentationSupportKHR)(VkPhysicalDevice, uint32_t);
    VkResult (*p_vkGetSwapchainImagesKHR)(VkDevice, VkSwapchainKHR, uint32_t *, VkImage *);
    VkResult (*p_vkQueuePresentKHR)(VkQueue, const VkPresentInfoKHR *);

    /* Optional. Returns TRUE if FS hack is active, otherwise returns FALSE. If
     * it returns TRUE, then real_sz will contain the actual display
     * resolution; user_sz will contain the app's requested mode; and dst_blit
     * will contain the area to blit the user image to in real coordinates.
     * All parameters are optional. */
    VkBool32 (*query_fs_hack)(VkExtent2D *real_sz, VkExtent2D *user_sz, VkRect2D *dst_blit);
};

extern const struct vulkan_funcs * CDECL __wine_get_vulkan_driver(HDC hdc, UINT version);

#endif /* __WINE_VULKAN_DRIVER_H */
