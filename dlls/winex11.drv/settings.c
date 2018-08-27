/*
 * Wine X11drv display settings functions
 *
 * Copyright 2003 Alexander James Pasadyn
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
#include <string.h>
#include <stdio.h>
#include <assert.h>

#define NONAMELESSUNION
#define NONAMELESSSTRUCT

#include "x11drv.h"

#include "windef.h"
#include "winreg.h"
#include "wingdi.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(x11settings);

static struct x11drv_mode_info *dd_modes = NULL;
static unsigned int dd_mode_count = 0;
static unsigned int dd_max_modes = 0;
/* All Windows drivers seen so far either support 32 bit depths, or 24 bit depths, but never both. So if we have
 * a 32 bit framebuffer, report 32 bit bpps, otherwise 24 bit ones.
 */
static const unsigned int depths_24[]  = {8, 16, 24};
static const unsigned int depths_32[]  = {8, 16, 32};

/* pointers to functions that actually do the hard stuff */
static int (*pGetCurrentMode)(void);
static LONG (*pSetCurrentMode)(int mode);
static const char *handler_name;

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
    pGetCurrentMode = pNewGCM;
    pSetCurrentMode = pNewSCM;
    TRACE("Resolution settings now handled by: %s\n", name);
    if (reserve_depths)
        /* leave room for other depths */
        dd_max_modes = (3+1)*(nmodes);
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
void X11DRV_Settings_AddOneMode(unsigned int width, unsigned int height, unsigned int bpp, unsigned int freq)
{
    struct x11drv_mode_info *info = &dd_modes[dd_mode_count];
    DWORD dwBpp = screen_bpp;
    if (dd_mode_count >= dd_max_modes)
    {
        ERR("Maximum modes (%d) exceeded\n", dd_max_modes);
        return;
    }
    if (bpp == 0) bpp = dwBpp;
    info->width         = width;
    info->height        = height;
    info->refresh_rate  = freq;
    info->bpp           = bpp;
    TRACE("initialized mode %d: %dx%dx%d @%d Hz (%s)\n", 
          dd_mode_count, width, height, bpp, freq, handler_name);
    dd_mode_count++;
}

/* copy all the current modes using the other color depths */
void X11DRV_Settings_AddDepthModes(void)
{
    int i, j;
    int existing_modes = dd_mode_count;
    DWORD dwBpp = screen_bpp;
    const DWORD *depths = screen_bpp == 32 ? depths_32 : depths_24;

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
}

/* return the number of modes that are initialized */
unsigned int X11DRV_Settings_GetModeCount(void)
{
    return dd_mode_count;
}

/***********************************************************************
 * Default handlers if resolution switching is not enabled
 *
 */
static int currentMode = 0;
double fs_hack_user_to_real_w = 1., fs_hack_user_to_real_h = 1.;
double fs_hack_real_to_user_w = 1., fs_hack_real_to_user_h = 1.;
static int offs_x = 0, offs_y = 0;
static int fs_width = 0, fs_height = 0;

static int X11DRV_nores_GetCurrentMode(void)
{
    return currentMode;
}

static struct fs_mode {
    int w, h;
} fs_modes[] = {
    {0, 0}, /* mode 0 is the real mode */

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

BOOL fs_hack_enabled(void)
{
    return currentMode != 0;
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
        (w == fs_modes[0].w &&
         h == fs_modes[0].h);
}

void fs_hack_scale_user_to_real(POINT *pos)
{
    if(fs_hack_enabled()){
        TRACE("from %d,%d\n", pos->x, pos->y);
        pos->x *= fs_hack_user_to_real_w;
        pos->y *= fs_hack_user_to_real_h;
        TRACE("to %d,%d\n", pos->x, pos->y);
    }
}

void fs_hack_scale_real_to_user(POINT *pos)
{
    if(fs_hack_enabled()){
        TRACE("from %d,%d\n", pos->x, pos->y);
        pos->x *= fs_hack_real_to_user_w;
        pos->y *= fs_hack_real_to_user_h;
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
    if(fs_hack_enabled()){
        TRACE("from %d,%d\n", pos->x, pos->y);
        fs_hack_scale_user_to_real(pos);
        pos->x += offs_x;
        pos->y += offs_y;
        TRACE("to %d,%d\n", pos->x, pos->y);
    }
}

void fs_hack_real_to_user(POINT *pos)
{
    if(fs_hack_enabled()){
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

        X11DRV_resize_desktop(
                DisplayWidth(gdi_display, default_visual.screen),
                DisplayHeight(gdi_display, default_visual.screen));
    }else{
        double w = dd_modes[currentMode].width;
        double h = dd_modes[currentMode].height;
        if(fs_modes[0].w / (double)fs_modes[0].h < w / h){ /* real mode is narrower than fake mode */
            /* scale to fit width */
            h = fs_modes[0].w * (h / w);
            w = fs_modes[0].w;
            offs_x = 0;
            offs_y = (fs_modes[0].h - h) / 2;
            fs_width = fs_modes[0].w;
            fs_height = (int)h;
        }else{
            /* scale to fit height */
            w = fs_modes[0].h * (w / h);
            h = fs_modes[0].h;
            offs_x = (fs_modes[0].w - w) / 2;
            offs_y = 0;
            fs_width = (int)w;
            fs_height = fs_modes[0].h;
        }
        fs_hack_user_to_real_w = w / (double)dd_modes[currentMode].width;
        fs_hack_user_to_real_h = h / (double)dd_modes[currentMode].height;
        fs_hack_real_to_user_w = dd_modes[currentMode].width / (double)w;
        fs_hack_real_to_user_h = dd_modes[currentMode].height / (double)h;

        X11DRV_resize_desktop(
                DisplayWidth(gdi_display, default_visual.screen) - (dd_modes[0].width - w),
                DisplayHeight(gdi_display, default_visual.screen) - (dd_modes[0].height - h));
    }

    return DISP_CHANGE_SUCCESSFUL;
}

POINT fs_hack_current_mode(void)
{
    POINT ret = { dd_modes[currentMode].width,
        dd_modes[currentMode].height };
    return ret;
}

POINT fs_hack_real_mode(void)
{
    POINT ret = { dd_modes[0].width,
        dd_modes[0].height };
    return ret;
}

void X11DRV_Settings_Init(void)
{
    int i;
    RECT primary = get_primary_monitor_rect();
    X11DRV_Settings_SetHandlers("NoRes", 
                                X11DRV_nores_GetCurrentMode, 
                                X11DRV_nores_SetCurrentMode, 
                                sizeof(fs_modes) / sizeof(struct fs_mode), 1);
    fs_modes[0].w = primary.right - primary.left;
    fs_modes[0].h = primary.bottom - primary.top;
    for(i = 0; i < sizeof(fs_modes) / sizeof(struct fs_mode); ++i){
        if(i > 0 &&
                ((fs_modes[i].w == fs_modes[0].w &&
                  fs_modes[i].h == fs_modes[0].h) ||
                 (fs_modes[i].w > fs_modes[0].w ||
                  fs_modes[i].h > fs_modes[0].h)))
            continue;
        X11DRV_Settings_AddOneMode( fs_modes[i].w, fs_modes[i].h, 0, 60);
    }
    X11DRV_Settings_AddDepthModes();
}

static BOOL get_display_device_reg_key(char *key, unsigned len)
{
    static const char display_device_guid_prop[] = "__wine_display_device_guid";
    static const char video_path[] = "System\\CurrentControlSet\\Control\\Video\\{";
    static const char display0[] = "}\\0000";
    ATOM guid_atom;

    assert(len >= sizeof(video_path) + sizeof(display0) + 40);

    guid_atom = HandleToULong(GetPropA(GetDesktopWindow(), display_device_guid_prop));
    if (!guid_atom) return FALSE;

    memcpy(key, video_path, sizeof(video_path));

    if (!GlobalGetAtomNameA(guid_atom, key + strlen(key), 40))
        return FALSE;

    strcat(key, display0);

    TRACE("display device key %s\n", wine_dbgstr_a(key));
    return TRUE;
}

static BOOL read_registry_settings(DEVMODEW *dm)
{
    char wine_x11_reg_key[128];
    HKEY hkey;
    DWORD type, size;
    BOOL ret = TRUE;

    dm->dmFields = 0;

    if (!get_display_device_reg_key(wine_x11_reg_key, sizeof(wine_x11_reg_key)))
        return FALSE;

    if (RegOpenKeyExA(HKEY_CURRENT_CONFIG, wine_x11_reg_key, 0, KEY_READ, &hkey))
        return FALSE;

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
    query_value("DefaultSettings.Orientation", &dm->u1.s2.dmDisplayOrientation);
    query_value("DefaultSettings.FixedOutput", &dm->u1.s2.dmDisplayFixedOutput);

#undef query_value

    RegCloseKey(hkey);
    return ret;
}

static BOOL write_registry_settings(const DEVMODEW *dm)
{
    char wine_x11_reg_key[128];
    HKEY hkey;
    BOOL ret = TRUE;

    if (!get_display_device_reg_key(wine_x11_reg_key, sizeof(wine_x11_reg_key)))
        return FALSE;

    if (RegCreateKeyExA(HKEY_CURRENT_CONFIG, wine_x11_reg_key, 0, NULL,
                        REG_OPTION_VOLATILE, KEY_WRITE, NULL, &hkey, NULL))
        return FALSE;

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
    return ret;
}

/***********************************************************************
 *		EnumDisplaySettingsEx  (X11DRV.@)
 *
 */
BOOL CDECL X11DRV_EnumDisplaySettingsEx( LPCWSTR name, DWORD n, LPDEVMODEW devmode, DWORD flags)
{
    static const WCHAR dev_name[CCHDEVICENAME] =
        { 'W','i','n','e',' ','X','1','1',' ','d','r','i','v','e','r',0 };

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
        return read_registry_settings(devmode);
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

#define _X_FIELD(prefix, bits) if ((fields) & prefix##_##bits) {p+=sprintf(p, "%s%s", first ? "" : ",", #bits); first=FALSE;}
static const char * _CDS_flags(DWORD fields)
{
    BOOL first = TRUE;
    char buf[128];
    char *p = buf;
    _X_FIELD(CDS,UPDATEREGISTRY);_X_FIELD(CDS,TEST);_X_FIELD(CDS,FULLSCREEN);
    _X_FIELD(CDS,GLOBAL);_X_FIELD(CDS,SET_PRIMARY);_X_FIELD(CDS,RESET);
    _X_FIELD(CDS,SETRECT);_X_FIELD(CDS,NORESET);
    *p = 0;
    return wine_dbg_sprintf("%s", buf);
}
static const char * _DM_fields(DWORD fields)
{
    BOOL first = TRUE;
    char buf[128];
    char *p = buf;
    _X_FIELD(DM,BITSPERPEL);_X_FIELD(DM,PELSWIDTH);_X_FIELD(DM,PELSHEIGHT);
    _X_FIELD(DM,DISPLAYFLAGS);_X_FIELD(DM,DISPLAYFREQUENCY);_X_FIELD(DM,POSITION);
    *p = 0;
    return wine_dbg_sprintf("%s", buf);
}
#undef _X_FIELD

/***********************************************************************
 *		ChangeDisplaySettingsEx  (X11DRV.@)
 *
 */
LONG CDECL X11DRV_ChangeDisplaySettingsEx( LPCWSTR devname, LPDEVMODEW devmode,
                                           HWND hwnd, DWORD flags, LPVOID lpvoid )
{
    DWORD i, dwBpp = 0;
    DEVMODEW dm;
    BOOL def_mode = TRUE;
    char bpp_buffer[16], freq_buffer[18];

    TRACE("(%s,%p,%p,0x%08x,%p)\n",debugstr_w(devname),devmode,hwnd,flags,lpvoid);
    TRACE("flags=%s\n",_CDS_flags(flags));
    if (devmode)
    {
        /* this is the minimal dmSize that XP accepts */
        if (devmode->dmSize < FIELD_OFFSET(DEVMODEW, dmFields))
            return DISP_CHANGE_FAILED;

        TRACE("DM_fields=%s\n",_DM_fields(devmode->dmFields));
        TRACE("width=%d height=%d bpp=%d freq=%d (%s)\n",
              devmode->dmPelsWidth,devmode->dmPelsHeight,
              devmode->dmBitsPerPel,devmode->dmDisplayFrequency, handler_name);

        dwBpp = devmode->dmBitsPerPel;
        if (devmode->dmFields & DM_BITSPERPEL) def_mode &= !dwBpp;
        if (devmode->dmFields & DM_PELSWIDTH)  def_mode &= !devmode->dmPelsWidth;
        if (devmode->dmFields & DM_PELSHEIGHT) def_mode &= !devmode->dmPelsHeight;
        if (devmode->dmFields & DM_DISPLAYFREQUENCY) def_mode &= !devmode->dmDisplayFrequency;
    }

    if (def_mode || !dwBpp)
    {
        if (!X11DRV_EnumDisplaySettingsEx(devname, ENUM_REGISTRY_SETTINGS, &dm, 0))
        {
            ERR("Default mode not found!\n");
            return DISP_CHANGE_BADMODE;
        }
        if (def_mode)
        {
            TRACE("Return to original display mode (%s)\n", handler_name);
            devmode = &dm;
        }
        dwBpp = dm.dmBitsPerPel;
    }

    if ((devmode->dmFields & (DM_PELSWIDTH | DM_PELSHEIGHT)) != (DM_PELSWIDTH | DM_PELSHEIGHT))
    {
        WARN("devmode doesn't specify the resolution: %04x\n", devmode->dmFields);
        return DISP_CHANGE_BADMODE;
    }

    for (i = 0; i < dd_mode_count; i++)
    {
        if (devmode->dmFields & DM_BITSPERPEL)
        {
            if (dwBpp != dd_modes[i].bpp)
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
            devmode->dmDisplayFrequency != 0)
        {
            if (devmode->dmDisplayFrequency != dd_modes[i].refresh_rate)
                continue;
        }
        /* we have a valid mode */
        TRACE("Requested display settings match mode %d (%s)\n", i, handler_name);

        if (flags & CDS_UPDATEREGISTRY)
            write_registry_settings(devmode);

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
