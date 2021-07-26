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

static HRESULT uia_evm_add_msaa_event(struct uia_evl *evl, HWND hwnd,
        LONG obj_id, LONG child_id, LONG event);
static HRESULT uia_evm_add_uia_event(struct uia_evl *evl,
        IRawElementProviderSimple *elem_prov, UINT event);

/*
 * Custom COM interface that is passed to UIA providers. Allows them to raise
 * events to be sent to any clients with active event listener threads.
 */
static inline struct uia_evlc *impl_from_IUIAEvlConnection(IUIAEvlConnection *iface)
{
    return CONTAINING_RECORD(iface, struct uia_evlc, IUIAEvlConnection_iface);
}

static HRESULT WINAPI evlc_QueryInterface(IUIAEvlConnection *iface, REFIID riid,
        void **ppvObject)
{
    struct uia_evlc *This = impl_from_IUIAEvlConnection(iface);

    TRACE("(%p)->(%s %p)\n", This, debugstr_guid(riid), ppvObject);

    if (IsEqualIID(riid, &IID_IUIAEvlConnection) ||
            IsEqualIID(riid, &IID_IUnknown))
        *ppvObject = iface;
    else
    {
        WARN("no interface: %s\n", debugstr_guid(riid));
        *ppvObject = NULL;
        return E_NOINTERFACE;
    }

    IUIAEvlConnection_AddRef(iface);

    return S_OK;
}

static ULONG WINAPI evlc_AddRef(IUIAEvlConnection *iface)
{
    struct uia_evlc *This = impl_from_IUIAEvlConnection(iface);
    ULONG ref = InterlockedIncrement(&This->ref);

    TRACE("(%p) ref = %u\n", This, ref);
    return ref;
}

static FORCEINLINE ULONG WINAPI evlc_Release(IUIAEvlConnection *iface)
{
    struct uia_evlc *This = impl_from_IUIAEvlConnection(iface);
    ULONG ref = InterlockedDecrement(&This->ref);

    TRACE("(%p) ref = %u\n", This, ref);

    if(!ref)
        heap_free(This);

    return ref;
}

static HRESULT WINAPI evlc_ProviderRaiseEvent(IUIAEvlConnection *iface,
        LONG event_type, IRawElementProviderSimple *pRetVal)
{
    struct uia_evlc *This = impl_from_IUIAEvlConnection(iface);

    TRACE("(%p)\n", This);

    /* Do stuff here. */
    IRawElementProviderSimple_AddRef(pRetVal);
    uia_evm_add_uia_event(This->evl, pRetVal, event_type);

    return S_OK;
}

static HRESULT WINAPI evlc_CheckListenerStatus(IUIAEvlConnection *iface,
        VARIANT *val)
{
    V_VT(val) = VT_BOOL;
    V_BOOL(val) = VARIANT_TRUE;

    return S_OK;
}

static const IUIAEvlConnectionVtbl uia_evlc_vtbl = {
    evlc_QueryInterface,
    evlc_AddRef,
    evlc_Release,
    evlc_ProviderRaiseEvent,
    evlc_CheckListenerStatus,
};

static HRESULT create_uia_evlc_iface(IUIAEvlConnection **iface)
{
    struct uia_evlc *uia;

    uia = heap_alloc_zero(sizeof(*uia));
    if (!uia)
        return E_OUTOFMEMORY;

    uia->IUIAEvlConnection_iface.lpVtbl = &uia_evlc_vtbl;
    uia->ref = 1;
    *iface = &uia->IUIAEvlConnection_iface;

    return S_OK;
}

static EVENTID uia_msaa_event_to_uia_event_id(LONG obj_id, LONG event)
{
    switch (event)
    {
    case EVENT_OBJECT_ACCELERATORCHANGE: return UIA_AcceleratorKeyPropertyId;
    case EVENT_OBJECT_CREATE: return UIA_StructureChangedEventId; /* StructureChangeType_ChildAdded */
    case EVENT_OBJECT_DESTROY: return UIA_StructureChangedEventId; /* StructureChangeType_ChildRemoved */
    case EVENT_OBJECT_FOCUS: return UIA_AutomationFocusChangedEventId;
    case EVENT_OBJECT_HELPCHANGE: return UIA_AutomationPropertyChangedEventId; /* UIA_HelpTextPropertyId change */
    case EVENT_OBJECT_LOCATIONCHANGE: return UIA_AutomationPropertyChangedEventId; /* UIA_BoundingRectanglePropertyId change */
    case EVENT_OBJECT_NAMECHANGE: return UIA_AutomationPropertyChangedEventId; /* UIA_NamePropertyId change */
    case EVENT_OBJECT_PARENTCHANGE: return UIA_StructureChangedEventId; /* unsure of StructureChangeType, needs tests. */
    case EVENT_OBJECT_REORDER: return UIA_StructureChangedEventId; /* StructureChangeType_ChildrenReordered? */
    case EVENT_OBJECT_SELECTION: return	UIA_SelectionItem_ElementSelectedEventId;
    case EVENT_OBJECT_SELECTIONADD: return UIA_SelectionItem_ElementAddedToSelectionEventId;
    case EVENT_OBJECT_SELECTIONREMOVE: return UIA_SelectionItem_ElementRemovedFromSelectionEventId;
    case EVENT_OBJECT_HIDE: return UIA_StructureChangedEventId; /* StructureChangeType_ChildRemoved */
    case EVENT_OBJECT_SHOW: return UIA_StructureChangedEventId; /* StructureChangeType_ChildAdded */
    case EVENT_OBJECT_STATECHANGE: return UIA_AutomationPropertyChangedEventId; /* Various property-changed events. */
    case EVENT_OBJECT_VALUECHANGE: return UIA_AutomationPropertyChangedEventId; /* UIA_RangeValueValuePropertyId or UIA_ValueValuePropertyId */
    case EVENT_SYSTEM_ALERT: return UIA_SystemAlertEventId;
    case EVENT_SYSTEM_DIALOGEND: return UIA_Window_WindowClosedEventId;
    case EVENT_SYSTEM_DIALOGSTART: return UIA_Window_WindowOpenedEventId;
    case EVENT_SYSTEM_FOREGROUND: return UIA_AutomationFocusChangedEventId;
    case EVENT_SYSTEM_MENUEND: return UIA_MenuModeEndEventId;
    case EVENT_SYSTEM_MENUPOPUPEND: return UIA_MenuClosedEventId;
    case EVENT_SYSTEM_MENUPOPUPSTART: return UIA_MenuOpenedEventId;
    case EVENT_SYSTEM_MENUSTART: return UIA_MenuModeStartEventId;
    case EVENT_SYSTEM_MINIMIZEEND: return UIA_AutomationPropertyChangedEventId; /* UIA_WindowWindowVisualStatePropertyId change */
    case EVENT_SYSTEM_MINIMIZESTART: return UIA_AutomationPropertyChangedEventId; /* UIA_WindowWindowVisualStatePropertyId change */
    case EVENT_SYSTEM_MOVESIZEEND: return UIA_AutomationPropertyChangedEventId; /* UIA_BoundingRectanglePropertyId change */
    case EVENT_SYSTEM_MOVESIZESTART: return UIA_AutomationPropertyChangedEventId; /* UIA_BoundingRectanglePropertyId change */
    case EVENT_SYSTEM_SCROLLINGEND:
    case EVENT_SYSTEM_SCROLLINGSTART:
    case 0x8015: /* FIXME: EVENT_OBJECT_CONTENTSCROLLED. Do we not have this defined? */
        if (obj_id == OBJID_VSCROLL)
            return UIA_AutomationPropertyChangedEventId; /* UIA_ScrollVerticalScrollPercentPropertyId change */
        else if (obj_id == OBJID_HSCROLL)
            return UIA_AutomationPropertyChangedEventId; /* UIA_ScrollHorizontalScrollPercentPropertyId change */
        FIXME("Scroll events only supported on OBJID_VSCROLL/OBJID_HSCROLL!\n");
        break;

    default:
        FIXME("Unimplemented mapping for MSAA event %#x to UIA event!\n", event);
        break;
    }

    return 0;
}

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
    struct uia_evl *evl;
    HRESULT hr;

    if (idObject != OBJID_CLIENT && idObject != OBJID_WINDOW)
        return;

    evl = (struct uia_evl *)TlsGetValue(tls_index);
    hr = uia_evm_add_msaa_event(evl, hwnd, idObject, idChild, event);
    if (FAILED(hr))
        FIXME("Failed to add event to event message queue!\n");
}

static void uia_evl_check_evh_evm_match(struct uia_evl *evl, struct uia_evm *evm)
{
    struct list *evh_list = &evl->uia_evh_list;
    struct list *cursor, *cursor2;
    struct uia_evh *evh;

    EnterCriticalSection(&evl->ev_handler_cs);

    LIST_FOR_EACH_SAFE(cursor, cursor2, evh_list)
    {
        evh = LIST_ENTRY(cursor, struct uia_evh, entry);

        switch (evh->event_type)
        {
            case FOCUS_EVH:
            {
                IUIAutomationFocusChangedEventHandler *handler = evh->u.IUIAutomationFocusChangedEvh_iface;

                if (evm->event != UIA_AutomationFocusChangedEventId)
                    break;

                IUIAutomationFocusChangedEventHandler_HandleFocusChangedEvent(handler, evm->elem);
                break;
            }

            default:
                break;
        }
    }

    LeaveCriticalSection(&evl->ev_handler_cs);
}

static void uia_evl_process_evm_queue(struct uia_evl *evl)
{
    struct list *evm_queue = &evl->uia_evm_queue;
    struct list *cursor, *cursor2;
    struct uia_evm *evm;
    HRESULT hr;

    if (list_empty(&evl->uia_evm_queue))
        return;

    LIST_FOR_EACH_SAFE(cursor, cursor2, evm_queue)
    {
        evm = LIST_ENTRY(cursor, struct uia_evm, entry);

        if (evm->uia_evo == UIA_EVO_MSAA)
        {
            IAccessible *acc;
            VARIANT child_id;

            hr = AccessibleObjectFromEvent(evm->u.msaa_ev.hwnd, evm->u.msaa_ev.obj_id,
                    evm->u.msaa_ev.child_id, &acc, &child_id);
            if (FAILED(hr))
                goto message_abort;

            hr = create_uia_elem_from_msaa_acc(&evm->elem, acc, V_I4(&child_id));
            if (FAILED(hr))
                goto message_abort;
        }
        else
        {
            hr = create_uia_elem_from_raw_provider(&evm->elem, evm->u.uia_ev.elem_prov);
            if (FAILED(hr))
                goto message_abort;
        }

        uia_evl_check_evh_evm_match(evl, evm);
        IUIAutomationElement_Release(evm->elem);

message_abort:
        list_remove(cursor);
        heap_free(evm);
    }
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

    /*
     * Create interface to be passed to providers so that they can signal
     * events to active listeners.
     */
    create_uia_evlc_iface(&evl->evlc_iface);

    return S_OK;
}

static void uia_event_listener_thread_exit(struct uia_evl *evl)
{
    struct uia_data *data = evl->data;

    UnhookWinEvent(evl->object_focus_hook);
    UnhookWinEvent(evl->win_creation_hook);

    CoDisconnectObject((IUnknown *)evl->evlc_iface, 0);
    IUIAEvlConnection_Release(evl->evlc_iface);

    DeleteCriticalSection(&evl->ev_handler_cs);
    DeleteCriticalSection(&evl->evm_queue_cs);

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

        uia_evl_process_evm_queue(evl);

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
    list_init(&evl->uia_evm_queue);

    /*
     * Create an event handler to signal when the event listener threads
     * message pump has been initialized.
     */
    evl->pump_initialized = CreateEventW(NULL, 0, 0, NULL);
    evl->first_event = CreateEventW(NULL, 0, 0, NULL);
    InitializeCriticalSection(&evl->ev_handler_cs);
    InitializeCriticalSection(&evl->evm_queue_cs);

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

/*
 * uia_evm (Event Message) functions.
 */

/*
 * Add an event message to the message queue.
 */
static HRESULT uia_evm_add_message_to_queue(struct uia_evl *evl, struct uia_evm *evm)
{
    EnterCriticalSection(&evl->evm_queue_cs);

    list_add_tail(&evl->uia_evm_queue, &evm->entry);

    LeaveCriticalSection(&evl->evm_queue_cs);
    PostThreadMessageW(evl->tid, WM_NULL, 0, 0);

    return S_OK;
}

static HRESULT uia_evm_add_msaa_event(struct uia_evl *evl, HWND hwnd,
        LONG obj_id, LONG child_id, LONG event)
{
    struct uia_evm *evm = heap_alloc_zero(sizeof(*evm));

    TRACE("evl %p, hwnd %p, obj_id %d, child_id %d, event_id %d\n", evl, hwnd, obj_id, child_id, event);
    if (!evm)
        return E_OUTOFMEMORY;

    evm->uia_evo = UIA_EVO_MSAA;
    evm->u.msaa_ev.hwnd = hwnd;
    evm->u.msaa_ev.obj_id = obj_id;
    evm->u.msaa_ev.child_id = child_id;
    evm->event = uia_msaa_event_to_uia_event_id(obj_id, event);

    uia_evm_add_message_to_queue(evl, evm);

    return S_OK;
}

static HRESULT uia_evm_add_uia_event(struct uia_evl *evl,
        IRawElementProviderSimple *elem_prov, UINT event)
{
    struct uia_evm *evm = heap_alloc_zero(sizeof(*evm));

    TRACE("evl %p, elem_prov %p, event %#x\n", evl, elem_prov, event);
    if (!evm)
        return E_OUTOFMEMORY;

    evm->uia_evo = UIA_EVO_UIA;
    evm->u.uia_ev.elem_prov = elem_prov;
    evm->event = event;

    uia_evm_add_message_to_queue(evl, evm);

    return S_OK;
}
