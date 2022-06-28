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

#include <stdarg.h>
#include <stdio.h>
#include <dlfcn.h>

#include "windef.h"
#include "winbase.h"

#include "wine/debug.h"
#include "wine/heap.h"
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

static XContext vulkan_swapchain_context;

#define VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR 1000004000

static struct list surface_list = LIST_INIT( surface_list );

struct wine_vk_surface
{
    LONG ref;
    struct list entry;
    Window window;
    VkSurfaceKHR surface; /* native surface */
    VkPresentModeKHR present_mode;
    BOOL known_child; /* hwnd is or has a child */
    BOOL offscreen; /* drawable is offscreen */
    LONG swapchain_count; /* surface can have one active an many retired swapchains */
    HDC hdc;
    HWND hwnd;
    DWORD hwnd_thread_id;
};

typedef struct VkXlibSurfaceCreateInfoKHR
{
    VkStructureType sType;
    const void *pNext;
    VkXlibSurfaceCreateFlagsKHR flags;
    Display *dpy;
    Window window;
} VkXlibSurfaceCreateInfoKHR;

static VkResult (*pvkAcquireNextImageKHR)(VkDevice, VkSwapchainKHR, uint64_t, VkSemaphore, VkFence, uint32_t *);
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
static VkResult (*pvkWaitForFences)(VkDevice device, uint32_t fenceCount, const VkFence *pFences, VkBool32 waitAll, uint64_t timeout);
static VkResult (*pvkCreateFence)(VkDevice device, const VkFenceCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkFence *pFence);
static void (*pvkDestroyFence)(VkDevice device, VkFence fence, const VkAllocationCallbacks *pAllocator);

static void *X11DRV_get_vk_device_proc_addr(const char *name);
static void *X11DRV_get_vk_instance_proc_addr(VkInstance instance, const char *name);

static inline struct wine_vk_surface *surface_from_handle(VkSurfaceKHR handle)
{
    return (struct wine_vk_surface *)(uintptr_t)handle;
}

static void *vulkan_handle;

static BOOL WINAPI wine_vk_init(INIT_ONCE *once, void *param, void **context)
{
    if (!(vulkan_handle = dlopen(SONAME_LIBVULKAN, RTLD_NOW)))
    {
        ERR("Failed to load %s.\n", SONAME_LIBVULKAN);
        return TRUE;
    }

#define LOAD_FUNCPTR(f) if (!(p##f = dlsym(vulkan_handle, #f))) goto fail
#define LOAD_OPTIONAL_FUNCPTR(f) p##f = dlsym(vulkan_handle, #f)
    LOAD_FUNCPTR(vkAcquireNextImageKHR);
    LOAD_FUNCPTR(vkCreateInstance);
    LOAD_FUNCPTR(vkCreateSwapchainKHR);
    LOAD_FUNCPTR(vkCreateXlibSurfaceKHR);
    LOAD_FUNCPTR(vkDestroyInstance);
    LOAD_FUNCPTR(vkDestroySurfaceKHR);
    LOAD_FUNCPTR(vkDestroySwapchainKHR);
    LOAD_FUNCPTR(vkEnumerateInstanceExtensionProperties);
    LOAD_FUNCPTR(vkGetDeviceProcAddr);
    LOAD_FUNCPTR(vkGetInstanceProcAddr);
    LOAD_OPTIONAL_FUNCPTR(vkGetPhysicalDeviceSurfaceCapabilities2KHR);
    LOAD_FUNCPTR(vkGetPhysicalDeviceSurfaceCapabilitiesKHR);
    LOAD_OPTIONAL_FUNCPTR(vkGetPhysicalDeviceSurfaceFormats2KHR);
    LOAD_FUNCPTR(vkGetPhysicalDeviceSurfaceFormatsKHR);
    LOAD_FUNCPTR(vkGetPhysicalDeviceSurfacePresentModesKHR);
    LOAD_FUNCPTR(vkGetPhysicalDeviceSurfaceSupportKHR);
    LOAD_FUNCPTR(vkGetPhysicalDeviceXlibPresentationSupportKHR);
    LOAD_FUNCPTR(vkGetSwapchainImagesKHR);
    LOAD_FUNCPTR(vkQueuePresentKHR);
    LOAD_OPTIONAL_FUNCPTR(vkGetDeviceGroupSurfacePresentModesKHR);
    LOAD_OPTIONAL_FUNCPTR(vkGetPhysicalDevicePresentRectanglesKHR);
    LOAD_FUNCPTR(vkWaitForFences);
    LOAD_FUNCPTR(vkCreateFence);
    LOAD_FUNCPTR(vkDestroyFence);
#undef LOAD_FUNCPTR
#undef LOAD_OPTIONAL_FUNCPTR

    vulkan_swapchain_context = XUniqueContext();

    return TRUE;

fail:
    dlclose(vulkan_handle);
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

    if (surface->entry.next)
    {
        EnterCriticalSection(&context_section);
        list_remove(&surface->entry);
        LeaveCriticalSection(&context_section);
    }

    if (surface->window)
        XDestroyWindow(gdi_display, surface->window);

    heap_free(surface);
}

void wine_vk_surface_destroy(struct wine_vk_surface *surface)
{
    TRACE("Detaching surface %p, hwnd %p.\n", surface, surface->hwnd);
    XReparentWindow(gdi_display, surface->window, get_dummy_parent(), 0, 0);
    XSync(gdi_display, False);

    if (surface->hdc) ReleaseDC(surface->hwnd, surface->hdc);
    surface->hwnd_thread_id = 0;
    surface->hwnd = 0;
    surface->hdc = 0;
    wine_vk_surface_release(surface);
}

void destroy_vk_surface(HWND hwnd)
{
    struct wine_vk_surface *surface, *next;
    EnterCriticalSection(&context_section);
    LIST_FOR_EACH_ENTRY_SAFE(surface, next, &surface_list, struct wine_vk_surface, entry)
    {
        if (surface->hwnd != hwnd)
            continue;
        wine_vk_surface_destroy(surface);
    }
    LeaveCriticalSection(&context_section);
}

static BOOL wine_vk_surface_set_offscreen(struct wine_vk_surface *surface, BOOL offscreen)
{
#ifdef SONAME_LIBXCOMPOSITE
    if (usexcomposite)
    {
        if (!surface->offscreen && offscreen)
        {
            FIXME("Redirecting vulkan surface offscreen, expect degraded performance.\n");
            pXCompositeRedirectWindow(gdi_display, surface->window, CompositeRedirectManual);
        }
        else if (surface->offscreen && !offscreen)
        {
            FIXME("Putting vulkan surface back onscreen, expect standard performance.\n");
            pXCompositeUnredirectWindow(gdi_display, surface->window, CompositeRedirectManual);
        }
        surface->offscreen = offscreen;
        return TRUE;
    }
#endif

    if (offscreen) FIXME("Application requires child window rendering, which is not implemented yet!\n");
    surface->offscreen = offscreen;
    return !offscreen;
}

void resize_vk_surfaces(HWND hwnd, Window active, int mask, XWindowChanges *changes)
{
    struct wine_vk_surface *surface;
    EnterCriticalSection(&context_section);
    LIST_FOR_EACH_ENTRY(surface, &surface_list, struct wine_vk_surface, entry)
    {
        if (surface->hwnd != hwnd) continue;
        if (surface->window != active) XConfigureWindow(gdi_display, surface->window, mask, changes);
    }
    LeaveCriticalSection(&context_section);
}

void sync_vk_surface(HWND hwnd, BOOL known_child)
{
    struct wine_vk_surface *surface;
    DWORD surface_with_swapchain_count = 0;

    EnterCriticalSection(&context_section);
    LIST_FOR_EACH_ENTRY(surface, &surface_list, struct wine_vk_surface, entry)
    {
        if (surface->hwnd != hwnd) continue;
        if (surface->swapchain_count) surface_with_swapchain_count++;
        surface->known_child = known_child;
    }
    TRACE("hwnd %p surface_with_swapchain_count %u known_child %u\n", hwnd, surface_with_swapchain_count, known_child);
    LIST_FOR_EACH_ENTRY(surface, &surface_list, struct wine_vk_surface, entry)
    {
        if (surface->hwnd != hwnd) continue;
        if (surface_with_swapchain_count > 1) wine_vk_surface_set_offscreen(surface, TRUE);
        else wine_vk_surface_set_offscreen(surface, known_child);
    }
    LeaveCriticalSection(&context_section);
}

Window wine_vk_active_surface(HWND hwnd)
{
    struct wine_vk_surface *surface, *active = NULL;
    DWORD surface_with_swapchain_count = 0;
    Window window;

    EnterCriticalSection(&context_section);
    LIST_FOR_EACH_ENTRY(surface, &surface_list, struct wine_vk_surface, entry)
    {
        if (surface->hwnd != hwnd) continue;
        if (!surface->swapchain_count) continue;
        active = surface;
        surface_with_swapchain_count++;
    }
    if (!active) window = None;
    else
    {
        TRACE("hwnd %p surface_with_swapchain_count %u known_child %u\n", hwnd, surface_with_swapchain_count, active->known_child);
        if (surface_with_swapchain_count > 1) wine_vk_surface_set_offscreen(active, TRUE);
        else wine_vk_surface_set_offscreen(active, active->known_child);
        window = active->window;
    }
    LeaveCriticalSection(&context_section);

    return window;
}

void vulkan_thread_detach(void)
{
    struct wine_vk_surface *surface, *next;
    DWORD thread_id = GetCurrentThreadId();

    EnterCriticalSection(&context_section);
    LIST_FOR_EACH_ENTRY_SAFE(surface, next, &surface_list, struct wine_vk_surface, entry)
    {
        if (surface->hwnd_thread_id != thread_id)
            continue;
        wine_vk_surface_destroy(surface);
    }
    LeaveCriticalSection(&context_section);
}

static VkResult X11DRV_create_vk_instance_with_callback(const VkInstanceCreateInfo *create_info,
        const VkAllocationCallbacks *allocator, VkInstance *instance,
        VkResult (WINAPI *native_vkCreateInstance)(const VkInstanceCreateInfo *, const VkAllocationCallbacks *,
        VkInstance *, void * (*)(VkInstance, const char *), void *), void *native_vkCreateInstance_context)
{
    VkInstanceCreateInfo create_info_host;
    VkResult res;
    TRACE("create_info %p, allocator %p, instance %p, native_vkCreateInstance %p, context %p.\n",
            create_info, allocator, instance, native_vkCreateInstance, native_vkCreateInstance_context);

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

    if (native_vkCreateInstance)
        res = native_vkCreateInstance(&create_info_host, NULL /* allocator */, instance,
                pvkGetInstanceProcAddr, native_vkCreateInstance_context);
    else
        res = pvkCreateInstance(&create_info_host, NULL /* allocator */, instance);

    heap_free((void *)create_info_host.ppEnabledExtensionNames);
    return res;
}

static VkResult X11DRV_vkAcquireNextImageKHR(VkDevice device,
        VkSwapchainKHR swapchain, uint64_t timeout, VkSemaphore semaphore,
        VkFence fence, uint32_t *image_index)
{
    struct wine_vk_surface *surface = NULL;
    VkResult result;
    VkFence orig_fence;
    BOOL wait_fence = FALSE;
    HDC hdc = 0;

    EnterCriticalSection(&context_section);
    if (!XFindContext(gdi_display, (XID)swapchain, vulkan_swapchain_context, (char **)&surface))
    {
        wine_vk_surface_grab(surface);
        hdc = surface->hdc;
    }
    LeaveCriticalSection(&context_section);

    if (!surface || !surface->offscreen)
        wait_fence = FALSE;
    else if (surface->present_mode == VK_PRESENT_MODE_MAILBOX_KHR ||
             surface->present_mode == VK_PRESENT_MODE_FIFO_KHR)
        wait_fence = TRUE;

    orig_fence = fence;
    if (wait_fence && !fence)
    {
        VkFenceCreateInfo create_info;
        create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        create_info.pNext = NULL;
        create_info.flags = 0;
        pvkCreateFence(device, &create_info, NULL, &fence);
    }

    result = pvkAcquireNextImageKHR(device, swapchain, timeout, semaphore, fence, image_index);
    if ((result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR) && hdc && surface && surface->offscreen)
    {
        if (wait_fence) pvkWaitForFences(device, 1, &fence, 0, timeout);
    }

    if (fence != orig_fence) pvkDestroyFence(device, fence, NULL);
    if (surface) wine_vk_surface_release(surface);
    return result;
}

static VkResult X11DRV_vkAcquireNextImage2KHR(VkDevice device,
        const VkAcquireNextImageInfoKHR *acquire_info, uint32_t *image_index)
{
    static int once;
    if (!once++) FIXME("Emulating vkGetPhysicalDeviceSurfaceCapabilities2KHR with vkGetPhysicalDeviceSurfaceCapabilitiesKHR, pNext is ignored.\n");
    return X11DRV_vkAcquireNextImageKHR(device, acquire_info->swapchain, acquire_info->timeout, acquire_info->semaphore, acquire_info->fence, image_index);
}

static VkResult X11DRV_vkCreateInstance(const VkInstanceCreateInfo *create_info,
        const VkAllocationCallbacks *allocator, VkInstance *instance)
{
    return X11DRV_create_vk_instance_with_callback(create_info, allocator, instance, NULL, NULL);
}

static VkResult X11DRV_vkCreateSwapchainKHR(VkDevice device,
        const VkSwapchainCreateInfoKHR *create_info,
        const VkAllocationCallbacks *allocator, VkSwapchainKHR *swapchain)
{
    struct wine_vk_surface *other, *x11_surface = surface_from_handle(create_info->surface);
    VkSwapchainCreateInfoKHR create_info_host;
    VkResult result;

    TRACE("%p %p %p %p\n", device, create_info, allocator, swapchain);

    if (allocator)
        FIXME("Support for allocation callbacks not implemented yet\n");

    if (!x11_surface->hwnd)
        return VK_ERROR_SURFACE_LOST_KHR;

    create_info_host = *create_info;
    create_info_host.surface = x11_surface->surface;

    /* force fifo when running offscreen so the acquire fence is more likely to be vsynced */
    if (x11_surface->offscreen && create_info->presentMode == VK_PRESENT_MODE_MAILBOX_KHR)
        create_info_host.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    x11_surface->present_mode = create_info->presentMode;

    EnterCriticalSection(&context_section);
    LIST_FOR_EACH_ENTRY(other, &surface_list, struct wine_vk_surface, entry)
    {
        if (other->hwnd != x11_surface->hwnd) continue;
        if (!other->swapchain_count) continue;
        TRACE("hwnd %p already has a swapchain, moving surface offscreen\n", x11_surface->hwnd);
        wine_vk_surface_set_offscreen(other, TRUE);
        wine_vk_surface_set_offscreen(x11_surface, TRUE);
    }
    result = pvkCreateSwapchainKHR(device, &create_info_host, NULL /* allocator */, swapchain);
    if (result == VK_SUCCESS)
    {
        x11_surface->swapchain_count++;
        XSaveContext(gdi_display, (XID)(*swapchain), vulkan_swapchain_context, (char *)wine_vk_surface_grab(x11_surface));
    }
    LeaveCriticalSection(&context_section);
    return result;
}

static VkResult X11DRV_vkCreateWin32SurfaceKHR(VkInstance instance,
        const VkWin32SurfaceCreateInfoKHR *create_info,
        const VkAllocationCallbacks *allocator, VkSurfaceKHR *surface)
{
    VkResult res;
    VkXlibSurfaceCreateInfoKHR create_info_host;
    struct wine_vk_surface *x11_surface;

    TRACE("%p %p %p %p\n", instance, create_info, allocator, surface);

    if (allocator)
        FIXME("Support for allocation callbacks not implemented yet\n");

    x11_surface = heap_alloc_zero(sizeof(*x11_surface));
    if (!x11_surface)
        return VK_ERROR_OUT_OF_HOST_MEMORY;

    x11_surface->ref = 1;
    x11_surface->hwnd = create_info->hwnd;
    x11_surface->known_child = FALSE;
    x11_surface->swapchain_count = 0;
    if (x11_surface->hwnd)
    {
        x11_surface->hdc = GetDC(create_info->hwnd);
        x11_surface->window = create_client_window(create_info->hwnd, &default_visual);
        x11_surface->hwnd_thread_id = GetWindowThreadProcessId(x11_surface->hwnd, NULL);
    }
    else
    {
        x11_surface->window = create_dummy_client_window();
    }

    if (!x11_surface->window)
    {
        ERR("Failed to allocate client window for hwnd=%p\n", create_info->hwnd);

        /* VK_KHR_win32_surface only allows out of host and device memory as errors. */
        res = VK_ERROR_OUT_OF_HOST_MEMORY;
        goto err;
    }

    if (create_info->hwnd && (GetWindow( create_info->hwnd, GW_CHILD ) || GetAncestor( create_info->hwnd, GA_PARENT ) != GetDesktopWindow()))
    {
        x11_surface->known_child = TRUE;
        TRACE("hwnd %p creating offscreen child window surface\n", x11_surface->hwnd);
        if (!wine_vk_surface_set_offscreen(x11_surface, TRUE))
        {
            res = VK_ERROR_INCOMPATIBLE_DRIVER;
            goto err;
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
    list_add_tail(&surface_list, &x11_surface->entry);
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
    HWND hwnd = x11_surface->hwnd;

    TRACE("%p 0x%s %p\n", instance, wine_dbgstr_longlong(surface), allocator);

    if (allocator)
        FIXME("Support for allocation callbacks not implemented yet\n");

    /* vkDestroySurfaceKHR must handle VK_NULL_HANDLE (0) for surface. */
    if (x11_surface)
    {
        pvkDestroySurfaceKHR(instance, x11_surface->surface, NULL /* allocator */);

        wine_vk_surface_release(x11_surface);
        update_client_window(hwnd);
    }
}

static void X11DRV_vkDestroySwapchainKHR(VkDevice device, VkSwapchainKHR swapchain,
         const VkAllocationCallbacks *allocator)
{
    struct wine_vk_surface *surface;

    TRACE("%p, 0x%s %p\n", device, wine_dbgstr_longlong(swapchain), allocator);

    if (allocator)
        FIXME("Support for allocation callbacks not implemented yet\n");

    pvkDestroySwapchainKHR(device, swapchain, NULL /* allocator */);

    EnterCriticalSection(&context_section);
    if (!XFindContext(gdi_display, (XID)swapchain, vulkan_swapchain_context, (char **)&surface))
    {
        surface->swapchain_count--;
        wine_vk_surface_release(surface);
    }
    XDeleteContext(gdi_display, (XID)swapchain, vulkan_swapchain_context);
    LeaveCriticalSection(&context_section);
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

static const char *wine_vk_native_fn_name(const char *name)
{
    if (!strcmp(name, "vkCreateWin32SurfaceKHR"))
        return "vkCreateXlibSurfaceKHR";
    if (!strcmp(name, "vkGetPhysicalDeviceWin32PresentationSupportKHR"))
        return "vkGetPhysicalDeviceXlibPresentationSupportKHR";

    return name;
}

static void *X11DRV_vkGetDeviceProcAddr(VkDevice device, const char *name)
{
    void *proc_addr;

    TRACE("%p, %s\n", device, debugstr_a(name));

    if (!pvkGetDeviceProcAddr(device, wine_vk_native_fn_name(name)))
        return NULL;

    if ((proc_addr = X11DRV_get_vk_device_proc_addr(name)))
        return proc_addr;

    return pvkGetDeviceProcAddr(device, name);
}

static void *X11DRV_vkGetInstanceProcAddr(VkInstance instance, const char *name)
{
    void *proc_addr;

    TRACE("%p, %s\n", instance, debugstr_a(name));

    if (!pvkGetInstanceProcAddr(instance, wine_vk_native_fn_name(name)))
        return NULL;

    if ((proc_addr = X11DRV_get_vk_instance_proc_addr(instance, name)))
        return proc_addr;

    return pvkGetInstanceProcAddr(instance, name);
}

static VkResult X11DRV_vkGetPhysicalDevicePresentRectanglesKHR(VkPhysicalDevice phys_dev,
        VkSurfaceKHR surface, uint32_t *count, VkRect2D *rects)
{
    struct wine_vk_surface *x11_surface = surface_from_handle(surface);

    TRACE("%p, 0x%s, %p, %p\n", phys_dev, wine_dbgstr_longlong(surface), count, rects);

    if (!x11_surface->hwnd)
    {
        if (rects)
            return VK_ERROR_SURFACE_LOST_KHR;

        *count = 1;
        return VK_SUCCESS;
    }

    return pvkGetPhysicalDevicePresentRectanglesKHR(phys_dev, x11_surface->surface, count, rects);
}

static VkResult X11DRV_vkGetPhysicalDeviceSurfaceCapabilities2KHR(VkPhysicalDevice phys_dev,
        const VkPhysicalDeviceSurfaceInfo2KHR *surface_info, VkSurfaceCapabilities2KHR *capabilities)
{
    VkPhysicalDeviceSurfaceInfo2KHR surface_info_host;
    TRACE("%p, %p, %p\n", phys_dev, surface_info, capabilities);

    surface_info_host = *surface_info;
    surface_info_host.surface = surface_from_handle(surface_info->surface)->surface;

    if (pvkGetPhysicalDeviceSurfaceCapabilities2KHR)
        return pvkGetPhysicalDeviceSurfaceCapabilities2KHR(phys_dev, &surface_info_host, capabilities);

    /* Until the loader version exporting this function is common, emulate it using the older non-2 version. */
    if (surface_info->pNext || capabilities->pNext)
        FIXME("Emulating vkGetPhysicalDeviceSurfaceCapabilities2KHR with vkGetPhysicalDeviceSurfaceCapabilitiesKHR, pNext is ignored.\n");

    return pvkGetPhysicalDeviceSurfaceCapabilitiesKHR(phys_dev, surface_info_host.surface, &capabilities->surfaceCapabilities);
}

static VkResult X11DRV_vkGetPhysicalDeviceSurfaceCapabilitiesKHR(VkPhysicalDevice phys_dev,
        VkSurfaceKHR surface, VkSurfaceCapabilitiesKHR *capabilities)
{
    struct wine_vk_surface *x11_surface = surface_from_handle(surface);

    TRACE("%p, 0x%s, %p\n", phys_dev, wine_dbgstr_longlong(surface), capabilities);

    if (!x11_surface->hwnd)
        return VK_ERROR_SURFACE_LOST_KHR;

    return pvkGetPhysicalDeviceSurfaceCapabilitiesKHR(phys_dev, x11_surface->surface, capabilities);
}

static VkResult X11DRV_vkGetPhysicalDeviceSurfaceFormats2KHR(VkPhysicalDevice phys_dev,
        const VkPhysicalDeviceSurfaceInfo2KHR *surface_info, uint32_t *count, VkSurfaceFormat2KHR *formats)
{
    VkPhysicalDeviceSurfaceInfo2KHR surface_info_host = *surface_info;
    VkSurfaceFormatKHR *formats_host;
    uint32_t i;
    VkResult result;
    TRACE("%p, %p, %p, %p\n", phys_dev, surface_info, count, formats);

    surface_info_host = *surface_info;
    surface_info_host.surface = surface_from_handle(surface_info->surface)->surface;

    if (pvkGetPhysicalDeviceSurfaceFormats2KHR)
        return pvkGetPhysicalDeviceSurfaceFormats2KHR(phys_dev, &surface_info_host, count, formats);

    /* Until the loader version exporting this function is common, emulate it using the older non-2 version. */
    if (surface_info->pNext)
        FIXME("Emulating vkGetPhysicalDeviceSurfaceFormats2KHR with vkGetPhysicalDeviceSurfaceFormatsKHR, pNext is ignored.\n");

    if (!formats)
        return pvkGetPhysicalDeviceSurfaceFormatsKHR(phys_dev, surface_info_host.surface, count, NULL);

    formats_host = heap_calloc(*count, sizeof(*formats_host));
    if (!formats_host) return VK_ERROR_OUT_OF_HOST_MEMORY;
    result = pvkGetPhysicalDeviceSurfaceFormatsKHR(phys_dev, surface_info_host.surface, count, formats_host);
    if (result == VK_SUCCESS || result == VK_INCOMPLETE)
    {
        for (i = 0; i < *count; i++)
            formats[i].surfaceFormat = formats_host[i];
    }

    heap_free(formats_host);
    return result;
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

    VkSwapchainKHR swapchain;

    static int once;
    struct x11drv_escape_present_drawable escape;
    struct wine_vk_surface *surface = NULL;
    VkFence orig_fence;
    BOOL wait_fence = FALSE;
    HDC hdc = 0;

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

    if(res == VK_SUCCESS) 
    {
        for(uint32_t i = 0; i < present_info->swapchainCount; i++) 
        {
            swapchain = present_info->pSwapchains[i];

            EnterCriticalSection(&context_section);
            if (!XFindContext(gdi_display, (XID)swapchain, vulkan_swapchain_context, (char **)&surface))
            {
                wine_vk_surface_grab(surface);
                hdc = surface->hdc;
            }
            LeaveCriticalSection(&context_section);

            if (hdc && surface && surface->offscreen)
            {
                escape.code = X11DRV_PRESENT_DRAWABLE;
                escape.drawable = surface->window;
                escape.flush = TRUE;

                ExtEscape(hdc, X11DRV_ESCAPE, sizeof(escape), (char *)&escape, 0, NULL);
                if (surface->present_mode == VK_PRESENT_MODE_MAILBOX_KHR)
                    if (once++) FIXME("Application requires child window rendering with mailbox present mode, expect possible tearing!\n");
            }
        }
    }

    return res;
}

static VkSurfaceKHR X11DRV_wine_get_native_surface(VkSurfaceKHR surface)
{
    struct wine_vk_surface *x11_surface = surface_from_handle(surface);

    TRACE("0x%s\n", wine_dbgstr_longlong(surface));

    return x11_surface->surface;
}

static VkBool32 X11DRV_query_fs_hack(VkSurfaceKHR surface, VkExtent2D *real_sz, VkExtent2D *user_sz,
        VkRect2D *dst_blit, VkFilter *filter)
{
    struct wine_vk_surface *x11_surface = surface_from_handle(surface);
    HMONITOR monitor;
    HWND hwnd;

    if (wm_is_steamcompmgr(gdi_display))
        return VK_FALSE;

    if (XFindContext(gdi_display, x11_surface->window, winContext, (char **)&hwnd) != 0)
    {
        ERR("Failed to find hwnd context\n");
        return VK_FALSE;
    }

    monitor = fs_hack_monitor_from_hwnd(hwnd);
    if(fs_hack_enabled(monitor) && !x11_surface->offscreen){
        RECT real_rect = fs_hack_real_mode(monitor);
        RECT user_rect = fs_hack_current_mode(monitor);
        SIZE scaled = fs_hack_get_scaled_screen_size(monitor);
        POINT scaled_origin;

        scaled_origin.x = user_rect.left;
        scaled_origin.y = user_rect.top;
        fs_hack_point_user_to_real(&scaled_origin);
        scaled_origin.x -= real_rect.left;
        scaled_origin.y -= real_rect.top;

        TRACE("real_rect:%s user_rect:%s scaled:%dx%d scaled_origin:%s\n", wine_dbgstr_rect(&real_rect),
              wine_dbgstr_rect(&user_rect), scaled.cx, scaled.cy, wine_dbgstr_point(&scaled_origin));

        if(real_sz){
            real_sz->width = real_rect.right - real_rect.left;
            real_sz->height = real_rect.bottom - real_rect.top;
        }

        if(user_sz){
            user_sz->width = user_rect.right - user_rect.left;
            user_sz->height = user_rect.bottom - user_rect.top;
        }

        if(dst_blit){
            dst_blit->offset.x = scaled_origin.x;
            dst_blit->offset.y = scaled_origin.y;
            dst_blit->extent.width = scaled.cx;
            dst_blit->extent.height = scaled.cy;
        }

        if (filter)
            *filter = fs_hack_is_integer() ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;

        return VK_TRUE;
    }
    else if (fs_hack_enabled(monitor))
    {
        double scale = fs_hack_get_user_to_real_scale( monitor );
        RECT real_rect = fs_hack_real_mode(monitor);
        RECT client_rect;

        GetClientRect( hwnd, &client_rect );

        TRACE("rect %s, scale %lf.\n", wine_dbgstr_rect(&client_rect), scale);

        if (real_sz)
        {
            real_sz->width = real_rect.right - real_rect.left;
            real_sz->height = real_rect.bottom - real_rect.top;
        }

        if (user_sz)
        {
            user_sz->width = client_rect.right - client_rect.left;
            user_sz->height = client_rect.bottom - client_rect.top;
        }

        if (dst_blit)
        {
            /* FIXME: The non-fullscreen Windows are not scaled by fs_hack, so X window position and size
             * is not scaled in this case. */
            dst_blit->offset.x = client_rect.left * scale;
            dst_blit->offset.y = client_rect.top * scale;
            dst_blit->extent.width = (client_rect.right - client_rect.left) * scale;
            dst_blit->extent.height = (client_rect.bottom - client_rect.top) * scale;
        }

        if(filter)
            *filter = fs_hack_is_integer() ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;

        return VK_TRUE;
    }

    return VK_FALSE;
}

static const struct vulkan_funcs vulkan_funcs =
{
    X11DRV_vkAcquireNextImage2KHR,
    X11DRV_vkAcquireNextImageKHR,
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

    X11DRV_wine_get_native_surface,
    X11DRV_create_vk_instance_with_callback,
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

void destroy_vk_surface(HWND hwnd)
{
}

void resize_vk_surfaces(HWND hwnd, Window active, int mask, XWindowChanges changes)
{
}

void sync_vk_surface(HWND hwnd, BOOL known_child)
{
}

void vulkan_thread_detach(void)
{
}

#endif /* SONAME_LIBVULKAN */
