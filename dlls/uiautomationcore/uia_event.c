/*
 * Copyright 2021 Connor Mcadams for CodeWeavers
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

#define COBJMACROS
#include "uia_private.h"

#include "wine/debug.h"
#include "wine/heap.h"

WINE_DEFAULT_DEBUG_CHANNEL(uiautomation);

DWORD tls_index = TLS_OUT_OF_INDEXES;

/*
 * Event hook callback for window creation events.
 */
void CALLBACK uia_evl_window_create_proc(HWINEVENTHOOK hWinEventHook, DWORD event,
        HWND hwnd, LONG idObject, LONG idChild, DWORD idEventThread,
        DWORD dwmsEventTime)
{
    return;
}

/*
 * Event hook callback for MSAA object focus events.
 */
void CALLBACK uia_evl_msaa_obj_focus_proc(HWINEVENTHOOK hWinEventHook, DWORD event,
        HWND hwnd, LONG idObject, LONG idChild, DWORD idEventThread,
        DWORD dwmsEventTime)
{
    return;
}

/*
 * UI Automation Event Listener functions.
 * The first time an event handler interface is added on the client side, the
 * event listener thread is created. It is responsible for listening for
 * events being raised by UIA providers and MSAA servers, and subsequently
 * handling all relevant event handler interfaces.
 */
static HRESULT uia_event_listener_thread_initialize(struct uia_evl *evl)
{
    HRESULT hr;

    hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr))
        return hr;

    if (!TlsSetValue(tls_index, (LPVOID)evl))
        FIXME("Failed to set Tls index value!\n");

    evl->object_focus_hook = SetWinEventHook(EVENT_OBJECT_FOCUS,
            EVENT_OBJECT_FOCUS, 0, uia_evl_msaa_obj_focus_proc, 0, 0, WINEVENT_OUTOFCONTEXT);
    evl->win_creation_hook = SetWinEventHook(EVENT_OBJECT_CREATE,
            EVENT_OBJECT_CREATE, 0, uia_evl_window_create_proc, 0, 0, WINEVENT_OUTOFCONTEXT);

    return S_OK;
}

static void uia_event_listener_thread_exit(struct uia_evl *evl)
{
    struct uia_data *data = evl->data;

    UnhookWinEvent(evl->object_focus_hook);
    UnhookWinEvent(evl->win_creation_hook);

    heap_free(evl);
    data->evl = NULL;
    if (!TlsSetValue(tls_index, NULL))
        FIXME("Failed to set Tls index value!\n");

    CoUninitialize();
}

static DWORD WINAPI uia_event_listener_main(LPVOID lpParam)
{
    struct uia_evl *evl = (struct uia_evl*)lpParam;
    MSG msg = { };

    PeekMessageW(&msg, NULL, WM_USER, WM_USER, PM_NOREMOVE);
    SetEvent(evl->pump_initialized);

    if (FAILED(uia_event_listener_thread_initialize(evl)))
    {
        ERR("UI Automation Event Listener thread failed to start!\n");
        return 0;
    }

    WaitForSingleObject(evl->first_event, INFINITE);
    TRACE("UI Automation Event listener thread started!\n");
    /* From here, main loop, i.e monitor for messages. */
    while (GetMessageW(&msg, NULL, 0, 0))
    {
        BOOL exit = FALSE;

        if (msg.hwnd)
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            continue;
        }

        EnterCriticalSection(&evl->ev_handler_cs);

        if (list_empty(&evl->uia_evh_list))
            exit = TRUE;

        LeaveCriticalSection(&evl->ev_handler_cs);
        TRACE("Event listener thread ran, exit %d\n", exit);
        if (exit)
            break;
    }

    uia_event_listener_thread_exit(evl);
    TRACE("Event listener thread exited.\n");
    return 0;
}

static HRESULT start_uia_event_listener(struct uia_data *data)
{
    struct uia_evl *evl;

    evl = heap_alloc_zero(sizeof(*evl));
    if (!evl)
        return E_OUTOFMEMORY;

    if (tls_index == TLS_OUT_OF_INDEXES)
        tls_index = TlsAlloc();

    evl->data = data;
    list_init(&evl->uia_evh_list);

    /*
     * Create an event handler to signal when the event listener threads
     * message pump has been initialized.
     */
    evl->pump_initialized = CreateEventW(NULL, 0, 0, NULL);
    evl->first_event = CreateEventW(NULL, 0, 0, NULL);
    InitializeCriticalSection(&evl->ev_handler_cs);

    evl->h_thread = CreateThread(NULL, 0, uia_event_listener_main, evl, 0, &evl->tid);

    /* Wait for Window message queue creation. */
    WaitForSingleObject(evl->pump_initialized, INFINITE);
    PostThreadMessageW(evl->tid, WM_NULL, 0, 0);

    data->evl = evl;

    return S_OK;
}

static HRESULT uia_evh_add_event_handler(struct uia_data *data, struct uia_evh *evh)
{
    BOOL initialized = FALSE;

    /*
     * If this is the first event handler added, signified by the event listener
     * being inactive, start it before adding the event.
     */
    if (!data->evl)
    {
        HRESULT hr;

        hr = start_uia_event_listener(data);
        if (FAILED(hr))
            return hr;

        initialized = TRUE;
    }

    EnterCriticalSection(&data->evl->ev_handler_cs);

    list_add_tail(&data->evl->uia_evh_list, &evh->entry);

    LeaveCriticalSection(&data->evl->ev_handler_cs);

    if (initialized)
        SetEvent(data->evl->first_event);
    /*
     * Awaken the thread by triggering GetMessage.
     */
    PostThreadMessageW(data->evl->tid, WM_NULL, 0, 0);

    return S_OK;
}

/*
 * FIXME: Check if ref count is incremented/decremented when added/removed.
 */
HRESULT uia_evh_add_focus_event_handler(struct uia_data *data,
        IUIAutomationFocusChangedEventHandler *handler)
{
    struct uia_evh *evh = heap_alloc_zero(sizeof(*evh));

    TRACE("data %p, handler %p\n", data, handler);
    if (!evh)
        return E_OUTOFMEMORY;

    evh->event_type = FOCUS_EVH;
    evh->u.IUIAutomationFocusChangedEvh_iface = handler;

    uia_evh_add_event_handler(data, evh);

    return S_OK;
}

/*
 * Figure out HRESULT value when removing an event handler that hasn't been
 * added.
 */
HRESULT uia_evh_remove_focus_event_handler(struct uia_data *data,
        IUIAutomationFocusChangedEventHandler *handler)
{
    struct list *evh_list = &data->evl->uia_evh_list;
    struct list *cursor, *cursor2;
    struct uia_evh *evh;

    EnterCriticalSection(&data->evl->ev_handler_cs);

    LIST_FOR_EACH_SAFE(cursor, cursor2, evh_list)
    {
        evh = LIST_ENTRY(cursor, struct uia_evh, entry);
        if (evh->event_type == FOCUS_EVH
                && evh->u.IUIAutomationFocusChangedEvh_iface == handler)
        {
            list_remove(cursor);
            IUIAutomationFocusChangedEventHandler_Release(handler);
            heap_free(evh);
            goto exit;
        }
    }

exit:

    LeaveCriticalSection(&data->evl->ev_handler_cs);
    PostThreadMessageW(data->evl->tid, WM_NULL, 0, 0);

    return S_OK;
}

HRESULT uia_evh_remove_all_event_handlers(struct uia_data *data)
{
    struct list *evh_list = &data->evl->uia_evh_list;
    struct list *cursor, *cursor2;
    struct uia_evh *evh;

    EnterCriticalSection(&data->evl->ev_handler_cs);
    LIST_FOR_EACH_SAFE(cursor, cursor2, evh_list)
    {
        evh = LIST_ENTRY(cursor, struct uia_evh, entry);
        switch (evh->event_type)
        {
            case BASIC_EVH:
                IUIAutomationEventHandler_Release(evh->u.IUIAutomationEvh_iface);
                break;

            case CHANGES_EVH:
                IUIAutomationChangesEventHandler_Release(evh->u.IUIAutomationChangesEvh_iface);
                break;

            case FOCUS_EVH:
                IUIAutomationFocusChangedEventHandler_Release(evh->u.IUIAutomationFocusChangedEvh_iface);
                break;

            case PROPERTY_EVH:
                IUIAutomationPropertyChangedEventHandler_Release(evh->u.IUIAutomationPropertyChangedEvh_iface);
                break;

            case STRUCTURE_EVH:
                IUIAutomationStructureChangedEventHandler_Release(evh->u.IUIAutomationStructureChangedEvh_iface);
                break;

            case TEXT_EDIT_EVH:
                IUIAutomationTextEditTextChangedEventHandler_Release(evh->u.IUIAutomationTextEditTextChangedEvh_iface);
                break;

            default:
                break;
        }

        list_remove(cursor);
        heap_free(evh);
    }

    LeaveCriticalSection(&data->evl->ev_handler_cs);
    PostThreadMessageW(data->evl->tid, WM_NULL, 0, 0);

    return S_OK;
}
