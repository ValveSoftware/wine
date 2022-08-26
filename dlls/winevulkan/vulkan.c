/* Wine Vulkan ICD implementation
 *
 * Copyright 2017 Roderick Colenbrander
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

#if 0
#pragma makedep unix
#endif

#include "config.h"
#include <math.h>
#include <time.h>
#include <stdlib.h>

#include "vulkan_private.h"
#include "winreg.h"
#include "ntuser.h"

WINE_DEFAULT_DEBUG_CHANNEL(vulkan);

#define wine_vk_find_struct(s, t) wine_vk_find_struct_((void *)s, VK_STRUCTURE_TYPE_##t)
static void *wine_vk_find_struct_(void *s, VkStructureType t)
{
    VkBaseOutStructure *header;

    for (header = s; header; header = header->pNext)
    {
        if (header->sType == t)
            return header;
    }

    return NULL;
}

#define wine_vk_count_struct(s, t) wine_vk_count_struct_((void *)s, VK_STRUCTURE_TYPE_##t)
static uint32_t wine_vk_count_struct_(void *s, VkStructureType t)
{
    const VkBaseInStructure *header;
    uint32_t result = 0;

    for (header = s; header; header = header->pNext)
    {
        if (header->sType == t)
            result++;
    }

    return result;
}

static const struct vulkan_funcs *vk_funcs;

#define WINE_VK_ADD_DISPATCHABLE_MAPPING(instance, object, native_handle) \
    wine_vk_add_handle_mapping((instance), (uint64_t) (uintptr_t) (object), (uint64_t) (uintptr_t) (native_handle), &(object)->mapping)
#define WINE_VK_ADD_NON_DISPATCHABLE_MAPPING(instance, object, native_handle) \
    wine_vk_add_handle_mapping((instance), (uint64_t) (uintptr_t) (object), (uint64_t) (native_handle), &(object)->mapping)
static void  wine_vk_add_handle_mapping(struct VkInstance_T *instance, uint64_t wrapped_handle,
        uint64_t native_handle, struct wine_vk_mapping *mapping)
{
    if (instance->enable_wrapper_list)
    {
        mapping->native_handle = native_handle;
        mapping->wine_wrapped_handle = wrapped_handle;
        pthread_rwlock_wrlock(&instance->wrapper_lock);
        list_add_tail(&instance->wrappers, &mapping->link);
        pthread_rwlock_unlock(&instance->wrapper_lock);
    }
}

#define WINE_VK_REMOVE_HANDLE_MAPPING(instance, object) \
    wine_vk_remove_handle_mapping((instance), &(object)->mapping)
static void wine_vk_remove_handle_mapping(struct VkInstance_T *instance, struct wine_vk_mapping *mapping)
{
    if (instance->enable_wrapper_list)
    {
        pthread_rwlock_wrlock(&instance->wrapper_lock);
        list_remove(&mapping->link);
        pthread_rwlock_unlock(&instance->wrapper_lock);
    }
}

static uint64_t wine_vk_get_wrapper(struct VkInstance_T *instance, uint64_t native_handle)
{
    struct wine_vk_mapping *mapping;
    uint64_t result = 0;

    pthread_rwlock_rdlock(&instance->wrapper_lock);
    LIST_FOR_EACH_ENTRY(mapping, &instance->wrappers, struct wine_vk_mapping, link)
    {
        if (mapping->native_handle == native_handle)
        {
            result = mapping->wine_wrapped_handle;
            break;
        }
    }
    pthread_rwlock_unlock(&instance->wrapper_lock);
    return result;
}

static VkBool32 debug_utils_callback_conversion(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT message_types,
    const VkDebugUtilsMessengerCallbackDataEXT_host *callback_data,
    void *user_data)
{
    struct wine_vk_debug_utils_params params;
    VkDebugUtilsObjectNameInfoEXT *object_name_infos;
    struct wine_debug_utils_messenger *object;
    void *ret_ptr;
    ULONG ret_len;
    VkBool32 result;
    unsigned int i;

    TRACE("%i, %u, %p, %p\n", severity, message_types, callback_data, user_data);

    object = user_data;

    if (!object->instance->instance)
    {
        /* instance wasn't yet created, this is a message from the native loader */
        return VK_FALSE;
    }

    /* FIXME: we should pack all referenced structs instead of passing pointers */
    params.user_callback = object->user_callback;
    params.user_data = object->user_data;
    params.severity = severity;
    params.message_types = message_types;
    params.data = *((VkDebugUtilsMessengerCallbackDataEXT *) callback_data);

    object_name_infos = calloc(params.data.objectCount, sizeof(*object_name_infos));

    for (i = 0; i < params.data.objectCount; i++)
    {
        object_name_infos[i].sType = callback_data->pObjects[i].sType;
        object_name_infos[i].pNext = callback_data->pObjects[i].pNext;
        object_name_infos[i].objectType = callback_data->pObjects[i].objectType;
        object_name_infos[i].pObjectName = callback_data->pObjects[i].pObjectName;

        if (wine_vk_is_type_wrapped(callback_data->pObjects[i].objectType))
        {
            object_name_infos[i].objectHandle = wine_vk_get_wrapper(object->instance, callback_data->pObjects[i].objectHandle);
            if (!object_name_infos[i].objectHandle)
            {
                WARN("handle conversion failed 0x%s\n", wine_dbgstr_longlong(callback_data->pObjects[i].objectHandle));
                free(object_name_infos);
                return VK_FALSE;
            }
        }
        else
        {
            object_name_infos[i].objectHandle = callback_data->pObjects[i].objectHandle;
        }
    }

    params.data.pObjects = object_name_infos;

    /* applications should always return VK_FALSE */
    result = KeUserModeCallback( NtUserCallVulkanDebugUtilsCallback, &params, sizeof(params),
                                 &ret_ptr, &ret_len );

    free(object_name_infos);

    return result;
}

static VkBool32 debug_report_callback_conversion(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT object_type,
    uint64_t object_handle, size_t location, int32_t code, const char *layer_prefix, const char *message, void *user_data)
{
    struct wine_vk_debug_report_params params;
    struct wine_debug_report_callback *object;
    void *ret_ptr;
    ULONG ret_len;

    TRACE("%#x, %#x, 0x%s, 0x%s, %d, %p, %p, %p\n", flags, object_type, wine_dbgstr_longlong(object_handle),
        wine_dbgstr_longlong(location), code, layer_prefix, message, user_data);

    object = user_data;

    if (!object->instance->instance)
    {
        /* instance wasn't yet created, this is a message from the native loader */
        return VK_FALSE;
    }

    /* FIXME: we should pack all referenced structs instead of passing pointers */
    params.user_callback = object->user_callback;
    params.user_data = object->user_data;
    params.flags = flags;
    params.object_type = object_type;
    params.location = location;
    params.code = code;
    params.layer_prefix = layer_prefix;
    params.message = message;

    params.object_handle = wine_vk_get_wrapper(object->instance, object_handle);
    if (!params.object_handle)
        params.object_type = VK_DEBUG_REPORT_OBJECT_TYPE_UNKNOWN_EXT;

    return KeUserModeCallback( NtUserCallVulkanDebugReportCallback, &params, sizeof(params),
                               &ret_ptr, &ret_len );
}

static void wine_vk_physical_device_free(struct VkPhysicalDevice_T *phys_dev)
{
    if (!phys_dev)
        return;

    WINE_VK_REMOVE_HANDLE_MAPPING(phys_dev->instance, phys_dev);
    free(phys_dev->extensions);
    free(phys_dev);
}

static struct VkPhysicalDevice_T *wine_vk_physical_device_alloc(struct VkInstance_T *instance,
        VkPhysicalDevice phys_dev)
{
    struct VkPhysicalDevice_T *object;
    uint32_t num_host_properties, num_properties = 0;
    VkExtensionProperties *host_properties = NULL;
    VkResult res;
    unsigned int i, j;

    if (!(object = calloc(1, sizeof(*object))))
        return NULL;

    object->base.loader_magic = VULKAN_ICD_MAGIC_VALUE;
    object->instance = instance;
    object->phys_dev = phys_dev;

    WINE_VK_ADD_DISPATCHABLE_MAPPING(instance, object, phys_dev);

    res = instance->funcs.p_vkEnumerateDeviceExtensionProperties(phys_dev,
            NULL, &num_host_properties, NULL);
    if (res != VK_SUCCESS)
    {
        ERR("Failed to enumerate device extensions, res=%d\n", res);
        goto err;
    }

    host_properties = calloc(num_host_properties, sizeof(*host_properties));
    if (!host_properties)
    {
        ERR("Failed to allocate memory for device properties!\n");
        goto err;
    }

    res = instance->funcs.p_vkEnumerateDeviceExtensionProperties(phys_dev,
            NULL, &num_host_properties, host_properties);
    if (res != VK_SUCCESS)
    {
        ERR("Failed to enumerate device extensions, res=%d\n", res);
        goto err;
    }

    /* Count list of extensions for which we have an implementation.
     * TODO: perform translation for platform specific extensions.
     */
    for (i = 0; i < num_host_properties; i++)
    {
        if (wine_vk_device_extension_supported(host_properties[i].extensionName))
        {
            TRACE("Enabling extension '%s' for physical device %p\n", host_properties[i].extensionName, object);
            num_properties++;
        }
        else
        {
            TRACE("Skipping extension '%s', no implementation found in winevulkan.\n", host_properties[i].extensionName);
        }
    }

    TRACE("Host supported extensions %u, Wine supported extensions %u\n", num_host_properties, num_properties);

    if (!(object->extensions = calloc(num_properties, sizeof(*object->extensions))))
    {
        ERR("Failed to allocate memory for device extensions!\n");
        goto err;
    }

    for (i = 0, j = 0; i < num_host_properties; i++)
    {
        if (wine_vk_device_extension_supported(host_properties[i].extensionName))
        {
            object->extensions[j] = host_properties[i];
            j++;
        }
    }
    object->extension_count = num_properties;

    free(host_properties);
    return object;

err:
    wine_vk_physical_device_free(object);
    free(host_properties);
    return NULL;
}

static void wine_vk_free_command_buffers(struct VkDevice_T *device,
        struct wine_cmd_pool *pool, uint32_t count, const VkCommandBuffer *buffers)
{
    unsigned int i;

    for (i = 0; i < count; i++)
    {
        if (!buffers[i])
            continue;

        device->funcs.p_vkFreeCommandBuffers(device->device, pool->command_pool, 1, &buffers[i]->command_buffer);
        list_remove(&buffers[i]->pool_link);
        WINE_VK_REMOVE_HANDLE_MAPPING(device->phys_dev->instance, buffers[i]);
        free(buffers[i]);
    }
}

static void wine_vk_device_get_queues(struct VkDevice_T *device,
        uint32_t family_index, uint32_t queue_count, VkDeviceQueueCreateFlags flags,
        struct VkQueue_T* queues)
{
    VkDeviceQueueInfo2 queue_info;
    unsigned int i;

    for (i = 0; i < queue_count; i++)
    {
        struct VkQueue_T *queue = &queues[i];

        queue->base.loader_magic = VULKAN_ICD_MAGIC_VALUE;
        queue->device = device;
        queue->family_index = family_index;
        queue->queue_index = i;
        queue->flags = flags;

        /* The Vulkan spec says:
         *
         * "vkGetDeviceQueue must only be used to get queues that were created
         * with the flags parameter of VkDeviceQueueCreateInfo set to zero."
         */
        if (flags && device->funcs.p_vkGetDeviceQueue2)
        {
            queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2;
            queue_info.pNext = NULL;
            queue_info.flags = flags;
            queue_info.queueFamilyIndex = family_index;
            queue_info.queueIndex = i;
            device->funcs.p_vkGetDeviceQueue2(device->device, &queue_info, &queue->queue);
        }
        else
        {
            device->funcs.p_vkGetDeviceQueue(device->device, family_index, i, &queue->queue);
        }

        WINE_VK_ADD_DISPATCHABLE_MAPPING(device->phys_dev->instance, queue, queue->queue);
    }
}

static void wine_vk_device_free_create_info(VkDeviceCreateInfo *create_info)
{
    free_VkDeviceCreateInfo_struct_chain(create_info);
}

static char **parse_xr_extensions(unsigned int *len)
{
    char *xr_str, *iter, *start, **list;
    unsigned int extension_count = 0, o = 0;

    xr_str = getenv("__WINE_OPENXR_VK_DEVICE_EXTENSIONS");
    if (!xr_str)
    {
        *len = 0;
        return NULL;
    }
    xr_str = strdup(xr_str);

    TRACE("got var: %s\n", xr_str);

    iter = xr_str;
    while(*iter){
        if(*iter++ == ' ')
            extension_count++;
    }
    /* count the one ending in NUL */
    if(iter != xr_str)
        extension_count++;
    if(!extension_count){
        *len = 0;
        return NULL;
    }

    TRACE("counted %u extensions\n", extension_count);

    list = malloc(extension_count * sizeof(char *));

    start = iter = xr_str;
    do{
        if(*iter == ' '){
            *iter = 0;
            list[o++] = strdup(start);
            TRACE("added %s to list\n", list[o-1]);
            iter++;
            start = iter;
        }else if(*iter == 0){
            list[o++] = strdup(start);
            TRACE("added %s to list\n", list[o-1]);
            break;
        }else{
            iter++;
        }
    }while(1);

    free(xr_str);

    *len = extension_count;

    return list;
}

static VkResult wine_vk_device_convert_create_info(const VkDeviceCreateInfo *src,
        VkDeviceCreateInfo *dst, BOOL *must_free_extensions)
{
    unsigned int i, append_xr = 0, wine_extension_count;
    VkResult res;

    static const char *wine_xr_extension_name = "VK_WINE_openxr_device_extensions";

    *dst = *src;

    if ((res = convert_VkDeviceCreateInfo_struct_chain(src->pNext, dst)) < 0)
    {
        WARN("Failed to convert VkDeviceCreateInfo pNext chain, res=%d.\n", res);
        return res;
    }

    /* Should be filtered out by loader as ICDs don't support layers. */
    dst->enabledLayerCount = 0;
    dst->ppEnabledLayerNames = NULL;

    for (i = 0; i < dst->enabledExtensionCount; i++)
    {
        const char *extension_name = dst->ppEnabledExtensionNames[i];
        if (!strcmp(extension_name, wine_xr_extension_name))
        {
            append_xr = 1;
            break;
        }
    }

    if (append_xr)
    {
        unsigned int xr_extensions_len, o = 0;
        char **xr_extensions_list = parse_xr_extensions(&xr_extensions_len);

        char **new_extensions_list = malloc(sizeof(char *) * (dst->enabledExtensionCount + xr_extensions_len));

        if(!xr_extensions_list)
            WARN("Requested to use XR extensions, but none are set!\n");

        for (i = 0; i < dst->enabledExtensionCount; i++)
        {
            if (strcmp(dst->ppEnabledExtensionNames[i], wine_xr_extension_name) != 0)
            {
                new_extensions_list[o++] = strdup(dst->ppEnabledExtensionNames[i]);
            }
        }

        TRACE("appending XR extensions:\n");
        for (i = 0; i < xr_extensions_len; ++i)
        {
            TRACE("\t%s\n", xr_extensions_list[i]);
            new_extensions_list[o++] = xr_extensions_list[i];
        }
        dst->enabledExtensionCount = o;
        dst->ppEnabledExtensionNames = (const char * const *)new_extensions_list;

        free(xr_extensions_list);

        *must_free_extensions = TRUE;
        wine_extension_count = dst->enabledExtensionCount - xr_extensions_len;
    }else{
        *must_free_extensions = FALSE;
        wine_extension_count = dst->enabledExtensionCount;
    }

    TRACE("Enabled %u extensions.\n", dst->enabledExtensionCount);
    for (i = 0; i < wine_extension_count; i++)
    {
        TRACE("Extension %u: %s.\n", i, debugstr_a(dst->ppEnabledExtensionNames[i]));
    }

    return VK_SUCCESS;
}

static void wine_vk_device_free_create_info_extensions(VkDeviceCreateInfo *create_info)
{
    unsigned int i;
    for(i = 0; i < create_info->enabledExtensionCount; ++i)
        free((void*)create_info->ppEnabledExtensionNames[i]);
    free((void*)create_info->ppEnabledExtensionNames);
}


/* Helper function used for freeing a device structure. This function supports full
 * and partial object cleanups and can thus be used for vkCreateDevice failures.
 */
static void wine_vk_device_free(struct VkDevice_T *device)
{
    struct VkQueue_T *queue;

    if (!device)
        return;

    if (device->queues)
    {
        unsigned int i;
        for (i = 0; i < device->queue_count; i++)
        {
            queue = &device->queues[i];
            if (queue && queue->queue)
                WINE_VK_REMOVE_HANDLE_MAPPING(device->phys_dev->instance, queue);
        }
        free(device->queues);
        device->queues = NULL;
    }

    if (device->device && device->funcs.p_vkDestroyDevice)
    {
        WINE_VK_REMOVE_HANDLE_MAPPING(device->phys_dev->instance, device);
        device->funcs.p_vkDestroyDevice(device->device, NULL /* pAllocator */);
    }

    free(device);
}

NTSTATUS init_vulkan(void *args)
{
    vk_funcs = *(const struct vulkan_funcs **)args;
    *(const struct unix_funcs **)args = &loader_funcs;
    return STATUS_SUCCESS;
}

/* Helper function for converting between win32 and host compatible VkInstanceCreateInfo.
 * This function takes care of extensions handled at winevulkan layer, a Wine graphics
 * driver is responsible for handling e.g. surface extensions.
 */
static VkResult wine_vk_instance_convert_create_info(const VkInstanceCreateInfo *src,
        VkInstanceCreateInfo *dst, struct VkInstance_T *object)
{
    VkDebugUtilsMessengerCreateInfoEXT *debug_utils_messenger;
    VkDebugReportCallbackCreateInfoEXT *debug_report_callback;
    VkBaseInStructure *header;
    unsigned int i;
    VkResult res;

    *dst = *src;

    if ((res = convert_VkInstanceCreateInfo_struct_chain(src->pNext, dst)) < 0)
    {
        WARN("Failed to convert VkInstanceCreateInfo pNext chain, res=%d.\n", res);
        return res;
    }

    object->utils_messenger_count = wine_vk_count_struct(dst, DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT);
    object->utils_messengers =  calloc(object->utils_messenger_count, sizeof(*object->utils_messengers));
    header = (VkBaseInStructure *) dst;
    for (i = 0; i < object->utils_messenger_count; i++)
    {
        header = wine_vk_find_struct(header->pNext, DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT);
        debug_utils_messenger = (VkDebugUtilsMessengerCreateInfoEXT *) header;

        object->utils_messengers[i].instance = object;
        object->utils_messengers[i].debug_messenger = VK_NULL_HANDLE;
        object->utils_messengers[i].user_callback = debug_utils_messenger->pfnUserCallback;
        object->utils_messengers[i].user_data = debug_utils_messenger->pUserData;

        /* convert_VkInstanceCreateInfo_struct_chain already copied the chain,
         * so we can modify it in-place.
         */
        debug_utils_messenger->pfnUserCallback = (void *) &debug_utils_callback_conversion;
        debug_utils_messenger->pUserData = &object->utils_messengers[i];
    }

    debug_report_callback = wine_vk_find_struct(header->pNext, DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT);
    if (debug_report_callback)
    {
        object->default_callback.instance = object;
        object->default_callback.debug_callback = VK_NULL_HANDLE;
        object->default_callback.user_callback = debug_report_callback->pfnCallback;
        object->default_callback.user_data = debug_report_callback->pUserData;

        debug_report_callback->pfnCallback = (void *) &debug_report_callback_conversion;
        debug_report_callback->pUserData = &object->default_callback;
    }

    /* ICDs don't support any layers, so nothing to copy. Modern versions of the loader
     * filter this data out as well.
     */
    if (object->quirks & WINEVULKAN_QUIRK_IGNORE_EXPLICIT_LAYERS) {
        dst->enabledLayerCount = 0;
        dst->ppEnabledLayerNames = NULL;
        WARN("Ignoring explicit layers!\n");
    } else if (dst->enabledLayerCount) {
        FIXME("Loading explicit layers is not supported by winevulkan!\n");
        return VK_ERROR_LAYER_NOT_PRESENT;
    }

    TRACE("Enabled extensions: %u\n", dst->enabledExtensionCount);
    for (i = 0; i < dst->enabledExtensionCount; i++)
    {
        const char *extension_name = dst->ppEnabledExtensionNames[i];
        TRACE("Extension %u: %s.\n", i, debugstr_a(extension_name));
        if (!strcmp(extension_name, "VK_EXT_debug_utils") || !strcmp(extension_name, "VK_EXT_debug_report"))
        {
            object->enable_wrapper_list = VK_TRUE;
        }
    }

    return VK_SUCCESS;
}

/* Helper function which stores wrapped physical devices in the instance object. */
static VkResult wine_vk_instance_load_physical_devices(struct VkInstance_T *instance)
{
    VkPhysicalDevice *tmp_phys_devs;
    uint32_t phys_dev_count;
    unsigned int i;
    VkResult res;

    res = instance->funcs.p_vkEnumeratePhysicalDevices(instance->instance, &phys_dev_count, NULL);
    if (res != VK_SUCCESS)
    {
        ERR("Failed to enumerate physical devices, res=%d\n", res);
        return res;
    }
    if (!phys_dev_count)
        return res;

    if (!(tmp_phys_devs = calloc(phys_dev_count, sizeof(*tmp_phys_devs))))
        return VK_ERROR_OUT_OF_HOST_MEMORY;

    res = instance->funcs.p_vkEnumeratePhysicalDevices(instance->instance, &phys_dev_count, tmp_phys_devs);
    if (res != VK_SUCCESS)
    {
        free(tmp_phys_devs);
        return res;
    }

    instance->phys_devs = calloc(phys_dev_count, sizeof(*instance->phys_devs));
    if (!instance->phys_devs)
    {
        free(tmp_phys_devs);
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    /* Wrap each native physical device handle into a dispatchable object for the ICD loader. */
    for (i = 0; i < phys_dev_count; i++)
    {
        struct VkPhysicalDevice_T *phys_dev = wine_vk_physical_device_alloc(instance, tmp_phys_devs[i]);
        if (!phys_dev)
        {
            ERR("Unable to allocate memory for physical device!\n");
            free(tmp_phys_devs);
            return VK_ERROR_OUT_OF_HOST_MEMORY;
        }

        instance->phys_devs[i] = phys_dev;
        instance->phys_dev_count = i + 1;
    }
    instance->phys_dev_count = phys_dev_count;

    free(tmp_phys_devs);
    return VK_SUCCESS;
}

static struct VkPhysicalDevice_T *wine_vk_instance_wrap_physical_device(struct VkInstance_T *instance,
        VkPhysicalDevice physical_device)
{
    unsigned int i;

    for (i = 0; i < instance->phys_dev_count; ++i)
    {
        struct VkPhysicalDevice_T *current = instance->phys_devs[i];
        if (current->phys_dev == physical_device)
            return current;
    }

    ERR("Unrecognized physical device %p.\n", physical_device);
    return NULL;
}

/* Helper function used for freeing an instance structure. This function supports full
 * and partial object cleanups and can thus be used for vkCreateInstance failures.
 */
static void wine_vk_instance_free(struct VkInstance_T *instance)
{
    if (!instance)
        return;

    if (instance->phys_devs)
    {
        unsigned int i;

        for (i = 0; i < instance->phys_dev_count; i++)
        {
            wine_vk_physical_device_free(instance->phys_devs[i]);
        }
        free(instance->phys_devs);
    }

    if (instance->instance)
    {
        vk_funcs->p_vkDestroyInstance(instance->instance, NULL /* allocator */);
        WINE_VK_REMOVE_HANDLE_MAPPING(instance, instance);
    }

    pthread_rwlock_destroy(&instance->wrapper_lock);
    free(instance->utils_messengers);

    free(instance);
}

NTSTATUS wine_vkAllocateCommandBuffers(void *args)
{
    struct vkAllocateCommandBuffers_params *params = args;
    VkDevice device = params->device;
    const VkCommandBufferAllocateInfo *allocate_info = params->pAllocateInfo;
    VkCommandBuffer *buffers = params->pCommandBuffers;
    struct wine_cmd_pool *pool;
    VkResult res = VK_SUCCESS;
    unsigned int i;

    TRACE("%p, %p, %p\n", device, allocate_info, buffers);

    pool = wine_cmd_pool_from_handle(allocate_info->commandPool);

    memset(buffers, 0, allocate_info->commandBufferCount * sizeof(*buffers));

    for (i = 0; i < allocate_info->commandBufferCount; i++)
    {
        VkCommandBufferAllocateInfo_host allocate_info_host;

        /* TODO: future extensions (none yet) may require pNext conversion. */
        allocate_info_host.pNext = allocate_info->pNext;
        allocate_info_host.sType = allocate_info->sType;
        allocate_info_host.commandPool = pool->command_pool;
        allocate_info_host.level = allocate_info->level;
        allocate_info_host.commandBufferCount = 1;

        TRACE("Allocating command buffer %u from pool 0x%s.\n",
                i, wine_dbgstr_longlong(allocate_info_host.commandPool));

        if (!(buffers[i] = calloc(1, sizeof(**buffers))))
        {
            res = VK_ERROR_OUT_OF_HOST_MEMORY;
            break;
        }

        buffers[i]->base.loader_magic = VULKAN_ICD_MAGIC_VALUE;
        buffers[i]->device = device;
        list_add_tail(&pool->command_buffers, &buffers[i]->pool_link);
        res = device->funcs.p_vkAllocateCommandBuffers(device->device,
                &allocate_info_host, &buffers[i]->command_buffer);
        WINE_VK_ADD_DISPATCHABLE_MAPPING(device->phys_dev->instance, buffers[i], buffers[i]->command_buffer);
        if (res != VK_SUCCESS)
        {
            ERR("Failed to allocate command buffer, res=%d.\n", res);
            buffers[i]->command_buffer = VK_NULL_HANDLE;
            break;
        }
    }

    if (res != VK_SUCCESS)
    {
        wine_vk_free_command_buffers(device, pool, i + 1, buffers);
        memset(buffers, 0, allocate_info->commandBufferCount * sizeof(*buffers));
    }

    return res;
}

NTSTATUS wine_vkCreateDevice(void *args)
{
    struct vkCreateDevice_params *params = args;
    VkPhysicalDevice phys_dev = params->physicalDevice;
    const VkDeviceCreateInfo *create_info = params->pCreateInfo;
    const VkAllocationCallbacks *allocator = params->pAllocator;
    VkDevice *device = params->pDevice;
    return __wine_create_vk_device_with_callback(phys_dev, create_info, allocator, device, NULL, NULL);
}

VkResult WINAPI __wine_create_vk_device_with_callback(VkPhysicalDevice phys_dev,
        const VkDeviceCreateInfo *create_info,
        const VkAllocationCallbacks *allocator, VkDevice *device,
        VkResult (WINAPI *native_vkCreateDevice)(VkPhysicalDevice, const VkDeviceCreateInfo *, const VkAllocationCallbacks *,
        VkDevice *, void * (*)(VkInstance, const char *), void *), void *native_vkCreateDevice_context)
{
    VkPhysicalDeviceFeatures features = {0};
    VkPhysicalDeviceFeatures2 *features2;
    VkDeviceCreateInfo create_info_host;
    struct VkQueue_T *next_queue;
    struct VkDevice_T *object;
    unsigned int i;
    BOOL create_info_free_extensions;
    VkResult res;

    TRACE("%p, %p, %p, %p\n", phys_dev, create_info, allocator, device);

    if (allocator)
        FIXME("Support for allocation callbacks not implemented yet\n");

    if (TRACE_ON(vulkan))
    {
        VkPhysicalDeviceProperties_host properties;

        phys_dev->instance->funcs.p_vkGetPhysicalDeviceProperties(phys_dev->phys_dev, &properties);

        TRACE("Device name: %s.\n", debugstr_a(properties.deviceName));
        TRACE("Vendor ID: %#x, Device ID: %#x.\n", properties.vendorID, properties.deviceID);
        TRACE("Driver version: %#x.\n", properties.driverVersion);
    }

    if (!(object = calloc(1, sizeof(*object))))
        return VK_ERROR_OUT_OF_HOST_MEMORY;

    object->base.base.loader_magic = VULKAN_ICD_MAGIC_VALUE;
    object->phys_dev = phys_dev;

    res = wine_vk_device_convert_create_info(create_info, &create_info_host, &create_info_free_extensions);
    if (res != VK_SUCCESS)
        goto fail;

    /* Enable shaderStorageImageWriteWithoutFormat for fshack
     * This is available on all hardware and driver combinations we care about.
     */
    if (create_info_host.pEnabledFeatures)
    {
        features = *create_info_host.pEnabledFeatures;
        features.shaderStorageImageWriteWithoutFormat = VK_TRUE;
        create_info_host.pEnabledFeatures = &features;
    }
    if ((features2 = wine_vk_find_struct(&create_info_host, PHYSICAL_DEVICE_FEATURES_2)))
    {
        features2->features.shaderStorageImageWriteWithoutFormat = VK_TRUE;
    }
    else if (!create_info_host.pEnabledFeatures)
    {
        features.shaderStorageImageWriteWithoutFormat = VK_TRUE;
        create_info_host.pEnabledFeatures = &features;
    }

    if (native_vkCreateDevice)
        res = native_vkCreateDevice(phys_dev->phys_dev,
                &create_info_host, NULL /* allocator */, &object->device,
                vk_funcs->p_vkGetInstanceProcAddr, native_vkCreateDevice_context);
    else
        res = phys_dev->instance->funcs.p_vkCreateDevice(phys_dev->phys_dev,
                &create_info_host, NULL /* allocator */, &object->device);

    wine_vk_device_free_create_info(&create_info_host);
    if(create_info_free_extensions)
        wine_vk_device_free_create_info_extensions(&create_info_host);
    WINE_VK_ADD_DISPATCHABLE_MAPPING(phys_dev->instance, object, object->device);
    if (res != VK_SUCCESS)
    {
        WARN("Failed to create device, res=%d.\n", res);
        goto fail;
    }

    /* Just load all function pointers we are aware off. The loader takes care of filtering.
     * We use vkGetDeviceProcAddr as opposed to vkGetInstanceProcAddr for efficiency reasons
     * as functions pass through fewer dispatch tables within the loader.
     */
#define USE_VK_FUNC(name) \
    object->funcs.p_##name = (void *)vk_funcs->p_vkGetDeviceProcAddr(object->device, #name); \
    if (object->funcs.p_##name == NULL) \
        TRACE("Not found '%s'.\n", #name);
    ALL_VK_DEVICE_FUNCS()
#undef USE_VK_FUNC

    /* We need to cache all queues within the device as each requires wrapping since queues are
     * dispatchable objects.
     */
    for (i = 0; i < create_info_host.queueCreateInfoCount; i++)
    {
        object->queue_count += create_info_host.pQueueCreateInfos[i].queueCount;
    }

    if (!(object->queues = calloc(object->queue_count, sizeof(*object->queues))))
    {
        res = VK_ERROR_OUT_OF_HOST_MEMORY;
        goto fail;
    }

    next_queue = object->queues;
    for (i = 0; i < create_info_host.queueCreateInfoCount; i++)
    {
        uint32_t flags = create_info_host.pQueueCreateInfos[i].flags;
        uint32_t family_index = create_info_host.pQueueCreateInfos[i].queueFamilyIndex;
        uint32_t queue_count = create_info_host.pQueueCreateInfos[i].queueCount;

        TRACE("Queue family index %u, queue count %u.\n", family_index, queue_count);

        wine_vk_device_get_queues(object, family_index, queue_count, flags, next_queue);
        next_queue += queue_count;
    }

    object->base.quirks = phys_dev->instance->quirks;

    *device = object;
    TRACE("Created device %p (native device %p).\n", object, object->device);
    return VK_SUCCESS;

fail:
    wine_vk_device_free(object);
    return res;
}

NTSTATUS wine_vkCreateInstance(void *args)
{
    struct vkCreateInstance_params *params = args;
    const VkInstanceCreateInfo *create_info = params->pCreateInfo;
    const VkAllocationCallbacks *allocator = params->pAllocator;
    VkInstance *instance = params->pInstance;
    return __wine_create_vk_instance_with_callback(create_info, allocator, instance, NULL, NULL);
}

VkResult WINAPI __wine_create_vk_instance_with_callback(const VkInstanceCreateInfo *create_info,
        const VkAllocationCallbacks *allocator, VkInstance *instance,
        VkResult (WINAPI *native_vkCreateInstance)(const VkInstanceCreateInfo *, const VkAllocationCallbacks *,
        VkInstance *, void * (*)(VkInstance, const char *), void *), void *native_vkCreateInstance_context)
{
    VkInstanceCreateInfo create_info_host;
    const VkApplicationInfo *app_info;
    struct VkInstance_T *object;
    VkResult res;

    TRACE("create_info %p, allocator %p, instance %p, native_vkCreateInstance %p, context %p.\n",
            create_info, allocator, instance, native_vkCreateInstance, native_vkCreateInstance_context);

    if (allocator)
        FIXME("Support for allocation callbacks not implemented yet\n");

    if (!(object = calloc(1, sizeof(*object))))
    {
        ERR("Failed to allocate memory for instance\n");
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }
    object->base.loader_magic = VULKAN_ICD_MAGIC_VALUE;
    list_init(&object->wrappers);
    pthread_rwlock_init(&object->wrapper_lock, NULL);

    res = wine_vk_instance_convert_create_info(create_info, &create_info_host, object);
    if (res != VK_SUCCESS)
    {
        wine_vk_instance_free(object);
        return res;
    }

    if (native_vkCreateInstance && !vk_funcs->create_vk_instance_with_callback)
        ERR("Driver create_vk_instance_with_callback is not available.\n");

    if (native_vkCreateInstance && vk_funcs->create_vk_instance_with_callback)
        res = vk_funcs->create_vk_instance_with_callback(&create_info_host, NULL /* allocator */, &object->instance,
                native_vkCreateInstance, native_vkCreateInstance_context);
    else
        res = vk_funcs->p_vkCreateInstance(&create_info_host, NULL /* allocator */, &object->instance);
    free_VkInstanceCreateInfo_struct_chain(&create_info_host);
    if (res != VK_SUCCESS)
    {
        ERR("Failed to create instance, res=%d\n", res);
        wine_vk_instance_free(object);
        return res;
    }

    WINE_VK_ADD_DISPATCHABLE_MAPPING(object, object, object->instance);

    /* Load all instance functions we are aware of. Note the loader takes care
     * of any filtering for extensions which were not requested, but which the
     * ICD may support.
     */
#define USE_VK_FUNC(name) \
    object->funcs.p_##name = (void *)vk_funcs->p_vkGetInstanceProcAddr(object->instance, #name);
    ALL_VK_INSTANCE_FUNCS()
#undef USE_VK_FUNC

    /* Cache physical devices for vkEnumeratePhysicalDevices within the instance as
     * each vkPhysicalDevice is a dispatchable object, which means we need to wrap
     * the native physical devices and present those to the application.
     * Cleanup happens as part of wine_vkDestroyInstance.
     */
    res = wine_vk_instance_load_physical_devices(object);
    if (res != VK_SUCCESS)
    {
        ERR("Failed to load physical devices, res=%d\n", res);
        wine_vk_instance_free(object);
        return res;
    }

    if ((app_info = create_info->pApplicationInfo))
    {
        TRACE("Application name %s, application version %#x.\n",
                debugstr_a(app_info->pApplicationName), app_info->applicationVersion);
        TRACE("Engine name %s, engine version %#x.\n", debugstr_a(app_info->pEngineName),
                app_info->engineVersion);
        TRACE("API version %#x.\n", app_info->apiVersion);

        if (app_info->pEngineName && !strcmp(app_info->pEngineName, "idTech"))
            object->quirks |= WINEVULKAN_QUIRK_GET_DEVICE_PROC_ADDR;
    }

    object->quirks |= WINEVULKAN_QUIRK_ADJUST_MAX_IMAGE_COUNT;

    *instance = object;
    TRACE("Created instance %p (native instance %p).\n", object, object->instance);
    return VK_SUCCESS;
}

NTSTATUS wine_vkDestroyDevice(void *args)
{
    struct vkDestroyDevice_params *params = args;
    VkDevice device = params->device;
    const VkAllocationCallbacks *allocator = params->pAllocator;

    TRACE("%p %p\n", device, allocator);

    if (allocator)
        FIXME("Support for allocation callbacks not implemented yet\n");

    wine_vk_device_free(device);
    return STATUS_SUCCESS;
}

NTSTATUS wine_vkDestroyInstance(void *args)
{
    struct vkDestroyInstance_params *params = args;
    VkInstance instance = params->instance;
    const VkAllocationCallbacks *allocator = params->pAllocator;

    TRACE("%p, %p\n", instance, allocator);

    if (allocator)
        FIXME("Support allocation allocators\n");

    wine_vk_instance_free(instance);
    return STATUS_SUCCESS;
}

NTSTATUS wine_vkEnumerateDeviceExtensionProperties(void *args)
{
    struct vkEnumerateDeviceExtensionProperties_params *params = args;
    VkPhysicalDevice phys_dev = params->physicalDevice;
    const char *layer_name = params->pLayerName;
    uint32_t *count = params->pPropertyCount;
    VkExtensionProperties *properties = params->pProperties;

    TRACE("%p, %p, %p, %p\n", phys_dev, layer_name, count, properties);

    /* This shouldn't get called with layer_name set, the ICD loader prevents it. */
    if (layer_name)
    {
        ERR("Layer enumeration not supported from ICD.\n");
        return VK_ERROR_LAYER_NOT_PRESENT;
    }

    if (!properties)
    {
        *count = phys_dev->extension_count;
        return VK_SUCCESS;
    }

    *count = min(*count, phys_dev->extension_count);
    memcpy(properties, phys_dev->extensions, *count * sizeof(*properties));

    TRACE("Returning %u extensions.\n", *count);
    return *count < phys_dev->extension_count ? VK_INCOMPLETE : VK_SUCCESS;
}

NTSTATUS wine_vkEnumerateInstanceExtensionProperties(void *args)
{
    struct vkEnumerateInstanceExtensionProperties_params *params = args;
    uint32_t *count = params->pPropertyCount;
    VkExtensionProperties *properties = params->pProperties;
    uint32_t num_properties = 0, num_host_properties;
    VkExtensionProperties *host_properties;
    unsigned int i, j;
    VkResult res;

    res = vk_funcs->p_vkEnumerateInstanceExtensionProperties(NULL, &num_host_properties, NULL);
    if (res != VK_SUCCESS)
        return res;

    if (!(host_properties = calloc(num_host_properties, sizeof(*host_properties))))
        return VK_ERROR_OUT_OF_HOST_MEMORY;

    res = vk_funcs->p_vkEnumerateInstanceExtensionProperties(NULL, &num_host_properties, host_properties);
    if (res != VK_SUCCESS)
    {
        ERR("Failed to retrieve host properties, res=%d.\n", res);
        free(host_properties);
        return res;
    }

    /* The Wine graphics driver provides us with all extensions supported by the host side
     * including extension fixup (e.g. VK_KHR_xlib_surface -> VK_KHR_win32_surface). It is
     * up to us here to filter the list down to extensions for which we have thunks.
     */
    for (i = 0; i < num_host_properties; i++)
    {
        if (wine_vk_instance_extension_supported(host_properties[i].extensionName))
            num_properties++;
        else
            TRACE("Instance extension '%s' is not supported.\n", host_properties[i].extensionName);
    }

    if (!properties)
    {
        TRACE("Returning %u extensions.\n", num_properties);
        *count = num_properties;
        free(host_properties);
        return VK_SUCCESS;
    }

    for (i = 0, j = 0; i < num_host_properties && j < *count; i++)
    {
        if (wine_vk_instance_extension_supported(host_properties[i].extensionName))
        {
            TRACE("Enabling extension '%s'.\n", host_properties[i].extensionName);
            properties[j++] = host_properties[i];
        }
    }
    *count = min(*count, num_properties);

    free(host_properties);
    return *count < num_properties ? VK_INCOMPLETE : VK_SUCCESS;
}

NTSTATUS wine_vkEnumerateDeviceLayerProperties(void *args)
{
    struct vkEnumerateDeviceLayerProperties_params *params = args;
    uint32_t *count = params->pPropertyCount;

    TRACE("%p, %p, %p\n", params->physicalDevice, count, params->pProperties);

    *count = 0;
    return VK_SUCCESS;
}

NTSTATUS wine_vkEnumerateInstanceVersion(void *args)
{
    struct vkEnumerateInstanceVersion_params *params = args;
    uint32_t *version = params->pApiVersion;
    VkResult res;

    static VkResult (*p_vkEnumerateInstanceVersion)(uint32_t *version);
    if (!p_vkEnumerateInstanceVersion)
        p_vkEnumerateInstanceVersion = vk_funcs->p_vkGetInstanceProcAddr(NULL, "vkEnumerateInstanceVersion");

    if (p_vkEnumerateInstanceVersion)
    {
        res = p_vkEnumerateInstanceVersion(version);
    }
    else
    {
        *version = VK_API_VERSION_1_0;
        res = VK_SUCCESS;
    }

    TRACE("API version %u.%u.%u.\n",
            VK_VERSION_MAJOR(*version), VK_VERSION_MINOR(*version), VK_VERSION_PATCH(*version));
    *version = min(WINE_VK_VERSION, *version);
    return res;
}

NTSTATUS wine_vkEnumeratePhysicalDevices(void *args)
{
    struct vkEnumeratePhysicalDevices_params *params = args;
    VkInstance instance = params->instance;
    uint32_t *count = params->pPhysicalDeviceCount;
    VkPhysicalDevice *devices = params->pPhysicalDevices;
    unsigned int i;

    TRACE("%p %p %p\n", instance, count, devices);

    if (!devices)
    {
        *count = instance->phys_dev_count;
        return VK_SUCCESS;
    }

    *count = min(*count, instance->phys_dev_count);
    for (i = 0; i < *count; i++)
    {
        devices[i] = instance->phys_devs[i];
    }

    TRACE("Returning %u devices.\n", *count);
    return *count < instance->phys_dev_count ? VK_INCOMPLETE : VK_SUCCESS;
}

NTSTATUS wine_vkFreeCommandBuffers(void *args)
{
    struct vkFreeCommandBuffers_params *params = args;
    VkDevice device = params->device;
    struct wine_cmd_pool *pool = wine_cmd_pool_from_handle(params->commandPool);
    uint32_t count = params->commandBufferCount;
    const VkCommandBuffer *buffers = params->pCommandBuffers;

    TRACE("%p, 0x%s, %u, %p\n", device, wine_dbgstr_longlong(params->commandPool), count, buffers);

    wine_vk_free_command_buffers(device, pool, count, buffers);
    return STATUS_SUCCESS;
}

static VkQueue wine_vk_device_find_queue(VkDevice device, const VkDeviceQueueInfo2 *info)
{
    struct VkQueue_T* queue;
    uint32_t i;

    for (i = 0; i < device->queue_count; i++)
    {
        queue = &device->queues[i];
        if (queue->family_index == info->queueFamilyIndex
                && queue->queue_index == info->queueIndex
                && queue->flags == info->flags)
        {
            return queue;
        }
    }

    return VK_NULL_HANDLE;
}

NTSTATUS wine_vkGetDeviceQueue(void *args)
{
    struct vkGetDeviceQueue_params *params = args;
    VkDevice device = params->device;
    uint32_t family_index = params->queueFamilyIndex;
    uint32_t queue_index = params->queueIndex;
    VkQueue *queue = params->pQueue;
    VkDeviceQueueInfo2 queue_info;

    TRACE("%p, %u, %u, %p\n", device, family_index, queue_index, queue);

    queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2;
    queue_info.pNext = NULL;
    queue_info.flags = 0;
    queue_info.queueFamilyIndex = family_index;
    queue_info.queueIndex = queue_index;

    *queue = wine_vk_device_find_queue(device, &queue_info);
    return STATUS_SUCCESS;
}

NTSTATUS wine_vkGetDeviceQueue2(void *args)
{
    struct vkGetDeviceQueue2_params *params = args;
    VkDevice device = params->device;
    const VkDeviceQueueInfo2 *info = params->pQueueInfo;
    VkQueue *queue = params->pQueue;
    const VkBaseInStructure *chain;

    TRACE("%p, %p, %p\n", device, info, queue);

    if ((chain = info->pNext))
        FIXME("Ignoring a linked structure of type %u.\n", chain->sType);

    *queue = wine_vk_device_find_queue(device, info);
    return STATUS_SUCCESS;
}

NTSTATUS wine_vkCreateCommandPool(void *args)
{
    struct vkCreateCommandPool_params *params = args;
    VkDevice device = params->device;
    const VkCommandPoolCreateInfo *info = params->pCreateInfo;
    const VkAllocationCallbacks *allocator = params->pAllocator;
    VkCommandPool *command_pool = params->pCommandPool;
    struct wine_cmd_pool *object;
    VkResult res;

    TRACE("%p, %p, %p, %p\n", device, info, allocator, command_pool);

    if (allocator)
        FIXME("Support for allocation callbacks not implemented yet\n");

    if (!(object = calloc(1, sizeof(*object))))
        return VK_ERROR_OUT_OF_HOST_MEMORY;

    list_init(&object->command_buffers);

    res = device->funcs.p_vkCreateCommandPool(device->device, info, NULL, &object->command_pool);

    if (res == VK_SUCCESS)
    {
        WINE_VK_ADD_NON_DISPATCHABLE_MAPPING(device->phys_dev->instance, object, object->command_pool);
        *command_pool = wine_cmd_pool_to_handle(object);
    }
    else
    {
        free(object);
    }

    return res;
}

NTSTATUS wine_vkDestroyCommandPool(void *args)
{
    struct vkDestroyCommandPool_params *params = args;
    VkDevice device = params->device;
    VkCommandPool handle = params->commandPool;
    const VkAllocationCallbacks *allocator = params->pAllocator;
    struct wine_cmd_pool *pool = wine_cmd_pool_from_handle(handle);
    struct VkCommandBuffer_T *buffer, *cursor;

    TRACE("%p, 0x%s, %p\n", device, wine_dbgstr_longlong(handle), allocator);

    if (!handle)
        return STATUS_SUCCESS;

    if (allocator)
        FIXME("Support for allocation callbacks not implemented yet\n");

    /* The Vulkan spec says:
     *
     * "When a pool is destroyed, all command buffers allocated from the pool are freed."
     */
    LIST_FOR_EACH_ENTRY_SAFE(buffer, cursor, &pool->command_buffers, struct VkCommandBuffer_T, pool_link)
    {
        WINE_VK_REMOVE_HANDLE_MAPPING(device->phys_dev->instance, buffer);
        free(buffer);
    }

    WINE_VK_REMOVE_HANDLE_MAPPING(device->phys_dev->instance, pool);

    device->funcs.p_vkDestroyCommandPool(device->device, pool->command_pool, NULL);
    free(pool);
    return STATUS_SUCCESS;
}

static VkResult wine_vk_enumerate_physical_device_groups(struct VkInstance_T *instance,
        VkResult (*p_vkEnumeratePhysicalDeviceGroups)(VkInstance, uint32_t *, VkPhysicalDeviceGroupProperties *),
        uint32_t *count, VkPhysicalDeviceGroupProperties *properties)
{
    unsigned int i, j;
    VkResult res;

    res = p_vkEnumeratePhysicalDeviceGroups(instance->instance, count, properties);
    if (res < 0 || !properties)
        return res;

    for (i = 0; i < *count; ++i)
    {
        VkPhysicalDeviceGroupProperties *current = &properties[i];
        for (j = 0; j < current->physicalDeviceCount; ++j)
        {
            VkPhysicalDevice dev = current->physicalDevices[j];
            if (!(current->physicalDevices[j] = wine_vk_instance_wrap_physical_device(instance, dev)))
                return VK_ERROR_INITIALIZATION_FAILED;
        }
    }

    return res;
}

NTSTATUS wine_vkEnumeratePhysicalDeviceGroups(void *args)
{
    struct vkEnumeratePhysicalDeviceGroups_params *params = args;
    VkInstance instance = params->instance;
    uint32_t *count = params->pPhysicalDeviceGroupCount;
    VkPhysicalDeviceGroupProperties *properties = params->pPhysicalDeviceGroupProperties;

    TRACE("%p, %p, %p\n", instance, count, properties);
    return wine_vk_enumerate_physical_device_groups(instance,
            instance->funcs.p_vkEnumeratePhysicalDeviceGroups, count, properties);
}

NTSTATUS wine_vkEnumeratePhysicalDeviceGroupsKHR(void *args)
{
    struct vkEnumeratePhysicalDeviceGroupsKHR_params *params = args;
    VkInstance instance = params->instance;
    uint32_t *count = params->pPhysicalDeviceGroupCount;
    VkPhysicalDeviceGroupProperties *properties = params->pPhysicalDeviceGroupProperties;

    TRACE("%p, %p, %p\n", instance, count, properties);
    return wine_vk_enumerate_physical_device_groups(instance,
            instance->funcs.p_vkEnumeratePhysicalDeviceGroupsKHR, count, properties);
}

NTSTATUS wine_vkGetPhysicalDeviceExternalFenceProperties(void *args)
{
    struct vkGetPhysicalDeviceExternalFenceProperties_params *params = args;
    VkPhysicalDevice phys_dev = params->physicalDevice;
    const VkPhysicalDeviceExternalFenceInfo *fence_info = params->pExternalFenceInfo;
    VkExternalFenceProperties *properties = params->pExternalFenceProperties;

    TRACE("%p, %p, %p\n", phys_dev, fence_info, properties);
    properties->exportFromImportedHandleTypes = 0;
    properties->compatibleHandleTypes = 0;
    properties->externalFenceFeatures = 0;
    return STATUS_SUCCESS;
}

NTSTATUS wine_vkGetPhysicalDeviceExternalFencePropertiesKHR(void *args)
{
    struct vkGetPhysicalDeviceExternalFencePropertiesKHR_params *params = args;
    VkPhysicalDevice phys_dev = params->physicalDevice;
    const VkPhysicalDeviceExternalFenceInfo *fence_info = params->pExternalFenceInfo;
    VkExternalFenceProperties *properties = params->pExternalFenceProperties;

    TRACE("%p, %p, %p\n", phys_dev, fence_info, properties);
    properties->exportFromImportedHandleTypes = 0;
    properties->compatibleHandleTypes = 0;
    properties->externalFenceFeatures = 0;
    return STATUS_SUCCESS;
}

NTSTATUS wine_vkGetPhysicalDeviceExternalBufferProperties(void *args)
{
    struct vkGetPhysicalDeviceExternalBufferProperties_params *params = args;
    VkPhysicalDevice phys_dev = params->physicalDevice;
    const VkPhysicalDeviceExternalBufferInfo *buffer_info = params->pExternalBufferInfo;
    VkExternalBufferProperties *properties = params->pExternalBufferProperties;

    TRACE("%p, %p, %p\n", phys_dev, buffer_info, properties);
    memset(&properties->externalMemoryProperties, 0, sizeof(properties->externalMemoryProperties));
    return STATUS_SUCCESS;
}

NTSTATUS wine_vkGetPhysicalDeviceExternalBufferPropertiesKHR(void *args)
{
    struct vkGetPhysicalDeviceExternalBufferPropertiesKHR_params *params = args;
    VkPhysicalDevice phys_dev = params->physicalDevice;
    const VkPhysicalDeviceExternalBufferInfo *buffer_info = params->pExternalBufferInfo;
    VkExternalBufferProperties *properties = params->pExternalBufferProperties;

    TRACE("%p, %p, %p\n", phys_dev, buffer_info, properties);
    memset(&properties->externalMemoryProperties, 0, sizeof(properties->externalMemoryProperties));
    return STATUS_SUCCESS;
}

NTSTATUS wine_vkGetPhysicalDeviceImageFormatProperties2(void *args)
{
    struct vkGetPhysicalDeviceImageFormatProperties2_params *params = args;
    VkPhysicalDevice phys_dev = params->physicalDevice;
    const VkPhysicalDeviceImageFormatInfo2 *format_info = params->pImageFormatInfo;
    VkImageFormatProperties2 *properties = params->pImageFormatProperties;
    VkExternalImageFormatProperties *external_image_properties;
    VkResult res;

    TRACE("%p, %p, %p\n", phys_dev, format_info, properties);

    res = thunk_vkGetPhysicalDeviceImageFormatProperties2(phys_dev, format_info, properties);

    if ((external_image_properties = wine_vk_find_struct(properties, EXTERNAL_IMAGE_FORMAT_PROPERTIES)))
    {
        VkExternalMemoryProperties *p = &external_image_properties->externalMemoryProperties;
        p->externalMemoryFeatures = 0;
        p->exportFromImportedHandleTypes = 0;
        p->compatibleHandleTypes = 0;
    }

    return res;
}

NTSTATUS wine_vkGetPhysicalDeviceImageFormatProperties2KHR(void *args)
{
    struct vkGetPhysicalDeviceImageFormatProperties2KHR_params *params = args;
    VkPhysicalDevice phys_dev = params->physicalDevice;
    const VkPhysicalDeviceImageFormatInfo2 *format_info = params->pImageFormatInfo;
    VkImageFormatProperties2 *properties = params->pImageFormatProperties;
    VkExternalImageFormatProperties *external_image_properties;
    VkResult res;

    TRACE("%p, %p, %p\n", phys_dev, format_info, properties);

    res = thunk_vkGetPhysicalDeviceImageFormatProperties2KHR(phys_dev, format_info, properties);

    if ((external_image_properties = wine_vk_find_struct(properties, EXTERNAL_IMAGE_FORMAT_PROPERTIES)))
    {
        VkExternalMemoryProperties *p = &external_image_properties->externalMemoryProperties;
        p->externalMemoryFeatures = 0;
        p->exportFromImportedHandleTypes = 0;
        p->compatibleHandleTypes = 0;
    }

    return res;
}

/* From ntdll/unix/sync.c */
#define NANOSECONDS_IN_A_SECOND 1000000000
#define TICKSPERSEC             10000000

static inline VkTimeDomainEXT get_performance_counter_time_domain(void)
{
#if !defined(__APPLE__) && defined(HAVE_CLOCK_GETTIME)
# ifdef CLOCK_MONOTONIC_RAW
    return VK_TIME_DOMAIN_CLOCK_MONOTONIC_RAW_EXT;
# else
    return VK_TIME_DOMAIN_CLOCK_MONOTONIC_EXT;
# endif
#else
    FIXME("No mapping for VK_TIME_DOMAIN_QUERY_PERFORMANCE_COUNTER_EXT on this platform.\n");
    return VK_TIME_DOMAIN_QUERY_PERFORMANCE_COUNTER_EXT;
#endif
}

static VkTimeDomainEXT map_to_host_time_domain(VkTimeDomainEXT domain)
{
    /* Matches ntdll/unix/sync.c's performance counter implementation. */
    if (domain == VK_TIME_DOMAIN_QUERY_PERFORMANCE_COUNTER_EXT)
        return get_performance_counter_time_domain();

    return domain;
}

static inline uint64_t convert_monotonic_timestamp(uint64_t value)
{
    return value / (NANOSECONDS_IN_A_SECOND / TICKSPERSEC);
}

static inline uint64_t convert_timestamp(VkTimeDomainEXT host_domain, VkTimeDomainEXT target_domain, uint64_t value)
{
    if (host_domain == target_domain)
        return value;

    /* Convert between MONOTONIC time in ns -> QueryPerformanceCounter */
    if ((host_domain == VK_TIME_DOMAIN_CLOCK_MONOTONIC_RAW_EXT || host_domain == VK_TIME_DOMAIN_CLOCK_MONOTONIC_EXT)
            && target_domain == VK_TIME_DOMAIN_QUERY_PERFORMANCE_COUNTER_EXT)
        return convert_monotonic_timestamp(value);

    FIXME("Couldn't translate between host domain %d and target domain %d\n", host_domain, target_domain);
    return value;
}

NTSTATUS wine_vkGetCalibratedTimestampsEXT(void *args)
{
    struct vkGetCalibratedTimestampsEXT_params *params = args;
    VkDevice device = params->device;
    uint32_t timestamp_count = params->timestampCount;
    const VkCalibratedTimestampInfoEXT *timestamp_infos = params->pTimestampInfos;
    uint64_t *timestamps = params->pTimestamps;
    uint64_t *max_deviation = params->pMaxDeviation;
    VkCalibratedTimestampInfoEXT* host_timestamp_infos;
    unsigned int i;
    VkResult res;
    TRACE("%p, %u, %p, %p, %p\n", device, timestamp_count, timestamp_infos, timestamps, max_deviation);

    if (!(host_timestamp_infos = malloc(sizeof(VkCalibratedTimestampInfoEXT) * timestamp_count)))
        return VK_ERROR_OUT_OF_HOST_MEMORY;

    for (i = 0; i < timestamp_count; i++)
    {
        host_timestamp_infos[i].sType = timestamp_infos[i].sType;
        host_timestamp_infos[i].pNext = timestamp_infos[i].pNext;
        host_timestamp_infos[i].timeDomain = map_to_host_time_domain(timestamp_infos[i].timeDomain);
    }

    res = device->funcs.p_vkGetCalibratedTimestampsEXT(device->device, timestamp_count, host_timestamp_infos, timestamps, max_deviation);
    if (res != VK_SUCCESS)
        return res;

    for (i = 0; i < timestamp_count; i++)
        timestamps[i] = convert_timestamp(host_timestamp_infos[i].timeDomain, timestamp_infos[i].timeDomain, timestamps[i]);

    free(host_timestamp_infos);

    return res;
}

NTSTATUS wine_vkGetPhysicalDeviceCalibrateableTimeDomainsEXT(void *args)
{
    struct vkGetPhysicalDeviceCalibrateableTimeDomainsEXT_params *params = args;
    VkPhysicalDevice phys_dev = params->physicalDevice;
    uint32_t *time_domain_count = params->pTimeDomainCount;
    VkTimeDomainEXT *time_domains = params->pTimeDomains;
    BOOL supports_device = FALSE, supports_monotonic = FALSE, supports_monotonic_raw = FALSE;
    const VkTimeDomainEXT performance_counter_domain = get_performance_counter_time_domain();
    VkTimeDomainEXT *host_time_domains;
    uint32_t host_time_domain_count;
    VkTimeDomainEXT out_time_domains[2];
    uint32_t out_time_domain_count;
    unsigned int i;
    VkResult res;

    TRACE("%p, %p, %p\n", phys_dev, time_domain_count, time_domains);

    /* Find out the time domains supported on the host */
    res = phys_dev->instance->funcs.p_vkGetPhysicalDeviceCalibrateableTimeDomainsEXT(phys_dev->phys_dev, &host_time_domain_count, NULL);
    if (res != VK_SUCCESS)
        return res;

    if (!(host_time_domains = malloc(sizeof(VkTimeDomainEXT) * host_time_domain_count)))
        return VK_ERROR_OUT_OF_HOST_MEMORY;

    res = phys_dev->instance->funcs.p_vkGetPhysicalDeviceCalibrateableTimeDomainsEXT(phys_dev->phys_dev, &host_time_domain_count, host_time_domains);
    if (res != VK_SUCCESS)
    {
        free(host_time_domains);
        return res;
    }

    for (i = 0; i < host_time_domain_count; i++)
    {
        if (host_time_domains[i] == VK_TIME_DOMAIN_DEVICE_EXT)
            supports_device = TRUE;
        else if (host_time_domains[i] == VK_TIME_DOMAIN_CLOCK_MONOTONIC_EXT)
            supports_monotonic = TRUE;
        else if (host_time_domains[i] == VK_TIME_DOMAIN_CLOCK_MONOTONIC_RAW_EXT)
            supports_monotonic_raw = TRUE;
        else
            FIXME("Unknown time domain %d\n", host_time_domains[i]);
    }

    free(host_time_domains);

    out_time_domain_count = 0;

    /* Map our monotonic times -> QPC */
    if (supports_monotonic_raw && performance_counter_domain == VK_TIME_DOMAIN_CLOCK_MONOTONIC_RAW_EXT)
        out_time_domains[out_time_domain_count++] = VK_TIME_DOMAIN_QUERY_PERFORMANCE_COUNTER_EXT;
    else if (supports_monotonic && performance_counter_domain == VK_TIME_DOMAIN_CLOCK_MONOTONIC_EXT)
        out_time_domains[out_time_domain_count++] = VK_TIME_DOMAIN_QUERY_PERFORMANCE_COUNTER_EXT;
    else
        FIXME("VK_TIME_DOMAIN_QUERY_PERFORMANCE_COUNTER_EXT not supported on this platform.\n");

    /* Forward the device domain time */
    if (supports_device)
        out_time_domains[out_time_domain_count++] = VK_TIME_DOMAIN_DEVICE_EXT;

    /* Send the count/domains back to the app */
    if (!time_domains)
    {
        *time_domain_count = out_time_domain_count;
        return VK_SUCCESS;
    }

    for (i = 0; i < min(*time_domain_count, out_time_domain_count); i++)
        time_domains[i] = out_time_domains[i];

    res = *time_domain_count < out_time_domain_count ? VK_INCOMPLETE : VK_SUCCESS;
    *time_domain_count = out_time_domain_count;
    return res;
}

NTSTATUS wine_vkGetPhysicalDeviceExternalSemaphoreProperties(void *args)
{
    struct vkGetPhysicalDeviceExternalSemaphoreProperties_params *params = args;
    VkPhysicalDevice phys_dev = params->physicalDevice;
    const VkPhysicalDeviceExternalSemaphoreInfo *semaphore_info = params->pExternalSemaphoreInfo;
    VkExternalSemaphoreProperties *properties = params->pExternalSemaphoreProperties;

    TRACE("%p, %p, %p\n", phys_dev, semaphore_info, properties);
    properties->exportFromImportedHandleTypes = 0;
    properties->compatibleHandleTypes = 0;
    properties->externalSemaphoreFeatures = 0;
    return STATUS_SUCCESS;
}

NTSTATUS wine_vkGetPhysicalDeviceExternalSemaphorePropertiesKHR(void *args)
{
    struct vkGetPhysicalDeviceExternalSemaphorePropertiesKHR_params *params = args;
    VkPhysicalDevice phys_dev = params->physicalDevice;
    const VkPhysicalDeviceExternalSemaphoreInfo *semaphore_info = params->pExternalSemaphoreInfo;
    VkExternalSemaphoreProperties *properties = params->pExternalSemaphoreProperties;

    TRACE("%p, %p, %p\n", phys_dev, semaphore_info, properties);
    properties->exportFromImportedHandleTypes = 0;
    properties->compatibleHandleTypes = 0;
    properties->externalSemaphoreFeatures = 0;
    return STATUS_SUCCESS;
}

VkResult WINAPI wine_vkSetPrivateDataEXT(VkDevice device, VkObjectType object_type, uint64_t object_handle,
        VkPrivateDataSlotEXT private_data_slot, uint64_t data)
{
    TRACE("%p, %#x, 0x%s, 0x%s, 0x%s\n", device, object_type, wine_dbgstr_longlong(object_handle),
            wine_dbgstr_longlong(private_data_slot), wine_dbgstr_longlong(data));

    object_handle = wine_vk_unwrap_handle(object_type, object_handle);
    return device->funcs.p_vkSetPrivateDataEXT(device->device, object_type, object_handle, private_data_slot, data);
}

void WINAPI wine_vkGetPrivateDataEXT(VkDevice device, VkObjectType object_type, uint64_t object_handle,
        VkPrivateDataSlotEXT private_data_slot, uint64_t *data)
{
    TRACE("%p, %#x, 0x%s, 0x%s, %p\n", device, object_type, wine_dbgstr_longlong(object_handle),
            wine_dbgstr_longlong(private_data_slot), data);

    object_handle = wine_vk_unwrap_handle(object_type, object_handle);
    device->funcs.p_vkGetPrivateDataEXT(device->device, object_type, object_handle, private_data_slot, data);
}

/*
#version 460

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(binding = 0) uniform sampler2D texSampler;
layout(binding = 1) uniform writeonly image2D outImage;
layout(push_constant) uniform pushConstants {
    //both in real image coords
    vec2 offset;
    vec2 extents;
} constants;

void main()
{
    vec2 texcoord = (vec2(gl_GlobalInvocationID.xy) - constants.offset) / constants.extents;
    vec4 c = texture(texSampler, texcoord);

    // Convert linear -> srgb
    bvec3 isLo = lessThanEqual(c.rgb, vec3(0.0031308f));
    vec3 loPart = c.rgb * 12.92f;
    vec3 hiPart = pow(c.rgb, vec3(5.0f / 12.0f)) * 1.055f - 0.055f;
    c.rgb = mix(hiPart, loPart, isLo);

    imageStore(outImage, ivec2(gl_GlobalInvocationID.xy), c);
}

*/
const uint32_t blit_comp_spv[] = {
    0x07230203,0x00010000,0x0008000a,0x0000005e,0x00000000,0x00020011,0x00000001,0x00020011,
    0x00000038,0x0006000b,0x00000001,0x4c534c47,0x6474732e,0x3035342e,0x00000000,0x0003000e,
    0x00000000,0x00000001,0x0006000f,0x00000005,0x00000004,0x6e69616d,0x00000000,0x0000000d,
    0x00060010,0x00000004,0x00000011,0x00000008,0x00000008,0x00000001,0x00030003,0x00000002,
    0x000001cc,0x00040005,0x00000004,0x6e69616d,0x00000000,0x00050005,0x00000009,0x63786574,
    0x64726f6f,0x00000000,0x00080005,0x0000000d,0x475f6c67,0x61626f6c,0x766e496c,0x7461636f,
    0x496e6f69,0x00000044,0x00060005,0x00000012,0x68737570,0x736e6f43,0x746e6174,0x00000073,
    0x00050006,0x00000012,0x00000000,0x7366666f,0x00007465,0x00050006,0x00000012,0x00000001,
    0x65747865,0x0073746e,0x00050005,0x00000014,0x736e6f63,0x746e6174,0x00000073,0x00030005,
    0x00000021,0x00000063,0x00050005,0x00000025,0x53786574,0x6c706d61,0x00007265,0x00040005,
    0x0000002d,0x6f4c7369,0x00000000,0x00040005,0x00000035,0x61506f6c,0x00007472,0x00040005,
    0x0000003a,0x61506968,0x00007472,0x00050005,0x00000055,0x4974756f,0x6567616d,0x00000000,
    0x00040047,0x0000000d,0x0000000b,0x0000001c,0x00050048,0x00000012,0x00000000,0x00000023,
    0x00000000,0x00050048,0x00000012,0x00000001,0x00000023,0x00000008,0x00030047,0x00000012,
    0x00000002,0x00040047,0x00000025,0x00000022,0x00000000,0x00040047,0x00000025,0x00000021,
    0x00000000,0x00040047,0x00000055,0x00000022,0x00000000,0x00040047,0x00000055,0x00000021,
    0x00000001,0x00030047,0x00000055,0x00000019,0x00040047,0x0000005d,0x0000000b,0x00000019,
    0x00020013,0x00000002,0x00030021,0x00000003,0x00000002,0x00030016,0x00000006,0x00000020,
    0x00040017,0x00000007,0x00000006,0x00000002,0x00040020,0x00000008,0x00000007,0x00000007,
    0x00040015,0x0000000a,0x00000020,0x00000000,0x00040017,0x0000000b,0x0000000a,0x00000003,
    0x00040020,0x0000000c,0x00000001,0x0000000b,0x0004003b,0x0000000c,0x0000000d,0x00000001,
    0x00040017,0x0000000e,0x0000000a,0x00000002,0x0004001e,0x00000012,0x00000007,0x00000007,
    0x00040020,0x00000013,0x00000009,0x00000012,0x0004003b,0x00000013,0x00000014,0x00000009,
    0x00040015,0x00000015,0x00000020,0x00000001,0x0004002b,0x00000015,0x00000016,0x00000000,
    0x00040020,0x00000017,0x00000009,0x00000007,0x0004002b,0x00000015,0x0000001b,0x00000001,
    0x00040017,0x0000001f,0x00000006,0x00000004,0x00040020,0x00000020,0x00000007,0x0000001f,
    0x00090019,0x00000022,0x00000006,0x00000001,0x00000000,0x00000000,0x00000000,0x00000001,
    0x00000000,0x0003001b,0x00000023,0x00000022,0x00040020,0x00000024,0x00000000,0x00000023,
    0x0004003b,0x00000024,0x00000025,0x00000000,0x0004002b,0x00000006,0x00000028,0x00000000,
    0x00020014,0x0000002a,0x00040017,0x0000002b,0x0000002a,0x00000003,0x00040020,0x0000002c,
    0x00000007,0x0000002b,0x00040017,0x0000002e,0x00000006,0x00000003,0x0004002b,0x00000006,
    0x00000031,0x3b4d2e1c,0x0006002c,0x0000002e,0x00000032,0x00000031,0x00000031,0x00000031,
    0x00040020,0x00000034,0x00000007,0x0000002e,0x0004002b,0x00000006,0x00000038,0x414eb852,
    0x0004002b,0x00000006,0x0000003d,0x3ed55555,0x0006002c,0x0000002e,0x0000003e,0x0000003d,
    0x0000003d,0x0000003d,0x0004002b,0x00000006,0x00000040,0x3f870a3d,0x0004002b,0x00000006,
    0x00000042,0x3d6147ae,0x0004002b,0x0000000a,0x00000049,0x00000000,0x00040020,0x0000004a,
    0x00000007,0x00000006,0x0004002b,0x0000000a,0x0000004d,0x00000001,0x0004002b,0x0000000a,
    0x00000050,0x00000002,0x00090019,0x00000053,0x00000006,0x00000001,0x00000000,0x00000000,
    0x00000000,0x00000002,0x00000000,0x00040020,0x00000054,0x00000000,0x00000053,0x0004003b,
    0x00000054,0x00000055,0x00000000,0x00040017,0x00000059,0x00000015,0x00000002,0x0004002b,
    0x0000000a,0x0000005c,0x00000008,0x0006002c,0x0000000b,0x0000005d,0x0000005c,0x0000005c,
    0x0000004d,0x00050036,0x00000002,0x00000004,0x00000000,0x00000003,0x000200f8,0x00000005,
    0x0004003b,0x00000008,0x00000009,0x00000007,0x0004003b,0x00000020,0x00000021,0x00000007,
    0x0004003b,0x0000002c,0x0000002d,0x00000007,0x0004003b,0x00000034,0x00000035,0x00000007,
    0x0004003b,0x00000034,0x0000003a,0x00000007,0x0004003d,0x0000000b,0x0000000f,0x0000000d,
    0x0007004f,0x0000000e,0x00000010,0x0000000f,0x0000000f,0x00000000,0x00000001,0x00040070,
    0x00000007,0x00000011,0x00000010,0x00050041,0x00000017,0x00000018,0x00000014,0x00000016,
    0x0004003d,0x00000007,0x00000019,0x00000018,0x00050083,0x00000007,0x0000001a,0x00000011,
    0x00000019,0x00050041,0x00000017,0x0000001c,0x00000014,0x0000001b,0x0004003d,0x00000007,
    0x0000001d,0x0000001c,0x00050088,0x00000007,0x0000001e,0x0000001a,0x0000001d,0x0003003e,
    0x00000009,0x0000001e,0x0004003d,0x00000023,0x00000026,0x00000025,0x0004003d,0x00000007,
    0x00000027,0x00000009,0x00070058,0x0000001f,0x00000029,0x00000026,0x00000027,0x00000002,
    0x00000028,0x0003003e,0x00000021,0x00000029,0x0004003d,0x0000001f,0x0000002f,0x00000021,
    0x0008004f,0x0000002e,0x00000030,0x0000002f,0x0000002f,0x00000000,0x00000001,0x00000002,
    0x000500bc,0x0000002b,0x00000033,0x00000030,0x00000032,0x0003003e,0x0000002d,0x00000033,
    0x0004003d,0x0000001f,0x00000036,0x00000021,0x0008004f,0x0000002e,0x00000037,0x00000036,
    0x00000036,0x00000000,0x00000001,0x00000002,0x0005008e,0x0000002e,0x00000039,0x00000037,
    0x00000038,0x0003003e,0x00000035,0x00000039,0x0004003d,0x0000001f,0x0000003b,0x00000021,
    0x0008004f,0x0000002e,0x0000003c,0x0000003b,0x0000003b,0x00000000,0x00000001,0x00000002,
    0x0007000c,0x0000002e,0x0000003f,0x00000001,0x0000001a,0x0000003c,0x0000003e,0x0005008e,
    0x0000002e,0x00000041,0x0000003f,0x00000040,0x00060050,0x0000002e,0x00000043,0x00000042,
    0x00000042,0x00000042,0x00050083,0x0000002e,0x00000044,0x00000041,0x00000043,0x0003003e,
    0x0000003a,0x00000044,0x0004003d,0x0000002e,0x00000045,0x0000003a,0x0004003d,0x0000002e,
    0x00000046,0x00000035,0x0004003d,0x0000002b,0x00000047,0x0000002d,0x000600a9,0x0000002e,
    0x00000048,0x00000047,0x00000046,0x00000045,0x00050041,0x0000004a,0x0000004b,0x00000021,
    0x00000049,0x00050051,0x00000006,0x0000004c,0x00000048,0x00000000,0x0003003e,0x0000004b,
    0x0000004c,0x00050041,0x0000004a,0x0000004e,0x00000021,0x0000004d,0x00050051,0x00000006,
    0x0000004f,0x00000048,0x00000001,0x0003003e,0x0000004e,0x0000004f,0x00050041,0x0000004a,
    0x00000051,0x00000021,0x00000050,0x00050051,0x00000006,0x00000052,0x00000048,0x00000002,
    0x0003003e,0x00000051,0x00000052,0x0004003d,0x00000053,0x00000056,0x00000055,0x0004003d,
    0x0000000b,0x00000057,0x0000000d,0x0007004f,0x0000000e,0x00000058,0x00000057,0x00000057,
    0x00000000,0x00000001,0x0004007c,0x00000059,0x0000005a,0x00000058,0x0004003d,0x0000001f,
    0x0000005b,0x00000021,0x00040063,0x00000056,0x0000005a,0x0000005b,0x000100fd,0x00010038
};

static VkResult create_pipeline(VkDevice device, struct VkSwapchainKHR_T *swapchain, VkShaderModule shaderModule)
{
    VkResult res;
#if defined(USE_STRUCT_CONVERSION)
    VkComputePipelineCreateInfo_host pipelineInfo = {0};
#else
    VkComputePipelineCreateInfo pipelineInfo = {0};
#endif

    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineInfo.stage.module = shaderModule;
    pipelineInfo.stage.pName = "main";
    pipelineInfo.layout = swapchain->pipeline_layout;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineInfo.basePipelineIndex = -1;

    res = device->funcs.p_vkCreateComputePipelines(device->device, VK_NULL_HANDLE, 1, &pipelineInfo, NULL, &swapchain->pipeline);
    if(res != VK_SUCCESS){
        ERR("vkCreateComputePipelines: %d\n", res);
        return res;
    }

    return VK_SUCCESS;
}

static VkResult create_descriptor_set(VkDevice device, struct VkSwapchainKHR_T *swapchain, struct fs_hack_image *hack)
{
    VkResult res;
#if defined(USE_STRUCT_CONVERSION)
    VkDescriptorSetAllocateInfo_host descriptorAllocInfo = {0};
    VkWriteDescriptorSet_host descriptorWrites[2] = {{0}, {0}};
    VkDescriptorImageInfo_host userDescriptorImageInfo = {0}, realDescriptorImageInfo = {0};
#else
    VkDescriptorSetAllocateInfo descriptorAllocInfo = {0};
    VkWriteDescriptorSet descriptorWrites[2] = {{0}, {0}};
    VkDescriptorImageInfo userDescriptorImageInfo = {0}, realDescriptorImageInfo = {0};
#endif

    descriptorAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptorAllocInfo.descriptorPool = swapchain->descriptor_pool;
    descriptorAllocInfo.descriptorSetCount = 1;
    descriptorAllocInfo.pSetLayouts = &swapchain->descriptor_set_layout;

    res = device->funcs.p_vkAllocateDescriptorSets(device->device, &descriptorAllocInfo, &hack->descriptor_set);
    if(res != VK_SUCCESS){
        ERR("vkAllocateDescriptorSets: %d\n", res);
        return res;
    }

    userDescriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    userDescriptorImageInfo.imageView = hack->user_view;
    userDescriptorImageInfo.sampler = swapchain->sampler;

    realDescriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    realDescriptorImageInfo.imageView = hack->blit_view;

    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = hack->descriptor_set;
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pImageInfo = &userDescriptorImageInfo;

    descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[1].dstSet = hack->descriptor_set;
    descriptorWrites[1].dstBinding = 1;
    descriptorWrites[1].dstArrayElement = 0;
    descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].pImageInfo = &realDescriptorImageInfo;

    device->funcs.p_vkUpdateDescriptorSets(device->device, 2, descriptorWrites, 0, NULL);

    return VK_SUCCESS;
}

static VkResult init_blit_images(VkDevice device, struct VkSwapchainKHR_T *swapchain)
{
    VkResult res;
    VkSamplerCreateInfo samplerInfo = {0};
    VkDescriptorPoolSize poolSizes[2] = {{0}, {0}};
    VkDescriptorPoolCreateInfo poolInfo = {0};
    VkDescriptorSetLayoutBinding layoutBindings[2] = {{0}, {0}};
    VkDescriptorSetLayoutCreateInfo descriptorLayoutInfo = {0};
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {0};
    VkPushConstantRange pushConstants;
    VkShaderModuleCreateInfo shaderInfo = {0};
    VkShaderModule shaderModule = 0;
#if defined(USE_STRUCT_CONVERSION)
    VkImageViewCreateInfo_host viewInfo = {0};
#else
    VkImageViewCreateInfo viewInfo = {0};
#endif
    uint32_t i;

    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = swapchain->fs_hack_filter;
    samplerInfo.minFilter = swapchain->fs_hack_filter;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1;
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;

    res = device->funcs.p_vkCreateSampler(device->device, &samplerInfo, NULL, &swapchain->sampler);
    if(res != VK_SUCCESS)
    {
        WARN("vkCreateSampler failed, res=%d\n", res);
        return res;
    }

    poolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[0].descriptorCount = swapchain->n_images;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    poolSizes[1].descriptorCount = swapchain->n_images;

    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 2;
    poolInfo.pPoolSizes = poolSizes;
    poolInfo.maxSets = swapchain->n_images;

    res = device->funcs.p_vkCreateDescriptorPool(device->device, &poolInfo, NULL, &swapchain->descriptor_pool);
    if(res != VK_SUCCESS){
        ERR("vkCreateDescriptorPool: %d\n", res);
        goto fail;
    }

    layoutBindings[0].binding = 0;
    layoutBindings[0].descriptorCount = 1;
    layoutBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    layoutBindings[0].pImmutableSamplers = NULL;
    layoutBindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    layoutBindings[1].binding = 1;
    layoutBindings[1].descriptorCount = 1;
    layoutBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    layoutBindings[1].pImmutableSamplers = NULL;
    layoutBindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    descriptorLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorLayoutInfo.bindingCount = 2;
    descriptorLayoutInfo.pBindings = layoutBindings;

    res = device->funcs.p_vkCreateDescriptorSetLayout(device->device, &descriptorLayoutInfo, NULL, &swapchain->descriptor_set_layout);
    if(res != VK_SUCCESS){
        ERR("vkCreateDescriptorSetLayout: %d\n", res);
        goto fail;
    }

    pushConstants.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushConstants.offset = 0;
    pushConstants.size = 4 * sizeof(float); /* 2 * vec2 */

    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &swapchain->descriptor_set_layout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstants;

    res = device->funcs.p_vkCreatePipelineLayout(device->device, &pipelineLayoutInfo, NULL, &swapchain->pipeline_layout);
    if(res != VK_SUCCESS){
        ERR("vkCreatePipelineLayout: %d\n", res);
        goto fail;
    }

    shaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderInfo.codeSize = sizeof(blit_comp_spv);
    shaderInfo.pCode = blit_comp_spv;

    res = device->funcs.p_vkCreateShaderModule(device->device, &shaderInfo, NULL, &shaderModule);
    if(res != VK_SUCCESS){
        ERR("vkCreateShaderModule: %d\n", res);
        goto fail;
    }

    res = create_pipeline(device, swapchain, shaderModule);
    if(res != VK_SUCCESS)
        goto fail;

    device->funcs.p_vkDestroyShaderModule(device->device, shaderModule, NULL);

    /* create imageviews */
    for(i = 0; i < swapchain->n_images; ++i){
        struct fs_hack_image *hack = &swapchain->fs_hack_images[i];

        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = hack->swapchain_image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = VK_FORMAT_B8G8R8A8_UNORM;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        res = device->funcs.p_vkCreateImageView(device->device, &viewInfo, NULL, &hack->blit_view);
        if(res != VK_SUCCESS){
            ERR("vkCreateImageView(blit): %d\n", res);
            goto fail;
        }

        res = create_descriptor_set(device, swapchain, hack);
        if(res != VK_SUCCESS)
            goto fail;
    }

    return VK_SUCCESS;

fail:
    for(i = 0; i < swapchain->n_images; ++i){
        struct fs_hack_image *hack = &swapchain->fs_hack_images[i];

        device->funcs.p_vkDestroyImageView(device->device, hack->blit_view, NULL);
        hack->blit_view = VK_NULL_HANDLE;
    }

    device->funcs.p_vkDestroyShaderModule(device->device, shaderModule, NULL);

    device->funcs.p_vkDestroyPipeline(device->device, swapchain->pipeline, NULL);
    swapchain->pipeline = VK_NULL_HANDLE;

    device->funcs.p_vkDestroyPipelineLayout(device->device, swapchain->pipeline_layout, NULL);
    swapchain->pipeline_layout = VK_NULL_HANDLE;

    device->funcs.p_vkDestroyDescriptorSetLayout(device->device, swapchain->descriptor_set_layout, NULL);
    swapchain->descriptor_set_layout = VK_NULL_HANDLE;

    device->funcs.p_vkDestroyDescriptorPool(device->device, swapchain->descriptor_pool, NULL);
    swapchain->descriptor_pool = VK_NULL_HANDLE;

    device->funcs.p_vkDestroySampler(device->device, swapchain->sampler, NULL);
    swapchain->sampler = VK_NULL_HANDLE;

    return res;
}

static void destroy_fs_hack_image(VkDevice device, struct VkSwapchainKHR_T *swapchain, struct fs_hack_image *hack)
{
    device->funcs.p_vkDestroyImageView(device->device, hack->user_view, NULL);
    device->funcs.p_vkDestroyImageView(device->device, hack->blit_view, NULL);
    device->funcs.p_vkDestroyImage(device->device, hack->user_image, NULL);
    if(hack->cmd)
        device->funcs.p_vkFreeCommandBuffers(device->device,
                swapchain->cmd_pools[hack->cmd_queue_idx],
                    1, &hack->cmd);
    device->funcs.p_vkDestroySemaphore(device->device, hack->blit_finished, NULL);
}


static VkResult init_fs_hack_images(VkDevice device, struct VkSwapchainKHR_T *swapchain, const VkSwapchainCreateInfoKHR *createinfo)
{
    VkResult res;
    VkImage *real_images = NULL;
    VkDeviceSize userMemTotal = 0, offs;
    VkImageCreateInfo imageInfo = {0};
    VkSemaphoreCreateInfo semaphoreInfo = {0};
#if defined(USE_STRUCT_CONVERSION)
    VkMemoryRequirements_host userMemReq;
    VkMemoryAllocateInfo_host allocInfo = {0};
    VkPhysicalDeviceMemoryProperties_host memProperties;
    VkImageViewCreateInfo_host viewInfo = {0};
#else
    VkMemoryRequirements userMemReq;
    VkMemoryAllocateInfo allocInfo = {0};
    VkPhysicalDeviceMemoryProperties memProperties;
    VkImageViewCreateInfo viewInfo = {0};
#endif
    uint32_t count, i = 0, user_memory_type = -1;

    res = device->funcs.p_vkGetSwapchainImagesKHR(device->device, swapchain->swapchain, &count, NULL);
    if(res != VK_SUCCESS)
    {
        WARN("vkGetSwapchainImagesKHR failed, res=%d\n", res);
        return res;
    }

    real_images = malloc(count * sizeof(VkImage));
    swapchain->cmd_pools = calloc(device->queue_count, sizeof(VkCommandPool));
    swapchain->fs_hack_images = calloc(count, sizeof(struct fs_hack_image));
    if(!real_images || !swapchain->cmd_pools || !swapchain->fs_hack_images)
        goto fail;

    res = device->funcs.p_vkGetSwapchainImagesKHR(device->device, swapchain->swapchain, &count, real_images);
    if(res != VK_SUCCESS)
    {
        WARN("vkGetSwapchainImagesKHR failed, res=%d\n", res);
        goto fail;
    }

    /* create user images */
    for(i = 0; i < count; ++i){
        struct fs_hack_image *hack = &swapchain->fs_hack_images[i];

        hack->swapchain_image = real_images[i];

        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        res = device->funcs.p_vkCreateSemaphore(device->device, &semaphoreInfo, NULL, &hack->blit_finished);
        if(res != VK_SUCCESS)
        {
            WARN("vkCreateSemaphore failed, res=%d\n", res);
            goto fail;
        }

        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = swapchain->user_extent.width;
        imageInfo.extent.height = swapchain->user_extent.height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = createinfo->imageArrayLayers;
        imageInfo.format = createinfo->imageFormat;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = createinfo->imageUsage | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageInfo.sharingMode = createinfo->imageSharingMode;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.queueFamilyIndexCount = createinfo->queueFamilyIndexCount;
        imageInfo.pQueueFamilyIndices = createinfo->pQueueFamilyIndices;

        if (createinfo-> flags & VK_SWAPCHAIN_CREATE_MUTABLE_FORMAT_BIT_KHR)
            imageInfo.flags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT | VK_IMAGE_CREATE_EXTENDED_USAGE_BIT;
        else if (createinfo->imageFormat != VK_FORMAT_B8G8R8A8_SRGB)
            imageInfo.flags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;

        res = device->funcs.p_vkCreateImage(device->device, &imageInfo, NULL, &hack->user_image);
        if(res != VK_SUCCESS){
            ERR("vkCreateImage failed: %d\n", res);
            goto fail;
        }

        device->funcs.p_vkGetImageMemoryRequirements(device->device, hack->user_image, &userMemReq);

        offs = userMemTotal % userMemReq.alignment;
        if(offs)
            userMemTotal += userMemReq.alignment - offs;

        userMemTotal += userMemReq.size;

        swapchain->n_images++;
    }

    /* allocate backing memory */
    device->phys_dev->instance->funcs.p_vkGetPhysicalDeviceMemoryProperties(device->phys_dev->phys_dev, &memProperties);

    for (i = 0; i < memProperties.memoryTypeCount; i++){
        if((memProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) == VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT){
            if(userMemReq.memoryTypeBits & (1 << i)){
                user_memory_type = i;
                break;
            }
        }
    }

    if(user_memory_type == -1){
        ERR("unable to find suitable memory type\n");
        res = VK_ERROR_OUT_OF_HOST_MEMORY;
        goto fail;
    }

    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = userMemTotal;
    allocInfo.memoryTypeIndex = user_memory_type;

    res = device->funcs.p_vkAllocateMemory(device->device, &allocInfo, NULL, &swapchain->user_image_memory);
    if(res != VK_SUCCESS){
        ERR("vkAllocateMemory: %d\n", res);
        goto fail;
    }

    /* bind backing memory and create imageviews */
    userMemTotal = 0;
    for(i = 0; i < count; ++i){
        device->funcs.p_vkGetImageMemoryRequirements(device->device, swapchain->fs_hack_images[i].user_image, &userMemReq);

        offs = userMemTotal % userMemReq.alignment;
        if(offs)
            userMemTotal += userMemReq.alignment - offs;

        res = device->funcs.p_vkBindImageMemory(device->device, swapchain->fs_hack_images[i].user_image, swapchain->user_image_memory, userMemTotal);
        if(res != VK_SUCCESS){
            ERR("vkBindImageMemory: %d\n", res);
            goto fail;
        }

        userMemTotal += userMemReq.size;

        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = swapchain->fs_hack_images[i].user_image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = VK_FORMAT_B8G8R8A8_SRGB;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        res = device->funcs.p_vkCreateImageView(device->device, &viewInfo, NULL, &swapchain->fs_hack_images[i].user_view);
        if(res != VK_SUCCESS){
            ERR("vkCreateImageView(user): %d\n", res);
            goto fail;
        }
    }

    free(real_images);

    return VK_SUCCESS;

fail:
    for(i = 0; i < swapchain->n_images; ++i)
        destroy_fs_hack_image(device, swapchain, &swapchain->fs_hack_images[i]);
    free(real_images);
    free(swapchain->cmd_pools);
    free(swapchain->fs_hack_images);
    return res;
}

#if defined(USE_STRUCT_CONVERSION)
static inline void convert_VkSwapchainCreateInfoKHR_win_to_host(const VkSwapchainCreateInfoKHR *in, VkSwapchainCreateInfoKHR_host *out)
#else
static inline void convert_VkSwapchainCreateInfoKHR_win_to_host(const VkSwapchainCreateInfoKHR *in, VkSwapchainCreateInfoKHR *out)
#endif
{
    if (!in) return;

    out->sType = in->sType;
    out->pNext = in->pNext;
    out->flags = in->flags;
    out->surface = wine_surface_from_handle(in->surface)->driver_surface;
    out->minImageCount = in->minImageCount;
    out->imageFormat = in->imageFormat;
    out->imageColorSpace = in->imageColorSpace;
    out->imageExtent = in->imageExtent;
    out->imageArrayLayers = in->imageArrayLayers;
    out->imageUsage = in->imageUsage;
    out->imageSharingMode = in->imageSharingMode;
    out->queueFamilyIndexCount = in->queueFamilyIndexCount;
    out->pQueueFamilyIndices = in->pQueueFamilyIndices;
    out->preTransform = in->preTransform;
    out->compositeAlpha = in->compositeAlpha;
    out->presentMode = in->presentMode;
    out->clipped = in->clipped;
    out->oldSwapchain = in->oldSwapchain;
}

NTSTATUS wine_vkCreateSwapchainKHR(void *args)
{
    struct vkCreateSwapchainKHR_params *params = args;
    VkDevice device = params->device;
    const VkSwapchainCreateInfoKHR *create_info = params->pCreateInfo;
    const VkAllocationCallbacks *allocator = params->pAllocator;
    VkSwapchainKHR *swapchain = params->pSwapchain;
#if defined(USE_STRUCT_CONVERSION)
    VkSwapchainCreateInfoKHR_host native_info;
#else
    VkSwapchainCreateInfoKHR native_info;
#endif
    VkResult result;
    VkExtent2D user_sz;
    struct VkSwapchainKHR_T *object;

    TRACE("%p, %p, %p, %p\n", device, create_info, allocator, swapchain);

    if (!(object = calloc(1, sizeof(*object))))
    {
        ERR("Failed to allocate memory for swapchain\n");
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    convert_VkSwapchainCreateInfoKHR_win_to_host(create_info, &native_info);

    if(native_info.oldSwapchain)
        native_info.oldSwapchain = ((struct VkSwapchainKHR_T *)(UINT_PTR)native_info.oldSwapchain)->swapchain;

    if(vk_funcs->query_fs_hack &&
            vk_funcs->query_fs_hack(native_info.surface, &object->real_extent, &user_sz, &object->blit_dst, &object->fs_hack_filter) &&
            native_info.imageExtent.width == user_sz.width &&
            native_info.imageExtent.height == user_sz.height)
    {
        uint32_t count;
        VkSurfaceCapabilitiesKHR caps = {0};

        device->phys_dev->instance->funcs.p_vkGetPhysicalDeviceQueueFamilyProperties(device->phys_dev->phys_dev, &count, NULL);

        device->queue_props = malloc(sizeof(VkQueueFamilyProperties) * count);

        device->phys_dev->instance->funcs.p_vkGetPhysicalDeviceQueueFamilyProperties(device->phys_dev->phys_dev, &count, device->queue_props);

        result = device->phys_dev->instance->funcs.p_vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device->phys_dev->phys_dev, native_info.surface, &caps);
        if(result != VK_SUCCESS)
        {
            TRACE("vkGetPhysicalDeviceSurfaceCapabilities failed, res=%d\n", result);
            free(object);
            return result;
        }

        if (!(caps.supportedUsageFlags & VK_IMAGE_USAGE_STORAGE_BIT))
            FIXME("Swapchain does not support required VK_IMAGE_USAGE_STORAGE_BIT\n");

        native_info.imageExtent = object->real_extent;
        native_info.imageFormat = VK_FORMAT_B8G8R8A8_UNORM;
        native_info.imageUsage = VK_IMAGE_USAGE_STORAGE_BIT;

        if(create_info->imageFormat != VK_FORMAT_B8G8R8A8_UNORM &&
                create_info->imageFormat != VK_FORMAT_B8G8R8A8_SRGB){
            FIXME("swapchain image format is not BGRA8 UNORM/SRGB. Things may go badly. %d\n", native_info.imageFormat);
        }

        object->fs_hack_enabled = TRUE;
    }

    result = device->funcs.p_vkCreateSwapchainKHR(device->device, &native_info, NULL, &object->swapchain);
    if(result != VK_SUCCESS)
    {
        TRACE("vkCreateSwapchainKHR failed, res=%d\n", result);
        free(object);
        return result;
    }

    WINE_VK_ADD_NON_DISPATCHABLE_MAPPING(device->phys_dev->instance, object, object->swapchain);

    if(object->fs_hack_enabled){
        object->user_extent = create_info->imageExtent;

        result = init_fs_hack_images(device, object, create_info);
        if(result != VK_SUCCESS){
            ERR("creating fs hack images failed: %d\n", result);
            device->funcs.p_vkDestroySwapchainKHR(device->device, object->swapchain, NULL);
            WINE_VK_REMOVE_HANDLE_MAPPING(device->phys_dev->instance, object);
            free(object);
            return result;
        }

        result = init_blit_images(device, object);
        if(result != VK_SUCCESS){
            ERR("creating blit images failed: %d\n", result);
            device->funcs.p_vkDestroySwapchainKHR(device->device, object->swapchain, NULL);
            WINE_VK_REMOVE_HANDLE_MAPPING(device->phys_dev->instance, object);
            free(object);
            return result;
        }
    }

    *swapchain = (uint64_t)(UINT_PTR)object;

    return result;
}

NTSTATUS wine_vkCreateWin32SurfaceKHR(void *args)
{
    struct vkCreateWin32SurfaceKHR_params *params = args;
    VkInstance instance = params->instance;
    const VkWin32SurfaceCreateInfoKHR *createInfo = params->pCreateInfo;
    const VkAllocationCallbacks *allocator = params->pAllocator;
    VkSurfaceKHR *surface = params->pSurface;
    struct wine_surface *object;
    VkResult res;

    TRACE("%p, %p, %p, %p\n", instance, createInfo, allocator, surface);

    if (allocator)
        FIXME("Support for allocation callbacks not implemented yet\n");

    object = calloc(1, sizeof(*object));

    if (!object)
        return VK_ERROR_OUT_OF_HOST_MEMORY;

    res = instance->funcs.p_vkCreateWin32SurfaceKHR(instance->instance, createInfo, NULL, &object->driver_surface);

    if (res != VK_SUCCESS)
    {
        free(object);
        return res;
    }

    object->surface = vk_funcs->p_wine_get_native_surface(object->driver_surface);

    WINE_VK_ADD_NON_DISPATCHABLE_MAPPING(instance, object, object->surface);

    *surface = wine_surface_to_handle(object);

    return VK_SUCCESS;
}

NTSTATUS wine_vkDestroySurfaceKHR(void *args)
{
    struct vkDestroySurfaceKHR_params *params = args;
    VkInstance instance = params->instance;
    VkSurfaceKHR surface = params->surface;
    const VkAllocationCallbacks *allocator = params->pAllocator;
    struct wine_surface *object = wine_surface_from_handle(surface);

    TRACE("%p, 0x%s, %p\n", instance, wine_dbgstr_longlong(surface), allocator);

    if (!object)
        return STATUS_SUCCESS;

    instance->funcs.p_vkDestroySurfaceKHR(instance->instance, object->driver_surface, NULL);

    WINE_VK_REMOVE_HANDLE_MAPPING(instance, object);
    free(object);
    return STATUS_SUCCESS;
}

static inline void adjust_max_image_count(VkPhysicalDevice phys_dev, VkSurfaceCapabilitiesKHR* capabilities)
{
    /* Many Windows games, for example Strange Brigade, No Man's Sky, Path of Exile
     * and World War Z, do not expect that maxImageCount can be set to 0.
     * A value of 0 means that there is no limit on the number of images.
     * Nvidia reports 8 on Windows, AMD 16.
     * https://vulkan.gpuinfo.org/displayreport.php?id=9122#surface
     * https://vulkan.gpuinfo.org/displayreport.php?id=9121#surface
     */
    if ((phys_dev->instance->quirks & WINEVULKAN_QUIRK_ADJUST_MAX_IMAGE_COUNT) && !capabilities->maxImageCount)
    {
        capabilities->maxImageCount = max(capabilities->minImageCount, 16);
    }
}

NTSTATUS wine_vkGetPhysicalDeviceSurfaceCapabilitiesKHR(void *args)
{
    struct vkGetPhysicalDeviceSurfaceCapabilitiesKHR_params *params = args;
    VkPhysicalDevice phys_dev = params->physicalDevice;
    VkSurfaceKHR surface = params->surface;
    VkSurfaceCapabilitiesKHR *capabilities = params->pSurfaceCapabilities;
    VkResult res;
    VkExtent2D user_res;

    TRACE("%p, 0x%s, %p\n", phys_dev, wine_dbgstr_longlong(surface), capabilities);

    res = thunk_vkGetPhysicalDeviceSurfaceCapabilitiesKHR(phys_dev, surface, capabilities);

    if (res == VK_SUCCESS)
        adjust_max_image_count(phys_dev, capabilities);

    if (res == VK_SUCCESS && vk_funcs->query_fs_hack &&
            vk_funcs->query_fs_hack(wine_surface_from_handle(surface)->driver_surface, NULL, &user_res, NULL, NULL)){
        capabilities->currentExtent = user_res;
        capabilities->minImageExtent = user_res;
        capabilities->maxImageExtent = user_res;
    }

    return res;
}

NTSTATUS wine_vkGetPhysicalDeviceSurfaceCapabilities2KHR(void *args)
{
    struct vkGetPhysicalDeviceSurfaceCapabilities2KHR_params *params = args;
    VkPhysicalDevice phys_dev = params->physicalDevice;
    const VkPhysicalDeviceSurfaceInfo2KHR *surface_info = params->pSurfaceInfo;
    VkSurfaceCapabilities2KHR *capabilities = params->pSurfaceCapabilities;
    VkResult res;
    VkExtent2D user_res;

    TRACE("%p, %p, %p\n", phys_dev, surface_info, capabilities);

    res = thunk_vkGetPhysicalDeviceSurfaceCapabilities2KHR(phys_dev, surface_info, capabilities);

    if (res == VK_SUCCESS)
        adjust_max_image_count(phys_dev, &capabilities->surfaceCapabilities);

    if (res == VK_SUCCESS && vk_funcs->query_fs_hack &&
            vk_funcs->query_fs_hack(wine_surface_from_handle(surface_info->surface)->driver_surface, NULL, &user_res, NULL, NULL)){
        capabilities->surfaceCapabilities.currentExtent = user_res;
        capabilities->surfaceCapabilities.minImageExtent = user_res;
        capabilities->surfaceCapabilities.maxImageExtent = user_res;
    }

    return res;
}

NTSTATUS wine_vkCreateDebugUtilsMessengerEXT(void *args)
{
    struct vkCreateDebugUtilsMessengerEXT_params *params = args;
    VkInstance instance = params->instance;
    const VkDebugUtilsMessengerCreateInfoEXT *create_info = params->pCreateInfo;
    const VkAllocationCallbacks *allocator = params->pAllocator;
    VkDebugUtilsMessengerEXT *messenger = params->pMessenger;
    VkDebugUtilsMessengerCreateInfoEXT wine_create_info;
    struct wine_debug_utils_messenger *object;
    VkResult res;

    TRACE("%p, %p, %p, %p\n", instance, create_info, allocator, messenger);

    if (allocator)
        FIXME("Support for allocation callbacks not implemented yet\n");

    if (!(object = calloc(1, sizeof(*object))))
        return VK_ERROR_OUT_OF_HOST_MEMORY;

    object->instance = instance;
    object->user_callback = create_info->pfnUserCallback;
    object->user_data = create_info->pUserData;

    wine_create_info = *create_info;

    wine_create_info.pfnUserCallback = (void *) &debug_utils_callback_conversion;
    wine_create_info.pUserData = object;

    res = instance->funcs.p_vkCreateDebugUtilsMessengerEXT(instance->instance, &wine_create_info, NULL,  &object->debug_messenger);

    if (res != VK_SUCCESS)
    {
        free(object);
        return res;
    }

    WINE_VK_ADD_NON_DISPATCHABLE_MAPPING(instance, object, object->debug_messenger);
    *messenger = wine_debug_utils_messenger_to_handle(object);

    return VK_SUCCESS;
}

NTSTATUS wine_vkDestroyDebugUtilsMessengerEXT(void *args)
{
    struct vkDestroyDebugUtilsMessengerEXT_params *params = args;
    VkInstance instance = params->instance;
    VkDebugUtilsMessengerEXT messenger = params->messenger;
    const VkAllocationCallbacks *allocator = params->pAllocator;
    struct wine_debug_utils_messenger *object;

    TRACE("%p, 0x%s, %p\n", instance, wine_dbgstr_longlong(messenger), allocator);

    object = wine_debug_utils_messenger_from_handle(messenger);

    if (!object)
        return STATUS_SUCCESS;

    instance->funcs.p_vkDestroyDebugUtilsMessengerEXT(instance->instance, object->debug_messenger, NULL);
    WINE_VK_REMOVE_HANDLE_MAPPING(instance, object);

    free(object);
    return STATUS_SUCCESS;
}

NTSTATUS wine_vkCreateDebugReportCallbackEXT(void *args)
{
    struct vkCreateDebugReportCallbackEXT_params *params = args;
    VkInstance instance = params->instance;
    const VkDebugReportCallbackCreateInfoEXT *create_info = params->pCreateInfo;
    const VkAllocationCallbacks *allocator = params->pAllocator;
    VkDebugReportCallbackEXT *callback = params->pCallback;
    VkDebugReportCallbackCreateInfoEXT wine_create_info;
    struct wine_debug_report_callback *object;
    VkResult res;

    TRACE("%p, %p, %p, %p\n", instance, create_info, allocator, callback);

    if (allocator)
        FIXME("Support for allocation callbacks not implemented yet\n");

    if (!(object = calloc(1, sizeof(*object))))
        return VK_ERROR_OUT_OF_HOST_MEMORY;

    object->instance = instance;
    object->user_callback = create_info->pfnCallback;
    object->user_data = create_info->pUserData;

    wine_create_info = *create_info;

    wine_create_info.pfnCallback = (void *) debug_report_callback_conversion;
    wine_create_info.pUserData = object;

    res = instance->funcs.p_vkCreateDebugReportCallbackEXT(instance->instance, &wine_create_info, NULL, &object->debug_callback);

    if (res != VK_SUCCESS)
    {
        free(object);
        return res;
    }

    WINE_VK_ADD_NON_DISPATCHABLE_MAPPING(instance, object, object->debug_callback);
    *callback = wine_debug_report_callback_to_handle(object);

    return VK_SUCCESS;
}

NTSTATUS wine_vkDestroyDebugReportCallbackEXT(void *args)
{
    struct vkDestroyDebugReportCallbackEXT_params *params = args;
    VkInstance instance = params->instance;
    VkDebugReportCallbackEXT callback = params->callback;
    const VkAllocationCallbacks *allocator = params->pAllocator;
    struct wine_debug_report_callback *object;

    TRACE("%p, 0x%s, %p\n", instance, wine_dbgstr_longlong(callback), allocator);

    object = wine_debug_report_callback_from_handle(callback);

    if (!object)
        return STATUS_SUCCESS;

    instance->funcs.p_vkDestroyDebugReportCallbackEXT(instance->instance, object->debug_callback, NULL);

    WINE_VK_REMOVE_HANDLE_MAPPING(instance, object);

    free(object);
    return STATUS_SUCCESS;
}

NTSTATUS wine_vkAcquireNextImage2KHR(void *args)
{
    struct vkAcquireNextImage2KHR_params *params = args;
    VkDevice device = params->device;
    const VkAcquireNextImageInfoKHR *pAcquireInfo = params->pAcquireInfo;
    uint32_t *pImageIndex = params->pImageIndex;
#if defined(USE_STRUCT_CONVERSION)
    VkAcquireNextImageInfoKHR_host image_info_host = {0};
#else
    VkAcquireNextImageInfoKHR image_info_host = {0};
#endif
    struct VkSwapchainKHR_T *object = (struct VkSwapchainKHR_T *)(UINT_PTR)pAcquireInfo->swapchain;
    TRACE("%p, %p, %p\n", device, pAcquireInfo, pImageIndex);

    image_info_host.sType = pAcquireInfo->sType;
    image_info_host.pNext = pAcquireInfo->pNext;
    image_info_host.swapchain = object->swapchain;
    image_info_host.timeout = pAcquireInfo->timeout;
    image_info_host.semaphore = pAcquireInfo->semaphore;
    image_info_host.fence = pAcquireInfo->fence;
    image_info_host.deviceMask = pAcquireInfo->deviceMask;

    return device->funcs.p_vkAcquireNextImage2KHR(device->device, &image_info_host, pImageIndex);
}

NTSTATUS wine_vkDestroySwapchainKHR(void *args)
{
    struct vkDestroySwapchainKHR_params *params = args;
    VkDevice device = params->device;
    VkSwapchainKHR swapchain = params->swapchain;
    const VkAllocationCallbacks *pAllocator = params->pAllocator;
    struct VkSwapchainKHR_T *object = (struct VkSwapchainKHR_T *)(UINT_PTR)swapchain;
    uint32_t i;

    TRACE("%p, 0x%s, %p\n", device, wine_dbgstr_longlong(swapchain), pAllocator);

    if(!object)
        return STATUS_SUCCESS;

    if(object->fs_hack_enabled){
        for(i = 0; i < object->n_images; ++i)
            destroy_fs_hack_image(device, object, &object->fs_hack_images[i]);

        for(i = 0; i < device->queue_count; ++i)
            if(object->cmd_pools[i])
                device->funcs.p_vkDestroyCommandPool(device->device, object->cmd_pools[i], NULL);

        device->funcs.p_vkDestroyPipeline(device->device, object->pipeline, NULL);
        device->funcs.p_vkDestroyPipelineLayout(device->device, object->pipeline_layout, NULL);
        device->funcs.p_vkDestroyDescriptorSetLayout(device->device, object->descriptor_set_layout, NULL);
        device->funcs.p_vkDestroyDescriptorPool(device->device, object->descriptor_pool, NULL);
        device->funcs.p_vkDestroySampler(device->device, object->sampler, NULL);
        device->funcs.p_vkFreeMemory(device->device, object->user_image_memory, NULL);
        free(object->cmd_pools);
        free(object->fs_hack_images);
    }

    device->funcs.p_vkDestroySwapchainKHR(device->device, object->swapchain, NULL);

    WINE_VK_REMOVE_HANDLE_MAPPING(device->phys_dev->instance, object);
    free(object);
    return STATUS_SUCCESS;
}

NTSTATUS wine_vkGetSwapchainImagesKHR(void *args)
{
    struct vkGetSwapchainImagesKHR_params *params = args;
    VkDevice device = params->device;
    VkSwapchainKHR swapchain = params->swapchain;
    uint32_t *pSwapchainImageCount = params->pSwapchainImageCount;
    VkImage *pSwapchainImages = params->pSwapchainImages;
    struct VkSwapchainKHR_T *object = (struct VkSwapchainKHR_T *)(UINT_PTR)swapchain;
    uint32_t i;

    TRACE("%p, 0x%s, %p, %p\n", device, wine_dbgstr_longlong(swapchain), pSwapchainImageCount, pSwapchainImages);

    if(pSwapchainImages && object->fs_hack_enabled){
        if(*pSwapchainImageCount > object->n_images)
            *pSwapchainImageCount = object->n_images;
        for(i = 0; i < *pSwapchainImageCount ; ++i)
            pSwapchainImages[i] = object->fs_hack_images[i].user_image;
        return *pSwapchainImageCount == object->n_images ? VK_SUCCESS : VK_INCOMPLETE;
    }

    return device->funcs.p_vkGetSwapchainImagesKHR(device->device, object->swapchain, pSwapchainImageCount, pSwapchainImages);
}

static VkCommandBuffer create_hack_cmd(VkQueue queue, struct VkSwapchainKHR_T *swapchain, uint32_t queue_idx)
{
#if defined(USE_STRUCT_CONVERSION)
    VkCommandBufferAllocateInfo_host allocInfo = {0};
#else
    VkCommandBufferAllocateInfo allocInfo = {0};
#endif
    VkCommandBuffer cmd;
    VkResult result;

    if(!swapchain->cmd_pools[queue_idx]){
        VkCommandPoolCreateInfo poolInfo = {0};

        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = queue_idx;

        result = queue->device->funcs.p_vkCreateCommandPool(queue->device->device, &poolInfo, NULL, &swapchain->cmd_pools[queue_idx]);
        if(result != VK_SUCCESS){
            ERR("vkCreateCommandPool failed, res=%d\n", result);
            return NULL;
        }
    }

    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = swapchain->cmd_pools[queue_idx];
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    result = queue->device->funcs.p_vkAllocateCommandBuffers(queue->device->device, &allocInfo, &cmd);
    if(result != VK_SUCCESS){
        ERR("vkAllocateCommandBuffers failed, res=%d\n", result);
        return NULL;
    }

    return cmd;
}

static VkResult record_compute_cmd(VkDevice device, struct VkSwapchainKHR_T *swapchain, struct fs_hack_image *hack)
{
    VkResult result;
#if defined(USE_STRUCT_CONVERSION)
    VkImageMemoryBarrier_host barriers[3] = {{0}};
    VkCommandBufferBeginInfo_host beginInfo = {0};
#else
    VkImageMemoryBarrier barriers[3] = {{0}};
    VkCommandBufferBeginInfo beginInfo = {0};
#endif
    float constants[4];

    TRACE("recording compute command\n");

    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

    device->funcs.p_vkBeginCommandBuffer(hack->cmd, &beginInfo);

    /* for the cs we run... */
    /* transition user image from PRESENT_SRC to SHADER_READ */
    barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barriers[0].oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    barriers[0].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[0].image = hack->user_image;
    barriers[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barriers[0].subresourceRange.baseMipLevel = 0;
    barriers[0].subresourceRange.levelCount = 1;
    barriers[0].subresourceRange.baseArrayLayer = 0;
    barriers[0].subresourceRange.layerCount = 1;
    barriers[0].srcAccessMask = 0;
    barriers[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    /* storage image... */
    /* transition swapchain image from whatever to GENERAL */
    barriers[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barriers[1].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barriers[1].newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[1].image = hack->swapchain_image;
    barriers[1].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barriers[1].subresourceRange.baseMipLevel = 0;
    barriers[1].subresourceRange.levelCount = 1;
    barriers[1].subresourceRange.baseArrayLayer = 0;
    barriers[1].subresourceRange.layerCount = 1;
    barriers[1].srcAccessMask = 0;
    barriers[1].dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;

    device->funcs.p_vkCmdPipelineBarrier(
            hack->cmd,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            0,
            0, NULL,
            0, NULL,
            2, barriers
    );

    /* perform blit shader */
    device->funcs.p_vkCmdBindPipeline(hack->cmd,
            VK_PIPELINE_BIND_POINT_COMPUTE, swapchain->pipeline);

    device->funcs.p_vkCmdBindDescriptorSets(hack->cmd,
            VK_PIPELINE_BIND_POINT_COMPUTE, swapchain->pipeline_layout,
            0, 1, &hack->descriptor_set, 0, NULL);

    /* vec2: blit dst offset in real coords */
    constants[0] = swapchain->blit_dst.offset.x;
    constants[1] = swapchain->blit_dst.offset.y;

    /* offset by 0.5f because sampling is relative to pixel center */
    constants[0] -= 0.5f * swapchain->blit_dst.extent.width / swapchain->user_extent.width ;
    constants[1] -= 0.5f * swapchain->blit_dst.extent.height / swapchain->user_extent.height;

    /* vec2: blit dst extents in real coords */
    constants[2] = swapchain->blit_dst.extent.width;
    constants[3] = swapchain->blit_dst.extent.height;
    device->funcs.p_vkCmdPushConstants(hack->cmd,
            swapchain->pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT,
            0, sizeof(constants), constants);

    /* local sizes in shader are 8 */
    device->funcs.p_vkCmdDispatch(hack->cmd, ceil(swapchain->real_extent.width / 8.),
            ceil(swapchain->real_extent.height / 8.), 1);

    /* transition user image from SHADER_READ back to PRESENT_SRC */
    barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barriers[0].oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barriers[0].newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[0].image = hack->user_image;
    barriers[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barriers[0].subresourceRange.baseMipLevel = 0;
    barriers[0].subresourceRange.levelCount = 1;
    barriers[0].subresourceRange.baseArrayLayer = 0;
    barriers[0].subresourceRange.layerCount = 1;
    barriers[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barriers[0].dstAccessMask = 0;

    /* transition swapchain image from GENERAL to PRESENT_SRC */
    barriers[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barriers[1].oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    barriers[1].newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    barriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[1].image = hack->swapchain_image;
    barriers[1].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barriers[1].subresourceRange.baseMipLevel = 0;
    barriers[1].subresourceRange.levelCount = 1;
    barriers[1].subresourceRange.baseArrayLayer = 0;
    barriers[1].subresourceRange.layerCount = 1;
    barriers[1].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barriers[1].dstAccessMask = 0;

    device->funcs.p_vkCmdPipelineBarrier(
            hack->cmd,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            0,
            0, NULL,
            0, NULL,
            2, barriers
    );

    result = device->funcs.p_vkEndCommandBuffer(hack->cmd);
    if(result != VK_SUCCESS){
        ERR("vkEndCommandBuffer: %d\n", result);
        return result;
    }

    return VK_SUCCESS;
}

NTSTATUS wine_vkQueuePresentKHR(void *args)
{
    struct vkQueuePresentKHR_params *params = args;
    VkQueue queue = params->queue;
    const VkPresentInfoKHR *pPresentInfo = params->pPresentInfo;
    VkResult res;
    VkPresentInfoKHR our_presentInfo;
    VkSwapchainKHR *arr;
    VkCommandBuffer *blit_cmds = NULL;
    VkSubmitInfo submitInfo = {0};
    VkSemaphore blit_sema;
    struct VkSwapchainKHR_T *swapchain;
    uint32_t i, n_hacks = 0;
    uint32_t queue_idx;

    TRACE("%p, %p\n", queue, pPresentInfo);

    our_presentInfo = *pPresentInfo;

    for(i = 0; i < our_presentInfo.swapchainCount; ++i){
        swapchain = (struct VkSwapchainKHR_T *)(UINT_PTR)our_presentInfo.pSwapchains[i];

        if(swapchain->fs_hack_enabled){
            struct fs_hack_image *hack = &swapchain->fs_hack_images[our_presentInfo.pImageIndices[i]];

            if(!blit_cmds){
                queue_idx = queue->family_index;
                blit_cmds = malloc(our_presentInfo.swapchainCount * sizeof(VkCommandBuffer));
                blit_sema = hack->blit_finished;
            }

            if(!hack->cmd || hack->cmd_queue_idx != queue_idx){
                if(hack->cmd)
                    queue->device->funcs.p_vkFreeCommandBuffers(queue->device->device,
                            swapchain->cmd_pools[hack->cmd_queue_idx],
                            1, &hack->cmd);

                hack->cmd_queue_idx = queue_idx;
                hack->cmd = create_hack_cmd(queue, swapchain, queue_idx);

                if(!hack->cmd){
                    free(blit_cmds);
                    return VK_ERROR_DEVICE_LOST;
                }

                if(queue->device->queue_props[queue_idx].queueFlags & VK_QUEUE_COMPUTE_BIT) /* TODO */
                    res = record_compute_cmd(queue->device, swapchain, hack);
                else{
                    ERR("Present queue does not support compute!\n");
                    res = VK_ERROR_DEVICE_LOST;
                }

                if(res != VK_SUCCESS){
                    queue->device->funcs.p_vkFreeCommandBuffers(queue->device->device,
                            swapchain->cmd_pools[hack->cmd_queue_idx],
                            1, &hack->cmd);
                    hack->cmd = NULL;
                    free(blit_cmds);
                    return res;
                }
            }

            blit_cmds[n_hacks] = hack->cmd;

            ++n_hacks;
        }
    }

    if(n_hacks > 0){
        VkPipelineStageFlags waitStage, *waitStages, *waitStages_arr = NULL;

        if(pPresentInfo->waitSemaphoreCount > 1){
            waitStages_arr = malloc(sizeof(VkPipelineStageFlags) * pPresentInfo->waitSemaphoreCount);
            for(i = 0; i < pPresentInfo->waitSemaphoreCount; ++i)
                waitStages_arr[i] = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
            waitStages = waitStages_arr;
        }else{
            waitStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
            waitStages = &waitStage;
        }

        /* blit user image to real image */
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.waitSemaphoreCount = pPresentInfo->waitSemaphoreCount;
        submitInfo.pWaitSemaphores = pPresentInfo->pWaitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = n_hacks;
        submitInfo.pCommandBuffers = blit_cmds;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &blit_sema;

        res = queue->device->funcs.p_vkQueueSubmit(queue->queue, 1, &submitInfo, VK_NULL_HANDLE);
        if(res != VK_SUCCESS)
            ERR("vkQueueSubmit: %d\n", res);

        free(waitStages_arr);
        free(blit_cmds);

        our_presentInfo.waitSemaphoreCount = 1;
        our_presentInfo.pWaitSemaphores = &blit_sema;
    }

    arr = malloc(our_presentInfo.swapchainCount * sizeof(VkSwapchainKHR));
    if(!arr){
        ERR("Failed to allocate memory for swapchain array\n");
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    for(i = 0; i < our_presentInfo.swapchainCount; ++i)
        arr[i] = ((struct VkSwapchainKHR_T *)(UINT_PTR)our_presentInfo.pSwapchains[i])->swapchain;

    our_presentInfo.pSwapchains = arr;

    res = queue->device->funcs.p_vkQueuePresentKHR(queue->queue, &our_presentInfo);

    free(arr);

    return res;
}

static void fixup_pipeline_feedback(VkPipelineCreationFeedback *feedback, uint32_t count)
{
#if defined(USE_STRUCT_CONVERSION)
    struct host_pipeline_feedback
    {
        VkPipelineCreationFeedbackFlags flags;
        uint64_t duration;
    } *host_feedback;
    int64_t i;

    host_feedback = (void *) feedback;

    for (i = count - 1; i >= 0; i--)
    {
        memmove(&feedback[i].duration, &host_feedback[i].duration, sizeof(uint64_t));
        feedback[i].flags = host_feedback[i].flags;
    }
#endif
}

static void fixup_pipeline_feedback_info(const void *pipeline_info)
{
    VkPipelineCreationFeedbackCreateInfo *feedback;

    feedback = wine_vk_find_struct(pipeline_info, PIPELINE_CREATION_FEEDBACK_CREATE_INFO);

    if (!feedback)
        return;

    fixup_pipeline_feedback(feedback->pPipelineCreationFeedback, 1);
    fixup_pipeline_feedback(feedback->pPipelineStageCreationFeedbacks,
        feedback->pipelineStageCreationFeedbackCount);
}

NTSTATUS wine_vkCreateComputePipelines(void *args)
{
    struct vkCreateComputePipelines_params *params = args;
    VkResult res;
    uint32_t i;

    TRACE("%p, 0x%s, %u, %p, %p, %p\n", params->device, wine_dbgstr_longlong(params->pipelineCache),
        params->createInfoCount, params->pCreateInfos, params->pAllocator, params->pPipelines);

    res = thunk_vkCreateComputePipelines(params->device, params->pipelineCache,
        params->createInfoCount, params->pCreateInfos, params->pAllocator, params->pPipelines);

    for (i = 0; i < params->createInfoCount; i++)
        fixup_pipeline_feedback_info(&params->pCreateInfos[i]);

    return res;
}

NTSTATUS wine_vkCreateGraphicsPipelines(void *args)
{
    struct vkCreateGraphicsPipelines_params *params = args;
    VkResult res;
    uint32_t i;

    TRACE("%p, 0x%s, %u, %p, %p, %p\n", params->device, wine_dbgstr_longlong(params->pipelineCache),
        params->createInfoCount, params->pCreateInfos, params->pAllocator, params->pPipelines);

    res = thunk_vkCreateGraphicsPipelines(params->device, params->pipelineCache,
        params->createInfoCount, params->pCreateInfos, params->pAllocator, params->pPipelines);

    for (i = 0; i < params->createInfoCount; i++)
        fixup_pipeline_feedback_info(&params->pCreateInfos[i]);

    return res;
}

NTSTATUS wine_vkCreateRayTracingPipelinesKHR(void *args)
{
    struct vkCreateRayTracingPipelinesKHR_params *params = args;
    VkResult res;
    uint32_t i;

    TRACE("%p, 0x%s, 0x%s, %u, %p, %p, %p\n", params->device,
        wine_dbgstr_longlong(params->deferredOperation), wine_dbgstr_longlong(params->pipelineCache),
        params->createInfoCount, params->pCreateInfos, params->pAllocator, params->pPipelines);

    res = thunk_vkCreateRayTracingPipelinesKHR(params->device, params->deferredOperation, params->pipelineCache,
        params->createInfoCount, params->pCreateInfos, params->pAllocator, params->pPipelines);

    for (i = 0; i < params->createInfoCount; i++)
        fixup_pipeline_feedback_info(&params->pCreateInfos[i]);

    return res;
}

NTSTATUS wine_vkCreateRayTracingPipelinesNV(void *args)
{
    struct vkCreateRayTracingPipelinesNV_params *params = args;
    VkResult res;
    uint32_t i;

    TRACE("%p, 0x%s, %u, %p, %p, %p\n", params->device, wine_dbgstr_longlong(params->pipelineCache),
        params->createInfoCount, params->pCreateInfos, params->pAllocator, params->pPipelines);

    res = thunk_vkCreateRayTracingPipelinesNV(params->device, params->pipelineCache,
        params->createInfoCount, params->pCreateInfos, params->pAllocator, params->pPipelines);

    for (i = 0; i < params->createInfoCount; i++)
        fixup_pipeline_feedback_info(&params->pCreateInfos[i]);

    return res;
}

BOOL WINAPI wine_vk_is_available_instance_function(VkInstance instance, const char *name)
{
    return !!vk_funcs->p_vkGetInstanceProcAddr(instance->instance, name);
}

BOOL WINAPI wine_vk_is_available_device_function(VkDevice device, const char *name)
{
    return !!vk_funcs->p_vkGetDeviceProcAddr(device->device, name);
}

VkDevice WINAPI __wine_get_native_VkDevice(VkDevice device)
{
    return device->device;
}

VkInstance WINAPI __wine_get_native_VkInstance(VkInstance instance)
{
    return instance->instance;
}

VkPhysicalDevice WINAPI __wine_get_native_VkPhysicalDevice(VkPhysicalDevice phys_dev)
{
    return phys_dev->phys_dev;
}

VkQueue WINAPI __wine_get_native_VkQueue(VkQueue queue)
{
    return queue->queue;
}

VkPhysicalDevice WINAPI __wine_get_wrapped_VkPhysicalDevice(VkInstance instance, VkPhysicalDevice native_phys_dev)
{
    uint32_t i;
    for(i = 0; i < instance->phys_dev_count; ++i){
        if(instance->phys_devs[i]->phys_dev == native_phys_dev)
            return instance->phys_devs[i];
    }
    WARN("Unknown native physical device: %p\n", native_phys_dev);
    return NULL;
}
