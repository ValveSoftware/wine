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

typedef struct {
    IUIAutomationFocusChangedEventHandler IUIAutomationFocusChangedEventHandler_iface;
    LONG ref;
} event_data;

static BOOL keyboard_up;
static BOOL use_steam_osk;
static unsigned int steam_app_id;

static const WCHAR tabtip_window_class_name[]  = L"IPTip_Main_Window";
static LRESULT CALLBACK tabtip_win_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

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
        WCHAR link_buf[1024] = { 0 };
        RECT rect = { 0 };
        VARIANT var, var2;
        INT ct_id;
        BSTR name;

        IUIAutomationElement_get_CurrentControlType(sender, &ct_id);
        IUIAutomationElement_get_CurrentName(sender, &name);
        IUIAutomationElement_get_CurrentBoundingRectangle(sender, &rect);
        IUIAutomationElement_GetCurrentPropertyValue(sender, UIA_IsKeyboardFocusablePropertyId, &var);
        IUIAutomationElement_GetCurrentPropertyValue(sender, UIA_ValueIsReadOnlyPropertyId, &var2);

        if (use_steam_osk && (ct_id == UIA_EditControlTypeId) && variant_to_bool(&var) &&
                !variant_to_bool(&var2))
        {
            WCHAR *cur_buf_pos = link_buf;

            if (steam_app_id)
                cur_buf_pos += wsprintfW(cur_buf_pos, L"steam://open/keyboard?AppID=%d", steam_app_id);
            else
                cur_buf_pos += wsprintfW(cur_buf_pos, L"steam://open/keyboard");

            if (rect.left || rect.top || rect.right || rect.bottom)
            {
                if (steam_app_id)
                    wsprintfW(cur_buf_pos, L"&XPosition=%d&YPosition=%d&Width=%d&Height=%d&Mode=0",
                            rect.left, rect.top, (rect.right - rect.left), (rect.bottom - rect.top));
                else
                    wsprintfW(cur_buf_pos, L"?XPosition=%d&YPosition=%d&Width=%d&Height=%d&Mode=0",
                            rect.left, rect.top, (rect.right - rect.left), (rect.bottom - rect.top));
            }

            WINE_TRACE("Keyboard up!\n");
            keyboard_up = TRUE;
        }
        else if (keyboard_up)
        {
            if (steam_app_id)
                wsprintfW(link_buf, L"steam://close/keyboard?AppID=%d", steam_app_id);
            else
                wsprintfW(link_buf, L"steam://close/keyboard");

            WINE_TRACE("Keyboard down!\n");
            keyboard_up = FALSE;
        }

        if (lstrlenW(link_buf))
            ShellExecuteW(NULL, NULL, link_buf, NULL, NULL, SW_SHOWNOACTIVATE);

        if (ct_id >= 50000)
            ct_id -= 50000;
        else
            ct_id = 0;

        WINE_TRACE("element name: %s, ct_id %s, rect { %d, %d } - { %d, %d }\n", wine_dbgstr_w(name), ct_id_str[ct_id],
                rect.left, rect.top, rect.right, rect.bottom);
        SysFreeString(name);
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

static const char *osk_disable_appids[] = {
    "1182900", /* A Plague Tale: Requiem */
    "752590", /* A Plague Tale: Innocence */
};

static void tabtip_use_osk_check(void)
{
    const char *var = getenv("SteamDeck");

    if (var && !strcmp(var, "1"))
        use_steam_osk = TRUE;
    else
        use_steam_osk = FALSE;

    if ((var = getenv("SteamAppId")))
    {
        int i;

        for (i = 0; i < ARRAY_SIZE(osk_disable_appids); i++)
        {
            if (!strcmp(var, osk_disable_appids[i]))
            {
                WINE_TRACE("Disabling OSK auto-popup for appid %s\n", var);
                use_steam_osk = FALSE;
                break;
            }
        }
        steam_app_id = strtol(var, NULL, 10);
    }

    WINE_TRACE("use_steam_osk=%d\n", use_steam_osk);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
    HANDLE wine_exit_event, started_event;
    IUIAutomation *uia_iface;
    WNDCLASSW wc = { };
    event_data data = { };
    int ret = 0;
    HWND hwnd;

    keyboard_up = FALSE;
    tabtip_use_osk_check();

    wine_exit_event = started_event = NULL;
    NtSetInformationProcess( GetCurrentProcess(), ProcessWineMakeProcessSystem,
                             &wine_exit_event, sizeof(HANDLE *) );
    started_event = CreateEventW(NULL, TRUE, FALSE, L"TABTIP_STARTED_EVENT");
    if (!wine_exit_event || !started_event)
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

    SetEvent(started_event);

    if (FAILED(create_uia_event_handler(&uia_iface, &data)))
    {
        ret = -1;
        goto exit;
    }

    wc.lpfnWndProc   = tabtip_win_proc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = tabtip_window_class_name;
    RegisterClassW(&wc);

    hwnd = CreateWindowExW(0, tabtip_window_class_name,
            L"Input", WS_OVERLAPPEDWINDOW, 4, 4, 0, 0, NULL,
            NULL, hInstance, NULL);

    if (!hwnd)
    {
        ERR("Failed to create hwnd!\n");
        UnregisterClassW(tabtip_window_class_name, hInstance);
        ret = -1;
        goto exit;
    }

    while (MsgWaitForMultipleObjects(1, &wine_exit_event, FALSE, INFINITE, QS_ALLINPUT) != WAIT_OBJECT_0)
    {
        BOOL quit = FALSE;
        MSG msg;

        while (PeekMessageW(&msg, 0, 0, 0, PM_REMOVE))
        {
            switch (msg.message)
            {
            case WM_QUIT: /* Unlikely to ever happen, but handle anyways. */
                quit = TRUE;
                break;

            default:
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
                break;
            }
        }

        if (quit)
            break;
    }

    IUIAutomation_RemoveAllEventHandlers(uia_iface);
    IUIAutomation_Release(uia_iface);

    CoUninitialize();

exit:
    if (wine_exit_event) CloseHandle(wine_exit_event);
    if (started_event) CloseHandle(started_event);

    return ret;
}
