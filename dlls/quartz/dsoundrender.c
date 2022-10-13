/*
 * Direct Sound Audio Renderer
 *
 * Copyright 2004 Christian Costa
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

#include "quartz_private.h"

#include "uuids.h"
#include "vfwmsgs.h"
#include "windef.h"
#include "winbase.h"
#include "dshow.h"
#include "evcode.h"
#include "strmif.h"
#include "initguid.h"
#include "dsound.h"
#include "amaudio.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(quartz);

struct dsound_render
{
    struct strmbase_filter filter;
    struct strmbase_passthrough passthrough;
    IAMDirectSound IAMDirectSound_iface;
    IBasicAudio IBasicAudio_iface;
    IQualityControl IQualityControl_iface;
    IUnknown *system_clock;

    struct strmbase_sink sink;

    /* Signaled when the filter has completed a state change. The filter waits
     * for this event in IBaseFilter::GetState(). */
    HANDLE state_event;
    /* Signaled when a flush or state change occurs, i.e. anything that needs
     * to immediately unblock the streaming thread. */
    HANDLE flush_event;
    HANDLE empty_event;
    REFERENCE_TIME stream_start;
    BOOL eos;

    IDirectSound8 *dsound;
    IDirectSoundBuffer *dsbuffer;

    DWORD buffer_size;
    DWORD write_pos;

    LONG volume;
    LONG pan;
};

static struct dsound_render *impl_from_strmbase_pin(struct strmbase_pin *iface)
{
    return CONTAINING_RECORD(iface, struct dsound_render, sink.pin);
}

static struct dsound_render *impl_from_strmbase_filter(struct strmbase_filter *iface)
{
    return CONTAINING_RECORD(iface, struct dsound_render, filter);
}

static struct dsound_render *impl_from_IBasicAudio(IBasicAudio *iface)
{
    return CONTAINING_RECORD(iface, struct dsound_render, IBasicAudio_iface);
}

static struct dsound_render *impl_from_IAMDirectSound(IAMDirectSound *iface)
{
    return CONTAINING_RECORD(iface, struct dsound_render, IAMDirectSound_iface);
}

static HRESULT dsound_render_write_data(struct dsound_render *filter, const void *data, DWORD size)
{
    WAVEFORMATEX *wfx = (WAVEFORMATEX *)filter->sink.pin.mt.pbFormat;
    unsigned char silence = wfx->wBitsPerSample == 8 ? 128  : 0;
    DWORD write_end, size1, size2;
    void *data1, *data2;
    HRESULT hr;

    TRACE("filter %p, data %p, size %#lx\n", filter, data, size);

    if (!size)
        size = filter->buffer_size / 2;
    else
        size = min(size, filter->buffer_size / 2);

    if (filter->write_pos == -1)
        filter->write_pos = 0;
    else
    {
        DWORD play_pos, write_pos;

        if (FAILED(hr = IDirectSoundBuffer_GetCurrentPosition(filter->dsbuffer,
                &play_pos, &write_pos)))
            return hr;

        if (filter->write_pos - play_pos <= write_pos - play_pos)
        {
            WARN("Buffer underrun detected, filter %p, dsound play pos %#lx, write pos %#lx, filter write pos %#lx!\n",
                    filter, play_pos, write_pos, filter->write_pos);
            filter->write_pos = write_pos;
        }

        write_end = (filter->write_pos + size) % filter->buffer_size;
        if (write_end - filter->write_pos >= play_pos - filter->write_pos)
            return S_FALSE;
    }

    if (FAILED(hr = IDirectSoundBuffer_Lock(filter->dsbuffer, filter->write_pos,
            size, &data1, &size1, &data2, &size2, 0)))
    {
        ERR("Failed to lock buffer %p, hr %#lx\n", filter->dsbuffer, hr);
        return hr;
    }

    TRACE("Locked data1 %p, size1 %#lx, data2 %p, size2 %#lx\n", data1, size1, data2, size2);

    if (!data)
    {
        memset(data1, silence, size1);
        memset(data2, silence, size2);
    }
    else
    {
        memcpy(data1, data, size1);
        data = (char *)data + size1;
        memcpy(data2, data, size2);
    }

    if (FAILED(hr = IDirectSoundBuffer_Unlock(filter->dsbuffer, data1, size1, data2, size2)))
    {
        ERR("Failed to unlock buffer %p, hr %#lx\n", filter->dsbuffer, hr);
        return hr;
    }

    TRACE("Unlocked data1 %p, size1 %#lx, data2 %p, size2 %#lx\n", data1, size1, data2, size2);

    filter->write_pos += size;
    filter->write_pos %= filter->buffer_size;

    TRACE("Written size %#lx, write_pos %#lx\n", size, filter->write_pos);

    return S_OK;
}

static HRESULT dsound_render_wait_play_end(struct dsound_render *filter)
{
    DWORD start_pos, play_pos, end_pos = filter->write_pos;
    HRESULT hr;

    if (FAILED(hr = IDirectSoundBuffer_GetCurrentPosition(filter->dsbuffer,
            &start_pos, NULL)))
    {
        ERR("Failed to get buffer positions, hr %#lx\n", hr);
        return hr;
    }

    if (FAILED(hr = dsound_render_write_data(filter, NULL, 0)))
        return hr;

    while (filter->filter.state == State_Running)
    {
        HANDLE handles[] = {filter->empty_event, filter->flush_event};

        if (FAILED(hr = IDirectSoundBuffer_GetCurrentPosition(filter->dsbuffer,
                &play_pos, NULL)))
        {
            ERR("Failed to get buffer positions, hr %#lx\n", hr);
            return hr;
        }

        if (play_pos - start_pos >= end_pos - start_pos)
            break;

        WARN("Waiting for EOS, start_pos %#lx, play_pos %#lx, end_pos %#lx\n",
                start_pos, play_pos, end_pos);

        WaitForMultipleObjects(2, handles, FALSE, INFINITE);
    }

    return S_OK;
}

static HRESULT dsound_render_configure_buffer(struct dsound_render *filter, IMediaSample *sample)
{
    HRESULT hr;
    AM_MEDIA_TYPE *amt;

    if (IMediaSample_GetMediaType(sample, &amt) == S_OK)
    {
        AM_MEDIA_TYPE *orig = &filter->sink.pin.mt;
        WAVEFORMATEX *origfmt = (WAVEFORMATEX *)orig->pbFormat;
        WAVEFORMATEX *newfmt = (WAVEFORMATEX *)amt->pbFormat;

        TRACE("Format change.\n");
        strmbase_dump_media_type(amt);

        if (origfmt->wFormatTag == newfmt->wFormatTag &&
            origfmt->nChannels == newfmt->nChannels &&
            origfmt->nBlockAlign == newfmt->nBlockAlign &&
            origfmt->wBitsPerSample == newfmt->wBitsPerSample &&
            origfmt->cbSize ==  newfmt->cbSize)
        {
            if (origfmt->nSamplesPerSec != newfmt->nSamplesPerSec)
            {
                hr = IDirectSoundBuffer_SetFrequency(filter->dsbuffer,
                                                     newfmt->nSamplesPerSec);
                if (FAILED(hr))
                    return VFW_E_TYPE_NOT_ACCEPTED;
                FreeMediaType(orig);
                CopyMediaType(orig, amt);
                IMediaSample_SetMediaType(sample, NULL);
            }
        }
        else
            return VFW_E_TYPE_NOT_ACCEPTED;
    }

    return S_OK;
}

static HRESULT WINAPI dsound_render_sink_Receive(struct strmbase_sink *iface, IMediaSample *sample)
{
    struct dsound_render *filter = impl_from_strmbase_pin(&iface->pin);
    WAVEFORMATEX *wfx = (WAVEFORMATEX *)filter->sink.pin.mt.pbFormat;
    REFERENCE_TIME current = 0, start = 0, stop;
    HRESULT hr;
    BYTE *data;
    LONG size;

    TRACE("filter %p, sample %p.\n", filter, sample);

    if (filter->eos || filter->sink.flushing)
        return S_FALSE;

    if (filter->filter.state == State_Stopped)
        return VFW_E_WRONG_STATE;

    if (FAILED(hr = dsound_render_configure_buffer(filter, sample)))
        return hr;

    if (filter->filter.clock)
    {
        if (SUCCEEDED(IMediaSample_GetTime(sample, &start, &stop)))
            strmbase_passthrough_update_time(&filter->passthrough, start);

        if (filter->stream_start != -1 && SUCCEEDED(IReferenceClock_GetTime(filter->filter.clock, &current)))
            current -= filter->stream_start;
    }

    if (filter->filter.state == State_Paused)
        SetEvent(filter->state_event);

    if (FAILED(hr = IMediaSample_GetPointer(sample, &data)))
    {
        ERR("Failed to get buffer pointer, hr %#lx.\n", hr);
        return hr;
    }

    size = IMediaSample_GetActualDataLength(sample);

    if (filter->write_pos == -1 && start > current)
    {
        /* prepend some silence if the first sample doesn't start at 0 */
        DWORD length = (start - current) * wfx->nAvgBytesPerSec / 10000000;
        length -= length % wfx->nBlockAlign;

        if (length != 0 && FAILED(hr = dsound_render_write_data(filter, NULL, length)))
            return hr;
    }
    else if (filter->write_pos == -1 && start < current)
    {
        /* or skip some data instead if the first sample is late */
        DWORD offset = (current - start) * wfx->nAvgBytesPerSec / 10000000;
        offset -= offset % wfx->nBlockAlign;

        data += min(size, offset);
        size -= min(size, offset);
    }

    while (size && (hr = dsound_render_write_data(filter, data, size)) == S_FALSE)
    {
        HANDLE handles[] = {filter->empty_event, filter->flush_event};

        WARN("Could not write %#lx bytes, waiting for buffer to empty.\n", size);

        if (WaitForMultipleObjects(2, handles, FALSE, INFINITE) == WAIT_OBJECT_0 + 1)
        {
            TRACE("Flush event notified, dropping sample.\n");
            return S_OK;
        }
    }

    return hr;
}

static HRESULT dsound_render_sink_query_interface(struct strmbase_pin *iface, REFIID iid, void **out)
{
    struct dsound_render *filter = impl_from_strmbase_pin(iface);

    if (IsEqualGUID(iid, &IID_IMemInputPin))
        *out = &filter->sink.IMemInputPin_iface;
    else
        return E_NOINTERFACE;

    IUnknown_AddRef((IUnknown *)*out);
    return S_OK;
}

static HRESULT dsound_render_sink_query_accept(struct strmbase_pin *iface, const AM_MEDIA_TYPE * pmt)
{
    if (!IsEqualIID(&pmt->majortype, &MEDIATYPE_Audio))
        return S_FALSE;

    return S_OK;
}

static HRESULT dsound_render_sink_connect(struct strmbase_sink *iface, IPin *peer, const AM_MEDIA_TYPE *mt)
{
    struct dsound_render *filter = impl_from_strmbase_pin(&iface->pin);
    const WAVEFORMATEX *format = (WAVEFORMATEX *)mt->pbFormat;
    IDirectSoundNotify *notify;
    DSBUFFERDESC buf_desc;
    HRESULT hr = S_OK;

    memset(&buf_desc,0,sizeof(DSBUFFERDESC));
    buf_desc.dwSize = sizeof(DSBUFFERDESC);
    buf_desc.dwFlags = DSBCAPS_CTRLVOLUME | DSBCAPS_CTRLPAN |
                       DSBCAPS_CTRLFREQUENCY | DSBCAPS_GLOBALFOCUS |
                       DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_CTRLPOSITIONNOTIFY;
    buf_desc.dwBufferBytes = format->nAvgBytesPerSec;
    buf_desc.lpwfxFormat = (WAVEFORMATEX *)format;

    filter->buffer_size = format->nAvgBytesPerSec;
    filter->write_pos = -1;

    if (FAILED(hr = IDirectSound8_CreateSoundBuffer(filter->dsound, &buf_desc, &filter->dsbuffer, NULL)))
        ERR("Failed to create sound buffer, hr %#lx.\n", hr);
    else if (FAILED(hr = IDirectSoundBuffer_QueryInterface(filter->dsbuffer, &IID_IDirectSoundNotify,
                (void **)&notify)))
        ERR("Failed to query IDirectSoundNotify iface, hr %#lx.\n", hr);
    else
    {
        DSBPOSITIONNOTIFY positions[10] = {{.dwOffset = 0, .hEventNotify = filter->empty_event}};
        int i;

        TRACE("Created buffer %p with size %#lx\n", filter->dsbuffer, filter->buffer_size);

        for (i = 1; i < ARRAY_SIZE(positions); ++i)
        {
            positions[i] = positions[i - 1];
            positions[i].dwOffset += filter->buffer_size / ARRAY_SIZE(positions);
        }

        if (FAILED(hr = IDirectSoundNotify_SetNotificationPositions(notify,
                ARRAY_SIZE(positions), positions)))
            ERR("Failed to set notification positions, hr %#lx\n", hr);

        IDirectSoundNotify_Release(notify);
    }

    if (SUCCEEDED(hr))
    {
        hr = IDirectSoundBuffer_SetVolume(filter->dsbuffer, filter->volume);
        if (FAILED(hr))
            ERR("Failed to set volume to %ld, hr %#lx.\n", filter->volume, hr);

        hr = IDirectSoundBuffer_SetPan(filter->dsbuffer, filter->pan);
        if (FAILED(hr))
            ERR("Failed to set pan to %ld, hr %#lx.\n", filter->pan, hr);
        hr = S_OK;
    }

    if (FAILED(hr) && hr != VFW_E_ALREADY_CONNECTED)
    {
        if (filter->dsbuffer)
            IDirectSoundBuffer_Release(filter->dsbuffer);
        filter->dsbuffer = NULL;
    }

    return hr;
}

static void dsound_render_sink_disconnect(struct strmbase_sink *iface)
{
    struct dsound_render *filter = impl_from_strmbase_pin(&iface->pin);

    TRACE("filter %p\n", filter);

    if (filter->dsbuffer)
        IDirectSoundBuffer_Release(filter->dsbuffer);
    filter->dsbuffer = NULL;
}

static HRESULT dsound_render_sink_eos(struct strmbase_sink *iface)
{
    struct dsound_render *filter = impl_from_strmbase_pin(&iface->pin);
    IFilterGraph *graph = filter->filter.graph;
    IMediaEventSink *event_sink;

    filter->eos = TRUE;

    if (filter->filter.state == State_Running && graph
            && SUCCEEDED(IFilterGraph_QueryInterface(graph,
            &IID_IMediaEventSink, (void **)&event_sink)))
    {
        IMediaEventSink_Notify(event_sink, EC_COMPLETE, S_OK,
                (LONG_PTR)&filter->filter.IBaseFilter_iface);
        IMediaEventSink_Release(event_sink);
    }
    strmbase_passthrough_eos(&filter->passthrough);
    SetEvent(filter->state_event);

    if (filter->dsbuffer && filter->write_pos != -1)
        dsound_render_wait_play_end(filter);

    return S_OK;
}

static HRESULT dsound_render_sink_begin_flush(struct strmbase_sink *iface)
{
    struct dsound_render *filter = impl_from_strmbase_pin(&iface->pin);

    SetEvent(filter->flush_event);
    return S_OK;
}

static HRESULT dsound_render_sink_end_flush(struct strmbase_sink *iface)
{
    struct dsound_render *filter = impl_from_strmbase_pin(&iface->pin);

    EnterCriticalSection(&filter->filter.stream_cs);

    filter->eos = FALSE;
    strmbase_passthrough_invalidate_time(&filter->passthrough);
    ResetEvent(filter->flush_event);

    if (filter->dsbuffer)
    {
        void *buffer;
        DWORD size;

        /* Force a reset */
        IDirectSoundBuffer_Lock(filter->dsbuffer, 0, 0, &buffer, &size, NULL, NULL, DSBLOCK_ENTIREBUFFER);
        memset(buffer, 0, size);
        IDirectSoundBuffer_Unlock(filter->dsbuffer, buffer, size, NULL, 0);
        filter->write_pos = -1;
    }

    LeaveCriticalSection(&filter->filter.stream_cs);
    return S_OK;
}

static const struct strmbase_sink_ops sink_ops =
{
    .base.pin_query_interface = dsound_render_sink_query_interface,
    .base.pin_query_accept = dsound_render_sink_query_accept,
    .pfnReceive = dsound_render_sink_Receive,
    .sink_connect = dsound_render_sink_connect,
    .sink_disconnect = dsound_render_sink_disconnect,
    .sink_eos = dsound_render_sink_eos,
    .sink_begin_flush = dsound_render_sink_begin_flush,
    .sink_end_flush = dsound_render_sink_end_flush,
};

static void dsound_render_destroy(struct strmbase_filter *iface)
{
    struct dsound_render *filter = impl_from_strmbase_filter(iface);

    if (filter->dsbuffer)
        IDirectSoundBuffer_Release(filter->dsbuffer);
    filter->dsbuffer = NULL;
    if (filter->dsound)
        IDirectSound8_Release(filter->dsound);
    filter->dsound = NULL;

    IUnknown_Release(filter->system_clock);

    if (filter->sink.pin.peer)
        IPin_Disconnect(filter->sink.pin.peer);
    IPin_Disconnect(&filter->sink.pin.IPin_iface);
    strmbase_sink_cleanup(&filter->sink);

    CloseHandle(filter->state_event);
    CloseHandle(filter->flush_event);
    CloseHandle(filter->empty_event);

    strmbase_passthrough_cleanup(&filter->passthrough);
    strmbase_filter_cleanup(&filter->filter);
    free(filter);
}

static struct strmbase_pin *dsound_render_get_pin(struct strmbase_filter *iface, unsigned int index)
{
    struct dsound_render *filter = impl_from_strmbase_filter(iface);

    if (index == 0)
        return &filter->sink.pin;
    return NULL;
}

static HRESULT dsound_render_query_interface(struct strmbase_filter *iface, REFIID iid, void **out)
{
    struct dsound_render *filter = impl_from_strmbase_filter(iface);

    if (IsEqualGUID(iid, &IID_IAMDirectSound))
        *out = &filter->IAMDirectSound_iface;
    else if (IsEqualGUID(iid, &IID_IBasicAudio))
        *out = &filter->IBasicAudio_iface;
    else if (IsEqualGUID(iid, &IID_IMediaPosition))
        *out = &filter->passthrough.IMediaPosition_iface;
    else if (IsEqualGUID(iid, &IID_IMediaSeeking))
        *out = &filter->passthrough.IMediaSeeking_iface;
    else if (IsEqualGUID(iid, &IID_IQualityControl))
        *out = &filter->IQualityControl_iface;
    else if (IsEqualGUID(iid, &IID_IReferenceClock))
        return IUnknown_QueryInterface(filter->system_clock, iid, out);
    else
        return E_NOINTERFACE;

    IUnknown_AddRef((IUnknown *)*out);
    return S_OK;
}

static HRESULT dsound_render_init_stream(struct strmbase_filter *iface)
{
    struct dsound_render *filter = impl_from_strmbase_filter(iface);

    if (filter->sink.pin.peer)
        ResetEvent(filter->state_event);
    filter->eos = FALSE;
    ResetEvent(filter->flush_event);

    return filter->sink.pin.peer ? S_FALSE : S_OK;
}

static HRESULT dsound_render_start_stream(struct strmbase_filter *iface, REFERENCE_TIME start)
{
    struct dsound_render *filter = impl_from_strmbase_filter(iface);
    IFilterGraph *graph = filter->filter.graph;
    IMediaEventSink *event_sink;

    filter->stream_start = start;

    SetEvent(filter->state_event);

    if (filter->sink.pin.peer)
        IDirectSoundBuffer_Play(filter->dsbuffer, 0, 0, DSBPLAY_LOOPING);

    if ((filter->eos || !filter->sink.pin.peer) && graph
            && SUCCEEDED(IFilterGraph_QueryInterface(graph,
            &IID_IMediaEventSink, (void **)&event_sink)))
    {
        IMediaEventSink_Notify(event_sink, EC_COMPLETE, S_OK,
                (LONG_PTR)&filter->filter.IBaseFilter_iface);
        IMediaEventSink_Release(event_sink);
    }

    return S_OK;
}

static HRESULT dsound_render_stop_stream(struct strmbase_filter *iface)
{
    struct dsound_render *filter = impl_from_strmbase_filter(iface);

    if (filter->sink.pin.peer)
        IDirectSoundBuffer_Stop(filter->dsbuffer);

    filter->stream_start = -1;

    return S_OK;
}

static HRESULT dsound_render_cleanup_stream(struct strmbase_filter *iface)
{
    struct dsound_render *filter = impl_from_strmbase_filter(iface);

    strmbase_passthrough_invalidate_time(&filter->passthrough);
    SetEvent(filter->state_event);
    SetEvent(filter->flush_event);

    return S_OK;
}

static HRESULT dsound_render_wait_state(struct strmbase_filter *iface, DWORD timeout)
{
    struct dsound_render *filter = impl_from_strmbase_filter(iface);

    if (WaitForSingleObject(filter->state_event, timeout) == WAIT_TIMEOUT)
        return VFW_S_STATE_INTERMEDIATE;
    return S_OK;
}

static const struct strmbase_filter_ops filter_ops =
{
    .filter_destroy = dsound_render_destroy,
    .filter_get_pin = dsound_render_get_pin,
    .filter_query_interface = dsound_render_query_interface,
    .filter_init_stream = dsound_render_init_stream,
    .filter_start_stream = dsound_render_start_stream,
    .filter_stop_stream = dsound_render_stop_stream,
    .filter_cleanup_stream = dsound_render_cleanup_stream,
    .filter_wait_state = dsound_render_wait_state,
};

static HRESULT WINAPI basic_audio_QueryInterface(IBasicAudio *iface, REFIID riid, void **out)
{
    struct dsound_render *filter = impl_from_IBasicAudio(iface);
    return IUnknown_QueryInterface(filter->filter.outer_unk, riid, out);
}

static ULONG WINAPI basic_audio_AddRef(IBasicAudio *iface)
{
    struct dsound_render *filter = impl_from_IBasicAudio(iface);
    return IUnknown_AddRef(filter->filter.outer_unk);
}

static ULONG WINAPI basic_audio_Release(IBasicAudio *iface)
{
    struct dsound_render *filter = impl_from_IBasicAudio(iface);
    return IUnknown_Release(filter->filter.outer_unk);
}

static HRESULT WINAPI basic_audio_GetTypeInfoCount(IBasicAudio *iface, UINT *count)
{
    struct dsound_render *filter = impl_from_IBasicAudio(iface);

    TRACE("filter %p, count %p.\n", filter, count);

    *count = 1;

    return S_OK;
}

static HRESULT WINAPI basic_audio_GetTypeInfo(IBasicAudio *iface, UINT index,
        LCID lcid, ITypeInfo **typeinfo)
{
    struct dsound_render *filter = impl_from_IBasicAudio(iface);

    TRACE("filter %p, index %u, lcid %lu, typeinfo %p.\n", filter, index, lcid, typeinfo);

    return strmbase_get_typeinfo(IBasicAudio_tid, typeinfo);
}

static HRESULT WINAPI basic_audio_GetIDsOfNames(IBasicAudio *iface, REFIID iid,
        LPOLESTR *names, UINT count, LCID lcid, DISPID *ids)
{
    struct dsound_render *filter = impl_from_IBasicAudio(iface);
    ITypeInfo *typeinfo;
    HRESULT hr;

    TRACE("filter %p, iid %s, names %p, count %u, lcid %lu, ids %p.\n",
            filter, debugstr_guid(iid), names, count, lcid, ids);

    if (SUCCEEDED(hr = strmbase_get_typeinfo(IBasicAudio_tid, &typeinfo)))
    {
        hr = ITypeInfo_GetIDsOfNames(typeinfo, names, count, ids);
        ITypeInfo_Release(typeinfo);
    }

    return hr;
}

static HRESULT WINAPI basic_audio_Invoke(IBasicAudio *iface, DISPID id, REFIID iid, LCID lcid,
        WORD flags, DISPPARAMS *params, VARIANT *result, EXCEPINFO *excepinfo, UINT *error_arg)
{
    ITypeInfo *typeinfo;
    HRESULT hr;

    TRACE("filter %p, id %ld, iid %s, lcid %lu, flags %u, params %p, result %p, excepinfo %p, error_arg %p.\n",
          iface, id, debugstr_guid(iid), lcid, flags, params, result, excepinfo, error_arg);

    if (SUCCEEDED(hr = strmbase_get_typeinfo(IBasicAudio_tid, &typeinfo)))
    {
        hr = ITypeInfo_Invoke(typeinfo, iface, id, flags, params, result, excepinfo, error_arg);
        ITypeInfo_Release(typeinfo);
    }

    return hr;
}

static HRESULT WINAPI basic_audio_put_Volume(IBasicAudio *iface, LONG volume)
{
    struct dsound_render *filter = impl_from_IBasicAudio(iface);

    TRACE("filter %p, volume %ld.\n", filter, volume);

    if (volume > DSBVOLUME_MAX || volume < DSBVOLUME_MIN)
        return E_INVALIDARG;

    if (filter->dsbuffer && FAILED(IDirectSoundBuffer_SetVolume(filter->dsbuffer, volume)))
        return E_FAIL;

    filter->volume = volume;
    return S_OK;
}

static HRESULT WINAPI basic_audio_get_Volume(IBasicAudio *iface, LONG *volume)
{
    struct dsound_render *filter = impl_from_IBasicAudio(iface);

    TRACE("filter %p, volume %p.\n", filter, volume);

    if (!volume)
        return E_POINTER;

    *volume = filter->volume;
    return S_OK;
}

static HRESULT WINAPI basic_audio_put_Balance(IBasicAudio *iface, LONG balance)
{
    struct dsound_render *filter = impl_from_IBasicAudio(iface);

    TRACE("filter %p, balance %ld.\n", filter, balance);

    if (balance < DSBPAN_LEFT || balance > DSBPAN_RIGHT)
        return E_INVALIDARG;

    if (filter->dsbuffer && FAILED(IDirectSoundBuffer_SetPan(filter->dsbuffer, balance)))
        return E_FAIL;

    filter->pan = balance;
    return S_OK;
}

static HRESULT WINAPI basic_audio_get_Balance(IBasicAudio *iface, LONG *balance)
{
    struct dsound_render *filter = impl_from_IBasicAudio(iface);

    TRACE("filter %p, balance %p.\n", filter, balance);

    if (!balance)
        return E_POINTER;

    *balance = filter->pan;
    return S_OK;
}

static const IBasicAudioVtbl basic_audio_vtbl =
{
    basic_audio_QueryInterface,
    basic_audio_AddRef,
    basic_audio_Release,
    basic_audio_GetTypeInfoCount,
    basic_audio_GetTypeInfo,
    basic_audio_GetIDsOfNames,
    basic_audio_Invoke,
    basic_audio_put_Volume,
    basic_audio_get_Volume,
    basic_audio_put_Balance,
    basic_audio_get_Balance,
};

static HRESULT WINAPI am_direct_sound_QueryInterface(IAMDirectSound *iface, REFIID riid, void **out)
{
    struct dsound_render *filter = impl_from_IAMDirectSound(iface);
    return IUnknown_QueryInterface(filter->filter.outer_unk, riid, out);
}

static ULONG WINAPI am_direct_sound_AddRef(IAMDirectSound *iface)
{
    struct dsound_render *filter = impl_from_IAMDirectSound(iface);
    return IUnknown_AddRef(filter->filter.outer_unk);
}

static ULONG WINAPI am_direct_sound_Release(IAMDirectSound *iface)
{
    struct dsound_render *filter = impl_from_IAMDirectSound(iface);
    return IUnknown_Release(filter->filter.outer_unk);
}

static HRESULT WINAPI am_direct_sound_GetDirectSoundInterface(IAMDirectSound *iface,  IDirectSound **ds)
{
    struct dsound_render *filter = impl_from_IAMDirectSound(iface);
    FIXME("filter %p, ds %p stub!\n", filter, ds);
    return E_NOTIMPL;
}

static HRESULT WINAPI am_direct_sound_GetPrimaryBufferInterface(IAMDirectSound *iface, IDirectSoundBuffer **buf)
{
    struct dsound_render *filter = impl_from_IAMDirectSound(iface);
    FIXME("filter %p, buf %p stub!\n", filter, buf);
    return E_NOTIMPL;
}

static HRESULT WINAPI am_direct_sound_GetSecondaryBufferInterface(IAMDirectSound *iface, IDirectSoundBuffer **buf)
{
    struct dsound_render *filter = impl_from_IAMDirectSound(iface);
    FIXME("filter %p, buf %p stub!\n", filter, buf);
    return E_NOTIMPL;
}

static HRESULT WINAPI am_direct_sound_ReleaseDirectSoundInterface(IAMDirectSound *iface, IDirectSound *ds)
{
    struct dsound_render *filter = impl_from_IAMDirectSound(iface);
    FIXME("filter %p, ds %p stub!\n", filter, ds);
    return E_NOTIMPL;
}

static HRESULT WINAPI am_direct_sound_ReleasePrimaryBufferInterface(IAMDirectSound *iface, IDirectSoundBuffer *buf)
{
    struct dsound_render *filter = impl_from_IAMDirectSound(iface);
    FIXME("filter %p, buf %p stub!\n", filter, buf);
    return E_NOTIMPL;
}

static HRESULT WINAPI am_direct_sound_ReleaseSecondaryBufferInterface(IAMDirectSound *iface, IDirectSoundBuffer *buf)
{
    struct dsound_render *filter = impl_from_IAMDirectSound(iface);
    FIXME("filter %p, buf %p stub!\n", filter, buf);
    return E_NOTIMPL;
}

static HRESULT WINAPI am_direct_sound_SetFocusWindow(IAMDirectSound *iface, HWND hwnd, BOOL bgaudible)
{
    struct dsound_render *filter = impl_from_IAMDirectSound(iface);
    FIXME("filter %p, hwnd %p, bgaudible %d stub!\n", filter, hwnd, bgaudible);
    return E_NOTIMPL;
}

static HRESULT WINAPI am_direct_sound_GetFocusWindow(IAMDirectSound *iface, HWND *hwnd, BOOL *bgaudible)
{
    struct dsound_render *filter = impl_from_IAMDirectSound(iface);
    FIXME("filter %p, hwnd %p, bgaudible %p stub!\n", filter, hwnd, bgaudible);
    return E_NOTIMPL;
}

static const IAMDirectSoundVtbl am_direct_sound_vtbl =
{
    am_direct_sound_QueryInterface,
    am_direct_sound_AddRef,
    am_direct_sound_Release,
    am_direct_sound_GetDirectSoundInterface,
    am_direct_sound_GetPrimaryBufferInterface,
    am_direct_sound_GetSecondaryBufferInterface,
    am_direct_sound_ReleaseDirectSoundInterface,
    am_direct_sound_ReleasePrimaryBufferInterface,
    am_direct_sound_ReleaseSecondaryBufferInterface,
    am_direct_sound_SetFocusWindow,
    am_direct_sound_GetFocusWindow,
};

static struct dsound_render *impl_from_IQualityControl(IQualityControl *iface)
{
    return CONTAINING_RECORD(iface, struct dsound_render, IQualityControl_iface);
}

static HRESULT WINAPI quality_control_QueryInterface(IQualityControl *iface,
        REFIID iid, void **out)
{
    struct dsound_render *filter = impl_from_IQualityControl(iface);
    return IUnknown_QueryInterface(filter->filter.outer_unk, iid, out);
}

static ULONG WINAPI quality_control_AddRef(IQualityControl *iface)
{
    struct dsound_render *filter = impl_from_IQualityControl(iface);
    return IUnknown_AddRef(filter->filter.outer_unk);
}

static ULONG WINAPI quality_control_Release(IQualityControl *iface)
{
    struct dsound_render *filter = impl_from_IQualityControl(iface);
    return IUnknown_Release(filter->filter.outer_unk);
}

static HRESULT WINAPI quality_control_Notify(IQualityControl *iface,
        IBaseFilter *sender, Quality q)
{
    struct dsound_render *filter = impl_from_IQualityControl(iface);
    FIXME("filter %p, sender %p, type %#x, proportion %ld, late %s, timestamp %s, stub!\n",
            filter, sender, q.Type, q.Proportion, debugstr_time(q.Late), debugstr_time(q.TimeStamp));
    return E_NOTIMPL;
}

static HRESULT WINAPI quality_control_SetSink(IQualityControl *iface, IQualityControl *sink)
{
    struct dsound_render *filter = impl_from_IQualityControl(iface);
    FIXME("filter %p, sink %p stub!\n", filter, sink);
    return E_NOTIMPL;
}

static const IQualityControlVtbl quality_control_vtbl =
{
    quality_control_QueryInterface,
    quality_control_AddRef,
    quality_control_Release,
    quality_control_Notify,
    quality_control_SetSink,
};

HRESULT dsound_render_create(IUnknown *outer, IUnknown **out)
{
    static const DSBUFFERDESC buffer_desc = {
        .dwSize = sizeof(DSBUFFERDESC),
        .dwFlags = DSBCAPS_PRIMARYBUFFER,
    };

    struct dsound_render *object;
    IDirectSoundBuffer *buffer;
    HRESULT hr;

    TRACE("outer %p, out %p.\n", outer, out);

    if (!(object = calloc(1, sizeof(*object))))
        return E_OUTOFMEMORY;

    strmbase_filter_init(&object->filter, outer, &CLSID_DSoundRender, &filter_ops);

    if (FAILED(hr = system_clock_create(&object->filter.IUnknown_inner, &object->system_clock)))
    {
        strmbase_filter_cleanup(&object->filter);
        free(object);
        return hr;
    }

    if (FAILED(hr = DirectSoundCreate8(NULL, &object->dsound, NULL)))
    {
        IUnknown_Release(object->system_clock);
        strmbase_filter_cleanup(&object->filter);
        free(object);
        return hr == DSERR_NODRIVER ? VFW_E_NO_AUDIO_HARDWARE : hr;
    }

    if (FAILED(hr = IDirectSound8_SetCooperativeLevel(object->dsound,
            GetDesktopWindow(), DSSCL_PRIORITY)))
    {
        IDirectSound8_Release(object->dsound);
        IUnknown_Release(object->system_clock);
        strmbase_filter_cleanup(&object->filter);
        free(object);
        return hr;
    }

    if (SUCCEEDED(hr = IDirectSound8_CreateSoundBuffer(object->dsound,
            &buffer_desc, &buffer, NULL)))
    {
        IDirectSoundBuffer_Play(buffer, 0, 0, DSBPLAY_LOOPING);
        IDirectSoundBuffer_Release(buffer);
    }

    strmbase_passthrough_init(&object->passthrough, (IUnknown *)&object->filter.IBaseFilter_iface);
    ISeekingPassThru_Init(&object->passthrough.ISeekingPassThru_iface, TRUE, &object->sink.pin.IPin_iface);

    strmbase_sink_init(&object->sink, &object->filter, L"Audio Input pin (rendered)", &sink_ops, NULL);
    object->stream_start = -1;

    object->state_event = CreateEventW(NULL, TRUE, TRUE, NULL);
    object->flush_event = CreateEventW(NULL, TRUE, TRUE, NULL);
    object->empty_event = CreateEventW(NULL, FALSE, FALSE, NULL);

    object->IBasicAudio_iface.lpVtbl = &basic_audio_vtbl;
    object->IAMDirectSound_iface.lpVtbl = &am_direct_sound_vtbl;
    object->IQualityControl_iface.lpVtbl = &quality_control_vtbl;

    TRACE("Created DirectSound renderer %p.\n", object);
    *out = &object->filter.IUnknown_inner;

    return S_OK;
}
