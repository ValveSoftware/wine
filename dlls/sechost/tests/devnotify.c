/* Test device notification registration via sechost
 *
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
#include <assert.h>

#include "windef.h"
#include "winbase.h"
#include "winuser.h"
#include "winnls.h"
#include "dbt.h"
#include "initguid.h"
#include "ddk/hidclass.h"

#include "wine/test.h"

typedef DWORD (CALLBACK *REGISTER_DEVICE_NOTIFY_CALLBACK)(HANDLE hRecipient, DWORD flags,
    DEV_BROADCAST_HDR *);

typedef struct
{
    REGISTER_DEVICE_NOTIFY_CALLBACK pNotificationCallback;
    HWND                            hRecipient;
} REGISTER_DEVICE_NOTIFY;

static HDEVNOTIFY (WINAPI * pI_ScRegisterDeviceNotification)(REGISTER_DEVICE_NOTIFY *data, LPVOID filter, DWORD flags);
static DWORD (WINAPI * pI_ScUnregisterDeviceNotification)(HDEVNOTIFY notify);

static void init_function_pointers(void)
{
    HMODULE hdll = LoadLibraryA("sechost.dll");

#define GET_PROC(func) \
    if (!(p ## func = (void*)GetProcAddress(hdll, #func))) \
      trace("GetProcAddress(%s) failed\n", #func)

    GET_PROC(I_ScRegisterDeviceNotification);
    GET_PROC(I_ScUnregisterDeviceNotification);
#undef GET_PROC
}

static DWORD CALLBACK change_callback(HANDLE hRecipient, DWORD flags, DEV_BROADCAST_HDR *dbh)
{
    return 0;
}

static void test_RegisterDeviceNotification(void)
{
    HDEVNOTIFY hnotify;
    REGISTER_DEVICE_NOTIFY data;
    DEV_BROADCAST_DEVICEINTERFACE_W dbh;
    BOOL ret;

    memset(&dbh, 0, sizeof(dbh));

    dbh.dbcc_size = sizeof(dbh);
    dbh.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
    dbh.dbcc_classguid = GUID_DEVINTERFACE_HID;

    data.pNotificationCallback = change_callback;
    data.hRecipient = NULL;

    /* Test I_ScRegisterDeviceNotification behavior */
    /* FIXME: Behavior of other flags hasn't yet been learned */
    hnotify = pI_ScRegisterDeviceNotification(&data, &dbh, 2);
    ok(hnotify != 0, "I_ScRegisterDeviceNotification failed\n");

    ret = pI_ScUnregisterDeviceNotification(hnotify);
    ok(ret, "I_ScUnregisterDeviceNotification failed with a valid handle\n");
    ret = pI_ScUnregisterDeviceNotification(hnotify);
    ok(!ret, "I_ScUnregisterDeviceNotification succeeded with an already released handle\n");
    ret = pI_ScUnregisterDeviceNotification(NULL);
    ok(!ret, "I_ScUnregisterDeviceNotification succeeded with a NULL handle\n");

    /* FIXME: Find a way to trigger a device notification for testing */
}

START_TEST(devnotify)
{
    init_function_pointers();

    if (pI_ScRegisterDeviceNotification && pI_ScUnregisterDeviceNotification)
        test_RegisterDeviceNotification();
    else
        win_skip("I_ScRegisterDeviceNotification is not available\n");
}
