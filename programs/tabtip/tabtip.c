/*
 * Copyright 2021 Connor McAdams
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

#include "windows.h"
#define COBJMACROS
#include <initguid.h>
#include "uiautomation.h"
#include "ole2.h"
#include "strsafe.h"
#include "oleacc.h"
#include "shellapi.h"
#include <wchar.h>
#include <winternl.h>

#include "wine/debug.h"
#ifndef UNICODE
#define UNICODE
#endif

WINE_DEFAULT_DEBUG_CHANNEL(tabtip);

extern HANDLE CDECL __wine_make_process_system(void);
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

enum {
    EVENT_PGM_EXIT,
    EVENT_WINE_EXIT,
    THREAD_EVENT_COUNT,
};

struct thread_data {
    HANDLE events[THREAD_EVENT_COUNT];
    HWND main_hwnd;
};

typedef struct {
    IUIAutomationFocusChangedEventHandler IUIAutomationFocusChangedEventHandler_iface;
    LONG ref;
} event_data;

DWORD last_keyup_event;
BOOL keyboard_up;
BOOL use_steam_osk;

static const char *ct_id_str[] = {
    "UIA_ButtonControlTypeId (50000)",
    "UIA_CalendarControlTypeId (50001)",
    "UIA_CheckBoxControlTypeId (50002)",
    "UIA_ComboBoxControlTypeId (50003)",
    "UIA_EditControlTypeId (50004)",
    "UIA_HyperlinkControlTypeId (50005)",
    "UIA_ImageControlTypeId (50006)",
    "UIA_ListItemControlTypeId (50007)",
    "UIA_ListControlTypeId (50008)",
    "UIA_MenuControlTypeId (50009)",
    "UIA_MenuBarControlTypeId (50010)",
    "UIA_MenuItemControlTypeId (50011)",
    "UIA_ProgressBarControlTypeId (50012)",
    "UIA_RadioButtonControlTypeId (50013)",
    "UIA_ScrollBarControlTypeId (50014)",
    "UIA_SliderControlTypeId (50015)",
    "UIA_SpinnerControlTypeId (50016)",
    "UIA_StatusBarControlTypeId (50017)",
    "UIA_TabControlTypeId (50018)",
    "UIA_TabItemControlTypeId (50019)",
    "UIA_TextControlTypeId (50020)",
    "UIA_ToolBarControlTypeId (50021)",
    "UIA_ToolTipControlTypeId (50022)",
    "UIA_TreeControlTypeId (50023)",
    "UIA_TreeItemControlTypeId (50024)",
    "UIA_CustomControlTypeId (50025)",
    "UIA_GroupControlTypeId (50026)",
    "UIA_ThumbControlTypeId (50027)",
    "UIA_DataGridControlTypeId (50028)",
    "UIA_DataItemControlTypeId (50029)",
    "UIA_DocumentControlTypeId (50030)",
    "UIA_SplitButtonControlTypeId (50031)",
    "UIA_WindowControlTypeId (50032)",
    "UIA_PaneControlTypeId (50033)",
    "UIA_HeaderControlTypeId (50034)",
    "UIA_HeaderItemControlTypeId (50035)",
    "UIA_TableControlTypeId (50036)",
    "UIA_TitleBarControlTypeId (50037)",
    "UIA_SeparatorControlTypeId (50038)",
    "UIA_SemanticZoomControlTypeId (50039)",
    "UIA_AppBarControlTypeId (50040)",
};

/*
 * IUIAutomationFocusChangedEventHandler vtbl.
 */
static inline event_data *impl_from_uia_focus_event(IUIAutomationFocusChangedEventHandler *iface)
{
    return CONTAINING_RECORD(iface, event_data, IUIAutomationFocusChangedEventHandler_iface);
}

HRESULT WINAPI uia_focus_event_QueryInterface(IUIAutomationFocusChangedEventHandler *iface,
        REFIID riid, void **ppv)
{
    event_data *This = impl_from_uia_focus_event(iface);

    WINE_TRACE("This %p, %s\n", This, debugstr_guid( riid ));
    if (IsEqualIID(riid, &IID_IUIAutomationFocusChangedEventHandler) ||
            IsEqualIID(riid, &IID_IUnknown)) {
        *ppv = iface;
    } else {
        *ppv = NULL;
        return E_NOINTERFACE;
    }

    IUIAutomationFocusChangedEventHandler_AddRef(iface);
    return S_OK;
}

ULONG WINAPI uia_focus_event_AddRef(IUIAutomationFocusChangedEventHandler* iface)
{
    event_data *This = impl_from_uia_focus_event(iface);
    ULONG ref = InterlockedIncrement(&This->ref);

    WINE_TRACE("This %p, ref %d\n", This, ref);

    return ref;
}

ULONG WINAPI uia_focus_event_Release(IUIAutomationFocusChangedEventHandler* iface)
{
    event_data *This = impl_from_uia_focus_event(iface);
    ULONG ref = InterlockedDecrement(&This->ref);

    WINE_TRACE("This %p, ref %d\n", This, ref);

    return ref;
}

static BOOL variant_to_bool(VARIANT *v)
{
    if (V_VT(v) == VT_BOOL && (V_BOOL(v) == VARIANT_TRUE))
        return TRUE;

    return FALSE;
}

/*** IUIAutomationFocusChangedEventHandler methods ***/
HRESULT WINAPI uia_focus_event_HandleFocusChangedEvent(IUIAutomationFocusChangedEventHandler *iface,
        IUIAutomationElement *sender)
{
    event_data *This = impl_from_uia_focus_event(iface);

    WINE_TRACE("This %p, sender %p\n", This, sender);
    if (sender)
    {
        RECT rect = { 0 };
        VARIANT var, var2;
        INT ct_id;
        BSTR name;

        IUIAutomationElement_get_CurrentControlType(sender, &ct_id);
        IUIAutomationElement_get_CurrentName(sender, &name);
        IUIAutomationElement_get_CurrentBoundingRectangle(sender, &rect);
        IUIAutomationElement_GetCurrentPropertyValue(sender, UIA_IsKeyboardFocusablePropertyId, &var);
        IUIAutomationElement_GetCurrentPropertyValue(sender, UIA_ValueIsReadOnlyPropertyId, &var2);

        if (use_steam_osk && (last_keyup_event < (GetTickCount() - 5000)) &&
                (ct_id == UIA_EditControlTypeId) && variant_to_bool(&var) && !variant_to_bool(&var2))
        {
            if (!keyboard_up)
            {
                WINE_TRACE("Keyboard up!\n");
                keyboard_up = TRUE;
                if (rect.left || rect.top || rect.right || rect.bottom)
                {
                    WCHAR link_buf[1024];

                    wsprintfW(link_buf, L"steam://open/keyboard?XPosition=%d&YPosition=%d&Width=%d&Height=%d&Mode=0",
                            rect.left, rect.top, (rect.right - rect.left), (rect.bottom - rect.top));
                    ShellExecuteW(NULL, NULL, link_buf, NULL, NULL, SW_SHOWNOACTIVATE);
                }
                else
                    ShellExecuteW(NULL, NULL, L"steam://open/keyboard", NULL, NULL, SW_SHOWNOACTIVATE);

                last_keyup_event = GetTickCount();
            }
        }
        else
        {
            if (keyboard_up)
            {
                WINE_TRACE("Keyboard down!\n");
                ShellExecuteW(NULL, NULL, L"steam://close/keyboard", NULL, NULL, SW_SHOWNOACTIVATE);
                keyboard_up = FALSE;
            }
        }

        if (ct_id >= 50000)
            ct_id -= 50000;
        else
            ct_id = 0;

        WINE_TRACE("element name: %s, ct_id %s, rect { %d, %d } - { %d, %d }\n", wine_dbgstr_w(name), ct_id_str[ct_id],
                rect.left, rect.top, rect.right, rect.bottom);
    }

    return S_OK;
}

IUIAutomationFocusChangedEventHandlerVtbl uia_focus_event_vtbl = {
    uia_focus_event_QueryInterface,
    uia_focus_event_AddRef,
    uia_focus_event_Release,
    uia_focus_event_HandleFocusChangedEvent,
};

static HRESULT create_uia_event_handler(IUIAutomation **uia_iface, event_data *data)
{
    HRESULT hr;

    hr = CoCreateInstance(&CLSID_CUIAutomation, NULL, CLSCTX_INPROC_SERVER,
            &IID_IUIAutomation, (void **)uia_iface);
    if (FAILED(hr))
    {
        ERR("Failed to create IUIAutomation interface, hr %#x\n", hr);
        return hr;
    }

    data->IUIAutomationFocusChangedEventHandler_iface.lpVtbl = &uia_focus_event_vtbl;
    data->ref = 1;

    hr = IUIAutomation_AddFocusChangedEventHandler(*uia_iface, NULL,
            &data->IUIAutomationFocusChangedEventHandler_iface);
    if (FAILED(hr))
        ERR("Failed to add focus changed event handler, hr %#x\n", hr);

    return hr;
}

static DWORD WINAPI tabtip_exit_watcher(LPVOID lpParam)
{
    struct thread_data *data = (struct thread_data *)lpParam;
    DWORD event;

    event = WaitForMultipleObjects(THREAD_EVENT_COUNT, data->events, FALSE, INFINITE);
    switch (event)
    {
    case EVENT_PGM_EXIT:
        break;

    case EVENT_WINE_EXIT:
        PostMessageW(data->main_hwnd, WM_DESTROY, 0, 0);
        break;

    default:
        break;
    }

    return 0;
}

static void tabtip_use_osk_check(void)
{
    const char *var = getenv("SteamDeck");

    if (var && !strcmp(var, "1"))
        use_steam_osk = TRUE;
    else
        use_steam_osk = FALSE;

    WINE_TRACE("use_steam_osk=%d\n", use_steam_osk);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
    HANDLE wine_exit_event, pgm_exit_event, started_event;
    // Register the window class.
    const wchar_t CLASS_NAME[]  = L"IPTip_Main_Window";
    struct thread_data t_data = { };
    IUIAutomation *uia_iface;
    WNDCLASSW wc = { };
    event_data data = { };
    MSG msg = { };
    int ret = 0;
    HWND hwnd;

    wine_exit_event = pgm_exit_event = started_event = NULL;
    last_keyup_event = 0;
    keyboard_up = FALSE;
    tabtip_use_osk_check();

    NtSetInformationProcess( GetCurrentProcess(), ProcessWineMakeProcessSystem,
                             &wine_exit_event, sizeof(HANDLE *) );
    pgm_exit_event = CreateEventW(NULL, 0, 0, NULL);
    started_event = CreateEventW(NULL, TRUE, FALSE, L"TABTIP_STARTED_EVENT");

    if (!pgm_exit_event || !wine_exit_event || !started_event)
    {
        ERR("Failed to create event handles!\n");
        ret = -1;
        goto exit;
    }

    if (FAILED(CoInitializeEx(NULL, COINIT_MULTITHREADED)))
    {
        ERR("CoInitialize failed!\n");
        ret = -1;
        goto exit;
    }

    if (FAILED(create_uia_event_handler(&uia_iface, &data)))
    {
        ret = -1;
        goto exit;
    }

    SetEvent(started_event);

    wc.lpfnWndProc   = WindowProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = CLASS_NAME;

    RegisterClassW(&wc);

    hwnd = CreateWindowExW(0, CLASS_NAME,
            L"Input", WS_OVERLAPPEDWINDOW, 4, 4, 0, 0, NULL,
            NULL, hInstance, NULL);

    if (!hwnd)
    {
        ERR("Failed to create hwnd!\n");
        ret = -1;
        goto exit;
    }

    t_data.events[EVENT_WINE_EXIT] = wine_exit_event;
    t_data.events[EVENT_PGM_EXIT]  = pgm_exit_event;
    t_data.main_hwnd = hwnd;
    CreateThread(NULL, 0, tabtip_exit_watcher, &t_data, 0, NULL);

    while (GetMessageW(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    SetEvent(pgm_exit_event);
    IUIAutomation_RemoveAllEventHandlers(uia_iface);
    IUIAutomation_Release(uia_iface);

    CoUninitialize();

exit:

    if (wine_exit_event)
        CloseHandle(wine_exit_event);

    if (pgm_exit_event)
        CloseHandle(pgm_exit_event);

    if (started_event)
        CloseHandle(started_event);

    return ret;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            FillRect(hdc, &ps.rcPaint, (HBRUSH) (COLOR_WINDOW+1));

            EndPaint(hwnd, &ps);
        }
        return 0;
    default:
        break;
    }

    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}
