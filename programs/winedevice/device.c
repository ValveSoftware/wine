/*
 * Service process to load a kernel driver
 *
 * Copyright 2007 Alexandre Julliard
 * Copyright 2016 Sebastian Lackner
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

#include <stdarg.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winternl.h"
#include "ddk/wdm.h"
#include "wine/rbtree.h"
#include "wine/svcctl.h"
#include "wine/unicode.h"
#include "wine/debug.h"
#include "wine/heap.h"

WINE_DEFAULT_DEBUG_CHANNEL(winedevice);

static const WCHAR servicesW[] = {'\\','R','e','g','i','s','t','r','y',
                                  '\\','M','a','c','h','i','n','e',
                                  '\\','S','y','s','t','e','m',
                                  '\\','C','u','r','r','e','n','t','C','o','n','t','r','o','l','S','e','t',
                                  '\\','S','e','r','v','i','c','e','s',
                                  '\\',0};

extern NTSTATUS CDECL wine_ntoskrnl_main_loop( HANDLE stop_event );

static const WCHAR winedeviceW[] = {'w','i','n','e','d','e','v','i','c','e',0};
static SERVICE_STATUS_HANDLE service_handle;
static SC_HANDLE manager_handle;
static HANDLE stop_event;

struct wine_driver
{
    struct wine_rb_entry entry;
    UNICODE_STRING service_name;
    WCHAR name[1];
};

static int wine_drivers_rb_compare( const void *key, const struct wine_rb_entry *entry )
{
    const struct wine_driver *driver = WINE_RB_ENTRY_VALUE( entry, const struct wine_driver, entry );
    return strcmpW( (const WCHAR *)key, driver->name );
}

static struct wine_rb_tree wine_drivers = { wine_drivers_rb_compare };

/* helper function to update service status */
static void set_service_status( SERVICE_STATUS_HANDLE handle, DWORD state, DWORD accepted )
{
    SERVICE_STATUS status;
    status.dwServiceType             = SERVICE_WIN32;
    status.dwCurrentState            = state;
    status.dwControlsAccepted        = accepted;
    status.dwWin32ExitCode           = 0;
    status.dwServiceSpecificExitCode = 0;
    status.dwCheckPoint              = 0;
    status.dwWaitHint                = (state == SERVICE_START_PENDING) ? 10000 : 0;
    SetServiceStatus( handle, &status );
}

/* call the driver unload function */
static void unload_driver( struct wine_rb_entry *entry, void *context )
{
    struct wine_driver *driver = WINE_RB_ENTRY_VALUE( entry, struct wine_driver, entry );

    ZwUnloadDriver( &driver->service_name );
    RtlFreeUnicodeString( &driver->service_name );
    HeapFree( GetProcessHeap(), 0, driver );
}

/* load a driver and notify services.exe about the status change */
static NTSTATUS create_driver( const WCHAR *driver_name )
{
    struct wine_driver *driver;
    NTSTATUS status;
    DWORD length;
    WCHAR *str;

    length = FIELD_OFFSET( struct wine_driver, name[strlenW(driver_name) + 1] );
    if (!(driver = HeapAlloc( GetProcessHeap(), 0, length )))
        return STATUS_NO_MEMORY;

    strcpyW( driver->name, driver_name );

    if (wine_rb_put( &wine_drivers, driver_name, &driver->entry ))
    {
        HeapFree( GetProcessHeap(), 0, driver );
        return STATUS_UNSUCCESSFUL;
    }

    if (!(str = heap_alloc( sizeof(servicesW) + strlenW(driver_name)*sizeof(WCHAR) )))
    {
        status = STATUS_NO_MEMORY;
        goto error;
    }
    lstrcpyW( str, servicesW );
    lstrcatW( str, driver_name );
    RtlInitUnicodeString( &driver->service_name, str );

    status = ZwLoadDriver( &driver->service_name );
    if (status != STATUS_SUCCESS)
    {
        ERR( "failed to create driver %s: %08x\n", debugstr_w(driver->name), status );
        goto error;
    }

    return STATUS_SUCCESS;

error:
    wine_rb_remove( &wine_drivers, &driver->entry );
    HeapFree( GetProcessHeap(), 0, driver );
    return status;
}

static DWORD device_handler( DWORD ctrl, const WCHAR *driver_name )
{
    struct wine_rb_entry *entry;
    DWORD result = NO_ERROR;

    entry = wine_rb_get( &wine_drivers, driver_name );

    switch (ctrl)
    {
    case SERVICE_CONTROL_START:
        if (entry) break;
        result = RtlNtStatusToDosError(create_driver( driver_name ));
        break;

    case SERVICE_CONTROL_STOP:
        if (!entry) break;
        unload_driver( entry, NULL );
        break;

    default:
        FIXME( "got driver ctrl %x for %s\n", ctrl, wine_dbgstr_w(driver_name) );
        break;
    }
    return result;
}

static DWORD WINAPI service_handler( DWORD ctrl, DWORD event_type, LPVOID event_data, LPVOID context )
{
    const WCHAR *service_group = context;

    if (ctrl & SERVICE_CONTROL_FORWARD_FLAG)
    {
        if (!event_data) return ERROR_INVALID_PARAMETER;
        return device_handler( ctrl & ~SERVICE_CONTROL_FORWARD_FLAG, (const WCHAR *)event_data );
    }

    switch (ctrl)
    {
    case SERVICE_CONTROL_STOP:
    case SERVICE_CONTROL_SHUTDOWN:
        TRACE( "shutting down %s\n", wine_dbgstr_w(service_group) );
        set_service_status( service_handle, SERVICE_STOP_PENDING, 0 );
        SetEvent( stop_event );
        return NO_ERROR;
    default:
        FIXME( "got service ctrl %x for %s\n", ctrl, wine_dbgstr_w(service_group) );
        set_service_status( service_handle, SERVICE_RUNNING,
                            SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN );
        return NO_ERROR;
    }
}

static void WINAPI ServiceMain( DWORD argc, LPWSTR *argv )
{
    const WCHAR *service_group = (argc >= 2) ? argv[1] : argv[0];

    if (!(stop_event = CreateEventW( NULL, TRUE, FALSE, NULL )))
        return;
    if (!(manager_handle = OpenSCManagerW( NULL, NULL, SC_MANAGER_CONNECT )))
        return;
    if (!(service_handle = RegisterServiceCtrlHandlerExW( winedeviceW, service_handler, (void *)service_group )))
        return;

    TRACE( "starting service group %s\n", wine_dbgstr_w(service_group) );
    set_service_status( service_handle, SERVICE_RUNNING,
                        SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN );

    wine_ntoskrnl_main_loop( stop_event );

    TRACE( "service group %s stopped\n", wine_dbgstr_w(service_group) );
    wine_rb_destroy( &wine_drivers, unload_driver, NULL );
    set_service_status( service_handle, SERVICE_STOPPED, 0 );
    CloseServiceHandle( manager_handle );
    CloseHandle( stop_event );
}

int wmain( int argc, WCHAR *argv[] )
{
    SERVICE_TABLE_ENTRYW service_table[2];

    service_table[0].lpServiceName = (void *)winedeviceW;
    service_table[0].lpServiceProc = ServiceMain;
    service_table[1].lpServiceName = NULL;
    service_table[1].lpServiceProc = NULL;

    StartServiceCtrlDispatcherW( service_table );
    return 0;
}
