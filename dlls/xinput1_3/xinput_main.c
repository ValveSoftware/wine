/*
 * The Wine project - Xinput Joystick Library
 * Copyright 2008 Andrew Fenn
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
#include <assert.h>
#include <stdarg.h>
#include <string.h>

#include "wine/debug.h"
#include "windef.h"
#include "winbase.h"
#include "winerror.h"
#include "winuser.h"

#include "xinput.h"
#include "xinput_private.h"

/* Not defined in the headers, used only by XInputGetStateEx */
#define XINPUT_GAMEPAD_GUIDE 0x0400

WINE_DEFAULT_DEBUG_CHANNEL(xinput);

static HINSTANCE xinput_instance;

/* xinput_crit guards controllers array */
static CRITICAL_SECTION_DEBUG xinput_critsect_debug =
{
    0, 0, &xinput_crit,
    { &xinput_critsect_debug.ProcessLocksList, &xinput_critsect_debug.ProcessLocksList },
      0, 0, { (DWORD_PTR)(__FILE__ ": xinput_crit") }
};
CRITICAL_SECTION xinput_crit = { &xinput_critsect_debug, -1, 0, 0, 0, 0 };

static CRITICAL_SECTION_DEBUG controller_critsect_debug[XUSER_MAX_COUNT] =
{
    {
        0, 0, &controllers[0].crit,
        { &controller_critsect_debug[0].ProcessLocksList, &controller_critsect_debug[0].ProcessLocksList },
          0, 0, { (DWORD_PTR)(__FILE__ ": controllers[0].crit") }
    },
    {
        0, 0, &controllers[1].crit,
        { &controller_critsect_debug[1].ProcessLocksList, &controller_critsect_debug[1].ProcessLocksList },
          0, 0, { (DWORD_PTR)(__FILE__ ": controllers[1].crit") }
    },
    {
        0, 0, &controllers[2].crit,
        { &controller_critsect_debug[2].ProcessLocksList, &controller_critsect_debug[2].ProcessLocksList },
          0, 0, { (DWORD_PTR)(__FILE__ ": controllers[2].crit") }
    },
    {
        0, 0, &controllers[3].crit,
        { &controller_critsect_debug[3].ProcessLocksList, &controller_critsect_debug[3].ProcessLocksList },
          0, 0, { (DWORD_PTR)(__FILE__ ": controllers[3].crit") }
    },
};

xinput_controller controllers[XUSER_MAX_COUNT] = {
    {{ &controller_critsect_debug[0], -1, 0, 0, 0, 0 }},
    {{ &controller_critsect_debug[1], -1, 0, 0, 0, 0 }},
    {{ &controller_critsect_debug[2], -1, 0, 0, 0, 0 }},
    {{ &controller_critsect_debug[3], -1, 0, 0, 0, 0 }},
};

static BOOL verify_and_lock_device(xinput_controller *device)
{
    if (!device->platform_private)
        return FALSE;

    EnterCriticalSection(&device->crit);

    if (!device->platform_private)
    {
        LeaveCriticalSection(&device->crit);
        return FALSE;
    }

    return TRUE;
}

static void unlock_device(xinput_controller *device)
{
    LeaveCriticalSection(&device->crit);
}

static BOOL msg_wnd_quit;
static HWND msg_wnd;
static HANDLE msg_wnd_thread;
static HINSTANCE msg_wnd_module;

static void find_gamepads(BOOL force);

static LRESULT CALLBACK device_notification_wndproc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    switch(msg)
    {
    case WM_DEVICECHANGE:
        find_gamepads(TRUE);
        break;
    case WM_USER:
        DestroyWindow(msg_wnd);
        break;
    case WM_DESTROY:
        msg_wnd_quit = TRUE;
        break;
    }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

static DWORD WINAPI msg_wnd_threadproc(void *user)
{
    MSG msg;
    WNDCLASSEXW cls = {0};
    HANDLE inst;
    HANDLE evt = user;

    static const WCHAR xinput_class[] = {'_','_','w','i','n','e','_','x','i','n','p','u','t','_','d','e','v','n','o','t','i','f','y',0};

    cls.cbSize = sizeof(cls);
    cls.hInstance = xinput_instance;
    cls.lpszClassName = xinput_class;
    cls.lpfnWndProc = device_notification_wndproc;
    RegisterClassExW(&cls);

    msg_wnd = CreateWindowExW(0, xinput_class, NULL, 0, 0, 0, 0, 0,
            HWND_MESSAGE, NULL, NULL, NULL);

    RegisterDeviceNotificationW(msg_wnd, NULL, 0);

    msg_wnd_quit = FALSE;

    SetEvent(evt);
    evt = NULL;

    while(!msg_wnd_quit && ((int)GetMessageW(&msg, msg_wnd, 0, 0)) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    EnterCriticalSection(&xinput_crit);
    inst = msg_wnd_module;
    msg_wnd_module = NULL;
    CloseHandle(msg_wnd_thread);
    msg_wnd_thread = NULL;
    msg_wnd = NULL;
    LeaveCriticalSection(&xinput_crit);

    FreeLibraryAndExitThread(inst, 0);

    return 0;
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
    switch(reason)
    {
        case DLL_PROCESS_ATTACH:
            xinput_instance = inst;
            DisableThreadLibraryCalls(inst);
            break;
        case DLL_PROCESS_DETACH:
            if (reserved) break;
            HID_destroy_gamepads(controllers);
            break;
    }
    return TRUE;
}

static void find_gamepads(BOOL force)
{
    static ULONGLONG last_check = 0;
    ULONGLONG now;

#define DELAY_BETWEEN_CHECKS_MS 2000

    now = GetTickCount64();
    if (!force && (now - last_check) < DELAY_BETWEEN_CHECKS_MS)
        return;

    EnterCriticalSection(&xinput_crit);

    if (!msg_wnd_thread)
    {
        HANDLE evt = CreateEventW(NULL, 0, 0, NULL);
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                (const WCHAR *)&msg_wnd_threadproc, &msg_wnd_module);
        msg_wnd_thread = CreateThread(NULL, 0, &msg_wnd_threadproc, evt, 0, NULL);
        WaitForSingleObject(evt, INFINITE);
        CloseHandle(evt);
    }

    if (!force && (now - last_check) < DELAY_BETWEEN_CHECKS_MS)
    {
        LeaveCriticalSection(&xinput_crit);
        return;
    }

    last_check = now;

    HID_find_gamepads(controllers);

    LeaveCriticalSection(&xinput_crit);
}

void WINAPI DECLSPEC_HOTPATCH XInputEnable(BOOL enable)
{
    int index;

    TRACE("(enable %d)\n", enable);

    /* Setting to false will stop messages from XInputSetState being sent
    to the controllers. Setting to true will send the last vibration
    value (sent to XInputSetState) to the controller and allow messages to
    be sent */

    if (enable)
    {
        find_gamepads(FALSE);
    }
    else
    {
        EnterCriticalSection(&xinput_crit);
        if(msg_wnd)
            PostMessageW(msg_wnd, WM_USER, 0, 0);
        LeaveCriticalSection(&xinput_crit);
    }

    for (index = 0; index < XUSER_MAX_COUNT; index ++)
    {
        if (!verify_and_lock_device(&controllers[index])) continue;
        HID_enable(&controllers[index], enable);
        unlock_device(&controllers[index]);
    }
}

DWORD WINAPI DECLSPEC_HOTPATCH XInputSetState(DWORD index, XINPUT_VIBRATION* vibration)
{
    DWORD ret;

    TRACE("(index %u, vibration %p)\n", index, vibration);

    find_gamepads(FALSE);

    if (index >= XUSER_MAX_COUNT)
        return ERROR_BAD_ARGUMENTS;
    if (!verify_and_lock_device(&controllers[index]))
        return ERROR_DEVICE_NOT_CONNECTED;

    ret = HID_set_state(&controllers[index], vibration);

    unlock_device(&controllers[index]);

    return ret;
}

/* Some versions of SteamOverlayRenderer hot-patch XInputGetStateEx() and call
 * XInputGetState() in the hook, so we need a wrapper. */
static DWORD xinput_get_state(DWORD index, XINPUT_STATE *state)
{
    if (!state)
        return ERROR_BAD_ARGUMENTS;

    find_gamepads(FALSE);

    if (index >= XUSER_MAX_COUNT)
        return ERROR_BAD_ARGUMENTS;
    if (!verify_and_lock_device(&controllers[index]))
        return ERROR_DEVICE_NOT_CONNECTED;

    HID_update_state(&controllers[index], state);

    if (!controllers[index].platform_private)
    {
        /* update_state may have disconnected the controller */
        unlock_device(&controllers[index]);
        return ERROR_DEVICE_NOT_CONNECTED;
    }

    unlock_device(&controllers[index]);

    return ERROR_SUCCESS;
}


DWORD WINAPI DECLSPEC_HOTPATCH XInputGetState(DWORD index, XINPUT_STATE* state)
{
    DWORD ret;

    TRACE("(index %u, state %p)!\n", index, state);

    ret = xinput_get_state(index, state);
    if (ret != ERROR_SUCCESS)
        return ret;

    /* The main difference between this and the Ex version is the media guide button */
    state->Gamepad.wButtons &= ~XINPUT_GAMEPAD_GUIDE;

    return ERROR_SUCCESS;
}

DWORD WINAPI DECLSPEC_HOTPATCH XInputGetStateEx(DWORD index, XINPUT_STATE* state)
{
    TRACE("(index %u, state %p)!\n", index, state);

    return xinput_get_state(index, state);
}

DWORD WINAPI DECLSPEC_HOTPATCH XInputGetKeystroke(DWORD index, DWORD reserved, PXINPUT_KEYSTROKE keystroke)
{
    static int warn_once;

    if (!warn_once++)
        FIXME("(index %u, reserved %u, keystroke %p) Stub!\n", index, reserved, keystroke);

    if (index >= XUSER_MAX_COUNT)
        return ERROR_BAD_ARGUMENTS;
    if (!controllers[index].platform_private)
        return ERROR_DEVICE_NOT_CONNECTED;

    return ERROR_NOT_SUPPORTED;
}

DWORD WINAPI DECLSPEC_HOTPATCH XInputGetCapabilities(DWORD index, DWORD flags, XINPUT_CAPABILITIES* capabilities)
{
    TRACE("(index %u, flags 0x%x, capabilities %p)\n", index, flags, capabilities);

    find_gamepads(FALSE);

    if (index >= XUSER_MAX_COUNT)
        return ERROR_BAD_ARGUMENTS;

    if (!verify_and_lock_device(&controllers[index]))
        return ERROR_DEVICE_NOT_CONNECTED;

    if (flags & XINPUT_FLAG_GAMEPAD && controllers[index].caps.SubType != XINPUT_DEVSUBTYPE_GAMEPAD)
    {
        unlock_device(&controllers[index]);
        return ERROR_DEVICE_NOT_CONNECTED;
    }

    memcpy(capabilities, &controllers[index].caps, sizeof(*capabilities));

    unlock_device(&controllers[index]);

    return ERROR_SUCCESS;
}

DWORD WINAPI DECLSPEC_HOTPATCH XInputGetDSoundAudioDeviceGuids(DWORD index, GUID* render_guid, GUID* capture_guid)
{
    FIXME("(index %u, render guid %p, capture guid %p) Stub!\n", index, render_guid, capture_guid);

    if (index >= XUSER_MAX_COUNT)
        return ERROR_BAD_ARGUMENTS;
    if (!controllers[index].platform_private)
        return ERROR_DEVICE_NOT_CONNECTED;

    return ERROR_NOT_SUPPORTED;
}

DWORD WINAPI DECLSPEC_HOTPATCH XInputGetBatteryInformation(DWORD index, BYTE type, XINPUT_BATTERY_INFORMATION* battery)
{
    static int once;

    if (!once++)
        FIXME("(index %u, type %u, battery %p) Stub!\n", index, type, battery);

    if (index >= XUSER_MAX_COUNT)
        return ERROR_BAD_ARGUMENTS;
    if (!controllers[index].platform_private)
        return ERROR_DEVICE_NOT_CONNECTED;

    return ERROR_NOT_SUPPORTED;
}
