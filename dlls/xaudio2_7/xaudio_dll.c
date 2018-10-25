/*
 * Copyright (c) 2015 Mark Harmstone
 * Copyright (c) 2015 Andrew Eikum for CodeWeavers
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

#include "config.h"

#include <stdarg.h>

#define NONAMELESSUNION
#define COBJMACROS

#include "xaudio_private.h"

#include "ole2.h"
#include "rpcproxy.h"

#include "wine/debug.h"
#include "wine/heap.h"

WINE_DEFAULT_DEBUG_CHANNEL(xaudio2);

static HINSTANCE instance;

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD reason, void *pReserved)
{
    TRACE("(%p, %d, %p)\n", hinstDLL, reason, pReserved);

    switch (reason)
    {
    case DLL_PROCESS_ATTACH:
        instance = hinstDLL;
        DisableThreadLibraryCalls( hinstDLL );
        break;
    }
    return TRUE;
}

HRESULT WINAPI DllCanUnloadNow(void)
{
    return S_FALSE;
}

HRESULT WINAPI DllRegisterServer(void)
{
    TRACE("\n");
    return __wine_register_resources(instance);
}

HRESULT WINAPI DllUnregisterServer(void)
{
    TRACE("\n");
    return __wine_unregister_resources(instance);
}

HRESULT WINAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void **ppv)
{
    TRACE("(%s, %s, %p)\n", debugstr_guid(rclsid), debugstr_guid(riid), ppv);

    if(IsEqualGUID(rclsid, &CLSID_XAudio20) ||
            IsEqualGUID(rclsid, &CLSID_XAudio21) ||
            IsEqualGUID(rclsid, &CLSID_XAudio22) ||
            IsEqualGUID(rclsid, &CLSID_XAudio23) ||
            IsEqualGUID(rclsid, &CLSID_XAudio24) ||
            IsEqualGUID(rclsid, &CLSID_XAudio25) ||
            IsEqualGUID(rclsid, &CLSID_XAudio26) ||
            IsEqualGUID(rclsid, &CLSID_XAudio27))
        return make_xaudio2_factory(riid, ppv);

    if(IsEqualGUID(rclsid, &CLSID_AudioVolumeMeter20) ||
                IsEqualGUID(rclsid, &CLSID_AudioVolumeMeter21) ||
                IsEqualGUID(rclsid, &CLSID_AudioVolumeMeter22) ||
                IsEqualGUID(rclsid, &CLSID_AudioVolumeMeter23) ||
                IsEqualGUID(rclsid, &CLSID_AudioVolumeMeter24) ||
                IsEqualGUID(rclsid, &CLSID_AudioVolumeMeter25) ||
                IsEqualGUID(rclsid, &CLSID_AudioVolumeMeter26) ||
                IsEqualGUID(rclsid, &CLSID_AudioVolumeMeter27))
        return make_xapo_factory(&CLSID_AudioVolumeMeter27, riid, ppv);

    if(IsEqualGUID(rclsid, &CLSID_AudioReverb20) ||
                IsEqualGUID(rclsid, &CLSID_AudioReverb21) ||
                IsEqualGUID(rclsid, &CLSID_AudioReverb22) ||
                IsEqualGUID(rclsid, &CLSID_AudioReverb23) ||
                IsEqualGUID(rclsid, &CLSID_AudioReverb24) ||
                IsEqualGUID(rclsid, &CLSID_AudioReverb25) ||
                IsEqualGUID(rclsid, &CLSID_AudioReverb26) ||
                IsEqualGUID(rclsid, &CLSID_AudioReverb27))
        return make_xapo_factory(&CLSID_AudioReverb27, riid, ppv);

    return CLASS_E_CLASSNOTAVAILABLE;
}

#if XAUDIO2_VER >= 8
HRESULT WINAPI XAudio2Create(IXAudio2 **ppxa2, UINT32 flags, XAUDIO2_PROCESSOR proc)
{
    HRESULT hr;
    IXAudio2 *xa2;
    IClassFactory *cf;

    TRACE("%p 0x%x 0x%x\n", ppxa2, flags, proc);

    hr = make_xaudio2_factory(&IID_IClassFactory, (void**)&cf);
    if(FAILED(hr))
        return hr;

    hr = IClassFactory_CreateInstance(cf, NULL, &IID_IXAudio2, (void**)&xa2);
    IClassFactory_Release(cf);
    if(FAILED(hr))
        return hr;

    hr = xaudio2_initialize(This->faudio, flags, proc);
    if(FAILED(hr)){
        IXAudio2_Release(xa2);
        return hr;
    }

    *ppxa2 = xa2;

    return S_OK;
}
#endif /* XAUDIO2_VER >= 8 */
