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
#include <unistd.h>
#include <stdbool.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <poll.h>
#include <sys/eventfd.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winnt.h"
#include "winioctl.h"
#include "wine/server.h"
#include "wine/list.h"

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
#if defined(USE_STRUCT_CONVERSION)
    VkPhysicalDeviceProperties_host physdev_properties;
#else
    VkPhysicalDeviceProperties physdev_properties;
#endif
    VkResult res;
    unsigned int i, j;
    bool has_memory_priority = false;

    if (!(object = calloc(1, sizeof(*object))))
        return NULL;

    object->base.loader_magic = VULKAN_ICD_MAGIC_VALUE;
    object->instance = instance;
    object->phys_dev = phys_dev;

    instance->funcs.p_vkGetPhysicalDeviceProperties(phys_dev, &physdev_properties);
    object->api_version = physdev_properties.apiVersion;

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
        if (!strcmp(host_properties[i].extensionName, "VK_KHR_external_memory_fd"))
        {
            TRACE("Substituting VK_KHR_external_memory_fd for VK_KHR_external_memory_win32\n");

            snprintf(host_properties[i].extensionName, sizeof(host_properties[i].extensionName),
                    VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME);
            host_properties[i].specVersion = VK_KHR_EXTERNAL_MEMORY_WIN32_SPEC_VERSION;
        }
        if (!strcmp(host_properties[i].extensionName, "VK_KHR_external_semaphore_fd"))
        {
            TRACE("Substituting VK_KHR_external_semaphore_fd for VK_KHR_external_semaphore_win32\n");

            snprintf(host_properties[i].extensionName, sizeof(host_properties[i].extensionName),
                    VK_KHR_EXTERNAL_SEMAPHORE_WIN32_EXTENSION_NAME);
            host_properties[i].specVersion = VK_KHR_EXTERNAL_SEMAPHORE_WIN32_SPEC_VERSION;
        }
        if (!strcmp(host_properties[i].extensionName, "VK_EXT_memory_priority"))
        {
            has_memory_priority = true;
        }

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

    if (instance->quirks & WINEVULKAN_QUIRK_EXPOSE_KEYED_MUTEX)
        num_properties++;

    if (!has_memory_priority && (instance->quirks & WINEVULKAN_QUIRK_EXPOSE_MEM_PRIORITY))
        num_properties++;

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

    if (instance->quirks & WINEVULKAN_QUIRK_EXPOSE_KEYED_MUTEX)
    {
        TRACE("Faking VK_KHR_win32_keyed_mutex extension.\n");
        snprintf(object->extensions[j].extensionName, sizeof(object->extensions[j].extensionName),
                VK_KHR_WIN32_KEYED_MUTEX_EXTENSION_NAME);
        object->extensions[j].specVersion = VK_KHR_WIN32_KEYED_MUTEX_SPEC_VERSION;
        j++;
    }

    if (!has_memory_priority && (instance->quirks & WINEVULKAN_QUIRK_EXPOSE_MEM_PRIORITY))
    {
        TRACE("Faking VK_EXT_memory_priority extension.\n");
        snprintf(object->extensions[j].extensionName, sizeof(object->extensions[j].extensionName),
                VK_EXT_MEMORY_PRIORITY_EXTENSION_NAME);
        object->extensions[j].specVersion = VK_EXT_MEMORY_PRIORITY_SPEC_VERSION;
        j++;

        object->fake_memory_priority = true;
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

        pthread_mutex_init(&queue->submissions_mutex, NULL);
        pthread_cond_init(&queue->submissions_cond, NULL);
        list_init(&queue->submissions);

        pthread_mutex_init(&queue->signaller_mutex, NULL);
        pthread_cond_init(&queue->signaller_cond, NULL);
        list_init(&queue->signal_ops);

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

static VkResult wine_vk_device_convert_create_info(VkPhysicalDevice phys_dev, const VkDeviceCreateInfo *src,
        VkDeviceCreateInfo *dst, BOOL *must_free_extensions)
{
    unsigned int i, append_xr = 0, replace_win32 = 0, timeline_enabled = 0, drop_extension = 0, wine_extension_count;
    VkResult res;

    static const char *wine_xr_extension_name = "VK_WINE_openxr_device_extensions";

    *dst = *src;

    if ((res = convert_VkDeviceCreateInfo_struct_chain(src->pNext, dst)) < 0)
    {
        WARN("Failed to convert VkDeviceCreateInfo pNext chain, res=%d.\n", res);
        return res;
    }

    if (phys_dev->fake_memory_priority)
    {
        VkBaseOutStructure *header;

        for (header = (void *) dst; header; header = header->pNext)
        {
            if (header->pNext && header->pNext->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PRIORITY_FEATURES_EXT)
            {
                VkBaseOutStructure *memory_priority = header->pNext;

                header->pNext = memory_priority->pNext;
                free(memory_priority);
                break;
            }
        }
    }

    /* Should be filtered out by loader as ICDs don't support layers. */
    dst->enabledLayerCount = 0;
    dst->ppEnabledLayerNames = NULL;

    for (i = 0; i < dst->enabledExtensionCount; i++)
    {
        const char *extension_name = dst->ppEnabledExtensionNames[i];
        if (!strcmp(extension_name, wine_xr_extension_name))
            append_xr = 1;
        else if (!strcmp(extension_name, "VK_KHR_external_memory_win32") || !strcmp(extension_name, "VK_KHR_external_semaphore_win32"))
            replace_win32 = 1;
        else if (!strcmp(extension_name, "VK_KHR_win32_keyed_mutex") || (phys_dev->fake_memory_priority && !strcmp(extension_name, "VK_EXT_memory_priority")))
            drop_extension = 1;
        else if (!strcmp(extension_name, "VK_KHR_timeline_semaphore"))
            timeline_enabled = 1;
    }

    if (append_xr || replace_win32 || drop_extension)
    {
        unsigned int xr_extensions_len = 0, o = 0, j;
        char **xr_extensions_list = NULL;
        char **new_extensions_list;

        if (append_xr)
            xr_extensions_list = parse_xr_extensions(&xr_extensions_len);

        new_extensions_list = malloc(sizeof(char *) * (dst->enabledExtensionCount + xr_extensions_len + replace_win32));

        if(append_xr && !xr_extensions_list)
            WARN("Requested to use XR extensions, but none are set!\n");

        for (i = 0; i < dst->enabledExtensionCount; i++)
        {
            if (append_xr && !strcmp(dst->ppEnabledExtensionNames[i], wine_xr_extension_name))
                continue;

            if (drop_extension && (!strcmp(src->ppEnabledExtensionNames[i], "VK_KHR_win32_keyed_mutex") ||
                (phys_dev->fake_memory_priority && !strcmp(src->ppEnabledExtensionNames[i], "VK_EXT_memory_priority"))))
            {
                TRACE("Ignoring active extension %s.\n", src->ppEnabledExtensionNames[i]);
                continue;
            }

            if (replace_win32 && !strcmp(src->ppEnabledExtensionNames[i], "VK_KHR_external_memory_win32"))
                new_extensions_list[o] = strdup("VK_KHR_external_memory_fd");
            else if (replace_win32 && !strcmp(src->ppEnabledExtensionNames[i], "VK_KHR_external_semaphore_win32"))
            {
                new_extensions_list[o] = strdup("VK_KHR_external_semaphore_fd");

                /* D3D12-Fence interoperable semaphores are implemented using timeline semaphores */
                if (!timeline_enabled && (phys_dev->api_version < VK_API_VERSION_1_2 || phys_dev->instance->api_version < VK_API_VERSION_1_2))
                {
                    for (j = 0; j < phys_dev->extension_count; j++)
                    {
                        if (!strcmp(phys_dev->extensions[j].extensionName, "VK_KHR_timeline_semaphore"))
                        {
                            new_extensions_list[++o] = strdup("VK_KHR_timeline_semaphore");
                            break;
                        }
                    }
                }

            }
            else
                new_extensions_list[o] = strdup(dst->ppEnabledExtensionNames[i]);
            ++o;
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

static bool is_virtual_queue(struct VkQueue_T *queue)
{
    return __atomic_load_n(&queue->virtual_queue, __ATOMIC_ACQUIRE);
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

            if (is_virtual_queue(queue))
            {
                pthread_mutex_lock(&queue->submissions_mutex);
                pthread_mutex_lock(&queue->signaller_mutex);
                queue->stop = 1;
                pthread_mutex_unlock(&queue->submissions_mutex);
                pthread_mutex_unlock(&queue->signaller_mutex);

                pthread_cond_signal(&queue->submissions_cond);
                pthread_cond_signal(&queue->signaller_cond);

                pthread_join(queue->virtual_queue_thread, NULL);
                pthread_join(queue->signal_thread, NULL);
            }

            pthread_mutex_destroy(&queue->submissions_mutex);
            pthread_mutex_destroy(&queue->signaller_mutex);

            pthread_cond_destroy(&queue->submissions_cond);
            pthread_cond_destroy(&queue->signaller_cond);

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

    res = wine_vk_device_convert_create_info(phys_dev, create_info, &create_info_host, &create_info_free_extensions);
    if (res != VK_SUCCESS)
        goto fail;

    if (native_vkCreateDevice)
        res = native_vkCreateDevice(phys_dev->phys_dev,
                &create_info_host, NULL /* allocator */, &object->device,
                vk_funcs->p_vkGetInstanceProcAddr, native_vkCreateDevice_context);
    else
        res = phys_dev->instance->funcs.p_vkCreateDevice(phys_dev->phys_dev,
                &create_info_host, NULL /* allocator */, &object->device);

    wine_vk_device_free_create_info(&create_info_host);
    if (create_info_free_extensions)
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

    if ((app_info = create_info->pApplicationInfo))
    {
        TRACE("Application name %s, application version %#x.\n",
                debugstr_a(app_info->pApplicationName), app_info->applicationVersion);
        TRACE("Engine name %s, engine version %#x.\n", debugstr_a(app_info->pEngineName),
                app_info->engineVersion);
        TRACE("API version %#x.\n", app_info->apiVersion);

        object->api_version = app_info->apiVersion;

        if (app_info->pEngineName && !strcmp(app_info->pEngineName, "idTech"))
            object->quirks |= WINEVULKAN_QUIRK_GET_DEVICE_PROC_ADDR;

        if (app_info->pEngineName && !strcmp(app_info->pEngineName, "nvpro-sample"))
            object->quirks |= (WINEVULKAN_QUIRK_EXPOSE_MEM_PRIORITY | WINEVULKAN_QUIRK_EXPOSE_KEYED_MUTEX);
    }

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

static inline void wine_vk_normalize_handle_types_win(VkExternalMemoryHandleTypeFlags *types)
{
    *types &=
        VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT |
        VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT |
        VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT |
        VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_KMT_BIT |
        VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_HEAP_BIT |
        VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT |
        VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT |
        VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_MAPPED_FOREIGN_MEMORY_BIT_EXT;
}

static inline void wine_vk_normalize_handle_types_host(VkExternalMemoryHandleTypeFlags *types)
{
    *types &=
        VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT |
        VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT |
/*      predicated on VK_KHR_external_memory_dma_buf
        VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT | */
        VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_MAPPED_FOREIGN_MEMORY_BIT_EXT;
}

static const VkExternalMemoryHandleTypeFlagBits wine_vk_handle_over_fd_types =
                VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT |
                VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT |
                VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT |
                VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_KMT_BIT;

static void wine_vk_get_physical_device_external_buffer_properties(VkPhysicalDevice phys_dev,
        void (*p_vkGetPhysicalDeviceExternalBufferProperties)(VkPhysicalDevice, const VkPhysicalDeviceExternalBufferInfo *, VkExternalBufferProperties *),
        const VkPhysicalDeviceExternalBufferInfo *buffer_info, VkExternalBufferProperties *properties)
{
    VkPhysicalDeviceExternalBufferInfo buffer_info_dup = *buffer_info;

    wine_vk_normalize_handle_types_win(&buffer_info_dup.handleType);
    if (buffer_info_dup.handleType & wine_vk_handle_over_fd_types)
        buffer_info_dup.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
    wine_vk_normalize_handle_types_host(&buffer_info_dup.handleType);

    if (buffer_info->handleType && !buffer_info_dup.handleType)
    {
        memset(&properties->externalMemoryProperties, 0, sizeof(properties->externalMemoryProperties));
        return;
    }

    p_vkGetPhysicalDeviceExternalBufferProperties(phys_dev->phys_dev, &buffer_info_dup, properties);

    if (properties->externalMemoryProperties.exportFromImportedHandleTypes & VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT)
        properties->externalMemoryProperties.exportFromImportedHandleTypes |= wine_vk_handle_over_fd_types;
    wine_vk_normalize_handle_types_win(&properties->externalMemoryProperties.exportFromImportedHandleTypes);

    if (properties->externalMemoryProperties.compatibleHandleTypes & VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT)
        properties->externalMemoryProperties.compatibleHandleTypes |= wine_vk_handle_over_fd_types;
    wine_vk_normalize_handle_types_win(&properties->externalMemoryProperties.compatibleHandleTypes);
}

NTSTATUS wine_vkGetPhysicalDeviceExternalBufferProperties(void *args)
{
    struct vkGetPhysicalDeviceExternalBufferProperties_params *params = args;
    VkPhysicalDevice phys_dev = params->physicalDevice;
    const VkPhysicalDeviceExternalBufferInfo *buffer_info = params->pExternalBufferInfo;
    VkExternalBufferProperties *properties = params->pExternalBufferProperties;

    TRACE("%p, %p, %p\n", phys_dev, buffer_info, properties);

    wine_vk_get_physical_device_external_buffer_properties(phys_dev, phys_dev->instance->funcs.p_vkGetPhysicalDeviceExternalBufferProperties, buffer_info, properties);
    return STATUS_SUCCESS;
}

NTSTATUS wine_vkGetPhysicalDeviceExternalBufferPropertiesKHR(void *args)
{
    struct vkGetPhysicalDeviceExternalBufferPropertiesKHR_params *params = args;
    VkPhysicalDevice phys_dev = params->physicalDevice;
    const VkPhysicalDeviceExternalBufferInfo *buffer_info = params->pExternalBufferInfo;
    VkExternalBufferProperties *properties = params->pExternalBufferProperties;

    TRACE("%p, %p, %p\n", phys_dev, buffer_info, properties);

    wine_vk_get_physical_device_external_buffer_properties(phys_dev, phys_dev->instance->funcs.p_vkGetPhysicalDeviceExternalBufferPropertiesKHR, buffer_info, properties);
    return STATUS_SUCCESS;
}

static VkResult wine_vk_get_physical_device_image_format_properties_2(VkPhysicalDevice phys_dev,
        VkResult (*p_vkGetPhysicalDeviceImageFormatProperties2)(VkPhysicalDevice, const VkPhysicalDeviceImageFormatInfo2 *, VkImageFormatProperties2 *),
        const VkPhysicalDeviceImageFormatInfo2 *format_info, VkImageFormatProperties2 *properties)
{
    VkPhysicalDeviceExternalImageFormatInfo *external_image_info_dup = NULL;
    const VkPhysicalDeviceExternalImageFormatInfo *external_image_info;
    VkPhysicalDeviceImageFormatInfo2 format_info_host = *format_info;
    VkExternalImageFormatProperties *external_image_properties;
    VkResult res;

    if ((external_image_info = wine_vk_find_struct(format_info, PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO)) && external_image_info->handleType)
    {
        if ((res = convert_VkPhysicalDeviceImageFormatInfo2_struct_chain(format_info->pNext, &format_info_host)) < 0)
        {
            WARN("Failed to convert VkPhysicalDeviceImageFormatInfo2 pNext chain, res=%d.\n", res);
            return res;
        }
        external_image_info_dup = wine_vk_find_struct(&format_info_host, PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO);

        wine_vk_normalize_handle_types_win(&external_image_info_dup->handleType);

        if (external_image_info_dup->handleType & wine_vk_handle_over_fd_types)
            external_image_info_dup->handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;

        wine_vk_normalize_handle_types_host(&external_image_info_dup->handleType);
        if (!external_image_info_dup->handleType)
        {
            WARN("Unsupported handle type %#x.\n", external_image_info->handleType);
            return VK_ERROR_FORMAT_NOT_SUPPORTED;
        }
    }

    res = p_vkGetPhysicalDeviceImageFormatProperties2(phys_dev, &format_info_host, properties);

    if (external_image_info_dup)
        free_VkPhysicalDeviceImageFormatInfo2_struct_chain(&format_info_host);

    if ((external_image_properties = wine_vk_find_struct(properties, EXTERNAL_IMAGE_FORMAT_PROPERTIES)))
    {
        VkExternalMemoryProperties *p = &external_image_properties->externalMemoryProperties;
        if (p->exportFromImportedHandleTypes & VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT)
            p->exportFromImportedHandleTypes |= wine_vk_handle_over_fd_types;
        wine_vk_normalize_handle_types_win(&p->exportFromImportedHandleTypes);

        if (p->compatibleHandleTypes & VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT)
            p->compatibleHandleTypes |= wine_vk_handle_over_fd_types;
        wine_vk_normalize_handle_types_win(&p->compatibleHandleTypes);
    }

    return res;
}

NTSTATUS wine_vkGetPhysicalDeviceImageFormatProperties2(void *args)
{
    struct vkGetPhysicalDeviceImageFormatProperties2_params *params = args;
    VkPhysicalDevice phys_dev = params->physicalDevice;
    const VkPhysicalDeviceImageFormatInfo2 *format_info = params->pImageFormatInfo;
    VkImageFormatProperties2 *properties = params->pImageFormatProperties;

    TRACE("%p, %p, %p\n", phys_dev, format_info, properties);

    return wine_vk_get_physical_device_image_format_properties_2(phys_dev, thunk_vkGetPhysicalDeviceImageFormatProperties2, format_info, properties);
}

NTSTATUS wine_vkGetPhysicalDeviceImageFormatProperties2KHR(void *args)
{
    struct vkGetPhysicalDeviceImageFormatProperties2KHR_params *params = args;
    VkPhysicalDevice phys_dev = params->physicalDevice;
    const VkPhysicalDeviceImageFormatInfo2 *format_info = params->pImageFormatInfo;
    VkImageFormatProperties2 *properties = params->pImageFormatProperties;

    TRACE("%p, %p, %p\n", phys_dev, format_info, properties);

    return wine_vk_get_physical_device_image_format_properties_2(phys_dev, thunk_vkGetPhysicalDeviceImageFormatProperties2KHR, format_info, properties);
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

static inline void wine_vk_normalize_semaphore_handle_types_win(VkExternalSemaphoreHandleTypeFlags *types)
{
    *types &=
        VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT |
        VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT |
        VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D12_FENCE_BIT;
}

static inline void wine_vk_normalize_semaphore_handle_types_host(VkExternalSemaphoreHandleTypeFlags *types)
{
    *types &=
        VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT |
        VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT;
}

static void wine_vk_get_physical_device_external_semaphore_properties(VkPhysicalDevice phys_dev,
    void (*p_vkGetPhysicalDeviceExternalSemaphoreProperties)(VkPhysicalDevice, const VkPhysicalDeviceExternalSemaphoreInfo *, VkExternalSemaphoreProperties *),
    const VkPhysicalDeviceExternalSemaphoreInfo *semaphore_info, VkExternalSemaphoreProperties *properties)
{
    VkPhysicalDeviceExternalSemaphoreInfo semaphore_info_dup = *semaphore_info, semaphore_info_host;
    VkSemaphoreTypeCreateInfo semaphore_type_info, *p_semaphore_type_info;
    unsigned int i;
    VkResult res;

    if ((res = convert_VkPhysicalDeviceExternalSemaphoreInfo_struct_chain(semaphore_info->pNext, &semaphore_info_dup)) < 0)
    {
        WARN("Failed to convert VkPhysicalDeviceExternalSemaphoreInfo pNext chain, res=%d.\n", res);

        properties->exportFromImportedHandleTypes = 0;
        properties->compatibleHandleTypes = 0;
        properties->externalSemaphoreFeatures = 0;
        return;
    }

    semaphore_info_host = semaphore_info_dup;

    switch(semaphore_info->handleType)
    {
        case VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT:
            semaphore_info_host.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;
            break;
        case VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D12_FENCE_BIT:
        {
            if (phys_dev->api_version < VK_API_VERSION_1_2 ||
                phys_dev->instance->api_version < VK_API_VERSION_1_2)
            {
                for (i = 0; i < phys_dev->extension_count; i++)
                {
                    if (!strcmp(phys_dev->extensions[i].extensionName, "VK_KHR_timeline_semaphore"))
                        break;
                }
                if (i == phys_dev->extension_count)
                {
                    free_VkPhysicalDeviceExternalSemaphoreInfo_struct_chain(&semaphore_info_dup);
                    properties->exportFromImportedHandleTypes = 0;
                    properties->compatibleHandleTypes = 0;
                    properties->externalSemaphoreFeatures = 0;
                    return;
                }
            }

            if ((p_semaphore_type_info = wine_vk_find_struct(&semaphore_info_host, SEMAPHORE_TYPE_CREATE_INFO)))
            {
                p_semaphore_type_info->semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
                p_semaphore_type_info->initialValue = 0;
            }
            else
            {
                semaphore_type_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
                semaphore_type_info.pNext = semaphore_info_host.pNext;
                semaphore_type_info.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
                semaphore_type_info.initialValue = 0;

                semaphore_info_host.pNext = &semaphore_type_info;
            }

            semaphore_info_host.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;
            break;
        }
        default:
            semaphore_info_host.handleType = 0;
    }

    if (semaphore_info->handleType && !semaphore_info_host.handleType)
    {
        free_VkPhysicalDeviceExternalSemaphoreInfo_struct_chain(&semaphore_info_dup);

        properties->exportFromImportedHandleTypes = 0;
        properties->compatibleHandleTypes = 0;
        properties->externalSemaphoreFeatures = 0;
        return;
    }

    p_vkGetPhysicalDeviceExternalSemaphoreProperties(phys_dev->phys_dev, &semaphore_info_host, properties);

    free_VkPhysicalDeviceExternalSemaphoreInfo_struct_chain(&semaphore_info_dup);

    if (properties->exportFromImportedHandleTypes & VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT)
        properties->exportFromImportedHandleTypes = semaphore_info->handleType;
    wine_vk_normalize_semaphore_handle_types_win(&properties->exportFromImportedHandleTypes);

    if (properties->compatibleHandleTypes & VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT)
        properties->compatibleHandleTypes = semaphore_info->handleType;
    wine_vk_normalize_semaphore_handle_types_win(&properties->compatibleHandleTypes);
}

NTSTATUS wine_vkGetPhysicalDeviceExternalSemaphoreProperties(void *args)
{
    struct vkGetPhysicalDeviceExternalSemaphoreProperties_params *params = args;
    VkPhysicalDevice phys_dev = params->physicalDevice;
    const VkPhysicalDeviceExternalSemaphoreInfo *semaphore_info = params->pExternalSemaphoreInfo;
    VkExternalSemaphoreProperties *properties = params->pExternalSemaphoreProperties;

    TRACE("%p, %p, %p\n", phys_dev, semaphore_info, properties);
    wine_vk_get_physical_device_external_semaphore_properties(phys_dev, phys_dev->instance->funcs.p_vkGetPhysicalDeviceExternalSemaphoreProperties, semaphore_info, properties);

    return STATUS_SUCCESS;
}

NTSTATUS wine_vkGetPhysicalDeviceExternalSemaphorePropertiesKHR(void *args)
{
    struct vkGetPhysicalDeviceExternalSemaphorePropertiesKHR_params *params = args;
    VkPhysicalDevice phys_dev = params->physicalDevice;
    const VkPhysicalDeviceExternalSemaphoreInfo *semaphore_info = params->pExternalSemaphoreInfo;
    VkExternalSemaphoreProperties *properties = params->pExternalSemaphoreProperties;

    TRACE("%p, %p, %p\n", phys_dev, semaphore_info, properties);
    wine_vk_get_physical_device_external_semaphore_properties(phys_dev, phys_dev->instance->funcs.p_vkGetPhysicalDeviceExternalSemaphorePropertiesKHR, semaphore_info, properties);

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
#version 450

layout(binding = 0) uniform sampler2D texSampler;
layout(binding = 1, rgba8) uniform writeonly image2D outImage;
layout(push_constant) uniform pushConstants {
    //both in real image coords
    vec2 offset;
    vec2 extents;
} constants;

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

void main()
{
    vec2 texcoord = (vec2(gl_GlobalInvocationID.xy) - constants.offset) / constants.extents;
    vec4 c = texture(texSampler, texcoord);
    imageStore(outImage, ivec2(gl_GlobalInvocationID.xy), c.bgra);
}
*/
const uint32_t blit_comp_spv[] = {
    0x07230203,0x00010000,0x00080006,0x00000037,0x00000000,0x00020011,0x00000001,0x0006000b,
    0x00000001,0x4c534c47,0x6474732e,0x3035342e,0x00000000,0x0003000e,0x00000000,0x00000001,
    0x0006000f,0x00000005,0x00000004,0x6e69616d,0x00000000,0x0000000d,0x00060010,0x00000004,
    0x00000011,0x00000008,0x00000008,0x00000001,0x00030003,0x00000002,0x000001c2,0x00040005,
    0x00000004,0x6e69616d,0x00000000,0x00050005,0x00000009,0x63786574,0x64726f6f,0x00000000,
    0x00080005,0x0000000d,0x475f6c67,0x61626f6c,0x766e496c,0x7461636f,0x496e6f69,0x00000044,
    0x00060005,0x00000012,0x68737570,0x736e6f43,0x746e6174,0x00000073,0x00050006,0x00000012,
    0x00000000,0x7366666f,0x00007465,0x00050006,0x00000012,0x00000001,0x65747865,0x0073746e,
    0x00050005,0x00000014,0x736e6f63,0x746e6174,0x00000073,0x00030005,0x00000021,0x00000063,
    0x00050005,0x00000025,0x53786574,0x6c706d61,0x00007265,0x00050005,0x0000002c,0x4974756f,
    0x6567616d,0x00000000,0x00040047,0x0000000d,0x0000000b,0x0000001c,0x00050048,0x00000012,
    0x00000000,0x00000023,0x00000000,0x00050048,0x00000012,0x00000001,0x00000023,0x00000008,
    0x00030047,0x00000012,0x00000002,0x00040047,0x00000025,0x00000022,0x00000000,0x00040047,
    0x00000025,0x00000021,0x00000000,0x00040047,0x0000002c,0x00000022,0x00000000,0x00040047,
    0x0000002c,0x00000021,0x00000001,0x00030047,0x0000002c,0x00000019,0x00040047,0x00000036,
    0x0000000b,0x00000019,0x00020013,0x00000002,0x00030021,0x00000003,0x00000002,0x00030016,
    0x00000006,0x00000020,0x00040017,0x00000007,0x00000006,0x00000002,0x00040020,0x00000008,
    0x00000007,0x00000007,0x00040015,0x0000000a,0x00000020,0x00000000,0x00040017,0x0000000b,
    0x0000000a,0x00000003,0x00040020,0x0000000c,0x00000001,0x0000000b,0x0004003b,0x0000000c,
    0x0000000d,0x00000001,0x00040017,0x0000000e,0x0000000a,0x00000002,0x0004001e,0x00000012,
    0x00000007,0x00000007,0x00040020,0x00000013,0x00000009,0x00000012,0x0004003b,0x00000013,
    0x00000014,0x00000009,0x00040015,0x00000015,0x00000020,0x00000001,0x0004002b,0x00000015,
    0x00000016,0x00000000,0x00040020,0x00000017,0x00000009,0x00000007,0x0004002b,0x00000015,
    0x0000001b,0x00000001,0x00040017,0x0000001f,0x00000006,0x00000004,0x00040020,0x00000020,
    0x00000007,0x0000001f,0x00090019,0x00000022,0x00000006,0x00000001,0x00000000,0x00000000,
    0x00000000,0x00000001,0x00000000,0x0003001b,0x00000023,0x00000022,0x00040020,0x00000024,
    0x00000000,0x00000023,0x0004003b,0x00000024,0x00000025,0x00000000,0x0004002b,0x00000006,
    0x00000028,0x00000000,0x00090019,0x0000002a,0x00000006,0x00000001,0x00000000,0x00000000,
    0x00000000,0x00000002,0x00000004,0x00040020,0x0000002b,0x00000000,0x0000002a,0x0004003b,
    0x0000002b,0x0000002c,0x00000000,0x00040017,0x00000030,0x00000015,0x00000002,0x0004002b,
    0x0000000a,0x00000034,0x00000008,0x0004002b,0x0000000a,0x00000035,0x00000001,0x0006002c,
    0x0000000b,0x00000036,0x00000034,0x00000034,0x00000035,0x00050036,0x00000002,0x00000004,
    0x00000000,0x00000003,0x000200f8,0x00000005,0x0004003b,0x00000008,0x00000009,0x00000007,
    0x0004003b,0x00000020,0x00000021,0x00000007,0x0004003d,0x0000000b,0x0000000f,0x0000000d,
    0x0007004f,0x0000000e,0x00000010,0x0000000f,0x0000000f,0x00000000,0x00000001,0x00040070,
    0x00000007,0x00000011,0x00000010,0x00050041,0x00000017,0x00000018,0x00000014,0x00000016,
    0x0004003d,0x00000007,0x00000019,0x00000018,0x00050083,0x00000007,0x0000001a,0x00000011,
    0x00000019,0x00050041,0x00000017,0x0000001c,0x00000014,0x0000001b,0x0004003d,0x00000007,
    0x0000001d,0x0000001c,0x00050088,0x00000007,0x0000001e,0x0000001a,0x0000001d,0x0003003e,
    0x00000009,0x0000001e,0x0004003d,0x00000023,0x00000026,0x00000025,0x0004003d,0x00000007,
    0x00000027,0x00000009,0x00070058,0x0000001f,0x00000029,0x00000026,0x00000027,0x00000002,
    0x00000028,0x0003003e,0x00000021,0x00000029,0x0004003d,0x0000002a,0x0000002d,0x0000002c,
    0x0004003d,0x0000000b,0x0000002e,0x0000000d,0x0007004f,0x0000000e,0x0000002f,0x0000002e,
    0x0000002e,0x00000000,0x00000001,0x0004007c,0x00000030,0x00000031,0x0000002f,0x0004003d,
    0x0000001f,0x00000032,0x00000021,0x0009004f,0x0000001f,0x00000033,0x00000032,0x00000032,
    0x00000002,0x00000001,0x00000000,0x00000003,0x00040063,0x0000002d,0x00000031,0x00000033,
    0x000100fd,0x00010038
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
    VkDeviceSize blitMemTotal = 0, offs;
    VkImageCreateInfo imageInfo = {0};
#if defined(USE_STRUCT_CONVERSION)
    VkMemoryRequirements_host blitMemReq;
    VkMemoryAllocateInfo_host allocInfo = {0};
    VkPhysicalDeviceMemoryProperties_host memProperties;
    VkImageViewCreateInfo_host viewInfo = {0};
#else
    VkMemoryRequirements blitMemReq;
    VkMemoryAllocateInfo allocInfo = {0};
    VkPhysicalDeviceMemoryProperties memProperties;
    VkImageViewCreateInfo viewInfo = {0};
#endif
    uint32_t blit_memory_type = -1, i;

    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = swapchain->fs_hack_filter;
    samplerInfo.minFilter = swapchain->fs_hack_filter;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
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

    if(!(swapchain->surface_usage & VK_IMAGE_USAGE_STORAGE_BIT)){
        TRACE("using intermediate blit images\n");
        /* create intermediate blit images */
        for(i = 0; i < swapchain->n_images; ++i){
            struct fs_hack_image *hack = &swapchain->fs_hack_images[i];

            imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            imageInfo.imageType = VK_IMAGE_TYPE_2D;
            imageInfo.extent.width = swapchain->real_extent.width;
            imageInfo.extent.height = swapchain->real_extent.height;
            imageInfo.extent.depth = 1;
            imageInfo.mipLevels = 1;
            imageInfo.arrayLayers = 1;
            imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
            imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
            imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
            imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
            res = device->funcs.p_vkCreateImage(device->device, &imageInfo, NULL, &hack->blit_image);
            if(res != VK_SUCCESS){
                ERR("vkCreateImage failed: %d\n", res);
                goto fail;
            }

            device->funcs.p_vkGetImageMemoryRequirements(device->device, hack->blit_image, &blitMemReq);

            offs = blitMemTotal % blitMemReq.alignment;
            if(offs)
                blitMemTotal += blitMemReq.alignment - offs;

            blitMemTotal += blitMemReq.size;
        }

        /* allocate backing memory */
        device->phys_dev->instance->funcs.p_vkGetPhysicalDeviceMemoryProperties(device->phys_dev->phys_dev, &memProperties);

        for(i = 0; i < memProperties.memoryTypeCount; i++){
            if((memProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) == VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT){
                if(blitMemReq.memoryTypeBits & (1 << i)){
                    blit_memory_type = i;
                    break;
                }
            }
        }

        if(blit_memory_type == -1){
            ERR("unable to find suitable memory type\n");
            res = VK_ERROR_OUT_OF_HOST_MEMORY;
            goto fail;
        }

        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = blitMemTotal;
        allocInfo.memoryTypeIndex = blit_memory_type;

        res = device->funcs.p_vkAllocateMemory(device->device, &allocInfo, NULL, &swapchain->blit_image_memory);
        if(res != VK_SUCCESS){
            ERR("vkAllocateMemory: %d\n", res);
            goto fail;
        }

        /* bind backing memory and create imageviews */
        blitMemTotal = 0;
        for(i = 0; i < swapchain->n_images; ++i){
            struct fs_hack_image *hack = &swapchain->fs_hack_images[i];

            device->funcs.p_vkGetImageMemoryRequirements(device->device, hack->blit_image, &blitMemReq);

            offs = blitMemTotal % blitMemReq.alignment;
            if(offs)
                blitMemTotal += blitMemReq.alignment - offs;

            res = device->funcs.p_vkBindImageMemory(device->device, hack->blit_image, swapchain->blit_image_memory, blitMemTotal);
            if(res != VK_SUCCESS){
                ERR("vkBindImageMemory: %d\n", res);
                goto fail;
            }

            blitMemTotal += blitMemReq.size;
        }
    }else
        TRACE("blitting directly to swapchain images\n");

    /* create imageviews */
    for(i = 0; i < swapchain->n_images; ++i){
        struct fs_hack_image *hack = &swapchain->fs_hack_images[i];

        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = hack->blit_image ? hack->blit_image : hack->swapchain_image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
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

        device->funcs.p_vkDestroyImage(device->device, hack->blit_image, NULL);
        hack->blit_image = VK_NULL_HANDLE;
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

    device->funcs.p_vkFreeMemory(device->device, swapchain->blit_image_memory, NULL);
    swapchain->blit_image_memory = VK_NULL_HANDLE;

    device->funcs.p_vkDestroySampler(device->device, swapchain->sampler, NULL);
    swapchain->sampler = VK_NULL_HANDLE;

    return res;
}

static void destroy_fs_hack_image(VkDevice device, struct VkSwapchainKHR_T *swapchain, struct fs_hack_image *hack)
{
    device->funcs.p_vkDestroyImageView(device->device, hack->user_view, NULL);
    device->funcs.p_vkDestroyImageView(device->device, hack->blit_view, NULL);
    device->funcs.p_vkDestroyImage(device->device, hack->user_image, NULL);
    device->funcs.p_vkDestroyImage(device->device, hack->blit_image, NULL);
    if(hack->cmd)
        device->funcs.p_vkFreeCommandBuffers(device->device,
                swapchain->cmd_pools[hack->cmd_queue_idx],
                    1, &hack->cmd);
    device->funcs.p_vkDestroySemaphore(device->device, hack->blit_finished, NULL);
}

#if defined(USE_STRUCT_CONVERSION)
static VkResult init_fs_hack_images(VkDevice device, struct VkSwapchainKHR_T *swapchain, VkSwapchainCreateInfoKHR_host *createinfo)
#else
static VkResult init_fs_hack_images(VkDevice device, struct VkSwapchainKHR_T *swapchain, VkSwapchainCreateInfoKHR *createinfo)
#endif
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
        imageInfo.usage = createinfo->imageUsage | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        imageInfo.sharingMode = createinfo->imageSharingMode;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.queueFamilyIndexCount = createinfo->queueFamilyIndexCount;
        imageInfo.pQueueFamilyIndices = createinfo->pQueueFamilyIndices;
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
        viewInfo.format = createinfo->imageFormat;
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

        object->surface_usage = caps.supportedUsageFlags;
        TRACE("surface usage flags: 0x%x\n", object->surface_usage);

        native_info.imageExtent = object->real_extent;
        native_info.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT; /* XXX: check if supported by surface */

        if(native_info.imageFormat != VK_FORMAT_B8G8R8A8_UNORM &&
                native_info.imageFormat != VK_FORMAT_B8G8R8A8_SRGB){
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

        result = init_fs_hack_images(device, object, &native_info);
        if(result != VK_SUCCESS){
            ERR("creating fs hack images failed: %d\n", result);
            device->funcs.p_vkDestroySwapchainKHR(device->device, object->swapchain, NULL);
            WINE_VK_REMOVE_HANDLE_MAPPING(device->phys_dev->instance, object);
            free(object);
            return result;
        }

        /* FIXME: would be nice to do this on-demand, but games can use up all
         * memory so we fail to allocate later */
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
    image_info_host.semaphore = wine_semaphore_from_handle(pAcquireInfo->semaphore)->semaphore;
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
        device->funcs.p_vkFreeMemory(device->device, object->blit_image_memory, NULL);
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
    VkImageCopy region = {0};
#if defined(USE_STRUCT_CONVERSION)
    VkImageMemoryBarrier_host barriers[3] = {{0}};
    VkCommandBufferBeginInfo_host beginInfo = {0};
#else
    VkImageMemoryBarrier barriers[3] = {{0}};
    VkCommandBufferBeginInfo beginInfo = {0};
#endif
    float constants[4];

    TRACE("recording compute command\n");

#if 0
    /* DOOM runs out of memory when allocating blit images after loading. */
    if(!swapchain->blit_image_memory){
        result = init_blit_images(device, swapchain);
        if(result != VK_SUCCESS)
            return result;
    }
#endif

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
    /* transition blit image from whatever to GENERAL */
    barriers[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barriers[1].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barriers[1].newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[1].image = hack->blit_image ? hack->blit_image : hack->swapchain_image;
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

    device->funcs.p_vkCmdPipelineBarrier(
            hack->cmd,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            0,
            0, NULL,
            0, NULL,
            1, barriers
    );

    if(hack->blit_image){
        /* for the copy... */
        /* no transition, just a barrier for our access masks (w -> r) */
        barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barriers[0].oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        barriers[0].newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barriers[0].image = hack->blit_image;
        barriers[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barriers[0].subresourceRange.baseMipLevel = 0;
        barriers[0].subresourceRange.levelCount = 1;
        barriers[0].subresourceRange.baseArrayLayer = 0;
        barriers[0].subresourceRange.layerCount = 1;
        barriers[0].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barriers[0].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        /* for the copy... */
        /* transition swapchain image from whatever to TRANSFER_DST
         * we don't care about the contents... */
        barriers[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barriers[1].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barriers[1].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barriers[1].image = hack->swapchain_image;
        barriers[1].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barriers[1].subresourceRange.baseMipLevel = 0;
        barriers[1].subresourceRange.levelCount = 1;
        barriers[1].subresourceRange.baseArrayLayer = 0;
        barriers[1].subresourceRange.layerCount = 1;
        barriers[1].srcAccessMask = 0;
        barriers[1].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        device->funcs.p_vkCmdPipelineBarrier(
                hack->cmd,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                0,
                0, NULL,
                0, NULL,
                2, barriers
        );

        /* copy from blit image to swapchain image */
        region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.srcSubresource.layerCount = 1;
        region.srcOffset.x = 0;
        region.srcOffset.y = 0;
        region.srcOffset.z = 0;
        region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.dstSubresource.layerCount = 1;
        region.dstOffset.x = 0;
        region.dstOffset.y = 0;
        region.dstOffset.z = 0;
        region.extent.width = swapchain->real_extent.width;
        region.extent.height = swapchain->real_extent.height;
        region.extent.depth = 1;

        device->funcs.p_vkCmdCopyImage(hack->cmd,
                hack->blit_image, VK_IMAGE_LAYOUT_GENERAL,
                hack->swapchain_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1, &region);

        /* transition swapchain image from TRANSFER_DST_OPTIMAL to PRESENT_SRC */
        barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barriers[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barriers[0].newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barriers[0].image = hack->swapchain_image;
        barriers[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barriers[0].subresourceRange.baseMipLevel = 0;
        barriers[0].subresourceRange.levelCount = 1;
        barriers[0].subresourceRange.baseArrayLayer = 0;
        barriers[0].subresourceRange.layerCount = 1;
        barriers[0].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barriers[0].dstAccessMask = 0;

        device->funcs.p_vkCmdPipelineBarrier(
                hack->cmd,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                0,
                0, NULL,
                0, NULL,
                1, barriers
        );
    }else{
        /* transition swapchain image from GENERAL to PRESENT_SRC */
        barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barriers[0].oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        barriers[0].newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barriers[0].image = hack->swapchain_image;
        barriers[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barriers[0].subresourceRange.baseMipLevel = 0;
        barriers[0].subresourceRange.levelCount = 1;
        barriers[0].subresourceRange.baseArrayLayer = 0;
        barriers[0].subresourceRange.layerCount = 1;
        barriers[0].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barriers[0].dstAccessMask = 0;

        device->funcs.p_vkCmdPipelineBarrier(
                hack->cmd,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                0,
                0, NULL,
                0, NULL,
                1, barriers
        );
    }

    result = device->funcs.p_vkEndCommandBuffer(hack->cmd);
    if(result != VK_SUCCESS){
        ERR("vkEndCommandBuffer: %d\n", result);
        return result;
    }

    return VK_SUCCESS;
}

static VkResult record_graphics_cmd(VkDevice device, struct VkSwapchainKHR_T *swapchain, struct fs_hack_image *hack)
{
    VkResult result;
    VkImageBlit blitregion = {0};
    VkImageSubresourceRange range = {0};
    VkClearColorValue black = {{0.f, 0.f, 0.f}};
#if defined(USE_STRUCT_CONVERSION)
    VkImageMemoryBarrier_host barriers[2] = {{0}};
    VkCommandBufferBeginInfo_host beginInfo = {0};
#else
    VkImageMemoryBarrier barriers[2] = {{0}};
    VkCommandBufferBeginInfo beginInfo = {0};
#endif

    TRACE("recording graphics command\n");

    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

    device->funcs.p_vkBeginCommandBuffer(hack->cmd, &beginInfo);

    /* transition real image from whatever to TRANSFER_DST_OPTIMAL */
    barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barriers[0].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barriers[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[0].image = hack->swapchain_image;
    barriers[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barriers[0].subresourceRange.baseMipLevel = 0;
    barriers[0].subresourceRange.levelCount = 1;
    barriers[0].subresourceRange.baseArrayLayer = 0;
    barriers[0].subresourceRange.layerCount = 1;
    barriers[0].srcAccessMask = 0;
    barriers[0].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    /* transition user image from PRESENT_SRC to TRANSFER_SRC_OPTIMAL */
    barriers[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barriers[1].oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    barriers[1].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[1].image = hack->user_image;
    barriers[1].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barriers[1].subresourceRange.baseMipLevel = 0;
    barriers[1].subresourceRange.levelCount = 1;
    barriers[1].subresourceRange.baseArrayLayer = 0;
    barriers[1].subresourceRange.layerCount = 1;
    barriers[1].srcAccessMask = 0;
    barriers[1].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

    device->funcs.p_vkCmdPipelineBarrier(
            hack->cmd,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            0, NULL,
            0, NULL,
            2, barriers
    );

    /* clear the image */
    range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    range.baseMipLevel = 0;
    range.levelCount = 1;
    range.baseArrayLayer = 0;
    range.layerCount = 1;

    device->funcs.p_vkCmdClearColorImage(
            hack->cmd, hack->swapchain_image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            &black, 1, &range);

    /* perform blit */
    blitregion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blitregion.srcSubresource.layerCount = 1;
    blitregion.srcOffsets[0].x = 0;
    blitregion.srcOffsets[0].y = 0;
    blitregion.srcOffsets[0].z = 0;
    blitregion.srcOffsets[1].x = swapchain->user_extent.width;
    blitregion.srcOffsets[1].y = swapchain->user_extent.height;
    blitregion.srcOffsets[1].z = 1;
    blitregion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blitregion.dstSubresource.layerCount = 1;
    blitregion.dstOffsets[0].x = swapchain->blit_dst.offset.x;
    blitregion.dstOffsets[0].y = swapchain->blit_dst.offset.y;
    blitregion.dstOffsets[0].z = 0;
    blitregion.dstOffsets[1].x = swapchain->blit_dst.offset.x + swapchain->blit_dst.extent.width;
    blitregion.dstOffsets[1].y = swapchain->blit_dst.offset.y + swapchain->blit_dst.extent.height;
    blitregion.dstOffsets[1].z = 1;

    device->funcs.p_vkCmdBlitImage(hack->cmd,
            hack->user_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            hack->swapchain_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &blitregion, swapchain->fs_hack_filter);

    /* transition real image from TRANSFER_DST to PRESENT_SRC */
    barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barriers[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barriers[0].newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[0].image = hack->swapchain_image;
    barriers[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barriers[0].subresourceRange.baseMipLevel = 0;
    barriers[0].subresourceRange.levelCount = 1;
    barriers[0].subresourceRange.baseArrayLayer = 0;
    barriers[0].subresourceRange.layerCount = 1;
    barriers[0].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barriers[0].dstAccessMask = 0;

    /* transition user image from TRANSFER_SRC_OPTIMAL to back to PRESENT_SRC */
    barriers[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barriers[1].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barriers[1].newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    barriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[1].image = hack->user_image;
    barriers[1].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barriers[1].subresourceRange.baseMipLevel = 0;
    barriers[1].subresourceRange.levelCount = 1;
    barriers[1].subresourceRange.baseArrayLayer = 0;
    barriers[1].subresourceRange.layerCount = 1;
    barriers[1].srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barriers[1].dstAccessMask = 0;

    device->funcs.p_vkCmdPipelineBarrier(
            hack->cmd,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
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

static VkResult fshack_vk_queue_present(VkQueue queue, const VkPresentInfoKHR *pPresentInfo)
{
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

                if(queue->device->queue_props[queue_idx].queueFlags & VK_QUEUE_GRAPHICS_BIT)
                    res = record_graphics_cmd(queue->device, swapchain, hack);
                else if(queue->device->queue_props[queue_idx].queueFlags & VK_QUEUE_COMPUTE_BIT)
                    res = record_compute_cmd(queue->device, swapchain, hack);
                else{
                    ERR("Present queue is neither graphics nor compute queue!\n");
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
    if (!strcmp(name, "vkGetMemoryWin32HandleKHR") || !strcmp(name, "vkGetMemoryWin32HandlePropertiesKHR"))
        name = "vkGetMemoryFdKHR";
    if (!strcmp(name, "vkGetSemaphoreWin32HandleKHR"))
        name = "vkGetSemaphoreFdKHR";
    if (!strcmp(name, "vkImportSemaphoreWin32HandleKHR"))
        name = "vkImportSemaphoreFdKHR";
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
    if (is_virtual_queue(queue))
    {
        FIXME("STEAMVR IS USING NATIVE HANDLE OF VIRTUALIZED QUEUE, THIS IS UNTESTED.\n");
        pthread_mutex_lock(&queue->submissions_mutex);
        while (queue->processing)
            pthread_cond_wait(&queue->submissions_cond, &queue->submissions_mutex);
        pthread_mutex_unlock(&queue->submissions_mutex);
    }

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

#define IOCTL_SHARED_GPU_RESOURCE_CREATE           CTL_CODE(FILE_DEVICE_VIDEO, 0, METHOD_BUFFERED, FILE_WRITE_ACCESS)

struct shared_resource_create
{
    obj_handle_t unix_handle;
    WCHAR name[1];
};

static HANDLE create_gpu_resource(int fd, LPCWSTR name)
{
    static const WCHAR shared_gpu_resourceW[] = {'\\','?','?','\\','S','h','a','r','e','d','G','p','u','R','e','s','o','u','r','c','e',0};
    HANDLE unix_resource = INVALID_HANDLE_VALUE;
    struct shared_resource_create *inbuff;
    UNICODE_STRING shared_gpu_resource_us;
    HANDLE shared_resource;
    OBJECT_ATTRIBUTES attr;
    IO_STATUS_BLOCK iosb;
    NTSTATUS status;
    DWORD in_size;

    TRACE("Creating shared vulkan resource fd %d name %s.\n", fd, debugstr_w(name));

    if (wine_server_fd_to_handle(fd, GENERIC_ALL, 0, &unix_resource) != STATUS_SUCCESS)
        return INVALID_HANDLE_VALUE;

    init_unicode_string(&shared_gpu_resource_us, shared_gpu_resourceW);

    attr.Length = sizeof(attr);
    attr.RootDirectory = 0;
    attr.Attributes = 0;
    attr.ObjectName = &shared_gpu_resource_us;
    attr.SecurityDescriptor = NULL;
    attr.SecurityQualityOfService = NULL;

    if ((status = NtCreateFile(&shared_resource, GENERIC_READ | GENERIC_WRITE, &attr, &iosb, NULL, FILE_ATTRIBUTE_NORMAL, FILE_SHARE_READ | FILE_SHARE_WRITE, FILE_OPEN, 0, NULL, 0)))
    {
        ERR("Failed to load open a shared resource handle, status %#lx.\n", (long int)status);
        NtClose(unix_resource);
        return INVALID_HANDLE_VALUE;
    }

    in_size = sizeof(*inbuff) + (name ? lstrlenW(name) * sizeof(WCHAR) : 0);
    inbuff = calloc(1, in_size);
    inbuff->unix_handle = wine_server_obj_handle(unix_resource);
    if (name)
        lstrcpyW(&inbuff->name[0], name);

    if ((status = NtDeviceIoControlFile(shared_resource, NULL, NULL, NULL, &iosb, IOCTL_SHARED_GPU_RESOURCE_CREATE,
            inbuff, in_size, NULL, 0)))

    free(inbuff);
    NtClose(unix_resource);

    if (status)
    {
        ERR("Failed to create video resource, status %#lx.\n", (long int)status);
        NtClose(shared_resource);
        return INVALID_HANDLE_VALUE;
    }

    return shared_resource;
}

#define IOCTL_SHARED_GPU_RESOURCE_OPEN             CTL_CODE(FILE_DEVICE_VIDEO, 1, METHOD_BUFFERED, FILE_WRITE_ACCESS)

struct shared_resource_open
{
    obj_handle_t kmt_handle;
    WCHAR name[1];
};

static HANDLE open_shared_resource(HANDLE kmt_handle, LPCWSTR name)
{
    static const WCHAR shared_gpu_resourceW[] = {'\\','?','?','\\','S','h','a','r','e','d','G','p','u','R','e','s','o','u','r','c','e',0};
    UNICODE_STRING shared_gpu_resource_us;
    struct shared_resource_open *inbuff;
    HANDLE shared_resource;
    OBJECT_ATTRIBUTES attr;
    IO_STATUS_BLOCK iosb;
    NTSTATUS status;
    DWORD in_size;

    init_unicode_string(&shared_gpu_resource_us, shared_gpu_resourceW);

    attr.Length = sizeof(attr);
    attr.RootDirectory = 0;
    attr.Attributes = 0;
    attr.ObjectName = &shared_gpu_resource_us;
    attr.SecurityDescriptor = NULL;
    attr.SecurityQualityOfService = NULL;

    if ((status = NtCreateFile(&shared_resource, GENERIC_READ | GENERIC_WRITE, &attr, &iosb, NULL, FILE_ATTRIBUTE_NORMAL, FILE_SHARE_READ | FILE_SHARE_WRITE, FILE_OPEN, 0, NULL, 0)))
    {
        ERR("Failed to load open a shared resource handle, status %#lx.\n", (long int)status);
        return INVALID_HANDLE_VALUE;
    }

    in_size = sizeof(*inbuff) + (name ? lstrlenW(name) * sizeof(WCHAR) : 0);
    inbuff = calloc(1, in_size);
    inbuff->kmt_handle = wine_server_obj_handle(kmt_handle);
    if (name)
        lstrcpyW(&inbuff->name[0], name);

    status = NtDeviceIoControlFile(shared_resource, NULL, NULL, NULL, &iosb, IOCTL_SHARED_GPU_RESOURCE_OPEN,
            inbuff, in_size, NULL, 0);

    free(inbuff);

    if (status)
    {
        ERR("Failed to open video resource, status %#lx.\n", (long int)status);
        NtClose(shared_resource);
        return INVALID_HANDLE_VALUE;
    }

    return shared_resource;
}

#define IOCTL_SHARED_GPU_RESOURCE_GET_UNIX_RESOURCE           CTL_CODE(FILE_DEVICE_VIDEO, 3, METHOD_BUFFERED, FILE_READ_ACCESS)

static int get_shared_resource_fd(HANDLE shared_resource)
{
    IO_STATUS_BLOCK iosb;
    obj_handle_t unix_resource;
    NTSTATUS status;
    int ret;

    if (NtDeviceIoControlFile(shared_resource, NULL, NULL, NULL, &iosb, IOCTL_SHARED_GPU_RESOURCE_GET_UNIX_RESOURCE,
            NULL, 0, &unix_resource, sizeof(unix_resource)))
        return -1;

    status = wine_server_handle_to_fd(wine_server_ptr_handle(unix_resource), FILE_READ_DATA, &ret, NULL);
    NtClose(wine_server_ptr_handle(unix_resource));
    return status == STATUS_SUCCESS ? ret : -1;
}

#define IOCTL_SHARED_GPU_RESOURCE_GETKMT           CTL_CODE(FILE_DEVICE_VIDEO, 2, METHOD_BUFFERED, FILE_READ_ACCESS)

static HANDLE get_shared_resource_kmt_handle(HANDLE shared_resource)
{
    IO_STATUS_BLOCK iosb;
    obj_handle_t kmt_handle;

    if (NtDeviceIoControlFile(shared_resource, NULL, NULL, NULL, &iosb, IOCTL_SHARED_GPU_RESOURCE_GETKMT,
            NULL, 0, &kmt_handle, sizeof(kmt_handle)))
        return INVALID_HANDLE_VALUE;

    return wine_server_ptr_handle(kmt_handle);
}

NTSTATUS wine_vkAllocateMemory(void *args)
{
    struct vkAllocateMemory_params *params = args;
    VkDevice device = params->device;
    const VkMemoryAllocateInfo *allocate_info = params->pAllocateInfo;
    const VkAllocationCallbacks *allocator = params->pAllocator;
    VkDeviceMemory *memory = params->pMemory;

    const VkImportMemoryWin32HandleInfoKHR *handle_import_info;
    const VkExportMemoryWin32HandleInfoKHR *handle_export_info;
    VkMemoryAllocateInfo allocate_info_dup = *allocate_info;
    VkExportMemoryAllocateInfo *export_info;
    VkImportMemoryFdInfoKHR fd_import_info;
    struct wine_dev_mem *object;
    VkResult res;
    int fd;

#if defined(USE_STRUCT_CONVERSION)
    VkMemoryAllocateInfo_host allocate_info_host;
    VkMemoryGetFdInfoKHR_host get_fd_info;
#else
    VkMemoryAllocateInfo allocate_info_host;
    VkMemoryGetFdInfoKHR get_fd_info;
#endif

    TRACE("%p %p %p %p\n", device, allocate_info, allocator, memory);

    if (allocator)
        FIXME("Support for allocation callbacks not implemented yet\n");

    if ((res = convert_VkMemoryAllocateInfo_struct_chain(allocate_info->pNext, &allocate_info_dup)) < 0)
    {
        WARN("Failed to convert VkMemoryAllocateInfo pNext chain, res=%d.\n", res);
        return res;
    }

    if (device->phys_dev->fake_memory_priority)
    {
        VkBaseOutStructure *header;

        for (header = (void *) &allocate_info_dup; header; header = header->pNext)
        {
            if (header->pNext && header->pNext->sType == VK_STRUCTURE_TYPE_MEMORY_PRIORITY_ALLOCATE_INFO_EXT)
            {
                VkBaseOutStructure *memory_priority = header->pNext;

                header->pNext = memory_priority->pNext;
                free(memory_priority);
                break;
            }
        }
    }

    if (!(object = calloc(1, sizeof(*object))))
    {
        free_VkMemoryAllocateInfo_struct_chain(&allocate_info_dup);
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    object->dev_mem = VK_NULL_HANDLE;
    object->handle = INVALID_HANDLE_VALUE;
    fd_import_info.fd = -1;
    fd_import_info.pNext = NULL;

    /* find and process handle import/export info and grab it */
    handle_import_info = wine_vk_find_struct(allocate_info, IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR);
    handle_export_info = wine_vk_find_struct(allocate_info, EXPORT_MEMORY_WIN32_HANDLE_INFO_KHR);
    if (handle_export_info && handle_export_info->pAttributes && handle_export_info->pAttributes->lpSecurityDescriptor)
        FIXME("Support for custom security descriptor not implemented.\n");

    if ((export_info = wine_vk_find_struct(&allocate_info_dup, EXPORT_MEMORY_ALLOCATE_INFO)))
    {
        object->handle_types = export_info->handleTypes;
        if (export_info->handleTypes & wine_vk_handle_over_fd_types)
            export_info->handleTypes |= VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
        wine_vk_normalize_handle_types_host(&export_info->handleTypes);
    }

    /* Vulkan consumes imported FDs, but not imported HANDLEs */
    if (handle_import_info)
    {
        fd_import_info.sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR;
        fd_import_info.pNext = allocate_info_dup.pNext;
        fd_import_info.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;

        switch (handle_import_info->handleType)
        {
            case VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT:
            case VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT:
                if (handle_import_info->handle)
                    NtDuplicateObject( NtCurrentProcess(), handle_import_info->handle, NtCurrentProcess(), &object->handle, 0, 0, DUPLICATE_SAME_ACCESS );
                else if (handle_import_info->name)
                    object->handle = open_shared_resource( 0, handle_import_info->name );
                break;
            case VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT:
            case VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_KMT_BIT:
                /* FIXME: the spec says that device memory imported from a KMT handle doesn't keep a reference to the underyling payload.
                   This means that in cases where on windows an application leaks VkDeviceMemory objects, we leak the full payload.  To
                   fix this, we would need wine_dev_mem objects to store no reference to the payload, that means no host VkDeviceMemory
                   object (as objects imported from FDs hold a reference to the payload), and no win32 handle to the object. We would then
                   extend make_vulkan to have the thunks converting wine_dev_mem to native handles open the VkDeviceMemory from the KMT
                   handle, use it in the host function, then close it again. */
                object->handle = open_shared_resource( handle_import_info->handle, NULL );
                break;
            default:
                WARN("Invalid handle type %08x passed in.\n", handle_import_info->handleType);
                res = VK_ERROR_INVALID_EXTERNAL_HANDLE;
                goto done;
        }

        if (object->handle != INVALID_HANDLE_VALUE)
            fd_import_info.fd = get_shared_resource_fd(object->handle);

        if (fd_import_info.fd == -1)
        {
            TRACE("Couldn't access resource handle or name. type=%08x handle=%p name=%s\n", handle_import_info->handleType, handle_import_info->handle,
                    handle_import_info->name ? debugstr_w(handle_import_info->name) : "");
            res = VK_ERROR_INVALID_EXTERNAL_HANDLE;
            goto done;
        }
    }

    allocate_info_host.sType = allocate_info_dup.sType;
    allocate_info_host.pNext = fd_import_info.fd == -1 ? allocate_info_dup.pNext : &fd_import_info;
    allocate_info_host.allocationSize = allocate_info_dup.allocationSize;
    allocate_info_host.memoryTypeIndex = allocate_info_dup.memoryTypeIndex;

    if ((res = device->funcs.p_vkAllocateMemory(device->device, &allocate_info_host, NULL, &object->dev_mem)) == VK_SUCCESS)
    {
        if (object->handle == INVALID_HANDLE_VALUE && export_info && export_info->handleTypes & VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT)
        {
            get_fd_info.sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR;
            get_fd_info.pNext = NULL;
            get_fd_info.memory = object->dev_mem;
            get_fd_info.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;

            if (device->funcs.p_vkGetMemoryFdKHR(device->device, &get_fd_info, &fd) == VK_SUCCESS)
            {
                object->handle = create_gpu_resource(fd, handle_export_info ? handle_export_info->name : NULL);
                object->access = handle_export_info ? handle_export_info->dwAccess : GENERIC_ALL;
                if (handle_export_info && handle_export_info->pAttributes)
                    object->inherit = handle_export_info->pAttributes->bInheritHandle;
                else
                    object->inherit = FALSE;
                close(fd);
            }

            if (object->handle == INVALID_HANDLE_VALUE)
            {
                res = VK_ERROR_OUT_OF_HOST_MEMORY;
                goto done;
            }
        }

        WINE_VK_ADD_NON_DISPATCHABLE_MAPPING(device->phys_dev->instance, object, object->dev_mem);
        *memory = wine_dev_mem_to_handle(object);
    }

    done:

    if (res != VK_SUCCESS)
    {
        device->funcs.p_vkFreeMemory(device->device, object->dev_mem, NULL);
        if (fd_import_info.fd != -1)
            close(fd_import_info.fd);
        if (object->handle != INVALID_HANDLE_VALUE)
            NtClose(object->handle);
        free(object);
    }

    free_VkMemoryAllocateInfo_struct_chain(&allocate_info_dup);

    return res;
}

NTSTATUS wine_vkGetMemoryWin32HandleKHR(void *args)
{
    struct vkGetMemoryWin32HandleKHR_params *params = args;
    VkDevice device = params->device;
    const VkMemoryGetWin32HandleInfoKHR *handle_info = params->pGetWin32HandleInfo;
    HANDLE *handle = params->pHandle;

    struct wine_dev_mem *dev_mem = wine_dev_mem_from_handle(handle_info->memory);
    const VkBaseInStructure *chain;
    HANDLE ret;

    TRACE("%p, %p %p\n", device, handle_info, handle);

    if (!(dev_mem->handle_types & handle_info->handleType))
        return VK_ERROR_UNKNOWN;

    if ((chain = handle_info->pNext))
        FIXME("Ignoring a linked structure of type %u.\n", chain->sType);

    switch(handle_info->handleType)
    {
        case VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT:
        case VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT:
            return !NtDuplicateObject( NtCurrentProcess(), dev_mem->handle, NtCurrentProcess(), handle, dev_mem->access, dev_mem->inherit ? OBJ_INHERIT : 0, 0) ?
                VK_SUCCESS : VK_ERROR_OUT_OF_HOST_MEMORY;
        case VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT:
        case VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_KMT_BIT:
        {
            if ((ret = get_shared_resource_kmt_handle(dev_mem->handle)) == INVALID_HANDLE_VALUE)
                return VK_ERROR_OUT_OF_HOST_MEMORY;
            *handle = ret;
            return VK_SUCCESS;
        }
        default:
            FIXME("Unable to get handle of type %x, did the application ignore the capabilities?\n", handle_info->handleType);
            return VK_ERROR_UNKNOWN;
    }
}

NTSTATUS wine_vkFreeMemory(void *args)
{
    struct vkFreeMemory_params *params = args;
    VkDevice device = params->device;
    VkDeviceMemory handle = params->memory;
    const VkAllocationCallbacks *allocator = params->pAllocator;

    struct wine_dev_mem *dev_mem = wine_dev_mem_from_handle(handle);

    TRACE("%p 0x%s, %p\n", device, wine_dbgstr_longlong(handle), allocator);

    if (allocator)
        FIXME("Support for allocation callbacks not implemented yet\n");

    if (!handle)
        return STATUS_SUCCESS;

    WINE_VK_REMOVE_HANDLE_MAPPING(device->phys_dev->instance, dev_mem);
    device->funcs.p_vkFreeMemory(device->device, dev_mem->dev_mem, NULL);
    if (dev_mem->handle != INVALID_HANDLE_VALUE)
        NtClose(dev_mem->handle);
    free(dev_mem);
    return STATUS_SUCCESS;
}

NTSTATUS wine_vkGetMemoryWin32HandlePropertiesKHR(void *args)
{
    struct vkGetMemoryWin32HandlePropertiesKHR_params *params = args;
    VkDevice device = params->device;
    VkExternalMemoryHandleTypeFlagBits type = params->handleType;
    HANDLE handle = params->handle;
    VkMemoryWin32HandlePropertiesKHR *properties = params->pMemoryWin32HandleProperties;

    TRACE("%p %u %p %p\n", device, type, handle, properties);

    /* VUID-vkGetMemoryWin32HandlePropertiesKHR-handleType-00666
       handleType must not be one of the handle types defined as opaque */
    return VK_ERROR_INVALID_EXTERNAL_HANDLE;
}

NTSTATUS wine_vkCreateBuffer(void *args)
{
    struct vkCreateBuffer_params *params = args;
    VkDevice device = params->device;
    const VkBufferCreateInfo *create_info = params->pCreateInfo;
    const VkAllocationCallbacks *allocator = params->pAllocator;
    VkBuffer *buffer = params->pBuffer;

    VkExternalMemoryBufferCreateInfo *external_memory_info;
    VkResult res;

#if defined(USE_STRUCT_CONVERSION)
    VkBufferCreateInfo_host create_info_host;
#else
    VkBufferCreateInfo create_info_host;
#endif

    TRACE("%p %p %p %p\n", device, create_info, allocator, buffer);

    if (allocator)
        FIXME("Support for allocation callbacks not implemented yet\n");

    if ((res = convert_VkBufferCreateInfo_struct_chain(create_info->pNext, (VkBufferCreateInfo *) &create_info_host)))
    {
        WARN("Failed to convert VkBufferCreateInfo pNext chain, res=%d.\n", res);
        return res;
    }

    if ((external_memory_info = wine_vk_find_struct(&create_info_host, EXTERNAL_MEMORY_BUFFER_CREATE_INFO)))
    {
        if (external_memory_info->handleTypes & wine_vk_handle_over_fd_types)
            external_memory_info->handleTypes |= VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
        wine_vk_normalize_handle_types_host(&external_memory_info->handleTypes);
    }

    create_info_host.sType = create_info->sType;
    create_info_host.flags = create_info->flags;
    create_info_host.size = create_info->size;
    create_info_host.usage = create_info->usage;
    create_info_host.sharingMode = create_info->sharingMode;
    create_info_host.queueFamilyIndexCount = create_info->queueFamilyIndexCount;
    create_info_host.pQueueFamilyIndices = create_info->pQueueFamilyIndices;

    res = device->funcs.p_vkCreateBuffer(device->device, &create_info_host, NULL, buffer);

    free_VkBufferCreateInfo_struct_chain((VkBufferCreateInfo *) &create_info_host);

    return res;
}

NTSTATUS wine_vkCreateImage(void *args)
{
    struct vkCreateImage_params *params = args;
    VkDevice device = params->device;
    const VkImageCreateInfo *create_info = params->pCreateInfo;
    const VkAllocationCallbacks *allocator = params->pAllocator;
    VkImage *image = params->pImage;

    VkExternalMemoryImageCreateInfo *external_memory_info;
    VkImageCreateInfo create_info_host = *create_info;
    VkResult res;

    TRACE("%p %p %p %p\n", device, create_info, allocator, image);

    if (allocator)
        FIXME("Support for allocation callbacks not implemented yet\n");

    if ((res = convert_VkImageCreateInfo_struct_chain(create_info->pNext, &create_info_host)))
    {
        WARN("Failed to convert VkImageCreateInfo pNext chain, res=%d.\n", res);
        return res;
    }

    if ((external_memory_info = wine_vk_find_struct(&create_info_host, EXTERNAL_MEMORY_IMAGE_CREATE_INFO)))
    {
        if (external_memory_info->handleTypes & wine_vk_handle_over_fd_types)
            external_memory_info->handleTypes |= VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT_KHR;
        wine_vk_normalize_handle_types_host(&external_memory_info->handleTypes);
    }

    res = device->funcs.p_vkCreateImage(device->device, &create_info_host, NULL, image);

    free_VkImageCreateInfo_struct_chain(&create_info_host);

    return res;
}

#define IOCTL_SHARED_GPU_RESOURCE_SET_OBJECT           CTL_CODE(FILE_DEVICE_VIDEO, 6, METHOD_BUFFERED, FILE_WRITE_ACCESS)

static bool set_shared_resource_object(HANDLE shared_resource, unsigned int index, HANDLE handle)
{
    IO_STATUS_BLOCK iosb;
    struct shared_resource_set_object
    {
        unsigned int index;
        obj_handle_t handle;
    } params;

    params.index = index;
    params.handle = wine_server_obj_handle(handle);

    return NtDeviceIoControlFile(shared_resource, NULL, NULL, NULL, &iosb, IOCTL_SHARED_GPU_RESOURCE_SET_OBJECT,
            &params, sizeof(params), NULL, 0) == STATUS_SUCCESS;
}

#define IOCTL_SHARED_GPU_RESOURCE_GET_OBJECT           CTL_CODE(FILE_DEVICE_VIDEO, 6, METHOD_BUFFERED, FILE_READ_ACCESS)

static HANDLE get_shared_resource_object(HANDLE shared_resource, unsigned int index)
{
    IO_STATUS_BLOCK iosb;
    obj_handle_t handle;

    if (NtDeviceIoControlFile(shared_resource, NULL, NULL, NULL, &iosb, IOCTL_SHARED_GPU_RESOURCE_GET_OBJECT,
            &index, sizeof(index), &handle, sizeof(handle)))
        return NULL;

    return wine_server_ptr_handle(handle);
}

static void d3d12_semaphore_lock(struct wine_semaphore *semaphore)
{
    assert( semaphore->handle_type == VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D12_FENCE_BIT );
    pthread_mutex_lock(&semaphore->d3d12_fence_shm->mutex);
}

static void d3d12_semaphore_unlock(struct wine_semaphore *semaphore)
{
    assert( semaphore->handle_type == VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D12_FENCE_BIT );
    pthread_mutex_unlock(&semaphore->d3d12_fence_shm->mutex);
}

/* returns -1 when there is no queued update that would satisfy the wait */
static uint64_t d3d12_semaphore_try_get_wait_value_locked(struct wine_semaphore *semaphore, uint64_t virtual_value,
        struct VkQueue_T *waiting_queue)
{
    struct pending_update *update;
    uint64_t ret = -1;
    unsigned int i;

    if (semaphore->d3d12_fence_shm->virtual_value >= virtual_value)
        return 0;

    for (i = 0; i < semaphore->d3d12_fence_shm->pending_updates_count; i++)
    {
        update = &semaphore->d3d12_fence_shm->pending_updates[i];

        if (update->virtual_value < virtual_value)
            continue;

        if (update->signalling_pid == getpid() && waiting_queue && update->signalling_queue == waiting_queue)
            return 0;

        ret = min(ret, update->physical_value);
    }

    return ret;
}

static struct pending_wait *d3d12_semaphore_push_wait_locked(struct wine_semaphore *semaphore, uint64_t virtual_value)
{
    struct pending_wait *wait;
    unsigned int i;

    for (i = 0; i < ARRAY_SIZE(semaphore->d3d12_fence_shm->pending_waits); i++)
    {
        wait = &semaphore->d3d12_fence_shm->pending_waits[i];
        if (!wait->present)
            break;
    }

    if (i == ARRAY_SIZE(semaphore->d3d12_fence_shm->pending_waits))
    {
        FIXME("Failed to wait on semaphore %p, maximum waits exceeded.\n", semaphore);
        return NULL;
    }

    wait->present = true;
    wait->satisfied = false;
    wait->virtual_value = virtual_value;
    wait->physical_value = 0;

    return wait;
}

static uint64_t d3d12_semaphore_pop_wait_locked(struct wine_semaphore *semaphore, struct pending_wait *wait)
{
    wait->satisfied = false;
    wait->present = false;

    return wait->physical_value;
}

static void d3d12_semaphore_satisfy_waits_locked(struct wine_semaphore *semaphore, uint64_t virtual_value,
        uint64_t physical_value)
{
    struct pending_wait *wait;
    unsigned int i;

    for (i = 0; i < ARRAY_SIZE(semaphore->d3d12_fence_shm->pending_waits); i++)
    {
        wait = &semaphore->d3d12_fence_shm->pending_waits[i];

        if (wait->present && !wait->satisfied && wait->virtual_value <= virtual_value)
        {
            wait->satisfied = true;
            wait->physical_value = physical_value;
            pthread_cond_signal(&wait->cond);
        }
    }
}

static uint64_t d3d12_semaphore_add_pending_signal_locked(struct wine_semaphore *semaphore, uint64_t virtual_value,
        struct VkQueue_T *signalling_queue)
{
    struct pending_update *update;

    if (semaphore->d3d12_fence_shm->pending_updates_count == ARRAY_SIZE(semaphore->d3d12_fence_shm->pending_updates))
    {
        FIXME("Failed to queue signal on d3d12 semaphore, maximum concurrent signals exceeded.\n");
        return 0;
    }

    update = &semaphore->d3d12_fence_shm->pending_updates[
        semaphore->d3d12_fence_shm->pending_updates_count++];

    update->virtual_value = virtual_value;
    update->physical_value = ++semaphore->d3d12_fence_shm->counter;
    update->signalling_pid = getpid();
    update->signalling_queue = signalling_queue;

    return update->physical_value;
}

static struct pending_update d3d12_semaphore_peek_added_signal_locked(struct wine_semaphore *semaphore)
{
    return semaphore->d3d12_fence_shm->pending_updates[semaphore->d3d12_fence_shm->pending_updates_count - 1];
}

static bool d3d12_semaphore_pop_pending_signal_locked(struct wine_semaphore *semaphore, uint64_t phys_val, struct pending_update *ret)
{
    struct pending_update *update;
    unsigned int i;

    for (i = 0; i < semaphore->d3d12_fence_shm->pending_updates_count; i++)
    {
        if (semaphore->d3d12_fence_shm->pending_updates[i].physical_value == phys_val)
        {
            update = &semaphore->d3d12_fence_shm->pending_updates[i];
            if (ret)
                *ret = *update;
            *update = semaphore->d3d12_fence_shm->pending_updates[--semaphore->d3d12_fence_shm->pending_updates_count];
            return true;
        }
    }

    return false;
}

static void d3d12_semaphore_update_phys_val_locked(struct wine_semaphore *sem, uint64_t phys_val)
{
    struct pending_update pending;

    /* Based off linked VKD3D-Proton implementation, but we don't signal CPU waits here.
        * https://github.com/HansKristian-Work/vkd3d-proton/blob/829ac72e3d381006a843c183e613e8ee77e0b292/libs/vkd3d/command.c#L758 */
    while (sem->d3d12_fence_shm->physical_value < phys_val)
    {
        sem->d3d12_fence_shm->physical_value++;

        if (d3d12_semaphore_pop_pending_signal_locked(sem, sem->d3d12_fence_shm->physical_value, &pending))
            sem->d3d12_fence_shm->virtual_value = pending.virtual_value;
    }
}

NTSTATUS wine_vkCreateSemaphore(void *args)
{
    struct vkCreateSemaphore_params *params = args;
    VkDevice device = params->device;
    const VkSemaphoreCreateInfo *create_info = params->pCreateInfo;
    const VkAllocationCallbacks *allocator = params->pAllocator;
    VkSemaphore *semaphore = params->pSemaphore;

    VkExportSemaphoreWin32HandleInfoKHR *export_handle_info = wine_vk_find_struct(create_info, EXPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR);
    VkExportSemaphoreCreateInfo *export_semaphore_info, timeline_export_info;
    VkSemaphoreCreateInfo create_info_dup = *create_info, create_info_host;
    VkSemaphoreTypeCreateInfo *found_type_info, type_info;
    VkSemaphoreGetFdInfoKHR_host fd_info;
    pthread_mutexattr_t mutex_attr;
    struct wine_semaphore *object;
    pthread_condattr_t cond_attr;
    OBJECT_ATTRIBUTES attr;
    HANDLE section_handle;
    LARGE_INTEGER li;
    unsigned int i;
    VkResult res;
    SIZE_T size;
    int fd;

    TRACE("(%p, %p, %p, %p)\n", device, create_info, allocator, semaphore);

    if (allocator)
        FIXME("Support for allocation callbacks not implemented yet\n");

    if ((res = convert_VkSemaphoreCreateInfo_struct_chain(create_info->pNext, &create_info_dup)))
    {
        WARN("Failed to convert VkSemaphoreCreateInfo pNext chain, res=%d.\n", res);
        return res;
    }

    if (!(object = calloc(1, sizeof(*object))))
    {
        free_VkSemaphoreCreateInfo_struct_chain(&create_info_dup);
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }
    object->handle = INVALID_HANDLE_VALUE;

    create_info_host = create_info_dup;

    if ((export_semaphore_info = wine_vk_find_struct(&create_info_host, EXPORT_SEMAPHORE_CREATE_INFO)))
    {
        object->export_types = export_semaphore_info->handleTypes;
        if (export_semaphore_info->handleTypes & VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT)
            export_semaphore_info->handleTypes |= VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;
        wine_vk_normalize_semaphore_handle_types_host(&export_semaphore_info->handleTypes);
    }

    if ((res = device->funcs.p_vkCreateSemaphore(device->device, &create_info_host, NULL, &object->semaphore)) != VK_SUCCESS)
        goto done;

    if (export_semaphore_info && export_semaphore_info->handleTypes == VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT)
    {
        fd_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_GET_FD_INFO_KHR;
        fd_info.pNext = NULL;
        fd_info.semaphore = object->semaphore;
        fd_info.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;

        if ((res = device->funcs.p_vkGetSemaphoreFdKHR(device->device, &fd_info, &fd)) == VK_SUCCESS)
        {
            object->handle = create_gpu_resource(fd, export_handle_info ? export_handle_info->name : NULL);
            close(fd);
        }

        if (object->handle == INVALID_HANDLE_VALUE)
        {
            res = VK_ERROR_OUT_OF_HOST_MEMORY;
            goto done;
        }
    }
    else if (object->export_types & VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D12_FENCE_BIT)
    {
        /* compatibleHandleTypes doesn't include any other types */
        assert(object->export_types == VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D12_FENCE_BIT);
        object->handle_type = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D12_FENCE_BIT;

        timeline_export_info.sType = VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO;
        timeline_export_info.pNext = NULL;
        timeline_export_info.handleTypes = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;

        type_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
        type_info.pNext = &timeline_export_info;
        type_info.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
        type_info.initialValue = 0;

        create_info_host.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        create_info_host.pNext = &type_info;
        create_info_host.flags = 0;

        if ((res = device->funcs.p_vkCreateSemaphore(device->device, &create_info_host, NULL, &object->fence_timeline_semaphore)) != VK_SUCCESS)
            goto done;

        fd_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_GET_FD_INFO_KHR;
        fd_info.pNext = NULL;
        fd_info.semaphore = object->fence_timeline_semaphore;
        fd_info.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;

        if ((res = device->funcs.p_vkGetSemaphoreFdKHR(device->device, &fd_info, &fd)) == VK_SUCCESS)
        {
            object->handle = create_gpu_resource(fd, export_handle_info ? export_handle_info->name : NULL);
            close(fd);
        }

        if (object->handle == INVALID_HANDLE_VALUE)
        {
            res = VK_ERROR_OUT_OF_HOST_MEMORY;
            goto done;
        }

        /* Shared Fence Memory */
        InitializeObjectAttributes(&attr, NULL, 0, NULL, NULL);
        size = li.QuadPart = sizeof(*object->d3d12_fence_shm);
        if (NtCreateSection(&section_handle, STANDARD_RIGHTS_REQUIRED | SECTION_QUERY | SECTION_MAP_READ | SECTION_MAP_WRITE, &attr, &li, PAGE_READWRITE, SEC_COMMIT, NULL))
        {
            res = VK_ERROR_OUT_OF_HOST_MEMORY;
            goto done;
        }

        if (!set_shared_resource_object(object->handle, 0, section_handle))
        {
            NtClose(section_handle);
            res = VK_ERROR_OUT_OF_HOST_MEMORY;
            goto done;
        }

        if (NtMapViewOfSection(section_handle, GetCurrentProcess(), (void**) &object->d3d12_fence_shm, 0, 0, NULL, &size, ViewShare, 0, PAGE_READWRITE))
        {
            NtClose(section_handle);
            res = VK_ERROR_OUT_OF_HOST_MEMORY;
            goto done;
        }

        NtClose(section_handle);

        if ((found_type_info = wine_vk_find_struct(create_info, SEMAPHORE_TYPE_CREATE_INFO)))
            object->d3d12_fence_shm->virtual_value = found_type_info->initialValue;

        pthread_mutexattr_init(&mutex_attr);
        pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED);
        if (pthread_mutex_init(&object->d3d12_fence_shm->mutex, &mutex_attr))
        {
            pthread_mutexattr_destroy(&mutex_attr);
            res = VK_ERROR_OUT_OF_HOST_MEMORY;
            goto done;
        }
        pthread_mutexattr_destroy(&mutex_attr);

        for (i = 0; i < ARRAY_SIZE(object->d3d12_fence_shm->pending_waits); i++)
        {
            pthread_condattr_init(&cond_attr);
            pthread_condattr_setpshared(&cond_attr, PTHREAD_PROCESS_SHARED);
            pthread_cond_init(&object->d3d12_fence_shm->pending_waits[i].cond, &cond_attr);
            pthread_condattr_destroy(&cond_attr);
        }

        WINE_VK_ADD_NON_DISPATCHABLE_MAPPING(device->phys_dev->instance, object, object->fence_timeline_semaphore);
    }

    WINE_VK_ADD_NON_DISPATCHABLE_MAPPING(device->phys_dev->instance, object, object->semaphore);
    *semaphore = wine_semaphore_to_handle(object);

    done:

    if (res != VK_SUCCESS)
    {
        pthread_mutex_destroy(&object->d3d12_fence_shm->mutex);
        if (object->d3d12_fence_shm)
            NtUnmapViewOfSection(GetCurrentProcess(), object->d3d12_fence_shm);
        if (object->handle != INVALID_HANDLE_VALUE)
            NtClose(object->handle);
        if (object->semaphore != VK_NULL_HANDLE)
            device->funcs.p_vkDestroySemaphore(device->device, object->semaphore, NULL);
        if (object->fence_timeline_semaphore != VK_NULL_HANDLE)
            device->funcs.p_vkDestroySemaphore(device->device, object->fence_timeline_semaphore, NULL);
        free(object);
    }

    free_VkSemaphoreCreateInfo_struct_chain(&create_info_dup);

    return res;
}

NTSTATUS wine_vkGetSemaphoreWin32HandleKHR(void *args)
{
    struct vkGetSemaphoreWin32HandleKHR_params *params = args;
    const VkSemaphoreGetWin32HandleInfoKHR *handle_info = params->pGetWin32HandleInfo;
    HANDLE *handle = params->pHandle;

    struct wine_semaphore *semaphore = wine_semaphore_from_handle(handle_info->semaphore);

    if (!(semaphore->export_types & handle_info->handleType))
        return VK_ERROR_INVALID_EXTERNAL_HANDLE;

    if (NtDuplicateObject( NtCurrentProcess(), semaphore->handle, NtCurrentProcess(), handle, 0, 0, DUPLICATE_SAME_ACCESS ))
        return VK_ERROR_INVALID_EXTERNAL_HANDLE;

    return VK_SUCCESS;
}

NTSTATUS wine_vkDestroySemaphore(void *args)
{
    struct vkDestroySemaphore_params *params = args;
    VkDevice device = params->device;
    VkSemaphore handle = params->semaphore;
    const VkAllocationCallbacks *allocator = params->pAllocator;

    struct wine_semaphore *semaphore = wine_semaphore_from_handle(handle);

    TRACE("%p 0x%s, %p\n", device, wine_dbgstr_longlong(handle), allocator);

    if (allocator)
        FIXME("Support for allocation callbacks not implemented yet\n");

    if (!handle)
        return VK_SUCCESS;

    if (semaphore->handle != INVALID_HANDLE_VALUE)
        NtClose(semaphore->handle);

    if (semaphore->d3d12_fence_shm)
        NtUnmapViewOfSection(GetCurrentProcess(), semaphore->d3d12_fence_shm);

    WINE_VK_REMOVE_HANDLE_MAPPING(device->phys_dev->instance, semaphore);
    device->funcs.p_vkDestroySemaphore(device->device, semaphore->semaphore, NULL);

    if (semaphore->fence_timeline_semaphore)
        device->funcs.p_vkDestroySemaphore(device->device, semaphore->fence_timeline_semaphore, NULL);

    free(semaphore);
    return VK_SUCCESS;
}

NTSTATUS wine_vkImportSemaphoreWin32HandleKHR(void *args)
{
    struct vkImportSemaphoreWin32HandleKHR_params *params = args;
    VkDevice device = params->device;
    const VkImportSemaphoreWin32HandleInfoKHR *handle_info = params->pImportSemaphoreWin32HandleInfo;

    struct wine_semaphore *semaphore = wine_semaphore_from_handle(handle_info->semaphore);
    VkImportSemaphoreFdInfoKHR_host fd_info;
    struct wine_semaphore output_semaphore;
    VkSemaphoreTypeCreateInfo type_info;
    VkSemaphoreCreateInfo create_info;
    HANDLE d3d12_fence_shm;
    NTSTATUS stat;
    VkResult res;
    SIZE_T size;

    TRACE("(%p, %p). semaphore = %p handle = %p\n", device, handle_info, handle_info->semaphore, handle_info->handle);

    if (handle_info->handleType == VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D12_FENCE_BIT && !semaphore->fence_timeline_semaphore)
    {
        type_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
        type_info.pNext = NULL;
        type_info.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
        type_info.initialValue = 0;

        create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        create_info.pNext = &type_info;
        create_info.flags = 0;

        if ((res = device->funcs.p_vkCreateSemaphore(device->device, &create_info, NULL, &semaphore->fence_timeline_semaphore)) != VK_SUCCESS)
        {
            ERR("Failed to create timeline semaphore backing D3D12 semaphore. vr %d.\n", res);
            return res;
        };

        WINE_VK_ADD_NON_DISPATCHABLE_MAPPING(device->phys_dev->instance, semaphore, semaphore->fence_timeline_semaphore);
    }

    output_semaphore = *semaphore;
    output_semaphore.handle = NULL;
    output_semaphore.handle_type = handle_info->handleType;
    output_semaphore.d3d12_fence_shm = NULL;

    fd_info.sType = VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_FD_INFO_KHR;
    fd_info.pNext = handle_info->pNext;
    fd_info.semaphore = wine_semaphore_host_handle(&output_semaphore);
    fd_info.flags = handle_info->flags;
    fd_info.handleType = handle_info->handleType;

    if (handle_info->handleType == VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT ||
        handle_info->handleType == VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D12_FENCE_BIT)
    {
        if (handle_info->name)
        {
            FIXME("Importing win32 semaphore by name not supported.\n");
            return VK_ERROR_INVALID_EXTERNAL_HANDLE;
        }

        if (NtDuplicateObject( NtCurrentProcess(), handle_info->handle, NtCurrentProcess(), &output_semaphore.handle, 0, 0, DUPLICATE_SAME_ACCESS ))
            return VK_ERROR_INVALID_EXTERNAL_HANDLE;

        fd_info.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;
        if ((fd_info.fd = get_shared_resource_fd(output_semaphore.handle)) == -1)
        {
            WARN("Invalid handle %p.\n", handle_info->handle);
            NtClose(output_semaphore.handle);
            return VK_ERROR_INVALID_EXTERNAL_HANDLE;
        }

        if (handle_info->handleType == VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D12_FENCE_BIT)
        {
            if (handle_info->flags & VK_SEMAPHORE_IMPORT_TEMPORARY_BIT)
            {
                FIXME("Temporarily importing d3d12 fences unsupported.\n");
                close(fd_info.fd);
                NtClose(output_semaphore.handle);
                return VK_ERROR_INVALID_EXTERNAL_HANDLE;
            }

            if (!(d3d12_fence_shm = get_shared_resource_object(output_semaphore.handle, 0)))
            {
                ERR("Failed to get D3D12 semaphore memory.\n");
                close(fd_info.fd);
                NtClose(output_semaphore.handle);
                return VK_ERROR_OUT_OF_HOST_MEMORY;
            }

            size = sizeof(*output_semaphore.d3d12_fence_shm);
            if ((stat = NtMapViewOfSection(d3d12_fence_shm, GetCurrentProcess(), (void**) &output_semaphore.d3d12_fence_shm, 0, 0, NULL, &size, ViewShare, 0, PAGE_READWRITE)))
            {
                ERR("Failed to map D3D12 semaphore memory. stat %#x.\n", stat);
                close(fd_info.fd);
                NtClose(d3d12_fence_shm);
                NtClose(output_semaphore.handle);
                return VK_ERROR_OUT_OF_HOST_MEMORY;
            }

            NtClose(d3d12_fence_shm);
        }
    }

    wine_vk_normalize_semaphore_handle_types_host(&fd_info.handleType);

    if (!fd_info.handleType)
    {
        FIXME("Importing win32 semaphore with handle type %#x not supported.\n", handle_info->handleType);
        return VK_ERROR_INVALID_EXTERNAL_HANDLE;
    }

    if ((res = device->funcs.p_vkImportSemaphoreFdKHR(device->device, &fd_info)) == VK_SUCCESS)
    {
        if (semaphore->handle)
            NtClose(semaphore->handle);
        if (semaphore->d3d12_fence_shm)
            NtUnmapViewOfSection(GetCurrentProcess(), semaphore->d3d12_fence_shm);

        *semaphore = output_semaphore;
    }
    else
    {
        if (output_semaphore.handle)
            NtClose(output_semaphore.handle);
        if (output_semaphore.d3d12_fence_shm)
            NtUnmapViewOfSection(GetCurrentProcess(), output_semaphore.d3d12_fence_shm);

        /* importing FDs transfers ownership, importing NT handles does not  */
        close(fd_info.fd);
    }

    return res;
}


static NTSTATUS vk_get_semaphore_counter_value(VkDevice device, VkSemaphore semaphore, uint64_t *value, bool khr);
static NTSTATUS wine_vk_get_semaphore_counter_value(VkDevice device, VkSemaphore handle, uint64_t *value, bool khr)
{
    struct wine_semaphore *semaphore = wine_semaphore_from_handle(handle);

    if (semaphore->handle_type == VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D12_FENCE_BIT)
    {
        d3d12_semaphore_lock(semaphore);
        *value = semaphore->d3d12_fence_shm->virtual_value;
        d3d12_semaphore_unlock(semaphore);
        return VK_SUCCESS;
    }

    return vk_get_semaphore_counter_value(device, handle, value, khr);
}

static NTSTATUS vk_get_semaphore_counter_value(VkDevice device, VkSemaphore semaphore, uint64_t *value, bool khr)
{
    if (khr)
        return thunk_vkGetSemaphoreCounterValueKHR(device, semaphore, value);
    else
        return thunk_vkGetSemaphoreCounterValue(device, semaphore, value);
}

NTSTATUS wine_vkGetSemaphoreCounterValue(void *args)
{
    struct vkGetSemaphoreCounterValue_params *params = args;
    VkDevice device = params->device;
    VkSemaphore semaphore = params->semaphore;
    uint64_t *value = params->pValue;

    return wine_vk_get_semaphore_counter_value(device, semaphore, value, false);
}

NTSTATUS wine_vkGetSemaphoreCounterValueKHR(void *args)
{
    struct vkGetSemaphoreCounterValue_params *params = args;
    VkDevice device = params->device;
    VkSemaphore semaphore = params->semaphore;
    uint64_t *value = params->pValue;

    return wine_vk_get_semaphore_counter_value(device, semaphore, value, true);
}

static NTSTATUS vk_signal_semaphore(VkDevice device, const VkSemaphoreSignalInfo *signal_info, bool khr);
static NTSTATUS wine_vk_signal_semaphore(VkDevice device, const VkSemaphoreSignalInfo *signal_info, bool khr)
{
    uint64_t phys_val;
    VkResult vr;

    struct wine_semaphore *semaphore = wine_semaphore_from_handle(signal_info->semaphore);

    TRACE("(%p, %p)\n", device, signal_info);

    if (semaphore->handle_type == VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D12_FENCE_BIT)
    {
        d3d12_semaphore_lock(semaphore);

        /* vkWaitSemaphore w/ WAIT_ANY wakes on every physical value increment to check if the wait is satisfied, so
           if there are no scheduled signals, step the physical value */
        if ((vr = vk_get_semaphore_counter_value(device, signal_info->semaphore, &phys_val, khr)) != VK_SUCCESS)
        {
            d3d12_semaphore_unlock(semaphore);
            return vr;
        }

        d3d12_semaphore_update_phys_val_locked(semaphore, phys_val);

        if (!semaphore->d3d12_fence_shm->pending_updates_count)
        {
            VkSemaphoreSignalInfo step_signal_info;

            assert(semaphore->d3d12_fence_shm->counter == phys_val);
            phys_val++;

            semaphore->d3d12_fence_shm->counter = semaphore->d3d12_fence_shm->physical_value = phys_val;

            step_signal_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO;
            step_signal_info.pNext = NULL;
            step_signal_info.semaphore = signal_info->semaphore;
            step_signal_info.value = phys_val;

            vr = vk_signal_semaphore(device, &step_signal_info, khr);
            if (vr != VK_SUCCESS)
            {
                d3d12_semaphore_unlock(semaphore);
                return vr;
            }
        }

        /* If a queue is already waiting on the pending physical value of a previous submit, this won't wake it up. */
        d3d12_semaphore_satisfy_waits_locked(semaphore, signal_info->value, 0);
        semaphore->d3d12_fence_shm->virtual_value = signal_info->value;

        d3d12_semaphore_unlock(semaphore);
        return VK_SUCCESS;
    }

    return vk_signal_semaphore(device, signal_info, khr);
}

static NTSTATUS vk_signal_semaphore(VkDevice device, const VkSemaphoreSignalInfo *signal_info, bool khr)
{
    if (khr)
        return thunk_vkSignalSemaphoreKHR(device, signal_info);
    else
        return thunk_vkSignalSemaphore(device, signal_info);
}

NTSTATUS wine_vkSignalSemaphore(void *args)
{
    struct vkSignalSemaphore_params *params = args;
    VkDevice device = params->device;
    const VkSemaphoreSignalInfo *signal_info = params->pSignalInfo;

    return wine_vk_signal_semaphore(device, signal_info, false);
}

NTSTATUS wine_vkSignalSemaphoreKHR(void *args)
{
    struct vkSignalSemaphore_params *params = args;
    VkDevice device = params->device;
    const VkSemaphoreSignalInfo *signal_info = params->pSignalInfo;

    return wine_vk_signal_semaphore(device, signal_info, true);
}

static NTSTATUS vk_wait_semaphores(VkDevice device, const VkSemaphoreWaitInfo *wait_info, uint64_t timeout, bool khr);
static NTSTATUS wine_vk_wait_semaphores(VkDevice device, const VkSemaphoreWaitInfo *wait_info, uint64_t timeout, bool khr)
{
    VkSemaphoreWaitInfo wait_info_dup = *wait_info;
    struct timespec abs_timeout, start_time;
    struct pending_wait **pending_waits;
    struct pending_wait *pending_wait;
    unsigned int i, remaining_waits;
    VkSemaphore* semaphores_dup;
    uint64_t *values_dup;
    int64_t tv_sec_wide;
    uint64_t phys_val;
    int wait_stat;
    VkResult res;

    TRACE("(%p, %p, 0x%s)\n", device, wait_info, wine_dbgstr_longlong(timeout));

    if (timeout)
    {
        clock_gettime(CLOCK_REALTIME, &start_time);

        abs_timeout.tv_sec = tv_sec_wide = start_time.tv_sec + (timeout / NANOSECONDS_IN_A_SECOND);
        abs_timeout.tv_nsec = start_time.tv_nsec + (timeout % NANOSECONDS_IN_A_SECOND);
        if (abs_timeout.tv_nsec >= NANOSECONDS_IN_A_SECOND)
        {
            abs_timeout.tv_sec++;
            tv_sec_wide++;
            abs_timeout.tv_nsec-=NANOSECONDS_IN_A_SECOND;
        }

        /* tv_sec is still! 32-bit on x86 */
        if (tv_sec_wide > abs_timeout.tv_sec)
            abs_timeout.tv_sec = INT_MAX;
    }

    wait_info_dup.pSemaphores = semaphores_dup = calloc(wait_info->semaphoreCount, sizeof(VkSemaphore));
    wait_info_dup.pValues = values_dup = calloc(wait_info->semaphoreCount, sizeof(uint64_t));
    pending_waits = calloc(wait_info->semaphoreCount, sizeof(struct pending_wait *));

    for (i = 0; i < wait_info->semaphoreCount; i++)
    {
        struct wine_semaphore *semaphore = wine_semaphore_from_handle(wait_info->pSemaphores[i]);

        semaphores_dup[i] = wait_info->pSemaphores[i];
        values_dup[i] = wait_info->pValues[i];

        if (semaphore->handle_type == VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D12_FENCE_BIT)
        {
            d3d12_semaphore_lock(semaphore);
            if ((values_dup[i] = d3d12_semaphore_try_get_wait_value_locked(semaphore, wait_info->pValues[i], NULL)) == -1)
            {
                if (!timeout)
                {
                    d3d12_semaphore_unlock(semaphore);
                    continue;
                }

                pending_wait = d3d12_semaphore_push_wait_locked(semaphore, wait_info->pValues[i]);

                if (wait_info->flags & VK_SEMAPHORE_WAIT_ANY_BIT)
                {
                    /* Keep scheduling a wait of current physical_value+1 until the desired virtual value is signaled */
                    values_dup[i] = semaphore->d3d12_fence_shm->physical_value + 1;
                    pending_waits[i] = pending_wait;
                }
                else
                {
                    while (!pending_wait->satisfied && wait_stat != ETIMEDOUT)
                        wait_stat = pthread_cond_timedwait(&pending_wait->cond, &semaphore->d3d12_fence_shm->mutex, &abs_timeout);

                    values_dup[i] = d3d12_semaphore_pop_wait_locked(semaphore, pending_wait);

                    if (wait_stat == ETIMEDOUT)
                    {
                        d3d12_semaphore_unlock(semaphore);
                        free(semaphores_dup);
                        free(values_dup);
                        free(pending_waits);
                        return VK_TIMEOUT;
                    }
                }
            }
            d3d12_semaphore_unlock(semaphore);
        }
    }

    do
    {
        if (timeout)
        {
            clock_gettime(CLOCK_REALTIME, &start_time);

            if (start_time.tv_sec > abs_timeout.tv_sec ||
                    (start_time.tv_sec == abs_timeout.tv_sec && start_time.tv_nsec >= abs_timeout.tv_nsec))
                timeout = 0;
            else
                timeout = ((abs_timeout.tv_sec - start_time.tv_sec) * NANOSECONDS_IN_A_SECOND) +
                    (abs_timeout.tv_nsec - start_time.tv_nsec);
        }

        remaining_waits = 0;
        res = vk_wait_semaphores(device, &wait_info_dup, timeout, khr);

        for (i = 0; i < wait_info->semaphoreCount; i++)
        {
            struct wine_semaphore * semaphore = wine_semaphore_from_handle(wait_info->pSemaphores[i]);

            if (pending_waits[i])
            {
                remaining_waits++;

                d3d12_semaphore_lock(semaphore);
                if (res != VK_SUCCESS || pending_waits[i]->satisfied)
                {
                    values_dup[i] = pending_waits[i]->physical_value;
                    d3d12_semaphore_pop_wait_locked(semaphore, pending_waits[i]);
                    pending_waits[i] = NULL;
                }
                d3d12_semaphore_unlock(semaphore);
            }
        }
    }
    while (res == VK_SUCCESS && remaining_waits);

    /* Make sure the physical value we waited on is processed before returning */
    for (i = 0; i < wait_info_dup.semaphoreCount; i++)
    {
        struct wine_semaphore *semaphore = wine_semaphore_from_handle(wait_info_dup.pSemaphores[i]);

        if (semaphore->handle_type == VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D12_FENCE_BIT)
        {
            d3d12_semaphore_lock(semaphore);
            if (wait_info->flags & VK_SEMAPHORE_WAIT_ANY_BIT)
            {
                if (!vk_get_semaphore_counter_value(device, semaphores_dup[i], &phys_val, khr))
                    d3d12_semaphore_update_phys_val_locked(semaphore, phys_val);
            }
            else
                d3d12_semaphore_update_phys_val_locked(semaphore, values_dup[i]);
            d3d12_semaphore_unlock(semaphore);
        }
    }

    free(semaphores_dup);
    free(values_dup);
    free(pending_waits);
    return res;
}

static NTSTATUS vk_wait_semaphores(VkDevice device, const VkSemaphoreWaitInfo *wait_info, uint64_t timeout, bool khr)
{
    if (khr)
        return thunk_vkWaitSemaphoresKHR(device, wait_info, timeout);
    else
        return thunk_vkWaitSemaphores(device, wait_info, timeout);
}

NTSTATUS wine_vkWaitSemaphores(void *args)
{
    struct vkWaitSemaphores_params *params = args;
    VkDevice device = params->device;
    const VkSemaphoreWaitInfo *wait_info = params->pWaitInfo;
    uint64_t timeout = params->timeout;

    return wine_vk_wait_semaphores(device, wait_info, timeout, false);
}

NTSTATUS wine_vkWaitSemaphoresKHR(void *args)
{
    struct vkWaitSemaphores_params *params = args;
    VkDevice device = params->device;
    const VkSemaphoreWaitInfo *wait_info = params->pWaitInfo;
    uint64_t timeout = params->timeout;

    return wine_vk_wait_semaphores(device, wait_info, timeout, true);
}

struct signal_op
{
    enum
    {
        SIGNAL_TYPE_SEMAPHORE,
        SIGNAL_TYPE_FENCE,
    } signal_type;

    union
    {
        struct
        {
            struct wine_semaphore *obj;
            uint64_t phys_val;

            bool khr;
        } semaphore;

        struct wine_fence *fence;
    };

    struct list entry;
};

static void *queue_signaller_worker(void *arg)
{
    struct VkQueue_T *queue = (struct VkQueue_T *)arg;
    VkSemaphoreWaitInfo wait_info;
    struct signal_op *signal_op;
    VkSemaphore sem_handle;
    bool device_lost;
    VkFence fence;
    uint64_t buf;
    VkResult vr;

    for (;;)
    {
        pthread_mutex_lock(&queue->signaller_mutex);

        while (!queue->stop && list_empty(&queue->signal_ops))
            pthread_cond_wait(&queue->signaller_cond, &queue->signaller_mutex);

        if (queue->stop)
        {
            assert( list_empty(&queue->signal_ops) );
            pthread_mutex_unlock(&queue->signaller_mutex);
            return NULL;
        }

        signal_op = LIST_ENTRY(list_head(&queue->signal_ops), struct signal_op, entry);
        list_remove(&signal_op->entry);

        device_lost = queue->device_lost;

        pthread_mutex_unlock(&queue->signaller_mutex);

        if (signal_op->signal_type == SIGNAL_TYPE_SEMAPHORE)
        {
            wait_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
            wait_info.pNext = NULL;
            wait_info.flags = 0;
            wait_info.semaphoreCount = 1;
            sem_handle = wine_semaphore_to_handle(signal_op->semaphore.obj);
            wait_info.pSemaphores = &sem_handle;
            wait_info.pValues = &signal_op->semaphore.phys_val;
            if (!device_lost && (vr = vk_wait_semaphores(queue->device, &wait_info, -1, signal_op->semaphore.khr)) < 0)
            {
                /* likely GPU hang */
                fprintf(stderr, "winevulkan/queue_signaller_worker: Semaphore wait failed, vr %d.\n", vr);
                continue;
            }

            d3d12_semaphore_lock(signal_op->semaphore.obj);
            d3d12_semaphore_update_phys_val_locked(signal_op->semaphore.obj, signal_op->semaphore.phys_val);
            d3d12_semaphore_unlock(signal_op->semaphore.obj);
        }
        else
        {
            fence = wine_fence_to_handle(signal_op->fence);

            if (!device_lost && (vr = thunk_vkWaitForFences(queue->device, 1, &fence, VK_TRUE, -1)) < 0)
            {
                /* likely GPU hang */
                fprintf(stderr, "winevulkan/queue_signaller_worker: Fence wait failed, vr %d.\n", vr);
                continue;
            }

            buf = 1;
            assert( write(signal_op->fence->eventfd, &buf, sizeof(buf)) != -1 );
        }

        free(signal_op);
    }

    return NULL;
}

static VkSubmitInfo *copy_VkSubmitInfo(const VkSubmitInfo *in, uint32_t submit_count)
{
    VkSubmitInfo *out = malloc(sizeof(*out) * submit_count);
    unsigned int i;

    for (i = 0; i < submit_count; i++)
    {
        out[i].sType = in[i].sType;
        out[i].waitSemaphoreCount = in[i].waitSemaphoreCount;
        out[i].pWaitSemaphores = memdup(in[i].pWaitSemaphores, in[i].waitSemaphoreCount, sizeof(out[i].pWaitSemaphores[0]));
        out[i].pWaitDstStageMask = memdup(in[i].pWaitDstStageMask, in[i].waitSemaphoreCount, sizeof(out[i].pWaitDstStageMask[0]));
        out[i].commandBufferCount = in[i].commandBufferCount;
        out[i].pCommandBuffers = memdup(in[i].pCommandBuffers, in[i].commandBufferCount, sizeof(out[i].pCommandBuffers[0]));
        out[i].signalSemaphoreCount = in[i].signalSemaphoreCount;
        out[i].pSignalSemaphores = memdup(in[i].pSignalSemaphores, in[i].signalSemaphoreCount, sizeof(out[i].pSignalSemaphores[0]));

        convert_VkSubmitInfo_struct_chain(in[i].pNext, &out[i]);
    }

    return out;
}

static void free_copied_VkSubmitInfo(VkSubmitInfo *info, uint32_t submit_count)
{
    unsigned int i;

    for (i = 0; i < submit_count; i++)
    {
        free_VkSubmitInfo_struct_chain(&info[i]);

        free((VkSemaphore *)         info[i].pWaitSemaphores);
        free((VkPipelineStageFlags*) info[i].pWaitDstStageMask);
        free((VkCommandBuffer*)      info[i].pCommandBuffers);
        free((VkSemaphore*)          info[i].pSignalSemaphores);
    }

    free(info);
}

static VkSubmitInfo2 *copy_VkSubmitInfo2(const VkSubmitInfo2 *in, uint32_t submit_count)
{
    VkSubmitInfo2 *out = malloc(sizeof(*out) * submit_count);
    VkCommandBufferSubmitInfo *cmdbuf_submit_info;
    VkSemaphoreSubmitInfo *sem_submit_info;
    unsigned int i, k;

    for (i = 0; i < submit_count; i++)
    {
        out[i].sType = in[i].sType;
        out[i].flags = in[i].flags;

        out[i].waitSemaphoreInfoCount = in[i].waitSemaphoreInfoCount;
        out[i].pWaitSemaphoreInfos = sem_submit_info = memdup(in[i].pWaitSemaphoreInfos,
                                            in[i].waitSemaphoreInfoCount,
                                            sizeof(out[i].pWaitSemaphoreInfos[0]));
        for (k = 0; k < out[i].waitSemaphoreInfoCount; k++)
        {
            if (sem_submit_info[k].pNext)
            {
                FIXME("pNext chain conversion for VkSemaphoreSubmitInfo not supported.\n");
                sem_submit_info[k].pNext = NULL;
            }
        }

        out[i].commandBufferInfoCount = in[i].commandBufferInfoCount;
        out[i].pCommandBufferInfos = cmdbuf_submit_info = memdup(in[i].pCommandBufferInfos,
                                            in[i].commandBufferInfoCount,
                                            sizeof(out[i].pCommandBufferInfos[0]));
        for (k = 0; k < out[i].commandBufferInfoCount; k++)
        {
            if (cmdbuf_submit_info[k].pNext)
            {
                FIXME("pNext chain conversion for VkCommandBufferSubmitInfo not supported.\n");
                cmdbuf_submit_info[k].pNext = NULL;
            }
        }

        out[i].signalSemaphoreInfoCount = in[i].signalSemaphoreInfoCount;
        out[i].pSignalSemaphoreInfos = sem_submit_info =memdup(in[i].pSignalSemaphoreInfos,
                                              in[i].signalSemaphoreInfoCount,
                                              sizeof(out[i].pSignalSemaphoreInfos[0]));
        for (k = 0; k < out[i].signalSemaphoreInfoCount; k++)
        {
            if (sem_submit_info[k].pNext)
            {
                FIXME("pNext chain conversion for VkSemaphoreSubmitInfo not supported.\n");
                sem_submit_info[k].pNext = NULL;
            }
        }

        convert_VkSubmitInfo2_struct_chain(in[i].pNext, &out[i]);
    }

    return out;
}

static void free_copied_VkSubmitInfo2(VkSubmitInfo2 *info, uint32_t submit_count)
{
    unsigned int i;

    for (i = 0; i < submit_count; i++)
    {
        free_VkSubmitInfo2_struct_chain(&info[i]);

        free((VkSemaphoreSubmitInfo  *)    info[i].pWaitSemaphoreInfos);
        free((VkCommandBufferSubmitInfo *) info[i].pCommandBufferInfos);
        free((VkSemaphoreSubmitInfo *)     info[i].pSignalSemaphoreInfos);
    }

    free(info);
}

struct queue_submit_unit
{
    uint32_t submit_count;
    VkSubmitInfo *submits;
    VkSubmitInfo2 *submits2;
    VkFence fence;
    bool khr;

    struct pending_wait **waits;

    struct list entry;
};

/* Abstracts away the differences between VkSubmitInfo and VkSubmitInfo2. */
static bool for_each_d3d12_semaphore(struct queue_submit_unit *unit, bool signal,
                                     struct wine_semaphore **semaphore_out, uint64_t **value_out, uint32_t counter)
{
    VkTimelineSemaphoreSubmitInfo *timeline_values;
    struct wine_semaphore *semaphore;
    unsigned int i, j, k;
    uint32_t sem_count;

    for (i = 0, k = 0; i < unit->submit_count; i++)
    {
        if (unit->submits)
        {
            timeline_values = wine_vk_find_struct(&unit->submits[i], TIMELINE_SEMAPHORE_SUBMIT_INFO);

            if (signal)
                sem_count = unit->submits[i].signalSemaphoreCount;
            else
                sem_count = unit->submits[i].waitSemaphoreCount;

            for (j = 0; j < sem_count; j++)
            {
                if (signal)
                    semaphore = wine_semaphore_from_handle(unit->submits[i].pSignalSemaphores[j]);
                else
                    semaphore = wine_semaphore_from_handle(unit->submits[i].pWaitSemaphores[j]);

                if (semaphore->handle_type != VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D12_FENCE_BIT)
                    continue;

                if (k++ == counter)
                {
                    *semaphore_out = semaphore;
                    if (signal)
                        *value_out = (uint64_t *) &timeline_values->pSignalSemaphoreValues[j];
                    else
                        *value_out = (uint64_t *) &timeline_values->pWaitSemaphoreValues[j];
                    return true;
                }
            }
        }
        else
        {
            if (signal)
                sem_count = unit->submits2[i].signalSemaphoreInfoCount;
            else
                sem_count = unit->submits2[i].waitSemaphoreInfoCount;

            for (j = 0; j < sem_count; j++)
            {
                if (signal)
                    semaphore = wine_semaphore_from_handle(unit->submits2[i].pSignalSemaphoreInfos[j].semaphore);
                else
                    semaphore = wine_semaphore_from_handle(unit->submits2[i].pWaitSemaphoreInfos[j].semaphore);

                if (semaphore->handle_type != VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D12_FENCE_BIT)
                    continue;

                if (k++ == counter)
                {
                    *semaphore_out = semaphore;
                    if (signal)
                        *value_out = (uint64_t *) &unit->submits2[i].pSignalSemaphoreInfos[j].value;
                    else
                        *value_out = (uint64_t *) &unit->submits2[i].pWaitSemaphoreInfos[j].value;
                    return true;
                }
            }
        }
    }

    return false;
}

static void *virtual_queue_worker(void *arg)
{
    struct VkQueue_T *queue = (struct VkQueue_T *)arg;
    struct queue_submit_unit *submit_unit;
    struct signal_op *signal_op;
    struct wine_semaphore *sem;
    struct pending_wait *wait;
    struct wine_fence *fence;
    bool device_lost = false;
    uint64_t *timeline_value;
    unsigned int i;
    VkResult vr;

    for (;;)
    {
        pthread_mutex_lock(&queue->submissions_mutex);

        while (!queue->stop && list_empty(&queue->submissions))
            pthread_cond_wait(&queue->submissions_cond, &queue->submissions_mutex);

        if (queue->stop)
        {
            assert( list_empty(&queue->submissions) );
            pthread_mutex_unlock(&queue->submissions_mutex);
            return NULL;
        }

        submit_unit = LIST_ENTRY(list_head(&queue->submissions), struct queue_submit_unit, entry);
        list_remove(&submit_unit->entry);

        pthread_mutex_unlock(&queue->submissions_mutex);

        if (device_lost)
            goto free_submit_unit;

        /* Wait for all fences to have a pending signal */
        for (i = 0; for_each_d3d12_semaphore(submit_unit, false, &sem, &timeline_value, i); i++)
        {
            if ((wait = submit_unit->waits[i++]))
            {
                assert(wait);
                d3d12_semaphore_lock(sem);

                while (!wait->satisfied)
                    pthread_cond_wait(&wait->cond, &sem->d3d12_fence_shm->mutex);

                *timeline_value = d3d12_semaphore_pop_wait_locked(sem, wait);

                d3d12_semaphore_unlock(sem);
            }
        }

        for (i = 0; for_each_d3d12_semaphore(submit_unit, true, &sem, &timeline_value, i); i++)
        {
            d3d12_semaphore_lock(sem);

            *timeline_value = d3d12_semaphore_add_pending_signal_locked(sem, *timeline_value, queue);
        }

        if (submit_unit->submits)
            vr = thunk_vkQueueSubmit(queue, submit_unit->submit_count, submit_unit->submits, submit_unit->fence);
        else
        {
            if (submit_unit->khr)
                vr = thunk_vkQueueSubmit2KHR(queue, submit_unit->submit_count, submit_unit->submits2, submit_unit->fence);
            else
                vr = thunk_vkQueueSubmit2(queue, submit_unit->submit_count, submit_unit->submits2, submit_unit->fence);
        }

        pthread_mutex_lock(&queue->signaller_mutex);

        for (i = 0; for_each_d3d12_semaphore(submit_unit, true, &sem, &timeline_value, i); i++)
        {
            if (vr == VK_SUCCESS)
            {
                struct pending_update added_signal = d3d12_semaphore_peek_added_signal_locked(sem);
                d3d12_semaphore_satisfy_waits_locked(sem, added_signal.virtual_value, added_signal.physical_value);

                signal_op = malloc(sizeof(*signal_op));
                signal_op->signal_type = SIGNAL_TYPE_SEMAPHORE;
                signal_op->semaphore.obj = sem;
                signal_op->semaphore.phys_val = added_signal.physical_value;
                signal_op->semaphore.khr = submit_unit->khr;

                list_add_tail(&queue->signal_ops, &signal_op->entry);
            }
            else
            {
                d3d12_semaphore_pop_pending_signal_locked(sem, *timeline_value, NULL);
            }

            d3d12_semaphore_unlock(sem);
        }

        if (vr == VK_SUCCESS && (fence = wine_fence_from_handle(submit_unit->fence)))
        {
            signal_op = malloc(sizeof(*signal_op));
            signal_op->signal_type = SIGNAL_TYPE_FENCE;
            signal_op->fence = fence;

            list_add_tail(&queue->signal_ops, &signal_op->entry);
        }

        pthread_cond_signal(&queue->signaller_cond);
        pthread_mutex_unlock(&queue->signaller_mutex);

        if (vr != VK_SUCCESS)
        {
            fprintf(stderr, "winevulkan/virtual_queue_worker: queue submission failed with %d, treating as DEVICE_LOST.\n", vr);
            pthread_mutex_lock(&queue->submissions_mutex);
            queue->device_lost = device_lost = true;
            pthread_mutex_unlock(&queue->submissions_mutex);

            if ((fence = wine_fence_from_handle(submit_unit->fence)))
            {
                uint64_t buf = 1;
                assert( write(fence->eventfd, &buf, sizeof(buf)) != -1 );
            }
        }

free_submit_unit:
        if (submit_unit->submits)
            free_copied_VkSubmitInfo(submit_unit->submits, submit_unit->submit_count);
        else
            free_copied_VkSubmitInfo2(submit_unit->submits2, submit_unit->submit_count);
        free(submit_unit->waits);
        free(submit_unit);

        pthread_mutex_lock(&queue->submissions_mutex);
        if (list_empty(&queue->submissions))
        {
            queue->processing = false;
        }
        pthread_cond_signal(&queue->submissions_cond);
        pthread_mutex_unlock(&queue->submissions_mutex);
    }

    return NULL;
}

static void init_virtual_queue(struct VkQueue_T *queue)
{
    if (is_virtual_queue(queue))
        return;

    pthread_mutex_lock(&queue->submissions_mutex);

    if (queue->virtual_queue)
    {
        pthread_mutex_unlock(&queue->submissions_mutex);
        return;
    }

    __atomic_store_n(&queue->virtual_queue, 1, __ATOMIC_RELEASE);

    pthread_create(&queue->virtual_queue_thread, NULL, virtual_queue_worker, queue);
    pthread_create(&queue->signal_thread, NULL, queue_signaller_worker, queue);

    queue->virtual_queue = true;

    pthread_mutex_unlock(&queue->submissions_mutex);
}

static NTSTATUS virtual_queue_submit(struct VkQueue_T *queue, uint32_t submit_count, const VkSubmitInfo *submits, VkFence fence)
{
    VkTimelineSemaphoreSubmitInfo *timeline_submit_info, *host_timeline_values;
    VkD3D12FenceSubmitInfoKHR *d3d12_submit_info;
    struct queue_submit_unit *submit_unit;
    struct wine_semaphore *sem;
    unsigned int i, j, k;
    uint64_t wait_value;
    bool device_lost;

    init_virtual_queue(queue);

    pthread_mutex_lock(&queue->submissions_mutex);
    device_lost = queue->device_lost;
    pthread_mutex_unlock(&queue->submissions_mutex);
    if (device_lost)
        return VK_ERROR_DEVICE_LOST;

    submit_unit = malloc(sizeof(*submit_unit));
    submit_unit->submit_count = submit_count;
    submit_unit->submits = copy_VkSubmitInfo(submits, submit_count);
    submit_unit->submits2 = NULL;
    submit_unit->fence = fence;
    submit_unit->waits = NULL;
    submit_unit->khr = queue->device->phys_dev->api_version < VK_API_VERSION_1_2 ||
                       queue->device->phys_dev->instance->api_version < VK_API_VERSION_1_2;

    /* As D3D12 fences are rewindable, we add the wait synchronously as not to miss a temporarily signalled value
     between vkQueueSubmit and processing the submit unit*/
    for (i = 0, k = 0; i < submit_count; i++)
    {
        timeline_submit_info = wine_vk_find_struct(&submits[i], TIMELINE_SEMAPHORE_SUBMIT_INFO);
        d3d12_submit_info = wine_vk_find_struct(&submits[i], D3D12_FENCE_SUBMIT_INFO_KHR);

        host_timeline_values = wine_vk_find_struct(&submit_unit->submits[i], TIMELINE_SEMAPHORE_SUBMIT_INFO);

        if (d3d12_submit_info && !host_timeline_values)
        {
            host_timeline_values = malloc(sizeof(*host_timeline_values));

            host_timeline_values->sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
            host_timeline_values->pNext = submit_unit->submits[i].pNext;
            host_timeline_values->waitSemaphoreValueCount = d3d12_submit_info->waitSemaphoreValuesCount;
            host_timeline_values->pWaitSemaphoreValues =
                    memdup(d3d12_submit_info->pWaitSemaphoreValues, d3d12_submit_info->waitSemaphoreValuesCount,
                        sizeof(host_timeline_values->pWaitSemaphoreValues[0]));
            host_timeline_values->signalSemaphoreValueCount = d3d12_submit_info->signalSemaphoreValuesCount;
            host_timeline_values->pSignalSemaphoreValues =
                    memdup(d3d12_submit_info->pSignalSemaphoreValues, d3d12_submit_info->signalSemaphoreValuesCount,
                        sizeof(host_timeline_values->pSignalSemaphoreValues[0]));

            submit_unit->submits[i].pNext = host_timeline_values;
        }

        for (j = 0; j < submits[i].waitSemaphoreCount; j++)
        {
            sem = wine_semaphore_from_handle(submits[i].pWaitSemaphores[j]);

            if (sem->handle_type != VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D12_FENCE_BIT)
                continue;

            if (timeline_submit_info)
                wait_value = timeline_submit_info->pWaitSemaphoreValues[j];
            else
                wait_value = d3d12_submit_info->pWaitSemaphoreValues[j];

            submit_unit->waits = realloc(submit_unit->waits, (k + 1) * sizeof(*submit_unit->waits));
            submit_unit->waits[k] = NULL;

            d3d12_semaphore_lock(sem);

            if ((((uint64_t*)host_timeline_values->pWaitSemaphoreValues)[j] =
                                d3d12_semaphore_try_get_wait_value_locked(sem, wait_value, queue)) == -1)
                submit_unit->waits[k] = d3d12_semaphore_push_wait_locked(sem, wait_value);

            d3d12_semaphore_unlock(sem);
            k++;
        }
    }

    pthread_mutex_lock(&queue->submissions_mutex);
    queue->processing = true;
    if (fence)
    {
        wine_fence_from_handle(fence)->queue = queue;
        wine_fence_from_handle(fence)->wait_assist = true;
    }
    list_add_tail(&queue->submissions, &submit_unit->entry);
    pthread_cond_signal(&queue->submissions_cond);
    pthread_mutex_unlock(&queue->submissions_mutex);

    return VK_SUCCESS;
}

NTSTATUS wine_vkQueueSubmit(void *args)
{
    struct vkQueueSubmit_params *params = args;
    VkQueue queue = params->queue;
    uint32_t submit_count = params->submitCount;
    const VkSubmitInfo *submits = params->pSubmits;
    VkFence fence = params->fence;

    unsigned int i, k;

    TRACE("(%p %u %p 0x%s)\n", queue, submit_count, submits, wine_dbgstr_longlong(fence));

    if (is_virtual_queue(queue))
        return virtual_queue_submit(queue, submit_count, submits, fence);

    for (i = 0; i < submit_count; i++)
    {
        if (wine_vk_find_struct(&submits[i], WIN32_KEYED_MUTEX_ACQUIRE_RELEASE_INFO_KHR))
            FIXME("VkWin32KeyedMutexAcquireReleaseInfoKHR structure unhandled.\n");

        for (k = 0; k < submits[i].waitSemaphoreCount; k++)
        {
            if (wine_semaphore_from_handle(submits[i].pWaitSemaphores[k])->handle_type ==
                    VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D12_FENCE_BIT)
                return virtual_queue_submit(queue, submit_count, submits, fence);
        }

        for (k = 0; k < submits[i].signalSemaphoreCount; k++)
        {
            if (wine_semaphore_from_handle(submits[i].pSignalSemaphores[k])->handle_type ==
                    VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D12_FENCE_BIT)
                return virtual_queue_submit(queue, submit_count, submits, fence);
        }
    }

    if (fence)
        wine_fence_from_handle(fence)->queue = queue;

    return thunk_vkQueueSubmit(queue, submit_count, submits, fence);
}

static NTSTATUS virtual_queue_submit2(struct VkQueue_T *queue, uint32_t submit_count, const VkSubmitInfo2 *submits, VkFence fence, bool khr)
{
    VkSemaphoreSubmitInfo *sem_submit_info;
    struct queue_submit_unit *submit_unit;
    VkSubmitInfo2 *queue_submit;
    struct wine_semaphore *sem;
    unsigned int i, j, k;
    uint64_t wait_value;
    bool device_lost;

    init_virtual_queue(queue);

    pthread_mutex_lock(&queue->submissions_mutex);
    device_lost = queue->device_lost;
    pthread_mutex_unlock(&queue->submissions_mutex);
    if (device_lost)
        return VK_ERROR_DEVICE_LOST;

    submit_unit = malloc(sizeof(*submit_unit));
    submit_unit->submit_count = submit_count;
    submit_unit->submits = NULL;
    submit_unit->submits2 = copy_VkSubmitInfo2(submits, submit_count);
    submit_unit->fence = fence;
    submit_unit->waits = NULL;
    submit_unit->khr = khr;

    /* As D3D12 fences are rewindable, we add the wait synchronously as not to miss a temporarily signalled value
     between vkQueueSubmit and processing the submit unit */
    for (i = 0, k = 0; i < submit_count; i++)
    {
        queue_submit = &submit_unit->submits2[i];

        for (j = 0; j < queue_submit->waitSemaphoreInfoCount; j++)
        {
            sem_submit_info = (VkSemaphoreSubmitInfo *) &queue_submit->pWaitSemaphoreInfos[j];
            sem = wine_semaphore_from_handle(sem_submit_info->semaphore);

            if (sem->handle_type != VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D12_FENCE_BIT)
                continue;

            wait_value = sem_submit_info->value;

            submit_unit->waits = realloc(submit_unit->waits, (k + 1) * sizeof(*submit_unit->waits));
            submit_unit->waits[k] = NULL;

            d3d12_semaphore_lock(sem);

            if ((sem_submit_info->value =
                                d3d12_semaphore_try_get_wait_value_locked(sem, wait_value, queue)) == -1)
                submit_unit->waits[k] = d3d12_semaphore_push_wait_locked(sem, wait_value);

            d3d12_semaphore_unlock(sem);
            k++;
        }
    }

    pthread_mutex_lock(&queue->submissions_mutex);
    queue->processing = true;
    if (fence)
    {
        wine_fence_from_handle(fence)->queue = queue;
        wine_fence_from_handle(fence)->wait_assist = true;
    }
    list_add_tail(&queue->submissions, &submit_unit->entry);
    pthread_cond_signal(&queue->submissions_cond);
    pthread_mutex_unlock(&queue->submissions_mutex);

    return VK_SUCCESS;
}

static NTSTATUS vk_queue_submit_2(VkQueue queue, uint32_t submit_count, const VkSubmitInfo2 *submits, VkFence fence, bool khr)
{
    unsigned int i, k;

    TRACE("(%p, %u, %p, %s)\n", queue, submit_count, submits, wine_dbgstr_longlong(fence));

    if (is_virtual_queue(queue))
        return virtual_queue_submit2(queue, submit_count, submits, fence, khr);

    for (i = 0; i < submit_count; i++)
    {
        if (wine_vk_find_struct(&submits[i], WIN32_KEYED_MUTEX_ACQUIRE_RELEASE_INFO_KHR))
            FIXME("VkWin32KeyedMutexAcquireReleaseInfoKHR structure unhandled.\n");

        for (k = 0; k < submits[i].waitSemaphoreInfoCount; k++)
        {
            if (wine_semaphore_from_handle(submits[i].pWaitSemaphoreInfos[k].semaphore)->handle_type ==
                    VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D12_FENCE_BIT)
                return virtual_queue_submit2(queue, submit_count, submits, fence, khr);
        }

        for (k = 0; k < submits[i].signalSemaphoreInfoCount; k++)
        {
            if (wine_semaphore_from_handle(submits[i].pSignalSemaphoreInfos[k].semaphore)->handle_type ==
                    VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D12_FENCE_BIT)
                return virtual_queue_submit2(queue, submit_count, submits, fence, khr);
        }
    }

    if (fence)
        wine_fence_from_handle(fence)->queue = queue;

    if (khr)
        return thunk_vkQueueSubmit2KHR(queue, submit_count, submits, fence);
    else
        return thunk_vkQueueSubmit2(queue, submit_count, submits, fence);
}

NTSTATUS wine_vkQueueSubmit2(void *args)
{
    struct vkQueueSubmit2_params *params = args;
    VkQueue queue = params->queue;
    uint32_t submit_count = params->submitCount;
    const VkSubmitInfo2 *submits = params->pSubmits;
    VkFence fence = params->fence;

    return vk_queue_submit_2(queue, submit_count, submits, fence, false);
}

NTSTATUS wine_vkQueueSubmit2KHR(void *args)
{
    struct vkQueueSubmit2_params *params = args;
    VkQueue queue = params->queue;
    uint32_t submit_count = params->submitCount;
    const VkSubmitInfo2 *submits = params->pSubmits;
    VkFence fence = params->fence;

    return vk_queue_submit_2(queue, submit_count, submits, fence, true);
}

static inline VkSemaphore *convert_VkSemaphore_array_win_to_host(const VkSemaphore *in, uint32_t count)
{
    VkSemaphore *out;
    unsigned int i;

    if (!in || !count) return NULL;

    out = malloc(count * sizeof(*out));
    for (i = 0; i < count; i++)
    {
        out[i] = in[i] ? wine_semaphore_from_handle(in[i])->semaphore : VK_NULL_HANDLE;
    }

    return out;
}

static inline void free_VkSemaphore_array(VkSemaphore *in, uint32_t count)
{
    if (!in) return;

    free(in);
}

NTSTATUS wine_vkQueuePresentKHR(void *args)
{
    struct vkQueuePresentKHR_params *params = args;
    VkQueue queue = params->queue;
    const VkPresentInfoKHR *present_info = params->pPresentInfo;

    VkPresentInfoKHR host_present_info = *present_info;
    struct wine_semaphore *semaphore;
    VkSemaphore *host_semaphores;
    unsigned int i;
    VkResult vr;

    TRACE("%p %p\n", queue, present_info);

    for (i = 0; i < present_info->waitSemaphoreCount; i++)
    {
        semaphore = wine_semaphore_from_handle(present_info->pWaitSemaphores[i]);

        if (semaphore->handle_type == VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D12_FENCE_BIT)
        {
            FIXME("Waiting on D3D12-Fence compatible timeline semaphore not supported.\n");
            return VK_ERROR_OUT_OF_HOST_MEMORY;
        }
    }

    if (is_virtual_queue(queue))
    {
        pthread_mutex_lock(&queue->submissions_mutex);
        while (queue->processing)
            pthread_cond_wait(&queue->submissions_cond, &queue->submissions_mutex);
        pthread_mutex_unlock(&queue->submissions_mutex);
    }

    host_present_info.pWaitSemaphores = host_semaphores = convert_VkSemaphore_array_win_to_host(present_info->pWaitSemaphores, present_info->waitSemaphoreCount);
    vr = fshack_vk_queue_present(queue, &host_present_info);
    free_VkSemaphore_array(host_semaphores, present_info->waitSemaphoreCount);
    return vr;
}

NTSTATUS wine_vkQueueBindSparse(void *args)
{
    struct vkQueueBindSparse_params *params = args;
    VkQueue queue = params->queue;
    uint32_t bind_info_count = params->bindInfoCount;
    const VkBindSparseInfo *bind_info = params->pBindInfo;
    VkFence fence = params->fence;

    struct wine_semaphore *semaphore;
    const VkBindSparseInfo *batch;
    unsigned int i, k;

    TRACE("(%p, %u, %p, 0x%s)\n", queue, bind_info_count, bind_info, wine_dbgstr_longlong(fence));

    if (is_virtual_queue(queue))
    {
        FIXME("Can't process sparse bind calls on virtual queue, flushing.\n");
        pthread_mutex_lock(&queue->submissions_mutex);
        while (queue->processing)
            pthread_cond_wait(&queue->submissions_cond, &queue->submissions_mutex);
        pthread_mutex_unlock(&queue->submissions_mutex);
    }

    for (i = 0; i < bind_info_count; i++)
    {
        batch = &bind_info[i];

        for (k = 0; k < batch->waitSemaphoreCount; k++)
        {
            semaphore = wine_semaphore_from_handle(batch->pWaitSemaphores[k]);

            if (semaphore->handle_type == VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D12_FENCE_BIT)
            {
                FIXME("Waiting on D3D12-Fence compatible timeline semaphore not supported.\n");
                return VK_ERROR_OUT_OF_HOST_MEMORY;
            }
        }

        for(k = 0; k < batch->signalSemaphoreCount; k++)
        {
            semaphore = wine_semaphore_from_handle(batch->pSignalSemaphores[k]);

            if (semaphore->handle_type == VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D12_FENCE_BIT)
            {
                FIXME("Signalling D3D12-Fence compatible timeline semaphore not supported.\n");
                return VK_ERROR_OUT_OF_HOST_MEMORY;
            }
        }
    }

    return thunk_vkQueueBindSparse(queue, bind_info_count, bind_info, fence);
}

NTSTATUS wine_vkCreateFence(void *args)
{
    struct vkCreateFence_params *params = args;
    VkDevice device = params->device;
    const VkFenceCreateInfo *create_info = params->pCreateInfo;
    const VkAllocationCallbacks *allocator = params->pAllocator;
    VkFence *fence = params->pFence;

    struct wine_fence *object;
    VkResult vr;

    TRACE("(%p, %p, %p, %p)\n", device, create_info, allocator, fence);

    if (allocator)
        FIXME("Support for allocation callbacks not implemented yet\n");

    if (!(object = calloc(1, sizeof(*object))))
        return VK_ERROR_OUT_OF_HOST_MEMORY;

    if ((object->eventfd = eventfd(0, EFD_CLOEXEC)) == -1)
        ERR("Failed to create eventfd for fence.\n");

    if ((vr = device->funcs.p_vkCreateFence(device->device, create_info, allocator, &object->fence)) == VK_SUCCESS)
        *fence = wine_fence_to_handle(object);
    else
        free(object);

    return vr;
}

NTSTATUS wine_vkDestroyFence(void *args)
{
    struct vkDestroyFence_params *params = args;
    VkDevice device = params->device;
    VkFence handle = params->fence;
    const VkAllocationCallbacks *allocator = params->pAllocator;

    struct wine_fence *fence = wine_fence_from_handle(handle);

    TRACE("(%p, %p, %p)\n", device, fence, allocator);

    if (allocator)
        FIXME("Support for allocation callbacks not implemented yet\n");

    if (!handle)
        return STATUS_SUCCESS;

    if (fence->eventfd != -1)
        close(fence->eventfd);

    device->funcs.p_vkDestroyFence(device->device, fence->fence, allocator);
    free(fence);

    return STATUS_SUCCESS;
}

NTSTATUS wine_vkResetFences(void *args)
{
    struct vkResetFences_params *params = args;
    VkDevice device = params->device;
    uint32_t fence_count = params->fenceCount;
    const VkFence *fences = params->pFences;

    struct wine_fence *fence;
    unsigned int i;
    uint64_t buf;
    VkResult vr;

    TRACE("(%p, %u, %p)\n", device, fence_count, fences);

    if ((vr = thunk_vkResetFences(device, fence_count, fences)) != VK_SUCCESS)
        return vr;

    for (i = 0; i < fence_count; i++)
    {
        fence = wine_fence_from_handle(fences[i]);

        fence->queue = NULL;
        fence->swapchain = NULL;
        if (fence->wait_assist)
        {
            fence->wait_assist = false;
            if (read(fence->eventfd, &buf, sizeof(buf)) == -1)
                ERR("Failed to reset event fd.\n");
        }
    }

    return VK_SUCCESS;
}

NTSTATUS wine_vkWaitForFences(void *args)
{
    struct vkWaitForFences_params *params = args;
    VkDevice device = params->device;
    uint32_t fence_count = params->fenceCount;
    const VkFence *fences = params->pFences;
    VkBool32 wait_all = params->waitAll;
    uint64_t timeout = params->timeout;

    struct signal_op *signal_op;
    bool assisted_wait = false;
    struct wine_fence *fence;
    struct pollfd *wait_fds;
    struct pollfd wait_fd;
    unsigned int i;
    VkResult vr;
    int ret;

    TRACE("(%p, %u, %p, %u, 0x%s)\n", device, fence_count, fences, wait_all, wine_dbgstr_longlong(timeout));

    for (i = 0; i < fence_count; i++)
    {
        fence = wine_fence_from_handle(fences[i]);
        if (!fence->wait_assist)
            continue;

        if (!wait_all && fence_count > 1)
        {
            assisted_wait = true;
            break;
        }

        wait_fd.fd = fence->eventfd;
        wait_fd.events = POLLIN;
        ret = poll(&wait_fd, 1, timeout / 1000000);
        if (ret == -1)
        {
            ERR("Failed to poll wait assisted fence.\n");
            return VK_ERROR_OUT_OF_HOST_MEMORY;
        }
        if (!ret)
            return VK_TIMEOUT;

        if (wait_fd.revents & (POLLERR | POLLHUP | POLLNVAL))
            ERR("Polling on fd %d returned %#x.", fence->eventfd, wait_fd.revents);
        return VK_SUCCESS;
    }

    if (assisted_wait)
    {
        /* Turn all non assisted waits into assisted waits, then poll on all */
        wait_fds = malloc( sizeof(wait_fds[0]) * fence_count );

        for (i = 0; i < fence_count; i++)
        {
            if (!fence->wait_assist)
            {
                assert(fence->queue || fence->swapchain);

                if (fence->queue)
                {
                    fence->wait_assist = true;

                    /* If virtual-queue requiring work was submitted after the work signalling this mutex,
                     * we will end up unnecessarily waiting on that work first,
                     * but this will only happen once per queue */
                    init_virtual_queue(fence->queue);

                    signal_op = malloc(sizeof(*signal_op));
                    signal_op->signal_type = SIGNAL_TYPE_FENCE;
                    signal_op->fence = fence;

                    pthread_mutex_lock(&fence->queue->signaller_mutex);
                    list_add_tail(&fence->queue->signal_ops, &signal_op->entry);
                    pthread_cond_signal(&fence->queue->signaller_cond);
                    pthread_mutex_unlock(&fence->queue->signaller_mutex);
                }
                else
                {
                    FIXME("Wait assist for swapchain signaled fences not supported.\n");
                    free(wait_fds);
                    return VK_ERROR_OUT_OF_HOST_MEMORY;
                }
            }

            wait_fds[i].fd = fence->eventfd;
            wait_fds[i].events = POLLIN;
        }

        if (poll(wait_fds, fence_count, timeout / 1000000) == -1)
        {
            ERR("Failed to poll wait assisted fences.\n");
            vr = VK_ERROR_OUT_OF_HOST_MEMORY;
        }

        free(wait_fds);
    }
    else
    {
        vr = thunk_vkWaitForFences(device, fence_count, fences, wait_all, timeout);
    }

    return vr;
}

NTSTATUS wine_vkQueueWaitIdle(void *args)
{
    struct vkQueueWaitIdle_params *params = args;
    VkQueue queue = params->queue;

    TRACE("(%p)\n", queue);

    if (is_virtual_queue(queue))
    {
        pthread_mutex_lock(&queue->submissions_mutex);
        while (queue->processing)
            pthread_cond_wait(&queue->submissions_cond, &queue->submissions_mutex);
        pthread_mutex_unlock(&queue->submissions_mutex);
    }

    return queue->device->funcs.p_vkQueueWaitIdle(queue->queue);
}
