/*
 * Copyright 2022 Connor McAdams for CodeWeavers
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

#include "uia_private.h"

#include "wine/debug.h"
#include "wine/heap.h"

WINE_DEFAULT_DEBUG_CHANNEL(uiautomation);

struct uia_element {
    IUIAutomationElement9 IUIAutomationElement9_iface;
    LONG ref;

    HUIANODE node;
};

static inline struct uia_element *impl_from_IUIAutomationElement9(IUIAutomationElement9 *iface)
{
    return CONTAINING_RECORD(iface, struct uia_element, IUIAutomationElement9_iface);
}

enum uia_com_event_types {
    UIA_COM_FOCUS_EVENT_TYPE,
    UIA_COM_EVENT_TYPE_COUNT,
};

struct uia_com_event {
    struct list main_event_list_entry;
    struct list event_type_list_entry;

    const struct uia_event_info *event_info;
    struct UiaCacheRequest cache_req;
    IUIAutomationElement *elem;
    HUIAEVENT uia_event;

    int event_type;
    union {
        IUnknown *handler;
        IUIAutomationFocusChangedEventHandler *focus_handler;
    } u;
};

/*
 * UI Automation COM client event thread functions.
 */
struct uia_com_event_thread
{
    HANDLE hthread;
    HWND hwnd;
    LONG ref;

    struct list *winevent_queue;
    struct list events_list;
    struct list event_type_list[UIA_COM_EVENT_TYPE_COUNT];
};

static struct uia_com_event_thread com_event_thread;
static CRITICAL_SECTION com_event_thread_cs;
static CRITICAL_SECTION_DEBUG com_event_thread_cs_debug =
{
    0, 0, &com_event_thread_cs,
    { &com_event_thread_cs_debug.ProcessLocksList, &com_event_thread_cs_debug.ProcessLocksList },
      0, 0, { (DWORD_PTR)(__FILE__ ": com_event_thread_cs") }
};
static CRITICAL_SECTION com_event_thread_cs = { &com_event_thread_cs_debug, -1, 0, 0, 0, 0 };

#define WM_UIA_COM_EVENT_THREAD_PROCESS_WINEVENT (WM_USER + 1)
#define WM_UIA_COM_EVENT_THREAD_STOP (WM_USER + 2)
static LRESULT CALLBACK uia_com_event_thread_msg_proc(HWND hwnd, UINT msg, WPARAM wparam,
        LPARAM lparam)
{
    switch (msg)
    {
    default:
        break;
    }

    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

struct uia_queue_winevent
{
    DWORD event_id;
    HWND hwnd;
    LONG objid;
    LONG cid;
    DWORD thread;
    DWORD event_time;

    struct list winevent_queue_entry;
};

void CALLBACK uia_com_client_winevent_proc(HWINEVENTHOOK hook, DWORD event, HWND hwnd, LONG objid, LONG cid,
        DWORD thread, DWORD event_time)
{
    struct uia_queue_winevent *queue_winevent = heap_alloc_zero(sizeof(*queue_winevent));

    TRACE("%p, %ld, %p, %ld, %ld, %ld, %ld\n", hook, event, hwnd, objid, cid, thread, event_time);

    if (!queue_winevent)
    {
        ERR("Failed to allocate queue_winevent\n");
        return;
    }

    queue_winevent->event_id = event;
    queue_winevent->hwnd = hwnd;
    queue_winevent->objid = objid;
    queue_winevent->cid = cid;
    queue_winevent->thread = thread;
    queue_winevent->event_time = event_time;
    EnterCriticalSection(&com_event_thread_cs);
    if (com_event_thread.winevent_queue)
        list_add_tail(com_event_thread.winevent_queue, &queue_winevent->winevent_queue_entry);
    else
        heap_free(queue_winevent);
    LeaveCriticalSection(&com_event_thread_cs);

    PostMessageW(com_event_thread.hwnd, WM_UIA_COM_EVENT_THREAD_PROCESS_WINEVENT, 0, 0);
}

static const WCHAR *ignored_window_classes[] = {
    L"OleMainThreadWndClass",
    L"IME",
    L"Message",
};

static BOOL is_ignored_hwnd(HWND hwnd)
{
    WCHAR buf[256] = { 0 };

    if (GetClassNameW(hwnd, buf, ARRAY_SIZE(buf)))
    {
        int i;

        for (i = 0; i < ARRAY_SIZE(ignored_window_classes); i++)
        {
            if (!lstrcmpW(buf, ignored_window_classes[i]))
                return TRUE;
        }
    }

    return FALSE;
}

static HRESULT uia_com_event_thread_handle_winevent(struct uia_queue_winevent *winevent)
{
    switch (winevent->event_id)
    {
    case EVENT_OBJECT_CREATE:
    {
        struct list *cursor, *cursor2;

        if (winevent->objid != OBJID_WINDOW)
            break;

        LIST_FOR_EACH_SAFE(cursor, cursor2, &com_event_thread.events_list)
        {
            struct uia_com_event *event = LIST_ENTRY(cursor, struct uia_com_event, main_event_list_entry);
            HRESULT hr;

            hr = UiaEventAddWindow(event->uia_event, winevent->hwnd);
            if (FAILED(hr))
                WARN("UiaEventAddWindow failed with hr %#lx\n", hr);
        }
        break;
    }

    case EVENT_OBJECT_FOCUS:
    {
        IRawElementProviderSimple *elprov = NULL;
        HRESULT hr;

        if (winevent->cid == CHILDID_SELF)
        {
            hr = create_base_hwnd_provider(winevent->hwnd, &elprov);
            if (FAILED(hr))
                WARN("Failed to create BaseHwnd provider with hr %#lx\n", hr);
        }
        else
        {
            IAccessible *acc = NULL;
            VARIANT v;

            hr = AccessibleObjectFromEvent(winevent->hwnd, winevent->objid, winevent->cid, &acc, &v);
            if (FAILED(hr) || !acc)
                break;

            hr = create_msaa_provider(acc, V_I4(&v), winevent->hwnd, FALSE, TRUE, &elprov);
            IAccessible_Release(acc);
            if (FAILED(hr))
                WARN("Failed to create MSAA proxy provider with hr %#lx\n", hr);
        }

        if (!elprov)
            break;

        hr = UiaRaiseAutomationEvent(elprov, UIA_AutomationFocusChangedEventId);
        if (FAILED(hr))
            WARN("Failed to raise event with hr %#lx\n", hr);

        IRawElementProviderSimple_Release(elprov);
        break;
    }

    default:
        break;
    }

    return S_OK;
}

static HRESULT uia_com_event_thread_process_winevent_queue(struct list *winevent_queue)
{
    struct uia_queue_winevent prev_winevent = { 0 };
    struct list *cursor, *cursor2;

    if (list_empty(winevent_queue))
        return S_OK;

    LIST_FOR_EACH_SAFE(cursor, cursor2, winevent_queue)
    {
        struct uia_queue_winevent *winevent = LIST_ENTRY(cursor, struct uia_queue_winevent, winevent_queue_entry);
        HRESULT hr;

        list_remove(cursor);
        TRACE("Processing: event_id %ld, hwnd %p, objid %ld, cid %ld, thread %ld, event_time %ld\n", winevent->event_id,
                winevent->hwnd,winevent->objid, winevent->cid, winevent->thread, winevent->event_time);

        if (is_ignored_hwnd(winevent->hwnd) || ((prev_winevent.event_id == winevent->event_id) &&
                    (prev_winevent.hwnd == winevent->hwnd) && (prev_winevent.objid == winevent->objid) &&
                    (prev_winevent.cid == winevent->cid) && (prev_winevent.thread == winevent->thread) &&
                    (prev_winevent.event_time == winevent->event_time)))
        {
            heap_free(winevent);
            continue;
        }

        hr = uia_com_event_thread_handle_winevent(winevent);
        if (FAILED(hr))
            WARN("Failed to handle winevent, hr %#lx\n", hr);
        prev_winevent = *winevent;
        heap_free(winevent);
    }

    return S_OK;
}

static DWORD WINAPI uia_com_event_thread_proc(void *arg)
{
    HANDLE initialized_event = arg;
    struct list winevent_queue;
    HWINEVENTHOOK hook;
    HWND hwnd;
    MSG msg;

    list_init(&winevent_queue);
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    hwnd = CreateWindowW(L"Message", NULL, 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, NULL, NULL);
    if (!hwnd)
    {
        WARN("CreateWindow failed: %ld\n", GetLastError());
        CoUninitialize();
        FreeLibraryAndExitThread(huia_module, 1);
    }

    SetWindowLongPtrW(hwnd, GWLP_WNDPROC, (LONG_PTR)uia_com_event_thread_msg_proc);
    com_event_thread.hwnd = hwnd;
    com_event_thread.winevent_queue = &winevent_queue;
    hook = SetWinEventHook(EVENT_MIN, EVENT_MAX, 0, uia_com_client_winevent_proc, 0, 0,
            WINEVENT_OUTOFCONTEXT);

    /* Initialization complete, thread can now process window messages. */
    SetEvent(initialized_event);
    TRACE("Event thread started.\n");
    while (GetMessageW(&msg, NULL, 0, 0))
    {
        if (msg.message == WM_UIA_COM_EVENT_THREAD_STOP || msg.message == WM_UIA_COM_EVENT_THREAD_PROCESS_WINEVENT)
        {
            HRESULT hr;

            hr = uia_com_event_thread_process_winevent_queue(&winevent_queue);
            if (FAILED(hr))
                WARN("Process winevent queue failed with hr %#lx\n", hr);

            if (msg.message == WM_UIA_COM_EVENT_THREAD_STOP)
                break;
        }

        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    TRACE("Shutting down UI Automation COM event thread.\n");

    UnhookWinEvent(hook);
    DestroyWindow(hwnd);
    CoUninitialize();
    FreeLibraryAndExitThread(huia_module, 0);
}

static BOOL uia_start_com_event_thread(void)
{
    BOOL started = TRUE;

    EnterCriticalSection(&com_event_thread_cs);
    if (++com_event_thread.ref == 1)
    {
        HANDLE ready_event = NULL;
        HANDLE events[2];
        HMODULE hmodule;
        DWORD wait_obj;
        int i;

        /* Increment DLL reference count. */
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                (const WCHAR *)uia_start_com_event_thread, &hmodule);

        list_init(&com_event_thread.events_list);
        for (i = 0; i < UIA_COM_EVENT_TYPE_COUNT; i++)
            list_init(&com_event_thread.event_type_list[i]);

        events[0] = ready_event = CreateEventW(NULL, FALSE, FALSE, NULL);
        if (!(com_event_thread.hthread = CreateThread(NULL, 0, uia_com_event_thread_proc,
                ready_event, 0, NULL)))
        {
            FreeLibrary(hmodule);
            started = FALSE;
            goto exit;
        }

        events[1] = com_event_thread.hthread;
        wait_obj = WaitForMultipleObjects(2, events, FALSE, INFINITE);
        if (wait_obj != WAIT_OBJECT_0)
        {
            CloseHandle(com_event_thread.hthread);
            started = FALSE;
        }

exit:
        if (ready_event)
            CloseHandle(ready_event);
        if (!started)
            memset(&com_event_thread, 0, sizeof(com_event_thread));
    }

    LeaveCriticalSection(&com_event_thread_cs);
    return started;
}

static void uia_stop_com_event_thread(void)
{
    EnterCriticalSection(&com_event_thread_cs);
    if (!--com_event_thread.ref)
    {
        PostMessageW(com_event_thread.hwnd, WM_UIA_COM_EVENT_THREAD_STOP, 0, 0);
        CloseHandle(com_event_thread.hthread);
        memset(&com_event_thread, 0, sizeof(com_event_thread));
    }
    LeaveCriticalSection(&com_event_thread_cs);
}

static HRESULT create_uia_element(IUIAutomationElement **iface, HUIANODE node);
static void WINAPI uia_com_event_callback(struct UiaEventArgs *args, SAFEARRAY *req_data, BSTR tree_struct)
{
    struct uia_event_args *event_args = impl_from_UiaEventArgs(args);
    struct uia_com_event *com_event;
    IUIAutomationElement *elem;
    LONG idx[2] = { 0 };
    HUIANODE node;
    HRESULT hr;
    VARIANT v;

    TRACE("%p, %p, %p\n", args, req_data, tree_struct);

    hr = SafeArrayGetElement(req_data, idx, &v);
    if (FAILED(hr))
        WARN("%d: hr %#lx\n", __LINE__, hr);

    hr = UiaHUiaNodeFromVariant(&v, &node);
    if (FAILED(hr))
        WARN("%d: hr %#lx\n", __LINE__, hr);
    VariantClear(&v);

    hr = create_uia_element(&elem, node);
    if (FAILED(hr))
        WARN("%d: hr %#lx\n", __LINE__, hr);

    com_event = (struct uia_com_event *)event_args->event_handler_data;
    switch (args->Type)
    {
    case EventArgsType_Simple:
        if (args->EventId == UIA_AutomationFocusChangedEventId)
            hr = IUIAutomationFocusChangedEventHandler_HandleFocusChangedEvent(com_event->u.focus_handler, elem);
        break;

    case EventArgsType_PropertyChanged:
    case EventArgsType_StructureChanged:
    case EventArgsType_AsyncContentLoaded:
    case EventArgsType_WindowClosed:
    case EventArgsType_TextEditTextChanged:
    case EventArgsType_Changes:
    default:
        break;
    }

    if (FAILED(hr))
        WARN("Event handler failed with hr %#lx\n", hr);

    IUIAutomationElement_Release(elem);
    SafeArrayDestroy(req_data);
    SysFreeString(tree_struct);
}

static const struct UiaCondition UiaTrueCondition  = { ConditionType_True };
static const struct UiaCacheRequest DefaultCacheReq = {
    (struct UiaCondition *)&UiaTrueCondition,
    TreeScope_Element,
    NULL, 0,
    NULL, 0,
    AutomationElementMode_Full,
};

static HRESULT add_uia_com_event(const struct uia_event_info *event_info, int com_event_type,
        IUIAutomationElement *elem, IUnknown *handler, int scope, int *prop_ids, int prop_ids_count,
        struct UiaCacheRequest *cache_req)
{
    struct uia_com_event *com_event = heap_alloc_zero(sizeof(*com_event));
    struct uia_element *element;
    HUIAEVENT event;
    HRESULT hr;

    element = impl_from_IUIAutomationElement9((IUIAutomationElement9 *)elem);
    hr = uia_add_event(element->node, event_info->event_id, (UiaEventCallback *)uia_com_event_callback, scope,
            prop_ids, prop_ids_count, cache_req, (void *)com_event, &event);
    if (FAILED(hr))
    {
        heap_free(com_event);
        return hr;
    }

    if (!uia_start_com_event_thread())
    {
        ERR("Failed to start COM event thread!\n");
        heap_free(com_event);
        return E_FAIL;
    }

    switch (com_event_type)
    {
    case UIA_COM_FOCUS_EVENT_TYPE:
        hr = IUnknown_QueryInterface(handler, &IID_IUIAutomationFocusChangedEventHandler,
                (void **)&com_event->u.focus_handler);
        break;

    default:
        break;
    }

    if (FAILED(hr))
    {
        ERR("Failed to get event handler interface, hr %#lx\n", hr);
        uia_stop_com_event_thread();
        heap_free(com_event);
        return hr;
    }

    com_event->cache_req = *cache_req;
    com_event->uia_event = event;
    com_event->event_info = event_info;
    list_add_tail(&com_event_thread.events_list, &com_event->main_event_list_entry);
    list_add_tail(&com_event_thread.event_type_list[com_event_type], &com_event->event_type_list_entry);

    return S_OK;
}

static void remove_uia_com_event(struct uia_com_event *event)
{
    HRESULT hr;

    list_remove(&event->main_event_list_entry);
    list_remove(&event->event_type_list_entry);
    hr = UiaRemoveEvent(event->uia_event);
    if (FAILED(hr))
        WARN("UiaRemoveEvent failed with hr %#lx\n", hr);

    if (event->elem)
        IUIAutomationElement_Release(event->elem);
    IUnknown_Release(event->u.handler);
    heap_free(event);

    uia_stop_com_event_thread();
}

static HRESULT find_uia_com_event(const struct uia_event_info *event_info, int com_event_type,
        IUIAutomationElement *elem, IUnknown *handler, struct uia_com_event **out_event)
{
    struct list *cursor, *cursor2;
    IUnknown *unk, *unk2;
    HRESULT hr;

    *out_event = NULL;
    hr = IUnknown_QueryInterface(handler, &IID_IUnknown, (void **)&unk);
    if (FAILED(hr) || !unk)
        return hr;

    LIST_FOR_EACH_SAFE(cursor, cursor2, &com_event_thread.event_type_list[com_event_type])
    {
        struct uia_com_event *event = LIST_ENTRY(cursor, struct uia_com_event, event_type_list_entry);
        HRESULT hr;

        if (event->event_info != event_info)
            continue;

        hr = IUnknown_QueryInterface(event->u.handler, &IID_IUnknown, (void **)&unk2);
        if (FAILED(hr) || !unk2)
            continue;

        if ((unk == unk2) && (!elem || (elem == event->elem)))
            *out_event = event;

        IUnknown_Release(unk2);
        if (*out_event)
            break;
    }

    IUnknown_Release(unk);

    return S_OK;
}

/*
 * IUIAutomationElement interface.
 */
static HRESULT WINAPI uia_element_QueryInterface(IUIAutomationElement9 *iface, REFIID riid, void **ppv)
{
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IUIAutomationElement) ||
            IsEqualIID(riid, &IID_IUIAutomationElement2) || IsEqualIID(riid, &IID_IUIAutomationElement3) ||
            IsEqualIID(riid, &IID_IUIAutomationElement4) || IsEqualIID(riid, &IID_IUIAutomationElement5) ||
            IsEqualIID(riid, &IID_IUIAutomationElement6) || IsEqualIID(riid, &IID_IUIAutomationElement7) ||
            IsEqualIID(riid, &IID_IUIAutomationElement8) || IsEqualIID(riid, &IID_IUIAutomationElement9))
        *ppv = iface;
    else
        return E_NOINTERFACE;

    IUIAutomationElement9_AddRef(iface);
    return S_OK;
}

static ULONG WINAPI uia_element_AddRef(IUIAutomationElement9 *iface)
{
    struct uia_element *element = impl_from_IUIAutomationElement9(iface);
    ULONG ref = InterlockedIncrement(&element->ref);

    TRACE("%p, refcount %ld\n", element, ref);
    return ref;
}

static ULONG WINAPI uia_element_Release(IUIAutomationElement9 *iface)
{
    struct uia_element *element = impl_from_IUIAutomationElement9(iface);
    ULONG ref = InterlockedDecrement(&element->ref);

    TRACE("%p, refcount %ld\n", element, ref);
    if (!ref)
    {
        UiaNodeRelease(element->node);
        heap_free(element);
    }

    return ref;
}

static HRESULT WINAPI uia_element_SetFocus(IUIAutomationElement9 *iface)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_GetRuntimeId(IUIAutomationElement9 *iface, SAFEARRAY **runtime_id)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_FindFirst(IUIAutomationElement9 *iface, enum TreeScope scope,
        IUIAutomationCondition *condition, IUIAutomationElement **found)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_FindAll(IUIAutomationElement9 *iface, enum TreeScope scope,
        IUIAutomationCondition *condition, IUIAutomationElementArray **found)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_FindFirstBuildCache(IUIAutomationElement9 *iface, enum TreeScope scope,
        IUIAutomationCondition *condition, IUIAutomationCacheRequest *cache_req, IUIAutomationElement **found)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_FindAllBuildCache(IUIAutomationElement9 *iface, enum TreeScope scope,
        IUIAutomationCondition *condition, IUIAutomationCacheRequest *cache_req, IUIAutomationElementArray **found)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_BuildUpdatedCache(IUIAutomationElement9 *iface, IUIAutomationCacheRequest *cache_req,
        IUIAutomationElement **updated_elem)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_GetCurrentPropertyValue(IUIAutomationElement9 *iface, PROPERTYID prop_id,
        VARIANT *ret_val)
{
    TRACE("%p, %d, %p\n", iface, prop_id, ret_val);

    return IUIAutomationElement9_GetCurrentPropertyValueEx(iface, prop_id, FALSE, ret_val);
}

static HRESULT WINAPI uia_element_GetCurrentPropertyValueEx(IUIAutomationElement9 *iface, PROPERTYID prop_id,
        BOOL ignore_default, VARIANT *ret_val)
{
    struct uia_element *element = impl_from_IUIAutomationElement9(iface);
    HRESULT hr;
    VARIANT v;

    TRACE("%p, %d, %d, %p\n", iface, prop_id, ignore_default, ret_val);

    if (!ignore_default)
        FIXME("Default property values currently unimplemented\n");

    VariantInit(&v);
    hr = UiaGetPropertyValue(element->node, prop_id, &v);
    *ret_val = v;

    return hr;
}

static HRESULT WINAPI uia_element_GetCachedPropertyValue(IUIAutomationElement9 *iface, PROPERTYID prop_id,
        VARIANT *ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_GetCachedPropertyValueEx(IUIAutomationElement9 *iface, PROPERTYID prop_id,
        BOOL ignore_default, VARIANT *ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_GetCurrentPatternAs(IUIAutomationElement9 *iface, PATTERNID pattern_id,
        REFIID riid, void **out_pattern)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_GetCachedPatternAs(IUIAutomationElement9 *iface, PATTERNID pattern_id,
        REFIID riid, void **out_pattern)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_GetCurrentPattern(IUIAutomationElement9 *iface, PATTERNID pattern_id,
        IUnknown **out_pattern)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_GetCachedPattern(IUIAutomationElement9 *iface, PATTERNID pattern_id,
        IUnknown **patternObject)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_GetCachedParent(IUIAutomationElement9 *iface, IUIAutomationElement **parent)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_GetCachedChildren(IUIAutomationElement9 *iface,
        IUIAutomationElementArray **children)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_get_CurrentProcessId(IUIAutomationElement9 *iface, int *ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_get_CurrentControlType(IUIAutomationElement9 *iface, CONTROLTYPEID *ret_val)
{
    struct uia_element *element = impl_from_IUIAutomationElement9(iface);
    HRESULT hr;
    VARIANT v;

    TRACE("%p, %p\n", element, ret_val);

    *ret_val = 0;
    VariantInit(&v);
    hr = UiaGetPropertyValue(element->node, UIA_ControlTypePropertyId, &v);
    if (SUCCEEDED(hr) && V_VT(&v) == VT_I4)
        *ret_val = V_I4(&v);

    return S_OK;
}

static HRESULT WINAPI uia_element_get_CurrentLocalizedControlType(IUIAutomationElement9 *iface, BSTR *ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_get_CurrentName(IUIAutomationElement9 *iface, BSTR *ret_val)
{
    struct uia_element *element = impl_from_IUIAutomationElement9(iface);
    HRESULT hr;
    VARIANT v;

    TRACE("%p, %p\n", element, ret_val);

    *ret_val = NULL;
    VariantInit(&v);
    hr = UiaGetPropertyValue(element->node, UIA_NamePropertyId, &v);
    if (SUCCEEDED(hr) && V_VT(&v) == VT_BSTR)
        *ret_val = V_BSTR(&v);

    return S_OK;
}

static HRESULT WINAPI uia_element_get_CurrentAcceleratorKey(IUIAutomationElement9 *iface, BSTR *ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_get_CurrentAccessKey(IUIAutomationElement9 *iface, BSTR *ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_get_CurrentHasKeyboardFocus(IUIAutomationElement9 *iface, BOOL *ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_get_CurrentIsKeyboardFocusable(IUIAutomationElement9 *iface, BOOL *ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_get_CurrentIsEnabled(IUIAutomationElement9 *iface, BOOL *ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_get_CurrentAutomationId(IUIAutomationElement9 *iface, BSTR *ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_get_CurrentClassName(IUIAutomationElement9 *iface, BSTR *ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_get_CurrentHelpText(IUIAutomationElement9 *iface, BSTR *ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_get_CurrentCulture(IUIAutomationElement9 *iface, int *ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_get_CurrentIsControlElement(IUIAutomationElement9 *iface, BOOL *ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_get_CurrentIsContentElement(IUIAutomationElement9 *iface, BOOL *ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_get_CurrentIsPassword(IUIAutomationElement9 *iface, BOOL *ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_get_CurrentNativeWindowHandle(IUIAutomationElement9 *iface, UIA_HWND *ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_get_CurrentItemType(IUIAutomationElement9 *iface, BSTR *ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_get_CurrentIsOffscreen(IUIAutomationElement9 *iface, BOOL *ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_get_CurrentOrientation(IUIAutomationElement9 *iface, enum OrientationType *ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_get_CurrentFrameworkId(IUIAutomationElement9 *iface, BSTR *ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_get_CurrentIsRequiredForForm(IUIAutomationElement9 *iface, BOOL *ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_get_CurrentItemStatus(IUIAutomationElement9 *iface, BSTR *ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_get_CurrentBoundingRectangle(IUIAutomationElement9 *iface, RECT *ret_val)
{
    struct uia_element *element = impl_from_IUIAutomationElement9(iface);
    HRESULT hr;
    VARIANT v;

    TRACE("%p, %p\n", element, ret_val);

    memset(ret_val, 0, sizeof(*ret_val));
    VariantInit(&v);
    hr = UiaGetPropertyValue(element->node, UIA_BoundingRectanglePropertyId, &v);
    if (SUCCEEDED(hr) && V_VT(&v) == (VT_R8 | VT_ARRAY))
    {
        double vals[4];
        LONG idx;

        for (idx = 0; idx < ARRAY_SIZE(vals); idx++)
            SafeArrayGetElement(V_ARRAY(&v), &idx, &vals[idx]);

        ret_val->left = vals[0];
        ret_val->top = vals[1];
        ret_val->right = ret_val->left + vals[2];
        ret_val->bottom = ret_val->top + vals[3];
        VariantClear(&v);
    }

    return S_OK;
}

static HRESULT WINAPI uia_element_get_CurrentLabeledBy(IUIAutomationElement9 *iface, IUIAutomationElement **ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_get_CurrentAriaRole(IUIAutomationElement9 *iface, BSTR *ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_get_CurrentAriaProperties(IUIAutomationElement9 *iface, BSTR *ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_get_CurrentIsDataValidForForm(IUIAutomationElement9 *iface, BOOL *ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_get_CurrentControllerFor(IUIAutomationElement9 *iface,
        IUIAutomationElementArray **ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_get_CurrentDescribedBy(IUIAutomationElement9 *iface,
        IUIAutomationElementArray **ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_get_CurrentFlowsTo(IUIAutomationElement9 *iface, IUIAutomationElementArray **ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_get_CurrentProviderDescription(IUIAutomationElement9 *iface, BSTR *ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_get_CachedProcessId(IUIAutomationElement9 *iface, int *ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_get_CachedControlType(IUIAutomationElement9 *iface, CONTROLTYPEID *ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_get_CachedLocalizedControlType(IUIAutomationElement9 *iface, BSTR *ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_get_CachedName(IUIAutomationElement9 *iface, BSTR *ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_get_CachedAcceleratorKey(IUIAutomationElement9 *iface, BSTR *ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_get_CachedAccessKey(IUIAutomationElement9 *iface, BSTR *ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_get_CachedHasKeyboardFocus(IUIAutomationElement9 *iface, BOOL *ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_get_CachedIsKeyboardFocusable(IUIAutomationElement9 *iface, BOOL *ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_get_CachedIsEnabled(IUIAutomationElement9 *iface, BOOL *ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_get_CachedAutomationId(IUIAutomationElement9 *iface, BSTR *ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_get_CachedClassName(IUIAutomationElement9 *iface, BSTR *ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_get_CachedHelpText(IUIAutomationElement9 *iface, BSTR *ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_get_CachedCulture(IUIAutomationElement9 *iface, int *ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_get_CachedIsControlElement(IUIAutomationElement9 *iface, BOOL *ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_get_CachedIsContentElement(IUIAutomationElement9 *iface, BOOL *ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_get_CachedIsPassword(IUIAutomationElement9 *iface, BOOL *ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_get_CachedNativeWindowHandle(IUIAutomationElement9 *iface, UIA_HWND *ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_get_CachedItemType(IUIAutomationElement9 *iface, BSTR *ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_get_CachedIsOffscreen(IUIAutomationElement9 *iface, BOOL *ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_get_CachedOrientation(IUIAutomationElement9 *iface,
        enum OrientationType *ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_get_CachedFrameworkId(IUIAutomationElement9 *iface, BSTR *ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_get_CachedIsRequiredForForm(IUIAutomationElement9 *iface, BOOL *ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_get_CachedItemStatus(IUIAutomationElement9 *iface, BSTR *ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_get_CachedBoundingRectangle(IUIAutomationElement9 *iface, RECT *ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_get_CachedLabeledBy(IUIAutomationElement9 *iface, IUIAutomationElement **ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_get_CachedAriaRole(IUIAutomationElement9 *iface, BSTR *ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_get_CachedAriaProperties(IUIAutomationElement9 *iface, BSTR *ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_get_CachedIsDataValidForForm(IUIAutomationElement9 *iface, BOOL *ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_get_CachedControllerFor(IUIAutomationElement9 *iface,
        IUIAutomationElementArray **ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_get_CachedDescribedBy(IUIAutomationElement9 *iface,
        IUIAutomationElementArray **ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_get_CachedFlowsTo(IUIAutomationElement9 *iface, IUIAutomationElementArray **ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_get_CachedProviderDescription(IUIAutomationElement9 *iface, BSTR *ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_GetClickablePoint(IUIAutomationElement9 *iface, POINT *clickable, BOOL *got_clickable)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_get_CurrentOptimizeForVisualContent(IUIAutomationElement9 *iface, BOOL *ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_get_CachedOptimizeForVisualContent(IUIAutomationElement9 *iface, BOOL *ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_get_CurrentLiveSetting(IUIAutomationElement9 *iface, enum LiveSetting *ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_get_CachedLiveSetting(IUIAutomationElement9 *iface, enum LiveSetting *ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_get_CurrentFlowsFrom(IUIAutomationElement9 *iface,
        IUIAutomationElementArray **ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_get_CachedFlowsFrom(IUIAutomationElement9 *iface, IUIAutomationElementArray **ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_ShowContextMenu(IUIAutomationElement9 *iface)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_get_CurrentIsPeripheral(IUIAutomationElement9 *iface, BOOL *ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_get_CachedIsPeripheral(IUIAutomationElement9 *iface, BOOL *ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_get_CurrentPositionInSet(IUIAutomationElement9 *iface, int *ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_get_CurrentSizeOfSet(IUIAutomationElement9 *iface, int *ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_get_CurrentLevel(IUIAutomationElement9 *iface, int *ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_get_CurrentAnnotationTypes(IUIAutomationElement9 *iface, SAFEARRAY **ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_get_CurrentAnnotationObjects(IUIAutomationElement9 *iface,
        IUIAutomationElementArray **ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_get_CachedPositionInSet(IUIAutomationElement9 *iface, int *ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_get_CachedSizeOfSet(IUIAutomationElement9 *iface, int *ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_get_CachedLevel(IUIAutomationElement9 *iface, int *ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_get_CachedAnnotationTypes(IUIAutomationElement9 *iface, SAFEARRAY **ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_get_CachedAnnotationObjects(IUIAutomationElement9 *iface,
        IUIAutomationElementArray **ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_get_CurrentLandmarkType(IUIAutomationElement9 *iface, LANDMARKTYPEID *ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_get_CurrentLocalizedLandmarkType(IUIAutomationElement9 *iface, BSTR *ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_get_CachedLandmarkType(IUIAutomationElement9 *iface, LANDMARKTYPEID *ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_get_CachedLocalizedLandmarkType(IUIAutomationElement9 *iface, BSTR *ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_get_CurrentFullDescription(IUIAutomationElement9 *iface, BSTR *ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_get_CachedFullDescription(IUIAutomationElement9 *iface, BSTR *ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_FindFirstWithOptions(IUIAutomationElement9 *iface, enum TreeScope scope,
        IUIAutomationCondition *condition, enum TreeTraversalOptions traversal_opts, IUIAutomationElement *root,
        IUIAutomationElement **found)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_FindAllWithOptions(IUIAutomationElement9 *iface, enum TreeScope scope,
        IUIAutomationCondition *condition, enum TreeTraversalOptions traversal_opts, IUIAutomationElement *root,
        IUIAutomationElementArray **found)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_FindFirstWithOptionsBuildCache(IUIAutomationElement9 *iface, enum TreeScope scope,
        IUIAutomationCondition *condition, IUIAutomationCacheRequest *cache_req,
        enum TreeTraversalOptions traversal_opts, IUIAutomationElement *root, IUIAutomationElement **found)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_FindAllWithOptionsBuildCache(IUIAutomationElement9 *iface, enum TreeScope scope,
        IUIAutomationCondition *condition, IUIAutomationCacheRequest *cache_req,
        enum TreeTraversalOptions traversal_opts, IUIAutomationElement *root, IUIAutomationElementArray **found)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_GetCurrentMetadataValue(IUIAutomationElement9 *iface, int target_id,
        METADATAID metadata_id, VARIANT *ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_get_CurrentHeadingLevel(IUIAutomationElement9 *iface, HEADINGLEVELID *ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_get_CachedHeadingLevel(IUIAutomationElement9 *iface, HEADINGLEVELID *ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_get_CurrentIsDialog(IUIAutomationElement9 *iface, BOOL *ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_element_get_CachedIsDialog(IUIAutomationElement9 *iface, BOOL *ret_val)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static const IUIAutomationElement9Vtbl uia_element_vtbl = {
    uia_element_QueryInterface,
    uia_element_AddRef,
    uia_element_Release,
    uia_element_SetFocus,
    uia_element_GetRuntimeId,
    uia_element_FindFirst,
    uia_element_FindAll,
    uia_element_FindFirstBuildCache,
    uia_element_FindAllBuildCache,
    uia_element_BuildUpdatedCache,
    uia_element_GetCurrentPropertyValue,
    uia_element_GetCurrentPropertyValueEx,
    uia_element_GetCachedPropertyValue,
    uia_element_GetCachedPropertyValueEx,
    uia_element_GetCurrentPatternAs,
    uia_element_GetCachedPatternAs,
    uia_element_GetCurrentPattern,
    uia_element_GetCachedPattern,
    uia_element_GetCachedParent,
    uia_element_GetCachedChildren,
    uia_element_get_CurrentProcessId,
    uia_element_get_CurrentControlType,
    uia_element_get_CurrentLocalizedControlType,
    uia_element_get_CurrentName,
    uia_element_get_CurrentAcceleratorKey,
    uia_element_get_CurrentAccessKey,
    uia_element_get_CurrentHasKeyboardFocus,
    uia_element_get_CurrentIsKeyboardFocusable,
    uia_element_get_CurrentIsEnabled,
    uia_element_get_CurrentAutomationId,
    uia_element_get_CurrentClassName,
    uia_element_get_CurrentHelpText,
    uia_element_get_CurrentCulture,
    uia_element_get_CurrentIsControlElement,
    uia_element_get_CurrentIsContentElement,
    uia_element_get_CurrentIsPassword,
    uia_element_get_CurrentNativeWindowHandle,
    uia_element_get_CurrentItemType,
    uia_element_get_CurrentIsOffscreen,
    uia_element_get_CurrentOrientation,
    uia_element_get_CurrentFrameworkId,
    uia_element_get_CurrentIsRequiredForForm,
    uia_element_get_CurrentItemStatus,
    uia_element_get_CurrentBoundingRectangle,
    uia_element_get_CurrentLabeledBy,
    uia_element_get_CurrentAriaRole,
    uia_element_get_CurrentAriaProperties,
    uia_element_get_CurrentIsDataValidForForm,
    uia_element_get_CurrentControllerFor,
    uia_element_get_CurrentDescribedBy,
    uia_element_get_CurrentFlowsTo,
    uia_element_get_CurrentProviderDescription,
    uia_element_get_CachedProcessId,
    uia_element_get_CachedControlType,
    uia_element_get_CachedLocalizedControlType,
    uia_element_get_CachedName,
    uia_element_get_CachedAcceleratorKey,
    uia_element_get_CachedAccessKey,
    uia_element_get_CachedHasKeyboardFocus,
    uia_element_get_CachedIsKeyboardFocusable,
    uia_element_get_CachedIsEnabled,
    uia_element_get_CachedAutomationId,
    uia_element_get_CachedClassName,
    uia_element_get_CachedHelpText,
    uia_element_get_CachedCulture,
    uia_element_get_CachedIsControlElement,
    uia_element_get_CachedIsContentElement,
    uia_element_get_CachedIsPassword,
    uia_element_get_CachedNativeWindowHandle,
    uia_element_get_CachedItemType,
    uia_element_get_CachedIsOffscreen,
    uia_element_get_CachedOrientation,
    uia_element_get_CachedFrameworkId,
    uia_element_get_CachedIsRequiredForForm,
    uia_element_get_CachedItemStatus,
    uia_element_get_CachedBoundingRectangle,
    uia_element_get_CachedLabeledBy,
    uia_element_get_CachedAriaRole,
    uia_element_get_CachedAriaProperties,
    uia_element_get_CachedIsDataValidForForm,
    uia_element_get_CachedControllerFor,
    uia_element_get_CachedDescribedBy,
    uia_element_get_CachedFlowsTo,
    uia_element_get_CachedProviderDescription,
    uia_element_GetClickablePoint,
    uia_element_get_CurrentOptimizeForVisualContent,
    uia_element_get_CachedOptimizeForVisualContent,
    uia_element_get_CurrentLiveSetting,
    uia_element_get_CachedLiveSetting,
    uia_element_get_CurrentFlowsFrom,
    uia_element_get_CachedFlowsFrom,
    uia_element_ShowContextMenu,
    uia_element_get_CurrentIsPeripheral,
    uia_element_get_CachedIsPeripheral,
    uia_element_get_CurrentPositionInSet,
    uia_element_get_CurrentSizeOfSet,
    uia_element_get_CurrentLevel,
    uia_element_get_CurrentAnnotationTypes,
    uia_element_get_CurrentAnnotationObjects,
    uia_element_get_CachedPositionInSet,
    uia_element_get_CachedSizeOfSet,
    uia_element_get_CachedLevel,
    uia_element_get_CachedAnnotationTypes,
    uia_element_get_CachedAnnotationObjects,
    uia_element_get_CurrentLandmarkType,
    uia_element_get_CurrentLocalizedLandmarkType,
    uia_element_get_CachedLandmarkType,
    uia_element_get_CachedLocalizedLandmarkType,
    uia_element_get_CurrentFullDescription,
    uia_element_get_CachedFullDescription,
    uia_element_FindFirstWithOptions,
    uia_element_FindAllWithOptions,
    uia_element_FindFirstWithOptionsBuildCache,
    uia_element_FindAllWithOptionsBuildCache,
    uia_element_GetCurrentMetadataValue,
    uia_element_get_CurrentHeadingLevel,
    uia_element_get_CachedHeadingLevel,
    uia_element_get_CurrentIsDialog,
    uia_element_get_CachedIsDialog,
};

static HRESULT create_uia_element(IUIAutomationElement **iface, HUIANODE node)
{
    struct uia_element *element = heap_alloc_zero(sizeof(*element));

    *iface = NULL;
    if (!element)
        return E_OUTOFMEMORY;

    element->IUIAutomationElement9_iface.lpVtbl = &uia_element_vtbl;
    element->ref = 1;
    element->node = node;

    *iface = (IUIAutomationElement *)&element->IUIAutomationElement9_iface;
    return S_OK;
}
/*
 * IUIAutomation interface.
 */
struct uia_iface {
    IUIAutomation6 IUIAutomation6_iface;
    LONG ref;

    BOOL is_cui8;
};

static inline struct uia_iface *impl_from_IUIAutomation6(IUIAutomation6 *iface)
{
    return CONTAINING_RECORD(iface, struct uia_iface, IUIAutomation6_iface);
}

static HRESULT WINAPI uia_iface_QueryInterface(IUIAutomation6 *iface, REFIID riid, void **ppv)
{
    struct uia_iface *uia_iface = impl_from_IUIAutomation6(iface);

    *ppv = NULL;
    if (IsEqualIID(riid, &IID_IUIAutomation) || IsEqualIID(riid, &IID_IUnknown))
        *ppv = iface;
    else if (uia_iface->is_cui8 &&
            (IsEqualIID(riid, &IID_IUIAutomation2) ||
            IsEqualIID(riid, &IID_IUIAutomation3) ||
            IsEqualIID(riid, &IID_IUIAutomation4) ||
            IsEqualIID(riid, &IID_IUIAutomation5) ||
            IsEqualIID(riid, &IID_IUIAutomation6)))
        *ppv = iface;
    else
        return E_NOINTERFACE;

    IUIAutomation6_AddRef(iface);
    return S_OK;
}

static ULONG WINAPI uia_iface_AddRef(IUIAutomation6 *iface)
{
    struct uia_iface *uia_iface = impl_from_IUIAutomation6(iface);
    ULONG ref = InterlockedIncrement(&uia_iface->ref);

    TRACE("%p, refcount %ld\n", uia_iface, ref);
    return ref;
}

static ULONG WINAPI uia_iface_Release(IUIAutomation6 *iface)
{
    struct uia_iface *uia_iface = impl_from_IUIAutomation6(iface);
    ULONG ref = InterlockedDecrement(&uia_iface->ref);

    TRACE("%p, refcount %ld\n", uia_iface, ref);
    if (!ref)
        heap_free(uia_iface);
    return ref;
}

static HRESULT WINAPI uia_iface_CompareElements(IUIAutomation6 *iface, IUIAutomationElement *elem1,
        IUIAutomationElement *elem2, BOOL *match)
{
    FIXME("%p, %p, %p, %p: stub\n", iface, elem1, elem2, match);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_iface_CompareRuntimeIds(IUIAutomation6 *iface, SAFEARRAY *rt_id1, SAFEARRAY *rt_id2,
        BOOL *match)
{
    FIXME("%p, %p, %p, %p: stub\n", iface, rt_id1, rt_id2, match);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_iface_GetRootElement(IUIAutomation6 *iface, IUIAutomationElement **root)
{
    FIXME("%p, %p: stub\n", iface, root);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_iface_ElementFromHandle(IUIAutomation6 *iface, UIA_HWND hwnd, IUIAutomationElement **out_elem)
{
    HUIANODE node;
    HRESULT hr;

    TRACE("%p, %p, %p\n", iface, hwnd, out_elem);

    hr = UiaNodeFromHandle((HWND)hwnd, &node);
    if (FAILED(hr) || !node)
        return hr;

    return create_uia_element(out_elem, node);
}

static HRESULT WINAPI uia_iface_ElementFromPoint(IUIAutomation6 *iface, POINT pt, IUIAutomationElement **out_elem)
{
    FIXME("%p, %s, %p: stub\n", iface, wine_dbgstr_point(&pt), out_elem);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_iface_GetFocusedElement(IUIAutomation6 *iface, IUIAutomationElement **out_elem)
{
    FIXME("%p, %p: stub\n", iface, out_elem);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_iface_GetRootElementBuildCache(IUIAutomation6 *iface, IUIAutomationCacheRequest *cache_req,
        IUIAutomationElement **out_root)
{
    FIXME("%p, %p, %p: stub\n", iface, cache_req, out_root);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_iface_ElementFromHandleBuildCache(IUIAutomation6 *iface, UIA_HWND hwnd,
        IUIAutomationCacheRequest *cache_req, IUIAutomationElement **out_elem)
{
    FIXME("%p, %p, %p, %p: stub\n", iface, hwnd, cache_req, out_elem);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_iface_ElementFromPointBuildCache(IUIAutomation6 *iface, POINT pt,
        IUIAutomationCacheRequest *cache_req, IUIAutomationElement **out_elem)
{
    FIXME("%p, %s, %p, %p: stub\n", iface, wine_dbgstr_point(&pt), cache_req, out_elem);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_iface_GetFocusedElementBuildCache(IUIAutomation6 *iface,
        IUIAutomationCacheRequest *cache_req, IUIAutomationElement **out_elem)
{
    FIXME("%p, %p, %p: stub\n", iface, cache_req, out_elem);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_iface_CreateTreeWalker(IUIAutomation6 *iface, IUIAutomationCondition *cond,
        IUIAutomationTreeWalker **out_walker)
{
    FIXME("%p, %p, %p: stub\n", iface, cond, out_walker);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_iface_get_ControlViewWalker(IUIAutomation6 *iface, IUIAutomationTreeWalker **out_walker)
{
    FIXME("%p, %p: stub\n", iface, out_walker);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_iface_get_ContentViewWalker(IUIAutomation6 *iface, IUIAutomationTreeWalker **out_walker)
{
    FIXME("%p, %p: stub\n", iface, out_walker);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_iface_get_RawViewWalker(IUIAutomation6 *iface, IUIAutomationTreeWalker **out_walker)
{
    FIXME("%p, %p: stub\n", iface, out_walker);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_iface_get_RawViewCondition(IUIAutomation6 *iface, IUIAutomationCondition **out_condition)
{
    FIXME("%p, %p: stub\n", iface, out_condition);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_iface_get_ControlViewCondition(IUIAutomation6 *iface, IUIAutomationCondition **out_condition)
{
    FIXME("%p, %p: stub\n", iface, out_condition);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_iface_get_ContentViewCondition(IUIAutomation6 *iface, IUIAutomationCondition **out_condition)
{
    FIXME("%p, %p: stub\n", iface, out_condition);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_iface_CreateCacheRequest(IUIAutomation6 *iface, IUIAutomationCacheRequest **out_cache_req)
{
    FIXME("%p, %p: stub\n", iface, out_cache_req);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_iface_CreateTrueCondition(IUIAutomation6 *iface, IUIAutomationCondition **out_condition)
{
    FIXME("%p, %p: stub\n", iface, out_condition);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_iface_CreateFalseCondition(IUIAutomation6 *iface, IUIAutomationCondition **out_condition)
{
    FIXME("%p, %p: stub\n", iface, out_condition);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_iface_CreatePropertyCondition(IUIAutomation6 *iface, PROPERTYID prop_id, VARIANT val,
        IUIAutomationCondition **out_condition)
{
    FIXME("%p, %d, %s, %p: stub\n", iface, prop_id, debugstr_variant(&val), out_condition);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_iface_CreatePropertyConditionEx(IUIAutomation6 *iface, PROPERTYID prop_id, VARIANT val,
        enum PropertyConditionFlags flags, IUIAutomationCondition **out_condition)
{
    FIXME("%p, %d, %s, %#x, %p: stub\n", iface, prop_id, debugstr_variant(&val), flags, out_condition);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_iface_CreateAndCondition(IUIAutomation6 *iface, IUIAutomationCondition *cond1,
        IUIAutomationCondition *cond2, IUIAutomationCondition **out_condition)
{
    FIXME("%p, %p, %p, %p: stub\n", iface, cond1, cond2, out_condition);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_iface_CreateAndConditionFromArray(IUIAutomation6 *iface, SAFEARRAY *conds,
        IUIAutomationCondition **out_condition)
{
    FIXME("%p, %p, %p: stub\n", iface, conds, out_condition);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_iface_CreateAndConditionFromNativeArray(IUIAutomation6 *iface, IUIAutomationCondition **conds,
        int conds_count, IUIAutomationCondition **out_condition)
{
    FIXME("%p, %p, %d, %p: stub\n", iface, conds, conds_count, out_condition);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_iface_CreateOrCondition(IUIAutomation6 *iface, IUIAutomationCondition *cond1,
        IUIAutomationCondition *cond2, IUIAutomationCondition **out_condition)
{
    FIXME("%p, %p, %p, %p: stub\n", iface, cond1, cond2, out_condition);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_iface_CreateOrConditionFromArray(IUIAutomation6 *iface, SAFEARRAY *conds,
        IUIAutomationCondition **out_condition)
{
    FIXME("%p, %p, %p: stub\n", iface, conds, out_condition);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_iface_CreateOrConditionFromNativeArray(IUIAutomation6 *iface, IUIAutomationCondition **conds,
        int conds_count, IUIAutomationCondition **out_condition)
{
    FIXME("%p, %p, %d, %p: stub\n", iface, conds, conds_count, out_condition);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_iface_CreateNotCondition(IUIAutomation6 *iface, IUIAutomationCondition *cond,
        IUIAutomationCondition **out_condition)
{
    FIXME("%p, %p, %p: stub\n", iface, cond, out_condition);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_iface_AddAutomationEventHandler(IUIAutomation6 *iface, EVENTID event_id,
        IUIAutomationElement *elem, enum TreeScope scope, IUIAutomationCacheRequest *cache_req,
        IUIAutomationEventHandler *handler)
{
    FIXME("%p, %d, %p, %#x, %p, %p: stub\n", iface, event_id, elem, scope, cache_req, handler);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_iface_RemoveAutomationEventHandler(IUIAutomation6 *iface, EVENTID event_id,
        IUIAutomationElement *elem, IUIAutomationEventHandler *handler)
{
    FIXME("%p, %d, %p, %p: stub\n", iface, event_id, elem, handler);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_iface_AddPropertyChangedEventHandlerNativeArray(IUIAutomation6 *iface,
        IUIAutomationElement *elem, enum TreeScope scope, IUIAutomationCacheRequest *cache_req,
        IUIAutomationPropertyChangedEventHandler *handler, PROPERTYID *props, int props_count)
{
    FIXME("%p, %p, %#x, %p, %p, %p, %d: stub\n", iface, elem, scope, cache_req, handler, props, props_count);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_iface_AddPropertyChangedEventHandler(IUIAutomation6 *iface,
        IUIAutomationElement *elem, enum TreeScope scope, IUIAutomationCacheRequest *cache_req,
        IUIAutomationPropertyChangedEventHandler *handler, SAFEARRAY *props)
{
    FIXME("%p, %p, %#x, %p, %p, %p: stub\n", iface, elem, scope, cache_req, handler, props);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_iface_RemovePropertyChangedEventHandler(IUIAutomation6 *iface,
        IUIAutomationElement *elem, IUIAutomationPropertyChangedEventHandler *handler)
{
    FIXME("%p, %p, %p: stub\n", iface, elem, handler);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_iface_AddStructureChangedEventHandler(IUIAutomation6 *iface,
        IUIAutomationElement *elem, enum TreeScope scope, IUIAutomationCacheRequest *cache_req,
        IUIAutomationStructureChangedEventHandler *handler)
{
    FIXME("%p, %p, %#x, %p, %p: stub\n", iface, elem, scope, cache_req, handler);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_iface_RemoveStructureChangedEventHandler(IUIAutomation6 *iface,
        IUIAutomationElement *elem, IUIAutomationStructureChangedEventHandler *handler)
{
    FIXME("%p, %p, %p: stub\n", iface, elem, handler);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_iface_AddFocusChangedEventHandler(IUIAutomation6 *iface,
        IUIAutomationCacheRequest *cache_req, IUIAutomationFocusChangedEventHandler *handler)
{
    const struct uia_event_info *event_info = uia_event_info_from_id(UIA_AutomationFocusChangedEventId);
    IUIAutomationElement *element;
    HRESULT hr;

    TRACE("%p, %p, %p\n", iface, cache_req, handler);

    if (cache_req)
        FIXME("Cache req parameter currently ignored\n");

    hr = IUIAutomation6_ElementFromHandle(iface, GetDesktopWindow(), &element);
    if (FAILED(hr) || !element)
    {
        WARN("Failed to get desktop element, hr %#lx\n", hr);
        return hr;
    }

    return add_uia_com_event(event_info, UIA_COM_FOCUS_EVENT_TYPE, element, (IUnknown *)handler, TreeScope_SubTree, NULL,
            0, (struct UiaCacheRequest *)&DefaultCacheReq);
}

static HRESULT WINAPI uia_iface_RemoveFocusChangedEventHandler(IUIAutomation6 *iface,
        IUIAutomationFocusChangedEventHandler *handler)
{
    const struct uia_event_info *event_info = uia_event_info_from_id(UIA_AutomationFocusChangedEventId);
    struct uia_com_event *event;
    HRESULT hr;

    TRACE("%p, %p\n", iface, handler);

    hr = find_uia_com_event(event_info, UIA_COM_FOCUS_EVENT_TYPE, NULL, (IUnknown *)handler, &event);
    if (SUCCEEDED(hr) && event)
        remove_uia_com_event(event);

    return S_OK;
}

static HRESULT WINAPI uia_iface_RemoveAllEventHandlers(IUIAutomation6 *iface)
{
    FIXME("%p: stub\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_iface_IntNativeArrayToSafeArray(IUIAutomation6 *iface, int *arr, int arr_count,
        SAFEARRAY **out_sa)
{
    FIXME("%p, %p, %d, %p: stub\n", iface, arr, arr_count, out_sa);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_iface_IntSafeArrayToNativeArray(IUIAutomation6 *iface, SAFEARRAY *sa, int **out_arr,
        int *out_arr_count)
{
    FIXME("%p, %p, %p, %p: stub\n", iface, sa, out_arr, out_arr_count);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_iface_RectToVariant(IUIAutomation6 *iface, RECT rect, VARIANT *out_var)
{
    FIXME("%p, %s, %p: stub\n", iface, wine_dbgstr_rect(&rect), out_var);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_iface_VariantToRect(IUIAutomation6 *iface, VARIANT var, RECT *out_rect)
{
    FIXME("%p, %s, %p: stub\n", iface, debugstr_variant(&var), out_rect);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_iface_SafeArrayToRectNativeArray(IUIAutomation6 *iface, SAFEARRAY *sa, RECT **out_rect_arr,
        int *out_rect_arr_count)
{
    FIXME("%p, %p, %p, %p: stub\n", iface, sa, out_rect_arr, out_rect_arr_count);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_iface_CreateProxyFactoryEntry(IUIAutomation6 *iface, IUIAutomationProxyFactory *factory,
        IUIAutomationProxyFactoryEntry **out_entry)
{
    FIXME("%p, %p, %p: stub\n", iface, factory, out_entry);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_iface_get_ProxyFactoryMapping(IUIAutomation6 *iface,
        IUIAutomationProxyFactoryMapping **out_factory_map)
{
    FIXME("%p, %p: stub\n", iface, out_factory_map);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_iface_GetPropertyProgrammaticName(IUIAutomation6 *iface, PROPERTYID prop_id, BSTR *out_name)
{
    FIXME("%p, %d, %p: stub\n", iface, prop_id, out_name);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_iface_GetPatternProgrammaticName(IUIAutomation6 *iface, PATTERNID pattern_id, BSTR *out_name)
{
    FIXME("%p, %d, %p: stub\n", iface, pattern_id, out_name);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_iface_PollForPotentialSupportedPatterns(IUIAutomation6 *iface, IUIAutomationElement *elem,
        SAFEARRAY **out_pattern_ids, SAFEARRAY **out_pattern_names)
{
    FIXME("%p, %p, %p, %p: stub\n", iface, elem, out_pattern_ids, out_pattern_names);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_iface_PollForPotentialSupportedProperties(IUIAutomation6 *iface, IUIAutomationElement *elem,
        SAFEARRAY **out_prop_ids, SAFEARRAY **out_prop_names)
{
    FIXME("%p, %p, %p, %p: stub\n", iface, elem, out_prop_ids, out_prop_names);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_iface_CheckNotSupported(IUIAutomation6 *iface, VARIANT in_val, BOOL *match)
{
    FIXME("%p, %s, %p: stub\n", iface, debugstr_variant(&in_val), match);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_iface_get_ReservedNotSupportedValue(IUIAutomation6 *iface, IUnknown **out_unk)
{
    FIXME("%p, %p: stub\n", iface, out_unk);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_iface_get_ReservedMixedAttributeValue(IUIAutomation6 *iface, IUnknown **out_unk)
{
    FIXME("%p, %p: stub\n", iface, out_unk);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_iface_ElementFromIAccessible(IUIAutomation6 *iface, IAccessible *acc, int cid,
        IUIAutomationElement **out_elem)
{
    FIXME("%p, %p, %d, %p: stub\n", iface, acc, cid, out_elem);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_iface_ElementFromIAccessibleBuildCache(IUIAutomation6 *iface, IAccessible *acc, int cid,
        IUIAutomationCacheRequest *cache_req, IUIAutomationElement **out_elem)
{
    FIXME("%p, %p, %d, %p, %p: stub\n", iface, acc, cid, cache_req, out_elem);
    return E_NOTIMPL;
}

/* IUIAutomation2 methods */
static HRESULT WINAPI uia_iface_get_AutoSetFocus(IUIAutomation6 *iface, BOOL *out_auto_set_focus)
{
    FIXME("%p, %p: stub\n", iface, out_auto_set_focus);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_iface_put_AutoSetFocus(IUIAutomation6 *iface, BOOL auto_set_focus)
{
    FIXME("%p, %d: stub\n", iface, auto_set_focus);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_iface_get_ConnectionTimeout(IUIAutomation6 *iface, DWORD *out_timeout)
{
    FIXME("%p, %p: stub\n", iface, out_timeout);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_iface_put_ConnectionTimeout(IUIAutomation6 *iface, DWORD timeout)
{
    FIXME("%p, %ld: stub\n", iface, timeout);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_iface_get_TransactionTimeout(IUIAutomation6 *iface, DWORD *out_timeout)
{
    FIXME("%p, %p: stub\n", iface, out_timeout);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_iface_put_TransactionTimeout(IUIAutomation6 *iface, DWORD timeout)
{
    FIXME("%p, %ld: stub\n", iface, timeout);
    return E_NOTIMPL;
}

/* IUIAutomation3 methods */
static HRESULT WINAPI uia_iface_AddTextEditTextChangedEventHandler(IUIAutomation6 *iface, IUIAutomationElement *elem,
        enum TreeScope scope, enum TextEditChangeType change_type, IUIAutomationCacheRequest *cache_req,
        IUIAutomationTextEditTextChangedEventHandler *handler)
{
    FIXME("%p, %p, %#x, %d, %p, %p: stub\n", iface, elem, scope, change_type, cache_req, handler);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_iface_RemoveTextEditTextChangedEventHandler(IUIAutomation6 *iface, IUIAutomationElement *elem,
        IUIAutomationTextEditTextChangedEventHandler *handler)
{
    FIXME("%p, %p, %p: stub\n", iface, elem, handler);
    return E_NOTIMPL;
}

/* IUIAutomation4 methods */
static HRESULT WINAPI uia_iface_AddChangesEventHandler(IUIAutomation6 *iface, IUIAutomationElement *elem,
        enum TreeScope scope, int *change_types, int change_types_count, IUIAutomationCacheRequest *cache_req,
        IUIAutomationChangesEventHandler *handler)
{
    FIXME("%p, %p, %#x, %p, %d, %p, %p: stub\n", iface, elem, scope, change_types, change_types_count, cache_req,
            handler);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_iface_RemoveChangesEventHandler(IUIAutomation6 *iface, IUIAutomationElement *elem,
        IUIAutomationChangesEventHandler *handler)
{
    FIXME("%p, %p, %p: stub\n", iface, elem, handler);
    return E_NOTIMPL;
}

/* IUIAutomation5 methods */
static HRESULT WINAPI uia_iface_AddNotificationEventHandler(IUIAutomation6 *iface, IUIAutomationElement *elem,
        enum TreeScope scope, IUIAutomationCacheRequest *cache_req, IUIAutomationNotificationEventHandler *handler)
{
    FIXME("%p, %p, %#x, %p, %p: stub\n", iface, elem, scope, cache_req, handler);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_iface_RemoveNotificationEventHandler(IUIAutomation6 *iface, IUIAutomationElement *elem,
        IUIAutomationNotificationEventHandler *handler)
{
    FIXME("%p, %p, %p: stub\n", iface, elem, handler);
    return E_NOTIMPL;
}

/* IUIAutomation6 methods */
static HRESULT WINAPI uia_iface_CreateEventHandlerGroup(IUIAutomation6 *iface,
        IUIAutomationEventHandlerGroup **out_handler_group)
{
    FIXME("%p, %p: stub\n", iface, out_handler_group);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_iface_AddEventHandlerGroup(IUIAutomation6 *iface, IUIAutomationElement *elem,
        IUIAutomationEventHandlerGroup *handler_group)
{
    FIXME("%p, %p, %p: stub\n", iface, elem, handler_group);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_iface_RemoveEventHandlerGroup(IUIAutomation6 *iface, IUIAutomationElement *elem,
        IUIAutomationEventHandlerGroup *handler_group)
{
    FIXME("%p, %p, %p: stub\n", iface, elem, handler_group);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_iface_get_ConnectionRecoveryBehavior(IUIAutomation6 *iface,
        enum ConnectionRecoveryBehaviorOptions *out_conn_recovery_opts)
{
    FIXME("%p, %p: stub\n", iface, out_conn_recovery_opts);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_iface_put_ConnectionRecoveryBehavior(IUIAutomation6 *iface,
        enum ConnectionRecoveryBehaviorOptions conn_recovery_opts)
{
    FIXME("%p, %#x: stub\n", iface, conn_recovery_opts);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_iface_get_CoalesceEvents(IUIAutomation6 *iface,
        enum CoalesceEventsOptions *out_coalesce_events_opts)
{
    FIXME("%p, %p: stub\n", iface, out_coalesce_events_opts);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_iface_put_CoalesceEvents(IUIAutomation6 *iface,
        enum CoalesceEventsOptions coalesce_events_opts)
{
    FIXME("%p, %#x: stub\n", iface, coalesce_events_opts);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_iface_AddActiveTextPositionChangedEventHandler(IUIAutomation6 *iface,
        IUIAutomationElement *elem, enum TreeScope scope, IUIAutomationCacheRequest *cache_req,
        IUIAutomationActiveTextPositionChangedEventHandler *handler)
{
    FIXME("%p, %p, %#x, %p, %p: stub\n", iface, elem, scope, cache_req, handler);
    return E_NOTIMPL;
}

static HRESULT WINAPI uia_iface_RemoveActiveTextPositionChangedEventHandler(IUIAutomation6 *iface,
        IUIAutomationElement *elem, IUIAutomationActiveTextPositionChangedEventHandler *handler)
{
    FIXME("%p, %p, %p\n", iface, elem, handler);
    return E_NOTIMPL;
}

static const IUIAutomation6Vtbl uia_iface_vtbl = {
    uia_iface_QueryInterface,
    uia_iface_AddRef,
    uia_iface_Release,
    /* IUIAutomation methods */
    uia_iface_CompareElements,
    uia_iface_CompareRuntimeIds,
    uia_iface_GetRootElement,
    uia_iface_ElementFromHandle,
    uia_iface_ElementFromPoint,
    uia_iface_GetFocusedElement,
    uia_iface_GetRootElementBuildCache,
    uia_iface_ElementFromHandleBuildCache,
    uia_iface_ElementFromPointBuildCache,
    uia_iface_GetFocusedElementBuildCache,
    uia_iface_CreateTreeWalker,
    uia_iface_get_ControlViewWalker,
    uia_iface_get_ContentViewWalker,
    uia_iface_get_RawViewWalker,
    uia_iface_get_RawViewCondition,
    uia_iface_get_ControlViewCondition,
    uia_iface_get_ContentViewCondition,
    uia_iface_CreateCacheRequest,
    uia_iface_CreateTrueCondition,
    uia_iface_CreateFalseCondition,
    uia_iface_CreatePropertyCondition,
    uia_iface_CreatePropertyConditionEx,
    uia_iface_CreateAndCondition,
    uia_iface_CreateAndConditionFromArray,
    uia_iface_CreateAndConditionFromNativeArray,
    uia_iface_CreateOrCondition,
    uia_iface_CreateOrConditionFromArray,
    uia_iface_CreateOrConditionFromNativeArray,
    uia_iface_CreateNotCondition,
    uia_iface_AddAutomationEventHandler,
    uia_iface_RemoveAutomationEventHandler,
    uia_iface_AddPropertyChangedEventHandlerNativeArray,
    uia_iface_AddPropertyChangedEventHandler,
    uia_iface_RemovePropertyChangedEventHandler,
    uia_iface_AddStructureChangedEventHandler,
    uia_iface_RemoveStructureChangedEventHandler,
    uia_iface_AddFocusChangedEventHandler,
    uia_iface_RemoveFocusChangedEventHandler,
    uia_iface_RemoveAllEventHandlers,
    uia_iface_IntNativeArrayToSafeArray,
    uia_iface_IntSafeArrayToNativeArray,
    uia_iface_RectToVariant,
    uia_iface_VariantToRect,
    uia_iface_SafeArrayToRectNativeArray,
    uia_iface_CreateProxyFactoryEntry,
    uia_iface_get_ProxyFactoryMapping,
    uia_iface_GetPropertyProgrammaticName,
    uia_iface_GetPatternProgrammaticName,
    uia_iface_PollForPotentialSupportedPatterns,
    uia_iface_PollForPotentialSupportedProperties,
    uia_iface_CheckNotSupported,
    uia_iface_get_ReservedNotSupportedValue,
    uia_iface_get_ReservedMixedAttributeValue,
    uia_iface_ElementFromIAccessible,
    uia_iface_ElementFromIAccessibleBuildCache,
    /* IUIAutomation2 methods */
    uia_iface_get_AutoSetFocus,
    uia_iface_put_AutoSetFocus,
    uia_iface_get_ConnectionTimeout,
    uia_iface_put_ConnectionTimeout,
    uia_iface_get_TransactionTimeout,
    uia_iface_put_TransactionTimeout,
    /* IUIAutomation3 methods */
    uia_iface_AddTextEditTextChangedEventHandler,
    uia_iface_RemoveTextEditTextChangedEventHandler,
    /* IUIAutomation4 methods */
    uia_iface_AddChangesEventHandler,
    uia_iface_RemoveChangesEventHandler,
    /* IUIAutomation5 methods */
    uia_iface_AddNotificationEventHandler,
    uia_iface_RemoveNotificationEventHandler,
    /* IUIAutomation6 methods */
    uia_iface_CreateEventHandlerGroup,
    uia_iface_AddEventHandlerGroup,
    uia_iface_RemoveEventHandlerGroup,
    uia_iface_get_ConnectionRecoveryBehavior,
    uia_iface_put_ConnectionRecoveryBehavior,
    uia_iface_get_CoalesceEvents,
    uia_iface_put_CoalesceEvents,
    uia_iface_AddActiveTextPositionChangedEventHandler,
    uia_iface_RemoveActiveTextPositionChangedEventHandler,
};

HRESULT create_uia_iface(IUnknown **iface, BOOL is_cui8)
{
    struct uia_iface *uia;

    uia = heap_alloc_zero(sizeof(*uia));
    if (!uia)
        return E_OUTOFMEMORY;

    uia->IUIAutomation6_iface.lpVtbl = &uia_iface_vtbl;
    uia->is_cui8 = is_cui8;
    uia->ref = 1;

    *iface = (IUnknown *)&uia->IUIAutomation6_iface;
    return S_OK;
}
