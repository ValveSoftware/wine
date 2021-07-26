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

static void uia_evh_add_event_handler(struct uia_data *data, struct uia_evh *evh)
{
    list_add_tail(&data->uia_evh_list, &evh->entry);
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
    struct list *evh_list = &data->uia_evh_list;
    struct list *cursor, *cursor2;
    struct uia_evh *evh;

    LIST_FOR_EACH_SAFE(cursor, cursor2, evh_list)
    {
        evh = LIST_ENTRY(cursor, struct uia_evh, entry);
        if (evh->event_type == FOCUS_EVH
                && evh->u.IUIAutomationFocusChangedEvh_iface == handler)
        {
            list_remove(cursor);
            IUIAutomationFocusChangedEventHandler_Release(handler);
            heap_free(evh);
            return S_OK;
        }
    }

    return S_OK;
}
