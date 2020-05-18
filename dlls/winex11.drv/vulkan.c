/* X11DRV Vulkan implementation
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

/* NOTE: If making changes here, consider whether they should be reflected in
 * the other drivers. */

#include "config.h"
#include "wine/port.h"

#include <stdarg.h>
#include <stdio.h>

#include "windef.h"
#include "winbase.h"

#include "wine/debug.h"
#include "wine/heap.h"
#include "wine/library.h"
#include "x11drv.h"
#include "xcomposite.h"

#define VK_NO_PROTOTYPES
#define WINE_VK_HOST

#include "wine/vulkan.h"
#include "wine/vulkan_driver.h"

WINE_DEFAULT_DEBUG_CHANNEL(vulkan);

#ifdef SONAME_LIBVULKAN
WINE_DECLARE_DEBUG_CHANNEL(fps);

static CRITICAL_SECTION context_section;
static CRITICAL_SECTION_DEBUG critsect_debug =
{
    0, 0, &context_section,
    { &critsect_debug.ProcessLocksList, &critsect_debug.ProcessLocksList },
      0, 0, { (DWORD_PTR)(__FILE__ ": context_section") }
};
static CRITICAL_SECTION context_section = { &critsect_debug, -1, 0, 0, 0, 0 };

static XContext vulkan_hwnd_context;
static XContext vulkan_swapchain_surface_context;

static struct vulkan_funcs vulkan_funcs;

#define VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR 1000004000

struct wine_vk_surface
{
    LONG ref;
    Window window;
    HDC child_window_dc;
    VkSurfaceKHR surface; /* native surface */
};

typedef struct VkXlibSurfaceCreateInfoKHR
{
    VkStructureType sType;
    const void *pNext;
    VkXlibSurfaceCreateFlagsKHR flags;
    Display *dpy;
    Window window;
} VkXlibSurfaceCreateInfoKHR;

static VkResult (*pvkCreateInstance)(const VkInstanceCreateInfo *, const VkAllocationCallbacks *, VkInstance *);
static VkResult (*pvkCreateSwapchainKHR)(VkDevice, const VkSwapchainCreateInfoKHR *, const VkAllocationCallbacks *, VkSwapchainKHR *);
static VkResult (*pvkCreateXlibSurfaceKHR)(VkInstance, const VkXlibSurfaceCreateInfoKHR *, const VkAllocationCallbacks *, VkSurfaceKHR *);
static void (*pvkDestroyInstance)(VkInstance, const VkAllocationCallbacks *);
static void (*pvkDestroySurfaceKHR)(VkInstance, VkSurfaceKHR, const VkAllocationCallbacks *);
static void (*pvkDestroySwapchainKHR)(VkDevice, VkSwapchainKHR, const VkAllocationCallbacks *);
static VkResult (*pvkEnumerateInstanceExtensionProperties)(const char *, uint32_t *, VkExtensionProperties *);
static VkResult (*pvkGetDeviceGroupSurfacePresentModesKHR)(VkDevice, VkSurfaceKHR, VkDeviceGroupPresentModeFlagsKHR *);
static void * (*pvkGetDeviceProcAddr)(VkDevice, const char *);
static void * (*pvkGetInstanceProcAddr)(VkInstance, const char *);
static VkResult (*pvkGetPhysicalDevicePresentRectanglesKHR)(VkPhysicalDevice, VkSurfaceKHR, uint32_t *, VkRect2D *);
static VkResult (*pvkGetPhysicalDeviceSurfaceCapabilities2KHR)(VkPhysicalDevice, const VkPhysicalDeviceSurfaceInfo2KHR *, VkSurfaceCapabilities2KHR *);
static VkResult (*pvkGetPhysicalDeviceSurfaceCapabilitiesKHR)(VkPhysicalDevice, VkSurfaceKHR, VkSurfaceCapabilitiesKHR *);
static VkResult (*pvkGetPhysicalDeviceSurfaceFormats2KHR)(VkPhysicalDevice, const VkPhysicalDeviceSurfaceInfo2KHR *, uint32_t *, VkSurfaceFormat2KHR *);
static VkResult (*pvkGetPhysicalDeviceSurfaceFormatsKHR)(VkPhysicalDevice, VkSurfaceKHR, uint32_t *, VkSurfaceFormatKHR *);
static VkResult (*pvkGetPhysicalDeviceSurfacePresentModesKHR)(VkPhysicalDevice, VkSurfaceKHR, uint32_t *, VkPresentModeKHR *);
static VkResult (*pvkGetPhysicalDeviceSurfaceSupportKHR)(VkPhysicalDevice, uint32_t, VkSurfaceKHR, VkBool32 *);
static VkBool32 (*pvkGetPhysicalDeviceXlibPresentationSupportKHR)(VkPhysicalDevice, uint32_t, Display *, VisualID);
static VkResult (*pvkGetSwapchainImagesKHR)(VkDevice, VkSwapchainKHR, uint32_t *, VkImage *);
static VkResult (*pvkQueuePresentKHR)(VkQueue, const VkPresentInfoKHR *);

static void *X11DRV_get_vk_device_proc_addr(const char *name);
static void *X11DRV_get_vk_instance_proc_addr(VkInstance instance, const char *name);

static inline struct wine_vk_surface *surface_from_handle(VkSurfaceKHR handle)
{
    return (struct wine_vk_surface *)(uintptr_t)handle;
}

static void *vulkan_handle;

static BOOL WINAPI wine_vk_init(INIT_ONCE *once, void *param, void **context)
{
    if (!(vulkan_handle = wine_dlopen(SONAME_LIBVULKAN, RTLD_NOW, NULL, 0)))
    {
        ERR("Failed to load %s.\n", SONAME_LIBVULKAN);
        return TRUE;
    }

#define LOAD_FUNCPTR(f) if (!(p##f = wine_dlsym(vulkan_handle, #f, NULL, 0))) goto fail;
#define LOAD_OPTIONAL_FUNCPTR(f) p##f = wine_dlsym(vulkan_handle, #f, NULL, 0);
    LOAD_FUNCPTR(vkCreateInstance)
    LOAD_FUNCPTR(vkCreateSwapchainKHR)
    LOAD_FUNCPTR(vkCreateXlibSurfaceKHR)
    LOAD_FUNCPTR(vkDestroyInstance)
    LOAD_FUNCPTR(vkDestroySurfaceKHR)
    LOAD_FUNCPTR(vkDestroySwapchainKHR)
    LOAD_FUNCPTR(vkEnumerateInstanceExtensionProperties)
    LOAD_FUNCPTR(vkGetDeviceProcAddr)
    LOAD_FUNCPTR(vkGetInstanceProcAddr)
    LOAD_OPTIONAL_FUNCPTR(vkGetPhysicalDeviceSurfaceCapabilities2KHR)
    LOAD_FUNCPTR(vkGetPhysicalDeviceSurfaceCapabilitiesKHR)
    LOAD_OPTIONAL_FUNCPTR(vkGetPhysicalDeviceSurfaceFormats2KHR)
    LOAD_FUNCPTR(vkGetPhysicalDeviceSurfaceFormatsKHR)
    LOAD_FUNCPTR(vkGetPhysicalDeviceSurfacePresentModesKHR)
    LOAD_FUNCPTR(vkGetPhysicalDeviceSurfaceSupportKHR)
    LOAD_FUNCPTR(vkGetPhysicalDeviceXlibPresentationSupportKHR)
    LOAD_FUNCPTR(vkGetSwapchainImagesKHR)
    LOAD_FUNCPTR(vkQueuePresentKHR)
    LOAD_OPTIONAL_FUNCPTR(vkGetDeviceGroupSurfacePresentModesKHR)
    LOAD_OPTIONAL_FUNCPTR(vkGetPhysicalDevicePresentRectanglesKHR)

    if(!pvkGetPhysicalDeviceSurfaceCapabilities2KHR){
        vulkan_funcs.p_vkGetPhysicalDeviceSurfaceCapabilities2KHR = NULL;
    }
    if(!pvkGetPhysicalDeviceSurfaceFormats2KHR){
        vulkan_funcs.p_vkGetPhysicalDeviceSurfaceFormats2KHR = NULL;
    }
#undef LOAD_FUNCPTR
#undef LOAD_OPTIONAL_FUNCPTR

    vulkan_hwnd_context = XUniqueContext();
    vulkan_swapchain_surface_context = XUniqueContext();

    return TRUE;

fail:
    wine_dlclose(vulkan_handle, NULL, 0);
    vulkan_handle = NULL;
    return TRUE;
}

/* Helper function for converting between win32 and X11 compatible VkInstanceCreateInfo.
 * Caller is responsible for allocation and cleanup of 'dst'.
 */
static VkResult wine_vk_instance_convert_create_info(const VkInstanceCreateInfo *src,
        VkInstanceCreateInfo *dst)
{
    unsigned int i;
    const char **enabled_extensions = NULL;

    dst->sType = src->sType;
    dst->flags = src->flags;
    dst->pApplicationInfo = src->pApplicationInfo;
    dst->pNext = src->pNext;
    dst->enabledLayerCount = 0;
    dst->ppEnabledLayerNames = NULL;
    dst->enabledExtensionCount = 0;
    dst->ppEnabledExtensionNames = NULL;

    if (src->enabledExtensionCount > 0)
    {
        enabled_extensions = heap_calloc(src->enabledExtensionCount, sizeof(*src->ppEnabledExtensionNames));
        if (!enabled_extensions)
        {
            ERR("Failed to allocate memory for enabled extensions\n");
            return VK_ERROR_OUT_OF_HOST_MEMORY;
        }

        for (i = 0; i < src->enabledExtensionCount; i++)
        {
            /* Substitute extension with X11 ones else copy. Long-term, when we
             * support more extensions, we should store these in a list.
             */
            if (!strcmp(src->ppEnabledExtensionNames[i], "VK_KHR_win32_surface"))
            {
                enabled_extensions[i] = "VK_KHR_xlib_surface";
            }
            else
            {
                enabled_extensions[i] = src->ppEnabledExtensionNames[i];
            }
        }
        dst->ppEnabledExtensionNames = enabled_extensions;
        dst->enabledExtensionCount = src->enabledExtensionCount;
    }

    return VK_SUCCESS;
}

static struct wine_vk_surface *wine_vk_surface_grab(struct wine_vk_surface *surface)
{
    InterlockedIncrement(&surface->ref);
    return surface;
}

static void wine_vk_surface_release(struct wine_vk_surface *surface)
{
    if (InterlockedDecrement(&surface->ref))
        return;

    if (surface->window)
        XDestroyWindow(gdi_display, surface->window);

    heap_free(surface);
}

void wine_vk_surface_destroy(HWND hwnd)
{
    struct wine_vk_surface *surface;
    EnterCriticalSection(&context_section);
    if (!XFindContext(gdi_display, (XID)hwnd, vulkan_hwnd_context, (char **)&surface))
    {
        wine_vk_surface_release(surface);
    }
    XDeleteContext(gdi_display, (XID)hwnd, vulkan_hwnd_context);
    LeaveCriticalSection(&context_section);
}

static VkResult X11DRV_vkCreateInstance(const VkInstanceCreateInfo *create_info,
        const VkAllocationCallbacks *allocator, VkInstance *instance)
{
    VkInstanceCreateInfo create_info_host;
    VkResult res;
    TRACE("create_info %p, allocator %p, instance %p\n", create_info, allocator, instance);

    if (allocator)
        FIXME("Support for allocation callbacks not implemented yet\n");

    /* Perform a second pass on converting VkInstanceCreateInfo. Winevulkan
     * performed a first pass in which it handles everything except for WSI
     * functionality such as VK_KHR_win32_surface. Handle this now.
     */
    res = wine_vk_instance_convert_create_info(create_info, &create_info_host);
    if (res != VK_SUCCESS)
    {
        ERR("Failed to convert instance create info, res=%d\n", res);
        return res;
    }

    res = pvkCreateInstance(&create_info_host, NULL /* allocator */, instance);

    heap_free((void *)create_info_host.ppEnabledExtensionNames);
    return res;
}

static VkResult X11DRV_vkCreateSwapchainKHR(VkDevice device,
        const VkSwapchainCreateInfoKHR *create_info,
        const VkAllocationCallbacks *allocator, VkSwapchainKHR *swapchain)
{
    VkResult res;
    VkSwapchainCreateInfoKHR create_info_host;
    struct wine_vk_surface *x11_surface = surface_from_handle(create_info->surface);

    TRACE("%p %p %p %p\n", device, create_info, allocator, swapchain);

    if (allocator)
        FIXME("Support for allocation callbacks not implemented yet\n");

    create_info_host = *create_info;
    create_info_host.surface = x11_surface->surface;

    res = pvkCreateSwapchainKHR(device, &create_info_host, NULL /* allocator */, swapchain);
    if (res == VK_SUCCESS)
    {
        XSaveContext(gdi_display, (XID)(*swapchain), vulkan_swapchain_surface_context, (char *)x11_surface);
    }
    return res;
}

static VkResult X11DRV_vkCreateWin32SurfaceKHR(VkInstance instance,
        const VkWin32SurfaceCreateInfoKHR *create_info,
        const VkAllocationCallbacks *allocator, VkSurfaceKHR *surface)
{
    VkResult res;
    VkXlibSurfaceCreateInfoKHR create_info_host;
    struct wine_vk_surface *x11_surface, *prev;

    TRACE("%p %p %p %p\n", instance, create_info, allocator, surface);

    if (allocator)
        FIXME("Support for allocation callbacks not implemented yet\n");

    x11_surface = heap_alloc_zero(sizeof(*x11_surface));
    if (!x11_surface)
        return VK_ERROR_OUT_OF_HOST_MEMORY;

    x11_surface->ref = 1;

    x11_surface->window = create_client_window(create_info->hwnd, &default_visual);
    if (!x11_surface->window)
    {
        ERR("Failed to allocate client window for hwnd=%p\n", create_info->hwnd);

        /* VK_KHR_win32_surface only allows out of host and device memory as errors. */
        res = VK_ERROR_OUT_OF_HOST_MEMORY;
        goto err;
    }

    /* child window rendering. */
    if (GetAncestor(create_info->hwnd, GA_PARENT) != GetDesktopWindow())
    {
#ifdef SONAME_LIBXCOMPOSITE
        if (usexcomposite)
        {
            pXCompositeRedirectWindow(gdi_display, x11_surface->window, CompositeRedirectManual);
            x11_surface->child_window_dc = GetDC(create_info->hwnd);
        }
#else
        if (0)
        {
        }
#endif
        else
        {
            FIXME("Child window rendering is not supported without X Composite Extension!\n");
            return VK_ERROR_INCOMPATIBLE_DRIVER;
        }
    }

    create_info_host.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
    create_info_host.pNext = NULL;
    create_info_host.flags = 0; /* reserved */
    create_info_host.dpy = gdi_display;
    create_info_host.window = x11_surface->window;

    res = pvkCreateXlibSurfaceKHR(instance, &create_info_host, NULL /* allocator */, &x11_surface->surface);
    if (res != VK_SUCCESS)
    {
        ERR("Failed to create Xlib surface, res=%d\n", res);
        goto err;
    }

    EnterCriticalSection(&context_section);
    if (!XFindContext(gdi_display, (XID)create_info->hwnd, vulkan_hwnd_context, (char **)&prev))
    {
        wine_vk_surface_release(prev);
    }
    XSaveContext(gdi_display, (XID)create_info->hwnd, vulkan_hwnd_context, (char *)wine_vk_surface_grab(x11_surface));
    LeaveCriticalSection(&context_section);

    *surface = (uintptr_t)x11_surface;

    TRACE("Created surface=0x%s\n", wine_dbgstr_longlong(*surface));
    return VK_SUCCESS;

err:
    wine_vk_surface_release(x11_surface);
    return res;
}

static void X11DRV_vkDestroyInstance(VkInstance instance, const VkAllocationCallbacks *allocator)
{
    TRACE("%p %p\n", instance, allocator);

    if (allocator)
        FIXME("Support for allocation callbacks not implemented yet\n");

    pvkDestroyInstance(instance, NULL /* allocator */);
}

static void X11DRV_vkDestroySurfaceKHR(VkInstance instance, VkSurfaceKHR surface,
        const VkAllocationCallbacks *allocator)
{
    struct wine_vk_surface *x11_surface = surface_from_handle(surface);

    TRACE("%p 0x%s %p\n", instance, wine_dbgstr_longlong(surface), allocator);

    if (allocator)
        FIXME("Support for allocation callbacks not implemented yet\n");

    /* vkDestroySurfaceKHR must handle VK_NULL_HANDLE (0) for surface. */
    if (x11_surface)
    {
        pvkDestroySurfaceKHR(instance, x11_surface->surface, NULL /* allocator */);

        wine_vk_surface_release(x11_surface);
    }
}

static void X11DRV_vkDestroySwapchainKHR(VkDevice device, VkSwapchainKHR swapchain,
         const VkAllocationCallbacks *allocator)
{
    TRACE("%p, 0x%s %p\n", device, wine_dbgstr_longlong(swapchain), allocator);

    if (allocator)
        FIXME("Support for allocation callbacks not implemented yet\n");

    pvkDestroySwapchainKHR(device, swapchain, NULL /* allocator */);
    XDeleteContext(gdi_display, (XID)swapchain, vulkan_swapchain_surface_context);
}

static VkResult X11DRV_vkEnumerateInstanceExtensionProperties(const char *layer_name,
        uint32_t *count, VkExtensionProperties* properties)
{
    unsigned int i;
    VkResult res;

    TRACE("layer_name %s, count %p, properties %p\n", debugstr_a(layer_name), count, properties);

    /* This shouldn't get called with layer_name set, the ICD loader prevents it. */
    if (layer_name)
    {
        ERR("Layer enumeration not supported from ICD.\n");
        return VK_ERROR_LAYER_NOT_PRESENT;
    }

    /* We will return the same number of instance extensions reported by the host back to
     * winevulkan. Along the way we may replace xlib extensions with their win32 equivalents.
     * Winevulkan will perform more detailed filtering as it knows whether it has thunks
     * for a particular extension.
     */
    res = pvkEnumerateInstanceExtensionProperties(layer_name, count, properties);
    if (!properties || res < 0)
        return res;

    for (i = 0; i < *count; i++)
    {
        /* For now the only x11 extension we need to fixup. Long-term we may need an array. */
        if (!strcmp(properties[i].extensionName, "VK_KHR_xlib_surface"))
        {
            TRACE("Substituting VK_KHR_xlib_surface for VK_KHR_win32_surface\n");

            snprintf(properties[i].extensionName, sizeof(properties[i].extensionName),
                    VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
            properties[i].specVersion = VK_KHR_WIN32_SURFACE_SPEC_VERSION;
        }
    }

    TRACE("Returning %u extensions.\n", *count);
    return res;
}

static VkResult X11DRV_vkGetDeviceGroupSurfacePresentModesKHR(VkDevice device,
        VkSurfaceKHR surface, VkDeviceGroupPresentModeFlagsKHR *flags)
{
    struct wine_vk_surface *x11_surface = surface_from_handle(surface);

    TRACE("%p, 0x%s, %p\n", device, wine_dbgstr_longlong(surface), flags);

    return pvkGetDeviceGroupSurfacePresentModesKHR(device, x11_surface->surface, flags);
}

static void *X11DRV_vkGetDeviceProcAddr(VkDevice device, const char *name)
{
    void *proc_addr;

    TRACE("%p, %s\n", device, debugstr_a(name));

    if ((proc_addr = X11DRV_get_vk_device_proc_addr(name)))
        return proc_addr;

    return pvkGetDeviceProcAddr(device, name);
}

static void *X11DRV_vkGetInstanceProcAddr(VkInstance instance, const char *name)
{
    void *proc_addr;

    TRACE("%p, %s\n", instance, debugstr_a(name));

    if ((proc_addr = X11DRV_get_vk_instance_proc_addr(instance, name)))
        return proc_addr;

    return pvkGetInstanceProcAddr(instance, name);
}

static VkResult X11DRV_vkGetPhysicalDevicePresentRectanglesKHR(VkPhysicalDevice phys_dev,
        VkSurfaceKHR surface, uint32_t *count, VkRect2D *rects)
{
    struct wine_vk_surface *x11_surface = surface_from_handle(surface);

    TRACE("%p, 0x%s, %p, %p\n", phys_dev, wine_dbgstr_longlong(surface), count, rects);

    return pvkGetPhysicalDevicePresentRectanglesKHR(phys_dev, x11_surface->surface, count, rects);
}

static VkResult X11DRV_vkGetPhysicalDeviceSurfaceCapabilitiesKHR(VkPhysicalDevice phys_dev,
        VkSurfaceKHR surface, VkSurfaceCapabilitiesKHR *capabilities)
{
    struct wine_vk_surface *x11_surface = surface_from_handle(surface);

    TRACE("%p, 0x%s, %p\n", phys_dev, wine_dbgstr_longlong(surface), capabilities);

    return pvkGetPhysicalDeviceSurfaceCapabilitiesKHR(phys_dev, x11_surface->surface, capabilities);
}

static VkResult X11DRV_vkGetPhysicalDeviceSurfaceFormatsKHR(VkPhysicalDevice phys_dev,
        VkSurfaceKHR surface, uint32_t *count, VkSurfaceFormatKHR *formats)
{
    struct wine_vk_surface *x11_surface = surface_from_handle(surface);

    TRACE("%p, 0x%s, %p, %p\n", phys_dev, wine_dbgstr_longlong(surface), count, formats);

    return pvkGetPhysicalDeviceSurfaceFormatsKHR(phys_dev, x11_surface->surface, count, formats);
}

static VkResult X11DRV_vkGetPhysicalDeviceSurfacePresentModesKHR(VkPhysicalDevice phys_dev,
        VkSurfaceKHR surface, uint32_t *count, VkPresentModeKHR *modes)
{
    struct wine_vk_surface *x11_surface = surface_from_handle(surface);

    TRACE("%p, 0x%s, %p, %p\n", phys_dev, wine_dbgstr_longlong(surface), count, modes);

    return pvkGetPhysicalDeviceSurfacePresentModesKHR(phys_dev, x11_surface->surface, count, modes);
}

static VkResult X11DRV_vkGetPhysicalDeviceSurfaceCapabilities2KHR(VkPhysicalDevice phys_dev,
        const VkPhysicalDeviceSurfaceInfo2KHR* surface_info, VkSurfaceCapabilities2KHR *capabilities)
{
    struct wine_vk_surface *x11_surface = surface_from_handle(surface_info->surface);
    VkPhysicalDeviceSurfaceInfo2KHR x11_surface_info;
    x11_surface_info.sType   = surface_info->sType;
    x11_surface_info.pNext   = surface_info->pNext;
    x11_surface_info.surface = x11_surface->surface;

    TRACE("%p, %p, %p\n", phys_dev, surface_info, capabilities);

    return pvkGetPhysicalDeviceSurfaceCapabilities2KHR(phys_dev, &x11_surface_info, capabilities);
}

static VkResult X11DRV_vkGetPhysicalDeviceSurfaceFormats2KHR(VkPhysicalDevice phys_dev,
        const VkPhysicalDeviceSurfaceInfo2KHR* surface_info, uint32_t *count, VkSurfaceFormat2KHR *formats)
{
    struct wine_vk_surface *x11_surface = surface_from_handle(surface_info->surface);
    VkPhysicalDeviceSurfaceInfo2KHR x11_surface_info;
    x11_surface_info.sType   = surface_info->sType;
    x11_surface_info.pNext   = surface_info->pNext;
    x11_surface_info.surface = x11_surface->surface;

    TRACE("%p, %p, %p, %p\n", phys_dev, surface_info, count, formats);

    return pvkGetPhysicalDeviceSurfaceFormats2KHR(phys_dev, &x11_surface_info, count, formats);
}

static VkResult X11DRV_vkGetPhysicalDeviceSurfaceSupportKHR(VkPhysicalDevice phys_dev,
        uint32_t index, VkSurfaceKHR surface, VkBool32 *supported)
{
    struct wine_vk_surface *x11_surface = surface_from_handle(surface);

    TRACE("%p, %u, 0x%s, %p\n", phys_dev, index, wine_dbgstr_longlong(surface), supported);

    return pvkGetPhysicalDeviceSurfaceSupportKHR(phys_dev, index, x11_surface->surface, supported);
}

static VkBool32 X11DRV_vkGetPhysicalDeviceWin32PresentationSupportKHR(VkPhysicalDevice phys_dev,
        uint32_t index)
{
    TRACE("%p %u\n", phys_dev, index);

    return pvkGetPhysicalDeviceXlibPresentationSupportKHR(phys_dev, index, gdi_display,
            default_visual.visual->visualid);
}

static VkResult X11DRV_vkGetSwapchainImagesKHR(VkDevice device,
        VkSwapchainKHR swapchain, uint32_t *count, VkImage *images)
{
    TRACE("%p, 0x%s %p %p\n", device, wine_dbgstr_longlong(swapchain), count, images);
    return pvkGetSwapchainImagesKHR(device, swapchain, count, images);
}

static VkResult X11DRV_vkQueuePresentKHR(VkQueue queue, const VkPresentInfoKHR *present_info)
{
    VkResult res;

    TRACE("%p, %p\n", queue, present_info);

    res = pvkQueuePresentKHR(queue, present_info);

    if (TRACE_ON(fps))
    {
        static unsigned long frames, frames_total;
        static long prev_time, start_time;
        DWORD time;

        time = GetTickCount();
        frames++;
        frames_total++;
        if (time - prev_time > 1500)
        {
            TRACE_(fps)("%p @ approx %.2ffps, total %.2ffps\n",
                    queue, 1000.0 * frames / (time - prev_time),
                    1000.0 * frames_total / (time - start_time));
            prev_time = time;
            frames = 0;
            if (!start_time)
                start_time = time;
        }
    }

    for (uint32_t i = 0 ; i < present_info->swapchainCount; ++i)
    {
        struct wine_vk_surface *x11_surface;
        if (!XFindContext(gdi_display, (XID)present_info->pSwapchains[i],
                          vulkan_swapchain_surface_context, (char **)&x11_surface) &&
            x11_surface->child_window_dc)
        {
            struct x11drv_escape_flush_gl_drawable escape;
            escape.code = X11DRV_FLUSH_GL_DRAWABLE;
            escape.gl_drawable = x11_surface->window;
            escape.flush = TRUE;
            ExtEscape(x11_surface->child_window_dc, X11DRV_ESCAPE, sizeof(escape), (LPSTR)&escape, 0, NULL);
        }
    }

    return res;
}

static VkBool32 X11DRV_query_fs_hack(VkExtent2D *real_sz, VkExtent2D *user_sz,
        VkRect2D *dst_blit, VkFilter *filter)
{
    if(fs_hack_enabled()){
        POINT real_res = fs_hack_real_mode();
        POINT user_res = fs_hack_current_mode();
        POINT scaled = fs_hack_get_scaled_screen_size();
        POINT scaled_origin = {0, 0};

        if(filter)
            *filter = fs_hack_is_integer() ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;

        fs_hack_user_to_real(&scaled_origin);

        if(real_sz){
            real_sz->width = real_res.x;
            real_sz->height = real_res.y;
        }

        if(user_sz){
            user_sz->width = user_res.x;
            user_sz->height = user_res.y;
        }

        if(dst_blit){
            dst_blit->offset.x = scaled_origin.x;
            dst_blit->offset.y = scaled_origin.y;
            dst_blit->extent.width = scaled.x;
            dst_blit->extent.height = scaled.y;
        }

        return VK_TRUE;
    }
    return VK_FALSE;
}

static struct vulkan_funcs vulkan_funcs =
{
    X11DRV_vkCreateInstance,
    X11DRV_vkCreateSwapchainKHR,
    X11DRV_vkCreateWin32SurfaceKHR,
    X11DRV_vkDestroyInstance,
    X11DRV_vkDestroySurfaceKHR,
    X11DRV_vkDestroySwapchainKHR,
    X11DRV_vkEnumerateInstanceExtensionProperties,
    X11DRV_vkGetDeviceGroupSurfacePresentModesKHR,
    X11DRV_vkGetDeviceProcAddr,
    X11DRV_vkGetInstanceProcAddr,
    X11DRV_vkGetPhysicalDevicePresentRectanglesKHR,
    X11DRV_vkGetPhysicalDeviceSurfaceCapabilities2KHR,
    X11DRV_vkGetPhysicalDeviceSurfaceCapabilitiesKHR,
    X11DRV_vkGetPhysicalDeviceSurfaceFormats2KHR,
    X11DRV_vkGetPhysicalDeviceSurfaceFormatsKHR,
    X11DRV_vkGetPhysicalDeviceSurfacePresentModesKHR,
    X11DRV_vkGetPhysicalDeviceSurfaceSupportKHR,
    X11DRV_vkGetPhysicalDeviceWin32PresentationSupportKHR,
    X11DRV_vkGetSwapchainImagesKHR,
    X11DRV_vkQueuePresentKHR,
    X11DRV_query_fs_hack,
};

static void *X11DRV_get_vk_device_proc_addr(const char *name)
{
    return get_vulkan_driver_device_proc_addr(&vulkan_funcs, name);
}

static void *X11DRV_get_vk_instance_proc_addr(VkInstance instance, const char *name)
{
    return get_vulkan_driver_instance_proc_addr(&vulkan_funcs, instance, name);
}

const struct vulkan_funcs *get_vulkan_driver(UINT version)
{
    static INIT_ONCE init_once = INIT_ONCE_STATIC_INIT;

    if (version != WINE_VULKAN_DRIVER_VERSION)
    {
        ERR("version mismatch, vulkan wants %u but driver has %u\n", version, WINE_VULKAN_DRIVER_VERSION);
        return NULL;
    }

    InitOnceExecuteOnce(&init_once, wine_vk_init, NULL, NULL);
    if (vulkan_handle)
        return &vulkan_funcs;

    return NULL;
}

#else /* No vulkan */

const struct vulkan_funcs *get_vulkan_driver(UINT version)
{
    ERR("Wine was built without Vulkan support.\n");
    return NULL;
}

void wine_vk_surface_destroy(HWND hwnd)
{
}

#endif /* SONAME_LIBVULKAN */
