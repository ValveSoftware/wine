/*
 * Infinite pin tee filter unit tests
 *
 * Copyright 2023 Zeb Figura for CodeWeavers
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
#include <stdbool.h>
#include "dshow.h"
#include "mmreg.h"
#include "wine/strmbase.h"
#include "wine/test.h"

static IBaseFilter *create_infinite_tee(void)
{
    IBaseFilter *filter = NULL;
    HRESULT hr = CoCreateInstance(&CLSID_InfTee, NULL, CLSCTX_INPROC_SERVER,
            &IID_IBaseFilter, (void **)&filter);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    return filter;
}

static ULONG get_refcount(void *iface)
{
    IUnknown *unknown = iface;
    IUnknown_AddRef(unknown);
    return IUnknown_Release(unknown);
}

static bool compare_media_types(const AM_MEDIA_TYPE *a, const AM_MEDIA_TYPE *b)
{
    return !memcmp(a, b, offsetof(AM_MEDIA_TYPE, pbFormat))
            && !memcmp(a->pbFormat, b->pbFormat, a->cbFormat);
}

struct testfilter
{
    struct strmbase_filter filter;
    struct strmbase_source source;
    struct strmbase_sink sink;
    const AM_MEDIA_TYPE *sink_mt;
    AM_MEDIA_TYPE source_mt;
    bool require_temporal_compression;
    HANDLE sample_event, eos_event, segment_event;
    unsigned int got_begin_flush, got_end_flush;
    unsigned int got_connect;
};

static inline struct testfilter *impl_from_strmbase_filter(struct strmbase_filter *iface)
{
    return CONTAINING_RECORD(iface, struct testfilter, filter);
}

static struct strmbase_pin *testfilter_get_pin(struct strmbase_filter *iface, unsigned int index)
{
    struct testfilter *filter = impl_from_strmbase_filter(iface);
    if (!index)
        return &filter->source.pin;
    else if (index == 1)
        return &filter->sink.pin;
    return NULL;
}

static void testfilter_destroy(struct strmbase_filter *iface)
{
    struct testfilter *filter = impl_from_strmbase_filter(iface);
    CloseHandle(filter->sample_event);
    CloseHandle(filter->eos_event);
    CloseHandle(filter->segment_event);
    strmbase_source_cleanup(&filter->source);
    strmbase_sink_cleanup(&filter->sink);
    strmbase_filter_cleanup(&filter->filter);
}

static const struct strmbase_filter_ops testfilter_ops =
{
    .filter_get_pin = testfilter_get_pin,
    .filter_destroy = testfilter_destroy,
};

static HRESULT testsource_query_accept(struct strmbase_pin *iface, const AM_MEDIA_TYPE *mt)
{
    struct testfilter *filter = impl_from_strmbase_filter(iface->filter);

    if (filter->require_temporal_compression && !mt->bTemporalCompression)
        return S_FALSE;
    return S_OK;
}

static HRESULT testsource_get_media_type(struct strmbase_pin *iface, unsigned int index, AM_MEDIA_TYPE *mt)
{
    struct testfilter *filter = impl_from_strmbase_filter(iface->filter);
    if (!index)
    {
        CopyMediaType(mt, &filter->source_mt);
        return S_OK;
    }
    return VFW_S_NO_MORE_ITEMS;
}

static void test_sink_allocator(struct strmbase_source *source)
{
    ALLOCATOR_PROPERTIES req_props = {1, 256, 1, 0}, ret_props;
    IMemAllocator *req_allocator, *ret_allocator;
    IMemInputPin *input;
    HRESULT hr;

    IPin_QueryInterface(source->pin.peer, &IID_IMemInputPin, (void **)&input);

    hr = IMemInputPin_GetAllocatorRequirements(input, &ret_props);
    ok(hr == E_NOTIMPL, "Got hr %#lx.\n", hr);

    hr = IMemInputPin_GetAllocator(input, &ret_allocator);
    todo_wine ok(hr == S_OK, "Got hr %#lx.\n", hr);

    if (hr == S_OK)
    {
        hr = IMemInputPin_NotifyAllocator(input, ret_allocator, TRUE);
        ok(hr == S_OK, "Got hr %#lx.\n", hr);
        IMemAllocator_Release(ret_allocator);
    }

    CoCreateInstance(&CLSID_MemoryAllocator, NULL, CLSCTX_INPROC_SERVER,
            &IID_IMemAllocator, (void **)&req_allocator);

    hr = IMemInputPin_NotifyAllocator(input, req_allocator, TRUE);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);

    hr = IMemInputPin_GetAllocator(input, &ret_allocator);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(ret_allocator == req_allocator, "Allocators didn't match.\n");
    IMemAllocator_Release(ret_allocator);

    hr = IMemAllocator_SetProperties(req_allocator, &req_props, &ret_props);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);

    source->pAllocator = req_allocator;

    IMemInputPin_Release(input);
}

static HRESULT WINAPI testsource_AttemptConnection(struct strmbase_source *iface,
        IPin *peer, const AM_MEDIA_TYPE *mt)
{
    struct testfilter *filter = impl_from_strmbase_filter(iface->pin.filter);
    HRESULT hr;

    ++filter->got_connect;

    iface->pin.peer = peer;
    IPin_AddRef(peer);
    CopyMediaType(&iface->pin.mt, mt);

    if (FAILED(hr = IPin_ReceiveConnection(peer, &iface->pin.IPin_iface, mt)))
    {
        ok(hr == VFW_E_TYPE_NOT_ACCEPTED, "Got hr %#lx.\n", hr);
        IPin_Release(peer);
        iface->pin.peer = NULL;
        FreeMediaType(&iface->pin.mt);
    }

    test_sink_allocator(iface);

    return hr;
}

static const struct strmbase_source_ops testsource_ops =
{
    .base.pin_query_accept = testsource_query_accept,
    .base.pin_get_media_type = testsource_get_media_type,
    .pfnAttemptConnection = testsource_AttemptConnection,
};

static HRESULT testsink_query_interface(struct strmbase_pin *iface, REFIID iid, void **out)
{
    struct testfilter *filter = impl_from_strmbase_filter(iface->filter);

    if (IsEqualGUID(iid, &IID_IMemInputPin))
        *out = &filter->sink.IMemInputPin_iface;
    else
        return E_NOINTERFACE;

    IUnknown_AddRef((IUnknown *)*out);
    return S_OK;
}

static HRESULT testsink_get_media_type(struct strmbase_pin *iface, unsigned int index, AM_MEDIA_TYPE *mt)
{
    struct testfilter *filter = impl_from_strmbase_filter(iface->filter);
    if (!index && filter->sink_mt)
    {
        CopyMediaType(mt, filter->sink_mt);
        return S_OK;
    }
    return VFW_S_NO_MORE_ITEMS;
}

static HRESULT testsink_connect(struct strmbase_sink *iface, IPin *peer, const AM_MEDIA_TYPE *mt)
{
    struct testfilter *filter = impl_from_strmbase_filter(iface->pin.filter);
    if (filter->sink_mt && !IsEqualGUID(&mt->majortype, &filter->sink_mt->majortype))
        return VFW_E_TYPE_NOT_ACCEPTED;
    return S_OK;
}

static HRESULT WINAPI testsink_Receive(struct strmbase_sink *iface, IMediaSample *sample)
{
    struct testfilter *filter = impl_from_strmbase_filter(iface->pin.filter);
    REFERENCE_TIME start, stop;
    BYTE *data, expect[200];
    LONG size, i;
    HRESULT hr;

    size = IMediaSample_GetSize(sample);
    ok(size == 256, "Got size %lu.\n", size);
    size = IMediaSample_GetActualDataLength(sample);
    ok(size == 200, "Got valid size %lu.\n", size);

    hr = IMediaSample_GetPointer(sample, &data);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    for (i = 0; i < size; ++i)
        expect[i] = i;
    ok(!memcmp(data, expect, size), "Data didn't match.\n");

    hr = IMediaSample_GetTime(sample, &start, &stop);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(start == 30000, "Got start time %s.\n", wine_dbgstr_longlong(start));
    ok(stop == 40000, "Got stop time %s.\n", wine_dbgstr_longlong(stop));

    hr = IMediaSample_GetMediaTime(sample, &start, &stop);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(start == 10000, "Got start time %s.\n", wine_dbgstr_longlong(start));
    ok(stop == 20000, "Got stop time %s.\n", wine_dbgstr_longlong(stop));

    hr = IMediaSample_IsDiscontinuity(sample);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    hr = IMediaSample_IsPreroll(sample);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    hr = IMediaSample_IsSyncPoint(sample);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);

    SetEvent(filter->sample_event);

    return S_OK;
}

static HRESULT testsink_new_segment(struct strmbase_sink *iface,
        REFERENCE_TIME start, REFERENCE_TIME stop, double rate)
{
    struct testfilter *filter = impl_from_strmbase_filter(iface->pin.filter);
    ok(start == 10000, "Got start %s.\n", wine_dbgstr_longlong(start));
    ok(stop == 20000, "Got stop %s.\n", wine_dbgstr_longlong(stop));
    ok(rate == 1.0, "Got rate %.16e.\n", rate);
    SetEvent(filter->segment_event);
    return S_OK;
}

static HRESULT testsink_eos(struct strmbase_sink *iface)
{
    struct testfilter *filter = impl_from_strmbase_filter(iface->pin.filter);
    SetEvent(filter->eos_event);
    return S_OK;
}

static HRESULT testsink_begin_flush(struct strmbase_sink *iface)
{
    struct testfilter *filter = impl_from_strmbase_filter(iface->pin.filter);
    ++filter->got_begin_flush;
    return S_OK;
}

static HRESULT testsink_end_flush(struct strmbase_sink *iface)
{
    struct testfilter *filter = impl_from_strmbase_filter(iface->pin.filter);
    ++filter->got_end_flush;
    return S_OK;
}

static const struct strmbase_sink_ops testsink_ops =
{
    .base.pin_query_interface = testsink_query_interface,
    .base.pin_get_media_type = testsink_get_media_type,
    .sink_connect = testsink_connect,
    .pfnReceive = testsink_Receive,
    .sink_new_segment = testsink_new_segment,
    .sink_eos = testsink_eos,
    .sink_begin_flush = testsink_begin_flush,
    .sink_end_flush = testsink_end_flush,
};

static void testfilter_init(struct testfilter *filter)
{
    static const GUID clsid = {0xabacab};
    memset(filter, 0, sizeof(*filter));
    strmbase_filter_init(&filter->filter, NULL, &clsid, &testfilter_ops);
    strmbase_source_init(&filter->source, &filter->filter, L"source", &testsource_ops);
    strmbase_sink_init(&filter->sink, &filter->filter, L"sink", &testsink_ops, NULL);
    filter->sample_event = CreateEventW(NULL, FALSE, FALSE, NULL);
    filter->segment_event = CreateEventW(NULL, FALSE, FALSE, NULL);
    filter->eos_event = CreateEventW(NULL, FALSE, FALSE, NULL);
    filter->require_temporal_compression = true;
}

#define check_interface(a, b, c) check_interface_(__LINE__, a, b, c)
static void check_interface_(unsigned int line, void *iface_ptr, REFIID iid, BOOL supported)
{
    IUnknown *iface = iface_ptr;
    HRESULT hr, expected_hr;
    ULONG ref, expect_ref;
    IUnknown *unk;

    expected_hr = supported ? S_OK : E_NOINTERFACE;

    expect_ref = get_refcount(iface);

    hr = IUnknown_QueryInterface(iface, iid, (void **)&unk);
    ok_(__FILE__, line)(hr == expected_hr, "Got hr %#lx, expected %#lx.\n", hr, expected_hr);
    if (SUCCEEDED(hr))
    {
        ref = get_refcount(iface);
        ok_(__FILE__, line)(ref == expect_ref + 1, "Expected %lu references, got %lu.\n", expect_ref + 1, ref);
        ref = get_refcount(unk);
        ok_(__FILE__, line)(ref == expect_ref + 1, "Expected %lu references, got %lu.\n", expect_ref + 1, ref);
        IUnknown_Release(unk);
    }
}

static void test_interfaces(void)
{
    IBaseFilter *filter = create_infinite_tee();
    ULONG ref;
    IPin *pin;

    check_interface(filter, &IID_IBaseFilter, TRUE);
    check_interface(filter, &IID_IMediaFilter, TRUE);
    check_interface(filter, &IID_IPersist, TRUE);
    check_interface(filter, &IID_IUnknown, TRUE);

    check_interface(filter, &IID_IAMFilterMiscFlags, FALSE);
    check_interface(filter, &IID_IBasicAudio, FALSE);
    check_interface(filter, &IID_IBasicVideo, FALSE);
    check_interface(filter, &IID_IKsPropertySet, FALSE);
    check_interface(filter, &IID_IMediaPosition, FALSE);
    check_interface(filter, &IID_IMediaSeeking, FALSE);
    check_interface(filter, &IID_IMemInputPin, FALSE);
    check_interface(filter, &IID_IPersistPropertyBag, FALSE);
    check_interface(filter, &IID_IPin, FALSE);
    check_interface(filter, &IID_IQualityControl, FALSE);
    check_interface(filter, &IID_IQualProp, FALSE);
    check_interface(filter, &IID_IReferenceClock, FALSE);
    check_interface(filter, &IID_ISeekingPassThru, FALSE);
    check_interface(filter, &IID_IVideoWindow, FALSE);

    IBaseFilter_FindPin(filter, L"Input", &pin);

    check_interface(pin, &IID_IMemInputPin, TRUE);
    check_interface(pin, &IID_IPin, TRUE);
    todo_wine check_interface(pin, &IID_IQualityControl, TRUE);
    check_interface(pin, &IID_IUnknown, TRUE);

    check_interface(pin, &IID_IKsPropertySet, FALSE);
    check_interface(pin, &IID_IMediaPosition, FALSE);
    check_interface(pin, &IID_IMediaSeeking, FALSE);

    IPin_Release(pin);

    IBaseFilter_FindPin(filter, L"Output1", &pin);

    todo_wine check_interface(pin, &IID_IMediaPosition, TRUE);
    todo_wine check_interface(pin, &IID_IMediaSeeking, TRUE);
    check_interface(pin, &IID_IPin, TRUE);
    todo_wine check_interface(pin, &IID_IQualityControl, TRUE);
    check_interface(pin, &IID_IUnknown, TRUE);

    check_interface(pin, &IID_IAsyncReader, FALSE);
    check_interface(pin, &IID_IKsPropertySet, FALSE);
    check_interface(pin, &IID_IMemInputPin, FALSE);

    IPin_Release(pin);

    ref = IBaseFilter_Release(filter);
    ok(!ref, "Got unexpected refcount %ld.\n", ref);
}

static void testsink_add(struct testfilter *testsink, IBaseFilter *filter, IFilterGraph2 *graph, const WCHAR *id)
{
    AM_MEDIA_TYPE req_mt =
    {
        .majortype = MEDIATYPE_Stream,
        .subtype = MEDIASUBTYPE_Avi,
        .formattype = FORMAT_None,
        .bTemporalCompression = TRUE,
    };
    IPin *source;
    HRESULT hr;

    testfilter_init(testsink);
    IFilterGraph2_AddFilter(graph, &testsink->filter.IBaseFilter_iface, L"sink");
    hr = IBaseFilter_FindPin(filter, id, &source);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    hr = IFilterGraph2_ConnectDirect(graph, source, &testsink->sink.pin.IPin_iface, &req_mt);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    IPin_Release(source);
}

static void testsink_remove(struct testfilter *testsink, IBaseFilter *filter, IFilterGraph2 *graph)
{
    IPin *source = testsink->sink.pin.peer;
    HRESULT hr;
    ULONG ref;

    // fixme zf: this is ugly!
    hr = IFilterGraph2_Disconnect(graph, &testsink->sink.pin.IPin_iface);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    hr = IFilterGraph2_Disconnect(graph, source);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    IFilterGraph2_RemoveFilter(graph, &testsink->filter.IBaseFilter_iface);
    ref = IBaseFilter_Release(&testsink->filter.IBaseFilter_iface);
    ok(!ref, "Got outstanding refcount %ld.\n", ref);
}

#define check_pin_info(a, b, c, d) check_pin_info_(__LINE__, a, b, c, d)
static void check_pin_info_(int line, IBaseFilter *filter, IPin *pin, const WCHAR *expect_id, PIN_DIRECTION expect_dir)
{
    PIN_DIRECTION dir;
    PIN_INFO info;
    ULONG count;
    HRESULT hr;
    IPin *pin2;
    WCHAR *id;

    hr = IPin_QueryPinInfo(pin, &info);
    ok_(__FILE__, line)(hr == S_OK, "Got hr %#lx.\n", hr);
    ok_(__FILE__, line)(info.pFilter == filter, "Expected filter %p, got %p.\n", filter, info.pFilter);
    ok_(__FILE__, line)(info.dir == expect_dir, "Got direction %d.\n", info.dir);
    ok_(__FILE__, line)(!wcscmp(info.achName, expect_id), "Got name %s.\n", debugstr_w(info.achName));
    IBaseFilter_Release(info.pFilter);

    hr = IPin_QueryDirection(pin, &dir);
    ok_(__FILE__, line)(hr == S_OK, "Got hr %#lx.\n", hr);
    ok_(__FILE__, line)(dir == expect_dir, "Got direction %d.\n", dir);

    hr = IPin_QueryId(pin, &id);
    ok_(__FILE__, line)(hr == S_OK, "Got hr %#lx.\n", hr);
    ok_(__FILE__, line)(!wcscmp(id, expect_id), "Got id %s.\n", debugstr_w(id));
    CoTaskMemFree(id);

    hr = IPin_QueryInternalConnections(pin, NULL, &count);
    ok_(__FILE__, line)(hr == E_NOTIMPL, "Got hr %#lx.\n", hr);

    hr = IBaseFilter_FindPin(filter, expect_id, &pin2);
    ok_(__FILE__, line)(hr == S_OK, "Got hr %#lx.\n", hr);
    ok_(__FILE__, line)(pin2 == pin, "Pins didn't match.\n");
    IPin_Release(pin2);
}

static void test_enum_pins(void)
{
    AM_MEDIA_TYPE req_mt =
    {
        .majortype = MEDIATYPE_Stream,
        .subtype = MEDIASUBTYPE_Avi,
        .formattype = FORMAT_None,
        .bTemporalCompression = TRUE,
    };
    IBaseFilter *filter = create_infinite_tee();
    struct testfilter testsource, testsinks[2];
    IEnumPins *enum1, *enum2;
    IFilterGraph2 *graph;
    IPin *pins[5], *sink;
    ULONG count, ref;
    HRESULT hr;

    /* Tests for basic IEnumPins behaviour. */

    ref = get_refcount(filter);
    ok(ref == 1, "Got unexpected refcount %ld.\n", ref);

    hr = IBaseFilter_EnumPins(filter, NULL);
    ok(hr == E_POINTER, "Got hr %#lx.\n", hr);

    hr = IBaseFilter_EnumPins(filter, &enum1);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ref = get_refcount(filter);
    ok(ref == 2, "Got unexpected refcount %ld.\n", ref);
    ref = get_refcount(enum1);
    ok(ref == 1, "Got unexpected refcount %ld.\n", ref);

    hr = IEnumPins_Next(enum1, 1, NULL, NULL);
    ok(hr == E_POINTER, "Got hr %#lx.\n", hr);

    hr = IEnumPins_Next(enum1, 1, pins, NULL);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ref = get_refcount(filter);
    ok(ref == 3, "Got unexpected refcount %ld.\n", ref);
    ref = get_refcount(pins[0]);
    ok(ref == 3, "Got unexpected refcount %ld.\n", ref);
    ref = get_refcount(enum1);
    ok(ref == 1, "Got unexpected refcount %ld.\n", ref);
    IPin_Release(pins[0]);
    ref = get_refcount(filter);
    ok(ref == 2, "Got unexpected refcount %ld.\n", ref);

    hr = IEnumPins_Next(enum1, 1, pins, NULL);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ref = get_refcount(filter);
    todo_wine ok(ref == 2, "Got unexpected refcount %ld.\n", ref);
    ref = get_refcount(pins[0]);
    todo_wine ok(ref == 2, "Got unexpected refcount %ld.\n", ref);
    ref = get_refcount(enum1);
    ok(ref == 1, "Got unexpected refcount %ld.\n", ref);
    IPin_Release(pins[0]);
    ref = get_refcount(filter);
    ok(ref == 2, "Got unexpected refcount %ld.\n", ref);

    hr = IEnumPins_Next(enum1, 1, pins, NULL);
    ok(hr == S_FALSE, "Got hr %#lx.\n", hr);

    hr = IEnumPins_Reset(enum1);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);

    hr = IEnumPins_Next(enum1, 1, pins, &count);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(count == 1, "Got count %lu.\n", count);
    IPin_Release(pins[0]);

    hr = IEnumPins_Next(enum1, 1, pins, &count);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(count == 1, "Got count %lu.\n", count);
    IPin_Release(pins[0]);

    hr = IEnumPins_Next(enum1, 1, pins, &count);
    ok(hr == S_FALSE, "Got hr %#lx.\n", hr);
    ok(!count, "Got count %lu.\n", count);

    hr = IEnumPins_Reset(enum1);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);

    hr = IEnumPins_Next(enum1, 2, pins, NULL);
    ok(hr == E_INVALIDARG, "Got hr %#lx.\n", hr);

    hr = IEnumPins_Next(enum1, 2, pins, &count);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(count == 2, "Got count %lu.\n", count);
    IPin_Release(pins[0]);
    IPin_Release(pins[1]);

    hr = IEnumPins_Next(enum1, 2, pins, &count);
    ok(hr == S_FALSE, "Got hr %#lx.\n", hr);
    ok(!count, "Got count %lu.\n", count);

    hr = IEnumPins_Reset(enum1);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);

    hr = IEnumPins_Next(enum1, 3, pins, &count);
    ok(hr == S_FALSE, "Got hr %#lx.\n", hr);
    ok(count == 2, "Got count %lu.\n", count);
    IPin_Release(pins[0]);
    IPin_Release(pins[1]);

    hr = IEnumPins_Reset(enum1);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);

    hr = IEnumPins_Clone(enum1, &enum2);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);

    hr = IEnumPins_Skip(enum1, 3);
    ok(hr == S_FALSE, "Got hr %#lx.\n", hr);

    hr = IEnumPins_Skip(enum1, 2);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);

    hr = IEnumPins_Skip(enum1, 1);
    ok(hr == S_FALSE, "Got hr %#lx.\n", hr);

    hr = IEnumPins_Next(enum1, 1, pins, NULL);
    ok(hr == S_FALSE, "Got hr %#lx.\n", hr);

    hr = IEnumPins_Next(enum2, 1, pins, NULL);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    IPin_Release(pins[0]);

    IEnumPins_Release(enum2);

    /* Test pin information.
     *
     * Connecting to a pin adds a new pin at the end of the list.
     * Disconnecting from a pin removes the pin that is disconnected from
     * (rather than, say, whatever pin is currently not connected). */

    testfilter_init(&testsource);
    CoCreateInstance(&CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER,
            &IID_IFilterGraph2, (void **)&graph);
    IFilterGraph2_AddFilter(graph, &testsource.filter.IBaseFilter_iface, L"source");
    IFilterGraph2_AddFilter(graph, filter, L"tee");
    IBaseFilter_FindPin(filter, L"Input", &sink);
    hr = IFilterGraph2_ConnectDirect(graph, &testsource.source.pin.IPin_iface, sink, &req_mt);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);

    hr = IEnumPins_Reset(enum1);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);

    hr = IEnumPins_Next(enum1, 5, pins, &count);
    ok(hr == S_FALSE, "Got hr %#lx.\n", hr);
    ok(count == 2, "Got count %lu.\n", count);
    check_pin_info(filter, pins[0], L"Input", PINDIR_INPUT);
    check_pin_info(filter, pins[1], L"Output1", PINDIR_OUTPUT);
    IPin_Release(pins[0]);
    IPin_Release(pins[1]);

    hr = IEnumPins_Reset(enum1);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);

    testsink_add(&testsinks[0], filter, graph, L"Output1");

    hr = IEnumPins_Next(enum1, 5, pins, &count);
    ok(hr == S_FALSE, "Got hr %#lx.\n", hr);
    ok(count == 3, "Got count %lu.\n", count);
    check_pin_info(filter, pins[0], L"Input", PINDIR_INPUT);
    check_pin_info(filter, pins[1], L"Output1", PINDIR_OUTPUT);
    check_pin_info(filter, pins[2], L"Output2", PINDIR_OUTPUT);
    IPin_Release(pins[0]);
    IPin_Release(pins[1]);
    IPin_Release(pins[2]);

    hr = IEnumPins_Reset(enum1);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);

    testsink_remove(&testsinks[0], filter, graph);

    hr = IEnumPins_Next(enum1, 5, pins, &count);
    ok(hr == S_FALSE, "Got hr %#lx.\n", hr);
    ok(count == 2, "Got count %lu.\n", count);
    check_pin_info(filter, pins[0], L"Input", PINDIR_INPUT);
    check_pin_info(filter, pins[1], L"Output2", PINDIR_OUTPUT);
    IPin_Release(pins[0]);
    IPin_Release(pins[1]);

    hr = IEnumPins_Reset(enum1);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);

    testsink_add(&testsinks[0], filter, graph, L"Output2");

    hr = IEnumPins_Next(enum1, 5, pins, &count);
    ok(hr == S_FALSE, "Got hr %#lx.\n", hr);
    ok(count == 3, "Got count %lu.\n", count);
    check_pin_info(filter, pins[0], L"Input", PINDIR_INPUT);
    check_pin_info(filter, pins[1], L"Output2", PINDIR_OUTPUT);
    check_pin_info(filter, pins[2], L"Output3", PINDIR_OUTPUT);
    IPin_Release(pins[0]);
    IPin_Release(pins[1]);
    IPin_Release(pins[2]);

    hr = IEnumPins_Reset(enum1);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);

    testsink_add(&testsinks[1], filter, graph, L"Output3");
    testsink_remove(&testsinks[1], filter, graph);
    testsink_add(&testsinks[1], filter, graph, L"Output4");

    hr = IEnumPins_Next(enum1, 5, pins, &count);
    ok(hr == S_FALSE, "Got hr %#lx.\n", hr);
    ok(count == 4, "Got count %lu.\n", count);
    check_pin_info(filter, pins[0], L"Input", PINDIR_INPUT);
    check_pin_info(filter, pins[1], L"Output2", PINDIR_OUTPUT);
    check_pin_info(filter, pins[2], L"Output4", PINDIR_OUTPUT);
    check_pin_info(filter, pins[3], L"Output5", PINDIR_OUTPUT);
    IPin_Release(pins[0]);
    IPin_Release(pins[1]);
    IPin_Release(pins[2]);
    IPin_Release(pins[3]);

    testsink_remove(&testsinks[0], filter, graph);
    testsink_remove(&testsinks[1], filter, graph);

    IEnumPins_Release(enum1);

    IFilterGraph2_Disconnect(graph, &testsource.source.pin.IPin_iface);
    IFilterGraph2_Disconnect(graph, sink);
    IPin_Release(sink);
    ref = IFilterGraph2_Release(graph);
    ok(!ref, "Got outstanding refcount %ld.\n", ref);
    ref = IBaseFilter_Release(&testsource.filter.IBaseFilter_iface);
    ok(!ref, "Got outstanding refcount %ld.\n", ref);

    ref = IBaseFilter_Release(filter);
    ok(!ref, "Got outstanding refcount %ld.\n", ref);
}

static const GUID test_iid = {0x33333333};
static LONG outer_ref = 1;

static HRESULT WINAPI outer_QueryInterface(IUnknown *iface, REFIID iid, void **out)
{
    if (IsEqualGUID(iid, &IID_IUnknown)
            || IsEqualGUID(iid, &IID_IBaseFilter)
            || IsEqualGUID(iid, &test_iid))
    {
        *out = (IUnknown *)0xdeadbeef;
        return S_OK;
    }
    ok(0, "unexpected call %s\n", wine_dbgstr_guid(iid));
    return E_NOINTERFACE;
}

static ULONG WINAPI outer_AddRef(IUnknown *iface)
{
    return InterlockedIncrement(&outer_ref);
}

static ULONG WINAPI outer_Release(IUnknown *iface)
{
    return InterlockedDecrement(&outer_ref);
}

static const IUnknownVtbl outer_vtbl =
{
    outer_QueryInterface,
    outer_AddRef,
    outer_Release,
};

static IUnknown test_outer = {&outer_vtbl};

static void test_aggregation(void)
{
    IBaseFilter *filter, *filter2;
    IUnknown *unk, *unk2;
    HRESULT hr;
    ULONG ref;

    filter = (IBaseFilter *)0xdeadbeef;
    hr = CoCreateInstance(&CLSID_InfTee, &test_outer, CLSCTX_INPROC_SERVER,
            &IID_IBaseFilter, (void **)&filter);
    ok(hr == E_NOINTERFACE, "Got hr %#lx.\n", hr);
    ok(!filter, "Got interface %p.\n", filter);

    hr = CoCreateInstance(&CLSID_InfTee, &test_outer, CLSCTX_INPROC_SERVER,
            &IID_IUnknown, (void **)&unk);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(outer_ref == 1, "Got unexpected refcount %ld.\n", outer_ref);
    ok(unk != &test_outer, "Returned IUnknown should not be outer IUnknown.\n");
    ref = get_refcount(unk);
    ok(ref == 1, "Got unexpected refcount %ld.\n", ref);

    ref = IUnknown_AddRef(unk);
    ok(ref == 2, "Got unexpected refcount %ld.\n", ref);
    ok(outer_ref == 1, "Got unexpected refcount %ld.\n", outer_ref);

    ref = IUnknown_Release(unk);
    ok(ref == 1, "Got unexpected refcount %ld.\n", ref);
    ok(outer_ref == 1, "Got unexpected refcount %ld.\n", outer_ref);

    hr = IUnknown_QueryInterface(unk, &IID_IUnknown, (void **)&unk2);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(unk2 == unk, "Got unexpected IUnknown %p.\n", unk2);
    IUnknown_Release(unk2);

    hr = IUnknown_QueryInterface(unk, &IID_IBaseFilter, (void **)&filter);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);

    hr = IBaseFilter_QueryInterface(filter, &IID_IUnknown, (void **)&unk2);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(unk2 == (IUnknown *)0xdeadbeef, "Got unexpected IUnknown %p.\n", unk2);

    hr = IBaseFilter_QueryInterface(filter, &IID_IBaseFilter, (void **)&filter2);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(filter2 == (IBaseFilter *)0xdeadbeef, "Got unexpected IBaseFilter %p.\n", filter2);

    hr = IUnknown_QueryInterface(unk, &test_iid, (void **)&unk2);
    ok(hr == E_NOINTERFACE, "Got hr %#lx.\n", hr);
    ok(!unk2, "Got unexpected IUnknown %p.\n", unk2);

    hr = IBaseFilter_QueryInterface(filter, &test_iid, (void **)&unk2);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(unk2 == (IUnknown *)0xdeadbeef, "Got unexpected IUnknown %p.\n", unk2);

    IBaseFilter_Release(filter);
    ref = IUnknown_Release(unk);
    ok(!ref, "Got unexpected refcount %ld.\n", ref);
    ok(outer_ref == 1, "Got unexpected refcount %ld.\n", outer_ref);
}

static void test_enum_media_types(void)
{
    IBaseFilter *filter = create_infinite_tee();
    IEnumMediaTypes *enum1, *enum2;
    AM_MEDIA_TYPE *mts[2];
    ULONG ref, count;
    HRESULT hr;
    IPin *pin;

    IBaseFilter_FindPin(filter, L"Input", &pin);

    hr = IPin_EnumMediaTypes(pin, &enum1);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);

    hr = IEnumMediaTypes_Next(enum1, 1, mts, NULL);
    ok(hr == S_FALSE, "Got hr %#lx.\n", hr);

    hr = IEnumMediaTypes_Next(enum1, 1, mts, &count);
    ok(hr == S_FALSE, "Got hr %#lx.\n", hr);
    ok(!count, "Got count %lu.\n", count);

    hr = IEnumMediaTypes_Reset(enum1);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);

    hr = IEnumMediaTypes_Next(enum1, 1, mts, NULL);
    ok(hr == S_FALSE, "Got hr %#lx.\n", hr);

    hr = IEnumMediaTypes_Clone(enum1, &enum2);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);

    hr = IEnumMediaTypes_Skip(enum1, 1);
    ok(hr == S_FALSE, "Got hr %#lx.\n", hr);

    hr = IEnumMediaTypes_Next(enum2, 1, mts, NULL);
    ok(hr == S_FALSE, "Got hr %#lx.\n", hr);

    IEnumMediaTypes_Release(enum1);
    IEnumMediaTypes_Release(enum2);
    IPin_Release(pin);

    ref = IBaseFilter_Release(filter);
    ok(!ref, "Got outstanding refcount %ld.\n", ref);
}

static void test_unconnected_filter_state(void)
{
    IBaseFilter *filter = create_infinite_tee();
    FILTER_STATE state;
    HRESULT hr;
    ULONG ref;

    hr = IBaseFilter_GetState(filter, 0, &state);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(state == State_Stopped, "Got state %u.\n", state);

    hr = IBaseFilter_Pause(filter);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);

    hr = IBaseFilter_GetState(filter, 0, &state);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(state == State_Paused, "Got state %u.\n", state);

    hr = IBaseFilter_Run(filter, 0);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);

    hr = IBaseFilter_GetState(filter, 0, &state);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(state == State_Running, "Got state %u.\n", state);

    hr = IBaseFilter_Pause(filter);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);

    hr = IBaseFilter_GetState(filter, 0, &state);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(state == State_Paused, "Got state %u.\n", state);

    hr = IBaseFilter_Stop(filter);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);

    hr = IBaseFilter_GetState(filter, 0, &state);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(state == State_Stopped, "Got state %u.\n", state);

    hr = IBaseFilter_Run(filter, 0);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);

    hr = IBaseFilter_GetState(filter, 0, &state);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(state == State_Running, "Got state %u.\n", state);

    hr = IBaseFilter_Stop(filter);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);

    hr = IBaseFilter_GetState(filter, 0, &state);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(state == State_Stopped, "Got state %u.\n", state);

    ref = IBaseFilter_Release(filter);
    ok(!ref, "Got outstanding refcount %ld.\n", ref);
}

static void test_source_media_types(const AM_MEDIA_TYPE *connection_mt, const AM_MEDIA_TYPE *source_mt, IPin *source)
{
    IEnumMediaTypes *enummt;
    AM_MEDIA_TYPE *mts[3];
    AM_MEDIA_TYPE req_mt;
    ULONG count;
    HRESULT hr;

    hr = IPin_EnumMediaTypes(source, &enummt);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    hr = IEnumMediaTypes_Next(enummt, 3, mts, &count);
    ok(hr == S_FALSE, "Got hr %#lx.\n", hr);
    ok(count == 1, "Got %lu types.\n", count);
    ok(compare_media_types(mts[0], source_mt), "Media types didn't match.\n");
    CoTaskMemFree(mts[0]);
    IEnumMediaTypes_Release(enummt);

    /* The smart tee has some logic to accept the connection media type. The
     * infinite tee does not; it defers straight to the upstream filter. */

    hr = IPin_QueryAccept(source, connection_mt);
    todo_wine ok(hr == S_FALSE, "Got hr %#lx.\n", hr);

    hr = IPin_QueryAccept(source, source_mt);
    todo_wine ok(hr == S_FALSE, "Got hr %#lx.\n", hr);

    req_mt.majortype = MEDIATYPE_Audio;
    req_mt.subtype = MEDIASUBTYPE_PCM;
    req_mt.formattype = test_iid;
    req_mt.cbFormat = sizeof(count);
    req_mt.pbFormat = (BYTE *)&count;
    req_mt.bTemporalCompression = TRUE;
    hr = IPin_QueryAccept(source, &req_mt);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
}

static void test_source_connection(const AM_MEDIA_TYPE *source_mt, IFilterGraph2 *graph,
        IMediaControl *control, struct testfilter *testsink, struct testfilter *testsource, IPin *source)
{
    AM_MEDIA_TYPE req_mt = *source_mt;
    ALLOCATOR_PROPERTIES props;
    AM_MEDIA_TYPE mt;
    HRESULT hr;
    IPin *peer;

    ok(testsource->got_connect == 1, "Got %u calls to Connect().\n", testsource->got_connect);
    testsource->got_connect = 0;

    peer = (IPin *)0xdeadbeef;
    hr = IPin_ConnectedTo(source, &peer);
    ok(hr == VFW_E_NOT_CONNECTED, "Got hr %#lx.\n", hr);
    ok(!peer, "Got peer %p.\n", peer);

    hr = IPin_ConnectionMediaType(source, &mt);
    ok(hr == VFW_E_NOT_CONNECTED, "Got hr %#lx.\n", hr);

    /* Exact connection. */

    hr = IMediaControl_Pause(control);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    hr = IFilterGraph2_ConnectDirect(graph, source, &testsink->sink.pin.IPin_iface, &req_mt);
    ok(hr == VFW_E_NOT_STOPPED, "Got hr %#lx.\n", hr);
    hr = IMediaControl_Stop(control);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);

    /* QueryAccept() upstream is consulted. */

    hr = IFilterGraph2_ConnectDirect(graph, source, &testsink->sink.pin.IPin_iface, &req_mt);
    todo_wine ok(hr == VFW_E_TYPE_NOT_ACCEPTED, "Got hr %#lx.\n", hr);
    if (hr == S_OK)
    {
        IFilterGraph2_Disconnect(graph, source);
        IFilterGraph2_Disconnect(graph, &testsink->sink.pin.IPin_iface);
    }
    req_mt.bTemporalCompression = TRUE;

    /* The upstream filter is disconnected and reconnected with the new media
     * type. Matching the upstream media type at all a priori is not necessary.
     * FIXME ZF: How does having multiple output pins affect this? */

    req_mt.majortype = MEDIATYPE_Audio;
    ok(testsource->got_connect == 0, "Got %u calls to Connect().\n", testsource->got_connect);
    hr = IFilterGraph2_ConnectDirect(graph, source, &testsink->sink.pin.IPin_iface, &req_mt);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    todo_wine ok(testsource->got_connect == 1, "Got %u calls to Connect().\n", testsource->got_connect);
    todo_wine ok(compare_media_types(&testsource->source.pin.mt, &req_mt), "Expected media type to be changed.\n");
    testsource->got_connect = 0;

    hr = IPin_ConnectedTo(source, &peer);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(peer == &testsink->sink.pin.IPin_iface, "Got peer %p.\n", peer);
    IPin_Release(peer);

    hr = IPin_ConnectionMediaType(source, &mt);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(compare_media_types(&mt, &req_mt), "Media types didn't match.\n");
    ok(compare_media_types(&testsink->sink.pin.mt, &req_mt), "Media types didn't match.\n");

    hr = IMediaControl_Pause(control);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    hr = IFilterGraph2_Disconnect(graph, source);
    ok(hr == VFW_E_NOT_STOPPED, "Got hr %#lx.\n", hr);
    hr = IMediaControl_Stop(control);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);

    ok(testsink->sink.pAllocator && testsink->sink.pAllocator != testsource->source.pAllocator,
            "Got unexpected allocator %p.\n", testsink->sink.pAllocator);

    /* The sink allocators get the same properties as the source allocator. */
    hr = IMemAllocator_GetProperties(testsink->sink.pAllocator, &props);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(props.cBuffers == 1, "Got %ld buffers.\n", props.cBuffers);
    ok(props.cbBuffer == 256, "Got size %ld.\n", props.cbBuffer);
    ok(props.cbAlign == 1, "Got alignment %ld.\n", props.cbAlign);
    ok(!props.cbPrefix, "Got prefix %ld.\n", props.cbPrefix);

    hr = IFilterGraph2_Disconnect(graph, source);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    hr = IFilterGraph2_Disconnect(graph, source);
    ok(hr == S_FALSE, "Got hr %#lx.\n", hr);
    ok(testsink->sink.pin.peer == source, "Got peer %p.\n", testsink->sink.pin.peer);
    IFilterGraph2_Disconnect(graph, &testsink->sink.pin.IPin_iface);

    /* The upstream filter isn't reconnected if the media type was already the
     * same. */

    ok(testsource->got_connect == 0, "Got %u calls to Connect().\n", testsource->got_connect);
    hr = IFilterGraph2_ConnectDirect(graph, source, &testsink->sink.pin.IPin_iface, &req_mt);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(testsource->got_connect == 0, "Got %u calls to Connect().\n", testsource->got_connect);
    IFilterGraph2_Disconnect(graph, source);
    IFilterGraph2_Disconnect(graph, &testsink->sink.pin.IPin_iface);

    /* Connection with wildcards. QueryAccept() is still consulted. */

    hr = IFilterGraph2_ConnectDirect(graph, source, &testsink->sink.pin.IPin_iface, NULL);
    todo_wine ok(hr == VFW_E_NO_ACCEPTABLE_TYPES, "Got hr %#lx.\n", hr);
    if (hr == S_OK)
    {
        IFilterGraph2_Disconnect(graph, source);
        IFilterGraph2_Disconnect(graph, &testsink->sink.pin.IPin_iface);
    }

    testsource->require_temporal_compression = false;

    ok(testsource->got_connect == 0, "Got %u calls to Connect().\n", testsource->got_connect);
    hr = IFilterGraph2_ConnectDirect(graph, source, &testsink->sink.pin.IPin_iface, NULL);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(compare_media_types(&testsink->sink.pin.mt, source_mt), "Media types didn't match.\n");
    todo_wine ok(compare_media_types(&testsource->source.pin.mt, source_mt), "Media types didn't match.\n");
    todo_wine ok(testsource->got_connect == 1, "Got %u calls to Connect().\n", testsource->got_connect);
    testsource->got_connect = 0;
    IFilterGraph2_Disconnect(graph, source);
    IFilterGraph2_Disconnect(graph, &testsink->sink.pin.IPin_iface);

    req_mt.majortype = GUID_NULL;
    ok(testsource->got_connect == 0, "Got %u calls to Connect().\n", testsource->got_connect);
    hr = IFilterGraph2_ConnectDirect(graph, source, &testsink->sink.pin.IPin_iface, &req_mt);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(compare_media_types(&testsink->sink.pin.mt, source_mt), "Media types didn't match.\n");
    ok(testsource->got_connect == 0, "Got %u calls to Connect().\n", testsource->got_connect);
    IFilterGraph2_Disconnect(graph, source);
    IFilterGraph2_Disconnect(graph, &testsink->sink.pin.IPin_iface);

    req_mt.subtype = MEDIASUBTYPE_RGB32;
    hr = IFilterGraph2_ConnectDirect(graph, source, &testsink->sink.pin.IPin_iface, &req_mt);
    ok(hr == VFW_E_NO_ACCEPTABLE_TYPES, "Got hr %#lx.\n", hr);

    req_mt.subtype = GUID_NULL;
    hr = IFilterGraph2_ConnectDirect(graph, source, &testsink->sink.pin.IPin_iface, &req_mt);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(compare_media_types(&testsink->sink.pin.mt, source_mt), "Media types didn't match.\n");
    IFilterGraph2_Disconnect(graph, source);
    IFilterGraph2_Disconnect(graph, &testsink->sink.pin.IPin_iface);

    req_mt.formattype = FORMAT_WaveFormatEx;
    hr = IFilterGraph2_ConnectDirect(graph, source, &testsink->sink.pin.IPin_iface, &req_mt);
    ok(hr == VFW_E_NO_ACCEPTABLE_TYPES, "Got hr %#lx.\n", hr);

    req_mt = *source_mt;
    req_mt.formattype = GUID_NULL;
    hr = IFilterGraph2_ConnectDirect(graph, source, &testsink->sink.pin.IPin_iface, &req_mt);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(compare_media_types(&testsink->sink.pin.mt, source_mt), "Media types didn't match.\n");
    IFilterGraph2_Disconnect(graph, source);
    IFilterGraph2_Disconnect(graph, &testsink->sink.pin.IPin_iface);

    req_mt.subtype = MEDIASUBTYPE_RGB32;
    hr = IFilterGraph2_ConnectDirect(graph, source, &testsink->sink.pin.IPin_iface, &req_mt);
    ok(hr == VFW_E_NO_ACCEPTABLE_TYPES, "Got hr %#lx.\n", hr);

    req_mt.subtype = GUID_NULL;
    hr = IFilterGraph2_ConnectDirect(graph, source, &testsink->sink.pin.IPin_iface, &req_mt);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(compare_media_types(&testsink->sink.pin.mt, source_mt), "Media types didn't match.\n");
    IFilterGraph2_Disconnect(graph, source);
    IFilterGraph2_Disconnect(graph, &testsink->sink.pin.IPin_iface);

    req_mt.majortype = MEDIATYPE_Audio;
    hr = IFilterGraph2_ConnectDirect(graph, source, &testsink->sink.pin.IPin_iface, &req_mt);
    ok(hr == VFW_E_NO_ACCEPTABLE_TYPES, "Got hr %#lx.\n", hr);

    /* Test enumeration of sink media types. */

    req_mt.majortype = MEDIATYPE_Audio;
    req_mt.subtype = MEDIASUBTYPE_PCM;
    req_mt.formattype = test_iid;
    testsink->sink_mt = &req_mt;

    ok(testsource->got_connect == 0, "Got %u calls to Connect().\n", testsource->got_connect);
    hr = IFilterGraph2_ConnectDirect(graph, source, &testsink->sink.pin.IPin_iface, NULL);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(compare_media_types(&testsink->sink.pin.mt, &req_mt), "Media types didn't match.\n");
    todo_wine ok(compare_media_types(&testsource->source.pin.mt, &req_mt), "Media types didn't match.\n");
    todo_wine ok(testsource->got_connect == 1, "Got %u calls to Connect().\n", testsource->got_connect);
    testsource->got_connect = 0;
    IFilterGraph2_Disconnect(graph, source);
    IFilterGraph2_Disconnect(graph, &testsink->sink.pin.IPin_iface);

    testsink->sink_mt = NULL;

    testsource->require_temporal_compression = true;
}

static void test_connect_pin(void)
{
    AM_MEDIA_TYPE req_mt =
    {
        .majortype = MEDIATYPE_Stream,
        .subtype = MEDIASUBTYPE_Avi,
        .formattype = FORMAT_None,
        .lSampleSize = 1,
    };
    IBaseFilter *filter = create_infinite_tee();
    struct testfilter testsource, testsink, testsink2;
    IPin *sink, *source1, *source2, *peer;
    AM_MEDIA_TYPE mt, *mts[3];
    IEnumMediaTypes *enummt;
    IMediaControl *control;
    IFilterGraph2 *graph;
    HRESULT hr;
    ULONG ref;

    testfilter_init(&testsource);
    testfilter_init(&testsink);
    testfilter_init(&testsink2);
    CoCreateInstance(&CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER,
            &IID_IFilterGraph2, (void **)&graph);
    IFilterGraph2_AddFilter(graph, &testsource.filter.IBaseFilter_iface, L"source");
    IFilterGraph2_AddFilter(graph, &testsink.filter.IBaseFilter_iface, L"sink");
    IFilterGraph2_AddFilter(graph, &testsink2.filter.IBaseFilter_iface, L"sink2");
    IFilterGraph2_AddFilter(graph, filter, L"tee");
    IFilterGraph2_QueryInterface(graph, &IID_IMediaControl, (void **)&control);
    IBaseFilter_FindPin(filter, L"Input", &sink);
    IBaseFilter_FindPin(filter, L"Output1", &source1);

    testsource.source_mt.majortype = MEDIATYPE_Video;
    testsource.source_mt.subtype = MEDIASUBTYPE_RGB8;
    testsource.source_mt.formattype = FORMAT_VideoInfo;

    hr = IPin_EnumMediaTypes(sink, &enummt);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    hr = IEnumMediaTypes_Next(enummt, 1, mts, NULL);
    ok(hr == S_FALSE, "Got hr %#lx.\n", hr);
    IEnumMediaTypes_Release(enummt);

    hr = IPin_EnumMediaTypes(source1, &enummt);
    ok(hr == VFW_E_NOT_CONNECTED, "Got hr %#lx.\n", hr);

    hr = IPin_QueryAccept(sink, &req_mt);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    hr = IPin_QueryAccept(source1, &req_mt);
    todo_wine ok(hr == S_FALSE, "Got hr %#lx.\n", hr);

    /* Test sink connection. */

    peer = (IPin *)0xdeadbeef;
    hr = IPin_ConnectedTo(sink, &peer);
    ok(hr == VFW_E_NOT_CONNECTED, "Got hr %#lx.\n", hr);
    ok(!peer, "Got peer %p.\n", peer);

    hr = IPin_ConnectionMediaType(sink, &mt);
    ok(hr == VFW_E_NOT_CONNECTED, "Got hr %#lx.\n", hr);

    hr = IMediaControl_Pause(control);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    hr = IFilterGraph2_ConnectDirect(graph, &testsource.source.pin.IPin_iface, sink, &req_mt);
    ok(hr == VFW_E_NOT_STOPPED, "Got hr %#lx.\n", hr);
    hr = IMediaControl_Stop(control);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);

    hr = IFilterGraph2_ConnectDirect(graph, &testsource.source.pin.IPin_iface, sink, &req_mt);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);

    hr = IPin_ConnectedTo(sink, &peer);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(peer == &testsource.source.pin.IPin_iface, "Got peer %p.\n", peer);
    IPin_Release(peer);

    hr = IPin_ConnectionMediaType(sink, &mt);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(compare_media_types(&mt, &req_mt), "Media types didn't match.\n");

    hr = IPin_EnumMediaTypes(sink, &enummt);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    hr = IEnumMediaTypes_Next(enummt, 1, mts, NULL);
    ok(hr == S_FALSE, "Got hr %#lx.\n", hr);
    IEnumMediaTypes_Release(enummt);

    test_source_media_types(&req_mt, &testsource.source_mt, source1);
    test_source_connection(&testsource.source_mt, graph, control, &testsink, &testsource, source1);

    hr = IMediaControl_Pause(control);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    hr = IFilterGraph2_Disconnect(graph, sink);
    ok(hr == VFW_E_NOT_STOPPED, "Got hr %#lx.\n", hr);
    hr = IMediaControl_Stop(control);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);

    hr = IFilterGraph2_Disconnect(graph, sink);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    hr = IFilterGraph2_Disconnect(graph, sink);
    ok(hr == S_FALSE, "Got hr %#lx.\n", hr);
    ok(testsource.source.pin.peer == sink, "Got peer %p.\n", testsource.source.pin.peer);
    IFilterGraph2_Disconnect(graph, &testsource.source.pin.IPin_iface);

    peer = (IPin *)0xdeadbeef;
    hr = IPin_ConnectedTo(sink, &peer);
    ok(hr == VFW_E_NOT_CONNECTED, "Got hr %#lx.\n", hr);
    ok(!peer, "Got peer %p.\n", peer);

    hr = IPin_ConnectionMediaType(sink, &mt);
    ok(hr == VFW_E_NOT_CONNECTED, "Got hr %#lx.\n", hr);

    /* Test connection of a second source while there's already a source
     * connected. */
    req_mt.bTemporalCompression = TRUE;
    hr = IFilterGraph2_ConnectDirect(graph, &testsource.source.pin.IPin_iface, sink, &req_mt);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    hr = IFilterGraph2_ConnectDirect(graph, source1, &testsink.sink.pin.IPin_iface, &req_mt);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    req_mt.bTemporalCompression = FALSE;

    IBaseFilter_FindPin(filter, L"Output10", &source2);
    test_source_connection(&testsource.source_mt, graph, control, &testsink2, &testsource, source2);
    IPin_Release(source2);

    hr = IFilterGraph2_Disconnect(graph, source1);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    hr = IFilterGraph2_Disconnect(graph, &testsink.sink.pin.IPin_iface);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);

    IFilterGraph2_Disconnect(graph, sink);
    IFilterGraph2_Disconnect(graph, &testsource.source.pin.IPin_iface);

    IPin_Release(sink);
    IPin_Release(source1);
    IMediaControl_Release(control);
    ref = IFilterGraph2_Release(graph);
    ok(!ref, "Got outstanding refcount %ld.\n", ref);
    ref = IBaseFilter_Release(filter);
    ok(!ref, "Got outstanding refcount %ld.\n", ref);
    ref = IBaseFilter_Release(&testsource.filter.IBaseFilter_iface);
    ok(!ref, "Got outstanding refcount %ld.\n", ref);
    ref = IBaseFilter_Release(&testsink.filter.IBaseFilter_iface);
    ok(!ref, "Got outstanding refcount %ld.\n", ref);
    ref = IBaseFilter_Release(&testsink2.filter.IBaseFilter_iface);
    ok(!ref, "Got outstanding refcount %ld.\n", ref);
}

static void test_streaming(void)
{
    AM_MEDIA_TYPE req_mt =
    {
        .majortype = MEDIATYPE_Stream,
        .subtype = MEDIASUBTYPE_Avi,
        .formattype = FORMAT_None,
        .bTemporalCompression = TRUE,
    };
    IBaseFilter *filter = create_infinite_tee();
    struct testfilter testsource, testsinks[2];
    REFERENCE_TIME start, stop;
    IMemAllocator *allocator;
    IMediaControl *control;
    IFilterGraph2 *graph;
    IMediaSample *sample;
    IMemInputPin *input;
    LONG size, i;
    HRESULT hr;
    IPin *sink;
    BYTE *data;
    ULONG ref;

    testfilter_init(&testsource);
    CoCreateInstance(&CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER,
            &IID_IFilterGraph2, (void **)&graph);
    IFilterGraph2_QueryInterface(graph, &IID_IMediaControl, (void **)&control);
    IFilterGraph2_AddFilter(graph, &testsource.filter.IBaseFilter_iface, L"source");
    IFilterGraph2_AddFilter(graph, filter, L"tee");
    IBaseFilter_FindPin(filter, L"Input", &sink);
    IPin_QueryInterface(sink, &IID_IMemInputPin, (void **)&input);
    hr = IFilterGraph2_ConnectDirect(graph, &testsource.source.pin.IPin_iface, sink, &req_mt);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);

    testsink_add(&testsinks[0], filter, graph, L"Output1");
    testsink_add(&testsinks[1], filter, graph, L"Output2");

    hr = IMemInputPin_ReceiveCanBlock(input);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);

    hr = IMemInputPin_GetAllocator(input, &allocator);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);

    hr = IMemAllocator_GetBuffer(allocator, &sample, NULL, NULL, 0);
    ok(hr == VFW_E_NOT_COMMITTED, "Got hr %#lx.\n", hr);

    hr = IMediaControl_Pause(control);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);

    hr = IMemAllocator_GetBuffer(allocator, &sample, NULL, NULL, 0);
    todo_wine ok(hr == S_OK, "Got hr %#lx.\n", hr);
    if (hr != S_OK)
    {
        IMemAllocator_Commit(allocator);
        hr = IMemAllocator_GetBuffer(allocator, &sample, NULL, NULL, 0);
        ok(hr == S_OK, "Got hr %#lx.\n", hr);
    }

    hr = IMediaSample_GetPointer(sample, &data);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    size = IMediaSample_GetSize(sample);
    ok(size == 256, "Got size %ld.\n", size);
    for (i = 0; i < 200; ++i)
        data[i] = i;
    hr = IMediaSample_SetActualDataLength(sample, 200);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);

    start = 10000;
    stop = 20000;
    hr = IMediaSample_SetMediaTime(sample, &start, &stop);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    start = 30000;
    stop = 40000;
    hr = IMediaSample_SetTime(sample, &start, &stop);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    hr = IMediaSample_SetDiscontinuity(sample, TRUE);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    hr = IMediaSample_SetPreroll(sample, TRUE);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    hr = IMediaSample_SetSyncPoint(sample, TRUE);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);

    hr = IMemInputPin_Receive(input, sample);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(!WaitForSingleObject(testsinks[0].sample_event, 1000), "Wait timed out.\n");
    ok(!WaitForSingleObject(testsinks[1].sample_event, 1000), "Wait timed out.\n");

    hr = IPin_NewSegment(sink, 10000, 20000, 1.0);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(!WaitForSingleObject(testsinks[0].segment_event, 1000), "Wait timed out.\n");
    ok(!WaitForSingleObject(testsinks[1].segment_event, 1000), "Wait timed out.\n");

    hr = IPin_EndOfStream(sink);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(!WaitForSingleObject(testsinks[0].eos_event, 1000), "Wait timed out.\n");
    ok(!WaitForSingleObject(testsinks[1].eos_event, 1000), "Wait timed out.\n");

    hr = IPin_EndOfStream(sink);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(!WaitForSingleObject(testsinks[0].eos_event, 1000), "Wait timed out.\n");
    ok(!WaitForSingleObject(testsinks[1].eos_event, 1000), "Wait timed out.\n");

    ok(!testsinks[0].got_begin_flush, "Got %u calls to IPin::BeginFlush().\n", testsinks[0].got_begin_flush);
    ok(!testsinks[1].got_begin_flush, "Got %u calls to IPin::BeginFlush().\n", testsinks[1].got_begin_flush);
    hr = IPin_BeginFlush(sink);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(testsinks[0].got_begin_flush == 1, "Got %u calls to IPin::BeginFlush().\n", testsinks[0].got_begin_flush);
    ok(testsinks[1].got_begin_flush == 1, "Got %u calls to IPin::BeginFlush().\n", testsinks[1].got_begin_flush);

    hr = IMemInputPin_Receive(input, sample);
    todo_wine ok(hr == S_FALSE, "Got hr %#lx.\n", hr);

    hr = IPin_EndOfStream(sink);
    todo_wine ok(hr == S_OK, "Got hr %#lx.\n", hr);
    /* No EOS events are sent downstream, however. */

    ok(!testsinks[0].got_end_flush, "Got %u calls to IPin::EndFlush().\n", testsinks[0].got_end_flush);
    ok(!testsinks[1].got_end_flush, "Got %u calls to IPin::EndFlush().\n", testsinks[1].got_end_flush);
    hr = IPin_EndFlush(sink);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(testsinks[0].got_end_flush == 1, "Got %u calls to IPin::EndFlush().\n", testsinks[0].got_end_flush);
    ok(testsinks[1].got_end_flush == 1, "Got %u calls to IPin::EndFlush().\n", testsinks[1].got_end_flush);

    hr = IMemInputPin_Receive(input, sample);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(!WaitForSingleObject(testsinks[0].sample_event, 1000), "Wait timed out.\n");
    ok(!WaitForSingleObject(testsinks[1].sample_event, 1000), "Wait timed out.\n");

    hr = IMediaControl_Stop(control);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);

    hr = IMemInputPin_Receive(input, sample);
    todo_wine ok(hr == VFW_E_WRONG_STATE, "Got hr %#lx.\n", hr);

    hr = IPin_EndOfStream(sink);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    /* No EOS events are sent downstream, however. */

    IMediaSample_Release(sample);
    IMemAllocator_Release(allocator);

    testsink_remove(&testsinks[0], filter, graph);
    testsink_remove(&testsinks[1], filter, graph);
    IFilterGraph2_Disconnect(graph, &testsource.source.pin.IPin_iface);
    IFilterGraph2_Disconnect(graph, sink);
    IMemInputPin_Release(input);
    IPin_Release(sink);
    IMediaControl_Release(control);
    ref = IFilterGraph2_Release(graph);
    ok(!ref, "Got outstanding refcount %ld.\n", ref);
    ref = IBaseFilter_Release(&testsource.filter.IBaseFilter_iface);
    ok(!ref, "Got outstanding refcount %ld.\n", ref);

    ref = IBaseFilter_Release(filter);
    ok(!ref, "Got outstanding refcount %ld.\n", ref);
}

START_TEST(inftee)
{
    CoInitialize(NULL);

    test_interfaces();
    test_enum_pins();
    test_aggregation();
    test_enum_media_types();
    test_unconnected_filter_state();
    test_connect_pin();
    test_streaming();

    CoUninitialize();
}
