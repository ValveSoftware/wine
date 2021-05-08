#include <stdarg.h>
#include <stdbool.h>
#include <assert.h>

#include "windef.h"
#include "winbase.h"
#include "wine/debug.h"
#include "wine/heap.h"

#include "wine/vulkan.h"

#define COBJMACROS
#include "d3d11.h"
#include "d3d12.h"

#include "amd_ags.h"

WINE_DEFAULT_DEBUG_CHANNEL(amd_ags);

enum amd_ags_version
{
    AMD_AGS_VERSION_5_1_1,
    AMD_AGS_VERSION_5_2_0,
    AMD_AGS_VERSION_5_2_1,
    AMD_AGS_VERSION_5_3_0,
    AMD_AGS_VERSION_5_4_0,
    AMD_AGS_VERSION_5_4_1,

    AMD_AGS_VERSION_COUNT
};

static const struct
{
    int major;
    int minor;
    int patch;
    unsigned int device_size;
}
amd_ags_info[AMD_AGS_VERSION_COUNT] =
{
    {5, 1, 1, sizeof(AGSDeviceInfo_511)},
    {5, 2, 0, sizeof(AGSDeviceInfo_520)},
    {5, 2, 1, sizeof(AGSDeviceInfo_520)},
    {5, 3, 0, sizeof(AGSDeviceInfo_520)},
    {5, 4, 0, sizeof(AGSDeviceInfo_540)},
    {5, 4, 1, sizeof(AGSDeviceInfo_541)},
};

#define DEF_FIELD(name) {DEVICE_FIELD_##name, {offsetof(AGSDeviceInfo_511, name), offsetof(AGSDeviceInfo_520, name), \
        offsetof(AGSDeviceInfo_520, name), offsetof(AGSDeviceInfo_520, name), offsetof(AGSDeviceInfo_540, name), \
        offsetof(AGSDeviceInfo_541, name)}}
#define DEF_FIELD_520_BELOW(name) {DEVICE_FIELD_##name, {offsetof(AGSDeviceInfo_511, name), offsetof(AGSDeviceInfo_520, name), \
        offsetof(AGSDeviceInfo_520, name), offsetof(AGSDeviceInfo_520, name), -1, \
        -1}}
#define DEF_FIELD_540_UP(name) {DEVICE_FIELD_##name, {-1, -1, \
        -1, -1, offsetof(AGSDeviceInfo_540, name), \
        offsetof(AGSDeviceInfo_541, name)}}

#define DEVICE_FIELD_adapterString 0
#define DEVICE_FIELD_architectureVersion 1
#define DEVICE_FIELD_asicFamily 2
#define DEVICE_FIELD_vendorId 3
#define DEVICE_FIELD_deviceId 4
#define DEVICE_FIELD_isPrimaryDevice 5
#define DEVICE_FIELD_localMemoryInBytes 6
#define DEVICE_FIELD_numDisplays 7
#define DEVICE_FIELD_displays 8

static const struct
{
    unsigned int field_index;
    int offset[AMD_AGS_VERSION_COUNT];
}
device_struct_fields[] =
{
    DEF_FIELD(adapterString),
    DEF_FIELD_520_BELOW(architectureVersion),
    DEF_FIELD_540_UP(asicFamily),
    DEF_FIELD(vendorId),
    DEF_FIELD(deviceId),
    DEF_FIELD(isPrimaryDevice),
    DEF_FIELD(localMemoryInBytes),
    DEF_FIELD(numDisplays),
    DEF_FIELD(displays),
};

#undef DEF_FIELD

#define GET_DEVICE_FIELD_ADDR(device, name, type, version) \
        (device_struct_fields[DEVICE_FIELD_##name].offset[version] == -1 ? NULL \
        : (type *)((BYTE *)device + device_struct_fields[DEVICE_FIELD_##name].offset[version]))

#define SET_DEVICE_FIELD(device, name, type, version, value) { \
        type *addr; \
        if ((addr = GET_DEVICE_FIELD_ADDR(device, name, type, version))) \
            *addr = value; \
    }

struct AGSContext
{
    enum amd_ags_version version;
    unsigned int device_count;
    struct AGSDeviceInfo *devices;
    VkPhysicalDeviceProperties *properties;
    VkPhysicalDeviceMemoryProperties *memory_properties;
};

static HMODULE hd3d12;
static typeof(D3D12CreateDevice) *pD3D12CreateDevice;

static BOOL load_d3d12_functions(void)
{
    if (hd3d12)
        return TRUE;

    if (!(hd3d12 = LoadLibraryA("d3d12.dll")))
        return FALSE;

    pD3D12CreateDevice = (void *)GetProcAddress(hd3d12, "D3D12CreateDevice");
    return TRUE;
}

static AGSReturnCode vk_get_physical_device_properties(unsigned int *out_count,
        VkPhysicalDeviceProperties **out, VkPhysicalDeviceMemoryProperties **out_memory)
{
    VkPhysicalDeviceProperties *properties = NULL;
    VkPhysicalDeviceMemoryProperties *memory_properties = NULL;
    VkPhysicalDevice *vk_physical_devices = NULL;
    VkInstance vk_instance = VK_NULL_HANDLE;
    VkInstanceCreateInfo create_info;
    AGSReturnCode ret = AGS_SUCCESS;
    uint32_t count, i;
    VkResult vr;

    *out = NULL;
    *out_count = 0;

    memset(&create_info, 0, sizeof(create_info));
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    if ((vr = vkCreateInstance(&create_info, NULL, &vk_instance) < 0))
    {
        WARN("Failed to create Vulkan instance, vr %d.\n", vr);
        goto done;
    }

    if ((vr = vkEnumeratePhysicalDevices(vk_instance, &count, NULL)) < 0)
    {
        WARN("Failed to enumerate devices, vr %d.\n", vr);
        goto done;
    }

    if (!(vk_physical_devices = heap_calloc(count, sizeof(*vk_physical_devices))))
    {
        WARN("Failed to allocate memory.\n");
        ret = AGS_OUT_OF_MEMORY;
        goto done;
    }

    if ((vr = vkEnumeratePhysicalDevices(vk_instance, &count, vk_physical_devices)) < 0)
    {
        WARN("Failed to enumerate devices, vr %d.\n", vr);
        goto done;
    }

    if (!(properties = heap_calloc(count, sizeof(*properties))))
    {
        WARN("Failed to allocate memory.\n");
        ret = AGS_OUT_OF_MEMORY;
        goto done;
    }

    if (!(memory_properties = heap_calloc(count, sizeof(*memory_properties))))
    {
        WARN("Failed to allocate memory.\n");
        heap_free(properties);
        ret = AGS_OUT_OF_MEMORY;
        goto done;
    }

    for (i = 0; i < count; ++i)
        vkGetPhysicalDeviceProperties(vk_physical_devices[i], &properties[i]);

    for (i = 0; i < count; ++i)
        vkGetPhysicalDeviceMemoryProperties(vk_physical_devices[i], &memory_properties[i]);

    *out_count = count;
    *out = properties;
    *out_memory = memory_properties;

done:
    heap_free(vk_physical_devices);
    if (vk_instance)
        vkDestroyInstance(vk_instance, NULL);
    return ret;
}

static enum amd_ags_version determine_ags_version(void)
{
    /* AMD AGS is not binary compatible between versions (even minor versions), and the game
     * does not request a specific version when calling agsInit().
     * Checking the version of amd_ags_x64.dll shipped with the game is the only way to
     * determine what version the game was built against.
     *
     * An update to AGS 5.4.1 included an amd_ags_x64.dll with no file version info.
     * In case of an error, assume it's that version.
     */
    enum amd_ags_version ret = AMD_AGS_VERSION_5_4_1;
    DWORD infosize;
    void *infobuf = NULL;
    void *val;
    UINT vallen, i;
    VS_FIXEDFILEINFO *info;
    UINT16 major, minor, patch;

    infosize = GetFileVersionInfoSizeW(L"amd_ags_x64.dll", NULL);
    if (!infosize)
    {
        WARN("Unable to determine desired version of amd_ags_x64.dll.\n");
        goto done;
    }

    if (!(infobuf = heap_alloc(infosize)))
    {
        WARN("Failed to allocate memory.\n");
        goto done;
    }

    if (!GetFileVersionInfoW(L"amd_ags_x64.dll", 0, infosize, infobuf))
    {
        WARN("Unable to determine desired version of amd_ags_x64.dll.\n");
        goto done;
    }

    if (!VerQueryValueW(infobuf, L"\\", &val, &vallen) || (vallen != sizeof(VS_FIXEDFILEINFO)))
    {
        WARN("Unable to determine desired version of amd_ags_x64.dll.\n");
        goto done;
    }

    info = val;
    major = info->dwFileVersionMS >> 16;
    minor = info->dwFileVersionMS;
    patch = info->dwFileVersionLS >> 16;
    TRACE("Found amd_ags_x64.dll v%d.%d.%d\n", major, minor, patch);

    for (i = 0; i < ARRAY_SIZE(amd_ags_info); i++)
    {
        if ((major == amd_ags_info[i].major) &&
            (minor == amd_ags_info[i].minor) &&
            (patch == amd_ags_info[i].patch))
        {
            ret = i;
            break;
        }
    }

done:
    heap_free(infobuf);
    TRACE("Using AGS v%d.%d.%d interface\n",
          amd_ags_info[ret].major, amd_ags_info[ret].minor, amd_ags_info[ret].patch);
    return ret;
}

struct monitor_enum_context
{
    const char *adapter_name;
    AGSDisplayInfo **ret_displays;
    int *ret_display_count;
};

static BOOL WINAPI monitor_enum_proc(HMONITOR hmonitor, HDC hdc, RECT *rect, LPARAM context)
{
    struct monitor_enum_context *c = (struct monitor_enum_context *)context;
    MONITORINFOEXA monitor_info;
    AGSDisplayInfo *new_alloc;
    DISPLAY_DEVICEA device;
    AGSDisplayInfo *info;
    unsigned int i, mode;
    DEVMODEA dev_mode;


    monitor_info.cbSize = sizeof(monitor_info);
    GetMonitorInfoA(hmonitor, (MONITORINFO *)&monitor_info);
    TRACE("monitor_info.szDevice %s.\n", debugstr_a(monitor_info.szDevice));

    device.cb = sizeof(device);
    i = 0;
    while (EnumDisplayDevicesA(NULL, i, &device, 0))
    {
        TRACE("device.DeviceName %s, device.DeviceString %s.\n", debugstr_a(device.DeviceName), debugstr_a(device.DeviceString));
        ++i;
        if (strcmp(device.DeviceString, c->adapter_name) || strcmp(device.DeviceName, monitor_info.szDevice))
            continue;

        if (*c->ret_display_count)
        {
            if (!(new_alloc = heap_realloc(*c->ret_displays, sizeof(*new_alloc) * (*c->ret_display_count + 1))))
            {
                ERR("No memory.");
                return FALSE;
            }
            *c->ret_displays = new_alloc;
        }
        else if (!(*c->ret_displays = heap_alloc(sizeof(**c->ret_displays))))
        {
            ERR("No memory.");
            return FALSE;
        }
        info = &(*c->ret_displays)[*c->ret_display_count];
        memset(info, 0, sizeof(*info));
        strcpy(info->displayDeviceName, device.DeviceName);
        if (EnumDisplayDevicesA(info->displayDeviceName, 0, &device, 0))
        {
            strcpy(info->name, device.DeviceString);
        }
        else
        {
            ERR("Could not get monitor name for device %s.\n", debugstr_a(info->displayDeviceName));
            strcpy(info->name, "Unknown");
        }
        if (monitor_info.dwFlags & MONITORINFOF_PRIMARY)
            info->displayFlags |= AGS_DISPLAYFLAG_PRIMARY_DISPLAY;

        mode = 0;
        memset(&dev_mode, 0, sizeof(dev_mode));
        dev_mode.dmSize = sizeof(dev_mode);
        while (EnumDisplaySettingsExA(monitor_info.szDevice, mode, &dev_mode, EDS_RAWMODE))
        {
            ++mode;
            if (dev_mode.dmPelsWidth > info->maxResolutionX)
                info->maxResolutionX = dev_mode.dmPelsWidth;
            if (dev_mode.dmPelsHeight > info->maxResolutionY)
                info->maxResolutionY = dev_mode.dmPelsHeight;
            if (dev_mode.dmDisplayFrequency > info->maxRefreshRate)
                info->maxRefreshRate = dev_mode.dmDisplayFrequency;
            memset(&dev_mode, 0, sizeof(dev_mode));
            dev_mode.dmSize = sizeof(dev_mode);
        }

        info->currentResolution.offsetX = monitor_info.rcMonitor.left;
        info->currentResolution.offsetY = monitor_info.rcMonitor.top;
        info->currentResolution.width = monitor_info.rcMonitor.right - monitor_info.rcMonitor.left;
        info->currentResolution.height = monitor_info.rcMonitor.bottom - monitor_info.rcMonitor.top;
        info->visibleResolution = info->currentResolution;

        memset(&dev_mode, 0, sizeof(dev_mode));
        dev_mode.dmSize = sizeof(dev_mode);

        if (EnumDisplaySettingsExA(monitor_info.szDevice, ENUM_CURRENT_SETTINGS, &dev_mode, EDS_RAWMODE))
            info->currentRefreshRate = dev_mode.dmDisplayFrequency;
        else
            ERR("Could not get current display settings.\n");
        ++*c->ret_display_count;

        TRACE("Added display %s for %s.\n", debugstr_a(monitor_info.szDevice), debugstr_a(c->adapter_name));
    }

    return TRUE;
}

static void init_device_displays(const char *adapter_name, AGSDisplayInfo **ret_displays, int *ret_display_count)
{
    struct monitor_enum_context context;

    TRACE("adapter_name %s.\n", debugstr_a(adapter_name));

    context.adapter_name = adapter_name;
    context.ret_displays = ret_displays;
    context.ret_display_count = ret_display_count;

    EnumDisplayMonitors(NULL, NULL, monitor_enum_proc, (LPARAM)&context);
}

static AGSReturnCode init_ags_context(AGSContext *context)
{
    AGSReturnCode ret;
    unsigned int i, j;
    BYTE *device;

    context->version = determine_ags_version();
    context->device_count = 0;
    context->devices = NULL;
    context->properties = NULL;
    context->memory_properties = NULL;

    ret = vk_get_physical_device_properties(&context->device_count, &context->properties, &context->memory_properties);
    if (ret != AGS_SUCCESS || !context->device_count)
        return ret;

    assert(context->version < AMD_AGS_VERSION_COUNT);

    if (!(context->devices = heap_calloc(context->device_count, amd_ags_info[context->version].device_size)))
    {
        WARN("Failed to allocate memory.\n");
        heap_free(context->properties);
        heap_free(context->memory_properties);
        return AGS_OUT_OF_MEMORY;
    }

    device = (BYTE *)context->devices;
    for (i = 0; i < context->device_count; ++i)
    {
        const VkPhysicalDeviceProperties *vk_properties = &context->properties[i];
        const VkPhysicalDeviceMemoryProperties *vk_memory_properties = &context->memory_properties[i];
        VkDeviceSize local_memory_size = 0;

        for (j = 0; j < vk_memory_properties->memoryHeapCount; j++)
        {
            if (vk_memory_properties->memoryHeaps[j].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
            {
                local_memory_size = vk_memory_properties->memoryHeaps[j].size;
                break;
            }
        }
        TRACE("device %s, %04x:%04x, reporting local memory size 0x%s bytes\n", debugstr_a(vk_properties->deviceName),
                vk_properties->vendorID, vk_properties->deviceID, wine_dbgstr_longlong(local_memory_size));

        SET_DEVICE_FIELD(device, adapterString, const char *, context->version, vk_properties->deviceName);
        SET_DEVICE_FIELD(device, vendorId, int, context->version, vk_properties->vendorID);
        SET_DEVICE_FIELD(device, deviceId, int, context->version, vk_properties->deviceID);
        if (vk_properties->vendorID == 0x1002)
        {
            SET_DEVICE_FIELD(device, architectureVersion, ArchitectureVersion, context->version, ArchitectureVersion_GCN);
            SET_DEVICE_FIELD(device, asicFamily, AsicFamily, context->version, AsicFamily_GCN4);
        }
        SET_DEVICE_FIELD(device, localMemoryInBytes, ULONG64, context->version, local_memory_size);
        if (!i)
            SET_DEVICE_FIELD(device, isPrimaryDevice, int, context->version, 1);

        init_device_displays(vk_properties->deviceName,
                GET_DEVICE_FIELD_ADDR(device, displays, AGSDisplayInfo *, context->version),
                GET_DEVICE_FIELD_ADDR(device, numDisplays, int, context->version));

        device += amd_ags_info[context->version].device_size;
    }

    return AGS_SUCCESS;
}

AGSReturnCode WINAPI agsInit(AGSContext **context, const AGSConfiguration *config, AGSGPUInfo *gpu_info)
{
    struct AGSContext *object;
    AGSReturnCode ret;

    TRACE("context %p, config %p, gpu_info %p.\n", context, config, gpu_info);

    if (!context || !gpu_info)
        return AGS_INVALID_ARGS;

    if (config)
        FIXME("Ignoring config %p.\n", config);

    if (!(object = heap_alloc(sizeof(*object))))
        return AGS_OUT_OF_MEMORY;

    if ((ret = init_ags_context(object)) != AGS_SUCCESS)
    {
        heap_free(object);
        return ret;
    }

    memset(gpu_info, 0, sizeof(*gpu_info));
    gpu_info->agsVersionMajor = amd_ags_info[object->version].major;
    gpu_info->agsVersionMinor = amd_ags_info[object->version].minor;
    gpu_info->agsVersionPatch = amd_ags_info[object->version].patch;
    gpu_info->driverVersion = "20.50.03.05-210326a-365573E-RadeonSoftwareAdrenalin2020";
    gpu_info->radeonSoftwareVersion  = "21.3.2";
    gpu_info->numDevices = object->device_count;
    gpu_info->devices = object->devices;

    TRACE("Created context %p.\n", object);

    *context = object;

    return AGS_SUCCESS;
}

AGSReturnCode WINAPI agsDeInit(AGSContext *context)
{
    unsigned int i;
    BYTE *device;

    TRACE("context %p.\n", context);

    if (context)
    {
        heap_free(context->memory_properties);
        heap_free(context->properties);
        device = (BYTE *)context->devices;
        for (i = 0; i < context->device_count; ++i)
        {
            heap_free(*GET_DEVICE_FIELD_ADDR(device, displays, AGSDisplayInfo *, context->version));
            device += amd_ags_info[context->version].device_size;
        }
        heap_free(context->devices);
        heap_free(context);
    }

    return AGS_SUCCESS;
}

AGSReturnCode WINAPI agsGetCrossfireGPUCount(AGSContext *context, int *gpu_count)
{
    TRACE("context %p gpu_count %p stub!\n", context, gpu_count);

    if (!context || !gpu_count)
        return AGS_INVALID_ARGS;

    *gpu_count = 1;
    return AGS_SUCCESS;
}

AGSReturnCode WINAPI agsDriverExtensionsDX12_CreateDevice(AGSContext *context,
        const AGSDX12DeviceCreationParams *creation_params, const AGSDX12ExtensionParams *extension_params,
        AGSDX12ReturnedParams *returned_params)
{
    HRESULT hr;

    TRACE("feature level %#x, app %s, engine %s %#x %#x.\n", creation_params->FeatureLevel, debugstr_w(extension_params->pAppName),
            debugstr_w(extension_params->pEngineName), extension_params->appVersion, extension_params->engineVersion);

    if (!load_d3d12_functions())
    {
        ERR("Could not load d3d12.dll.\n");
        return AGS_MISSING_D3D_DLL;
    }

    memset(returned_params, 0, sizeof(*returned_params));
    if (FAILED(hr = pD3D12CreateDevice((IUnknown *)creation_params->pAdapter, creation_params->FeatureLevel,
            &creation_params->iid, (void **)&returned_params->pDevice)))
    {
        ERR("D3D12CreateDevice failed, hr %#x.\n", hr);
        return AGS_DX_FAILURE;
    }

    TRACE("Created d3d12 device %p.\n", returned_params->pDevice);

    return AGS_SUCCESS;
}

AGSReturnCode WINAPI agsDriverExtensionsDX12_DestroyDevice(AGSContext* context, ID3D12Device* device, unsigned int* device_refs)
{
    ULONG ref_count;

    if (!device)
        return AGS_SUCCESS;

    ref_count = ID3D12Device_Release(device);
    if (device_refs)
        *device_refs = (unsigned int)ref_count;

    return AGS_SUCCESS;
}

AGSDriverVersionResult WINAPI agsCheckDriverVersion(const char* version_reported, unsigned int version_required)
{
    FIXME("version_reported %s, version_required %d semi-stub.\n", debugstr_a(version_reported), version_required);

    return AGS_SOFTWAREVERSIONCHECK_OK;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, void *reserved)
{
    TRACE("%p, %u, %p.\n", instance, reason, reserved);

    switch (reason)
    {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(instance);
            break;
    }

    return TRUE;
}
