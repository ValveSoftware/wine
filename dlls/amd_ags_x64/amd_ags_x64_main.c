#include "config.h"

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

struct AGSContext
{
    unsigned int device_count;
    AGSDeviceInfo *devices;
    VkPhysicalDeviceProperties *properties;
};

static AGSReturnCode vk_get_physical_device_properties(unsigned int *out_count,
        VkPhysicalDeviceProperties **out)
{
    VkPhysicalDeviceProperties *properties = NULL;
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

    for (i = 0; i < count; ++i)
        vkGetPhysicalDeviceProperties(vk_physical_devices[i], &properties[i]);

    *out_count = count;
    *out = properties;

done:
    heap_free(vk_physical_devices);
    if (vk_instance)
        vkDestroyInstance(vk_instance, NULL);
    return ret;
}

static AGSReturnCode init_ags_context(AGSContext *context)
{
    AGSReturnCode ret;
    unsigned int i;

    context->device_count = 0;
    context->devices = NULL;
    context->properties = NULL;

    ret = vk_get_physical_device_properties(&context->device_count, &context->properties);
    if (ret != AGS_SUCCESS || !context->device_count)
        return ret;

    if (!(context->devices = heap_calloc(context->device_count, sizeof(*context->devices))))
    {
        WARN("Failed to allocate memory.\n");
        heap_free(context->properties);
        return AGS_OUT_OF_MEMORY;
    }

    for (i = 0; i < context->device_count; ++i)
    {
        const VkPhysicalDeviceProperties *vk_properties = &context->properties[i];
        AGSDeviceInfo *device = &context->devices[i];

        device->adapterString = vk_properties->deviceName;
        device->vendorId = vk_properties->vendorID;
        device->deviceId = vk_properties->deviceID;

        if (device->vendorId == 0x1002)
            device->architectureVersion = ArchitectureVersion_GCN;

        if (!i)
            device->isPrimaryDevice = 1;
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
    gpu_info->agsVersionMajor = AMD_AGS_VERSION_MAJOR;
    gpu_info->agsVersionMinor = AMD_AGS_VERSION_MINOR;
    gpu_info->agsVersionPatch = AMD_AGS_VERSION_PATCH;
    gpu_info->driverVersion = "18.10.16-180516a-328911C-RadeonSoftwareAdrenalin";
    gpu_info->radeonSoftwareVersion  = "18.5.1";
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
        heap_free(context->properties);
        heap_free(context->devices);
        heap_free(context);
    }

    return AGS_SUCCESS;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, void *reserved)
{
    TRACE("%p, %u, %p.\n", instance, reason, reserved);

    switch (reason)
    {
        case DLL_WINE_PREATTACH:
            return FALSE; /* Prefer native. */
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(instance);
            break;
    }

    return TRUE;
}
