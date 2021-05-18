/*
 * Copyright 2008 Henri Verbeet for CodeWeavers
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
 *
 */

#include "config.h"

#include "dxgi_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(dxgi);

static inline struct dxgi_factory *impl_from_IWineDXGIFactory(IWineDXGIFactory *iface)
{
    return CONTAINING_RECORD(iface, struct dxgi_factory, IWineDXGIFactory_iface);
}

static HRESULT STDMETHODCALLTYPE DECLSPEC_HOTPATCH dxgi_factory_QueryInterface(IWineDXGIFactory *iface, REFIID iid, void **out)
{
    struct dxgi_factory *factory = impl_from_IWineDXGIFactory(iface);

    TRACE("iface %p, iid %s, out %p.\n", iface, debugstr_guid(iid), out);

    if (IsEqualGUID(iid, &IID_IWineDXGIFactory)
            || IsEqualGUID(iid, &IID_IDXGIFactory7)
            || IsEqualGUID(iid, &IID_IDXGIFactory6)
            || IsEqualGUID(iid, &IID_IDXGIFactory5)
            || IsEqualGUID(iid, &IID_IDXGIFactory4)
            || IsEqualGUID(iid, &IID_IDXGIFactory3)
            || IsEqualGUID(iid, &IID_IDXGIFactory2)
            || (factory->extended && IsEqualGUID(iid, &IID_IDXGIFactory1))
            || IsEqualGUID(iid, &IID_IDXGIFactory)
            || IsEqualGUID(iid, &IID_IDXGIObject)
            || IsEqualGUID(iid, &IID_IUnknown))
    {
        IUnknown_AddRef(iface);
        *out = iface;
        return S_OK;
    }

    WARN("%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid(iid));

    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE DECLSPEC_HOTPATCH dxgi_factory_AddRef(IWineDXGIFactory *iface)
{
    struct dxgi_factory *factory = impl_from_IWineDXGIFactory(iface);
    ULONG refcount = InterlockedIncrement(&factory->refcount);

    TRACE("%p increasing refcount to %u.\n", iface, refcount);

    return refcount;
}

static ULONG STDMETHODCALLTYPE DECLSPEC_HOTPATCH dxgi_factory_Release(IWineDXGIFactory *iface)
{
    struct dxgi_factory *factory = impl_from_IWineDXGIFactory(iface);
    ULONG refcount = InterlockedDecrement(&factory->refcount);

    TRACE("%p decreasing refcount to %u.\n", iface, refcount);

    if (!refcount)
    {
        if (factory->device_window)
            DestroyWindow(factory->device_window);

        wined3d_decref(factory->wined3d);
        wined3d_private_store_cleanup(&factory->private_store);
        heap_free(factory);
    }

    return refcount;
}

static HRESULT STDMETHODCALLTYPE dxgi_factory_SetPrivateData(IWineDXGIFactory *iface,
        REFGUID guid, UINT data_size, const void *data)
{
    struct dxgi_factory *factory = impl_from_IWineDXGIFactory(iface);

    TRACE("iface %p, guid %s, data_size %u, data %p.\n", iface, debugstr_guid(guid), data_size, data);

    return dxgi_set_private_data(&factory->private_store, guid, data_size, data);
}

static HRESULT STDMETHODCALLTYPE dxgi_factory_SetPrivateDataInterface(IWineDXGIFactory *iface,
        REFGUID guid, const IUnknown *object)
{
    struct dxgi_factory *factory = impl_from_IWineDXGIFactory(iface);

    TRACE("iface %p, guid %s, object %p.\n", iface, debugstr_guid(guid), object);

    return dxgi_set_private_data_interface(&factory->private_store, guid, object);
}

static HRESULT STDMETHODCALLTYPE dxgi_factory_GetPrivateData(IWineDXGIFactory *iface,
        REFGUID guid, UINT *data_size, void *data)
{
    struct dxgi_factory *factory = impl_from_IWineDXGIFactory(iface);

    TRACE("iface %p, guid %s, data_size %p, data %p.\n", iface, debugstr_guid(guid), data_size, data);

    return dxgi_get_private_data(&factory->private_store, guid, data_size, data);
}

static HRESULT STDMETHODCALLTYPE dxgi_factory_GetParent(IWineDXGIFactory *iface, REFIID iid, void **parent)
{
    WARN("iface %p, iid %s, parent %p.\n", iface, debugstr_guid(iid), parent);

    *parent = NULL;

    return E_NOINTERFACE;
}

static HRESULT STDMETHODCALLTYPE dxgi_factory_EnumAdapters1(IWineDXGIFactory *iface,
        UINT adapter_idx, IDXGIAdapter1 **adapter)
{
    struct dxgi_factory *factory = impl_from_IWineDXGIFactory(iface);
    struct dxgi_adapter *adapter_object;
    UINT adapter_count;
    HRESULT hr;

    TRACE("iface %p, adapter_idx %u, adapter %p.\n", iface, adapter_idx, adapter);

    if (!adapter)
        return DXGI_ERROR_INVALID_CALL;

    wined3d_mutex_lock();
    adapter_count = wined3d_get_adapter_count(factory->wined3d);
    wined3d_mutex_unlock();

    if (adapter_idx >= adapter_count)
    {
        *adapter = NULL;
        return DXGI_ERROR_NOT_FOUND;
    }

    if (FAILED(hr = dxgi_adapter_create(factory, adapter_idx, &adapter_object)))
    {
        *adapter = NULL;
        return hr;
    }

    *adapter = (IDXGIAdapter1 *)&adapter_object->IWineDXGIAdapter_iface;

    TRACE("Returning adapter %p.\n", *adapter);

    return S_OK;
}

static HRESULT STDMETHODCALLTYPE dxgi_factory_EnumAdapters(IWineDXGIFactory *iface,
        UINT adapter_idx, IDXGIAdapter **adapter)
{
    TRACE("iface %p, adapter_idx %u, adapter %p.\n", iface, adapter_idx, adapter);

    return dxgi_factory_EnumAdapters1(iface, adapter_idx, (IDXGIAdapter1 **)adapter);
}

static HRESULT STDMETHODCALLTYPE dxgi_factory_MakeWindowAssociation(IWineDXGIFactory *iface,
        HWND window, UINT flags)
{
    struct dxgi_factory *factory = impl_from_IWineDXGIFactory(iface);

    TRACE("iface %p, window %p, flags %#x.\n", iface, window, flags);

    if (flags > DXGI_MWA_VALID)
        return DXGI_ERROR_INVALID_CALL;

    if (!window)
    {
        wined3d_unregister_windows(factory->wined3d);
        return S_OK;
    }

    if (!wined3d_register_window(factory->wined3d, window, NULL, flags))
        return E_FAIL;

    return S_OK;
}

static HRESULT STDMETHODCALLTYPE dxgi_factory_GetWindowAssociation(IWineDXGIFactory *iface, HWND *window)
{
    TRACE("iface %p, window %p.\n", iface, window);

    if (!window)
        return DXGI_ERROR_INVALID_CALL;

    /* The tests show that this always returns NULL for some unknown reason. */
    *window = NULL;

    return S_OK;
}

static HRESULT STDMETHODCALLTYPE dxgi_factory_CreateSwapChain(IWineDXGIFactory *iface,
        IUnknown *device, DXGI_SWAP_CHAIN_DESC *desc, IDXGISwapChain **swapchain)
{
    struct dxgi_factory *factory = impl_from_IWineDXGIFactory(iface);
    DXGI_SWAP_CHAIN_FULLSCREEN_DESC fullscreen_desc;
    DXGI_SWAP_CHAIN_DESC1 swapchain_desc;

    TRACE("iface %p, device %p, desc %p, swapchain %p.\n", iface, device, desc, swapchain);

    if (!desc)
    {
        WARN("Invalid pointer.\n");
        return DXGI_ERROR_INVALID_CALL;
    }

    swapchain_desc.Width = desc->BufferDesc.Width;
    swapchain_desc.Height = desc->BufferDesc.Height;
    swapchain_desc.Format = desc->BufferDesc.Format;
    swapchain_desc.Stereo = FALSE;
    swapchain_desc.SampleDesc = desc->SampleDesc;
    swapchain_desc.BufferUsage = desc->BufferUsage;
    swapchain_desc.BufferCount = desc->BufferCount;
    swapchain_desc.Scaling = DXGI_SCALING_STRETCH;
    swapchain_desc.SwapEffect = desc->SwapEffect;
    swapchain_desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
    swapchain_desc.Flags = desc->Flags;

    fullscreen_desc.RefreshRate = desc->BufferDesc.RefreshRate;
    fullscreen_desc.ScanlineOrdering = desc->BufferDesc.ScanlineOrdering;
    fullscreen_desc.Scaling = desc->BufferDesc.Scaling;
    fullscreen_desc.Windowed = desc->Windowed;

    return IWineDXGIFactory_CreateSwapChainForHwnd(&factory->IWineDXGIFactory_iface,
            device, desc->OutputWindow, &swapchain_desc, &fullscreen_desc, NULL,
            (IDXGISwapChain1 **)swapchain);
}

static HRESULT STDMETHODCALLTYPE dxgi_factory_CreateSoftwareAdapter(IWineDXGIFactory *iface,
        HMODULE swrast, IDXGIAdapter **adapter)
{
    FIXME("iface %p, swrast %p, adapter %p stub!\n", iface, swrast, adapter);

    return E_NOTIMPL;
}

static BOOL STDMETHODCALLTYPE dxgi_factory_IsCurrent(IWineDXGIFactory *iface)
{
    static BOOL once = FALSE;

    if (!once++)
        FIXME("iface %p stub!\n", iface);
    else
        WARN("iface %p stub!\n", iface);

    return TRUE;
}

static BOOL STDMETHODCALLTYPE dxgi_factory_IsWindowedStereoEnabled(IWineDXGIFactory *iface)
{
    FIXME("iface %p stub!\n", iface);

    return FALSE;
}

struct proxy_swapchain
{
    IDXGISwapChain4 IDXGISwapChain4_iface;
    IDXGISwapChain4 *swapchain;
};

static inline struct proxy_swapchain *proxy_swapchain_from_IDXGISwapChain4(IDXGISwapChain4 *iface)
{
    return CONTAINING_RECORD(iface, struct proxy_swapchain, IDXGISwapChain4_iface);
}

/* IUnknown methods */

static HRESULT STDMETHODCALLTYPE DECLSPEC_HOTPATCH proxy_swapchain_QueryInterface(IDXGISwapChain4 *iface, REFIID riid, void **object)
{
    struct proxy_swapchain *swapchain = proxy_swapchain_from_IDXGISwapChain4(iface);
    HRESULT hr;
    IUnknown *unk;

    TRACE("iface %p, riid %s, object %p\n", iface, debugstr_guid(riid), object);

    if (IsEqualGUID(riid, &IID_IUnknown)
            || IsEqualGUID(riid, &IID_IDXGIObject)
            || IsEqualGUID(riid, &IID_IDXGIDeviceSubObject)
            || IsEqualGUID(riid, &IID_IDXGISwapChain)
            || IsEqualGUID(riid, &IID_IDXGISwapChain1)
            || IsEqualGUID(riid, &IID_IDXGISwapChain2)
            || IsEqualGUID(riid, &IID_IDXGISwapChain3)
            || IsEqualGUID(riid, &IID_IDXGISwapChain4))
    {
        hr = IDXGISwapChain4_QueryInterface(swapchain->swapchain, riid, (void **)&unk);
        if(SUCCEEDED(hr))
            /* return proxy */
            *object = iface;
        else
            *object = NULL;

        return hr;
    }

    WARN("%s not implemented, returning E_NOINTERFACE\n", debugstr_guid(riid));

    *object = NULL;
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE DECLSPEC_HOTPATCH proxy_swapchain_AddRef(IDXGISwapChain4 *iface)
{
    struct proxy_swapchain *swapchain = proxy_swapchain_from_IDXGISwapChain4(iface);

    TRACE("swapchain %p.\n", swapchain);

    return IDXGISwapChain4_AddRef(swapchain->swapchain);
}

static ULONG STDMETHODCALLTYPE DECLSPEC_HOTPATCH proxy_swapchain_Release(IDXGISwapChain4 *iface)
{
    struct proxy_swapchain *swapchain = proxy_swapchain_from_IDXGISwapChain4(iface);
    ULONG refcount = IDXGISwapChain4_Release(swapchain->swapchain);

    TRACE("%p decreasing refcount to %u.\n", swapchain, refcount);

    if (!refcount)
        heap_free(swapchain);

    return refcount;
}

/* IDXGIObject methods */

static HRESULT STDMETHODCALLTYPE DECLSPEC_HOTPATCH proxy_swapchain_SetPrivateData(IDXGISwapChain4 *iface,
        REFGUID guid, UINT data_size, const void *data)
{
    struct proxy_swapchain *swapchain = proxy_swapchain_from_IDXGISwapChain4(iface);

    TRACE("iface %p, guid %s, data_size %u, data %p.\n", iface, debugstr_guid(guid), data_size, data);

    return IDXGISwapChain4_SetPrivateData(swapchain->swapchain, guid, data_size, data);
}

static HRESULT STDMETHODCALLTYPE DECLSPEC_HOTPATCH proxy_swapchain_SetPrivateDataInterface(IDXGISwapChain4 *iface,
        REFGUID guid, const IUnknown *object)
{
    struct proxy_swapchain *swapchain = proxy_swapchain_from_IDXGISwapChain4(iface);

    TRACE("iface %p, guid %s, object %p.\n", iface, debugstr_guid(guid), object);

    return IDXGISwapChain4_SetPrivateDataInterface(swapchain->swapchain, guid, object);
}

static HRESULT STDMETHODCALLTYPE DECLSPEC_HOTPATCH proxy_swapchain_GetPrivateData(IDXGISwapChain4 *iface,
        REFGUID guid, UINT *data_size, void *data)
{
    struct proxy_swapchain *swapchain = proxy_swapchain_from_IDXGISwapChain4(iface);

    TRACE("iface %p, guid %s, data_size %p, data %p.\n", iface, debugstr_guid(guid), data_size, data);

    return IDXGISwapChain4_GetPrivateData(swapchain->swapchain, guid, data_size, data);
}

static HRESULT STDMETHODCALLTYPE DECLSPEC_HOTPATCH proxy_swapchain_GetParent(IDXGISwapChain4 *iface, REFIID riid, void **parent)
{
    struct proxy_swapchain *swapchain = proxy_swapchain_from_IDXGISwapChain4(iface);

    TRACE("iface %p, riid %s, parent %p.\n", iface, debugstr_guid(riid), parent);

    return IDXGISwapChain4_GetParent(swapchain->swapchain, riid, parent);
}

/* IDXGIDeviceSubObject methods */

static HRESULT STDMETHODCALLTYPE proxy_swapchain_GetDevice(IDXGISwapChain4 *iface, REFIID riid, void **device)
{
    struct proxy_swapchain *swapchain = proxy_swapchain_from_IDXGISwapChain4(iface);

    TRACE("iface %p, riid %s, device %p.\n", iface, debugstr_guid(riid), device);

    return IDXGISwapChain4_GetDevice(swapchain->swapchain, riid, device);
}

/* IDXGISwapChain methods */

HRESULT STDMETHODCALLTYPE DECLSPEC_HOTPATCH proxy_swapchain_Present(IDXGISwapChain4 *iface, UINT sync_interval, UINT flags)
{
    struct proxy_swapchain *swapchain = proxy_swapchain_from_IDXGISwapChain4(iface);

    TRACE("iface %p, sync_interval %u, flags %#x.\n", iface, sync_interval, flags);

    return IDXGISwapChain4_Present(swapchain->swapchain, sync_interval, flags);
}

static HRESULT STDMETHODCALLTYPE DECLSPEC_HOTPATCH proxy_swapchain_GetBuffer(IDXGISwapChain4 *iface,
        UINT buffer_idx, REFIID riid, void **surface)
{
    struct proxy_swapchain *swapchain = proxy_swapchain_from_IDXGISwapChain4(iface);

    TRACE(".\n");

    return IDXGISwapChain4_GetBuffer(swapchain->swapchain, buffer_idx, riid, surface);
}

static HRESULT STDMETHODCALLTYPE DECLSPEC_HOTPATCH proxy_swapchain_SetFullscreenState(IDXGISwapChain4 *iface,
        BOOL fullscreen, IDXGIOutput *target)
{
    struct proxy_swapchain *swapchain = proxy_swapchain_from_IDXGISwapChain4(iface);

    TRACE(".\n");

    return IDXGISwapChain4_SetFullscreenState(swapchain->swapchain, fullscreen, target);
}

static HRESULT STDMETHODCALLTYPE DECLSPEC_HOTPATCH proxy_swapchain_GetFullscreenState(IDXGISwapChain4 *iface,
        BOOL *fullscreen, IDXGIOutput **target)
{
    struct proxy_swapchain *swapchain = proxy_swapchain_from_IDXGISwapChain4(iface);

    TRACE(".\n");

    return IDXGISwapChain4_GetFullscreenState(swapchain->swapchain, fullscreen, target);
}

static HRESULT STDMETHODCALLTYPE DECLSPEC_HOTPATCH proxy_swapchain_GetDesc(IDXGISwapChain4 *iface, DXGI_SWAP_CHAIN_DESC *desc)
{
    struct proxy_swapchain *swapchain = proxy_swapchain_from_IDXGISwapChain4(iface);

    TRACE(".\n");

    return IDXGISwapChain4_GetDesc(swapchain->swapchain, desc);
}

static HRESULT STDMETHODCALLTYPE DECLSPEC_HOTPATCH proxy_swapchain_ResizeBuffers(IDXGISwapChain4 *iface,
        UINT buffer_count, UINT width, UINT height, DXGI_FORMAT format, UINT flags)
{
    struct proxy_swapchain *swapchain = proxy_swapchain_from_IDXGISwapChain4(iface);

    TRACE(".\n");

    return IDXGISwapChain4_ResizeBuffers(swapchain->swapchain, buffer_count, width, height, format, flags);
}

static HRESULT STDMETHODCALLTYPE DECLSPEC_HOTPATCH proxy_swapchain_ResizeTarget(IDXGISwapChain4 *iface,
        const DXGI_MODE_DESC *target_mode_desc)
{
    struct proxy_swapchain *swapchain = proxy_swapchain_from_IDXGISwapChain4(iface);

    TRACE(".\n");

    return IDXGISwapChain4_ResizeTarget(swapchain->swapchain, target_mode_desc);
}

static HRESULT STDMETHODCALLTYPE DECLSPEC_HOTPATCH proxy_swapchain_GetContainingOutput(IDXGISwapChain4 *iface, IDXGIOutput **output)
{
    struct proxy_swapchain *swapchain = proxy_swapchain_from_IDXGISwapChain4(iface);

    TRACE(".\n");

    return IDXGISwapChain4_GetContainingOutput(swapchain->swapchain, output);
}

static HRESULT STDMETHODCALLTYPE DECLSPEC_HOTPATCH proxy_swapchain_GetFrameStatistics(IDXGISwapChain4 *iface,
        DXGI_FRAME_STATISTICS *stats)
{
    struct proxy_swapchain *swapchain = proxy_swapchain_from_IDXGISwapChain4(iface);

    TRACE(".\n");

    return IDXGISwapChain4_GetFrameStatistics(swapchain->swapchain, stats);
}

static HRESULT STDMETHODCALLTYPE DECLSPEC_HOTPATCH proxy_swapchain_GetLastPresentCount(IDXGISwapChain4 *iface,
        UINT *last_present_count)
{
    struct proxy_swapchain *swapchain = proxy_swapchain_from_IDXGISwapChain4(iface);

    TRACE(".\n");

    return IDXGISwapChain4_GetLastPresentCount(swapchain->swapchain, last_present_count);
}

/* IDXGISwapChain1 methods */

static HRESULT STDMETHODCALLTYPE DECLSPEC_HOTPATCH proxy_swapchain_GetDesc1(IDXGISwapChain4 *iface, DXGI_SWAP_CHAIN_DESC1 *desc)
{
    struct proxy_swapchain *swapchain = proxy_swapchain_from_IDXGISwapChain4(iface);

    TRACE(".\n");

    return IDXGISwapChain4_GetDesc1(swapchain->swapchain, desc);
}

static HRESULT STDMETHODCALLTYPE DECLSPEC_HOTPATCH proxy_swapchain_GetFullscreenDesc(IDXGISwapChain4 *iface,
        DXGI_SWAP_CHAIN_FULLSCREEN_DESC *desc)
{
    struct proxy_swapchain *swapchain = proxy_swapchain_from_IDXGISwapChain4(iface);

    TRACE(".\n");

    return IDXGISwapChain4_GetFullscreenDesc(swapchain->swapchain, desc);
}

static HRESULT STDMETHODCALLTYPE DECLSPEC_HOTPATCH proxy_swapchain_GetHwnd(IDXGISwapChain4 *iface, HWND *hwnd)
{
    struct proxy_swapchain *swapchain = proxy_swapchain_from_IDXGISwapChain4(iface);

    TRACE(".\n");

    return IDXGISwapChain4_GetHwnd(swapchain->swapchain, hwnd);
}

static HRESULT STDMETHODCALLTYPE DECLSPEC_HOTPATCH proxy_swapchain_GetCoreWindow(IDXGISwapChain4 *iface,
        REFIID iid, void **core_window)
{
    struct proxy_swapchain *swapchain = proxy_swapchain_from_IDXGISwapChain4(iface);

    TRACE(".\n");

    return IDXGISwapChain4_GetCoreWindow(swapchain->swapchain, iid, core_window);
}

HRESULT STDMETHODCALLTYPE DECLSPEC_HOTPATCH proxy_swapchain_Present1(IDXGISwapChain4 *iface,
        UINT sync_interval, UINT flags, const DXGI_PRESENT_PARAMETERS *present_parameters)
{
    struct proxy_swapchain *swapchain = proxy_swapchain_from_IDXGISwapChain4(iface);

    TRACE(".\n");

    return IDXGISwapChain4_Present1(swapchain->swapchain, sync_interval, flags, present_parameters);
}

static BOOL STDMETHODCALLTYPE DECLSPEC_HOTPATCH proxy_swapchain_IsTemporaryMonoSupported(IDXGISwapChain4 *iface)
{
    struct proxy_swapchain *swapchain = proxy_swapchain_from_IDXGISwapChain4(iface);

    TRACE(".\n");

    return IDXGISwapChain4_IsTemporaryMonoSupported(swapchain->swapchain);
}

static HRESULT STDMETHODCALLTYPE DECLSPEC_HOTPATCH proxy_swapchain_GetRestrictToOutput(IDXGISwapChain4 *iface, IDXGIOutput **output)
{
    struct proxy_swapchain *swapchain = proxy_swapchain_from_IDXGISwapChain4(iface);

    TRACE(".\n");

    return IDXGISwapChain4_GetRestrictToOutput(swapchain->swapchain, output);
}

static HRESULT STDMETHODCALLTYPE DECLSPEC_HOTPATCH proxy_swapchain_SetBackgroundColor(IDXGISwapChain4 *iface, const DXGI_RGBA *color)
{
    struct proxy_swapchain *swapchain = proxy_swapchain_from_IDXGISwapChain4(iface);

    TRACE(".\n");

    return IDXGISwapChain4_SetBackgroundColor(swapchain->swapchain, color);
}

static HRESULT STDMETHODCALLTYPE DECLSPEC_HOTPATCH proxy_swapchain_GetBackgroundColor(IDXGISwapChain4 *iface, DXGI_RGBA *color)
{
    struct proxy_swapchain *swapchain = proxy_swapchain_from_IDXGISwapChain4(iface);

    TRACE(".\n");

    return IDXGISwapChain4_GetBackgroundColor(swapchain->swapchain, color);
}

static HRESULT STDMETHODCALLTYPE DECLSPEC_HOTPATCH proxy_swapchain_SetRotation(IDXGISwapChain4 *iface, DXGI_MODE_ROTATION rotation)
{
    struct proxy_swapchain *swapchain = proxy_swapchain_from_IDXGISwapChain4(iface);

    TRACE(".\n");

    return IDXGISwapChain4_SetRotation(swapchain->swapchain, rotation);
}

static HRESULT STDMETHODCALLTYPE DECLSPEC_HOTPATCH proxy_swapchain_GetRotation(IDXGISwapChain4 *iface, DXGI_MODE_ROTATION *rotation)
{
    struct proxy_swapchain *swapchain = proxy_swapchain_from_IDXGISwapChain4(iface);

    TRACE(".\n");

    return IDXGISwapChain4_GetRotation(swapchain->swapchain, rotation);
}

/* IDXGISwapChain2 methods */

static HRESULT STDMETHODCALLTYPE DECLSPEC_HOTPATCH proxy_swapchain_SetSourceSize(IDXGISwapChain4 *iface, UINT width, UINT height)
{
    struct proxy_swapchain *swapchain = proxy_swapchain_from_IDXGISwapChain4(iface);

    TRACE(".\n");

    return IDXGISwapChain4_SetSourceSize(swapchain->swapchain, width, height);
}

static HRESULT STDMETHODCALLTYPE DECLSPEC_HOTPATCH proxy_swapchain_GetSourceSize(IDXGISwapChain4 *iface, UINT *width, UINT *height)
{
    struct proxy_swapchain *swapchain = proxy_swapchain_from_IDXGISwapChain4(iface);

    TRACE(".\n");

    return IDXGISwapChain4_GetSourceSize(swapchain->swapchain, width, height);
}

static HRESULT STDMETHODCALLTYPE DECLSPEC_HOTPATCH proxy_swapchain_SetMaximumFrameLatency(IDXGISwapChain4 *iface, UINT max_latency)
{
    struct proxy_swapchain *swapchain = proxy_swapchain_from_IDXGISwapChain4(iface);

    TRACE(".\n");

    return IDXGISwapChain4_SetMaximumFrameLatency(swapchain->swapchain, max_latency);
}

static HRESULT STDMETHODCALLTYPE DECLSPEC_HOTPATCH proxy_swapchain_GetMaximumFrameLatency(IDXGISwapChain4 *iface, UINT *max_latency)
{
    struct proxy_swapchain *swapchain = proxy_swapchain_from_IDXGISwapChain4(iface);

    TRACE(".\n");

    return IDXGISwapChain4_GetMaximumFrameLatency(swapchain->swapchain, max_latency);
}

static HANDLE STDMETHODCALLTYPE DECLSPEC_HOTPATCH proxy_swapchain_GetFrameLatencyWaitableObject(IDXGISwapChain4 *iface)
{
    struct proxy_swapchain *swapchain = proxy_swapchain_from_IDXGISwapChain4(iface);

    TRACE(".\n");

    return IDXGISwapChain4_GetFrameLatencyWaitableObject(swapchain->swapchain);
}

static HRESULT STDMETHODCALLTYPE DECLSPEC_HOTPATCH proxy_swapchain_SetMatrixTransform(IDXGISwapChain4 *iface,
        const DXGI_MATRIX_3X2_F *matrix)
{
    struct proxy_swapchain *swapchain = proxy_swapchain_from_IDXGISwapChain4(iface);

    TRACE(".\n");

    return IDXGISwapChain4_SetMatrixTransform(swapchain->swapchain, matrix);
}

static HRESULT STDMETHODCALLTYPE DECLSPEC_HOTPATCH proxy_swapchain_GetMatrixTransform(IDXGISwapChain4 *iface,
        DXGI_MATRIX_3X2_F *matrix)
{
    struct proxy_swapchain *swapchain = proxy_swapchain_from_IDXGISwapChain4(iface);

    TRACE(".\n");

    return IDXGISwapChain4_GetMatrixTransform(swapchain->swapchain, matrix);
}

/* IDXGISwapChain3 methods */

static UINT STDMETHODCALLTYPE DECLSPEC_HOTPATCH proxy_swapchain_GetCurrentBackBufferIndex(IDXGISwapChain4 *iface)
{
    struct proxy_swapchain *swapchain = proxy_swapchain_from_IDXGISwapChain4(iface);

    TRACE(".\n");

    return IDXGISwapChain4_GetCurrentBackBufferIndex(swapchain->swapchain);
}

static HRESULT STDMETHODCALLTYPE DECLSPEC_HOTPATCH proxy_swapchain_CheckColorSpaceSupport(IDXGISwapChain4 *iface,
        DXGI_COLOR_SPACE_TYPE colour_space, UINT *colour_space_support)
{
    struct proxy_swapchain *swapchain = proxy_swapchain_from_IDXGISwapChain4(iface);

    TRACE(".\n");

    return IDXGISwapChain4_CheckColorSpaceSupport(swapchain->swapchain, colour_space, colour_space_support);
}

static HRESULT STDMETHODCALLTYPE DECLSPEC_HOTPATCH proxy_swapchain_SetColorSpace1(IDXGISwapChain4 *iface,
        DXGI_COLOR_SPACE_TYPE colour_space)
{
    struct proxy_swapchain *swapchain = proxy_swapchain_from_IDXGISwapChain4(iface);

    TRACE(".\n");

    return IDXGISwapChain4_SetColorSpace1(swapchain->swapchain, colour_space);
}

static HRESULT STDMETHODCALLTYPE DECLSPEC_HOTPATCH proxy_swapchain_ResizeBuffers1(IDXGISwapChain4 *iface,
        UINT buffer_count, UINT width, UINT height, DXGI_FORMAT format, UINT flags,
        const UINT *node_mask, IUnknown * const *present_queue)
{
    struct proxy_swapchain *swapchain = proxy_swapchain_from_IDXGISwapChain4(iface);

    TRACE(".\n");

    return IDXGISwapChain4_ResizeBuffers1(swapchain->swapchain, buffer_count, width, height, format, flags, node_mask, present_queue);
}

static HRESULT STDMETHODCALLTYPE DECLSPEC_HOTPATCH proxy_swapchain_SetHDRMetaData(IDXGISwapChain4 *iface,
        DXGI_HDR_METADATA_TYPE type, UINT size, void *metadata)
{
    struct proxy_swapchain *swapchain = proxy_swapchain_from_IDXGISwapChain4(iface);

    TRACE(".\n");

    return IDXGISwapChain4_SetHDRMetaData(swapchain->swapchain, type, size, metadata);
}

static const struct IDXGISwapChain4Vtbl proxy_swapchain_vtbl =
{
    /* IUnknown methods */
    proxy_swapchain_QueryInterface,
    proxy_swapchain_AddRef,
    proxy_swapchain_Release,
    /* IDXGIObject methods */
    proxy_swapchain_SetPrivateData,
    proxy_swapchain_SetPrivateDataInterface,
    proxy_swapchain_GetPrivateData,
    proxy_swapchain_GetParent,
    /* IDXGIDeviceSubObject methods */
    proxy_swapchain_GetDevice,
    /* IDXGISwapChain methods */
    proxy_swapchain_Present,
    proxy_swapchain_GetBuffer,
    proxy_swapchain_SetFullscreenState,
    proxy_swapchain_GetFullscreenState,
    proxy_swapchain_GetDesc,
    proxy_swapchain_ResizeBuffers,
    proxy_swapchain_ResizeTarget,
    proxy_swapchain_GetContainingOutput,
    proxy_swapchain_GetFrameStatistics,
    proxy_swapchain_GetLastPresentCount,
    /* IDXGISwapChain1 methods */
    proxy_swapchain_GetDesc1,
    proxy_swapchain_GetFullscreenDesc,
    proxy_swapchain_GetHwnd,
    proxy_swapchain_GetCoreWindow,
    proxy_swapchain_Present1,
    proxy_swapchain_IsTemporaryMonoSupported,
    proxy_swapchain_GetRestrictToOutput,
    proxy_swapchain_SetBackgroundColor,
    proxy_swapchain_GetBackgroundColor,
    proxy_swapchain_SetRotation,
    proxy_swapchain_GetRotation,
    /* IDXGISwapChain2 methods */
    proxy_swapchain_SetSourceSize,
    proxy_swapchain_GetSourceSize,
    proxy_swapchain_SetMaximumFrameLatency,
    proxy_swapchain_GetMaximumFrameLatency,
    proxy_swapchain_GetFrameLatencyWaitableObject,
    proxy_swapchain_SetMatrixTransform,
    proxy_swapchain_GetMatrixTransform,
    /* IDXGISwapChain3 methods */
    proxy_swapchain_GetCurrentBackBufferIndex,
    proxy_swapchain_CheckColorSpaceSupport,
    proxy_swapchain_SetColorSpace1,
    proxy_swapchain_ResizeBuffers1,
    /* IDXGISwapChain4 methods */
    proxy_swapchain_SetHDRMetaData,
};

static HRESULT STDMETHODCALLTYPE DECLSPEC_HOTPATCH dxgi_factory_CreateSwapChainForHwnd(IWineDXGIFactory *iface,
        IUnknown *device, HWND window, const DXGI_SWAP_CHAIN_DESC1 *desc,
        const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *fullscreen_desc,
        IDXGIOutput *output, IDXGISwapChain1 **swapchain)
{
    IWineDXGISwapChainFactory *swapchain_factory;
    ID3D12CommandQueue *command_queue;
    HRESULT hr;

    TRACE("iface %p, device %p, window %p, desc %p, fullscreen_desc %p, output %p, swapchain %p.\n",
            iface, device, window, desc, fullscreen_desc, output, swapchain);

    if (!device || !window || !desc || !swapchain)
    {
        WARN("Invalid pointer.\n");
        return DXGI_ERROR_INVALID_CALL;
    }

    if (desc->Stereo)
    {
        FIXME("Stereo swapchains are not supported.\n");
        return DXGI_ERROR_UNSUPPORTED;
    }

    if (!dxgi_validate_swapchain_desc(desc))
        return DXGI_ERROR_INVALID_CALL;

    if (output)
        FIXME("Ignoring output %p.\n", output);

    if (SUCCEEDED(IUnknown_QueryInterface(device, &IID_IWineDXGISwapChainFactory, (void **)&swapchain_factory)))
    {
        IDXGISwapChain4 *swapchain_impl;
        hr = IWineDXGISwapChainFactory_create_swapchain(swapchain_factory,
                (IDXGIFactory *)iface, window, desc, fullscreen_desc, output, (IDXGISwapChain1 **)&swapchain_impl);
        IWineDXGISwapChainFactory_Release(swapchain_factory);
        if (SUCCEEDED(hr))
        {
            struct proxy_swapchain *obj;

            obj = heap_alloc_zero(sizeof(*obj));
            obj->IDXGISwapChain4_iface.lpVtbl = &proxy_swapchain_vtbl;
            obj->swapchain = swapchain_impl;
            *swapchain = (IDXGISwapChain1 *)&obj->IDXGISwapChain4_iface;
        }
        else
        {
            *swapchain = NULL;
        }
        return hr;
    }

    if (SUCCEEDED(IUnknown_QueryInterface(device, &IID_ID3D12CommandQueue, (void **)&command_queue)))
    {
        hr = d3d12_swapchain_create(iface, command_queue, window, desc, fullscreen_desc, swapchain);
        ID3D12CommandQueue_Release(command_queue);
        return hr;
    }

    ERR("This is not the device we're looking for.\n");
    return DXGI_ERROR_UNSUPPORTED;
}

static HRESULT STDMETHODCALLTYPE dxgi_factory_CreateSwapChainForCoreWindow(IWineDXGIFactory *iface,
        IUnknown *device, IUnknown *window, const DXGI_SWAP_CHAIN_DESC1 *desc,
        IDXGIOutput *output, IDXGISwapChain1 **swapchain)
{
    FIXME("iface %p, device %p, window %p, desc %p, output %p, swapchain %p stub!\n",
            iface, device, window, desc, output, swapchain);

    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE dxgi_factory_GetSharedResourceAdapterLuid(IWineDXGIFactory *iface,
        HANDLE resource, LUID *luid)
{
    FIXME("iface %p, resource %p, luid %p stub!\n", iface, resource, luid);

    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE dxgi_factory_RegisterOcclusionStatusWindow(IWineDXGIFactory *iface,
        HWND window, UINT message, DWORD *cookie)
{
    FIXME("iface %p, window %p, message %#x, cookie %p stub!\n",
            iface, window, message, cookie);

    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE dxgi_factory_RegisterStereoStatusEvent(IWineDXGIFactory *iface,
        HANDLE event, DWORD *cookie)
{
    FIXME("iface %p, event %p, cookie %p stub!\n", iface, event, cookie);

    return E_NOTIMPL;
}

static void STDMETHODCALLTYPE dxgi_factory_UnregisterStereoStatus(IWineDXGIFactory *iface, DWORD cookie)
{
    FIXME("iface %p, cookie %#x stub!\n", iface, cookie);
}

static HRESULT STDMETHODCALLTYPE dxgi_factory_RegisterStereoStatusWindow(IWineDXGIFactory *iface,
        HWND window, UINT message, DWORD *cookie)
{
    FIXME("iface %p, window %p, message %#x, cookie %p stub!\n",
            iface, window, message, cookie);

    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE dxgi_factory_RegisterOcclusionStatusEvent(IWineDXGIFactory *iface,
        HANDLE event, DWORD *cookie)
{
    FIXME("iface %p, event %p, cookie %p stub!\n", iface, event, cookie);

    return E_NOTIMPL;
}

static void STDMETHODCALLTYPE dxgi_factory_UnregisterOcclusionStatus(IWineDXGIFactory *iface, DWORD cookie)
{
    FIXME("iface %p, cookie %#x stub!\n", iface, cookie);
}

static HRESULT STDMETHODCALLTYPE dxgi_factory_CreateSwapChainForComposition(IWineDXGIFactory *iface,
        IUnknown *device, const DXGI_SWAP_CHAIN_DESC1 *desc, IDXGIOutput *output, IDXGISwapChain1 **swapchain)
{
    FIXME("iface %p, device %p, desc %p, output %p, swapchain %p stub!\n",
            iface, device, desc, output, swapchain);

    return E_NOTIMPL;
}

static UINT STDMETHODCALLTYPE dxgi_factory_GetCreationFlags(IWineDXGIFactory *iface)
{
    FIXME("iface %p stub!\n", iface);

    return 0;
}

static HRESULT STDMETHODCALLTYPE dxgi_factory_EnumAdapterByLuid(IWineDXGIFactory *iface,
        LUID luid, REFIID iid, void **adapter)
{
    unsigned int adapter_index;
    DXGI_ADAPTER_DESC1 desc;
    IDXGIAdapter1 *adapter1;
    HRESULT hr;

    TRACE("iface %p, luid %08x:%08x, iid %s, adapter %p.\n",
            iface, luid.HighPart, luid.LowPart, debugstr_guid(iid), adapter);

    if (!adapter)
        return DXGI_ERROR_INVALID_CALL;

    adapter_index = 0;
    while ((hr = dxgi_factory_EnumAdapters1(iface, adapter_index, &adapter1)) == S_OK)
    {
        if (FAILED(hr = IDXGIAdapter1_GetDesc1(adapter1, &desc)))
        {
            WARN("Failed to get adapter %u desc, hr %#x.\n", adapter_index, hr);
            ++adapter_index;
            continue;
        }

        if (desc.AdapterLuid.LowPart == luid.LowPart
                && desc.AdapterLuid.HighPart == luid.HighPart)
        {
            hr = IDXGIAdapter1_QueryInterface(adapter1, iid, adapter);
            IDXGIAdapter1_Release(adapter1);
            return hr;
        }

        IDXGIAdapter1_Release(adapter1);
        ++adapter_index;
    }
    if (hr != DXGI_ERROR_NOT_FOUND)
        WARN("Failed to enumerate adapters, hr %#x.\n", hr);

    WARN("Adapter could not be found.\n");
    return DXGI_ERROR_NOT_FOUND;
}

static HRESULT STDMETHODCALLTYPE dxgi_factory_EnumWarpAdapter(IWineDXGIFactory *iface,
        REFIID iid, void **adapter)
{
    FIXME("iface %p, iid %s, adapter %p stub!\n", iface, debugstr_guid(iid), adapter);

    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE dxgi_factory_CheckFeatureSupport(IWineDXGIFactory *iface,
        DXGI_FEATURE feature, void *feature_data, UINT data_size)
{
    TRACE("iface %p, feature %#x, feature_data %p, data_size %u.\n",
            iface, feature, feature_data, data_size);

    switch (feature)
    {
        case DXGI_FEATURE_PRESENT_ALLOW_TEARING:
            if (data_size != sizeof(BOOL))
                return DXGI_ERROR_INVALID_CALL;
            *(BOOL *)feature_data = TRUE;
            return S_OK;

        default:
            WARN("Unsupported feature %#x.\n", feature);
            return DXGI_ERROR_INVALID_CALL;
    }
}

static HRESULT STDMETHODCALLTYPE dxgi_factory_EnumAdapterByGpuPreference(IWineDXGIFactory *iface,
        UINT adapter_idx, DXGI_GPU_PREFERENCE gpu_preference, REFIID iid, void **adapter)
{
    IDXGIAdapter1 *adapter_object;
    HRESULT hr;

    TRACE("iface %p, adapter_idx %u, gpu_preference %#x, iid %s, adapter %p.\n",
            iface, adapter_idx, gpu_preference, debugstr_guid(iid), adapter);

    if (gpu_preference != DXGI_GPU_PREFERENCE_UNSPECIFIED)
        FIXME("Ignoring GPU preference %#x.\n", gpu_preference);

    if (FAILED(hr = dxgi_factory_EnumAdapters1(iface, adapter_idx, &adapter_object)))
        return hr;

    hr = IDXGIAdapter1_QueryInterface(adapter_object, iid, adapter);
    IDXGIAdapter1_Release(adapter_object);
    return hr;
}

static HRESULT STDMETHODCALLTYPE dxgi_factory_RegisterAdaptersChangedEvent(IWineDXGIFactory *iface,
        HANDLE event, DWORD *cookie)
{
    FIXME("iface %p, event %p, cookie %p stub!\n", iface, event, cookie);

    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE dxgi_factory_UnregisterAdaptersChangedEvent(IWineDXGIFactory *iface,
        DWORD cookie)
{
    FIXME("iface %p, cookie %#x stub!\n", iface, cookie);

    return E_NOTIMPL;
}

static const struct IWineDXGIFactoryVtbl dxgi_factory_vtbl =
{
    dxgi_factory_QueryInterface,
    dxgi_factory_AddRef,
    dxgi_factory_Release,
    dxgi_factory_SetPrivateData,
    dxgi_factory_SetPrivateDataInterface,
    dxgi_factory_GetPrivateData,
    dxgi_factory_GetParent,
    dxgi_factory_EnumAdapters,
    dxgi_factory_MakeWindowAssociation,
    dxgi_factory_GetWindowAssociation,
    dxgi_factory_CreateSwapChain,
    dxgi_factory_CreateSoftwareAdapter,
    /* IDXGIFactory1 methods */
    dxgi_factory_EnumAdapters1,
    dxgi_factory_IsCurrent,
    /* IDXGIFactory2 methods */
    dxgi_factory_IsWindowedStereoEnabled,
    dxgi_factory_CreateSwapChainForHwnd,
    dxgi_factory_CreateSwapChainForCoreWindow,
    dxgi_factory_GetSharedResourceAdapterLuid,
    dxgi_factory_RegisterStereoStatusWindow,
    dxgi_factory_RegisterStereoStatusEvent,
    dxgi_factory_UnregisterStereoStatus,
    dxgi_factory_RegisterOcclusionStatusWindow,
    dxgi_factory_RegisterOcclusionStatusEvent,
    dxgi_factory_UnregisterOcclusionStatus,
    dxgi_factory_CreateSwapChainForComposition,
    /* IDXGIFactory3 methods */
    dxgi_factory_GetCreationFlags,
    /* IDXGIFactory4 methods */
    dxgi_factory_EnumAdapterByLuid,
    dxgi_factory_EnumWarpAdapter,
    /* IDXIGFactory5 methods */
    dxgi_factory_CheckFeatureSupport,
    /* IDXGIFactory6 methods */
    dxgi_factory_EnumAdapterByGpuPreference,
    /* IDXGIFactory7 methods */
    dxgi_factory_RegisterAdaptersChangedEvent,
    dxgi_factory_UnregisterAdaptersChangedEvent,
};

struct dxgi_factory *unsafe_impl_from_IDXGIFactory(IDXGIFactory *iface)
{
    IWineDXGIFactory *wine_factory;
    struct dxgi_factory *factory;
    HRESULT hr;

    if (!iface)
        return NULL;
    if (FAILED(hr = IDXGIFactory_QueryInterface(iface, &IID_IWineDXGIFactory, (void **)&wine_factory)))
    {
        ERR("Failed to get IWineDXGIFactory interface, hr %#x.\n", hr);
        return NULL;
    }
    assert(wine_factory->lpVtbl == &dxgi_factory_vtbl);
    factory = CONTAINING_RECORD(wine_factory, struct dxgi_factory, IWineDXGIFactory_iface);
    IWineDXGIFactory_Release(wine_factory);
    return factory;
}

static HRESULT dxgi_factory_init(struct dxgi_factory *factory, BOOL extended)
{
    factory->IWineDXGIFactory_iface.lpVtbl = &dxgi_factory_vtbl;
    factory->refcount = 1;
    wined3d_private_store_init(&factory->private_store);

    wined3d_mutex_lock();
    factory->wined3d = wined3d_create(0);
    wined3d_mutex_unlock();
    if (!factory->wined3d)
    {
        wined3d_private_store_cleanup(&factory->private_store);
        return DXGI_ERROR_UNSUPPORTED;
    }

    factory->extended = extended;

    return S_OK;
}

HRESULT dxgi_factory_create(REFIID riid, void **factory, BOOL extended)
{
    struct dxgi_factory *object;
    HRESULT hr;

    if (!(object = heap_alloc_zero(sizeof(*object))))
        return E_OUTOFMEMORY;

    if (FAILED(hr = dxgi_factory_init(object, extended)))
    {
        WARN("Failed to initialize factory, hr %#x.\n", hr);
        heap_free(object);
        return hr;
    }

    TRACE("Created factory %p.\n", object);

    hr = IWineDXGIFactory_QueryInterface(&object->IWineDXGIFactory_iface, riid, factory);
    IWineDXGIFactory_Release(&object->IWineDXGIFactory_iface);
    return hr;
}

HWND dxgi_factory_get_device_window(struct dxgi_factory *factory)
{
    wined3d_mutex_lock();

    if (!factory->device_window)
    {
        if (!(factory->device_window = CreateWindowA("static", "DXGI device window",
                WS_DISABLED, 0, 0, 0, 0, NULL, NULL, NULL, NULL)))
        {
            wined3d_mutex_unlock();
            ERR("Failed to create a window.\n");
            return NULL;
        }
        TRACE("Created device window %p for factory %p.\n", factory->device_window, factory);
    }

    wined3d_mutex_unlock();

    return factory->device_window;
}
