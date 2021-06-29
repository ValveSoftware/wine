/*
 * Copyright 2021 Connor McAdams for CodeWeavers
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

#include "uiautomation.h"
#include "oleacc.h"

#include "wine/list.h"

/*
 * EVH = Event Handler.
 */
enum {
    BASIC_EVH,
    CHANGES_EVH,
    FOCUS_EVH,
    PROPERTY_EVH,
    STRUCTURE_EVH,
    TEXT_EDIT_EVH,
};

struct uia_evh
{
    struct list entry;

    UINT event_type;
    union
    {
        IUIAutomationEventHandler                    *IUIAutomationEvh_iface;
        IUIAutomationChangesEventHandler             *IUIAutomationChangesEvh_iface;
        IUIAutomationFocusChangedEventHandler        *IUIAutomationFocusChangedEvh_iface;
        IUIAutomationPropertyChangedEventHandler     *IUIAutomationPropertyChangedEvh_iface;
        IUIAutomationStructureChangedEventHandler    *IUIAutomationStructureChangedEvh_iface;
        IUIAutomationTextEditTextChangedEventHandler *IUIAutomationTextEditTextChangedEvh_iface;
    } u;
};

/*
 * EVM = Event message. Result of an event being raised, either with
 * UiaRaiseAutomationEvent or an MSAA event with NotifyWinEvent.
 */
struct uia_evm
{
    struct list entry;

    UINT event;
    IUIAutomationElement *elem;

    enum uia_event_origin
    {
        UIA_EVO_MSAA,
        UIA_EVO_UIA,
    } uia_evo;

    union
    {
        struct
        {
            HWND hwnd;

            LONG obj_id, child_id;
        } msaa_ev;
        struct
        {
            IRawElementProviderSimple *elem_prov;
        } uia_ev;
    } u;
};

struct uia_data;

/*
 * EVL = Event listener.
 */
struct uia_evl
{
    HANDLE h_thread, pump_initialized, first_event;
    CRITICAL_SECTION ev_handler_cs;
    CRITICAL_SECTION evm_queue_cs;
    UINT tid;

    HWINEVENTHOOK win_creation_hook;
    HWINEVENTHOOK object_focus_hook;

    struct uia_data *data;

    struct list uia_evh_list;
    struct list uia_evm_queue;
};

struct uia_data {
    IUIAutomation IUIAutomation_iface;
    LONG ref;

    struct uia_evl *evl;
};

HRESULT create_uia_iface(IUIAutomation **) DECLSPEC_HIDDEN;

HRESULT create_uia_elem_from_raw_provider(IUIAutomationElement **,
        IRawElementProviderSimple *) DECLSPEC_HIDDEN;
HRESULT create_uia_elem_from_msaa_acc(IUIAutomationElement **,
        IAccessible *, INT) DECLSPEC_HIDDEN;

/*
 * uia_event.c functions.
 */
HRESULT uia_evh_add_focus_event_handler(struct uia_data *data,
        IUIAutomationFocusChangedEventHandler *handler) DECLSPEC_HIDDEN;
HRESULT uia_evh_remove_focus_event_handler(struct uia_data *data,
        IUIAutomationFocusChangedEventHandler *handler) DECLSPEC_HIDDEN;
HRESULT uia_evh_remove_all_event_handlers(struct uia_data *data) DECLSPEC_HIDDEN;
