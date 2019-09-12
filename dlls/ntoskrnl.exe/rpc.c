/*
 * RPC connection with services.exe
 *
 * Copyright 1995 Sven Verdoolaege
 * Copyright 2005 Mike McCormack
 * Copyright 2007 Rolf Kalbermatter
 * Copyright 2019 Micah N Gorrell for CodeWeavers
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

#include <stdarg.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winbase.h"
#include "winreg.h"
#include "winsvc.h"
#include "winternl.h"
#include "dbt.h"
#include "svcctl.h"
#include "wine/exception.h"
#include "ddk/ntifs.h"
#include "wine/debug.h"
#include "wine/heap.h"
#include "wine/rbtree.h"

#include "ntoskrnl_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(plugplay);

/******************************************************************************
 * RPC connection with services.exe
 */
void __RPC_FAR * __RPC_USER MIDL_user_allocate(SIZE_T len)
{
    return heap_alloc(len);
}

void __RPC_USER MIDL_user_free(void __RPC_FAR * ptr)
{
    heap_free(ptr);
}

static handle_t rpc_wstr_bind(RPC_WSTR str)
{
    WCHAR transport[] = SVCCTL_TRANSPORT;
    WCHAR endpoint[] = SVCCTL_ENDPOINT;
    RPC_WSTR binding_str;
    RPC_STATUS status;
    handle_t rpc_handle;

    status = RpcStringBindingComposeW(NULL, transport, str, endpoint, NULL, &binding_str);
    if (status != RPC_S_OK)
    {
        ERR("RpcStringBindingComposeW failed (%d)\n", (DWORD)status);
        return NULL;
    }

    status = RpcBindingFromStringBindingW(binding_str, &rpc_handle);
    RpcStringFreeW(&binding_str);

    if (status != RPC_S_OK)
    {
        ERR("Couldn't connect to services.exe: error code %u\n", (DWORD)status);
        return NULL;
    }

    return rpc_handle;
}

static handle_t rpc_cstr_bind(RPC_CSTR str)
{
    RPC_CSTR transport = (RPC_CSTR)SVCCTL_TRANSPORTA;
    RPC_CSTR endpoint = (RPC_CSTR)SVCCTL_ENDPOINTA;
    RPC_CSTR binding_str;
    RPC_STATUS status;
    handle_t rpc_handle;

    status = RpcStringBindingComposeA(NULL, transport, str, endpoint, NULL, &binding_str);
    if (status != RPC_S_OK)
    {
        ERR("RpcStringBindingComposeW failed (%d)\n", (DWORD)status);
        return NULL;
    }

    status = RpcBindingFromStringBindingA(binding_str, &rpc_handle);
    RpcStringFreeA(&binding_str);

    if (status != RPC_S_OK)
    {
        ERR("Couldn't connect to services.exe: error code %u\n", (DWORD)status);
        return NULL;
    }

    return rpc_handle;
}

DECLSPEC_HIDDEN handle_t __RPC_USER MACHINE_HANDLEA_bind(MACHINE_HANDLEA MachineName)
{
    return rpc_cstr_bind((RPC_CSTR)MachineName);
}

DECLSPEC_HIDDEN void __RPC_USER MACHINE_HANDLEA_unbind(MACHINE_HANDLEA MachineName, handle_t h)
{
    RpcBindingFree(&h);
}

DECLSPEC_HIDDEN handle_t __RPC_USER MACHINE_HANDLEW_bind(MACHINE_HANDLEW MachineName)
{
    return rpc_wstr_bind((RPC_WSTR)MachineName);
}

DECLSPEC_HIDDEN void __RPC_USER MACHINE_HANDLEW_unbind(MACHINE_HANDLEW MachineName, handle_t h)
{
    RpcBindingFree(&h);
}

DECLSPEC_HIDDEN handle_t __RPC_USER SVCCTL_HANDLEW_bind(SVCCTL_HANDLEW MachineName)
{
    return rpc_wstr_bind((RPC_WSTR)MachineName);
}

DECLSPEC_HIDDEN void __RPC_USER SVCCTL_HANDLEW_unbind(SVCCTL_HANDLEW MachineName, handle_t h)
{
    RpcBindingFree(&h);
}

static LONG WINAPI rpc_filter(EXCEPTION_POINTERS *eptr)
{
    return I_RpcExceptionFilter(eptr->ExceptionRecord->ExceptionCode);
}

static DWORD map_exception_code(DWORD exception_code)
{
    switch (exception_code)
    {
    case RPC_X_NULL_REF_POINTER:
        return ERROR_INVALID_ADDRESS;
    case RPC_X_ENUM_VALUE_OUT_OF_RANGE:
    case RPC_X_BYTE_COUNT_TOO_SMALL:
        return ERROR_INVALID_PARAMETER;
    case RPC_S_INVALID_BINDING:
    case RPC_X_SS_IN_NULL_CONTEXT:
        return ERROR_INVALID_HANDLE;
    default:
        return exception_code;
    }
}

DWORD send_device_notification(DEV_BROADCAST_DEVICEINTERFACE_W *broadcast, BOOL enable)
{
    DWORD err;

    __TRY
    {
        err = svcctl_SendDeviceNotification(NULL,
                    enable ? DBT_DEVICEARRIVAL : DBT_DEVICEREMOVECOMPLETE,
                    (const BYTE *) broadcast, broadcast->dbcc_size);
    }
    __EXCEPT(rpc_filter)
    {
        err = map_exception_code(GetExceptionCode());
    }
    __ENDTRY

    TRACE("send result (%d)\n", err);
    return err;
}
