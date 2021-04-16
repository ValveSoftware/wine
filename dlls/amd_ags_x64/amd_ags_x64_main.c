#include <stdarg.h>
#include <stdbool.h>

#include "windef.h"
#include "winbase.h"
#include "wine/debug.h"
#include "wine/heap.h"

#include "wine/vulkan.h"

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

struct
{
    int major;
    int minor;
    int patch;
}
static const amd_ags_versions[AMD_AGS_VERSION_COUNT] =
{
    {5, 1, 1},
    {5, 2, 0},
    {5, 2, 1},
    {5, 3, 0},
    {5, 4, 0},
    {5, 4, 1},
};

struct AGSContext
{
    enum amd_ags_version version;
    unsigned int device_count;
    AGSDeviceInfo *devices;
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

    for (i = 0; i < ARRAY_SIZE(amd_ags_versions); i++)
    {
        if ((major == amd_ags_versions[i].major) &&
            (minor == amd_ags_versions[i].minor) &&
            (patch == amd_ags_versions[i].patch))
        {
            ret = i;
            break;
        }
    }

done:
    heap_free(infobuf);
    TRACE("Using AGS v%d.%d.%d interface\n",
          amd_ags_versions[ret].major, amd_ags_versions[ret].minor, amd_ags_versions[ret].patch);
    return ret;
}

static AGSReturnCode init_ags_context(AGSContext *context)
{
    AGSReturnCode ret;
    unsigned int i, j;

    context->version = determine_ags_version();
    context->device_count = 0;
    context->devices = NULL;
    context->properties = NULL;
    context->memory_properties = NULL;

    ret = vk_get_physical_device_properties(&context->device_count, &context->properties, &context->memory_properties);
    if (ret != AGS_SUCCESS || !context->device_count)
        return ret;

    if (!(context->devices = heap_calloc(context->device_count, sizeof(*context->devices))))
    {
        WARN("Failed to allocate memory.\n");
        heap_free(context->properties);
        heap_free(context->memory_properties);
        return AGS_OUT_OF_MEMORY;
    }

    for (i = 0; i < context->device_count; ++i)
    {
        const VkPhysicalDeviceProperties *vk_properties = &context->properties[i];
        const VkPhysicalDeviceMemoryProperties *vk_memory_properties = &context->memory_properties[i];
        AGSDeviceInfo *device = &context->devices[i];
        VkDeviceSize local_memory_size = 0;

        for (j = 0; j < vk_memory_properties->memoryHeapCount; j++)
        {
            if (vk_memory_properties->memoryHeaps[j].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
            {
                local_memory_size = vk_memory_properties->memoryHeaps[j].size;
                break;
            }
        }
        TRACE("reporting local memory size 0x%s bytes\n", wine_dbgstr_longlong(local_memory_size));

        switch (context->version)
        {
        case AMD_AGS_VERSION_5_1_1:
            device->agsDeviceInfo511.adapterString = vk_properties->deviceName;
            device->agsDeviceInfo511.vendorId = vk_properties->vendorID;
            device->agsDeviceInfo511.deviceId = vk_properties->deviceID;
            device->agsDeviceInfo511.localMemoryInBytes = local_memory_size;

            if (device->agsDeviceInfo511.vendorId == 0x1002)
                device->agsDeviceInfo511.architectureVersion = ArchitectureVersion_GCN;

            if (!i)
                device->agsDeviceInfo511.isPrimaryDevice = 1;
            break;
        case AMD_AGS_VERSION_5_2_0:
        case AMD_AGS_VERSION_5_2_1:
        case AMD_AGS_VERSION_5_3_0:
            device->agsDeviceInfo520.adapterString = vk_properties->deviceName;
            device->agsDeviceInfo520.vendorId = vk_properties->vendorID;
            device->agsDeviceInfo520.deviceId = vk_properties->deviceID;
            device->agsDeviceInfo520.localMemoryInBytes = local_memory_size;

            if (device->agsDeviceInfo520.vendorId == 0x1002)
                device->agsDeviceInfo520.architectureVersion = ArchitectureVersion_GCN;

            if (!i)
                device->agsDeviceInfo520.isPrimaryDevice = 1;
            break;
        case AMD_AGS_VERSION_5_4_0:
            device->agsDeviceInfo540.adapterString = vk_properties->deviceName;
            device->agsDeviceInfo540.vendorId = vk_properties->vendorID;
            device->agsDeviceInfo540.deviceId = vk_properties->deviceID;
            device->agsDeviceInfo540.localMemoryInBytes = local_memory_size;

            if (device->agsDeviceInfo540.vendorId == 0x1002)
                device->agsDeviceInfo540.asicFamily = AsicFamily_GCN4;

            if (!i)
                device->agsDeviceInfo540.isPrimaryDevice = 1;
            break;
        case AMD_AGS_VERSION_5_4_1:
        default:
            device->agsDeviceInfo541.adapterString = vk_properties->deviceName;
            device->agsDeviceInfo541.vendorId = vk_properties->vendorID;
            device->agsDeviceInfo541.deviceId = vk_properties->deviceID;
            device->agsDeviceInfo541.localMemoryInBytes = local_memory_size;

            if (device->agsDeviceInfo541.vendorId == 0x1002)
                device->agsDeviceInfo541.asicFamily = AsicFamily_GCN4;

            if (!i)
                device->agsDeviceInfo541.isPrimaryDevice = 1;
            break;
        }
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
    gpu_info->agsVersionMajor = amd_ags_versions[object->version].major;
    gpu_info->agsVersionMinor = amd_ags_versions[object->version].minor;
    gpu_info->agsVersionPatch = amd_ags_versions[object->version].patch;
    gpu_info->driverVersion = "20.20.2-180516a-328911C-RadeonSoftwareAdrenalin";
    gpu_info->radeonSoftwareVersion  = "20.20.2";
    gpu_info->numDevices = object->device_count;
    gpu_info->devices = object->devices;

    TRACE("Created context %p.\n", object);

    *context = object;

    return AGS_SUCCESS;
}

AGSReturnCode WINAPI agsDeInit(AGSContext *context)
{
    TRACE("context %p.\n", context);

    if (context)
    {
        heap_free(context->memory_properties);
        heap_free(context->properties);
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
