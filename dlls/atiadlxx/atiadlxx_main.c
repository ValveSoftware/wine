/* Headers: https://github.com/GPUOpen-LibrariesAndSDKs/display-library */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#define COBJMACROS
#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "winuser.h"
#include "objbase.h"
#include "initguid.h"
#include "wine/debug.h"
#include "cfgmgr32.h"
#include "devpkey.h"
#include "ntddvdeo.h"
#include "math.h"

#include "dxgi1_6.h"

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

#define ADL_MAX_PATH 256

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

typedef struct ADLGraphicCoreInfo
{
    int iGCGen;

    union
    {
        int iNumCUs;
        int iNumWGPs;
    };

    union
    {
        int iNumPEsPerCU;
        int iNumPEsPerWGP;
    };

    int iNumSIMDs;
    int iNumROPs;
    int iReserved[11];
} ADLGraphicCoreInfo;

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

#define ADL_MAX_DISPLAY_NAME 256

#define ADL_DISPLAY_DDCINFO_PIXEL_FORMAT_RGB656           0x00000001
#define ADL_DISPLAY_DDCINFO_PIXEL_FORMAT_RGB666           0x00000002
#define ADL_DISPLAY_DDCINFO_PIXEL_FORMAT_RGB888           0x00000004
#define ADL_DISPLAY_DDCINFO_PIXEL_FORMAT_RGB101010        0x00000008
#define ADL_DISPLAY_DDCINFO_PIXEL_FORMAT_RGB161616        0x00000010
#define ADL_DISPLAY_DDCINFO_PIXEL_FORMAT_RGB_RESERVED1    0x00000020
#define ADL_DISPLAY_DDCINFO_PIXEL_FORMAT_RGB_RESERVED2    0x00000040
#define ADL_DISPLAY_DDCINFO_PIXEL_FORMAT_RGB_RESERVED3    0x00000080
#define ADL_DISPLAY_DDCINFO_PIXEL_FORMAT_XRGB_BIAS101010  0x00000100
#define ADL_DISPLAY_DDCINFO_PIXEL_FORMAT_YCBCR444_8BPCC   0x00000200
#define ADL_DISPLAY_DDCINFO_PIXEL_FORMAT_YCBCR444_10BPCC  0x00000400
#define ADL_DISPLAY_DDCINFO_PIXEL_FORMAT_YCBCR444_12BPCC  0x00000800
#define ADL_DISPLAY_DDCINFO_PIXEL_FORMAT_YCBCR422_8BPCC   0x00001000
#define ADL_DISPLAY_DDCINFO_PIXEL_FORMAT_YCBCR422_10BPCC  0x00002000
#define ADL_DISPLAY_DDCINFO_PIXEL_FORMAT_YCBCR422_12BPCC  0x00004000
#define ADL_DISPLAY_DDCINFO_PIXEL_FORMAT_YCBCR420_8BPCC   0x00008000
#define ADL_DISPLAY_DDCINFO_PIXEL_FORMAT_YCBCR420_10BPCC  0x00010000
#define ADL_DISPLAY_DDCINFO_PIXEL_FORMAT_YCBCR420_12BPCC  0x00020000

#define ADL_TF_sRGB           0x0001
#define ADL_TF_BT709          0x0002
#define ADL_TF_PQ2084         0x0004
#define ADL_TF_PQ2084_INTERIM 0x0008
#define ADL_TF_LINEAR_0_1     0x0010
#define ADL_TF_LINEAR_0_125   0x0020
#define ADL_TF_DOLBYVISION    0x0040
#define ADL_TF_GAMMA_22       0x0080

#define ADL_CS_sRGB           0x0001
#define ADL_CS_BT601          0x0002
#define ADL_CS_BT709          0x0004
#define ADL_CS_BT2020         0x0008
#define ADL_CS_ADOBE          0x0010
#define ADL_CS_P3             0x0020
#define ADL_CS_scRGB_MS_REF   0x0040
#define ADL_CS_DISPLAY_NATIVE 0x0080
#define ADL_CS_APP_CONTROL    0x0100
#define ADL_CS_DOLBYVISION    0x0200

typedef struct ADLDDCInfo2
{
    int ulSize;
    int ulSupportsDDC;
    int ulManufacturerID;
    int ulProductID;
    char cDisplayName[ADL_MAX_DISPLAY_NAME];
    int ulMaxHResolution;
    int ulMaxVResolution;
    int ulMaxRefresh;
    int ulPTMCx;
    int ulPTMCy;
    int ulPTMRefreshRate;
    int ulDDCInfoFlag;
    int bPackedPixelSupported;
    int iPanelPixelFormat;
    int ulSerialID;
    int ulMinLuminanceData;
    int ulAvgLuminanceData;
    int ulMaxLuminanceData;
    int iSupportedTransferFunction;
    int iSupportedColorSpace;
    int iNativeDisplayChromaticityRedX;
    int iNativeDisplayChromaticityRedY;
    int iNativeDisplayChromaticityGreenX;
    int iNativeDisplayChromaticityGreenY;
    int iNativeDisplayChromaticityBlueX;
    int iNativeDisplayChromaticityBlueY;
    int iNativeDisplayChromaticityWhitePointX;
    int iNativeDisplayChromaticityWhitePointY;
    int iDiffuseScreenReflectance;
    int iSpecularScreenReflectance;
    int iSupportedHDR;
    int iFreesyncFlags;
    int ulMinLuminanceNoDimmingData;
    int ulMaxBacklightMaxLuminanceData;
    int ulMinBacklightMaxLuminanceData;
    int ulMaxBacklightMinLuminanceData;
    int ulMinBacklightMinLuminanceData;
    int ulScreenWidth;
    int ulScreenHeight;
    int iReserved[2];
} ADLDDCInfo2;

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

static void ascii_from_unicode( char *dst, const WCHAR *src )
{
    while ((*dst++ = *src++))
        ;
}

DEFINE_DEVPROPKEY(DEVPROPKEY_GPU_LUID, 0x60b193cb, 0x5276, 0x4d0f, 0x96, 0xfc, 0xf1, 0x73, 0xab, 0xad, 0x3e, 0xc6, 2);

struct monitor
{
    UINT32 output_id;
    int logical_adapter_index;
    int physical_adapter_index;
    char display_name[ADL_MAX_PATH];
    DXGI_OUTPUT_DESC1 dxgi_output_desc;
    BOOL found_dxgi_output;
    DISPLAYCONFIG_SOURCE_MODE mode;
    DISPLAYCONFIG_RATIONAL refresh_rate;
    DISPLAYCONFIG_ROTATION rotation;
};

struct gpu
{
    LUID luid;
    char device_string[256];
    char device_path[256];
    struct monitor *displays;
    UINT32 vendor_id;
    int display_count;
    int adapter_count;
    int first_adapter_index;
    DXGI_ADAPTER_DESC1 dxgi_adapter_desc;
};

struct adapter
{
    struct gpu *gpu;
    struct monitor *display;
    UINT32 source_id;
    char gdi_device_name[32];
    char driver_path[256];
    BOOL active;
};

typedef struct adl_context
{
    ADL_MAIN_MALLOC_CALLBACK malloc;

    struct gpu *gpus;
    int gpu_count;

    struct adapter *adapters;
    int adapter_count;
}
*ADL_CONTEXT_HANDLE;

static ADL_CONTEXT_HANDLE default_ctx;

int CDECL ADL2_Main_Control_Destroy(ADL_CONTEXT_HANDLE ctx)
{
    int i;

    TRACE("ctx %p.\n", ctx);

    if (!ctx) return ADL_ERR;
    if (ctx == default_ctx) default_ctx = NULL;

    for (i = 0; i < ctx->gpu_count; ++i)
        free(ctx->gpus[i].displays);

    free(ctx->adapters);
    free(ctx->gpus);
    free(ctx);
    return ADL_OK;
}

static int init_info(ADL_CONTEXT_HANDLE ctx)
{
    DISPLAYCONFIG_PATH_INFO *paths = NULL;
    DISPLAYCONFIG_MODE_INFO *modes = NULL;
    DXGI_ADAPTER_DESC1 adapter_desc;
    DXGI_OUTPUT_DESC1 output_desc1;
    IDXGIFactory1 *dxgi_factory = NULL;
    UINT32 path_count, mode_count;
    WCHAR *gpu_iface_list = NULL, *p;
    IDXGIAdapter1 *dxgi_adapter;
    IDXGIOutput *output;
    DISPLAY_DEVICEA dd;
    IDXGIOutput6 *output6;
    unsigned int i, j, k, count;
    int err = ADL_ERR;
    struct monitor *display;
    struct adapter *adapter;
    struct gpu *gpu;
    char buffer[256];
    WCHAR instance_id[256];
    DEVPROPTYPE type;
    DWORD ret;
    ULONG size;

    if (FAILED(CreateDXGIFactory1(&IID_IDXGIFactory1, (void**)&dxgi_factory)))
        goto done;

    if (CM_Get_Device_Interface_List_SizeW(&size, &GUID_DEVINTERFACE_DISPLAY_ADAPTER, NULL, CM_GET_DEVICE_INTERFACE_LIST_PRESENT))
        goto done;

    gpu_iface_list = malloc(size * sizeof(*gpu_iface_list));
    if (CM_Get_Device_Interface_ListW(&GUID_DEVINTERFACE_DISPLAY_ADAPTER, NULL, gpu_iface_list, size, CM_GET_DEVICE_INTERFACE_LIST_PRESENT))
        goto done;

    /* Get GPUs. */
    p = gpu_iface_list;
    count = ctx->adapter_count;
    while (*p)
    {
        DEVINST devinst;

        ascii_from_unicode(buffer, p);
        ctx->gpus = realloc(ctx->gpus, (ctx->gpu_count + 1) * sizeof(*ctx->gpus));
        memset(&ctx->gpus[ctx->gpu_count], 0, sizeof(*ctx->gpus));
        strcpy(ctx->gpus[ctx->gpu_count].device_path, buffer);
        size = ARRAY_SIZE(instance_id);
        if (CM_Get_Device_Interface_PropertyW(p, &DEVPKEY_Device_InstanceId, &type, (BYTE *)instance_id, &size, 0))
            continue;
        if (CM_Locate_DevNodeW(&devinst, instance_id, 0))
            continue;
        size = sizeof(ctx->gpus[i].luid);
        if (CM_Get_DevNode_PropertyW(devinst, &DEVPROPKEY_GPU_LUID, &type, &ctx->gpus[ctx->gpu_count].luid, &size, 0))
            continue;

        ++ctx->gpu_count;
        p += wcslen(p) + 1;
    }
    ctx->adapter_count = count;
    free(gpu_iface_list);

    /* Get adapters and monitors. */
    do
    {
        if (GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &path_count, &mode_count))
            goto done;

        paths = realloc(paths, sizeof(*paths) * path_count);
        modes = realloc(modes, sizeof(*modes) * mode_count);

        ret = QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &path_count, paths, &mode_count, modes, NULL);
    } while (ret == ERROR_INSUFFICIENT_BUFFER);
    if (ret) goto done;

    for (i = 0; i < path_count; ++i)
    {
        DISPLAYCONFIG_SOURCE_DEVICE_NAME source_name;
        DISPLAYCONFIG_TARGET_DEVICE_NAME target_name;

        for (j = 0; j < ctx->gpu_count; ++j)
        {
            if (!memcmp(&ctx->gpus[j].luid, &paths[i].sourceInfo.adapterId, sizeof(ctx->gpus[j].luid))) break;
        }
        if (j == ctx->gpu_count)
        {
            ERR("Adapter luid %#x:%#x not found in gpu list.\n",
                    (int)paths[i].sourceInfo.adapterId.HighPart, (int)paths[i].sourceInfo.adapterId.LowPart );
            continue;
        }
        gpu = &ctx->gpus[j];

        for (j = 0; j < ctx->adapter_count; ++j)
        {
            if (ctx->adapters[j].source_id == paths[i].sourceInfo.id) break;
        }
        if (j == ctx->adapter_count)
        {
            ctx->adapters = realloc(ctx->adapters, (j + 1) * sizeof(*ctx->adapters));
            adapter = &ctx->adapters[j];
            memset(adapter, 0, sizeof(*ctx->adapters));
            adapter->gpu = gpu;
            adapter->source_id = paths[i].sourceInfo.id;

            source_name.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
            source_name.header.size = sizeof(source_name);
            source_name.header.adapterId = paths[i].sourceInfo.adapterId;
            source_name.header.id = paths[i].sourceInfo.id;
            if (DisplayConfigGetDeviceInfo(&source_name.header)) goto done;
            ascii_from_unicode(adapter->gdi_device_name, source_name.viewGdiDeviceName);

            if (!gpu->adapter_count) gpu->first_adapter_index = ctx->adapter_count;
            ++ctx->adapter_count;
            ++gpu->adapter_count;
        }
        else adapter = &ctx->adapters[j];

        for (j = 0; j < gpu->display_count; ++j)
        {
            if (paths[i].targetInfo.id == gpu->displays[j].output_id) break;
        }

        if (j == gpu->display_count)
        {
            gpu->displays = realloc(gpu->displays, (gpu->display_count + 1) * sizeof(*gpu->displays));
            display = &gpu->displays[gpu->display_count];
            memset(display, 0, sizeof(*gpu->displays));
            display->output_id = paths[i].targetInfo.id;
            display->physical_adapter_index = gpu->first_adapter_index;
            display->logical_adapter_index = adapter - &ctx->adapters[0];
            target_name.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME;
            target_name.header.size = sizeof(target_name);
            target_name.header.adapterId = paths[i].targetInfo.adapterId;
            target_name.header.id = paths[i].targetInfo.id;
            if (DisplayConfigGetDeviceInfo(&target_name.header)) goto done;
            ascii_from_unicode(display->display_name, target_name.monitorFriendlyDeviceName);
            display->mode = modes[paths[i].sourceInfo.modeInfoIdx].sourceMode;
            display->refresh_rate = paths[i].targetInfo.refreshRate;
            display->rotation = paths[i].targetInfo.rotation;
            if (adapter->display)
                ERR("Display is already assigned for adapter %s.\n", adapter->gdi_device_name);
            adapter->display = display;
            adapter->active = TRUE;
            ++gpu->display_count;
        }
    }

    /* Get driver path and device name for adapters. */
    dd.cb = sizeof(dd);
    for (i = 0; EnumDisplayDevicesA(NULL, i, &dd, EDD_GET_DEVICE_INTERFACE_NAME); ++i)
    {
        for (j = 0; j < ctx->adapter_count; ++j)
        {
            if (stricmp(ctx->adapters[i].gdi_device_name, dd.DeviceName)) continue;
            strcpy(ctx->adapters[i].gpu->device_string, dd.DeviceString);
            strcpy(ctx->adapters[i].driver_path, dd.DeviceKey);
        }
    }

    /* Fake adapters for GPUs without outputs. */
    for (i = 0; i < ctx->gpu_count; ++i)
    {
        gpu = &ctx->gpus[i];
        if (gpu->adapter_count) continue;
        ctx->adapters = realloc(ctx->adapters, (ctx->adapter_count + 1) * sizeof(*ctx->adapters));
        adapter = &ctx->adapters[ctx->adapter_count];
        memset(adapter, 0, sizeof(*ctx->adapters));
        adapter->gpu = gpu;
        gpu->first_adapter_index = ctx->adapter_count;
        ++gpu->adapter_count;
        ++ctx->adapter_count;
        sprintf(adapter->gdi_device_name, "\\\\.\\DISPLAY%d", ctx->adapter_count);
        ERR("No adapter for GPU %s, using fake %s.\n", gpu->device_string, adapter->gdi_device_name);
    }

    /* Initialize info queried from dxgi. */
    for (i = 0; !IDXGIFactory1_EnumAdapters1(dxgi_factory, i, &dxgi_adapter); ++i)
    {
        if (FAILED(IDXGIAdapter1_GetDesc1(dxgi_adapter, &adapter_desc)))
        {
            IDXGIAdapter1_Release(dxgi_adapter);
            continue;
        }

        for (j = 0; j < ctx->gpu_count; ++j)
        {
            if (memcmp(&adapter_desc.AdapterLuid, &ctx->gpus[j].luid, sizeof(adapter_desc.AdapterLuid))) continue;
            gpu = &ctx->gpus[j];
            gpu->dxgi_adapter_desc = adapter_desc;
            gpu->vendor_id = adapter_desc.VendorId;
            if (!gpu->device_string[0])
                ascii_from_unicode(gpu->device_string, adapter_desc.Description);
            break;
        }
        if (j == ctx->gpu_count)
            ERR("Could not find gpu for dxgi adapter device %s.\n", debugstr_w(adapter_desc.Description));

        for (j = 0; !IDXGIAdapter1_EnumOutputs(dxgi_adapter, j, &output); ++j)
        {
            if (IDXGIOutput_QueryInterface(output, &IID_IDXGIOutput6, (void**)&output6))
            {
                IDXGIOutput_Release(output);
                continue;
            }
            if (IDXGIOutput6_GetDesc1(output6, &output_desc1))
            {
                IDXGIOutput_Release(output);
                IDXGIOutput6_Release(output6);
                continue;
            }

            ascii_from_unicode(buffer, output_desc1.DeviceName);
            for (k = 0; k < ctx->adapter_count; ++k)
            {
                adapter = &ctx->adapters[k];
                if (stricmp(buffer, adapter->gdi_device_name) || !adapter->display) continue;
                adapter->display->dxgi_output_desc = output_desc1;
                adapter->display->found_dxgi_output = TRUE;
                adapter->active = TRUE;
                break;
            }
            if (k == ctx->adapter_count)
                ERR("No adapter found for dxgi output %s.\n", debugstr_w(output_desc1.DeviceName));
            IDXGIOutput6_Release(output6);
            IDXGIOutput_Release(output);
        }
        IDXGIAdapter1_Release(dxgi_adapter);
    }
    for (i = 0; i < ctx->adapter_count; ++i)
    {
        if (!(display = ctx->adapters[i].display)) continue;
        if (!display->found_dxgi_output)
        {
            ERR("No dxgi output found for display %d, %s, adapter %d, %s.\n",
                    i, display->display_name, display->logical_adapter_index,
                    ctx->adapters[i].gdi_device_name);
        }
    }
    err = ADL_OK;

done:
    free(paths);
    free(modes);
    if (dxgi_factory) IDXGIFactory1_Release(dxgi_factory);
    return err;
}

int CDECL ADL2_Main_Control_Create(ADL_MAIN_MALLOC_CALLBACK cb, int arg, ADL_CONTEXT_HANDLE *ptr)
{
    ADL_CONTEXT_HANDLE ctx;
    int ret;

    TRACE("cb %p, arg %d, ptr %p.\n", cb, arg, ptr);

    ctx = calloc( 1, sizeof(**ptr));
    ctx->malloc = cb;
    if ((ret = init_info(ctx)))
    {
        ERR("Initialization failed.\n");
        ADL2_Main_Control_Destroy(ctx);
        return ret;
    }
    if (default_ctx) ADL2_Main_Control_Destroy(default_ctx);
    default_ctx = ctx;
    *ptr = ctx;
    TRACE("-> %p.\n", *ptr);
    return ADL_OK;
}

int CDECL ADL_Main_Control_Create(ADL_MAIN_MALLOC_CALLBACK cb, int arg)
{
    TRACE("cb %p, arg %d.\n", cb, arg);

    return ADL2_Main_Control_Create(cb, arg, &default_ctx);
}

int CDECL ADL_Main_Control_Destroy(void)
{
    TRACE(".\n");

    return ADL2_Main_Control_Destroy(default_ctx);
}

int CDECL ADL2_Adapter_NumberOfAdapters_Get(ADL_CONTEXT_HANDLE ctx, int *count)
{
    TRACE("ctx %p, count %p.\n", ctx, count);

    *count = ctx->adapter_count;
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
    char buffer[256], *p;

    memset(adapters, 0, input_size);

    for (i = 0; i < count; i++)
    {
        adapters[i].iSize = sizeof(ADLAdapterInfo);
        adapters[i].iAdapterIndex = i;
        adapters[i].iOSDisplayIndex = i;

        strcpy(buffer, ctx->adapters[i].gpu->device_path + 4);
        if ((p = strrchr(buffer, '#'))) *p = 0;
        for (p = buffer; *p; ++p)
            if (*p == '#') *p = '\\';
        if (ctx->adapters[i].source_id)
            sprintf(adapters[i].strPNPString, "%s&%02d", buffer, ctx->adapters[i].source_id + 1);
        else
            strcpy(adapters[i].strPNPString, buffer);
        strcpy(adapters[i].strUDID, adapters[i].strPNPString);
        for (p = adapters[i].strUDID; *p; ++p)
            if (*p == '\\') *p = '_';
        *p++ = 'A';
        *p = 0;

        adapters[i].iVendorID = convert_vendor_id(ctx->adapters[i].gpu->vendor_id);
        strcpy(adapters[i].strAdapterName, ctx->adapters[i].gpu->device_string);
        strcpy(adapters[i].strDisplayName, ctx->adapters[i].gdi_device_name);
        strcpy(adapters[i].strDriverPath, ctx->adapters[i].driver_path);
    }
    return ADL_OK;
}

int CDECL ADL_Adapter_AdapterInfo_Get(ADLAdapterInfo *adapters, int input_size)
{
    TRACE("adapters %p, input_size %d.\n", adapters, input_size);

    if (!adapters) return ADL_ERR_INVALID_PARAM;
    if (input_size != default_ctx->adapter_count * sizeof(ADLAdapterInfo)) return ADL_ERR_INVALID_PARAM;

    return adapter_info_get(default_ctx, adapters, input_size);
}

int CDECL ADL2_Adapter_AdapterInfoX2_Get(ADL_CONTEXT_HANDLE ctx, ADLAdapterInfo **info)
{
    TRACE("ctx %p, info %p.\n", ctx, info);

    *info = ctx->malloc( ctx->adapter_count * sizeof(**info) );
    return adapter_info_get(ctx, *info, ctx->adapter_count * sizeof(**info));
}

int CDECL ADL2_Adapter_Active_Get(ADL_CONTEXT_HANDLE ctx, int adapter_index, int *status)
{
    TRACE("ctx %p, adapter_index %d, status %p.\n", ctx, adapter_index, status);

    if (adapter_index >= ctx->adapter_count) return ADL_ERR_INVALID_ADL_IDX;
    *status = ctx->adapters[adapter_index].active;
    return ADL_OK;
}

int CDECL ADL2_Display_DisplayInfo_Get(ADL_CONTEXT_HANDLE ctx, int adapter_index, int *num_displays, ADLDisplayInfo **info, int force_detect)
{
    struct gpu *gpu;
    int i;

    TRACE("adapter %d, num_displays %p, info %p.\n", adapter_index, num_displays, info);

    if (info == NULL || num_displays == NULL) return ADL_ERR_NULL_POINTER;

    if (adapter_index >= ctx->adapter_count) return ADL_ERR_INVALID_PARAM;

    gpu = ctx->adapters[adapter_index].gpu;
    *num_displays = gpu->display_count;

    *info = ctx->malloc(*num_displays * sizeof(**info));
    memset(*info, 0, *num_displays * sizeof(**info));

    for (i = 0; i < *num_displays; i++)
    {
        (*info)[i].displayID.iDisplayLogicalAdapterIndex = gpu->displays[i].logical_adapter_index;
        (*info)[i].displayID.iDisplayLogicalIndex = i;
        (*info)[i].displayID.iDisplayPhysicalAdapterIndex = gpu->displays[i].physical_adapter_index;
        (*info)[i].displayID.iDisplayPhysicalIndex = i;
        strcpy((*info)[i].strDisplayName, gpu->displays[i].display_name);
        (*info)[i].iDisplayType = 2 /* ADL_DT_LCD_PANEL */;
        (*info)[i].iDisplayOutputType = 4 /* ADL_DOT_DIGITAL */;
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

static int chroma_value_conv(float v)
{
    return lrintf(v * 10000);
}

int CDECL ADL2_Display_DDCInfo2_Get(ADL_CONTEXT_HANDLE ctx, int adapter_index, int display_index, ADLDDCInfo2 *info)
{
    DXGI_OUTPUT_DESC1 *desc;
    struct monitor *display;
    struct gpu *gpu;
    FIXME("ctx %p, adapter_index %d, display_index %d, info %p semi-stub.\n", ctx, adapter_index,
            display_index, info);
    memset(info, 0, sizeof(*info));
    info->ulSize = sizeof(*info);

    if (adapter_index >= ctx->adapter_count) return ADL_ERR_INVALID_PARAM;
    gpu = ctx->adapters[adapter_index].gpu;
    if (display_index >= gpu->display_count) return ADL_OK;
    display = &gpu->displays[display_index];

    desc = &display->dxgi_output_desc;
    info->ulSupportsDDC = 1;
    strcpy(info->cDisplayName, display->display_name);
    info->iSupportedHDR = (desc->ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020);
    info->iPanelPixelFormat = ADL_DISPLAY_DDCINFO_PIXEL_FORMAT_RGB888;
    info->iSupportedTransferFunction = ADL_TF_sRGB | ADL_TF_LINEAR_0_1;
    info->iSupportedColorSpace = ADL_CS_sRGB;
    if (info->iSupportedHDR)
    {
        TRACE("HDR is supported.\n");
        info->iPanelPixelFormat |= ADL_DISPLAY_DDCINFO_PIXEL_FORMAT_RGB101010;
        info->iSupportedTransferFunction |= ADL_TF_PQ2084;
        info->iSupportedColorSpace |= ADL_CS_BT2020;
    }
    info->iNativeDisplayChromaticityRedX = chroma_value_conv(desc->RedPrimary[0]);
    info->iNativeDisplayChromaticityRedY = chroma_value_conv(desc->RedPrimary[1]);
    info->iNativeDisplayChromaticityGreenX = chroma_value_conv(desc->GreenPrimary[0]);
    info->iNativeDisplayChromaticityGreenY = chroma_value_conv(desc->GreenPrimary[1]);
    info->iNativeDisplayChromaticityBlueX = chroma_value_conv(desc->BluePrimary[0]);
    info->iNativeDisplayChromaticityBlueY = chroma_value_conv(desc->BluePrimary[1]);
    info->iNativeDisplayChromaticityWhitePointX = chroma_value_conv(desc->WhitePoint[0]);
    info->iNativeDisplayChromaticityWhitePointY = chroma_value_conv(desc->WhitePoint[1]);

    info->ulMinLuminanceData = desc->MinLuminance;
    info->ulMaxLuminanceData = desc->MaxLuminance;
    info->ulAvgLuminanceData = desc->MaxFullFrameLuminance;
    return ADL_OK;
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
    FIXME("adapter %d, asic_type %p, valids %p, stub.\n", adapter_index, asic_type, valids);

    if (asic_type == NULL || valids == NULL)
        return ADL_ERR_NULL_POINTER;

    if (ctx->adapters[adapter_index].gpu->vendor_id != VENDOR_AMD)
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
    FIXME("ctx %p, adapter %d, core_clock %p, memory_clock %p, stub.\n", ctx, adapter_index, core_clock, memory_clock);

    if (core_clock == NULL || memory_clock == NULL) return ADL_ERR;
    if (adapter_index >= ctx->adapter_count) return ADL_ERR_INVALID_ADL_IDX;
    if (ctx->adapters[adapter_index].gpu->vendor_id != VENDOR_AMD) return ADL_ERR_INVALID_ADL_IDX;

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


int CDECL ADL2_Adapter_Graphic_Core_Info_Get(ADL_CONTEXT_HANDLE context, int adapter_index, ADLGraphicCoreInfo *info)
{
    FIXME("context %p, adapter_index %d, info %p stub.\n", context, adapter_index, info);

    memset(info, 0, sizeof(*info));
    info->iNumPEsPerCU = 1000;
    info->iNumCUs = 1000;
    info->iGCGen = 100;
    info->iNumROPs = 10000;
    info->iNumSIMDs = 10000;
    return ADL_OK;
}

/* documented in the "Linux Specific APIs" section, present and used on Windows */
int CDECL ADL2_Adapter_MemoryInfo_Get(ADL_CONTEXT_HANDLE ctx, int adapter_index, ADLMemoryInfo *mem_info)
{
    FIXME("ctx %p, adapter %d, mem_info %p stub.\n", ctx, adapter_index, mem_info);

    if (mem_info == NULL) return ADL_ERR_NULL_POINTER;
    if (adapter_index >= ctx->adapter_count) return ADL_ERR_INVALID_ADL_IDX;
    if (ctx->adapters[adapter_index].gpu->vendor_id != VENDOR_AMD) return ADL_ERR;

    mem_info->iMemorySize = ctx->adapters[adapter_index].gpu->dxgi_adapter_desc.DedicatedVideoMemory;
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
    int i;

    FIXME("platform %p, stub.\n", platform);

    *platform = GRAPHICS_PLATFORM_UNKNOWN;

    for (i = 0; i < ctx->gpu_count; ++i)
    {
        if (ctx->gpus[i].vendor_id == VENDOR_AMD)
        {
            *platform = GRAPHICS_PLATFORM_DESKTOP;
            break;
        }
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

int CDECL ADL2_Display_DisplayMapConfig_Get(ADL_CONTEXT_HANDLE ctx, int adapter_index, int *display_map_count, ADLDisplayMap **display_maps,
        int *display_target_count, ADLDisplayTarget **display_targets, int options)
{
    struct gpu *gpu;
    int i;

    TRACE("ctx %p, adapter_index %d, display_map_count %p, display_maps %p, "
            "display_target_count %p, display_targets %p, options %d.\n",
            ctx, adapter_index, display_map_count, display_maps, display_target_count,
            display_targets, options);

    if (adapter_index >= ctx->adapter_count) return ADL_ERR_INVALID_ADL_IDX;
    gpu = ctx->adapters[adapter_index].gpu;
    if (!gpu->display_count) return ADL_ERR_NOT_SUPPORTED;
    *display_map_count = gpu->display_count;
    *display_maps = ctx->malloc(*display_map_count * sizeof(**display_maps));
    memset(*display_maps, 0, *display_map_count * sizeof(**display_maps));
    *display_target_count = gpu->display_count;
    *display_targets = ctx->malloc(*display_target_count * sizeof(**display_targets));
    memset(*display_targets, 0, *display_target_count * sizeof(**display_targets));

    for (i = 0; i < gpu->display_count; ++i)
    {
        ADLMode *m = (ADLMode *)&(*display_maps)[i].displayMode;
        DISPLAYCONFIG_SOURCE_MODE *dc_mode = &gpu->displays[i].mode;

        (*display_maps)[i].iDisplayMapIndex = gpu->displays[i].logical_adapter_index;
        (*display_maps)[i].iNumDisplayTarget = 1;
        (*display_maps)[i].iFirstDisplayTargetArrayIndex = i;
        (*display_maps)[i].iDisplayMapMask = 0xf;
        (*display_maps)[i].iDisplayMapValue = 0x4; /* ADL_DISPLAY_DISPLAYMAP_MANNER_SINGLE */

        m->displayID.iDisplayLogicalAdapterIndex = gpu->displays[i].logical_adapter_index;
        m->displayID.iDisplayLogicalIndex = i;
        m->iAdapterIndex = gpu->displays[i].logical_adapter_index;
        m->iXPos = dc_mode->position.x;
        m->iYPos = dc_mode->position.y;
        m->iXRes = dc_mode->width;
        m->iYRes = dc_mode->height;
        m->iColourDepth = 32;
        m->fRefreshRate = (float)gpu->displays[i].refresh_rate.Numerator / gpu->displays[i].refresh_rate.Denominator;
        m->iOrientation = (gpu->displays[i].rotation - 1) * 90;
        m->iModeMask = 0xff;
        m->iModeValue = 0x46;

        (*display_targets)[i].displayID = m->displayID;
        (*display_targets)[i].iDisplayMapIndex = i;
        (*display_targets)[i].iDisplayTargetMask = 1;
        (*display_targets)[i].iDisplayTargetValue = 1;
    }
    return ADL_OK;
}

int CDECL ADL_Display_DisplayMapConfig_Get(int adapter_index, int *display_map_count, ADLDisplayMap **display_maps,
        int *display_target_count, ADLDisplayTarget **display_targets, int options)
{
    TRACE("adapter_index %d, display_map_count %p, display_maps %p, "
            "display_target_count %p, display_targets %p, options %d.\n",
            adapter_index, display_map_count, display_maps, display_target_count,
            display_targets, options);

    return ADL2_Display_DisplayMapConfig_Get(default_ctx, adapter_index, display_map_count, display_maps,
            display_target_count, display_targets, options);
}
