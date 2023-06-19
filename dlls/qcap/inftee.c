/*
 * Infinite pin tee filter
 *
 * Copyright 2023 Zeb Figura
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

#include "qcap_private.h"
#include <wine/list.h>

WINE_DEFAULT_DEBUG_CHANNEL(quartz);

struct inf_tee_source
{
    struct strmbase_source source;
    struct list entry;
};

struct inf_tee
{
    struct strmbase_filter filter;

    struct strmbase_sink sink;

    struct list sources;
    unsigned int source_index;
};

static struct inf_tee *impl_from_strmbase_filter(struct strmbase_filter *iface)
{
    return CONTAINING_RECORD(iface, struct inf_tee, filter);
}

static struct inf_tee *impl_from_strmbase_pin(struct strmbase_pin *pin)
{
    return impl_from_strmbase_filter(pin->filter);
}

static void remove_source(struct inf_tee_source *source)
{
    strmbase_source_cleanup(&source->source);
    list_remove(&source->entry);
    free(source);
}

static HRESULT inf_tee_source_get_media_type(struct strmbase_pin *iface,
        unsigned int index, AM_MEDIA_TYPE *mt)
{
    struct inf_tee *filter = impl_from_strmbase_pin(iface);
    IEnumMediaTypes *enummt;
    AM_MEDIA_TYPE *pmt;
    HRESULT hr;

    if (!filter->sink.pin.peer)
        return VFW_E_NOT_CONNECTED;

    if (FAILED(hr = IPin_EnumMediaTypes(filter->sink.pin.peer, &enummt)))
        return hr;

    if ((!index || IEnumMediaTypes_Skip(enummt, index) == S_OK)
            && IEnumMediaTypes_Next(enummt, 1, &pmt, NULL) == S_OK)
    {
        CopyMediaType(mt, pmt);
        DeleteMediaType(pmt);
        IEnumMediaTypes_Release(enummt);
        return S_OK;
    }

    IEnumMediaTypes_Release(enummt);
    return VFW_S_NO_MORE_ITEMS;
}

static HRESULT add_source(struct inf_tee *filter);

static HRESULT WINAPI inf_tee_source_DecideBufferSize(struct strmbase_source *iface,
        IMemAllocator *allocator, ALLOCATOR_PROPERTIES *props)
{
    struct inf_tee *filter = impl_from_strmbase_pin(&iface->pin);
    ALLOCATOR_PROPERTIES ret_props;
    HRESULT hr;

    if (!filter->sink.pin.peer)
    {
        WARN("Sink is not connected; returning VFW_E_NOT_CONNECTED.\n");
        return VFW_E_NOT_CONNECTED;
    }

    /* Copy the properties from the upstream sink. */
    if (FAILED(hr = IMemAllocator_GetProperties(filter->sink.pAllocator, props)))
        return hr;

    if (FAILED(hr = IMemAllocator_SetProperties(allocator, props, &ret_props)))
        return hr;

    if (FAILED(hr = add_source(filter)))
        return hr;

    return S_OK;
}

static void inf_tee_source_disconnect(struct strmbase_source *iface)
{
//    struct inf_tee_source *source = impl_source_from_strmbase_pin(&iface->pin);

//    remove_source(source);
}

static const struct strmbase_source_ops source_ops =
{
    .base.pin_get_media_type = inf_tee_source_get_media_type,
    .pfnAttemptConnection = BaseOutputPinImpl_AttemptConnection,
    .pfnDecideAllocator = BaseOutputPinImpl_DecideAllocator,
    .pfnDecideBufferSize = inf_tee_source_DecideBufferSize,
    .source_disconnect = inf_tee_source_disconnect,
};

static HRESULT add_source(struct inf_tee *filter)
{
    struct inf_tee_source *source;
    WCHAR name[19];

    if (!(source = calloc(1, sizeof(*source))))
        return E_OUTOFMEMORY;

    swprintf(name, ARRAY_SIZE(name), L"Output%u", filter->source_index++);
    strmbase_source_init(&source->source, &filter->filter, name, &source_ops);
    list_add_tail(&filter->sources, &source->entry);
    return S_OK;
}

static HRESULT inf_tee_sink_query_interface(struct strmbase_pin *iface, REFIID iid, void **out)
{
    struct inf_tee *filter = impl_from_strmbase_pin(iface);

    if (IsEqualGUID(iid, &IID_IMemInputPin))
        *out = &filter->sink.IMemInputPin_iface;
    else
        return E_NOINTERFACE;

    IUnknown_AddRef((IUnknown *)*out);
    return S_OK;
}

static HRESULT WINAPI inf_tee_sink_Receive(struct strmbase_sink *iface, IMediaSample *sample)
{
    struct inf_tee *filter = impl_from_strmbase_pin(&iface->pin);
    struct inf_tee_source *source;

    LIST_FOR_EACH_ENTRY(source, &filter->sources, struct inf_tee_source, entry)
    {
        HRESULT hr;

        if (!source->source.pin.peer)
            continue;

        if (FAILED(hr = IMemInputPin_Receive(source->source.pMemInputPin, sample)))
            WARN("Receive() returned %#lx.\n", hr);
    }

    return S_OK;
}

static const struct strmbase_sink_ops sink_ops =
{
    .base.pin_query_interface = inf_tee_sink_query_interface,
    .pfnReceive = inf_tee_sink_Receive,
};

static struct strmbase_pin *inf_tee_get_pin(struct strmbase_filter *iface, unsigned int index)
{
    struct inf_tee *filter = impl_from_strmbase_filter(iface);
    struct inf_tee_source *source;

    if (index == 0)
        return &filter->sink.pin;
    LIST_FOR_EACH_ENTRY(source, &filter->sources, struct inf_tee_source, entry)
    {
        if (!--index)
            return &source->source.pin;
    }
    return NULL;
}

static void inf_tee_destroy(struct strmbase_filter *iface)
{
    struct inf_tee *filter = impl_from_strmbase_filter(iface);
    struct inf_tee_source *source, *cursor;

    LIST_FOR_EACH_ENTRY_SAFE(source, cursor, &filter->sources, struct inf_tee_source, entry)
        remove_source(source);

    strmbase_sink_cleanup(&filter->sink);
    strmbase_filter_cleanup(&filter->filter);
    free(filter);
}

static const struct strmbase_filter_ops filter_ops =
{
    .filter_get_pin = inf_tee_get_pin,
    .filter_destroy = inf_tee_destroy,
};

HRESULT inf_tee_create(IUnknown *outer, IUnknown **out)
{
    struct inf_tee *object;
    HRESULT hr;

    if (!(object = calloc(1, sizeof(*object))))
        return E_OUTOFMEMORY;

    strmbase_filter_init(&object->filter, outer, &CLSID_InfTee, &filter_ops);

    strmbase_sink_init(&object->sink, &object->filter, L"Input", &sink_ops, NULL);

    list_init(&object->sources);
    object->source_index = 1;

    if (FAILED(hr = add_source(object)))
    {
        inf_tee_destroy(&object->filter);
        return hr;
    }

    TRACE("Created infinite tee %p.\n", object);
    *out = &object->filter.IUnknown_inner;
    return S_OK;
}
