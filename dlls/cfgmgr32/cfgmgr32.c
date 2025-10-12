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

static LSTATUS guid_from_string( const WCHAR *str, GUID *guid )
{
    UNICODE_STRING guid_str;
    RtlInitUnicodeString( &guid_str, str );
    if (RtlGUIDFromString( &guid_str, guid )) return ERROR_INVALID_DATA;
    return ERROR_SUCCESS;
}

static const WCHAR *guid_string( const GUID *guid, WCHAR *buffer, UINT length )
{
    swprintf( buffer, length, L"{%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}",
              guid->Data1, guid->Data2, guid->Data3, guid->Data4[0], guid->Data4[1], guid->Data4[2],
              guid->Data4[3], guid->Data4[4], guid->Data4[5], guid->Data4[6], guid->Data4[7] );
    return buffer;
}

static const WCHAR control_classW[]  = L"System\\CurrentControlSet\\Control\\Class\\";
static const WCHAR device_classesW[] = L"System\\CurrentControlSet\\Control\\DeviceClasses\\";
static const WCHAR enum_rootW[]      = L"System\\CurrentControlSet\\Enum\\";

static struct key_cache
{
    HKEY root;
    const WCHAR *prefix;
    UINT prefix_len;
    HKEY hkey;
} cache[] =
{
    { HKEY_LOCAL_MACHINE, control_classW, ARRAY_SIZE(control_classW) - 1, (HKEY)-1 },
    { HKEY_LOCAL_MACHINE, device_classesW, ARRAY_SIZE(device_classesW) - 1, (HKEY)-1 },
    { HKEY_LOCAL_MACHINE, enum_rootW, ARRAY_SIZE(enum_rootW) - 1, (HKEY)-1 },
};

static HKEY cache_root_key( HKEY root, const WCHAR *key, const WCHAR **path )
{
    HKEY hkey;

    for (struct key_cache *entry = cache; entry < cache + ARRAY_SIZE(cache); entry++)
    {
        if (entry->root != root) continue;
        if (wcsnicmp( key, entry->prefix, entry->prefix_len )) continue;
        if (path) *path = key + entry->prefix_len;

        if (entry->hkey != (HKEY)-1 || RegOpenKeyExW( root, entry->prefix, 0, KEY_ALL_ACCESS, &hkey )) return entry->hkey;
        if (InterlockedCompareExchangePointer( (void *)&entry->hkey, hkey, (HKEY)-1 ) != (HKEY)-1) RegCloseKey( hkey );
        return entry->hkey;
    }

    if (path) *path = key;
    return root;
}

static LSTATUS open_key( HKEY root, const WCHAR *key, REGSAM access, BOOL open, HKEY *hkey )
{
    if ((root = cache_root_key( root, key, &key )) == (HKEY)-1) return ERROR_FILE_NOT_FOUND;
    if (open) return RegOpenKeyExW( root, key, 0, access, hkey );
    return RegCreateKeyExW( root, key, 0, NULL, 0, access, NULL, hkey, NULL );
}

static LSTATUS open_class_key( HKEY root, const WCHAR *key, REGSAM access, BOOL open, HKEY *hkey )
{
    WCHAR path[MAX_PATH];
    swprintf( path, ARRAY_SIZE(path), L"%s%s", control_classW, key );
    return open_key( root, path, access, open, hkey );
}

static LSTATUS open_device_classes_key( HKEY root, const WCHAR *key, REGSAM access, BOOL open, HKEY *hkey )
{
    WCHAR path[MAX_PATH];
    swprintf( path, ARRAY_SIZE(path), L"%s%s", device_classesW, key );
    return open_key( root, path, access, open, hkey );
}

static CONFIGRET map_error( LSTATUS err )
{
    switch (err)
    {
    case ERROR_FILE_NOT_FOUND:                    return CR_NO_SUCH_REGISTRY_KEY;
    case ERROR_NO_MORE_ITEMS:                     return CR_NO_SUCH_VALUE;
    case ERROR_SUCCESS:                           return CR_SUCCESS;
    default: WARN( "unmapped error %lu\n", err ); return CR_FAILURE;
    }
}

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
 *           CM_Enumerate_Classes_Ex (cfgmgr32.@)
 */
CONFIGRET WINAPI CM_Enumerate_Classes_Ex( ULONG index, GUID *class, ULONG flags, HMACHINE machine )
{
    WCHAR buffer[39];
    LSTATUS err;
    HKEY root;

    TRACE( "index %lu, class %s, flags %#lx, machine %p\n", index, debugstr_guid(class), flags, machine );
    if (machine) FIXME( "machine %p not implemented!\n", machine );

    if (!class) return CR_INVALID_POINTER;
    if (flags & ~CM_ENUMERATE_CLASSES_BITS) return CR_INVALID_FLAG;

    if (flags == CM_ENUMERATE_CLASSES_INSTALLER) root = cache_root_key( HKEY_LOCAL_MACHINE, control_classW, NULL );
    else root = cache_root_key( HKEY_LOCAL_MACHINE, device_classesW, NULL );
    if (root == (HKEY)-1) return CR_NO_SUCH_REGISTRY_KEY;

    if ((err = RegEnumKeyW( root, index, buffer, ARRAY_SIZE(buffer) ))) return map_error( err );
    return map_error( guid_from_string( buffer, class ) );
}

/***********************************************************************
 *           CM_Enumerate_Classes (cfgmgr32.@)
 */
CONFIGRET WINAPI CM_Enumerate_Classes( ULONG index, GUID *class, ULONG flags )
{
    return CM_Enumerate_Classes_Ex( index, class, flags, NULL );
}

/***********************************************************************
 *           CM_Enumerate_Enumerators_ExW (cfgmgr32.@)
 */
CONFIGRET WINAPI CM_Enumerate_Enumerators_ExW( ULONG index, WCHAR *buffer, ULONG *len, ULONG flags, HMACHINE machine )
{
    LSTATUS err;
    HKEY root;

    TRACE( "index %lu, buffer %p, len %p, flags %#lx, machine %p\n", index, buffer, len, flags, machine );
    if (machine) FIXME( "machine %p not implemented!\n", machine );

    if (!buffer || !len) return CR_INVALID_POINTER;
    if (!*len && buffer) return CR_INVALID_DATA;
    if (flags) return CR_INVALID_FLAG;

    root = cache_root_key( HKEY_LOCAL_MACHINE, enum_rootW, NULL );
    if (root == (HKEY)-1) return CR_NO_SUCH_REGISTRY_KEY;

    if (!(err = RegEnumKeyExW( root, index, buffer, len, NULL, NULL, NULL, NULL ))) *len += 1;
    return map_error( err );
}

/***********************************************************************
 *           CM_Enumerate_Enumerators_ExA (cfgmgr32.@)
 */
CONFIGRET WINAPI CM_Enumerate_Enumerators_ExA( ULONG index, char *bufferA, ULONG *lenA, ULONG flags, HMACHINE machine )
{
    WCHAR bufferW[MAX_PATH];
    DWORD lenW = ARRAY_SIZE(bufferW), maxA;
    CONFIGRET ret;

    TRACE( "index %lu, bufferA %p, lenA %p, flags %#lx, machine %p\n", index, bufferA, lenA, flags, machine );

    if (!bufferA || !lenA) return CR_INVALID_POINTER;
    if (!(maxA = *lenA) && bufferA) return CR_INVALID_DATA;

    if ((ret = CM_Enumerate_Enumerators_ExW( index, bufferW, &lenW, flags, NULL ))) return ret;
    if ((*lenA = WideCharToMultiByte( CP_ACP, 0, bufferW, lenW, NULL, 0, NULL, NULL )) > maxA || !bufferA) return CR_BUFFER_SMALL;
    WideCharToMultiByte( CP_ACP, 0, bufferW, lenW, bufferA, maxA, NULL, NULL );

    return CR_SUCCESS;
}

/***********************************************************************
 *           CM_Enumerate_EnumeratorsW (cfgmgr32.@)
 */
CONFIGRET WINAPI CM_Enumerate_EnumeratorsW( ULONG index, WCHAR *buffer, ULONG *len, ULONG flags )
{
    return CM_Enumerate_Enumerators_ExW( index, buffer, len, flags, NULL );
}

/***********************************************************************
 *           CM_Enumerate_EnumeratorsA (cfgmgr32.@)
 */
CONFIGRET WINAPI CM_Enumerate_EnumeratorsA( ULONG index, char *buffer, ULONG *len, ULONG flags )
{
    return CM_Enumerate_Enumerators_ExA( index, buffer, len, flags, NULL );
}

/***********************************************************************
 *           CM_Get_Class_Key_Name_ExW (cfgmgr32.@)
 */
CONFIGRET WINAPI CM_Get_Class_Key_Name_ExW( GUID *guid, WCHAR *name, ULONG *len, ULONG flags, HMACHINE machine )
{
    UINT capacity;

    TRACE( "guid %s, name %p, len %p, flags %#lx, machine %p\n", debugstr_guid(guid), name, len, flags, machine );
    if (machine) FIXME( "machine %p not implemented!\n", machine );
    if (flags) FIXME( "flags %#lx not implemented!\n", flags );

    if (!guid || !len) return CR_INVALID_POINTER;
    if ((capacity = *len) && !name) return CR_INVALID_POINTER;

    *len = 39;
    if (capacity < *len) return CR_BUFFER_SMALL;
    guid_string( guid, name, capacity );
    return CR_SUCCESS;
}

/***********************************************************************
 *           CM_Get_Class_Key_Name_ExA (cfgmgr32.@)
 */
CONFIGRET WINAPI CM_Get_Class_Key_Name_ExA( GUID *class, char *nameA, ULONG *len, ULONG flags, HMACHINE machine )
{
    WCHAR nameW[39];
    CONFIGRET ret;

    if ((ret = CM_Get_Class_Key_Name_ExW( class, nameA ? nameW : NULL, len, flags, machine ))) return ret;
    if (nameA) WideCharToMultiByte( CP_ACP, 0, nameW, 39, nameA, 39, NULL, NULL );

    return CR_SUCCESS;
}

/***********************************************************************
 *           CM_Get_Class_Key_NameW (cfgmgr32.@)
 */
CONFIGRET WINAPI CM_Get_Class_Key_NameW( GUID *class, WCHAR *name, ULONG *len, ULONG flags )
{
    return CM_Get_Class_Key_Name_ExW( class, name, len, flags, NULL );
}

/***********************************************************************
 *           CM_Get_Class_Key_NameA (cfgmgr32.@)
 */
CONFIGRET WINAPI CM_Get_Class_Key_NameA( GUID *class, char *name, ULONG *len, ULONG flags )
{
    return CM_Get_Class_Key_Name_ExA( class, name, len, flags, NULL );
}

/***********************************************************************
 *           CM_Open_Class_Key_ExW (cfgmgr32.@)
 */
CONFIGRET WINAPI CM_Open_Class_Key_ExW( GUID *class, const WCHAR *name, REGSAM access, REGDISPOSITION disposition, HKEY *hkey, ULONG flags, HMACHINE machine )
{
    BOOL open = disposition == RegDisposition_OpenExisting;
    WCHAR buffer[39];

    TRACE( "class %s, name %s, access %#lx, disposition %#lx, hkey %p, flags %#lx\n", debugstr_guid(class), debugstr_w(name), access, disposition, hkey, flags );
    if (machine) FIXME( "machine %p not implemented!\n", machine );

    if (name) return CR_INVALID_DATA;
    if (!hkey) return CR_INVALID_POINTER;
    if (flags & ~CM_OPEN_CLASS_KEY_BITS) return CR_INVALID_FLAG;

    if (!class) *buffer = 0;
    else guid_string( class, buffer, ARRAY_SIZE(buffer) );

    if (flags == CM_OPEN_CLASS_KEY_INSTALLER) return map_error( open_class_key( HKEY_LOCAL_MACHINE, buffer, access, open, hkey ) );
    return map_error( open_device_classes_key( HKEY_LOCAL_MACHINE, buffer, access, open, hkey ) );
}

/***********************************************************************
 *           CM_Open_Class_Key_ExA (cfgmgr32.@)
 */
CONFIGRET WINAPI CM_Open_Class_Key_ExA( GUID *class, const char *nameA, REGSAM access, REGDISPOSITION disposition, HKEY *hkey, ULONG flags, HMACHINE machine )
{
    WCHAR nameW[MAX_PATH];

    TRACE( "guid %s, nameA %s, access %#lx, disposition %#lx, hkey %p, flags %#lx\n", debugstr_guid(class), debugstr_a(nameA), access, disposition, hkey, flags );

    if (nameA) MultiByteToWideChar( CP_ACP, 0, nameA, -1, nameW, ARRAY_SIZE(nameW) );
    return CM_Open_Class_Key_ExW( class, nameA ? nameW : NULL, access, disposition, hkey, flags, machine );
}

/***********************************************************************
 *           CM_Open_Class_KeyW (cfgmgr32.@)
 */
CONFIGRET WINAPI CM_Open_Class_KeyW( GUID *class, const WCHAR *name, REGSAM access, REGDISPOSITION disposition, HKEY *hkey, ULONG flags )
{
    return CM_Open_Class_Key_ExW( class, name, access, disposition, hkey, flags, NULL );
}

/***********************************************************************
 *           CM_Open_Class_KeyA (cfgmgr32.@)
 */
CONFIGRET WINAPI CM_Open_Class_KeyA( GUID *class, const char *name, REGSAM access, REGDISPOSITION disposition, HKEY *hkey, ULONG flags )
{
    return CM_Open_Class_Key_ExA( class, name, access, disposition, hkey, flags, NULL );
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
