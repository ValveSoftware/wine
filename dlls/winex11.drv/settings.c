/*
 * Wine X11drv display settings functions
 *
 * Copyright 2003 Alexander James Pasadyn
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
#include <stdlib.h>
#include <math.h>
#include <stdlib.h>

#define NONAMELESSUNION
#define NONAMELESSSTRUCT

#include "x11drv.h"

#include "windef.h"
#include "winreg.h"
#include "wingdi.h"
#include "wine/debug.h"
#include "wine/unicode.h"

WINE_DEFAULT_DEBUG_CHANNEL(x11settings);

static struct x11drv_mode_info *dd_modes = NULL;
static unsigned int dd_mode_count = 0;
static unsigned int dd_max_modes = 0;
/* All Windows drivers seen so far either support 32 bit depths, or 24 bit depths, but never both. So if we have
 * a 32 bit framebuffer, report 32 bit bpps, otherwise 24 bit ones.
 */
static const unsigned int depths_24[]  = {8, 16, 24};
static const unsigned int depths_32[]  = {8, 16, 32};
const unsigned int *depths;

/* pointers to functions that actually do the hard stuff */
static int (*pGetCurrentMode)(void);
static LONG (*pSetCurrentMode)(int mode);
static const char *handler_name;

static const struct fs_mode {
    int w, h;
} fs_modes[] = {
    /* this table should provide a few resolution options for common display
     * ratios, so users can choose to render at lower resolution for
     * performance. */
    { 640,  480}, /*  4:3 */
    { 800,  600}, /*  4:3 */
    {1024,  768}, /*  4:3 */
    {1600, 1200}, /*  4:3 */
    { 960,  540}, /* 16:9 */
    {1280,  720}, /* 16:9 */
    {1600,  900}, /* 16:9 */
    {1920, 1080}, /* 16:9 */
    {2560, 1440}, /* 16:9 */
    {2880, 1620}, /* 16:9 */
    {3200, 1800}, /* 16:9 */
    {1440,  900}, /*  8:5 */
    {1680, 1050}, /*  8:5 */
    {1920, 1200}, /*  8:5 */
    {2560, 1600}, /*  8:5 */
    {1440,  960}, /*  3:2 */
    {1920, 1280}, /*  3:2 */
    {2560, 1080}, /* 21:9 ultra-wide */
    {1920,  800}, /* 12:5 */
    {3840, 1600}, /* 12:5 */
    {1280, 1024}, /*  5:4 */
};

/*
 * Set the handlers for resolution changing functions
 * and initialize the master list of modes
 */
struct x11drv_mode_info *X11DRV_Settings_SetHandlers(const char *name,
                                                     int (*pNewGCM)(void),
                                                     LONG (*pNewSCM)(int),
                                                     unsigned int nmodes,
                                                     int reserve_depths)
{
    handler_name = name;
    if(pNewGCM)
        pGetCurrentMode = pNewGCM;
    if(pNewSCM)
        pSetCurrentMode = pNewSCM;
    TRACE("Resolution settings now handled by: %s\n", name);
    nmodes += ARRAY_SIZE(fs_modes);
    if (reserve_depths)
        /* leave room for other depths and refresh rates */
        dd_max_modes = 2*(3+1)*(nmodes);
    else 
        dd_max_modes = nmodes;

    if (dd_modes) 
    {
        TRACE("Destroying old display modes array\n");
        HeapFree(GetProcessHeap(), 0, dd_modes);
    }
    dd_modes = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*dd_modes) * dd_max_modes);
    dd_mode_count = 0;
    TRACE("Initialized new display modes array\n");
    return dd_modes;
}

/* Add one mode to the master list */
BOOL X11DRV_Settings_AddOneMode(unsigned int width, unsigned int height, unsigned int bpp, unsigned int freq)
{
    unsigned int i;
    struct x11drv_mode_info *info = &dd_modes[dd_mode_count];
    DWORD dwBpp = screen_bpp;
    const char *appid;
    if (dd_mode_count >= dd_max_modes)
    {
        ERR("Maximum modes (%d) exceeded\n", dd_max_modes);
        return FALSE;
    }
    if (bpp == 0) bpp = dwBpp;

    if ((appid = getenv("SteamAppId")) && !strcmp(appid, "297130"))
    {
        /* Titan Souls renders incorrectly if we report modes smaller than 800x600 */
        if (height <= 600 && !(height == 600 && width == 800))
        {
            return FALSE;
        }
    }

    for(i = 0; i < dd_mode_count; ++i)
    {
        if(dd_modes[i].width == width &&
                dd_modes[i].height == height &&
                dd_modes[i].refresh_rate == freq &&
                dd_modes[i].bpp == bpp)
        {
            return FALSE;
        }
    }

    info->width         = width;
    info->height        = height;
    info->refresh_rate  = freq;
    info->bpp           = bpp;
    TRACE("initialized mode %d: %dx%dx%d @%d Hz (%s)\n", 
          dd_mode_count, width, height, bpp, freq, handler_name);
    dd_mode_count++;

    return TRUE;
}

static int sort_display_modes(const void *l, const void *r)
{
    const struct x11drv_mode_info *left = l, *right = r;

    /* largest first */
    if(left->width < right->width)
        return 1;

    if(left->width > right->width)
        return -1;

    if(left->height < right->height)
        return 1;

    if(left->height > right->height)
        return -1;

    return 0;
}

static int currentMode = -1, realMode = -1;

/* copy all the current modes using the other color depths */
void X11DRV_Settings_AddDepthModes(void)
{
    int i, j;
    int existing_modes = dd_mode_count;
    DWORD dwBpp = screen_bpp;
    const DWORD *depths = screen_bpp == 32 ? depths_32 : depths_24;
    struct fs_mode real_mode;
    unsigned int real_rate;

    real_mode.w = dd_modes[realMode].width;
    real_mode.h = dd_modes[realMode].height;
    real_rate = dd_modes[realMode].refresh_rate;

    /* Linux reports far fewer resolutions than Windows; add "missing" modes
     * that some games may expect. */
    for(i = 0; i < ARRAY_SIZE(fs_modes); ++i)
    {
        if(fs_modes[i].w <= real_mode.w &&
                fs_modes[i].h <= real_mode.h)
            X11DRV_Settings_AddOneMode(fs_modes[i].w, fs_modes[i].h, 0, dd_modes[realMode].refresh_rate);
    }

    qsort(dd_modes, dd_mode_count, sizeof(*dd_modes), sort_display_modes);

    /* synthesize 60 FPS mode if needed */
    if(real_rate != 60)
    {
        for(i = 0; i < existing_modes; ++i)
        {
            X11DRV_Settings_AddOneMode(dd_modes[i].width, dd_modes[i].height, dwBpp, 60);
        }
    }

    existing_modes = dd_mode_count;
    for (j=0; j<3; j++)
    {
        if (depths[j] != dwBpp)
        {
            for (i=0; i < existing_modes; i++)
            {
                X11DRV_Settings_AddOneMode(dd_modes[i].width, dd_modes[i].height,
                                           depths[j], dd_modes[i].refresh_rate);
            }
        }
    }

    X11DRV_Settings_SetRealMode(real_mode.w, real_mode.h);
}

/* return the number of modes that are initialized */
unsigned int X11DRV_Settings_GetModeCount(void)
{
    return dd_mode_count;
}

/* TODO: Remove the old display settings handler interface once all backends are migrated to the new interface */
static struct x11drv_settings_handler handler;

/* Cached display modes for a device, protected by modes_section */
static WCHAR cached_device_name[CCHDEVICENAME];
static DWORD cached_flags;
static DEVMODEW *cached_modes;
static UINT cached_mode_count;

static CRITICAL_SECTION modes_section;
static CRITICAL_SECTION_DEBUG modes_critsect_debug =
{
    0, 0, &modes_section,
    {&modes_critsect_debug.ProcessLocksList, &modes_critsect_debug.ProcessLocksList},
     0, 0, {(DWORD_PTR)(__FILE__ ": modes_section")}
};
static CRITICAL_SECTION modes_section = {&modes_critsect_debug, -1, 0, 0, 0, 0};

void X11DRV_Settings_SetHandler(const struct x11drv_settings_handler *new_handler)
{
    if (new_handler->priority > handler.priority)
    {
        handler = *new_handler;
        TRACE("Display settings are now handled by: %s.\n", handler.name);
    }
}

/***********************************************************************
 * Default handlers if resolution switching is not enabled
 *
 */
double fs_hack_user_to_real_w = 1., fs_hack_user_to_real_h = 1.;
double fs_hack_real_to_user_w = 1., fs_hack_real_to_user_h = 1.;
static int offs_x = 0, offs_y = 0;
static int fs_width = 0, fs_height = 0;

void X11DRV_Settings_SetRealMode(unsigned int w, unsigned int h)
{
    unsigned int i;

    currentMode = realMode = -1;

    for(i = 0; i < dd_mode_count; ++i)
    {
        if(dd_modes[i].width == w &&
                dd_modes[i].height == h)
        {
            currentMode = i;
            break;
        }
    }

    if(currentMode < 0)
    {
        FIXME("Couldn't find current mode?! Returning 0...\n");
        currentMode = 0;
    }

    realMode = currentMode;

    TRACE("Set realMode to %d\n", realMode);
}

static int X11DRV_nores_GetCurrentMode(void)
{
    return currentMode;
}

BOOL fs_hack_enabled(void)
{
    return currentMode >= 0 &&
        currentMode != realMode;
}

BOOL fs_hack_mapping_required(void)
{
    /* steamcompmgr does our mapping for us */
    return !wm_is_steamcompmgr(NULL) &&
        currentMode >= 0 &&
        currentMode != realMode;
}

BOOL fs_hack_is_integer(void)
{
    static int is_int = -1;
    if(is_int < 0)
    {
        const char *e = getenv("WINE_FULLSCREEN_INTEGER_SCALING");
        is_int = e && strcmp(e, "0");
    }
    return is_int;
}

BOOL fs_hack_matches_current_mode(int w, int h)
{
    return fs_hack_enabled() &&
        (w == dd_modes[currentMode].width &&
         h == dd_modes[currentMode].height);
}

BOOL fs_hack_matches_real_mode(int w, int h)
{
    return fs_hack_enabled() &&
        (w == dd_modes[realMode].width &&
         h == dd_modes[realMode].height);
}

BOOL fs_hack_matches_last_mode(int w, int h)
{
    return w == fs_width && h == fs_height;
}

void fs_hack_scale_user_to_real(POINT *pos)
{
    if(fs_hack_mapping_required()){
        TRACE("from %d,%d\n", pos->x, pos->y);
        pos->x = lround(pos->x * fs_hack_user_to_real_w);
        pos->y = lround(pos->y * fs_hack_user_to_real_h);
        TRACE("to %d,%d\n", pos->x, pos->y);
    }
}

void fs_hack_scale_real_to_user(POINT *pos)
{
    if(fs_hack_mapping_required()){
        TRACE("from %d,%d\n", pos->x, pos->y);
        pos->x = lround(pos->x * fs_hack_real_to_user_w);
        pos->y = lround(pos->y * fs_hack_real_to_user_h);
        TRACE("to %d,%d\n", pos->x, pos->y);
    }
}

POINT fs_hack_get_scaled_screen_size(void)
{
    POINT p = { dd_modes[currentMode].width,
        dd_modes[currentMode].height };
    fs_hack_scale_user_to_real(&p);
    return p;
}

void fs_hack_user_to_real(POINT *pos)
{
    if(fs_hack_mapping_required()){
        TRACE("from %d,%d\n", pos->x, pos->y);
        fs_hack_scale_user_to_real(pos);
        pos->x += offs_x;
        pos->y += offs_y;
        TRACE("to %d,%d\n", pos->x, pos->y);
    }
}

void fs_hack_real_to_user(POINT *pos)
{
    if(fs_hack_mapping_required()){
        TRACE("from %d,%d\n", pos->x, pos->y);

        if(pos->x <= offs_x)
            pos->x = 0;
        else
            pos->x -= offs_x;

        if(pos->y <= offs_y)
            pos->y = 0;
        else
            pos->y -= offs_y;

        if(pos->x >= fs_width)
            pos->x = fs_width - 1;
        if(pos->y >= fs_height)
            pos->y = fs_height - 1;

        fs_hack_scale_real_to_user(pos);

        TRACE("to %d,%d\n", pos->x, pos->y);
    }
}

void fs_hack_rect_user_to_real(RECT *rect)
{
    fs_hack_user_to_real((POINT *)&rect->left);
    fs_hack_user_to_real((POINT *)&rect->right);
}

/* this is for clipping */
void fs_hack_rgndata_user_to_real(RGNDATA *data)
{
    unsigned int i;
    XRectangle *xrect;

    if (data && fs_hack_mapping_required())
    {
        xrect = (XRectangle *)data->Buffer;
        for (i = 0; i < data->rdh.nCount; i++)
        {
            POINT p;
            p.x = xrect[i].x;
            p.y = xrect[i].y;
            fs_hack_user_to_real(&p);
            xrect[i].x = p.x;
            xrect[i].y = p.y;
            xrect[i].width  *= fs_hack_user_to_real_w;
            xrect[i].height *= fs_hack_user_to_real_h;
        }
    }
}

static LONG X11DRV_nores_SetCurrentMode(int mode)
{
    if (mode >= dd_mode_count)
       return DISP_CHANGE_FAILED;

    currentMode = mode;
    TRACE("set current mode to: %ux%u\n",
            dd_modes[currentMode].width,
            dd_modes[currentMode].height);
    if(currentMode == 0){
        fs_hack_user_to_real_w = 1.;
        fs_hack_user_to_real_h = 1.;
        fs_hack_real_to_user_w = 1.;
        fs_hack_real_to_user_h = 1.;
        offs_x = offs_y = 0;
        fs_width = dd_modes[currentMode].width;
        fs_height = dd_modes[currentMode].height;
    }else{
        double w = dd_modes[currentMode].width;
        double h = dd_modes[currentMode].height;

        if(fs_hack_is_integer()){
            unsigned int scaleFactor = min(dd_modes[realMode].width / w,
                                           dd_modes[realMode].height / h);

            w *= scaleFactor;
            h *= scaleFactor;

            offs_x = (dd_modes[realMode].width  - w) / 2;
            offs_y = (dd_modes[realMode].height - h) / 2;

            fs_width  = dd_modes[realMode].width;
            fs_height = dd_modes[realMode].height;
        }else if(dd_modes[realMode].width / (double)dd_modes[realMode].height < w / h){ /* real mode is narrower than fake mode */
            /* scale to fit width */
            h = dd_modes[realMode].width * (h / w);
            w = dd_modes[realMode].width;
            offs_x = 0;
            offs_y = (dd_modes[realMode].height - h) / 2;
            fs_width = dd_modes[realMode].width;
            fs_height = (int)h;
        }else{
            /* scale to fit height */
            w = dd_modes[realMode].height * (w / h);
            h = dd_modes[realMode].height;
            offs_x = (dd_modes[realMode].width - w) / 2;
            offs_y = 0;
            fs_width = (int)w;
            fs_height = dd_modes[realMode].height;
        }
        fs_hack_user_to_real_w = w / (double)dd_modes[currentMode].width;
        fs_hack_user_to_real_h = h / (double)dd_modes[currentMode].height;
        fs_hack_real_to_user_w = dd_modes[currentMode].width / (double)w;
        fs_hack_real_to_user_h = dd_modes[currentMode].height / (double)h;
    }

    X11DRV_DisplayDevices_Update( TRUE );

    return DISP_CHANGE_SUCCESSFUL;
}

void fs_hack_choose_mode(int w, int h)
{
    unsigned int i;

    for(i = 0; i < dd_mode_count; ++i)
    {
        if(dd_modes[i].width == w &&
                dd_modes[i].height == h)
        {
            X11DRV_nores_SetCurrentMode(i);
            break;
        }
    }
}

POINT fs_hack_current_mode(void)
{
    POINT ret = { dd_modes[currentMode].width,
        dd_modes[currentMode].height };
    return ret;
}

POINT fs_hack_real_mode(void)
{
    POINT ret = { dd_modes[realMode].width,
        dd_modes[realMode].height };
    return ret;
}

void X11DRV_Settings_Init(void)
{
    RECT primary = get_host_primary_monitor_rect();

    depths = screen_bpp == 32 ? depths_32 : depths_24;

    X11DRV_Settings_SetHandlers("NoRes", 
                                X11DRV_nores_GetCurrentMode, 
                                X11DRV_nores_SetCurrentMode, 
                                1, 0);
    X11DRV_Settings_AddOneMode( primary.right - primary.left, primary.bottom - primary.top, 0, 60);
}

static BOOL get_display_device_reg_key(const WCHAR *device_name, WCHAR *key, unsigned len)
{
    static const WCHAR display[] = {'\\','\\','.','\\','D','I','S','P','L','A','Y'};
    static const WCHAR video_value_fmt[] = {'\\','D','e','v','i','c','e','\\',
                                            'V','i','d','e','o','%','d',0};
    static const WCHAR video_key[] = {'H','A','R','D','W','A','R','E','\\',
                                      'D','E','V','I','C','E','M','A','P','\\',
                                      'V','I','D','E','O','\\',0};
    WCHAR value_name[MAX_PATH], buffer[MAX_PATH], *end_ptr;
    DWORD adapter_index, size;

    /* Device name has to be \\.\DISPLAY%d */
    if (strncmpiW(device_name, display, ARRAY_SIZE(display)))
        return FALSE;

    /* Parse \\.\DISPLAY* */
    adapter_index = strtolW(device_name + ARRAY_SIZE(display), &end_ptr, 10) - 1;
    if (*end_ptr)
        return FALSE;

    /* Open \Device\Video* in HKLM\HARDWARE\DEVICEMAP\VIDEO\ */
    sprintfW(value_name, video_value_fmt, adapter_index);
    size = sizeof(buffer);
    if (RegGetValueW(HKEY_LOCAL_MACHINE, video_key, value_name, RRF_RT_REG_SZ, NULL, buffer, &size))
        return FALSE;

    if (len < lstrlenW(buffer + 18) + 1)
        return FALSE;

    /* Skip \Registry\Machine\ prefix */
    lstrcpyW(key, buffer + 18);
    TRACE("display device %s registry settings key %s.\n", wine_dbgstr_w(device_name), wine_dbgstr_w(key));
    return TRUE;
}

static BOOL read_registry_settings(const WCHAR *device_name, DEVMODEW *dm)
{
    WCHAR wine_x11_reg_key[MAX_PATH];
    HANDLE mutex;
    HKEY hkey;
    DWORD type, size;
    BOOL ret = TRUE;

    dm->dmFields = 0;

    mutex = get_display_device_init_mutex();
    if (!get_display_device_reg_key(device_name, wine_x11_reg_key, ARRAY_SIZE(wine_x11_reg_key)))
    {
        release_display_device_init_mutex(mutex);
        return FALSE;
    }

    if (RegOpenKeyExW(HKEY_CURRENT_CONFIG, wine_x11_reg_key, 0, KEY_READ, &hkey))
    {
        release_display_device_init_mutex(mutex);
        return FALSE;
    }

#define query_value(name, data) \
    size = sizeof(DWORD); \
    if (RegQueryValueExA(hkey, name, 0, &type, (LPBYTE)(data), &size) || \
        type != REG_DWORD || size != sizeof(DWORD)) \
        ret = FALSE

    query_value("DefaultSettings.BitsPerPel", &dm->dmBitsPerPel);
    dm->dmFields |= DM_BITSPERPEL;
    query_value("DefaultSettings.XResolution", &dm->dmPelsWidth);
    dm->dmFields |= DM_PELSWIDTH;
    query_value("DefaultSettings.YResolution", &dm->dmPelsHeight);
    dm->dmFields |= DM_PELSHEIGHT;
    query_value("DefaultSettings.VRefresh", &dm->dmDisplayFrequency);
    dm->dmFields |= DM_DISPLAYFREQUENCY;
    query_value("DefaultSettings.Flags", &dm->u2.dmDisplayFlags);
    dm->dmFields |= DM_DISPLAYFLAGS;
    query_value("DefaultSettings.XPanning", &dm->u1.s2.dmPosition.x);
    query_value("DefaultSettings.YPanning", &dm->u1.s2.dmPosition.y);
    dm->dmFields |= DM_POSITION;
    query_value("DefaultSettings.Orientation", &dm->u1.s2.dmDisplayOrientation);
    dm->dmFields |= DM_DISPLAYORIENTATION;
    query_value("DefaultSettings.FixedOutput", &dm->u1.s2.dmDisplayFixedOutput);

#undef query_value

    RegCloseKey(hkey);
    release_display_device_init_mutex(mutex);
    return ret;
}

static BOOL write_registry_settings(const WCHAR *device_name, const DEVMODEW *dm)
{
    WCHAR wine_x11_reg_key[MAX_PATH];
    HANDLE mutex;
    HKEY hkey;
    BOOL ret = TRUE;

    mutex = get_display_device_init_mutex();
    if (!get_display_device_reg_key(device_name, wine_x11_reg_key, ARRAY_SIZE(wine_x11_reg_key)))
    {
        release_display_device_init_mutex(mutex);
        return FALSE;
    }

    if (RegCreateKeyExW(HKEY_CURRENT_CONFIG, wine_x11_reg_key, 0, NULL,
                        REG_OPTION_VOLATILE, KEY_WRITE, NULL, &hkey, NULL))
    {
        release_display_device_init_mutex(mutex);
        return FALSE;
    }

#define set_value(name, data) \
    if (RegSetValueExA(hkey, name, 0, REG_DWORD, (const BYTE*)(data), sizeof(DWORD))) \
        ret = FALSE

    set_value("DefaultSettings.BitsPerPel", &dm->dmBitsPerPel);
    set_value("DefaultSettings.XResolution", &dm->dmPelsWidth);
    set_value("DefaultSettings.YResolution", &dm->dmPelsHeight);
    set_value("DefaultSettings.VRefresh", &dm->dmDisplayFrequency);
    set_value("DefaultSettings.Flags", &dm->u2.dmDisplayFlags);
    set_value("DefaultSettings.XPanning", &dm->u1.s2.dmPosition.x);
    set_value("DefaultSettings.YPanning", &dm->u1.s2.dmPosition.y);
    set_value("DefaultSettings.Orientation", &dm->u1.s2.dmDisplayOrientation);
    set_value("DefaultSettings.FixedOutput", &dm->u1.s2.dmDisplayFixedOutput);

#undef set_value

    RegCloseKey(hkey);
    release_display_device_init_mutex(mutex);
    return ret;
}

BOOL get_primary_adapter(WCHAR *name)
{
    DISPLAY_DEVICEW dd;
    DWORD i;

    dd.cb = sizeof(dd);
    for (i = 0; EnumDisplayDevicesW(NULL, i, &dd, 0); ++i)
    {
        if (dd.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE)
        {
            lstrcpyW(name, dd.DeviceName);
            return TRUE;
        }
    }

    return FALSE;
}

/***********************************************************************
 *		EnumDisplaySettingsEx  (X11DRV.@)
 *
 */
BOOL CDECL X11DRV_EnumDisplaySettingsEx( LPCWSTR name, DWORD n, LPDEVMODEW devmode, DWORD flags)
{
    static const WCHAR dev_name[CCHDEVICENAME] =
        { 'W','i','n','e',' ','X','1','1',' ','d','r','i','v','e','r',0 };
    DEVMODEW *modes;
    UINT mode_count;
    ULONG_PTR id;

    /* Use the new interface if it is available */
    if (!handler.name)
        goto old_interface;

    if (n == ENUM_REGISTRY_SETTINGS)
    {
        if (!read_registry_settings(name, devmode))
        {
            ERR("Failed to get %s registry display settings.\n", wine_dbgstr_w(name));
            return FALSE;
        }
        goto done;
    }

    if (n == ENUM_CURRENT_SETTINGS)
    {
        if (!handler.get_id(name, &id) || !handler.get_current_mode(id, devmode))
        {
            ERR("Failed to get %s current display settings.\n", wine_dbgstr_w(name));
            return FALSE;
        }
        goto done;
    }

    EnterCriticalSection(&modes_section);
    if (n == 0 || lstrcmpiW(cached_device_name, name) || cached_flags != flags)
    {
        if (!handler.get_id(name, &id) || !handler.get_modes(id, flags, &modes, &mode_count))
        {
            ERR("Failed to get %s supported display modes.\n", wine_dbgstr_w(name));
            LeaveCriticalSection(&modes_section);
            return FALSE;
        }

        if (cached_modes)
            handler.free_modes(cached_modes);
        lstrcpyW(cached_device_name, name);
        cached_flags = flags;
        cached_modes = modes;
        cached_mode_count = mode_count;
    }

    if (n >= cached_mode_count)
    {
        LeaveCriticalSection(&modes_section);
        WARN("handler:%s device:%s mode index:%#x not found.\n", handler.name, wine_dbgstr_w(name), n);
        SetLastError(ERROR_NO_MORE_FILES);
        return FALSE;
    }

    memcpy(devmode, (BYTE *)cached_modes + (sizeof(*cached_modes) + cached_modes[0].dmDriverExtra) * n, sizeof(*devmode));
    LeaveCriticalSection(&modes_section);

done:
    /* Set generic fields */
    devmode->dmSize = FIELD_OFFSET(DEVMODEW, dmICMMethod);
    devmode->dmDriverExtra = 0;
    devmode->dmSpecVersion = DM_SPECVERSION;
    devmode->dmDriverVersion = DM_SPECVERSION;
    lstrcpyW(devmode->dmDeviceName, dev_name);
    return TRUE;

old_interface:
    devmode->dmSize = FIELD_OFFSET(DEVMODEW, dmICMMethod);
    devmode->dmSpecVersion = DM_SPECVERSION;
    devmode->dmDriverVersion = DM_SPECVERSION;
    memcpy(devmode->dmDeviceName, dev_name, sizeof(dev_name));
    devmode->dmDriverExtra = 0;
    devmode->u2.dmDisplayFlags = 0;
    devmode->dmDisplayFrequency = 0;
    devmode->u1.s2.dmPosition.x = 0;
    devmode->u1.s2.dmPosition.y = 0;
    devmode->u1.s2.dmDisplayOrientation = 0;
    devmode->u1.s2.dmDisplayFixedOutput = 0;

    if (n == ENUM_CURRENT_SETTINGS)
    {
        TRACE("mode %d (current) -- getting current mode (%s)\n", n, handler_name);
        n = pGetCurrentMode();
    }
    if (n == ENUM_REGISTRY_SETTINGS)
    {
        TRACE("mode %d (registry) -- getting default mode (%s)\n", n, handler_name);
        return read_registry_settings(name, devmode);
    }
    if (n < dd_mode_count)
    {
        devmode->dmPelsWidth = dd_modes[n].width;
        devmode->dmPelsHeight = dd_modes[n].height;
        devmode->dmBitsPerPel = dd_modes[n].bpp;
        devmode->dmDisplayFrequency = dd_modes[n].refresh_rate;
        devmode->dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL |
                            DM_DISPLAYFLAGS;
        if (devmode->dmDisplayFrequency)
        {
            devmode->dmFields |= DM_DISPLAYFREQUENCY;
            TRACE("mode %d -- %dx%dx%dbpp @%d Hz (%s)\n", n,
                  devmode->dmPelsWidth, devmode->dmPelsHeight, devmode->dmBitsPerPel,
                  devmode->dmDisplayFrequency, handler_name);
        }
        else
        {
            TRACE("mode %d -- %dx%dx%dbpp (%s)\n", n,
                  devmode->dmPelsWidth, devmode->dmPelsHeight, devmode->dmBitsPerPel, 
                  handler_name);
        }
        return TRUE;
    }
    TRACE("mode %d -- not present (%s)\n", n, handler_name);
    SetLastError(ERROR_NO_MORE_FILES);
    return FALSE;
}

BOOL is_detached_mode(const DEVMODEW *mode)
{
    return mode->dmFields & DM_POSITION &&
           mode->dmFields & DM_PELSWIDTH &&
           mode->dmFields & DM_PELSHEIGHT &&
           mode->dmPelsWidth == 0 &&
           mode->dmPelsHeight == 0;
}

/***********************************************************************
 *		ChangeDisplaySettingsEx  (X11DRV.@)
 *
 */
LONG CDECL X11DRV_ChangeDisplaySettingsEx( LPCWSTR devname, LPDEVMODEW devmode,
                                           HWND hwnd, DWORD flags, LPVOID lpvoid )
{
    WCHAR primary_adapter[CCHDEVICENAME];
    char bpp_buffer[16], freq_buffer[18];
    DEVMODEW default_mode;
    DWORD i;

    if (!get_primary_adapter(primary_adapter))
        return DISP_CHANGE_FAILED;

    if (!devname && !devmode)
    {
        default_mode.dmSize = sizeof(default_mode);
        if (!EnumDisplaySettingsExW(primary_adapter, ENUM_REGISTRY_SETTINGS, &default_mode, 0))
        {
            ERR("Default mode not found for %s!\n", wine_dbgstr_w(primary_adapter));
            return DISP_CHANGE_BADMODE;
        }

        devname = primary_adapter;
        devmode = &default_mode;
    }

    if (is_detached_mode(devmode))
    {
        FIXME("Detaching adapters is currently unsupported.\n");
        return DISP_CHANGE_SUCCESSFUL;
    }

    for (i = 0; i < dd_mode_count; i++)
    {
        if (devmode->dmFields & DM_BITSPERPEL)
        {
            if (devmode->dmBitsPerPel != dd_modes[i].bpp)
                continue;
        }
        if (devmode->dmFields & DM_PELSWIDTH)
        {
            if (devmode->dmPelsWidth != dd_modes[i].width)
                continue;
        }
        if (devmode->dmFields & DM_PELSHEIGHT)
        {
            if (devmode->dmPelsHeight != dd_modes[i].height)
                continue;
        }
        if ((devmode->dmFields & DM_DISPLAYFREQUENCY) && (dd_modes[i].refresh_rate != 0) &&
            devmode->dmDisplayFrequency != 0 && devmode->dmDisplayFrequency != 1)
        {
            if (devmode->dmDisplayFrequency != dd_modes[i].refresh_rate)
                continue;
        }
        /* we have a valid mode */
        TRACE("Requested display settings match mode %d (%s)\n", i, handler_name);

        if (flags & CDS_UPDATEREGISTRY && !write_registry_settings(devname, devmode))
        {
            ERR("Failed to write %s display settings to registry.\n", wine_dbgstr_w(devname));
            return DISP_CHANGE_NOTUPDATED;
        }

        if (lstrcmpiW(primary_adapter, devname))
        {
            FIXME("Changing non-primary adapter %s settings is currently unsupported.\n",
                  wine_dbgstr_w(devname));
            return DISP_CHANGE_SUCCESSFUL;
        }

        if (!(flags & (CDS_TEST | CDS_NORESET)))
            return pSetCurrentMode(i);

        return DISP_CHANGE_SUCCESSFUL;
    }

    /* no valid modes found, only print the fields we were trying to matching against */
    bpp_buffer[0] = freq_buffer[0] = 0;
    if (devmode->dmFields & DM_BITSPERPEL)
        sprintf(bpp_buffer, "bpp=%u ",  devmode->dmBitsPerPel);
    if ((devmode->dmFields & DM_DISPLAYFREQUENCY) && (devmode->dmDisplayFrequency != 0))
        sprintf(freq_buffer, "freq=%u ", devmode->dmDisplayFrequency);
    ERR("No matching mode found: width=%d height=%d %s%s(%s)\n",
        devmode->dmPelsWidth, devmode->dmPelsHeight, bpp_buffer, freq_buffer, handler_name);

    return DISP_CHANGE_BADMODE;
}
