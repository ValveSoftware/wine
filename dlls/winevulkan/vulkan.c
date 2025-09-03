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
#include <time.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>
#include <stdio.h>
#include <assert.h>
#include <limits.h>
#ifdef HAVE_SYS_SYSCALL_H
# include <sys/syscall.h>
#endif

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winnt.h"
#include "winioctl.h"
#include "wine/server.h"
#include "wine/list.h"

#include "vulkan_private.h"
#include "wine/vulkan_driver.h"
#include "wine/rbtree.h"
#include "ntgdi.h"
#include "ntuser.h"

WINE_DEFAULT_DEBUG_CHANNEL(vulkan);

static int debug_level;

static PFN_vkCreateInstance p_vkCreateInstance;
static PFN_vkEnumerateInstanceVersion p_vkEnumerateInstanceVersion;
static PFN_vkEnumerateInstanceExtensionProperties p_vkEnumerateInstanceExtensionProperties;

static struct wine_instance *wine_instance_from_handle(VkInstance handle)
{
    struct vulkan_instance *object = vulkan_instance_from_handle(handle);
    return CONTAINING_RECORD(object, struct wine_instance, obj);
}

static struct wine_phys_dev *wine_phys_dev_from_handle(VkPhysicalDevice handle)
{
    struct vulkan_physical_device *object = vulkan_physical_device_from_handle(handle);
    return CONTAINING_RECORD(object, struct wine_phys_dev, obj);
}

static struct wine_semaphore *wine_semaphore_from_handle(VkSemaphore handle)
{
    struct vulkan_semaphore *object = vulkan_semaphore_from_handle(handle);
    return CONTAINING_RECORD(object, struct wine_semaphore, obj);
}

static void vulkan_object_init_ptr( struct vulkan_object *obj, UINT64 host_handle, struct vulkan_client_object *client )
{
    obj->host_handle = host_handle;
    obj->client_handle = (UINT_PTR)client;
    client->unix_handle = (UINT_PTR)obj;
}

static BOOL is_wow64(void)
{
    return sizeof(void *) == sizeof(UINT64) && NtCurrentTeb()->WowTebOffset;
}

static BOOL use_external_memory(void)
{
    return is_wow64();
}

static ULONG_PTR zero_bits = 0;

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

const struct vulkan_funcs *vk_funcs;

#define wine_vk_find_unlink_struct(s, t) wine_vk_find_unlink_struct_((void *)s, VK_STRUCTURE_TYPE_##t)
static void *wine_vk_find_unlink_struct_(void *s, VkStructureType t)
{
    VkBaseInStructure *prev = s;
    VkBaseInStructure *header;

    for (header = (VkBaseInStructure *)prev->pNext; header; prev = header, header = (VkBaseInStructure *)header->pNext)
    {
        if (header->sType == t) {
            prev->pNext = header->pNext;
            header->pNext = NULL;
            return header;
        }
    }

    return NULL;
}

static int vulkan_object_compare(const void *key, const struct rb_entry *entry)
{
    struct vulkan_object *object = RB_ENTRY_VALUE(entry, struct vulkan_object, entry);
    const uint64_t *host_handle = key;
    if (*host_handle < object->host_handle) return -1;
    if (*host_handle > object->host_handle) return 1;
    return 0;
}

static void vulkan_instance_insert_object( struct vulkan_instance *instance, struct vulkan_object *obj )
{
    struct wine_instance *impl = CONTAINING_RECORD(instance, struct wine_instance, obj);
    if (impl->objects.compare)
    {
        pthread_rwlock_wrlock( &impl->objects_lock );
        rb_put( &impl->objects, &obj->host_handle, &obj->entry );
        pthread_rwlock_unlock( &impl->objects_lock );
    }
}

static void vulkan_instance_remove_object( struct vulkan_instance *instance, struct vulkan_object *obj )
{
    struct wine_instance *impl = CONTAINING_RECORD(instance, struct wine_instance, obj);
    if (impl->objects.compare)
    {
        pthread_rwlock_wrlock( &impl->objects_lock );
        rb_remove( &impl->objects, &obj->entry );
        pthread_rwlock_unlock( &impl->objects_lock );
    }
}

static uint64_t client_handle_from_host(struct vulkan_instance *obj, uint64_t host_handle)
{
    struct wine_instance *instance = CONTAINING_RECORD(obj, struct wine_instance, obj);
    struct rb_entry *entry;
    uint64_t result = 0;

    pthread_rwlock_rdlock(&instance->objects_lock);
    if ((entry = rb_get(&instance->objects, &host_handle)))
    {
        struct vulkan_object *object = RB_ENTRY_VALUE(entry, struct vulkan_object, entry);
        result = object->client_handle;
    }
    pthread_rwlock_unlock(&instance->objects_lock);
    return result;
}

struct vk_callback_funcs callback_funcs;

static UINT append_string(const char *name, char *strings, UINT *strings_len)
{
    UINT len = name ? strlen(name) + 1 : 0;
    if (strings && len) memcpy(strings + *strings_len, name, len);
    *strings_len += len;
    return len;
}

static void append_debug_utils_label(const VkDebugUtilsLabelEXT *label, struct debug_utils_label *dst,
        char *strings, UINT *strings_len)
{
    if (label->pNext) FIXME("Unsupported VkDebugUtilsLabelEXT pNext chain\n");
    memcpy(dst->color, label->color, sizeof(dst->color));
    dst->label_name_len = append_string(label->pLabelName, strings, strings_len);
}

static void append_debug_utils_object(const VkDebugUtilsObjectNameInfoEXT *object, struct debug_utils_object *dst,
        char *strings, UINT *strings_len)
{
    if (object->pNext) FIXME("Unsupported VkDebugUtilsObjectNameInfoEXT pNext chain\n");
    dst->object_type = object->objectType;
    dst->object_handle = object->objectHandle;
    dst->object_name_len = append_string(object->pObjectName, strings, strings_len);
}

static void signal_timeline_sem(struct vulkan_device *device, VkSemaphore sem, UINT64 *value)
{
    /* May be called from native thread. */
    struct VkSemaphoreSignalInfo info = { 0 };
    VkResult res;

    info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO;
    info.semaphore = sem;
    info.value = *value + 1;
    __atomic_store_n(value, info.value, __ATOMIC_RELEASE);
    if (device->physical_device->api_version < VK_API_VERSION_1_2 || device->physical_device->instance->api_version < VK_API_VERSION_1_2)
        res = device->p_vkSignalSemaphoreKHR(device->host.device, &info);
    else
        res = device->p_vkSignalSemaphore(device->host.device, &info);
    if (res != VK_SUCCESS)
        fprintf(stderr, "err:winevulkan:signal_timeline_sem vkSignalSemaphore failed, res=%d.\n", res);
}

static VkResult wait_semaphores(struct vulkan_device *device, const VkSemaphoreWaitInfo *wait_info, uint64_t timeout)
{
    if (device->physical_device->api_version < VK_API_VERSION_1_2 || device->physical_device->instance->api_version < VK_API_VERSION_1_2)
        return device->p_vkWaitSemaphoresKHR(device->host.device, wait_info, timeout);
    return device->p_vkWaitSemaphores(device->host.device, wait_info, timeout);
}

static VkResult get_semaphore_value(struct vulkan_device *device, VkSemaphore sem, uint64_t *value)
{
    if (device->physical_device->api_version < VK_API_VERSION_1_2 || device->physical_device->instance->api_version < VK_API_VERSION_1_2)
        return device->p_vkGetSemaphoreCounterValueKHR(device->host.device, sem, value);
    return device->p_vkGetSemaphoreCounterValue(device->host.device, sem, value);
}


static void set_transient_client_handle(struct wine_instance *instance, uint64_t client_handle)
{
    uint64_t *handle = pthread_getspecific(instance->transient_object_handle);
    if (!handle)
    {
        handle = malloc(sizeof(uint64_t));
        pthread_setspecific(instance->transient_object_handle, handle);
    }
    *handle = client_handle;
}

static uint64_t get_transient_handle(struct wine_instance *instance)
{
    uint64_t *handle = pthread_getspecific(instance->transient_object_handle);
    return handle && *handle;
}


static VkBool32 debug_utils_callback_conversion(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT message_types,
    const VkDebugUtilsMessengerCallbackDataEXT *callback_data,
    void *user_data)
{
    const VkDeviceAddressBindingCallbackDataEXT *address = NULL;
    struct wine_vk_debug_utils_params *params;
    struct wine_debug_utils_messenger *object;
    struct debug_utils_object dummy_object, *objects;
    struct debug_utils_label dummy_label, *labels;
    VkInstance instance;
    struct wine_instance *wine_instance;
    UINT size, strings_len;
    char *ptr, *strings;
    ULONG ret_len;
    void *ret_ptr;
    unsigned int i;

    TRACE("%i, %u, %p, %p\n", severity, message_types, callback_data, user_data);

    object = user_data;
    instance = object->instance->host.instance;

    if (!instance)
    {
        /* instance wasn't yet created, this is a message from the host loader */
        return VK_FALSE;
    }

    wine_instance = CONTAINING_RECORD(object->instance, struct wine_instance, obj);

    if ((address = callback_data->pNext))
    {
        if (address->sType != VK_STRUCTURE_TYPE_DEVICE_ADDRESS_BINDING_CALLBACK_DATA_EXT) address = NULL;
        if (!address || address->pNext) FIXME("Unsupported VkDebugUtilsMessengerCallbackDataEXT pNext chain\n");
    }

    strings_len = 0;
    append_string(callback_data->pMessageIdName, NULL, &strings_len);
    append_string(callback_data->pMessage, NULL, &strings_len);
    for (i = 0; i < callback_data->queueLabelCount; i++)
        append_debug_utils_label(callback_data->pQueueLabels + i, &dummy_label, NULL, &strings_len);
    for (i = 0; i < callback_data->cmdBufLabelCount; i++)
        append_debug_utils_label(callback_data->pCmdBufLabels + i, &dummy_label, NULL, &strings_len);
    for (i = 0; i < callback_data->objectCount; i++)
        append_debug_utils_object(callback_data->pObjects + i, &dummy_object, NULL, &strings_len);

    size = sizeof(*params);
    size += sizeof(*labels) * (callback_data->queueLabelCount + callback_data->cmdBufLabelCount);
    size += sizeof(*objects) * callback_data->objectCount;

    if (!(params = malloc(size + strings_len))) return VK_FALSE;
    ptr = (char *)(params + 1);
    strings = (char *)params + size;

    params->dispatch.callback = callback_funcs.call_vulkan_debug_utils_callback;
    params->user_callback = object->user_callback;
    params->user_data = object->user_data;
    params->severity = severity;
    params->message_types = message_types;
    params->flags = callback_data->flags;
    params->message_id_number = callback_data->messageIdNumber;

    strings_len = 0;
    params->message_id_name_len = append_string(callback_data->pMessageIdName, strings, &strings_len);
    params->message_len = append_string(callback_data->pMessage, strings, &strings_len);

    labels = (void *)ptr;
    for (i = 0; i < callback_data->queueLabelCount; i++)
        append_debug_utils_label(callback_data->pQueueLabels + i, labels + i, strings, &strings_len);
    params->queue_label_count = callback_data->queueLabelCount;
    ptr += callback_data->queueLabelCount * sizeof(*labels);

    labels = (void *)ptr;
    for (i = 0; i < callback_data->cmdBufLabelCount; i++)
        append_debug_utils_label(callback_data->pCmdBufLabels + i, labels + i, strings, &strings_len);
    params->cmd_buf_label_count = callback_data->cmdBufLabelCount;
    ptr += callback_data->cmdBufLabelCount * sizeof(*labels);

    objects = (void *)ptr;
    for (i = 0; i < callback_data->objectCount; i++)
    {
        append_debug_utils_object(callback_data->pObjects + i, objects + i, strings, &strings_len);

        if (wine_vk_is_type_wrapped(objects[i].object_type))
        {
            objects[i].object_handle = client_handle_from_host(object->instance, objects[i].object_handle);
            if (!objects[i].object_handle)
                objects[i].object_handle = get_transient_handle(wine_instance);
            if (!objects[i].object_handle)
            {
                WARN("handle conversion failed 0x%s\n", wine_dbgstr_longlong(callback_data->pObjects[i].objectHandle));
                free(params);
                return VK_FALSE;
            }
        }
    }
    params->object_count = callback_data->objectCount;
    ptr += callback_data->objectCount * sizeof(*objects);

    if (address)
    {
        params->has_address_binding = TRUE;
        params->address_binding.flags = address->flags;
        params->address_binding.base_address = address->baseAddress;
        params->address_binding.size = address->size;
        params->address_binding.binding_type = address->bindingType;
    }

    /* applications should always return VK_FALSE */
    KeUserDispatchCallback(&params->dispatch, size + strings_len, &ret_ptr, &ret_len);
    free(params);

    if (ret_len == sizeof(VkBool32)) return *(VkBool32 *)ret_ptr;
    return VK_FALSE;
}

static VkBool32 debug_report_callback_conversion(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT object_type,
    uint64_t object_handle, size_t location, int32_t code, const char *layer_prefix, const char *message, void *user_data)
{
    struct wine_vk_debug_report_params *params;
    struct wine_debug_report_callback *object;
    UINT strings_len;
    ULONG ret_len;
    void *ret_ptr;
    char *strings;

    TRACE("%#x, %#x, 0x%s, 0x%s, %d, %p, %p, %p\n", flags, object_type, wine_dbgstr_longlong(object_handle),
        wine_dbgstr_longlong(location), code, layer_prefix, message, user_data);

    object = user_data;

    if (!object->instance->host.instance)
    {
        /* instance wasn't yet created, this is a message from the host loader */
        return VK_FALSE;
    }

    strings_len = 0;
    append_string(layer_prefix, NULL, &strings_len);
    append_string(message, NULL, &strings_len);

    if (!(params = malloc(sizeof(*params) + strings_len))) return VK_FALSE;
    strings = (char *)(params + 1);

    params->dispatch.callback = callback_funcs.call_vulkan_debug_report_callback;
    params->user_callback = object->user_callback;
    params->user_data = object->user_data;
    params->flags = flags;
    params->object_type = object_type;
    params->location = location;
    params->code = code;
    params->object_handle = client_handle_from_host(object->instance, object_handle);
    if (!params->object_handle) params->object_type = VK_DEBUG_REPORT_OBJECT_TYPE_UNKNOWN_EXT;

    strings_len = 0;
    params->layer_len = append_string(layer_prefix, strings, &strings_len);
    params->message_len = append_string(message, strings, &strings_len);

    KeUserDispatchCallback(&params->dispatch, sizeof(*params) + strings_len, &ret_ptr, &ret_len);
    free(params);

    if (ret_len == sizeof(VkBool32)) return *(VkBool32 *)ret_ptr;
    return VK_FALSE;
}

static void wine_phys_dev_cleanup(struct wine_phys_dev *phys_dev)
{
    free(phys_dev->extensions);
}

static VkResult wine_vk_physical_device_init(struct wine_phys_dev *object, VkPhysicalDevice host_physical_device,
        VkPhysicalDevice client_physical_device, struct vulkan_instance *instance)
{
    BOOL have_memory_placed = FALSE, have_map_memory2 = FALSE;
    uint32_t num_host_properties, num_properties = 0;
    VkExtensionProperties *host_properties = NULL;
    VkPhysicalDeviceProperties physdev_properties;
    BOOL have_external_memory_host = FALSE, have_external_memory_fd = FALSE, have_external_semaphore_fd = FALSE;
    VkResult res;
    unsigned int i, j;

    vulkan_object_init_ptr(&object->obj.obj, (UINT_PTR)host_physical_device, &client_physical_device->obj);
    object->obj.instance = instance;

    instance->p_vkGetPhysicalDeviceMemoryProperties(host_physical_device, &object->memory_properties);

    instance->p_vkGetPhysicalDeviceProperties(host_physical_device, &physdev_properties);
    object->obj.api_version = physdev_properties.apiVersion;

    res = instance->p_vkEnumerateDeviceExtensionProperties(host_physical_device,
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

    res = instance->p_vkEnumerateDeviceExtensionProperties(host_physical_device,
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
            have_external_memory_fd = TRUE;
        }
        if (!strcmp(host_properties[i].extensionName, "VK_KHR_external_semaphore_fd"))
        {
            TRACE("Substituting VK_KHR_external_semaphore_fd for VK_KHR_external_semaphore_win32\n");

            snprintf(host_properties[i].extensionName, sizeof(host_properties[i].extensionName),
                    VK_KHR_EXTERNAL_SEMAPHORE_WIN32_EXTENSION_NAME);
            host_properties[i].specVersion = VK_KHR_EXTERNAL_SEMAPHORE_WIN32_SPEC_VERSION;
            have_external_semaphore_fd = TRUE;
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
        if (!strcmp(host_properties[i].extensionName, "VK_EXT_external_memory_host"))
            have_external_memory_host = TRUE;
        else if (!strcmp(host_properties[i].extensionName, "VK_EXT_map_memory_placed"))
            have_memory_placed = TRUE;
        else if (!strcmp(host_properties[i].extensionName, "VK_KHR_map_memory2"))
            have_map_memory2 = TRUE;
    }

    if (have_external_memory_fd && have_external_semaphore_fd)
        ++num_properties; /* VK_KHR_win32_keyed_mutex */

    ++num_properties; /* VK_WINE_openxr_device_extensions */
    ++num_properties; /* VK_WINE_openvr_device_extensions */

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
    if (have_external_memory_fd && have_external_semaphore_fd)
    {
        strcpy(object->extensions[j].extensionName, VK_KHR_WIN32_KEYED_MUTEX_EXTENSION_NAME);
        object->extensions[j].specVersion = VK_KHR_WIN32_KEYED_MUTEX_SPEC_VERSION;
        TRACE("Enabling extension '%s' for physical device %p\n", object->extensions[j].extensionName, object);
        ++j;
    }

    strcpy(object->extensions[j].extensionName, "VK_WINE_openxr_device_extensions");
    TRACE("Enabling extension '%s' for physical device %p\n", object->extensions[j].extensionName, object);
    ++j;
    strcpy(object->extensions[j].extensionName, "VK_WINE_openvr_device_extensions");
    TRACE("Enabling extension '%s' for physical device %p\n", object->extensions[j].extensionName, object);
    ++j;

    object->extension_count = num_properties;
    TRACE("Host supported extensions %u, Wine supported extensions %u\n", num_host_properties, num_properties);

    if (zero_bits && have_memory_placed && have_map_memory2)
    {
        VkPhysicalDeviceMapMemoryPlacedFeaturesEXT map_placed_feature =
        {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAP_MEMORY_PLACED_FEATURES_EXT,
        };
        VkPhysicalDeviceFeatures2 features =
        {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
            .pNext = &map_placed_feature,
        };

        instance->p_vkGetPhysicalDeviceFeatures2KHR(host_physical_device, &features);
        if (map_placed_feature.memoryMapPlaced && map_placed_feature.memoryUnmapReserve)
        {
            VkPhysicalDeviceMapMemoryPlacedPropertiesEXT map_placed_props =
            {
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAP_MEMORY_PLACED_PROPERTIES_EXT,
            };
            VkPhysicalDeviceProperties2 props =
            {
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
                .pNext = &map_placed_props,
            };

            instance->p_vkGetPhysicalDeviceProperties2(host_physical_device, &props);
            object->map_placed_align = map_placed_props.minPlacedMemoryMapAlignment;
            TRACE( "Using placed map with alignment %u\n", object->map_placed_align );
        }
    }

    if (zero_bits && have_external_memory_host && !object->map_placed_align)
    {
        VkPhysicalDeviceExternalMemoryHostPropertiesEXT host_mem_props =
        {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_MEMORY_HOST_PROPERTIES_EXT,
        };
        VkPhysicalDeviceProperties2 props =
        {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
            .pNext = &host_mem_props,
        };
        instance->p_vkGetPhysicalDeviceProperties2KHR(host_physical_device, &props);
        object->external_memory_align = host_mem_props.minImportedHostPointerAlignment;
        if (object->external_memory_align)
            TRACE("Using VK_EXT_external_memory_host for memory mapping with alignment: %u\n",
                  object->external_memory_align);
    }

    free(host_properties);
    return VK_SUCCESS;

err:
    wine_phys_dev_cleanup(object);
    free(host_properties);
    return res;
}

static void wine_vk_free_command_buffers(struct vulkan_device *device,
        struct wine_cmd_pool *pool, uint32_t count, const VkCommandBuffer *buffers)
{
    struct vulkan_instance *instance = device->physical_device->instance;
    struct wine_cmd_buffer *buffer;
    unsigned int i;

    for (i = 0; i < count; i++)
    {
        if (!buffers[i])
            continue;
        buffer = wine_cmd_buffer_from_handle(buffers[i]);
        if (!buffer)
            continue;

        device->p_vkFreeCommandBuffers(device->host.device, pool->host.command_pool, 1,
                                             &buffer->host.command_buffer);
        vulkan_instance_remove_object(instance, &buffer->obj);
        buffer->client.command_buffer->obj.unix_handle = 0;
        free(buffer);
    }
}

static void wine_vk_device_init_queues(struct vulkan_device *device, const VkDeviceQueueCreateInfo *info)
{
    struct vulkan_queue *queues = device->queues + device->queue_count;
    VkQueue client_queues = device->client.device->queues + device->queue_count;
    VkDeviceQueueInfo2 queue_info;
    UINT i;

    TRACE("Queue family index %u, queue count %u.\n", info->queueFamilyIndex, info->queueCount);

    for (i = 0; i < info->queueCount; i++)
    {
        struct vulkan_queue *queue = queues + i;
        VkQueue host_queue, client_queue = client_queues + i;

        /* The Vulkan spec says:
         *
         * "vkGetDeviceQueue must only be used to get queues that were created
         * with the flags parameter of VkDeviceQueueCreateInfo set to zero."
         */
        if (info->flags && device->p_vkGetDeviceQueue2)
        {
            queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2;
            queue_info.pNext = NULL;
            queue_info.flags = info->flags;
            queue_info.queueFamilyIndex = info->queueFamilyIndex;
            queue_info.queueIndex = i;
            device->p_vkGetDeviceQueue2(device->host.device, &queue_info, &host_queue);
        }
        else
        {
            device->p_vkGetDeviceQueue(device->host.device, info->queueFamilyIndex, i, &host_queue);
        }

        vulkan_object_init_ptr(&queue->obj, (UINT_PTR)host_queue, &client_queue->obj);
        queue->device = device;
        queue->family_index = info->queueFamilyIndex;
        queue->queue_index = i;
        queue->flags = info->flags;

        TRACE("Got device %p queue %p, host_queue %p.\n", device, queue, queue->host.queue);
    }

    device->queue_count += info->queueCount;
}

static char *cc_strdup(struct conversion_context *ctx, const char *s)
{
    int len = strlen(s) + 1;
    char *ret;

    ret = conversion_context_alloc(ctx, len);
    memcpy(ret, s, len);
    return ret;
}

static const char *find_extension(const char *const *extensions, uint32_t count, const char *ext)
{
    while (count--)
    {
        if (!strcmp(extensions[count], ext))
            return extensions[count];
    }
    return NULL;
}

static void parse_vr_extensions(struct conversion_context *ctx, const char **extra_extensions, unsigned int *extra_count,
        const char *ext_str)
{
    char *iter, *start;

    if (!ext_str) return;
    iter = cc_strdup(ctx, ext_str);

    TRACE("got var: %s\n", iter);
    start = iter;
    do
    {
        if(*iter == ' ')
        {
            *iter = 0;
            if (!find_extension(extra_extensions, *extra_count, start))
            {
                extra_extensions[(*extra_count)++] = cc_strdup(ctx, start);
                TRACE("added %s to list\n", extra_extensions[(*extra_count) - 1]);
            }
            iter++;
            start = iter;
        }
        else if(*iter == 0)
        {
            if (!find_extension(extra_extensions, *extra_count, start))
            {
                extra_extensions[(*extra_count)++] = cc_strdup(ctx, start);
                TRACE("added %s to list\n", extra_extensions[(*extra_count) - 1]);
            }
            break;
        }
        else
        {
            iter++;
        }
    } while (1);
}

static void parse_openxr_extensions(struct conversion_context *ctx, const char **extra_extensions, unsigned int *extra_count)
{
    parse_vr_extensions(ctx, extra_extensions, extra_count, getenv("__WINE_OPENXR_VK_DEVICE_EXTENSIONS"));
}

static void parse_openvr_extensions(struct conversion_context *ctx, const char **extra_extensions, unsigned int *extra_count,
        struct wine_phys_dev *phys_dev)
{
    VkPhysicalDeviceProperties prop;
    char name[64];

    phys_dev->obj.instance->p_vkGetPhysicalDeviceProperties(phys_dev->obj.host.physical_device, &prop);
    sprintf( name, "VK_WINE_OPENVR_DEVICE_EXTS_PCIID_%04x_%04x", prop.vendorID, prop.deviceID );
    parse_vr_extensions(ctx, extra_extensions, extra_count, getenv(name));
}

static VkResult wine_vk_device_convert_create_info(VkPhysicalDevice client_physical_device,
        struct conversion_context *ctx, const VkDeviceCreateInfo *src, VkDeviceCreateInfo *dst,
        struct vulkan_device *device)
{
    static const char *wine_xr_extension_name = "VK_WINE_openxr_device_extensions";
    static const char *wine_vr_extension_name = "VK_WINE_openvr_device_extensions";
    struct wine_phys_dev *phys_dev = wine_phys_dev_from_handle(client_physical_device);
    const char *extra_extensions[64], * const*extensions = src->ppEnabledExtensionNames;
    unsigned int i, extra_count = 0, extensions_count = src->enabledExtensionCount;
    unsigned int j, remove_count = 0;
    const char *remove_extensions[64];
    VkBaseOutStructure *header;

    *dst = *src;
    if ((header = (VkBaseOutStructure *)dst->pNext) && header->sType == VK_STRUCTURE_TYPE_CREATE_INFO_WINE_DEVICE_CALLBACK)
        dst->pNext = header->pNext;

    /* Should be filtered out by loader as ICDs don't support layers. */
    dst->enabledLayerCount = 0;
    dst->ppEnabledLayerNames = NULL;

    TRACE("Enabled %u extensions.\n", extensions_count);
    for (i = 0; i < extensions_count; i++)
    {
        const char *extension_name = extensions[i];
        TRACE("Extension %u: %s.\n", i, debugstr_a(extension_name));
    }

    if (find_extension(extensions, extensions_count, wine_xr_extension_name))
    {
        parse_openxr_extensions(ctx, extra_extensions, &extra_count);
        remove_extensions[remove_count++] = wine_xr_extension_name;
    }

    if (find_extension(extensions, extensions_count, wine_vr_extension_name))
    {
        parse_openvr_extensions(ctx, extra_extensions, &extra_count, phys_dev);
        remove_extensions[remove_count++] = wine_vr_extension_name;
    }

    if (find_extension(extensions, extensions_count, "VK_KHR_external_memory_win32"))
    {
        extra_extensions[extra_count++] = "VK_KHR_external_memory_fd";
        remove_extensions[remove_count++] = "VK_KHR_external_memory_win32";
    }

    if (find_extension(extensions, extensions_count, "VK_KHR_external_semaphore_win32"))
    {
        extra_extensions[extra_count++] = "VK_KHR_external_semaphore_fd";
        remove_extensions[remove_count++] = "VK_KHR_external_semaphore_win32";
    }

    if (find_extension(extensions, extensions_count, "VK_KHR_win32_keyed_mutex"))
    {
        if (!find_extension(extensions, extensions_count, "VK_KHR_external_memory_win32"))
            extra_extensions[extra_count++] = "VK_KHR_external_memory_fd";
        if (!find_extension(extensions, extensions_count, "VK_KHR_external_semaphore_win32"))
            extra_extensions[extra_count++] = "VK_KHR_external_semaphore_fd";
        remove_extensions[remove_count++] = "VK_KHR_win32_keyed_mutex";
        device->keyed_mutexes_enabled = TRUE;
    }


    if ((phys_dev->obj.api_version < VK_API_VERSION_1_2 || phys_dev->obj.instance->api_version < VK_API_VERSION_1_2)
                && !find_extension(extensions, extensions_count, "VK_KHR_timeline_semaphore"))
        extra_extensions[extra_count++] = "VK_KHR_timeline_semaphore";

    if (phys_dev->map_placed_align)
    {
        VkPhysicalDeviceMapMemoryPlacedFeaturesEXT *map_placed_features;
        map_placed_features = conversion_context_alloc(ctx, sizeof(*map_placed_features));
        map_placed_features->sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAP_MEMORY_PLACED_FEATURES_EXT;
        map_placed_features->pNext = (void *)dst->pNext;
        map_placed_features->memoryMapPlaced = VK_TRUE;
        map_placed_features->memoryMapRangePlaced = VK_FALSE;
        map_placed_features->memoryUnmapReserve = VK_TRUE;
        dst->pNext = map_placed_features;

        if (!find_extension(extensions, extensions_count, "VK_EXT_map_memory_placed"))
            extra_extensions[extra_count++] = "VK_EXT_map_memory_placed";
        if (!find_extension(extensions, extensions_count, "VK_KHR_map_memory2"))
            extra_extensions[extra_count++] = "VK_KHR_map_memory2";
    }
    else if (phys_dev->external_memory_align)
    {
        if (!find_extension(extensions, extensions_count, "VK_KHR_external_memory"))
            extra_extensions[extra_count++] = "VK_KHR_external_memory";
        if (!find_extension(extensions, extensions_count, "VK_EXT_external_memory_host"))
            extra_extensions[extra_count++] = "VK_EXT_external_memory_host";
    }

    if (extra_count)
    {
        const char **new_extensions;

        dst->enabledExtensionCount += extra_count;
        new_extensions = conversion_context_alloc(ctx, dst->enabledExtensionCount * sizeof(*new_extensions));
        memcpy(new_extensions, extensions, extensions_count * sizeof(*new_extensions));
        for (i = 0; i < extensions_count; i++)
        {
            for (j = 0; j < remove_count; ++j)
            {
                if (!strcmp(new_extensions[i], remove_extensions[j]))
                {
                    --dst->enabledExtensionCount;
                    --extensions_count;
                    memmove(&new_extensions[i], &new_extensions[i + 1], sizeof(*new_extensions) * (extensions_count - i));
                    --i;
                    break;
                }
            }
        }
        memcpy(new_extensions + extensions_count, extra_extensions, extra_count * sizeof(*new_extensions));
        dst->ppEnabledExtensionNames = new_extensions;
    }

    return VK_SUCCESS;
}

NTSTATUS init_vulkan(void *arg)
{
    const struct vk_callback_funcs *funcs = arg;

    vk_funcs = __wine_get_vulkan_driver(WINE_VULKAN_DRIVER_VERSION);
    if (!vk_funcs)
    {
        ERR("Failed to load Wine graphics driver supporting Vulkan.\n");
        return STATUS_UNSUCCESSFUL;
    }

    callback_funcs = *funcs;
    p_vkCreateInstance = (PFN_vkCreateInstance)vk_funcs->p_vkGetInstanceProcAddr(NULL, "vkCreateInstance");
    p_vkEnumerateInstanceVersion = (PFN_vkEnumerateInstanceVersion)vk_funcs->p_vkGetInstanceProcAddr(NULL, "vkEnumerateInstanceVersion");
    p_vkEnumerateInstanceExtensionProperties = (PFN_vkEnumerateInstanceExtensionProperties)vk_funcs->p_vkGetInstanceProcAddr(NULL, "vkEnumerateInstanceExtensionProperties");

    if (is_wow64())
    {
        SYSTEM_BASIC_INFORMATION info;

        NtQuerySystemInformation(SystemEmulationBasicInformation, &info, sizeof(info), NULL);
        zero_bits = (ULONG_PTR)info.HighestUserAddress | 0x7fffffff;
    }

    return STATUS_SUCCESS;
}

/* Helper function for converting between win32 and host compatible VkInstanceCreateInfo.
 * This function takes care of extensions handled at winevulkan layer, a Wine graphics
 * driver is responsible for handling e.g. surface extensions.
 */
static VkResult wine_vk_instance_convert_create_info(struct conversion_context *ctx,
        const VkInstanceCreateInfo *src, VkInstanceCreateInfo *dst, struct wine_instance *instance)
{
    VkDebugUtilsMessengerCreateInfoEXT *debug_utils_messenger;
    VkDebugReportCallbackCreateInfoEXT *debug_report_callback;
    const char **new_extensions;
    VkBaseInStructure *header;
    unsigned int i;

    *dst = *src;

    if ((header = (VkBaseInStructure *)dst->pNext) && header->sType == VK_STRUCTURE_TYPE_CREATE_INFO_WINE_INSTANCE_CALLBACK)
        dst->pNext = header->pNext;

    instance->utils_messenger_count = wine_vk_count_struct(dst, DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT);
    instance->utils_messengers =  calloc(instance->utils_messenger_count, sizeof(*instance->utils_messengers));
    header = (VkBaseInStructure *) dst;
    for (i = 0; i < instance->utils_messenger_count; i++)
    {
        header = find_next_struct(header->pNext, VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT);
        debug_utils_messenger = (VkDebugUtilsMessengerCreateInfoEXT *) header;

        instance->utils_messengers[i].instance = &instance->obj;
        instance->utils_messengers[i].host.debug_messenger = VK_NULL_HANDLE;
        instance->utils_messengers[i].user_callback = (UINT_PTR)debug_utils_messenger->pfnUserCallback;
        instance->utils_messengers[i].user_data = (UINT_PTR)debug_utils_messenger->pUserData;

        /* convert_VkInstanceCreateInfo_* already copied the chain, so we can modify it in-place. */
        debug_utils_messenger->pfnUserCallback = (void *) &debug_utils_callback_conversion;
        debug_utils_messenger->pUserData = &instance->utils_messengers[i];
    }

    if ((debug_report_callback = find_next_struct(dst->pNext, VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT)))
    {
        instance->default_callback.instance = &instance->obj;
        instance->default_callback.host.debug_callback = VK_NULL_HANDLE;
        instance->default_callback.user_callback = (UINT_PTR)debug_report_callback->pfnCallback;
        instance->default_callback.user_data = (UINT_PTR)debug_report_callback->pUserData;

        debug_report_callback->pfnCallback = (void *) &debug_report_callback_conversion;
        debug_report_callback->pUserData = &instance->default_callback;
    }

    /* ICDs don't support any layers, so nothing to copy. Modern versions of the loader
     * filter this data out as well.
     */
    if (dst->enabledLayerCount)
    {
        FIXME("Loading explicit layers is not supported by winevulkan!\n");
        return VK_ERROR_LAYER_NOT_PRESENT;
    }

    for (i = 0; i < src->enabledExtensionCount; i++)
    {
        const char *extension_name = src->ppEnabledExtensionNames[i];
        TRACE("Extension %u: %s.\n", i, debugstr_a(extension_name));
    }

    new_extensions = conversion_context_alloc(ctx, (src->enabledExtensionCount + 2) *
                                              sizeof(*src->ppEnabledExtensionNames));
    memcpy(new_extensions, src->ppEnabledExtensionNames,
           dst->enabledExtensionCount * sizeof(*dst->ppEnabledExtensionNames));
    dst->ppEnabledExtensionNames = new_extensions;
    dst->enabledExtensionCount = src->enabledExtensionCount;

    for (i = 0; i < dst->enabledExtensionCount; i++)
    {
        const char *extension_name = dst->ppEnabledExtensionNames[i];
        if (!strcmp(extension_name, "VK_EXT_debug_utils") || !strcmp(extension_name, "VK_EXT_debug_report"))
        {
            rb_init(&instance->objects, vulkan_object_compare);
            pthread_rwlock_init(&instance->objects_lock, NULL);
        }
        if (!strcmp(extension_name, "VK_KHR_win32_surface"))
        {
            new_extensions[i] = vk_funcs->p_get_host_surface_extension();
            instance->enable_win32_surface = VK_TRUE;
        }
    }

    if (use_external_memory())
    {
        new_extensions[dst->enabledExtensionCount++] = "VK_KHR_get_physical_device_properties2";
        new_extensions[dst->enabledExtensionCount++] = "VK_KHR_external_memory_capabilities";
    }

    TRACE("Enabled %u instance extensions.\n", dst->enabledExtensionCount);

    return VK_SUCCESS;
}

/* Helper function which stores wrapped physical devices in the instance object. */
static VkResult wine_vk_instance_init_physical_devices(struct wine_instance *object)
{
    struct vulkan_instance *instance = &object->obj;
    struct wine_phys_dev *physical_devices = object->phys_devs;
    VkInstance client_instance = instance->client.instance;
    VkPhysicalDevice *host_physical_devices;
    uint32_t phys_dev_count;
    unsigned int i;
    VkResult res;

    res = instance->p_vkEnumeratePhysicalDevices(instance->host.instance, &phys_dev_count, NULL);
    if (res != VK_SUCCESS)
    {
        ERR("Failed to enumerate physical devices, res=%d\n", res);
        return res;
    }
    if (!phys_dev_count)
        return res;

    if (phys_dev_count > client_instance->phys_dev_count)
    {
        client_instance->phys_dev_count = phys_dev_count;
        return VK_ERROR_OUT_OF_POOL_MEMORY;
    }
    client_instance->phys_dev_count = phys_dev_count;

    if (!(host_physical_devices = calloc(phys_dev_count, sizeof(*host_physical_devices))))
        return VK_ERROR_OUT_OF_HOST_MEMORY;

    res = instance->p_vkEnumeratePhysicalDevices(instance->host.instance, &phys_dev_count, host_physical_devices);
    if (res != VK_SUCCESS)
    {
        free(host_physical_devices);
        return res;
    }

    /* Wrap each host physical device handle into a dispatchable object for the ICD loader. */
    for (i = 0; i < phys_dev_count; i++)
    {
        struct wine_phys_dev *phys_dev = physical_devices + i;
        res = wine_vk_physical_device_init(phys_dev, host_physical_devices[i], &client_instance->phys_devs[i], instance);
        if (res != VK_SUCCESS)
            goto err;
        TRACE("added host_physical_devices[i] %p.\n", host_physical_devices[i]);
    }
    object->phys_dev_count = phys_dev_count;

    free(host_physical_devices);
    return VK_SUCCESS;

err:
    while (i) wine_phys_dev_cleanup(&physical_devices[--i]);
    free(host_physical_devices);
    return res;
}

static struct wine_phys_dev *wine_vk_instance_wrap_physical_device(struct wine_instance *instance,
        VkPhysicalDevice host_physical_device)
{
    struct wine_phys_dev *physical_devices = instance->phys_devs;
    uint32_t physical_device_count = instance->phys_dev_count;
    unsigned int i;

    for (i = 0; i < physical_device_count; ++i)
    {
        struct wine_phys_dev *current = physical_devices + i;
        if (current->obj.host.physical_device == host_physical_device) return current;
    }

    ERR("Unrecognized physical device %p.\n", host_physical_device);
    return NULL;
}

VkResult wine_vkAllocateCommandBuffers(VkDevice client_device, const VkCommandBufferAllocateInfo *allocate_info,
                                       VkCommandBuffer *buffers )
{
    struct vulkan_device *device = vulkan_device_from_handle(client_device);
    struct vulkan_instance *instance = device->physical_device->instance;
    struct wine_cmd_buffer *buffer;
    struct wine_cmd_pool *pool;
    VkResult res = VK_SUCCESS;
    unsigned int i;

    pool = wine_cmd_pool_from_handle(allocate_info->commandPool);

    for (i = 0; i < allocate_info->commandBufferCount; i++)
    {
        VkCommandBufferAllocateInfo allocate_info_host;
        VkCommandBuffer host_command_buffer, client_command_buffer = buffers[i];

        /* TODO: future extensions (none yet) may require pNext conversion. */
        allocate_info_host.pNext = allocate_info->pNext;
        allocate_info_host.sType = allocate_info->sType;
        allocate_info_host.commandPool = pool->host.command_pool;
        allocate_info_host.level = allocate_info->level;
        allocate_info_host.commandBufferCount = 1;

        TRACE("Allocating command buffer %u from pool 0x%s.\n",
                i, wine_dbgstr_longlong(allocate_info_host.commandPool));

        if (!(buffer = calloc(1, sizeof(*buffer))))
        {
            res = VK_ERROR_OUT_OF_HOST_MEMORY;
            break;
        }

        res = device->p_vkAllocateCommandBuffers(device->host.device, &allocate_info_host,
                                                       &host_command_buffer);
        if (res != VK_SUCCESS)
        {
            ERR("Failed to allocate command buffer, res=%d.\n", res);
            free(buffer);
            break;
        }

        vulkan_object_init_ptr(&buffer->obj, (UINT_PTR)host_command_buffer, &client_command_buffer->obj);
        buffer->device = device;
        vulkan_instance_insert_object(instance, &buffer->obj);
    }

    if (res != VK_SUCCESS)
        wine_vk_free_command_buffers(device, pool, i, buffers);

    return res;
}

VkResult wine_vkCreateDevice(VkPhysicalDevice client_physical_device, const VkDeviceCreateInfo *create_info,
                             const VkAllocationCallbacks *allocator, VkDevice *ret, void *client_ptr)
{
    struct vulkan_physical_device *physical_device = vulkan_physical_device_from_handle(client_physical_device);
    struct vulkan_instance *instance = physical_device->instance;
    VkDevice host_device, client_device = client_ptr;
    VkDeviceCreateInfo create_info_host;
    struct conversion_context ctx;
    struct vulkan_device *device;
    unsigned int queue_count, props_count, i;
    VkResult res;
    size_t size;
    void *ptr;

    PFN_native_vkCreateDevice native_create_device = NULL;
    void *native_create_device_context = NULL;
    VkCreateInfoWineDeviceCallback *callback;

    if (allocator)
        FIXME("Support for allocation callbacks not implemented yet\n");

    if (TRACE_ON(vulkan))
    {
        VkPhysicalDeviceProperties properties;

        instance->p_vkGetPhysicalDeviceProperties(physical_device->host.physical_device, &properties);

        TRACE("Device name: %s.\n", debugstr_a(properties.deviceName));
        TRACE("Vendor ID: %#x, Device ID: %#x.\n", properties.vendorID, properties.deviceID);
        TRACE("Driver version: %#x.\n", properties.driverVersion);
    }

    size = sizeof(*device);

    instance->p_vkGetPhysicalDeviceQueueFamilyProperties(physical_device->host.physical_device, &props_count, NULL);
    size += props_count * sizeof(*device->queue_props);

    /* We need to cache all queues within the device as each requires wrapping since queues are dispatchable objects. */
    for (queue_count = 0, i = 0; i < create_info->queueCreateInfoCount; i++)
        queue_count += create_info->pQueueCreateInfos[i].queueCount;
    size += queue_count * sizeof(*device->queues);

    if (!(device = ptr = calloc(1, size))) return VK_ERROR_OUT_OF_HOST_MEMORY;
    ptr = (char *)ptr + sizeof(*device);
    device->queue_props = ptr;
    ptr = (char *)ptr + props_count * sizeof(*device->queue_props);
    device->queues = ptr;
    ptr = (char *)ptr + queue_count * sizeof(*device->queues);

    if ((callback = (VkCreateInfoWineDeviceCallback *)create_info->pNext)
            && callback->sType == VK_STRUCTURE_TYPE_CREATE_INFO_WINE_DEVICE_CALLBACK)
    {
        native_create_device = callback->native_create_callback;
        native_create_device_context = callback->context;
    }

    pthread_mutex_init(&device->signaller_mutex, NULL);
    list_init(&device->sem_poll_list);
    list_init(&device->free_fence_ops_list);

    init_conversion_context(&ctx);
    res = wine_vk_device_convert_create_info(client_physical_device, &ctx, create_info, &create_info_host, device);
    if (res == VK_SUCCESS)
    {
        VkPhysicalDeviceFeatures features = {0};
        VkPhysicalDeviceFeatures2 *features2;

        /* Enable shaderStorageImageWriteWithoutFormat for fshack
         * This is available on all hardware and driver combinations we care about.
         */
        if (create_info_host.pEnabledFeatures)
        {
            features = *create_info_host.pEnabledFeatures;
            features.shaderStorageImageWriteWithoutFormat = VK_TRUE;
            create_info_host.pEnabledFeatures = &features;
        }
        if ((features2 = find_next_struct(&create_info_host, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2)))
        {
            features2->features.shaderStorageImageWriteWithoutFormat = VK_TRUE;
        }
        else if (!create_info_host.pEnabledFeatures)
        {
            features.shaderStorageImageWriteWithoutFormat = VK_TRUE;
            create_info_host.pEnabledFeatures = &features;
        }

        if (native_create_device)
            res = native_create_device(physical_device->host.physical_device, &create_info_host,
                                       NULL /* allocator */, &host_device,
                                       (void *)vk_funcs->p_vkGetInstanceProcAddr, native_create_device_context);
        else
            res = instance->p_vkCreateDevice(physical_device->host.physical_device, &create_info_host,
                                             NULL /* allocator */, &host_device);
    }
    free_conversion_context(&ctx);
    if (res != VK_SUCCESS)
    {
        WARN("Failed to create device, res=%d.\n", res);
        pthread_mutex_destroy(&device->signaller_mutex);
        free(device);
        return res;
    }

    vulkan_object_init_ptr(&device->obj, (UINT_PTR)host_device, &client_device->obj);
    device->physical_device = physical_device;

    /* Just load all function pointers we are aware off. The loader takes care of filtering.
     * We use vkGetDeviceProcAddr as opposed to vkGetInstanceProcAddr for efficiency reasons
     * as functions pass through fewer dispatch tables within the loader.
     */
#define USE_VK_FUNC(name)                                                                          \
    device->p_##name = (void *)vk_funcs->p_vkGetDeviceProcAddr(device->host.device, #name);  \
    if (device->p_##name == NULL) TRACE("Not found '%s'.\n", #name);
    ALL_VK_DEVICE_FUNCS
#undef USE_VK_FUNC

    instance->p_vkGetPhysicalDeviceQueueFamilyProperties(physical_device->host.physical_device, &props_count, device->queue_props);

    for (i = 0; i < create_info_host.queueCreateInfoCount; i++)
        wine_vk_device_init_queues(device, create_info_host.pQueueCreateInfos + i);

    client_device->quirks = CONTAINING_RECORD(instance, struct wine_instance, obj)->quirks;

    TRACE("Created device %p, host_device %p.\n", device, device->host.device);
    for (i = 0; i < device->queue_count; i++)
    {
        struct vulkan_queue *queue = device->queues + i;
        vulkan_instance_insert_object(instance, &queue->obj);
    }
    vulkan_instance_insert_object(instance, &device->obj);

    *ret = client_device;
    return VK_SUCCESS;
}

VkResult wine_vkCreateInstance(const VkInstanceCreateInfo *create_info,
                               const VkAllocationCallbacks *allocator, VkInstance *ret,
                               void *client_ptr)
{
    PFN_native_vkCreateInstance native_create_instance = NULL;
    void *native_create_instance_context = NULL;
    VkCreateInfoWineInstanceCallback *callback;
    VkInstanceCreateInfo create_info_host;
    const VkApplicationInfo *app_info;
    struct conversion_context ctx;
    struct wine_instance *instance;
    VkInstance host_instance, client_instance = client_ptr;
    unsigned int i;
    VkResult res;

    if (allocator)
        FIXME("Support for allocation callbacks not implemented yet\n");

    if (!(instance = calloc(1, offsetof(struct wine_instance, phys_devs[client_instance->phys_dev_count]))))
    {
        ERR("Failed to allocate memory for instance\n");
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    if ((callback = (VkCreateInfoWineInstanceCallback *)create_info->pNext)
            && callback->sType == VK_STRUCTURE_TYPE_CREATE_INFO_WINE_INSTANCE_CALLBACK)
    {
        native_create_instance = callback->native_create_callback;
        native_create_instance_context = callback->context;
    }

    init_conversion_context(&ctx);
    res = wine_vk_instance_convert_create_info(&ctx, create_info, &create_info_host, instance);
    if (res == VK_SUCCESS)
    {
        if (native_create_instance)
            res = native_create_instance(&create_info_host, NULL /* allocator */, &host_instance,
                    (void *)vk_funcs->p_vkGetInstanceProcAddr, native_create_instance_context);
        else
            res = p_vkCreateInstance(&create_info_host, NULL /* allocator */, &host_instance);
    }
    free_conversion_context(&ctx);
    if (res != VK_SUCCESS)
    {
        ERR("Failed to create instance, res=%d\n", res);
        free(instance->utils_messengers);
        free(instance);
        return res;
    }

    vulkan_object_init_ptr(&instance->obj.obj, (UINT_PTR)host_instance, &client_instance->obj);
    instance->obj.p_insert_object = vulkan_instance_insert_object;
    instance->obj.p_remove_object = vulkan_instance_remove_object;

    /* Load all instance functions we are aware of. Note the loader takes care
     * of any filtering for extensions which were not requested, but which the
     * ICD may support.
     */
#define USE_VK_FUNC(name) \
    instance->obj.p_##name = (void *)vk_funcs->p_vkGetInstanceProcAddr(instance->obj.host.instance, #name);
    ALL_VK_INSTANCE_FUNCS
#undef USE_VK_FUNC

    /* Cache physical devices for vkEnumeratePhysicalDevices within the instance as
     * each vkPhysicalDevice is a dispatchable object, which means we need to wrap
     * the host physical devices and present those to the application.
     * Cleanup happens as part of wine_vkDestroyInstance.
     */
    res = wine_vk_instance_init_physical_devices(instance);
    if (res != VK_SUCCESS)
    {
        ERR("Failed to load physical devices, res=%d\n", res);
        instance->obj.p_vkDestroyInstance(instance->obj.host.instance, NULL /* allocator */);
        free(instance->utils_messengers);
        free(instance);
        return res;
    }

    if ((app_info = create_info->pApplicationInfo))
    {
        TRACE("Application name %s, application version %#x.\n",
                debugstr_a(app_info->pApplicationName), app_info->applicationVersion);
        TRACE("Engine name %s, engine version %#x.\n", debugstr_a(app_info->pEngineName),
                app_info->engineVersion);
        TRACE("API version %#x.\n", app_info->apiVersion);

        instance->obj.api_version = app_info->apiVersion;

        if (app_info->pEngineName && !strcmp(app_info->pEngineName, "idTech"))
            instance->quirks |= WINEVULKAN_QUIRK_GET_DEVICE_PROC_ADDR;
    }

    pthread_key_create(&instance->transient_object_handle, free);

    TRACE("Created instance %p, host_instance %p.\n", instance, instance->obj.host.instance);

    for (i = 0; i < instance->phys_dev_count; i++)
    {
        struct wine_phys_dev *phys_dev = &instance->phys_devs[i];
        vulkan_instance_insert_object(&instance->obj, &phys_dev->obj.obj);
    }
    vulkan_instance_insert_object(&instance->obj, &instance->obj.obj);

    *ret = client_instance;
    return VK_SUCCESS;
}

void wine_vkDestroyDevice(VkDevice client_device, const VkAllocationCallbacks *allocator)
{
    struct pending_d3d12_fence_op *op, *next;
    struct vulkan_device *device = vulkan_device_from_handle(client_device);
    struct vulkan_instance *instance = device->physical_device->instance;
    unsigned int i;

    if (allocator)
        FIXME("Support for allocation callbacks not implemented yet\n");
    if (!device)
        return;

    if (device->signaller_thread)
    {
        TRACE("Shutting down signaller thread.\n");
        pthread_mutex_lock(&device->signaller_mutex);
        device->stop = 1;
        signal_timeline_sem(device, device->sem_poll_update.sem, &device->sem_poll_update.value);
        pthread_mutex_unlock(&device->signaller_mutex);
        pthread_join(device->signaller_thread, NULL);
        device->p_vkDestroySemaphore(device->host.device, device->sem_poll_update.sem, NULL);
        pthread_cond_destroy(&device->sem_poll_updated_cond);
        TRACE("Signaller thread shut down.\n");
    }
    pthread_mutex_destroy(&device->signaller_mutex);

    LIST_FOR_EACH_ENTRY_SAFE(op, next, &device->free_fence_ops_list, struct pending_d3d12_fence_op, entry)
    {
        device->p_vkDestroySemaphore(device->host.device, op->local_sem.sem, NULL);
        free(op);
    }

    device->p_vkDestroyDevice(device->host.device, NULL /* pAllocator */);
    for (i = 0; i < device->queue_count; i++)
        vulkan_instance_remove_object(instance, &device->queues[i].obj);
    vulkan_instance_remove_object(instance, &device->obj);

    free(device);
}

void wine_vkDestroyInstance(VkInstance client_instance, const VkAllocationCallbacks *allocator)
{
    struct wine_instance *instance = wine_instance_from_handle(client_instance);
    unsigned int i;

    if (allocator)
        FIXME("Support allocation allocators\n");
    if (!instance)
        return;

    instance->obj.p_vkDestroyInstance(instance->obj.host.instance, NULL /* allocator */);
    for (i = 0; i < instance->phys_dev_count; i++)
    {
        vulkan_instance_remove_object(&instance->obj, &instance->phys_devs[i].obj.obj);
        wine_phys_dev_cleanup(&instance->phys_devs[i]);
    }
    vulkan_instance_remove_object(&instance->obj, &instance->obj.obj);

    pthread_key_delete(instance->transient_object_handle);

    if (instance->objects.compare) pthread_rwlock_destroy(&instance->objects_lock);
    free(instance->utils_messengers);
    free(instance);
}

VkResult wine_vkEnumerateDeviceExtensionProperties(VkPhysicalDevice client_physical_device, const char *layer_name,
                                                   uint32_t *count, VkExtensionProperties *properties)
{
    struct wine_phys_dev *phys_dev = wine_phys_dev_from_handle(client_physical_device);

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

VkResult wine_vkEnumerateInstanceExtensionProperties(const char *name, uint32_t *count,
                                                     VkExtensionProperties *properties)
{
    uint32_t num_properties = 0, num_host_properties;
    VkExtensionProperties *host_properties;
    unsigned int i, j, surface;
    VkResult res;

    res = p_vkEnumerateInstanceExtensionProperties(NULL, &num_host_properties, NULL);
    if (res != VK_SUCCESS)
        return res;

    if (!(host_properties = calloc(num_host_properties, sizeof(*host_properties))))
        return VK_ERROR_OUT_OF_HOST_MEMORY;

    res = p_vkEnumerateInstanceExtensionProperties(NULL, &num_host_properties, host_properties);
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
    for (i = 0, surface = 0; i < num_host_properties; i++)
    {
        if (wine_vk_instance_extension_supported(host_properties[i].extensionName)
                || (wine_vk_is_host_surface_extension(host_properties[i].extensionName) && !surface++))
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

    for (i = 0, j = 0, surface = 0; i < num_host_properties && j < *count; i++)
    {
        if (wine_vk_instance_extension_supported(host_properties[i].extensionName))
        {
            TRACE("Enabling extension '%s'.\n", host_properties[i].extensionName);
            properties[j++] = host_properties[i];
        }
        else if (wine_vk_is_host_surface_extension(host_properties[i].extensionName) && !surface++)
        {
            VkExtensionProperties win32_surface = {VK_KHR_WIN32_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_SPEC_VERSION};
            TRACE("Enabling VK_KHR_win32_surface.\n");
            properties[j++] = win32_surface;
        }
    }
    *count = min(*count, num_properties);

    free(host_properties);
    return *count < num_properties ? VK_INCOMPLETE : VK_SUCCESS;
}

VkResult wine_vkEnumerateDeviceLayerProperties(VkPhysicalDevice client_physical_device, uint32_t *count,
                                               VkLayerProperties *properties)
{
    *count = 0;
    return VK_SUCCESS;
}

VkResult wine_vkEnumerateInstanceVersion(uint32_t *version)
{
    VkResult res;

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

VkResult wine_vkEnumeratePhysicalDevices(VkInstance client_instance, uint32_t *count, VkPhysicalDevice *client_physical_devices)
{
    struct wine_instance *instance = wine_instance_from_handle(client_instance);
    unsigned int i;

    if (!client_physical_devices)
    {
        *count = instance->phys_dev_count;
        return VK_SUCCESS;
    }

    *count = min(*count, instance->phys_dev_count);
    for (i = 0; i < *count; i++)
    {
        client_physical_devices[i] = instance->phys_devs[i].obj.client.physical_device;
    }

    TRACE("Returning %u devices.\n", *count);
    return *count < instance->phys_dev_count ? VK_INCOMPLETE : VK_SUCCESS;
}

void wine_vkFreeCommandBuffers(VkDevice client_device, VkCommandPool command_pool, uint32_t count,
                               const VkCommandBuffer *buffers)
{
    struct vulkan_device *device = vulkan_device_from_handle(client_device);
    struct wine_cmd_pool *pool = wine_cmd_pool_from_handle(command_pool);

    wine_vk_free_command_buffers(device, pool, count, buffers);
}

static VkQueue wine_vk_device_find_queue(VkDevice client_device, const VkDeviceQueueInfo2 *info)
{
    struct vulkan_device *device = vulkan_device_from_handle(client_device);
    struct vulkan_queue *queue;
    uint32_t i;

    for (i = 0; i < device->queue_count; i++)
    {
        queue = &device->queues[i];
        if (queue->family_index == info->queueFamilyIndex
                && queue->queue_index == info->queueIndex
                && queue->flags == info->flags)
        {
            return queue->client.queue;
        }
    }

    return VK_NULL_HANDLE;
}

void wine_vkGetDeviceQueue(VkDevice client_device, uint32_t family_index, uint32_t queue_index, VkQueue *client_queue)
{
    VkDeviceQueueInfo2 queue_info;

    queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2;
    queue_info.pNext = NULL;
    queue_info.flags = 0;
    queue_info.queueFamilyIndex = family_index;
    queue_info.queueIndex = queue_index;

    *client_queue = wine_vk_device_find_queue(client_device, &queue_info);
}

void wine_vkGetDeviceQueue2(VkDevice client_device, const VkDeviceQueueInfo2 *info, VkQueue *client_queue)
{
    const VkBaseInStructure *chain;

    if ((chain = info->pNext))
        FIXME("Ignoring a linked structure of type %u.\n", chain->sType);

    *client_queue = wine_vk_device_find_queue(client_device, info);
}

VkResult wine_vkCreateCommandPool(VkDevice client_device, const VkCommandPoolCreateInfo *info,
                                  const VkAllocationCallbacks *allocator, VkCommandPool *command_pool,
                                  void *client_ptr)
{
    struct vulkan_device *device = vulkan_device_from_handle(client_device);
    struct vulkan_instance *instance = device->physical_device->instance;
    struct vk_command_pool *client_command_pool = client_ptr;
    VkCommandPool host_command_pool;
    struct wine_cmd_pool *object;
    VkResult res;

    if (allocator)
        FIXME("Support for allocation callbacks not implemented yet\n");

    if (!(object = calloc(1, sizeof(*object))))
        return VK_ERROR_OUT_OF_HOST_MEMORY;

    res = device->p_vkCreateCommandPool(device->host.device, info, NULL, &host_command_pool);
    if (res != VK_SUCCESS)
    {
        free(object);
        return res;
    }

    vulkan_object_init_ptr(&object->obj, host_command_pool, &client_command_pool->obj);
    vulkan_instance_insert_object(instance, &object->obj);

    *command_pool = object->client.command_pool;
    return VK_SUCCESS;
}

void wine_vkDestroyCommandPool(VkDevice client_device, VkCommandPool handle,
                               const VkAllocationCallbacks *allocator)
{
    struct vulkan_device *device = vulkan_device_from_handle(client_device);
    struct vulkan_instance *instance = device->physical_device->instance;
    struct wine_cmd_pool *pool = wine_cmd_pool_from_handle(handle);

    if (allocator)
        FIXME("Support for allocation callbacks not implemented yet\n");

    device->p_vkDestroyCommandPool(device->host.device, pool->host.command_pool, NULL);
    vulkan_instance_remove_object(instance, &pool->obj);
    free(pool);
}

static VkResult wine_vk_enumerate_physical_device_groups(struct wine_instance *instance,
        VkResult (*p_vkEnumeratePhysicalDeviceGroups)(VkInstance, uint32_t *, VkPhysicalDeviceGroupProperties *),
        uint32_t *count, VkPhysicalDeviceGroupProperties *properties)
{
    unsigned int i, j;
    VkResult res;

    res = p_vkEnumeratePhysicalDeviceGroups(instance->obj.host.instance, count, properties);
    if (res < 0 || !properties)
        return res;

    for (i = 0; i < *count; ++i)
    {
        VkPhysicalDeviceGroupProperties *current = &properties[i];
        for (j = 0; j < current->physicalDeviceCount; ++j)
        {
            VkPhysicalDevice host_physical_device = current->physicalDevices[j];
            struct wine_phys_dev *phys_dev = wine_vk_instance_wrap_physical_device(instance, host_physical_device);
            if (!phys_dev)
                return VK_ERROR_INITIALIZATION_FAILED;
            current->physicalDevices[j] = phys_dev->obj.client.physical_device;
        }
    }

    return res;
}

VkResult wine_vkEnumeratePhysicalDeviceGroups(VkInstance client_instance, uint32_t *count,
                                              VkPhysicalDeviceGroupProperties *properties)
{
    struct wine_instance *instance = wine_instance_from_handle(client_instance);

    return wine_vk_enumerate_physical_device_groups(instance,
            instance->obj.p_vkEnumeratePhysicalDeviceGroups, count, properties);
}

VkResult wine_vkEnumeratePhysicalDeviceGroupsKHR(VkInstance client_instance, uint32_t *count,
                                                 VkPhysicalDeviceGroupProperties *properties)
{
    struct wine_instance *instance = wine_instance_from_handle(client_instance);

    return wine_vk_enumerate_physical_device_groups(instance,
            instance->obj.p_vkEnumeratePhysicalDeviceGroupsKHR, count, properties);
}

void wine_vkGetPhysicalDeviceExternalFenceProperties(VkPhysicalDevice client_physical_device,
                                                     const VkPhysicalDeviceExternalFenceInfo *fence_info,
                                                     VkExternalFenceProperties *properties)
{
    properties->exportFromImportedHandleTypes = 0;
    properties->compatibleHandleTypes = 0;
    properties->externalFenceFeatures = 0;
}

void wine_vkGetPhysicalDeviceExternalFencePropertiesKHR(VkPhysicalDevice client_physical_device,
                                                        const VkPhysicalDeviceExternalFenceInfo *fence_info,
                                                        VkExternalFenceProperties *properties)
{
    properties->exportFromImportedHandleTypes = 0;
    properties->compatibleHandleTypes = 0;
    properties->externalFenceFeatures = 0;
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

static void wine_vk_get_physical_device_external_buffer_properties(struct wine_phys_dev *phys_dev,
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

    p_vkGetPhysicalDeviceExternalBufferProperties(phys_dev->obj.host.physical_device, &buffer_info_dup, properties);

    if (properties->externalMemoryProperties.exportFromImportedHandleTypes & VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT)
        properties->externalMemoryProperties.exportFromImportedHandleTypes |= wine_vk_handle_over_fd_types;
    wine_vk_normalize_handle_types_win(&properties->externalMemoryProperties.exportFromImportedHandleTypes);

    if (properties->externalMemoryProperties.compatibleHandleTypes & VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT)
        properties->externalMemoryProperties.compatibleHandleTypes |= wine_vk_handle_over_fd_types;
    wine_vk_normalize_handle_types_win(&properties->externalMemoryProperties.compatibleHandleTypes);
}

void wine_vkGetPhysicalDeviceExternalBufferProperties(VkPhysicalDevice client_physical_device,
                                                      const VkPhysicalDeviceExternalBufferInfo *buffer_info,
                                                      VkExternalBufferProperties *properties)
{
    struct wine_phys_dev *phys_dev = wine_phys_dev_from_handle(client_physical_device);
    wine_vk_get_physical_device_external_buffer_properties(phys_dev, phys_dev->obj.instance->p_vkGetPhysicalDeviceExternalBufferProperties, buffer_info, properties);
}

void wine_vkGetPhysicalDeviceExternalBufferPropertiesKHR(VkPhysicalDevice client_physical_device,
                                                         const VkPhysicalDeviceExternalBufferInfo *buffer_info,
                                                         VkExternalBufferProperties *properties)
{
    struct wine_phys_dev *phys_dev = wine_phys_dev_from_handle(client_physical_device);
    wine_vk_get_physical_device_external_buffer_properties(phys_dev, phys_dev->obj.instance->p_vkGetPhysicalDeviceExternalBufferPropertiesKHR, buffer_info, properties);
}

static VkResult wine_vk_get_physical_device_image_format_properties_2(struct vulkan_physical_device *physical_device,
        VkResult (*p_vkGetPhysicalDeviceImageFormatProperties2)(VkPhysicalDevice, const VkPhysicalDeviceImageFormatInfo2 *, VkImageFormatProperties2 *),
        const VkPhysicalDeviceImageFormatInfo2 *format_info, VkImageFormatProperties2 *properties)
{
    VkPhysicalDeviceExternalImageFormatInfo *external_image_info;
    VkExternalImageFormatProperties *external_image_properties;
    VkResult res;

    if ((external_image_info = find_next_struct(format_info, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO))
            && external_image_info->handleType)
    {
        wine_vk_normalize_handle_types_win(&external_image_info->handleType);

        if (external_image_info->handleType & wine_vk_handle_over_fd_types)
            external_image_info->handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;

        wine_vk_normalize_handle_types_host(&external_image_info->handleType);
        if (!external_image_info->handleType)
        {
            FIXME("Unsupported handle type %#x.\n", external_image_info->handleType);
            return VK_ERROR_FORMAT_NOT_SUPPORTED;
        }
    }

    res = p_vkGetPhysicalDeviceImageFormatProperties2(physical_device->host.physical_device, format_info, properties);

    if ((external_image_properties = find_next_struct(properties,
                                                      VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES)))
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

VkResult wine_vkGetPhysicalDeviceImageFormatProperties2(VkPhysicalDevice client_physical_device,
                                                        const VkPhysicalDeviceImageFormatInfo2 *format_info,
                                                        VkImageFormatProperties2 *properties)
{
    struct vulkan_physical_device *physical_device = vulkan_physical_device_from_handle(client_physical_device);
    struct vulkan_instance *instance = physical_device->instance;

    return wine_vk_get_physical_device_image_format_properties_2(physical_device,
            instance->p_vkGetPhysicalDeviceImageFormatProperties2,
            format_info, properties);
}

VkResult wine_vkGetPhysicalDeviceImageFormatProperties2KHR(VkPhysicalDevice client_physical_device,
                                                           const VkPhysicalDeviceImageFormatInfo2 *format_info,
                                                           VkImageFormatProperties2 *properties)
{
    struct vulkan_physical_device *physical_device = vulkan_physical_device_from_handle(client_physical_device);
    struct vulkan_instance *instance = physical_device->instance;

    return wine_vk_get_physical_device_image_format_properties_2(physical_device,
            instance->p_vkGetPhysicalDeviceImageFormatProperties2KHR,
            format_info, properties);
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

static VkResult wine_vk_get_timestamps(struct vulkan_device *device, uint32_t timestamp_count,
                                       const VkCalibratedTimestampInfoEXT *timestamp_infos,
                                       uint64_t *timestamps, uint64_t *max_deviation,
                                       VkResult (*get_timestamps)(VkDevice, uint32_t, const VkCalibratedTimestampInfoEXT *, uint64_t *, uint64_t *))
{
    VkCalibratedTimestampInfoEXT* host_timestamp_infos;
    unsigned int i;
    VkResult res;

    if (timestamp_count == 0)
        return VK_SUCCESS;

    if (!(host_timestamp_infos = calloc(sizeof(VkCalibratedTimestampInfoEXT), timestamp_count)))
        return VK_ERROR_OUT_OF_HOST_MEMORY;

    for (i = 0; i < timestamp_count; i++)
    {
        host_timestamp_infos[i].sType = timestamp_infos[i].sType;
        host_timestamp_infos[i].pNext = timestamp_infos[i].pNext;
        host_timestamp_infos[i].timeDomain = map_to_host_time_domain(timestamp_infos[i].timeDomain);
    }

    res = get_timestamps(device->host.device, timestamp_count, host_timestamp_infos, timestamps, max_deviation);
    if (res == VK_SUCCESS)
    {
        for (i = 0; i < timestamp_count; i++)
            timestamps[i] = convert_timestamp(host_timestamp_infos[i].timeDomain, timestamp_infos[i].timeDomain, timestamps[i]);
    }

    free(host_timestamp_infos);

    return res;
}

static VkResult wine_vk_get_time_domains(struct vulkan_physical_device *physical_device,
                                         uint32_t *time_domain_count,
                                         VkTimeDomainEXT *time_domains,
                                         VkResult (*get_domains)(VkPhysicalDevice, uint32_t *, VkTimeDomainEXT *))
{
    BOOL supports_device = FALSE, supports_monotonic = FALSE, supports_monotonic_raw = FALSE;
    const VkTimeDomainEXT performance_counter_domain = get_performance_counter_time_domain();
    VkTimeDomainEXT *host_time_domains;
    uint32_t host_time_domain_count;
    VkTimeDomainEXT out_time_domains[2];
    uint32_t out_time_domain_count;
    unsigned int i;
    VkResult res;

    /* Find out the time domains supported on the host */
    res = get_domains(physical_device->host.physical_device, &host_time_domain_count, NULL);
    if (res != VK_SUCCESS)
        return res;

    if (!(host_time_domains = malloc(sizeof(VkTimeDomainEXT) * host_time_domain_count)))
        return VK_ERROR_OUT_OF_HOST_MEMORY;

    res = get_domains(physical_device->host.physical_device, &host_time_domain_count, host_time_domains);
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

VkResult wine_vkGetCalibratedTimestampsEXT(VkDevice client_device, uint32_t timestamp_count,
                                           const VkCalibratedTimestampInfoEXT *timestamp_infos,
                                           uint64_t *timestamps, uint64_t *max_deviation)
{
    struct vulkan_device *device = vulkan_device_from_handle(client_device);

    TRACE("%p, %u, %p, %p, %p\n", device, timestamp_count, timestamp_infos, timestamps, max_deviation);

    return wine_vk_get_timestamps(device, timestamp_count, timestamp_infos, timestamps, max_deviation,
                                  device->p_vkGetCalibratedTimestampsEXT);
}

VkResult wine_vkGetCalibratedTimestampsKHR(VkDevice client_device, uint32_t timestamp_count,
                                           const VkCalibratedTimestampInfoKHR *timestamp_infos,
                                           uint64_t *timestamps, uint64_t *max_deviation)
{
    struct vulkan_device *device = vulkan_device_from_handle(client_device);

    TRACE("%p, %u, %p, %p, %p\n", device, timestamp_count, timestamp_infos, timestamps, max_deviation);

    return wine_vk_get_timestamps(device, timestamp_count, timestamp_infos, timestamps, max_deviation,
                                  device->p_vkGetCalibratedTimestampsKHR);
}

VkResult wine_vkGetPhysicalDeviceCalibrateableTimeDomainsEXT(VkPhysicalDevice client_physical_device,
                                                             uint32_t *time_domain_count,
                                                             VkTimeDomainEXT *time_domains)
{
    struct vulkan_physical_device *physical_device = vulkan_physical_device_from_handle(client_physical_device);
    struct vulkan_instance *instance = physical_device->instance;

    TRACE("%p, %p, %p\n", physical_device, time_domain_count, time_domains);

    return wine_vk_get_time_domains(physical_device, time_domain_count, time_domains,
                                    instance->p_vkGetPhysicalDeviceCalibrateableTimeDomainsEXT);
}

VkResult wine_vkGetPhysicalDeviceCalibrateableTimeDomainsKHR(VkPhysicalDevice client_physical_device,
                                                             uint32_t *time_domain_count,
                                                             VkTimeDomainKHR *time_domains)
{
    struct vulkan_physical_device *physical_device = vulkan_physical_device_from_handle(client_physical_device);
    struct vulkan_instance *instance = physical_device->instance;

    TRACE("%p, %p, %p\n", physical_device, time_domain_count, time_domains);

    return wine_vk_get_time_domains(physical_device, time_domain_count, time_domains,
                                    instance->p_vkGetPhysicalDeviceCalibrateableTimeDomainsKHR);
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

static void wine_vk_get_physical_device_external_semaphore_properties(struct wine_phys_dev *phys_dev,
    void (*p_vkGetPhysicalDeviceExternalSemaphoreProperties)(VkPhysicalDevice, const VkPhysicalDeviceExternalSemaphoreInfo *, VkExternalSemaphoreProperties *),
    const VkPhysicalDeviceExternalSemaphoreInfo *semaphore_info, VkExternalSemaphoreProperties *properties)
{
    VkPhysicalDeviceExternalSemaphoreInfo semaphore_info_dup = *semaphore_info;
    VkSemaphoreTypeCreateInfo semaphore_type_info, *p_semaphore_type_info;

    switch(semaphore_info->handleType)
    {
        case VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT:
            semaphore_info_dup.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;
            break;
        case VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D12_FENCE_BIT:
        {
            unsigned int i;

            if (phys_dev->obj.api_version < VK_API_VERSION_1_2 ||
                phys_dev->obj.instance->api_version < VK_API_VERSION_1_2)
            {
                for (i = 0; i < phys_dev->extension_count; i++)
                {
                    if (!strcmp(phys_dev->extensions[i].extensionName, "VK_KHR_timeline_semaphore"))
                        break;
                }
                if (i == phys_dev->extension_count)
                {
                    properties->exportFromImportedHandleTypes = 0;
                    properties->compatibleHandleTypes = 0;
                    properties->externalSemaphoreFeatures = 0;
                    return;
                }
            }

            if ((p_semaphore_type_info = find_next_struct(&semaphore_info_dup, VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO)))
            {
                p_semaphore_type_info->semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
                p_semaphore_type_info->initialValue = 0;
            }
            else
            {
                semaphore_type_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
                semaphore_type_info.pNext = semaphore_info_dup.pNext;
                semaphore_type_info.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
                semaphore_type_info.initialValue = 0;

                semaphore_info_dup.pNext = &semaphore_type_info;
            }

            semaphore_info_dup.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;
            break;
        }
        default:
            semaphore_info_dup.handleType = 0;
            break;
    }

    if (semaphore_info->handleType && !semaphore_info_dup.handleType)
    {
        properties->exportFromImportedHandleTypes = 0;
        properties->compatibleHandleTypes = 0;
        properties->externalSemaphoreFeatures = 0;
        return;
    }

    p_vkGetPhysicalDeviceExternalSemaphoreProperties(phys_dev->obj.host.physical_device, &semaphore_info_dup, properties);

    if (properties->exportFromImportedHandleTypes & VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT)
        properties->exportFromImportedHandleTypes = semaphore_info->handleType;
    wine_vk_normalize_semaphore_handle_types_win(&properties->exportFromImportedHandleTypes);

    if (properties->compatibleHandleTypes & VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT)
        properties->compatibleHandleTypes = semaphore_info->handleType;
    wine_vk_normalize_semaphore_handle_types_win(&properties->compatibleHandleTypes);
}

void wine_vkGetPhysicalDeviceExternalSemaphoreProperties(VkPhysicalDevice client_physical_device,
                                                         const VkPhysicalDeviceExternalSemaphoreInfo *info,
                                                         VkExternalSemaphoreProperties *properties)
{
    struct wine_phys_dev *phys_dev = wine_phys_dev_from_handle(client_physical_device);

    TRACE("%p, %p, %p\n", phys_dev, info, properties);
    wine_vk_get_physical_device_external_semaphore_properties(phys_dev, phys_dev->obj.instance->p_vkGetPhysicalDeviceExternalSemaphoreProperties, info, properties);
}

void wine_vkGetPhysicalDeviceExternalSemaphorePropertiesKHR(VkPhysicalDevice client_physical_device,
                                                            const VkPhysicalDeviceExternalSemaphoreInfo *info,
                                                            VkExternalSemaphoreProperties *properties)
{
    struct wine_phys_dev *phys_dev = wine_phys_dev_from_handle(client_physical_device);

    TRACE("%p, %p, %p\n", phys_dev, info, properties);
    wine_vk_get_physical_device_external_semaphore_properties(phys_dev, phys_dev->obj.instance->p_vkGetPhysicalDeviceExternalSemaphorePropertiesKHR, info, properties);
}

#define IOCTL_SHARED_GPU_RESOURCE_CREATE           CTL_CODE(FILE_DEVICE_VIDEO, 0, METHOD_BUFFERED, FILE_WRITE_ACCESS)

struct shared_resource_create
{
    UINT64 resource_size;
    obj_handle_t unix_handle;
    WCHAR name[1];
};

/* helper for internal ioctl calls */
typedef struct
{
    union
    {
        NTSTATUS Status;
        ULONG Pointer;
    };
    ULONG Information;
} IO_STATUS_BLOCK32;

static NTSTATUS wine_ioctl(HANDLE file, ULONG code, void *in_buffer, ULONG in_size, void *out_buffer, ULONG out_size)
{
    IO_STATUS_BLOCK32 io32;
    IO_STATUS_BLOCK io;

    /* the 32-bit iosb is filled for overlapped file handles */
    io.Pointer = &io32;
    return NtDeviceIoControlFile(file, NULL, NULL, NULL, &io, code, in_buffer, in_size, out_buffer, out_size);
}

static HANDLE create_gpu_resource(int fd, LPCWSTR name, UINT64 resource_size)
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
    inbuff->resource_size = resource_size;
    if (name)
        lstrcpyW(&inbuff->name[0], name);

    if ((status = wine_ioctl(shared_resource, IOCTL_SHARED_GPU_RESOURCE_CREATE, inbuff, in_size, NULL, 0)))

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

struct shared_resource_info
{
    UINT64 resource_size;
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

    status = wine_ioctl(shared_resource, IOCTL_SHARED_GPU_RESOURCE_OPEN, inbuff, in_size, NULL, 0);

    free(inbuff);

    if (status)
    {
        ERR("Failed to open video resource, status %#lx.\n", (long int)status);
        NtClose(shared_resource);
        return INVALID_HANDLE_VALUE;
    }

    return shared_resource;
}

#define IOCTL_SHARED_GPU_RESOURCE_GET_INFO CTL_CODE(FILE_DEVICE_VIDEO, 7, METHOD_BUFFERED, FILE_READ_ACCESS)

static BOOL shared_resource_get_info(HANDLE handle, struct shared_resource_info *info)
{
    unsigned int status;

    status = wine_ioctl(handle, IOCTL_SHARED_GPU_RESOURCE_GET_INFO, NULL, 0, info, sizeof(*info));
    if (status)
        ERR("Failed to get shared resource info, status %#x.\n", status);

    return !status;
}

#define IOCTL_SHARED_GPU_RESOURCE_GET_UNIX_RESOURCE           CTL_CODE(FILE_DEVICE_VIDEO, 3, METHOD_BUFFERED, FILE_READ_ACCESS)

static int get_shared_resource_fd(HANDLE shared_resource)
{
    obj_handle_t unix_resource;
    NTSTATUS status;
    int ret;

    if (wine_ioctl(shared_resource, IOCTL_SHARED_GPU_RESOURCE_GET_UNIX_RESOURCE, NULL, 0, &unix_resource, sizeof(unix_resource)))
        return -1;

    status = wine_server_handle_to_fd(wine_server_ptr_handle(unix_resource), FILE_READ_DATA, &ret, NULL);
    NtClose(wine_server_ptr_handle(unix_resource));
    return status == STATUS_SUCCESS ? ret : -1;
}

#define IOCTL_SHARED_GPU_RESOURCE_GETKMT           CTL_CODE(FILE_DEVICE_VIDEO, 2, METHOD_BUFFERED, FILE_READ_ACCESS)

static HANDLE get_shared_resource_kmt_handle(HANDLE shared_resource)
{
    obj_handle_t kmt_handle;

    if (wine_ioctl(shared_resource, IOCTL_SHARED_GPU_RESOURCE_GETKMT, NULL, 0, &kmt_handle, sizeof(kmt_handle)))
        return INVALID_HANDLE_VALUE;

    return wine_server_ptr_handle(kmt_handle);
}

static bool set_shared_resource_object(HANDLE shared_resource, unsigned int index, HANDLE handle);
static HANDLE get_shared_resource_object(HANDLE shared_resource, unsigned int index);

static void destroy_keyed_mutex(struct vulkan_device *device, struct wine_device_memory *memory)
{
    if (memory->keyed_mutex_shm)
    {
        NtUnmapViewOfSection(GetCurrentProcess(), memory->keyed_mutex_shm);
        memory->keyed_mutex_shm = NULL;
    }
    if (memory->keyed_mutex_sem)
    {
        device->p_vkDestroySemaphore(device->host.device, memory->keyed_mutex_sem, NULL);
        memory->keyed_mutex_sem = VK_NULL_HANDLE;
    }
}

static void create_keyed_mutex(struct vulkan_device *device, struct wine_device_memory *memory)
{
    VkExportSemaphoreCreateInfo timeline_export_info;
    VkSemaphoreTypeCreateInfo type_info;
    VkSemaphoreCreateInfo create_info;
    VkSemaphoreGetFdInfoKHR fd_info;
    pthread_mutexattr_t mutex_attr;
    OBJECT_ATTRIBUTES attr;
    HANDLE section_handle;
    LARGE_INTEGER li;
    HANDLE handle;
    SIZE_T size;
    VkResult vr;
    int fd;

    InitializeObjectAttributes(&attr, NULL, 0, NULL, NULL);
    size = li.QuadPart = sizeof(*memory->keyed_mutex_shm);
    if (NtCreateSection(&section_handle, STANDARD_RIGHTS_REQUIRED | SECTION_QUERY | SECTION_MAP_READ | SECTION_MAP_WRITE, &attr, &li, PAGE_READWRITE, SEC_COMMIT, NULL))
    {
        ERR("NtCreateSection failed.\n");
        return;
    }

    if (!set_shared_resource_object(memory->handle, 0, section_handle))
    {
        NtClose(section_handle);
        ERR("set_shared_resource_object failed.\n");
        return;
    }

    if (NtMapViewOfSection(section_handle, GetCurrentProcess(), (void**) &memory->keyed_mutex_shm, 0, 0, NULL, &size, ViewShare, 0, PAGE_READWRITE))
    {
        NtClose(section_handle);
        ERR("NtMapViewOfSection failed.\n");
        return;
    }

    NtClose(section_handle);

    timeline_export_info.sType = VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO;
    timeline_export_info.pNext = NULL;
    timeline_export_info.handleTypes = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;

    type_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
    type_info.pNext = &timeline_export_info;
    type_info.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
    type_info.initialValue = 0;

    create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    create_info.pNext = &type_info;
    create_info.flags = 0;

    if ((vr = device->p_vkCreateSemaphore(device->host.device, &create_info, NULL, &memory->keyed_mutex_sem)) != VK_SUCCESS)
    {
        ERR("Failed to create semaphore, vr %d.\n", vr);
        goto error;
    }
    fd_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_GET_FD_INFO_KHR;
    fd_info.pNext = NULL;
    fd_info.semaphore = memory->keyed_mutex_sem;
    fd_info.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;

    if ((vr = device->p_vkGetSemaphoreFdKHR(device->host.device, &fd_info, &fd)) != VK_SUCCESS)
    {
        ERR("Failed to export semaphore fd, vr %d.\n", vr);
        goto error;
    }
    if (wine_server_fd_to_handle(fd, GENERIC_ALL, 0, &handle) != STATUS_SUCCESS)
    {
        ERR("wine_server_fd_to_handle failed.\n");
        close(fd);
        goto error;
    }
    close(fd);
    if (!set_shared_resource_object(memory->handle, 1, handle))
    {
        ERR("set_shared_resource_object failed.\n");
        NtClose(handle);
        goto error;
    }
    NtClose(handle);

    pthread_mutexattr_init(&mutex_attr);
    pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED);
    if (pthread_mutex_init(&memory->keyed_mutex_shm->mutex, &mutex_attr))
    memory->keyed_mutex_shm->instance_id_counter = 1;
    memory->keyed_mutex_instance_id = ++memory->keyed_mutex_shm->instance_id_counter;
    TRACE("memory %p, created keyed mutex.\n", memory);
    return;

error:
    destroy_keyed_mutex(device, memory);
}

static void import_keyed_mutex(struct vulkan_device *device, struct wine_device_memory *memory)
{
    VkSemaphoreTypeCreateInfo type_info;
    VkImportSemaphoreFdInfoKHR fd_info;
    VkSemaphoreCreateInfo create_info;
    HANDLE section_handle, sem_handle;
    SIZE_T size;

    VkResult vr;

    if (!(section_handle = get_shared_resource_object(memory->handle, 0)))
    {
        TRACE("No section handle.\n");
        return;
    }
    if (!(sem_handle = get_shared_resource_object(memory->handle, 1)))
    {
        ERR("No smeaphore handle.\n");
        NtClose(section_handle);
        return;
    }

    size = sizeof(*memory->keyed_mutex_shm);
    if (NtMapViewOfSection(section_handle, GetCurrentProcess(), (void**) &memory->keyed_mutex_shm, 0, 0, NULL, &size, ViewShare, 0, PAGE_READWRITE))
    {
        ERR("NtMapViewOfSection failed.\n");
        goto error;
    }

    type_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
    type_info.pNext = NULL;
    type_info.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
    type_info.initialValue = 0;

    create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    create_info.pNext = &type_info;
    create_info.flags = 0;

    if ((vr = device->p_vkCreateSemaphore(device->host.device, &create_info, NULL, &memory->keyed_mutex_sem)) != VK_SUCCESS)
    {
        ERR("Failed to create semaphore, vr %d.\n", vr);
        goto error;
    }

    fd_info.sType = VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_FD_INFO_KHR;
    fd_info.pNext = NULL;
    fd_info.semaphore = memory->keyed_mutex_sem;
    fd_info.flags = 0;
    fd_info.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;

    if (wine_server_handle_to_fd(sem_handle, FILE_READ_DATA, &fd_info.fd, NULL))
    {
        ERR("wine_server_handle_to_fd failed.\n");
        goto error;
    }

    vr = device->p_vkImportSemaphoreFdKHR(device->host.device, &fd_info);
    if (vr != VK_SUCCESS)
    {
        ERR("vkImportSemaphoreFdKHR failed, vr %d.\n", vr);
        close(fd_info.fd);
        goto error;
    }
    /* Not closing fd on successful import, the driver now owns it. */

    memory->keyed_mutex_instance_id = InterlockedIncrement64((LONGLONG *)&memory->keyed_mutex_shm->instance_id_counter);
    TRACE("memory %p, imported keyed mutex.\n", memory);
    return;
error:
    NtClose(section_handle);
    NtClose(sem_handle);
    destroy_keyed_mutex(device, memory);
}

static VkResult acquire_keyed_mutex(struct vulkan_device *device, struct wine_device_memory *memory, uint64_t key,
        uint32_t timeout_ms)
{
    ULONG end_wait, curr_tick, remaining_wait;
    VkSemaphoreWaitInfo wait_info = { 0 };
    uint64_t timeline;
    VkResult vr;

    if (!memory->keyed_mutex_shm)
        return VK_ERROR_UNKNOWN;

    wait_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
    wait_info.semaphoreCount = 1;
    wait_info.pSemaphores = &memory->keyed_mutex_sem;
    wait_info.pValues = &timeline;

    end_wait = NtGetTickCount() + timeout_ms;

    while (1)
    {
        pthread_mutex_lock(&memory->keyed_mutex_shm->mutex);

        if (memory->keyed_mutex_shm->acquired_to_instance)
        {
            if ((vr = get_semaphore_value(device, memory->keyed_mutex_sem, &timeline)) != VK_SUCCESS)
            {
                pthread_mutex_unlock(&memory->keyed_mutex_shm->mutex);
                return VK_ERROR_UNKNOWN;
            }
            assert(timeline == memory->keyed_mutex_shm->timeline_value
                    || timeline == memory->keyed_mutex_shm->timeline_value + 1);
            if (timeline == memory->keyed_mutex_shm->timeline_value + 1)
            {
                /* released from queue. */
                assert(memory->keyed_mutex_shm->timeline_queued_release == timeline);
                memory->keyed_mutex_shm->timeline_queued_release = 0;
                ++memory->keyed_mutex_shm->timeline_value;
                memory->keyed_mutex_shm->acquired_to_instance = 0;
            }
        }

        if (memory->keyed_mutex_shm->acquired_to_instance == memory->keyed_mutex_instance_id
                && !memory->keyed_mutex_shm->timeline_queued_release)
        {
            /* Already acquired to this device. */
            pthread_mutex_unlock(&memory->keyed_mutex_shm->mutex);
            return VK_ERROR_UNKNOWN;
        }
        if (!memory->keyed_mutex_shm->acquired_to_instance && memory->keyed_mutex_shm->key == key)
        {
            /* Can acquire. */
            memory->keyed_mutex_shm->acquired_to_instance = memory->keyed_mutex_instance_id;
            pthread_mutex_unlock(&memory->keyed_mutex_shm->mutex);
            return VK_SUCCESS;
        }
        curr_tick = NtGetTickCount();
        if (!timeout_ms || curr_tick >= end_wait)
        {
            pthread_mutex_unlock(&memory->keyed_mutex_shm->mutex);
            return VK_TIMEOUT;
        }
        remaining_wait = timeout_ms == INFINITE ? INFINITE : end_wait - curr_tick;
        timeline = memory->keyed_mutex_shm->timeline_value + 1;
        pthread_mutex_unlock(&memory->keyed_mutex_shm->mutex);

        vr = wait_semaphores(device, &wait_info, remaining_wait * 1000000ull);
        if (vr != VK_SUCCESS && vr != VK_TIMEOUT)
        {
            ERR("vkWaitSemaphores failed, vr %d.\n", vr);
            return VK_ERROR_UNKNOWN;
        }
    }
}

static VkResult release_keyed_mutex(struct vulkan_device *device, struct wine_device_memory *memory, uint64_t key,
        uint64_t *timeline_value)
{
    if (!memory->keyed_mutex_shm)
        return VK_ERROR_UNKNOWN;

    pthread_mutex_lock(&memory->keyed_mutex_shm->mutex);
    if (memory->keyed_mutex_shm->acquired_to_instance != memory->keyed_mutex_instance_id
            || memory->keyed_mutex_shm->timeline_queued_release)
    {
        pthread_mutex_unlock(&memory->keyed_mutex_shm->mutex);
        return VK_ERROR_UNKNOWN;
    }
    memory->keyed_mutex_shm->key = key;
    if (timeline_value)
    {
        /* Return timeline value to signal from queue. */
        *timeline_value = memory->keyed_mutex_shm->timeline_value + 1;
        memory->keyed_mutex_shm->timeline_queued_release = *timeline_value;
    }
    else
    {
        /* Release immediately. */
        memory->keyed_mutex_shm->acquired_to_instance = 0;
        signal_timeline_sem(device, memory->keyed_mutex_sem, &memory->keyed_mutex_shm->timeline_value);
    }
    pthread_mutex_unlock(&memory->keyed_mutex_shm->mutex);

    return VK_SUCCESS;
}

VkResult wine_vkAllocateMemory(VkDevice client_device, const VkMemoryAllocateInfo *alloc_info,
                               const VkAllocationCallbacks *allocator, VkDeviceMemory *ret)
{
    struct vulkan_device *device = vulkan_device_from_handle(client_device);
    struct wine_phys_dev *physical_device = CONTAINING_RECORD(device->physical_device, struct wine_phys_dev, obj);
    struct vulkan_instance *instance = device->physical_device->instance;
    struct wine_instance *wine_instance = CONTAINING_RECORD(instance, struct wine_instance, obj);
    struct wine_device_memory *memory;
    VkMemoryAllocateInfo info = *alloc_info;
    VkImportMemoryHostPointerInfoEXT host_pointer_info;
    VkDeviceMemory host_device_memory;
    uint32_t mem_flags;
    void *mapping = NULL;
    VkResult result;

    const VkImportMemoryWin32HandleInfoKHR *handle_import_info;
    const VkExportMemoryWin32HandleInfoKHR *handle_export_info;
    VkExportMemoryAllocateInfo *export_info;
    VkImportMemoryFdInfoKHR fd_import_info;
    VkMemoryGetFdInfoKHR get_fd_info;
    int fd;

    if (!(memory = calloc(sizeof(*memory), 1)))
        return VK_ERROR_OUT_OF_HOST_MEMORY;

    memory->handle = INVALID_HANDLE_VALUE;
    fd_import_info.fd = -1;
    fd_import_info.pNext = NULL;

    /* find and process handle import/export info and grab it */
    handle_import_info = wine_vk_find_unlink_struct(&info, IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR);
    handle_export_info = wine_vk_find_unlink_struct(&info, EXPORT_MEMORY_WIN32_HANDLE_INFO_KHR);
    if (handle_export_info && handle_export_info->pAttributes && handle_export_info->pAttributes->lpSecurityDescriptor)
        FIXME("Support for custom security descriptor not implemented.\n");

    if ((export_info = find_next_struct(alloc_info, VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO)))
    {
        memory->handle_types = export_info->handleTypes;
        if (export_info->handleTypes & wine_vk_handle_over_fd_types)
            export_info->handleTypes |= VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
        wine_vk_normalize_handle_types_host(&export_info->handleTypes);
    }

    mem_flags = physical_device->memory_properties.memoryTypes[alloc_info->memoryTypeIndex].propertyFlags;

    /* Vulkan consumes imported FDs, but not imported HANDLEs */
    if (handle_import_info)
    {
        struct shared_resource_info res_info;

        fd_import_info.sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR;
        fd_import_info.pNext = info.pNext;
        fd_import_info.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
        info.pNext = &fd_import_info;

        TRACE("import handle type %#x.\n", handle_import_info->handleType);

        switch (handle_import_info->handleType)
        {
            case VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT:
            case VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT:
                if (handle_import_info->handle)
                    NtDuplicateObject( NtCurrentProcess(), handle_import_info->handle, NtCurrentProcess(), &memory->handle, 0, 0, DUPLICATE_SAME_ACCESS );
                else if (handle_import_info->name)
                    memory->handle = open_shared_resource( 0, handle_import_info->name );
                break;
            case VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT:
            case VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_KMT_BIT:
                /* FIXME: the spec says that device memory imported from a KMT handle doesn't keep a reference to the underyling payload.
                   This means that in cases where on windows an application leaks VkDeviceMemory objects, we leak the full payload.  To
                   fix this, we would need wine_dev_mem objects to store no reference to the payload, that means no host VkDeviceMemory
                   object (as objects imported from FDs hold a reference to the payload), and no win32 handle to the object. We would then
                   extend make_vulkan to have the thunks converting wine_dev_mem to native handles open the VkDeviceMemory from the KMT
                   handle, use it in the host function, then close it again. */
                memory->handle = open_shared_resource( handle_import_info->handle, NULL );
                break;
            default:
                WARN("Invalid handle type %08x passed in.\n", handle_import_info->handleType);
                result = VK_ERROR_INVALID_EXTERNAL_HANDLE;
                goto done;
        }

        if (memory->handle != INVALID_HANDLE_VALUE)
            fd_import_info.fd = get_shared_resource_fd(memory->handle);

        if (fd_import_info.fd == -1)
        {
            TRACE("Couldn't access resource handle or name. type=%08x handle=%p name=%s\n", handle_import_info->handleType, handle_import_info->handle,
                    handle_import_info->name ? debugstr_w(handle_import_info->name) : "");
            result = VK_ERROR_INVALID_EXTERNAL_HANDLE;
            goto done;
        }

        /* From VkMemoryAllocateInfo spec: "if the parameters define an import operation and the external handle type is
         * VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT, VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_KMT_BIT,
         * or VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT, allocationSize is ignored.". Although test suggests
         * that it is also true for opaque Win32 handles. */
        if (shared_resource_get_info(memory->handle, &res_info))
        {
            if (res_info.resource_size)
            {
                TRACE("Shared resource size %llu.\n", (long long)res_info.resource_size);
                if (info.allocationSize && info.allocationSize != res_info.resource_size)
                    FIXME("Shared resource allocationSize %llu, resource_size %llu.\n",
                            (long long)info.allocationSize, (long long)res_info.resource_size);
                info.allocationSize = res_info.resource_size;
            }
            else
            {
                ERR("Zero shared resource size.\n");
            }
        }
        if (device->keyed_mutexes_enabled)
            import_keyed_mutex(device, memory);
    }
    else if (physical_device->external_memory_align && (mem_flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) &&
        !find_next_struct(alloc_info->pNext, VK_STRUCTURE_TYPE_IMPORT_MEMORY_HOST_POINTER_INFO_EXT))
    {
        /* For host visible memory, we try to use VK_EXT_external_memory_host on wow64
         * to ensure that mapped pointer is 32-bit. */
        VkMemoryHostPointerPropertiesEXT props =
        {
            .sType = VK_STRUCTURE_TYPE_MEMORY_HOST_POINTER_PROPERTIES_EXT,
        };
        uint32_t i, align = physical_device->external_memory_align - 1;
        SIZE_T alloc_size = info.allocationSize;
        static int once;

        if (!once++)
            FIXME("Using VK_EXT_external_memory_host\n");

        if (NtAllocateVirtualMemory(GetCurrentProcess(), &mapping, zero_bits, &alloc_size,
                                    MEM_COMMIT, PAGE_READWRITE))
        {
            ERR("NtAllocateVirtualMemory failed\n");
            free(memory);
            return VK_ERROR_OUT_OF_HOST_MEMORY;
        }

        result = device->p_vkGetMemoryHostPointerPropertiesEXT(device->host.device,
                VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT, mapping, &props);
        if (result != VK_SUCCESS)
        {
            ERR("vkGetMemoryHostPointerPropertiesEXT failed: %d\n", result);
            free(memory);
            return result;
        }

        if (!(props.memoryTypeBits & (1u << info.memoryTypeIndex)))
        {
            /* If requested memory type is not allowed to use external memory,
             * try to find a supported compatible type. */
            uint32_t mask = mem_flags & ~VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
            for (i = 0; i < physical_device->memory_properties.memoryTypeCount; i++)
            {
                if (!(props.memoryTypeBits & (1u << i)))
                    continue;
                if ((physical_device->memory_properties.memoryTypes[i].propertyFlags & mask) != mask)
                    continue;

                TRACE("Memory type not compatible with host memory, using %u instead\n", i);
                info.memoryTypeIndex = i;
                break;
            }
            if (i == physical_device->memory_properties.memoryTypeCount)
            {
                FIXME("Not found compatible memory type\n");
                alloc_size = 0;
                NtFreeVirtualMemory(GetCurrentProcess(), &mapping, &alloc_size, MEM_RELEASE);
            }
        }

        if (props.memoryTypeBits & (1u << info.memoryTypeIndex))
        {
            host_pointer_info.sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_HOST_POINTER_INFO_EXT;
            host_pointer_info.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT;
            host_pointer_info.pHostPointer = mapping;
            host_pointer_info.pNext = info.pNext;
            info.pNext = &host_pointer_info;

            info.allocationSize = (info.allocationSize + align) & ~align;
        }
    }

    set_transient_client_handle(wine_instance, (uintptr_t)memory);
    result = device->p_vkAllocateMemory(device->host.device, &info, NULL, &host_device_memory);
    if (result == VK_SUCCESS && memory->handle == INVALID_HANDLE_VALUE && export_info && export_info->handleTypes & VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT)
    {
        get_fd_info.sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR;
        get_fd_info.pNext = NULL;
        get_fd_info.memory = host_device_memory;
        get_fd_info.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;

        if (device->p_vkGetMemoryFdKHR(device->host.device, &get_fd_info, &fd) == VK_SUCCESS)
        {
            memory->handle = create_gpu_resource(fd, handle_export_info ? handle_export_info->name : NULL, alloc_info->allocationSize);
            memory->access = handle_export_info ? handle_export_info->dwAccess : GENERIC_ALL;
            if (handle_export_info && handle_export_info->pAttributes)
                memory->inherit = handle_export_info->pAttributes->bInheritHandle;
            else
                memory->inherit = FALSE;
            close(fd);
            if (device->keyed_mutexes_enabled)
                create_keyed_mutex(device, memory);
        }

        if (memory->handle == INVALID_HANDLE_VALUE)
        {
            device->p_vkFreeMemory(device->host.device, host_device_memory, NULL);
            result = VK_ERROR_OUT_OF_HOST_MEMORY;
            goto done;
        }
    }
done:
    if (result != VK_SUCCESS)
    {
        if (fd_import_info.fd != -1)
            close(fd_import_info.fd);
        if (memory->handle != INVALID_HANDLE_VALUE)
            NtClose(memory->handle);
        free(memory);
        return result;
    }

    vulkan_object_init(&memory->obj, host_device_memory);
    memory->size = info.allocationSize;
    memory->vm_map = mapping;
    vulkan_instance_insert_object(instance, &memory->obj);

    *ret = memory->client.device_memory;
    return VK_SUCCESS;
}

void wine_vkFreeMemory(VkDevice client_device, VkDeviceMemory memory_handle, const VkAllocationCallbacks *allocator)
{
    struct vulkan_device *device = vulkan_device_from_handle(client_device);
    struct wine_phys_dev *physical_device = CONTAINING_RECORD(device->physical_device, struct wine_phys_dev, obj);
    struct vulkan_instance *instance = device->physical_device->instance;
    struct wine_device_memory *memory;

    if (!memory_handle)
        return;
    memory = wine_device_memory_from_handle(memory_handle);

    destroy_keyed_mutex(device, memory);
    if (memory->vm_map && !physical_device->external_memory_align)
    {
        const VkMemoryUnmapInfoKHR info =
        {
            .sType = VK_STRUCTURE_TYPE_MEMORY_UNMAP_INFO_KHR,
            .memory = memory->host.device_memory,
            .flags = VK_MEMORY_UNMAP_RESERVE_BIT_EXT,
        };
        device->p_vkUnmapMemory2KHR(device->host.device, &info);
    }

    device->p_vkFreeMemory(device->host.device, memory->host.device_memory, NULL);
    vulkan_instance_remove_object(instance, &memory->obj);

    if (memory->vm_map)
    {
        SIZE_T alloc_size = 0;
        NtFreeVirtualMemory(GetCurrentProcess(), &memory->vm_map, &alloc_size, MEM_RELEASE);
    }

    if (memory->handle != INVALID_HANDLE_VALUE)
        NtClose(memory->handle);

    free(memory);
}

VkResult wine_vkMapMemory(VkDevice client_device, VkDeviceMemory memory, VkDeviceSize offset,
                          VkDeviceSize size, VkMemoryMapFlags flags, void **data)
{
    const VkMemoryMapInfoKHR info =
    {
      .sType = VK_STRUCTURE_TYPE_MEMORY_MAP_INFO_KHR,
      .flags = flags,
      .memory = memory,
      .offset = offset,
      .size = size,
   };

   return wine_vkMapMemory2KHR(client_device, &info, data);
}

VkResult wine_vkMapMemory2KHR(VkDevice client_device, const VkMemoryMapInfoKHR *map_info, void **data)
{
    struct vulkan_device *device = vulkan_device_from_handle(client_device);
    struct wine_phys_dev *physical_device = CONTAINING_RECORD(device->physical_device, struct wine_phys_dev, obj);
    struct wine_device_memory *memory = wine_device_memory_from_handle(map_info->memory);
    VkMemoryMapInfoKHR info = *map_info;
    VkMemoryMapPlacedInfoEXT placed_info =
    {
        .sType = VK_STRUCTURE_TYPE_MEMORY_MAP_PLACED_INFO_EXT,
    };
    VkResult result;

    info.memory = memory->host.device_memory;
    if (memory->vm_map)
    {
        *data = (char *)memory->vm_map + info.offset;
        TRACE("returning %p\n", *data);
        return VK_SUCCESS;
    }

    if (physical_device->map_placed_align)
    {
        SIZE_T alloc_size = memory->size;

        placed_info.pNext = info.pNext;
        info.pNext = &placed_info;
        info.offset = 0;
        info.size = VK_WHOLE_SIZE;
        info.flags |=  VK_MEMORY_MAP_PLACED_BIT_EXT;

        if (NtAllocateVirtualMemory(GetCurrentProcess(), &placed_info.pPlacedAddress, zero_bits, &alloc_size,
                                    MEM_COMMIT, PAGE_READWRITE))
        {
            ERR("NtAllocateVirtualMemory failed\n");
            return VK_ERROR_OUT_OF_HOST_MEMORY;
        }
    }

    if (device->p_vkMapMemory2KHR)
    {
        result = device->p_vkMapMemory2KHR(device->host.device, &info, data);
    }
    else
    {
        assert(!info.pNext);
        result = device->p_vkMapMemory(device->host.device, info.memory, info.offset,
                                             info.size, info.flags, data);
    }

    if (placed_info.pPlacedAddress)
    {
        if (result != VK_SUCCESS)
        {
            SIZE_T alloc_size = 0;
            ERR("vkMapMemory2EXT failed: %d\n", result);
            NtFreeVirtualMemory(GetCurrentProcess(), &placed_info.pPlacedAddress, &alloc_size, MEM_RELEASE);
            return result;
        }
        memory->vm_map = placed_info.pPlacedAddress;
        *data = (char *)memory->vm_map + map_info->offset;
        TRACE("Using placed mapping %p\n", memory->vm_map);
    }

#ifdef _WIN64
    if (NtCurrentTeb()->WowTebOffset && result == VK_SUCCESS && (UINT_PTR)*data >> 32)
    {
        FIXME("returned mapping %p does not fit 32-bit pointer\n", *data);
        device->p_vkUnmapMemory(device->host.device, memory->host.device_memory);
        *data = NULL;
        result = VK_ERROR_OUT_OF_HOST_MEMORY;
    }
#endif

    return result;
}

void wine_vkUnmapMemory(VkDevice client_device, VkDeviceMemory memory)
{
    const VkMemoryUnmapInfoKHR info =
    {
        .sType = VK_STRUCTURE_TYPE_MEMORY_UNMAP_INFO_KHR,
        .memory = memory,
    };

    wine_vkUnmapMemory2KHR(client_device, &info);
}

VkResult wine_vkUnmapMemory2KHR(VkDevice client_device, const VkMemoryUnmapInfoKHR *unmap_info)
{
    struct vulkan_device *device = vulkan_device_from_handle(client_device);
    struct wine_phys_dev *physical_device = CONTAINING_RECORD(device->physical_device, struct wine_phys_dev, obj);
    struct wine_device_memory *memory = wine_device_memory_from_handle(unmap_info->memory);
    VkMemoryUnmapInfoKHR info;
    VkResult result;

    if (memory->vm_map && physical_device->external_memory_align)
        return VK_SUCCESS;

    if (!device->p_vkUnmapMemory2KHR)
    {
        assert(!unmap_info->pNext && !memory->vm_map);
        device->p_vkUnmapMemory(device->host.device, memory->host.device_memory);
        return VK_SUCCESS;
    }

    info = *unmap_info;
    info.memory = memory->host.device_memory;
    if (memory->vm_map)
        info.flags |= VK_MEMORY_UNMAP_RESERVE_BIT_EXT;

    result = device->p_vkUnmapMemory2KHR(device->host.device, &info);

    if (result == VK_SUCCESS && memory->vm_map)
    {
        SIZE_T size = 0;
        NtFreeVirtualMemory(GetCurrentProcess(), &memory->vm_map, &size, MEM_RELEASE);
        memory->vm_map = NULL;
    }
    return result;
}

VkResult wine_vkCreateBuffer(VkDevice client_device, const VkBufferCreateInfo *create_info,
                             const VkAllocationCallbacks *allocator, VkBuffer *buffer)
{
    struct vulkan_device *device = vulkan_device_from_handle(client_device);
    struct wine_phys_dev *physical_device = CONTAINING_RECORD(device->physical_device, struct wine_phys_dev, obj);
    VkExternalMemoryBufferCreateInfo external_memory_info, *ext_info;
    VkBufferCreateInfo info = *create_info;

    if ((ext_info = find_next_struct(create_info, VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO)))
    {
        if (ext_info->handleTypes & wine_vk_handle_over_fd_types)
            ext_info->handleTypes |= VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
        wine_vk_normalize_handle_types_host(&ext_info->handleTypes);
    }
    else if (physical_device->external_memory_align &&
        !find_next_struct(info.pNext, VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO))
    {
        external_memory_info.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO;
        external_memory_info.pNext = info.pNext;
        external_memory_info.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT;
        info.pNext = &external_memory_info;
    }

    return device->p_vkCreateBuffer(device->host.device, &info, NULL, buffer);
}

VkResult wine_vkCreateImage(VkDevice client_device, const VkImageCreateInfo *create_info,
                            const VkAllocationCallbacks *allocator, VkImage *image)
{
    struct vulkan_device *device = vulkan_device_from_handle(client_device);
    struct wine_phys_dev *physical_device = CONTAINING_RECORD(device->physical_device, struct wine_phys_dev, obj);
    VkExternalMemoryImageCreateInfo external_memory_info, *update_info;
    VkImageCreateInfo info = *create_info;

    if ((update_info = find_next_struct(info.pNext, VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO)))
    {
        if (update_info->handleTypes & wine_vk_handle_over_fd_types)
            update_info->handleTypes |= VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT_KHR;
        wine_vk_normalize_handle_types_host(&update_info->handleTypes);
    }
    else if (physical_device->external_memory_align)
    {
        external_memory_info.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
        external_memory_info.pNext = info.pNext;
        external_memory_info.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT;
        info.pNext = &external_memory_info;
    }

    return device->p_vkCreateImage(device->host.device, &info, NULL, image);
}

VkResult wine_vkCreateDebugUtilsMessengerEXT(VkInstance client_instance,
                                             const VkDebugUtilsMessengerCreateInfoEXT *create_info,
                                             const VkAllocationCallbacks *allocator,
                                             VkDebugUtilsMessengerEXT *messenger)
{
    struct vulkan_instance *instance = vulkan_instance_from_handle(client_instance);
    VkDebugUtilsMessengerCreateInfoEXT wine_create_info;
    VkDebugUtilsMessengerEXT host_debug_messenger;
    struct wine_debug_utils_messenger *object;
    VkResult res;

    if (allocator)
        FIXME("Support for allocation callbacks not implemented yet\n");

    if (!(object = calloc(1, sizeof(*object))))
        return VK_ERROR_OUT_OF_HOST_MEMORY;

    wine_create_info = *create_info;
    wine_create_info.pfnUserCallback = (void *) &debug_utils_callback_conversion;
    wine_create_info.pUserData = object;

    res = instance->p_vkCreateDebugUtilsMessengerEXT(instance->host.instance, &wine_create_info,
                                                           NULL, &host_debug_messenger);
    if (res != VK_SUCCESS)
    {
        free(object);
        return res;
    }

    vulkan_object_init(&object->obj, host_debug_messenger);
    object->instance = instance;
    object->user_callback = (UINT_PTR)create_info->pfnUserCallback;
    object->user_data = (UINT_PTR)create_info->pUserData;
    vulkan_instance_insert_object(instance, &object->obj);

    *messenger = object->client.debug_messenger;
    return VK_SUCCESS;
}

void wine_vkDestroyDebugUtilsMessengerEXT(VkInstance client_instance, VkDebugUtilsMessengerEXT messenger,
                                          const VkAllocationCallbacks *allocator)
{
    struct vulkan_instance *instance = vulkan_instance_from_handle(client_instance);
    struct wine_debug_utils_messenger *object;

    object = wine_debug_utils_messenger_from_handle(messenger);

    if (!object)
        return;

    instance->p_vkDestroyDebugUtilsMessengerEXT(instance->host.instance, object->host.debug_messenger, NULL);
    vulkan_instance_remove_object(instance, &object->obj);

    free(object);
}

VkResult wine_vkCreateDebugReportCallbackEXT(VkInstance client_instance,
                                             const VkDebugReportCallbackCreateInfoEXT *create_info,
                                             const VkAllocationCallbacks *allocator,
                                             VkDebugReportCallbackEXT *callback)
{
    struct vulkan_instance *instance = vulkan_instance_from_handle(client_instance);
    VkDebugReportCallbackCreateInfoEXT wine_create_info;
    VkDebugReportCallbackEXT host_debug_callback;
    struct wine_debug_report_callback *object;
    VkResult res;

    if (allocator)
        FIXME("Support for allocation callbacks not implemented yet\n");

    if (!(object = calloc(1, sizeof(*object))))
        return VK_ERROR_OUT_OF_HOST_MEMORY;

    wine_create_info = *create_info;
    wine_create_info.pfnCallback = (void *) debug_report_callback_conversion;
    wine_create_info.pUserData = object;

    res = instance->p_vkCreateDebugReportCallbackEXT(instance->host.instance, &wine_create_info,
                                                           NULL, &host_debug_callback);
    if (res != VK_SUCCESS)
    {
        free(object);
        return res;
    }

    vulkan_object_init(&object->obj, host_debug_callback);
    object->instance = instance;
    object->user_callback = (UINT_PTR)create_info->pfnCallback;
    object->user_data = (UINT_PTR)create_info->pUserData;
    vulkan_instance_insert_object(instance, &object->obj);

    *callback = object->client.debug_callback;
    return VK_SUCCESS;
}

void wine_vkDestroyDebugReportCallbackEXT(VkInstance client_instance, VkDebugReportCallbackEXT callback,
                                          const VkAllocationCallbacks *allocator)
{
    struct vulkan_instance *instance = vulkan_instance_from_handle(client_instance);
    struct wine_debug_report_callback *object;

    object = wine_debug_report_callback_from_handle(callback);

    if (!object)
        return;

    instance->p_vkDestroyDebugReportCallbackEXT(instance->host.instance, object->host.debug_callback, NULL);
    vulkan_instance_remove_object(instance, &object->obj);

    free(object);
}

VkResult wine_vkCreateDeferredOperationKHR(VkDevice device_handle,
                                           const VkAllocationCallbacks* allocator,
                                           VkDeferredOperationKHR*      operation)
{
    struct vulkan_device *device = vulkan_device_from_handle(device_handle);
    struct vulkan_instance *instance = device->physical_device->instance;
    VkDeferredOperationKHR host_deferred_operation;
    struct wine_deferred_operation *object;
    VkResult res;

    if (allocator)
        FIXME("Support for allocation callbacks not implemented yet\n");

    if (!(object = calloc(1, sizeof(*object))))
        return VK_ERROR_OUT_OF_HOST_MEMORY;

    res = device->p_vkCreateDeferredOperationKHR(device->host.device, NULL, &host_deferred_operation);
    if (res != VK_SUCCESS)
    {
        free(object);
        return res;
    }

    vulkan_object_init(&object->obj, host_deferred_operation);
    init_conversion_context(&object->ctx);
    vulkan_instance_insert_object(instance, &object->obj);

    *operation = object->client.deferred_operation;
    return VK_SUCCESS;
}

void wine_vkDestroyDeferredOperationKHR(VkDevice device_handle,
                                        VkDeferredOperationKHR       operation,
                                        const VkAllocationCallbacks* allocator)
{
    struct vulkan_device *device = vulkan_device_from_handle(device_handle);
    struct vulkan_instance *instance = device->physical_device->instance;
    struct wine_deferred_operation *object;

    object = wine_deferred_operation_from_handle(operation);

    if (!object)
        return;

    device->p_vkDestroyDeferredOperationKHR(device->host.device, object->host.deferred_operation, NULL);
    vulkan_instance_remove_object(instance, &object->obj);

    free_conversion_context(&object->ctx);
    free(object);
}

static void substitute_function_name(const char **name)
{
    if (!strcmp(*name, "vkGetMemoryWin32HandleKHR") || !strcmp(*name, "vkGetMemoryWin32HandlePropertiesKHR"))
        *name = "vkGetMemoryFdKHR";
    else if (!strcmp(*name, "vkGetSemaphoreWin32HandleKHR"))
        *name = "vkGetSemaphoreFdKHR";
    else if (!strcmp(*name, "vkImportSemaphoreWin32HandleKHR"))
        *name = "vkImportSemaphoreFdKHR";
    else if (!strcmp(*name, "wine_vkAcquireKeyedMutex") || !strcmp(*name, "wine_vkReleaseKeyedMutex"))
        *name = "vkImportSemaphoreFdKHR";
}

#ifdef _WIN64

NTSTATUS vk_is_available_instance_function(void *arg)
{
    struct is_available_instance_function_params *params = arg;
    struct wine_instance *instance = wine_instance_from_handle(params->instance);
    substitute_function_name(&params->name);

    if (!strcmp(params->name, "vkCreateWin32SurfaceKHR"))
        return instance->enable_win32_surface;
    if (!strcmp(params->name, "vkGetPhysicalDeviceWin32PresentationSupportKHR"))
        return instance->enable_win32_surface;

    return !!vk_funcs->p_vkGetInstanceProcAddr(instance->obj.host.instance, params->name);
}

NTSTATUS vk_is_available_device_function(void *arg)
{
    struct is_available_device_function_params *params = arg;
    struct vulkan_device *device = vulkan_device_from_handle(params->device);
    substitute_function_name(&params->name);
    return !!vk_funcs->p_vkGetDeviceProcAddr(device->host.device, params->name);
}

#endif /* _WIN64 */

NTSTATUS vk_is_available_instance_function32(void *arg)
{
    struct
    {
        UINT32 instance;
        UINT32 name;
    } *params = arg;
    struct wine_instance *instance = wine_instance_from_handle(UlongToPtr(params->instance));
    const char *name = UlongToPtr(params->name);

    if (!strcmp(UlongToPtr(params->name), "vkCreateWin32SurfaceKHR"))
        return instance->enable_win32_surface;
    if (!strcmp(UlongToPtr(params->name), "vkGetPhysicalDeviceWin32PresentationSupportKHR"))
        return instance->enable_win32_surface;

    substitute_function_name(&name);
    return !!vk_funcs->p_vkGetInstanceProcAddr(instance->obj.host.instance, name);
}

NTSTATUS vk_is_available_device_function32(void *arg)
{
    struct
    {
        UINT32 device;
        UINT32 name;
    } *params = arg;
    struct vulkan_device *device = vulkan_device_from_handle(UlongToPtr(params->device));
    const char *name = UlongToPtr(params->name);
    substitute_function_name(&name);
    return !!vk_funcs->p_vkGetDeviceProcAddr(device->host.device, name);
}

VkResult wine_vkGetMemoryWin32HandleKHR(VkDevice device, const VkMemoryGetWin32HandleInfoKHR *handle_info, HANDLE *handle)
{
    struct wine_device_memory *dev_mem = wine_device_memory_from_handle(handle_info->memory);
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

VkResult wine_vkGetMemoryWin32HandlePropertiesKHR(VkDevice device_handle, VkExternalMemoryHandleTypeFlagBits type, HANDLE handle, VkMemoryWin32HandlePropertiesKHR *properties)
{
    struct vulkan_device *device = vulkan_device_from_handle(device_handle);
    struct wine_phys_dev *physical_device = CONTAINING_RECORD(device->physical_device, struct wine_phys_dev, obj);
    unsigned int i;

    TRACE("%p %u %p %p\n", device, type, handle, properties);

    if (!(type & wine_vk_handle_over_fd_types))
    {
        FIXME("type %#x.\n", type);
        return VK_ERROR_INVALID_EXTERNAL_HANDLE;
    }

    properties->memoryTypeBits = 0;
    for (i = 0; i < physical_device->memory_properties.memoryTypeCount; ++i)
        if (physical_device->memory_properties.memoryTypes[i].propertyFlags == VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
            properties->memoryTypeBits |= 1u << i;

    return VK_SUCCESS;
}

#define IOCTL_SHARED_GPU_RESOURCE_SET_OBJECT           CTL_CODE(FILE_DEVICE_VIDEO, 6, METHOD_BUFFERED, FILE_WRITE_ACCESS)

static bool set_shared_resource_object(HANDLE shared_resource, unsigned int index, HANDLE handle)
{
    struct shared_resource_set_object
    {
        unsigned int index;
        obj_handle_t handle;
    } params;

    params.index = index;
    params.handle = wine_server_obj_handle(handle);

    return wine_ioctl(shared_resource, IOCTL_SHARED_GPU_RESOURCE_SET_OBJECT, &params, sizeof(params), NULL, 0) == STATUS_SUCCESS;
}

#define IOCTL_SHARED_GPU_RESOURCE_GET_OBJECT           CTL_CODE(FILE_DEVICE_VIDEO, 6, METHOD_BUFFERED, FILE_READ_ACCESS)

static HANDLE get_shared_resource_object(HANDLE shared_resource, unsigned int index)
{
    obj_handle_t handle;

    if (wine_ioctl(shared_resource, IOCTL_SHARED_GPU_RESOURCE_GET_OBJECT, &index, sizeof(index), &handle, sizeof(handle)))
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

static VkSemaphore create_timeline_semaphore(struct vulkan_device *device)
{
    VkSemaphoreTypeCreateInfo timeline_info = { 0 };
    VkSemaphoreCreateInfo create_info = { 0 };
    VkSemaphore sem = 0;
    VkResult res;

    timeline_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
    timeline_info.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
    create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    create_info.pNext = &timeline_info;

    res = device->p_vkCreateSemaphore(device->host.device, &create_info, NULL, &sem);
    if (res != VK_SUCCESS)
        ERR("vkCreateSemaphore failed, res=%d\n", res);
    return sem;
}

static void release_fence_op(struct vulkan_device *device, struct pending_d3d12_fence_op *op)
{
    list_remove(&op->entry);
    vulkan_instance_remove_object(device->physical_device->instance, &op->semaphore->obj.obj);
    vulkan_object_init(&op->semaphore->obj.obj, op->semaphore->semaphore);
    vulkan_instance_insert_object(device->physical_device->instance, &op->semaphore->obj.obj);
    op->semaphore = NULL;
    list_add_head(&device->free_fence_ops_list, &op->entry);
}

static int wait_info_realloc(VkSemaphoreWaitInfo *wait_info, uint32_t *wait_alloc_count)
{
    VkSemaphore *new_sem;
    uint64_t *new_values;

    if (wait_info->semaphoreCount + 1 <= *wait_alloc_count)
        return 1;
    new_sem = realloc((void *)wait_info->pSemaphores, *wait_alloc_count * 2 * sizeof(*new_sem));
    if (!new_sem)
    {
        fprintf(stderr, "err:winevulkan:wait_info_realloc no memory.\n");
        return 0;
    }
    new_values = realloc((void *)wait_info->pValues, *wait_alloc_count * 2 * sizeof(*new_values));
    if (!new_values)
    {
        fprintf(stderr, "err:winevulkan:wait_info_realloc no memory.\n");
        return 0;
    }
    *wait_alloc_count *= 2;
    wait_info->pSemaphores = new_sem;
    wait_info->pValues = new_values;
    return 1;
}

static int add_sem_wait(VkSemaphoreWaitInfo *wait_info, uint32_t *wait_alloc_count, VkSemaphore sem, uint64_t value)
{
    if (!wait_info_realloc(wait_info, wait_alloc_count))
        return 0;
    ((VkSemaphore *)wait_info->pSemaphores)[wait_info->semaphoreCount] = sem;
    ((uint64_t *)wait_info->pValues)[wait_info->semaphoreCount] = value;
    ++wait_info->semaphoreCount;
    return 1;
}

static int semaphore_process(struct vulkan_device *device, struct wine_semaphore *sem,
        VkSemaphoreWaitInfo *wait_info, uint32_t *wait_alloc_count)
{
    /* Called from native thread. */
    struct pending_d3d12_fence_op *op, *op2;
    uint64_t global_sem_wait_value;
    int virtual_value_updated = 0;
    uint64_t value, virtual_value;
    VkResult res;
    uint32_t i;

    /* Check local pending signal ops completion, update shared semaphore. */
    d3d12_semaphore_lock( sem );
    virtual_value = sem->d3d12_fence_shm->virtual_value;
    LIST_FOR_EACH_ENTRY_SAFE(op, op2, &sem->pending_signals, struct pending_d3d12_fence_op, entry)
    {
        res = get_semaphore_value(device, op->local_sem.sem, &value);
        if (res != VK_SUCCESS)
        {
            fprintf(stderr, "err:winevulkan:semaphore_process vkGetSemaphoreCounterValue failed, res=%d.\n", res);
            goto signal_op_complete;
        }
        if (value <= op->local_sem.value)
        {
            if (!add_sem_wait(wait_info, wait_alloc_count, op->local_sem.sem, op->local_sem.value + 1))
            {
                d3d12_semaphore_unlock(sem);
                return 0;
            }
            continue;
        }

        virtual_value = max( sem->d3d12_fence_shm->virtual_value, op->virtual_value );
        sem->d3d12_fence_shm->virtual_value = op->virtual_value;
        virtual_value_updated = 1;
signal_op_complete:
        op->local_sem.value = value;
        release_fence_op(device, op);
    }

    if (sem->d3d12_fence_shm->virtual_value < virtual_value)
    {
        uint32_t idx = sem->d3d12_fence_shm->reset_backlog_count;

        if (debug_level >= 3)
            fprintf(stderr, "warn:winevulkan:semaphore_process resetting semaphore %p virtual value.\n", sem);
        if (idx == ARRAY_SIZE(sem->d3d12_fence_shm->reset_backlog))
        {
            sem->d3d12_fence_shm->last_dropped_reset_physical = sem->d3d12_fence_shm->reset_backlog[0].physical_at_reset;
            --idx;
            memmove(&sem->d3d12_fence_shm->reset_backlog[0], &sem->d3d12_fence_shm->reset_backlog[1],
                    sizeof(*sem->d3d12_fence_shm->reset_backlog) * sem->d3d12_fence_shm->reset_backlog_count);
        }
        else
        {
            ++sem->d3d12_fence_shm->reset_backlog_count;
        }
        sem->d3d12_fence_shm->last_reset_physical = sem->d3d12_fence_shm->physical_value + 1;
        sem->d3d12_fence_shm->reset_backlog[idx].physical_at_reset = sem->d3d12_fence_shm->last_reset_physical;
        sem->d3d12_fence_shm->reset_backlog[idx].virtual_before_reset = virtual_value;
    }
    if (virtual_value_updated)
        signal_timeline_sem(device, sem->fence_timeline_semaphore, &sem->d3d12_fence_shm->physical_value);
    global_sem_wait_value = sem->d3d12_fence_shm->physical_value + 1;

    /* Complete satisfied local waits. */
    LIST_FOR_EACH_ENTRY_SAFE(op, op2, &sem->pending_waits, struct pending_d3d12_fence_op, entry)
    {
        if (op->virtual_value > virtual_value)
        {
            if (op->shared_physical_value > sem->d3d12_fence_shm->last_reset_physical)
                continue;
            for (i = 0; i < sem->d3d12_fence_shm->reset_backlog_count; ++i)
            {
                if (sem->d3d12_fence_shm->reset_backlog[i].physical_at_reset >= op->shared_physical_value
                        && sem->d3d12_fence_shm->reset_backlog[i].virtual_before_reset >= op->virtual_value)
                    break;
            }
            if (i == sem->d3d12_fence_shm->reset_backlog_count)
            {
                if (sem->d3d12_fence_shm->last_dropped_reset_physical < op->shared_physical_value)
                    continue;
                fprintf(stderr, "err:winevulkan:semaphore_process wait needs reset backlog beyond cut off.\n");
            }
        }

        signal_timeline_sem(device, op->local_sem.sem, &op->local_sem.value);
        release_fence_op(device, op);
    }
    d3d12_semaphore_unlock(sem);

    /* Only poll shared semaphore if there are waits pending. */
    if (list_empty(&sem->pending_waits))
        return 1;
    return add_sem_wait(wait_info, wait_alloc_count, sem->fence_timeline_semaphore, global_sem_wait_value);
}

#define SIGNALLER_INITIAL_WAIT_COUNT 256

void *signaller_worker(void *arg)
{
#ifdef HAVE_SYS_SYSCALL_H
    int unix_tid = syscall( __NR_gettid );
#else
    int unix_tid = -1;
#endif
    struct vulkan_device *device = arg;
    struct wine_semaphore *sem;
    VkSemaphoreWaitInfo wait_info = { 0 };
    uint32_t wait_alloc_count = 0;
    VkResult res;

    if (debug_level)
        fprintf(stderr, "[%d] msg:winevulkan:signaller_worker started.\n", unix_tid);

    wait_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
    wait_info.flags = VK_SEMAPHORE_WAIT_ANY_BIT;
    wait_alloc_count = SIGNALLER_INITIAL_WAIT_COUNT;
    if (!(wait_info.pSemaphores = malloc(sizeof(*wait_info.pSemaphores) * wait_alloc_count)))
    {
        fprintf(stderr, "err:winevulkan:signaller_worker no memory.\n");
        return NULL;
    }
    if (!(wait_info.pValues = malloc(sizeof(*wait_info.pValues) * wait_alloc_count)))
    {
        fprintf(stderr, "err:winevulkan:signaller_worker no memory.\n");
        free((void *)wait_info.pSemaphores);
        return NULL;
    }

    for (;;)
    {
        pthread_mutex_lock(&device->signaller_mutex);
        if (device->stop)
        {
            pthread_mutex_unlock(&device->signaller_mutex);
            break;
        }
        wait_info.semaphoreCount = 1;
        *(VkSemaphore *)wait_info.pSemaphores = device->sem_poll_update.sem;
        *(uint64_t *)wait_info.pValues = device->sem_poll_update.value + 1;
        LIST_FOR_EACH_ENTRY(sem, &device->sem_poll_list, struct wine_semaphore, poll_entry)
        {
            if (!semaphore_process(device, sem, &wait_info, &wait_alloc_count))
            {
                pthread_mutex_unlock(&device->signaller_mutex);
                break;
            }
        }
        device->sem_poll_update_value = device->sem_poll_update.value;
        pthread_cond_signal(&device->sem_poll_updated_cond);
        pthread_mutex_unlock(&device->signaller_mutex);
        while ((res = wait_semaphores(device, &wait_info, 3000000000ull)) == VK_TIMEOUT)
        {
            if (wait_info.semaphoreCount > 1)
                fprintf(stderr, "err:winevulkan:signaller_worker wait timed out with non-empty poll list.\n");
        }
        if (res != VK_SUCCESS)
        {
            fprintf(stderr, "err:winevulkan:signaller_worker error waiting for semaphores, vr %d.\n", res);
            break;
        }
    }

    free((void *)wait_info.pSemaphores);
    free((void *)wait_info.pValues);
    if (debug_level)
        fprintf(stderr, "[%d] msg:winevulkan:signaller_worker exiting.\n", unix_tid);

    return NULL;
}

static void register_sem_poll(struct vulkan_device *device, struct wine_semaphore *semaphore)
{
    pthread_mutex_lock(&device->signaller_mutex);
    if (!device->signaller_thread)
    {
        device->sem_poll_update.sem = create_timeline_semaphore(device);
        device->sem_poll_update.value = 0;
        pthread_cond_init(&device->sem_poll_updated_cond, NULL);
        if (TRACE_ON(vulkan))
            debug_level = 4;
        else if (WARN_ON(vulkan))
            debug_level = 3;
        else if (FIXME_ON(vulkan))
            debug_level = 2;
        else if (ERR_ON(vulkan))
            debug_level = 1;
        else
            debug_level = 0;
        if (pthread_create(&device->signaller_thread, NULL, signaller_worker, device))
            ERR("Failed to create signaller_worker.\n");
        WARN("d3d12 fence used, created signaller worker.\n");
    }
    assert(!semaphore->poll_entry.next);
    list_add_head(&device->sem_poll_list, &semaphore->poll_entry);
    signal_timeline_sem(device, device->sem_poll_update.sem, &device->sem_poll_update.value);
    pthread_mutex_unlock(&device->signaller_mutex);
}

static void update_sem_poll_wait_processed(struct vulkan_device *device)
{
    uint64_t update_value;

    signal_timeline_sem(device, device->sem_poll_update.sem, &device->sem_poll_update.value);
    update_value = device->sem_poll_update.value;
    while (device->sem_poll_update_value < update_value)
        pthread_cond_wait(&device->sem_poll_updated_cond, &device->signaller_mutex);
}

static void unregister_sem_poll(struct vulkan_device *device, struct wine_semaphore *semaphore)
{
    struct list *entry;

    pthread_mutex_lock(&device->signaller_mutex);
    list_remove(&semaphore->poll_entry);
    semaphore->poll_entry.next = semaphore->poll_entry.prev = NULL;
    update_sem_poll_wait_processed(device);
    pthread_mutex_unlock(&device->signaller_mutex);

    while ((entry = list_head(&semaphore->pending_waits)))
        release_fence_op(device, CONTAINING_RECORD(entry, struct pending_d3d12_fence_op, entry));
    while ((entry = list_head(&semaphore->pending_signals)))
        release_fence_op(device, CONTAINING_RECORD(entry, struct pending_d3d12_fence_op, entry));
}

static struct pending_d3d12_fence_op *get_free_fence_op(struct vulkan_device *device)
{
    struct pending_d3d12_fence_op *op;
    struct list *entry;

    if ((entry = list_head(&device->free_fence_ops_list)))
    {
        list_remove(entry);
        return CONTAINING_RECORD(entry, struct pending_d3d12_fence_op, entry);
    }

    if (!(op = malloc(sizeof(*op))))
    {
        ERR("No memory.\n");
        return NULL;
    }
    op->local_sem.sem = create_timeline_semaphore(device);
    op->local_sem.value = 0;
    ++device->allocated_fence_ops_count;
    TRACE("Total allocated fence ops %u.\n", device->allocated_fence_ops_count);
    return op;
}

static void add_sem_wait_op(struct vulkan_device *device, struct wine_semaphore *semaphore, uint64_t virtual_value,
        VkSemaphore *phys_semaphore, uint64_t *phys_wait_value)
{
    struct pending_d3d12_fence_op *op;

    pthread_mutex_lock(&device->signaller_mutex);
    LIST_FOR_EACH_ENTRY(op, &semaphore->pending_waits, struct pending_d3d12_fence_op, entry)
    {
        if (op->virtual_value == virtual_value)
        {
            *phys_semaphore = op->local_sem.sem;
            *phys_wait_value = op->local_sem.value + 1;
            pthread_mutex_unlock(&device->signaller_mutex);
            return;
        }
    }
    if ((op = get_free_fence_op(device)))
    {
        op->virtual_value = virtual_value;
        op->shared_physical_value = __atomic_load_n(&semaphore->d3d12_fence_shm->physical_value, __ATOMIC_ACQUIRE) + 1;
        *phys_semaphore = op->local_sem.sem;
        *phys_wait_value = op->local_sem.value + 1;
        op->semaphore = semaphore;
        list_add_tail(&semaphore->pending_waits, &op->entry);
        vulkan_instance_remove_object(device->physical_device->instance, &semaphore->obj.obj);
        vulkan_object_init(&semaphore->obj.obj, op->local_sem.sem);
        vulkan_instance_insert_object(device->physical_device->instance, &semaphore->obj.obj);

        signal_timeline_sem(device, device->sem_poll_update.sem, &device->sem_poll_update.value);
        TRACE("added wait op, semaphore %p, %s, temp sem %s, %s.\n", semaphore, wine_dbgstr_longlong(virtual_value),
                wine_dbgstr_longlong(op->local_sem.sem), wine_dbgstr_longlong(op->local_sem.value));
    }
    else
    {
        *phys_semaphore = 0;
        *phys_wait_value = 0;
    }
    pthread_mutex_unlock(&device->signaller_mutex);
}

static void add_sem_signal_op(struct vulkan_device *device, struct wine_semaphore *semaphore, uint64_t virtual_value,
        VkSemaphore *phys_semaphore, uint64_t *phys_signal_value, BOOL signal_immediate)
{
    struct pending_d3d12_fence_op *op;
    UINT64 value;

    pthread_mutex_lock(&device->signaller_mutex);
    if ((op = get_free_fence_op(device)))
    {
        op->virtual_value = virtual_value;
        *phys_semaphore = op->local_sem.sem;
        *phys_signal_value = op->local_sem.value + 1;
        op->semaphore = semaphore;
        list_add_tail(&semaphore->pending_signals, &op->entry);
        vulkan_instance_remove_object(device->physical_device->instance, &semaphore->obj.obj);
        vulkan_object_init(&semaphore->obj.obj, op->local_sem.sem);
        vulkan_instance_insert_object(device->physical_device->instance, &semaphore->obj.obj);

        if (signal_immediate)
        {
            value = op->local_sem.value;
            signal_timeline_sem(device, op->local_sem.sem, &value);
            update_sem_poll_wait_processed(device);
            TRACE("signal op %p, semaphore %p, %s, temp sem %s, %s.\n", op, semaphore, wine_dbgstr_longlong(virtual_value),
                    wine_dbgstr_longlong(op->local_sem.sem), wine_dbgstr_longlong(op->local_sem.value));
        }
        else
        {
            signal_timeline_sem(device, device->sem_poll_update.sem, &device->sem_poll_update.value);
            TRACE("added signal op, semaphore %p, %s, temp sem %s, %s.\n", semaphore, wine_dbgstr_longlong(virtual_value),
                    wine_dbgstr_longlong(op->local_sem.sem), wine_dbgstr_longlong(op->local_sem.value));
        }
    }
    else
    {
        *phys_semaphore = 0;
        *phys_signal_value = 0;
    }
    pthread_mutex_unlock(&device->signaller_mutex);
}

VkResult wine_vkCreateSemaphore(VkDevice client_device, const VkSemaphoreCreateInfo *create_info,
        const VkAllocationCallbacks *allocator, VkSemaphore *semaphore)
{
    struct vulkan_device *device = vulkan_device_from_handle(client_device);

    VkExportSemaphoreWin32HandleInfoKHR *export_handle_info = wine_vk_find_unlink_struct(create_info, EXPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR);
    VkExportSemaphoreCreateInfo *export_semaphore_info, timeline_export_info;
    VkSemaphoreCreateInfo create_info_dup = *create_info;
    VkSemaphoreTypeCreateInfo *found_type_info, type_info;
    VkSemaphoreGetFdInfoKHR fd_info;
    pthread_mutexattr_t mutex_attr;
    struct wine_semaphore *object;
    OBJECT_ATTRIBUTES attr;
    HANDLE section_handle;
    LARGE_INTEGER li;
    VkResult res;
    SIZE_T size;
    int fd;

    TRACE("(%p, %p, %p, %p)\n", device, create_info, allocator, semaphore);

    if (allocator)
        FIXME("Support for allocation callbacks not implemented yet\n");

    if (!(object = calloc(1, sizeof(*object))))
        return VK_ERROR_OUT_OF_HOST_MEMORY;

    list_init(&object->pending_signals);
    list_init(&object->pending_waits);

    object->handle = INVALID_HANDLE_VALUE;

    if ((export_semaphore_info = find_next_struct(&create_info_dup, VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO)))
    {
        object->export_types = export_semaphore_info->handleTypes;
        if (export_semaphore_info->handleTypes & VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT)
            export_semaphore_info->handleTypes |= VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;
        wine_vk_normalize_semaphore_handle_types_host(&export_semaphore_info->handleTypes);
    }

    if ((res = device->p_vkCreateSemaphore(device->host.device, &create_info_dup, NULL, &object->semaphore)) != VK_SUCCESS)
        goto done;

    if (export_semaphore_info && export_semaphore_info->handleTypes == VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT)
    {
        fd_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_GET_FD_INFO_KHR;
        fd_info.pNext = NULL;
        fd_info.semaphore = object->semaphore;
        fd_info.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;

        if ((res = device->p_vkGetSemaphoreFdKHR(device->host.device, &fd_info, &fd)) == VK_SUCCESS)
        {
            object->handle = create_gpu_resource(fd, export_handle_info ? export_handle_info->name : NULL, 0);
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

        create_info_dup.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        create_info_dup.pNext = &type_info;
        create_info_dup.flags = 0;

        if ((res = device->p_vkCreateSemaphore(device->host.device, &create_info_dup, NULL, &object->fence_timeline_semaphore)) != VK_SUCCESS)
            goto done;

        fd_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_GET_FD_INFO_KHR;
        fd_info.pNext = NULL;
        fd_info.semaphore = object->fence_timeline_semaphore;
        fd_info.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;

        if ((res = device->p_vkGetSemaphoreFdKHR(device->host.device, &fd_info, &fd)) == VK_SUCCESS)
        {
            object->handle = create_gpu_resource(fd, export_handle_info ? export_handle_info->name : NULL, 0);
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

        if ((found_type_info = find_next_struct(create_info, VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO)))
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

        vulkan_object_init(&object->obj.obj, object->fence_timeline_semaphore);
        vulkan_instance_insert_object(device->physical_device->instance, &object->obj.obj);
        object->obj.d3d12_fence = TRUE;
    }
    if (object->fence_timeline_semaphore == VK_NULL_HANDLE)
    {
        vulkan_object_init(&object->obj.obj, object->semaphore);
        vulkan_instance_insert_object(device->physical_device->instance, &object->obj.obj);
    }
    *semaphore = object->obj.client.handle;

    done:

    if (res != VK_SUCCESS)
    {
        if (object->d3d12_fence_shm)
        {
            pthread_mutex_destroy(&object->d3d12_fence_shm->mutex);
            NtUnmapViewOfSection(GetCurrentProcess(), object->d3d12_fence_shm);
        }
        if (object->handle != INVALID_HANDLE_VALUE)
            NtClose(object->handle);
        if (object->semaphore != VK_NULL_HANDLE)
            device->p_vkDestroySemaphore(device->host.device, object->semaphore, NULL);
        if (object->fence_timeline_semaphore != VK_NULL_HANDLE)
            device->p_vkDestroySemaphore(device->host.device, object->fence_timeline_semaphore, NULL);
        free(object);
    }
    else if (object->handle_type == VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D12_FENCE_BIT)
        register_sem_poll(device, object);
    if (res == VK_SUCCESS)
    {
        TRACE("-> %p (native %#llx, shared %#llx).\n", object, (long long)object->semaphore, (long long)object->fence_timeline_semaphore);
    }

    return res;
}

VkResult wine_vkGetSemaphoreWin32HandleKHR(VkDevice client_device, const VkSemaphoreGetWin32HandleInfoKHR *handle_info,
        HANDLE *handle)
{
    struct wine_semaphore *semaphore = wine_semaphore_from_handle(handle_info->semaphore);

    if (!(semaphore->export_types & handle_info->handleType))
        return VK_ERROR_INVALID_EXTERNAL_HANDLE;

    if (NtDuplicateObject( NtCurrentProcess(), semaphore->handle, NtCurrentProcess(), handle, 0, 0, DUPLICATE_SAME_ACCESS ))
        return VK_ERROR_INVALID_EXTERNAL_HANDLE;

    return VK_SUCCESS;
}

void wine_vkDestroySemaphore(VkDevice client_device, VkSemaphore semaphore_handle, const VkAllocationCallbacks *allocator)
{
    struct vulkan_device *device = vulkan_device_from_handle(client_device);
    struct wine_semaphore *semaphore = wine_semaphore_from_handle(semaphore_handle);

    TRACE("%p, %p, %p\n", device, semaphore, allocator);

    if (allocator)
        FIXME("Support for allocation callbacks not implemented yet\n");

    if (!semaphore)
        return;

    if (semaphore->poll_entry.next)
        unregister_sem_poll(device, semaphore);

    if (semaphore->handle != INVALID_HANDLE_VALUE)
        NtClose(semaphore->handle);

    if (semaphore->d3d12_fence_shm)
        NtUnmapViewOfSection(GetCurrentProcess(), semaphore->d3d12_fence_shm);

    vulkan_instance_remove_object(device->physical_device->instance, &semaphore->obj.obj);
    device->p_vkDestroySemaphore(device->host.device, semaphore->semaphore, NULL);

    if (semaphore->fence_timeline_semaphore)
        device->p_vkDestroySemaphore(device->host.device, semaphore->fence_timeline_semaphore, NULL);

    free(semaphore);
}

VkResult wine_vkImportSemaphoreWin32HandleKHR(VkDevice client_device,
        const VkImportSemaphoreWin32HandleInfoKHR *handle_info)
{
    struct vulkan_device *device = vulkan_device_from_handle(client_device);
    struct wine_semaphore *semaphore = wine_semaphore_from_handle(handle_info->semaphore);
    struct wine_semaphore output_semaphore;
    VkSemaphoreTypeCreateInfo type_info;
    VkImportSemaphoreFdInfoKHR fd_info;
    VkSemaphoreCreateInfo create_info;
    HANDLE d3d12_fence_shm;
    NTSTATUS stat;
    VkResult res;
    SIZE_T size;

    TRACE("(%p, %p). semaphore = %p handle = %p\n", device, handle_info, semaphore, handle_info->handle);

    if (semaphore->poll_entry.next)
        unregister_sem_poll(device, semaphore);

    if (handle_info->handleType == VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D12_FENCE_BIT && !semaphore->fence_timeline_semaphore)
    {
        type_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
        type_info.pNext = NULL;
        type_info.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
        type_info.initialValue = 0;

        create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        create_info.pNext = &type_info;
        create_info.flags = 0;

        if ((res = device->p_vkCreateSemaphore(device->host.device, &create_info, NULL, &semaphore->fence_timeline_semaphore)) != VK_SUCCESS)
        {
            ERR("Failed to create timeline semaphore backing D3D12 semaphore. vr %d.\n", res);
            return res;
        };

        vulkan_instance_remove_object(device->physical_device->instance, &semaphore->obj.obj);
        vulkan_object_init(&semaphore->obj.obj, semaphore->fence_timeline_semaphore);
        vulkan_instance_insert_object(device->physical_device->instance, &semaphore->obj.obj);
        semaphore->obj.d3d12_fence = TRUE;
    }

    output_semaphore = *semaphore;
    output_semaphore.handle = NULL;
    output_semaphore.handle_type = handle_info->handleType;
    output_semaphore.d3d12_fence_shm = NULL;

    fd_info.sType = VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_FD_INFO_KHR;
    fd_info.pNext = handle_info->pNext;
    fd_info.semaphore = output_semaphore.obj.host.semaphore;
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
                ERR("Failed to map D3D12 semaphore memory. stat %#x.\n", (int)stat);
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

    if ((res = device->p_vkImportSemaphoreFdKHR(device->host.device, &fd_info)) == VK_SUCCESS)
    {
        if (semaphore->handle)
            NtClose(semaphore->handle);
        if (semaphore->d3d12_fence_shm)
            NtUnmapViewOfSection(GetCurrentProcess(), semaphore->d3d12_fence_shm);

        *semaphore = output_semaphore;
        assert(!semaphore->poll_entry.next);
        if (semaphore->handle_type == VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D12_FENCE_BIT)
            register_sem_poll(device, semaphore);
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

static VkResult wine_vk_get_semaphore_counter_value(VkDevice device_handle, VkSemaphore semaphore_handle, uint64_t *value, bool khr)
{
    struct wine_semaphore *semaphore = wine_semaphore_from_handle(semaphore_handle);
    struct vulkan_device *device = vulkan_device_from_handle(device_handle);

    if (semaphore->handle_type == VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D12_FENCE_BIT)
    {
        d3d12_semaphore_lock(semaphore);
        *value = semaphore->d3d12_fence_shm->virtual_value;
        d3d12_semaphore_unlock(semaphore);
        return VK_SUCCESS;
    }

    if (khr)
        return device->p_vkGetSemaphoreCounterValueKHR(device->host.device, semaphore->obj.host.semaphore, value);
    else
        return device->p_vkGetSemaphoreCounterValue(device->host.device, semaphore->obj.host.semaphore, value);
}

VkResult wine_vkGetSemaphoreCounterValue(VkDevice device_handle, VkSemaphore semaphore_handle, uint64_t *value)
{
    return wine_vk_get_semaphore_counter_value(device_handle, semaphore_handle, value, false);
}

VkResult wine_vkGetSemaphoreCounterValueKHR(VkDevice device_handle, VkSemaphore semaphore_handle, uint64_t *value)
{
    return wine_vk_get_semaphore_counter_value(device_handle, semaphore_handle, value, true);
}

static NTSTATUS wine_vk_signal_semaphore(VkDevice device_handle, const VkSemaphoreSignalInfo *signal_info, bool khr)
{
    struct wine_semaphore *semaphore = wine_semaphore_from_handle(signal_info->semaphore);
    struct vulkan_device *device = vulkan_device_from_handle(device_handle);
    VkSemaphoreSignalInfo dup_signal_info = *signal_info;

    TRACE("(%p, %p)\n", device, signal_info);

    if (semaphore->handle_type == VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D12_FENCE_BIT)
    {
        add_sem_signal_op(device, semaphore, signal_info->value, &dup_signal_info.semaphore, &dup_signal_info.value, TRUE);
        return VK_SUCCESS;
    }
    else
        dup_signal_info.semaphore = semaphore->obj.host.semaphore;

    if (khr)
        return device->p_vkSignalSemaphoreKHR(device->host.device, &dup_signal_info);
    else
        return device->p_vkSignalSemaphore(device->host.device, &dup_signal_info);
}

VkResult wine_vkSignalSemaphore(VkDevice device_handle, const VkSemaphoreSignalInfo *signal_info)
{
    return wine_vk_signal_semaphore(device_handle, signal_info, false);
}

VkResult wine_vkSignalSemaphoreKHR(VkDevice device_handle, const VkSemaphoreSignalInfo *signal_info)
{
    return wine_vk_signal_semaphore(device_handle, signal_info, true);
}

static void unwrap_semaphore(struct vulkan_device *device, VkSemaphore *sem_handle, uint64_t *value, BOOL signal)
{
    struct wine_semaphore *sem = wine_semaphore_from_handle(*sem_handle);

    if (!sem)
        return;

    if (sem->handle_type != VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D12_FENCE_BIT)
    {
        *sem_handle = sem->obj.host.semaphore;
        return;
    }
    if (signal)
        add_sem_signal_op(device, sem, *value, sem_handle, value, FALSE);
    else
        add_sem_wait_op(device, sem, *value, sem_handle, value);
}

static VkResult unwrap_semaphore_array(const VkSemaphore **sems, const uint64_t **values_out,
        uint32_t count, struct conversion_context *ctx, BOOL signal, struct vulkan_device *device)
{
    const uint64_t *values = NULL;
    const VkSemaphore *in;
    VkSemaphore *out;
    unsigned int i;

    in = *sems;
    *sems = NULL;

    if (!in || !count)
        return VK_SUCCESS;

    out = conversion_context_alloc(ctx, count * sizeof(*out));
    for (i = 0; i < count; ++i)
    {
        struct wine_semaphore *sem;
        if (!in[i])
        {
            out[i] = VK_NULL_HANDLE;
            continue;
        }
        sem = wine_semaphore_from_handle(in[i]);
        if (sem->handle_type != VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D12_FENCE_BIT)
        {
            out[i] = sem->obj.host.semaphore;
            continue;
        }
        if (!values_out)
        {
            ERR("D3D12 fence without values specified.\n");
            return VK_ERROR_UNKNOWN;
        }
        if (!values)
        {
            values = *values_out;
            *values_out = conversion_context_alloc(ctx, count * sizeof(*values_out));
            memcpy((void *)*values_out, values, count * sizeof(*values));
        }
        if (signal)
            add_sem_signal_op(device, sem, values[i], &out[i], (uint64_t *)&(*values_out)[i], FALSE);
        else
            add_sem_wait_op(device, sem, values[i], &out[i], (uint64_t *)&(*values_out)[i]);
    }
    *sems = out;
    return VK_SUCCESS;
}

static VkResult wine_vk_wait_semaphores(VkDevice device_handle, const VkSemaphoreWaitInfo *wait_info, uint64_t timeout, bool khr)
{
    struct vulkan_device *device = vulkan_device_from_handle(device_handle);
    VkSemaphoreWaitInfo wait_info_dup = *wait_info;
    struct conversion_context ctx;
    VkResult ret;

    init_conversion_context(&ctx);
    if ((ret = unwrap_semaphore_array(&wait_info_dup.pSemaphores, &wait_info_dup.pValues,
            wait_info->semaphoreCount, &ctx, FALSE, device)))
        goto done;

    if (khr)
        ret = device->p_vkWaitSemaphoresKHR(device->host.device, &wait_info_dup, timeout);
    else
        ret = device->p_vkWaitSemaphores(device->host.device, &wait_info_dup, timeout);
done:
    free_conversion_context(&ctx);
    return ret;
}

VkResult wine_vkWaitSemaphores(VkDevice device, const VkSemaphoreWaitInfo *wait_info, uint64_t timeout)
{
    TRACE("%p %p %s.\n", device, wait_info, wine_dbgstr_longlong(timeout));

    return wine_vk_wait_semaphores(device, wait_info, timeout, false);
}

VkResult wine_vkWaitSemaphoresKHR(VkDevice device, const VkSemaphoreWaitInfo *wait_info, uint64_t timeout)
{
    TRACE("%p %p %s.\n", device, wait_info, wine_dbgstr_longlong(timeout));

    return wine_vk_wait_semaphores(device, wait_info, timeout, true);
}

struct struct_chain_def
{
    VkStructureType sType;
    unsigned int size;
};

static VkResult process_keyed_mutexes(struct conversion_context *ctx, struct vulkan_device *device,
        uint32_t submit_count, const void *submits_win, size_t submit_size, uint32_t **signal_counts,
        VkSemaphoreSubmitInfo ***signal_infos)
{
    VkWin32KeyedMutexAcquireReleaseInfoKHR *keyed_mutex_info;
    struct wine_device_memory *memory;
    VkResult ret = VK_ERROR_UNKNOWN;
    uint32_t i, j, signal_count = 0;
    void *ptr;

    for (i = 0; i < submit_count; ++i)
    {
        ptr = (char *)submits_win + i * submit_size;
        if (!(keyed_mutex_info = find_next_struct(ptr, VK_STRUCTURE_TYPE_WIN32_KEYED_MUTEX_ACQUIRE_RELEASE_INFO_KHR)))
            continue;
        for (j = 0; j < keyed_mutex_info->acquireCount; ++j)
        {
            memory = wine_device_memory_from_handle(keyed_mutex_info->pAcquireSyncs[j]);
            if ((ret = acquire_keyed_mutex(device, memory, keyed_mutex_info->pAcquireKeys[j],
                    keyed_mutex_info->pAcquireTimeouts[j])) == VK_SUCCESS)
                continue;
            while (j)
            {
                --j;
                memory = wine_device_memory_from_handle(keyed_mutex_info->pAcquireSyncs[j]);
                release_keyed_mutex(device, memory, keyed_mutex_info->pAcquireKeys[j], NULL);
            }
            goto error;
        }
        /* Pre-check release error conditions. */
        for (j = 0; j < keyed_mutex_info->releaseCount; ++j)
        {
            memory = wine_device_memory_from_handle(keyed_mutex_info->pReleaseSyncs[j]);
            if (!memory->keyed_mutex_shm)
                goto error;
            if (memory->keyed_mutex_shm->acquired_to_instance != memory->keyed_mutex_instance_id)
                goto error;
        }
        signal_count += keyed_mutex_info->releaseCount;
    }

    if (!signal_count)
    {
        *signal_counts = NULL;
        return VK_SUCCESS;
    }
    *signal_counts = conversion_context_alloc(ctx, sizeof(**signal_counts) * submit_count);
    *signal_infos = conversion_context_alloc(ctx, sizeof(**signal_infos) * submit_count);
    for (i = 0; i < submit_count; ++i)
    {
        ptr = (char *)submits_win + i * submit_size;
        if (!(keyed_mutex_info = wine_vk_find_unlink_struct(ptr, WIN32_KEYED_MUTEX_ACQUIRE_RELEASE_INFO_KHR)))
        {
            (*signal_counts)[i] = 0;
            continue;
        }
        (*signal_counts)[i] = keyed_mutex_info->releaseCount;
        (*signal_infos)[i] = conversion_context_alloc(ctx, sizeof(***signal_infos) * keyed_mutex_info->releaseCount);
        for (j = 0; j < keyed_mutex_info->releaseCount; ++j)
        {
            memory = wine_device_memory_from_handle(keyed_mutex_info->pReleaseSyncs[j]);
            (*signal_infos)[i][j].sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
            (*signal_infos)[i][j].pNext = NULL;
            (*signal_infos)[i][j].semaphore = memory->keyed_mutex_sem;
            (*signal_infos)[i][j].stageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
            (*signal_infos)[i][j].deviceIndex = 0;
            ret = release_keyed_mutex(device, memory, keyed_mutex_info->pReleaseKeys[j], &(*signal_infos)[i][j].value);
            if (ret != VK_SUCCESS)
            {
                /* This should only be possible if a racing submit queued release before us, currently not handled. */
                ERR("release_keyed_mutex failed, ret %d.\n", ret);
                (*signal_infos)[i][j].value = 0;
            }
        }
    }

    return VK_SUCCESS;

error:
    while (i)
    {
        --i;
        ptr = (char *)submits_win + i * submit_size;
        if (!(keyed_mutex_info = wine_vk_find_unlink_struct(ptr, WIN32_KEYED_MUTEX_ACQUIRE_RELEASE_INFO_KHR)))
            continue;
        for (j = 0; j < keyed_mutex_info->acquireCount; ++j)
        {
            memory = wine_device_memory_from_handle(keyed_mutex_info->pAcquireSyncs[j]);
            release_keyed_mutex(device, memory, keyed_mutex_info->pAcquireKeys[j], NULL);
        }
    }
    return ret;
}

static void duplicate_array_for_unwrapping_copy_size(struct conversion_context *ctx, void **ptr, unsigned int size,
        unsigned int copy_size)
{
    void *out;

    if (!size)
        return;

    out = conversion_context_alloc(ctx, size);
    if (*ptr)
        memcpy(out, *ptr, copy_size);
    *ptr = out;
}

VkResult wine_vkQueueSubmit(VkQueue queue_handle, uint32_t submit_count, const VkSubmitInfo *submits_orig, VkFence fence)
{
    struct vulkan_queue *queue = vulkan_queue_from_handle(queue_handle);
    struct vulkan_device *device = queue->device;
    VkTimelineSemaphoreSubmitInfo *timeline_submit_info, ts_info_copy;
    VkD3D12FenceSubmitInfoKHR *d3d12_submit_info;
    const uint64_t **values;
    struct conversion_context ctx;
    VkSubmitInfo *submits;
    VkSemaphoreSubmitInfo **km_infos;
    uint32_t *km_counts;
    unsigned int i, j;
    VkResult ret;

    TRACE("(%p %u %p 0x%s)\n", queue_handle, submit_count, submits_orig, wine_dbgstr_longlong(fence));

    init_conversion_context(&ctx);
    MEMDUP(&ctx, submits, submits_orig, submit_count);
    if ((ret = process_keyed_mutexes(&ctx, device, submit_count, submits, sizeof(*submits), &km_counts, &km_infos)))
        return ret;

    for (i = 0; i < submit_count; ++i)
    {
        timeline_submit_info = find_next_struct(&submits[i], VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO);
        d3d12_submit_info = wine_vk_find_unlink_struct(&submits[i], D3D12_FENCE_SUBMIT_INFO_KHR);
        if (d3d12_submit_info && timeline_submit_info)
            WARN("Both TIMELINE_SEMAPHORE_SUBMIT_INFO and D3D12_FENCE_SUBMIT_INFO_KHR specified.\n");
        if (d3d12_submit_info && !timeline_submit_info)
        {
            timeline_submit_info = conversion_context_alloc(&ctx, sizeof(*timeline_submit_info));
            timeline_submit_info->sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
            timeline_submit_info->pNext = submits[i].pNext;
            timeline_submit_info->waitSemaphoreValueCount = d3d12_submit_info->waitSemaphoreValuesCount;
            MEMDUP(&ctx, timeline_submit_info->pWaitSemaphoreValues, d3d12_submit_info->pWaitSemaphoreValues, d3d12_submit_info->waitSemaphoreValuesCount);
            timeline_submit_info->signalSemaphoreValueCount = d3d12_submit_info->signalSemaphoreValuesCount;
            MEMDUP(&ctx, timeline_submit_info->pSignalSemaphoreValues, d3d12_submit_info->pSignalSemaphoreValues, d3d12_submit_info->signalSemaphoreValuesCount);
            submits[i].pNext = timeline_submit_info;
        }

        if (timeline_submit_info)
            values = &timeline_submit_info->pWaitSemaphoreValues;
        else
            values = NULL;
        unwrap_semaphore_array(&submits[i].pWaitSemaphores, values, submits[i].waitSemaphoreCount, &ctx, FALSE, device);

        if (timeline_submit_info)
            values = &timeline_submit_info->pSignalSemaphoreValues;
        else
            values = NULL;
        unwrap_semaphore_array(&submits[i].pSignalSemaphores, values, submits[i].signalSemaphoreCount, &ctx, TRUE, device);
        if (km_counts && km_counts[i])
        {
            if (timeline_submit_info)
            {
                ts_info_copy = *timeline_submit_info;
                timeline_submit_info = &ts_info_copy;
                duplicate_array_for_unwrapping_copy_size(&ctx, (void **)&timeline_submit_info->pSignalSemaphoreValues,
                        (timeline_submit_info->signalSemaphoreValueCount + km_counts[i]) * sizeof(*timeline_submit_info->pSignalSemaphoreValues),
                        timeline_submit_info->signalSemaphoreValueCount * sizeof(*timeline_submit_info->pSignalSemaphoreValues));
            }
            else
            {
                timeline_submit_info = &ts_info_copy;
                timeline_submit_info->sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
                timeline_submit_info->pNext = submits[i].pNext;
                timeline_submit_info->waitSemaphoreValueCount = 0;
                timeline_submit_info->signalSemaphoreValueCount = 0;
                timeline_submit_info->pSignalSemaphoreValues = conversion_context_alloc(&ctx, km_counts[i] * sizeof(*timeline_submit_info->pSignalSemaphoreValues));
                submits[i].pNext = timeline_submit_info;
            }
            duplicate_array_for_unwrapping_copy_size(&ctx, (void **)&submits[i].pSignalSemaphores,
                    (submits[i].signalSemaphoreCount + km_counts[i]) * sizeof(*submits[i].pSignalSemaphores),
                    submits[i].signalSemaphoreCount * sizeof(*submits[i].pSignalSemaphores));
            for (j = 0; j < km_counts[i]; ++j)
            {
                ((uint64_t *)timeline_submit_info->pSignalSemaphoreValues)[j + timeline_submit_info->signalSemaphoreValueCount++]
                        = km_infos[i][j].value;
                ((VkSemaphore *)submits[i].pSignalSemaphores)[j + submits[i].signalSemaphoreCount++] = km_infos[i][j].semaphore;
            }
        }

        if (submits[i].pCommandBuffers && submits[i].commandBufferCount)
        {
            VkCommandBuffer *out;

            out = conversion_context_alloc(&ctx, submits[i].commandBufferCount * sizeof(*out));
            for (j = 0; j < submits[i].commandBufferCount; ++j)
                out[j] = wine_cmd_buffer_from_handle(submits[i].pCommandBuffers[j])->host.command_buffer;
            submits[i].pCommandBuffers = out;
        }
    }
    ret = queue->device->p_vkQueueSubmit(queue->host.queue, submit_count, submits, fence);
    free_conversion_context(&ctx);
    return ret;
}

static void duplicate_array_for_unwrapping(struct conversion_context *ctx, void **ptr, unsigned int size)
{
    duplicate_array_for_unwrapping_copy_size(ctx, ptr, size, size);
}

static VkResult vk_queue_submit_2(VkQueue queue_handle, uint32_t submit_count, const VkSubmitInfo2 *submits_orig,
        VkFence fence, bool khr)
{
    struct vulkan_queue *queue = vulkan_queue_from_handle(queue_handle);
    struct vulkan_device *device = queue->device;
    struct conversion_context ctx;
    VkSemaphoreSubmitInfo **km_infos;
    uint32_t *km_counts, count;
    VkSubmitInfo2 *submits;
    unsigned int i, j;
    VkResult ret;

    TRACE("(%p, %u, %p, %s)\n", queue_handle, submit_count, submits_orig, wine_dbgstr_longlong(fence));

    init_conversion_context(&ctx);
    MEMDUP(&ctx, submits, submits_orig, submit_count);
    if ((ret = process_keyed_mutexes(&ctx, device, submit_count, submits, sizeof(*submits), &km_counts, &km_infos)))
        return ret;
    for (i = 0; i < submit_count; ++i)
    {
        duplicate_array_for_unwrapping(&ctx, (void **)&submits[i].pWaitSemaphoreInfos,
                submits[i].waitSemaphoreInfoCount * sizeof(*submits[i].pWaitSemaphoreInfos));
        for (j = 0; j < submits[i].waitSemaphoreInfoCount; ++j)
            unwrap_semaphore(queue->device, &((VkSemaphoreSubmitInfo *)submits[i].pWaitSemaphoreInfos)[j].semaphore,
                    &((VkSemaphoreSubmitInfo *)submits[i].pWaitSemaphoreInfos)[j].value, FALSE);

        count = submits[i].signalSemaphoreInfoCount + (km_counts ? km_counts[i] : 0);
        duplicate_array_for_unwrapping_copy_size(&ctx, (void **)&submits[i].pSignalSemaphoreInfos,
                count * sizeof(*submits[i].pSignalSemaphoreInfos),
                submits[i].signalSemaphoreInfoCount * sizeof(*submits[i].pSignalSemaphoreInfos));
        for (j = 0; j < submits[i].signalSemaphoreInfoCount; ++j)
            unwrap_semaphore(queue->device, &((VkSemaphoreSubmitInfo *)submits[i].pSignalSemaphoreInfos)[j].semaphore,
                    &((VkSemaphoreSubmitInfo *)submits[i].pSignalSemaphoreInfos)[j].value, TRUE);
        for (; j < count; ++j)
            ((VkSemaphoreSubmitInfo *)submits[i].pSignalSemaphoreInfos)[j] = km_infos[i][j - submits[i].signalSemaphoreInfoCount];
        submits[i].signalSemaphoreInfoCount = count;

        if (submits[i].pCommandBufferInfos && submits[i].commandBufferInfoCount)
        {
            duplicate_array_for_unwrapping(&ctx, (void **)&submits[i].pCommandBufferInfos,
                    submits[i].commandBufferInfoCount * sizeof(*submits[i].pCommandBufferInfos));
            for (j = 0; j < submits[i].commandBufferInfoCount; ++j)
                ((VkCommandBufferSubmitInfo *)submits[i].pCommandBufferInfos)[j].commandBuffer
                        = wine_cmd_buffer_from_handle(submits[i].pCommandBufferInfos[j].commandBuffer)->host.command_buffer;
        }
    }

    if (khr)
        ret = queue->device->p_vkQueueSubmit2KHR(queue->host.queue, submit_count, submits, fence);
    else
        ret = queue->device->p_vkQueueSubmit2(queue->host.queue, submit_count, submits, fence);
    free_conversion_context(&ctx);
    return ret;
}

VkResult wine_vkQueueSubmit2(VkQueue queue, uint32_t submit_count, const VkSubmitInfo2 *submits, VkFence fence)
{
    return vk_queue_submit_2(queue, submit_count, submits, fence, false);
}

VkResult wine_vkQueueSubmit2KHR(VkQueue queue, uint32_t submit_count, const VkSubmitInfo2 *submits, VkFence fence)
{
    return vk_queue_submit_2(queue, submit_count, submits, fence, true);
}

VkResult wine_vkQueueBindSparse(VkQueue queue_handle, uint32_t bind_info_count, const VkBindSparseInfo *bind_info, VkFence fence)
{
    struct vulkan_queue *queue = vulkan_queue_from_handle(queue_handle);
    struct wine_semaphore *semaphore;
    struct conversion_context ctx;
    VkBindSparseInfo *batch;
    unsigned int i, j, k;
    VkResult ret;

    TRACE("(%p, %u, %p, 0x%s)\n", queue, bind_info_count, bind_info, wine_dbgstr_longlong(fence));

    for (i = 0; i < bind_info_count; i++)
    {
        batch = (VkBindSparseInfo *)&bind_info[i];

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

    init_conversion_context(&ctx);
    for (i = 0; i < bind_info_count; ++i)
    {
        batch = (VkBindSparseInfo *)&bind_info[i];
        unwrap_semaphore_array(&batch->pWaitSemaphores, NULL, batch->waitSemaphoreCount, &ctx, FALSE, queue->device);
        unwrap_semaphore_array(&batch->pSignalSemaphores, NULL, batch->signalSemaphoreCount, &ctx, TRUE, queue->device);

        duplicate_array_for_unwrapping(&ctx, (void **)&batch->pBufferBinds, batch->bufferBindCount * sizeof(*batch->pBufferBinds));
        for (j = 0; j < batch->bufferBindCount; ++j)
        {
            VkSparseBufferMemoryBindInfo *bind = (VkSparseBufferMemoryBindInfo *)&batch->pBufferBinds[j];
            duplicate_array_for_unwrapping(&ctx, (void **)&bind->pBinds, bind->bindCount * sizeof(*bind->pBinds));
            for (k = 0; k < bind->bindCount; ++k)
                if (bind->pBinds[k].memory)
                    ((VkSparseMemoryBind *)bind->pBinds)[k].memory = wine_device_memory_from_handle(bind->pBinds[k].memory)->host.device_memory;
        }

        duplicate_array_for_unwrapping(&ctx, (void **)&batch->pImageOpaqueBinds, batch->imageOpaqueBindCount * sizeof(*batch->pImageOpaqueBinds));
        for (j = 0; j < batch->imageOpaqueBindCount; ++j)
        {
            VkSparseImageOpaqueMemoryBindInfo *bind = (VkSparseImageOpaqueMemoryBindInfo *)&batch->pImageOpaqueBinds[j];
            duplicate_array_for_unwrapping(&ctx, (void **)&bind->pBinds, bind->bindCount * sizeof(*bind->pBinds));
            for (k = 0; k < bind->bindCount; ++k)
                if (bind->pBinds[k].memory)
                    ((VkSparseMemoryBind *)bind->pBinds)[k].memory = wine_device_memory_from_handle(bind->pBinds[k].memory)->host.device_memory;
        }

        duplicate_array_for_unwrapping(&ctx, (void **)&batch->pImageBinds, batch->imageBindCount * sizeof(*batch->pImageBinds));
        for (j = 0; j < batch->imageBindCount; ++j)
        {
            VkSparseImageMemoryBindInfo *bind = (VkSparseImageMemoryBindInfo *)&batch->pImageBinds[j];
            duplicate_array_for_unwrapping(&ctx, (void **)&bind->pBinds, bind->bindCount * sizeof(*bind->pBinds));
            for (k = 0; k < bind->bindCount; ++k)
                if (bind->pBinds[k].memory)
                    ((VkSparseImageMemoryBind *)bind->pBinds)[k].memory = wine_device_memory_from_handle(bind->pBinds[k].memory)->host.device_memory;
        }
    }
    ret = queue->device->p_vkQueueBindSparse(queue->host.queue, bind_info_count, bind_info, fence);
    free_conversion_context(&ctx);
    return ret;
}

VkResult wine_wine_vkAcquireKeyedMutex(VkDevice device, VkDeviceMemory memory, uint64_t key, uint32_t timeout_ms)
{
    return acquire_keyed_mutex(vulkan_device_from_handle(device), wine_device_memory_from_handle(memory), key, timeout_ms);
}

VkResult wine_wine_vkReleaseKeyedMutex(VkDevice device, VkDeviceMemory memory, uint64_t key)
{
    return release_keyed_mutex(vulkan_device_from_handle(device), wine_device_memory_from_handle(memory), key, NULL);
}

static void fixup_device_id(UINT *vendor_id, UINT *device_id)
{
    const char *sgi;

    if (*vendor_id == 0x10de /* NVIDIA */ && (sgi = getenv("WINE_HIDE_NVIDIA_GPU")) && *sgi != '0')
    {
        *vendor_id = 0x1002; /* AMD */
        *device_id = 0x73df; /* RX 6700XT */
    }
    else if (*vendor_id == 0x1002 /* AMD */ && (sgi = getenv("WINE_HIDE_AMD_GPU")) && *sgi != '0')
    {
        *vendor_id = 0x10de; /* NVIDIA */
        *device_id = 0x2487; /* RTX 3060 */
    }
    else if (*vendor_id == 0x1002 && (*device_id == 0x163f || *device_id == 0x1435) && (sgi = getenv("WINE_HIDE_VANGOGH_GPU")) && *sgi != '0')
    {
        *device_id = 0x687f; /* Radeon RX Vega 56/64 */
    }
    else if (*vendor_id == 0x8086 /* Intel */ && (sgi = getenv("WINE_HIDE_INTEL_GPU")) && *sgi != '0')
    {
        *vendor_id = 0x1002; /* AMD */
        *device_id = 0x73df; /* RX 6700XT */
    }
}

void wine_vkGetPhysicalDeviceProperties(VkPhysicalDevice client_physical_device,
        VkPhysicalDeviceProperties *properties)
{
    struct wine_phys_dev *phys_dev = wine_phys_dev_from_handle(client_physical_device);

    TRACE("%p, %p\n", phys_dev, properties);

    phys_dev->obj.instance->p_vkGetPhysicalDeviceProperties(phys_dev->obj.host.physical_device, properties);
    fixup_device_id(&properties->vendorID, &properties->deviceID);
}

void wine_vkGetPhysicalDeviceProperties2(VkPhysicalDevice client_physical_device,
        VkPhysicalDeviceProperties2 *properties)
{
    struct wine_phys_dev *phys_dev = wine_phys_dev_from_handle(client_physical_device);

    TRACE("%p, %p\n", phys_dev, properties);

    phys_dev->obj.instance->p_vkGetPhysicalDeviceProperties2(phys_dev->obj.host.physical_device, properties);
    fixup_device_id(&properties->properties.vendorID, &properties->properties.deviceID);
}

void wine_vkGetPhysicalDeviceProperties2KHR(VkPhysicalDevice client_physical_device,
        VkPhysicalDeviceProperties2 *properties)
{
    struct wine_phys_dev *phys_dev = wine_phys_dev_from_handle(client_physical_device);

    TRACE("%p, %p\n", phys_dev, properties);

    phys_dev->obj.instance->p_vkGetPhysicalDeviceProperties2KHR(phys_dev->obj.host.physical_device, properties);
    fixup_device_id(&properties->properties.vendorID, &properties->properties.deviceID);
}

DECLSPEC_EXPORT VkDevice __wine_get_native_VkDevice(VkDevice handle)
{
    struct vulkan_device *device = vulkan_device_from_handle(handle);

    return device->host.device;
}

DECLSPEC_EXPORT VkInstance __wine_get_native_VkInstance(VkInstance handle)
{
    struct vulkan_instance *instance = vulkan_instance_from_handle(handle);

    return instance->host.instance;
}

DECLSPEC_EXPORT VkPhysicalDevice __wine_get_native_VkPhysicalDevice(VkPhysicalDevice handle)
{
    struct vulkan_physical_device *phys_dev;

    if (!handle) return NULL;

    phys_dev = vulkan_physical_device_from_handle(handle);
    return phys_dev->host.physical_device;
}

DECLSPEC_EXPORT VkQueue __wine_get_native_VkQueue(VkQueue handle)
{
    struct vulkan_queue *queue = vulkan_queue_from_handle(handle);

    return queue->host.queue;
}

DECLSPEC_EXPORT VkPhysicalDevice __wine_get_wrapped_VkPhysicalDevice(VkInstance handle, VkPhysicalDevice native_phys_dev)
{
    struct wine_instance *instance = wine_instance_from_handle(handle);
    unsigned int i;

    for (i = 0; i < instance->phys_dev_count; ++i)
    {
        if (instance->phys_devs[i].obj.host.physical_device == native_phys_dev)
            return instance->phys_devs[i].obj.client.physical_device;
    }

    ERR("Unknown native physical device: %p, instance %p, handle %p\n", native_phys_dev, instance, handle);
    return NULL;
}
