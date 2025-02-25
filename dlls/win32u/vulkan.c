/*
 * Vulkan display driver loading
 *
 * Copyright (c) 2017 Roderick Colenbrander
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

#include "config.h"

#include <math.h>
#include <dlfcn.h>
#include <pthread.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "win32u_private.h"
#include "ntuser_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(vulkan);

PFN_vkGetDeviceProcAddr p_vkGetDeviceProcAddr = NULL;
PFN_vkGetInstanceProcAddr p_vkGetInstanceProcAddr = NULL;

static void *vulkan_handle;
static struct vulkan_funcs vulkan_funcs;

#ifdef SONAME_LIBVULKAN

WINE_DECLARE_DEBUG_CHANNEL(fps);

static const struct vulkan_driver_funcs *driver_funcs;

static pthread_mutex_t surface_list_lock = PTHREAD_MUTEX_INITIALIZER;

struct surface
{
    struct vulkan_surface obj;
    void *driver_private;
    HWND hwnd;

    struct list entry;
    struct list temp_entry;
};

static struct surface *surface_from_handle( VkSurfaceKHR handle )
{
    struct vulkan_surface *obj = vulkan_surface_from_handle( handle );
    return CONTAINING_RECORD( obj, struct surface, obj );
}

/* Return whether integer scaling is on */
static BOOL fs_hack_is_integer(void)
{
    static int is_int = -1;
    if (is_int < 0)
    {
        const char *e = getenv( "WINE_FULLSCREEN_INTEGER_SCALING" );
        is_int = e && strcmp( e, "0" );
        TRACE( "is_integer_scaling: %s\n", is_int ? "TRUE" : "FALSE" );
    }
    return is_int;
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

static const char *debugstr_vkextent2d( const VkExtent2D *ext )
{
    if (!ext) return "(null)";
    return wine_dbg_sprintf( "(%d,%d)", (int)ext->width, (int)ext->height );
}

struct swapchain
{
    struct vulkan_swapchain obj;
    struct surface *surface;
    VkExtent2D extents;

    /* fs hack data below */
    BOOL fs_hack_enabled;
    uint32_t raw_monitor_dpi;
    VkExtent2D host_extents;
    VkCommandPool *cmd_pools; /* VkCommandPool[device->queue_count] */
    VkDeviceMemory user_image_memory;
    uint32_t n_images;
    struct fs_hack_image *fs_hack_images; /* struct fs_hack_image[n_images] */
    VkSampler sampler;
    VkDescriptorPool descriptor_pool;
    VkDescriptorSetLayout descriptor_set_layout;
    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline;
};

static struct swapchain *swapchain_from_handle( VkSwapchainKHR handle )
{
    struct vulkan_swapchain *obj = vulkan_swapchain_from_handle( handle );
    return CONTAINING_RECORD( obj, struct swapchain, obj );
}

static VkResult win32u_vkCreateWin32SurfaceKHR( VkInstance client_instance, const VkWin32SurfaceCreateInfoKHR *create_info,
                                                const VkAllocationCallbacks *allocator, VkSurfaceKHR *ret )
{
    struct vulkan_instance *instance = vulkan_instance_from_handle( client_instance );
    VkSurfaceKHR host_surface;
    struct surface *surface;
    HWND dummy = NULL;
    VkResult res;
    WND *win;

    TRACE( "client_instance %p, create_info %p, allocator %p, ret %p\n", client_instance, create_info, allocator, ret );
    if (allocator) FIXME( "Support for allocation callbacks not implemented yet\n" );

    if (!(surface = calloc( 1, sizeof(*surface) ))) return VK_ERROR_OUT_OF_HOST_MEMORY;

    /* Windows allows surfaces to be created with no HWND, they return VK_ERROR_SURFACE_LOST_KHR later */
    if (!(surface->hwnd = create_info->hwnd))
    {
        static const WCHAR staticW[] = {'s','t','a','t','i','c',0};
        UNICODE_STRING static_us = RTL_CONSTANT_STRING( staticW );
        dummy = NtUserCreateWindowEx( 0, &static_us, &static_us, &static_us, WS_POPUP, 0, 0, 0, 0,
                                      NULL, NULL, NULL, NULL, 0, NULL, 0, FALSE );
        WARN( "Created dummy window %p for null surface window\n", dummy );
        surface->hwnd = dummy;
    }

    if ((res = driver_funcs->p_vulkan_surface_create( surface->hwnd, instance, &host_surface, &surface->driver_private )))
    {
        if (dummy) NtUserDestroyWindow( dummy );
        free( surface );
        return res;
    }

    pthread_mutex_lock( &surface_list_lock );
    if (!(win = get_win_ptr( surface->hwnd )) || win == WND_DESKTOP || win == WND_OTHER_PROCESS)
        list_init( &surface->entry );
    else
    {
        list_add_tail( &win->vulkan_surfaces, &surface->entry );
        release_win_ptr( win );
    }
    pthread_mutex_unlock( &surface_list_lock );

    vulkan_object_init( &surface->obj.obj, host_surface );
    surface->obj.instance = instance;
    instance->p_insert_object( instance, &surface->obj.obj );

    if (dummy) NtUserDestroyWindow( dummy );

    *ret = surface->obj.client.surface;
    return VK_SUCCESS;
}

static void win32u_vkDestroySurfaceKHR( VkInstance client_instance, VkSurfaceKHR client_surface,
                                        const VkAllocationCallbacks *allocator )
{
    struct vulkan_instance *instance = vulkan_instance_from_handle( client_instance );
    struct surface *surface = surface_from_handle( client_surface );
    WND *win;

    if (!surface) return;

    TRACE( "instance %p, handle 0x%s, allocator %p\n", instance, wine_dbgstr_longlong( client_surface ), allocator );
    if (allocator) FIXME( "Support for allocation callbacks not implemented yet\n" );

    pthread_mutex_lock( &surface_list_lock );
    if ((win = get_win_ptr( surface->hwnd )) && win != WND_DESKTOP && win != WND_OTHER_PROCESS)
    {
        list_remove( &surface->entry );
        release_win_ptr( win );
    }
    pthread_mutex_unlock( &surface_list_lock );

    instance->p_vkDestroySurfaceKHR( instance->host.instance, surface->obj.host.surface, NULL /* allocator */ );
    driver_funcs->p_vulkan_surface_destroy( surface->hwnd, surface->driver_private );

    instance->p_remove_object( instance, &surface->obj.obj );

    free( surface );
}

static void adjust_surface_capabilities( struct vulkan_instance *instance, struct surface *surface,
                                         VkSurfaceCapabilitiesKHR *capabilities )
{
    RECT client_rect;

    /* Many Windows games, for example Strange Brigade, No Man's Sky, Path of Exile
     * and World War Z, do not expect that maxImageCount can be set to 0.
     * A value of 0 means that there is no limit on the number of images.
     * Nvidia reports 8 on Windows, AMD 16.
     * https://vulkan.gpuinfo.org/displayreport.php?id=9122#surface
     * https://vulkan.gpuinfo.org/displayreport.php?id=9121#surface
     */
    if (!capabilities->maxImageCount) capabilities->maxImageCount = max( capabilities->minImageCount, 16 );

    /* Update the image extents to match what the Win32 WSI would provide. */
    /* FIXME: handle DPI scaling, somehow */
    NtUserGetClientRect( surface->hwnd, &client_rect, NtUserGetDpiForWindow( surface->hwnd ) );
    capabilities->minImageExtent.width = client_rect.right - client_rect.left;
    capabilities->minImageExtent.height = client_rect.bottom - client_rect.top;
    capabilities->maxImageExtent.width = client_rect.right - client_rect.left;
    capabilities->maxImageExtent.height = client_rect.bottom - client_rect.top;
    capabilities->currentExtent.width = client_rect.right - client_rect.left;
    capabilities->currentExtent.height = client_rect.bottom - client_rect.top;
}

static VkResult win32u_vkGetPhysicalDeviceSurfaceCapabilitiesKHR( VkPhysicalDevice client_physical_device, VkSurfaceKHR client_surface,
                                                                  VkSurfaceCapabilitiesKHR *capabilities )
{
    struct vulkan_physical_device *physical_device = vulkan_physical_device_from_handle( client_physical_device );
    struct surface *surface = surface_from_handle( client_surface );
    struct vulkan_instance *instance = physical_device->instance;
    VkResult res;

    if (!NtUserIsWindow( surface->hwnd )) return VK_ERROR_SURFACE_LOST_KHR;
    res = instance->p_vkGetPhysicalDeviceSurfaceCapabilitiesKHR( physical_device->host.physical_device,
                                                       surface->obj.host.surface, capabilities );
    if (!res) adjust_surface_capabilities( instance, surface, capabilities );
    return res;
}

static VkResult win32u_vkGetPhysicalDeviceSurfaceCapabilities2KHR( VkPhysicalDevice client_physical_device, const VkPhysicalDeviceSurfaceInfo2KHR *surface_info,
                                                                   VkSurfaceCapabilities2KHR *capabilities )
{
    struct vulkan_physical_device *physical_device = vulkan_physical_device_from_handle( client_physical_device );
    struct surface *surface = surface_from_handle( surface_info->surface );
    VkPhysicalDeviceSurfaceInfo2KHR surface_info_host = *surface_info;
    struct vulkan_instance *instance = physical_device->instance;
    VkResult res;

    if (!instance->p_vkGetPhysicalDeviceSurfaceCapabilities2KHR)
    {
        /* Until the loader version exporting this function is common, emulate it using the older non-2 version. */
        if (surface_info->pNext || capabilities->pNext) FIXME( "Emulating vkGetPhysicalDeviceSurfaceCapabilities2KHR, ignoring pNext.\n" );
        return win32u_vkGetPhysicalDeviceSurfaceCapabilitiesKHR( client_physical_device, surface_info->surface,
                                                                 &capabilities->surfaceCapabilities );
    }

    surface_info_host.surface = surface->obj.host.surface;

    if (!NtUserIsWindow( surface->hwnd )) return VK_ERROR_SURFACE_LOST_KHR;
    res = instance->p_vkGetPhysicalDeviceSurfaceCapabilities2KHR( physical_device->host.physical_device,
                                                                     &surface_info_host, capabilities );
    if (!res) adjust_surface_capabilities( instance, surface, &capabilities->surfaceCapabilities );
    return res;
}

static VkResult win32u_vkGetPhysicalDevicePresentRectanglesKHR( VkPhysicalDevice client_physical_device, VkSurfaceKHR client_surface,
                                                                uint32_t *rect_count, VkRect2D *rects )
{
    struct vulkan_physical_device *physical_device = vulkan_physical_device_from_handle( client_physical_device );
    struct surface *surface = surface_from_handle( client_surface );
    struct vulkan_instance *instance = physical_device->instance;

    if (!NtUserIsWindow( surface->hwnd ))
    {
        if (rects && !*rect_count) return VK_INCOMPLETE;
        if (rects) memset( rects, 0, sizeof(VkRect2D) );
        *rect_count = 1;
        return VK_SUCCESS;
    }

    return instance->p_vkGetPhysicalDevicePresentRectanglesKHR( physical_device->host.physical_device,
                                                                   surface->obj.host.surface, rect_count, rects );
}

static VkResult win32u_vkGetPhysicalDeviceSurfaceFormatsKHR( VkPhysicalDevice client_physical_device, VkSurfaceKHR client_surface,
                                                             uint32_t *format_count, VkSurfaceFormatKHR *formats )
{
    struct vulkan_physical_device *physical_device = vulkan_physical_device_from_handle( client_physical_device );
    struct surface *surface = surface_from_handle( client_surface );
    struct vulkan_instance *instance = physical_device->instance;

    if (surface->hwnd) vulkan_update_surfaces( surface->hwnd );
    return instance->p_vkGetPhysicalDeviceSurfaceFormatsKHR( physical_device->host.physical_device,
                                                                surface->obj.host.surface, format_count, formats );
}

static VkResult win32u_vkGetPhysicalDeviceSurfaceFormats2KHR( VkPhysicalDevice client_physical_device, const VkPhysicalDeviceSurfaceInfo2KHR *surface_info,
                                                              uint32_t *format_count, VkSurfaceFormat2KHR *formats )
{
    struct vulkan_physical_device *physical_device = vulkan_physical_device_from_handle( client_physical_device );
    struct surface *surface = surface_from_handle( surface_info->surface );
    VkPhysicalDeviceSurfaceInfo2KHR surface_info_host = *surface_info;
    struct vulkan_instance *instance = physical_device->instance;
    VkResult res;

    if (surface->hwnd) vulkan_update_surfaces( surface->hwnd );

    if (!instance->p_vkGetPhysicalDeviceSurfaceFormats2KHR)
    {
        VkSurfaceFormatKHR *surface_formats;
        UINT i;

        /* Until the loader version exporting this function is common, emulate it using the older non-2 version. */
        if (surface_info->pNext) FIXME( "Emulating vkGetPhysicalDeviceSurfaceFormats2KHR, ignoring pNext.\n" );
        if (!formats) return win32u_vkGetPhysicalDeviceSurfaceFormatsKHR( client_physical_device, surface_info->surface, format_count, NULL );

        surface_formats = calloc( *format_count, sizeof(*surface_formats) );
        if (!surface_formats) return VK_ERROR_OUT_OF_HOST_MEMORY;

        res = win32u_vkGetPhysicalDeviceSurfaceFormatsKHR( client_physical_device, surface_info->surface, format_count, surface_formats );
        if (!res || res == VK_INCOMPLETE) for (i = 0; i < *format_count; i++) formats[i].surfaceFormat = surface_formats[i];

        free( surface_formats );
        return res;
    }

    surface_info_host.surface = surface->obj.host.surface;

    return instance->p_vkGetPhysicalDeviceSurfaceFormats2KHR( physical_device->host.physical_device,
                                                                 &surface_info_host, format_count, formats );
}

static VkBool32 win32u_vkGetPhysicalDeviceWin32PresentationSupportKHR( VkPhysicalDevice client_physical_device, uint32_t queue )
{
    struct vulkan_physical_device *physical_device = vulkan_physical_device_from_handle( client_physical_device );
    return driver_funcs->p_vkGetPhysicalDeviceWin32PresentationSupportKHR( physical_device->host.physical_device, queue );
}

/*
#version 460

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(binding = 0) uniform sampler2D texSampler;
layout(binding = 1) uniform writeonly image2D outImage;
layout(push_constant) uniform pushConstants {
    //both in real image coords
    vec2 offset;
    vec2 extents;
} constants;

void main()
{
    vec2 texcoord = (vec2(gl_GlobalInvocationID.xy) - constants.offset) / constants.extents;
    vec4 c = texture(texSampler, texcoord);

    // Convert linear -> srgb
    bvec3 isLo = lessThanEqual(c.rgb, vec3(0.0031308f));
    vec3 loPart = c.rgb * 12.92f;
    vec3 hiPart = pow(c.rgb, vec3(5.0f / 12.0f)) * 1.055f - 0.055f;
    c.rgb = mix(hiPart, loPart, isLo);

    imageStore(outImage, ivec2(gl_GlobalInvocationID.xy), c);
}

*/
const uint32_t blit_comp_spv[] =
{
    0x07230203, 0x00010000, 0x0008000a, 0x0000005e, 0x00000000, 0x00020011, 0x00000001, 0x00020011,
    0x00000038, 0x0006000b, 0x00000001, 0x4c534c47, 0x6474732e, 0x3035342e, 0x00000000, 0x0003000e,
    0x00000000, 0x00000001, 0x0006000f, 0x00000005, 0x00000004, 0x6e69616d, 0x00000000, 0x0000000d,
    0x00060010, 0x00000004, 0x00000011, 0x00000008, 0x00000008, 0x00000001, 0x00030003, 0x00000002,
    0x000001cc, 0x00040005, 0x00000004, 0x6e69616d, 0x00000000, 0x00050005, 0x00000009, 0x63786574,
    0x64726f6f, 0x00000000, 0x00080005, 0x0000000d, 0x475f6c67, 0x61626f6c, 0x766e496c, 0x7461636f,
    0x496e6f69, 0x00000044, 0x00060005, 0x00000012, 0x68737570, 0x736e6f43, 0x746e6174, 0x00000073,
    0x00050006, 0x00000012, 0x00000000, 0x7366666f, 0x00007465, 0x00050006, 0x00000012, 0x00000001,
    0x65747865, 0x0073746e, 0x00050005, 0x00000014, 0x736e6f63, 0x746e6174, 0x00000073, 0x00030005,
    0x00000021, 0x00000063, 0x00050005, 0x00000025, 0x53786574, 0x6c706d61, 0x00007265, 0x00040005,
    0x0000002d, 0x6f4c7369, 0x00000000, 0x00040005, 0x00000035, 0x61506f6c, 0x00007472, 0x00040005,
    0x0000003a, 0x61506968, 0x00007472, 0x00050005, 0x00000055, 0x4974756f, 0x6567616d, 0x00000000,
    0x00040047, 0x0000000d, 0x0000000b, 0x0000001c, 0x00050048, 0x00000012, 0x00000000, 0x00000023,
    0x00000000, 0x00050048, 0x00000012, 0x00000001, 0x00000023, 0x00000008, 0x00030047, 0x00000012,
    0x00000002, 0x00040047, 0x00000025, 0x00000022, 0x00000000, 0x00040047, 0x00000025, 0x00000021,
    0x00000000, 0x00040047, 0x00000055, 0x00000022, 0x00000000, 0x00040047, 0x00000055, 0x00000021,
    0x00000001, 0x00030047, 0x00000055, 0x00000019, 0x00040047, 0x0000005d, 0x0000000b, 0x00000019,
    0x00020013, 0x00000002, 0x00030021, 0x00000003, 0x00000002, 0x00030016, 0x00000006, 0x00000020,
    0x00040017, 0x00000007, 0x00000006, 0x00000002, 0x00040020, 0x00000008, 0x00000007, 0x00000007,
    0x00040015, 0x0000000a, 0x00000020, 0x00000000, 0x00040017, 0x0000000b, 0x0000000a, 0x00000003,
    0x00040020, 0x0000000c, 0x00000001, 0x0000000b, 0x0004003b, 0x0000000c, 0x0000000d, 0x00000001,
    0x00040017, 0x0000000e, 0x0000000a, 0x00000002, 0x0004001e, 0x00000012, 0x00000007, 0x00000007,
    0x00040020, 0x00000013, 0x00000009, 0x00000012, 0x0004003b, 0x00000013, 0x00000014, 0x00000009,
    0x00040015, 0x00000015, 0x00000020, 0x00000001, 0x0004002b, 0x00000015, 0x00000016, 0x00000000,
    0x00040020, 0x00000017, 0x00000009, 0x00000007, 0x0004002b, 0x00000015, 0x0000001b, 0x00000001,
    0x00040017, 0x0000001f, 0x00000006, 0x00000004, 0x00040020, 0x00000020, 0x00000007, 0x0000001f,
    0x00090019, 0x00000022, 0x00000006, 0x00000001, 0x00000000, 0x00000000, 0x00000000, 0x00000001,
    0x00000000, 0x0003001b, 0x00000023, 0x00000022, 0x00040020, 0x00000024, 0x00000000, 0x00000023,
    0x0004003b, 0x00000024, 0x00000025, 0x00000000, 0x0004002b, 0x00000006, 0x00000028, 0x00000000,
    0x00020014, 0x0000002a, 0x00040017, 0x0000002b, 0x0000002a, 0x00000003, 0x00040020, 0x0000002c,
    0x00000007, 0x0000002b, 0x00040017, 0x0000002e, 0x00000006, 0x00000003, 0x0004002b, 0x00000006,
    0x00000031, 0x3b4d2e1c, 0x0006002c, 0x0000002e, 0x00000032, 0x00000031, 0x00000031, 0x00000031,
    0x00040020, 0x00000034, 0x00000007, 0x0000002e, 0x0004002b, 0x00000006, 0x00000038, 0x414eb852,
    0x0004002b, 0x00000006, 0x0000003d, 0x3ed55555, 0x0006002c, 0x0000002e, 0x0000003e, 0x0000003d,
    0x0000003d, 0x0000003d, 0x0004002b, 0x00000006, 0x00000040, 0x3f870a3d, 0x0004002b, 0x00000006,
    0x00000042, 0x3d6147ae, 0x0004002b, 0x0000000a, 0x00000049, 0x00000000, 0x00040020, 0x0000004a,
    0x00000007, 0x00000006, 0x0004002b, 0x0000000a, 0x0000004d, 0x00000001, 0x0004002b, 0x0000000a,
    0x00000050, 0x00000002, 0x00090019, 0x00000053, 0x00000006, 0x00000001, 0x00000000, 0x00000000,
    0x00000000, 0x00000002, 0x00000000, 0x00040020, 0x00000054, 0x00000000, 0x00000053, 0x0004003b,
    0x00000054, 0x00000055, 0x00000000, 0x00040017, 0x00000059, 0x00000015, 0x00000002, 0x0004002b,
    0x0000000a, 0x0000005c, 0x00000008, 0x0006002c, 0x0000000b, 0x0000005d, 0x0000005c, 0x0000005c,
    0x0000004d, 0x00050036, 0x00000002, 0x00000004, 0x00000000, 0x00000003, 0x000200f8, 0x00000005,
    0x0004003b, 0x00000008, 0x00000009, 0x00000007, 0x0004003b, 0x00000020, 0x00000021, 0x00000007,
    0x0004003b, 0x0000002c, 0x0000002d, 0x00000007, 0x0004003b, 0x00000034, 0x00000035, 0x00000007,
    0x0004003b, 0x00000034, 0x0000003a, 0x00000007, 0x0004003d, 0x0000000b, 0x0000000f, 0x0000000d,
    0x0007004f, 0x0000000e, 0x00000010, 0x0000000f, 0x0000000f, 0x00000000, 0x00000001, 0x00040070,
    0x00000007, 0x00000011, 0x00000010, 0x00050041, 0x00000017, 0x00000018, 0x00000014, 0x00000016,
    0x0004003d, 0x00000007, 0x00000019, 0x00000018, 0x00050083, 0x00000007, 0x0000001a, 0x00000011,
    0x00000019, 0x00050041, 0x00000017, 0x0000001c, 0x00000014, 0x0000001b, 0x0004003d, 0x00000007,
    0x0000001d, 0x0000001c, 0x00050088, 0x00000007, 0x0000001e, 0x0000001a, 0x0000001d, 0x0003003e,
    0x00000009, 0x0000001e, 0x0004003d, 0x00000023, 0x00000026, 0x00000025, 0x0004003d, 0x00000007,
    0x00000027, 0x00000009, 0x00070058, 0x0000001f, 0x00000029, 0x00000026, 0x00000027, 0x00000002,
    0x00000028, 0x0003003e, 0x00000021, 0x00000029, 0x0004003d, 0x0000001f, 0x0000002f, 0x00000021,
    0x0008004f, 0x0000002e, 0x00000030, 0x0000002f, 0x0000002f, 0x00000000, 0x00000001, 0x00000002,
    0x000500bc, 0x0000002b, 0x00000033, 0x00000030, 0x00000032, 0x0003003e, 0x0000002d, 0x00000033,
    0x0004003d, 0x0000001f, 0x00000036, 0x00000021, 0x0008004f, 0x0000002e, 0x00000037, 0x00000036,
    0x00000036, 0x00000000, 0x00000001, 0x00000002, 0x0005008e, 0x0000002e, 0x00000039, 0x00000037,
    0x00000038, 0x0003003e, 0x00000035, 0x00000039, 0x0004003d, 0x0000001f, 0x0000003b, 0x00000021,
    0x0008004f, 0x0000002e, 0x0000003c, 0x0000003b, 0x0000003b, 0x00000000, 0x00000001, 0x00000002,
    0x0007000c, 0x0000002e, 0x0000003f, 0x00000001, 0x0000001a, 0x0000003c, 0x0000003e, 0x0005008e,
    0x0000002e, 0x00000041, 0x0000003f, 0x00000040, 0x00060050, 0x0000002e, 0x00000043, 0x00000042,
    0x00000042, 0x00000042, 0x00050083, 0x0000002e, 0x00000044, 0x00000041, 0x00000043, 0x0003003e,
    0x0000003a, 0x00000044, 0x0004003d, 0x0000002e, 0x00000045, 0x0000003a, 0x0004003d, 0x0000002e,
    0x00000046, 0x00000035, 0x0004003d, 0x0000002b, 0x00000047, 0x0000002d, 0x000600a9, 0x0000002e,
    0x00000048, 0x00000047, 0x00000046, 0x00000045, 0x00050041, 0x0000004a, 0x0000004b, 0x00000021,
    0x00000049, 0x00050051, 0x00000006, 0x0000004c, 0x00000048, 0x00000000, 0x0003003e, 0x0000004b,
    0x0000004c, 0x00050041, 0x0000004a, 0x0000004e, 0x00000021, 0x0000004d, 0x00050051, 0x00000006,
    0x0000004f, 0x00000048, 0x00000001, 0x0003003e, 0x0000004e, 0x0000004f, 0x00050041, 0x0000004a,
    0x00000051, 0x00000021, 0x00000050, 0x00050051, 0x00000006, 0x00000052, 0x00000048, 0x00000002,
    0x0003003e, 0x00000051, 0x00000052, 0x0004003d, 0x00000053, 0x00000056, 0x00000055, 0x0004003d,
    0x0000000b, 0x00000057, 0x0000000d, 0x0007004f, 0x0000000e, 0x00000058, 0x00000057, 0x00000057,
    0x00000000, 0x00000001, 0x0004007c, 0x00000059, 0x0000005a, 0x00000058, 0x0004003d, 0x0000001f,
    0x0000005b, 0x00000021, 0x00040063, 0x00000056, 0x0000005a, 0x0000005b, 0x000100fd, 0x00010038,
};

static VkResult create_pipeline( struct vulkan_device *device, struct swapchain *swapchain, VkShaderModule shaderModule )
{
    VkComputePipelineCreateInfo pipelineInfo = {0};
    VkResult res;

    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineInfo.stage.module = shaderModule;
    pipelineInfo.stage.pName = "main";
    pipelineInfo.layout = swapchain->pipeline_layout;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineInfo.basePipelineIndex = -1;

    if ((res = device->p_vkCreateComputePipelines( device->host.device, VK_NULL_HANDLE, 1,
                                                   &pipelineInfo, NULL, &swapchain->pipeline )))
    {
        ERR( "vkCreateComputePipelines: %d\n", res );
        return res;
    }

    return VK_SUCCESS;
}

static VkResult create_descriptor_set( struct vulkan_device *device, struct swapchain *swapchain, struct fs_hack_image *hack )
{
    VkDescriptorImageInfo userDescriptorImageInfo = {0}, realDescriptorImageInfo = {0};
    VkDescriptorSetAllocateInfo descriptorAllocInfo = {0};
    VkWriteDescriptorSet descriptorWrites[2] = {{0}, {0}};
    VkResult res;

    descriptorAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptorAllocInfo.descriptorPool = swapchain->descriptor_pool;
    descriptorAllocInfo.descriptorSetCount = 1;
    descriptorAllocInfo.pSetLayouts = &swapchain->descriptor_set_layout;

    if ((res = device->p_vkAllocateDescriptorSets( device->host.device, &descriptorAllocInfo, &hack->descriptor_set )))
    {
        ERR( "vkAllocateDescriptorSets: %d\n", res );
        return res;
    }

    userDescriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    userDescriptorImageInfo.imageView = hack->user_view;
    userDescriptorImageInfo.sampler = swapchain->sampler;

    realDescriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    realDescriptorImageInfo.imageView = hack->blit_view;

    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = hack->descriptor_set;
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pImageInfo = &userDescriptorImageInfo;

    descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[1].dstSet = hack->descriptor_set;
    descriptorWrites[1].dstBinding = 1;
    descriptorWrites[1].dstArrayElement = 0;
    descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].pImageInfo = &realDescriptorImageInfo;

    device->p_vkUpdateDescriptorSets( device->host.device, 2, descriptorWrites, 0, NULL );
    return VK_SUCCESS;
}

static VkResult init_blit_images( struct vulkan_device *device, struct swapchain *swapchain )
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
    VkImageViewCreateInfo viewInfo = {0};
    uint32_t i;

    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = samplerInfo.minFilter = fs_hack_is_integer() ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1;
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;

    if ((res = device->p_vkCreateSampler( device->host.device, &samplerInfo, NULL, &swapchain->sampler )))
    {
        WARN( "vkCreateSampler failed, res=%d\n", res );
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

    if ((res = device->p_vkCreateDescriptorPool( device->host.device, &poolInfo, NULL, &swapchain->descriptor_pool )))
    {
        ERR( "vkCreateDescriptorPool: %d\n", res );
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

    if ((res = device->p_vkCreateDescriptorSetLayout( device->host.device, &descriptorLayoutInfo,
                                                      NULL, &swapchain->descriptor_set_layout )))
    {
        ERR( "vkCreateDescriptorSetLayout: %d\n", res );
        goto fail;
    }

    pushConstants.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushConstants.offset = 0;
    pushConstants.size = 4 * sizeof(float); /* 2 * vec2 */

    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &swapchain->descriptor_set_layout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstants;

    if ((res = device->p_vkCreatePipelineLayout( device->host.device, &pipelineLayoutInfo, NULL,
                                                 &swapchain->pipeline_layout )))
    {
        ERR( "vkCreatePipelineLayout: %d\n", res );
        goto fail;
    }

    shaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderInfo.codeSize = sizeof(blit_comp_spv);
    shaderInfo.pCode = blit_comp_spv;

    if ((res = device->p_vkCreateShaderModule( device->host.device, &shaderInfo, NULL, &shaderModule )))
    {
        ERR( "vkCreateShaderModule: %d\n", res );
        goto fail;
    }

    if ((res = create_pipeline( device, swapchain, shaderModule ))) goto fail;

    device->p_vkDestroyShaderModule( device->host.device, shaderModule, NULL );

    for (i = 0; i < swapchain->n_images; ++i)
    {
        struct fs_hack_image *hack = &swapchain->fs_hack_images[i];

        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = hack->swapchain_image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = VK_FORMAT_B8G8R8A8_UNORM;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        if ((res = device->p_vkCreateImageView( device->host.device, &viewInfo, NULL, &hack->blit_view )))
        {
            ERR( "vkCreateImageView(blit): %d\n", res );
            goto fail;
        }

        if ((res = create_descriptor_set( device, swapchain, hack ))) goto fail;
    }

    return VK_SUCCESS;

fail:
    for (i = 0; i < swapchain->n_images; ++i)
    {
        struct fs_hack_image *hack = &swapchain->fs_hack_images[i];

        device->p_vkDestroyImageView( device->host.device, hack->blit_view, NULL );
        hack->blit_view = VK_NULL_HANDLE;
    }

    device->p_vkDestroyShaderModule( device->host.device, shaderModule, NULL );

    device->p_vkDestroyPipeline( device->host.device, swapchain->pipeline, NULL );
    swapchain->pipeline = VK_NULL_HANDLE;

    device->p_vkDestroyPipelineLayout( device->host.device, swapchain->pipeline_layout, NULL );
    swapchain->pipeline_layout = VK_NULL_HANDLE;

    device->p_vkDestroyDescriptorSetLayout( device->host.device, swapchain->descriptor_set_layout, NULL );
    swapchain->descriptor_set_layout = VK_NULL_HANDLE;

    device->p_vkDestroyDescriptorPool( device->host.device, swapchain->descriptor_pool, NULL );
    swapchain->descriptor_pool = VK_NULL_HANDLE;

    device->p_vkDestroySampler( device->host.device, swapchain->sampler, NULL );
    swapchain->sampler = VK_NULL_HANDLE;

    return res;
}

static void destroy_fs_hack_image( struct vulkan_device *device, struct swapchain *swapchain, struct fs_hack_image *hack )
{
    device->p_vkDestroyImageView( device->host.device, hack->user_view, NULL );
    device->p_vkDestroyImageView( device->host.device, hack->blit_view, NULL );
    device->p_vkDestroyImage( device->host.device, hack->user_image, NULL );
    if (hack->cmd) device->p_vkFreeCommandBuffers( device->host.device, swapchain->cmd_pools[hack->cmd_queue_idx], 1, &hack->cmd );
    device->p_vkDestroySemaphore( device->host.device, hack->blit_finished, NULL );
}

static VkResult init_fs_hack_images( struct vulkan_device *device, struct swapchain *swapchain,
                                     const VkSwapchainCreateInfoKHR *createinfo )
{
    struct vulkan_physical_device *physical_device = device->physical_device;
    struct vulkan_instance *instance = physical_device->instance;
    VkResult res;
    VkImage *real_images = NULL;
    VkDeviceSize userMemTotal = 0, offs;
    VkImageCreateInfo imageInfo = {0};
    VkSemaphoreCreateInfo semaphoreInfo = {0};
    VkMemoryRequirements userMemReq;
    VkMemoryAllocateInfo allocInfo = {0};
    VkPhysicalDeviceMemoryProperties memProperties;
    VkImageViewCreateInfo viewInfo = {0};
    uint32_t count, i = 0, user_memory_type = -1;

    if ((res = device->p_vkGetSwapchainImagesKHR( device->host.device, swapchain->obj.host.swapchain, &count, NULL )))
    {
        WARN( "vkGetSwapchainImagesKHR failed, res=%d\n", res );
        return res;
    }

    real_images = malloc( count * sizeof(VkImage) );
    swapchain->cmd_pools = calloc( device->queue_count, sizeof(VkCommandPool) );
    swapchain->fs_hack_images = calloc( count, sizeof(struct fs_hack_image) );
    if (!real_images || !swapchain->cmd_pools || !swapchain->fs_hack_images) goto fail;

    if ((res = device->p_vkGetSwapchainImagesKHR( device->host.device, swapchain->obj.host.swapchain, &count, real_images )))
    {
        WARN( "vkGetSwapchainImagesKHR failed, res=%d\n", res );
        goto fail;
    }

    /* create user images */
    for (i = 0; i < count; ++i)
    {
        struct fs_hack_image *hack = &swapchain->fs_hack_images[i];

        hack->swapchain_image = real_images[i];

        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        if ((res = device->p_vkCreateSemaphore( device->host.device, &semaphoreInfo, NULL, &hack->blit_finished )))
        {
            WARN( "vkCreateSemaphore failed, res=%d\n", res );
            goto fail;
        }

        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = swapchain->extents.width;
        imageInfo.extent.height = swapchain->extents.height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = createinfo->imageArrayLayers;
        imageInfo.format = createinfo->imageFormat;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = createinfo->imageUsage | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageInfo.sharingMode = createinfo->imageSharingMode;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.queueFamilyIndexCount = createinfo->queueFamilyIndexCount;
        imageInfo.pQueueFamilyIndices = createinfo->pQueueFamilyIndices;

        if (createinfo->flags & VK_SWAPCHAIN_CREATE_MUTABLE_FORMAT_BIT_KHR)
            imageInfo.flags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT | VK_IMAGE_CREATE_EXTENDED_USAGE_BIT;
        else if (createinfo->imageFormat != VK_FORMAT_B8G8R8A8_SRGB)
            imageInfo.flags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;

        if ((res = device->p_vkCreateImage( device->host.device, &imageInfo, NULL, &hack->user_image )))
        {
            ERR( "vkCreateImage failed: %d\n", res );
            goto fail;
        }

        device->p_vkGetImageMemoryRequirements( device->host.device, hack->user_image, &userMemReq );

        offs = userMemTotal % userMemReq.alignment;
        if (offs) userMemTotal += userMemReq.alignment - offs;

        userMemTotal += userMemReq.size;

        swapchain->n_images++;
    }

    /* allocate backing memory */
    instance->p_vkGetPhysicalDeviceMemoryProperties( physical_device->host.physical_device, &memProperties );

    for (i = 0; i < memProperties.memoryTypeCount; i++)
    {
        UINT flag = memProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        if (flag == VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
        {
            if (userMemReq.memoryTypeBits & (1 << i))
            {
                user_memory_type = i;
                break;
            }
        }
    }

    if (user_memory_type == -1)
    {
        ERR( "unable to find suitable memory type\n" );
        res = VK_ERROR_OUT_OF_HOST_MEMORY;
        goto fail;
    }

    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = userMemTotal;
    allocInfo.memoryTypeIndex = user_memory_type;

    if ((res = device->p_vkAllocateMemory( device->host.device, &allocInfo, NULL, &swapchain->user_image_memory )))
    {
        ERR( "vkAllocateMemory: %d\n", res );
        goto fail;
    }

    /* bind backing memory and create imageviews */
    userMemTotal = 0;
    for (i = 0; i < count; ++i)
    {
        device->p_vkGetImageMemoryRequirements( device->host.device, swapchain->fs_hack_images[i].user_image, &userMemReq );

        offs = userMemTotal % userMemReq.alignment;
        if (offs) userMemTotal += userMemReq.alignment - offs;

        if ((res = device->p_vkBindImageMemory( device->host.device, swapchain->fs_hack_images[i].user_image,
                                                swapchain->user_image_memory, userMemTotal )))
        {
            ERR( "vkBindImageMemory: %d\n", res );
            goto fail;
        }

        userMemTotal += userMemReq.size;

        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = swapchain->fs_hack_images[i].user_image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = VK_FORMAT_B8G8R8A8_SRGB;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        if ((res = device->p_vkCreateImageView( device->host.device, &viewInfo, NULL,
                                                &swapchain->fs_hack_images[i].user_view )))
        {
            ERR( "vkCreateImageView(user): %d\n", res );
            goto fail;
        }
    }

    free( real_images );

    return VK_SUCCESS;

fail:
    for (i = 0; i < swapchain->n_images; ++i) destroy_fs_hack_image( device, swapchain, &swapchain->fs_hack_images[i] );
    free( real_images );
    free( swapchain->cmd_pools );
    free( swapchain->fs_hack_images );
    return res;
}

static VkResult win32u_vkCreateSwapchainKHR( VkDevice client_device, const VkSwapchainCreateInfoKHR *create_info,
                                             const VkAllocationCallbacks *allocator, VkSwapchainKHR *ret )
{
    struct swapchain *swapchain, *old_swapchain = swapchain_from_handle( create_info->oldSwapchain );
    struct surface *surface = surface_from_handle( create_info->surface );
    struct vulkan_device *device = vulkan_device_from_handle( client_device );
    struct vulkan_physical_device *physical_device = device->physical_device;
    struct vulkan_instance *instance = physical_device->instance;
    VkSwapchainCreateInfoKHR create_info_host = *create_info;
    VkSurfaceCapabilitiesKHR capabilities;
    VkSwapchainKHR host_swapchain;
    VkResult res;

    if (!NtUserIsWindow( surface->hwnd ))
    {
        ERR( "surface %p, hwnd %p is invalid!\n", surface, surface->hwnd );
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    if (surface) create_info_host.surface = surface->obj.host.surface;
    if (old_swapchain) create_info_host.oldSwapchain = old_swapchain->obj.host.swapchain;

    /* update the host surface to commit any pending size change */
    driver_funcs->p_vulkan_surface_update( surface->hwnd, surface->driver_private );

    /* Windows allows client rect to be empty, but host Vulkan often doesn't, adjust extents back to the host capabilities */
    res = instance->p_vkGetPhysicalDeviceSurfaceCapabilitiesKHR( physical_device->host.physical_device, surface->obj.host.surface, &capabilities );
    if (res) return res;

    create_info_host.imageExtent.width = max( create_info_host.imageExtent.width, capabilities.minImageExtent.width );
    create_info_host.imageExtent.height = max( create_info_host.imageExtent.height, capabilities.minImageExtent.height );

    if (!(swapchain = calloc( 1, sizeof(*swapchain) ))) return VK_ERROR_OUT_OF_HOST_MEMORY;

    if (driver_funcs->p_vulkan_surface_enable_fshack( surface->hwnd, surface->driver_private ))
    {
        VkSurfaceCapabilitiesKHR caps = {0};

        if ((res = instance->p_vkGetPhysicalDeviceSurfaceCapabilitiesKHR( physical_device->host.physical_device,
                                                                          create_info_host.surface, &caps )))
        {
            TRACE( "vkGetPhysicalDeviceSurfaceCapabilities failed, res=%d\n", res );
            free( swapchain );
            return res;
        }

        if (!(caps.supportedUsageFlags & VK_IMAGE_USAGE_STORAGE_BIT))
            FIXME( "Swapchain does not support required VK_IMAGE_USAGE_STORAGE_BIT\n" );

        swapchain->host_extents = capabilities.minImageExtent;
        create_info_host.imageExtent = capabilities.minImageExtent;
        create_info_host.imageFormat = VK_FORMAT_B8G8R8A8_UNORM;
        create_info_host.imageUsage = VK_IMAGE_USAGE_STORAGE_BIT;

        if (create_info->imageFormat != VK_FORMAT_B8G8R8A8_UNORM && create_info->imageFormat != VK_FORMAT_B8G8R8A8_SRGB)
            FIXME( "swapchain image format is not BGRA8 UNORM/SRGB. Things may go badly. %d\n",
                   create_info_host.imageFormat );

        swapchain->fs_hack_enabled = TRUE;
    }

    if ((res = device->p_vkCreateSwapchainKHR( device->host.device, &create_info_host, NULL, &host_swapchain )))
    {
        free( swapchain );
        return res;
    }

    vulkan_object_init( &swapchain->obj.obj, host_swapchain );
    swapchain->surface = surface;
    swapchain->extents = create_info->imageExtent;

    if (swapchain->fs_hack_enabled)
    {
        if ((res = init_fs_hack_images( device, swapchain, create_info )))
        {
            ERR( "creating fs hack images failed: %d\n", res );
            device->p_vkDestroySwapchainKHR( device->host.device, swapchain->obj.host.swapchain, NULL );
            free( swapchain );
            return res;
        }

        if ((res = init_blit_images( device, swapchain )))
        {
            ERR( "creating blit images failed: %d\n", res );
            device->p_vkDestroySwapchainKHR( device->host.device, swapchain->obj.host.swapchain, NULL );
            free( swapchain );
            return res;
        }

        swapchain->raw_monitor_dpi = NtUserGetWinMonitorDpi( surface->hwnd, MDT_RAW_DPI );
        WARN( "Enabled fullscreen hack on swapchain %p, scalind from %s -> %s\n", swapchain,
              debugstr_vkextent2d(&swapchain->extents), debugstr_vkextent2d(&swapchain->host_extents) );
    }

    instance->p_insert_object( instance, &swapchain->obj.obj );

    *ret = swapchain->obj.client.swapchain;
    return VK_SUCCESS;
}

void win32u_vkDestroySwapchainKHR( VkDevice client_device, VkSwapchainKHR client_swapchain,
                                   const VkAllocationCallbacks *allocator )
{
    struct vulkan_device *device = vulkan_device_from_handle( client_device );
    struct vulkan_instance *instance = device->physical_device->instance;
    struct swapchain *swapchain = swapchain_from_handle( client_swapchain );
    int i;

    if (allocator) FIXME( "Support for allocation callbacks not implemented yet\n" );
    if (!swapchain) return;

    if (swapchain->fs_hack_enabled)
    {
        for (i = 0; i < swapchain->n_images; ++i) destroy_fs_hack_image( device, swapchain, &swapchain->fs_hack_images[i] );
        for (i = 0; i < device->queue_count; ++i)
        {
            if (!swapchain->cmd_pools[i]) continue;
            device->p_vkDestroyCommandPool( device->host.device, swapchain->cmd_pools[i], NULL );
        }

        device->p_vkDestroyPipeline( device->host.device, swapchain->pipeline, NULL );
        device->p_vkDestroyPipelineLayout( device->host.device, swapchain->pipeline_layout, NULL );
        device->p_vkDestroyDescriptorSetLayout( device->host.device, swapchain->descriptor_set_layout, NULL );
        device->p_vkDestroyDescriptorPool( device->host.device, swapchain->descriptor_pool, NULL );
        device->p_vkDestroySampler( device->host.device, swapchain->sampler, NULL );
        device->p_vkFreeMemory( device->host.device, swapchain->user_image_memory, NULL );
        free( swapchain->cmd_pools );
        free( swapchain->fs_hack_images );
    }

    device->p_vkDestroySwapchainKHR( device->host.device, swapchain->obj.host.swapchain, NULL );
    instance->p_remove_object( instance, &swapchain->obj.obj );

    free( swapchain );
}

static BOOL extents_equals( const VkExtent2D *extents, const RECT *rect )
{
    return extents->width == rect->right - rect->left && extents->height == rect->bottom - rect->top;
}

static VkResult win32u_vkAcquireNextImage2KHR( VkDevice client_device, const VkAcquireNextImageInfoKHR *acquire_info,
                                               uint32_t *image_index )
{
    struct vulkan_semaphore *semaphore = vulkan_semaphore_from_handle( acquire_info->semaphore );
    struct swapchain *swapchain = swapchain_from_handle( acquire_info->swapchain );
    struct vulkan_device *device = vulkan_device_from_handle( client_device );
    VkAcquireNextImageInfoKHR acquire_info_host = *acquire_info;
    struct surface *surface = swapchain->surface;
    RECT client_rect;
    VkResult res;

    acquire_info_host.swapchain = swapchain->obj.host.swapchain;
    acquire_info_host.semaphore = semaphore ? semaphore->host.semaphore : 0;

    res = device->p_vkAcquireNextImage2KHR( device->host.device, &acquire_info_host, image_index );

    if (!res && (driver_funcs->p_vulkan_surface_enable_fshack( surface->hwnd, surface->driver_private ) != swapchain->fs_hack_enabled
        || (swapchain->fs_hack_enabled && swapchain->raw_monitor_dpi != NtUserGetWinMonitorDpi( surface->hwnd, MDT_RAW_DPI ))))
    {
        WARN( "window %p swapchain %p needs fullscreen hack VK_SUBOPTIMAL_KHR\n", surface->hwnd, swapchain );
        return VK_SUBOPTIMAL_KHR;
    }

    if (!res && NtUserGetClientRect( surface->hwnd, &client_rect, NtUserGetDpiForWindow( surface->hwnd ) ) &&
        !extents_equals( &swapchain->extents, &client_rect ))
    {
        WARN( "Swapchain size %dx%d does not match client rect %s, returning VK_SUBOPTIMAL_KHR\n",
              swapchain->extents.width, swapchain->extents.height, wine_dbgstr_rect( &client_rect ) );
        return VK_SUBOPTIMAL_KHR;
    }

    return res;
}

static VkResult win32u_vkAcquireNextImageKHR( VkDevice client_device, VkSwapchainKHR client_swapchain, uint64_t timeout,
                                              VkSemaphore client_semaphore, VkFence fence, uint32_t *image_index )
{
    struct swapchain *swapchain = swapchain_from_handle( client_swapchain );
    struct vulkan_device *device = vulkan_device_from_handle( client_device );
    struct surface *surface = swapchain->surface;
    struct vulkan_semaphore *semaphore = vulkan_semaphore_from_handle( client_semaphore );
    RECT client_rect;
    VkResult res;

    res = device->p_vkAcquireNextImageKHR( device->host.device, swapchain->obj.host.swapchain, timeout,
                                           semaphore ? semaphore->host.semaphore : 0, fence, image_index );

    if (!res && (driver_funcs->p_vulkan_surface_enable_fshack( surface->hwnd, surface->driver_private ) != swapchain->fs_hack_enabled
        || (swapchain->fs_hack_enabled && swapchain->raw_monitor_dpi != NtUserGetWinMonitorDpi( surface->hwnd, MDT_RAW_DPI ))))
    {
        WARN( "window %p swapchain %p needs fullscreen hack VK_SUBOPTIMAL_KHR\n", surface->hwnd, swapchain );
        return VK_SUBOPTIMAL_KHR;
    }

    if (!res && NtUserGetClientRect( surface->hwnd, &client_rect, NtUserGetDpiForWindow( surface->hwnd ) ) &&
        !extents_equals( &swapchain->extents, &client_rect ))
    {
        WARN( "Swapchain size %dx%d does not match client rect %s, returning VK_SUBOPTIMAL_KHR\n",
              swapchain->extents.width, swapchain->extents.height, wine_dbgstr_rect( &client_rect ) );
        return VK_SUBOPTIMAL_KHR;
    }

    return res;
}

static VkResult win32u_vkGetSwapchainImagesKHR( VkDevice client_device, VkSwapchainKHR client_swapchain,
                                                uint32_t *count, VkImage *images )
{
    struct vulkan_device *device = vulkan_device_from_handle( client_device );
    struct swapchain *swapchain = swapchain_from_handle( client_swapchain );
    uint32_t i;

    if (images && swapchain->fs_hack_enabled)
    {
        if (*count > swapchain->n_images) *count = swapchain->n_images;
        for (i = 0; i < *count; ++i) images[i] = swapchain->fs_hack_images[i].user_image;
        return *count == swapchain->n_images ? VK_SUCCESS : VK_INCOMPLETE;
    }

    return device->p_vkGetSwapchainImagesKHR( device->host.device, swapchain->obj.host.swapchain, count, images );
}

static VkCommandBuffer create_hack_cmd( struct vulkan_queue *queue, struct swapchain *swapchain, uint32_t queue_idx )
{
    VkCommandBufferAllocateInfo allocInfo = {0};
    VkCommandBuffer cmd;
    VkResult res;

    if (!swapchain->cmd_pools[queue_idx])
    {
        VkCommandPoolCreateInfo poolInfo = {0};

        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = queue_idx;

        if ((res = queue->device->p_vkCreateCommandPool( queue->device->host.device, &poolInfo, NULL,
                                                         &swapchain->cmd_pools[queue_idx] )))
        {
            ERR( "vkCreateCommandPool failed, res=%d\n", res );
            return NULL;
        }
    }

    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = swapchain->cmd_pools[queue_idx];
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    if ((res = queue->device->p_vkAllocateCommandBuffers( queue->device->host.device, &allocInfo, &cmd )))
    {
        ERR( "vkAllocateCommandBuffers failed, res=%d\n", res );
        return NULL;
    }

    return cmd;
}

static VkResult record_compute_cmd( struct vulkan_device *device, struct swapchain *swapchain, struct fs_hack_image *hack )
{
    VkResult res;
    VkImageMemoryBarrier barriers[3] = {{0}};
    VkCommandBufferBeginInfo beginInfo = {0};
    float constants[4];

    TRACE( "recording compute command\n" );

    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

    device->p_vkBeginCommandBuffer( hack->cmd, &beginInfo );

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
    /* transition swapchain image from whatever to GENERAL */
    barriers[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barriers[1].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barriers[1].newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[1].image = hack->swapchain_image;
    barriers[1].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barriers[1].subresourceRange.baseMipLevel = 0;
    barriers[1].subresourceRange.levelCount = 1;
    barriers[1].subresourceRange.baseArrayLayer = 0;
    barriers[1].subresourceRange.layerCount = 1;
    barriers[1].srcAccessMask = 0;
    barriers[1].dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;

    device->p_vkCmdPipelineBarrier( hack->cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                    0, 0, NULL, 0, NULL, 2, barriers );

    /* perform blit shader */
    device->p_vkCmdBindPipeline( hack->cmd, VK_PIPELINE_BIND_POINT_COMPUTE, swapchain->pipeline );

    device->p_vkCmdBindDescriptorSets( hack->cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                                       swapchain->pipeline_layout, 0, 1, &hack->descriptor_set, 0, NULL );

    /* vec2: blit dst offset in real coords */
    constants[0] = 0;
    constants[1] = 0;

    /* offset by 0.5f because sampling is relative to pixel center */
    constants[0] -= 0.5f * swapchain->host_extents.width / swapchain->extents.width;
    constants[1] -= 0.5f * swapchain->host_extents.height / swapchain->extents.height;

    /* vec2: blit dst extents in real coords */
    constants[2] = swapchain->host_extents.width;
    constants[3] = swapchain->host_extents.height;
    device->p_vkCmdPushConstants( hack->cmd, swapchain->pipeline_layout,
                                  VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(constants), constants );

    /* local sizes in shader are 8 */
    device->p_vkCmdDispatch( hack->cmd, ceil( swapchain->host_extents.width / 8. ),
                             ceil( swapchain->host_extents.height / 8. ), 1 );

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

    /* transition swapchain image from GENERAL to PRESENT_SRC */
    barriers[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barriers[1].oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    barriers[1].newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    barriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[1].image = hack->swapchain_image;
    barriers[1].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barriers[1].subresourceRange.baseMipLevel = 0;
    barriers[1].subresourceRange.levelCount = 1;
    barriers[1].subresourceRange.baseArrayLayer = 0;
    barriers[1].subresourceRange.layerCount = 1;
    barriers[1].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barriers[1].dstAccessMask = 0;

    device->p_vkCmdPipelineBarrier( hack->cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                    0, 0, NULL, 0, NULL, 2, barriers );

    if ((res = device->p_vkEndCommandBuffer( hack->cmd )))
    {
        ERR( "vkEndCommandBuffer: %d\n", res );
        return res;
    }

    return VK_SUCCESS;
}

static VkResult win32u_vkQueuePresentKHR( VkQueue client_queue, const VkPresentInfoKHR *present_info )
{
    static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

    struct vulkan_queue *queue = vulkan_queue_from_handle( client_queue );
    VkSwapchainKHR swapchains_buffer[16], *swapchains = swapchains_buffer;
    VkPresentInfoKHR present_info_host = *present_info;
    struct vulkan_device *device = queue->device;
    struct vulkan_semaphore *semaphore;
    VkCommandBuffer *blit_cmds = NULL;
    VkSemaphore *semaphores = NULL;
    VkSubmitInfo submitInfo = {0};
    VkSemaphore blit_sema;
    UINT i, n_hacks = 0;
    uint32_t queue_idx;
    VkResult res;

    TRACE( "queue %p, present_info %p\n", queue, present_info );

    if (present_info->waitSemaphoreCount)
    {
        semaphores = malloc( present_info->waitSemaphoreCount * sizeof(*semaphores) );
        for (i = 0; i < present_info->waitSemaphoreCount; ++i)
        {
            semaphore = vulkan_semaphore_from_handle(present_info->pWaitSemaphores[i]);

            if (semaphore->d3d12_fence)
            {
                FIXME("Waiting on D3D12-Fence compatible timeline semaphore not supported.\n");
                free( semaphores );
                return VK_ERROR_OUT_OF_HOST_MEMORY;
            }
            semaphores[i] = semaphore->host.semaphore;
        }
        present_info_host.pWaitSemaphores = semaphores;
    }

    if (present_info->swapchainCount > ARRAY_SIZE(swapchains_buffer) &&
        !(swapchains = malloc( present_info->swapchainCount * sizeof(*swapchains) )))
    {
        free( semaphores );
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    for (i = 0; i < present_info->swapchainCount; i++)
    {
        struct swapchain *swapchain = swapchain_from_handle( present_info->pSwapchains[i] );
        swapchains[i] = swapchain->obj.host.swapchain;
    }

    present_info_host.pSwapchains = swapchains;

    for (i = 0; i < present_info_host.swapchainCount; ++i)
    {
        struct swapchain *swapchain = swapchain_from_handle( present_info->pSwapchains[i] );

        if (swapchain->fs_hack_enabled)
        {
            struct fs_hack_image *hack = &swapchain->fs_hack_images[present_info->pImageIndices[i]];

            if (!blit_cmds)
            {
                queue_idx = queue->family_index;
                blit_cmds = malloc( present_info->swapchainCount * sizeof(VkCommandBuffer) );
                blit_sema = hack->blit_finished;
            }

            if (!hack->cmd || hack->cmd_queue_idx != queue_idx)
            {
                if (hack->cmd) device->p_vkFreeCommandBuffers( queue->device->host.device, swapchain->cmd_pools[hack->cmd_queue_idx],
                                                               1, &hack->cmd );

                hack->cmd_queue_idx = queue_idx;
                hack->cmd = create_hack_cmd( queue, swapchain, queue_idx );

                if (!hack->cmd)
                {
                    free( blit_cmds );
                    free( semaphores );
                    return VK_ERROR_DEVICE_LOST;
                }

                if (queue->device->queue_props[queue_idx].queueFlags & VK_QUEUE_COMPUTE_BIT) /* TODO */
                    res = record_compute_cmd( queue->device, swapchain, hack );
                else
                {
                    ERR( "Present queue does not support compute!\n" );
                    res = VK_ERROR_DEVICE_LOST;
                }

                if (res != VK_SUCCESS)
                {
                    device->p_vkFreeCommandBuffers( queue->device->host.device, swapchain->cmd_pools[hack->cmd_queue_idx],
                                                    1, &hack->cmd );
                    hack->cmd = NULL;
                    free( blit_cmds );
                    free( semaphores );
                    return res;
                }
            }

            blit_cmds[n_hacks] = hack->cmd;

            ++n_hacks;
        }
    }

    if (n_hacks > 0)
    {
        VkPipelineStageFlags waitStage, *waitStages, *waitStages_arr = NULL;

        if (present_info->waitSemaphoreCount > 1)
        {
            waitStages_arr = malloc( sizeof(VkPipelineStageFlags) * present_info->waitSemaphoreCount );
            for (i = 0; i < present_info->waitSemaphoreCount; ++i)
                waitStages_arr[i] = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
            waitStages = waitStages_arr;
        }
        else
        {
            waitStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
            waitStages = &waitStage;
        }

        /* blit user image to real image */
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.waitSemaphoreCount = present_info_host.waitSemaphoreCount;
        submitInfo.pWaitSemaphores = present_info_host.pWaitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = n_hacks;
        submitInfo.pCommandBuffers = blit_cmds;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &blit_sema;

        if ((res = device->p_vkQueueSubmit( queue->host.queue, 1, &submitInfo, VK_NULL_HANDLE )))
        {
            ERR( "vkQueueSubmit: %d\n", res );
        }

        free( waitStages_arr );
        free( blit_cmds );

        present_info_host.waitSemaphoreCount = 1;
        present_info_host.pWaitSemaphores = &blit_sema;
    }

    pthread_mutex_lock( &lock );
    res = device->p_vkQueuePresentKHR( queue->host.queue, &present_info_host );
    pthread_mutex_unlock( &lock );

    for (i = 0; i < present_info->swapchainCount; i++)
    {
        struct swapchain *swapchain = swapchain_from_handle( present_info->pSwapchains[i] );
        VkResult swapchain_res = present_info->pResults ? present_info->pResults[i] : res;
        struct surface *surface = swapchain->surface;
        RECT client_rect;

        if (surface->hwnd)
            driver_funcs->p_vulkan_surface_presented( surface->hwnd, surface->driver_private, swapchain_res );

        if (swapchain_res < VK_SUCCESS) continue;
        if (!NtUserGetClientRect( surface->hwnd, &client_rect, NtUserGetDpiForWindow( surface->hwnd ) ))
        {
            WARN( "Swapchain window %p is invalid, returning VK_ERROR_OUT_OF_DATE_KHR\n", surface->hwnd );
            if (present_info->pResults) present_info->pResults[i] = VK_ERROR_OUT_OF_DATE_KHR;
            if (res >= VK_SUCCESS) res = VK_ERROR_OUT_OF_DATE_KHR;
        }
        else if (swapchain_res)
            WARN( "Present returned status %d for swapchain %p\n", swapchain_res, swapchain );
        else if (!extents_equals( &swapchain->extents, &client_rect ))
        {
            WARN( "Swapchain size %dx%d does not match client rect %s, returning VK_SUBOPTIMAL_KHR\n",
                  swapchain->extents.width, swapchain->extents.height, wine_dbgstr_rect( &client_rect ) );
            if (present_info->pResults) present_info->pResults[i] = VK_SUBOPTIMAL_KHR;
            if (!res) res = VK_SUBOPTIMAL_KHR;
        }
    }

    if (swapchains != swapchains_buffer) free( swapchains );
    free( semaphores );

    if (TRACE_ON( fps ))
    {
        static unsigned long frames, frames_total;
        static long prev_time, start_time;
        DWORD time;

        time = NtGetTickCount();
        frames++;
        frames_total++;

        if (time - prev_time > 1500)
        {
            TRACE_(fps)( "%p @ approx %.2ffps, total %.2ffps\n", queue, 1000.0 * frames / (time - prev_time),
                         1000.0 * frames_total / (time - start_time) );
            prev_time = time;
            frames = 0;

            if (!start_time) start_time = time;
        }
    }

    return res;
}

static const char *win32u_get_host_surface_extension(void)
{
    return driver_funcs->p_get_host_surface_extension();
}

static struct vulkan_funcs vulkan_funcs =
{
    .p_vkCreateWin32SurfaceKHR = win32u_vkCreateWin32SurfaceKHR,
    .p_vkDestroySurfaceKHR = win32u_vkDestroySurfaceKHR,
    .p_vkGetPhysicalDeviceSurfaceCapabilitiesKHR = win32u_vkGetPhysicalDeviceSurfaceCapabilitiesKHR,
    .p_vkGetPhysicalDeviceSurfaceCapabilities2KHR = win32u_vkGetPhysicalDeviceSurfaceCapabilities2KHR,
    .p_vkGetPhysicalDevicePresentRectanglesKHR = win32u_vkGetPhysicalDevicePresentRectanglesKHR,
    .p_vkGetPhysicalDeviceSurfaceFormatsKHR = win32u_vkGetPhysicalDeviceSurfaceFormatsKHR,
    .p_vkGetPhysicalDeviceSurfaceFormats2KHR = win32u_vkGetPhysicalDeviceSurfaceFormats2KHR,
    .p_vkGetPhysicalDeviceWin32PresentationSupportKHR = win32u_vkGetPhysicalDeviceWin32PresentationSupportKHR,
    .p_vkCreateSwapchainKHR = win32u_vkCreateSwapchainKHR,
    .p_vkDestroySwapchainKHR = win32u_vkDestroySwapchainKHR,
    .p_vkAcquireNextImage2KHR = win32u_vkAcquireNextImage2KHR,
    .p_vkAcquireNextImageKHR = win32u_vkAcquireNextImageKHR,
    .p_vkGetSwapchainImagesKHR = win32u_vkGetSwapchainImagesKHR,
    .p_vkQueuePresentKHR = win32u_vkQueuePresentKHR,
    .p_get_host_surface_extension = win32u_get_host_surface_extension,
};

static VkResult nulldrv_vulkan_surface_create( HWND hwnd, const struct vulkan_instance *instance, VkSurfaceKHR *surface, void **private )
{
    VkHeadlessSurfaceCreateInfoEXT create_info = {.sType = VK_STRUCTURE_TYPE_HEADLESS_SURFACE_CREATE_INFO_EXT};
    return instance->p_vkCreateHeadlessSurfaceEXT( instance->host.instance, &create_info, NULL, surface );
}

static void nulldrv_vulkan_surface_destroy( HWND hwnd, void *private )
{
}

static void nulldrv_vulkan_surface_detach( HWND hwnd, void *private )
{
}

static void nulldrv_vulkan_surface_update( HWND hwnd, void *private )
{
}

static void nulldrv_vulkan_surface_presented( HWND hwnd, void *private, VkResult result )
{
}

static BOOL nulldrv_vulkan_surface_enable_fshack( HWND hwnd, void *private )
{
    return FALSE;
}

static VkBool32 nulldrv_vkGetPhysicalDeviceWin32PresentationSupportKHR( VkPhysicalDevice device, uint32_t queue )
{
    return VK_TRUE;
}

static const char *nulldrv_get_host_surface_extension(void)
{
    return "VK_EXT_headless_surface";
}

static const struct vulkan_driver_funcs nulldrv_funcs =
{
    .p_vulkan_surface_create = nulldrv_vulkan_surface_create,
    .p_vulkan_surface_destroy = nulldrv_vulkan_surface_destroy,
    .p_vulkan_surface_detach = nulldrv_vulkan_surface_detach,
    .p_vulkan_surface_update = nulldrv_vulkan_surface_update,
    .p_vulkan_surface_presented = nulldrv_vulkan_surface_presented,
    .p_vulkan_surface_enable_fshack = nulldrv_vulkan_surface_enable_fshack,
    .p_vkGetPhysicalDeviceWin32PresentationSupportKHR = nulldrv_vkGetPhysicalDeviceWin32PresentationSupportKHR,
    .p_get_host_surface_extension = nulldrv_get_host_surface_extension,
};

static void vulkan_driver_init(void)
{
    UINT status;

    if ((status = user_driver->pVulkanInit( WINE_VULKAN_DRIVER_VERSION, vulkan_handle, &driver_funcs )) &&
        status != STATUS_NOT_IMPLEMENTED)
    {
        ERR( "Failed to initialize the driver vulkan functions, status %#x\n", status );
        return;
    }

    if (status == STATUS_NOT_IMPLEMENTED) driver_funcs = &nulldrv_funcs;
    else vulkan_funcs.p_get_host_surface_extension = driver_funcs->p_get_host_surface_extension;
}

static void vulkan_driver_load(void)
{
    static pthread_once_t init_once = PTHREAD_ONCE_INIT;
    pthread_once( &init_once, vulkan_driver_init );
}

static VkResult lazydrv_vulkan_surface_create( HWND hwnd, const struct vulkan_instance *instance, VkSurfaceKHR *surface, void **private )
{
    vulkan_driver_load();
    return driver_funcs->p_vulkan_surface_create( hwnd, instance, surface, private );
}

static void lazydrv_vulkan_surface_destroy( HWND hwnd, void *private )
{
    vulkan_driver_load();
    return driver_funcs->p_vulkan_surface_destroy( hwnd, private );
}

static void lazydrv_vulkan_surface_detach( HWND hwnd, void *private )
{
    vulkan_driver_load();
    return driver_funcs->p_vulkan_surface_detach( hwnd, private );
}

static void lazydrv_vulkan_surface_update( HWND hwnd, void *private )
{
    vulkan_driver_load();
    return driver_funcs->p_vulkan_surface_update( hwnd, private );
}

static void lazydrv_vulkan_surface_presented( HWND hwnd, void *private, VkResult result )
{
    vulkan_driver_load();
    driver_funcs->p_vulkan_surface_presented( hwnd, private, result );
}

static BOOL lazydrv_vulkan_surface_enable_fshack( HWND hwnd, void *private )
{
    vulkan_driver_load();
    return driver_funcs->p_vulkan_surface_enable_fshack( hwnd, private );
}

static VkBool32 lazydrv_vkGetPhysicalDeviceWin32PresentationSupportKHR( VkPhysicalDevice device, uint32_t queue )
{
    vulkan_driver_load();
    return driver_funcs->p_vkGetPhysicalDeviceWin32PresentationSupportKHR( device, queue );
}

static const char *lazydrv_get_host_surface_extension(void)
{
    vulkan_driver_load();
    return driver_funcs->p_get_host_surface_extension();
}

static const struct vulkan_driver_funcs lazydrv_funcs =
{
    .p_vulkan_surface_create = lazydrv_vulkan_surface_create,
    .p_vulkan_surface_destroy = lazydrv_vulkan_surface_destroy,
    .p_vulkan_surface_detach = lazydrv_vulkan_surface_detach,
    .p_vulkan_surface_update = lazydrv_vulkan_surface_update,
    .p_vulkan_surface_presented = lazydrv_vulkan_surface_presented,
    .p_vulkan_surface_enable_fshack = lazydrv_vulkan_surface_enable_fshack,
    .p_vkGetPhysicalDeviceWin32PresentationSupportKHR = lazydrv_vkGetPhysicalDeviceWin32PresentationSupportKHR,
    .p_get_host_surface_extension = lazydrv_get_host_surface_extension,
};

static void vulkan_init_once(void)
{
    if (!(vulkan_handle = dlopen( SONAME_LIBVULKAN, RTLD_NOW )))
    {
        ERR( "Failed to load %s\n", SONAME_LIBVULKAN );
        return;
    }

#define LOAD_FUNCPTR( f )                                                                          \
    if (!(p_##f = dlsym( vulkan_handle, #f )))                                                     \
    {                                                                                              \
        ERR( "Failed to find " #f "\n" );                                                          \
        dlclose( vulkan_handle );                                                                  \
        vulkan_handle = NULL;                                                                      \
        return;                                                                                    \
    }

    LOAD_FUNCPTR( vkGetDeviceProcAddr );
    LOAD_FUNCPTR( vkGetInstanceProcAddr );
#undef LOAD_FUNCPTR

    driver_funcs = &lazydrv_funcs;
    vulkan_funcs.p_vkGetInstanceProcAddr = p_vkGetInstanceProcAddr;
    vulkan_funcs.p_vkGetDeviceProcAddr = p_vkGetDeviceProcAddr;
}

void vulkan_update_surfaces( HWND hwnd )
{
    struct surface *surface;
    struct list temp_list;
    WND *win;

    list_init( &temp_list );
    pthread_mutex_lock( &surface_list_lock );

    if (!(win = get_win_ptr( hwnd )) || win == WND_DESKTOP || win == WND_OTHER_PROCESS)
    {
        pthread_mutex_unlock( &surface_list_lock );
        return;
    }
    LIST_FOR_EACH_ENTRY( surface, &win->vulkan_surfaces, struct surface, entry )
    {
        list_add_tail( &temp_list, &surface->temp_entry );
    }
    release_win_ptr( win );

    LIST_FOR_EACH_ENTRY( surface, &temp_list, struct surface, temp_entry )
        driver_funcs->p_vulkan_surface_update( surface->hwnd, surface->driver_private );

    pthread_mutex_unlock( &surface_list_lock );
}

void vulkan_detach_surfaces( struct list *surfaces )
{
    struct surface *surface, *next;

    pthread_mutex_lock( &surface_list_lock );
    LIST_FOR_EACH_ENTRY_SAFE( surface, next, surfaces, struct surface, entry )
    {
        driver_funcs->p_vulkan_surface_detach( surface->hwnd, surface->driver_private );
        list_remove( &surface->entry );
        list_init( &surface->entry );
        surface->hwnd = NULL;
    }
    pthread_mutex_unlock( &surface_list_lock );
}

#else /* SONAME_LIBVULKAN */

void vulkan_detach_surfaces( struct list *surfaces )
{
}

static void vulkan_init_once(void)
{
    ERR( "Wine was built without Vulkan support.\n" );
}

#endif /* SONAME_LIBVULKAN */

BOOL vulkan_init(void)
{
    static pthread_once_t init_once = PTHREAD_ONCE_INIT;
    pthread_once( &init_once, vulkan_init_once );
    return !!vulkan_handle;
}

/***********************************************************************
 *      __wine_get_vulkan_driver  (win32u.so)
 */
const struct vulkan_funcs *__wine_get_vulkan_driver( UINT version )
{
    if (version != WINE_VULKAN_DRIVER_VERSION)
    {
        ERR( "version mismatch, vulkan wants %u but win32u has %u\n", version, WINE_VULKAN_DRIVER_VERSION );
        return NULL;
    }

    if (!vulkan_init()) return NULL;
    return &vulkan_funcs;
}
