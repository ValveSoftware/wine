/*
 * Copyright 2016 Matteo Bruni for CodeWeavers
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
#include "d3dx11.h"
#include "d3dcompiler.h"
#include "dxhelpers.h"
#include "winternl.h"

#include "wine/debug.h"
#include "wine/list.h"

WINE_DEFAULT_DEBUG_CHANNEL(d3dx);

struct asyncdataloader
{
    ID3DX11DataLoader ID3DX11DataLoader_iface;

    union
    {
        struct
        {
            WCHAR *path;
        } file;
        struct
        {
            HMODULE module;
            HRSRC rsrc;
        } resource;
    } u;
    void *data;
    DWORD size;
};

static inline struct asyncdataloader *impl_from_ID3DX11DataLoader(ID3DX11DataLoader *iface)
{
    return CONTAINING_RECORD(iface, struct asyncdataloader, ID3DX11DataLoader_iface);
}

static HRESULT WINAPI memorydataloader_Load(ID3DX11DataLoader *iface)
{
    TRACE("iface %p.\n", iface);
    return S_OK;
}

static HRESULT WINAPI memorydataloader_Decompress(ID3DX11DataLoader *iface, void **data, SIZE_T *size)
{
    struct asyncdataloader *loader = impl_from_ID3DX11DataLoader(iface);

    TRACE("iface %p, data %p, size %p.\n", iface, data, size);

    *data = loader->data;
    *size = loader->size;

    return S_OK;
}

static HRESULT WINAPI memorydataloader_Destroy(ID3DX11DataLoader *iface)
{
    struct asyncdataloader *loader = impl_from_ID3DX11DataLoader(iface);

    TRACE("iface %p.\n", iface);

    free(loader);
    return S_OK;
}

static const ID3DX11DataLoaderVtbl memorydataloadervtbl =
{
    memorydataloader_Load,
    memorydataloader_Decompress,
    memorydataloader_Destroy
};

HRESULT load_file(const WCHAR *path, void **data, DWORD *size)
{
    HRESULT hr;

    hr = d3dx_load_file(path, data, size);
    return (hr == D3DX_HELPER_ERR_FILE_NOT_FOUND) ? D3D11_ERROR_FILE_NOT_FOUND : hr;
}

static HRESULT WINAPI filedataloader_Load(ID3DX11DataLoader *iface)
{
    struct asyncdataloader *loader = impl_from_ID3DX11DataLoader(iface);
    void *data;
    DWORD size;
    HRESULT hr;

    TRACE("iface %p.\n", iface);

    /* Always buffer file contents, even if Load() was already called. */
    if (FAILED((hr = load_file(loader->u.file.path, &data, &size))))
        return hr;

    free(loader->data);
    loader->data = data;
    loader->size = size;

    return S_OK;
}

static HRESULT WINAPI filedataloader_Decompress(ID3DX11DataLoader *iface, void **data, SIZE_T *size)
{
    struct asyncdataloader *loader = impl_from_ID3DX11DataLoader(iface);

    TRACE("iface %p, data %p, size %p.\n", iface, data, size);

    if (!loader->data)
        return E_FAIL;

    *data = loader->data;
    *size = loader->size;

    return S_OK;
}

static HRESULT WINAPI filedataloader_Destroy(ID3DX11DataLoader *iface)
{
    struct asyncdataloader *loader = impl_from_ID3DX11DataLoader(iface);

    TRACE("iface %p.\n", iface);

    free(loader->u.file.path);
    free(loader->data);
    free(loader);

    return S_OK;
}

static const ID3DX11DataLoaderVtbl filedataloadervtbl =
{
    filedataloader_Load,
    filedataloader_Decompress,
    filedataloader_Destroy
};

static HRESULT load_resource_initA(HMODULE module, const char *resource, HRSRC *rsrc)
{
    HRESULT hr = d3dx_load_resource_initA(module, resource, rsrc);
    return (hr == D3DX_HELPER_ERR_INVALID_DATA) ? D3DX11_ERR_INVALID_DATA : hr;
}

static HRESULT load_resource_initW(HMODULE module, const WCHAR *resource, HRSRC *rsrc)
{
    HRESULT hr = d3dx_load_resource_initW(module, resource, rsrc);
    return (hr == D3DX_HELPER_ERR_INVALID_DATA) ? D3DX11_ERR_INVALID_DATA : hr;
}

static HRESULT load_resource(HMODULE module, HRSRC rsrc, void **data, DWORD *size)
{
    HRESULT hr = d3dx_load_resource(module, rsrc, data, size);
    return (hr == D3DX_HELPER_ERR_INVALID_DATA) ? D3DX11_ERR_INVALID_DATA : hr;
}

HRESULT load_resourceA(HMODULE module, const char *resource, void **data, DWORD *size)
{
    HRESULT hr;
    HRSRC rsrc;

    if (FAILED((hr = load_resource_initA(module, resource, &rsrc))))
        return hr;
    return load_resource(module, rsrc, data, size);
}

HRESULT load_resourceW(HMODULE module, const WCHAR *resource, void **data, DWORD *size)
{
    HRESULT hr;
    HRSRC rsrc;

    if ((FAILED(hr = load_resource_initW(module, resource, &rsrc))))
        return hr;
    return load_resource(module, rsrc, data, size);
}

static HRESULT WINAPI resourcedataloader_Load(ID3DX11DataLoader *iface)
{
    struct asyncdataloader *loader = impl_from_ID3DX11DataLoader(iface);

    TRACE("iface %p.\n", iface);

    if (loader->data)
        return S_OK;

    return load_resource(loader->u.resource.module, loader->u.resource.rsrc,
            &loader->data, &loader->size);
}

static HRESULT WINAPI resourcedataloader_Decompress(ID3DX11DataLoader *iface, void **data, SIZE_T *size)
{
    struct asyncdataloader *loader = impl_from_ID3DX11DataLoader(iface);

    TRACE("iface %p, data %p, size %p.\n", iface, data, size);

    if (!loader->data)
        return E_FAIL;

    *data = loader->data;
    *size = loader->size;

    return S_OK;
}

static HRESULT WINAPI resourcedataloader_Destroy(ID3DX11DataLoader *iface)
{
    struct asyncdataloader *loader = impl_from_ID3DX11DataLoader(iface);

    TRACE("iface %p.\n", iface);

    free(loader);

    return S_OK;
}

static const ID3DX11DataLoaderVtbl resourcedataloadervtbl =
{
    resourcedataloader_Load,
    resourcedataloader_Decompress,
    resourcedataloader_Destroy
};

struct texture_info_processor
{
    ID3DX11DataProcessor ID3DX11DataProcessor_iface;
    D3DX11_IMAGE_INFO *info;
};

static inline struct texture_info_processor *impl_from_ID3DX11DataProcessor(ID3DX11DataProcessor *iface)
{
    return CONTAINING_RECORD(iface, struct texture_info_processor, ID3DX11DataProcessor_iface);
}

static HRESULT WINAPI texture_info_processor_Process(ID3DX11DataProcessor *iface, void *data, SIZE_T size)
{
    struct texture_info_processor *processor = impl_from_ID3DX11DataProcessor(iface);

    TRACE("iface %p, data %p, size %Iu.\n", iface, data, size);
    return get_image_info(data, size, processor->info);
}

static HRESULT WINAPI texture_info_processor_CreateDeviceObject(ID3DX11DataProcessor *iface, void **object)
{
    TRACE("iface %p, object %p.\n", iface, object);
    return S_OK;
}

static HRESULT WINAPI texture_info_processor_Destroy(ID3DX11DataProcessor *iface)
{
    struct texture_info_processor *processor = impl_from_ID3DX11DataProcessor(iface);

    TRACE("iface %p.\n", iface);

    free(processor);
    return S_OK;
}

static ID3DX11DataProcessorVtbl texture_info_processor_vtbl =
{
    texture_info_processor_Process,
    texture_info_processor_CreateDeviceObject,
    texture_info_processor_Destroy
};

struct texture_processor
{
    ID3DX11DataProcessor ID3DX11DataProcessor_iface;
    ID3D11Device *device;
    D3DX11_IMAGE_INFO img_info;
    D3DX11_IMAGE_LOAD_INFO load_info;
    D3D11_SUBRESOURCE_DATA *resource_data;
};

static inline struct texture_processor *texture_processor_from_ID3DX11DataProcessor(ID3DX11DataProcessor *iface)
{
    return CONTAINING_RECORD(iface, struct texture_processor, ID3DX11DataProcessor_iface);
}

static HRESULT WINAPI texture_processor_Process(ID3DX11DataProcessor *iface, void *data, SIZE_T size)
{
    struct texture_processor *processor = texture_processor_from_ID3DX11DataProcessor(iface);

    TRACE("iface %p, data %p, size %Iu.\n", iface, data, size);

    if (processor->resource_data)
    {
        WARN("Called multiple times.\n");
        free(processor->resource_data);
        processor->resource_data = NULL;
    }
    return load_texture_data(data, size, &processor->load_info, &processor->resource_data);
}

static HRESULT WINAPI texture_processor_CreateDeviceObject(ID3DX11DataProcessor *iface, void **object)
{
    struct texture_processor *processor = texture_processor_from_ID3DX11DataProcessor(iface);

    TRACE("iface %p, object %p.\n", iface, object);

    if (!processor->resource_data)
        return E_FAIL;

    return create_d3d_texture(processor->device, &processor->load_info,
            processor->resource_data, (ID3D11Resource **)object);
}

static HRESULT WINAPI texture_processor_Destroy(ID3DX11DataProcessor *iface)
{
    struct texture_processor *processor = texture_processor_from_ID3DX11DataProcessor(iface);

    TRACE("iface %p.\n", iface);

    ID3D11Device_Release(processor->device);
    free(processor->resource_data);
    free(processor);
    return S_OK;
}

static ID3DX11DataProcessorVtbl texture_processor_vtbl =
{
    texture_processor_Process,
    texture_processor_CreateDeviceObject,
    texture_processor_Destroy
};

struct srv_processor
{
    ID3DX11DataProcessor ID3DX11DataProcessor_iface;
    ID3DX11DataProcessor *texture_processor;
};

static inline struct srv_processor *srv_processor_from_ID3DX11DataProcessor(ID3DX11DataProcessor *iface)
{
    return CONTAINING_RECORD(iface, struct srv_processor, ID3DX11DataProcessor_iface);
}

static HRESULT WINAPI srv_processor_Process(ID3DX11DataProcessor *iface, void *data, SIZE_T size)
{
    struct srv_processor *processor = srv_processor_from_ID3DX11DataProcessor(iface);

    TRACE("iface %p, data %p, size %Iu.\n", iface, data, size);

    return ID3DX11DataProcessor_Process(processor->texture_processor, data, size);
}

static HRESULT WINAPI srv_processor_CreateDeviceObject(ID3DX11DataProcessor *iface, void **object)
{
    struct srv_processor *processor = srv_processor_from_ID3DX11DataProcessor(iface);
    struct texture_processor *tex_processor = texture_processor_from_ID3DX11DataProcessor(processor->texture_processor);
    ID3D11Resource *texture_resource;
    HRESULT hr;

    TRACE("iface %p, object %p.\n", iface, object);

    hr = ID3DX11DataProcessor_CreateDeviceObject(processor->texture_processor, (void **)&texture_resource);
    if (FAILED(hr))
        return hr;

    hr = ID3D11Device_CreateShaderResourceView(tex_processor->device, texture_resource, NULL,
            (ID3D11ShaderResourceView **)object);
    ID3D11Resource_Release(texture_resource);
    return hr;
}

static HRESULT WINAPI srv_processor_Destroy(ID3DX11DataProcessor *iface)
{
    struct srv_processor *processor = srv_processor_from_ID3DX11DataProcessor(iface);

    TRACE("iface %p.\n", iface);

    ID3DX11DataProcessor_Destroy(processor->texture_processor);
    free(processor);
    return S_OK;
}

static ID3DX11DataProcessorVtbl srv_processor_vtbl =
{
    srv_processor_Process,
    srv_processor_CreateDeviceObject,
    srv_processor_Destroy
};

HRESULT WINAPI D3DX11CompileFromMemory(const char *data, SIZE_T data_size, const char *filename,
        const D3D10_SHADER_MACRO *defines, ID3D10Include *include, const char *entry_point,
        const char *target, UINT sflags, UINT eflags, ID3DX11ThreadPump *pump, ID3D10Blob **shader,
        ID3D10Blob **error_messages, HRESULT *hresult)
{
    TRACE("data %s, data_size %Iu, filename %s, defines %p, include %p, entry_point %s, target %s, "
            "sflags %#x, eflags %#x, pump %p, shader %p, error_messages %p, hresult %p.\n",
            debugstr_an(data, data_size), data_size, debugstr_a(filename), defines, include,
            debugstr_a(entry_point), debugstr_a(target), sflags, eflags, pump, shader,
            error_messages, hresult);

    if (pump)
        FIXME("Unimplemented ID3DX11ThreadPump handling.\n");

    return D3DCompile(data, data_size, filename, defines, include, entry_point, target,
            sflags, eflags, shader, error_messages);
}

HRESULT WINAPI D3DX11CompileFromFileA(const char *filename, const D3D10_SHADER_MACRO *defines,
        ID3D10Include *include, const char *entry_point, const char *target, UINT sflags, UINT eflags,
        ID3DX11ThreadPump *pump, ID3D10Blob **shader, ID3D10Blob **error_messages, HRESULT *hresult)
{
    WCHAR filename_w[MAX_PATH];

    TRACE("filename %s, defines %p, include %p, entry_point %s, target %s, sflags %#x, "
            "eflags %#x, pump %p, shader %p, error_messages %p, hresult %p.\n",
            debugstr_a(filename), defines, include, debugstr_a(entry_point), debugstr_a(target),
            sflags, eflags, pump, shader, error_messages, hresult);

    MultiByteToWideChar(CP_ACP, 0, filename, -1, filename_w, ARRAY_SIZE(filename_w));

    return D3DX11CompileFromFileW(filename_w, defines, include, entry_point, target,
            sflags, eflags, pump, shader, error_messages, hresult);
}

HRESULT WINAPI D3DX11CompileFromFileW(const WCHAR *filename, const D3D10_SHADER_MACRO *defines,
        ID3D10Include *include, const char *entry_point, const char *target, UINT sflags, UINT eflags,
        ID3DX11ThreadPump *pump, ID3D10Blob **shader, ID3D10Blob **error_messages, HRESULT *hresult)
{
    HRESULT hr;

    TRACE("filename %s, defines %p, include %p, entry_point %s, target %s, sflags %#x, "
            "eflags %#x, pump %p, shader %p, error_messages %p, hresult %p.\n",
            debugstr_w(filename), defines, include, debugstr_a(entry_point), debugstr_a(target),
            sflags, eflags, pump, shader, error_messages, hresult);

    if (pump)
        FIXME("Unimplemented ID3DX11ThreadPump handling.\n");

    if (!include)
        include = D3D_COMPILE_STANDARD_FILE_INCLUDE;

    hr = D3DCompileFromFile(filename, defines, include, entry_point, target, sflags, eflags, shader, error_messages);
    if (hresult)
        *hresult = hr;

    return hr;
}

HRESULT WINAPI D3DX11CreateAsyncMemoryLoader(const void *data, SIZE_T data_size, ID3DX11DataLoader **loader)
{
    struct asyncdataloader *object;

    TRACE("data %p, data_size %Iu, loader %p.\n", data, data_size, loader);

    if (!data || !loader)
        return E_FAIL;

    object = calloc(1, sizeof(*object));
    if (!object)
        return E_OUTOFMEMORY;

    object->ID3DX11DataLoader_iface.lpVtbl = &memorydataloadervtbl;
    object->data = (void *)data;
    object->size = data_size;

    *loader = &object->ID3DX11DataLoader_iface;

    return S_OK;
}

HRESULT WINAPI D3DX11CreateAsyncFileLoaderA(const char *filename, ID3DX11DataLoader **loader)
{
    WCHAR *filename_w;
    HRESULT hr;
    int len;

    TRACE("filename %s, loader %p.\n", debugstr_a(filename), loader);

    if (!filename || !loader)
        return E_FAIL;

    len = MultiByteToWideChar(CP_ACP, 0, filename, -1, NULL, 0);
    filename_w = malloc(len * sizeof(*filename_w));
    MultiByteToWideChar(CP_ACP, 0, filename, -1, filename_w, len);

    hr = D3DX11CreateAsyncFileLoaderW(filename_w, loader);

    free(filename_w);

    return hr;
}

HRESULT WINAPI D3DX11CreateAsyncFileLoaderW(const WCHAR *filename, ID3DX11DataLoader **loader)
{
    struct asyncdataloader *object;

    TRACE("filename %s, loader %p.\n", debugstr_w(filename), loader);

    if (!filename || !loader)
        return E_FAIL;

    object = calloc(1, sizeof(*object));
    if (!object)
        return E_OUTOFMEMORY;

    object->ID3DX11DataLoader_iface.lpVtbl = &filedataloadervtbl;
    object->u.file.path = malloc((lstrlenW(filename) + 1) * sizeof(WCHAR));
    if (!object->u.file.path)
    {
        free(object);
        return E_OUTOFMEMORY;
    }
    lstrcpyW(object->u.file.path, filename);
    object->data = NULL;
    object->size = 0;

    *loader = &object->ID3DX11DataLoader_iface;

    return S_OK;
}

HRESULT WINAPI D3DX11CreateAsyncResourceLoaderA(HMODULE module, const char *resource, ID3DX11DataLoader **loader)
{
    struct asyncdataloader *object;
    HRSRC rsrc;
    HRESULT hr;

    TRACE("module %p, resource %s, loader %p.\n", module, debugstr_a(resource), loader);

    if (!loader)
        return E_FAIL;

    object = calloc(1, sizeof(*object));
    if (!object)
        return E_OUTOFMEMORY;

    if (FAILED((hr = load_resource_initA(module, resource, &rsrc))))
    {
        free(object);
        return hr;
    }

    object->ID3DX11DataLoader_iface.lpVtbl = &resourcedataloadervtbl;
    object->u.resource.module = module;
    object->u.resource.rsrc = rsrc;
    object->data = NULL;
    object->size = 0;

    *loader = &object->ID3DX11DataLoader_iface;

    return S_OK;
}

HRESULT WINAPI D3DX11CreateAsyncResourceLoaderW(HMODULE module, const WCHAR *resource, ID3DX11DataLoader **loader)
{
    struct asyncdataloader *object;
    HRSRC rsrc;
    HRESULT hr;

    TRACE("module %p, resource %s, loader %p.\n", module, debugstr_w(resource), loader);

    if (!loader)
        return E_FAIL;

    object = calloc(1, sizeof(*object));
    if (!object)
        return E_OUTOFMEMORY;

    if (FAILED((hr = load_resource_initW(module, resource, &rsrc))))
    {
        free(object);
        return hr;
    }

    object->ID3DX11DataLoader_iface.lpVtbl = &resourcedataloadervtbl;
    object->u.resource.module = module;
    object->u.resource.rsrc = rsrc;
    object->data = NULL;
    object->size = 0;

    *loader = &object->ID3DX11DataLoader_iface;

    return S_OK;
}

HRESULT WINAPI D3DX11CreateAsyncTextureInfoProcessor(D3DX11_IMAGE_INFO *info, ID3DX11DataProcessor **processor)
{
    struct texture_info_processor *object;

    TRACE("info %p, processor %p.\n", info, processor);

    if (!processor)
        return E_INVALIDARG;

    object = malloc(sizeof(*object));
    if (!object)
        return E_OUTOFMEMORY;

    object->ID3DX11DataProcessor_iface.lpVtbl = &texture_info_processor_vtbl;
    object->info = info;

    *processor = &object->ID3DX11DataProcessor_iface;
    return S_OK;
}

HRESULT WINAPI D3DX11CreateAsyncTextureProcessor(ID3D11Device *device,
        D3DX11_IMAGE_LOAD_INFO *load_info, ID3DX11DataProcessor **processor)
{
    struct texture_processor *object;

    TRACE("device %p, load_info %p, processor %p.\n", device, load_info, processor);

    if (!device || !processor)
        return E_INVALIDARG;

    object = calloc(1, sizeof(*object));
    if (!object)
        return E_OUTOFMEMORY;

    object->ID3DX11DataProcessor_iface.lpVtbl = &texture_processor_vtbl;
    object->device = device;
    ID3D11Device_AddRef(device);
    init_load_info(load_info, &object->load_info);
    if (!object->load_info.pSrcInfo)
        object->load_info.pSrcInfo = &object->img_info;

    *processor = &object->ID3DX11DataProcessor_iface;
    return S_OK;
}

HRESULT WINAPI D3DX11CreateAsyncShaderResourceViewProcessor(ID3D11Device *device,
        D3DX11_IMAGE_LOAD_INFO *load_info, ID3DX11DataProcessor **processor)
{
    struct srv_processor *object;
    HRESULT hr;

    TRACE("device %p, load_info %p, processor %p.\n", device, load_info, processor);

    if (!device || !processor)
        return E_INVALIDARG;

    object = calloc(1, sizeof(*object));
    if (!object)
        return E_OUTOFMEMORY;

    hr = D3DX11CreateAsyncTextureProcessor(device, load_info, &object->texture_processor);
    if (FAILED(hr))
    {
        free(object);
        return hr;
    }
    object->ID3DX11DataProcessor_iface.lpVtbl = &srv_processor_vtbl;
    *processor = &object->ID3DX11DataProcessor_iface;
    return S_OK;
}

struct work_item
{
    struct list entry;

    ID3DX11DataLoader *loader;
    ID3DX11DataProcessor *processor;
    HRESULT *result;
    void **object;
};

static inline void work_item_free(struct work_item *work_item, BOOL cancel)
{
    ID3DX11DataLoader_Destroy(work_item->loader);
    ID3DX11DataProcessor_Destroy(work_item->processor);
    if (cancel && work_item->result)
        *work_item->result = S_FALSE;
    free(work_item);
}

#define THREAD_PUMP_EXITING UINT_MAX
struct thread_pump
{
    ID3DX11ThreadPump ID3DX11ThreadPump_iface;
    LONG refcount;

    LONG processing_count;

    SRWLOCK io_lock;
    CONDITION_VARIABLE io_cv;
    unsigned int io_count;
    struct list io_queue;

    SRWLOCK proc_lock;
    CONDITION_VARIABLE proc_cv;
    unsigned int proc_count;
    struct list proc_queue;

    SRWLOCK device_lock;
    unsigned int device_count;
    struct list device_queue;

    unsigned int thread_count;
    HANDLE threads[1];
};

static inline struct thread_pump *impl_from_ID3DX11ThreadPump(ID3DX11ThreadPump *iface)
{
    return CONTAINING_RECORD(iface, struct thread_pump, ID3DX11ThreadPump_iface);
}

static HRESULT WINAPI thread_pump_QueryInterface(ID3DX11ThreadPump *iface, REFIID riid, void **out)
{
    TRACE("iface %p, riid %s, out %p.\n", iface, debugstr_guid(riid), out);

    if (IsEqualGUID(riid, &IID_ID3DX11ThreadPump)
            || IsEqualGUID(riid, &IID_IUnknown))
    {
        ID3DX11ThreadPump_AddRef(iface);
        *out = iface;
        return S_OK;
    }

    WARN("%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid(riid));
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI thread_pump_AddRef(ID3DX11ThreadPump *iface)
{
    struct thread_pump *thread_pump = impl_from_ID3DX11ThreadPump(iface);
    ULONG refcount = InterlockedIncrement(&thread_pump->refcount);

    TRACE("%p increasing refcount to %lu.\n", iface, refcount);

    return refcount;
}

static ULONG WINAPI thread_pump_Release(ID3DX11ThreadPump *iface)
{
    struct thread_pump *thread_pump = impl_from_ID3DX11ThreadPump(iface);
    ULONG refcount = InterlockedDecrement(&thread_pump->refcount);
    struct work_item *item, *next;
    struct list list;
    unsigned int i;

    TRACE("%p decreasing refcount to %lu.\n", iface, refcount);

    if (!refcount)
    {
        AcquireSRWLockExclusive(&thread_pump->io_lock);
        thread_pump->io_count = THREAD_PUMP_EXITING;
        ReleaseSRWLockExclusive(&thread_pump->io_lock);
        WakeAllConditionVariable(&thread_pump->io_cv);

        AcquireSRWLockExclusive(&thread_pump->proc_lock);
        thread_pump->proc_count = THREAD_PUMP_EXITING;
        ReleaseSRWLockExclusive(&thread_pump->proc_lock);
        WakeAllConditionVariable(&thread_pump->proc_cv);

        AcquireSRWLockExclusive(&thread_pump->device_lock);
        thread_pump->device_count = THREAD_PUMP_EXITING;
        ReleaseSRWLockExclusive(&thread_pump->device_lock);

        for (i = 0; i < thread_pump->thread_count; ++i)
        {
            if (!thread_pump->threads[i])
                continue;

            WaitForSingleObject(thread_pump->threads[i], INFINITE);
            CloseHandle(thread_pump->threads[i]);
        }

        list_init(&list);
        list_move_tail(&list, &thread_pump->io_queue);
        list_move_tail(&list, &thread_pump->proc_queue);
        list_move_tail(&list, &thread_pump->device_queue);
        LIST_FOR_EACH_ENTRY_SAFE(item, next, &list, struct work_item, entry)
        {
            list_remove(&item->entry);
            work_item_free(item, TRUE);
        }

        free(thread_pump);
    }

    return refcount;
}

static HRESULT WINAPI thread_pump_AddWorkItem(ID3DX11ThreadPump *iface, ID3DX11DataLoader *loader,
        ID3DX11DataProcessor *processor, HRESULT *result, void **object)
{
    struct thread_pump *thread_pump = impl_from_ID3DX11ThreadPump(iface);
    struct work_item *work_item;

    TRACE("iface %p, loader %p, processor %p, result %p, object %p.\n",
            iface, loader, processor, result, object);

    work_item = malloc(sizeof(*work_item));
    if (!work_item)
        return E_OUTOFMEMORY;

    work_item->loader = loader;
    work_item->processor = processor;
    work_item->result = result;
    work_item->object = object;

    if (object)
        *object = NULL;

    InterlockedIncrement(&thread_pump->processing_count);
    AcquireSRWLockExclusive(&thread_pump->io_lock);
    ++thread_pump->io_count;
    list_add_tail(&thread_pump->io_queue, &work_item->entry);
    ReleaseSRWLockExclusive(&thread_pump->io_lock);
    WakeConditionVariable(&thread_pump->io_cv);
    return S_OK;
}

static UINT WINAPI thread_pump_GetWorkItemCount(ID3DX11ThreadPump *iface)
{
    struct thread_pump *thread_pump = impl_from_ID3DX11ThreadPump(iface);
    UINT ret;

    TRACE("iface %p.\n", iface);

    AcquireSRWLockExclusive(&thread_pump->device_lock);
    ret = thread_pump->processing_count + thread_pump->device_count;
    ReleaseSRWLockExclusive(&thread_pump->device_lock);
    return ret;
}

static HRESULT WINAPI thread_pump_WaitForAllItems(ID3DX11ThreadPump *iface)
{
    struct thread_pump *thread_pump = impl_from_ID3DX11ThreadPump(iface);
    HRESULT hr;
    LONG v;

    TRACE("iface %p.\n", iface);

    for (;;)
    {
        if (FAILED((hr = ID3DX11ThreadPump_ProcessDeviceWorkItems(iface, UINT_MAX))))
            return hr;

        AcquireSRWLockExclusive(&thread_pump->device_lock);
        if (thread_pump->device_count)
        {
            ReleaseSRWLockExclusive(&thread_pump->device_lock);
            continue;
        }
        v = thread_pump->processing_count;
        ReleaseSRWLockExclusive(&thread_pump->device_lock);
        if (!v)
            break;

        RtlWaitOnAddress(&thread_pump->processing_count, &v, sizeof(v), NULL);
    }

    return S_OK;
}

static HRESULT WINAPI thread_pump_ProcessDeviceWorkItems(ID3DX11ThreadPump *iface, UINT count)
{
    struct thread_pump *thread_pump = impl_from_ID3DX11ThreadPump(iface);
    struct work_item *work_item;
    HRESULT hr;
    UINT i;

    TRACE("iface %p, count %u.\n", iface, count);

    for (i = 0; i < count; ++i)
    {
        AcquireSRWLockExclusive(&thread_pump->device_lock);
        if (!thread_pump->device_count)
        {
            ReleaseSRWLockExclusive(&thread_pump->device_lock);
            break;
        }

        --thread_pump->device_count;
        work_item = LIST_ENTRY(list_head(&thread_pump->device_queue), struct work_item, entry);
        list_remove(&work_item->entry);
        ReleaseSRWLockExclusive(&thread_pump->device_lock);

        hr = ID3DX11DataProcessor_CreateDeviceObject(work_item->processor, work_item->object);
        if (work_item->result)
            *work_item->result = hr;
        work_item_free(work_item, FALSE);
    }

    return S_OK;
}

static void purge_list(struct list *list, LONG *count)
{
    struct work_item *work_item;

    while (!list_empty(list))
    {
        work_item = LIST_ENTRY(list_head(list), struct work_item, entry);
        list_remove(&work_item->entry);
        work_item_free(work_item, TRUE);

        if (count && !InterlockedDecrement(count))
            RtlWakeAddressAll(count);
    }
}

static HRESULT WINAPI thread_pump_PurgeAllItems(ID3DX11ThreadPump *iface)
{
    struct thread_pump *thread_pump = impl_from_ID3DX11ThreadPump(iface);
    LONG v;

    TRACE("iface %p.\n", iface);

    for (;;)
    {
        AcquireSRWLockExclusive(&thread_pump->io_lock);
        purge_list(&thread_pump->io_queue, &thread_pump->processing_count);
        thread_pump->io_count = 0;
        ReleaseSRWLockExclusive(&thread_pump->io_lock);

        AcquireSRWLockExclusive(&thread_pump->proc_lock);
        purge_list(&thread_pump->proc_queue, &thread_pump->processing_count);
        thread_pump->proc_count = 0;
        ReleaseSRWLockExclusive(&thread_pump->proc_lock);

        AcquireSRWLockExclusive(&thread_pump->device_lock);
        purge_list(&thread_pump->device_queue, NULL);
        thread_pump->device_count = 0;
        v = thread_pump->processing_count;
        ReleaseSRWLockExclusive(&thread_pump->device_lock);
        if (!v)
            break;

        RtlWaitOnAddress(&thread_pump->processing_count, &v, sizeof(v), NULL);
    }

    return S_OK;
}

static HRESULT WINAPI thread_pump_GetQueueStatus(ID3DX11ThreadPump *iface,
        UINT *io_queue, UINT *process_queue, UINT *device_queue)
{
    struct thread_pump *thread_pump = impl_from_ID3DX11ThreadPump(iface);

    TRACE("iface %p, io_queue %p, process_queue %p, device_queue %p.\n",
            iface, io_queue, process_queue, device_queue);

    *io_queue = thread_pump->io_count;
    *process_queue = thread_pump->proc_count;
    *device_queue = thread_pump->device_count;
    return S_OK;
}

static const ID3DX11ThreadPumpVtbl thread_pump_vtbl =
{
    thread_pump_QueryInterface,
    thread_pump_AddRef,
    thread_pump_Release,
    thread_pump_AddWorkItem,
    thread_pump_GetWorkItemCount,
    thread_pump_WaitForAllItems,
    thread_pump_ProcessDeviceWorkItems,
    thread_pump_PurgeAllItems,
    thread_pump_GetQueueStatus
};

static DWORD WINAPI io_thread(void *arg)
{
    struct thread_pump *thread_pump = arg;
    struct work_item *work_item;
    HRESULT hr;

    TRACE("%p thread started.\n", thread_pump);

    for (;;)
    {
        AcquireSRWLockExclusive(&thread_pump->io_lock);

        while (!thread_pump->io_count)
            SleepConditionVariableSRW(&thread_pump->io_cv, &thread_pump->io_lock, INFINITE, 0);

        if (thread_pump->io_count == THREAD_PUMP_EXITING)
        {
            ReleaseSRWLockExclusive(&thread_pump->io_lock);
            return 0;
        }

        --thread_pump->io_count;
        work_item = LIST_ENTRY(list_head(&thread_pump->io_queue), struct work_item, entry);
        list_remove(&work_item->entry);
        ReleaseSRWLockExclusive(&thread_pump->io_lock);

        if (FAILED(hr = ID3DX11DataLoader_Load(work_item->loader)))
        {
            if (work_item->result)
                *work_item->result = hr;
            work_item_free(work_item, FALSE);
            if (!InterlockedDecrement(&thread_pump->processing_count))
                RtlWakeAddressAll(&thread_pump->processing_count);
            continue;
        }

        AcquireSRWLockExclusive(&thread_pump->proc_lock);
        if (thread_pump->proc_count == THREAD_PUMP_EXITING)
        {
            ReleaseSRWLockExclusive(&thread_pump->proc_lock);
            work_item_free(work_item, TRUE);
            return 0;
        }

        list_add_tail(&thread_pump->proc_queue, &work_item->entry);
        ++thread_pump->proc_count;
        ReleaseSRWLockExclusive(&thread_pump->proc_lock);
        WakeConditionVariable(&thread_pump->proc_cv);
    }
    return 0;
}

static DWORD WINAPI proc_thread(void *arg)
{
    struct thread_pump *thread_pump = arg;
    struct work_item *work_item;
    SIZE_T size;
    void *data;
    HRESULT hr;

    TRACE("%p thread started.\n", thread_pump);

    for (;;)
    {
        AcquireSRWLockExclusive(&thread_pump->proc_lock);

        while (!thread_pump->proc_count)
            SleepConditionVariableSRW(&thread_pump->proc_cv, &thread_pump->proc_lock, INFINITE, 0);

        if (thread_pump->proc_count == THREAD_PUMP_EXITING)
        {
            ReleaseSRWLockExclusive(&thread_pump->proc_lock);
            return 0;
        }

        --thread_pump->proc_count;
        work_item = LIST_ENTRY(list_head(&thread_pump->proc_queue), struct work_item, entry);
        list_remove(&work_item->entry);
        ReleaseSRWLockExclusive(&thread_pump->proc_lock);

        if (FAILED(hr = ID3DX11DataLoader_Decompress(work_item->loader, &data, &size)))
        {
            if (work_item->result)
                *work_item->result = hr;
            work_item_free(work_item, FALSE);
            if (!InterlockedDecrement(&thread_pump->processing_count))
                RtlWakeAddressAll(&thread_pump->processing_count);
            continue;
        }

        if (thread_pump->device_count == THREAD_PUMP_EXITING)
        {
            work_item_free(work_item, TRUE);
            return 0;
        }

        if (FAILED(hr = ID3DX11DataProcessor_Process(work_item->processor, data, size)))
        {
            if (work_item->result)
                *work_item->result = hr;
            work_item_free(work_item, FALSE);
            if (!InterlockedDecrement(&thread_pump->processing_count))
                RtlWakeAddressAll(&thread_pump->processing_count);
            continue;
        }

        AcquireSRWLockExclusive(&thread_pump->device_lock);
        if (thread_pump->device_count == THREAD_PUMP_EXITING)
        {
            ReleaseSRWLockExclusive(&thread_pump->device_lock);
            work_item_free(work_item, TRUE);
            return 0;
        }

        list_add_tail(&thread_pump->device_queue, &work_item->entry);
        ++thread_pump->device_count;
        InterlockedDecrement(&thread_pump->processing_count);
        RtlWakeAddressAll(&thread_pump->processing_count);
        ReleaseSRWLockExclusive(&thread_pump->device_lock);
    }
    return 0;
}

HRESULT WINAPI D3DX11CreateThreadPump(UINT io_threads, UINT proc_threads, ID3DX11ThreadPump **pump)
{
    struct thread_pump *object;
    unsigned int i;

    TRACE("io_threads %u, proc_threads %u, pump %p.\n", io_threads, proc_threads, pump);

    if (io_threads >= 1024 || proc_threads >= 1024)
        return E_FAIL;

    if (!io_threads)
        io_threads = 1;
    if (!proc_threads)
    {
        SYSTEM_INFO info;

        GetSystemInfo(&info);
        proc_threads = info.dwNumberOfProcessors;
    }

    if (!(object = calloc(1, FIELD_OFFSET(struct thread_pump, threads[io_threads + proc_threads]))))
        return E_OUTOFMEMORY;

    object->ID3DX11ThreadPump_iface.lpVtbl = &thread_pump_vtbl;
    object->refcount = 1;
    InitializeSRWLock(&object->io_lock);
    InitializeConditionVariable(&object->io_cv);
    list_init(&object->io_queue);
    InitializeSRWLock(&object->proc_lock);
    InitializeConditionVariable(&object->proc_cv);
    list_init(&object->proc_queue);
    InitializeSRWLock(&object->device_lock);
    list_init(&object->device_queue);
    object->thread_count = io_threads + proc_threads;

    for (i = 0; i < object->thread_count; ++i)
    {
        object->threads[i] = CreateThread(NULL, 0, i < io_threads ? io_thread : proc_thread, object, 0, NULL);
        if (!object->threads[i])
        {
            ID3DX11ThreadPump_Release(&object->ID3DX11ThreadPump_iface);
            return E_FAIL;
        }
    }

    *pump = &object->ID3DX11ThreadPump_iface;
    return S_OK;
}
