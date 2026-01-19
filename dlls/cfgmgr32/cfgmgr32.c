/*
 * Copyright (C) 2023 Mohamad Al-Jaf
 * Copyright (C) 2025 Vibhav Pant
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

#include "cfgmgr32_private.h"
#include "initguid.h"
#include "devpkey.h"

WINE_DEFAULT_DEBUG_CHANNEL(setupapi);

/***********************************************************************
 *           CM_MapCrToWin32Err (cfgmgr32.@)
 */
DWORD WINAPI CM_MapCrToWin32Err( CONFIGRET code, DWORD default_error )
{
    TRACE( "code: %#lx, default_error: %ld\n", code, default_error );

    switch (code)
    {
    case CR_SUCCESS:                  return ERROR_SUCCESS;
    case CR_OUT_OF_MEMORY:            return ERROR_NOT_ENOUGH_MEMORY;
    case CR_INVALID_POINTER:          return ERROR_INVALID_USER_BUFFER;
    case CR_INVALID_FLAG:             return ERROR_INVALID_FLAGS;
    case CR_INVALID_DEVNODE:
    case CR_INVALID_DEVICE_ID:
    case CR_INVALID_MACHINENAME:
    case CR_INVALID_PROPERTY:
    case CR_INVALID_REFERENCE_STRING: return ERROR_INVALID_DATA;
    case CR_NO_SUCH_DEVNODE:
    case CR_NO_SUCH_VALUE:
    case CR_NO_SUCH_DEVICE_INTERFACE: return ERROR_NOT_FOUND;
    case CR_ALREADY_SUCH_DEVNODE:     return ERROR_ALREADY_EXISTS;
    case CR_BUFFER_SMALL:             return ERROR_INSUFFICIENT_BUFFER;
    case CR_NO_REGISTRY_HANDLE:       return ERROR_INVALID_HANDLE;
    case CR_REGISTRY_ERROR:           return ERROR_REGISTRY_CORRUPT;
    case CR_NO_SUCH_REGISTRY_KEY:     return ERROR_FILE_NOT_FOUND;
    case CR_REMOTE_COMM_FAILURE:
    case CR_MACHINE_UNAVAILABLE:
    case CR_NO_CM_SERVICES:           return ERROR_SERVICE_NOT_ACTIVE;
    case CR_ACCESS_DENIED:            return ERROR_ACCESS_DENIED;
    case CR_CALL_NOT_IMPLEMENTED:     return ERROR_CALL_NOT_IMPLEMENTED;
    }

    return default_error;
}

/***********************************************************************
 *           CM_Get_Device_Interface_PropertyW (cfgmgr32.@)
 */
CONFIGRET WINAPI CM_Get_Device_Interface_PropertyW( LPCWSTR device_interface, const DEVPROPKEY *property_key,
                                                    DEVPROPTYPE *property_type, BYTE *property_buffer,
                                                    ULONG *property_buffer_size, ULONG flags )
{
    SP_DEVICE_INTERFACE_DATA iface = {sizeof(iface)};
    HDEVINFO set;
    DWORD err;
    BOOL ret;

    TRACE( "%s %p %p %p %p %ld.\n", debugstr_w(device_interface), property_key, property_type, property_buffer,
           property_buffer_size, flags);

    if (!property_key) return CR_FAILURE;
    if (!device_interface || !property_type || !property_buffer_size) return CR_INVALID_POINTER;
    if (*property_buffer_size && !property_buffer) return CR_INVALID_POINTER;
    if (flags) return CR_INVALID_FLAG;

    set = SetupDiCreateDeviceInfoListExW( NULL, NULL, NULL, NULL );
    if (set == INVALID_HANDLE_VALUE) return CR_OUT_OF_MEMORY;
    if (!SetupDiOpenDeviceInterfaceW( set, device_interface, 0, &iface ))
    {
        SetupDiDestroyDeviceInfoList( set );
        TRACE( "No interface %s, err %lu.\n", debugstr_w( device_interface ), GetLastError());
        return CR_NO_SUCH_DEVICE_INTERFACE;
    }

    ret = SetupDiGetDeviceInterfacePropertyW( set, &iface, property_key, property_type, property_buffer,
                                              *property_buffer_size, property_buffer_size, 0 );
    err = ret ? 0 : GetLastError();
    SetupDiDestroyDeviceInfoList( set );
    switch (err)
    {
    case ERROR_SUCCESS:
        return CR_SUCCESS;
    case ERROR_INSUFFICIENT_BUFFER:
        return CR_BUFFER_SMALL;
    case ERROR_NOT_FOUND:
        return CR_NO_SUCH_VALUE;
    default:
        return CR_FAILURE;
    }
}
