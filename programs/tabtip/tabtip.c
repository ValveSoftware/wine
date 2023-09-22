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

static BOOL keyboard_up;
static BOOL use_steam_osk;
static unsigned int steam_app_id;

struct str_id_pair {
    int id;
    const char *str;
};

static const struct str_id_pair uia_control_type_id_strs[] = {
    { UIA_ButtonControlTypeId,       "UIA_ButtonControlTypeId", },
    { UIA_CalendarControlTypeId,     "UIA_CalendarControlTypeId", },
    { UIA_CheckBoxControlTypeId,     "UIA_CheckBoxControlTypeId", },
    { UIA_ComboBoxControlTypeId,     "UIA_ComboBoxControlTypeId", },
    { UIA_EditControlTypeId,         "UIA_EditControlTypeId", },
    { UIA_HyperlinkControlTypeId,    "UIA_HyperlinkControlTypeId", },
    { UIA_ImageControlTypeId,        "UIA_ImageControlTypeId", },
    { UIA_ListItemControlTypeId,     "UIA_ListItemControlTypeId", },
    { UIA_ListControlTypeId,         "UIA_ListControlTypeId", },
    { UIA_MenuControlTypeId,         "UIA_MenuControlTypeId", },
    { UIA_MenuBarControlTypeId,      "UIA_MenuBarControlTypeId", },
    { UIA_MenuItemControlTypeId,     "UIA_MenuItemControlTypeId", },
    { UIA_ProgressBarControlTypeId,  "UIA_ProgressBarControlTypeId", },
    { UIA_RadioButtonControlTypeId,  "UIA_RadioButtonControlTypeId", },
    { UIA_ScrollBarControlTypeId,    "UIA_ScrollBarControlTypeId", },
    { UIA_SliderControlTypeId,       "UIA_SliderControlTypeId", },
    { UIA_SpinnerControlTypeId,      "UIA_SpinnerControlTypeId", },
    { UIA_StatusBarControlTypeId,    "UIA_StatusBarControlTypeId", },
    { UIA_TabControlTypeId,          "UIA_TabControlTypeId", },
    { UIA_TabItemControlTypeId,      "UIA_TabItemControlTypeId", },
    { UIA_TextControlTypeId,         "UIA_TextControlTypeId", },
    { UIA_ToolBarControlTypeId,      "UIA_ToolBarControlTypeId", },
    { UIA_ToolTipControlTypeId,      "UIA_ToolTipControlTypeId", },
    { UIA_TreeControlTypeId,         "UIA_TreeControlTypeId", },
    { UIA_TreeItemControlTypeId,     "UIA_TreeItemControlTypeId", },
    { UIA_CustomControlTypeId,       "UIA_CustomControlTypeId", },
    { UIA_GroupControlTypeId,        "UIA_GroupControlTypeId", },
    { UIA_ThumbControlTypeId,        "UIA_ThumbControlTypeId", },
    { UIA_DataGridControlTypeId,     "UIA_DataGridControlTypeId", },
    { UIA_DataItemControlTypeId,     "UIA_DataItemControlTypeId", },
    { UIA_DocumentControlTypeId,     "UIA_DocumentControlTypeId", },
    { UIA_SplitButtonControlTypeId,  "UIA_SplitButtonControlTypeId", },
    { UIA_WindowControlTypeId,       "UIA_WindowControlTypeId", },
    { UIA_PaneControlTypeId,         "UIA_PaneControlTypeId", },
    { UIA_HeaderControlTypeId,       "UIA_HeaderControlTypeId", },
    { UIA_HeaderItemControlTypeId,   "UIA_HeaderItemControlTypeId", },
    { UIA_TableControlTypeId,        "UIA_TableControlTypeId", },
    { UIA_TitleBarControlTypeId,     "UIA_TitleBarControlTypeId", },
    { UIA_SeparatorControlTypeId,    "UIA_SeparatorControlTypeId", },
    { UIA_SemanticZoomControlTypeId, "UIA_SemanticZoomControlTypeId", },
    { UIA_AppBarControlTypeId,       "UIA_AppBarControlTypeId", },
};

static int __cdecl str_id_pair_compare(const void *a, const void *b)
{
    const int *id = a;
    const struct str_id_pair *pair = b;

    return ((*id) > pair->id) - ((*id) < pair->id);
}

#define get_str_for_id(id, id_pair) \
    get_str_from_id_pair( (id), (id_pair), (ARRAY_SIZE(id_pair)) )
static const char *get_str_from_id_pair(int id, const struct str_id_pair *id_pair, int id_pair_size)
{
    const struct str_id_pair *pair;

    if ((pair = bsearch(&id, id_pair, id_pair_size, sizeof(*pair), str_id_pair_compare)))
        return pair->str;

    return "";
}

static const WCHAR tabtip_window_class_name[]  = L"IPTip_Main_Window";
static LRESULT CALLBACK tabtip_win_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

#define MAX_LINK_BUF 4096
struct osk_link_data {
    WCHAR link_buf[MAX_LINK_BUF];
    WCHAR *link_buf_pos;
    int args_count;
};

static void osk_link_init(struct osk_link_data *data, const WCHAR *link)
{
    data->link_buf_pos = data->link_buf;
    data->link_buf_pos += wsprintfW(data->link_buf, L"%s", link);
    data->args_count = 0;
}

#define MAX_ARG_BUF 512
static const WCHAR max_int_arg_str[] = L"=-0000000000";
static const WCHAR separator_arg_start_str[] = L"?";
static const WCHAR separator_arg_cont_str[] = L"&";
static const int max_separator_str_len = max(ARRAY_SIZE(separator_arg_start_str), ARRAY_SIZE(separator_arg_cont_str));
static void osk_link_add_int_arg(struct osk_link_data *data, const WCHAR *arg, int arg_val)
{
    const WCHAR *separator_str = !data->args_count ? separator_arg_start_str : separator_arg_cont_str;
    WCHAR arg_buf[MAX_ARG_BUF] = { 0 };
    int arg_buf_len;

    if ((lstrlenW(arg) + ARRAY_SIZE(max_int_arg_str)) >= MAX_ARG_BUF)
    {
        ERR("Arg would overflow buffer, suggest upping argument buffer size.\n");
        return;
    }

    arg_buf_len = wsprintfW(arg_buf, L"%s=%d", arg, arg_val);
    if ((arg_buf_len + (MAX_LINK_BUF - (data->link_buf_pos - data->link_buf)) + max_separator_str_len) >= MAX_LINK_BUF)
    {
        ERR("Adding another arg would overflow buffer, suggest upping link buffer size.\n");
        return;
    }

    data->link_buf_pos += wsprintfW(data->link_buf_pos, L"%s%s", separator_str, arg_buf);
    data->args_count++;
}

/*
 * IUIAutomationFocusChangedEventHandler vtbl.
 */
static HRESULT WINAPI FocusChangedHandler_QueryInterface(IUIAutomationFocusChangedEventHandler *iface,
        REFIID riid, void **ppv)
{
    *ppv = NULL;
    if (IsEqualIID(riid, &IID_IUIAutomationFocusChangedEventHandler) || IsEqualIID(riid, &IID_IUnknown))
        *ppv = iface;
    else
        return E_NOINTERFACE;

    IUIAutomationFocusChangedEventHandler_AddRef(iface);
    return S_OK;
}

static ULONG WINAPI FocusChangedHandler_AddRef(IUIAutomationFocusChangedEventHandler* iface)
{
    return 2;
}

static ULONG WINAPI FocusChangedHandler_Release(IUIAutomationFocusChangedEventHandler* iface)
{
    return 1;
}

static HRESULT WINAPI FocusChangedHandler_HandleFocusChangedEvent(IUIAutomationFocusChangedEventHandler *iface,
        IUIAutomationElement *sender)
{
    BOOL is_readonly, has_kbd_focus;
    struct osk_link_data link_data = { 0 };
    RECT rect = { 0 };
    BSTR name = NULL;
    int control_type;
    HRESULT hr;
    VARIANT v;

    WINE_TRACE("sender %p\n", sender);

    /* Should never happen, handle it anyways just in case. */
    if (!sender)
        return S_OK;

    hr = IUIAutomationElement_get_CachedBoundingRectangle(sender, &rect);
    if (FAILED(hr)) WINE_ERR("Failed to get cached bounding rect, hr %#x\n", hr);

    hr = IUIAutomationElement_get_CachedControlType(sender, &control_type);
    if (FAILED(hr)) WINE_ERR("Failed to get cached control type, hr %#x\n", hr);

    hr = IUIAutomationElement_get_CachedName(sender, &name);
    if (FAILED(hr)) WINE_ERR("Failed to get cached name, hr %#x\n", hr);

    hr = IUIAutomationElement_get_CachedHasKeyboardFocus(sender, &has_kbd_focus);
    if (FAILED(hr)) WINE_ERR("Failed to get cached has keyboard focus property, hr %#x\n", hr);

    VariantInit(&v);
    hr = IUIAutomationElement_GetCachedPropertyValueEx(sender, UIA_ValueIsReadOnlyPropertyId, TRUE, &v);
    if (FAILED(hr)) WINE_ERR("Failed to get cached property value for UIA_ValueIsReadOnlyPropertyId, hr %#x\n", hr);
    is_readonly = ((V_VT(&v) == VT_BOOL) && (V_BOOL(&v) == VARIANT_TRUE));
    VariantClear(&v);

    if (use_steam_osk && (control_type == UIA_EditControlTypeId) && has_kbd_focus && !is_readonly)
    {
        osk_link_init(&link_data, L"steam://open/keyboard");
        if (steam_app_id) osk_link_add_int_arg(&link_data, L"AppID", steam_app_id);
        if (rect.left || rect.top || rect.right || rect.bottom)
        {
            osk_link_add_int_arg(&link_data, L"XPosition", rect.left);
            osk_link_add_int_arg(&link_data, L"YPosition", rect.top);
            osk_link_add_int_arg(&link_data, L"Width", (rect.right - rect.left));
            osk_link_add_int_arg(&link_data, L"Height", (rect.bottom - rect.top));
            osk_link_add_int_arg(&link_data, L"Mode", 0);
        }

        WINE_TRACE("Keyboard up!\n");
        keyboard_up = TRUE;
    }
    else if (keyboard_up)
    {
        osk_link_init(&link_data, L"steam://close/keyboard");
        if (steam_app_id) osk_link_add_int_arg(&link_data, L"AppID", steam_app_id);

        WINE_TRACE("Keyboard down!\n");
        keyboard_up = FALSE;
    }

    if (use_steam_osk && link_data.link_buf_pos && (link_data.link_buf_pos != link_data.link_buf))
        ShellExecuteW(NULL, NULL, link_data.link_buf, NULL, NULL, SW_SHOWNOACTIVATE);

    WINE_TRACE("name %s, control_type %d (%s), rect %s, has_kbd_focus %d, is_readonly %d\n", wine_dbgstr_w(name),
            control_type, get_str_for_id(control_type, uia_control_type_id_strs), wine_dbgstr_rect(&rect),
            has_kbd_focus, is_readonly);
    SysFreeString(name);

    return S_OK;
}

static const IUIAutomationFocusChangedEventHandlerVtbl FocusChangedHandlerVtbl = {
    FocusChangedHandler_QueryInterface,
    FocusChangedHandler_AddRef,
    FocusChangedHandler_Release,
    FocusChangedHandler_HandleFocusChangedEvent,
};

static IUIAutomationFocusChangedEventHandler FocusChangedHandler = { &FocusChangedHandlerVtbl };

static const int uia_cache_props[] = { UIA_BoundingRectanglePropertyId, UIA_ControlTypePropertyId, UIA_NamePropertyId,
                                       UIA_HasKeyboardFocusPropertyId, UIA_ValueIsReadOnlyPropertyId, };
static HRESULT add_uia_event_handler(IUIAutomation **uia_iface)
{
    IUIAutomationCacheRequest *cache_req = NULL;
    IUIAutomationCondition *true_cond = NULL;
    HRESULT hr;
    int i;

    hr = CoCreateInstance(&CLSID_CUIAutomation8, NULL, CLSCTX_INPROC_SERVER, &IID_IUIAutomation, (void **)uia_iface);
    if (FAILED(hr))
    {
        ERR("Failed to create IUIAutomation interface, hr %#x\n", hr);
        return hr;
    }

    hr = IUIAutomation_CreateCacheRequest(*uia_iface, &cache_req);
    if (FAILED(hr))
        goto exit;

    hr = IUIAutomation_CreateTrueCondition(*uia_iface, &true_cond);
    if (FAILED(hr))
        goto exit;

    hr = IUIAutomationCacheRequest_put_TreeFilter(cache_req, true_cond);
    if (FAILED(hr))
        goto exit;

    for (i = 0; i < ARRAY_SIZE(uia_cache_props); i++)
    {
        hr = IUIAutomationCacheRequest_AddProperty(cache_req, uia_cache_props[i]);
        if (FAILED(hr))
        {
            ERR("Failed to add prop_id %d to cache req, hr %#x\n", uia_cache_props[i], hr);
            goto exit;
        }
    }

    hr = IUIAutomation_AddFocusChangedEventHandler(*uia_iface, cache_req, &FocusChangedHandler);
    if (FAILED(hr))
        ERR("Failed to add focus changed event handler, hr %#x\n", hr);

exit:
    if (cache_req)
        IUIAutomationCacheRequest_Release(cache_req);
    if (true_cond)
        IUIAutomationCondition_Release(true_cond);

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
    IUIAutomation *uia_iface = NULL;
    WNDCLASSW wc = { };
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

    if (FAILED(add_uia_event_handler(&uia_iface)))
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

exit:
    if (uia_iface)
    {
        IUIAutomation_RemoveAllEventHandlers(uia_iface);
        IUIAutomation_Release(uia_iface);
    }

    CoUninitialize();
    if (wine_exit_event) CloseHandle(wine_exit_event);
    if (started_event) CloseHandle(started_event);

    return ret;
}
