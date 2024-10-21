/*
 * Window related functions
 *
 * Copyright 1993, 1994, 1995, 1996, 2001 Alexandre Julliard
 * Copyright 1993 David Metcalfe
 * Copyright 1995, 1996 Alex Korobka
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

#include <poll.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <X11/Xlib.h>
#include <X11/Xresource.h>
#include <X11/Xutil.h>
#ifdef HAVE_LIBXSHAPE
#include <X11/extensions/shape.h>
#endif /* HAVE_LIBXSHAPE */
#ifdef HAVE_X11_EXTENSIONS_XINPUT2_H
#include <X11/extensions/XInput2.h>
#endif

/* avoid conflict with field names in included win32 headers */
#undef Status

#include "ntstatus.h"
#define WIN32_NO_STATUS

#include "x11drv.h"
#include "wingdi.h"
#include "winuser.h"

#include "wine/debug.h"
#include "wine/server.h"

WINE_DEFAULT_DEBUG_CHANNEL(x11drv);
WINE_DECLARE_DEBUG_CHANNEL(systray);

#define _NET_WM_MOVERESIZE_SIZE_TOPLEFT      0
#define _NET_WM_MOVERESIZE_SIZE_TOP          1
#define _NET_WM_MOVERESIZE_SIZE_TOPRIGHT     2
#define _NET_WM_MOVERESIZE_SIZE_RIGHT        3
#define _NET_WM_MOVERESIZE_SIZE_BOTTOMRIGHT  4
#define _NET_WM_MOVERESIZE_SIZE_BOTTOM       5
#define _NET_WM_MOVERESIZE_SIZE_BOTTOMLEFT   6
#define _NET_WM_MOVERESIZE_SIZE_LEFT         7
#define _NET_WM_MOVERESIZE_MOVE              8
#define _NET_WM_MOVERESIZE_SIZE_KEYBOARD     9
#define _NET_WM_MOVERESIZE_MOVE_KEYBOARD    10

#define _NET_WM_STATE_REMOVE  0
#define _NET_WM_STATE_ADD     1
#define _NET_WM_STATE_TOGGLE  2

#define SYSTEM_TRAY_REQUEST_DOCK    0
#define SYSTEM_TRAY_BEGIN_MESSAGE   1
#define SYSTEM_TRAY_CANCEL_MESSAGE  2

static const unsigned int net_wm_state_atoms[NB_NET_WM_STATES] =
{
    XATOM__KDE_NET_WM_STATE_SKIP_SWITCHER,
    XATOM__NET_WM_STATE_FULLSCREEN,
    XATOM__NET_WM_STATE_ABOVE,
    XATOM__NET_WM_STATE_MAXIMIZED_VERT,
    XATOM__NET_WM_STATE_SKIP_PAGER,
    XATOM__NET_WM_STATE_SKIP_TASKBAR
};

#define SWP_AGG_NOPOSCHANGE (SWP_NOSIZE | SWP_NOMOVE | SWP_NOCLIENTSIZE | SWP_NOCLIENTMOVE | SWP_NOZORDER)

/* is cursor clipping active? */
BOOL clipping_cursor = FALSE;

/* X context to associate a hwnd to an X window */
XContext winContext = 0;

/* X context to associate a struct x11drv_win_data to an hwnd */
static XContext win_data_context = 0;

/* time of last user event and window where it's stored */
static Time last_user_time;
static Window user_time_window;

static const WCHAR foreign_window_prop[] =
    {'_','_','w','i','n','e','_','x','1','1','_','f','o','r','e','i','g','n','_','w','i','n','d','o','w',0};
static const WCHAR whole_window_prop[] =
    {'_','_','w','i','n','e','_','x','1','1','_','w','h','o','l','e','_','w','i','n','d','o','w',0};
static const WCHAR clip_window_prop[] =
    {'_','_','w','i','n','e','_','x','1','1','_','c','l','i','p','_','w','i','n','d','o','w',0};

static pthread_mutex_t win_data_mutex = PTHREAD_MUTEX_INITIALIZER;


static int handle_wm_name_badwindow_error( Display *dpy, XErrorEvent *event, void *arg )
{
    if (event->error_code == BadWindow)
    {
        WARN( "BadWindow error when reading WM name from window %lx, ignoring.\n", event->resourceid );
        return 1;
    }

    return 0;
}

static int detect_wm(Display *dpy)
{
    Display *display = dpy ? dpy : thread_init_display(); /* DefaultRootWindow is a macro... */
    Window root = DefaultRootWindow(display), *wm_check;
    Atom type;
    int format, err;
    unsigned long count, remaining;
    char *wm_name;
    char const *sgi = getenv("SteamGameId");

    static int cached = -1;

    if(cached < 0){

        if (XGetWindowProperty( display, root, x11drv_atom(_NET_SUPPORTING_WM_CHECK), 0,
                                 sizeof(*wm_check)/sizeof(CARD32), False, x11drv_atom(WINDOW),
                                 &type, &format, &count, &remaining, (unsigned char **)&wm_check ) == Success){
            if (type == x11drv_atom(WINDOW)){
                /* The window returned by _NET_SUPPORTING_WM_CHECK might be stale,
                   so we may get errors when asking for its properties */
                X11DRV_expect_error( display, handle_wm_name_badwindow_error, NULL );
                err = XGetWindowProperty( display, *wm_check, x11drv_atom(_NET_WM_NAME), 0,
                                           256/sizeof(CARD32), False, x11drv_atom(UTF8_STRING),
                                           &type, &format, &count, &remaining, (unsigned char **)&wm_name);

                if (X11DRV_check_error() || err != Success || type != x11drv_atom(UTF8_STRING)){
                    X11DRV_expect_error( display, handle_wm_name_badwindow_error, NULL );
                    err = XGetWindowProperty( display, *wm_check, x11drv_atom(WM_NAME), 0,
                                               256/sizeof(CARD32), False, x11drv_atom(STRING),
                                               &type, &format, &count, &remaining, (unsigned char **)&wm_name);

                    if (X11DRV_check_error() || err != Success || type != x11drv_atom(STRING))
                        wm_name = NULL;
                }

                if(wm_name){
                    TRACE("Got WM name %s\n", wm_name);

                    if((strcmp(wm_name, "GNOME Shell") == 0) ||
                            (strcmp(wm_name, "Mutter") == 0))
                        cached = WINE_WM_X11_MUTTER;
                    else if(strcmp(wm_name, "steamcompmgr") == 0)
                        cached = WINE_WM_X11_STEAMCOMPMGR;
                    else if(strcmp(wm_name, "KWin") == 0)
                        cached = WINE_WM_X11_KDE;
                    else
                        cached = WINE_WM_UNKNOWN;

                    XFree(wm_name);
                }else{
                    TRACE("WM did not set _NET_WM_NAME or WM_NAME\n");
                    cached = WINE_WM_UNKNOWN;
                }
            }else
                cached = WINE_WM_UNKNOWN;

            XFree(wm_check);
        }else
            cached = WINE_WM_UNKNOWN;

        /* Street Fighter V expects a certain sequence of window resizes
           or gets stuck on startup. The AdjustWindowRect / WM_NCCALCSIZE
           hacks confuse it completely, so let's disable them */
        if (sgi && !strcmp(sgi, "310950")) cached = WINE_WM_UNKNOWN;

        __wine_set_window_manager(cached);
    }

    return cached;
}

BOOL wm_is_mutter(Display *display)
{
    return detect_wm(display) == WINE_WM_X11_MUTTER;
}

BOOL wm_is_kde(Display *display)
{
    return detect_wm(display) == WINE_WM_X11_KDE;
}

BOOL wm_is_steamcompmgr(Display *display)
{
    return detect_wm(display) == WINE_WM_X11_STEAMCOMPMGR;
}

/***********************************************************************
 * http://standards.freedesktop.org/startup-notification-spec
 */
static void remove_startup_notification(Display *display, Window window)
{
    static LONG startup_notification_removed = 0;
    char message[1024];
    const char *id;
    int i;
    int pos;
    XEvent xevent;
    const char *src;
    int srclen;

    if (InterlockedCompareExchange(&startup_notification_removed, 1, 0) != 0)
        return;

    if (!(id = getenv( "DESKTOP_STARTUP_ID" )) || !id[0]) return;

    if ((src = strstr( id, "_TIME" ))) update_user_time( atol( src + 5 ));

    pos = snprintf(message, sizeof(message), "remove: ID=");
    message[pos++] = '"';
    for (i = 0; id[i] && pos < sizeof(message) - 3; i++)
    {
        if (id[i] == '"' || id[i] == '\\')
            message[pos++] = '\\';
        message[pos++] = id[i];
    }
    message[pos++] = '"';
    message[pos++] = '\0';
    unsetenv( "DESKTOP_STARTUP_ID" );

    xevent.xclient.type = ClientMessage;
    xevent.xclient.message_type = x11drv_atom(_NET_STARTUP_INFO_BEGIN);
    xevent.xclient.display = display;
    xevent.xclient.window = window;
    xevent.xclient.format = 8;

    src = message;
    srclen = strlen(src) + 1;

    while (srclen > 0)
    {
        int msglen = srclen;
        if (msglen > 20)
            msglen = 20;
        memset(&xevent.xclient.data.b[0], 0, 20);
        memcpy(&xevent.xclient.data.b[0], src, msglen);
        src += msglen;
        srclen -= msglen;

        XSendEvent( display, DefaultRootWindow( display ), False, PropertyChangeMask, &xevent );
        xevent.xclient.message_type = x11drv_atom(_NET_STARTUP_INFO);
    }
}


static BOOL is_managed( HWND hwnd )
{
    struct x11drv_win_data *data = get_win_data( hwnd );
    BOOL ret = data && data->managed;
    release_win_data( data );
    return ret;
}

HWND *build_hwnd_list(void)
{
    NTSTATUS status;
    HWND *list;
    ULONG count = 128;

    for (;;)
    {
        if (!(list = malloc( count * sizeof(*list) ))) return NULL;
        status = NtUserBuildHwndList( 0, 0, 0, 0, 0, count, list, &count );
        if (!status) return list;
        free( list );
        if (status != STATUS_BUFFER_TOO_SMALL) return NULL;
    }
}

static BOOL has_owned_popups( HWND hwnd )
{
    HWND *list;
    UINT i;
    BOOL ret = FALSE;

    if (!(list = build_hwnd_list())) return FALSE;

    for (i = 0; list[i] != HWND_BOTTOM; i++)
    {
        if (list[i] == hwnd) break;  /* popups are always above owner */
        if (NtUserGetWindowRelative( list[i], GW_OWNER ) != hwnd) continue;
        if ((ret = is_managed( list[i] ))) break;
    }

    free( list );
    return ret;
}


/***********************************************************************
 *              alloc_win_data
 */
static struct x11drv_win_data *alloc_win_data( Display *display, HWND hwnd )
{
    struct x11drv_win_data *data;

    if ((data = calloc( 1, sizeof(*data) )))
    {
        data->display = display;
        data->vis = default_visual;
        data->hwnd = hwnd;
        pthread_mutex_lock( &win_data_mutex );
        XSaveContext( gdi_display, (XID)hwnd, win_data_context, (char *)data );
    }
    return data;
}


/***********************************************************************
 *		is_window_managed
 *
 * Check if a given window should be managed
 */
static BOOL is_window_managed( HWND hwnd, UINT swp_flags, const RECT *window_rect )
{
    DWORD style, ex_style;

    if (!managed_mode) return FALSE;

    /* child windows are not managed */
    style = NtUserGetWindowLongW( hwnd, GWL_STYLE );
    if ((style & (WS_CHILD|WS_POPUP)) == WS_CHILD) return FALSE;
    /* activated windows are managed */
    if (!(swp_flags & (SWP_NOACTIVATE|SWP_HIDEWINDOW))) return TRUE;
    if (hwnd == get_active_window()) return TRUE;
    /* windows with caption are managed */
    if ((style & WS_CAPTION) == WS_CAPTION) return TRUE;
    /* windows with thick frame are managed */
    if (style & WS_THICKFRAME) return TRUE;
    if (style & WS_POPUP)
    {
        HMONITOR hmon;
        MONITORINFO mi;

        /* popup with sysmenu == caption are managed */
        if (style & WS_SYSMENU) return TRUE;
        /* full-screen popup windows are managed */
        hmon = NtUserMonitorFromWindow( hwnd, MONITOR_DEFAULTTOPRIMARY );
        mi.cbSize = sizeof( mi );
        NtUserGetMonitorInfo( hmon, &mi );
        if (window_rect->left <= mi.rcWork.left && window_rect->right >= mi.rcWork.right &&
            window_rect->top <= mi.rcWork.top && window_rect->bottom >= mi.rcWork.bottom)
            return TRUE;
    }
    /* application windows are managed */
    ex_style = NtUserGetWindowLongW( hwnd, GWL_EXSTYLE );
    if (ex_style & WS_EX_APPWINDOW) return TRUE;
    /* windows that own popups are managed */
    if (has_owned_popups( hwnd )) return TRUE;
    /* default: not managed */
    return FALSE;
}


/***********************************************************************
 *		is_window_resizable
 *
 * Check if window should be made resizable by the window manager
 */
static inline BOOL is_window_resizable( struct x11drv_win_data *data, DWORD style )
{
    if (style & WS_THICKFRAME) return TRUE;
    /* Metacity needs the window to be resizable to make it fullscreen */
    return NtUserIsWindowRectFullScreen( &data->whole_rect );
}

BOOL is_window_rect_full_virtual_screen( const RECT *rect )
{
    RECT virtual_rect = NtUserGetVirtualScreenRect();
    return (rect->left <= virtual_rect.left && rect->right >= virtual_rect.right &&
            rect->top <= virtual_rect.top && rect->bottom >= virtual_rect.bottom);
}

/***********************************************************************
 *              get_mwm_decorations
 */
static unsigned long get_mwm_decorations( struct x11drv_win_data *data,
                                          DWORD style, DWORD ex_style,
                                          const RECT *window_rect,
                                          const RECT *client_rect )
{
    unsigned long ret = 0;

    if (!decorated_mode) return 0;

    if (EqualRect( window_rect, client_rect )) return 0;
    if (IsRectEmpty( window_rect )) return 0;
    if (data->shaped) return 0;

    if (ex_style & WS_EX_TOOLWINDOW) return 0;
    if (ex_style & WS_EX_LAYERED) return 0;

    if ((style & WS_CAPTION) == WS_CAPTION)
    {
        ret |= MWM_DECOR_TITLE | MWM_DECOR_BORDER;
        if (style & WS_SYSMENU) ret |= MWM_DECOR_MENU;
        if (style & WS_MINIMIZEBOX) ret |= MWM_DECOR_MINIMIZE;
        if (style & WS_MAXIMIZEBOX) ret |= MWM_DECOR_MAXIMIZE;
    }
    if (ex_style & WS_EX_DLGMODALFRAME) ret |= MWM_DECOR_BORDER;
    else if (style & WS_THICKFRAME){
        if((style & WS_CAPTION) == WS_CAPTION)
             ret |= MWM_DECOR_BORDER | MWM_DECOR_RESIZEH;
    }
    else if ((style & (WS_DLGFRAME|WS_BORDER)) == WS_DLGFRAME) ret |= MWM_DECOR_BORDER;
    return ret;
}


/***********************************************************************
 *              get_window_attributes
 *
 * Fill the window attributes structure for an X window.
 */
static int get_window_attributes( struct x11drv_win_data *data, XSetWindowAttributes *attr )
{
    attr->override_redirect = !data->managed;
    attr->colormap          = data->whole_colormap ? data->whole_colormap : default_colormap;
    attr->save_under        = ((NtUserGetClassLongW( data->hwnd, GCL_STYLE ) & CS_SAVEBITS) != 0);
    attr->bit_gravity       = NorthWestGravity;
    attr->backing_store     = NotUseful;
    attr->border_pixel      = 0;
    attr->event_mask        = (ExposureMask | PointerMotionMask |
                               ButtonPressMask | ButtonReleaseMask | EnterWindowMask |
                               KeyPressMask | KeyReleaseMask | FocusChangeMask |
                               KeymapStateMask | StructureNotifyMask);
    if (data->managed) attr->event_mask |= PropertyChangeMask;

    return (CWOverrideRedirect | CWSaveUnder | CWColormap | CWBorderPixel |
            CWEventMask | CWBitGravity | CWBackingStore);
}


/***********************************************************************
 *              sync_window_style
 *
 * Change the X window attributes when the window style has changed.
 */
static void sync_window_style( struct x11drv_win_data *data )
{
    if (data->whole_window != root_window)
    {
        XSetWindowAttributes attr;
        int mask = get_window_attributes( data, &attr );

        XChangeWindowAttributes( data->display, data->whole_window, mask, &attr );
        X11DRV_XInput2_Enable( data->display, data->whole_window, attr.event_mask );
    }
}


/***********************************************************************
 *              sync_window_region
 *
 * Update the X11 window region.
 */
static void sync_window_region( struct x11drv_win_data *data, HRGN win_region )
{
#ifdef HAVE_LIBXSHAPE
    HRGN hrgn = win_region;

    if (!data->whole_window) return;

    if (data->fs_hack)
    {
        ERR( "shaped windows with fs hack not supported, things may go badly\n" );
    }

    data->shaped = FALSE;

    if (IsRectEmpty( &data->window_rect ))  /* set an empty shape */
    {
        static XRectangle empty_rect;
        XShapeCombineRectangles( data->display, data->whole_window, ShapeBounding, 0, 0,
                                 &empty_rect, 1, ShapeSet, YXBanded );
        return;
    }

    if (hrgn == (HRGN)1)  /* hack: win_region == 1 means retrieve region from server */
    {
        if (!(hrgn = NtGdiCreateRectRgn( 0, 0, 0, 0 ))) return;
        if (NtUserGetWindowRgnEx( data->hwnd, hrgn, 0 ) == ERROR)
        {
            NtGdiDeleteObjectApp( hrgn );
            hrgn = 0;
        }
    }

    if (!hrgn)
    {
        XShapeCombineMask( data->display, data->whole_window, ShapeBounding, 0, 0, None, ShapeSet );
    }
    else
    {
        RGNDATA *pRegionData;

        if (NtUserGetWindowLongW( data->hwnd, GWL_EXSTYLE ) & WS_EX_LAYOUTRTL)
            NtUserMirrorRgn( data->hwnd, hrgn );
        if ((pRegionData = X11DRV_GetRegionData( hrgn, 0 )))
        {
            XShapeCombineRectangles( data->display, data->whole_window, ShapeBounding,
                                     data->window_rect.left - data->whole_rect.left,
                                     data->window_rect.top - data->whole_rect.top,
                                     (XRectangle *)pRegionData->Buffer,
                                     pRegionData->rdh.nCount, ShapeSet, YXBanded );
            free( pRegionData );
            data->shaped = TRUE;
        }
    }
    if (hrgn && hrgn != win_region) NtGdiDeleteObjectApp( hrgn );
#endif  /* HAVE_LIBXSHAPE */
}


/***********************************************************************
 *              sync_window_opacity
 */
static void sync_window_opacity( Display *display, Window win,
                                 COLORREF key, BYTE alpha, DWORD flags )
{
    unsigned long opacity = 0xffffffff;

    if (flags & LWA_ALPHA) opacity = (0xffffffff / 0xff) * alpha;

    if (opacity == 0xffffffff)
        XDeleteProperty( display, win, x11drv_atom(_NET_WM_WINDOW_OPACITY) );
    else
        XChangeProperty( display, win, x11drv_atom(_NET_WM_WINDOW_OPACITY),
                         XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&opacity, 1 );
}


/***********************************************************************
 *              sync_window_text
 */
static void sync_window_text( Display *display, Window win, const WCHAR *text )
{
    DWORD count, len;
    char *buffer, *utf8_buffer;
    XTextProperty prop;

    /* allocate new buffer for window text */
    len = lstrlenW( text );
    count = len * 3 + 1;
    if (!(buffer = malloc( count ))) return;
    ntdll_wcstoumbs( text, len + 1, buffer, count, FALSE );

    RtlUnicodeToUTF8N( NULL, 0, &count, text, len * sizeof(WCHAR) );
    if (!(utf8_buffer = malloc( count )))
    {
        free( buffer );
        return;
    }
    RtlUnicodeToUTF8N( utf8_buffer, count, &count, text, len * sizeof(WCHAR) );

    if (XmbTextListToTextProperty( display, &buffer, 1, XStdICCTextStyle, &prop ) == Success)
    {
        XSetWMName( display, win, &prop );
        XSetWMIconName( display, win, &prop );
        XFree( prop.value );
    }
    /*
      Implements a NET_WM UTF-8 title. It should be without a trailing \0,
      according to the standard
      ( http://www.pps.jussieu.fr/~jch/software/UTF8_STRING/UTF8_STRING.text ).
    */
    XChangeProperty( display, win, x11drv_atom(_NET_WM_NAME), x11drv_atom(UTF8_STRING),
                     8, PropModeReplace, (unsigned char *) utf8_buffer, count);

    free( utf8_buffer );
    free( buffer );
}


/***********************************************************************
 *              get_bitmap_argb
 *
 * Return the bitmap bits in ARGB format. Helper for setting icon hints.
 */
static unsigned long *get_bitmap_argb( HDC hdc, HBITMAP color, HBITMAP mask, unsigned int *size )
{
    char buffer[FIELD_OFFSET( BITMAPINFO, bmiColors[256] )];
    BITMAPINFO *info = (BITMAPINFO *)buffer;
    BITMAP bm;
    unsigned int *ptr, *bits = NULL;
    unsigned char *mask_bits = NULL;
    int i, j;
    BOOL has_alpha = FALSE;

    if (!NtGdiExtGetObjectW( color, sizeof(bm), &bm )) return NULL;
    info->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info->bmiHeader.biWidth = bm.bmWidth;
    info->bmiHeader.biHeight = -bm.bmHeight;
    info->bmiHeader.biPlanes = 1;
    info->bmiHeader.biBitCount = 32;
    info->bmiHeader.biCompression = BI_RGB;
    info->bmiHeader.biSizeImage = bm.bmWidth * bm.bmHeight * 4;
    info->bmiHeader.biXPelsPerMeter = 0;
    info->bmiHeader.biYPelsPerMeter = 0;
    info->bmiHeader.biClrUsed = 0;
    info->bmiHeader.biClrImportant = 0;
    *size = bm.bmWidth * bm.bmHeight + 2;
    if (!(bits = malloc( *size * sizeof(long) ))) goto failed;
    if (!NtGdiGetDIBitsInternal( hdc, color, 0, bm.bmHeight, bits + 2, info, DIB_RGB_COLORS, 0, 0 ))
        goto failed;

    bits[0] = bm.bmWidth;
    bits[1] = bm.bmHeight;

    for (i = 0; i < bm.bmWidth * bm.bmHeight; i++)
        if ((has_alpha = (bits[i + 2] & 0xff000000) != 0)) break;

    if (!has_alpha)
    {
        unsigned int width_bytes = (bm.bmWidth + 31) / 32 * 4;
        /* generate alpha channel from the mask */
        info->bmiHeader.biBitCount = 1;
        info->bmiHeader.biSizeImage = width_bytes * bm.bmHeight;
        if (!(mask_bits = malloc( info->bmiHeader.biSizeImage ))) goto failed;
        if (!NtGdiGetDIBitsInternal( hdc, mask, 0, bm.bmHeight, mask_bits, info, DIB_RGB_COLORS, 0, 0 ))
            goto failed;
        ptr = bits + 2;
        for (i = 0; i < bm.bmHeight; i++)
            for (j = 0; j < bm.bmWidth; j++, ptr++)
                if (!((mask_bits[i * width_bytes + j / 8] << (j % 8)) & 0x80)) *ptr |= 0xff000000;
        free( mask_bits );
    }

    /* convert to array of longs */
    if (bits && sizeof(long) > sizeof(int))
        for (i = *size - 1; i >= 0; i--) ((unsigned long *)bits)[i] = bits[i];

    return (unsigned long *)bits;

failed:
    free( bits );
    free( mask_bits );
    return NULL;
}


/***********************************************************************
 *              create_icon_pixmaps
 */
static BOOL create_icon_pixmaps( HDC hdc, const ICONINFO *icon, Pixmap *icon_ret, Pixmap *mask_ret )
{
    char buffer[FIELD_OFFSET( BITMAPINFO, bmiColors[256] )];
    BITMAPINFO *info = (BITMAPINFO *)buffer;
    XVisualInfo vis = default_visual;
    struct gdi_image_bits bits;
    Pixmap color_pixmap = 0, mask_pixmap = 0;
    int lines;
    unsigned int i;

    bits.ptr = NULL;
    bits.free = NULL;
    bits.is_copy = TRUE;

    info->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info->bmiHeader.biBitCount = 0;
    if (!(lines = NtGdiGetDIBitsInternal( hdc, icon->hbmColor, 0, 0, NULL, info, DIB_RGB_COLORS, 0, 0 )))
        goto failed;
    if (!(bits.ptr = malloc( info->bmiHeader.biSizeImage ))) goto failed;
    if (!NtGdiGetDIBitsInternal( hdc, icon->hbmColor, 0, lines, bits.ptr, info, DIB_RGB_COLORS, 0, 0 ))
        goto failed;

    color_pixmap = create_pixmap_from_image( hdc, &vis, info, &bits, DIB_RGB_COLORS );
    free( bits.ptr );
    bits.ptr = NULL;
    if (!color_pixmap) goto failed;

    info->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info->bmiHeader.biBitCount = 0;
    if (!(lines = NtGdiGetDIBitsInternal( hdc, icon->hbmMask, 0, 0, NULL, info, DIB_RGB_COLORS, 0, 0 )))
        goto failed;
    if (!(bits.ptr = malloc( info->bmiHeader.biSizeImage ))) goto failed;
    if (!NtGdiGetDIBitsInternal( hdc, icon->hbmMask, 0, lines, bits.ptr, info, DIB_RGB_COLORS, 0, 0 ))
        goto failed;

    /* invert the mask */
    for (i = 0; i < info->bmiHeader.biSizeImage / sizeof(DWORD); i++) ((DWORD *)bits.ptr)[i] ^= ~0u;

    vis.depth = 1;
    mask_pixmap = create_pixmap_from_image( hdc, &vis, info, &bits, DIB_RGB_COLORS );
    free( bits.ptr );
    bits.ptr = NULL;
    if (!mask_pixmap) goto failed;

    *icon_ret = color_pixmap;
    *mask_ret = mask_pixmap;
    return TRUE;

failed:
    if (color_pixmap) XFreePixmap( gdi_display, color_pixmap );
    free( bits.ptr );
    return FALSE;
}


static HICON get_icon_info( HICON icon, ICONINFO *ii )
{
    return icon && NtUserGetIconInfo( icon, ii, NULL, NULL, NULL, 0 ) ? icon : NULL;
}

/***********************************************************************
 *              fetch_icon_data
 */
static void fetch_icon_data( HWND hwnd, HICON icon_big, HICON icon_small )
{
    struct x11drv_win_data *data;
    ICONINFO ii, ii_small;
    HDC hDC;
    unsigned int size;
    unsigned long *bits;
    Pixmap icon_pixmap, mask_pixmap;

    icon_big = get_icon_info( icon_big, &ii );
    if (!icon_big)
    {
        icon_big = get_icon_info( (HICON)send_message( hwnd, WM_GETICON, ICON_BIG, 0 ), &ii );
        if (!icon_big)
            icon_big = get_icon_info( (HICON)NtUserGetClassLongPtrW( hwnd, GCLP_HICON ), &ii );
        if (!icon_big)
        {
            icon_big = LoadImageW( 0, (const WCHAR *)IDI_WINLOGO, IMAGE_ICON, 0, 0,
                                   LR_SHARED | LR_DEFAULTSIZE );
            icon_big = get_icon_info( icon_big, &ii );
        }
    }

    icon_small = get_icon_info( icon_small, &ii_small );
    if (!icon_small)
    {
        icon_small = get_icon_info( (HICON)send_message( hwnd, WM_GETICON, ICON_SMALL, 0 ), &ii_small );
        if (!icon_small)
            icon_small = get_icon_info( (HICON)NtUserGetClassLongPtrW( hwnd, GCLP_HICONSM ), &ii_small );
    }

    if (!icon_big) return;

    hDC = NtGdiCreateCompatibleDC(0);
    bits = get_bitmap_argb( hDC, ii.hbmColor, ii.hbmMask, &size );
    if (bits && icon_small)
    {
        unsigned int size_small;
        unsigned long *bits_small, *new;

        if ((bits_small = get_bitmap_argb( hDC, ii_small.hbmColor, ii_small.hbmMask, &size_small )) &&
            (bits_small[0] != bits[0] || bits_small[1] != bits[1]))  /* size must be different */
        {
            if ((new = realloc( bits, (size + size_small) * sizeof(unsigned long) )))
            {
                bits = new;
                memcpy( bits + size, bits_small, size_small * sizeof(unsigned long) );
                size += size_small;
            }
        }
        free( bits_small );
        NtGdiDeleteObjectApp( ii_small.hbmColor );
        NtGdiDeleteObjectApp( ii_small.hbmMask );
    }

    if (!create_icon_pixmaps( hDC, &ii, &icon_pixmap, &mask_pixmap )) icon_pixmap = mask_pixmap = 0;

    NtGdiDeleteObjectApp( ii.hbmColor );
    NtGdiDeleteObjectApp( ii.hbmMask );
    NtGdiDeleteObjectApp( hDC );

    if ((data = get_win_data( hwnd )))
    {
        if (data->icon_pixmap) XFreePixmap( gdi_display, data->icon_pixmap );
        if (data->icon_mask) XFreePixmap( gdi_display, data->icon_mask );
        free( data->icon_bits );
        data->icon_pixmap = icon_pixmap;
        data->icon_mask = mask_pixmap;
        data->icon_bits = bits;
        data->icon_size = size;
        release_win_data( data );
    }
    else
    {
        if (icon_pixmap) XFreePixmap( gdi_display, icon_pixmap );
        if (mask_pixmap) XFreePixmap( gdi_display, mask_pixmap );
        free( bits );
    }
}


/***********************************************************************
 *              set_size_hints
 *
 * set the window size hints
 */
static void set_size_hints( struct x11drv_win_data *data, DWORD style )
{
    XSizeHints* size_hints;

    if (!(size_hints = XAllocSizeHints())) return;

    size_hints->win_gravity = StaticGravity;
    size_hints->flags |= PWinGravity;

    /* don't update size hints if window is not in normal state */
    if (!(style & (WS_MINIMIZE | WS_MAXIMIZE)))
    {
        if (data->hwnd != NtUserGetDesktopWindow())  /* don't force position of desktop */
        {
            POINT pt = virtual_screen_to_root( data->whole_rect.left, data->whole_rect.top );
            size_hints->x = pt.x;
            size_hints->y = pt.y;
            size_hints->flags |= PPosition;
        }
        else size_hints->win_gravity = NorthWestGravity;

        if (!is_window_resizable( data, style ))
        {
            size_hints->max_width = data->whole_rect.right - data->whole_rect.left;
            size_hints->max_height = data->whole_rect.bottom - data->whole_rect.top;
            if (size_hints->max_width <= 0 ||size_hints->max_height <= 0)
                size_hints->max_width = size_hints->max_height = 1;
            size_hints->min_width = size_hints->max_width;
            size_hints->min_height = size_hints->max_height;
            size_hints->flags |= PMinSize | PMaxSize;
        }
    }
    XSetWMNormalHints( data->display, data->whole_window, size_hints );
    XFree( size_hints );
}

struct is_unmap_notify_param
{
    struct x11drv_win_data *data;
    BOOL found;
};

static Bool is_unmap_notify( Display *display, XEvent *event, XPointer arg )
{
    struct is_unmap_notify_param *p = (struct is_unmap_notify_param *)arg;

    if (!p->found)
        p->found = event->type == UnmapNotify &&
                   event->xany.serial >= p->data->unmapnotify_serial &&
                   event->xany.window == p->data->whole_window;
    return False;
}

/***********************************************************************
 *              set_mwm_hints
 */
static void set_mwm_hints( struct x11drv_win_data *data, UINT style, UINT ex_style )
{
    GUITHREADINFO info = {.cbSize = sizeof(GUITHREADINFO)};
    MwmHints mwm_hints;
    int enable_mutter_workaround, mapped;

    if (data->hwnd == NtUserGetDesktopWindow())
    {
        mwm_hints.functions        = MWM_FUNC_MOVE | MWM_FUNC_MINIMIZE | MWM_FUNC_CLOSE;
        if (is_desktop_fullscreen())
        {
            mwm_hints.decorations = 0;
            mwm_hints.functions |= MWM_FUNC_RESIZE;  /* some WMs need this to make it fullscreen */
        }
        else mwm_hints.decorations = MWM_DECOR_TITLE | MWM_DECOR_BORDER | MWM_DECOR_MENU | MWM_DECOR_MINIMIZE;
    }
    else
    {
        mwm_hints.decorations = get_mwm_decorations( data, style, ex_style, &data->window_rect, &data->client_rect );
        mwm_hints.functions = MWM_FUNC_MOVE;
        if (is_window_resizable( data, style )) mwm_hints.functions |= MWM_FUNC_RESIZE;
        if (!(style & WS_DISABLED))
        {
            mwm_hints.functions |= MWM_FUNC_CLOSE;
            if (style & WS_MINIMIZEBOX) mwm_hints.functions |= MWM_FUNC_MINIMIZE;
            if (style & WS_MAXIMIZEBOX) mwm_hints.functions |= MWM_FUNC_MAXIMIZE;

            /* The window can be programmatically minimized even without
               a minimize box button. Allow the WM to restore it. */
            if (style & WS_MINIMIZE)    mwm_hints.functions |= MWM_FUNC_MINIMIZE | MWM_FUNC_MAXIMIZE;
        }
    }

    TRACE( "%p setting mwm hints to %lx,%lx (style %x exstyle %x)\n",
           data->hwnd, mwm_hints.decorations, mwm_hints.functions, style, ex_style );

    enable_mutter_workaround = wm_is_mutter(data->display) && NtUserGetGUIThreadInfo( GetCurrentThreadId(), &info ) &&
                               info.hwndFocus == data->hwnd && !!data->prev_hints.decorations != !!mwm_hints.decorations &&
                               root_window == DefaultRootWindow(data->display);

    /* workaround for mutter gitlab bug #649, we cannot trust the
     * data->mapped flag as mapping is asynchronous.
     */
    if (enable_mutter_workaround)
    {
        XWindowAttributes attr;

        mapped = data->mapped;
        if (XGetWindowAttributes( data->display, data->whole_window, &attr ))
            mapped = (attr.map_state != IsUnmapped);
    }

    mwm_hints.flags = MWM_HINTS_FUNCTIONS | MWM_HINTS_DECORATIONS;
    mwm_hints.input_mode = 0;
    mwm_hints.status = 0;
    data->unmapnotify_serial = NextRequest( data->display );
    XChangeProperty( data->display, data->whole_window, x11drv_atom(_MOTIF_WM_HINTS),
                     x11drv_atom(_MOTIF_WM_HINTS), 32, PropModeReplace,
                     (unsigned char*)&mwm_hints, sizeof(mwm_hints)/sizeof(long) );

    if (enable_mutter_workaround && mapped)
    {
        DWORD end = NtGetTickCount() + 100;
        struct is_unmap_notify_param p;
        struct pollfd pfd;
        XEvent event;
        int timeout;

        /* workaround for mutter gitlab bug #649, wait for the map notify
         * event each time the decorations are modified before modifying
         * them again.
         */
        p.data = data;
        p.found = FALSE;
        TRACE("workaround mutter bug #649, waiting for UnmapNotify\n");
        pfd.fd = ConnectionNumber(data->display);
        pfd.events = POLLIN;
        for (;;)
        {
            XCheckIfEvent( data->display, &event, is_unmap_notify, (XPointer)&p );
            if (p.found) break;
            timeout = end - NtGetTickCount();
            if (timeout <= 0 || poll( &pfd, 1, timeout ) != 1)
            {
                WARN( "window %p/%lx unmap_notify wait timed out.\n", data->hwnd, data->whole_window );
                break;
            }
        }
    }

    if (wm_is_mutter(data->display) && NtUserGetGUIThreadInfo( GetCurrentThreadId(), &info ) &&
        info.hwndFocus == data->hwnd && !!data->prev_hints.decorations != !!mwm_hints.decorations)
    {
        LARGE_INTEGER frequency, counter;
        /* workaround for mutter gitlab bug #273 */
        TRACE("workaround mutter bug, setting take_focus_back\n");
        NtQueryPerformanceCounter( &counter, &frequency );
        data->take_focus_back = 1000 * counter.QuadPart / frequency.QuadPart;
    }

    data->prev_hints = mwm_hints;
}


/***********************************************************************
 *              set_style_hints
 */
static void set_style_hints( struct x11drv_win_data *data, DWORD style, DWORD ex_style )
{
    Window group_leader = data->whole_window;
    HWND owner = NtUserGetWindowRelative( data->hwnd, GW_OWNER );
    Window owner_win = 0;
    XWMHints *wm_hints;
    Atom window_type;

    if (owner)
    {
        owner = NtUserGetAncestor( owner, GA_ROOT );
        owner_win = X11DRV_get_whole_window( owner );
    }

    if (owner_win)
    {
        XSetTransientForHint( data->display, data->whole_window, owner_win );
        group_leader = owner_win;
    }

    /* Only use dialog type for owned popups. Metacity allows making fullscreen
     * only normal windows, and doesn't handle correctly TRANSIENT_FOR hint for
     * dialogs owned by fullscreen windows.
     */
    if (((style & WS_POPUP) || (ex_style & WS_EX_DLGMODALFRAME)) && owner)
        window_type = x11drv_atom(_NET_WM_WINDOW_TYPE_DIALOG);
    else
        window_type = x11drv_atom(_NET_WM_WINDOW_TYPE_NORMAL);

    XChangeProperty(data->display, data->whole_window, x11drv_atom(_NET_WM_WINDOW_TYPE),
		    XA_ATOM, 32, PropModeReplace, (unsigned char*)&window_type, 1);

    if ((wm_hints = XAllocWMHints()))
    {
        wm_hints->flags = InputHint | StateHint | WindowGroupHint;
        wm_hints->input = !use_take_focus && !(style & WS_DISABLED);
        wm_hints->initial_state = (style & WS_MINIMIZE) ? IconicState : NormalState;
        wm_hints->window_group = group_leader;
        if (data->icon_pixmap)
        {
            wm_hints->icon_pixmap = data->icon_pixmap;
            wm_hints->icon_mask = data->icon_mask;
            wm_hints->flags |= IconPixmapHint | IconMaskHint;
        }
        XSetWMHints( data->display, data->whole_window, wm_hints );
        XFree( wm_hints );
    }

    if (data->icon_bits)
        XChangeProperty( data->display, data->whole_window, x11drv_atom(_NET_WM_ICON),
                         XA_CARDINAL, 32, PropModeReplace,
                         (unsigned char *)data->icon_bits, data->icon_size );
    else
        XDeleteProperty( data->display, data->whole_window, x11drv_atom(_NET_WM_ICON) );

}


/***********************************************************************
 *              set_initial_wm_hints
 *
 * Set the window manager hints that don't change over the lifetime of a window.
 */
static void set_initial_wm_hints( Display *display, Window window )
{
    long i;
    Atom protocols[3];
    Atom dndVersion = WINE_XDND_VERSION;
    XClassHint *class_hints;

    /* wm protocols */
    i = 0;
    protocols[i++] = x11drv_atom(WM_DELETE_WINDOW);
    protocols[i++] = x11drv_atom(_NET_WM_PING);
    if (use_take_focus) protocols[i++] = x11drv_atom(WM_TAKE_FOCUS);
    XChangeProperty( display, window, x11drv_atom(WM_PROTOCOLS),
                     XA_ATOM, 32, PropModeReplace, (unsigned char *)protocols, i );

    /* class hints */
    if ((class_hints = XAllocClassHint()))
    {
        static char steam_proton[] = "steam_proton";
        const char *app_id = getenv("SteamAppId");
        char proton_app_class[128];

        if(app_id && *app_id){
            snprintf(proton_app_class, sizeof(proton_app_class), "steam_app_%s", app_id);
            class_hints->res_name = proton_app_class;
            class_hints->res_class = proton_app_class;
        }else{
            class_hints->res_name = steam_proton;
            class_hints->res_class = steam_proton;
        }

        XSetClassHint( display, window, class_hints );
        XFree( class_hints );
    }

    /* set the WM_CLIENT_MACHINE and WM_LOCALE_NAME properties */
    XSetWMProperties(display, window, NULL, NULL, NULL, 0, NULL, NULL, NULL);
    /* set the pid. together, these properties are needed so the window manager can kill us if we freeze */
    i = getpid();
    XChangeProperty(display, window, x11drv_atom(_NET_WM_PID),
                    XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&i, 1);

    XChangeProperty( display, window, x11drv_atom(XdndAware),
                     XA_ATOM, 32, PropModeReplace, (unsigned char*)&dndVersion, 1 );

    update_user_time( 0 );  /* make sure that the user time window exists */
    if (user_time_window)
        XChangeProperty( display, window, x11drv_atom(_NET_WM_USER_TIME_WINDOW),
                         XA_WINDOW, 32, PropModeReplace, (unsigned char *)&user_time_window, 1 );
}


/***********************************************************************
 *              make_owner_managed
 *
 * If the window is managed, make sure its owner window is too.
 */
static void make_owner_managed( HWND hwnd )
{
    HWND owner;

    if (!(owner = NtUserGetWindowRelative( hwnd, GW_OWNER ))) return;
    if (is_managed( owner )) return;
    if (!is_managed( hwnd )) return;

    NtUserSetWindowPos( owner, 0, 0, 0, 0, 0,
                        SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOSIZE | SWP_NOMOVE |
                        SWP_NOREDRAW | SWP_DEFERERASE | SWP_NOSENDCHANGING | SWP_STATECHANGED );
}


/***********************************************************************
 *              set_wm_hints
 *
 * Set all the window manager hints for a window.
 */
static void set_wm_hints( struct x11drv_win_data *data )
{
    DWORD style, ex_style;

    if (data->hwnd == NtUserGetDesktopWindow())
    {
        /* force some styles for the desktop to get the correct decorations */
        style = WS_POPUP | WS_VISIBLE | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
        ex_style = WS_EX_APPWINDOW;
    }
    else
    {
        style = NtUserGetWindowLongW( data->hwnd, GWL_STYLE );
        ex_style = NtUserGetWindowLongW( data->hwnd, GWL_EXSTYLE );
    }

    set_size_hints( data, style );
    set_mwm_hints( data, style, ex_style );
    set_style_hints( data, style, ex_style );
}


/***********************************************************************
 *     init_clip_window
 */
Window init_clip_window(void)
{
    struct x11drv_thread_data *data = x11drv_init_thread_data();

    if (!data->clip_window &&
        (data->clip_window = (Window)NtUserGetProp( NtUserGetDesktopWindow(), clip_window_prop )))
    {
        XSelectInput( data->display, data->clip_window, StructureNotifyMask );
    }
    return data->clip_window;
}


/***********************************************************************
 *     update_user_time
 */
void update_user_time( Time time )
{
    if (!user_time_window)
    {
        Window win = XCreateWindow( gdi_display, root_window, -1, -1, 1, 1, 0, CopyFromParent,
                                    InputOnly, CopyFromParent, 0, NULL );
        if (InterlockedCompareExchangePointer( (void **)&user_time_window, (void *)win, 0 ))
            XDestroyWindow( gdi_display, win );
        TRACE( "user time window %lx\n", user_time_window );
    }

    if (!time) return;
    XLockDisplay( gdi_display );
    if (!last_user_time || (long)(time - last_user_time) > 0)
    {
        last_user_time = time;
        XChangeProperty( gdi_display, user_time_window, x11drv_atom(_NET_WM_USER_TIME),
                         XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&time, 1 );
    }
    XUnlockDisplay( gdi_display );
}

static void update_desktop_fullscreen( Display *display )
{
    XEvent xev;

    if (!is_virtual_desktop()) return;

    xev.xclient.type = ClientMessage;
    xev.xclient.window = root_window;
    xev.xclient.message_type = x11drv_atom(_NET_WM_STATE);
    xev.xclient.serial = 0;
    xev.xclient.display = display;
    xev.xclient.send_event = True;
    xev.xclient.format = 32;
    xev.xclient.data.l[0] = is_desktop_fullscreen() ? _NET_WM_STATE_ADD : _NET_WM_STATE_REMOVE;
    xev.xclient.data.l[1] = x11drv_atom(_NET_WM_STATE_FULLSCREEN);
    xev.xclient.data.l[2] = 0;
    xev.xclient.data.l[3] = 1;

    TRACE("action=%li\n", xev.xclient.data.l[0]);

    XSendEvent( display, DefaultRootWindow(display), False,
                SubstructureRedirectMask | SubstructureNotifyMask, &xev );

    xev.xclient.data.l[1] = x11drv_atom(_NET_WM_STATE_MAXIMIZED_VERT);
    xev.xclient.data.l[2] = x11drv_atom(_NET_WM_STATE_MAXIMIZED_HORZ);
    XSendEvent( display, DefaultRootWindow(display), False,
                SubstructureRedirectMask | SubstructureNotifyMask, &xev );
}

/* Update _NET_WM_FULLSCREEN_MONITORS when _NET_WM_STATE_FULLSCREEN is set to support fullscreen
 * windows spanning multiple monitors */
static void update_net_wm_fullscreen_monitors( struct x11drv_win_data *data )
{
    long monitors[4];
    XEvent xev;

    if (!(data->net_wm_state & (1 << NET_WM_STATE_FULLSCREEN)) || is_virtual_desktop())
        return;

    /* If the current display device handler cannot detect dynamic device changes, do not use
     * _NET_WM_FULLSCREEN_MONITORS because xinerama_get_fullscreen_monitors() may report wrong
     * indices because of stale xinerama monitor information */
    if (!X11DRV_DisplayDevices_SupportEventHandlers())
        return;

    if (!xinerama_get_fullscreen_monitors( &data->whole_rect, monitors ))
    {
        ERR( "Failed to find xinerama monitors at %s\n", wine_dbgstr_rect(&data->whole_rect) );
        return;
    }

    /* If _NET_WM_FULLSCREEN_MONITORS is not set and the fullscreen monitors are spanning only one
     * monitor then do not set _NET_WM_FULLSCREEN_MONITORS.
     *
     * If _NET_WM_FULLSCREEN_MONITORS is set then the property needs to be updated because it can't
     * be deleted by sending a _NET_WM_FULLSCREEN_MONITORS client message to the root window
     * according to the wm-spec version 1.5. Having the window spanning more than two monitors also
     * needs the property set. In other cases, _NET_WM_FULLSCREEN_MONITORS doesn't need to be set.
     * What's more, setting _NET_WM_FULLSCREEN_MONITORS adds a constraint on Mutter so that such a
     * window can't be moved to another monitor by using the Shift+Super+Up/Down/Left/Right
     * shortcut. So the property should be added only when necessary. */
    if (monitors[0] == monitors[1] && monitors[1] == monitors[2] && monitors[2] == monitors[3]
        && !data->net_wm_fullscreen_monitors_set)
        return;

    /* If _NET_WM_FULLSCREEN_MONITORS is not set and the fullscreen monitors are spanning only one
     * monitor then do not set _NET_WM_FULLSCREEN_MONITORS.
     *
     * If _NET_WM_FULLSCREEN_MONITORS is set then the property needs to be updated because it can't
     * be deleted by sending a _NET_WM_FULLSCREEN_MONITORS client message to the root window
     * according to the wm-spec version 1.4. Having the window spanning more than two monitors also
     * needs the property set. In other cases, _NET_WM_FULLSCREEN_MONITORS doesn't need to be set.
     * What's more, setting _NET_WM_FULLSCREEN_MONITORS adds a constraint on Mutter so that such a
     * window can't be moved to another monitor by using the Shift+Super+Up/Down/Left/Right
     * shortcut. So the property should be added only when necessary. */
    if (monitors[0] == monitors[1] && monitors[1] == monitors[2] && monitors[2] == monitors[3])
    {
        unsigned long count, remaining;
        BOOL prop_found = FALSE;
        long *prop_data;
        int format;
        Atom type;

        if (!XGetWindowProperty( data->display, data->whole_window,
                                 x11drv_atom(_NET_WM_FULLSCREEN_MONITORS), 0, ~0, False,
                                 XA_CARDINAL, &type, &format, &count, &remaining,
                                 (unsigned char **)&prop_data ))
        {
            if (type == XA_CARDINAL && format == 32 && count == 4)
                prop_found = TRUE;
            XFree(prop_data);
        }

        if (!prop_found)
            return;
    }

    if (!data->mapped)
    {
        XChangeProperty( data->display, data->whole_window, x11drv_atom(_NET_WM_FULLSCREEN_MONITORS),
                         XA_CARDINAL, 32, PropModeReplace, (unsigned char *)monitors, 4 );
    }
    else
    {
        xev.xclient.type = ClientMessage;
        xev.xclient.window = data->whole_window;
        xev.xclient.message_type = x11drv_atom(_NET_WM_FULLSCREEN_MONITORS);
        xev.xclient.serial = 0;
        xev.xclient.display = data->display;
        xev.xclient.send_event = True;
        xev.xclient.format = 32;
        xev.xclient.data.l[4] = 1;
        memcpy( xev.xclient.data.l, monitors, sizeof(monitors) );
        XSendEvent( data->display, root_window, False,
                    SubstructureRedirectMask | SubstructureNotifyMask, &xev );
    }
    data->net_wm_fullscreen_monitors_set = TRUE;
}

/***********************************************************************
 *     update_net_wm_states
 */
void update_net_wm_states( struct x11drv_win_data *data )
{
    unsigned long net_wm_bypass_compositor = 0;
    UINT i, style, ex_style, new_state = 0;
    HMONITOR monitor;

    if (!data->managed) return;
    if (data->whole_window == root_window)
    {
        update_desktop_fullscreen(data->display);
        return;
    }

    style = NtUserGetWindowLongW( data->hwnd, GWL_STYLE );
    if (style & WS_MINIMIZE)
        new_state |= data->net_wm_state & ((1 << NET_WM_STATE_FULLSCREEN)|(1 << NET_WM_STATE_MAXIMIZED));
    monitor = fs_hack_monitor_from_hwnd( data->hwnd );
    if ((!data->fs_hack || fs_hack_enabled( monitor )) && NtUserIsWindowRectFullScreen( &data->whole_rect ))
    {
        if ((style & WS_MAXIMIZE) && (style & WS_CAPTION) == WS_CAPTION)
            new_state |= (1 << NET_WM_STATE_MAXIMIZED);
        else if (!(style & WS_MINIMIZE))
        {
            if (!wm_is_steamcompmgr( data->display ) || !fs_hack_enabled( monitor ))
            {
                net_wm_bypass_compositor = 1;

                /* when fs hack is enabled, we don't want steamcompmgr to resize the window to be fullscreened */
                new_state |= (1 << NET_WM_STATE_FULLSCREEN);
            }
        }
    }
    else if (style & WS_MAXIMIZE)
        new_state |= (1 << NET_WM_STATE_MAXIMIZED);

    ex_style = NtUserGetWindowLongW( data->hwnd, GWL_EXSTYLE );
    if ((ex_style & WS_EX_TOPMOST) &&
        /* This workaround was initially targetting some mutter and KDE issues, but
         * removing it causes failure to focus out from exclusive fullscreen windows.
         *
         * Many games do not have any specific logic to get out of exclusive fullscreen
         * mode, and we have currently no way to tell exclusive fullscreen from a window
         * with topmost + fullscreen styles, so we cannot properly implement it either.
         */
        !(new_state & (1 << NET_WM_STATE_FULLSCREEN)))
        new_state |= (1 << NET_WM_STATE_ABOVE);
    if (!data->add_taskbar)
    {
        if (data->skip_taskbar || (ex_style & WS_EX_NOACTIVATE)
            || (ex_style & WS_EX_TOOLWINDOW && !(ex_style & WS_EX_APPWINDOW)))
            new_state |= (1 << NET_WM_STATE_SKIP_TASKBAR) | (1 << NET_WM_STATE_SKIP_PAGER) | (1 << KDE_NET_WM_STATE_SKIP_SWITCHER);
        else if (!(ex_style & WS_EX_APPWINDOW) && NtUserGetWindowRelative( data->hwnd, GW_OWNER ))
            new_state |= (1 << NET_WM_STATE_SKIP_TASKBAR);
    }

    if (!data->mapped)  /* set the _NET_WM_STATE atom directly */
    {
        Atom atoms[NB_NET_WM_STATES+1];
        DWORD count;

        for (i = count = 0; i < NB_NET_WM_STATES; i++)
        {
            if (!(new_state & (1 << i))) continue;
            TRACE( "setting wm state %u for unmapped window %p/%lx\n",
                   i, data->hwnd, data->whole_window );
            atoms[count++] = X11DRV_Atoms[net_wm_state_atoms[i] - FIRST_XATOM];
            if (net_wm_state_atoms[i] == XATOM__NET_WM_STATE_MAXIMIZED_VERT)
                atoms[count++] = x11drv_atom(_NET_WM_STATE_MAXIMIZED_HORZ);
        }
        XChangeProperty( data->display, data->whole_window, x11drv_atom(_NET_WM_STATE), XA_ATOM,
                         32, PropModeReplace, (unsigned char *)atoms, count );
    }
    else  /* ask the window manager to do it for us */
    {
        XEvent xev;

        xev.xclient.type = ClientMessage;
        xev.xclient.window = data->whole_window;
        xev.xclient.message_type = x11drv_atom(_NET_WM_STATE);
        xev.xclient.serial = 0;
        xev.xclient.display = data->display;
        xev.xclient.send_event = True;
        xev.xclient.format = 32;
        xev.xclient.data.l[3] = 1;
        xev.xclient.data.l[4] = 0;

        for (i = 0; i < NB_NET_WM_STATES; i++)
        {
            TRACE( "setting wm state %u for window %p/%lx to %u prev %u\n",
                   i, data->hwnd, data->whole_window,
                   (new_state & (1 << i)) != 0, (data->net_wm_state & (1 << i)) != 0 );

            if (i == NET_WM_STATE_FULLSCREEN)
            {
                data->pending_fullscreen = (new_state & (1 << i))
                                            && !(data->net_wm_state & (1 << NET_WM_STATE_FULLSCREEN)
                                            && wm_is_steamcompmgr(data->display));
                TRACE( "set pending_fullscreen to: %u\n", data->pending_fullscreen );
            }

            xev.xclient.data.l[0] = (new_state & (1 << i)) ? _NET_WM_STATE_ADD : _NET_WM_STATE_REMOVE;
            xev.xclient.data.l[1] = X11DRV_Atoms[net_wm_state_atoms[i] - FIRST_XATOM];
            xev.xclient.data.l[2] = ((net_wm_state_atoms[i] == XATOM__NET_WM_STATE_MAXIMIZED_VERT) ?
                                     x11drv_atom(_NET_WM_STATE_MAXIMIZED_HORZ) : 0);
            XSendEvent( data->display, root_window, False,
                        SubstructureRedirectMask | SubstructureNotifyMask, &xev );
        }
    }
    data->net_wm_state = new_state;

    if (!(style & WS_MINIMIZE))
        update_net_wm_fullscreen_monitors( data );

    XChangeProperty( data->display, data->whole_window, x11drv_atom(_NET_WM_BYPASS_COMPOSITOR), XA_CARDINAL,
                     32, PropModeReplace, (unsigned char *)&net_wm_bypass_compositor, 1 );
}

/***********************************************************************
 *     read_net_wm_states
 */
void read_net_wm_states( Display* display, struct x11drv_win_data *data )
{
    Atom type, *state;
    int format;
    unsigned long i, j, count, remaining;
    DWORD new_state = 0;
    BOOL maximized_horz = FALSE;

    if (!data->whole_window) return;

    if (!XGetWindowProperty( display, data->whole_window, x11drv_atom(_NET_WM_STATE), 0,
                             65536/sizeof(CARD32), False, XA_ATOM, &type, &format, &count,
                             &remaining, (unsigned char **)&state ))
    {
        if (type == XA_ATOM && format == 32)
        {
            for (i = 0; i < count; i++)
            {
                if (state[i] == x11drv_atom(_NET_WM_STATE_MAXIMIZED_HORZ))
                    maximized_horz = TRUE;
                for (j=0; j < NB_NET_WM_STATES; j++)
                {
                    if (state[i] == X11DRV_Atoms[net_wm_state_atoms[j] - FIRST_XATOM])
                    {
                        new_state |= 1 << j;
                    }
                }
            }
        }
        XFree( state );
    }

    if (!maximized_horz)
        new_state &= ~(1 << NET_WM_STATE_MAXIMIZED);

    data->net_wm_state = new_state;
}


/***********************************************************************
 *     set_xembed_flags
 */
static void set_xembed_flags( struct x11drv_win_data *data, unsigned long flags )
{
    unsigned long info[2];

    if (!data->whole_window) return;

    info[0] = 0; /* protocol version */
    info[1] = flags;
    XChangeProperty( data->display, data->whole_window, x11drv_atom(_XEMBED_INFO),
                     x11drv_atom(_XEMBED_INFO), 32, PropModeReplace, (unsigned char*)info, 2 );
}

static int skip_iconify( Display *display )
{
    static int cached = -1;
    const char *env;

    if (cached == -1)
    {
        FIXME( "HACK: skip_iconify.\n" );
        cached = wm_is_steamcompmgr( display ) && (env = getenv( "SteamGameId" )) && !strcmp( env, "1827980" );
    }

    return cached;
}

/***********************************************************************
 *     map_window
 */
static void map_window( HWND hwnd, DWORD new_style )
{
    struct x11drv_win_data *data;

    make_owner_managed( hwnd );
    wait_for_withdrawn_state( hwnd, TRUE );

    if (!(data = get_win_data( hwnd ))) return;

    if (data->whole_window && !data->mapped)
    {
        TRACE( "win %p/%lx\n", data->hwnd, data->whole_window );

        remove_startup_notification( data->display, data->whole_window );
        set_wm_hints( data );

        if (!data->embedded)
        {
            update_net_wm_states( data );
            sync_window_style( data );
            XMapWindow( data->display, data->whole_window );
            /* Mutter always unminimizes windows when handling map requests. Restore iconic state */
            if (new_style & WS_MINIMIZE && !skip_iconify( data->display ))
                XIconifyWindow( data->display, data->whole_window, data->vis.screen );
            XFlush( data->display );
            if (data->surface && data->vis.visualid != default_visual.visualid)
                data->surface->funcs->flush( data->surface );
        }
        else set_xembed_flags( data, XEMBED_MAPPED );

        data->mapped = TRUE;
        data->iconic = (new_style & WS_MINIMIZE) != 0;
        update_net_wm_fullscreen_monitors( data );
    }
    release_win_data( data );
}


/***********************************************************************
 *     unmap_window
 */
static void unmap_window( HWND hwnd )
{
    struct x11drv_win_data *data;

    wait_for_withdrawn_state( hwnd, FALSE );

    if (!(data = get_win_data( hwnd ))) return;

    if (data->mapped)
    {
        TRACE( "win %p/%lx\n", data->hwnd, data->whole_window );

        if (data->embedded) set_xembed_flags( data, 0 );
        else if (!data->managed) XUnmapWindow( data->display, data->whole_window );
        else XWithdrawWindow( data->display, data->whole_window, data->vis.screen );

        data->mapped = FALSE;
        data->net_wm_state = 0;
        data->pending_fullscreen = FALSE;
    }
    release_win_data( data );
}


/***********************************************************************
 *     make_window_embedded
 */
void make_window_embedded( struct x11drv_win_data *data )
{
    /* the window cannot be mapped before being embedded */
    if (data->mapped)
    {
        if (!data->managed) XUnmapWindow( data->display, data->whole_window );
        else XWithdrawWindow( data->display, data->whole_window, data->vis.screen );
        data->net_wm_state = 0;
        data->pending_fullscreen = FALSE;
    }
    data->embedded = TRUE;
    data->managed = TRUE;
    sync_window_style( data );
    set_xembed_flags( data, (data->mapped || data->embedder) ? XEMBED_MAPPED : 0 );
}


/***********************************************************************
 *     get_decoration_rect
 */
static void get_decoration_rect( struct x11drv_win_data *data, RECT *rect,
                                 const RECT *window_rect, const RECT *client_rect )
{
    DWORD style, ex_style, style_mask = 0, ex_style_mask = 0;
    unsigned long decor;

    SetRectEmpty( rect );
    if (!data->managed) return;

    style = NtUserGetWindowLongW( data->hwnd, GWL_STYLE );
    ex_style = NtUserGetWindowLongW( data->hwnd, GWL_EXSTYLE );
    decor = get_mwm_decorations( data, style, ex_style, window_rect, client_rect );

    if (decor & MWM_DECOR_TITLE) style_mask |= WS_CAPTION;
    if (decor & MWM_DECOR_BORDER)
    {
        style_mask |= WS_DLGFRAME | WS_THICKFRAME;
        ex_style_mask |= WS_EX_DLGMODALFRAME;
    }

    AdjustWindowRectEx( rect, style & style_mask, FALSE, ex_style & ex_style_mask );
}


/***********************************************************************
 *		X11DRV_window_to_X_rect
 *
 * Convert a rect from client to X window coordinates
 */
static void X11DRV_window_to_X_rect( struct x11drv_win_data *data, RECT *rect,
                                     const RECT *window_rect, const RECT *client_rect )
{
    RECT rc;

    if (IsRectEmpty( rect )) return;

    get_decoration_rect( data, &rc, window_rect, client_rect );
    rect->left   -= rc.left;
    rect->right  -= rc.right;
    rect->top    -= rc.top;
    rect->bottom -= rc.bottom;
    if (rect->top >= rect->bottom) rect->bottom = rect->top + 1;
    if (rect->left >= rect->right) rect->right = rect->left + 1;
}


/***********************************************************************
 *		X11DRV_X_to_window_rect
 *
 * Opposite of X11DRV_window_to_X_rect
 */
void X11DRV_X_to_window_rect( struct x11drv_win_data *data, RECT *rect, int x, int y, int cx, int cy )
{
    RECT rc;

    get_decoration_rect( data, &rc, &data->window_rect, &data->client_rect );

    x += min( data->window_rect.left - data->whole_rect.left, rc.left );
    y += min( data->window_rect.top - data->whole_rect.top, rc.top );
    cx += max( (data->window_rect.right - data->window_rect.left) -
               (data->whole_rect.right - data->whole_rect.left), rc.right - rc.left );
    cy += max( (data->window_rect.bottom - data->window_rect.top) -
               (data->whole_rect.bottom - data->whole_rect.top), rc.bottom - rc.top );
    SetRect( rect, x, y, x + cx, y + cy );
}


/***********************************************************************
 *		sync_window_position
 *
 * Synchronize the X window position with the Windows one
 */
static HWND sync_window_position( struct x11drv_win_data *data,
                                  UINT swp_flags, const RECT *old_window_rect,
                                  const RECT *old_whole_rect, const RECT *old_client_rect )
{
    DWORD style = NtUserGetWindowLongW( data->hwnd, GWL_STYLE );
    DWORD ex_style = NtUserGetWindowLongW( data->hwnd, GWL_EXSTYLE );
    RECT original_rect = {0};
    HWND prev_window = NULL;
    XWindowChanges changes;
    unsigned int mask = 0;

    if (data->managed && data->iconic) return NULL;

    /* resizing a managed maximized window is not allowed */
    if (!(style & WS_MAXIMIZE) || !data->managed)
    {
        if (data->fs_hack)
        {
            HMONITOR monitor;
            RECT rect;

            monitor = fs_hack_monitor_from_hwnd( data->hwnd );
            rect = fs_hack_real_mode( monitor );
            changes.width = rect.right - rect.left;
            changes.height = rect.bottom - rect.top;
            TRACE( "change width:%d height:%d\n", changes.width, changes.height );
        }
        else
        {
            changes.width = data->whole_rect.right - data->whole_rect.left;
            changes.height = data->whole_rect.bottom - data->whole_rect.top;
        }
        /* if window rect is empty force size to 1x1 */
        if (changes.width <= 0 || changes.height <= 0) changes.width = changes.height = 1;
        if (changes.width > 65535) changes.width = 65535;
        if (changes.height > 65535) changes.height = 65535;
        mask |= CWWidth | CWHeight;
    }

    /* only the size is allowed to change for the desktop window */
    if (data->whole_window != root_window)
    {
        POINT pt = virtual_screen_to_root( data->whole_rect.left, data->whole_rect.top );
        changes.x = pt.x;
        changes.y = pt.y;
        mask |= CWX | CWY;
    }

    if (!(swp_flags & SWP_NOZORDER) || (swp_flags & SWP_SHOWWINDOW))
    {
        /* find window that this one must be after */
        HWND prev = NtUserGetWindowRelative( data->hwnd, GW_HWNDPREV );

        while (prev && !(NtUserGetWindowLongW( prev, GWL_STYLE ) & WS_VISIBLE))
            prev = NtUserGetWindowRelative( prev, GW_HWNDPREV );
        if (!(prev_window = prev))  /* top child */
        {
            changes.stack_mode = Above;
            mask |= CWStackMode;
        }
        /* should use stack_mode Below but most window managers don't get it right */
        /* and Above with a sibling doesn't work so well either, so we ignore it */
    }

    set_size_hints( data, style );
    set_mwm_hints( data, style, ex_style );
    /* KWin doesn't allow moving a window with _NET_WM_STATE_FULLSCREEN set. So we need to remove
     * _NET_WM_STATE_FULLSCREEN before moving the window and restore it later */
    if (wm_is_kde( data->display ) && NtUserIsWindowRectFullScreen( &data->whole_rect ))
    {
        original_rect = data->whole_rect;
        SetRectEmpty( &data->whole_rect );
    }
    update_net_wm_states( data );
    data->configure_serial = NextRequest( data->display );
    XReconfigureWMWindow( data->display, data->whole_window, data->vis.screen, mask, &changes );
    if (!IsRectEmpty( &original_rect ))
    {
        data->whole_rect = original_rect;
        update_net_wm_states( data );
    }
#ifdef HAVE_LIBXSHAPE
    if (IsRectEmpty( old_window_rect ) != IsRectEmpty( &data->window_rect ))
        sync_window_region( data, (HRGN)1 );
    if (data->shaped)
    {
        int old_x_offset = old_window_rect->left - old_whole_rect->left;
        int old_y_offset = old_window_rect->top - old_whole_rect->top;
        int new_x_offset = data->window_rect.left - data->whole_rect.left;
        int new_y_offset = data->window_rect.top - data->whole_rect.top;
        if (old_x_offset != new_x_offset || old_y_offset != new_y_offset)
            XShapeOffsetShape( data->display, data->whole_window, ShapeBounding,
                               new_x_offset - old_x_offset, new_y_offset - old_y_offset );
    }
#endif

    TRACE( "win %p/%lx pos %d,%d,%dx%d after %lx changes=%x serial=%lu\n",
           data->hwnd, data->whole_window, (int)data->whole_rect.left, (int)data->whole_rect.top,
           (int)(data->whole_rect.right - data->whole_rect.left),
           (int)(data->whole_rect.bottom - data->whole_rect.top),
           changes.sibling, mask, data->configure_serial );

    return prev_window;
}


/***********************************************************************
 *		sync_client_position
 *
 * Synchronize the X client window position with the Windows one
 */
static void sync_client_position( struct x11drv_win_data *data,
                                  const RECT *old_client_rect, const RECT *old_whole_rect )
{
    int mask = 0;
    XWindowChanges changes;

    changes.x      = data->client_rect.left - data->whole_rect.left;
    changes.y      = data->client_rect.top - data->whole_rect.top;
    changes.width  = min( max( 1, data->client_rect.right - data->client_rect.left ), 65535 );
    changes.height = min( max( 1, data->client_rect.bottom - data->client_rect.top ), 65535 );

    if (changes.x != old_client_rect->left - old_whole_rect->left) mask |= CWX;
    if (changes.y != old_client_rect->top  - old_whole_rect->top)  mask |= CWY;
    if (changes.width  != old_client_rect->right - old_client_rect->left) mask |= CWWidth;
    if (changes.height != old_client_rect->bottom - old_client_rect->top) mask |= CWHeight;

    if (data->fs_hack)
    {
        HMONITOR monitor;
        RECT rect;

        monitor = fs_hack_monitor_from_hwnd( data->hwnd );
        rect = fs_hack_real_mode( monitor );
        changes.x = 0;
        changes.y = 0;
        changes.width = rect.right - rect.left;
        changes.height = rect.bottom - rect.top;
        mask = CWX | CWY | CWWidth | CWHeight;
        TRACE( "x:%d y:%d width:%d height:%d\n", changes.x, changes.y, changes.width, changes.height );
    }

    if (mask)
    {
        if (data->client_window)
        {
            TRACE( "setting client win %lx pos %d,%d,%dx%d changes=%x\n",
                   data->client_window, changes.x, changes.y, changes.width, changes.height, mask );
            XConfigureWindow( gdi_display, data->client_window, mask, &changes );
        }
        resize_vk_surfaces( data->hwnd, data->client_window, mask, &changes );
    }
}


/***********************************************************************
 *		move_window_bits
 *
 * Move the window bits when a window is moved.
 */
static void move_window_bits( HWND hwnd, Window window, const RECT *old_rect, const RECT *new_rect,
                              const RECT *old_client_rect, const RECT *new_client_rect,
                              const RECT *new_window_rect )
{
    RECT src_rect = *old_rect;
    RECT dst_rect = *new_rect;
    HDC hdc_src, hdc_dst;
    INT code;
    HRGN rgn;
    HWND parent = 0;

    if (!window)
    {
        OffsetRect( &dst_rect, -new_window_rect->left, -new_window_rect->top );
        parent = NtUserGetAncestor( hwnd, GA_PARENT );
        hdc_src = NtUserGetDCEx( parent, 0, DCX_CACHE );
        hdc_dst = NtUserGetDCEx( hwnd, 0, DCX_CACHE | DCX_WINDOW );
    }
    else
    {
        OffsetRect( &dst_rect, -new_client_rect->left, -new_client_rect->top );
        /* make src rect relative to the old position of the window */
        OffsetRect( &src_rect, -old_client_rect->left, -old_client_rect->top );
        if (dst_rect.left == src_rect.left && dst_rect.top == src_rect.top) return;
        hdc_src = hdc_dst = NtUserGetDCEx( hwnd, 0, DCX_CACHE );
    }

    rgn = NtGdiCreateRectRgn( dst_rect.left, dst_rect.top, dst_rect.right, dst_rect.bottom );
    NtGdiExtSelectClipRgn( hdc_dst, rgn, RGN_COPY );
    NtGdiDeleteObjectApp( rgn );
    /* WS_CLIPCHILDREN doesn't exclude children from the window update
     * region, and ExcludeUpdateRgn call may inappropriately clip valid
     * child window contents from the copied parent window bits, but we
     * still want to avoid copying invalid window bits when possible.
     */
    if (!(NtUserGetWindowLongW( hwnd, GWL_STYLE ) & WS_CLIPCHILDREN ))
        NtUserExcludeUpdateRgn( hdc_dst, hwnd );

    code = X11DRV_START_EXPOSURES;
    NtGdiExtEscape( hdc_dst, NULL, 0, X11DRV_ESCAPE, sizeof(code), (LPSTR)&code, 0, NULL );

    TRACE( "copying bits for win %p/%lx %s -> %s\n",
           hwnd, window, wine_dbgstr_rect(&src_rect), wine_dbgstr_rect(&dst_rect) );
    NtGdiBitBlt( hdc_dst, dst_rect.left, dst_rect.top,
                 dst_rect.right - dst_rect.left, dst_rect.bottom - dst_rect.top,
                 hdc_src, src_rect.left, src_rect.top, SRCCOPY, 0, 0 );

    rgn = 0;
    code = X11DRV_END_EXPOSURES;
    NtGdiExtEscape( hdc_dst, NULL, 0, X11DRV_ESCAPE, sizeof(code), (LPSTR)&code, sizeof(rgn), (LPSTR)&rgn );

    NtUserReleaseDC( hwnd, hdc_dst );
    if (hdc_src != hdc_dst) NtUserReleaseDC( parent, hdc_src );

    if (rgn)
    {
        if (!window)
        {
            /* map region to client rect since we are using DCX_WINDOW */
            NtGdiOffsetRgn( rgn, new_window_rect->left - new_client_rect->left,
                       new_window_rect->top - new_client_rect->top );
            NtUserRedrawWindow( hwnd, NULL, rgn,
                                RDW_INVALIDATE | RDW_FRAME | RDW_ERASE | RDW_ALLCHILDREN );
        }
        else NtUserRedrawWindow( hwnd, NULL, rgn, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN );
        NtGdiDeleteObjectApp( rgn );
    }
}


/***********************************************************************
 *              get_dummy_parent
 *
 * Create a dummy parent window for child windows that don't have a true X11 parent.
 */
Window get_dummy_parent(void)
{
    static Window dummy_parent;

    if (!dummy_parent)
    {
        XSetWindowAttributes attrib;

        attrib.override_redirect = True;
        attrib.border_pixel = 0;
        attrib.colormap = default_colormap;

#ifdef HAVE_LIBXSHAPE
        {
            static XRectangle empty_rect;
            dummy_parent = XCreateWindow( gdi_display, root_window, 0, 0, 1, 1, 0,
                                          default_visual.depth, InputOutput, default_visual.visual,
                                          CWColormap | CWBorderPixel | CWOverrideRedirect, &attrib );
            XShapeCombineRectangles( gdi_display, dummy_parent, ShapeBounding, 0, 0, &empty_rect, 1,
                                     ShapeSet, YXBanded );
        }
#else
        dummy_parent = XCreateWindow( gdi_display, root_window, -1, -1, 1, 1, 0, default_visual.depth,
                                      InputOutput, default_visual.visual,
                                      CWColormap | CWBorderPixel | CWOverrideRedirect, &attrib );
        WARN("Xshape support is not compiled in. Applications under XWayland may have poor performance.\n");
#endif
        XMapWindow( gdi_display, dummy_parent );
    }
    return dummy_parent;
}


/**********************************************************************
 *		detach_client_window
 */
void detach_client_window( struct x11drv_win_data *data, Window client_window, BOOL reparent )
{
    unsigned int allow_flip = 0;

    if (data->client_window != client_window || !client_window) return;
    data->client_window = 0;

    if (!data->whole_window) return;

    XSelectInput( data->display, client_window, 0 );
    XFlush( data->display ); /* make sure XSelectInput is disabled for client_window after this point */
    XDeleteContext( data->display, client_window, winContext );

    XChangeProperty( gdi_display, client_window, x11drv_atom(_WINE_ALLOW_FLIP), XA_CARDINAL, 32,
                     PropModeReplace, (unsigned char *)&allow_flip, sizeof(allow_flip) / 4 );
    if (reparent) XReparentWindow( gdi_display, client_window, get_dummy_parent(), 0, 0 );
    TRACE( "%p/%lx detached client window %lx\n", data->hwnd, data->whole_window, client_window );
}


/**********************************************************************
 *             attach_client_window
 */
void attach_client_window( struct x11drv_win_data *data, Window client_window )
{
    unsigned int allow_flip = 1;

    if (data->client_window == client_window || !client_window) return;
    detach_client_window( data, data->client_window, TRUE );
    data->client_window = client_window;

    if (!data->whole_window) return;

    XSaveContext( data->display, client_window, winContext, (char *)data->hwnd );
    XSelectInput( data->display, client_window, ExposureMask );
    XFlush( data->display ); /* make sure XSelectInput is enabled for client_window after this point */

    XChangeProperty( gdi_display, client_window, x11drv_atom(_WINE_ALLOW_FLIP), XA_CARDINAL, 32,
                     PropModeReplace, (unsigned char *)&allow_flip, sizeof(allow_flip) / 4 );
    XReparentWindow( gdi_display, client_window, data->whole_window, data->client_rect.left - data->whole_rect.left,
                     data->client_rect.top - data->whole_rect.top );

    TRACE( "%p/%lx attached client window %lx\n", data->hwnd, data->whole_window, client_window );
}


/**********************************************************************
 *      destroy_client_window
 */
void destroy_client_window( HWND hwnd, Window client_window )
{
    struct x11drv_win_data *data;

    if (!client_window) return;

    if ((data = get_win_data( hwnd )))
    {
        detach_client_window( data, client_window, FALSE );
        release_win_data( data );
    }

    XDestroyWindow( gdi_display, client_window );
    TRACE( "%p destroyed client window %lx\n", hwnd, client_window );
}


/**********************************************************************
 *		create_dummy_client_window
 */
Window create_dummy_client_window(void)
{
    XSetWindowAttributes attr;

    attr.colormap = default_colormap;
    attr.bit_gravity = NorthWestGravity;
    attr.win_gravity = NorthWestGravity;
    attr.backing_store = NotUseful;
    attr.border_pixel = 0;

    return XCreateWindow( gdi_display, get_dummy_parent(), 0, 0, 1, 1, 0,
                          default_visual.depth, InputOutput, default_visual.visual,
                          CWBitGravity | CWWinGravity | CWBackingStore | CWColormap | CWBorderPixel, &attr );
}

/**********************************************************************
 *		create_client_window
 */
Window create_client_window( HWND hwnd, const XVisualInfo *visual, Colormap colormap )
{
    struct x11drv_win_data *data = get_win_data( hwnd );
    Window parent_window, client_window;
    XSetWindowAttributes attr;
    unsigned int attr_mask;
    int x, y, cx, cy;

    if (!data)
    {
        /* explicitly create data for HWND_MESSAGE windows since they can be used for OpenGL */
        HWND parent = NtUserGetAncestor( hwnd, GA_PARENT );
        if (parent == NtUserGetDesktopWindow() || NtUserGetAncestor( parent, GA_PARENT )) return 0;
        if (!(data = alloc_win_data( thread_init_display(), hwnd ))) return 0;
        NtUserGetClientRect( hwnd, &data->client_rect );
        data->window_rect = data->whole_rect = data->client_rect;
    }

    attr.colormap = colormap;
    attr.bit_gravity = NorthWestGravity;
    attr.win_gravity = NorthWestGravity;
    attr.backing_store = NotUseful;
    attr.border_pixel = 0;
    attr_mask = CWColormap | CWBitGravity | CWWinGravity | CWBackingStore | CWBorderPixel;

    x = data->client_rect.left - data->whole_rect.left;
    y = data->client_rect.top - data->whole_rect.top;
    cx = min( max( 1, data->client_rect.right - data->client_rect.left ), 65535 );
    cy = min( max( 1, data->client_rect.bottom - data->client_rect.top ), 65535 );

    if (data->fs_hack)
    {
        HMONITOR monitor = fs_hack_monitor_from_hwnd( hwnd );
        RECT rect = fs_hack_real_mode( monitor );
        cx = rect.right - rect.left;
        cy = rect.bottom - rect.top;

        TRACE( "width:%d height:%d\n", cx, cy );
    }

    TRACE( "setting client rect: %u, %u x %ux%u\n", x, y, cx, cy );

    /* NOTE: Creating the client windows as child of dummy parent has bad interactions with
     * Steam Overlay and will cause whole_window to minimize when the overlay opens... */
    parent_window = data->whole_window ? data->whole_window : get_dummy_parent();
    if ((client_window = XCreateWindow( gdi_display, parent_window, x, y, cx, cy, 0, default_visual.depth,
                                        InputOutput, visual->visual, attr_mask, &attr )))
    {
        XFlush( gdi_display ); /* make sure client_window is created for XSelectInput */
        XSync( data->display, False ); /* make sure client_window is known from data->display */

        attach_client_window( data, client_window );
        XMapWindow( gdi_display, data->client_window );

        TRACE( "%p xwin %lx/%lx\n", data->hwnd, data->whole_window, data->client_window );
    }

    release_win_data( data );
    return client_window;
}


void set_hwnd_style_props( Display *display, Window window, HWND hwnd )
{
    DWORD style = NtUserGetWindowLongW( hwnd, GWL_STYLE ), exstyle = NtUserGetWindowLongW( hwnd, GWL_EXSTYLE );

    TRACE( "display %p, window %lx, hwnd %p\n", display, window, hwnd );

    XChangeProperty( display, window, x11drv_atom(_WINE_HWND_STYLE), XA_CARDINAL, 32,
                     PropModeReplace, (unsigned char *)&style, sizeof(style) / 4 );
    XChangeProperty( display, window, x11drv_atom(_WINE_HWND_EXSTYLE), XA_CARDINAL, 32,
                     PropModeReplace, (unsigned char *)&exstyle, sizeof(exstyle) / 4 );
}


void set_gamescope_overlay_prop( Display *display, Window window, HWND hwnd )
{
    static const WCHAR class_name[] = {'X','a','l','i','a','O','v','e','r','l','a','y','B','o','x',0};
    WCHAR class_name_buf[16];
    UNICODE_STRING class_name_str;
    INT ret;

    class_name_str.Buffer = class_name_buf;
    class_name_str.MaximumLength = sizeof(class_name_buf);

    ret = NtUserGetClassName( hwnd, FALSE, &class_name_str );

    if (ret && !wcscmp( class_name_buf, class_name )) {
        DWORD one = 1;

        TRACE( "setting GAMESCOPE_XALIA_OVERLAY on window %lx, hwnd %p\n", window, hwnd );

        XChangeProperty( display, window, x11drv_atom(GAMESCOPE_XALIA_OVERLAY), XA_CARDINAL, 32,
                         PropModeReplace, (unsigned char *)&one, sizeof(one) / 4 );
    }
}


/**********************************************************************
 *		create_whole_window
 *
 * Create the whole X window for a given window
 */
static void create_whole_window( struct x11drv_win_data *data )
{
    unsigned int allow_flip = 0;
    int cx, cy, mask;
    XSetWindowAttributes attr;
    WCHAR text[1024];
    COLORREF key;
    BYTE alpha;
    DWORD layered_flags;
    HRGN win_rgn;
    POINT pos;

    if (!data->managed && is_window_managed( data->hwnd, SWP_NOACTIVATE, &data->window_rect ))
    {
        TRACE( "making win %p/%lx managed\n", data->hwnd, data->whole_window );
        data->managed = TRUE;
    }

    if ((win_rgn = NtGdiCreateRectRgn( 0, 0, 0, 0 )) &&
        NtUserGetWindowRgnEx( data->hwnd, win_rgn, 0 ) == ERROR)
    {
        NtGdiDeleteObjectApp( win_rgn );
        win_rgn = 0;
    }
    data->shaped = (win_rgn != 0);

    if (data->vis.visualid != default_visual.visualid)
        data->whole_colormap = XCreateColormap( data->display, root_window, data->vis.visual, AllocNone );

    mask = get_window_attributes( data, &attr );

    attr.background_pixel = XBlackPixel( data->display, data->vis.screen );
    mask |= CWBackPixel;

    if (!(cx = data->whole_rect.right - data->whole_rect.left)) cx = 1;
    else if (cx > 65535) cx = 65535;
    if (!(cy = data->whole_rect.bottom - data->whole_rect.top)) cy = 1;
    else if (cy > 65535) cy = 65535;

    if (data->fs_hack)
    {
        RECT rect = {0, 0, 0, 0};
        HMONITOR monitor;

        monitor = fs_hack_monitor_from_hwnd( data->hwnd );
        rect = fs_hack_real_mode( monitor );
        cx = rect.right - rect.left;
        cy = rect.bottom - rect.top;
        TRACE( "width:%d height:%d\n", cx, cy );
    }

    pos = virtual_screen_to_root( data->whole_rect.left, data->whole_rect.top );
    data->whole_window = XCreateWindow( data->display, root_window, pos.x, pos.y,
                                        cx, cy, 0, data->vis.depth, InputOutput,
                                        data->vis.visual, mask, &attr );
    if (!data->whole_window) goto done;

    XChangeProperty( data->display, data->whole_window, x11drv_atom(_WINE_ALLOW_FLIP), XA_CARDINAL, 32,
                     PropModeReplace, (unsigned char *)&allow_flip, sizeof(allow_flip) / 4 );

    X11DRV_XInput2_Enable( data->display, data->whole_window, attr.event_mask );
    set_initial_wm_hints( data->display, data->whole_window );
    set_wm_hints( data );

    XSaveContext( data->display, data->whole_window, winContext, (char *)data->hwnd );
    NtUserSetProp( data->hwnd, whole_window_prop, (HANDLE)data->whole_window );

    set_hwnd_style_props( data->display, data->whole_window, data->hwnd );

    set_gamescope_overlay_prop( data->display, data->whole_window, data->hwnd );

    /* set the window text */
    if (!NtUserInternalGetWindowText( data->hwnd, text, ARRAY_SIZE( text ))) text[0] = 0;
    sync_window_text( data->display, data->whole_window, text );

    /* set the window region */
    if (win_rgn || IsRectEmpty( &data->window_rect )) sync_window_region( data, win_rgn );

    /* set the window opacity */
    if (!NtUserGetLayeredWindowAttributes( data->hwnd, &key, &alpha, &layered_flags )) layered_flags = 0;
    sync_window_opacity( data->display, data->whole_window, key, alpha, layered_flags );

    XFlush( data->display );  /* make sure the window exists before we start painting to it */
    XSync( gdi_display, False ); /* make sure whole_window is known from gdi_display */

done:
    if (win_rgn) NtGdiDeleteObjectApp( win_rgn );
}


/**********************************************************************
 *		destroy_whole_window
 *
 * Destroy the whole X window for a given window.
 */
static void destroy_whole_window( struct x11drv_win_data *data, BOOL already_destroyed )
{
    TRACE( "win %p xwin %lx/%lx\n", data->hwnd, data->whole_window, data->client_window );

    detach_client_window( data, data->client_window, !already_destroyed );

    if (!data->whole_window)
    {
        if (data->embedded)
        {
            Window xwin = (Window)NtUserGetProp( data->hwnd, foreign_window_prop );
            if (xwin)
            {
                if (!already_destroyed) XSelectInput( data->display, xwin, 0 );
                XDeleteContext( data->display, xwin, winContext );
                NtUserRemoveProp( data->hwnd, foreign_window_prop );
            }
            return;
        }
    }
    else
    {
        XDeleteContext( data->display, data->whole_window, winContext );
        if (!already_destroyed)
        {
            XSync( gdi_display, False ); /* make sure XReparentWindow requests have completed before destroying whole_window */
            XDestroyWindow( data->display, data->whole_window );
        }
    }
    if (data->whole_colormap) XFreeColormap( data->display, data->whole_colormap );
    data->whole_window = 0;
    data->whole_colormap = 0;
    data->wm_state = WithdrawnState;
    data->net_wm_state = 0;
    data->pending_fullscreen = FALSE;
    data->mapped = FALSE;
    if (data->xic)
    {
        XUnsetICFocus( data->xic );
        XDestroyIC( data->xic );
        data->xic = 0;
    }
    /* Outlook stops processing messages after destroying a dialog, so we need an explicit flush */
    XFlush( data->display );
    if (data->surface) window_surface_release( data->surface );
    data->surface = NULL;
    NtUserRemoveProp( data->hwnd, whole_window_prop );
}


/**********************************************************************
 *		set_window_visual
 *
 * Change the visual by destroying and recreating the X window if needed.
 */
void set_window_visual( struct x11drv_win_data *data, const XVisualInfo *vis, BOOL use_alpha )
{
    Window client_window = data->client_window;

    if (!data->use_alpha == !use_alpha) return;
    if (data->surface) window_surface_release( data->surface );
    data->surface = NULL;
    data->use_alpha = use_alpha;

    if (data->vis.visualid == vis->visualid) return;
    client_window = data->client_window;
    /* detach the client before re-creating whole_window */
    detach_client_window( data, client_window, TRUE );
    destroy_whole_window( data, FALSE );
    data->vis = *vis;
    create_whole_window( data );
    /* attach the client back to the re-created whole_window */
    attach_client_window( data, client_window );
}


/*****************************************************************
 *		SetWindowText   (X11DRV.@)
 */
void X11DRV_SetWindowText( HWND hwnd, LPCWSTR text )
{
    Window win;

    if ((win = X11DRV_get_whole_window( hwnd )) && win != DefaultRootWindow(gdi_display))
    {
        Display *display = thread_init_display();
        sync_window_text( display, win, text );
    }
}


/***********************************************************************
 *		SetWindowStyle   (X11DRV.@)
 *
 * Update the X state of a window to reflect a style change
 */
void X11DRV_SetWindowStyle( HWND hwnd, INT offset, STYLESTRUCT *style )
{
    struct x11drv_win_data *data;
    DWORD changed = style->styleNew ^ style->styleOld;
    HWND parent = NtUserGetAncestor( hwnd, GA_PARENT );
    BOOL need_sync_gl = FALSE;

    if (offset == GWL_STYLE && (changed & WS_CHILD))
    {
        if (NtUserGetWindowRelative( parent, GW_CHILD ) ||
            NtUserGetAncestor( parent, GA_PARENT ) != NtUserGetDesktopWindow())
            sync_vk_surface( parent, TRUE );
        else
            sync_vk_surface( parent, FALSE );
        sync_vk_surface( hwnd, style->styleNew & WS_CHILD );
    }

    if (hwnd == NtUserGetDesktopWindow()) return;
    if (!(data = get_win_data( hwnd ))) return;
    if (!data->whole_window) goto done;

    if (offset == GWL_STYLE && (changed & WS_DISABLED)) set_wm_hints( data );

    if (offset == GWL_EXSTYLE && (changed & WS_EX_LAYERED)) /* changing WS_EX_LAYERED resets attributes */
    {
        data->layered = FALSE;
        data->layered_attributes = FALSE;
        need_sync_gl = TRUE;
        set_window_visual( data, &default_visual, FALSE );
        sync_window_opacity( data->display, data->whole_window, 0, 0, 0 );
        if (data->surface) set_surface_color_key( data->surface, CLR_INVALID );
    }
done:
    release_win_data( data );
    if (need_sync_gl) sync_gl_drawable( hwnd, FALSE );
}


/***********************************************************************
 *		DestroyWindow   (X11DRV.@)
 */
void X11DRV_DestroyWindow( HWND hwnd )
{
    struct x11drv_thread_data *thread_data = x11drv_thread_data();
    struct x11drv_win_data *data;
    HWND parent = NtUserGetAncestor( hwnd, GA_PARENT );

    if (!NtUserGetWindowRelative( parent, GW_CHILD ) &&
        NtUserGetAncestor( parent, GA_PARENT ) == NtUserGetDesktopWindow())
    {
        sync_gl_drawable( parent, FALSE );
        sync_vk_surface( parent, FALSE );
    }

    if (!(data = get_win_data( hwnd ))) return;

    destroy_whole_window( data, FALSE );
    if (thread_data->last_focus == hwnd) thread_data->last_focus = 0;
    if (thread_data->last_xic_hwnd == hwnd) thread_data->last_xic_hwnd = 0;
    if (data->icon_pixmap) XFreePixmap( gdi_display, data->icon_pixmap );
    if (data->icon_mask) XFreePixmap( gdi_display, data->icon_mask );
    free( data->icon_bits );
    XDeleteContext( gdi_display, (XID)hwnd, win_data_context );
    release_win_data( data );
    free( data );
    destroy_gl_drawable( hwnd );
    destroy_vk_surface( hwnd );
}


/***********************************************************************
 *		X11DRV_DestroyNotify
 */
BOOL X11DRV_DestroyNotify( HWND hwnd, XEvent *event )
{
    struct x11drv_win_data *data;
    BOOL embedded;

    if (!(data = get_win_data( hwnd ))) return FALSE;
    embedded = data->embedded;
    if (!embedded) FIXME( "window %p/%lx destroyed from the outside\n", hwnd, data->whole_window );

    destroy_whole_window( data, TRUE );
    release_win_data( data );
    if (embedded) send_message( hwnd, WM_CLOSE, 0, 0 );
    return TRUE;
}


/* initialize the desktop window id in the desktop manager process */
static BOOL create_desktop_win_data( Window win, HWND hwnd )
{
    struct x11drv_thread_data *thread_data = x11drv_thread_data();
    Display *display = thread_data->display;
    struct x11drv_win_data *data;

    if (!(data = alloc_win_data( display, hwnd ))) return FALSE;
    data->whole_window = win;
    data->managed = TRUE;
    NtUserSetProp( data->hwnd, whole_window_prop, (HANDLE)win );
    set_initial_wm_hints( display, win );
    release_win_data( data );
    if (thread_data->clip_window) XReparentWindow( display, thread_data->clip_window, win, 0, 0 );
    return TRUE;
}

/**********************************************************************
 *		SetDesktopWindow   (X11DRV.@)
 */
void X11DRV_SetDesktopWindow( HWND hwnd )
{
    unsigned int width, height;

    detect_wm( gdi_display );

    /* retrieve the real size of the desktop */
    SERVER_START_REQ( get_window_rectangles )
    {
        req->handle = wine_server_user_handle( hwnd );
        req->relative = COORDS_CLIENT;
        wine_server_call( req );
        width  = reply->window.right;
        height = reply->window.bottom;
    }
    SERVER_END_REQ;

    if (!width && !height)  /* not initialized yet */
    {
        RECT rect;

        X11DRV_DisplayDevices_Init( TRUE );
        rect = NtUserGetVirtualScreenRect();

        SERVER_START_REQ( set_window_pos )
        {
            req->handle        = wine_server_user_handle( hwnd );
            req->previous      = 0;
            req->swp_flags     = SWP_NOZORDER;
            req->window.left   = rect.left;
            req->window.top    = rect.top;
            req->window.right  = rect.right;
            req->window.bottom = rect.bottom;
            req->client        = req->window;
            wine_server_call( req );
        }
        SERVER_END_REQ;

        if (!is_virtual_desktop()) return;
        if (!create_desktop_win_data( root_window, hwnd ))
        {
            ERR( "Failed to create virtual desktop window data\n" );
            root_window = DefaultRootWindow( gdi_display );
        }
        else if (is_desktop_fullscreen())
        {
            Display *display = x11drv_thread_data()->display;
            TRACE("setting desktop to fullscreen\n");
            XChangeProperty( display, root_window, x11drv_atom(_NET_WM_STATE), XA_ATOM, 32, PropModeReplace,
                             (unsigned char*)&x11drv_atom(_NET_WM_STATE_FULLSCREEN), 1 );
        }
    }
    else
    {
        Window win = (Window)NtUserGetProp( hwnd, whole_window_prop );
        if (win && win != root_window)
        {
            X11DRV_init_desktop( win );
            X11DRV_DisplayDevices_Init( TRUE );
        }
    }
}


#define WM_WINE_NOTIFY_ACTIVITY WM_USER
#define WM_WINE_DELETE_TAB      (WM_USER + 1)
#define WM_WINE_ADD_TAB         (WM_USER + 2)

/**********************************************************************
 *           DesktopWindowProc   (X11DRV.@)
 */
LRESULT X11DRV_DesktopWindowProc( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
    switch (msg)
    {
    case WM_WINE_NOTIFY_ACTIVITY:
    {
        static ULONG last = 0;
        ULONG now = NtGetTickCount();
        /* calling XResetScreenSaver too often can cause performance
         * problems, so throttle it */
        if (now > last + 5000)
        {
            XResetScreenSaver( gdi_display );
            XFlush( gdi_display );
            last = now;
        }
        break;
    }
    case WM_WINE_DELETE_TAB:
        send_notify_message( (HWND)wp, WM_X11DRV_DELETE_TAB, 0, 0 );
        break;
    case WM_WINE_ADD_TAB:
        send_notify_message( (HWND)wp, WM_X11DRV_ADD_TAB, 0, 0 );
        break;
    case WM_DISPLAYCHANGE:
        X11DRV_resize_desktop();
        break;
    }
    return NtUserMessageCall( hwnd, msg, wp, lp, 0, NtUserDefWindowProc, FALSE );
}

/**********************************************************************
 *		CreateWindow   (X11DRV.@)
 */
BOOL X11DRV_CreateWindow( HWND hwnd )
{
    if (hwnd == NtUserGetDesktopWindow())
    {
        struct x11drv_thread_data *data = x11drv_init_thread_data();
        XSetWindowAttributes attr;

        data->xi2_rawinput_only = TRUE;
        X11DRV_XInput2_Enable( data->display, None, PointerMotionMask | ButtonPressMask | ButtonReleaseMask );

        /* create the cursor clipping window */
        attr.override_redirect = TRUE;
        attr.event_mask = StructureNotifyMask | FocusChangeMask;
        data->clip_window = XCreateWindow( data->display, root_window, 0, 0, 1, 1, 0, 0,
                                           InputOnly, default_visual.visual,
                                           CWOverrideRedirect | CWEventMask, &attr );
        X11DRV_XInput2_Enable( data->display, data->clip_window, attr.event_mask );
        XSelectInput( data->display, DefaultRootWindow( data->display ), PropertyChangeMask );
        XFlush( data->display );
        NtUserSetProp( hwnd, clip_window_prop, (HANDLE)data->clip_window );
        X11DRV_DisplayDevices_RegisterEventHandlers();
    }
    return TRUE;
}


/***********************************************************************
 *		get_win_data
 *
 * Lock and return the X11 data structure associated with a window.
 */
struct x11drv_win_data *get_win_data( HWND hwnd )
{
    char *data;

    if (!hwnd) return NULL;
    pthread_mutex_lock( &win_data_mutex );
    if (!XFindContext( gdi_display, (XID)hwnd, win_data_context, &data ))
        return (struct x11drv_win_data *)data;
    pthread_mutex_unlock( &win_data_mutex );
    return NULL;
}


/***********************************************************************
 *		release_win_data
 *
 * Release the data returned by get_win_data.
 */
void release_win_data( struct x11drv_win_data *data )
{
    if (data) pthread_mutex_unlock( &win_data_mutex );
}


/***********************************************************************
 *		X11DRV_create_win_data
 *
 * Create an X11 data window structure for an existing window.
 */
static struct x11drv_win_data *X11DRV_create_win_data( HWND hwnd, const RECT *window_rect,
                                                       const RECT *client_rect )
{
    Display *display;
    struct x11drv_win_data *data;
    HWND parent;

    if (!(parent = NtUserGetAncestor( hwnd, GA_PARENT ))) return NULL;  /* desktop */

    /* don't create win data for HWND_MESSAGE windows */
    if (parent != NtUserGetDesktopWindow() && !NtUserGetAncestor( parent, GA_PARENT )) return NULL;

    if (NtUserGetWindowThread( hwnd, NULL ) != GetCurrentThreadId()) return NULL;

    /* Recreate the parent gl_drawable now that we know there are child windows
     * that will need clipping support.
     */
    sync_gl_drawable( parent, TRUE );
    sync_vk_surface( parent, TRUE );

    display = thread_init_display();
    init_clip_window();  /* make sure the clip window is initialized in this thread */

    if (!(data = alloc_win_data( display, hwnd ))) return NULL;

    data->whole_rect = data->window_rect = *window_rect;
    data->client_rect = *client_rect;
    if (parent == NtUserGetDesktopWindow())
    {
        create_whole_window( data );
        TRACE( "win %p/%lx window %s whole %s client %s\n",
               hwnd, data->whole_window, wine_dbgstr_rect( &data->window_rect ),
               wine_dbgstr_rect( &data->whole_rect ), wine_dbgstr_rect( &data->client_rect ));
    }
    return data;
}


/***********************************************************************
 *		create_foreign_window
 *
 * Create a foreign window for the specified X window and its ancestors
 */
HWND create_foreign_window( Display *display, Window xwin )
{
    static const WCHAR classW[] = {'_','_','w','i','n','e','_','x','1','1','_','f','o','r','e','i','g','n','_','w','i','n','d','o','w',0};
    static BOOL class_registered;
    struct x11drv_win_data *data;
    HWND hwnd, parent;
    POINT pos;
    Window xparent, xroot;
    Window *xchildren;
    unsigned int nchildren;
    XWindowAttributes attr;
    UINT style = WS_CLIPCHILDREN;
    UNICODE_STRING class_name = RTL_CONSTANT_STRING( classW );

    if (!class_registered)
    {
        UNICODE_STRING version = { 0 };
        WNDCLASSEXW class;

        memset( &class, 0, sizeof(class) );
        class.cbSize        = sizeof(class);
        class.lpfnWndProc   = client_foreign_window_proc;
        class.lpszClassName = classW;
        if (!NtUserRegisterClassExWOW( &class, &class_name, &version, NULL, 0, 0, NULL ) &&
            RtlGetLastWin32Error() != ERROR_CLASS_ALREADY_EXISTS)
        {
            ERR( "Could not register foreign window class\n" );
            return FALSE;
        }
        class_registered = TRUE;
    }

    if (XFindContext( display, xwin, winContext, (char **)&hwnd )) hwnd = 0;
    if (hwnd) return hwnd;  /* already created */

    XSelectInput( display, xwin, StructureNotifyMask );
    if (!XGetWindowAttributes( display, xwin, &attr ) ||
        !XQueryTree( display, xwin, &xroot, &xparent, &xchildren, &nchildren ))
    {
        XSelectInput( display, xwin, 0 );
        return 0;
    }
    XFree( xchildren );

    if (xparent == xroot)
    {
        parent = NtUserGetDesktopWindow();
        style |= WS_POPUP;
        pos = root_to_virtual_screen( attr.x, attr.y );
    }
    else
    {
        parent = create_foreign_window( display, xparent );
        style |= WS_CHILD;
        pos.x = attr.x;
        pos.y = attr.y;
    }

    RtlInitUnicodeString( &class_name, classW );
    hwnd = NtUserCreateWindowEx( 0, &class_name, &class_name, NULL, style, pos.x, pos.y,
                                 attr.width, attr.height, parent, 0, NULL, NULL, 0, NULL,
                                 0, FALSE );

    if (!(data = alloc_win_data( display, hwnd )))
    {
        NtUserDestroyWindow( hwnd );
        return 0;
    }
    SetRect( &data->window_rect, pos.x, pos.y, pos.x + attr.width, pos.y + attr.height );
    data->whole_rect = data->client_rect = data->window_rect;
    data->whole_window = data->client_window = 0;
    data->embedded = TRUE;
    data->mapped = TRUE;

    NtUserSetProp( hwnd, foreign_window_prop, (HANDLE)xwin );
    XSaveContext( display, xwin, winContext, (char *)data->hwnd );

    TRACE( "win %lx parent %p style %08x %s -> hwnd %p\n",
           xwin, parent, style, wine_dbgstr_rect(&data->window_rect), hwnd );

    release_win_data( data );

    NtUserShowWindow( hwnd, SW_SHOW );
    return hwnd;
}


/***********************************************************************
 *              SystrayDockInit   (X11DRV.@)
 */
void X11DRV_SystrayDockInit( HWND hwnd )
{
    Display *display;

    if (is_virtual_desktop()) return;

    systray_hwnd = hwnd;
    display = thread_init_display();
    if (DefaultScreen( display ) == 0)
        systray_atom = x11drv_atom(_NET_SYSTEM_TRAY_S0);
    else
    {
        char systray_buffer[29]; /* strlen(_NET_SYSTEM_TRAY_S4294967295)+1 */
        sprintf( systray_buffer, "_NET_SYSTEM_TRAY_S%u", DefaultScreen( display ) );
        systray_atom = XInternAtom( display, systray_buffer, False );
    }

    XSelectInput( display, DefaultRootWindow( display ), StructureNotifyMask | PropertyChangeMask );
}


/***********************************************************************
 *              SystrayDockClear   (X11DRV.@)
 */
void X11DRV_SystrayDockClear( HWND hwnd )
{
    Window win = X11DRV_get_whole_window( hwnd );
    if (win) XClearArea( gdi_display, win, 0, 0, 0, 0, True );
}


/***********************************************************************
 *              SystrayDockRemove   (X11DRV.@)
 */
BOOL X11DRV_SystrayDockRemove( HWND hwnd )
{
    struct x11drv_win_data *data;
    BOOL ret;

    /* make sure we don't try to unmap it, it confuses some systray docks */
    if ((data = get_win_data( hwnd )))
    {
        if ((ret = data->embedded)) data->mapped = FALSE;
        release_win_data( data );
        return ret;
    }

    return FALSE;
}


/* find the X11 window owner the system tray selection */
static Window get_systray_selection_owner( Display *display )
{
    return XGetSelectionOwner( display, systray_atom );
}


static void get_systray_visual_info( Display *display, Window systray_window, XVisualInfo *info )
{
    XVisualInfo *list, template;
    VisualID *visual_id;
    Atom type;
    int format, num;
    unsigned long count, remaining;

    *info = default_visual;
    if (XGetWindowProperty( display, systray_window, x11drv_atom(_NET_SYSTEM_TRAY_VISUAL), 0,
                            65536/sizeof(CARD32), False, XA_VISUALID, &type, &format, &count,
                            &remaining, (unsigned char **)&visual_id ))
        return;

    if (type == XA_VISUALID && format == 32)
    {
        template.visualid = visual_id[0];
        if ((list = XGetVisualInfo( display, VisualIDMask, &template, &num )))
        {
            *info = list[0];
            TRACE_(systray)( "systray window %lx got visual %lx\n", systray_window, info->visualid );
            XFree( list );
        }
    }
    XFree( visual_id );
}


/***********************************************************************
 *              SystrayDockInsert   (X11DRV.@)
 */
BOOL X11DRV_SystrayDockInsert( HWND hwnd, UINT cx, UINT cy, void *icon )
{
    Display *display = thread_init_display();
    Window systray_window, window;
    XEvent ev;
    XVisualInfo visual;
    struct x11drv_win_data *data;

    if (!(systray_window = get_systray_selection_owner( display ))) return FALSE;

    get_systray_visual_info( display, systray_window, &visual );

    if (!(data = get_win_data( hwnd ))) return FALSE;
    set_window_visual( data, &visual, TRUE );
    make_window_embedded( data );
    window = data->whole_window;
    release_win_data( data );

    NtUserShowWindow( hwnd, SW_SHOWNA );

    TRACE_(systray)( "icon window %p/%lx\n", hwnd, window );

    /* send the docking request message */
    ev.xclient.type = ClientMessage;
    ev.xclient.window = systray_window;
    ev.xclient.message_type = x11drv_atom( _NET_SYSTEM_TRAY_OPCODE );
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = CurrentTime;
    ev.xclient.data.l[1] = SYSTEM_TRAY_REQUEST_DOCK;
    ev.xclient.data.l[2] = window;
    ev.xclient.data.l[3] = 0;
    ev.xclient.data.l[4] = 0;
    XSendEvent( display, systray_window, False, NoEventMask, &ev );

    return TRUE;
}


/***********************************************************************
 *		X11DRV_get_whole_window
 *
 * Return the X window associated with the full area of a window
 */
Window X11DRV_get_whole_window( HWND hwnd )
{
    struct x11drv_win_data *data = get_win_data( hwnd );
    Window ret;

    if (!data)
    {
        if (hwnd == NtUserGetDesktopWindow()) return root_window;
        return (Window)NtUserGetProp( hwnd, whole_window_prop );
    }
    ret = data->whole_window;
    release_win_data( data );
    return ret;
}


/***********************************************************************
 *		X11DRV_GetDC   (X11DRV.@)
 */
void X11DRV_GetDC( HDC hdc, HWND hwnd, HWND top, const RECT *win_rect,
                   const RECT *top_rect, DWORD flags )
{
    struct x11drv_escape_set_drawable escape;
    HWND parent;

    escape.code = X11DRV_SET_DRAWABLE;
    escape.mode = IncludeInferiors;
    escape.drawable = 0;

    escape.dc_rect.left         = win_rect->left - top_rect->left;
    escape.dc_rect.top          = win_rect->top - top_rect->top;
    escape.dc_rect.right        = win_rect->right - top_rect->left;
    escape.dc_rect.bottom       = win_rect->bottom - top_rect->top;

    if (top == hwnd)
    {
        struct x11drv_win_data *data = get_win_data( hwnd );

        escape.drawable = data ? data->whole_window : X11DRV_get_whole_window( hwnd );

        /* special case: when repainting the root window, clip out top-level windows */
        if (data && data->whole_window == root_window) escape.mode = ClipByChildren;
        release_win_data( data );
    }
    else
    {
        /* find the first ancestor that has a drawable */
        for (parent = hwnd; parent && parent != top; parent = NtUserGetAncestor( parent, GA_PARENT ))
            if ((escape.drawable = X11DRV_get_whole_window( parent ))) break;

        if (escape.drawable)
        {
            POINT pt = { 0, 0 };
            NtUserMapWindowPoints( 0, parent, &pt, 1 );
            escape.dc_rect = *win_rect;
            OffsetRect( &escape.dc_rect, pt.x, pt.y );
            if (flags & DCX_CLIPCHILDREN) escape.mode = ClipByChildren;
        }
        else escape.drawable = X11DRV_get_whole_window( top );
    }

    NtGdiExtEscape( hdc, NULL, 0, X11DRV_ESCAPE, sizeof(escape), (LPSTR)&escape, 0, NULL );
}


/***********************************************************************
 *		X11DRV_ReleaseDC  (X11DRV.@)
 */
void X11DRV_ReleaseDC( HWND hwnd, HDC hdc )
{
    struct x11drv_escape_set_drawable escape;

    escape.code = X11DRV_SET_DRAWABLE;
    escape.drawable = root_window;
    escape.mode = IncludeInferiors;
    escape.dc_rect = NtUserGetVirtualScreenRect();
    OffsetRect( &escape.dc_rect, -2 * escape.dc_rect.left, -2 * escape.dc_rect.top );
    NtGdiExtEscape( hdc, NULL, 0, X11DRV_ESCAPE, sizeof(escape), (LPSTR)&escape, 0, NULL );
}


/*************************************************************************
 *		ScrollDC   (X11DRV.@)
 */
BOOL X11DRV_ScrollDC( HDC hdc, INT dx, INT dy, HRGN update )
{
    RECT rect;
    BOOL ret;
    HRGN expose_rgn = 0;

    NtGdiGetAppClipBox( hdc, &rect );

    if (update)
    {
        INT code = X11DRV_START_EXPOSURES;
        NtGdiExtEscape( hdc, NULL, 0, X11DRV_ESCAPE, sizeof(code), (LPSTR)&code, 0, NULL );

        ret = NtGdiBitBlt( hdc, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
                           hdc, rect.left - dx, rect.top - dy, SRCCOPY, 0, 0 );

        code = X11DRV_END_EXPOSURES;
        NtGdiExtEscape( hdc, NULL, 0, X11DRV_ESCAPE, sizeof(code), (LPSTR)&code,
                        sizeof(expose_rgn), (LPSTR)&expose_rgn );
        if (expose_rgn)
        {
            NtGdiCombineRgn( update, update, expose_rgn, RGN_OR );
            NtGdiDeleteObjectApp( expose_rgn );
        }
    }
    else ret = NtGdiBitBlt( hdc, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
                            hdc, rect.left - dx, rect.top - dy, SRCCOPY, 0, 0 );

    return ret;
}


/***********************************************************************
 *		SetCapture  (X11DRV.@)
 */
void X11DRV_SetCapture( HWND hwnd, UINT flags )
{
    struct x11drv_thread_data *thread_data = x11drv_thread_data();
    struct x11drv_win_data *data;

    if (!(flags & (GUI_INMOVESIZE | GUI_INMENUMODE))) return;

    if (hwnd)
    {
        if (!(data = get_win_data( NtUserGetAncestor( hwnd, GA_ROOT )))) return;
        if (data->whole_window)
        {
            XFlush( gdi_display );
            XGrabPointer( data->display, data->whole_window, False,
                          PointerMotionMask | ButtonPressMask | ButtonReleaseMask,
                          GrabModeAsync, GrabModeAsync, None, None, CurrentTime );
            thread_data->grab_hwnd = data->hwnd;
        }
        release_win_data( data );
    }
    else  /* release capture */
    {
        if (!(data = get_win_data( thread_data->grab_hwnd ))) return;
        XFlush( gdi_display );
        XUngrabPointer( data->display, CurrentTime );
        XFlush( data->display );
        thread_data->grab_hwnd = NULL;
        release_win_data( data );
    }
}


/*****************************************************************
 *		SetParent   (X11DRV.@)
 */
void X11DRV_SetParent( HWND hwnd, HWND parent, HWND old_parent )
{
    struct x11drv_win_data *data;

    if (parent == old_parent) return;
    if (!(data = get_win_data( hwnd ))) return;
    if (data->embedded) goto done;

    if (parent != NtUserGetDesktopWindow()) /* a child window */
    {
        if (old_parent == NtUserGetDesktopWindow())
        {
            /* destroy the old X windows */
            destroy_whole_window( data, FALSE );
            data->managed = FALSE;
        }
        sync_vk_surface( hwnd, TRUE );
    }
    else  /* new top level window */
    {
        create_whole_window( data );
    }
done:
    release_win_data( data );
    set_gl_drawable_parent( hwnd, parent );

    /* Recreate the parent gl_drawable now that we know there are child windows
     * that will need clipping support.
     */
    sync_gl_drawable( parent, TRUE );
    sync_vk_surface( parent, TRUE );

    fetch_icon_data( hwnd, 0, 0 );
}


static inline BOOL get_surface_rect( const RECT *visible_rect, RECT *surface_rect )
{
    *surface_rect = NtUserGetVirtualScreenRect();

    if (!intersect_rect( surface_rect, surface_rect, visible_rect )) return FALSE;
    OffsetRect( surface_rect, -visible_rect->left, -visible_rect->top );
    surface_rect->left &= ~31;
    surface_rect->top  &= ~31;
    surface_rect->right  = max( surface_rect->left + 32, (surface_rect->right + 31) & ~31 );
    surface_rect->bottom = max( surface_rect->top + 32, (surface_rect->bottom + 31) & ~31 );
    return TRUE;
}

static BOOL CALLBACK update_child_window_fshack( HWND hwnd, LPARAM lparam );

static void window_update_fshack( struct x11drv_win_data *data, const RECT *window_rect_virt,
                                  const RECT *client_rect_virt, HMONITOR hmonitor, BOOL enable )
{
    BOOL set_hints = window_rect_virt == NULL; /* don't change hints yet in X11DRV_WindowPosChanging */
    RECT window_rect_host, client_rect_host;

    if (wm_is_steamcompmgr( data->display )) return;
    if (!!data->fs_hack == !!enable) return;
    data->fs_hack = enable;

    if (!window_rect_virt) window_rect_virt = &data->window_rect;
    if (!client_rect_virt) client_rect_virt = &data->client_rect;

    if (!enable)
    {
        window_rect_host = *window_rect_virt;
        client_rect_host = data->client_rect;
        OffsetRect( &client_rect_host, -data->whole_rect.left, -data->whole_rect.top );
    }
    else
    {
        window_rect_host = fs_hack_real_mode( hmonitor );

        if (data->whole_window)  /* HACK: top-level window, pretend client rect covers it fully */
            client_rect_host = window_rect_host;
        else
        {
            client_rect_host = *client_rect_virt;
            NtUserClientToScreen( data->hwnd, (POINT *)&client_rect_host.left );
            NtUserClientToScreen( data->hwnd, (POINT *)&client_rect_host.right );
            fs_hack_rect_user_to_real( &client_rect_host );
            OffsetRect( &client_rect_host, -window_rect_host.left, -window_rect_host.top );
        }
    }

    FIXME( "%sbling fshack for hwnd %p, mapping virt window %s, client %s to host window %s, client %s.\n",
           enable ? "Ena" : "Disa", data->hwnd, wine_dbgstr_rect( window_rect_virt ), wine_dbgstr_rect( client_rect_virt ),
           wine_dbgstr_rect( &window_rect_host ), wine_dbgstr_rect( &client_rect_host ) );

    if (data->whole_window)
    {
        POINT top_left = *(POINT *)&window_rect_host;
        RECT real_virtual;

        OffsetRect( &window_rect_host, -top_left.x, -top_left.y );
        real_virtual = fs_hack_get_real_virtual_screen();
        top_left.x -= real_virtual.left;
        top_left.y -= real_virtual.top;

        if (set_hints) set_wm_hints( data );

        window_rect_host.right = min( max( window_rect_host.right, 1 ), 65535 );
        window_rect_host.bottom = min( max( window_rect_host.bottom, 1 ), 65535 );
        XMoveResizeWindow( data->display, data->whole_window, top_left.x, top_left.y, window_rect_host.right, window_rect_host.bottom );

        if (set_hints) update_net_wm_states( data );
    }

    if (data->client_window)
    {
        POINT top_left = *(POINT *)&client_rect_host;
        OffsetRect( &client_rect_host, -top_left.x, -top_left.y );

        client_rect_host.right = min( max( client_rect_host.right, 1 ), 65535 );
        client_rect_host.bottom = min( max( client_rect_host.bottom, 1 ), 65535 );
        XMoveResizeWindow( gdi_display, data->client_window, top_left.x, top_left.y, client_rect_host.right, client_rect_host.bottom );

        sync_gl_drawable( data->hwnd, !data->whole_window );
        invalidate_vk_surfaces( data->hwnd );
    }

    NtUserEnumChildWindows( data->hwnd, update_child_window_fshack, MAKELONG(hmonitor, enable) );
}

static BOOL CALLBACK update_child_window_fshack( HWND hwnd, LPARAM lparam )
{
    struct x11drv_win_data *data;
    if (!(data = get_win_data( hwnd ))) return TRUE;
    if (data->client_window) window_update_fshack( data, NULL, NULL, UlongToPtr(LOWORD(lparam)), HIWORD(lparam) );
    release_win_data( data );
    return TRUE;
}

/***********************************************************************
 *		WindowPosChanging   (X11DRV.@)
 */
BOOL X11DRV_WindowPosChanging( HWND hwnd, HWND insert_after, UINT swp_flags,
                               const RECT *window_rect, const RECT *client_rect, RECT *visible_rect,
                               struct window_surface **surface )
{
    struct x11drv_win_data *data = get_win_data( hwnd );
    RECT surface_rect;
    DWORD flags;
    COLORREF key;
    BOOL layered = NtUserGetWindowLongW( hwnd, GWL_EXSTYLE ) & WS_EX_LAYERED;
    HMONITOR monitor;

    if (!data && !(data = X11DRV_create_win_data( hwnd, window_rect, client_rect ))) return TRUE;


    monitor = fs_hack_monitor_from_rect( window_rect );
    if (fs_hack_enabled( monitor ) && fs_hack_is_window_rect_fullscreen( monitor, window_rect ))
        window_update_fshack( data, window_rect, client_rect, monitor, TRUE );
    else
        window_update_fshack( data, window_rect, client_rect, monitor, FALSE );

    /* check if we need to switch the window to managed */
    if (!data->managed && data->whole_window && is_window_managed( hwnd, swp_flags, window_rect ))
    {
        TRACE( "making win %p/%lx managed\n", hwnd, data->whole_window );
        release_win_data( data );
        unmap_window( hwnd );
        if (!(data = get_win_data( hwnd ))) return TRUE;
        data->managed = TRUE;
    }

    *visible_rect = *window_rect;
    X11DRV_window_to_X_rect( data, visible_rect, window_rect, client_rect );

    /* create the window surface if necessary */

    if (!data->whole_window && !data->embedded) goto done;
    if (swp_flags & SWP_HIDEWINDOW) goto done;
    if (data->use_alpha) goto done;

    if (wine_vk_direct_window_draw( hwnd ))
    {
        if (*surface) window_surface_release( *surface );
        *surface = NULL;
        goto done;
    }
    if (!get_surface_rect( visible_rect, &surface_rect )) goto done;

    if (*surface) window_surface_release( *surface );
    *surface = NULL;  /* indicate that we want to draw directly to the window */

    if (data->embedded) goto done;
    if (data->whole_window == root_window) goto done;
    if (data->client_window) goto done;
    if (!client_side_graphics && !layered) goto done;

    if (data->surface)
    {
        if (EqualRect( &data->surface->rect, &surface_rect ))
        {
            /* existing surface is good enough */
            window_surface_add_ref( data->surface );
            *surface = data->surface;
            goto done;
        }
    }
    else if (!(swp_flags & SWP_SHOWWINDOW) && !(NtUserGetWindowLongW( hwnd, GWL_STYLE ) & WS_VISIBLE)) goto done;

    if (!layered || !NtUserGetLayeredWindowAttributes( hwnd, &key, NULL, &flags ) || !(flags & LWA_COLORKEY))
        key = CLR_INVALID;

    *surface = create_surface( hwnd, data->whole_window, &data->vis, &surface_rect, key, FALSE );

done:
    release_win_data( data );
    return TRUE;
}

static BOOL option_increament_configure_serial(void)
{
    static int increment = -1;
    if (increment == -1)
    {
        const char *e = getenv( "WINE_INCREMENT_CONFIGURE_SERIAL" );

        if (e)
            increment = atoi( e );
        else
            increment = (e = getenv( "SteamGameId" )) && !strcmp( e, "1689910" );
    }
    return increment;
}

static void restack_windows( struct x11drv_win_data *data, HWND prev )
{
    struct x11drv_win_data *prev_data;

    TRACE("data->hwnd %p, prev %p.\n", data->hwnd, prev);

    while (prev)
    {
        if (!(prev_data = get_win_data( prev ))) break;

        TRACE( "Raising window %p.\n", prev );

        if (prev_data->whole_window && data->display == prev_data->display)
            XRaiseWindow( prev_data->display, prev_data->whole_window );
        release_win_data( prev_data );
        prev = NtUserGetWindowRelative( prev, GW_HWNDPREV );
    }
}

/***********************************************************************
 *		WindowPosChanged   (X11DRV.@)
 */
void X11DRV_WindowPosChanged( HWND hwnd, HWND insert_after, UINT swp_flags,
                              const RECT *rectWindow, const RECT *rectClient,
                              const RECT *visible_rect, const RECT *valid_rects,
                              struct window_surface *surface )
{
    struct x11drv_thread_data *thread_data;
    struct x11drv_win_data *data;
    UINT new_style = NtUserGetWindowLongW( hwnd, GWL_STYLE );
    RECT old_window_rect, old_whole_rect, old_client_rect;
    HWND prev_window = NULL;
    BOOL needs_resize;
    int event_type;

    if (!(data = get_win_data( hwnd ))) return;

    thread_data = x11drv_thread_data();

    old_window_rect = data->window_rect;
    old_whole_rect  = data->whole_rect;
    old_client_rect = data->client_rect;
    data->window_rect = *rectWindow;
    data->whole_rect  = *visible_rect;
    data->client_rect = *rectClient;
    if (data->vis.visualid == default_visual.visualid)
    {
        if (surface) window_surface_add_ref( surface );
        if (data->surface) window_surface_release( data->surface );
        data->surface = surface;
    }

    TRACE( "win %p window %s client %s style %08x flags %08x\n",
           hwnd, wine_dbgstr_rect(rectWindow), wine_dbgstr_rect(rectClient), new_style, swp_flags );

    if (!IsRectEmpty( &valid_rects[0] ))
    {
        Window window = data->whole_window;
        int x_offset = old_whole_rect.left - data->whole_rect.left;
        int y_offset = old_whole_rect.top - data->whole_rect.top;

        /* if all that happened is that the whole window moved, copy everything */
        if (!(swp_flags & SWP_FRAMECHANGED) &&
            old_whole_rect.right   - data->whole_rect.right   == x_offset &&
            old_whole_rect.bottom  - data->whole_rect.bottom  == y_offset &&
            old_client_rect.left   - data->client_rect.left   == x_offset &&
            old_client_rect.right  - data->client_rect.right  == x_offset &&
            old_client_rect.top    - data->client_rect.top    == y_offset &&
            old_client_rect.bottom - data->client_rect.bottom == y_offset &&
            EqualRect( &valid_rects[0], &data->client_rect ))
        {
            /* if we have an X window the bits will be moved by the X server */
            if (!window && (x_offset != 0 || y_offset != 0))
            {
                release_win_data( data );
                move_window_bits( hwnd, window, &old_whole_rect, visible_rect,
                                  &old_client_rect, rectClient, rectWindow );
                if (!(data = get_win_data( hwnd ))) return;
            }
        }
        else
        {
            release_win_data( data );
            move_window_bits( hwnd, window, &valid_rects[1], &valid_rects[0],
                              &old_client_rect, rectClient, rectWindow );
            if (!(data = get_win_data( hwnd ))) return;
        }
    }

    XFlush( gdi_display );  /* make sure painting is done before we move the window */

    sync_client_position( data, &old_client_rect, &old_whole_rect );

    needs_resize = !data->client_window && (data->client_rect.right - data->client_rect.left !=
                   old_client_rect.right - old_client_rect.left ||
                   data->client_rect.bottom - data->client_rect.top !=
                   old_client_rect.bottom - old_client_rect.top);

    if (!data->whole_window)
    {
        release_win_data( data );
        if (needs_resize) sync_gl_drawable( hwnd, FALSE );
        return;
    }

    set_hwnd_style_props( data->display, data->whole_window, data->hwnd );

    if (data->fs_hack) needs_resize = TRUE;

    /* check if we are currently processing an event relevant to this window */
    event_type = 0;
    if (thread_data &&
        thread_data->current_event &&
        thread_data->current_event->xany.window == data->whole_window)
    {
        event_type = thread_data->current_event->type;
        if (event_type != ConfigureNotify && event_type != PropertyNotify &&
            event_type != GravityNotify && event_type != ReparentNotify)
            event_type = 0;  /* ignore other events */
    }

    if (data->mapped && event_type != ReparentNotify)
    {
        if (((swp_flags & SWP_HIDEWINDOW) && !(new_style & WS_VISIBLE)) ||
            (!event_type && !(new_style & WS_MINIMIZE) &&
             !is_window_rect_mapped( rectWindow ) && is_window_rect_mapped( &old_window_rect )))
        {
            release_win_data( data );
            unmap_window( hwnd );
            if (NtUserIsWindowRectFullScreen( &old_window_rect )) NtUserClipCursor( NULL );
            if (!(data = get_win_data( hwnd ))) return;
        }
    }

    /* don't change position if we are about to minimize or maximize a managed window */
    if (!event_type || event_type == PropertyNotify)
    {
        if (!(data->managed && (swp_flags & SWP_STATECHANGED) && (new_style & (WS_MINIMIZE|WS_MAXIMIZE)))
             || (!(new_style & WS_MINIMIZE) && wm_is_steamcompmgr( data->display )))
            prev_window = sync_window_position( data, swp_flags, &old_window_rect, &old_whole_rect, &old_client_rect );
        else if (option_increament_configure_serial())
            data->configure_serial = NextRequest( data->display );
    }

    if ((new_style & WS_VISIBLE) &&
        ((new_style & WS_MINIMIZE) || is_window_rect_mapped( rectWindow )))
    {
        if (!data->mapped)
        {
            BOOL needs_icon = !data->icon_pixmap;
            BOOL needs_map = TRUE;

            /* layered windows are mapped only once their attributes are set */
            if (NtUserGetWindowLongW( hwnd, GWL_EXSTYLE ) & WS_EX_LAYERED)
                needs_map = data->layered || IsRectEmpty( rectWindow );
            release_win_data( data );
            if (needs_icon) fetch_icon_data( hwnd, 0, 0 );
            if (needs_map) map_window( hwnd, new_style );

            if (!(data = get_win_data( hwnd ))) return;
            restack_windows( data, prev_window );
            release_win_data( data );
            return;
        }
        else if ((swp_flags & SWP_STATECHANGED) && (!data->iconic != !(new_style & WS_MINIMIZE)))
        {
            set_wm_hints( data );
            data->iconic = (new_style & WS_MINIMIZE) != 0;
            TRACE( "changing win %p iconic state to %u\n", data->hwnd, data->iconic );
            if (data->iconic)
            {
                if (!skip_iconify( data->display ))
                {
                    /* XIconifyWindow is essentially a no-op on Gamescope but has a side effect.
                     * Gamescope handles wm state change to iconic and immediately changes it back to normal.
                     * Upon that change back we would receive WM_STATE change notification and kick the window
                     * out of minimized state even if the window is not focused by Gamescope (possibly breaking Win-side
                     * focus and leading to hangs).
                     * Depending on what the game is going, various things may happen if we avoid that:
                     *  - Gamescope may change WM_STATE later when focusing our window and we will get the window out of minimized state correctly;
                     *  - The game might have no other windows and it just decided to minimize itself
                     *    (e. g., before opening Web browser), expecting the user to unminimize it manually which
                     *    is not possible on Gamescope. Ideally we'd have a way to detect such a case and unminimize
                     *    when needed, but without that just let Gamescope unminimize immediately avoiding that for
                     *    selected game(s) only. */
                    XIconifyWindow( data->display, data->whole_window, data->vis.screen );
                }
            }
            else if (is_window_rect_mapped( rectWindow ))
            {
                /* whole_window could be both iconic and mapped. Since XMapWindow() doesn't do
                 * anything if the window is already mapped, we need to unmap it first */
                if (data->mapped)
                {
                    if (wm_is_steamcompmgr( data->display ))
                    {
                        /* Gamescope will generate FocusOut event upon processing UnmapNotify. Ignore it. */
                        data->fake_unmap_serial = NextRequest( data->display );
                    }
                    XUnmapWindow( data->display, data->whole_window );
                }
                XMapWindow( data->display, data->whole_window );
            }
            update_net_wm_states( data );
        }
        else
        {
            if (swp_flags & (SWP_FRAMECHANGED|SWP_STATECHANGED)) set_wm_hints( data );
            if (!event_type || event_type == PropertyNotify)
            {
                update_net_wm_states( data );
                if (!prev_window && insert_after && data->net_wm_state & (1 << NET_WM_STATE_FULLSCREEN))
                {
                    prev_window = NtUserGetWindowRelative( hwnd, GW_HWNDPREV );
                    if (prev_window != insert_after) prev_window = NULL;
                }
            }
        }
    }

    restack_windows( data, prev_window );

    XFlush( data->display );  /* make sure changes are done before we start painting again */
    if (data->surface && data->vis.visualid != default_visual.visualid)
        data->surface->funcs->flush( data->surface );

    release_win_data( data );
    if (needs_resize) sync_gl_drawable( hwnd, FALSE );
}

/* check if the window icon should be hidden (i.e. moved off-screen) */
static BOOL hide_icon( struct x11drv_win_data *data )
{
    static const WCHAR trayW[] = {'S','h','e','l','l','_','T','r','a','y','W','n','d',0};
    UNICODE_STRING str = RTL_CONSTANT_STRING( trayW );

    if (data->managed) return TRUE;
    /* hide icons in desktop mode when the taskbar is active */
    if (!is_virtual_desktop()) return FALSE;
    return NtUserIsWindowVisible( NtUserFindWindowEx( 0, 0, &str, NULL, 0 ));
}

/***********************************************************************
 *           ShowWindow   (X11DRV.@)
 */
UINT X11DRV_ShowWindow( HWND hwnd, INT cmd, RECT *rect, UINT swp )
{
    int x, y;
    unsigned int width, height, border, depth;
    Window root, top;
    POINT pos;
    DWORD style = NtUserGetWindowLongW( hwnd, GWL_STYLE );
    struct x11drv_thread_data *thread_data = x11drv_thread_data();
    struct x11drv_win_data *data = get_win_data( hwnd );
    HMONITOR monitor;

    if (!data || !data->whole_window) goto done;
    if (style & WS_MINIMIZE)
    {
        if (((rect->left != -32000 || rect->top != -32000)) && hide_icon( data ))
        {
            OffsetRect( rect, -32000 - rect->left, -32000 - rect->top );
            swp &= ~(SWP_NOMOVE | SWP_NOCLIENTMOVE);
        }
        goto done;
    }
    if (!data->managed || !data->mapped || data->iconic) goto done;

    /* only fetch the new rectangle if the ShowWindow was a result of a window manager event */

    if (!thread_data->current_event || thread_data->current_event->xany.window != data->whole_window)
        goto done;

    if (thread_data->current_event->type != ConfigureNotify &&
        thread_data->current_event->type != PropertyNotify)
        goto done;

    TRACE( "win %p/%lx cmd %d at %s flags %08x\n",
           hwnd, data->whole_window, cmd, wine_dbgstr_rect(rect), swp );

    XGetGeometry( thread_data->display, data->whole_window,
                  &root, &x, &y, &width, &height, &border, &depth );
    XTranslateCoordinates( thread_data->display, data->whole_window, root, 0, 0, &x, &y, &top );
    pos = root_to_virtual_screen( x, y );
    monitor = fs_hack_monitor_from_rect( rect );
    if (data->fs_hack ||
        (fs_hack_enabled( monitor ) &&
         fs_hack_is_window_rect_fullscreen( monitor, rect )))
    {
        MONITORINFO info = {.cbSize = sizeof(MONITORINFO)};
        NtUserGetMonitorInfo( monitor, &info );
        X11DRV_X_to_window_rect( data, rect, info.rcMonitor.left, info.rcMonitor.top,
                                 info.rcMonitor.right - info.rcMonitor.left,
                                 info.rcMonitor.bottom - info.rcMonitor.top );
    }
    else
    {
        X11DRV_X_to_window_rect( data, rect, pos.x, pos.y, width, height );
    }
    swp &= ~(SWP_NOMOVE | SWP_NOCLIENTMOVE | SWP_NOSIZE | SWP_NOCLIENTSIZE);

done:
    release_win_data( data );
    return swp;
}


/**********************************************************************
 *		SetWindowIcon (X11DRV.@)
 *
 * hIcon or hIconSm has changed (or is being initialised for the
 * first time). Complete the X11 driver-specific initialisation
 * and set the window hints.
 */
void X11DRV_SetWindowIcon( HWND hwnd, UINT type, HICON icon )
{
    struct x11drv_win_data *data;

    if (!(data = get_win_data( hwnd ))) return;
    if (!data->whole_window) goto done;
    release_win_data( data );  /* release the lock, fetching the icon requires sending messages */

    if (type == ICON_BIG) fetch_icon_data( hwnd, icon, 0 );
    else fetch_icon_data( hwnd, 0, icon );

    if (!(data = get_win_data( hwnd ))) return;
    set_wm_hints( data );
done:
    release_win_data( data );
}


/***********************************************************************
 *		SetWindowRgn  (X11DRV.@)
 *
 * Assign specified region to window (for non-rectangular windows)
 */
void X11DRV_SetWindowRgn( HWND hwnd, HRGN hrgn, BOOL redraw )
{
    struct x11drv_win_data *data;

    if ((data = get_win_data( hwnd )))
    {
        sync_window_region( data, hrgn );
        release_win_data( data );
    }
    else if (X11DRV_get_whole_window( hwnd ))
    {
        send_message( hwnd, WM_X11DRV_SET_WIN_REGION, 0, 0 );
    }
}


/***********************************************************************
 *		SetLayeredWindowAttributes  (X11DRV.@)
 *
 * Set transparency attributes for a layered window.
 */
void X11DRV_SetLayeredWindowAttributes( HWND hwnd, COLORREF key, BYTE alpha, DWORD flags )
{
    struct x11drv_win_data *data = get_win_data( hwnd );
    BOOL need_sync_gl;

    if (data)
    {
        set_window_visual( data, &default_visual, FALSE );

        if (data->whole_window)
            sync_window_opacity( data->display, data->whole_window, key, alpha, flags );
        if (data->surface)
            set_surface_color_key( data->surface, (flags & LWA_COLORKEY) ? key : CLR_INVALID );

        need_sync_gl = !data->layered || !data->layered_attributes;
        data->layered = TRUE;
        data->layered_attributes = TRUE;
        if (!data->mapped)  /* mapping is delayed until attributes are set */
        {
            DWORD style = NtUserGetWindowLongW( data->hwnd, GWL_STYLE );

            if ((style & WS_VISIBLE) &&
                ((style & WS_MINIMIZE) || is_window_rect_mapped( &data->window_rect )))
            {
                release_win_data( data );
                map_window( hwnd, style );
                if (need_sync_gl) sync_gl_drawable( hwnd, FALSE );
                return;
            }
        }
        release_win_data( data );
        if (need_sync_gl) sync_gl_drawable( hwnd, FALSE );
    }
    else
    {
        Window win = X11DRV_get_whole_window( hwnd );
        if (win)
        {
            sync_window_opacity( gdi_display, win, key, alpha, flags );
            if (flags & LWA_COLORKEY)
                FIXME( "LWA_COLORKEY not supported on foreign process window %p\n", hwnd );
        }
    }
}


/*****************************************************************************
 *              UpdateLayeredWindow  (X11DRV.@)
 */
BOOL X11DRV_UpdateLayeredWindow( HWND hwnd, const UPDATELAYEREDWINDOWINFO *info,
                                 const RECT *window_rect )
{
    struct window_surface *surface;
    struct x11drv_win_data *data;
    BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, 0 };
    COLORREF color_key = (info->dwFlags & ULW_COLORKEY) ? info->crKey : CLR_INVALID;
    char buffer[FIELD_OFFSET( BITMAPINFO, bmiColors[256] )];
    BITMAPINFO *bmi = (BITMAPINFO *)buffer;
    void *src_bits, *dst_bits;
    RECT rect, src_rect;
    BOOL need_sync_gl;
    HDC hdc = 0;
    HBITMAP dib;
    BOOL mapped, ret = FALSE;

    if (!(data = get_win_data( hwnd ))) return FALSE;

    need_sync_gl = !data->layered;
    data->layered = TRUE;
    if (!data->embedded && argb_visual.visualid) set_window_visual( data, &argb_visual, TRUE );

    rect = *window_rect;
    OffsetRect( &rect, -window_rect->left, -window_rect->top );

    surface = data->surface;
    if (!surface || !EqualRect( &surface->rect, &rect ))
    {
        data->surface = create_surface( hwnd, data->whole_window, &data->vis, &rect,
                                        color_key, data->use_alpha );
        if (surface) window_surface_release( surface );
        surface = data->surface;
    }
    else set_surface_color_key( surface, color_key );

    if (surface) window_surface_add_ref( surface );
    mapped = data->mapped;
    release_win_data( data );

    /* layered windows are mapped only once their attributes are set */
    if (!mapped)
    {
        DWORD style = NtUserGetWindowLongW( hwnd, GWL_STYLE );

        if ((style & WS_VISIBLE) && ((style & WS_MINIMIZE) || is_window_rect_mapped( window_rect )))
            map_window( hwnd, style );
    }

    if (need_sync_gl) sync_gl_drawable( hwnd, FALSE );

    if (!surface) return FALSE;
    if (!info->hdcSrc)
    {
        window_surface_release( surface );
        return TRUE;
    }

    dst_bits = surface->funcs->get_info( surface, bmi );

    if (!(dib = NtGdiCreateDIBSection( info->hdcDst, NULL, 0, bmi, DIB_RGB_COLORS, 0, 0, 0, &src_bits )))
        goto done;
    if (!(hdc = NtGdiCreateCompatibleDC( 0 ))) goto done;

    NtGdiSelectBitmap( hdc, dib );

    surface->funcs->lock( surface );

    if (info->prcDirty)
    {
        intersect_rect( &rect, &rect, info->prcDirty );
        memcpy( src_bits, dst_bits, bmi->bmiHeader.biSizeImage );
        NtGdiPatBlt( hdc, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, BLACKNESS );
    }
    src_rect = rect;
    if (info->pptSrc) OffsetRect( &src_rect, info->pptSrc->x, info->pptSrc->y );
    NtGdiTransformPoints( info->hdcSrc, (POINT *)&src_rect, (POINT *)&src_rect, 2, NtGdiDPtoLP );

    if (info->dwFlags & ULW_ALPHA) blend = *info->pblend;
    ret = NtGdiAlphaBlend( hdc, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
                           info->hdcSrc, src_rect.left, src_rect.top,
                           src_rect.right - src_rect.left, src_rect.bottom - src_rect.top,
                           *(DWORD *)&blend, 0 );
    if (ret)
    {
        memcpy( dst_bits, src_bits, bmi->bmiHeader.biSizeImage );
        add_bounds_rect( surface->funcs->get_bounds( surface ), &rect );
    }

    surface->funcs->unlock( surface );
    surface->funcs->flush( surface );

done:
    window_surface_release( surface );
    if (hdc) NtGdiDeleteObjectApp( hdc );
    if (dib) NtGdiDeleteObjectApp( dib );
    return ret;
}

/* Add a window to taskbar */
static void taskbar_add_tab( HWND hwnd )
{
    struct x11drv_win_data *data;

    TRACE("hwnd %p\n", hwnd);

    data = get_win_data( hwnd );
    if (!data)
        return;

    data->add_taskbar = TRUE;
    data->skip_taskbar = FALSE;
    update_net_wm_states( data );
    release_win_data( data );
}

/* Delete a window from taskbar */
static void taskbar_delete_tab( HWND hwnd )
{
    struct x11drv_win_data *data;

    TRACE("hwnd %p\n", hwnd);

    data = get_win_data( hwnd );
    if (!data)
        return;

    data->skip_taskbar = TRUE;
    data->add_taskbar = FALSE;
    update_net_wm_states( data );
    release_win_data( data );
}

static void handle_window_desktop_resize( struct x11drv_win_data *data, UINT old_x, UINT old_y )
{
    HMONITOR monitor = fs_hack_monitor_from_hwnd( data->hwnd );

    if (fs_hack_mapping_required( monitor ) &&
        fs_hack_is_window_rect_fullscreen( monitor, &data->whole_rect ))
    {
        window_update_fshack( data, NULL, NULL, monitor, TRUE );
        return;
    }

    /* update the full screen state */
    update_net_wm_states( data );

    if (data->whole_window)
    {
        /* sync window position with the new virtual screen rect */
        POINT old_pos = {.x = data->whole_rect.left - old_x, .y = data->whole_rect.top - old_y};
        POINT pos = virtual_screen_to_root( data->whole_rect.left, data->whole_rect.top );
        XWindowChanges changes = {.x = pos.x, .y = pos.y};
        UINT mask = 0;

        if (old_pos.x != pos.x) mask |= CWX;
        if (old_pos.y != pos.y) mask |= CWY;
        if (data->fs_hack) mask |= CWX | CWY;

        if (mask) XReconfigureWMWindow( data->display, data->whole_window, data->vis.screen, mask, &changes );
    }

    if (!fs_hack_mapping_required( monitor ) ||
        !fs_hack_is_window_rect_fullscreen( monitor, &data->whole_rect ))
        window_update_fshack( data, NULL, NULL, monitor, FALSE );
}

/**********************************************************************
 *           X11DRV_WindowMessage   (X11DRV.@)
 */
LRESULT X11DRV_WindowMessage( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
    struct x11drv_win_data *data;

    switch(msg)
    {
    case WM_X11DRV_UPDATE_CLIPBOARD:
        return update_clipboard( hwnd );
    case WM_X11DRV_SET_WIN_REGION:
        if ((data = get_win_data( hwnd )))
        {
            sync_window_region( data, (HRGN)1 );
            release_win_data( data );
        }
        return 0;
    case WM_X11DRV_DESKTOP_RESIZED:
        if ((data = get_win_data( hwnd )))
        {
            handle_window_desktop_resize( data, wp, lp );
            release_win_data( data );
        }
        return 0;
    case WM_X11DRV_DELETE_TAB:
        taskbar_delete_tab( hwnd );
        return 0;
    case WM_X11DRV_ADD_TAB:
        taskbar_add_tab( hwnd );
        return 0;
    default:
        FIXME( "got window msg %x hwnd %p wp %lx lp %lx\n", msg, hwnd, (long)wp, lp );
        return 0;
    }
}


/***********************************************************************
 *              is_netwm_supported
 */
static BOOL is_netwm_supported( Display *display, Atom atom )
{
    static Atom *net_supported;
    static int net_supported_count = -1;
    int i;

    if (net_supported_count == -1)
    {
        Atom type;
        int format;
        unsigned long count, remaining;

        if (!XGetWindowProperty( display, DefaultRootWindow(display), x11drv_atom(_NET_SUPPORTED), 0,
                                 ~0UL, False, XA_ATOM, &type, &format, &count,
                                 &remaining, (unsigned char **)&net_supported ))
            net_supported_count = get_property_size( format, count ) / sizeof(Atom);
        else
            net_supported_count = 0;
    }

    for (i = 0; i < net_supported_count; i++)
        if (net_supported[i] == atom) return TRUE;
    return FALSE;
}


/***********************************************************************
 *              start_screensaver
 */
static LRESULT start_screensaver(void)
{
    if (!is_virtual_desktop())
    {
        const char *argv[3] = { "xdg-screensaver", "activate", NULL };
        int pid = __wine_unix_spawnvp( (char **)argv, FALSE );
        if (pid > 0)
        {
            TRACE( "started process %d\n", pid );
            return 0;
        }
    }
    return -1;
}


/***********************************************************************
 *           SysCommand   (X11DRV.@)
 *
 * Perform WM_SYSCOMMAND handling.
 */
LRESULT X11DRV_SysCommand( HWND hwnd, WPARAM wparam, LPARAM lparam )
{
    WPARAM hittest = wparam & 0x0f;
    int dir;
    struct x11drv_win_data *data;

    if (!(data = get_win_data( hwnd )))
    {
        if (wparam == SC_SCREENSAVE && hwnd == NtUserGetDesktopWindow()) return start_screensaver();
        return -1;
    }
    if (!data->whole_window || !data->managed || !data->mapped) goto failed;

    switch (wparam & 0xfff0)
    {
    case SC_MOVE:
        if (!hittest) dir = _NET_WM_MOVERESIZE_MOVE_KEYBOARD;
        else dir = _NET_WM_MOVERESIZE_MOVE;
        break;
    case SC_SIZE:
        /* windows without WS_THICKFRAME are not resizable through the window manager */
        if (!(NtUserGetWindowLongW( hwnd, GWL_STYLE ) & WS_THICKFRAME)) goto failed;

        switch (hittest)
        {
        case WMSZ_LEFT:        dir = _NET_WM_MOVERESIZE_SIZE_LEFT; break;
        case WMSZ_RIGHT:       dir = _NET_WM_MOVERESIZE_SIZE_RIGHT; break;
        case WMSZ_TOP:         dir = _NET_WM_MOVERESIZE_SIZE_TOP; break;
        case WMSZ_TOPLEFT:     dir = _NET_WM_MOVERESIZE_SIZE_TOPLEFT; break;
        case WMSZ_TOPRIGHT:    dir = _NET_WM_MOVERESIZE_SIZE_TOPRIGHT; break;
        case WMSZ_BOTTOM:      dir = _NET_WM_MOVERESIZE_SIZE_BOTTOM; break;
        case WMSZ_BOTTOMLEFT:  dir = _NET_WM_MOVERESIZE_SIZE_BOTTOMLEFT; break;
        case WMSZ_BOTTOMRIGHT: dir = _NET_WM_MOVERESIZE_SIZE_BOTTOMRIGHT; break;
        case 9:                dir = _NET_WM_MOVERESIZE_MOVE; break;
        default:               dir = _NET_WM_MOVERESIZE_SIZE_KEYBOARD; break;
        }
        break;

    case SC_KEYMENU:
        /* prevent a simple ALT press+release from activating the system menu,
         * as that can get confusing on managed windows */
        if ((WCHAR)lparam) goto failed;  /* got an explicit char */
        if (NtUserGetWindowLongPtrW( hwnd, GWLP_ID )) goto failed;  /* window has a real menu */
        if (!(NtUserGetWindowLongW( hwnd, GWL_STYLE ) & WS_SYSMENU)) goto failed;  /* no system menu */
        TRACE( "ignoring SC_KEYMENU wp %lx lp %lx\n", (long)wparam, lparam );
        release_win_data( data );
        return 0;

    default:
        goto failed;
    }

    if (NtUserGetWindowLongW( hwnd, GWL_STYLE ) & WS_MAXIMIZE) goto failed;

    if (!is_netwm_supported( data->display, x11drv_atom(_NET_WM_MOVERESIZE) ))
    {
        TRACE( "_NET_WM_MOVERESIZE not supported\n" );
        goto failed;
    }

    release_win_data( data );
    move_resize_window( hwnd, dir );
    return 0;

failed:
    release_win_data( data );
    return -1;
}

void X11DRV_FlashWindowEx( FLASHWINFO *pfinfo )
{
    struct x11drv_win_data *data = get_win_data( pfinfo->hwnd );
    XEvent xev;

    if (!data)
        return;

    if (data->mapped)
    {
        xev.type = ClientMessage;
        xev.xclient.window = data->whole_window;
        xev.xclient.message_type = x11drv_atom( _NET_WM_STATE );
        xev.xclient.serial = 0;
        xev.xclient.display = data->display;
        xev.xclient.send_event = True;
        xev.xclient.format = 32;
        xev.xclient.data.l[0] = pfinfo->dwFlags ?  _NET_WM_STATE_ADD : _NET_WM_STATE_REMOVE;
        xev.xclient.data.l[1] = x11drv_atom( _NET_WM_STATE_DEMANDS_ATTENTION );
        xev.xclient.data.l[2] = 0;
        xev.xclient.data.l[3] = 1;
        xev.xclient.data.l[4] = 0;

        XSendEvent( data->display, DefaultRootWindow( data->display ), False,
                    SubstructureNotifyMask, &xev );
    }
    release_win_data( data );
}

void init_win_context(void)
{
    init_recursive_mutex( &win_data_mutex );

    winContext = XUniqueContext();
    win_data_context = XUniqueContext();
    cursor_context = XUniqueContext();
}
