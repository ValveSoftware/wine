/*
 * Fullscreen Hack
 *
 * Simulate monitor resolution change
 *
 * Copyright 2020 Andrew Eikum for CodeWeavers
 * Copyright 2020 Zhiyi Zhang for CodeWeavers
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
#include <stdlib.h>

#include "x11drv.h"
#include "wine/debug.h"
#include "wine/list.h"

WINE_DEFAULT_DEBUG_CHANNEL(fshack);

static struct x11drv_display_device_handler real_device_handler;
static struct x11drv_settings_handler real_settings_handler;

static BOOL initialized;

/* A table of resolutions some games expect but host system may not report */
static const struct
{
    SIZE size;
    BOOL additional;
}
fs_monitor_sizes[] =
{
    {{640, 480}},   /*  4:3 */
    {{800, 600}},   /*  4:3 */
    {{1024, 768}},  /*  4:3 */
    {{1600, 1200}}, /*  4:3 */
    {{960, 540}},   /* 16:9 */
    {{1280, 720}},  /* 16:9 */
    {{1600, 900}},  /* 16:9 */
    {{1920, 1080}}, /* 16:9 */
    {{2560, 1440}}, /* 16:9 */
    {{2880, 1620}}, /* 16:9 */
    {{3200, 1800}}, /* 16:9 */
    {{1440, 900}},  /*  8:5 */
    {{1680, 1050}}, /*  8:5 */
    {{1920, 1200}}, /*  8:5 */
    {{2560, 1600}}, /*  8:5 */
    {{1440, 960}},  /*  3:2 */
    {{1920, 1280}}, /*  3:2 */
    {{2560, 1080}}, /* 21:9 ultra-wide */
    {{1920, 800}},  /* 12:5 */
    {{3840, 1600}}, /* 12:5 */
    {{1280, 1024}}, /*  5:4 */
    {{1280, 768}, TRUE },
};

/* A fake monitor for the fullscreen hack */
struct fs_monitor
{
    struct list entry;
    UINT_PTR gpu_id;
    UINT_PTR adapter_id;

    DEVMODEW user_mode;         /* Mode changed to by users */
    DEVMODEW real_mode;         /* Mode actually used by the host system */
    double user_to_real_scale;  /* Scale factor from fake monitor to real monitor */
    POINT top_left;             /* Top left corner of the fake monitor rectangle in real virtual screen coordinates */
};

/* Access to fs_monitors is protected by fs_lock */
static pthread_mutex_t fs_lock = PTHREAD_MUTEX_INITIALIZER;
static struct list fs_monitors = LIST_INIT( fs_monitors );

static WORD gamma_ramp_i[GAMMA_RAMP_SIZE * 3];
static float gamma_ramp[GAMMA_RAMP_SIZE * 4];
static LONG gamma_serial;

#define NEXT_DEVMODEW(mode) ((DEVMODEW *)((char *)((mode) + 1) + (mode)->dmDriverExtra))

static const char *debugstr_devmode( const DEVMODEW *devmode )
{
    char buffer[256], *buf = buffer;

    if (devmode->dmFields & DM_BITSPERPEL)
        buf += sprintf( buf, "bits %u ", (int)devmode->dmBitsPerPel );
    if (devmode->dmFields & DM_PELSWIDTH)
        buf += sprintf( buf, "width %u ", (int)devmode->dmPelsWidth );
    if (devmode->dmFields & DM_PELSHEIGHT)
        buf += sprintf( buf, "height %u ", (int)devmode->dmPelsHeight );
    if (devmode->dmFields & DM_DISPLAYFREQUENCY)
        buf += sprintf( buf, "%u Hz ", (int)devmode->dmDisplayFrequency );
    if (devmode->dmFields & DM_POSITION)
        buf += sprintf( buf, "pos (%d,%d) ", (int)devmode->dmPosition.x, (int)devmode->dmPosition.y );
    if (devmode->dmFields & DM_DISPLAYFLAGS)
        buf += sprintf( buf, "flags %#x ", (int)devmode->dmDisplayFlags );
    if (devmode->dmFields & DM_DISPLAYORIENTATION)
        buf += sprintf( buf, "orientation %u ", (int)devmode->dmDisplayOrientation );

    return wine_dbg_sprintf("%s", buffer);
}

static struct fs_monitor *find_adapter_monitor( struct list *monitors, struct gdi_adapter *adapter, DEVMODEW *mode )
{
    struct fs_monitor *monitor;

    LIST_FOR_EACH_ENTRY( monitor, monitors, struct fs_monitor, entry )
    {
        if (monitor->real_mode.dmPosition.x != mode->dmPosition.x) continue;
        if (monitor->real_mode.dmPosition.y != mode->dmPosition.y) continue;
        if (monitor->real_mode.dmPelsWidth != mode->dmPelsWidth) continue;
        if (monitor->real_mode.dmPelsHeight != mode->dmPelsHeight) continue;
        return monitor;
    }

    return NULL;
}

static void update_gpu_monitor_list( struct gdi_gpu *gpu, struct list *monitors )
{
    struct gdi_adapter *adapters;
    struct fs_monitor *monitor;
    int count;

    if (!real_device_handler.get_adapters( gpu->id, &adapters, &count )) return;

    while (count--)
    {
        struct gdi_adapter *adapter = adapters + count;
        DEVMODEW mode = {0};

        TRACE( "adapter %p id %p\n", adapter, (void *)adapter->id );

        if (!real_settings_handler.get_current_mode( adapter->id, &mode ))
        {
            WARN( "Failed to get current display mode\n" );
            continue;
        }

        if ((monitor = find_adapter_monitor( monitors, adapter, &mode )))
        {
            TRACE( "Reusing monitor %p, mode %s\n", monitor, debugstr_devmode( &mode ) );
            list_remove( &monitor->entry );
        }
        else if (!(monitor = calloc( 1, sizeof(*monitor) )))
        {
            WARN( "Failed to allocate monitor\n" );
            continue;
        }
        else
        {
            monitor->gpu_id = gpu->id;
            monitor->user_mode = mode;
            monitor->real_mode = mode;
            monitor->user_to_real_scale = 1.0;
            monitor->top_left.x = mode.dmPosition.x;
            monitor->top_left.y = mode.dmPosition.y;

            TRACE( "Created monitor %p, mode %s\n", monitor, debugstr_devmode( &mode ) );
        }

        monitor->adapter_id = adapter->id;
        list_add_tail( &fs_monitors, &monitor->entry );
    }

    real_device_handler.free_adapters( adapters );
}

static void update_monitor_list( struct gdi_gpu *gpus, int count )
{
    struct list monitors = LIST_INIT( monitors );
    struct fs_monitor *monitor, *next;

    list_move_tail( &monitors, &fs_monitors );

    while (count--)
    {
        struct list gpu_monitors = LIST_INIT( gpu_monitors );
        struct gdi_gpu *gpu = gpus + count;

        TRACE( "gpu %p id %p\n", gpu, (void *)gpu->id );

        LIST_FOR_EACH_ENTRY_SAFE( monitor, next, &monitors, struct fs_monitor, entry )
        {
            if (monitor->gpu_id != gpu->id) continue;
            list_remove( &monitor->entry );
            list_add_tail( &gpu_monitors, &monitor->entry );
        }

        update_gpu_monitor_list( gpu, &gpu_monitors );

        list_move_tail( &monitors, &gpu_monitors );
    }

    LIST_FOR_EACH_ENTRY_SAFE( monitor, next, &monitors, struct fs_monitor, entry )
    {
        TRACE( "Removing stale monitor %p with gpu id %p, adapter id %p\n",
               monitor, (void *)monitor->gpu_id, (void *)monitor->adapter_id );
        free( monitor );
    }
}

static void modes_append( DEVMODEW *modes, UINT *mode_count, UINT *resolutions, DEVMODEW *mode )
{
    BOOL is_new_resolution;
    const char *appid;
    int i;

    /* Titan Souls renders incorrectly if we report modes smaller than 800x600 */
    if ((appid = getenv( "SteamAppId" )) && !strcmp( appid, "297130" ))
    {
        if (mode->dmDisplayOrientation == DMDO_DEFAULT || mode->dmDisplayOrientation == DMDO_180)
        {
            if (mode->dmPelsHeight <= 600 && !(mode->dmPelsHeight == 600 && mode->dmPelsWidth == 800)) return;
        }
        else
        {
            if (mode->dmPelsWidth <= 600 && !(mode->dmPelsWidth == 600 && mode->dmPelsHeight == 800)) return;
        }
    }

    is_new_resolution = TRUE;

    for (i = 0; i < *mode_count; ++i)
    {
        if (modes[i].dmPelsWidth != mode->dmPelsWidth) continue;
        if (modes[i].dmPelsHeight != mode->dmPelsHeight) continue;
        is_new_resolution = FALSE;

        if (modes[i].dmBitsPerPel != mode->dmBitsPerPel) continue;
        if (modes[i].dmDisplayFrequency != mode->dmDisplayFrequency) continue;
        if (modes[i].dmDisplayOrientation != mode->dmDisplayOrientation) continue;
        if ((mode->dmFields & DM_DISPLAYFIXEDOUTPUT) != (modes[i].dmFields & DM_DISPLAYFIXEDOUTPUT)) continue;
        if (mode->dmFields & DM_DISPLAYFIXEDOUTPUT && modes[i].dmDisplayFixedOutput != mode->dmDisplayFixedOutput) continue;
        return; /* The exact mode is already added, nothing to do */
    }

    if (is_new_resolution)
    {
        /* Some games crash if we report too many unique resolutions (in terms of HxW) */
        if (limit_number_of_resolutions && *resolutions >= limit_number_of_resolutions) return;
        *resolutions = *resolutions + 1;
    }

    mode->dmFields = DM_DISPLAYORIENTATION | DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT |
                     DM_DISPLAYFLAGS | DM_DISPLAYFREQUENCY | (mode->dmFields & DM_DISPLAYFIXEDOUTPUT);
    mode->dmSize = sizeof(DEVMODEW);
    mode->dmDriverExtra = 0;
    mode->dmDisplayFlags = 0;

    TRACE( "adding mode %s\n", debugstr_devmode(mode) );

    modes[*mode_count] = *mode;
    *mode_count = *mode_count + 1;
}

static void monitor_get_modes( struct fs_monitor *monitor, DEVMODEW **modes, UINT *mode_count )
{
    UINT i, j, max_count, real_mode_count, resolutions = 0;
    DEVMODEW *real_modes, *real_mode, mode_host = {0};
    BOOL additional_modes = FALSE, center_modes = FALSE, landscape;
    const char *env;

    *mode_count = 0;
    *modes = NULL;

    if (!real_settings_handler.get_current_mode( monitor->adapter_id, &mode_host )) return;
    /* Fullscreen hack doesn't support changing display orientations */
    if (!real_settings_handler.get_modes( monitor->adapter_id, 0, &real_modes, &real_mode_count )) return;

    if ((env = getenv( "WINE_CENTER_DISPLAY_MODES" )))
        center_modes = (env[0] != '0');
    else if ((env = getenv( "SteamAppId" )))
        center_modes = !strcmp( env, "359870" );

    max_count = ARRAY_SIZE(fs_monitor_sizes) * DEPTH_COUNT + real_mode_count;
    if (center_modes) max_count += ARRAY_SIZE(fs_monitor_sizes) + real_mode_count;

    if (!(*modes = calloc( max_count, sizeof(DEVMODEW) )))
    {
        real_settings_handler.free_modes( real_modes );
        return;
    }

    /* Check the ratio of dmPelsWidth to dmPelsHeight to determine whether the host is currently in
     * portrait or landscape orientation. DMDO_DEFAULT is the natural orientation of the device,
     * which isn't necessarily a landscape mode */
    landscape = mode_host.dmPelsWidth >= mode_host.dmPelsHeight;

    /* Add the current mode early, in case we have to limit */
    modes_append( *modes, mode_count, &resolutions, &mode_host );

    if ((env = getenv( "WINE_ADDITIONAL_DISPLAY_MODES" )))
        additional_modes = (env[0] != '0');
    else if ((env = getenv( "SteamAppId" )))
        additional_modes = !strcmp( env, "979400" );

    if ((env = getenv( "WINE_CENTER_DISPLAY_MODES" )))
        center_modes = (env[0] != '0');
    else if ((env = getenv( "SteamAppId" )))
        center_modes = !strcmp( env, "359870" );

    /* Linux reports far fewer resolutions than Windows. Add modes that some games may expect. */
    for (i = 0; i < ARRAY_SIZE(fs_monitor_sizes); ++i)
    {
        DEVMODEW mode = mode_host;

        if (!additional_modes && fs_monitor_sizes[i].additional) continue;

        if (landscape)
        {
            mode.dmPelsWidth = fs_monitor_sizes[i].size.cx;
            mode.dmPelsHeight = fs_monitor_sizes[i].size.cy;
        }
        else
        {
            mode.dmPelsWidth = fs_monitor_sizes[i].size.cy;
            mode.dmPelsHeight = fs_monitor_sizes[i].size.cx;
        }

        /* Don't report modes that are larger than the current mode */
        if (mode.dmPelsWidth > mode_host.dmPelsWidth) continue;
        if (mode.dmPelsHeight > mode_host.dmPelsHeight) continue;

        for (j = 0; j < DEPTH_COUNT; ++j)
        {
            mode.dmBitsPerPel = depths[j];
            mode.dmDisplayFrequency = 60;
            modes_append( *modes, mode_count, &resolutions, &mode );
        }

        if (center_modes && mode.dmPelsWidth != mode_host.dmPelsWidth && mode.dmPelsHeight != mode_host.dmPelsHeight)
        {
            mode.dmFields |= DM_DISPLAYFIXEDOUTPUT;
            mode.dmDisplayFixedOutput = DMDFO_CENTER;
            modes_append( *modes, mode_count, &resolutions, &mode );
        }
    }

    for (i = 0, real_mode = real_modes; i < real_mode_count; ++i)
    {
        DEVMODEW mode = *real_mode;

        /* Don't report modes that are larger than the current mode */
        if (mode.dmPelsWidth <= mode_host.dmPelsWidth && mode.dmPelsHeight <= mode_host.dmPelsHeight)
        {
            modes_append( *modes, mode_count, &resolutions, &mode );

            if (center_modes && mode.dmPelsWidth != mode_host.dmPelsWidth && mode.dmPelsHeight != mode_host.dmPelsHeight)
            {
                mode.dmFields |= DM_DISPLAYFIXEDOUTPUT;
                mode.dmDisplayFixedOutput = DMDFO_CENTER;
                modes_append( *modes, mode_count, &resolutions, &mode );
            }
        }

        real_mode = NEXT_DEVMODEW(real_mode);
    }

    real_settings_handler.free_modes( real_modes );
}

static struct fs_monitor *monitor_from_adapter_id( ULONG_PTR adapter_id )
{
    struct fs_monitor *monitor;
    struct gdi_gpu *gpus;
    int count;

    LIST_FOR_EACH_ENTRY( monitor, &fs_monitors, struct fs_monitor, entry )
        if (monitor->adapter_id == adapter_id) return monitor;

    if (real_device_handler.get_gpus( &gpus, &count ))
    {
        update_monitor_list( gpus, count );
        real_device_handler.free_gpus( gpus );

        LIST_FOR_EACH_ENTRY( monitor, &fs_monitors, struct fs_monitor, entry )
            if (monitor->adapter_id == adapter_id) return monitor;
    }

    WARN( "Failed to find monitor for adapter id %p\n", (void *)adapter_id );
    return NULL;
}

static BOOL fs_get_modes( ULONG_PTR adapter_id, DWORD flags, DEVMODEW **new_modes, UINT *mode_count )
{
    struct fs_monitor *monitor;

    TRACE( "adapter_id %#zx, flags %#x, modes %p, modes_count %p\n",
           (size_t)adapter_id, (int)flags, new_modes, mode_count );

    pthread_mutex_lock( &fs_lock );

    if ((monitor = monitor_from_adapter_id( adapter_id )))
        monitor_get_modes( monitor, new_modes, mode_count );

    pthread_mutex_unlock( &fs_lock );
    return monitor && *new_modes;
}

static void fs_free_modes( DEVMODEW *modes )
{
    free( modes );
}

static BOOL fs_get_current_mode( ULONG_PTR adapter_id, DEVMODEW *mode )
{
    struct fs_monitor *monitor;

    TRACE( "adapter_id %p, mode %p\n", (void *)adapter_id, mode );

    pthread_mutex_lock( &fs_lock );
    if ((monitor = monitor_from_adapter_id( adapter_id )))
        *mode = monitor->user_mode;
    pthread_mutex_unlock( &fs_lock );

    return !!monitor;
}

static LONG fs_set_current_mode( ULONG_PTR adapter_id, const DEVMODEW *user_mode )
{
    struct fs_monitor *fs_monitor;
    DEVMODEW real_mode;
    double scale;

    TRACE( "id %p, mode %s\n", (void *)adapter_id, debugstr_devmode( user_mode ) );

    pthread_mutex_lock( &fs_lock );

    if (!(fs_monitor = monitor_from_adapter_id( adapter_id )))
    {
        pthread_mutex_unlock( &fs_lock );
        return DISP_CHANGE_FAILED;
    }

    if (is_detached_mode( &fs_monitor->real_mode ) && !is_detached_mode( user_mode ))
    {
        FIXME( "Attaching adapters is unsupported with fullscreen hack.\n" );
        return DISP_CHANGE_SUCCESSFUL;
    }

    /* Real modes may be changed since initialization */
    if (!real_settings_handler.get_current_mode( adapter_id, &real_mode ))
    {
        pthread_mutex_unlock( &fs_lock );
        return DISP_CHANGE_FAILED;
    }

    fs_monitor->user_mode = *user_mode;
    fs_monitor->real_mode = real_mode;
    lstrcpyW( fs_monitor->user_mode.dmDeviceName, L"fshack" );

    if (is_detached_mode( user_mode ))
    {
        fs_monitor->user_to_real_scale = 0;
        fs_monitor->top_left.x = 0;
        fs_monitor->top_left.y = 0;
    }
    /* Integer scaling */
    else if (fs_hack_is_integer())
    {
        scale = min( real_mode.dmPelsWidth / user_mode->dmPelsWidth,
                     real_mode.dmPelsHeight / user_mode->dmPelsHeight );
        fs_monitor->user_to_real_scale = scale;
        fs_monitor->top_left.x = real_mode.dmPosition.x +
                                 (real_mode.dmPelsWidth - user_mode->dmPelsWidth * scale) / 2;
        fs_monitor->top_left.y = real_mode.dmPosition.y +
                                 (real_mode.dmPelsHeight - user_mode->dmPelsHeight * scale) / 2;
    }
    /* If real mode is narrower than fake mode, scale to fit width */
    else if ((double)real_mode.dmPelsWidth / (double)real_mode.dmPelsHeight <
             (double)user_mode->dmPelsWidth / (double)user_mode->dmPelsHeight)
    {
        scale = (double)real_mode.dmPelsWidth / (double)user_mode->dmPelsWidth;
        fs_monitor->user_to_real_scale = scale;
        fs_monitor->top_left.x = real_mode.dmPosition.x;
        fs_monitor->top_left.y = real_mode.dmPosition.y +
                                 (real_mode.dmPelsHeight - user_mode->dmPelsHeight * scale) / 2;
    }
    /* Else scale to fit height */
    else
    {
        scale = (double)real_mode.dmPelsHeight / (double)user_mode->dmPelsHeight;
        fs_monitor->user_to_real_scale = scale;
        fs_monitor->top_left.x = real_mode.dmPosition.x +
                                 (real_mode.dmPelsWidth - user_mode->dmPelsWidth * scale) / 2;
        fs_monitor->top_left.y = real_mode.dmPosition.y;
    }

    TRACE( "real_mode x %d y %d width %d height %d\n", (int)real_mode.dmPosition.x,
           (int)real_mode.dmPosition.y, (int)real_mode.dmPelsWidth, (int)real_mode.dmPelsHeight );
    TRACE( "user_mode x %d y %d width %d height %d\n", (int)user_mode->dmPosition.x,
           (int)user_mode->dmPosition.y, (int)user_mode->dmPelsWidth, (int)user_mode->dmPelsHeight );
    TRACE( "user_to_real_scale %lf\n", fs_monitor->user_to_real_scale );
    TRACE( "top left corner:%s\n", wine_dbgstr_point( &fs_monitor->top_left ) );

    pthread_mutex_unlock( &fs_lock );
    return DISP_CHANGE_SUCCESSFUL;
}

/* Display device handler functions */

static BOOL fs_get_gpus( struct gdi_gpu **gpus, int *count )
{
    struct list monitors = LIST_INIT( monitors );

    TRACE( "gpus %p, count %p\n", gpus, count );

    if (!real_device_handler.get_gpus( gpus, count )) return FALSE;

    pthread_mutex_lock( &fs_lock );
    update_monitor_list( *gpus, *count );
    pthread_mutex_unlock( &fs_lock );

    return TRUE;
}

static BOOL fs_get_monitors( ULONG_PTR adapter_id, struct gdi_monitor **new_monitors, int *count )
{
    struct fs_monitor *fs_monitor;
    struct gdi_monitor *monitor;
    RECT rect;
    INT i;

    TRACE( "adapter_id %p, monitors %p, count %p\n", (void *)adapter_id, new_monitors, count );

    if (!real_device_handler.get_monitors( adapter_id, new_monitors, count )) return FALSE;

    pthread_mutex_lock( &fs_lock );

    for (i = 0; i < *count; ++i)
    {
        monitor = &(*new_monitors)[i];

        LIST_FOR_EACH_ENTRY( fs_monitor, &fs_monitors, struct fs_monitor, entry )
        {
            rect.left = fs_monitor->real_mode.dmPosition.x;
            rect.top = fs_monitor->real_mode.dmPosition.y;
            rect.right = rect.left + fs_monitor->real_mode.dmPelsWidth;
            rect.bottom = rect.top + fs_monitor->real_mode.dmPelsHeight;

            if (EqualRect( &rect, &monitor->rc_monitor ))
            {
                monitor->rc_monitor.left = fs_monitor->user_mode.dmPosition.x;
                monitor->rc_monitor.top = fs_monitor->user_mode.dmPosition.y;
                monitor->rc_monitor.right = monitor->rc_monitor.left + fs_monitor->user_mode.dmPelsWidth;
                monitor->rc_monitor.bottom = monitor->rc_monitor.top + fs_monitor->user_mode.dmPelsHeight;
                monitor->rc_work = monitor->rc_monitor;
                monitor->state_flags = DISPLAY_DEVICE_ATTACHED;
                if (fs_monitor->user_mode.dmPelsWidth && fs_monitor->user_mode.dmPelsHeight)
                    monitor->state_flags |= DISPLAY_DEVICE_ACTIVE;
            }
        }
    }

    pthread_mutex_unlock( &fs_lock );

    return TRUE;
}

/* Fullscreen hack helpers */

static struct fs_monitor *monitor_from_handle( HMONITOR handle )
{
    MONITORINFOEXW info = {.cbSize = sizeof(MONITORINFOEXW)};
    ULONG_PTR adapter_id;
    BOOL is_primary;

    TRACE( "handle %p\n", handle );

    if (!initialized) return NULL;

    if (!NtUserGetMonitorInfo( handle, (MONITORINFO *)&info )) return NULL;
    is_primary = !!(info.dwFlags & MONITORINFOF_PRIMARY);
    if (!real_settings_handler.get_id( info.szDevice, is_primary, &adapter_id )) return FALSE;
    return monitor_from_adapter_id( adapter_id );
}

/* Return whether fullscreen hack is enabled on a specific monitor */
BOOL fs_hack_enabled( HMONITOR monitor )
{
    struct fs_monitor *fs_monitor;
    BOOL enabled = FALSE;

    TRACE( "monitor %p\n", monitor );

    pthread_mutex_lock( &fs_lock );
    fs_monitor = monitor_from_handle( monitor );
    if (fs_monitor && (fs_monitor->user_mode.dmPelsWidth != fs_monitor->real_mode.dmPelsWidth ||
                       fs_monitor->user_mode.dmPelsHeight != fs_monitor->real_mode.dmPelsHeight))
        enabled = TRUE;
    pthread_mutex_unlock( &fs_lock );
    TRACE( "enabled: %s\n", enabled ? "TRUE" : "FALSE" );
    return enabled;
}

BOOL fs_hack_mapping_required( HMONITOR monitor )
{
    BOOL required;

    TRACE( "monitor %p\n", monitor );

    /* steamcompmgr does our mapping for us */
    required = !wm_is_steamcompmgr( NULL ) && fs_hack_enabled( monitor );
    TRACE( "required: %s\n", required ? "TRUE" : "FALSE" );
    return required;
}

/* Return whether integer scaling is on */
BOOL fs_hack_is_integer(void)
{
    static int is_int = -1;
    if (is_int < 0)
    {
        const char *e = getenv( "WINE_FULLSCREEN_INTEGER_SCALING" );
        is_int = e && strcmp( e, "0" );
    }
    TRACE( "is_interger_scaling: %s\n", is_int ? "TRUE" : "FALSE" );
    return is_int;
}

HMONITOR fs_hack_monitor_from_rect( const RECT *in_rect )
{
    RECT rect = *in_rect;

    TRACE( "rect %s\n", wine_dbgstr_rect( &rect ) );
    rect.right = rect.left + 1;
    rect.bottom = rect.top + 1;
    return NtUserMonitorFromRect( &rect, MONITOR_DEFAULTTOPRIMARY );
}

/* Get the monitor a window is on. MonitorFromWindow() doesn't work here because it finds the
 * monitor with the maximum overlapped rectangle when a window is spanned over two monitors, whereas
 * for the fullscreen hack, the monitor where the left top corner of the window is on is the correct
 * one. For example, a game with a window of 3840x2160 changes the primary monitor to 1280x720, if
 * there is a secondary monitor of 3840x2160 to the right, MonitorFromWindow() will return the
 * secondary monitor instead of the primary one. */
HMONITOR fs_hack_monitor_from_hwnd( HWND hwnd )
{
    RECT rect = {0};

    if (!NtUserGetWindowRect( hwnd, &rect )) ERR( "Invalid hwnd %p.\n", hwnd );

    TRACE( "hwnd %p rect %s\n", hwnd, wine_dbgstr_rect( &rect ) );
    return fs_hack_monitor_from_rect( &rect );
}

/* Return the rectangle of a monitor in current mode in user virtual screen coordinates */
RECT fs_hack_current_mode( HMONITOR monitor )
{
    struct fs_monitor *fs_monitor;
    RECT rect = {0};

    TRACE( "monitor %p\n", monitor );

    pthread_mutex_lock( &fs_lock );
    fs_monitor = monitor_from_handle( monitor );
    if (!fs_monitor)
    {
        pthread_mutex_unlock( &fs_lock );
        return rect;
    }

    rect.left = fs_monitor->user_mode.dmPosition.x;
    rect.top = fs_monitor->user_mode.dmPosition.y;
    rect.right = rect.left + fs_monitor->user_mode.dmPelsWidth;
    rect.bottom = rect.top + fs_monitor->user_mode.dmPelsHeight;
    pthread_mutex_unlock( &fs_lock );
    TRACE( "current mode rect: %s\n", wine_dbgstr_rect( &rect ) );
    return rect;
}

/* Return the rectangle of a monitor in real mode in real virtual screen coordinates */
RECT fs_hack_real_mode( HMONITOR monitor )
{
    struct fs_monitor *fs_monitor;
    RECT rect = {0};

    TRACE( "monitor %p\n", monitor );

    pthread_mutex_lock( &fs_lock );
    fs_monitor = monitor_from_handle( monitor );
    if (!fs_monitor)
    {
        pthread_mutex_unlock( &fs_lock );
        return rect;
    }

    rect.left = fs_monitor->real_mode.dmPosition.x;
    rect.top = fs_monitor->real_mode.dmPosition.y;
    rect.right = rect.left + fs_monitor->real_mode.dmPelsWidth;
    rect.bottom = rect.top + fs_monitor->real_mode.dmPelsHeight;
    pthread_mutex_unlock( &fs_lock );
    TRACE( "real mode rect: %s\n", wine_dbgstr_rect( &rect ) );
    return rect;
}

/* Return whether width and height are the same as the current mode used by a monitor */
BOOL fs_hack_matches_current_mode( HMONITOR monitor, INT width, INT height )
{
    MONITORINFO info = {.cbSize = sizeof(MONITORINFO)};
    BOOL matched;

    TRACE( "monitor %p\n", monitor );

    if (!NtUserGetMonitorInfo( monitor, &info )) return FALSE;

    matched = (width == info.rcMonitor.right - info.rcMonitor.left) &&
              (height == info.rcMonitor.bottom - info.rcMonitor.top);
    TRACE( "matched: %s\n", matched ? "TRUE" : "FALSE" );

    return matched;
}

/* Transform a point in user virtual screen coordinates to real virtual screen coordinates */
void fs_hack_point_user_to_real( POINT *pos )
{
    struct fs_monitor *fs_monitor;
    RECT rect;

    TRACE( "from %s\n", wine_dbgstr_point( pos ) );

    if (wm_is_steamcompmgr( NULL )) return;

    pthread_mutex_lock( &fs_lock );
    LIST_FOR_EACH_ENTRY( fs_monitor, &fs_monitors, struct fs_monitor, entry )
    {
        rect.left = fs_monitor->user_mode.dmPosition.x;
        rect.top = fs_monitor->user_mode.dmPosition.y;
        rect.right = rect.left + fs_monitor->user_mode.dmPelsWidth;
        rect.bottom = rect.top + fs_monitor->user_mode.dmPelsHeight;

        if (PtInRect( &rect, *pos ))
        {
            pos->x -= fs_monitor->user_mode.dmPosition.x;
            pos->y -= fs_monitor->user_mode.dmPosition.y;
            pos->x = lround( pos->x * fs_monitor->user_to_real_scale );
            pos->y = lround( pos->y * fs_monitor->user_to_real_scale );
            pos->x += fs_monitor->top_left.x;
            pos->y += fs_monitor->top_left.y;
            pthread_mutex_unlock( &fs_lock );
            TRACE( "to %s\n", wine_dbgstr_point( pos ) );
            return;
        }
    }
    pthread_mutex_unlock( &fs_lock );
    WARN( "%s not transformed.\n", wine_dbgstr_point( pos ) );
}

/* Transform a point in real virtual screen coordinates to user virtual screen coordinates */
void fs_hack_point_real_to_user( POINT *pos )
{
    struct fs_monitor *fs_monitor;
    RECT rect;

    TRACE( "from %s\n", wine_dbgstr_point( pos ) );

    if (wm_is_steamcompmgr( NULL )) return;

    pthread_mutex_lock( &fs_lock );
    LIST_FOR_EACH_ENTRY( fs_monitor, &fs_monitors, struct fs_monitor, entry )
    {
        rect.left = fs_monitor->real_mode.dmPosition.x;
        rect.top = fs_monitor->real_mode.dmPosition.y;
        rect.right = rect.left + fs_monitor->real_mode.dmPelsWidth;
        rect.bottom = rect.top + fs_monitor->real_mode.dmPelsHeight;

        if (PtInRect( &rect, *pos ))
        {
            pos->x -= fs_monitor->top_left.x;
            pos->y -= fs_monitor->top_left.y;
            pos->x = lround( pos->x / fs_monitor->user_to_real_scale );
            pos->y = lround( pos->y / fs_monitor->user_to_real_scale );
            pos->x += fs_monitor->user_mode.dmPosition.x;
            pos->y += fs_monitor->user_mode.dmPosition.y;
            pos->x = max( pos->x, fs_monitor->user_mode.dmPosition.x );
            pos->y = max( pos->y, fs_monitor->user_mode.dmPosition.y );
            pos->x = min( pos->x, fs_monitor->user_mode.dmPosition.x +
                                      (INT)fs_monitor->user_mode.dmPelsWidth - 1 );
            pos->y = min( pos->y, fs_monitor->user_mode.dmPosition.y +
                                      (INT)fs_monitor->user_mode.dmPelsHeight - 1 );
            pthread_mutex_unlock( &fs_lock );
            TRACE( "to %s\n", wine_dbgstr_point( pos ) );
            return;
        }
    }
    pthread_mutex_unlock( &fs_lock );
    WARN( "%s not transformed.\n", wine_dbgstr_point( pos ) );
}

/* Transform RGNDATA in user virtual screen coordinates to real virtual screen coordinates.
 * This is for clipping. Be sure to use Unsorted for Xlib calls after this transformation because
 * this may break the requirement of using YXBanded. For example, say there are two monitors aligned
 * horizontally with the primary monitor on the right. Each of monitor is of real resolution
 * 1920x1080 and the fake primary monitor resolution is 1024x768. Then (0, 10, 1024, 768) should be
 * transformed to (0, 14, 1920, 1080). While (1024, 10, 2944, 1080) should be transformed to
 * (1920, 10, 3840, 1080) and this is breaking YXBanded because it requires y in non-decreasing order */
void fs_hack_rgndata_user_to_real( RGNDATA *data )
{
    unsigned int i;
    XRectangle *xrect;
    RECT rect;

    if (!data || wm_is_steamcompmgr( NULL )) return;

    xrect = (XRectangle *)data->Buffer;
    for (i = 0; i < data->rdh.nCount; i++)
    {
        rect.left = xrect[i].x;
        rect.top = xrect[i].y;
        rect.right = xrect[i].x + xrect[i].width;
        rect.bottom = xrect[i].y + xrect[i].height;
        TRACE( "from rect %s\n", wine_dbgstr_rect( &rect ) );
        fs_hack_rect_user_to_real( &rect );
        TRACE( "to rect %s\n", wine_dbgstr_rect( &rect ) );
        xrect[i].x = rect.left;
        xrect[i].y = rect.top;
        xrect[i].width = rect.right - rect.left;
        xrect[i].height = rect.bottom - rect.top;
    }
}

/* Transform a rectangle in user virtual screen coordinates to real virtual screen coordinates. A
 * difference compared to fs_hack_point_user_to_real() is that fs_hack_point_user_to_real() finds
 * the wrong monitor if the point is on the right edge of the monitor rectangle. For example, when
 * there are two monitors of real size 1920x1080, the primary monitor is of user mode 1024x768 and
 * the secondary monitor is to the right. Rectangle (0, 0, 1024, 768) should transform to
 * (0, 0, 1920, 1080). If (1024, 768) is passed to fs_hack_point_user_to_real(),
 * fs_hack_point_user_to_real() will think (1024, 768) is on the secondary monitor, ends up
 * returning a wrong result to callers. */
void fs_hack_rect_user_to_real( RECT *rect )
{
    struct fs_monitor *fs_monitor;
    HMONITOR monitor;
    RECT point;

    TRACE( "from %s\n", wine_dbgstr_rect( rect ) );

    if (wm_is_steamcompmgr( NULL )) return;

    SetRect( &point, rect->left, rect->top, rect->left + 1, rect->top + 1 );
    monitor = NtUserMonitorFromRect( &point, MONITOR_DEFAULTTONEAREST );
    pthread_mutex_lock( &fs_lock );
    fs_monitor = monitor_from_handle( monitor );
    if (!fs_monitor)
    {
        pthread_mutex_unlock( &fs_lock );
        WARN( "%s not transformed.\n", wine_dbgstr_rect( rect ) );
        return;
    }

    OffsetRect( rect, -fs_monitor->user_mode.dmPosition.x,
                -fs_monitor->user_mode.dmPosition.y );
    rect->left = lround( rect->left * fs_monitor->user_to_real_scale );
    rect->right = lround( rect->right * fs_monitor->user_to_real_scale );
    rect->top = lround( rect->top * fs_monitor->user_to_real_scale );
    rect->bottom = lround( rect->bottom * fs_monitor->user_to_real_scale );
    OffsetRect( rect, fs_monitor->top_left.x, fs_monitor->top_left.y );
    pthread_mutex_unlock( &fs_lock );
    TRACE( "to %s\n", wine_dbgstr_rect( rect ) );
}

/* Get the user_to_real_scale value in a monitor */
double fs_hack_get_user_to_real_scale( HMONITOR monitor )
{
    struct fs_monitor *fs_monitor;
    double scale = 1.0;

    TRACE( "monitor %p\n", monitor );

    if (wm_is_steamcompmgr( NULL )) return scale;

    pthread_mutex_lock( &fs_lock );
    fs_monitor = monitor_from_handle( monitor );
    if (!fs_monitor)
    {
        pthread_mutex_unlock( &fs_lock );
        return scale;
    }
    scale = fs_monitor->user_to_real_scale;

    pthread_mutex_unlock( &fs_lock );
    TRACE( "scale %lf\n", scale );
    return scale;
}

/* Get the scaled scree size of a monitor */
SIZE fs_hack_get_scaled_screen_size( HMONITOR monitor )
{
    struct fs_monitor *fs_monitor;
    SIZE size = {0};

    TRACE( "monitor %p\n", monitor );

    pthread_mutex_lock( &fs_lock );
    fs_monitor = monitor_from_handle( monitor );
    if (!fs_monitor)
    {
        pthread_mutex_unlock( &fs_lock );
        return size;
    }

    if (wm_is_steamcompmgr( NULL ))
    {
        pthread_mutex_unlock( &fs_lock );
        size.cx = fs_monitor->user_mode.dmPelsWidth;
        size.cy = fs_monitor->user_mode.dmPelsHeight;
        TRACE( "width %d height %d\n", (int)size.cx, (int)size.cy );
        return size;
    }

    size.cx = lround( fs_monitor->user_mode.dmPelsWidth * fs_monitor->user_to_real_scale );
    size.cy = lround( fs_monitor->user_mode.dmPelsHeight * fs_monitor->user_to_real_scale );
    pthread_mutex_unlock( &fs_lock );
    TRACE( "width %d height %d\n", (int)size.cx, (int)size.cy );
    return size;
}

/* Get the real virtual screen size instead of virtual screen size using fake modes */
RECT fs_hack_get_real_virtual_screen(void)
{
    struct fs_monitor *fs_monitor;
    RECT rect, virtual = {0};

    pthread_mutex_lock( &fs_lock );
    LIST_FOR_EACH_ENTRY( fs_monitor, &fs_monitors, struct fs_monitor, entry )
    {
        rect.left = fs_monitor->real_mode.dmPosition.x;
        rect.top = fs_monitor->real_mode.dmPosition.y;
        rect.right = rect.left + fs_monitor->real_mode.dmPelsWidth;
        rect.bottom = rect.top + fs_monitor->real_mode.dmPelsHeight;

        union_rect( &virtual, &virtual, &rect );
    }
    pthread_mutex_unlock( &fs_lock );
    TRACE( "real virtual screen rect:%s\n", wine_dbgstr_rect( &virtual ) );
    return virtual;
}

/* Initialize the fullscreen hack, which is a layer on top of real settings handlers and real
 * display device handlers */
void fs_hack_init(void)
{
    struct x11drv_display_device_handler device_handler;
    struct x11drv_settings_handler settings_handler;
    RECT rect;

    real_device_handler = X11DRV_DisplayDevices_GetHandler();
    real_settings_handler = X11DRV_Settings_GetHandler();

    rect = get_host_primary_monitor_rect();
    xinerama_init( rect.right - rect.left, rect.bottom - rect.top );

    settings_handler.name = "Fullscreen Hack";
    settings_handler.priority = 500;
    settings_handler.get_id = real_settings_handler.get_id;
    settings_handler.get_modes = fs_get_modes;
    settings_handler.free_modes = fs_free_modes;
    settings_handler.get_current_mode = fs_get_current_mode;
    settings_handler.set_current_mode = fs_set_current_mode;
    X11DRV_Settings_SetHandler( &settings_handler );

    device_handler.name = "Fullscreen Hack";
    device_handler.priority = 500;
    device_handler.get_gpus = fs_get_gpus;
    device_handler.get_adapters = real_device_handler.get_adapters;
    device_handler.get_monitors = fs_get_monitors;
    device_handler.free_gpus = real_device_handler.free_gpus;
    device_handler.free_adapters = real_device_handler.free_adapters;
    device_handler.free_monitors = real_device_handler.free_monitors;
    device_handler.register_event_handlers = real_device_handler.register_event_handlers;
    X11DRV_DisplayDevices_SetHandler( &device_handler );

    initialized = TRUE;
}

const float *fs_hack_get_gamma_ramp( LONG *serial )
{
    if (gamma_serial == 0) return NULL;
    if (serial) *serial = gamma_serial;
    return gamma_ramp;
}

void fs_hack_set_gamma_ramp( const WORD *ramp )
{
    int i;
    if (memcmp( gamma_ramp_i, ramp, sizeof(gamma_ramp_i) ) == 0)
    {
        /* identical */
        return;
    }
    for (i = 0; i < GAMMA_RAMP_SIZE; ++i)
    {
        gamma_ramp[i * 4] = ramp[i] / 65535.f;
        gamma_ramp[i * 4 + 1] = ramp[i + GAMMA_RAMP_SIZE] / 65535.f;
        gamma_ramp[i * 4 + 2] = ramp[i + 2 * GAMMA_RAMP_SIZE] / 65535.f;
    }
    memcpy( gamma_ramp_i, ramp, sizeof(gamma_ramp_i) );
    InterlockedIncrement( &gamma_serial );
    TRACE( "new gamma serial: %u\n", (int)gamma_serial );
    if (gamma_serial == 0)
    {
        InterlockedIncrement( &gamma_serial );
    }
}
