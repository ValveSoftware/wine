/*
 * X11 mouse driver
 *
 * Copyright 1998 Ulrich Weigand
 * Copyright 2007 Henri Verbeet
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
#include "wine/port.h"

#include <X11/Xlib.h>
#include <X11/cursorfont.h>
#include <stdarg.h>
#ifdef HAVE_X11_EXTENSIONS_XINPUT2_H
#include <X11/extensions/XInput2.h>
#endif

#ifdef SONAME_LIBXCURSOR
# include <X11/Xcursor/Xcursor.h>
static void *xcursor_handle;
# define MAKE_FUNCPTR(f) static typeof(f) * p##f
MAKE_FUNCPTR(XcursorImageCreate);
MAKE_FUNCPTR(XcursorImageDestroy);
MAKE_FUNCPTR(XcursorImageLoadCursor);
MAKE_FUNCPTR(XcursorImagesCreate);
MAKE_FUNCPTR(XcursorImagesDestroy);
MAKE_FUNCPTR(XcursorImagesLoadCursor);
MAKE_FUNCPTR(XcursorLibraryLoadCursor);
# undef MAKE_FUNCPTR
#endif /* SONAME_LIBXCURSOR */

#define NONAMELESSUNION
#define OEMRESOURCE
#include "windef.h"
#include "winbase.h"
#include "winreg.h"

#include "x11drv.h"
#include "wine/server.h"
#include "wine/library.h"
#include "wine/unicode.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(cursor);

/**********************************************************************/

#ifndef Button6Mask
#define Button6Mask (1<<13)
#endif
#ifndef Button7Mask
#define Button7Mask (1<<14)
#endif

#define NB_BUTTONS   9     /* Windows can handle 5 buttons and the wheel too */

static const UINT button_down_flags[NB_BUTTONS] =
{
    MOUSEEVENTF_LEFTDOWN,
    MOUSEEVENTF_MIDDLEDOWN,
    MOUSEEVENTF_RIGHTDOWN,
    MOUSEEVENTF_WHEEL,
    MOUSEEVENTF_WHEEL,
    MOUSEEVENTF_XDOWN,  /* FIXME: horizontal wheel */
    MOUSEEVENTF_XDOWN,
    MOUSEEVENTF_XDOWN,
    MOUSEEVENTF_XDOWN
};

static const UINT button_up_flags[NB_BUTTONS] =
{
    MOUSEEVENTF_LEFTUP,
    MOUSEEVENTF_MIDDLEUP,
    MOUSEEVENTF_RIGHTUP,
    0,
    0,
    MOUSEEVENTF_XUP,
    MOUSEEVENTF_XUP,
    MOUSEEVENTF_XUP,
    MOUSEEVENTF_XUP
};

static const UINT button_down_data[NB_BUTTONS] =
{
    0,
    0,
    0,
    WHEEL_DELTA,
    -WHEEL_DELTA,
    XBUTTON1,
    XBUTTON2,
    XBUTTON1,
    XBUTTON2
};

static const UINT button_up_data[NB_BUTTONS] =
{
    0,
    0,
    0,
    0,
    0,
    XBUTTON1,
    XBUTTON2,
    XBUTTON1,
    XBUTTON2
};

XContext cursor_context = 0;

static HWND cursor_window;
static HCURSOR last_cursor;
static DWORD last_cursor_change;
static RECT clip_rect;
static Cursor create_cursor( HANDLE handle );

#ifdef HAVE_X11_EXTENSIONS_XINPUT2_H
static BOOL xinput2_available;
static BOOL broken_rawevents;
#define MAKE_FUNCPTR(f) static typeof(f) * p##f
MAKE_FUNCPTR(XIGetClientPointer);
MAKE_FUNCPTR(XIFreeDeviceInfo);
MAKE_FUNCPTR(XIQueryDevice);
MAKE_FUNCPTR(XIQueryVersion);
MAKE_FUNCPTR(XISelectEvents);
#undef MAKE_FUNCPTR
#endif

/***********************************************************************
 *		X11DRV_Xcursor_Init
 *
 * Load the Xcursor library for use.
 */
void X11DRV_Xcursor_Init(void)
{
#ifdef SONAME_LIBXCURSOR
    xcursor_handle = wine_dlopen(SONAME_LIBXCURSOR, RTLD_NOW, NULL, 0);
    if (!xcursor_handle)  /* wine_dlopen failed. */
    {
        WARN("Xcursor failed to load.  Using fallback code.\n");
        return;
    }
#define LOAD_FUNCPTR(f) \
        p##f = wine_dlsym(xcursor_handle, #f, NULL, 0)

    LOAD_FUNCPTR(XcursorImageCreate);
    LOAD_FUNCPTR(XcursorImageDestroy);
    LOAD_FUNCPTR(XcursorImageLoadCursor);
    LOAD_FUNCPTR(XcursorImagesCreate);
    LOAD_FUNCPTR(XcursorImagesDestroy);
    LOAD_FUNCPTR(XcursorImagesLoadCursor);
    LOAD_FUNCPTR(XcursorLibraryLoadCursor);
#undef LOAD_FUNCPTR
#endif /* SONAME_LIBXCURSOR */
}


/***********************************************************************
 *		get_empty_cursor
 */
static Cursor get_empty_cursor(void)
{
    static Cursor cursor;
    static const char data[] = { 0 };

    if (!cursor)
    {
        XColor bg;
        Pixmap pixmap;

        bg.red = bg.green = bg.blue = 0x0000;
        pixmap = XCreateBitmapFromData( gdi_display, root_window, data, 1, 1 );
        if (pixmap)
        {
            Cursor new = XCreatePixmapCursor( gdi_display, pixmap, pixmap, &bg, &bg, 0, 0 );
            if (InterlockedCompareExchangePointer( (void **)&cursor, (void *)new, 0 ))
                XFreeCursor( gdi_display, new );
            XFreePixmap( gdi_display, pixmap );
        }
    }
    return cursor;
}

/***********************************************************************
 *		set_window_cursor
 */
void set_window_cursor( Window window, HCURSOR handle )
{
    Cursor cursor, prev;

    if (!handle) cursor = get_empty_cursor();
    else if (XFindContext( gdi_display, (XID)handle, cursor_context, (char **)&cursor ))
    {
        /* try to create it */
        if (!(cursor = create_cursor( handle ))) return;

        XLockDisplay( gdi_display );
        if (!XFindContext( gdi_display, (XID)handle, cursor_context, (char **)&prev ))
        {
            /* someone else was here first */
            XFreeCursor( gdi_display, cursor );
            cursor = prev;
        }
        else
        {
            XSaveContext( gdi_display, (XID)handle, cursor_context, (char *)cursor );
            TRACE( "cursor %p created %lx\n", handle, cursor );
        }
        XUnlockDisplay( gdi_display );
    }

    XDefineCursor( gdi_display, window, cursor );
    /* make the change take effect immediately */
    XFlush( gdi_display );
}

/***********************************************************************
 *              sync_window_cursor
 */
void sync_window_cursor( Window window )
{
    HCURSOR cursor;

    SERVER_START_REQ( set_cursor )
    {
        req->flags = 0;
        wine_server_call( req );
        cursor = reply->prev_count >= 0 ? wine_server_ptr_handle( reply->prev_handle ) : 0;
    }
    SERVER_END_REQ;

    set_window_cursor( window, cursor );
}

#ifdef HAVE_X11_EXTENSIONS_XINPUT2_H
/***********************************************************************
 *              update_relative_valuators
 */
static void update_relative_valuators(XIAnyClassInfo **valuators, int n_valuators)
{
    struct x11drv_thread_data *thread_data = x11drv_thread_data();
    int i;

    thread_data->x_rel_valuator.number = -1;
    thread_data->y_rel_valuator.number = -1;
    thread_data->x_rel_valuator.accum = 0;
    thread_data->y_rel_valuator.accum = 0;

    for (i = 0; i < n_valuators; i++)
    {
        XIValuatorClassInfo *class = (XIValuatorClassInfo *)valuators[i];
        struct x11drv_valuator_data *valuator_data = NULL;

        if (valuators[i]->type != XIValuatorClass) continue;
        if (class->label == x11drv_atom( Rel_X ) ||
            (!class->label && class->number == 0 && class->mode == XIModeRelative))
        {
            valuator_data = &thread_data->x_rel_valuator;
        }
        else if (class->label == x11drv_atom( Rel_Y ) ||
                 (!class->label && class->number == 1 && class->mode == XIModeRelative))
        {
            valuator_data = &thread_data->y_rel_valuator;
        }

        if (valuator_data) {
            valuator_data->number = class->number;
            valuator_data->min = class->min;
            valuator_data->max = class->max;
        }
    }
}
#endif


/***********************************************************************
 *              enable_xinput2
 */
static void enable_xinput2(void)
{
#ifdef HAVE_X11_EXTENSIONS_XINPUT2_H
    struct x11drv_thread_data *data = x11drv_thread_data();
    XIEventMask mask;
    XIDeviceInfo *pointer_info;
    unsigned char mask_bits[XIMaskLen(XI_LASTEVENT)];
    int count;

    if (!xinput2_available) return;

    if (data->xi2_state == xi_unknown)
    {
        int major = 2, minor = 0;
        if (!pXIQueryVersion( data->display, &major, &minor )) data->xi2_state = xi_disabled;
        else
        {
            data->xi2_state = xi_unavailable;
            WARN( "X Input 2 not available\n" );
        }
    }
    if (data->xi2_state == xi_unavailable) return;
    if (!pXIGetClientPointer( data->display, None, &data->xi2_core_pointer )) return;

    mask.mask     = mask_bits;
    mask.mask_len = sizeof(mask_bits);
    mask.deviceid = XIAllDevices;
    memset( mask_bits, 0, sizeof(mask_bits) );
    XISetMask( mask_bits, XI_DeviceChanged );
    XISetMask( mask_bits, XI_RawMotion );
    XISetMask( mask_bits, XI_ButtonPress );

    pXISelectEvents( data->display, DefaultRootWindow( data->display ), &mask, 1 );

    pointer_info = pXIQueryDevice( data->display, data->xi2_core_pointer, &count );
    update_relative_valuators( pointer_info->classes, pointer_info->num_classes );
    pXIFreeDeviceInfo( pointer_info );

    /* This device info list is only used to find the initial current slave if
     * no XI_DeviceChanged events happened. If any hierarchy change occurred that
     * might be relevant here (eg. user switching mice after (un)plugging), a
     * XI_DeviceChanged event will point us to the right slave. So this list is
     * safe to be obtained statically at enable_xinput2() time.
     */
    if (data->xi2_devices) pXIFreeDeviceInfo( data->xi2_devices );
    data->xi2_devices = pXIQueryDevice( data->display, XIAllDevices, &data->xi2_device_count );
    data->xi2_current_slave = 0;

    data->xi2_state = xi_enabled;
#endif
}

/***********************************************************************
 *              disable_xinput2
 */
static void disable_xinput2(void)
{
#ifdef HAVE_X11_EXTENSIONS_XINPUT2_H
    struct x11drv_thread_data *data = x11drv_thread_data();
    XIEventMask mask;

    if (data->xi2_state != xi_enabled) return;

    TRACE( "disabling\n" );
    data->xi2_state = xi_disabled;

    mask.mask = NULL;
    mask.mask_len = 0;
    mask.deviceid = XIAllDevices;

    pXISelectEvents( data->display, DefaultRootWindow( data->display ), &mask, 1 );
    pXIFreeDeviceInfo( data->xi2_devices );
    data->x_rel_valuator.number = -1;
    data->y_rel_valuator.number = -1;
    data->x_rel_valuator.accum = 0;
    data->y_rel_valuator.accum = 0;
    data->xi2_devices = NULL;
    data->xi2_core_pointer = 0;
    data->xi2_current_slave = 0;
#endif
}


/***********************************************************************
 *		grab_clipping_window
 *
 * Start a pointer grab on the clip window.
 */
static BOOL grab_clipping_window( const RECT *clip )
{
    static const WCHAR messageW[] = {'M','e','s','s','a','g','e',0};
    struct x11drv_thread_data *data = x11drv_thread_data();
    Window clip_window;
    HWND msg_hwnd = 0;
    POINT pos;
    RECT real_clip;

    if (GetWindowThreadProcessId( GetDesktopWindow(), NULL ) == GetCurrentThreadId())
        return TRUE;  /* don't clip in the desktop process */

    if (!data) return FALSE;
    if (!(clip_window = init_clip_window())) return TRUE;

    if (!(msg_hwnd = CreateWindowW( messageW, NULL, 0, 0, 0, 0, 0, HWND_MESSAGE, 0,
                                    GetModuleHandleW(0), NULL )))
        return TRUE;

    /* enable XInput2 unless we are already clipping */
    if (!data->clip_hwnd) enable_xinput2();

    if (data->xi2_state != xi_enabled)
    {
        WARN( "XInput2 not supported, refusing to clip to %s\n", wine_dbgstr_rect(clip) );
        DestroyWindow( msg_hwnd );
        ClipCursor( NULL );
        return TRUE;
    }

    TRACE( "clipping to %s win %lx\n", wine_dbgstr_rect(clip), clip_window );

    if (!data->clip_hwnd) XUnmapWindow( data->display, clip_window );

    pos.x = clip->left;
    pos.y = clip->top;
    fs_hack_user_to_real(&pos);
    real_clip.left = pos.x;
    real_clip.top = pos.y;

    pos.x = clip->right;
    pos.y = clip->bottom;
    fs_hack_user_to_real(&pos);
    real_clip.right = pos.x;
    real_clip.bottom = pos.y;

    pos = virtual_screen_to_root( real_clip.left, real_clip.top );

    TRACE("setting real clip to %d,%d x %d,%d\n",
            pos.x, pos.y,
            real_clip.right - real_clip.left,
            real_clip.bottom - real_clip.top);

    XMoveResizeWindow( data->display, clip_window, pos.x, pos.y,
                       max( 1, real_clip.right - real_clip.left ), max( 1, real_clip.bottom - real_clip.top ) );
    XMapWindow( data->display, clip_window );

    /* if the rectangle is shrinking we may get a pointer warp */
    if (!data->clip_hwnd || clip->left > clip_rect.left || clip->top > clip_rect.top ||
        clip->right < clip_rect.right || clip->bottom < clip_rect.bottom)
        data->warp_serial = NextRequest( data->display );

    if (!XGrabPointer( data->display, clip_window, False,
                       PointerMotionMask | ButtonPressMask | ButtonReleaseMask,
                       GrabModeAsync, GrabModeAsync, clip_window, None, CurrentTime ))
        clipping_cursor = TRUE;

    if (!clipping_cursor)
    {
        disable_xinput2();
        DestroyWindow( msg_hwnd );
        return FALSE;
    }
    clip_rect = *clip;
    TRACE("new clip rect: %s\n", wine_dbgstr_rect(&clip_rect));
    if (!data->clip_hwnd) sync_window_cursor( clip_window );
    InterlockedExchangePointer( (void **)&cursor_window, msg_hwnd );
    data->clip_hwnd = msg_hwnd;
    SendMessageW( GetDesktopWindow(), WM_X11DRV_CLIP_CURSOR, 0, (LPARAM)msg_hwnd );
    return TRUE;
}

/***********************************************************************
 *		ungrab_clipping_window
 *
 * Release the pointer grab on the clip window.
 */
void ungrab_clipping_window(void)
{
    Display *display = thread_init_display();
    Window clip_window = init_clip_window();

    if (!clip_window) return;

    TRACE( "no longer clipping\n" );
    XUnmapWindow( display, clip_window );
    clipping_cursor = FALSE;
    SendMessageW( GetDesktopWindow(), WM_X11DRV_CLIP_CURSOR, 0, 0 );
}

/***********************************************************************
 *		reset_clipping_window
 *
 * Forcibly reset the window clipping on external events.
 */
void reset_clipping_window(void)
{
    ungrab_clipping_window();
    ClipCursor( NULL );  /* make sure the clip rectangle is reset too */
}

/***********************************************************************
 *             clip_cursor_notify
 *
 * Notification function called upon receiving a WM_X11DRV_CLIP_CURSOR.
 */
LRESULT clip_cursor_notify( HWND hwnd, HWND new_clip_hwnd )
{
    struct x11drv_thread_data *data = x11drv_init_thread_data();

    if (hwnd == GetDesktopWindow())  /* change the clip window stored in the desktop process */
    {
        static HWND clip_hwnd;

        HWND prev = clip_hwnd;
        clip_hwnd = new_clip_hwnd;
        if (prev || new_clip_hwnd) TRACE( "clip hwnd changed from %p to %p\n", prev, new_clip_hwnd );
        if (prev) SendNotifyMessageW( prev, WM_X11DRV_CLIP_CURSOR, 0, 0 );
    }
    else if (hwnd == data->clip_hwnd)  /* this is a notification that clipping has been reset */
    {
        TRACE( "clip hwnd reset from %p\n", hwnd );
        data->clip_hwnd = 0;
        data->clip_reset = GetTickCount();
        disable_xinput2();
        DestroyWindow( hwnd );
    }
    else if (hwnd == GetForegroundWindow())  /* request to clip */
    {
        RECT clip, virtual_rect = get_virtual_screen_rect();

        GetClipCursor( &clip );
        if (clip.left > virtual_rect.left || clip.right < virtual_rect.right ||
            clip.top > virtual_rect.top   || clip.bottom < virtual_rect.bottom)
            return grab_clipping_window( &clip );
    }
    return 0;
}

/***********************************************************************
 *		clip_fullscreen_window
 *
 * Turn on clipping if the active window is fullscreen.
 */
BOOL clip_fullscreen_window( HWND hwnd, BOOL reset )
{
    struct x11drv_win_data *data;
    struct x11drv_thread_data *thread_data;
    RECT rect;
    DWORD style;
    BOOL fullscreen;

    if (hwnd == GetDesktopWindow()) return FALSE;
    style = GetWindowLongW( hwnd, GWL_STYLE );
    if (!(style & WS_VISIBLE)) return FALSE;
    if ((style & (WS_POPUP | WS_CHILD)) == WS_CHILD) return FALSE;
    /* maximized windows don't count as full screen */
    if ((style & WS_MAXIMIZE) && (style & WS_CAPTION) == WS_CAPTION) return FALSE;
    if (!(data = get_win_data( hwnd ))) return FALSE;
    fullscreen = is_window_rect_fullscreen( &data->whole_rect );
    release_win_data( data );
    if (!fullscreen) return FALSE;
    if (!(thread_data = x11drv_thread_data())) return FALSE;
    if (!reset) {
        if (GetTickCount() - thread_data->clip_reset < 1000) return FALSE;
        if (clipping_cursor && thread_data->clip_hwnd) return FALSE;  /* already clipping */
    }
    rect = get_primary_monitor_rect();
    if (!grab_fullscreen)
    {
        RECT virtual_rect = get_virtual_screen_rect();
        if (!EqualRect( &rect, &virtual_rect )) return FALSE;
        if (root_window != DefaultRootWindow( gdi_display )) return FALSE;
    }
    TRACE( "win %p clipping fullscreen\n", hwnd );
    return grab_clipping_window( &rect );
}


/***********************************************************************
 *		is_old_motion_event
 */
static BOOL is_old_motion_event( unsigned long serial )
{
    struct x11drv_thread_data *thread_data = x11drv_thread_data();

    if (!thread_data->warp_serial) return FALSE;
    if ((long)(serial - thread_data->warp_serial) < 0) return TRUE;
    thread_data->warp_serial = 0;  /* we caught up now */
    return FALSE;
}


/***********************************************************************
 *		send_mouse_input
 *
 * Update the various window states on a mouse event.
 */
static void send_mouse_input( HWND hwnd, Window window, unsigned int state, INPUT *input )
{
    struct x11drv_win_data *data;
    POINT pt;

    input->type = INPUT_MOUSE;

    if (!hwnd)
    {
        struct x11drv_thread_data *thread_data = x11drv_thread_data();
        HWND clip_hwnd = thread_data->clip_hwnd;

        if (!clip_hwnd) return;
        if (thread_data->clip_window != window) return;
        if (InterlockedExchangePointer( (void **)&cursor_window, clip_hwnd ) != clip_hwnd ||
            input->u.mi.time - last_cursor_change > 100)
        {
            sync_window_cursor( window );
            last_cursor_change = input->u.mi.time;
        }

        pt.x = clip_rect.left;
        pt.y = clip_rect.top;
        fs_hack_user_to_real(&pt);

        pt.x += input->u.mi.dx;
        pt.y += input->u.mi.dy;
        fs_hack_real_to_user(&pt);

        input->u.mi.dx = pt.x;
        input->u.mi.dy = pt.y;

        __wine_send_input( hwnd, input );
        return;
    }

    if (window != root_window)
    {
        pt.x = input->u.mi.dx;
        pt.y = input->u.mi.dy;
    }
    else pt = root_to_virtual_screen( input->u.mi.dx, input->u.mi.dy );

    if (!(data = get_win_data( hwnd ))) return;

    if(data->fs_hack)
        fs_hack_real_to_user(&pt);

    input->u.mi.dx = pt.x;
    input->u.mi.dy = pt.y;

    if (window == data->whole_window && !data->fs_hack)
    {
        pt.x += data->whole_rect.left - data->client_rect.left;
        pt.y += data->whole_rect.top - data->client_rect.top;
    }

    if (GetWindowLongW( data->hwnd, GWL_EXSTYLE ) & WS_EX_LAYOUTRTL)
        pt.x = data->client_rect.right - data->client_rect.left - 1 - pt.x;
    MapWindowPoints( hwnd, 0, &pt, 1 );

    if (InterlockedExchangePointer( (void **)&cursor_window, hwnd ) != hwnd ||
        input->u.mi.time - last_cursor_change > 100)
    {
        sync_window_cursor( data->whole_window );
        last_cursor_change = input->u.mi.time;
    }
    release_win_data( data );

    if (hwnd != GetDesktopWindow())
    {
        hwnd = GetAncestor( hwnd, GA_ROOT );
        if ((input->u.mi.dwFlags & (MOUSEEVENTF_LEFTDOWN|MOUSEEVENTF_RIGHTDOWN)) && hwnd == GetForegroundWindow())
            clip_fullscreen_window( hwnd, FALSE );
    }

    /* update the wine server Z-order */

    if (hwnd != x11drv_thread_data()->grab_hwnd &&
        /* ignore event if a button is pressed, since the mouse is then grabbed too */
        !(state & (Button1Mask|Button2Mask|Button3Mask|Button4Mask|Button5Mask|Button6Mask|Button7Mask)))
    {
        RECT rect;
        SetRect( &rect, pt.x, pt.y, pt.x + 1, pt.y + 1 );

        SERVER_START_REQ( update_window_zorder )
        {
            req->window      = wine_server_user_handle( hwnd );
            req->rect.left   = rect.left;
            req->rect.top    = rect.top;
            req->rect.right  = rect.right;
            req->rect.bottom = rect.bottom;
            wine_server_call( req );
        }
        SERVER_END_REQ;
    }

    input->u.mi.dx = pt.x;
    input->u.mi.dy = pt.y;
    __wine_send_input( hwnd, input );
}

#ifdef SONAME_LIBXCURSOR

/***********************************************************************
 *              create_xcursor_frame
 *
 * Use Xcursor to create a frame of an X cursor from a Windows one.
 */
static XcursorImage *create_xcursor_frame( HDC hdc, const ICONINFOEXW *iinfo, HANDLE icon,
                                           HBITMAP hbmColor, unsigned char *color_bits, int color_size,
                                           HBITMAP hbmMask, unsigned char *mask_bits, int mask_size,
                                           int width, int height, int istep )
{
    XcursorImage *image, *ret = NULL;
    DWORD delay_jiffies, num_steps;
    int x, y, i;
    BOOL has_alpha = FALSE;
    XcursorPixel *ptr;

    image = pXcursorImageCreate( width, height );
    if (!image)
    {
        ERR("X11 failed to produce a cursor frame!\n");
        return NULL;
    }

    image->xhot = iinfo->xHotspot;
    image->yhot = iinfo->yHotspot;

    image->delay = 100; /* fallback delay, 100 ms */
    if (GetCursorFrameInfo(icon, 0x0 /* unknown parameter */, istep, &delay_jiffies, &num_steps) != 0)
        image->delay = (100 * delay_jiffies) / 6; /* convert jiffies (1/60s) to milliseconds */
    else
        WARN("Failed to retrieve animated cursor frame-rate for frame %d.\n", istep);

    /* draw the cursor frame to a temporary buffer then copy it into the XcursorImage */
    memset( color_bits, 0x00, color_size );
    SelectObject( hdc, hbmColor );
    if (!DrawIconEx( hdc, 0, 0, icon, width, height, istep, NULL, DI_NORMAL ))
    {
        TRACE("Could not draw frame %d (walk past end of frames).\n", istep);
        goto cleanup;
    }
    memcpy( image->pixels, color_bits, color_size );

    /* check if the cursor frame was drawn with an alpha channel */
    for (i = 0, ptr = image->pixels; i < width * height; i++, ptr++)
        if ((has_alpha = (*ptr & 0xff000000) != 0)) break;

    /* if no alpha channel was drawn then generate it from the mask */
    if (!has_alpha)
    {
        unsigned int width_bytes = (width + 31) / 32 * 4;

        /* draw the cursor mask to a temporary buffer */
        memset( mask_bits, 0xFF, mask_size );
        SelectObject( hdc, hbmMask );
        if (!DrawIconEx( hdc, 0, 0, icon, width, height, istep, NULL, DI_MASK ))
        {
            ERR("Failed to draw frame mask %d.\n", istep);
            goto cleanup;
        }
        /* use the buffer to directly modify the XcursorImage alpha channel */
        for (y = 0, ptr = image->pixels; y < height; y++)
            for (x = 0; x < width; x++, ptr++)
                if (!((mask_bits[y * width_bytes + x / 8] << (x % 8)) & 0x80))
                    *ptr |= 0xff000000;
    }
    ret = image;

cleanup:
    if (ret == NULL) pXcursorImageDestroy( image );
    return ret;
}

/***********************************************************************
 *              create_xcursor_cursor
 *
 * Use Xcursor to create an X cursor from a Windows one.
 */
static Cursor create_xcursor_cursor( HDC hdc, const ICONINFOEXW *iinfo, HANDLE icon, int width, int height )
{
    unsigned char *color_bits, *mask_bits;
    HBITMAP hbmColor = 0, hbmMask = 0;
    DWORD nFrames, delay_jiffies, i;
    int color_size, mask_size;
    BITMAPINFO *info = NULL;
    XcursorImages *images;
    XcursorImage **imgs;
    Cursor cursor = 0;

    /* Retrieve the number of frames to render */
    if (!GetCursorFrameInfo(icon, 0x0 /* unknown parameter */, 0, &delay_jiffies, &nFrames)) return 0;
    if (!(imgs = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(XcursorImage*)*nFrames ))) return 0;

    /* Allocate all of the resources necessary to obtain a cursor frame */
    if (!(info = HeapAlloc( GetProcessHeap(), 0, FIELD_OFFSET( BITMAPINFO, bmiColors[256] )))) goto cleanup;
    info->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info->bmiHeader.biWidth = width;
    info->bmiHeader.biHeight = -height;
    info->bmiHeader.biPlanes = 1;
    info->bmiHeader.biCompression = BI_RGB;
    info->bmiHeader.biXPelsPerMeter = 0;
    info->bmiHeader.biYPelsPerMeter = 0;
    info->bmiHeader.biClrUsed = 0;
    info->bmiHeader.biClrImportant = 0;
    info->bmiHeader.biBitCount = 32;
    color_size = width * height * 4;
    info->bmiHeader.biSizeImage = color_size;
    hbmColor = CreateDIBSection( hdc, info, DIB_RGB_COLORS, (VOID **) &color_bits, NULL, 0);
    if (!hbmColor)
    {
        ERR("Failed to create DIB section for cursor color data!\n");
        goto cleanup;
    }
    info->bmiHeader.biBitCount = 1;
    info->bmiColors[0].rgbRed      = 0;
    info->bmiColors[0].rgbGreen    = 0;
    info->bmiColors[0].rgbBlue     = 0;
    info->bmiColors[0].rgbReserved = 0;
    info->bmiColors[1].rgbRed      = 0xff;
    info->bmiColors[1].rgbGreen    = 0xff;
    info->bmiColors[1].rgbBlue     = 0xff;
    info->bmiColors[1].rgbReserved = 0;

    mask_size = ((width + 31) / 32 * 4) * height; /* width_bytes * height */
    info->bmiHeader.biSizeImage = mask_size;
    hbmMask = CreateDIBSection( hdc, info, DIB_RGB_COLORS, (VOID **) &mask_bits, NULL, 0);
    if (!hbmMask)
    {
        ERR("Failed to create DIB section for cursor mask data!\n");
        goto cleanup;
    }

    /* Create an XcursorImage for each frame of the cursor */
    for (i=0; i<nFrames; i++)
    {
        imgs[i] = create_xcursor_frame( hdc, iinfo, icon,
                                        hbmColor, color_bits, color_size,
                                        hbmMask, mask_bits, mask_size,
                                        width, height, i );
        if (!imgs[i]) goto cleanup;
    }

    /* Build an X cursor out of all of the frames */
    if (!(images = pXcursorImagesCreate( nFrames ))) goto cleanup;
    for (images->nimage = 0; images->nimage < nFrames; images->nimage++)
        images->images[images->nimage] = imgs[images->nimage];
    cursor = pXcursorImagesLoadCursor( gdi_display, images );
    pXcursorImagesDestroy( images ); /* Note: this frees each individual frame (calls XcursorImageDestroy) */
    HeapFree( GetProcessHeap(), 0, imgs );
    imgs = NULL;

cleanup:
    if (imgs)
    {
        /* Failed to produce a cursor, free previously allocated frames */
        for (i=0; i<nFrames && imgs[i]; i++)
            pXcursorImageDestroy( imgs[i] );
        HeapFree( GetProcessHeap(), 0, imgs );
    }
    /* Cleanup all of the resources used to obtain the frame data */
    if (hbmColor) DeleteObject( hbmColor );
    if (hbmMask) DeleteObject( hbmMask );
    HeapFree( GetProcessHeap(), 0, info );
    return cursor;
}

#endif /* SONAME_LIBXCURSOR */


struct system_cursors
{
    WORD id;
    const char *names[8];
};

static const struct system_cursors user32_cursors[] =
{
    { OCR_NORMAL,      { "left_ptr" }},
    { OCR_IBEAM,       { "xterm", "text" }},
    { OCR_WAIT,        { "watch", "wait" }},
    { OCR_CROSS,       { "cross" }},
    { OCR_UP,          { "center_ptr" }},
    { OCR_SIZE,        { "fleur", "size_all" }},
    { OCR_SIZEALL,     { "fleur", "size_all" }},
    { OCR_ICON,        { "icon" }},
    { OCR_SIZENWSE,    { "top_left_corner", "nw-resize" }},
    { OCR_SIZENESW,    { "top_right_corner", "ne-resize" }},
    { OCR_SIZEWE,      { "h_double_arrow", "size_hor", "ew-resize" }},
    { OCR_SIZENS,      { "v_double_arrow", "size_ver", "ns-resize" }},
    { OCR_NO,          { "not-allowed", "forbidden", "no-drop" }},
    { OCR_HAND,        { "hand2", "pointer", "pointing-hand" }},
    { OCR_APPSTARTING, { "left_ptr_watch" }},
    { OCR_HELP,        { "question_arrow", "help" }},
    { 0 }
};

static const struct system_cursors comctl32_cursors[] =
{
    { 102, { "move", "dnd-move" }},
    { 104, { "copy", "dnd-copy" }},
    { 105, { "left_ptr" }},
    { 106, { "col-resize", "split_v" }},
    { 107, { "col-resize", "split_v" }},
    { 108, { "hand2", "pointer", "pointing-hand" }},
    { 135, { "row-resize", "split_h" }},
    { 0 }
};

static const struct system_cursors ole32_cursors[] =
{
    { 1, { "no-drop", "dnd-no-drop" }},
    { 2, { "move", "dnd-move" }},
    { 3, { "copy", "dnd-copy" }},
    { 4, { "alias", "dnd-link" }},
    { 0 }
};

static const struct system_cursors riched20_cursors[] =
{
    { 105, { "hand2", "pointer", "pointing-hand" }},
    { 107, { "right_ptr" }},
    { 109, { "copy", "dnd-copy" }},
    { 110, { "move", "dnd-move" }},
    { 111, { "no-drop", "dnd-no-drop" }},
    { 0 }
};

static const struct
{
    const struct system_cursors *cursors;
    WCHAR name[16];
} module_cursors[] =
{
    { user32_cursors, {'u','s','e','r','3','2','.','d','l','l',0} },
    { comctl32_cursors, {'c','o','m','c','t','l','3','2','.','d','l','l',0} },
    { ole32_cursors, {'o','l','e','3','2','.','d','l','l',0} },
    { riched20_cursors, {'r','i','c','h','e','d','2','0','.','d','l','l',0} }
};

struct cursor_font_fallback
{
    const char  *name;
    unsigned int shape;
};

static const struct cursor_font_fallback fallbacks[] =
{
    { "X_cursor",            XC_X_cursor },
    { "arrow",               XC_arrow },
    { "based_arrow_down",    XC_based_arrow_down },
    { "based_arrow_up",      XC_based_arrow_up },
    { "boat",                XC_boat },
    { "bogosity",            XC_bogosity },
    { "bottom_left_corner",  XC_bottom_left_corner },
    { "bottom_right_corner", XC_bottom_right_corner },
    { "bottom_side",         XC_bottom_side },
    { "bottom_tee",          XC_bottom_tee },
    { "box_spiral",          XC_box_spiral },
    { "center_ptr",          XC_center_ptr },
    { "circle",              XC_circle },
    { "clock",               XC_clock },
    { "coffee_mug",          XC_coffee_mug },
    { "col-resize",          XC_sb_h_double_arrow },
    { "cross",               XC_cross },
    { "cross_reverse",       XC_cross_reverse },
    { "crosshair",           XC_crosshair },
    { "diamond_cross",       XC_diamond_cross },
    { "dot",                 XC_dot },
    { "dotbox",              XC_dotbox },
    { "double_arrow",        XC_double_arrow },
    { "draft_large",         XC_draft_large },
    { "draft_small",         XC_draft_small },
    { "draped_box",          XC_draped_box },
    { "exchange",            XC_exchange },
    { "fleur",               XC_fleur },
    { "gobbler",             XC_gobbler },
    { "gumby",               XC_gumby },
    { "h_double_arrow",      XC_sb_h_double_arrow },
    { "hand1",               XC_hand1 },
    { "hand2",               XC_hand2 },
    { "heart",               XC_heart },
    { "icon",                XC_icon },
    { "iron_cross",          XC_iron_cross },
    { "left_ptr",            XC_left_ptr },
    { "left_side",           XC_left_side },
    { "left_tee",            XC_left_tee },
    { "leftbutton",          XC_leftbutton },
    { "ll_angle",            XC_ll_angle },
    { "lr_angle",            XC_lr_angle },
    { "man",                 XC_man },
    { "middlebutton",        XC_middlebutton },
    { "mouse",               XC_mouse },
    { "pencil",              XC_pencil },
    { "pirate",              XC_pirate },
    { "plus",                XC_plus },
    { "question_arrow",      XC_question_arrow },
    { "right_ptr",           XC_right_ptr },
    { "right_side",          XC_right_side },
    { "right_tee",           XC_right_tee },
    { "rightbutton",         XC_rightbutton },
    { "row-resize",          XC_sb_v_double_arrow },
    { "rtl_logo",            XC_rtl_logo },
    { "sailboat",            XC_sailboat },
    { "sb_down_arrow",       XC_sb_down_arrow },
    { "sb_h_double_arrow",   XC_sb_h_double_arrow },
    { "sb_left_arrow",       XC_sb_left_arrow },
    { "sb_right_arrow",      XC_sb_right_arrow },
    { "sb_up_arrow",         XC_sb_up_arrow },
    { "sb_v_double_arrow",   XC_sb_v_double_arrow },
    { "shuttle",             XC_shuttle },
    { "sizing",              XC_sizing },
    { "spider",              XC_spider },
    { "spraycan",            XC_spraycan },
    { "star",                XC_star },
    { "target",              XC_target },
    { "tcross",              XC_tcross },
    { "top_left_arrow",      XC_top_left_arrow },
    { "top_left_corner",     XC_top_left_corner },
    { "top_right_corner",    XC_top_right_corner },
    { "top_side",            XC_top_side },
    { "top_tee",             XC_top_tee },
    { "trek",                XC_trek },
    { "ul_angle",            XC_ul_angle },
    { "umbrella",            XC_umbrella },
    { "ur_angle",            XC_ur_angle },
    { "v_double_arrow",      XC_sb_v_double_arrow },
    { "watch",               XC_watch },
    { "xterm",               XC_xterm }
};

static int fallback_cmp( const void *key, const void *member )
{
    const struct cursor_font_fallback *fallback = member;
    return strcmp( key, fallback->name );
}

static int find_fallback_shape( const char *name )
{
    struct cursor_font_fallback *fallback;

    if ((fallback = bsearch( name, fallbacks, sizeof(fallbacks) / sizeof(fallbacks[0]),
                             sizeof(*fallback), fallback_cmp )))
        return fallback->shape;
    return -1;
}

/***********************************************************************
 *		create_xcursor_system_cursor
 *
 * Create an X cursor for a system cursor.
 */
static Cursor create_xcursor_system_cursor( const ICONINFOEXW *info )
{
    static const WCHAR idW[] = {'%','h','u',0};
    const struct system_cursors *cursors;
    unsigned int i;
    Cursor cursor = 0;
    HMODULE module;
    HKEY key;
    const char * const *names = NULL;
    WCHAR *p, name[MAX_PATH * 2], valueW[64];
    char valueA[64];
    DWORD ret;

    if (!info->szModName[0]) return 0;

    p = strrchrW( info->szModName, '\\' );
    strcpyW( name, p ? p + 1 : info->szModName );
    p = name + strlenW( name );
    *p++ = ',';
    if (info->szResName[0]) strcpyW( p, info->szResName );
    else sprintfW( p, idW, info->wResID );
    valueA[0] = 0;

    /* @@ Wine registry key: HKCU\Software\Wine\X11 Driver\Cursors */
    if (!RegOpenKeyA( HKEY_CURRENT_USER, "Software\\Wine\\X11 Driver\\Cursors", &key ))
    {
        DWORD size = sizeof(valueW);
        ret = RegQueryValueExW( key, name, NULL, NULL, (BYTE *)valueW, &size );
        RegCloseKey( key );
        if (!ret)
        {
            if (!valueW[0]) return 0; /* force standard cursor */
            if (!WideCharToMultiByte( CP_UNIXCP, 0, valueW, -1, valueA, sizeof(valueA), NULL, NULL ))
                valueA[0] = 0;
            goto done;
        }
    }

    if (info->szResName[0]) goto done;  /* only integer resources are supported here */
    if (!(module = GetModuleHandleW( info->szModName ))) goto done;

    for (i = 0; i < sizeof(module_cursors)/sizeof(module_cursors[0]); i++)
        if (GetModuleHandleW( module_cursors[i].name ) == module) break;
    if (i == sizeof(module_cursors)/sizeof(module_cursors[0])) goto done;

    cursors = module_cursors[i].cursors;
    for (i = 0; cursors[i].id; i++)
        if (cursors[i].id == info->wResID)
        {
            strcpy( valueA, cursors[i].names[0] );
            names = cursors[i].names;
            break;
        }

done:
    if (valueA[0])
    {
#ifdef SONAME_LIBXCURSOR
        if (pXcursorLibraryLoadCursor)
        {
            if (!names)
                cursor = pXcursorLibraryLoadCursor( gdi_display, valueA );
            else
                while (*names && !cursor) cursor = pXcursorLibraryLoadCursor( gdi_display, *names++ );
        }
#endif
        if (!cursor)
        {
            int shape = find_fallback_shape( valueA );
            if (shape != -1) cursor = XCreateFontCursor( gdi_display, shape );
        }
        if (!cursor) WARN( "no system cursor found for %s mapped to %s\n",
                           debugstr_w(name), debugstr_a(valueA) );
    }
    else WARN( "no system cursor found for %s\n", debugstr_w(name) );
    return cursor;
}


/***********************************************************************
 *		create_xlib_monochrome_cursor
 *
 * Create a monochrome X cursor from a Windows one.
 */
static Cursor create_xlib_monochrome_cursor( HDC hdc, const ICONINFOEXW *icon, int width, int height )
{
    char buffer[FIELD_OFFSET( BITMAPINFO, bmiColors[256] )];
    BITMAPINFO *info = (BITMAPINFO *)buffer;
    const int and_y = 0;
    const int xor_y = height;
    unsigned int width_bytes = (width + 31) / 32 * 4;
    unsigned char *mask_bits = NULL;
    GC gc;
    XColor fg, bg;
    XVisualInfo vis = default_visual;
    Pixmap src_pixmap, bits_pixmap, mask_pixmap;
    struct gdi_image_bits bits;
    Cursor cursor = 0;

    info->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info->bmiHeader.biWidth = width;
    info->bmiHeader.biHeight = -height * 2;
    info->bmiHeader.biPlanes = 1;
    info->bmiHeader.biBitCount = 1;
    info->bmiHeader.biCompression = BI_RGB;
    info->bmiHeader.biSizeImage = width_bytes * height * 2;
    info->bmiHeader.biXPelsPerMeter = 0;
    info->bmiHeader.biYPelsPerMeter = 0;
    info->bmiHeader.biClrUsed = 0;
    info->bmiHeader.biClrImportant = 0;

    if (!(mask_bits = HeapAlloc( GetProcessHeap(), 0, info->bmiHeader.biSizeImage ))) goto done;
    if (!GetDIBits( hdc, icon->hbmMask, 0, height * 2, mask_bits, info, DIB_RGB_COLORS )) goto done;

    vis.depth = 1;
    bits.ptr = mask_bits;
    bits.free = NULL;
    bits.is_copy = TRUE;
    if (!(src_pixmap = create_pixmap_from_image( hdc, &vis, info, &bits, DIB_RGB_COLORS ))) goto done;

    bits_pixmap = XCreatePixmap( gdi_display, root_window, width, height, 1 );
    mask_pixmap = XCreatePixmap( gdi_display, root_window, width, height, 1 );
    gc = XCreateGC( gdi_display, src_pixmap, 0, NULL );
    XSetGraphicsExposures( gdi_display, gc, False );

    /* We have to do some magic here, as cursors are not fully
     * compatible between Windows and X11. Under X11, there are
     * only 3 possible color cursor: black, white and masked. So
     * we map the 4th Windows color (invert the bits on the screen)
     * to black and an additional white bit on another place
     * (+1,+1). This require some boolean arithmetic:
     *
     *         Windows          |          X11
     * And    Xor      Result   |   Bits     Mask     Result
     *  0      0     black      |    0        1     background
     *  0      1     white      |    1        1     foreground
     *  1      0     no change  |    X        0     no change
     *  1      1     inverted   |    0        1     background
     *
     * which gives:
     *  Bits = not 'And' and 'Xor' or 'And2' and 'Xor2'
     *  Mask = not 'And' or 'Xor' or 'And2' and 'Xor2'
     */
    XSetFunction( gdi_display, gc, GXcopy );
    XCopyArea( gdi_display, src_pixmap, bits_pixmap, gc, 0, and_y, width, height, 0, 0 );
    XCopyArea( gdi_display, src_pixmap, mask_pixmap, gc, 0, and_y, width, height, 0, 0 );
    XSetFunction( gdi_display, gc, GXandReverse );
    XCopyArea( gdi_display, src_pixmap, bits_pixmap, gc, 0, xor_y, width, height, 0, 0 );
    XSetFunction( gdi_display, gc, GXorReverse );
    XCopyArea( gdi_display, src_pixmap, mask_pixmap, gc, 0, xor_y, width, height, 0, 0 );
    /* additional white */
    XSetFunction( gdi_display, gc, GXand );
    XCopyArea( gdi_display, src_pixmap, src_pixmap,  gc, 0, xor_y, width, height, 0, and_y );
    XSetFunction( gdi_display, gc, GXor );
    XCopyArea( gdi_display, src_pixmap, mask_pixmap, gc, 0, and_y, width, height, 1, 1 );
    XCopyArea( gdi_display, src_pixmap, bits_pixmap, gc, 0, and_y, width, height, 1, 1 );
    XFreeGC( gdi_display, gc );

    fg.red = fg.green = fg.blue = 0xffff;
    bg.red = bg.green = bg.blue = 0;
    cursor = XCreatePixmapCursor( gdi_display, bits_pixmap, mask_pixmap,
                                  &fg, &bg, icon->xHotspot, icon->yHotspot );
    XFreePixmap( gdi_display, src_pixmap );
    XFreePixmap( gdi_display, bits_pixmap );
    XFreePixmap( gdi_display, mask_pixmap );

done:
    HeapFree( GetProcessHeap(), 0, mask_bits );
    return cursor;
}

/***********************************************************************
 *		create_xlib_load_mono_cursor
 *
 * Create a monochrome X cursor from a color Windows one by trying to load the monochrome resource.
 */
static Cursor create_xlib_load_mono_cursor( HDC hdc, HANDLE handle, int width, int height )
{
    Cursor cursor = None;
    HANDLE mono;
    ICONINFOEXW info;
    BITMAP bm;

    if (!(mono = CopyImage( handle, IMAGE_CURSOR, width, height, LR_MONOCHROME | LR_COPYFROMRESOURCE )))
        return None;

    info.cbSize = sizeof(info);
    if (GetIconInfoExW( mono, &info ))
    {
        if (!info.hbmColor)
        {
            GetObjectW( info.hbmMask, sizeof(bm), &bm );
            bm.bmHeight = max( 1, bm.bmHeight / 2 );
            /* make sure hotspot is valid */
            if (info.xHotspot >= bm.bmWidth || info.yHotspot >= bm.bmHeight)
            {
                info.xHotspot = bm.bmWidth / 2;
                info.yHotspot = bm.bmHeight / 2;
            }
            cursor = create_xlib_monochrome_cursor( hdc, &info, bm.bmWidth, bm.bmHeight );
        }
        else DeleteObject( info.hbmColor );
        DeleteObject( info.hbmMask );
    }
    DestroyCursor( mono );
    return cursor;
}

/***********************************************************************
 *		create_xlib_color_cursor
 *
 * Create a color X cursor from a Windows one.
 */
static Cursor create_xlib_color_cursor( HDC hdc, const ICONINFOEXW *icon, int width, int height )
{
    char buffer[FIELD_OFFSET( BITMAPINFO, bmiColors[256] )];
    BITMAPINFO *info = (BITMAPINFO *)buffer;
    XColor fg, bg;
    Cursor cursor = None;
    XVisualInfo vis = default_visual;
    Pixmap xor_pixmap, mask_pixmap;
    struct gdi_image_bits bits;
    unsigned int *color_bits = NULL, *ptr;
    unsigned char *mask_bits = NULL, *xor_bits = NULL;
    int i, x, y;
    BOOL has_alpha = FALSE;
    int rfg, gfg, bfg, rbg, gbg, bbg, fgBits, bgBits;
    unsigned int width_bytes = (width + 31) / 32 * 4;

    info->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info->bmiHeader.biWidth = width;
    info->bmiHeader.biHeight = -height;
    info->bmiHeader.biPlanes = 1;
    info->bmiHeader.biBitCount = 1;
    info->bmiHeader.biCompression = BI_RGB;
    info->bmiHeader.biSizeImage = width_bytes * height;
    info->bmiHeader.biXPelsPerMeter = 0;
    info->bmiHeader.biYPelsPerMeter = 0;
    info->bmiHeader.biClrUsed = 0;
    info->bmiHeader.biClrImportant = 0;

    if (!(mask_bits = HeapAlloc( GetProcessHeap(), 0, info->bmiHeader.biSizeImage ))) goto done;
    if (!GetDIBits( hdc, icon->hbmMask, 0, height, mask_bits, info, DIB_RGB_COLORS )) goto done;

    info->bmiHeader.biBitCount = 32;
    info->bmiHeader.biSizeImage = width * height * 4;
    if (!(color_bits = HeapAlloc( GetProcessHeap(), 0, info->bmiHeader.biSizeImage ))) goto done;
    if (!(xor_bits = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, width_bytes * height ))) goto done;
    GetDIBits( hdc, icon->hbmColor, 0, height, color_bits, info, DIB_RGB_COLORS );

    /* compute fg/bg color and xor bitmap based on average of the color values */

    rfg = gfg = bfg = rbg = gbg = bbg = fgBits = 0;
    for (y = 0, ptr = color_bits; y < height; y++)
    {
        for (x = 0; x < width; x++, ptr++)
        {
            int red   = (*ptr >> 16) & 0xff;
            int green = (*ptr >> 8) & 0xff;
            int blue  = (*ptr >> 0) & 0xff;
            if (red + green + blue > 0x40)
            {
                rfg += red;
                gfg += green;
                bfg += blue;
                fgBits++;
                xor_bits[y * width_bytes + x / 8] |= 0x80 >> (x % 8);
            }
            else
            {
                rbg += red;
                gbg += green;
                bbg += blue;
            }
        }
    }
    if (fgBits)
    {
        fg.red   = rfg * 257 / fgBits;
        fg.green = gfg * 257 / fgBits;
        fg.blue  = bfg * 257 / fgBits;
    }
    else fg.red = fg.green = fg.blue = 0;
    bgBits = width * height - fgBits;
    if (bgBits)
    {
        bg.red   = rbg * 257 / bgBits;
        bg.green = gbg * 257 / bgBits;
        bg.blue  = bbg * 257 / bgBits;
    }
    else bg.red = bg.green = bg.blue = 0;

    info->bmiHeader.biBitCount = 1;
    info->bmiHeader.biClrUsed = 0;
    info->bmiHeader.biSizeImage = width_bytes * height;

    /* generate mask from the alpha channel if we have one */

    for (i = 0, ptr = color_bits; i < width * height; i++, ptr++)
        if ((has_alpha = (*ptr & 0xff000000) != 0)) break;

    if (has_alpha)
    {
        memset( mask_bits, 0, width_bytes * height );
        for (y = 0, ptr = color_bits; y < height; y++)
            for (x = 0; x < width; x++, ptr++)
                if ((*ptr >> 24) > 25) /* more than 10% alpha */
                    mask_bits[y * width_bytes + x / 8] |= 0x80 >> (x % 8);
    }
    else  /* invert the mask */
    {
        unsigned int j;

        ptr = (unsigned int *)mask_bits;
        for (j = 0; j < info->bmiHeader.biSizeImage / sizeof(*ptr); j++, ptr++) *ptr ^= ~0u;
    }

    vis.depth = 1;
    bits.ptr = xor_bits;
    bits.free = NULL;
    bits.is_copy = TRUE;
    if (!(xor_pixmap = create_pixmap_from_image( hdc, &vis, info, &bits, DIB_RGB_COLORS ))) goto done;

    bits.ptr = mask_bits;
    mask_pixmap = create_pixmap_from_image( hdc, &vis, info, &bits, DIB_RGB_COLORS );

    if (mask_pixmap)
    {
        cursor = XCreatePixmapCursor( gdi_display, xor_pixmap, mask_pixmap,
                                      &fg, &bg, icon->xHotspot, icon->yHotspot );
        XFreePixmap( gdi_display, mask_pixmap );
    }
    XFreePixmap( gdi_display, xor_pixmap );

done:
    HeapFree( GetProcessHeap(), 0, color_bits );
    HeapFree( GetProcessHeap(), 0, xor_bits );
    HeapFree( GetProcessHeap(), 0, mask_bits );
    return cursor;
}

/***********************************************************************
 *		create_cursor
 *
 * Create an X cursor from a Windows one.
 */
static Cursor create_cursor( HANDLE handle )
{
    Cursor cursor = 0;
    ICONINFOEXW info;
    BITMAP bm;
    HDC hdc;

    if (!handle) return get_empty_cursor();

    info.cbSize = sizeof(info);
    if (!GetIconInfoExW( handle, &info )) return 0;

    if (use_system_cursors && (cursor = create_xcursor_system_cursor( &info )))
    {
        DeleteObject( info.hbmColor );
        DeleteObject( info.hbmMask );
        return cursor;
    }

    GetObjectW( info.hbmMask, sizeof(bm), &bm );
    if (!info.hbmColor) bm.bmHeight = max( 1, bm.bmHeight / 2 );

    /* make sure hotspot is valid */
    if (info.xHotspot >= bm.bmWidth || info.yHotspot >= bm.bmHeight)
    {
        info.xHotspot = bm.bmWidth / 2;
        info.yHotspot = bm.bmHeight / 2;
    }

    hdc = CreateCompatibleDC( 0 );

    if (info.hbmColor)
    {
#ifdef SONAME_LIBXCURSOR
        if (pXcursorImagesLoadCursor)
            cursor = create_xcursor_cursor( hdc, &info, handle, bm.bmWidth, bm.bmHeight );
#endif
        if (!cursor) cursor = create_xlib_load_mono_cursor( hdc, handle, bm.bmWidth, bm.bmHeight );
        if (!cursor) cursor = create_xlib_color_cursor( hdc, &info, bm.bmWidth, bm.bmHeight );
        DeleteObject( info.hbmColor );
    }
    else
    {
        cursor = create_xlib_monochrome_cursor( hdc, &info, bm.bmWidth, bm.bmHeight );
    }

    DeleteObject( info.hbmMask );
    DeleteDC( hdc );
    return cursor;
}

/***********************************************************************
 *		DestroyCursorIcon (X11DRV.@)
 */
void CDECL X11DRV_DestroyCursorIcon( HCURSOR handle )
{
    Cursor cursor;

    if (!XFindContext( gdi_display, (XID)handle, cursor_context, (char **)&cursor ))
    {
        TRACE( "%p xid %lx\n", handle, cursor );
        XFreeCursor( gdi_display, cursor );
        XDeleteContext( gdi_display, (XID)handle, cursor_context );
    }
}

/***********************************************************************
 *		SetCursor (X11DRV.@)
 */
void CDECL X11DRV_SetCursor( HCURSOR handle )
{
    if (InterlockedExchangePointer( (void **)&last_cursor, handle ) != handle ||
        GetTickCount() - last_cursor_change > 100)
    {
        last_cursor_change = GetTickCount();
        if (cursor_window) SendNotifyMessageW( cursor_window, WM_X11DRV_SET_CURSOR, 0, (LPARAM)handle );
    }
}

/***********************************************************************
 *		SetCursorPos (X11DRV.@)
 */
BOOL CDECL X11DRV_SetCursorPos( INT x, INT y )
{
    struct x11drv_thread_data *data = x11drv_init_thread_data();
    POINT pos = {x, y};

    fs_hack_user_to_real(&pos);
    pos = virtual_screen_to_root( pos.x, pos.y );

    TRACE("real setting to %u, %u\n",
            pos.x, pos.y);

    XWarpPointer( data->display, root_window, root_window, 0, 0, 0, 0, pos.x, pos.y );
    data->warp_serial = NextRequest( data->display );
    XNoOp( data->display );
    XFlush( data->display ); /* avoids bad mouse lag in games that do their own mouse warping */
    TRACE( "warped to (fake) %d,%d serial %lu\n", x, y, data->warp_serial );
    return TRUE;
}

/***********************************************************************
 *		GetCursorPos (X11DRV.@)
 */
BOOL CDECL X11DRV_GetCursorPos(LPPOINT pos)
{
    Display *display = thread_init_display();
    Window root, child;
    int rootX, rootY, winX, winY;
    unsigned int xstate;
    BOOL ret;

    ret = XQueryPointer( display, root_window, &root, &child, &rootX, &rootY, &winX, &winY, &xstate );
    if (ret)
    {
        POINT old = *pos;
        POINT p = root_to_virtual_screen( winX, winY );
        fs_hack_real_to_user(&p);
        TRACE( "pointer at %s server pos %s\n", wine_dbgstr_point(pos), wine_dbgstr_point(&old) );
    }
    return ret;
}

/***********************************************************************
 *		ClipCursor (X11DRV.@)
 */
BOOL CDECL X11DRV_ClipCursor( LPCRECT clip )
{
    RECT virtual_rect = get_virtual_screen_rect();

    if (!clip) clip = &virtual_rect;

    if (grab_pointer)
    {
        HWND foreground = GetForegroundWindow();

        /* we are clipping if the clip rectangle is smaller than the screen */
        if (!(!fs_hack_enabled() && clip->left == 0 && clip->top == 0 && fs_hack_matches_last_mode(clip->right, clip->bottom)) && /* fix games trying to reset clip to full screen */
                (clip->left > virtual_rect.left || clip->right < virtual_rect.right ||
                 clip->top > virtual_rect.top || clip->bottom < virtual_rect.bottom))
        {
            DWORD tid, pid;

            /* forward request to the foreground window if it's in a different thread */
            tid = GetWindowThreadProcessId( foreground, &pid );
            if (tid && tid != GetCurrentThreadId() && pid == GetCurrentProcessId())
            {
                TRACE( "forwarding clip request to %p\n", foreground );
                SendNotifyMessageW( foreground, WM_X11DRV_CLIP_CURSOR, 0, 0 );
                return TRUE;
            }
            else if (grab_clipping_window( clip )) return TRUE;
        }
        else /* check if we should switch to fullscreen clipping */
        {
            struct x11drv_thread_data *data = x11drv_thread_data();
            if (data)
            {
                if ((data->clip_hwnd && EqualRect( clip, &clip_rect )) || clip_fullscreen_window( foreground, TRUE ))
                    return TRUE;
            }
        }
    }
    ungrab_clipping_window();
    return TRUE;
}

/***********************************************************************
 *           move_resize_window
 */
void move_resize_window( HWND hwnd, int dir )
{
    Display *display = thread_display();
    DWORD pt;
    POINT pos;
    int button = 0;
    XEvent xev;
    Window win, root, child;
    unsigned int xstate;

    if (!(win = X11DRV_get_whole_window( hwnd ))) return;

    pt = GetMessagePos();
    pos = virtual_screen_to_root( (short)LOWORD( pt ), (short)HIWORD( pt ) );

    if (GetKeyState( VK_LBUTTON ) & 0x8000) button = 1;
    else if (GetKeyState( VK_MBUTTON ) & 0x8000) button = 2;
    else if (GetKeyState( VK_RBUTTON ) & 0x8000) button = 3;

    TRACE( "hwnd %p/%lx, pos %s, dir %d, button %d\n", hwnd, win, wine_dbgstr_point(&pos), dir, button );

    xev.xclient.type = ClientMessage;
    xev.xclient.window = win;
    xev.xclient.message_type = x11drv_atom(_NET_WM_MOVERESIZE);
    xev.xclient.serial = 0;
    xev.xclient.display = display;
    xev.xclient.send_event = True;
    xev.xclient.format = 32;
    xev.xclient.data.l[0] = pos.x; /* x coord */
    xev.xclient.data.l[1] = pos.y; /* y coord */
    xev.xclient.data.l[2] = dir; /* direction */
    xev.xclient.data.l[3] = button; /* button */
    xev.xclient.data.l[4] = 0; /* unused */

    /* need to ungrab the pointer that may have been automatically grabbed
     * with a ButtonPress event */
    XUngrabPointer( display, CurrentTime );
    XSendEvent(display, root_window, False, SubstructureNotifyMask | SubstructureRedirectMask, &xev);

    /* try to detect the end of the size/move by polling for the mouse button to be released */
    /* (some apps don't like it if we return before the size/move is done) */

    if (!button) return;
    SendMessageW( hwnd, WM_ENTERSIZEMOVE, 0, 0 );

    for (;;)
    {
        MSG msg;
        INPUT input;
        int x, y, rootX, rootY;

        if (!XQueryPointer( display, root_window, &root, &child, &rootX, &rootY, &x, &y, &xstate )) break;

        if (!(xstate & (Button1Mask << (button - 1))))
        {
            /* fake a button release event */
            pos = root_to_virtual_screen( x, y );
            input.type = INPUT_MOUSE;
            input.u.mi.dx          = pos.x;
            input.u.mi.dy          = pos.y;
            input.u.mi.mouseData   = button_up_data[button - 1];
            input.u.mi.dwFlags     = button_up_flags[button - 1] | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE;
            input.u.mi.time        = GetTickCount();
            input.u.mi.dwExtraInfo = 0;
            __wine_send_input( hwnd, &input );
        }

        while (PeekMessageW( &msg, 0, 0, 0, PM_REMOVE ))
        {
            if (!CallMsgFilterW( &msg, MSGF_SIZE ))
            {
                TranslateMessage( &msg );
                DispatchMessageW( &msg );
            }
        }

        if (!(xstate & (Button1Mask << (button - 1)))) break;
        MsgWaitForMultipleObjects( 0, NULL, FALSE, 100, QS_ALLINPUT );
    }

    TRACE( "hwnd %p/%lx done\n", hwnd, win );
    SendMessageW( hwnd, WM_EXITSIZEMOVE, 0, 0 );
}


/***********************************************************************
 *           X11DRV_ButtonPress
 */
BOOL X11DRV_ButtonPress( HWND hwnd, XEvent *xev )
{
    XButtonEvent *event = &xev->xbutton;
    int buttonNum = event->button - 1;
    INPUT input;

    if (buttonNum >= NB_BUTTONS) return FALSE;

    TRACE( "hwnd %p/%lx button %u pos %d,%d\n", hwnd, event->window, buttonNum, event->x, event->y );

    input.u.mi.dx          = event->x;
    input.u.mi.dy          = event->y;
    input.u.mi.mouseData   = button_down_data[buttonNum];
    input.u.mi.dwFlags     = button_down_flags[buttonNum] | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE;
    input.u.mi.time        = EVENT_x11_time_to_win32_time( event->time );
    input.u.mi.dwExtraInfo = 0;

    update_user_time( event->time );
    send_mouse_input( hwnd, event->window, event->state, &input );
    return TRUE;
}


/***********************************************************************
 *           X11DRV_ButtonRelease
 */
BOOL X11DRV_ButtonRelease( HWND hwnd, XEvent *xev )
{
    XButtonEvent *event = &xev->xbutton;
    int buttonNum = event->button - 1;
    INPUT input;

    if (buttonNum >= NB_BUTTONS || !button_up_flags[buttonNum]) return FALSE;

    TRACE( "hwnd %p/%lx button %u pos %d,%d\n", hwnd, event->window, buttonNum, event->x, event->y );

    input.u.mi.dx          = event->x;
    input.u.mi.dy          = event->y;
    input.u.mi.mouseData   = button_up_data[buttonNum];
    input.u.mi.dwFlags     = button_up_flags[buttonNum] | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE;
    input.u.mi.time        = EVENT_x11_time_to_win32_time( event->time );
    input.u.mi.dwExtraInfo = 0;

    send_mouse_input( hwnd, event->window, event->state, &input );
    return TRUE;
}


/***********************************************************************
 *           X11DRV_MotionNotify
 */
BOOL X11DRV_MotionNotify( HWND hwnd, XEvent *xev )
{
    XMotionEvent *event = &xev->xmotion;
    INPUT input;

    TRACE( "hwnd %p/%lx pos %d,%d is_hint %d serial %lu\n",
           hwnd, event->window, event->x, event->y, event->is_hint, event->serial );

    input.u.mi.dx          = event->x;
    input.u.mi.dy          = event->y;
    input.u.mi.mouseData   = 0;
    input.u.mi.dwFlags     = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
    input.u.mi.time        = EVENT_x11_time_to_win32_time( event->time );
    input.u.mi.dwExtraInfo = 0;

    if (!hwnd && is_old_motion_event( event->serial ))
    {
        TRACE( "pos %d,%d old serial %lu, ignoring\n", input.u.mi.dx, input.u.mi.dy, event->serial );
        return FALSE;
    }
    send_mouse_input( hwnd, event->window, event->state, &input );
    return TRUE;
}


/***********************************************************************
 *           X11DRV_EnterNotify
 */
BOOL X11DRV_EnterNotify( HWND hwnd, XEvent *xev )
{
    XCrossingEvent *event = &xev->xcrossing;
    INPUT input;

    TRACE( "hwnd %p/%lx pos %d,%d detail %d\n", hwnd, event->window, event->x, event->y, event->detail );

    if (event->detail == NotifyVirtual) return FALSE;
    if (hwnd == x11drv_thread_data()->grab_hwnd) return FALSE;

    /* simulate a mouse motion event */
    input.u.mi.dx          = event->x;
    input.u.mi.dy          = event->y;
    input.u.mi.mouseData   = 0;
    input.u.mi.dwFlags     = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
    input.u.mi.time        = EVENT_x11_time_to_win32_time( event->time );
    input.u.mi.dwExtraInfo = 0;

    if (is_old_motion_event( event->serial ))
    {
        TRACE( "pos %d,%d old serial %lu, ignoring\n", input.u.mi.dx, input.u.mi.dy, event->serial );
        return FALSE;
    }
    send_mouse_input( hwnd, event->window, event->state, &input );
    return TRUE;
}

#ifdef HAVE_X11_EXTENSIONS_XINPUT2_H

/***********************************************************************
 *           X11DRV_DeviceChanged
 */
static BOOL X11DRV_DeviceChanged( XGenericEventCookie *xev )
{
    XIDeviceChangedEvent *event = xev->data;
    struct x11drv_thread_data *data = x11drv_thread_data();

    if (event->deviceid != data->xi2_core_pointer) return FALSE;
    if (event->reason != XISlaveSwitch) return FALSE;

    update_relative_valuators( event->classes, event->num_classes );
    data->xi2_current_slave = event->sourceid;
    return TRUE;
}

/***********************************************************************
 *           X11DRV_RawMotion
 */
static BOOL X11DRV_RawMotion( XGenericEventCookie *xev )
{
    XIRawEvent *event = xev->data;
    const double *values = event->valuators.values;
    RECT virtual_rect;
    INPUT input;
    POINT pt;
    int i;
    double dx = 0, dy = 0, val;
    struct x11drv_thread_data *thread_data = x11drv_thread_data();
    struct x11drv_valuator_data *x_rel, *y_rel;

    if (thread_data->x_rel_valuator.number < 0 || thread_data->y_rel_valuator.number < 0) return FALSE;
    if (!event->valuators.mask_len) return FALSE;
    if (thread_data->xi2_state != xi_enabled) return FALSE;

    /* If there is no slave currently detected, no previous motion nor device
     * change events were received. Look it up now on the device list in this
     * case.
     */
    if (!thread_data->xi2_current_slave)
    {
        XIDeviceInfo *devices = thread_data->xi2_devices;

        for (i = 0; i < thread_data->xi2_device_count; i++)
        {
            if (devices[i].use != XISlavePointer) continue;
            if (devices[i].deviceid != event->deviceid) continue;
            if (devices[i].attachment != thread_data->xi2_core_pointer) continue;
            thread_data->xi2_current_slave = event->deviceid;
            break;
        }
    }

    if (event->deviceid != thread_data->xi2_current_slave) return FALSE;

    x_rel = &thread_data->x_rel_valuator;
    y_rel = &thread_data->y_rel_valuator;

    input.u.mi.mouseData   = 0;
    input.u.mi.dwFlags     = MOUSEEVENTF_MOVE;
    input.u.mi.time        = EVENT_x11_time_to_win32_time( event->time );
    input.u.mi.dwExtraInfo = 0;
    input.u.mi.dx          = 0;
    input.u.mi.dy          = 0;

    virtual_rect = get_virtual_screen_rect();

    for (i = 0; i <= max ( x_rel->number, y_rel->number ); i++)
    {
        if (!XIMaskIsSet( event->valuators.mask, i ))
            continue;
        val = *values++;
        if (i == x_rel->number)
        {
            dx = val;
            if (x_rel->min < x_rel->max)
                dx = val * (virtual_rect.right - virtual_rect.left)
                         / (x_rel->max - x_rel->min);
        }
        if (i == y_rel->number)
        {
            dy = val;
            if (y_rel->min < y_rel->max)
                dy = val * (virtual_rect.bottom - virtual_rect.top)
                         / (y_rel->max - y_rel->min);
        }
    }

    /* Accumulate the *double* dx/dy motions so sub-pixel motions wont be lost
     * when sent/cast to *LONG* input.u.mi.dx/dy.
     */
    x_rel->accum += dx;
    y_rel->accum += dy;
    if (fabs(x_rel->accum) < 1.0 && fabs(y_rel->accum) < 1.0)
    {
        TRACE( "accumulating raw motion (event %f,%f, accum %f,%f)\n", dx, dy, x_rel->accum, y_rel->accum );
        return TRUE;
    }
    input.u.mi.dx = x_rel->accum;
    input.u.mi.dy = y_rel->accum;
    x_rel->accum -= input.u.mi.dx;
    y_rel->accum -= input.u.mi.dy;

    if (broken_rawevents && is_old_motion_event( xev->serial ))
    {
        TRACE( "pos %d,%d old serial %lu, ignoring\n", input.u.mi.dx, input.u.mi.dy, xev->serial );
        return FALSE;
    }

    pt.x = input.u.mi.dx;
    pt.y = input.u.mi.dy;
    fs_hack_scale_real_to_user(&pt);
    input.u.mi.dx = pt.x;
    input.u.mi.dy = pt.y;

    TRACE( "pos %d,%d (event %f,%f, accum %f,%f)\n", input.u.mi.dx, input.u.mi.dy, dx, dy, x_rel->accum, y_rel->accum );

    input.type = INPUT_MOUSE;
    __wine_send_input( 0, &input );
    return TRUE;
}

#endif /* HAVE_X11_EXTENSIONS_XINPUT2_H */


/***********************************************************************
 *              X11DRV_XInput2_Init
 */
void X11DRV_XInput2_Init(void)
{
#if defined(SONAME_LIBXI) && defined(HAVE_X11_EXTENSIONS_XINPUT2_H)
    int event, error;
    void *libxi_handle = wine_dlopen( SONAME_LIBXI, RTLD_NOW, NULL, 0 );

    if (!libxi_handle)
    {
        WARN( "couldn't load %s\n", SONAME_LIBXI );
        return;
    }
#define LOAD_FUNCPTR(f) \
    if (!(p##f = wine_dlsym( libxi_handle, #f, NULL, 0))) \
    { \
        WARN("Failed to load %s.\n", #f); \
        return; \
    }

    LOAD_FUNCPTR(XIGetClientPointer);
    LOAD_FUNCPTR(XIFreeDeviceInfo);
    LOAD_FUNCPTR(XIQueryDevice);
    LOAD_FUNCPTR(XIQueryVersion);
    LOAD_FUNCPTR(XISelectEvents);
#undef LOAD_FUNCPTR

    xinput2_available = XQueryExtension( gdi_display, "XInputExtension", &xinput2_opcode, &event, &error );

    /* Until version 1.10.4 rawinput was broken in XOrg, see
     * https://bugs.freedesktop.org/show_bug.cgi?id=30068 */
    broken_rawevents  = strstr(XServerVendor( gdi_display ), "X.Org") &&
                        XVendorRelease( gdi_display ) < 11004000;

#else
    TRACE( "X Input 2 support not compiled in.\n" );
#endif
}


/***********************************************************************
 *           X11DRV_GenericEvent
 */
BOOL X11DRV_GenericEvent( HWND hwnd, XEvent *xev )
{
    BOOL ret = FALSE;
#ifdef HAVE_X11_EXTENSIONS_XINPUT2_H
    XGenericEventCookie *event = &xev->xcookie;

    if (!event->data) return FALSE;
    if (event->extension != xinput2_opcode) return FALSE;

    switch (event->evtype)
    {
    case XI_DeviceChanged:
        ret = X11DRV_DeviceChanged( event );
        break;
    case XI_RawMotion:
        ret = X11DRV_RawMotion( event );
        break;

    default:
        TRACE( "Unhandled event %#x\n", event->evtype );
        break;
    }
#endif
    return ret;
}
