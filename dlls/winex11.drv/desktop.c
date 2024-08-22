/*
 * X11DRV desktop window handling
 *
 * Copyright 2001 Alexandre Julliard
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
#include <X11/cursorfont.h>
#include <X11/Xlib.h>

#include "x11drv.h"

/* avoid conflict with field names in included win32 headers */
#undef Status
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(x11drv);

static RECT *host_monitor_rects;
static int host_monitor_rect_count;

#define _NET_WM_STATE_REMOVE 0
#define _NET_WM_STATE_ADD 1

/* Return TRUE if Wine is currently in virtual desktop mode */
BOOL is_virtual_desktop(void)
{
    return root_window != DefaultRootWindow( gdi_display );
}

/***********************************************************************
 *		X11DRV_init_desktop
 *
 * Setup the desktop when not using the root window.
 */
void X11DRV_init_desktop( Window win )
{
    if (host_monitor_rects) free( host_monitor_rects );
    if (!get_host_monitor_rects( &host_monitor_rects, &host_monitor_rect_count ))
        ERR("Failed to get host monitor rectangle.\n");
    root_window = win;
    managed_mode = FALSE;  /* no managed windows in desktop mode */
    fs_hack_disable();
}

/***********************************************************************
 *           X11DRV_CreateDesktop
 *
 * Create the X11 desktop window for the desktop mode.
 */
BOOL X11DRV_CreateDesktop( const WCHAR *name, UINT width, UINT height )
{
    XSetWindowAttributes win_attr;
    Window win;
    Display *display = thread_init_display();

    TRACE( "%s %ux%u\n", debugstr_w(name), width, height );

    /* Create window */
    win_attr.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask | EnterWindowMask |
                          PointerMotionMask | ButtonPressMask | ButtonReleaseMask | FocusChangeMask;
    win_attr.cursor = XCreateFontCursor( display, XC_top_left_arrow );

    if (default_visual.visual != DefaultVisual( display, DefaultScreen(display) ))
        win_attr.colormap = XCreateColormap( display, DefaultRootWindow(display),
                                             default_visual.visual, AllocNone );
    else
        win_attr.colormap = None;

    win = XCreateWindow( display, DefaultRootWindow(display),
                         0, 0, width, height, 0, default_visual.depth, InputOutput,
                         default_visual.visual, CWEventMask | CWCursor | CWColormap, &win_attr );
    if (!win) return FALSE;
    X11DRV_XInput2_Enable( display, win, win_attr.event_mask );
    XFlush( display );

    X11DRV_init_desktop( win );
    return TRUE;
}

BOOL is_desktop_fullscreen(void)
{
    Display *display = thread_display();
    unsigned int width, height, border, depth;
    int x, y, i;
    Window root;
    RECT rect;

    XGetGeometry( display, root_window, &root, &x, &y, &width, &height, &border, &depth );
    SetRect( &rect, x, y, x + width, y + height );

    for (i = 0; i < host_monitor_rect_count; i++)
        if (EqualRect( &host_monitor_rects[i], &rect) ) return TRUE;

    return FALSE;
}

/***********************************************************************
 *		X11DRV_resize_desktop
 */
void X11DRV_resize_desktop(void)
{
    static RECT old_virtual_rect;

    RECT virtual_rect = NtUserGetVirtualScreenRect();
    HWND hwnd = NtUserGetDesktopWindow();
    INT width = virtual_rect.right - virtual_rect.left, height = virtual_rect.bottom - virtual_rect.top;

    TRACE( "desktop %p change to (%dx%d)\n", hwnd, width, height );
    NtUserSetWindowPos( hwnd, 0, virtual_rect.left, virtual_rect.top, width, height,
                        SWP_NOZORDER | SWP_NOACTIVATE | SWP_DEFERERASE );

    /* HACK: always send the desktop resize notification, to eventually update fshack on windows */
    send_message_timeout( HWND_BROADCAST, WM_X11DRV_DESKTOP_RESIZED, old_virtual_rect.left,
                          old_virtual_rect.top, SMTO_ABORTIFHUNG, 2000, FALSE );

    old_virtual_rect = virtual_rect;
}
