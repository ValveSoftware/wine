/*
 * Copyright 2026
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

#ifndef __WINE_PE_BUILD
#include "config.h"
#endif

#define COBJMACROS

#include <stdarg.h>
#include <stdio.h>
#include <wctype.h>

#include "windef.h"
#include "winbase.h"
#include "mfapi.h"
#include "mfidl.h"
#include "mferror.h"
#include "mf_private.h"

#if defined(HAVE_LINUX_VIDEODEV2_H) && !defined(__WINE_PE_BUILD)
#define MF_HAVE_V4L2 1
#endif

#ifdef MF_HAVE_V4L2
#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <unistd.h>
#endif

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(mfplat);

struct video_capture_context
{
    WCHAR *symbolic_link;
};

enum video_source_state
{
    VIDEO_SOURCE_STOPPED,
    VIDEO_SOURCE_RUNNING,
    VIDEO_SOURCE_SHUTDOWN,
};

struct video_capture_source
{
    IMFMediaSource IMFMediaSource_iface;
    LONG refcount;
    CRITICAL_SECTION cs;
    IMFMediaEventQueue *event_queue;
    IMFStreamDescriptor *stream_descriptor;
    WCHAR *symbolic_link;
    int fd;
    enum video_source_state state;
};

static inline struct video_capture_source *impl_from_IMFMediaSource(IMFMediaSource *iface)
{
    return CONTAINING_RECORD(iface, struct video_capture_source, IMFMediaSource_iface);
}

static HRESULT set_attribute_size(IMFAttributes *attributes, REFGUID key, UINT32 width, UINT32 height)
{
    return IMFAttributes_SetUINT64(attributes, key, ((UINT64)width << 32) | height);
}

static HRESULT set_attribute_ratio(IMFAttributes *attributes, REFGUID key, UINT32 numerator, UINT32 denominator)
{
    return IMFAttributes_SetUINT64(attributes, key, ((UINT64)numerator << 32) | denominator);
}

static WCHAR *wcsdup_heap(const WCHAR *src)
{
    WCHAR *dst;
    SIZE_T len;

    if (!src)
        return NULL;

    len = lstrlenW(src) + 1;
    if (!(dst = malloc(len * sizeof(*dst))))
        return NULL;

    memcpy(dst, src, len * sizeof(*dst));
    return dst;
}

static HRESULT parse_symbolic_link(const WCHAR *symbolic_link, WCHAR **ret_path)
{
    const WCHAR *digits_start;
    UINT index;
    WCHAR path[32];

    if (!symbolic_link || !ret_path)
        return E_INVALIDARG;

    if (!wcsncmp(symbolic_link, L"/dev/video", 10))
    {
        if (!(*ret_path = wcsdup_heap(symbolic_link)))
            return E_OUTOFMEMORY;
        return S_OK;
    }

    digits_start = symbolic_link + lstrlenW(symbolic_link);
    while (digits_start > symbolic_link && iswdigit(digits_start[-1]))
        --digits_start;

    if (digits_start == symbolic_link || !*digits_start)
    {
        WARN("Unrecognized video symbolic link format %s.\n", wine_dbgstr_w(symbolic_link));
        return MF_E_NOT_FOUND;
    }

    index = wcstoul(digits_start, NULL, 10);
    swprintf(path, ARRAY_SIZE(path), L"/dev/video%u", index);

    if (!(*ret_path = wcsdup_heap(path)))
        return E_OUTOFMEMORY;

    return S_OK;
}

static HRESULT open_capture_fd(struct video_capture_source *source)
{
#ifdef MF_HAVE_V4L2
    char path[64];

    if (source->fd >= 0)
        return S_OK;

    if (!WideCharToMultiByte(CP_UTF8, 0, source->symbolic_link, -1, path, sizeof(path), NULL, NULL))
        return E_FAIL;

#ifdef O_CLOEXEC
    if ((source->fd = open(path, O_RDWR | O_NONBLOCK | O_CLOEXEC)) < 0 && errno == EINVAL)
#endif
        source->fd = open(path, O_RDWR | O_NONBLOCK);

    if (source->fd < 0)
#ifdef O_CLOEXEC
        if ((source->fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC)) < 0 && errno == EINVAL)
#endif
            source->fd = open(path, O_RDONLY | O_NONBLOCK);

    if (source->fd < 0)
    {
        WARN("Failed to open %s (errno %d).\n", path, errno);
        return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
    }

#ifdef O_CLOEXEC
    fcntl(source->fd, F_SETFD, FD_CLOEXEC);
#endif
    return S_OK;
#else
    WARN("V4L2 is not available in this build.\n");
    return E_NOTIMPL;
#endif
}

static void close_capture_fd(struct video_capture_source *source)
{
#ifdef MF_HAVE_V4L2
    if (source->fd >= 0)
    {
        close(source->fd);
        source->fd = -1;
    }
#endif
}

static HRESULT create_stream_descriptor(IMFStreamDescriptor **ret_descriptor)
{
    IMFMediaType *media_type;
    IMFMediaTypeHandler *handler;
    IMFStreamDescriptor *descriptor;
    HRESULT hr;

    *ret_descriptor = NULL;

    if (FAILED(hr = MFCreateMediaType(&media_type)))
        return hr;

    hr = IMFMediaType_SetGUID(media_type, &MF_MT_MAJOR_TYPE, &MFMediaType_Video);
    if (SUCCEEDED(hr))
        hr = IMFMediaType_SetGUID(media_type, &MF_MT_SUBTYPE, &MFVideoFormat_RGB24);
    if (SUCCEEDED(hr))
        hr = set_attribute_size((IMFAttributes *)media_type, &MF_MT_FRAME_SIZE, 640, 480);
    if (SUCCEEDED(hr))
        hr = set_attribute_ratio((IMFAttributes *)media_type, &MF_MT_FRAME_RATE, 30, 1);
    if (SUCCEEDED(hr))
        hr = set_attribute_ratio((IMFAttributes *)media_type, &MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
    if (SUCCEEDED(hr))
        hr = IMFMediaType_SetUINT32(media_type, &MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    if (SUCCEEDED(hr))
        hr = IMFMediaType_SetUINT32(media_type, &MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);

    if (SUCCEEDED(hr))
        hr = MFCreateStreamDescriptor(1, 1, &media_type, &descriptor);

    if (SUCCEEDED(hr))
    {
        if (FAILED(hr = IMFStreamDescriptor_GetMediaTypeHandler(descriptor, &handler)))
        {
            IMFStreamDescriptor_Release(descriptor);
        }
        else
        {
            if (FAILED(hr = IMFMediaTypeHandler_SetCurrentMediaType(handler, media_type)))
                IMFStreamDescriptor_Release(descriptor);
            else
                *ret_descriptor = descriptor;
            IMFMediaTypeHandler_Release(handler);
        }
    }

    IMFMediaType_Release(media_type);
    return hr;
}

static HRESULT WINAPI video_capture_source_QueryInterface(IMFMediaSource *iface, REFIID riid, void **obj)
{
    TRACE("%p, %s, %p.\n", iface, debugstr_guid(riid), obj);

    if (IsEqualIID(riid, &IID_IMFMediaSource) ||
            IsEqualIID(riid, &IID_IMFMediaEventGenerator) ||
            IsEqualIID(riid, &IID_IUnknown))
    {
        *obj = iface;
        IMFMediaSource_AddRef(iface);
        return S_OK;
    }

    *obj = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI video_capture_source_AddRef(IMFMediaSource *iface)
{
    struct video_capture_source *source = impl_from_IMFMediaSource(iface);
    return InterlockedIncrement(&source->refcount);
}

static ULONG WINAPI video_capture_source_Release(IMFMediaSource *iface)
{
    struct video_capture_source *source = impl_from_IMFMediaSource(iface);
    ULONG refcount = InterlockedDecrement(&source->refcount);

    if (!refcount)
    {
        close_capture_fd(source);
        if (source->event_queue)
            IMFMediaEventQueue_Release(source->event_queue);
        if (source->stream_descriptor)
            IMFStreamDescriptor_Release(source->stream_descriptor);
        free(source->symbolic_link);
        source->cs.DebugInfo->Spare[0] = 0;
        DeleteCriticalSection(&source->cs);
        free(source);
    }

    return refcount;
}

static HRESULT WINAPI video_capture_source_GetEvent(IMFMediaSource *iface, DWORD flags, IMFMediaEvent **event)
{
    struct video_capture_source *source = impl_from_IMFMediaSource(iface);
    return IMFMediaEventQueue_GetEvent(source->event_queue, flags, event);
}

static HRESULT WINAPI video_capture_source_BeginGetEvent(IMFMediaSource *iface, IMFAsyncCallback *callback, IUnknown *state)
{
    struct video_capture_source *source = impl_from_IMFMediaSource(iface);
    return IMFMediaEventQueue_BeginGetEvent(source->event_queue, callback, state);
}

static HRESULT WINAPI video_capture_source_EndGetEvent(IMFMediaSource *iface, IMFAsyncResult *result, IMFMediaEvent **event)
{
    struct video_capture_source *source = impl_from_IMFMediaSource(iface);
    return IMFMediaEventQueue_EndGetEvent(source->event_queue, result, event);
}

static HRESULT WINAPI video_capture_source_QueueEvent(IMFMediaSource *iface, MediaEventType event_type, REFGUID ext_type,
        HRESULT status, const PROPVARIANT *value)
{
    struct video_capture_source *source = impl_from_IMFMediaSource(iface);

    return IMFMediaEventQueue_QueueEventParamVar(source->event_queue, event_type, ext_type, status, value);
}

static HRESULT WINAPI video_capture_source_GetCharacteristics(IMFMediaSource *iface, DWORD *characteristics)
{
    struct video_capture_source *source = impl_from_IMFMediaSource(iface);
    HRESULT hr = S_OK;

    if (!characteristics)
        return E_POINTER;

    EnterCriticalSection(&source->cs);

    if (source->state == VIDEO_SOURCE_SHUTDOWN)
        hr = MF_E_SHUTDOWN;
    else
        *characteristics = MFMEDIASOURCE_CAN_PAUSE;

    LeaveCriticalSection(&source->cs);

    return hr;
}

static HRESULT WINAPI video_capture_source_CreatePresentationDescriptor(IMFMediaSource *iface,
        IMFPresentationDescriptor **descriptor)
{
    struct video_capture_source *source = impl_from_IMFMediaSource(iface);
    HRESULT hr;

    if (!descriptor)
        return E_POINTER;

    EnterCriticalSection(&source->cs);

    if (source->state == VIDEO_SOURCE_SHUTDOWN)
        hr = MF_E_SHUTDOWN;
    else if (SUCCEEDED(hr = MFCreatePresentationDescriptor(1, &source->stream_descriptor, descriptor)))
        hr = IMFPresentationDescriptor_SelectStream(*descriptor, 0);

    LeaveCriticalSection(&source->cs);

    return hr;
}

static HRESULT WINAPI video_capture_source_Start(IMFMediaSource *iface, IMFPresentationDescriptor *descriptor,
        const GUID *time_format, const PROPVARIANT *position)
{
    struct video_capture_source *source = impl_from_IMFMediaSource(iface);
    HRESULT hr = S_OK;

    TRACE("%p, %p, %p, %p.\n", iface, descriptor, time_format, position);

    EnterCriticalSection(&source->cs);

    if (source->state == VIDEO_SOURCE_SHUTDOWN)
        hr = MF_E_SHUTDOWN;
    else if (time_format && !IsEqualGUID(time_format, &GUID_NULL))
        hr = MF_E_UNSUPPORTED_TIME_FORMAT;
    else if (SUCCEEDED(hr = open_capture_fd(source)))
    {
        source->state = VIDEO_SOURCE_RUNNING;
        hr = IMFMediaEventQueue_QueueEventParamVar(source->event_queue, MESourceStarted, &GUID_NULL, S_OK, position);
    }

    LeaveCriticalSection(&source->cs);

    return hr;
}

static HRESULT WINAPI video_capture_source_Stop(IMFMediaSource *iface)
{
    struct video_capture_source *source = impl_from_IMFMediaSource(iface);
    HRESULT hr = S_OK;

    EnterCriticalSection(&source->cs);

    if (source->state == VIDEO_SOURCE_SHUTDOWN)
        hr = MF_E_SHUTDOWN;
    else
    {
        close_capture_fd(source);
        source->state = VIDEO_SOURCE_STOPPED;
        hr = IMFMediaEventQueue_QueueEventParamVar(source->event_queue, MESourceStopped, &GUID_NULL, S_OK, NULL);
    }

    LeaveCriticalSection(&source->cs);

    return hr;
}

static HRESULT WINAPI video_capture_source_Pause(IMFMediaSource *iface)
{
    struct video_capture_source *source = impl_from_IMFMediaSource(iface);
    HRESULT hr = S_OK;

    EnterCriticalSection(&source->cs);

    if (source->state == VIDEO_SOURCE_SHUTDOWN)
        hr = MF_E_SHUTDOWN;
    else if (source->state != VIDEO_SOURCE_RUNNING)
        hr = MF_E_INVALID_STATE_TRANSITION;
    else
    {
        source->state = VIDEO_SOURCE_STOPPED;
        hr = IMFMediaEventQueue_QueueEventParamVar(source->event_queue, MESourcePaused, &GUID_NULL, S_OK, NULL);
    }

    LeaveCriticalSection(&source->cs);

    return hr;
}

static HRESULT WINAPI video_capture_source_Shutdown(IMFMediaSource *iface)
{
    struct video_capture_source *source = impl_from_IMFMediaSource(iface);
    HRESULT hr;

    EnterCriticalSection(&source->cs);

    if (source->state == VIDEO_SOURCE_SHUTDOWN)
    {
        LeaveCriticalSection(&source->cs);
        return MF_E_SHUTDOWN;
    }

    close_capture_fd(source);
    source->state = VIDEO_SOURCE_SHUTDOWN;

    IMFMediaEventQueue_QueueEventParamVar(source->event_queue, MEError, &GUID_NULL, MF_E_SHUTDOWN, NULL);
    hr = IMFMediaEventQueue_Shutdown(source->event_queue);

    LeaveCriticalSection(&source->cs);

    return hr;
}

static const IMFMediaSourceVtbl video_capture_source_vtbl =
{
    video_capture_source_QueryInterface,
    video_capture_source_AddRef,
    video_capture_source_Release,
    video_capture_source_GetEvent,
    video_capture_source_BeginGetEvent,
    video_capture_source_EndGetEvent,
    video_capture_source_QueueEvent,
    video_capture_source_GetCharacteristics,
    video_capture_source_CreatePresentationDescriptor,
    video_capture_source_Start,
    video_capture_source_Stop,
    video_capture_source_Pause,
    video_capture_source_Shutdown,
};

static HRESULT create_video_capture_source(const WCHAR *symbolic_link, IMFMediaSource **ret_source)
{
    struct video_capture_source *source;
    HRESULT hr;

    *ret_source = NULL;

    if (!(source = calloc(1, sizeof(*source))))
        return E_OUTOFMEMORY;

    source->IMFMediaSource_iface.lpVtbl = &video_capture_source_vtbl;
    source->refcount = 1;
    source->fd = -1;
    source->state = VIDEO_SOURCE_STOPPED;
    InitializeCriticalSectionEx(&source->cs, 0, RTL_CRITICAL_SECTION_FLAG_FORCE_DEBUG_INFO);
    source->cs.DebugInfo->Spare[0] = (DWORD_PTR)(__FILE__ ": video_capture_source.cs");

    if (!(source->symbolic_link = wcsdup_heap(symbolic_link)))
    {
        hr = E_OUTOFMEMORY;
        goto failed;
    }

    if (FAILED(hr = create_stream_descriptor(&source->stream_descriptor)))
        goto failed;

    if (FAILED(hr = MFCreateEventQueue(&source->event_queue)))
        goto failed;

    *ret_source = &source->IMFMediaSource_iface;
    return S_OK;

failed:
    IMFMediaSource_Release(&source->IMFMediaSource_iface);
    return hr;
}

static HRESULT video_capture_create_object(IMFAttributes *attributes, void *user_context, IUnknown **obj)
{
    struct video_capture_context *context = user_context;
    WCHAR *symbolic_link = NULL, *unix_path = NULL;
    UINT32 len = 0;
    IMFMediaSource *source = NULL;
    HRESULT hr;

    TRACE("%p, %p, %p.\n", attributes, user_context, obj);

    if (!attributes || !obj)
        return E_INVALIDARG;

    if (FAILED(hr = IMFAttributes_GetAllocatedString(attributes,
            &MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK, &symbolic_link, &len)))
    {
        if (!context || !context->symbolic_link)
        {
            WARN("Failed to get video symbolic link, hr %#lx.\n", hr);
            return hr;
        }

        if (!(symbolic_link = wcsdup_heap(context->symbolic_link)))
            return E_OUTOFMEMORY;
    }

    if (FAILED(hr = parse_symbolic_link(symbolic_link, &unix_path)))
        goto done;

    hr = create_video_capture_source(unix_path, &source);
    if (SUCCEEDED(hr))
    {
        *obj = (IUnknown *)source;
        source = NULL;
    }

done:
    if (len)
        CoTaskMemFree(symbolic_link);
    else
        free(symbolic_link);
    free(unix_path);
    if (source)
        IMFMediaSource_Release(source);
    return hr;
}

static void video_capture_shutdown_object(void *user_context, IUnknown *obj)
{
    IMFMediaSource *source;

    TRACE("%p %p.\n", user_context, obj);

    if (obj && SUCCEEDED(IUnknown_QueryInterface(obj, &IID_IMFMediaSource, (void **)&source)))
    {
        IMFMediaSource_Shutdown(source);
        IMFMediaSource_Release(source);
    }
}

static void video_capture_free_private(void *user_context)
{
    struct video_capture_context *context = user_context;

    TRACE("%p.\n", user_context);

    if (!context)
        return;

    free(context->symbolic_link);
    free(context);
}

static const struct activate_funcs video_capture_activate_funcs =
{
    video_capture_create_object,
    video_capture_shutdown_object,
    video_capture_free_private,
};

HRESULT enum_video_capture_sources(IMFAttributes *attributes, IMFActivate ***ret_sources, UINT32 *ret_count)
{
    IMFActivate **sources = NULL;
    UINT32 count = 0, i;
    HRESULT hr = S_OK;

    TRACE("%p, %p, %p.\n", attributes, ret_sources, ret_count);

    if (!attributes || !ret_sources || !ret_count)
        return E_INVALIDARG;

    *ret_sources = NULL;
    *ret_count = 0;

    if (!(sources = CoTaskMemAlloc(10 * sizeof(*sources))))
        return E_OUTOFMEMORY;

    for (i = 0; i < 10; ++i)
    {
        struct video_capture_context *context;
#ifdef MF_HAVE_V4L2
        struct v4l2_capability caps = {{0}};
        WCHAR friendly[64];
        char path[32];
        int fd;

        sprintf(path, "/dev/video%u", i);
#ifdef O_CLOEXEC
        if ((fd = open(path, O_RDWR | O_NONBLOCK | O_CLOEXEC)) < 0 && errno == EINVAL)
#endif
            fd = open(path, O_RDWR | O_NONBLOCK);

        if (fd < 0)
#ifdef O_CLOEXEC
            if ((fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC)) < 0 && errno == EINVAL)
#endif
                fd = open(path, O_RDONLY | O_NONBLOCK);

        if (fd < 0)
            continue;

        if (ioctl(fd, VIDIOC_QUERYCAP, &caps) < 0)
        {
            close(fd);
            continue;
        }

        close(fd);

#ifdef V4L2_CAP_DEVICE_CAPS
        if (caps.capabilities & V4L2_CAP_DEVICE_CAPS)
            caps.capabilities = caps.device_caps;
#endif
        if (!(caps.capabilities & V4L2_CAP_VIDEO_CAPTURE))
            continue;

        if (!(context = calloc(1, sizeof(*context))))
        {
            hr = E_OUTOFMEMORY;
            break;
        }

        swprintf(friendly, ARRAY_SIZE(friendly), L"%S", caps.card);
#else
        WCHAR friendly[64];

        if (!(context = calloc(1, sizeof(*context))))
        {
            hr = E_OUTOFMEMORY;
            break;
        }

        swprintf(friendly, ARRAY_SIZE(friendly), L"Video Capture %u", i);
#endif

        if (!(context->symbolic_link = malloc(32 * sizeof(WCHAR))))
        {
            free(context);
            hr = E_OUTOFMEMORY;
            break;
        }

        swprintf(context->symbolic_link, 32, L"/dev/video%u", i);

        if (FAILED(hr = create_activation_object(context, &video_capture_activate_funcs, &sources[count])))
        {
            video_capture_free_private(context);
            break;
        }
        ++count;

        if (FAILED(hr = IMFActivate_SetGUID(sources[count - 1], &MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
                &MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID)))
            break;

        if (FAILED(hr = IMFActivate_SetString(sources[count - 1], &MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK,
                context->symbolic_link)))
            break;

        if (FAILED(hr = IMFActivate_SetString(sources[count - 1], &MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, friendly)))
            break;
    }

    if (SUCCEEDED(hr))
    {
        if (!count)
        {
            CoTaskMemFree(sources);
            sources = NULL;
        }

        *ret_sources = sources;
        *ret_count = count;
        return S_OK;
    }

    for (i = 0; i < count; ++i)
        IMFActivate_Release(sources[i]);
    CoTaskMemFree(sources);

    *ret_sources = NULL;
    *ret_count = 0;
    return hr;
}
