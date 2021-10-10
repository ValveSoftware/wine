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
#include "config.h"
#include <math.h>
#include <stdlib.h>

#define NONAMELESSSTRUCT
#define NONAMELESSUNION

#include "x11drv.h"
#include "wine/debug.h"
#include "wine/list.h"
#include "wine/heap.h"
#include "wine/unicode.h"

WINE_DEFAULT_DEBUG_CHANNEL(fshack);

static struct x11drv_display_device_handler real_device_handler;
static struct x11drv_settings_handler real_settings_handler;
static struct list fs_monitors = LIST_INIT(fs_monitors);

/* Access to fs_monitors is protected by fs_section */
static CRITICAL_SECTION fs_section;
static CRITICAL_SECTION_DEBUG critsect_debug =
{
    0, 0, &fs_section,
    {&critsect_debug.ProcessLocksList, &critsect_debug.ProcessLocksList},
    0, 0, {(DWORD_PTR)(__FILE__ ": fs_section")}
};
static CRITICAL_SECTION fs_section = {&critsect_debug, -1, 0, 0, 0, 0};

struct fs_monitor_size
{
    DWORD width;
    DWORD height;
};

/* A table of resolutions some games expect but host system may not report */
static struct fs_monitor_size fs_monitor_sizes[] =
{
    {640, 480},   /*  4:3 */
    {800, 600},   /*  4:3 */
    {1024, 768},  /*  4:3 */
    {1600, 1200}, /*  4:3 */
    {960, 540},   /* 16:9 */
    {1280, 720},  /* 16:9 */
    {1600, 900},  /* 16:9 */
    {1920, 1080}, /* 16:9 */
    {2560, 1440}, /* 16:9 */
    {2880, 1620}, /* 16:9 */
    {3200, 1800}, /* 16:9 */
    {1440, 900},  /*  8:5 */
    {1680, 1050}, /*  8:5 */
    {1920, 1200}, /*  8:5 */
    {2560, 1600}, /*  8:5 */
    {1440, 960},  /*  3:2 */
    {1920, 1280}, /*  3:2 */
    {2560, 1080}, /* 21:9 ultra-wide */
    {1920, 800},  /* 12:5 */
    {3840, 1600}, /* 12:5 */
    {1280, 1024}, /*  5:4 */
};

/* A fake monitor for the fullscreen hack */
struct fs_monitor
{
    struct list entry;

    DEVMODEW user_mode;         /* Mode changed to by users */
    DEVMODEW real_mode;         /* Mode actually used by the host system */
    double user_to_real_scale;  /* Scale factor from fake monitor to real monitor */
    POINT top_left;             /* Top left corner of the fake monitor rectangle in real virtual screen coordinates */
    DEVMODEW *modes;            /* Supported display modes */
    UINT mode_count;            /* Display mode count */
    UINT unique_resolutions;    /* Number of unique resolutions in terms of WxH */
};

static void add_fs_mode(struct fs_monitor *fs_monitor, DWORD depth, DWORD width, DWORD height,
                        DWORD frequency, DWORD orientation)
{
    int i;
    DEVMODEW *mode;
    const char *appid;
    BOOL is_new_resolution;

    /* Titan Souls renders incorrectly if we report modes smaller than 800x600 */
    if ((appid = getenv("SteamAppId")) && !strcmp(appid, "297130"))
    {
        if (orientation == DMDO_DEFAULT || orientation == DMDO_180)
        {
            if (height <= 600 && !(height == 600 && width == 800))
                return;
        }
        else
        {
            if (width <= 600 && !(width == 600 && height == 800))
                return;
        }
    }

    is_new_resolution = TRUE;

    for (i = 0; i < fs_monitor->mode_count; ++i)
    {
        if (fs_monitor->modes[i].dmPelsWidth == width &&
            fs_monitor->modes[i].dmPelsHeight == height)
        {
            is_new_resolution = FALSE;

            if (fs_monitor->modes[i].dmBitsPerPel == depth &&
                fs_monitor->modes[i].dmDisplayFrequency == frequency &&
                fs_monitor->modes[i].u1.s2.dmDisplayOrientation == orientation)
                return; /* The exact mode is already added, nothing to do */
        }
    }

    if (is_new_resolution) {
        /* Some games crash if we report too many unique resolutions (in terms of HxW) */
        if (limit_number_of_resolutions && fs_monitor->unique_resolutions >= limit_number_of_resolutions)
            return;

        fs_monitor->unique_resolutions++;
    }

    mode = &fs_monitor->modes[fs_monitor->mode_count++];
    mode->dmSize = sizeof(*mode);

    mode->dmDriverExtra = 0;
    mode->dmFields = DM_DISPLAYORIENTATION | DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT |
                     DM_DISPLAYFLAGS | DM_DISPLAYFREQUENCY;
    mode->u1.s2.dmDisplayOrientation = orientation;
    mode->dmBitsPerPel = depth;
    mode->dmPelsWidth = width;
    mode->dmPelsHeight = height;
    mode->u2.dmDisplayFlags = 0;
    mode->dmDisplayFrequency = frequency;
}

static BOOL fs_monitor_add_modes(struct fs_monitor *fs_monitor)
{
    DEVMODEW *real_modes, *real_mode, current_mode;
    UINT real_mode_count;
    DWORD width, height;
    ULONG_PTR real_id;
    ULONG offset;
    UINT i, j;

    if (!real_settings_handler.get_id(fs_monitor->user_mode.dmDeviceName, &real_id))
        return FALSE;

    if (!real_settings_handler.get_current_mode(real_id, &current_mode))
        return FALSE;

    /* Fullscreen hack doesn't support changing display orientations */
    if (!real_settings_handler.get_modes(real_id, 0, &real_modes, &real_mode_count))
        return FALSE;

    fs_monitor->mode_count = 0;
    fs_monitor->unique_resolutions = 0;
    fs_monitor->modes = heap_calloc(ARRAY_SIZE(fs_monitor_sizes) * DEPTH_COUNT + real_mode_count,
                                    sizeof(*fs_monitor->modes));
    if (!fs_monitor->modes)
    {
        real_settings_handler.free_modes(real_modes);
        return FALSE;
    }

    /* Add the current mode early, in case we have to limit */
    add_fs_mode(fs_monitor, current_mode.dmBitsPerPel, current_mode.dmPelsWidth,
                current_mode.dmPelsHeight, current_mode.dmDisplayFrequency,
                current_mode.u1.s2.dmDisplayOrientation);

    /* Linux reports far fewer resolutions than Windows. Add modes that some games may expect. */
    for (i = 0; i < ARRAY_SIZE(fs_monitor_sizes); ++i)
    {
        if (current_mode.u1.s2.dmDisplayOrientation == DMDO_DEFAULT ||
            current_mode.u1.s2.dmDisplayOrientation == DMDO_180)
        {
            width = fs_monitor_sizes[i].width;
            height = fs_monitor_sizes[i].height;
        }
        else
        {
            width = fs_monitor_sizes[i].height;
            height = fs_monitor_sizes[i].width;
        }

        /* Don't report modes that are larger than the current mode */
        if (width > current_mode.dmPelsWidth || height > current_mode.dmPelsHeight)
            continue;

        for (j = 0; j < DEPTH_COUNT; ++j)
            add_fs_mode(fs_monitor, depths[j], width, height, 60,
                        current_mode.u1.s2.dmDisplayOrientation);
    }

    for (i = 0; i < real_mode_count; ++i)
    {
        offset = (sizeof(*real_modes) + real_modes[0].dmDriverExtra) * i;
        real_mode = (DEVMODEW *)((BYTE *)real_modes + offset);

        /* Don't report real modes that are larger than the current mode */
        if (real_mode->dmPelsWidth > current_mode.dmPelsWidth ||
            real_mode->dmPelsHeight > current_mode.dmPelsHeight)
            continue;

        add_fs_mode(fs_monitor, real_mode->dmBitsPerPel, real_mode->dmPelsWidth,
                    real_mode->dmPelsHeight, real_mode->dmDisplayFrequency,
                    real_mode->u1.s2.dmDisplayOrientation);
    }
    real_settings_handler.free_modes(real_modes);

    /* Sort display modes so that X11DRV_EnumDisplaySettingsEx gets an already sorted mode list */
    qsort(fs_monitor->modes, fs_monitor->mode_count, sizeof(*fs_monitor->modes), mode_compare);
    return TRUE;
}

/* Add a fake monitor to fs_monitors list.
 * Call this function with fs_section entered */
static BOOL fs_add_monitor(const WCHAR *device_name)
{
    struct fs_monitor *fs_monitor;
    DEVMODEW real_mode;
    ULONG_PTR real_id;

    if (!real_settings_handler.get_id(device_name, &real_id))
        return FALSE;

    if (!real_settings_handler.get_current_mode(real_id, &real_mode))
        return FALSE;

    if (!(fs_monitor = heap_alloc(sizeof(*fs_monitor))))
        return FALSE;

    fs_monitor->user_mode = real_mode;
    fs_monitor->real_mode = real_mode;
    fs_monitor->user_to_real_scale = 1.0;
    fs_monitor->top_left.x = real_mode.u1.s2.dmPosition.x;
    fs_monitor->top_left.y = real_mode.u1.s2.dmPosition.y;
    lstrcpyW(fs_monitor->user_mode.dmDeviceName, device_name);
    if (!fs_monitor_add_modes(fs_monitor))
    {
        ERR("Failed to initialize display modes.\n");
        heap_free(fs_monitor);
        return FALSE;
    }
    list_add_tail(&fs_monitors, &fs_monitor->entry);
    return TRUE;
}

/* Fullscreen settings handler */

/* Convert fullscreen hack settings handler id to GDI device name */
static void fs_id_to_device_name(ULONG_PTR id, WCHAR *device_name)
{
    static WCHAR display_fmtW[] = {'\\','\\','.','\\','D','I','S','P','L','A','Y','%','d',0};
    sprintfW(device_name, display_fmtW, (INT)id);
}

static BOOL fs_get_id(const WCHAR *device_name, ULONG_PTR *id)
{
    static const WCHAR displayW[] = {'\\','\\','.','\\','D','I','S','P','L','A','Y'};
    long int display_index;
    WCHAR *end;

    if (strncmpiW( device_name, displayW, ARRAY_SIZE(displayW) ))
        return FALSE;

    display_index = strtolW( device_name + ARRAY_SIZE(displayW), &end, 10 );
    if (*end)
        return FALSE;

    *id = (ULONG_PTR)display_index;
    return TRUE;
}

/* Find a fs_monitor from a display name.
 * Call this function with fs_section entered */
static struct fs_monitor *fs_get_monitor_by_name(const WCHAR *name)
{
    struct fs_monitor *fs_monitor;

    TRACE("name %s\n", wine_dbgstr_w(name));

    LIST_FOR_EACH_ENTRY(fs_monitor, &fs_monitors, struct fs_monitor, entry)
    {
        if (!lstrcmpiW(fs_monitor->user_mode.dmDeviceName, name))
            return fs_monitor;
    }

    return NULL;
}

static BOOL fs_get_modes(ULONG_PTR id, DWORD flags, DEVMODEW **new_modes, UINT *mode_count)
{
    WCHAR device_name[CCHDEVICENAME];
    struct fs_monitor *fs_monitor;

    fs_id_to_device_name(id, device_name);
    EnterCriticalSection(&fs_section);
    if ((fs_monitor = fs_get_monitor_by_name(device_name)))
    {
        *new_modes = fs_monitor->modes;
        *mode_count = fs_monitor->mode_count;
        LeaveCriticalSection(&fs_section);
        return TRUE;
    }

    LeaveCriticalSection(&fs_section);
    return FALSE;
}

static void fs_free_modes(DEVMODEW *modes){}

/* Find a fs_monitor from a HMONITOR handle.
 * Call this function with fs_section entered */
static struct fs_monitor *fs_find_monitor_by_handle(HMONITOR monitor)
{
    MONITORINFOEXW monitor_info;

    TRACE("monitor %p\n", monitor);

    monitor_info.cbSize = sizeof(monitor_info);
    if (!GetMonitorInfoW(monitor, (MONITORINFO *)&monitor_info))
        return NULL;

    return fs_get_monitor_by_name(monitor_info.szDevice);
}

static BOOL fs_get_current_mode(ULONG_PTR id, DEVMODEW *mode)
{
    WCHAR device_name[CCHDEVICENAME];
    struct fs_monitor *fs_monitor;

    fs_id_to_device_name(id, device_name);

    EnterCriticalSection(&fs_section);
    fs_monitor = fs_get_monitor_by_name(device_name);
    if (fs_monitor)
    {
        *mode = fs_monitor->user_mode;
        LeaveCriticalSection(&fs_section);
        return TRUE;
    }
    LeaveCriticalSection(&fs_section);
    return FALSE;
}

static LONG fs_set_current_mode(ULONG_PTR id, DEVMODEW *user_mode)
{
    WCHAR device_name[CCHDEVICENAME];
    struct fs_monitor *fs_monitor;
    DEVMODEW real_mode;
    ULONG_PTR real_id;
    double scale;

    fs_id_to_device_name(id, device_name);

    EnterCriticalSection(&fs_section);
    fs_monitor = fs_get_monitor_by_name(device_name);
    if (!fs_monitor)
    {
        LeaveCriticalSection(&fs_section);
        return DISP_CHANGE_FAILED;
    }

    if (is_detached_mode(&fs_monitor->real_mode) && !is_detached_mode(user_mode))
    {
        FIXME("Attaching adapters is unsupported with fullscreen hack.\n");
        return DISP_CHANGE_SUCCESSFUL;
    }

    /* Real modes may be changed since initialization */
    if (!real_settings_handler.get_id(device_name, &real_id) ||
        !real_settings_handler.get_current_mode(real_id, &real_mode))
    {
        LeaveCriticalSection(&fs_section);
        return DISP_CHANGE_FAILED;
    }

    fs_monitor->user_mode = *user_mode;
    fs_monitor->real_mode = real_mode;
    lstrcpyW(fs_monitor->user_mode.dmDeviceName, device_name);

    if (is_detached_mode(user_mode))
    {
        fs_monitor->user_to_real_scale = 0;
        fs_monitor->top_left.x = 0;
        fs_monitor->top_left.y = 0;
    }
    /* Integer scaling */
    else if (fs_hack_is_integer())
    {
        scale = min(real_mode.dmPelsWidth / user_mode->dmPelsWidth, real_mode.dmPelsHeight / user_mode->dmPelsHeight);
        fs_monitor->user_to_real_scale = scale;
        fs_monitor->top_left.x = real_mode.u1.s2.dmPosition.x + (real_mode.dmPelsWidth - user_mode->dmPelsWidth * scale) / 2;
        fs_monitor->top_left.y = real_mode.u1.s2.dmPosition.y + (real_mode.dmPelsHeight - user_mode->dmPelsHeight * scale) / 2;
    }
    /* If real mode is narrower than fake mode, scale to fit width */
    else if ((double)real_mode.dmPelsWidth / (double)real_mode.dmPelsHeight
             < (double)user_mode->dmPelsWidth / (double)user_mode->dmPelsHeight)
    {
        scale = (double)real_mode.dmPelsWidth / (double)user_mode->dmPelsWidth;
        fs_monitor->user_to_real_scale = scale;
        fs_monitor->top_left.x = real_mode.u1.s2.dmPosition.x;
        fs_monitor->top_left.y = real_mode.u1.s2.dmPosition.y + (real_mode.dmPelsHeight - user_mode->dmPelsHeight * scale) / 2;
    }
    /* Else scale to fit height */
    else
    {
        scale = (double)real_mode.dmPelsHeight / (double)user_mode->dmPelsHeight;
        fs_monitor->user_to_real_scale = scale;
        fs_monitor->top_left.x = real_mode.u1.s2.dmPosition.x + (real_mode.dmPelsWidth - user_mode->dmPelsWidth * scale) / 2;
        fs_monitor->top_left.y = real_mode.u1.s2.dmPosition.y;
    }

    TRACE("real_mode x %d y %d width %d height %d\n", real_mode.u1.s2.dmPosition.x, real_mode.u1.s2.dmPosition.y,
          real_mode.dmPelsWidth, real_mode.dmPelsHeight);
    TRACE("user_mode x %d y %d width %d height %d\n", user_mode->u1.s2.dmPosition.x, user_mode->u1.s2.dmPosition.y,
          user_mode->dmPelsWidth, user_mode->dmPelsHeight);
    TRACE("user_to_real_scale %lf\n", fs_monitor->user_to_real_scale);
    TRACE("top left corner:%s\n", wine_dbgstr_point(&fs_monitor->top_left));

    LeaveCriticalSection(&fs_section);
    return DISP_CHANGE_SUCCESSFUL;
}

/* Display device handler functions */

static BOOL fs_get_monitors(ULONG_PTR adapter_id, struct gdi_monitor **new_monitors, int *count)
{
    struct gdi_monitor *monitor;
    struct fs_monitor *fs_monitor;
    RECT rect;
    INT i;

    if (!real_device_handler.get_monitors(adapter_id, new_monitors, count))
        return FALSE;

    EnterCriticalSection(&fs_section);
    for (i = 0; i < *count; ++i)
    {
        monitor = &(*new_monitors)[i];

        LIST_FOR_EACH_ENTRY(fs_monitor, &fs_monitors, struct fs_monitor, entry)
        {
            rect.left = fs_monitor->real_mode.u1.s2.dmPosition.x;
            rect.top = fs_monitor->real_mode.u1.s2.dmPosition.y;
            rect.right = rect.left + fs_monitor->real_mode.dmPelsWidth;
            rect.bottom = rect.top + fs_monitor->real_mode.dmPelsHeight;

            if (EqualRect(&rect, &monitor->rc_monitor))
            {
                monitor->rc_monitor.left = fs_monitor->user_mode.u1.s2.dmPosition.x;
                monitor->rc_monitor.top = fs_monitor->user_mode.u1.s2.dmPosition.y;
                monitor->rc_monitor.right = monitor->rc_monitor.left + fs_monitor->user_mode.dmPelsWidth;
                monitor->rc_monitor.bottom = monitor->rc_monitor.top + fs_monitor->user_mode.dmPelsHeight;
                monitor->rc_work = monitor->rc_monitor;
                monitor->state_flags = DISPLAY_DEVICE_ATTACHED;
                if (fs_monitor->user_mode.dmPelsWidth && fs_monitor->user_mode.dmPelsHeight)
                    monitor->state_flags |= DISPLAY_DEVICE_ACTIVE;
            }
        }
    }
    LeaveCriticalSection(&fs_section);
    return TRUE;
}

/* Fullscreen hack helpers */

/* Return whether fullscreen hack is enabled on a specific monitor */
BOOL fs_hack_enabled(HMONITOR monitor)
{
    struct fs_monitor *fs_monitor;
    BOOL enabled = FALSE;

    TRACE("monitor %p\n", monitor);

    EnterCriticalSection(&fs_section);
    fs_monitor = fs_find_monitor_by_handle(monitor);
    if (fs_monitor && (fs_monitor->user_mode.dmPelsWidth != fs_monitor->real_mode.dmPelsWidth ||
                       fs_monitor->user_mode.dmPelsHeight != fs_monitor->real_mode.dmPelsHeight))
        enabled = TRUE;
    LeaveCriticalSection(&fs_section);
    TRACE("enabled: %s\n", enabled ? "TRUE" : "FALSE");
    return enabled;
}

BOOL fs_hack_mapping_required(HMONITOR monitor)
{
    BOOL required;

    TRACE("monitor %p\n", monitor);

    /* steamcompmgr does our mapping for us */
    required = !wm_is_steamcompmgr(NULL) && fs_hack_enabled(monitor);
    TRACE("required: %s\n", required ? "TRUE" : "FALSE");
    return required;
}

/* Return whether integer scaling is on */
BOOL fs_hack_is_integer(void)
{
    static int is_int = -1;
    if (is_int < 0)
    {
        const char *e = getenv("WINE_FULLSCREEN_INTEGER_SCALING");
        is_int = e && strcmp(e, "0");
    }
    TRACE("is_interger_scaling: %s\n", is_int ? "TRUE" : "FALSE");
    return is_int;
}

HMONITOR fs_hack_monitor_from_rect(const RECT *in_rect)
{
    RECT rect = *in_rect;

    TRACE("rect %s\n", wine_dbgstr_rect(&rect));
    rect.right = rect.left + 1;
    rect.bottom = rect.top + 1;
    return MonitorFromRect(&rect, MONITOR_DEFAULTTOPRIMARY);
}

/* Get the monitor a window is on. MonitorFromWindow() doesn't work here because it finds the
 * monitor with the maximum overlapped rectangle when a window is spanned over two monitors, whereas
 * for the fullscreen hack, the monitor where the left top corner of the window is on is the correct
 * one. For example, a game with a window of 3840x2160 changes the primary monitor to 1280x720, if
 * there is a secondary monitor of 3840x2160 to the right, MonitorFromWindow() will return the
 * secondary monitor instead of the primary one. */
HMONITOR fs_hack_monitor_from_hwnd(HWND hwnd)
{
    RECT rect = {0};

    if (!GetWindowRect(hwnd, &rect))
        ERR("Invalid hwnd %p.\n", hwnd);

    TRACE("hwnd %p rect %s\n", hwnd, wine_dbgstr_rect(&rect));
    return fs_hack_monitor_from_rect(&rect);
}

/* Return the rectangle of a monitor in current mode in user virtual screen coordinates */
RECT fs_hack_current_mode(HMONITOR monitor)
{
    struct fs_monitor *fs_monitor;
    RECT rect = {0};

    TRACE("monitor %p\n", monitor);

    EnterCriticalSection(&fs_section);
    fs_monitor = fs_find_monitor_by_handle(monitor);
    if (!fs_monitor)
    {
        LeaveCriticalSection(&fs_section);
        return rect;
    }

    rect.left = fs_monitor->user_mode.u1.s2.dmPosition.x;
    rect.top = fs_monitor->user_mode.u1.s2.dmPosition.y;
    rect.right = rect.left + fs_monitor->user_mode.dmPelsWidth;
    rect.bottom = rect.top + fs_monitor->user_mode.dmPelsHeight;
    LeaveCriticalSection(&fs_section);
    TRACE("current mode rect: %s\n", wine_dbgstr_rect(&rect));
    return rect;
}

/* Return the rectangle of a monitor in real mode in real virtual screen coordinates */
RECT fs_hack_real_mode(HMONITOR monitor)
{
    struct fs_monitor *fs_monitor;
    RECT rect = {0};

    TRACE("monitor %p\n", monitor);

    EnterCriticalSection(&fs_section);
    fs_monitor = fs_find_monitor_by_handle(monitor);
    if (!fs_monitor)
    {
        LeaveCriticalSection(&fs_section);
        return rect;
    }

    rect.left = fs_monitor->real_mode.u1.s2.dmPosition.x;
    rect.top = fs_monitor->real_mode.u1.s2.dmPosition.y;
    rect.right = rect.left + fs_monitor->real_mode.dmPelsWidth;
    rect.bottom = rect.top + fs_monitor->real_mode.dmPelsHeight;
    LeaveCriticalSection(&fs_section);
    TRACE("real mode rect: %s\n", wine_dbgstr_rect(&rect));
    return rect;
}

/* Return whether width and height are the same as the current mode used by a monitor */
BOOL fs_hack_matches_current_mode(HMONITOR monitor, INT width, INT height)
{
    MONITORINFO monitor_info;
    BOOL matched;

    TRACE("monitor %p\n", monitor);

    monitor_info.cbSize = sizeof(monitor_info);
    if (!GetMonitorInfoW(monitor, &monitor_info))
        return FALSE;

    matched = (width == monitor_info.rcMonitor.right - monitor_info.rcMonitor.left)
              && (height == monitor_info.rcMonitor.bottom - monitor_info.rcMonitor.top);
    TRACE("matched: %s\n", matched ? "TRUE" : "FALSE");
    return matched;
}

/* Transform a point in user virtual screen coordinates to real virtual screen coordinates */
void fs_hack_point_user_to_real(POINT *pos)
{
    struct fs_monitor *fs_monitor;
    RECT rect;

    TRACE("from %d,%d\n", pos->x, pos->y);

    if (wm_is_steamcompmgr(NULL))
        return;

    EnterCriticalSection(&fs_section);
    LIST_FOR_EACH_ENTRY(fs_monitor, &fs_monitors, struct fs_monitor, entry)
    {
        rect.left = fs_monitor->user_mode.u1.s2.dmPosition.x;
        rect.top = fs_monitor->user_mode.u1.s2.dmPosition.y;
        rect.right = rect.left + fs_monitor->user_mode.dmPelsWidth;
        rect.bottom = rect.top + fs_monitor->user_mode.dmPelsHeight;

        if (PtInRect(&rect, *pos))
        {
            pos->x -= fs_monitor->user_mode.u1.s2.dmPosition.x;
            pos->y -= fs_monitor->user_mode.u1.s2.dmPosition.y;
            pos->x = lround(pos->x * fs_monitor->user_to_real_scale);
            pos->y = lround(pos->y * fs_monitor->user_to_real_scale);
            pos->x += fs_monitor->top_left.x;
            pos->y += fs_monitor->top_left.y;
            LeaveCriticalSection(&fs_section);
            TRACE("to %d,%d\n", pos->x, pos->y);
            return;
        }
    }
    LeaveCriticalSection(&fs_section);
    WARN("%d,%d not transformed.\n", pos->x, pos->y);
}

/* Transform a point in real virtual screen coordinates to user virtual screen coordinates */
void fs_hack_point_real_to_user(POINT *pos)
{
    struct fs_monitor *fs_monitor;
    RECT rect;

    TRACE("from %d,%d\n", pos->x, pos->y);

    if (wm_is_steamcompmgr(NULL))
        return;

    EnterCriticalSection(&fs_section);
    LIST_FOR_EACH_ENTRY(fs_monitor, &fs_monitors, struct fs_monitor, entry)
    {
        rect.left = fs_monitor->real_mode.u1.s2.dmPosition.x;
        rect.top = fs_monitor->real_mode.u1.s2.dmPosition.y;
        rect.right = rect.left + fs_monitor->real_mode.dmPelsWidth;
        rect.bottom = rect.top + fs_monitor->real_mode.dmPelsHeight;

        if (PtInRect(&rect, *pos))
        {
            pos->x -= fs_monitor->top_left.x;
            pos->y -= fs_monitor->top_left.y;
            pos->x = lround(pos->x / fs_monitor->user_to_real_scale);
            pos->y = lround(pos->y / fs_monitor->user_to_real_scale);
            pos->x += fs_monitor->user_mode.u1.s2.dmPosition.x;
            pos->y += fs_monitor->user_mode.u1.s2.dmPosition.y;
            pos->x = max(pos->x, fs_monitor->user_mode.u1.s2.dmPosition.x);
            pos->y = max(pos->y, fs_monitor->user_mode.u1.s2.dmPosition.y);
            pos->x = min(pos->x, fs_monitor->user_mode.u1.s2.dmPosition.x + (INT)fs_monitor->user_mode.dmPelsWidth - 1);
            pos->y = min(pos->y, fs_monitor->user_mode.u1.s2.dmPosition.y + (INT)fs_monitor->user_mode.dmPelsHeight - 1);
            LeaveCriticalSection(&fs_section);
            TRACE("to %d,%d\n", pos->x, pos->y);
            return;
        }
    }
    LeaveCriticalSection(&fs_section);
    WARN("%d,%d not transformed.\n", pos->x, pos->y);
}

/* Transform RGNDATA in user virtual screen coordinates to real virtual screen coordinates.
 * This is for clipping. Be sure to use Unsorted for Xlib calls after this transformation because
 * this may break the requirement of using YXBanded. For example, say there are two monitors aligned
 * horizontally with the primary monitor on the right. Each of monitor is of real resolution
 * 1920x1080 and the fake primary monitor resolution is 1024x768. Then (0, 10, 1024, 768) should be
 * transformed to (0, 14, 1920, 1080). While (1024, 10, 2944, 1080) should be transformed to
 * (1920, 10, 3840, 1080) and this is breaking YXBanded because it requires y in non-decreasing order */
void fs_hack_rgndata_user_to_real(RGNDATA *data)
{
    unsigned int i;
    XRectangle *xrect;
    RECT rect;

    if (!data || wm_is_steamcompmgr(NULL))
        return;

    xrect = (XRectangle *)data->Buffer;
    for (i = 0; i < data->rdh.nCount; i++)
    {
        rect.left = xrect[i].x;
        rect.top = xrect[i].y;
        rect.right = xrect[i].x + xrect[i].width;
        rect.bottom = xrect[i].y + xrect[i].height;
        TRACE("from rect %s\n", wine_dbgstr_rect(&rect));
        fs_hack_rect_user_to_real(&rect);
        TRACE("to rect %s\n", wine_dbgstr_rect(&rect));
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
void fs_hack_rect_user_to_real(RECT *rect)
{
    struct fs_monitor *fs_monitor;
    HMONITOR monitor;
    POINT point;

    TRACE("from %s\n", wine_dbgstr_rect(rect));

    if (wm_is_steamcompmgr(NULL))
        return;

    point.x = rect->left;
    point.y = rect->top;
    monitor = MonitorFromPoint(point, MONITOR_DEFAULTTONEAREST);
    EnterCriticalSection(&fs_section);
    fs_monitor = fs_find_monitor_by_handle(monitor);
    if (!fs_monitor)
    {
        LeaveCriticalSection(&fs_section);
        WARN("%s not transformed.\n", wine_dbgstr_rect(rect));
        return;
    }

    OffsetRect(rect, -fs_monitor->user_mode.u1.s2.dmPosition.x, -fs_monitor->user_mode.u1.s2.dmPosition.y);
    rect->left = lround(rect->left * fs_monitor->user_to_real_scale);
    rect->right = lround(rect->right * fs_monitor->user_to_real_scale);
    rect->top = lround(rect->top * fs_monitor->user_to_real_scale);
    rect->bottom = lround(rect->bottom * fs_monitor->user_to_real_scale);
    OffsetRect(rect, fs_monitor->top_left.x, fs_monitor->top_left.y);
    LeaveCriticalSection(&fs_section);
    TRACE("to %s\n", wine_dbgstr_rect(rect));
}

/* Get the user_to_real_scale value in a monitor */
double fs_hack_get_user_to_real_scale(HMONITOR monitor)
{
    struct fs_monitor *fs_monitor;
    double scale = 1.0;

    TRACE("monitor %p\n", monitor);

    if (wm_is_steamcompmgr(NULL))
        return scale;

    EnterCriticalSection(&fs_section);
    fs_monitor = fs_find_monitor_by_handle(monitor);
    if (!fs_monitor)
    {
        LeaveCriticalSection(&fs_section);
        return scale;
    }
    scale = fs_monitor->user_to_real_scale;

    LeaveCriticalSection(&fs_section);
    TRACE("scale %lf\n", scale);
    return scale;
}

/* Get the scaled scree size of a monitor */
SIZE fs_hack_get_scaled_screen_size(HMONITOR monitor)
{
    struct fs_monitor *fs_monitor;
    SIZE size = {0};

    TRACE("monitor %p\n", monitor);

    EnterCriticalSection(&fs_section);
    fs_monitor = fs_find_monitor_by_handle(monitor);
    if (!fs_monitor)
    {
        LeaveCriticalSection(&fs_section);
        return size;
    }

    if (wm_is_steamcompmgr(NULL))
    {
        LeaveCriticalSection(&fs_section);
        size.cx = fs_monitor->user_mode.dmPelsWidth;
        size.cy = fs_monitor->user_mode.dmPelsHeight;
        TRACE("width %d height %d\n", size.cx, size.cy);
        return size;
    }

    size.cx = lround(fs_monitor->user_mode.dmPelsWidth * fs_monitor->user_to_real_scale);
    size.cy = lround(fs_monitor->user_mode.dmPelsHeight * fs_monitor->user_to_real_scale);
    LeaveCriticalSection(&fs_section);
    TRACE("width %d height %d\n", size.cx, size.cy);
    return size;
}

/* Get the real virtual screen size instead of virtual screen size using fake modes */
RECT fs_hack_get_real_virtual_screen(void)
{
    struct fs_monitor *fs_monitor;
    RECT rect, virtual = {0};

    EnterCriticalSection(&fs_section);
    LIST_FOR_EACH_ENTRY(fs_monitor, &fs_monitors, struct fs_monitor, entry)
    {
        rect.left = fs_monitor->real_mode.u1.s2.dmPosition.x;
        rect.top = fs_monitor->real_mode.u1.s2.dmPosition.y;
        rect.right = rect.left + fs_monitor->real_mode.dmPelsWidth;
        rect.bottom = rect.top + fs_monitor->real_mode.dmPelsHeight;

        UnionRect(&virtual, &virtual, &rect);
    }
    LeaveCriticalSection(&fs_section);
    TRACE("real virtual screen rect:%s\n", wine_dbgstr_rect(&virtual));
    return virtual;
}

/* Initialize the fullscreen hack, which is a layer on top of real settings handlers and real
 * display device handlers */
void fs_hack_init(void)
{
    static WCHAR display_fmt[] = {'\\','\\','.','\\','D','I','S','P','L','A','Y','%','d',0};
    struct x11drv_display_device_handler device_handler;
    struct x11drv_settings_handler settings_handler;
    WCHAR device_name[CCHDEVICENAME];
    INT i = 0;

    real_device_handler = X11DRV_DisplayDevices_GetHandler();
    real_settings_handler = X11DRV_Settings_GetHandler();

    EnterCriticalSection(&fs_section);
    while (1)
    {
        sprintfW(device_name, display_fmt, ++i);
        if (!fs_add_monitor(device_name))
            break;
    }
    LeaveCriticalSection(&fs_section);

    settings_handler.name = "Fullscreen Hack";
    settings_handler.priority = 500;
    settings_handler.get_id = fs_get_id;
    settings_handler.get_modes = fs_get_modes;
    settings_handler.free_modes = fs_free_modes;
    settings_handler.get_current_mode = fs_get_current_mode;
    settings_handler.set_current_mode = fs_set_current_mode;
    settings_handler.convert_coordinates = NULL;
    X11DRV_Settings_SetHandler(&settings_handler);

    device_handler.name = "Fullscreen Hack";
    device_handler.priority = 500;
    device_handler.get_gpus = real_device_handler.get_gpus;
    device_handler.get_adapters = real_device_handler.get_adapters;
    device_handler.get_monitors = fs_get_monitors;
    device_handler.free_gpus = real_device_handler.free_gpus;
    device_handler.free_adapters = real_device_handler.free_adapters;
    device_handler.free_monitors = real_device_handler.free_monitors;
    device_handler.register_event_handlers = NULL;
    X11DRV_DisplayDevices_SetHandler(&device_handler);
}
