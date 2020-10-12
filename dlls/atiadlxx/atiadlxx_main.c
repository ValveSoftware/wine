/* Headers: https://github.com/GPUOpen-LibrariesAndSDKs/display-library */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#define COBJMACROS
#include "windef.h"
#include "winbase.h"
#include "winuser.h"
#include "objbase.h"
#include "wine/debug.h"
#include "wine/wined3d.h"

#define MAX_GPUS 64
#define VENDOR_AMD 0x1002

#define ADL_OK                            0
#define ADL_ERR                          -1
#define ADL_ERR_INVALID_PARAM            -3
#define ADL_ERR_INVALID_ADL_IDX          -5
#define ADL_ERR_NOT_SUPPORTED            -8
#define ADL_ERR_NULL_POINTER             -9

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


static struct wined3d *wined3d;

WINE_DEFAULT_DEBUG_CHANNEL(atiadlxx);

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, void *reserved)
{
    TRACE("(%p, %u, %p)\n", instance, reason, reserved);

    switch (reason)
    {
    case DLL_WINE_PREATTACH:
        return FALSE;    /* prefer native version */
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(instance);
        break;
    }

    return TRUE;
}

typedef void *(CALLBACK *ADL_MAIN_MALLOC_CALLBACK)(int);
typedef void *ADL_CONTEXT_HANDLE;

ADL_MAIN_MALLOC_CALLBACK adl_malloc;
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

static const ADLVersionsInfo version = {
    "16.11.2",
    "16.11.2",
    "",
};

static const ADLVersionsInfoX2 version2 = {
    "16.11.2",
    "16.11.2",
    "16.11.2",
    "",
};

int WINAPI ADL2_Main_Control_Create(ADL_MAIN_MALLOC_CALLBACK cb, int arg, ADL_CONTEXT_HANDLE *ptr)
{
    FIXME("cb %p, arg %d, ptr %p stub!\n", cb, arg, ptr);
    return ADL_OK;
}

int WINAPI ADL_Main_Control_Create(ADL_MAIN_MALLOC_CALLBACK cb, int arg)
{
    FIXME("cb %p, arg %d stub!\n", cb, arg);
    adl_malloc = cb;

    if ((wined3d = wined3d_create(0)))
        return ADL_OK;
    else
        return ADL_ERR;
}

int WINAPI ADL_Main_Control_Destroy(void)
{
    FIXME("stub!\n");

    if (wined3d != NULL)
        wined3d_decref(wined3d);

    return ADL_OK;
}

int WINAPI ADL2_Adapter_NumberOfAdapters_Get(ADL_CONTEXT_HANDLE *ptr, int *count)
{
    FIXME("ptr %p, count %p stub!\n", ptr, count);

    *count = 0;

    return ADL_OK;
}

int WINAPI ADL2_Graphics_VersionsX2_Get(ADL_CONTEXT_HANDLE *ptr, ADLVersionsInfoX2 *ver)
{
    FIXME("ptr %p, ver %p stub!\n", ptr, ver);
    memcpy(ver, &version2, sizeof(version2));
    return ADL_OK;
}

int WINAPI ADL_Graphics_Versions_Get(ADLVersionsInfo *ver)
{
    FIXME("ver %p stub!\n", ver);
    memcpy(ver, &version, sizeof(version));
    return ADL_OK;
}

int WINAPI ADL_Adapter_NumberOfAdapters_Get(int *count)
{
    FIXME("count %p stub!\n", count);
    *count = wined3d_get_adapter_count(wined3d);
    TRACE("*count = %d\n", *count);
    return ADL_OK;
}

static BOOL get_adapter_identifier(int index, struct wined3d_adapter_identifier *identifier)
{
    struct wined3d_adapter *adapter;
    HRESULT hr;

    adapter = wined3d_get_adapter(wined3d, index);
    if (adapter == NULL)
        return FALSE;

    memset(identifier, 0, sizeof(*identifier));
    hr = wined3d_adapter_get_identifier(adapter, 0, identifier);

    if (!SUCCEEDED(hr))
        return FALSE;

    return TRUE;
}

/* yep, seriously */
static int convert_vendor_id(int id)
{
    char str[16];
    snprintf(str, ARRAY_SIZE(str), "%x", id);
    return atoi(str);
}

int WINAPI ADL_Adapter_AdapterInfo_Get(ADLAdapterInfo *adapters, int input_size)
{
    int count, i;
    struct wined3d_adapter_identifier identifier;

    FIXME("adapters %p, input_size %d, stub!\n", adapters, input_size);

    count = wined3d_get_adapter_count(wined3d);

    if (!adapters) return ADL_ERR_INVALID_PARAM;
    if (input_size != count * sizeof(ADLAdapterInfo)) return ADL_ERR_INVALID_PARAM;

    memset(adapters, 0, input_size);

    for (i = 0; i < count; i++)
    {
        adapters[i].iSize = sizeof(ADLAdapterInfo);
        adapters[i].iAdapterIndex = i;

        if (!get_adapter_identifier(i, &identifier))
            return ADL_ERR;

        adapters[i].iVendorID = convert_vendor_id(identifier.vendor_id);
    }

    return ADL_OK;
}

int WINAPI ADL_Display_DisplayInfo_Get(int adapter_index, int *num_displays, ADLDisplayInfo **info, int force_detect)
{
    struct wined3d_adapter *adapter;
    struct wined3d_output *output;
    int i;

    FIXME("adapter %d, num_displays %p, info %p stub!\n", adapter_index, num_displays, info);

    if (info == NULL || num_displays == NULL) return ADL_ERR_NULL_POINTER;

    adapter = wined3d_get_adapter(wined3d, adapter_index);
    if (adapter == NULL) return ADL_ERR_INVALID_PARAM;

    *num_displays = wined3d_adapter_get_output_count(adapter);

    if (*num_displays == 0)
        return ADL_OK;

    *info = adl_malloc(*num_displays * sizeof(**info));
    memset(*info, 0, *num_displays * sizeof(**info));

    for (i = 0; i < *num_displays; i++)
    {
        output = wined3d_adapter_get_output(adapter, i);
        if (output == NULL)
            return ADL_ERR;

        (*info)[i].displayID.iDisplayLogicalIndex = i;
        (*info)[i].iDisplayInfoValue = ADL_DISPLAY_DISPLAYINFO_DISPLAYCONNECTED | ADL_DISPLAY_DISPLAYINFO_DISPLAYMAPPED;
        (*info)[i].iDisplayInfoMask = (*info)[i].iDisplayInfoValue;
    }

    return ADL_OK;
}

int WINAPI ADL_Adapter_Crossfire_Caps(int adapter_index, int *preffered, int *num_comb, ADLCrossfireComb** comb)
{
    FIXME("adapter %d, preffered %p, num_comb %p, comb %p stub!\n", adapter_index, preffered, num_comb, comb);
    return ADL_ERR;
}

int WINAPI ADL_Adapter_Crossfire_Get(int adapter_index, ADLCrossfireComb *comb, ADLCrossfireInfo *info)
{
    FIXME("adapter %d, comb %p, info %p, stub!\n", adapter_index, comb, info);
    return ADL_ERR;
}

int WINAPI ADL_Adapter_ASICFamilyType_Get(int adapter_index, int *asic_type, int *valids)
{
    struct wined3d_adapter_identifier identifier;

    FIXME("adapter %d, asic_type %p, valids %p, stub!\n", adapter_index, asic_type, valids);

    if (asic_type == NULL || valids == NULL)
        return  ADL_ERR_NULL_POINTER;

    if (!get_adapter_identifier(adapter_index, &identifier))
        return ADL_ERR_INVALID_ADL_IDX;

    if (identifier.vendor_id != VENDOR_AMD)
        return ADL_ERR_NOT_SUPPORTED;

    *asic_type = ADL_ASIC_DISCRETE;
    *valids = ADL_ASIC_MASK;

    return ADL_OK;
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
int WINAPI ADL_Adapter_ObservedClockInfo_Get(int adapter_index, int *core_clock, int *memory_clock)
{
    struct wined3d_adapter_identifier identifier;

    FIXME("adapter %d, core_clock %p, memory_clock %p, stub!\n", adapter_index, core_clock, memory_clock);

    if (core_clock == NULL || memory_clock == NULL) return ADL_ERR;
    if (!get_adapter_identifier(adapter_index, &identifier)) return ADL_ERR;
    if (identifier.vendor_id != VENDOR_AMD) return ADL_ERR_INVALID_ADL_IDX;

    /* default values based on RX580 */
    *core_clock = get_max_clock("sclk", 1350);
    *memory_clock = get_max_clock("mclk", 2000);

    TRACE("*core_clock: %i, *memory_clock %i\n", *core_clock, *memory_clock);

    return ADL_OK;
}

/* documented in the "Linux Specific APIs" section, present and used on Windows */
int WINAPI ADL_Adapter_MemoryInfo_Get(int adapter_index, ADLMemoryInfo *mem_info)
{
    struct wined3d_adapter_identifier identifier;

    FIXME("adapter %d, mem_info %p stub!\n", adapter_index, mem_info);

    if (mem_info == NULL) return ADL_ERR_NULL_POINTER;
    if (!get_adapter_identifier(adapter_index, &identifier)) return ADL_ERR_INVALID_ADL_IDX;
    if (identifier.vendor_id != VENDOR_AMD) return ADL_ERR;

    mem_info->iMemorySize = identifier.video_memory;
    mem_info->iMemoryBandwidth = 256000; /* not exposed on Linux, probably needs a lookup table */

    TRACE("iMemoryBandwidth %lli, iMemorySize %lli\n", mem_info->iMemoryBandwidth, mem_info->iMemorySize);
    return ADL_OK;
}

int WINAPI ADL_Graphics_Platform_Get(int *platform)
{
    struct wined3d_adapter_identifier identifier;
    int count, i;

    FIXME("platform %p, stub!\n", platform);

    *platform = GRAPHICS_PLATFORM_UNKNOWN;

    count = wined3d_get_adapter_count(wined3d);

    for (i = 0; i < count; i ++)
    {
        if (!get_adapter_identifier(i, &identifier))
            continue;

        if (identifier.vendor_id == VENDOR_AMD)
            *platform = GRAPHICS_PLATFORM_DESKTOP;
    }

    /* NOTE: The real value can be obtained by doing:
     * 1. ioctl(DRM_AMDGPU_INFO) with AMDGPU_INFO_DEV_INFO - dev_info.ids_flags & AMDGPU_IDS_FLAGS_FUSION
     * 2. VkPhysicalDeviceType() if we ever switch to wined3d vk adapter implementation
     */

    return ADL_OK;
}
