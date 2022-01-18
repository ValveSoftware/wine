/*
 * Xinerama support
 *
 * Copyright 2006 Alexandre Julliard
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

#include <stdarg.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#ifdef HAVE_X11_EXTENSIONS_XINERAMA_H
#include <X11/extensions/Xinerama.h>
#endif
#include <dlfcn.h>
#include "x11drv.h"
#include "wine/debug.h"
#include "wine/heap.h"

WINE_DEFAULT_DEBUG_CHANNEL(x11drv);

static MONITORINFOEXW default_monitor =
{
    sizeof(default_monitor),    /* cbSize */
    { 0, 0, 0, 0 },             /* rcMonitor */
    { 0, 0, 0, 0 },             /* rcWork */
    MONITORINFOF_PRIMARY,       /* dwFlags */
    { '\\','\\','.','\\','D','I','S','P','L','A','Y','1',0 }   /* szDevice */
};

static CRITICAL_SECTION xinerama_section;
static CRITICAL_SECTION_DEBUG xinerama_critsect_debug =
{
    0, 0, &xinerama_section,
    {&xinerama_critsect_debug.ProcessLocksList, &xinerama_critsect_debug.ProcessLocksList},
     0, 0, {(DWORD_PTR)(__FILE__ ": xinerama_section")}
};
static CRITICAL_SECTION xinerama_section = {&xinerama_critsect_debug, -1, 0, 0, 0, 0};
static MONITORINFOEXW *monitors;
static int nb_monitors;

static inline MONITORINFOEXW *get_primary(void)
{
    /* default to 0 if specified primary is invalid */
    int idx = primary_monitor;
    if (idx >= nb_monitors) idx = 0;
    return &monitors[idx];
}

#ifdef SONAME_LIBXINERAMA

#define MAKE_FUNCPTR(f) static typeof(f) * p##f

MAKE_FUNCPTR(XineramaQueryExtension);
MAKE_FUNCPTR(XineramaQueryScreens);

static void load_xinerama(void)
{
    void *handle;

    if (!(handle = dlopen(SONAME_LIBXINERAMA, RTLD_NOW)))
    {
        WARN( "failed to open %s\n", SONAME_LIBXINERAMA );
        return;
    }
    pXineramaQueryExtension = dlsym( handle, "XineramaQueryExtension" );
    if (!pXineramaQueryExtension) WARN( "XineramaQueryScreens not found\n" );
    pXineramaQueryScreens = dlsym( handle, "XineramaQueryScreens" );
    if (!pXineramaQueryScreens) WARN( "XineramaQueryScreens not found\n" );
}

static int query_screens(void)
{
    int i, count, event_base, error_base;
    XineramaScreenInfo *screens;

    if (!monitors)  /* first time around */
        load_xinerama();

    if (!pXineramaQueryExtension || !pXineramaQueryScreens ||
        !pXineramaQueryExtension( gdi_display, &event_base, &error_base ) ||
        !(screens = pXineramaQueryScreens( gdi_display, &count ))) return 0;

    if (monitors != &default_monitor) HeapFree( GetProcessHeap(), 0, monitors );
    if ((monitors = HeapAlloc( GetProcessHeap(), 0, count * sizeof(*monitors) )))
    {
        nb_monitors = count;
        for (i = 0; i < nb_monitors; i++)
        {
            monitors[i].cbSize = sizeof( monitors[i] );
            monitors[i].rcMonitor.left   = screens[i].x_org;
            monitors[i].rcMonitor.top    = screens[i].y_org;
            monitors[i].rcMonitor.right  = screens[i].x_org + screens[i].width;
            monitors[i].rcMonitor.bottom = screens[i].y_org + screens[i].height;
            monitors[i].dwFlags          = 0;
            monitors[i].rcWork           = get_work_area( &monitors[i].rcMonitor );
        }

        get_primary()->dwFlags |= MONITORINFOF_PRIMARY;
    }
    else count = 0;

    XFree( screens );
    return count;
}

#else  /* SONAME_LIBXINERAMA */

static inline int query_screens(void)
{
    return 0;
}

#endif  /* SONAME_LIBXINERAMA */

/* Get xinerama monitor indices required for _NET_WM_FULLSCREEN_MONITORS */
void xinerama_get_fullscreen_monitors( const RECT *rect, long *indices )
{
    RECT window_rect, intersect_rect, monitor_rect;
    POINT offset;
    INT i;

    /* Convert window rectangle to root coordinates */
    offset = virtual_screen_to_root( rect->left, rect->top );
    window_rect.left = offset.x;
    window_rect.top = offset.y;
    window_rect.right = window_rect.left + rect->right - rect->left;
    window_rect.bottom = window_rect.top + rect->bottom - rect->top;

    /* Compare to xinerama monitor rectangles in root coordinates */
    EnterCriticalSection( &xinerama_section );
    offset.x = INT_MAX;
    offset.y = INT_MAX;
    for (i = 0; i < nb_monitors; ++i)
    {
        offset.x = min( offset.x, monitors[i].rcMonitor.left );
        offset.y = min( offset.y, monitors[i].rcMonitor.top );
    }

    memset( indices, 0, sizeof(*indices) * 4 );
    for (i = 0; i < nb_monitors; ++i)
    {
        SetRect( &monitor_rect, monitors[i].rcMonitor.left - offset.x,
                 monitors[i].rcMonitor.top - offset.y, monitors[i].rcMonitor.right - offset.x,
                 monitors[i].rcMonitor.bottom - offset.y );
        IntersectRect( &intersect_rect, &window_rect, &monitor_rect );
        if (EqualRect( &intersect_rect, &monitor_rect ))
        {
            if (monitors[i].rcMonitor.top < monitors[indices[0]].rcMonitor.top)
                indices[0] = i;
            if (monitors[i].rcMonitor.bottom > monitors[indices[1]].rcMonitor.bottom)
                indices[1] = i;
            if (monitors[i].rcMonitor.left < monitors[indices[2]].rcMonitor.left)
                indices[2] = i;
            if (monitors[i].rcMonitor.right > monitors[indices[3]].rcMonitor.right)
                indices[3] = i;
        }
    }

    LeaveCriticalSection( &xinerama_section );
    TRACE( "fullsceen monitors: %ld,%ld,%ld,%ld.\n", indices[0], indices[1], indices[2], indices[3] );
}

static BOOL xinerama_get_gpus( struct gdi_gpu **new_gpus, int *count )
{
    static const WCHAR wine_adapterW[] = {'W','i','n','e',' ','A','d','a','p','t','e','r',0};
    struct gdi_gpu *gpus;

    /* Xinerama has no support for GPU, faking one */
    gpus = heap_calloc( 1, sizeof(*gpus) );
    if (!gpus)
        return FALSE;

    lstrcpyW( gpus[0].name, wine_adapterW );

    *new_gpus = gpus;
    *count = 1;

    return TRUE;
}

static void xinerama_free_gpus( struct gdi_gpu *gpus )
{
    heap_free( gpus );
}

static BOOL xinerama_get_adapters( ULONG_PTR gpu_id, struct gdi_adapter **new_adapters, int *count )
{
    struct gdi_adapter *adapters = NULL;
    INT index = 0;
    INT i, j;
    INT primary_index;
    BOOL mirrored;

    if (gpu_id)
        return FALSE;

    /* Being lazy, actual adapter count may be less */
    EnterCriticalSection( &xinerama_section );
    adapters = heap_calloc( nb_monitors, sizeof(*adapters) );
    if (!adapters)
    {
        LeaveCriticalSection( &xinerama_section );
        return FALSE;
    }

    primary_index = primary_monitor;
    if (primary_index >= nb_monitors)
        primary_index = 0;

    for (i = 0; i < nb_monitors; i++)
    {
        mirrored = FALSE;
        for (j = 0; j < i; j++)
        {
            if (EqualRect( &monitors[i].rcMonitor, &monitors[j].rcMonitor) && !IsRectEmpty( &monitors[j].rcMonitor ))
            {
                mirrored = TRUE;
                break;
            }
        }

        /* Mirrored monitors share the same adapter */
        if (mirrored)
            continue;

        /* Use monitor index as id */
        adapters[index].id = (ULONG_PTR)i;

        if (i == primary_index)
            adapters[index].state_flags |= DISPLAY_DEVICE_PRIMARY_DEVICE;

        if (!IsRectEmpty( &monitors[i].rcMonitor ))
            adapters[index].state_flags |= DISPLAY_DEVICE_ATTACHED_TO_DESKTOP;

        index++;
    }

    /* Primary adapter has to be first */
    if (primary_index)
    {
        struct gdi_adapter tmp;
        tmp = adapters[primary_index];
        adapters[primary_index] = adapters[0];
        adapters[0] = tmp;
    }

    *new_adapters = adapters;
    *count = index;
    LeaveCriticalSection( &xinerama_section );
    return TRUE;
}

static void xinerama_free_adapters( struct gdi_adapter *adapters )
{
    heap_free( adapters );
}

static BOOL xinerama_get_monitors( ULONG_PTR adapter_id, struct gdi_monitor **new_monitors, int *count )
{
    static const WCHAR generic_nonpnp_monitorW[] = {
        'G','e','n','e','r','i','c',' ',
        'N','o','n','-','P','n','P',' ','M','o','n','i','t','o','r',0};
    struct gdi_monitor *monitor;
    INT first = (INT)adapter_id;
    INT monitor_count = 0;
    INT index = 0;
    INT i;

    EnterCriticalSection( &xinerama_section );

    for (i = first; i < nb_monitors; i++)
    {
        if (i == first
            || (EqualRect( &monitors[i].rcMonitor, &monitors[first].rcMonitor )
                && !IsRectEmpty( &monitors[first].rcMonitor )))
            monitor_count++;
    }

    monitor = heap_calloc( monitor_count, sizeof(*monitor) );
    if (!monitor)
    {
        LeaveCriticalSection( &xinerama_section );
        return FALSE;
    }

    for (i = first; i < nb_monitors; i++)
    {
        if (i == first
            || (EqualRect( &monitors[i].rcMonitor, &monitors[first].rcMonitor )
                && !IsRectEmpty( &monitors[first].rcMonitor )))
        {
            lstrcpyW( monitor[index].name, generic_nonpnp_monitorW );
            monitor[index].rc_monitor = monitors[i].rcMonitor;
            monitor[index].rc_work = monitors[i].rcWork;
            /* Xinerama only reports monitors already attached */
            monitor[index].state_flags = DISPLAY_DEVICE_ATTACHED;
            monitor[index].edid_len = 0;
            monitor[index].edid = NULL;
            if (!IsRectEmpty( &monitors[i].rcMonitor ))
                monitor[index].state_flags |= DISPLAY_DEVICE_ACTIVE;

            index++;
        }
    }

    *new_monitors = monitor;
    *count = monitor_count;
    LeaveCriticalSection( &xinerama_section );
    return TRUE;
}

static void xinerama_free_monitors( struct gdi_monitor *monitors, int count )
{
    heap_free( monitors );
}

void xinerama_init( unsigned int width, unsigned int height )
{
    struct x11drv_display_device_handler handler;
    MONITORINFOEXW *primary;
    int i;
    RECT rect;

    if (is_virtual_desktop())
        return;

    EnterCriticalSection( &xinerama_section );

    SetRect( &rect, 0, 0, width, height );
    if (!query_screens())
    {
        default_monitor.rcMonitor = rect;
        default_monitor.rcWork = get_work_area( &default_monitor.rcMonitor );
        nb_monitors = 1;
        monitors = &default_monitor;
    }

    primary = get_primary();

    /* coordinates (0,0) have to point to the primary monitor origin */
    OffsetRect( &rect, -primary->rcMonitor.left, -primary->rcMonitor.top );
    for (i = 0; i < nb_monitors; i++)
    {
        OffsetRect( &monitors[i].rcMonitor, rect.left, rect.top );
        OffsetRect( &monitors[i].rcWork, rect.left, rect.top );
        TRACE( "monitor 0x%x: %s work %s%s\n",
               i, wine_dbgstr_rect(&monitors[i].rcMonitor),
               wine_dbgstr_rect(&monitors[i].rcWork),
               (monitors[i].dwFlags & MONITORINFOF_PRIMARY) ? " (primary)" : "" );
    }

    LeaveCriticalSection( &xinerama_section );

    handler.name = "Xinerama";
    handler.priority = 100;
    handler.get_gpus = xinerama_get_gpus;
    handler.get_adapters = xinerama_get_adapters;
    handler.get_monitors = xinerama_get_monitors;
    handler.free_gpus = xinerama_free_gpus;
    handler.free_adapters = xinerama_free_adapters;
    handler.free_monitors = xinerama_free_monitors;
    handler.register_event_handlers = NULL;
    X11DRV_DisplayDevices_SetHandler( &handler );
}
