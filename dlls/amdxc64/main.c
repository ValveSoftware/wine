/*
 * amdxc implementation
 *
 * Copyright 2023 Etaash Mathamsetty
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

#include <stdarg.h>

#include "ntstatus.h"
#include "winerror.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winbase.h"
#include "winternl.h"
#include "wine/debug.h"
#include "wine/heap.h"
#include "wine/vulkan.h"

#define COBJMACROS
#include "initguid.h"
#include "d3d12.h"

#include "amdxc_interfaces.h"

WINE_DEFAULT_DEBUG_CHANNEL(amdxc);

static BOOL check_fsr4_supported(ID3D12Device *device)
{
    ID3D12DXVKInteropDevice *interop;
    VkInstance instance;
    VkDevice vk_device;
    VkPhysicalDevice phys_device;
    VkPhysicalDeviceProperties2 prop = {0};
    VkPhysicalDeviceDriverProperties driver_prop = {0};
    const char **extensions = NULL;
    UINT extension_count = 0;
    UINT major, minor;
    BOOL has_float8 = FALSE, has_coopmat2 = FALSE, has_coopmat = FALSE;
    BOOL rdna3_workaround = FALSE, ret = FALSE;
    const char *env = getenv("DXIL_SPIRV_CONFIG");

    if (FAILED(ID3D12Device_QueryInterface(device, &IID_ID3D12DXVKInteropDevice, (void **)&interop)))
        return FALSE;

    if (FAILED(ID3D12DXVKInteropDevice_GetVulkanHandles(interop, &instance, &phys_device, &vk_device)))
        goto fail;

    prop.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    prop.pNext = &driver_prop;
    driver_prop.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES;

    vkGetPhysicalDeviceProperties2(phys_device, &prop);

    if (prop.properties.vendorID != 0x1002) goto fail;
    /* only RADV supports FSR4 */
    if (driver_prop.driverID != VK_DRIVER_ID_MESA_RADV) goto fail;

    major = VK_API_VERSION_MAJOR(prop.properties.driverVersion);
    minor = VK_API_VERSION_MINOR(prop.properties.driverVersion);

    if (FAILED(ID3D12DXVKInteropDevice_GetDeviceExtensions(interop, &extension_count, NULL)))
        goto fail;

    extensions = malloc(sizeof(*extensions) * extension_count);

    if (FAILED(ID3D12DXVKInteropDevice_GetDeviceExtensions(interop, &extension_count, extensions)))
        goto fail;

    for (UINT i = 0; i < extension_count; i++)
    {
        if (!strcmp("VK_NV_cooperative_matrix2", extensions[i]))
            has_coopmat2 = TRUE;
        if (!strcmp("VK_KHR_cooperative_matrix", extensions[i]))
            has_coopmat = TRUE;
        if (!strcmp("VK_EXT_shader_float8", extensions[i]))
            has_float8 = TRUE;
    }

    if (env && strstr(env, "wmma_rdna3_workaround"))
        rdna3_workaround = TRUE;

    if (major > 25 || (major == 25 && minor >= 2))
    {
        /* RDNA 4+ */
        if (has_coopmat2 && has_float8) ret = TRUE;
        /*
         * RDNA3 (or RDNA 2/1 with layer),
         * ensure the user is doing stuff correctly
         */
        if (has_coopmat && rdna3_workaround) ret = TRUE;
    }

fail:
    if (extensions) free(extensions);
    ID3D12DXVKInteropDevice_Release(interop);

    return ret;
}

struct AMDFSR4FFX
{
    IAmdExtFfxApi IAmdExtFfxApi_iface;
    LONG ref;
    BOOL fsr4_supported;
};

static struct AMDFSR4FFX* impl_from_IAmdExtFfxApi(IAmdExtFfxApi* iface)
{
    return CONTAINING_RECORD(iface, struct AMDFSR4FFX, IAmdExtFfxApi_iface);
}

ULONG STDMETHODCALLTYPE AMDFSR4FFX_AddRef(IAmdExtFfxApi *iface)
{
    struct AMDFSR4FFX* data = impl_from_IAmdExtFfxApi(iface);
    return InterlockedIncrement(&data->ref);
}

ULONG STDMETHODCALLTYPE AMDFSR4FFX_Release(IAmdExtFfxApi *iface)
{
    struct AMDFSR4FFX* data = impl_from_IAmdExtFfxApi(iface);
    ULONG ret = InterlockedDecrement(&data->ref);
    if (!ret) free(data);
    return ret;
}

HRESULT STDMETHODCALLTYPE AMDFSR4FFX_QueryInterface(IAmdExtFfxApi *iface, REFIID iid, void **obj)
{
    FIXME("%p %s %p", iface, debugstr_guid(iid), obj);

    return E_NOINTERFACE;
}

typedef HRESULT (__stdcall *updateffxapi_pfn)(void*, unsigned int);

HRESULT STDMETHODCALLTYPE AMDFSR4FFX_UpdateFfxApiProvider(IAmdExtFfxApi *iface, void* data, unsigned int size)
{
    static int once;
    const char *env;
    updateffxapi_pfn pfn;
    HMODULE amdffx;
    struct AMDFSR4FFX *this = impl_from_IAmdExtFfxApi(iface);

    TRACE("%p %p %u\n", iface, data, size);

    env = getenv("FSR4_UPGRADE");

    if (env && !strcmp(env, "1"))
    {
        amdffx = LoadLibraryA("amdxcffx64");
        if (!amdffx)
        {
            ERR("Failed to load FSR4 dll (amdxcffx64)!\n");
            return E_NOINTERFACE;
        }

        if (!this->fsr4_supported)
        {
            ERR("FSR4 not supported on this system!\n");
            return E_NOINTERFACE;
        }

        pfn = (updateffxapi_pfn)GetProcAddress(amdffx, "UpdateFfxApiProvider");

        if (pfn)
        {
            if (!once++) WARN("Replaced FSR3 with FSR4!\n");
            return pfn(data, size);
        }
    }

    return E_NOINTERFACE;
}

static const struct IAmdExtFfxApiVtbl AMDFSR4FFX_vtable = {
    AMDFSR4FFX_QueryInterface,
    AMDFSR4FFX_AddRef,
    AMDFSR4FFX_Release,
    AMDFSR4FFX_UpdateFfxApiProvider
};

struct AmdExtD3DShaderIntrinsics
{
    IAmdExtD3DShaderIntrinsics IAmdExtD3DShaderIntrinsics_iface;
    LONG ref;
};

struct AmdExtD3DShaderIntrinsics* impl_from_IAmdExtD3DShaderIntrinsics(IAmdExtD3DShaderIntrinsics *iface)
{
    return CONTAINING_RECORD(iface, struct AmdExtD3DShaderIntrinsics, IAmdExtD3DShaderIntrinsics_iface);
}

ULONG STDMETHODCALLTYPE AmdExtD3DShaderIntrinsics_AddRef(IAmdExtD3DShaderIntrinsics *iface)
{
    struct AmdExtD3DShaderIntrinsics *this = impl_from_IAmdExtD3DShaderIntrinsics(iface);
    return InterlockedIncrement(&this->ref);
}

ULONG STDMETHODCALLTYPE AmdExtD3DShaderIntrinsics_Release(IAmdExtD3DShaderIntrinsics *iface)
{
    struct AmdExtD3DShaderIntrinsics *this = impl_from_IAmdExtD3DShaderIntrinsics(iface);
    ULONG ret = InterlockedDecrement(&this->ref);
    if (!ret) free(this);
    return ret;
}

HRESULT STDMETHODCALLTYPE AmdExtD3DShaderIntrinsics_QueryInterface(IAmdExtD3DShaderIntrinsics *iface, REFIID iid, void **out)
{
    FIXME("%p %s %p stub!\n", iface, debugstr_guid(iid), out);
    return E_NOINTERFACE;
}

HRESULT STDMETHODCALLTYPE AmdExtD3DShaderIntrinsics_GetInfo(IAmdExtD3DShaderIntrinsics *iface,
                                                            AmdExtD3DShaderIntrinsicsInfo *info)
{
    FIXME("%p %p stub!\n", iface, info);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE AmdExtD3DShaderIntrinsics_CheckSupport(IAmdExtD3DShaderIntrinsics *iface,
                                                                 AmdExtD3DShaderIntrinsicsSupport opcode)
{
    if (opcode == AmdExtD3DShaderIntrinsicsSupport_Float8Conversion) return S_OK;
    if (opcode == AmdExtD3DShaderIntrinsicsSupport_WaveMatrix) return S_OK;

    FIXME("%p %u stub!\n", iface, opcode);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE AmdExtD3DShaderIntrinsics_Enable(IAmdExtD3DShaderIntrinsics *iface)
{
    TRACE("%p\n", iface);
    /* shader intrinsics are always handled by vkd3d-proton */
    return S_OK;
}

const static struct IAmdExtD3DShaderIntrinsicsVtbl AmdExtD3DShaderIntrinsics_vtable = {
    AmdExtD3DShaderIntrinsics_QueryInterface,
    AmdExtD3DShaderIntrinsics_AddRef,
    AmdExtD3DShaderIntrinsics_Release,
    AmdExtD3DShaderIntrinsics_GetInfo,
    AmdExtD3DShaderIntrinsics_CheckSupport,
    AmdExtD3DShaderIntrinsics_Enable
};

struct AmdExtD3DDevice8
{
    IAmdExtD3DDevice8 IAmdExtD3DDevice8_iface;
    LONG ref;
    BOOL fsr4_supported;
};

struct AmdExtD3DDevice8 *impl_from_IAmdExtD3DDevice8(IAmdExtD3DDevice8 *iface)
{
    return CONTAINING_RECORD(iface, struct AmdExtD3DDevice8, IAmdExtD3DDevice8_iface);
}

HRESULT STDMETHODCALLTYPE AmdExtD3DDevice8_QueryInterface(IAmdExtD3DDevice8 *iface, REFIID iid, void **out)
{
    TRACE("%p %s %p\n", iface, debugstr_guid(iid), out);
    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE AmdExtD3DDevice8_AddRef(IAmdExtD3DDevice8 *iface)
{
    struct AmdExtD3DDevice8* this = impl_from_IAmdExtD3DDevice8(iface);
    return InterlockedIncrement(&this->ref);
}

ULONG STDMETHODCALLTYPE AmdExtD3DDevice8_Release(IAmdExtD3DDevice8 *iface)
{
    struct AmdExtD3DDevice8* this = impl_from_IAmdExtD3DDevice8(iface);
    ULONG ret = InterlockedDecrement(&this->ref);
    if (!ret) free(this);
    return ret;
}

HRESULT STDMETHODCALLTYPE AmdExtD3DDevice8_CreateGraphicsPipelineState(IAmdExtD3DDevice8 *iface,
                                                                       const AmdExtD3DCreateInfo *pCreateInfo,
                                                                       const D3D12_GRAPHICS_PIPELINE_STATE_DESC *pDesc,
                                                                       REFIID iid, void **ppPipelineState)
{
    FIXME("%p %p %p %s %p stub!\n", iface, pCreateInfo, pDesc, debugstr_guid(iid), ppPipelineState);
    return E_NOTIMPL;
}

void STDMETHODCALLTYPE AmdExtD3DDevice8_PushMarker(IAmdExtD3DDevice8 *iface, ID3D12GraphicsCommandList *pGfxCmdList,
                                                   const char *pMarkerData)
{
    FIXME("%p %p %s stub!\n", iface, pGfxCmdList, pMarkerData);
}

void STDMETHODCALLTYPE AmdExtD3DDevice8_PopMarker(IAmdExtD3DDevice8 *iface, ID3D12GraphicsCommandList *pGfxCmdList)
{
    FIXME("%p %p stub!\n", iface, pGfxCmdList);
}

void STDMETHODCALLTYPE AmdExtD3DDevice8_SetMarker(IAmdExtD3DDevice8 *iface, ID3D12GraphicsCommandList *pGfxCmdList,
                                                  const char *pMarkerData)
{
    FIXME("%p %p %s stub!\n", iface, pGfxCmdList, pMarkerData);
}

HRESULT STDMETHODCALLTYPE AmdExtD3DDevice8_CheckExtFeatureSupport(IAmdExtD3DDevice8 *iface, AmdExtD3DCheckFeatureSupportType type,
                                                                  void *data, SIZE_T size)
{
    FIXME("%p %u %p %lu stub!\n", iface, type, data, (ULONG)size);
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE AmdExtD3DDevice8_CreateComputePipelineState(IAmdExtD3DDevice8 *iface,
                                                                      const AmdExtD3DCreateInfo* pAmdExtCreateInfo,
                                                                      const D3D12_COMPUTE_PIPELINE_STATE_DESC* pDesc,
                                                                      REFIID iid, void **ppPipelineState)
{
    FIXME("%p %p %p %s %p stub!\n", iface, pAmdExtCreateInfo, pDesc, debugstr_guid(iid), ppPipelineState);
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE AmdExtD3DDevice8_CreatePipelineState(IAmdExtD3DDevice8 *iface,
                                                               const AmdExtD3DCreateInfo *pAmdExtCreateInfo,
                                                               const D3D12_PIPELINE_STATE_STREAM_DESC* pDesc,
                                                               REFIID iid, void **ppPipelineState)
{
    FIXME("%p %p %p %s %p stub!\n", iface, pAmdExtCreateInfo, pDesc, debugstr_guid(iid), ppPipelineState);
    return E_NOTIMPL;
}

void STDMETHODCALLTYPE AmdExtD3DDevice8_SetPrimitiveTopology(IAmdExtD3DDevice8 *iface,
                                                             ID3D12GraphicsCommandList *pGfxCmdList,
                                                             AmdExtD3DPrimitiveTopology topology)
{
    FIXME("%p %p %u stub!\n", iface, pGfxCmdList, topology);
}

HRESULT STDMETHODCALLTYPE AmdExtD3DDevice8_CreateComputePipelineFromElf(IAmdExtD3DDevice8 *iface,
                                                                        AmdExtD3DPipelineElfInfo *pAmdExtCreateInfo,
                                                                        REFIID iid, void **ppPipelineState)
{
    FIXME("%p %p %s %p stub!\n", iface, pAmdExtCreateInfo, debugstr_guid(iid), ppPipelineState);
    return E_NOTIMPL;
}

void STDMETHODCALLTYPE AmdExtD3DDevice8_SetKernelArguments(IAmdExtD3DDevice8 *iface,
                                                           ID3D12GraphicsCommandList *pCmdList,
                                                           ULONG first, ULONG count, const void *ppValues)
{
    FIXME("%p %p %lu %lu %p stub!\n", iface, pCmdList, first, count, ppValues);
}

void STDMETHODCALLTYPE AmdExtD3DDevice8_GetGpuRtInterfaceVersion(IAmdExtD3DDevice8 *iface,
                                                                 AmdExtD3DGpuRtVersion *pInterfaceVersion)
{
    FIXME("%p %p stub!\n", iface, pInterfaceVersion);
}

void STDMETHODCALLTYPE AmdExtD3DDevice8_GetGpuRtBinaryVersion(IAmdExtD3DDevice8 *iface,
                                                              AmdExtD3DGpuRtVersion *pBinaryVersion)
{
    FIXME("%p %p stub!\n", iface, pBinaryVersion);
}

HRESULT STDMETHODCALLTYPE AmdExtD3DDevice8_CreateComputePipelineCrossCompile(IAmdExtD3DDevice8 *iface,
                                                                             const AmdExtD3DPipelineCrossCompileInfo* pAmdExtCreateInfo,
                                                                             REFIID iid, void **ppPipelineState)
{
    FIXME("%p %p %s %p stub!\n", iface, pAmdExtCreateInfo, debugstr_guid(iid), ppPipelineState);
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE AmdExtD3DDevice8_GetWaveMatrixProperties(IAmdExtD3DDevice8 *iface,
                                                                   SIZE_T *pCount, AmdExtWaveMatrixProperties *pProperties)
{
    struct AmdExtD3DDevice8 *this = impl_from_IAmdExtD3DDevice8(iface);
    static AmdExtWaveMatrixProperties prop[1] = {{
        16, 16, 16, AMD_EXT_WMMA_TYPE_FP8, AMD_EXT_WMMA_TYPE_FP8,
        AMD_EXT_WMMA_TYPE_FP32, AMD_EXT_WMMA_TYPE_FP32, FALSE}};

    TRACE("%p %p %p\n", iface, pCount, pProperties);

    if (!pCount) return E_INVALIDARG;

    if (*pCount >= 1)
    {
        if (this->fsr4_supported)
        {
            *pCount = 1;
            memcpy(pProperties, prop, sizeof(prop));
            return S_OK;
        } else {
            *pCount = 0;
            return S_OK;
        }
    } /* FIXME: Handle pCount == 0 */

    return S_OK;
}

static const struct IAmdExtD3DDevice8Vtbl AmdExtD3DDevice8_vtable = {
    AmdExtD3DDevice8_QueryInterface,
    AmdExtD3DDevice8_AddRef,
    AmdExtD3DDevice8_Release,
    AmdExtD3DDevice8_CreateGraphicsPipelineState,
    AmdExtD3DDevice8_PushMarker,
    AmdExtD3DDevice8_PopMarker,
    AmdExtD3DDevice8_SetMarker,
    AmdExtD3DDevice8_CheckExtFeatureSupport,
    AmdExtD3DDevice8_CreateComputePipelineState,
    AmdExtD3DDevice8_CreatePipelineState,
    AmdExtD3DDevice8_SetPrimitiveTopology,
    AmdExtD3DDevice8_CreateComputePipelineFromElf,
    AmdExtD3DDevice8_SetKernelArguments,
    AmdExtD3DDevice8_GetGpuRtInterfaceVersion,
    AmdExtD3DDevice8_GetGpuRtBinaryVersion,
    AmdExtD3DDevice8_CreateComputePipelineCrossCompile,
    AmdExtD3DDevice8_GetWaveMatrixProperties
};

struct AmdExtD3DFactory
{
    IAmdExtD3DFactory IAmdExtD3DFactory_iface;
    LONG ref;
};

struct AmdExtD3DFactory* impl_from_IAmdExtD3DFactory(IAmdExtD3DFactory *iface)
{
    return CONTAINING_RECORD(iface, struct AmdExtD3DFactory, IAmdExtD3DFactory_iface);
}

ULONG STDMETHODCALLTYPE AmdExtD3DFactory_AddRef(IAmdExtD3DFactory *iface)
{
    struct AmdExtD3DFactory *this = impl_from_IAmdExtD3DFactory(iface);
    return InterlockedIncrement(&this->ref);
}

ULONG STDMETHODCALLTYPE AmdExtD3DFactory_Release(IAmdExtD3DFactory *iface)
{
    struct AmdExtD3DFactory *this = impl_from_IAmdExtD3DFactory(iface);
    ULONG ret = InterlockedDecrement(&this->ref);
    if (!ret) free(this);
    return ret;
}

HRESULT STDMETHODCALLTYPE AmdExtD3DFactory_CreateInterface(IAmdExtD3DFactory *iface, IUnknown *outer, REFIID iid, void **out)
{
    TRACE("%p %p %s %p\n", iface, outer, debugstr_guid(iid), out);

    if (IsEqualGUID(iid, &IID_IAmdExtD3DShaderIntrinsics))
    {
        struct AmdExtD3DShaderIntrinsics *this = calloc(1, sizeof(struct AmdExtD3DShaderIntrinsics));
        this->IAmdExtD3DShaderIntrinsics_iface.lpVtbl = &AmdExtD3DShaderIntrinsics_vtable;
        this->ref = 1;
        *out = &this->IAmdExtD3DShaderIntrinsics_iface;
        return S_OK;
    } else if (IsEqualGUID(iid, &IID_IAmdExtD3DDevice8)) {
        struct AmdExtD3DDevice8 *this = calloc(1, sizeof(struct AmdExtD3DDevice8));
        this->IAmdExtD3DDevice8_iface.lpVtbl = &AmdExtD3DDevice8_vtable;
        this->ref = 1;
        this->fsr4_supported = check_fsr4_supported((ID3D12Device *)outer);
        TRACE("FSR 4 supported: %d\n", this->fsr4_supported);
        *out = &this->IAmdExtD3DDevice8_iface;
        return S_OK;
    } else {
        FIXME("unknown guid %s\n", debugstr_guid(iid));
    }

    return E_NOINTERFACE;
}

HRESULT STDMETHODCALLTYPE AmdExtD3DFactory_QueryInterface(IAmdExtD3DFactory *iface, REFIID iid, void **out)
{
    TRACE("%p %s %p\n", iface, debugstr_guid(iid), out);
    return E_NOINTERFACE;
}

static const struct IAmdExtD3DFactoryVtbl AmdExtD3DFactory_vtable = {
    AmdExtD3DFactory_QueryInterface,
    AmdExtD3DFactory_AddRef,
    AmdExtD3DFactory_Release,
    AmdExtD3DFactory_CreateInterface
};

HRESULT CDECL AmdExtD3DCreateInterface(IUnknown *outer, REFIID iid, void **obj)
{
    TRACE("outer %p, iid %s, obj %p\n", outer, debugstr_guid(iid), obj);

    if (IsEqualGUID(iid, &IID_IAmdExtFfxApi))
    {
        struct AMDFSR4FFX* ffx = calloc(1, sizeof(struct AMDFSR4FFX));
        ffx->IAmdExtFfxApi_iface.lpVtbl = &AMDFSR4FFX_vtable;
        ffx->ref = 1;
        ffx->fsr4_supported = check_fsr4_supported((ID3D12Device *)outer);
        TRACE("FSR 4 supported: %d\n", ffx->fsr4_supported);
        *obj = &ffx->IAmdExtFfxApi_iface;
        return S_OK;
    } else if (IsEqualGUID(iid, &IID_IAmdExtAntiLagApi)) {
        return ID3D12Device_QueryInterface((ID3D12Device *)outer, &IID_IAmdExtAntiLagApi, obj);
    } else if(IsEqualGUID(iid, &IID_IAmdExtD3DFactory)) {
        struct AmdExtD3DFactory *this = calloc(1, sizeof(struct AmdExtD3DFactory));
        this->IAmdExtD3DFactory_iface.lpVtbl = &AmdExtD3DFactory_vtable;
        this->ref = 1;
        *obj = &this->IAmdExtD3DFactory_iface;
        return S_OK;
    } else {
        FIXME("unknown guid: %s\n", debugstr_guid(iid));
    }

    return E_NOINTERFACE;
}

HMODULE WINAPI AmdGetDxcModuleHandle(void)
{
    return GetModuleHandleA(NULL);
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, void *reserved)
{
    const char *env;

    switch (reason)
    {
        case DLL_PROCESS_ATTACH:
        {
            if ((env = getenv("FSR4_WATERMARK")) && !strcmp(env, "1"))
            {
                _putenv("MLSR-WATERMARK=1");
            }
            break;
        }
        default: break;
    }

    return TRUE;
}
