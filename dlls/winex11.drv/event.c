/*
 * X11 event driver
 *
 * Copyright 1993 Alexandre Julliard
 *	     1999 Noel Borthwick
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

#include <poll.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/Xlib.h>
#include <X11/Xresource.h>
#include <X11/Xutil.h>
#ifdef HAVE_X11_EXTENSIONS_XINPUT2_H
#include <X11/extensions/XInput2.h>
#endif

#include <assert.h>
#include <stdarg.h>
#include <string.h>

#include "windef.h"
#include "winbase.h"

#include "x11drv.h"

/* avoid conflict with field names in included win32 headers */
#undef Status
#include "shlobj.h"  /* DROPFILES */
#include "shellapi.h"

#include "wine/server.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(event);

extern BOOL ximInComposeMode;

#define DndNotDnd       -1    /* OffiX drag&drop */
#define DndUnknown      0
#define DndRawData      1
#define DndFile         2
#define DndFiles        3
#define DndText         4
#define DndDir          5
#define DndLink         6
#define DndExe          7

#define DndEND          8

#define DndURL          128   /* KDE drag&drop */

#define XEMBED_EMBEDDED_NOTIFY        0
#define XEMBED_WINDOW_ACTIVATE        1
#define XEMBED_WINDOW_DEACTIVATE      2
#define XEMBED_REQUEST_FOCUS          3
#define XEMBED_FOCUS_IN               4
#define XEMBED_FOCUS_OUT              5
#define XEMBED_FOCUS_NEXT             6
#define XEMBED_FOCUS_PREV             7
#define XEMBED_MODALITY_ON            10
#define XEMBED_MODALITY_OFF           11
#define XEMBED_REGISTER_ACCELERATOR   12
#define XEMBED_UNREGISTER_ACCELERATOR 13
#define XEMBED_ACTIVATE_ACCELERATOR   14

Bool (*pXGetEventData)( Display *display, XEvent /*XGenericEventCookie*/ *event ) = NULL;
void (*pXFreeEventData)( Display *display, XEvent /*XGenericEventCookie*/ *event ) = NULL;

  /* Event handlers */
static BOOL X11DRV_FocusIn( HWND hwnd, XEvent *event );
static BOOL X11DRV_FocusOut( HWND hwnd, XEvent *event );
static BOOL X11DRV_Expose( HWND hwnd, XEvent *event );
static BOOL X11DRV_MapNotify( HWND hwnd, XEvent *event );
static BOOL X11DRV_UnmapNotify( HWND hwnd, XEvent *event );
static BOOL X11DRV_ReparentNotify( HWND hwnd, XEvent *event );
static BOOL X11DRV_ConfigureNotify( HWND hwnd, XEvent *event );
static BOOL X11DRV_PropertyNotify( HWND hwnd, XEvent *event );
static BOOL X11DRV_ClientMessage( HWND hwnd, XEvent *event );
static BOOL X11DRV_GravityNotify( HWND hwnd, XEvent *event );

#define MAX_EVENT_HANDLERS 128

static x11drv_event_handler handlers[MAX_EVENT_HANDLERS] =
{
    NULL,                     /*  0 reserved */
    NULL,                     /*  1 reserved */
    X11DRV_KeyEvent,          /*  2 KeyPress */
    X11DRV_KeyEvent,          /*  3 KeyRelease */
    X11DRV_ButtonPress,       /*  4 ButtonPress */
    X11DRV_ButtonRelease,     /*  5 ButtonRelease */
    X11DRV_MotionNotify,      /*  6 MotionNotify */
    X11DRV_EnterNotify,       /*  7 EnterNotify */
    NULL,                     /*  8 LeaveNotify */
    X11DRV_FocusIn,           /*  9 FocusIn */
    X11DRV_FocusOut,          /* 10 FocusOut */
    X11DRV_KeymapNotify,      /* 11 KeymapNotify */
    X11DRV_Expose,            /* 12 Expose */
    NULL,                     /* 13 GraphicsExpose */
    NULL,                     /* 14 NoExpose */
    NULL,                     /* 15 VisibilityNotify */
    NULL,                     /* 16 CreateNotify */
    X11DRV_DestroyNotify,     /* 17 DestroyNotify */
    X11DRV_UnmapNotify,       /* 18 UnmapNotify */
    X11DRV_MapNotify,         /* 19 MapNotify */
    NULL,                     /* 20 MapRequest */
    X11DRV_ReparentNotify,    /* 21 ReparentNotify */
    X11DRV_ConfigureNotify,   /* 22 ConfigureNotify */
    NULL,                     /* 23 ConfigureRequest */
    X11DRV_GravityNotify,     /* 24 GravityNotify */
    NULL,                     /* 25 ResizeRequest */
    NULL,                     /* 26 CirculateNotify */
    NULL,                     /* 27 CirculateRequest */
    X11DRV_PropertyNotify,    /* 28 PropertyNotify */
    X11DRV_SelectionClear,    /* 29 SelectionClear */
    X11DRV_SelectionRequest,  /* 30 SelectionRequest */
    NULL,                     /* 31 SelectionNotify */
    NULL,                     /* 32 ColormapNotify */
    X11DRV_ClientMessage,     /* 33 ClientMessage */
    X11DRV_MappingNotify,     /* 34 MappingNotify */
    X11DRV_GenericEvent       /* 35 GenericEvent */
};

static const char * event_names[MAX_EVENT_HANDLERS] =
{
    NULL, NULL, "KeyPress", "KeyRelease", "ButtonPress", "ButtonRelease",
    "MotionNotify", "EnterNotify", "LeaveNotify", "FocusIn", "FocusOut",
    "KeymapNotify", "Expose", "GraphicsExpose", "NoExpose", "VisibilityNotify",
    "CreateNotify", "DestroyNotify", "UnmapNotify", "MapNotify", "MapRequest",
    "ReparentNotify", "ConfigureNotify", "ConfigureRequest", "GravityNotify", "ResizeRequest",
    "CirculateNotify", "CirculateRequest", "PropertyNotify", "SelectionClear", "SelectionRequest",
    "SelectionNotify", "ColormapNotify", "ClientMessage", "MappingNotify", "GenericEvent"
};

int xinput2_opcode = 0;

/* return the name of an X event */
static const char *dbgstr_event( int type )
{
    if (type < MAX_EVENT_HANDLERS && event_names[type]) return event_names[type];
    return wine_dbg_sprintf( "Unknown event %d", type );
}

static inline void get_event_data( XEvent *event )
{
#if defined(GenericEvent) && defined(HAVE_XEVENT_XCOOKIE)
    if (event->xany.type != GenericEvent) return;
    if (!pXGetEventData || !pXGetEventData( event->xany.display, event )) event->xcookie.data = NULL;
#endif
}

static inline void free_event_data( XEvent *event )
{
#if defined(GenericEvent) && defined(HAVE_XEVENT_XCOOKIE)
    if (event->xany.type != GenericEvent) return;
    if (event->xcookie.data) pXFreeEventData( event->xany.display, event );
#endif
}

/***********************************************************************
 *           xembed_request_focus
 */
static void xembed_request_focus( Display *display, Window window, DWORD timestamp )
{
    XEvent xev;

    xev.xclient.type = ClientMessage;
    xev.xclient.window = window;
    xev.xclient.message_type = x11drv_atom(_XEMBED);
    xev.xclient.serial = 0;
    xev.xclient.display = display;
    xev.xclient.send_event = True;
    xev.xclient.format = 32;

    xev.xclient.data.l[0] = timestamp;
    xev.xclient.data.l[1] = XEMBED_REQUEST_FOCUS;
    xev.xclient.data.l[2] = 0;
    xev.xclient.data.l[3] = 0;
    xev.xclient.data.l[4] = 0;

    XSendEvent(display, window, False, NoEventMask, &xev);
    XFlush( display );
}

/***********************************************************************
 *           X11DRV_register_event_handler
 *
 * Register a handler for a given event type.
 * If already registered, overwrite the previous handler.
 */
void X11DRV_register_event_handler( int type, x11drv_event_handler handler, const char *name )
{
    assert( type < MAX_EVENT_HANDLERS );
    assert( !handlers[type] || handlers[type] == handler );
    handlers[type] = handler;
    event_names[type] = name;
    TRACE("registered handler %p for event %d %s\n", handler, type, debugstr_a(name) );
}


/***********************************************************************
 *           filter_event
 */
static Bool filter_event( Display *display, XEvent *event, char *arg )
{
    ULONG_PTR mask = (ULONG_PTR)arg;

    if ((mask & QS_ALLINPUT) == QS_ALLINPUT) return 1;

    switch(event->type)
    {
    case KeyPress:
    case KeyRelease:
    case KeymapNotify:
    case MappingNotify:
        return (mask & (QS_KEY|QS_HOTKEY)) != 0;
    case ButtonPress:
    case ButtonRelease:
        return (mask & QS_MOUSEBUTTON) != 0;
#ifdef GenericEvent
    case GenericEvent:
#ifdef HAVE_X11_EXTENSIONS_XINPUT2_H
        if (event->xcookie.extension == xinput2_opcode)
        {
            switch (event->xcookie.evtype)
            {
            case XI_RawButtonPress:
            case XI_RawButtonRelease:
                return (mask & QS_MOUSEBUTTON) != 0;
            case XI_RawMotion:
            case XI_RawTouchBegin:
            case XI_RawTouchUpdate:
            case XI_RawTouchEnd:
                return (mask & QS_INPUT) != 0;
            case XI_DeviceChanged:
                return (mask & (QS_INPUT|QS_MOUSEBUTTON)) != 0;
            }
        }
#endif
        return (mask & QS_SENDMESSAGE) != 0;
#endif
    case MotionNotify:
    case EnterNotify:
    case LeaveNotify:
        return (mask & QS_MOUSEMOVE) != 0;
    case Expose:
        return (mask & QS_PAINT) != 0;
    case FocusIn:
    case FocusOut:
    case MapNotify:
    case UnmapNotify:
    case ConfigureNotify:
    case PropertyNotify:
    case ClientMessage:
        return (mask & QS_POSTMESSAGE) != 0;
    default:
        return (mask & QS_SENDMESSAGE) != 0;
    }
}


enum event_merge_action
{
    MERGE_DISCARD,  /* discard the old event */
    MERGE_HANDLE,   /* handle the old event */
    MERGE_KEEP,     /* keep the old event for future merging */
    MERGE_IGNORE    /* ignore the new event, keep the old one */
};

/***********************************************************************
 *           merge_raw_motion_events
 */
#ifdef HAVE_X11_EXTENSIONS_XINPUT2_H
static enum event_merge_action merge_raw_motion_events( XIRawEvent *prev, XIRawEvent *next )
{
    int i, j, k;
    unsigned char mask;

    if (!prev->valuators.mask_len) return MERGE_HANDLE;
    if (!next->valuators.mask_len) return MERGE_HANDLE;

    mask = prev->valuators.mask[0] | next->valuators.mask[0];
    if (mask == next->valuators.mask[0])  /* keep next */
    {
        for (i = j = k = 0; i < 8; i++)
        {
            if (XIMaskIsSet( prev->valuators.mask, i ))
                next->valuators.values[j] += prev->valuators.values[k++];
            if (XIMaskIsSet( next->valuators.mask, i )) j++;
        }
        TRACE( "merging duplicate GenericEvent\n" );
        return MERGE_DISCARD;
    }
    if (mask == prev->valuators.mask[0])  /* keep prev */
    {
        for (i = j = k = 0; i < 8; i++)
        {
            if (XIMaskIsSet( next->valuators.mask, i ))
                prev->valuators.values[j] += next->valuators.values[k++];
            if (XIMaskIsSet( prev->valuators.mask, i )) j++;
        }
        TRACE( "merging duplicate GenericEvent\n" );
        return MERGE_IGNORE;
    }
    /* can't merge events with disjoint masks */
    return MERGE_HANDLE;
}
#endif

static int try_grab_pointer( Display *display )
{
    if (!grab_pointer)
        return 1;

    /* if we are already clipping the cursor in the current thread, we should not
     * call XGrabPointer here or it would change the confine-to window. */
    if (clipping_cursor && x11drv_thread_data()->clip_hwnd)
        return 1;

    if (XGrabPointer( display, root_window, False, 0, GrabModeAsync, GrabModeAsync,
                      None, None, CurrentTime ) != GrabSuccess)
        return 0;

    XUngrabPointer( display, CurrentTime );
    XFlush( display );
    return 1;
}

/***********************************************************************
 *           merge_events
 *
 * Try to merge 2 consecutive events.
 */
static enum event_merge_action merge_events( XEvent *prev, XEvent *next )
{
#ifdef HAVE_X11_EXTENSIONS_XINPUT2_H
    struct x11drv_thread_data *thread_data = x11drv_thread_data();
#endif

    switch (prev->type)
    {
    case ConfigureNotify:
        switch (next->type)
        {
        case ConfigureNotify:
            if (prev->xany.window == next->xany.window)
            {
                TRACE( "discarding duplicate ConfigureNotify for window %lx\n", prev->xany.window );
                return MERGE_DISCARD;
            }
            break;
        case Expose:
        case PropertyNotify:
            return MERGE_KEEP;
        }
        break;
    case MotionNotify:
        switch (next->type)
        {
        case MotionNotify:
            if (prev->xany.window == next->xany.window)
            {
                TRACE( "discarding duplicate MotionNotify for window %lx\n", prev->xany.window );
                return MERGE_DISCARD;
            }
            break;
#ifdef HAVE_X11_EXTENSIONS_XINPUT2_H
        case GenericEvent:
            if (next->xcookie.extension != xinput2_opcode) break;
            if (next->xcookie.evtype != XI_RawMotion) break;
            if (thread_data->xi2_rawinput_only) break;
            if (thread_data->warp_serial) break;
            return MERGE_KEEP;
        }
        break;
    case GenericEvent:
        if (prev->xcookie.extension != xinput2_opcode) break;
        if (prev->xcookie.evtype != XI_RawMotion) break;
        if (thread_data->xi2_rawinput_only) break;
        switch (next->type)
        {
        case GenericEvent:
            if (next->xcookie.extension != xinput2_opcode) break;
            if (next->xcookie.evtype != XI_RawMotion) break;
            if (thread_data->warp_serial) break;
            return merge_raw_motion_events( prev->xcookie.data, next->xcookie.data );
#endif
        }
        break;
    }
    return MERGE_HANDLE;
}


/***********************************************************************
 *           call_event_handler
 */
static inline BOOL call_event_handler( Display *display, XEvent *event )
{
    HWND hwnd;
    XEvent *prev;
    struct x11drv_thread_data *thread_data;
    BOOL ret;

    if (!handlers[event->type])
    {
        TRACE( "%s for win %lx, ignoring\n", dbgstr_event( event->type ), event->xany.window );
        return FALSE;  /* no handler, ignore it */
    }

#ifdef GenericEvent
    if (event->type == GenericEvent) hwnd = 0; else
#endif
    if (XFindContext( display, event->xany.window, winContext, (char **)&hwnd ) != 0)
        hwnd = 0;  /* not for a registered window */
    if (!hwnd && event->xany.window == root_window) hwnd = GetDesktopWindow();

    TRACE( "%lu %s for hwnd/window %p/%lx\n",
           event->xany.serial, dbgstr_event( event->type ), hwnd, event->xany.window );
    thread_data = x11drv_thread_data();
    prev = thread_data->current_event;
    thread_data->current_event = event;
    ret = handlers[event->type]( hwnd, event );
    thread_data->current_event = prev;
    return ret;
}


/***********************************************************************
 *           process_events
 */
static BOOL process_events( Display *display, Bool (*filter)(Display*, XEvent*,XPointer), ULONG_PTR arg )
{
    XEvent event, prev_event;
    int count = 0;
    BOOL queued = FALSE, overlay_enabled = FALSE, steam_keyboard_opened = FALSE;
    enum event_merge_action action = MERGE_DISCARD;
    ULONG_PTR overlay_filter = QS_KEY | QS_MOUSEBUTTON | QS_MOUSEMOVE;
    ULONG_PTR keyboard_filter = QS_MOUSEBUTTON | QS_MOUSEMOVE;

    if (WaitForSingleObject(steam_overlay_event, 0) == WAIT_OBJECT_0)
        overlay_enabled = TRUE;
    if (WaitForSingleObject(steam_keyboard_event, 0) == WAIT_OBJECT_0)
        steam_keyboard_opened = TRUE;

    prev_event.type = 0;
    while (XCheckIfEvent( display, &event, filter, (char *)arg ))
    {
        count++;
        if (overlay_enabled && filter_event( display, &event, (char *)overlay_filter )) continue;
        if (steam_keyboard_opened && filter_event( display, &event, (char *)keyboard_filter )) continue;
        if (XFilterEvent( &event, None ))
        {
            /*
             * SCIM on linux filters key events strangely. It does not filter the
             * KeyPress events for these keys however it does filter the
             * KeyRelease events. This causes wine to become very confused as
             * to the keyboard state.
             *
             * We need to let those KeyRelease events be processed so that the
             * keyboard state is correct.
             */
            if (event.type == KeyRelease)
            {
                KeySym keysym = 0;
                XKeyEvent *keyevent = &event.xkey;

                XLookupString(keyevent, NULL, 0, &keysym, NULL);
                if (!(keysym == XK_Shift_L ||
                    keysym == XK_Shift_R ||
                    keysym == XK_Control_L ||
                    keysym == XK_Control_R ||
                    keysym == XK_Alt_R ||
                    keysym == XK_Alt_L ||
                    keysym == XK_Meta_R ||
                    keysym == XK_Meta_L))
                        continue; /* not a key we care about, ignore it */
            }
            else
                continue;  /* filtered, ignore it */
        }
        get_event_data( &event );
        if (prev_event.type) action = merge_events( &prev_event, &event );
        switch( action )
        {
        case MERGE_HANDLE:  /* handle prev, keep new */
            queued |= call_event_handler( display, &prev_event );
            /* fall through */
        case MERGE_DISCARD:  /* discard prev, keep new */
            free_event_data( &prev_event );
            prev_event = event;
            break;
        case MERGE_KEEP:  /* handle new, keep prev for future merging */
            queued |= call_event_handler( display, &event );
            /* fall through */
        case MERGE_IGNORE: /* ignore new, keep prev for future merging */
            free_event_data( &event );
            break;
        }
    }
    if (prev_event.type) queued |= call_event_handler( display, &prev_event );
    free_event_data( &prev_event );
    XFlush( gdi_display );
    if (count) TRACE( "processed %d events, returning %d\n", count, queued );
    return queued;
}


/***********************************************************************
 *           MsgWaitForMultipleObjectsEx   (X11DRV.@)
 */
DWORD CDECL X11DRV_MsgWaitForMultipleObjectsEx( DWORD count, const HANDLE *handles,
                                                DWORD timeout, DWORD mask, DWORD flags )
{
    DWORD ret;
    struct x11drv_thread_data *data = TlsGetValue( thread_data_tls_index );

    if (!data)
    {
        if (!count && !timeout) return WAIT_TIMEOUT;
        return WaitForMultipleObjectsEx( count, handles, flags & MWMO_WAITALL,
                                         timeout, flags & MWMO_ALERTABLE );
    }

    if (data->current_event) mask = 0;  /* don't process nested events */

    if (process_events( data->display, filter_event, mask )) ret = count - 1;
    else if (count || timeout)
    {
        ret = WaitForMultipleObjectsEx( count, handles, flags & MWMO_WAITALL,
                                        timeout, flags & MWMO_ALERTABLE );
        if (ret == count - 1) process_events( data->display, filter_event, mask );
    }
    else ret = WAIT_TIMEOUT;

    return ret;
}

/***********************************************************************
 *           x11drv_time_to_ticks
 *
 * Make our timer and the X timer line up as best we can
 *  Pass 0 to retrieve the current adjustment value (times -1)
 */
DWORD x11drv_time_to_ticks( Time time )
{
  static DWORD adjust = 0;
  DWORD now = GetTickCount();
  DWORD ret;

  if (! adjust && time != 0)
  {
    ret = now;
    adjust = time - now;
  }
  else
  {
      /* If we got an event in the 'future', then our clock is clearly wrong. 
         If we got it more than 10000 ms in the future, then it's most likely
         that the clock has wrapped.  */

      ret = time - adjust;
      if (ret > now && ((ret - now) < 10000) && time != 0)
      {
        adjust += ret - now;
        ret    -= ret - now;
      }
  }

  return ret;

}

/*******************************************************************
 *         can_activate_window
 *
 * Check if we can activate the specified window.
 */
static inline BOOL can_activate_window( HWND hwnd )
{
    LONG style = GetWindowLongW( hwnd, GWL_STYLE );
    RECT rect;

    if (!(style & WS_VISIBLE)) return FALSE;
    if ((style & (WS_POPUP|WS_CHILD)) == WS_CHILD) return FALSE;
    if (style & WS_MINIMIZE) return FALSE;
    if (GetWindowLongW( hwnd, GWL_EXSTYLE ) & WS_EX_NOACTIVATE) return FALSE;
    if (hwnd == GetDesktopWindow()) return FALSE;
    if (GetWindowRect( hwnd, &rect ) && IsRectEmpty( &rect )) return FALSE;
    return !(style & WS_DISABLED);
}


/**********************************************************************
 *              set_input_focus
 *
 * Try to force focus for embedded or non-managed windows.
 */
static void set_input_focus( struct x11drv_win_data *data )
{
    XWindowChanges changes;
    DWORD timestamp;

    if (!data->whole_window) return;

    if (x11drv_time_to_ticks(0))
        /* ICCCM says don't use CurrentTime, so try to use last message time if possible */
        /* FIXME: this is not entirely correct */
        timestamp = GetMessageTime() - x11drv_time_to_ticks(0);
    else
        timestamp = CurrentTime;

    /* Set X focus and install colormap */
    changes.stack_mode = Above;
    XConfigureWindow( data->display, data->whole_window, CWStackMode, &changes );

    if (data->embedder)
        xembed_request_focus( data->display, data->embedder, timestamp );
    else
        XSetInputFocus( data->display, data->whole_window, RevertToParent, timestamp );

}

/**********************************************************************
 *              set_focus
 */
static void set_focus( XEvent *xev, HWND hwnd, Time time )
{
    HWND focus;
    Window win;
    GUITHREADINFO threadinfo;

    if (!try_grab_pointer( xev->xany.display ))
    {
        /* ask the foreground window to release its grab before trying to get ours */
        SendMessageW( GetForegroundWindow(), WM_X11DRV_RELEASE_CURSOR, 0, 0 );
        XSendEvent( xev->xany.display, xev->xany.window, False, 0, xev );
        return;
    }
    else
    {
        TRACE( "setting foreground window to %p\n", hwnd );
        SetForegroundWindow( hwnd );
    }

    threadinfo.cbSize = sizeof(threadinfo);
    GetGUIThreadInfo(0, &threadinfo);
    focus = threadinfo.hwndFocus;
    if (!focus) focus = threadinfo.hwndActive;
    if (focus) focus = GetAncestor( focus, GA_ROOT );
    win = X11DRV_get_whole_window(focus);

    if (win)
    {
        TRACE( "setting focus to %p (%lx) time=%ld\n", focus, win, time );
        XSetInputFocus( xev->xany.display, win, RevertToParent, time );
    }
}


/**********************************************************************
 *              handle_manager_message
 */
static void handle_manager_message( HWND hwnd, XEvent *xev )
{
    XClientMessageEvent *event = &xev->xclient;

    if (hwnd != GetDesktopWindow()) return;
    if (systray_atom && event->data.l[1] == systray_atom)
        change_systray_owner( event->display, event->data.l[2] );
}


/**********************************************************************
 *              handle_wm_protocols
 */
static void handle_wm_protocols( HWND hwnd, XEvent *xev )
{
    XClientMessageEvent *event = &xev->xclient;
    Atom protocol = (Atom)event->data.l[0];
    Time event_time = (Time)event->data.l[1];

    if (!protocol) return;

    if (protocol == x11drv_atom(WM_DELETE_WINDOW))
    {
        update_user_time( event_time );

        if (hwnd == GetDesktopWindow())
        {
            /* The desktop window does not have a close button that we can
             * pretend to click. Therefore, we simply send it a close command. */
            SendMessageW(hwnd, WM_SYSCOMMAND, SC_CLOSE, 0);
            return;
        }

        /* Ignore the delete window request if the window has been disabled
         * and we are in managed mode. This is to disallow applications from
         * being closed by the window manager while in a modal state.
         */
        if (IsWindowEnabled(hwnd))
        {
            HMENU hSysMenu;

            if (GetClassLongW(hwnd, GCL_STYLE) & CS_NOCLOSE) return;
            hSysMenu = GetSystemMenu(hwnd, FALSE);
            if (hSysMenu)
            {
                UINT state = GetMenuState(hSysMenu, SC_CLOSE, MF_BYCOMMAND);
                if (state == 0xFFFFFFFF || (state & (MF_DISABLED | MF_GRAYED)))
                    return;
            }
            if (GetActiveWindow() != hwnd)
            {
                LRESULT ma = SendMessageW( hwnd, WM_MOUSEACTIVATE,
                                           (WPARAM)GetAncestor( hwnd, GA_ROOT ),
                                           MAKELPARAM( HTCLOSE, WM_NCLBUTTONDOWN ) );
                switch(ma)
                {
                    case MA_NOACTIVATEANDEAT:
                    case MA_ACTIVATEANDEAT:
                        return;
                    case MA_NOACTIVATE:
                        break;
                    case MA_ACTIVATE:
                    case 0:
                        SetActiveWindow(hwnd);
                        break;
                    default:
                        WARN( "unknown WM_MOUSEACTIVATE code %d\n", (int) ma );
                        break;
                }
            }

            PostMessageW( hwnd, WM_SYSCOMMAND, SC_CLOSE, 0 );
        }
    }
    else if (protocol == x11drv_atom(WM_TAKE_FOCUS))
    {
        HWND last_focus = x11drv_thread_data()->last_focus;

        TRACE( "got take focus msg for %p, enabled=%d, visible=%d (style %08x), focus=%p, active=%p, fg=%p, last=%p\n",
               hwnd, IsWindowEnabled(hwnd), IsWindowVisible(hwnd), GetWindowLongW(hwnd, GWL_STYLE),
               GetFocus(), GetActiveWindow(), GetForegroundWindow(), last_focus );

        if (can_activate_window(hwnd))
        {
            /* simulate a mouse click on the menu to find out
             * whether the window wants to be activated */
            LRESULT ma = SendMessageW( hwnd, WM_MOUSEACTIVATE,
                                       (WPARAM)GetAncestor( hwnd, GA_ROOT ),
                                       MAKELONG( HTMENU, WM_LBUTTONDOWN ) );
            if (ma != MA_NOACTIVATEANDEAT && ma != MA_NOACTIVATE)
            {
                set_focus( xev, hwnd, event_time );
                return;
            }
        }
        else if (hwnd == GetDesktopWindow())
        {
            hwnd = GetForegroundWindow();
            if (!hwnd) hwnd = last_focus;
            if (!hwnd) hwnd = GetDesktopWindow();
            set_focus( xev, hwnd, event_time );
            return;
        }
        /* try to find some other window to give the focus to */
        hwnd = GetFocus();
        if (hwnd) hwnd = GetAncestor( hwnd, GA_ROOT );
        if (!hwnd) hwnd = GetActiveWindow();
        if (!hwnd) hwnd = last_focus;
        if (hwnd && can_activate_window(hwnd)) set_focus( xev, hwnd, event_time );
    }
    else if (protocol == x11drv_atom(_NET_WM_PING))
    {
      XClientMessageEvent xev;
      xev = *event;
      
      TRACE("NET_WM Ping\n");
      xev.window = DefaultRootWindow(xev.display);
      XSendEvent(xev.display, xev.window, False, SubstructureRedirectMask | SubstructureNotifyMask, (XEvent*)&xev);
    }
}


static const char * const focus_details[] =
{
    "NotifyAncestor",
    "NotifyVirtual",
    "NotifyInferior",
    "NotifyNonlinear",
    "NotifyNonlinearVirtual",
    "NotifyPointer",
    "NotifyPointerRoot",
    "NotifyDetailNone"
};

static const char * const focus_modes[] =
{
    "NotifyNormal",
    "NotifyGrab",
    "NotifyUngrab",
    "NotifyWhileGrabbed"
};

/**********************************************************************
 *              X11DRV_FocusIn
 */
static BOOL X11DRV_FocusIn( HWND hwnd, XEvent *xev )
{
    XFocusChangeEvent *event = &xev->xfocus;
    XIC xic;

    if (!hwnd) return FALSE;

    TRACE( "win %p xwin %lx detail=%s mode=%s\n", hwnd, event->window, focus_details[event->detail], focus_modes[event->mode] );

    if (event->detail == NotifyPointer) return FALSE;
    if (hwnd == GetDesktopWindow()) return FALSE;

    x11drv_thread_data()->keymapnotify_hwnd = hwnd;

    /* Focus was just restored but it can be right after super was
     * pressed and gnome-shell needs a bit of time to respond and
     * toggle the activity view. If we grab the cursor right away
     * it will cancel it and super key will do nothing.
     */
    if (event->mode == NotifyUngrab && wm_is_mutter(event->display))
        Sleep(100);

    if (!try_grab_pointer( event->display ))
    {
        /* ask the desktop window to release its grab before trying to get ours */
        SendMessageW( GetDesktopWindow(), WM_X11DRV_RELEASE_CURSOR, 0, 0 );
        XSendEvent( event->display, event->window, False, 0, xev );
        return FALSE;
    }

    /* ask the foreground window to re-apply the current ClipCursor rect */
    if (!SendMessageTimeoutW( GetForegroundWindow(), WM_X11DRV_CLIP_CURSOR_REQUEST, 0, 0,
                              SMTO_NOTIMEOUTIFNOTHUNG, 500, NULL ) && GetLastError() == ERROR_TIMEOUT)
        ERR( "WM_X11DRV_CLIP_CURSOR_REQUEST timed out.\n" );

    /* ignore wm specific NotifyUngrab / NotifyGrab events w.r.t focus */
    if (event->mode == NotifyGrab || event->mode == NotifyUngrab) return FALSE;

    if ((xic = X11DRV_get_ic( hwnd ))) XSetICFocus( xic );
    if (use_take_focus)
    {
        if (hwnd == GetForegroundWindow()) clip_fullscreen_window( hwnd, FALSE );
        return TRUE;
    }

    if (!can_activate_window(hwnd))
    {
        HWND hwnd = GetFocus();
        if (hwnd) hwnd = GetAncestor( hwnd, GA_ROOT );
        if (!hwnd) hwnd = GetActiveWindow();
        if (!hwnd) hwnd = x11drv_thread_data()->last_focus;
        if (hwnd && can_activate_window(hwnd)) set_focus( xev, hwnd, CurrentTime );
        return TRUE;
    }

    SetForegroundWindow( hwnd );
    return TRUE;
}

/**********************************************************************
 *              focus_out
 */
static void focus_out( Display *display , HWND hwnd )
 {
    HWND hwnd_tmp;
    Window focus_win;
    int revert;
    XIC xic;
    struct x11drv_win_data *data;

    if (ximInComposeMode) return;

    data = get_win_data(hwnd);
    if(data){
        ULONGLONG now = GetTickCount64();
        if(data->take_focus_back > 0 &&
                now >= data->take_focus_back &&
                now - data->take_focus_back < 1000){
            data->take_focus_back = 0;
            TRACE("workaround mutter bug, taking focus back\n");
            XSetInputFocus( data->display, data->whole_window, RevertToParent, CurrentTime);
            release_win_data(data);
            /* don't inform win32 client */
            return;
        }
        data->take_focus_back = 0;
        release_win_data(data);
    }

    x11drv_thread_data()->last_focus = hwnd;
    if ((xic = X11DRV_get_ic( hwnd ))) XUnsetICFocus( xic );

    if (is_virtual_desktop())
    {
        if (hwnd == GetDesktopWindow()) reset_clipping_window();
        return;
    }
    if (hwnd != GetForegroundWindow()) return;
    SendMessageW( hwnd, WM_CANCELMODE, 0, 0 );

    /* don't reset the foreground window, if the window which is
       getting the focus is a Wine window */

    XGetInputFocus( display, &focus_win, &revert );
    if (focus_win)
    {
        if (XFindContext( display, focus_win, winContext, (char **)&hwnd_tmp ) != 0)
            focus_win = 0;
    }

    if (!focus_win)
    {
        /* Abey : 6-Oct-99. Check again if the focus out window is the
           Foreground window, because in most cases the messages sent
           above must have already changed the foreground window, in which
           case we don't have to change the foreground window to 0 */
        if (hwnd == GetForegroundWindow())
        {
            TRACE( "lost focus, setting fg to desktop\n" );
            SetForegroundWindow( GetDesktopWindow() );
        }
    }
 }

/**********************************************************************
 *              X11DRV_FocusOut
 *
 * Note: only top-level windows get FocusOut events.
 */
static BOOL X11DRV_FocusOut( HWND hwnd, XEvent *xev )
{
    XFocusChangeEvent *event = &xev->xfocus;

    TRACE( "win %p xwin %lx detail=%s mode=%s\n", hwnd, event->window, focus_details[event->detail], focus_modes[event->mode] );

    if (event->detail == NotifyPointer)
    {
        if (!hwnd && event->window == x11drv_thread_data()->clip_window) reset_clipping_window();
        return TRUE;
    }
    if (!hwnd) return FALSE;

    if (hwnd == GetForegroundWindow()) ungrab_clipping_window();

    /* ignore wm specific NotifyUngrab / NotifyGrab events w.r.t focus */
    if (event->mode == NotifyGrab || event->mode == NotifyUngrab) return FALSE;

    focus_out( event->display, hwnd );
    return TRUE;
}


/***********************************************************************
 *           X11DRV_Expose
 */
static BOOL X11DRV_Expose( HWND hwnd, XEvent *xev )
{
    XExposeEvent *event = &xev->xexpose;
    RECT rect, abs_rect;
    POINT pos;
    struct x11drv_win_data *data;
    HRGN surface_region = 0;
    UINT flags = RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_ALLCHILDREN;

    TRACE( "win %p (%lx) %d,%d %dx%d\n",
           hwnd, event->window, event->x, event->y, event->width, event->height );

    if (event->window != root_window)
    {
        pos.x = event->x;
        pos.y = event->y;
    }
    else pos = root_to_virtual_screen( event->x, event->y );

    if (!(data = get_win_data( hwnd ))) return FALSE;

    rect.left   = pos.x;
    rect.top    = pos.y;
    rect.right  = pos.x + event->width;
    rect.bottom = pos.y + event->height;

    if (layered_window_client_hack && event->window == data->client_window)
        OffsetRect( &rect, data->client_rect.left - data->whole_rect.left,
                    data->client_rect.top - data->whole_rect.top );
    if (layered_window_client_hack || event->window != data->client_window)
    {
        if (data->surface)
        {
            surface_region = expose_surface( data->surface, &rect );
            if (!surface_region) flags = 0;
            else OffsetRgn( surface_region, data->whole_rect.left - data->client_rect.left,
                            data->whole_rect.top - data->client_rect.top );

            if (data->vis.visualid != default_visual.visualid)
                data->surface->funcs->flush( data->surface );
        }
        OffsetRect( &rect, data->whole_rect.left - data->client_rect.left,
                    data->whole_rect.top - data->client_rect.top );
    }

    if (event->window != root_window)
    {
        if (GetWindowLongW( data->hwnd, GWL_EXSTYLE ) & WS_EX_LAYOUTRTL)
            mirror_rect( &data->client_rect, &rect );
        abs_rect = rect;
        MapWindowPoints( hwnd, 0, (POINT *)&abs_rect, 2 );

        SERVER_START_REQ( update_window_zorder )
        {
            req->window      = wine_server_user_handle( hwnd );
            req->rect.left   = abs_rect.left;
            req->rect.top    = abs_rect.top;
            req->rect.right  = abs_rect.right;
            req->rect.bottom = abs_rect.bottom;
            wine_server_call( req );
        }
        SERVER_END_REQ;
    }
    else flags &= ~RDW_ALLCHILDREN;

    release_win_data( data );

    if (flags) RedrawWindow( hwnd, &rect, surface_region, flags );
    if (surface_region) DeleteObject( surface_region );
    return TRUE;
}


/**********************************************************************
 *		X11DRV_MapNotify
 */
static BOOL X11DRV_MapNotify( HWND hwnd, XEvent *event )
{
    struct x11drv_win_data *data;

    if (event->xany.window == x11drv_thread_data()->clip_window) return TRUE;

    if (!(data = get_win_data( hwnd ))) return FALSE;

    if (!data->managed && !data->embedded && data->mapped)
    {
        HWND hwndFocus = GetFocus();
        if (hwndFocus && IsChild( hwnd, hwndFocus ))
            set_input_focus( data );
    }
    release_win_data( data );
    return TRUE;
}


/**********************************************************************
 *		X11DRV_UnmapNotify
 */
static BOOL X11DRV_UnmapNotify( HWND hwnd, XEvent *event )
{
    return TRUE;
}


/***********************************************************************
 *           reparent_notify
 */
static void reparent_notify( Display *display, HWND hwnd, Window xparent, int x, int y )
{
    HWND parent, old_parent;
    DWORD style;

    style = GetWindowLongW( hwnd, GWL_STYLE );
    if (xparent == root_window)
    {
        parent = GetDesktopWindow();
        style = (style & ~WS_CHILD) | WS_POPUP;
    }
    else
    {
        if (!(parent = create_foreign_window( display, xparent ))) return;
        style = (style & ~WS_POPUP) | WS_CHILD;
    }

    ShowWindow( hwnd, SW_HIDE );
    old_parent = SetParent( hwnd, parent );
    SetWindowLongW( hwnd, GWL_STYLE, style );
    SetWindowPos( hwnd, HWND_TOP, x, y, 0, 0,
                  SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOCOPYBITS |
                  ((style & WS_VISIBLE) ? SWP_SHOWWINDOW : 0) );

    /* make old parent destroy itself if it no longer has children */
    if (old_parent != GetDesktopWindow()) PostMessageW( old_parent, WM_CLOSE, 0, 0 );
}


/***********************************************************************
 *           X11DRV_ReparentNotify
 */
static BOOL X11DRV_ReparentNotify( HWND hwnd, XEvent *xev )
{
    XReparentEvent *event = &xev->xreparent;
    struct x11drv_win_data *data;

    if (!(data = get_win_data( hwnd ))) return FALSE;

    if (!data->embedded)
    {
        release_win_data( data );
        return FALSE;
    }

    if (data->whole_window)
    {
        if (event->parent == root_window)
        {
            TRACE( "%p/%lx reparented to root\n", hwnd, data->whole_window );
            data->embedder = 0;
            release_win_data( data );
            SendMessageW( hwnd, WM_CLOSE, 0, 0 );
            return TRUE;
        }
        data->embedder = event->parent;
    }

    TRACE( "%p/%lx reparented to %lx\n", hwnd, data->whole_window, event->parent );
    release_win_data( data );

    reparent_notify( event->display, hwnd, event->parent, event->x, event->y );
    return TRUE;
}


/***********************************************************************
 *		X11DRV_ConfigureNotify
 */
static BOOL X11DRV_ConfigureNotify( HWND hwnd, XEvent *xev )
{
    XConfigureEvent *event = &xev->xconfigure;
    struct x11drv_win_data *data;
    RECT rect;
    POINT pos;
    UINT flags;
    HWND parent;
    BOOL root_coords;
    int cx, cy, x = event->x, y = event->y;
    DWORD style;

    if (!hwnd) return FALSE;
    if (!(data = get_win_data( hwnd ))) return FALSE;
    if (!data->mapped || data->iconic) goto done;
    if (data->whole_window && !data->managed) goto done;
    /* ignore synthetic events on foreign windows */
    if (event->send_event && !data->whole_window) goto done;
    if (data->configure_serial && (long)(data->configure_serial - event->serial) > 0)
    {
        TRACE( "win %p/%lx event %d,%d,%dx%d ignoring old serial %lu/%lu\n",
               hwnd, data->whole_window, event->x, event->y, event->width, event->height,
               event->serial, data->configure_serial );
        goto done;
    }
    if (data->pending_fullscreen)
    {
        TRACE( "win %p/%lx event %d,%d,%dx%d pending_fullscreen is pending, so ignoring\n",
               hwnd, data->whole_window, event->x, event->y, event->width, event->height );
        goto done;
    }

    /* Get geometry */

    parent = GetAncestor( hwnd, GA_PARENT );
    root_coords = event->send_event;  /* synthetic events are always in root coords */

    if (!root_coords && parent == GetDesktopWindow()) /* normal event, map coordinates to the root */
    {
        Window child;
        XTranslateCoordinates( event->display, event->window, root_window,
                               0, 0, &x, &y, &child );
        root_coords = TRUE;
    }

    if (!root_coords)
    {
        pos.x = x;
        pos.y = y;
    }
    else pos = root_to_virtual_screen( x, y );

    if(data->fs_hack){
        MONITORINFO monitor_info;
        HMONITOR monitor;

        monitor = fs_hack_monitor_from_hwnd( hwnd );
        monitor_info.cbSize = sizeof(monitor_info);
        GetMonitorInfoW( monitor, &monitor_info );
        rect = monitor_info.rcMonitor;
        TRACE( "monitor %p rect: %s\n", monitor, wine_dbgstr_rect(&rect) );
    }else{
        X11DRV_X_to_window_rect( data, &rect, pos.x, pos.y, event->width, event->height );
        if (root_coords) MapWindowPoints( 0, parent, (POINT *)&rect, 2 );
    }

    TRACE( "win %p/%lx new X rect %d,%d,%dx%d (event %d,%d,%dx%d)\n",
           hwnd, data->whole_window, rect.left, rect.top, rect.right-rect.left, rect.bottom-rect.top,
           event->x, event->y, event->width, event->height );

    /* Compare what has changed */

    {
        const char *steamgameid = getenv("SteamGameId");
        if(steamgameid && !strcmp(steamgameid, "590380")){
            /* Into The Breach is extremely picky about the size of its window. */
            if(is_window_rect_full_screen(&data->whole_rect) &&
                    is_window_rect_full_screen(&rect)){
                TRACE("window is fullscreen and new size is also fullscreen, so preserving window size\n");
                rect.right = rect.left + (data->whole_rect.right - data->whole_rect.left);
                rect.bottom = rect.top + (data->whole_rect.bottom - data->whole_rect.top);
            }
        }
    }

    x     = rect.left;
    y     = rect.top;
    cx    = rect.right - rect.left;
    cy    = rect.bottom - rect.top;
    flags = SWP_NOACTIVATE | SWP_NOZORDER;

    if (!data->whole_window) flags |= SWP_NOCOPYBITS;  /* we can't copy bits of foreign windows */

    if (data->window_rect.left == x && data->window_rect.top == y) flags |= SWP_NOMOVE;
    else
        TRACE( "%p moving from (%d,%d) to (%d,%d)\n",
               hwnd, data->window_rect.left, data->window_rect.top, x, y );

    if ((data->window_rect.right - data->window_rect.left == cx &&
         data->window_rect.bottom - data->window_rect.top == cy) ||
        IsRectEmpty( &data->window_rect ))
        flags |= SWP_NOSIZE;
    else
        TRACE( "%p resizing from (%dx%d) to (%dx%d)\n",
               hwnd, data->window_rect.right - data->window_rect.left,
               data->window_rect.bottom - data->window_rect.top, cx, cy );

    style = GetWindowLongW( data->hwnd, GWL_STYLE );
    if ((style & WS_CAPTION) == WS_CAPTION || !is_window_rect_full_screen( &data->whole_rect ))
    {
        read_net_wm_states( event->display, data );
        if ((data->net_wm_state & (1 << NET_WM_STATE_MAXIMIZED)))
        {
            if (!(style & WS_MAXIMIZE))
            {
                TRACE( "win %p/%lx is maximized\n", data->hwnd, data->whole_window );
                release_win_data( data );
                SendMessageW( data->hwnd, WM_SYSCOMMAND, SC_MAXIMIZE, 0 );
                return TRUE;
            }
        }
        else if (style & WS_MAXIMIZE)
        {
            TRACE( "window %p/%lx is no longer maximized\n", data->hwnd, data->whole_window );
            release_win_data( data );
            SendMessageW( data->hwnd, WM_SYSCOMMAND, SC_RESTORE, 0 );
            return TRUE;
        }
    }

    if ((flags & (SWP_NOSIZE | SWP_NOMOVE)) != (SWP_NOSIZE | SWP_NOMOVE))
    {
        release_win_data( data );
        SetWindowPos( hwnd, 0, x, y, cx, cy, flags );
        return TRUE;
    }

done:
    release_win_data( data );
    return FALSE;
}


/**********************************************************************
 *           X11DRV_GravityNotify
 */
static BOOL X11DRV_GravityNotify( HWND hwnd, XEvent *xev )
{
    XGravityEvent *event = &xev->xgravity;
    struct x11drv_win_data *data = get_win_data( hwnd );
    RECT window_rect;
    int x, y;

    if (!data) return FALSE;

    if (data->whole_window)  /* only handle this for foreign windows */
    {
        release_win_data( data );
        return FALSE;
    }

    x = event->x + data->window_rect.left - data->whole_rect.left;
    y = event->y + data->window_rect.top - data->whole_rect.top;

    TRACE( "win %p/%lx new X pos %d,%d (event %d,%d)\n",
           hwnd, data->whole_window, x, y, event->x, event->y );

    window_rect = data->window_rect;
    release_win_data( data );

    if (window_rect.left != x || window_rect.top != y)
        SetWindowPos( hwnd, 0, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOCOPYBITS );

    return TRUE;
}


/***********************************************************************
 *           get_window_wm_state
 */
static int get_window_wm_state( Display *display, Window window )
{
    struct
    {
        CARD32 state;
        XID     icon;
    } *state;
    Atom type;
    int format, ret = -1;
    unsigned long count, remaining;

    if (!XGetWindowProperty( display, window, x11drv_atom(WM_STATE), 0,
                             sizeof(*state)/sizeof(CARD32), False, x11drv_atom(WM_STATE),
                             &type, &format, &count, &remaining, (unsigned char **)&state ))
    {
        if (type == x11drv_atom(WM_STATE) && get_property_size( format, count ) >= sizeof(*state))
            ret = state->state;
        XFree( state );
    }
    return ret;
}


/***********************************************************************
 *           handle_wm_state_notify
 *
 * Handle a PropertyNotify for WM_STATE.
 */
static void handle_wm_state_notify( HWND hwnd, XPropertyEvent *event, BOOL update_window )
{
    struct x11drv_win_data *data = get_win_data( hwnd );
    DWORD style;

    if (!data) return;

    switch(event->state)
    {
    case PropertyDelete:
        TRACE( "%p/%lx: WM_STATE deleted from %d\n", data->hwnd, data->whole_window, data->wm_state );
        data->wm_state = WithdrawnState;
        break;
    case PropertyNewValue:
        {
            int old_state = data->wm_state;
            int new_state = get_window_wm_state( event->display, data->whole_window );
            if (new_state != -1 && new_state != data->wm_state)
            {
                TRACE( "%p/%lx: new WM_STATE %d from %d\n",
                       data->hwnd, data->whole_window, new_state, old_state );
                data->wm_state = new_state;
                /* ignore the initial state transition out of withdrawn state */
                /* metacity does Withdrawn->NormalState->IconicState when mapping an iconic window */
                if (!old_state) goto done;
            }
        }
        break;
    }

    if (!update_window || !data->managed || !data->mapped) goto done;

    style = GetWindowLongW( data->hwnd, GWL_STYLE );

    if (data->iconic && data->wm_state == NormalState)  /* restore window */
    {
        data->iconic = FALSE;
        read_net_wm_states( event->display, data );
        if ((style & WS_CAPTION) == WS_CAPTION && (data->net_wm_state & (1 << NET_WM_STATE_MAXIMIZED)))
        {
            if ((style & WS_MAXIMIZEBOX) && !(style & WS_DISABLED))
            {
                TRACE( "restoring to max %p/%lx\n", data->hwnd, data->whole_window );
                release_win_data( data );
                SendMessageW( hwnd, WM_SYSCOMMAND, SC_MAXIMIZE, 0 );
                return;
            }
            TRACE( "not restoring to max win %p/%lx style %08x\n", data->hwnd, data->whole_window, style );
        }
        else
        {
            if (style & (WS_MINIMIZE | WS_MAXIMIZE))
            {
                TRACE( "restoring win %p/%lx\n", data->hwnd, data->whole_window );
                release_win_data( data );
                if ((style & (WS_MINIMIZE | WS_VISIBLE)) == (WS_MINIMIZE | WS_VISIBLE))
                    SetForegroundWindow( hwnd );
                SendMessageW( hwnd, WM_SYSCOMMAND, SC_RESTORE, 0 );
                return;
            }
            TRACE( "not restoring win %p/%lx style %08x\n", data->hwnd, data->whole_window, style );
        }
    }
    else if (!data->iconic && data->wm_state == IconicState)
    {
        data->iconic = TRUE;
        if ((style & WS_MINIMIZEBOX) && !(style & WS_DISABLED))
        {
            TRACE( "minimizing win %p/%lx\n", data->hwnd, data->whole_window );
            release_win_data( data );
            SendMessageW( hwnd, WM_SYSCOMMAND, SC_MINIMIZE, 0 );
            return;
        }
        TRACE( "not minimizing win %p/%lx style %08x\n", data->hwnd, data->whole_window, style );
    }
done:
    release_win_data( data );
}


static void handle__net_wm_state_notify( HWND hwnd, XPropertyEvent *event )
{
    struct x11drv_win_data *data = get_win_data( hwnd );

    if(data->pending_fullscreen)
    {
        read_net_wm_states( event->display, data );
        if(data->net_wm_state & (1 << NET_WM_STATE_FULLSCREEN)){
            data->pending_fullscreen = FALSE;
            TRACE("PropertyNotify _NET_WM_STATE, now 0x%x, pending_fullscreen no longer pending.\n",
                    data->net_wm_state);
        }else
            TRACE("PropertyNotify _NET_WM_STATE, now 0x%x, pending_fullscreen still pending.\n",
                    data->net_wm_state);
    }

    release_win_data( data );
}

static int handle_gamescope_focused_app_error( Display *dpy, XErrorEvent *event, void *arg )
{
    WARN( "Failed to read GAMESCOPE_FOCUSED_APP property, ignoring.\n" );
    return 1;
}

static void handle_gamescope_focused_app( XPropertyEvent *event )
{
    static const char *sgi = NULL;
    static BOOL steam_keyboard_opened;

    unsigned long count, remaining, *property;
    int format, app_id, focused_app_id;
    BOOL keyboard_opened;
    Atom type;

    if (!sgi && !(sgi = getenv( "SteamGameId" ))) return;
    app_id = atoi( sgi );

    X11DRV_expect_error( event->display, handle_gamescope_focused_app_error, NULL );
    XGetWindowProperty( event->display, DefaultRootWindow( event->display ), x11drv_atom( GAMESCOPE_FOCUSED_APP ),
                        0, ~0UL, False, XA_CARDINAL, &type, &format, &count, &remaining, (unsigned char **)&property );
    if (X11DRV_check_error( event->display )) focused_app_id = app_id;
    else
    {
        if (!property) focused_app_id = app_id;
        else focused_app_id = *property;
        XFree( property );
    }

    keyboard_opened = app_id != focused_app_id;
    if (steam_keyboard_opened == keyboard_opened) return;
    steam_keyboard_opened = keyboard_opened;

    FIXME( "HACK: Got app id %u, focused app %u\n", app_id, focused_app_id );
    if (keyboard_opened)
    {
        FIXME( "HACK: Steam Keyboard is opened, filtering events.\n" );
        SetEvent( steam_keyboard_event );
    }
    else
    {
        FIXME( "HACK: Steam Keyboard is closed, stopping events filter.\n" );
        ResetEvent( steam_keyboard_event );
    }
}

/***********************************************************************
 *           X11DRV_PropertyNotify
 */
static BOOL X11DRV_PropertyNotify( HWND hwnd, XEvent *xev )
{
    XPropertyEvent *event = &xev->xproperty;
    char *name;

    if (event->atom == x11drv_atom( GAMESCOPE_FOCUSED_APP )) handle_gamescope_focused_app( event );

    if (!hwnd) return FALSE;

    name = XGetAtomName(event->display, event->atom);
    if(name){
        TRACE("win %p PropertyNotify atom: %s, state: 0x%x\n", hwnd, name, event->state);
        XFree(name);
    }

    if (event->atom == x11drv_atom(WM_STATE)) handle_wm_state_notify( hwnd, event, TRUE );
    else if (event->atom == x11drv_atom(_NET_WM_STATE)) handle__net_wm_state_notify( hwnd, event );
    return TRUE;
}


/* event filter to wait for a WM_STATE change notification on a window */
static Bool is_wm_state_notify( Display *display, XEvent *event, XPointer arg )
{
    if (event->xany.window != (Window)arg) return 0;
    return (event->type == DestroyNotify ||
            (event->type == PropertyNotify && event->xproperty.atom == x11drv_atom(WM_STATE)));
}

/***********************************************************************
 *           wait_for_withdrawn_state
 */
void wait_for_withdrawn_state( HWND hwnd, BOOL set )
{
    Display *display = thread_display();
    struct x11drv_win_data *data;
    DWORD end = GetTickCount() + 2000;

    TRACE( "waiting for window %p to become %swithdrawn\n", hwnd, set ? "" : "not " );

    for (;;)
    {
        XEvent event;
        Window window;
        int count = 0;

        if (!(data = get_win_data( hwnd ))) break;
        if (!data->managed || data->embedded || data->display != display) break;
        if (!(window = data->whole_window)) break;
        if (!data->mapped == !set)
        {
            TRACE( "window %p/%lx now %smapped\n", hwnd, window, data->mapped ? "" : "un" );
            break;
        }
        if ((data->wm_state == WithdrawnState) != !set)
        {
            TRACE( "window %p/%lx state now %d\n", hwnd, window, data->wm_state );
            break;
        }
        release_win_data( data );

        while (XCheckIfEvent( display, &event, is_wm_state_notify, (char *)window ))
        {
            count++;
            if (XFilterEvent( &event, None )) continue;  /* filtered, ignore it */
            if (event.type == DestroyNotify) call_event_handler( display, &event );
            else handle_wm_state_notify( hwnd, &event.xproperty, FALSE );
        }

        if (!count)
        {
            struct pollfd pfd;
            int timeout = end - GetTickCount();

            pfd.fd = ConnectionNumber(display);
            pfd.events = POLLIN;
            if (timeout <= 0 || poll( &pfd, 1, timeout ) != 1)
            {
                FIXME( "window %p/%lx wait timed out\n", hwnd, window );
                return;
            }
        }
    }
    release_win_data( data );
}


/*****************************************************************
 *		SetFocus   (X11DRV.@)
 *
 * Set the X focus.
 */
void CDECL X11DRV_SetFocus( HWND hwnd )
{
    struct x11drv_win_data *data;

    HWND parent;

    for (;;)
    {
        if (!(data = get_win_data( hwnd ))) return;
        if (data->embedded) break;
        parent = GetAncestor( hwnd, GA_PARENT );
        if (!parent || parent == GetDesktopWindow()) break;
        release_win_data( data );
        hwnd = parent;
    }
    if (!data->managed || data->embedder) set_input_focus( data );
    release_win_data( data );
}


static HWND find_drop_window( HWND hQueryWnd, LPPOINT lpPt )
{
    RECT tempRect;

    if (!IsWindowEnabled(hQueryWnd)) return 0;
    
    GetWindowRect(hQueryWnd, &tempRect);

    if(!PtInRect(&tempRect, *lpPt)) return 0;

    if (!IsIconic( hQueryWnd ))
    {
        POINT pt = *lpPt;
        ScreenToClient( hQueryWnd, &pt );
        GetClientRect( hQueryWnd, &tempRect );

        if (PtInRect( &tempRect, pt))
        {
            HWND ret = ChildWindowFromPointEx( hQueryWnd, pt, CWP_SKIPINVISIBLE|CWP_SKIPDISABLED );
            if (ret && ret != hQueryWnd)
            {
                ret = find_drop_window( ret, lpPt );
                if (ret) return ret;
            }
        }
    }

    if(!(GetWindowLongA( hQueryWnd, GWL_EXSTYLE ) & WS_EX_ACCEPTFILES)) return 0;
    
    ScreenToClient(hQueryWnd, lpPt);

    return hQueryWnd;
}

/**********************************************************************
 *           EVENT_DropFromOffix
 *
 * don't know if it still works (last Changelog is from 96/11/04)
 */
static void EVENT_DropFromOffiX( HWND hWnd, XClientMessageEvent *event )
{
    struct x11drv_win_data *data;
    POINT pt;
    unsigned long	data_length;
    unsigned long	aux_long;
    unsigned char*	p_data = NULL;
    Atom atom_aux;
    int			x, y, cx, cy, dummy;
    Window		win, w_aux_root, w_aux_child;

    if (!(data = get_win_data( hWnd ))) return;
    ERR("TODO: fs hack\n");
    cx = data->whole_rect.right - data->whole_rect.left;
    cy = data->whole_rect.bottom - data->whole_rect.top;
    win = data->whole_window;
    release_win_data( data );

    XQueryPointer( event->display, win, &w_aux_root, &w_aux_child,
                   &x, &y, &dummy, &dummy, (unsigned int*)&aux_long);
    pt = root_to_virtual_screen( x, y );

    /* find out drop point and drop window */
    if (pt.x < 0 || pt.y < 0 || pt.x > cx || pt.y > cy)
    {
	if (!(GetWindowLongW( hWnd, GWL_EXSTYLE ) & WS_EX_ACCEPTFILES)) return;
	pt.x = pt.y = 0;
    }
    else
    {
        if (!find_drop_window( hWnd, &pt )) return;
    }

    XGetWindowProperty( event->display, DefaultRootWindow(event->display),
                        x11drv_atom(DndSelection), 0, 65535, FALSE,
                        AnyPropertyType, &atom_aux, &dummy,
                        &data_length, &aux_long, &p_data);

    if( !aux_long && p_data)  /* don't bother if > 64K */
    {
        char *p = (char *)p_data;
        char *p_drop;

        aux_long = 0;
        while( *p )  /* calculate buffer size */
        {
            INT len = GetShortPathNameA( p, NULL, 0 );
            if (len) aux_long += len + 1;
            p += strlen(p) + 1;
        }
        if( aux_long && aux_long < 65535 )
        {
            HDROP                 hDrop;
            DROPFILES *lpDrop;

            aux_long += sizeof(DROPFILES) + 1;
            hDrop = GlobalAlloc( GMEM_SHARE, aux_long );
            lpDrop = GlobalLock( hDrop );

            if( lpDrop )
            {
                lpDrop->pFiles = sizeof(DROPFILES);
                lpDrop->pt = pt;
                lpDrop->fNC = FALSE;
                lpDrop->fWide = FALSE;
                p_drop = (char *)(lpDrop + 1);
                p = (char *)p_data;
                while(*p)
                {
                    if (GetShortPathNameA( p, p_drop, aux_long - (p_drop - (char *)lpDrop) ))
                        p_drop += strlen( p_drop ) + 1;
                    p += strlen(p) + 1;
                }
                *p_drop = '\0';
                PostMessageA( hWnd, WM_DROPFILES, (WPARAM)hDrop, 0L );
            }
        }
    }
    if( p_data ) XFree(p_data);
}

/**********************************************************************
 *           EVENT_DropURLs
 *
 * drop items are separated by \n
 * each item is prefixed by its mime type
 *
 * event->data.l[3], event->data.l[4] contains drop x,y position
 */
static void EVENT_DropURLs( HWND hWnd, XClientMessageEvent *event )
{
  struct x11drv_win_data *win_data;
  unsigned long	data_length;
  unsigned long	aux_long, drop_len = 0;
  unsigned char	*p_data = NULL; /* property data */
  char		*p_drop = NULL;
  char          *p, *next;
  int		x, y;
  POINT pos;
  DROPFILES *lpDrop;
  HDROP hDrop;
  union {
    Atom	atom_aux;
    int         i;
    Window      w_aux;
    unsigned int u;
  }		u; /* unused */

  if (!(GetWindowLongW( hWnd, GWL_EXSTYLE ) & WS_EX_ACCEPTFILES)) return;

  XGetWindowProperty( event->display, DefaultRootWindow(event->display),
                      x11drv_atom(DndSelection), 0, 65535, FALSE,
                      AnyPropertyType, &u.atom_aux, &u.i,
                      &data_length, &aux_long, &p_data);
  if (aux_long)
    WARN("property too large, truncated!\n");
  TRACE("urls=%s\n", p_data);

  if( !aux_long && p_data) {	/* don't bother if > 64K */
    /* calculate length */
    p = (char*) p_data;
    next = strchr(p, '\n');
    while (p) {
      if (next) *next=0;
      if (strncmp(p,"file:",5) == 0 ) {
	INT len = GetShortPathNameA( p+5, NULL, 0 );
	if (len) drop_len += len + 1;
      }
      if (next) {
	*next = '\n';
	p = next + 1;
	next = strchr(p, '\n');
      } else {
	p = NULL;
      }
    }

    if( drop_len && drop_len < 65535 ) {
      XQueryPointer( event->display, root_window, &u.w_aux, &u.w_aux,
                     &x, &y, &u.i, &u.i, &u.u);
      pos = root_to_virtual_screen( x, y );

      drop_len += sizeof(DROPFILES) + 1;
      hDrop = GlobalAlloc( GMEM_SHARE, drop_len );
      lpDrop = GlobalLock( hDrop );

      if( lpDrop && (win_data = get_win_data( hWnd )))
      {
	  lpDrop->pFiles = sizeof(DROPFILES);
	  lpDrop->pt = pos;
	  lpDrop->fNC =
	    (pos.x < (win_data->client_rect.left - win_data->whole_rect.left)  ||
	     pos.y < (win_data->client_rect.top - win_data->whole_rect.top)    ||
	     pos.x > (win_data->client_rect.right - win_data->whole_rect.left) ||
	     pos.y > (win_data->client_rect.bottom - win_data->whole_rect.top) );
	  lpDrop->fWide = FALSE;
	  p_drop = (char*)(lpDrop + 1);
          release_win_data( win_data );
      }

      /* create message content */
      if (p_drop) {
	p = (char*) p_data;
	next = strchr(p, '\n');
	while (p) {
	  if (next) *next=0;
	  if (strncmp(p,"file:",5) == 0 ) {
	    INT len = GetShortPathNameA( p+5, p_drop, 65535 );
	    if (len) {
	      TRACE("drop file %s as %s\n", p+5, p_drop);
	      p_drop += len+1;
	    } else {
	      WARN("can't convert file %s to dos name\n", p+5);
	    }
	  } else {
	    WARN("unknown mime type %s\n", p);
	  }
	  if (next) {
	    *next = '\n';
	    p = next + 1;
	    next = strchr(p, '\n');
	  } else {
	    p = NULL;
	  }
	  *p_drop = '\0';
	}

        GlobalUnlock(hDrop);
        PostMessageA( hWnd, WM_DROPFILES, (WPARAM)hDrop, 0L );
      }
    }
  }
  if( p_data ) XFree(p_data);
}


/**********************************************************************
 *              handle_xembed_protocol
 */
static void handle_xembed_protocol( HWND hwnd, XEvent *xev )
{
    XClientMessageEvent *event = &xev->xclient;

    switch (event->data.l[1])
    {
    case XEMBED_EMBEDDED_NOTIFY:
        {
            struct x11drv_win_data *data = get_win_data( hwnd );
            if (!data) break;

            TRACE( "win %p/%lx XEMBED_EMBEDDED_NOTIFY owner %lx\n", hwnd, event->window, event->data.l[3] );
            data->embedder = event->data.l[3];

            /* window has been marked as embedded before (e.g. systray) */
            if (data->embedded || !data->embedder /* broken QX11EmbedContainer implementation */)
            {
                release_win_data( data );
                break;
            }

            make_window_embedded( data );
            release_win_data( data );
            reparent_notify( event->display, hwnd, event->data.l[3], 0, 0 );
        }
        break;

    case XEMBED_WINDOW_DEACTIVATE:
        TRACE( "win %p/%lx XEMBED_WINDOW_DEACTIVATE message\n", hwnd, event->window );
        focus_out( event->display, GetAncestor( hwnd, GA_ROOT ) );
        break;

    case XEMBED_FOCUS_OUT:
        TRACE( "win %p/%lx XEMBED_FOCUS_OUT message\n", hwnd, event->window );
        focus_out( event->display, GetAncestor( hwnd, GA_ROOT ) );
        break;

    case XEMBED_MODALITY_ON:
        TRACE( "win %p/%lx XEMBED_MODALITY_ON message\n", hwnd, event->window );
        EnableWindow( hwnd, FALSE );
        break;

    case XEMBED_MODALITY_OFF:
        TRACE( "win %p/%lx XEMBED_MODALITY_OFF message\n", hwnd, event->window );
        EnableWindow( hwnd, TRUE );
        break;

    default:
        TRACE( "win %p/%lx XEMBED message %lu(%lu)\n",
               hwnd, event->window, event->data.l[1], event->data.l[2] );
        break;
    }
}


/**********************************************************************
 *              handle_dnd_protocol
 */
static void handle_dnd_protocol( HWND hwnd, XEvent *xev )
{
    XClientMessageEvent *event = &xev->xclient;
    Window root, child;
    int root_x, root_y, child_x, child_y;
    unsigned int u;

    /* query window (drag&drop event contains only drag window) */
    XQueryPointer( event->display, root_window, &root, &child,
                   &root_x, &root_y, &child_x, &child_y, &u);
    if (XFindContext( event->display, child, winContext, (char **)&hwnd ) != 0) hwnd = 0;
    if (!hwnd) return;
    if (event->data.l[0] == DndFile || event->data.l[0] == DndFiles)
        EVENT_DropFromOffiX(hwnd, event);
    else if (event->data.l[0] == DndURL)
        EVENT_DropURLs(hwnd, event);
}


struct client_message_handler
{
    int    atom;                     /* protocol atom */
    void (*handler)(HWND, XEvent *); /* corresponding handler function */
};

static const struct client_message_handler client_messages[] =
{
    { XATOM_MANAGER,      handle_manager_message },
    { XATOM_WM_PROTOCOLS, handle_wm_protocols },
    { XATOM__XEMBED,      handle_xembed_protocol },
    { XATOM_DndProtocol,  handle_dnd_protocol },
    { XATOM_XdndEnter,    X11DRV_XDND_EnterEvent },
    { XATOM_XdndPosition, X11DRV_XDND_PositionEvent },
    { XATOM_XdndDrop,     X11DRV_XDND_DropEvent },
    { XATOM_XdndLeave,    X11DRV_XDND_LeaveEvent }
};


/**********************************************************************
 *           X11DRV_ClientMessage
 */
static BOOL X11DRV_ClientMessage( HWND hwnd, XEvent *xev )
{
    XClientMessageEvent *event = &xev->xclient;
    unsigned int i;

    if (!hwnd) return FALSE;

    if (event->format != 32)
    {
        WARN( "Don't know how to handle format %d\n", event->format );
        return FALSE;
    }

    for (i = 0; i < ARRAY_SIZE( client_messages ); i++)
    {
        if (event->message_type == X11DRV_Atoms[client_messages[i].atom - FIRST_XATOM])
        {
            client_messages[i].handler( hwnd, xev );
            return TRUE;
        }
    }
    TRACE( "no handler found for %ld\n", event->message_type );
    return FALSE;
}
