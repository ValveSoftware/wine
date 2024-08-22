/*
 * X11DRV display device functions
 *
 * Copyright 2019 Zhiyi Zhang for CodeWeavers
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
#include "x11drv.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(x11drv);

static struct x11drv_display_device_handler host_handler;
static struct x11drv_settings_handler settings_handler;
RECT native_screen_rect;

#define NEXT_DEVMODEW(mode) ((DEVMODEW *)((char *)((mode) + 1) + (mode)->dmDriverExtra))

struct x11drv_display_depth
{
    struct list entry;
    x11drv_settings_id display_id;
    DWORD depth;
};

/* Display device emulated depth list, protected by modes_section */
static struct list x11drv_display_depth_list = LIST_INIT(x11drv_display_depth_list);

/* All Windows drivers seen so far either support 32 bit depths, or 24 bit depths, but never both. So if we have
 * a 32 bit framebuffer, report 32 bit bpps, otherwise 24 bit ones.
 */
static const unsigned int depths_24[]  = {8, 16, 24};
static const unsigned int depths_32[]  = {8, 16, 32};
const unsigned int *depths;

static pthread_mutex_t settings_mutex = PTHREAD_MUTEX_INITIALIZER;

void X11DRV_Settings_SetHandler(const struct x11drv_settings_handler *new_handler)
{
    if (new_handler->priority > settings_handler.priority)
    {
        settings_handler = *new_handler;
        TRACE("Display settings are now handled by: %s.\n", settings_handler.name);
    }
}

struct x11drv_settings_handler X11DRV_Settings_GetHandler(void)
{
    return settings_handler;
}

/***********************************************************************
 * Default handlers if resolution switching is not enabled
 *
 */
static BOOL nores_get_id(const WCHAR *device_name, BOOL is_primary, x11drv_settings_id *id)
{
    id->id = is_primary ? 1 : 0;
    return TRUE;
}

static BOOL nores_get_modes(x11drv_settings_id id, DWORD flags, DEVMODEW **new_modes, UINT *mode_count)
{
    RECT primary = get_host_primary_monitor_rect();
    DEVMODEW *modes;

    modes = calloc(1, sizeof(*modes));
    if (!modes)
    {
        RtlSetLastWin32Error( ERROR_NOT_ENOUGH_MEMORY );
        return FALSE;
    }

    modes[0].dmSize = sizeof(*modes);
    modes[0].dmDriverExtra = 0;
    modes[0].dmFields = DM_DISPLAYORIENTATION | DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT |
                        DM_DISPLAYFLAGS | DM_DISPLAYFREQUENCY;
    modes[0].dmDisplayOrientation = DMDO_DEFAULT;
    modes[0].dmBitsPerPel = screen_bpp;
    modes[0].dmPelsWidth = primary.right;
    modes[0].dmPelsHeight = primary.bottom;
    modes[0].dmDisplayFlags = 0;
    modes[0].dmDisplayFrequency = 60;

    *new_modes = modes;
    *mode_count = 1;
    return TRUE;
}

static void nores_free_modes(DEVMODEW *modes)
{
    free(modes);
}

static BOOL nores_get_current_mode(x11drv_settings_id id, DEVMODEW *mode)
{
    RECT primary = get_host_primary_monitor_rect();

    mode->dmFields = DM_DISPLAYORIENTATION | DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT |
                     DM_DISPLAYFLAGS | DM_DISPLAYFREQUENCY | DM_POSITION;
    mode->dmDisplayOrientation = DMDO_DEFAULT;
    mode->dmDisplayFlags = 0;
    mode->dmPosition.x = 0;
    mode->dmPosition.y = 0;

    if (id.id != 1)
    {
        FIXME("Non-primary adapters are unsupported.\n");
        mode->dmBitsPerPel = 0;
        mode->dmPelsWidth = 0;
        mode->dmPelsHeight = 0;
        mode->dmDisplayFrequency = 0;
        return TRUE;
    }

    mode->dmBitsPerPel = screen_bpp;
    mode->dmPelsWidth = primary.right;
    mode->dmPelsHeight = primary.bottom;
    mode->dmDisplayFrequency = 60;
    return TRUE;
}

static LONG nores_set_current_mode(x11drv_settings_id id, const DEVMODEW *mode)
{
    WARN("NoRes settings handler, ignoring mode change request.\n");
    return DISP_CHANGE_SUCCESSFUL;
}

/* default handler only gets the current X desktop resolution */
void X11DRV_Settings_Init(void)
{
    struct x11drv_settings_handler nores_handler;

    depths = screen_bpp == 32 ? depths_32 : depths_24;

    nores_handler.name = "NoRes";
    nores_handler.priority = 1;
    nores_handler.get_id = nores_get_id;
    nores_handler.get_modes = nores_get_modes;
    nores_handler.free_modes = nores_free_modes;
    nores_handler.get_current_mode = nores_get_current_mode;
    nores_handler.set_current_mode = nores_set_current_mode;
    X11DRV_Settings_SetHandler(&nores_handler);
}

static void set_display_depth(x11drv_settings_id display_id, DWORD depth)
{
    struct x11drv_display_depth *display_depth;

    pthread_mutex_lock( &settings_mutex );
    LIST_FOR_EACH_ENTRY(display_depth, &x11drv_display_depth_list, struct x11drv_display_depth, entry)
    {
        if (display_depth->display_id.id == display_id.id)
        {
            display_depth->depth = depth;
            pthread_mutex_unlock( &settings_mutex );
            return;
        }
    }

    display_depth = malloc(sizeof(*display_depth));
    if (!display_depth)
    {
        ERR("Failed to allocate memory.\n");
        pthread_mutex_unlock( &settings_mutex );
        return;
    }

    display_depth->display_id = display_id;
    display_depth->depth = depth;
    list_add_head(&x11drv_display_depth_list, &display_depth->entry);
    pthread_mutex_unlock( &settings_mutex );
}

static DWORD get_display_depth(x11drv_settings_id display_id)
{
    struct x11drv_display_depth *display_depth;
    DWORD depth;

    pthread_mutex_lock( &settings_mutex );
    LIST_FOR_EACH_ENTRY(display_depth, &x11drv_display_depth_list, struct x11drv_display_depth, entry)
    {
        if (display_depth->display_id.id == display_id.id)
        {
            depth = display_depth->depth;
            pthread_mutex_unlock( &settings_mutex );
            return depth;
        }
    }
    pthread_mutex_unlock( &settings_mutex );
    return screen_bpp;
}

INT X11DRV_GetDisplayDepth(LPCWSTR name, BOOL is_primary)
{
    x11drv_settings_id id;

    if (settings_handler.get_id( name, is_primary, &id ))
        return get_display_depth( id );

    return screen_bpp;
}

/***********************************************************************
 *      GetCurrentDisplaySettings  (X11DRV.@)
 *
 */
BOOL X11DRV_GetCurrentDisplaySettings( LPCWSTR name, BOOL is_primary, LPDEVMODEW devmode )
{
    DEVMODEW mode;
    x11drv_settings_id id;

    if (!settings_handler.get_id( name, is_primary, &id ) || !settings_handler.get_current_mode( id, &mode ))
    {
        ERR("Failed to get %s current display settings.\n", wine_dbgstr_w(name));
        return FALSE;
    }

    memcpy( &devmode->dmFields, &mode.dmFields, devmode->dmSize - offsetof(DEVMODEW, dmFields) );
    if (!is_detached_mode( devmode )) devmode->dmBitsPerPel = get_display_depth( id );
    return TRUE;
}

BOOL is_detached_mode(const DEVMODEW *mode)
{
    return mode->dmFields & DM_POSITION &&
           mode->dmFields & DM_PELSWIDTH &&
           mode->dmFields & DM_PELSHEIGHT &&
           mode->dmPelsWidth == 0 &&
           mode->dmPelsHeight == 0;
}

static BOOL is_same_devmode( const DEVMODEW *a, const DEVMODEW *b )
{
    return a->dmDisplayOrientation == b->dmDisplayOrientation &&
           a->dmBitsPerPel == b->dmBitsPerPel &&
           a->dmPelsWidth == b->dmPelsWidth &&
           a->dmPelsHeight == b->dmPelsHeight &&
           a->dmDisplayFrequency == b->dmDisplayFrequency;
}

/* Get the full display mode with all the necessary fields set.
 * Return NULL on failure. Caller should call free_full_mode() to free the returned mode. */
static DEVMODEW *get_full_mode(x11drv_settings_id id, DEVMODEW *dev_mode)
{
    DEVMODEW *modes, *full_mode, *found_mode = NULL;
    UINT mode_count, mode_idx;

    if (is_detached_mode(dev_mode))
        return dev_mode;

    if (!settings_handler.get_modes(id, EDS_ROTATEDMODE, &modes, &mode_count))
        return NULL;

    for (mode_idx = 0; mode_idx < mode_count; ++mode_idx)
    {
        found_mode = (DEVMODEW *)((BYTE *)modes + (sizeof(*modes) + modes[0].dmDriverExtra) * mode_idx);
        if (is_same_devmode( found_mode, dev_mode )) break;
    }

    if (!found_mode || mode_idx == mode_count)
    {
        settings_handler.free_modes(modes);
        return NULL;
    }

    if (!(full_mode = malloc(sizeof(*found_mode) + found_mode->dmDriverExtra)))
    {
        settings_handler.free_modes(modes);
        return NULL;
    }

    memcpy(full_mode, found_mode, sizeof(*found_mode) + found_mode->dmDriverExtra);
    settings_handler.free_modes(modes);

    full_mode->dmFields |= DM_POSITION;
    full_mode->dmPosition = dev_mode->dmPosition;
    return full_mode;
}

static void free_full_mode(DEVMODEW *mode)
{
    if (!is_detached_mode(mode))
        free(mode);
}

static LONG apply_display_settings( DEVMODEW *displays, x11drv_settings_id *ids, BOOL do_attach )
{
    DEVMODEW *full_mode;
    BOOL attached_mode;
    LONG count, ret;
    DEVMODEW *mode;

    for (count = 0, mode = displays; mode->dmSize; mode = NEXT_DEVMODEW(mode), count++)
    {
        x11drv_settings_id *id = ids + count;

        attached_mode = !is_detached_mode(mode);
        if ((attached_mode && !do_attach) || (!attached_mode && do_attach))
            continue;

        /* FIXME: get a full mode again because X11 driver extra data isn't portable */
        full_mode = get_full_mode(*id, mode);
        if (!full_mode)
            return DISP_CHANGE_BADMODE;

        TRACE("handler:%s changing %s to position:(%d,%d) resolution:%ux%u frequency:%uHz "
              "depth:%ubits orientation:%#x.\n", settings_handler.name,
              wine_dbgstr_w(mode->dmDeviceName),
              (int)full_mode->dmPosition.x, (int)full_mode->dmPosition.y, (int)full_mode->dmPelsWidth,
              (int)full_mode->dmPelsHeight, (int)full_mode->dmDisplayFrequency,
              (int)full_mode->dmBitsPerPel, (int)full_mode->dmDisplayOrientation);

        ret = settings_handler.set_current_mode(*id, full_mode);
        if (attached_mode && ret == DISP_CHANGE_SUCCESSFUL)
            set_display_depth(*id, full_mode->dmBitsPerPel);
        free_full_mode(full_mode);
        if (ret != DISP_CHANGE_SUCCESSFUL)
            return ret;
    }

    return DISP_CHANGE_SUCCESSFUL;
}

/***********************************************************************
 *      ChangeDisplaySettings  (X11DRV.@)
 *
 */
LONG X11DRV_ChangeDisplaySettings( LPDEVMODEW displays, LPCWSTR primary_name, HWND hwnd, DWORD flags, LPVOID lpvoid )
{
    LONG count, ret = DISP_CHANGE_BADPARAM;
    x11drv_settings_id *ids;
    DEVMODEW *mode;

    /* Convert virtual screen coordinates to root coordinates, and find display ids.
     * We cannot safely get the ids while changing modes, as the backend state may be invalidated.
     */
    for (count = 0, mode = displays; mode->dmSize; mode = NEXT_DEVMODEW( mode )) count++;

    if (!(ids = calloc( count, sizeof(*ids) ))) return DISP_CHANGE_FAILED;
    for (count = 0, mode = displays; mode->dmSize; mode = NEXT_DEVMODEW(mode), count++)
    {
        BOOL is_primary = !wcsicmp( mode->dmDeviceName, primary_name );
        if (!settings_handler.get_id( mode->dmDeviceName, is_primary, ids + count )) goto done;
    }

    /* Detach displays first to free up CRTCs */
    ret = apply_display_settings( displays, ids, FALSE );
    if (ret == DISP_CHANGE_SUCCESSFUL)
        ret = apply_display_settings( displays, ids, TRUE );

done:
    free( ids );
    return ret;
}

POINT virtual_screen_to_root(INT x, INT y)
{
    RECT virtual = fs_hack_get_real_virtual_screen();
    POINT pt;

    TRACE( "from %d,%d\n", x, y );

    pt.x = x;
    pt.y = y;
    fs_hack_point_user_to_real( &pt );
    TRACE( "to real %s\n", wine_dbgstr_point( &pt ) );

    pt.x -= virtual.left;
    pt.y -= virtual.top;
    TRACE( "to root %s\n", wine_dbgstr_point( &pt ) );
    return pt;
}

POINT root_to_virtual_screen(INT x, INT y)
{
    RECT virtual = fs_hack_get_real_virtual_screen();
    POINT pt;

    TRACE( "from root %d,%d\n", x, y );
    pt.x = x + virtual.left;
    pt.y = y + virtual.top;
    TRACE( "to real %s\n", wine_dbgstr_point( &pt ) );
    fs_hack_point_real_to_user( &pt );
    TRACE( "to user %s\n", wine_dbgstr_point( &pt ) );
    return pt;
}

/* Get the primary monitor rect from the host system */
RECT get_host_primary_monitor_rect(void)
{
    INT gpu_count, adapter_count, monitor_count;
    struct gdi_gpu *gpus = NULL;
    struct gdi_adapter *adapters = NULL;
    struct gdi_monitor *monitors = NULL;
    RECT rect = {0};

    /* The first monitor is always primary */
    if (host_handler.get_gpus(&gpus, &gpu_count, FALSE) && gpu_count &&
        host_handler.get_adapters(gpus[0].id, &adapters, &adapter_count) && adapter_count &&
        host_handler.get_monitors(adapters[0].id, &monitors, &monitor_count) && monitor_count)
        rect = monitors[0].rc_monitor;

    if (gpus) host_handler.free_gpus(gpus);
    if (adapters) host_handler.free_adapters(adapters);
    if (monitors) host_handler.free_monitors(monitors, monitor_count);
    return rect;
}

/* Get an array of host monitor rectangles in X11 root coordinates. Free the array when it's done */
BOOL get_host_monitor_rects( RECT **ret_rects, int *ret_count )
{
    int gpu_count, adapter_count, monitor_count, rect_count = 0;
    int gpu_idx, adapter_idx, monitor_idx, rect_idx;
    struct gdi_gpu *gpus = NULL;
    struct gdi_adapter *adapters = NULL;
    struct gdi_monitor *monitors = NULL;
    RECT *rects = NULL, *new_rects;
    POINT left_top = {INT_MAX, INT_MAX};

    if (!host_handler.get_gpus( &gpus, &gpu_count, FALSE )) goto failed;

    for (gpu_idx = 0; gpu_idx < gpu_count; gpu_idx++)
    {
        if (!host_handler.get_adapters( gpus[gpu_idx].id, &adapters, &adapter_count )) goto failed;

        for (adapter_idx = 0; adapter_idx < adapter_count; adapter_idx++)
        {
            if (!host_handler.get_monitors( adapters[adapter_idx].id, &monitors, &monitor_count )) goto failed;

            new_rects = realloc( rects, (rect_count + monitor_count) * sizeof(*rects) );
            if (!new_rects) goto failed;
            rects = new_rects;

            for (monitor_idx = 0; monitor_idx < monitor_count; monitor_idx++)
            {
                rects[rect_count++] = monitors[monitor_idx].rc_monitor;
                left_top.x = min( left_top.x, monitors[monitor_idx].rc_monitor.left );
                left_top.y = min( left_top.y, monitors[monitor_idx].rc_monitor.top );
            }

            host_handler.free_monitors( monitors, monitor_count );
            monitors = NULL;
        }

        host_handler.free_adapters( adapters );
        adapters = NULL;
    }

    host_handler.free_gpus( gpus );
    gpus = NULL;

    /* Convert from win32 virtual screen coordinates to X11 root coordinates */
    for (rect_idx = 0; rect_idx < rect_count; rect_idx++)
        OffsetRect( &rects[rect_idx], -left_top.x, -left_top.y );

    *ret_rects = rects;
    *ret_count = rect_count;
    return TRUE;

failed:
    if (monitors) host_handler.free_monitors( monitors, monitor_count );
    if (adapters) host_handler.free_adapters( adapters );
    if (gpus) host_handler.free_gpus( gpus );
    free( rects );
    *ret_rects = NULL;
    *ret_count = 0;
    return FALSE;
}

RECT get_work_area(const RECT *monitor_rect)
{
    Atom type;
    int format;
    unsigned long count, remaining, i;
    long *work_area;
    RECT work_rect;

    /* Try _GTK_WORKAREAS first as _NET_WORKAREA may be incorrect on multi-monitor systems */
    if (!XGetWindowProperty(gdi_display, DefaultRootWindow(gdi_display),
                            x11drv_atom(_GTK_WORKAREAS_D0), 0, ~0, False, XA_CARDINAL, &type,
                            &format, &count, &remaining, (unsigned char **)&work_area))
    {
        if (type == XA_CARDINAL && format == 32)
        {
            for (i = 0; i < count / 4; ++i)
            {
                work_rect.left = work_area[i * 4];
                work_rect.top = work_area[i * 4 + 1];
                work_rect.right = work_rect.left + work_area[i * 4 + 2];
                work_rect.bottom = work_rect.top + work_area[i * 4 + 3];

                if (intersect_rect( &work_rect, &work_rect, monitor_rect ))
                {
                    TRACE("work_rect:%s.\n", wine_dbgstr_rect(&work_rect));
                    XFree(work_area);
                    return work_rect;
                }
            }
        }
        XFree(work_area);
    }

    WARN("_GTK_WORKAREAS is not supported, fallback to _NET_WORKAREA. "
         "Work areas may be incorrect on multi-monitor systems.\n");
    if (!XGetWindowProperty(gdi_display, DefaultRootWindow(gdi_display), x11drv_atom(_NET_WORKAREA),
                            0, ~0, False, XA_CARDINAL, &type, &format, &count, &remaining,
                            (unsigned char **)&work_area))
    {
        if (type == XA_CARDINAL && format == 32 && count >= 4)
        {
            SetRect(&work_rect, work_area[0], work_area[1], work_area[0] + work_area[2],
                    work_area[1] + work_area[3]);

            if (intersect_rect( &work_rect, &work_rect, monitor_rect ))
            {
                TRACE("work_rect:%s.\n", wine_dbgstr_rect(&work_rect));
                XFree(work_area);
                return work_rect;
            }
        }
        XFree(work_area);
    }

    WARN("_NET_WORKAREA is not supported, Work areas may be incorrect.\n");
    TRACE("work_rect:%s.\n", wine_dbgstr_rect(monitor_rect));
    return *monitor_rect;
}

void X11DRV_DisplayDevices_SetHandler(const struct x11drv_display_device_handler *new_handler)
{
    if (new_handler->priority > host_handler.priority)
    {
        host_handler = *new_handler;
        TRACE("Display device functions are now handled by: %s\n", host_handler.name);
    }
}

struct x11drv_display_device_handler X11DRV_DisplayDevices_GetHandler(void)
{
    return host_handler;
}

void X11DRV_DisplayDevices_RegisterEventHandlers(void)
{
    if (host_handler.register_event_handlers) host_handler.register_event_handlers();
}

/* Report whether a display device handler supports detecting dynamic device changes */
BOOL X11DRV_DisplayDevices_SupportEventHandlers(void)
{
    return !!host_handler.register_event_handlers;
}

static BOOL force_display_devices_refresh;

static const char *debugstr_devmodew( const DEVMODEW *devmode )
{
    char position[32] = {0};

    if (devmode->dmFields & DM_POSITION)
    {
        snprintf( position, sizeof(position), " at (%d,%d)",
                 (int)devmode->dmPosition.x, (int)devmode->dmPosition.y );
    }

    return wine_dbg_sprintf( "%ux%u %ubits %uHz rotated %u degrees%s",
                             (unsigned int)devmode->dmPelsWidth,
                             (unsigned int)devmode->dmPelsHeight,
                             (unsigned int)devmode->dmBitsPerPel,
                             (unsigned int)devmode->dmDisplayFrequency,
                             (unsigned int)devmode->dmDisplayOrientation * 90,
                             position );
}

static void fixup_device_id(UINT *vendor_id, UINT *device_id)
{
    const char *sgi;

    if (*vendor_id == 0x10de /* NVIDIA */ && (sgi = getenv("WINE_HIDE_NVIDIA_GPU")) && *sgi != '0')
    {
        *vendor_id = 0x1002; /* AMD */
        *device_id = 0x73df; /* RX 6700XT */
    }
    else if (*vendor_id == 0x1002 /* AMD */ && (sgi = getenv("WINE_HIDE_AMD_GPU")) && *sgi != '0')
    {
        *vendor_id = 0x10de; /* NVIDIA */
        *device_id = 0x2487; /* RTX 3060 */
    }
    else if (*vendor_id == 0x1002 && (*device_id == 0x163f || *device_id == 0x1435) && (sgi = getenv("WINE_HIDE_VANGOGH_GPU")) && *sgi != '0')
    {
        *device_id = 0x687f; /* Radeon RX Vega 56/64 */
    }
}

BOOL X11DRV_UpdateDisplayDevices( const struct gdi_device_manager *device_manager, BOOL force, void *param )
{
    struct gdi_adapter *adapters;
    struct gdi_monitor *monitors;
    struct gdi_gpu *gpus;
    INT gpu_count, adapter_count, monitor_count;
    INT gpu, adapter, monitor;
    DEVMODEW *modes, *mode;
    UINT mode_count;

    if (!force && !force_display_devices_refresh) return TRUE;
    force_display_devices_refresh = FALSE;

    TRACE( "via %s\n", debugstr_a(host_handler.name) );

    /* Initialize GPUs */
    if (!host_handler.get_gpus( &gpus, &gpu_count, TRUE )) return FALSE;
    TRACE("GPU count: %d\n", gpu_count);

    for (gpu = 0; gpu < gpu_count; gpu++)
    {
        fixup_device_id( &gpus[gpu].vendor_id, &gpus[gpu].device_id );

        device_manager->add_gpu( &gpus[gpu], param );

        /* Initialize adapters */
        if (!host_handler.get_adapters( gpus[gpu].id, &adapters, &adapter_count )) break;
        TRACE("GPU: %#lx %s, adapter count: %d\n", gpus[gpu].id, wine_dbgstr_w(gpus[gpu].name), adapter_count);

        for (adapter = 0; adapter < adapter_count; adapter++)
        {
            DEVMODEW current_mode = {.dmSize = sizeof(current_mode)};
            WCHAR devname[32];
            char buffer[32];
            x11drv_settings_id settings_id;
            BOOL is_primary = adapters[adapter].state_flags & DISPLAY_DEVICE_PRIMARY_DEVICE;

            device_manager->add_adapter( &adapters[adapter], param );

            if (!host_handler.get_monitors( adapters[adapter].id, &monitors, &monitor_count )) break;
            TRACE("adapter: %#lx, monitor count: %d\n", adapters[adapter].id, monitor_count);

            /* Initialize monitors */
            for (monitor = 0; monitor < monitor_count; monitor++)
                device_manager->add_monitor( &monitors[monitor], param );

            host_handler.free_monitors( monitors, monitor_count );

            /* Get the settings handler id for the adapter */
            snprintf( buffer, sizeof(buffer), "\\\\.\\DISPLAY%d", adapter + 1 );
            asciiz_to_unicode( devname, buffer );
            if (!settings_handler.get_id( devname, is_primary, &settings_id )) break;

            settings_handler.get_current_mode( settings_id, &current_mode );
            if (!settings_handler.get_modes( settings_id, EDS_ROTATEDMODE, &modes, &mode_count ))
                continue;

            for (mode = modes; mode_count; mode_count--)
            {
                if (is_same_devmode( mode, &current_mode ))
                {
                    TRACE( "current mode: %s\n", debugstr_devmodew( &current_mode ) );
                    device_manager->add_mode( &current_mode, TRUE, param );
                }
                else
                {
                    TRACE( "mode: %s\n", debugstr_devmodew( mode ) );
                    device_manager->add_mode( mode, FALSE, param );
                }
                mode = (DEVMODEW *)((char *)mode + sizeof(*modes) + modes[0].dmDriverExtra);
            }

            settings_handler.free_modes( modes );
        }

        host_handler.free_adapters( adapters );
    }

    host_handler.free_gpus( gpus );
    return TRUE;
}

void X11DRV_DisplayDevices_Init(BOOL force)
{
    UINT32 num_path, num_mode;

    if (force) force_display_devices_refresh = TRUE;
    /* trigger refresh in win32u */
    NtUserGetDisplayConfigBufferSizes( QDC_ONLY_ACTIVE_PATHS, &num_path, &num_mode );

    if (!native_screen_rect.bottom) native_screen_rect = NtUserGetVirtualScreenRect();
}
