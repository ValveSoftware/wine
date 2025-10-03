/* Headers: https://github.com/GPUOpen-LibrariesAndSDKs/display-library */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#define COBJMACROS
#include "windef.h"
#include "winbase.h"
#include "winuser.h"
#include "objbase.h"
#include "initguid.h"
#include "wine/debug.h"

#include "dxgi.h"

#define MAX_GPUS 64
#define VENDOR_AMD 0x1002

#define ADL_OK                            0
#define ADL_ERR                          -1
#define ADL_ERR_INVALID_PARAM            -3
#define ADL_ERR_INVALID_ADL_IDX          -5
#define ADL_ERR_NOT_SUPPORTED            -8
#define ADL_ERR_NULL_POINTER             -9
#define ADL_ERR_INVALID_CALLBACK         -11

#define ADL_DISPLAY_DISPLAYINFO_DISPLAYCONNECTED            0x00000001
#define ADL_DISPLAY_DISPLAYINFO_DISPLAYMAPPED               0x00000002
#define ADL_DISPLAY_DISPLAYINFO_MASK 0x31fff

#define ADL_ASIC_DISCRETE    (1 << 0)
#define ADL_ASIC_MASK        0xAF

enum ADLPlatForm
{
    GRAPHICS_PLATFORM_DESKTOP  = 0,
    GRAPHICS_PLATFORM_MOBILE   = 1
};
#define GRAPHICS_PLATFORM_UNKNOWN -1


WINE_DEFAULT_DEBUG_CHANNEL(atiadlxx);

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, void *reserved)
{
    TRACE("(%p, %u, %p)\n", instance, reason, reserved);

    switch (reason)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(instance);
        break;
    }

    return TRUE;
}

typedef void *(CALLBACK *ADL_MAIN_MALLOC_CALLBACK)(int);
typedef struct adl_context
{
    ADL_MAIN_MALLOC_CALLBACK malloc;
    IDXGIFactory *dxgi_factory;
}
*ADL_CONTEXT_HANDLE;

static ADL_CONTEXT_HANDLE default_ctx;

#define ADL_MAX_PATH 256

typedef struct ADLVersionsInfo
{
    char strDriverVer[ADL_MAX_PATH];
    char strCatalystVersion[ADL_MAX_PATH];
    char strCatalystWebLink[ADL_MAX_PATH];
} ADLVersionsInfo, *LPADLVersionsInfo;

typedef struct ADLVersionsInfoX2
{
    char strDriverVer[ADL_MAX_PATH];
    char strCatalystVersion[ADL_MAX_PATH];
    char strCrimsonVersion[ADL_MAX_PATH];
    char strCatalystWebLink[ADL_MAX_PATH];
} ADLVersionsInfoX2, *LPADLVersionsInfoX2;

typedef struct ADLAdapterInfo {
    int iSize;
    int iAdapterIndex;
    char strUDID[ADL_MAX_PATH];
    int iBusNumber;
    int iDeviceNumber;
    int iFunctionNumber;
    int iVendorID;
    char strAdapterName[ADL_MAX_PATH];
    char strDisplayName[ADL_MAX_PATH];
    int iPresent;
    int iExist;
    char strDriverPath[ADL_MAX_PATH];
    char strDriverPathExt[ADL_MAX_PATH];
    char strPNPString[ADL_MAX_PATH];
    int iOSDisplayIndex;
} ADLAdapterInfo, *LPADLAdapterInfo;

typedef struct ADLDisplayID
{
    int iDisplayLogicalIndex;
    int iDisplayPhysicalIndex;
    int iDisplayLogicalAdapterIndex;
    int iDisplayPhysicalAdapterIndex;
} ADLDisplayID, *LPADLDisplayID;

typedef struct ADLDisplayInfo
{
    ADLDisplayID displayID;
    int  iDisplayControllerIndex;
    char strDisplayName[ADL_MAX_PATH];
    char strDisplayManufacturerName[ADL_MAX_PATH];
    int  iDisplayType;
    int  iDisplayOutputType;
    int  iDisplayConnector;
    int  iDisplayInfoMask;
    int  iDisplayInfoValue;
} ADLDisplayInfo, *LPADLDisplayInfo;

typedef struct ADLCrossfireComb
{
    int iNumLinkAdapter;
    int iAdaptLink[3];
} ADLCrossfireComb;

typedef struct ADLCrossfireInfo
{
  int iErrorCode;
  int iState;
  int iSupported;
} ADLCrossfireInfo;

typedef struct ADLMemoryInfo
{
    long long iMemorySize;
    char strMemoryType[ADL_MAX_PATH];
    long long iMemoryBandwidth;
} ADLMemoryInfo, *LPADLMemoryInfo;

typedef struct ADLDisplayTarget
{
    ADLDisplayID displayID;
    int iDisplayMapIndex;
    int iDisplayTargetMask;
    int iDisplayTargetValue;
} ADLDisplayTarget, *LPADLDisplayTarget;

typedef struct ADLMode
{
    int iAdapterIndex;
    ADLDisplayID displayID;
    int iXPos;
    int iYPos;
    int iXRes;
    int iYRes;
    int iColourDepth;
    float fRefreshRate;
    int iOrientation;
    int iModeFlag;
    int iModeMask;
    int iModeValue;
} ADLMode, *LPADLMode;

typedef struct ADLDisplayMap
{
    int iDisplayMapIndex;
    ADLMode displayMode;
    int iNumDisplayTarget;
    int iFirstDisplayTargetArrayIndex;
    int iDisplayMapMask;
    int iDisplayMapValue;
} ADLDisplayMap, *LPADLDisplayMap;

static const ADLVersionsInfo version = {
    "99.19.02-230831a-396538C-AMD-Software-Adrenalin-Edition",
    "",
    "http://support.amd.com/drivers/xml/driver_09_us.xml",
};

static const ADLVersionsInfoX2 version2 = {
    "99.19.02-230831a-396538C-AMD-Software-Adrenalin-Edition",
    "",
    "99.10.2",
    "http://support.amd.com/drivers/xml/driver_09_us.xml",
};

int CDECL ADL2_Main_Control_Destroy(ADL_CONTEXT_HANDLE context)
{
    TRACE("context %p stub.\n", context);

    if (!context) return ADL_ERR;
    if (context->dxgi_factory)
        IUnknown_Release(context->dxgi_factory);
    if (context == default_ctx) default_ctx = NULL;
    free(context);
    return ADL_OK;
}

int CDECL ADL2_Main_Control_Create(ADL_MAIN_MALLOC_CALLBACK cb, int arg, ADL_CONTEXT_HANDLE *ptr)
{
    ADL_CONTEXT_HANDLE ctx;

    TRACE("cb %p, arg %d, ptr %p.\n", cb, arg, ptr);

    ctx = calloc( 1, sizeof(**ptr));
    if (FAILED(CreateDXGIFactory(&IID_IDXGIFactory, (void**)&ctx->dxgi_factory)))
    {
        free(ctx);
        return ADL_ERR;
    }

    ctx->malloc = cb;
    if (default_ctx) ADL2_Main_Control_Destroy(default_ctx);
    default_ctx = ctx;
    *ptr = ctx;
    TRACE("-> %p.\n", *ptr);
    return ADL_OK;
}

int CDECL ADL_Main_Control_Create(ADL_MAIN_MALLOC_CALLBACK cb, int arg)
{
    TRACE("cb %p, arg %d stub!\n", cb, arg);

    return ADL2_Main_Control_Create(cb, arg, &default_ctx);
}

int CDECL ADL_Main_Control_Destroy(void)
{
    TRACE(".\n");

    return ADL2_Main_Control_Destroy(default_ctx);
}

int CDECL ADL2_Adapter_NumberOfAdapters_Get(ADL_CONTEXT_HANDLE ctx, int *count)
{
    IDXGIAdapter *adapter;

    TRACE("ctx %p, count %p.\n", ctx, count);

    *count = 0;
    while (SUCCEEDED(IDXGIFactory_EnumAdapters(ctx->dxgi_factory, *count, &adapter)))
    {
        (*count)++;
        IUnknown_Release(adapter);
    }

    TRACE("*count = %d\n", *count);
    return ADL_OK;
}

int CDECL ADL2_Graphics_VersionsX2_Get(ADL_CONTEXT_HANDLE ptr, ADLVersionsInfoX2 *ver)
{
    TRACE("ptr %p, ver %p.\n", ptr, ver);
    memcpy(ver, &version2, sizeof(version2));
    return ADL_OK;
}

int CDECL ADL_Graphics_Versions_Get(ADLVersionsInfo *ver)
{
    TRACE("ver %p.\n", ver);
    memcpy(ver, &version, sizeof(version));
    return ADL_OK;
}

int CDECL ADL_Adapter_NumberOfAdapters_Get(int *count)
{
    TRACE("count %p.\n", count);

    return ADL2_Adapter_NumberOfAdapters_Get(default_ctx, count);
}

static int get_adapter_desc(ADL_CONTEXT_HANDLE ctx, int adapter_index, DXGI_ADAPTER_DESC *desc)
{
    IDXGIAdapter *adapter;
    HRESULT hr;

    if (FAILED(IDXGIFactory_EnumAdapters(ctx->dxgi_factory, adapter_index, &adapter)))
        return ADL_ERR;

    hr = IDXGIAdapter_GetDesc(adapter, desc);

    IUnknown_Release(adapter);

    return SUCCEEDED(hr) ? ADL_OK : ADL_ERR;
}

/* yep, seriously */
static int convert_vendor_id(int id)
{
    char str[16];
    snprintf(str, ARRAY_SIZE(str), "%x", id);
    return atoi(str);
}

static int adapter_info_get(ADL_CONTEXT_HANDLE ctx, ADLAdapterInfo *adapters, int input_size)
{
    int i, count = input_size / sizeof(*adapters);
    DXGI_ADAPTER_DESC adapter_desc;

    memset(adapters, 0, input_size);

    for (i = 0; i < count; i++)
    {
        adapters[i].iSize = sizeof(ADLAdapterInfo);
        adapters[i].iAdapterIndex = i;

        if (get_adapter_desc(ctx, i, &adapter_desc) != ADL_OK)
            return ADL_ERR;

        adapters[i].iVendorID = convert_vendor_id(adapter_desc.VendorId);
    }
    return ADL_OK;
}

int CDECL ADL_Adapter_AdapterInfo_Get(ADLAdapterInfo *adapters, int input_size)
{
    int count;

    FIXME("adapters %p, input_size %d, stub!\n", adapters, input_size);

    ADL_Adapter_NumberOfAdapters_Get(&count);

    if (!adapters) return ADL_ERR_INVALID_PARAM;
    if (input_size != count * sizeof(ADLAdapterInfo)) return ADL_ERR_INVALID_PARAM;

    return adapter_info_get(default_ctx, adapters, input_size);
}

int CDECL ADL2_Adapter_AdapterInfoX2_Get(ADL_CONTEXT_HANDLE ctx, ADLAdapterInfo **info)
{
    int count;

    TRACE("ctx %p, info %p.\n", ctx, info);
    ADL2_Adapter_NumberOfAdapters_Get(ctx, &count);
    *info = ctx->malloc( count * sizeof(**info) );
    return adapter_info_get(ctx, *info, count * sizeof(**info));
}

int CDECL ADL2_Display_DisplayInfo_Get(ADL_CONTEXT_HANDLE ctx, int adapter_index, int *num_displays, ADLDisplayInfo **info, int force_detect)
{
    IDXGIAdapter *adapter;
    IDXGIOutput *output;
    int i;

    FIXME("adapter %d, num_displays %p, info %p stub!\n", adapter_index, num_displays, info);

    if (info == NULL || num_displays == NULL) return ADL_ERR_NULL_POINTER;

    if (FAILED(IDXGIFactory_EnumAdapters(ctx->dxgi_factory, adapter_index, &adapter)))
        return ADL_ERR_INVALID_PARAM;

    *num_displays = 0;

    while (SUCCEEDED(IDXGIAdapter_EnumOutputs(adapter, *num_displays, &output)))
    {
        (*num_displays)++;
        IUnknown_Release(output);
    }

    IUnknown_Release(adapter);

    if (*num_displays == 0)
        return ADL_OK;

    *info = ctx->malloc(*num_displays * sizeof(**info));
    memset(*info, 0, *num_displays * sizeof(**info));

    for (i = 0; i < *num_displays; i++)
    {
        (*info)[i].displayID.iDisplayLogicalIndex = i;
        (*info)[i].iDisplayInfoValue = ADL_DISPLAY_DISPLAYINFO_DISPLAYCONNECTED | ADL_DISPLAY_DISPLAYINFO_DISPLAYMAPPED;
        (*info)[i].iDisplayInfoMask = (*info)[i].iDisplayInfoValue;
    }

    return ADL_OK;
}

int CDECL ADL_Display_DisplayInfo_Get(int adapter_index, int *num_displays, ADLDisplayInfo **info, int force_detect)
{
    TRACE(".\n");

    return ADL2_Display_DisplayInfo_Get(default_ctx, adapter_index, num_displays, info, force_detect);
}

int CDECL ADL_Adapter_Crossfire_Caps(int adapter_index, int *preffered, int *num_comb, ADLCrossfireComb** comb)
{
    FIXME("adapter %d, preffered %p, num_comb %p, comb %p stub!\n", adapter_index, preffered, num_comb, comb);
    return ADL_ERR;
}

int CDECL ADL2_Adapter_Crossfire_Caps(ADL_CONTEXT_HANDLE context, int adapter_index, int *preffered, int *num_comb, ADLCrossfireComb** comb)
{
    FIXME("context %p, adapter %d, preffered %p, num_comb %p, comb %p stub!\n", context, adapter_index, preffered, num_comb, comb);
    return ADL_ERR;
}

int CDECL ADL_Adapter_Crossfire_Get(int adapter_index, ADLCrossfireComb *comb, ADLCrossfireInfo *info)
{
    FIXME("adapter %d, comb %p, info %p, stub!\n", adapter_index, comb, info);
    return ADL_ERR;
}

int CDECL ADL2_Adapter_ASICFamilyType_Get(ADL_CONTEXT_HANDLE ctx, int adapter_index, int *asic_type, int *valids)
{
    DXGI_ADAPTER_DESC adapter_desc;

    FIXME("adapter %d, asic_type %p, valids %p, stub!\n", adapter_index, asic_type, valids);

    if (asic_type == NULL || valids == NULL)
        return  ADL_ERR_NULL_POINTER;

    if (get_adapter_desc(ctx, adapter_index, &adapter_desc) != ADL_OK)
        return ADL_ERR_INVALID_ADL_IDX;

    if (adapter_desc.VendorId != VENDOR_AMD)
        return ADL_ERR_NOT_SUPPORTED;

    *asic_type = ADL_ASIC_DISCRETE;
    *valids = ADL_ASIC_MASK;

    return ADL_OK;
}

int CDECL ADL_Adapter_ASICFamilyType_Get(int adapter_index, int *asic_type, int *valids)
{
    TRACE("adapter %d, asic_type %p, valids %p.\n", adapter_index, asic_type, valids);
    return ADL2_Adapter_ASICFamilyType_Get(default_ctx, adapter_index, asic_type, valids);
}

static int get_max_clock(const char *clock, int default_value)
{
    char path[MAX_PATH], line[256];
    FILE *file;
    int drm_card, value = 0;

    for (drm_card = 0; drm_card < MAX_GPUS; drm_card++)
    {
        sprintf(path, "/sys/class/drm/card%d/device/pp_dpm_%s", drm_card, clock);
        file = fopen(path, "r");

        if (file == NULL)
            continue;

        while (fgets(line, sizeof(line), file) != NULL)
        {
            char *number;

            number = strchr(line, ' ');
            if (number == NULL)
            {
                WARN("pp_dpm_%s file has unexpected format\n", clock);
                break;
            }

            number++;
            value = max(strtol(number, NULL, 0), value);
        }
    }

    if (value != 0)
        return value;

    return default_value;
}

/* documented in the "Linux Specific APIs" section, present and used on Windows */
/* the name and documentation suggests that this returns current freqs, but it's actually max */
int CDECL ADL2_Adapter_ObservedClockInfo_Get(ADL_CONTEXT_HANDLE ctx, int adapter_index, int *core_clock, int *memory_clock)
{
    DXGI_ADAPTER_DESC adapter_desc;

    FIXME("adapter %d, core_clock %p, memory_clock %p, stub!\n", adapter_index, core_clock, memory_clock);

    if (core_clock == NULL || memory_clock == NULL) return ADL_ERR;
    if (get_adapter_desc(ctx, adapter_index, &adapter_desc) != ADL_OK) return ADL_ERR;
    if (adapter_desc.VendorId != VENDOR_AMD) return ADL_ERR_INVALID_ADL_IDX;

    /* default values based on RX580 */
    *core_clock = get_max_clock("sclk", 1350);
    *memory_clock = get_max_clock("mclk", 2000);

    TRACE("*core_clock: %i, *memory_clock %i\n", *core_clock, *memory_clock);

    return ADL_OK;
}

int CDECL ADL_Adapter_ObservedClockInfo_Get(int adapter_index, int *core_clock, int *memory_clock)
{
    TRACE("adapter_index %d, core_clock %p, memory_clock %p.\n", adapter_index, core_clock, memory_clock);

    return ADL2_Adapter_ObservedClockInfo_Get(default_ctx, adapter_index, core_clock, memory_clock);
}

/* documented in the "Linux Specific APIs" section, present and used on Windows */
int CDECL ADL2_Adapter_MemoryInfo_Get(ADL_CONTEXT_HANDLE ctx, int adapter_index, ADLMemoryInfo *mem_info)
{
    DXGI_ADAPTER_DESC adapter_desc;

    FIXME("adapter %d, mem_info %p stub!\n", adapter_index, mem_info);

    if (mem_info == NULL) return ADL_ERR_NULL_POINTER;
    if (get_adapter_desc(ctx, adapter_index, &adapter_desc) != ADL_OK) return ADL_ERR_INVALID_ADL_IDX;
    if (adapter_desc.VendorId != VENDOR_AMD) return ADL_ERR;

    mem_info->iMemorySize = adapter_desc.DedicatedVideoMemory;
    mem_info->iMemoryBandwidth = 256000; /* not exposed on Linux, probably needs a lookup table */

    TRACE("iMemoryBandwidth %s, iMemorySize %s\n",
            wine_dbgstr_longlong(mem_info->iMemoryBandwidth),
            wine_dbgstr_longlong(mem_info->iMemorySize));
    return ADL_OK;
}

int CDECL ADL_Adapter_MemoryInfo_Get(int adapter_index, ADLMemoryInfo *mem_info)
{
    TRACE("adapter_index %d, mem_info %p.\n", adapter_index, mem_info);

    return ADL2_Adapter_MemoryInfo_Get(default_ctx, adapter_index, mem_info);
}

int CDECL ADL2_Graphics_Platform_Get(ADL_CONTEXT_HANDLE ctx, int *platform)
{
    DXGI_ADAPTER_DESC adapter_desc;
    int count, i;

    FIXME("platform %p, stub!\n", platform);

    *platform = GRAPHICS_PLATFORM_UNKNOWN;

    ADL_Adapter_NumberOfAdapters_Get(&count);

    for (i = 0; i < count; i ++)
    {
        if (get_adapter_desc(ctx, i, &adapter_desc) != ADL_OK)
            continue;

        if (adapter_desc.VendorId == VENDOR_AMD)
            *platform = GRAPHICS_PLATFORM_DESKTOP;
    }

    /* NOTE: The real value can be obtained by doing:
     * 1. ioctl(DRM_AMDGPU_INFO) with AMDGPU_INFO_DEV_INFO - dev_info.ids_flags & AMDGPU_IDS_FLAGS_FUSION
     * 2. VkPhysicalDeviceType() if we ever want to use Vulkan directly
     */

    return ADL_OK;
}

int CDECL ADL_Graphics_Platform_Get(int *platform)
{
    TRACE("platform %p.\n", platform);

    return ADL2_Graphics_Platform_Get(default_ctx, platform);
}

int CDECL ADL_Display_DisplayMapConfig_Get(int adapter_index, int *display_map_count, ADLDisplayMap **display_maps,
        int *display_target_count, ADLDisplayTarget **display_targets, int options)
{
    FIXME("adapter_index %d, display_map_count %p, display_maps %p, "
            "display_target_count %p, display_targets %p, options %d stub.\n",
            adapter_index, display_map_count, display_maps, display_target_count,
            display_targets, options);

    return ADL_ERR;
}
