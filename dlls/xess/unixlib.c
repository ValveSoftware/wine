/*
 * XeSS Unix library
 *
 * Copyright 2025 the Wine project
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

#include <stdarg.h>
#include <stdlib.h>
#include <dlfcn.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winbase.h"
#include "winternl.h"

#include "wine/debug.h"
#include "unixlib.h"

WINE_DEFAULT_DEBUG_CHANNEL(xess);
WINE_DECLARE_DEBUG_CHANNEL(winediag);

/* helper functions to unwrap handles */
static VkInstance get_host_instance(VkInstance wine_instance)
{
    struct vulkan_instance *instance;
    if (!wine_instance) return NULL;
    instance = vulkan_instance_from_handle(wine_instance);
    return instance->host.instance;
}

static VkPhysicalDevice get_host_physical_device(VkPhysicalDevice wine_phys_dev)
{
    struct vulkan_physical_device *phys_dev;
    if (!wine_phys_dev) return NULL;
    phys_dev = vulkan_physical_device_from_handle(wine_phys_dev);
    return phys_dev->host.physical_device;
}

static VkDevice get_host_device(VkDevice wine_device)
{
    struct vulkan_device *device;
    if (!wine_device) return NULL;
    device = vulkan_device_from_handle(wine_device);
    return device->host.device;
}

static VkCommandBuffer get_host_command_buffer(VkCommandBuffer wine_cmd_buffer)
{
    struct vulkan_command_buffer *cmd_buffer;
    if (!wine_cmd_buffer) return NULL;
    cmd_buffer = vulkan_command_buffer_from_handle(wine_cmd_buffer);
    return cmd_buffer->host.command_buffer;
}

static VkDeviceMemory get_host_device_memory(VkDeviceMemory wine_device_memory)
{
    struct vulkan_device_memory *device_memory;
    if (!wine_device_memory) return VK_NULL_HANDLE;
    device_memory = vulkan_device_memory_from_handle(wine_device_memory);
    return device_memory->host.device_memory;
}

struct xess_context_init_params_entry
{
    xess_context_handle_t context;
    VkDeviceMemory temp_buffer_heap;
    VkDeviceMemory temp_texture_heap;
};

static struct xess_context_init_params_entry *xess_context_init_params_entries;
static size_t xess_context_init_params_entry_count;
static size_t xess_context_init_params_entry_capacity;

static struct xess_context_init_params_entry *find_xess_context_init_params_entry( xess_context_handle_t context )
{
    size_t i;

    for (i = 0; i < xess_context_init_params_entry_count; ++i)
    {
        if (xess_context_init_params_entries[i].context == context)
            return &xess_context_init_params_entries[i];
    }

    return NULL;
}

static BOOL store_xess_context_init_params_entry( xess_context_handle_t context,
                                                   VkDeviceMemory temp_buffer_heap,
                                                   VkDeviceMemory temp_texture_heap )
{
    struct xess_context_init_params_entry *entry;

    entry = find_xess_context_init_params_entry( context );
    if (entry)
    {
        entry->temp_buffer_heap = temp_buffer_heap;
        entry->temp_texture_heap = temp_texture_heap;
        return TRUE;
    }

    if (xess_context_init_params_entry_count == xess_context_init_params_entry_capacity)
    {
        size_t new_capacity = xess_context_init_params_entry_capacity ? xess_context_init_params_entry_capacity * 2 : 8;
        struct xess_context_init_params_entry *new_entries;

        new_entries = realloc( xess_context_init_params_entries, new_capacity * sizeof(*new_entries) );
        if (!new_entries)
            return FALSE;

        xess_context_init_params_entries = new_entries;
        xess_context_init_params_entry_capacity = new_capacity;
    }

    entry = &xess_context_init_params_entries[xess_context_init_params_entry_count++];
    entry->context = context;
    entry->temp_buffer_heap = temp_buffer_heap;
    entry->temp_texture_heap = temp_texture_heap;
    return TRUE;
}

static void remove_xess_context_init_params_entry( xess_context_handle_t context )
{
    size_t i;

    for (i = 0; i < xess_context_init_params_entry_count; ++i)
    {
        if (xess_context_init_params_entries[i].context == context)
        {
            xess_context_init_params_entries[i] = xess_context_init_params_entries[--xess_context_init_params_entry_count];
            return;
        }
    }
}

static void *override_library = NULL;

static xess_result_t (*p_xessDestroyContext)(xess_context_handle_t);
static xess_result_t (*p_xessGetVersion)(xess_version_t *);
static xess_result_t (*p_xessGetIntelXeFXVersion)(xess_context_handle_t, xess_version_t *);
static xess_result_t (*p_xessGetProperties)(xess_context_handle_t, const xess_2d_t *, xess_properties_t *);
static xess_result_t (*p_xessGetInputResolution)(xess_context_handle_t, const xess_2d_t *, xess_quality_settings_t, xess_2d_t *);
static xess_result_t (*p_xessGetOptimalInputResolution)(xess_context_handle_t, const xess_2d_t *, xess_quality_settings_t, xess_2d_t *, xess_2d_t *, xess_2d_t *);
static xess_result_t (*p_xessGetJitterScale)(xess_context_handle_t, float *, float *);
static xess_result_t (*p_xessGetVelocityScale)(xess_context_handle_t, float *, float *);
static xess_result_t (*p_xessGetExposureMultiplier)(xess_context_handle_t, float *);
static xess_result_t (*p_xessGetMaxResponsiveMaskValue)(xess_context_handle_t, float *);
static xess_result_t (*p_xessSetVelocityScale)(xess_context_handle_t, float, float);
static xess_result_t (*p_xessSetJitterScale)(xess_context_handle_t, float, float);
static xess_result_t (*p_xessSetExposureMultiplier)(xess_context_handle_t, float);
static xess_result_t (*p_xessSetMaxResponsiveMaskValue)(xess_context_handle_t, float);
static xess_result_t (*p_xessSetContextParameterF)(xess_context_handle_t, uint32_t, float);
static xess_result_t (*p_xessSetLoggingCallback)(xess_context_handle_t, xess_logging_level_t, xess_app_log_callback_t);
static xess_result_t (*p_xessIsOptimalDriver)(xess_context_handle_t);
static xess_result_t (*p_xessForceLegacyScaleFactors)(xess_context_handle_t, bool);
static xess_result_t (*p_xessGetPipelineBuildStatus)(xess_context_handle_t);
static xess_result_t (*p_xessSelectNetworkModel)(xess_context_handle_t, xess_network_model_t);
static xess_result_t (*p_xessStartDump)(xess_context_handle_t, const xess_dump_parameters_t *);
static xess_result_t (*p_xessGetProfilingData)(xess_context_handle_t, xess_profiling_data_t **);

static xess_result_t (*p_xessVKGetRequiredInstanceExtensions)(uint32_t*, const char* const**, uint32_t*);
static xess_result_t (*p_xessVKGetRequiredDeviceExtensions)(VkInstance, VkPhysicalDevice, uint32_t*, const char* const**);
static xess_result_t (*p_xessVKGetRequiredDeviceFeatures)(VkInstance, VkPhysicalDevice, void**);
static xess_result_t (*p_xessVKCreateContext)(VkInstance, VkPhysicalDevice, VkDevice, xess_context_handle_t *);
static xess_result_t (*p_xessVKBuildPipelines)(xess_context_handle_t, VkPipelineCache, bool, uint32_t);
static xess_result_t (*p_xessVKInit)(xess_context_handle_t, const xess_vk_init_params_t *);
static xess_result_t (*p_xessVKGetInitParams)(xess_context_handle_t, xess_vk_init_params_t *);
static xess_result_t (*p_xessVKExecute)(xess_context_handle_t, VkCommandBuffer, const xess_vk_execute_params_t*);

static NTSTATUS xess_init( void *args )
{
    struct xess_init_params *params = args;
    const char *xess_lib_path;

    TRACE("Initializing XeSS translation...\n");

    if (override_library)
    {
        params->result = XESS_RESULT_SUCCESS;
        return STATUS_SUCCESS;
    }

    /* Allow user to specify a custom XeSS implementation */
    xess_lib_path = getenv("XESS_LIB_OVERRIDE");
    if (!xess_lib_path)
    {
        /* Default path of "libxess.so" would clash with the Unixlib name. */
        xess_lib_path = "libxess_override.so";
    }

    TRACE("Loading XeSS implementation: %s\n", xess_lib_path);

    override_library = dlopen(xess_lib_path, RTLD_NOW);
    if (!override_library)
    {
        ERR_(winediag)("Failed to load XeSS library '%s': %s\n", xess_lib_path, dlerror());
        ERR_(winediag)("Please set XESS_LIB_OVERRIDE environment variable or install libxess_override.so\n");
        params->result = XESS_RESULT_ERROR_CANT_LOAD_LIBRARY;
        return STATUS_SUCCESS;
    }

#define LOAD_FUNCPTR(f) \
    if (!(p_##f = dlsym( override_library, #f ))) \
    { \
        ERR("Failed to load function '%s': %s\n", #f, dlerror()); \
        goto fail; \
    }

    /* Common API functions */
    LOAD_FUNCPTR(xessDestroyContext)
    LOAD_FUNCPTR(xessGetVersion)
    LOAD_FUNCPTR(xessGetIntelXeFXVersion)
    LOAD_FUNCPTR(xessGetProperties)
    LOAD_FUNCPTR(xessGetInputResolution)
    LOAD_FUNCPTR(xessGetOptimalInputResolution)
    LOAD_FUNCPTR(xessGetJitterScale)
    LOAD_FUNCPTR(xessGetVelocityScale)
    LOAD_FUNCPTR(xessGetExposureMultiplier)
    LOAD_FUNCPTR(xessGetMaxResponsiveMaskValue)
    LOAD_FUNCPTR(xessSetVelocityScale)
    LOAD_FUNCPTR(xessSetJitterScale)
    LOAD_FUNCPTR(xessSetExposureMultiplier)
    LOAD_FUNCPTR(xessSetMaxResponsiveMaskValue)
    LOAD_FUNCPTR(xessSetContextParameterF)
    LOAD_FUNCPTR(xessSetLoggingCallback)
    LOAD_FUNCPTR(xessIsOptimalDriver)
    LOAD_FUNCPTR(xessForceLegacyScaleFactors)
    LOAD_FUNCPTR(xessGetPipelineBuildStatus)
    LOAD_FUNCPTR(xessSelectNetworkModel)
    LOAD_FUNCPTR(xessStartDump)
    LOAD_FUNCPTR(xessGetProfilingData)

    /* Vulkan-specific functions */
    LOAD_FUNCPTR(xessVKGetRequiredInstanceExtensions)
    LOAD_FUNCPTR(xessVKGetRequiredDeviceExtensions)
    LOAD_FUNCPTR(xessVKGetRequiredDeviceFeatures)
    LOAD_FUNCPTR(xessVKCreateContext)
    LOAD_FUNCPTR(xessVKBuildPipelines)
    LOAD_FUNCPTR(xessVKInit)
    LOAD_FUNCPTR(xessVKGetInitParams)
    LOAD_FUNCPTR(xessVKExecute)
#undef LOAD_FUNCPTR

    TRACE("Vulkan XeSS implementation loaded successfully\n");
    params->result = XESS_RESULT_SUCCESS;
    return STATUS_SUCCESS;

fail:
    dlclose(override_library);
    override_library = NULL;
    params->result = XESS_RESULT_ERROR_CANT_LOAD_LIBRARY;
    return STATUS_SUCCESS;
}

static NTSTATUS xess_destroy_context( void *args )
{
    struct xess_destroy_context_params *params = args;
    if (!override_library) { params->result = XESS_RESULT_ERROR_CANT_LOAD_LIBRARY; return STATUS_SUCCESS; }
    params->result = p_xessDestroyContext( params->hContext );
    if (params->result == XESS_RESULT_SUCCESS)
        remove_xess_context_init_params_entry( params->hContext );
    return STATUS_SUCCESS;
}

static NTSTATUS xess_get_version( void *args )
{
    struct xess_get_version_params *params = args;
    if (!override_library) { params->result = XESS_RESULT_ERROR_CANT_LOAD_LIBRARY; return STATUS_SUCCESS; }
    params->result = p_xessGetVersion( params->pVersion );
    return STATUS_SUCCESS;
}

static NTSTATUS xess_get_intel_xefx_version( void *args )
{
    struct xess_get_intel_xefx_version_params *params = args;
    if (!override_library) { params->result = XESS_RESULT_ERROR_CANT_LOAD_LIBRARY; return STATUS_SUCCESS; }
    params->result = p_xessGetIntelXeFXVersion( params->hContext, params->pVersion );
    return STATUS_SUCCESS;
}

static NTSTATUS xess_get_properties( void *args )
{
    struct xess_get_properties_params *params = args;
    if (!override_library) { params->result = XESS_RESULT_ERROR_CANT_LOAD_LIBRARY; return STATUS_SUCCESS; }
    params->result = p_xessGetProperties( params->hContext, params->pOutputResolution, params->pProperties );
    return STATUS_SUCCESS;
}

static NTSTATUS xess_get_input_resolution( void *args )
{
    struct xess_get_input_resolution_params *params = args;
    if (!override_library) { params->result = XESS_RESULT_ERROR_CANT_LOAD_LIBRARY; return STATUS_SUCCESS; }
    params->result = p_xessGetInputResolution( params->hContext, params->pOutputResolution, params->qualitySetting, params->pInputResolution );
    return STATUS_SUCCESS;
}

static NTSTATUS xess_get_optimal_input_resolution( void *args )
{
    struct xess_get_optimal_input_resolution_params *params = args;
    if (!override_library) { params->result = XESS_RESULT_ERROR_CANT_LOAD_LIBRARY; return STATUS_SUCCESS; }
    params->result = p_xessGetOptimalInputResolution( params->hContext, params->pOutputResolution, params->qualitySetting,
                                                       params->pMinResolution, params->pMaxResolution, params->pOptimalResolution );
    return STATUS_SUCCESS;
}

static NTSTATUS xess_get_jitter_scale( void *args )
{
    struct xess_get_jitter_scale_params *params = args;
    if (!override_library) { params->result = XESS_RESULT_ERROR_CANT_LOAD_LIBRARY; return STATUS_SUCCESS; }
    params->result = p_xessGetJitterScale( params->hContext, params->pX, params->pY );
    return STATUS_SUCCESS;
}

static NTSTATUS xess_get_velocity_scale( void *args )
{
    struct xess_get_velocity_scale_params *params = args;
    if (!override_library) { params->result = XESS_RESULT_ERROR_CANT_LOAD_LIBRARY; return STATUS_SUCCESS; }
    params->result = p_xessGetVelocityScale( params->hContext, params->pX, params->pY );
    return STATUS_SUCCESS;
}

static NTSTATUS xess_get_exposure_multiplier( void *args )
{
    struct xess_get_exposure_multiplier_params *params = args;
    if (!override_library) { params->result = XESS_RESULT_ERROR_CANT_LOAD_LIBRARY; return STATUS_SUCCESS; }
    params->result = p_xessGetExposureMultiplier( params->hContext, params->pScale );
    return STATUS_SUCCESS;
}

static NTSTATUS xess_get_max_responsive_mask_value( void *args )
{
    struct xess_get_max_responsive_mask_value_params *params = args;
    if (!override_library) { params->result = XESS_RESULT_ERROR_CANT_LOAD_LIBRARY; return STATUS_SUCCESS; }
    params->result = p_xessGetMaxResponsiveMaskValue( params->hContext, params->pMaxValue );
    return STATUS_SUCCESS;
}

static NTSTATUS xess_set_velocity_scale( void *args )
{
    struct xess_set_velocity_scale_params *params = args;
    if (!override_library) { params->result = XESS_RESULT_ERROR_CANT_LOAD_LIBRARY; return STATUS_SUCCESS; }
    params->result = p_xessSetVelocityScale( params->hContext, params->x, params->y );
    return STATUS_SUCCESS;
}

static NTSTATUS xess_set_jitter_scale( void *args )
{
    struct xess_set_jitter_scale_params *params = args;
    if (!override_library) { params->result = XESS_RESULT_ERROR_CANT_LOAD_LIBRARY; return STATUS_SUCCESS; }
    params->result = p_xessSetJitterScale( params->hContext, params->x, params->y );
    return STATUS_SUCCESS;
}

static NTSTATUS xess_set_exposure_multiplier( void *args )
{
    struct xess_set_exposure_multiplier_params *params = args;
    if (!override_library) { params->result = XESS_RESULT_ERROR_CANT_LOAD_LIBRARY; return STATUS_SUCCESS; }
    params->result = p_xessSetExposureMultiplier( params->hContext, params->scale );
    return STATUS_SUCCESS;
}

static NTSTATUS xess_set_max_responsive_mask_value( void *args )
{
    struct xess_set_max_responsive_mask_value_params *params = args;
    if (!override_library) { params->result = XESS_RESULT_ERROR_CANT_LOAD_LIBRARY; return STATUS_SUCCESS; }
    params->result = p_xessSetMaxResponsiveMaskValue( params->hContext, params->maxValue );
    return STATUS_SUCCESS;
}

static NTSTATUS xess_set_context_parameter_f( void *args )
{
    struct xess_set_context_parameter_f_params *params = args;
    if (!override_library) { params->result = XESS_RESULT_ERROR_CANT_LOAD_LIBRARY; return STATUS_SUCCESS; }
    params->result = p_xessSetContextParameterF( params->hContext, params->param, params->value );
    return STATUS_SUCCESS;
}

static NTSTATUS xess_set_logging_callback( void *args )
{
    struct xess_set_logging_callback_params *params = args;
    if (!override_library) { params->result = XESS_RESULT_ERROR_CANT_LOAD_LIBRARY; return STATUS_SUCCESS; }
    params->result = p_xessSetLoggingCallback( params->hContext, params->loggingLevel, params->loggingFunction );
    return STATUS_SUCCESS;
}

static NTSTATUS xess_is_optimal_driver( void *args )
{
    struct xess_is_optimal_driver_params *params = args;
    if (!override_library) { params->result = XESS_RESULT_ERROR_CANT_LOAD_LIBRARY; return STATUS_SUCCESS; }
    params->result = p_xessIsOptimalDriver( params->hContext );
    return STATUS_SUCCESS;
}

static NTSTATUS xess_force_legacy_scale_factors( void *args )
{
    struct xess_force_legacy_scale_factors_params *params = args;
    if (!override_library) { params->result = XESS_RESULT_ERROR_CANT_LOAD_LIBRARY; return STATUS_SUCCESS; }
    params->result = p_xessForceLegacyScaleFactors( params->hContext, params->force );
    return STATUS_SUCCESS;
}

static NTSTATUS xess_get_pipeline_build_status( void *args )
{
    struct xess_get_pipeline_build_status_params *params = args;
    if (!override_library) { params->result = XESS_RESULT_ERROR_CANT_LOAD_LIBRARY; return STATUS_SUCCESS; }
    params->result = p_xessGetPipelineBuildStatus( params->hContext );
    return STATUS_SUCCESS;
}

static NTSTATUS xess_select_network_model( void *args )
{
    struct xess_select_network_model_params *params = args;
    if (!override_library) { params->result = XESS_RESULT_ERROR_CANT_LOAD_LIBRARY; return STATUS_SUCCESS; }
    params->result = p_xessSelectNetworkModel( params->hContext, params->network );
    return STATUS_SUCCESS;
}

static NTSTATUS xess_start_dump( void *args )
{
    struct xess_start_dump_params *params = args;
    if (!override_library) { params->result = XESS_RESULT_ERROR_CANT_LOAD_LIBRARY; return STATUS_SUCCESS; }
    params->result = p_xessStartDump( params->hContext, params->dump_parameters );
    return STATUS_SUCCESS;
}

static NTSTATUS xess_get_profiling_data( void *args )
{
    struct xess_get_profiling_data_params *params = args;
    if (!override_library) { params->result = XESS_RESULT_ERROR_CANT_LOAD_LIBRARY; return STATUS_SUCCESS; }
    params->result = p_xessGetProfilingData( params->hContext, params->pProfilingData );
    return STATUS_SUCCESS;
}

static NTSTATUS xess_vk_get_required_instance_extensions( void *args )
{
    struct xess_vk_get_required_instance_extensions_params *params = args;
    if (!override_library) { params->result = XESS_RESULT_ERROR_CANT_LOAD_LIBRARY; return STATUS_SUCCESS; }
    params->result = p_xessVKGetRequiredInstanceExtensions( params->instanceExtensionsCount, params->instanceExtensions, params->minVkApiVersion );
    return STATUS_SUCCESS;
}

static NTSTATUS xess_vk_get_required_device_extensions( void *args )
{
    struct xess_vk_get_required_device_extensions_params *params = args;
    if (!override_library) { params->result = XESS_RESULT_ERROR_CANT_LOAD_LIBRARY; return STATUS_SUCCESS; }
    params->result = p_xessVKGetRequiredDeviceExtensions( get_host_instance(params->instance), get_host_physical_device(params->physicalDevice), params->deviceExtensionsCount, params->deviceExtensions );
    return STATUS_SUCCESS;
}

static NTSTATUS xess_vk_get_required_device_features( void *args )
{
    struct xess_vk_get_required_device_features_params *params = args;
    if (!override_library) { params->result = XESS_RESULT_ERROR_CANT_LOAD_LIBRARY; return STATUS_SUCCESS; }
    params->result = p_xessVKGetRequiredDeviceFeatures( get_host_instance(params->instance), get_host_physical_device(params->physicalDevice), params->features );
    return STATUS_SUCCESS;
}

static NTSTATUS xess_vk_create_context( void *args )
{
    struct xess_vk_create_context_params *params = args;
    if (!override_library) { params->result = XESS_RESULT_ERROR_CANT_LOAD_LIBRARY; return STATUS_SUCCESS; }
    params->result = p_xessVKCreateContext( get_host_instance(params->instance), get_host_physical_device(params->physicalDevice), get_host_device(params->device), params->phContext );
    return STATUS_SUCCESS;
}

static NTSTATUS xess_vk_build_pipelines( void *args )
{
    struct xess_vk_build_pipelines_params *params = args;
    if (!override_library) { params->result = XESS_RESULT_ERROR_CANT_LOAD_LIBRARY; return STATUS_SUCCESS; }
    params->result = p_xessVKBuildPipelines( params->hContext, params->pipelineCache, params->blocking, params->initFlags );
    return STATUS_SUCCESS;
}

static NTSTATUS xess_vk_init( void *args )
{
    struct xess_vk_init_params *params = args;
    const xess_vk_init_params_t *init_params;
    VkDeviceMemory client_temp_buffer_heap = VK_NULL_HANDLE;
    VkDeviceMemory client_temp_texture_heap = VK_NULL_HANDLE;
    xess_vk_init_params_t host_init_params;

    if (!override_library) { params->result = XESS_RESULT_ERROR_CANT_LOAD_LIBRARY; return STATUS_SUCCESS; }

    init_params = params->pInitParams;
    if (init_params)
    {
        client_temp_buffer_heap = init_params->tempBufferHeap;
        client_temp_texture_heap = init_params->tempTextureHeap;
        host_init_params = *init_params;
        host_init_params.tempBufferHeap = get_host_device_memory( init_params->tempBufferHeap );
        host_init_params.tempTextureHeap = get_host_device_memory( init_params->tempTextureHeap );
        init_params = &host_init_params;
    }

    params->result = p_xessVKInit( params->hContext, init_params );
    if (params->result == XESS_RESULT_SUCCESS && params->pInitParams)
    {
        if (!store_xess_context_init_params_entry( params->hContext, client_temp_buffer_heap, client_temp_texture_heap ))
            ERR("Failed to store XeSS context init params for %p\n", params->hContext);
    }
    return STATUS_SUCCESS;
}

static NTSTATUS xess_vk_get_init_params( void *args )
{
    struct xess_vk_get_init_params_params *params = args;
    struct xess_context_init_params_entry *entry;

    if (!override_library) { params->result = XESS_RESULT_ERROR_CANT_LOAD_LIBRARY; return STATUS_SUCCESS; }
    params->result = p_xessVKGetInitParams( params->hContext, params->pInitParams );
    if (params->result == XESS_RESULT_SUCCESS && params->pInitParams)
    {
        entry = find_xess_context_init_params_entry( params->hContext );
        if (entry)
        {
            params->pInitParams->tempBufferHeap = entry->temp_buffer_heap;
            params->pInitParams->tempTextureHeap = entry->temp_texture_heap;
        }
    }
    return STATUS_SUCCESS;
}

static NTSTATUS xess_vk_execute( void *args )
{
    struct xess_vk_execute_params *params = args;
    if (!override_library) { params->result = XESS_RESULT_ERROR_CANT_LOAD_LIBRARY; return STATUS_SUCCESS; }
    params->result = p_xessVKExecute( params->hContext, get_host_command_buffer(params->pCommandBuffer), params->pExecParams );
    return STATUS_SUCCESS;
}

/* mapping from xess_funcs enum to implementations */
const unixlib_entry_t __wine_unix_call_funcs[] =
{
    xess_init,
    xess_destroy_context,
    xess_get_version,
    xess_get_intel_xefx_version,
    xess_get_properties,
    xess_get_input_resolution,
    xess_get_optimal_input_resolution,
    xess_get_jitter_scale,
    xess_get_velocity_scale,
    xess_get_exposure_multiplier,
    xess_get_max_responsive_mask_value,
    xess_set_velocity_scale,
    xess_set_jitter_scale,
    xess_set_exposure_multiplier,
    xess_set_max_responsive_mask_value,
    xess_set_context_parameter_f,
    xess_set_logging_callback,
    xess_is_optimal_driver,
    xess_force_legacy_scale_factors,
    xess_get_pipeline_build_status,
    xess_select_network_model,
    xess_start_dump,
    xess_get_profiling_data,
    xess_vk_get_required_instance_extensions,
    xess_vk_get_required_device_extensions,
    xess_vk_get_required_device_features,
    xess_vk_create_context,
    xess_vk_build_pipelines,
    xess_vk_init,
    xess_vk_get_init_params,
    xess_vk_execute,
};

#ifdef _WIN64

typedef ULONG PTR32;

struct xess_get_version_params32
{
    PTR32 pVersion;
    xess_result_t result;
};

struct xess_get_intel_xefx_version_params32
{
    PTR32 hContext;
    PTR32 pVersion;
    xess_result_t result;
};

struct xess_get_properties_params32
{
    PTR32 hContext;
    PTR32 pOutputResolution;
    PTR32 pProperties;
    xess_result_t result;
};

struct xess_get_input_resolution_params32
{
    PTR32 hContext;
    PTR32 pOutputResolution;
    xess_quality_settings_t qualitySetting;
    PTR32 pInputResolution;
    xess_result_t result;
};

struct xess_get_optimal_input_resolution_params32
{
    PTR32 hContext;
    PTR32 pOutputResolution;
    xess_quality_settings_t qualitySetting;
    PTR32 pMinResolution;
    PTR32 pMaxResolution;
    PTR32 pOptimalResolution;
    xess_result_t result;
};

struct xess_get_jitter_scale_params32
{
    PTR32 hContext;
    PTR32 pX;
    PTR32 pY;
    xess_result_t result;
};

struct xess_get_velocity_scale_params32
{
    PTR32 hContext;
    PTR32 pX;
    PTR32 pY;
    xess_result_t result;
};

struct xess_get_exposure_multiplier_params32
{
    PTR32 hContext;
    PTR32 pScale;
    xess_result_t result;
};

struct xess_get_max_responsive_mask_value_params32
{
    PTR32 hContext;
    PTR32 pMaxValue;
    xess_result_t result;
};

struct xess_destroy_context_params32
{
    PTR32 hContext;
    xess_result_t result;
};

struct xess_set_velocity_scale_params32
{
    PTR32 hContext;
    float x;
    float y;
    xess_result_t result;
};

struct xess_set_jitter_scale_params32
{
    PTR32 hContext;
    float x;
    float y;
    xess_result_t result;
};

struct xess_set_exposure_multiplier_params32
{
    PTR32 hContext;
    float scale;
    xess_result_t result;
};

struct xess_set_max_responsive_mask_value_params32
{
    PTR32 hContext;
    float maxValue;
    xess_result_t result;
};

struct xess_set_context_parameter_f_params32
{
    PTR32 hContext;
    uint32_t param;
    float value;
    xess_result_t result;
};

struct xess_set_logging_callback_params32
{
    PTR32 hContext;
    xess_logging_level_t loggingLevel;
    PTR32 loggingFunction;
    xess_result_t result;
};

struct xess_is_optimal_driver_params32
{
    PTR32 hContext;
    xess_result_t result;
};

struct xess_force_legacy_scale_factors_params32
{
    PTR32 hContext;
    bool force;
    xess_result_t result;
};

struct xess_get_pipeline_build_status_params32
{
    PTR32 hContext;
    xess_result_t result;
};

struct xess_select_network_model_params32
{
    PTR32 hContext;
    xess_network_model_t network;
    xess_result_t result;
};

struct xess_dump_parameters_t32
{
    PTR32 path;
    uint32_t frame_idx;
    uint32_t frame_count;
    xess_dump_elements_mask_t dump_elements_mask;
};

struct xess_start_dump_params32
{
    PTR32 hContext;
    PTR32 dump_parameters;
    xess_result_t result;
};

struct xess_get_profiling_data_params32
{
    PTR32 hContext;
    PTR32 pProfilingData;
    xess_result_t result;
};

struct xess_vk_get_required_instance_extensions_params32
{
    PTR32 instanceExtensionsCount;
    PTR32 instanceExtensions;
    PTR32 minVkApiVersion;
    xess_result_t result;
};

struct xess_vk_get_required_device_extensions_params32
{
    PTR32 instance;
    PTR32 physicalDevice;
    PTR32 deviceExtensionsCount;
    PTR32 deviceExtensions;
    xess_result_t result;
};

struct xess_vk_get_required_device_features_params32
{
    PTR32 instance;
    PTR32 physicalDevice;
    PTR32 features;
    xess_result_t result;
};

struct xess_vk_create_context_params32
{
    PTR32 instance;
    PTR32 physicalDevice;
    PTR32 device;
    PTR32 phContext;
    xess_result_t result;
};

struct xess_vk_init_params_t32
{
    xess_2d_t outputResolution;
    xess_quality_settings_t qualitySetting;
    uint32_t initFlags;
    uint32_t creationNodeMask;
    uint32_t visibleNodeMask;
    VkDeviceMemory tempBufferHeap;
    uint64_t bufferHeapOffset;
    VkDeviceMemory tempTextureHeap;
    uint64_t textureHeapOffset;
    VkPipelineCache pipelineCache;
};

struct xess_vk_init_params32
{
    PTR32 hContext;
    PTR32 pInitParams;
    xess_result_t result;
};

struct xess_vk_get_init_params_params32
{
    PTR32 hContext;
    PTR32 pInitParams;
    xess_result_t result;
};

struct xess_vk_execute_params32
{
    PTR32 hContext;
    PTR32 pCommandBuffer;
    PTR32 pExecParams;
    xess_result_t result;
};

static NTSTATUS wow64_xess_init( void *args )
{
    return xess_init( args );
}

static NTSTATUS wow64_xess_destroy_context( void *args )
{
    struct xess_destroy_context_params32 *params32 = args;
    struct xess_destroy_context_params params =
    {
        .hContext = ULongToPtr(params32->hContext),
    };
    NTSTATUS ret;

    ret = xess_destroy_context( &params );
    params32->result = params.result;
    return ret;
}

static NTSTATUS wow64_xess_get_version( void *args )
{
    struct xess_get_version_params32 *params32 = args;
    struct xess_get_version_params params =
    {
        .pVersion = ULongToPtr(params32->pVersion),
    };
    NTSTATUS ret;

    ret = xess_get_version( &params );
    params32->result = params.result;
    return ret;
}

static NTSTATUS wow64_xess_get_intel_xefx_version( void *args )
{
    struct xess_get_intel_xefx_version_params32 *params32 = args;
    struct xess_get_intel_xefx_version_params params =
    {
        .hContext = ULongToPtr(params32->hContext),
        .pVersion = ULongToPtr(params32->pVersion),
    };
    NTSTATUS ret;

    ret = xess_get_intel_xefx_version( &params );
    params32->result = params.result;
    return ret;
}

static NTSTATUS wow64_xess_get_properties( void *args )
{
    struct xess_get_properties_params32 *params32 = args;
    struct xess_get_properties_params params =
    {
        .hContext = ULongToPtr(params32->hContext),
        .pOutputResolution = ULongToPtr(params32->pOutputResolution),
        .pProperties = ULongToPtr(params32->pProperties),
    };
    NTSTATUS ret;

    ret = xess_get_properties( &params );
    params32->result = params.result;
    return ret;
}

static NTSTATUS wow64_xess_get_input_resolution( void *args )
{
    struct xess_get_input_resolution_params32 *params32 = args;
    struct xess_get_input_resolution_params params =
    {
        .hContext = ULongToPtr(params32->hContext),
        .pOutputResolution = ULongToPtr(params32->pOutputResolution),
        .qualitySetting = params32->qualitySetting,
        .pInputResolution = ULongToPtr(params32->pInputResolution),
    };
    NTSTATUS ret;

    ret = xess_get_input_resolution( &params );
    params32->result = params.result;
    return ret;
}

static NTSTATUS wow64_xess_get_optimal_input_resolution( void *args )
{
    struct xess_get_optimal_input_resolution_params32 *params32 = args;
    struct xess_get_optimal_input_resolution_params params =
    {
        .hContext = ULongToPtr(params32->hContext),
        .pOutputResolution = ULongToPtr(params32->pOutputResolution),
        .qualitySetting = params32->qualitySetting,
        .pMinResolution = ULongToPtr(params32->pMinResolution),
        .pMaxResolution = ULongToPtr(params32->pMaxResolution),
        .pOptimalResolution = ULongToPtr(params32->pOptimalResolution),
    };
    NTSTATUS ret;

    ret = xess_get_optimal_input_resolution( &params );
    params32->result = params.result;
    return ret;
}

static NTSTATUS wow64_xess_get_jitter_scale( void *args )
{
    struct xess_get_jitter_scale_params32 *params32 = args;
    struct xess_get_jitter_scale_params params =
    {
        .hContext = ULongToPtr(params32->hContext),
        .pX = ULongToPtr(params32->pX),
        .pY = ULongToPtr(params32->pY),
    };
    NTSTATUS ret;

    ret = xess_get_jitter_scale( &params );
    params32->result = params.result;
    return ret;
}

static NTSTATUS wow64_xess_get_velocity_scale( void *args )
{
    struct xess_get_velocity_scale_params32 *params32 = args;
    struct xess_get_velocity_scale_params params =
    {
        .hContext = ULongToPtr(params32->hContext),
        .pX = ULongToPtr(params32->pX),
        .pY = ULongToPtr(params32->pY),
    };
    NTSTATUS ret;

    ret = xess_get_velocity_scale( &params );
    params32->result = params.result;
    return ret;
}

static NTSTATUS wow64_xess_get_exposure_multiplier( void *args )
{
    struct xess_get_exposure_multiplier_params32 *params32 = args;
    struct xess_get_exposure_multiplier_params params =
    {
        .hContext = ULongToPtr(params32->hContext),
        .pScale = ULongToPtr(params32->pScale),
    };
    NTSTATUS ret;

    ret = xess_get_exposure_multiplier( &params );
    params32->result = params.result;
    return ret;
}

static NTSTATUS wow64_xess_get_max_responsive_mask_value( void *args )
{
    struct xess_get_max_responsive_mask_value_params32 *params32 = args;
    struct xess_get_max_responsive_mask_value_params params =
    {
        .hContext = ULongToPtr(params32->hContext),
        .pMaxValue = ULongToPtr(params32->pMaxValue),
    };
    NTSTATUS ret;

    ret = xess_get_max_responsive_mask_value( &params );
    params32->result = params.result;
    return ret;
}

static NTSTATUS wow64_xess_set_velocity_scale( void *args )
{
    struct xess_set_velocity_scale_params32 *params32 = args;
    struct xess_set_velocity_scale_params params =
    {
        .hContext = ULongToPtr(params32->hContext),
        .x = params32->x,
        .y = params32->y,
    };
    NTSTATUS ret;

    ret = xess_set_velocity_scale( &params );
    params32->result = params.result;
    return ret;
}

static NTSTATUS wow64_xess_set_jitter_scale( void *args )
{
    struct xess_set_jitter_scale_params32 *params32 = args;
    struct xess_set_jitter_scale_params params =
    {
        .hContext = ULongToPtr(params32->hContext),
        .x = params32->x,
        .y = params32->y,
    };
    NTSTATUS ret;

    ret = xess_set_jitter_scale( &params );
    params32->result = params.result;
    return ret;
}

static NTSTATUS wow64_xess_set_exposure_multiplier( void *args )
{
    struct xess_set_exposure_multiplier_params32 *params32 = args;
    struct xess_set_exposure_multiplier_params params =
    {
        .hContext = ULongToPtr(params32->hContext),
        .scale = params32->scale,
    };
    NTSTATUS ret;

    ret = xess_set_exposure_multiplier( &params );
    params32->result = params.result;
    return ret;
}

static NTSTATUS wow64_xess_set_max_responsive_mask_value( void *args )
{
    struct xess_set_max_responsive_mask_value_params32 *params32 = args;
    struct xess_set_max_responsive_mask_value_params params =
    {
        .hContext = ULongToPtr(params32->hContext),
        .maxValue = params32->maxValue,
    };
    NTSTATUS ret;

    ret = xess_set_max_responsive_mask_value( &params );
    params32->result = params.result;
    return ret;
}

static NTSTATUS wow64_xess_set_context_parameter_f( void *args )
{
    struct xess_set_context_parameter_f_params32 *params32 = args;
    struct xess_set_context_parameter_f_params params =
    {
        .hContext = ULongToPtr(params32->hContext),
        .param = params32->param,
        .value = params32->value,
    };
    NTSTATUS ret;

    ret = xess_set_context_parameter_f( &params );
    params32->result = params.result;
    return ret;
}

static NTSTATUS wow64_xess_set_logging_callback( void *args )
{
    struct xess_set_logging_callback_params32 *params32 = args;
    struct xess_set_logging_callback_params params =
    {
        .hContext = ULongToPtr(params32->hContext),
        .loggingLevel = params32->loggingLevel,
        .loggingFunction = ULongToPtr(params32->loggingFunction),
    };
    NTSTATUS ret;

    ret = xess_set_logging_callback( &params );
    params32->result = params.result;
    return ret;
}

static NTSTATUS wow64_xess_is_optimal_driver( void *args )
{
    struct xess_is_optimal_driver_params32 *params32 = args;
    struct xess_is_optimal_driver_params params =
    {
        .hContext = ULongToPtr(params32->hContext),
    };
    NTSTATUS ret;

    ret = xess_is_optimal_driver( &params );
    params32->result = params.result;
    return ret;
}

static NTSTATUS wow64_xess_force_legacy_scale_factors( void *args )
{
    struct xess_force_legacy_scale_factors_params32 *params32 = args;
    struct xess_force_legacy_scale_factors_params params =
    {
        .hContext = ULongToPtr(params32->hContext),
        .force = params32->force,
    };
    NTSTATUS ret;

    ret = xess_force_legacy_scale_factors( &params );
    params32->result = params.result;
    return ret;
}

static NTSTATUS wow64_xess_get_pipeline_build_status( void *args )
{
    struct xess_get_pipeline_build_status_params32 *params32 = args;
    struct xess_get_pipeline_build_status_params params =
    {
        .hContext = ULongToPtr(params32->hContext),
    };
    NTSTATUS ret;

    ret = xess_get_pipeline_build_status( &params );
    params32->result = params.result;
    return ret;
}

static NTSTATUS wow64_xess_select_network_model( void *args )
{
    struct xess_select_network_model_params32 *params32 = args;
    struct xess_select_network_model_params params =
    {
        .hContext = ULongToPtr(params32->hContext),
        .network = params32->network,
    };
    NTSTATUS ret;

    ret = xess_select_network_model( &params );
    params32->result = params.result;
    return ret;
}

static NTSTATUS wow64_xess_start_dump( void *args )
{
    struct xess_start_dump_params32 *params32 = args;
    const struct xess_dump_parameters_t32 *dump_params32 = ULongToPtr(params32->dump_parameters);
    xess_dump_parameters_t dump_params;
    struct xess_start_dump_params params =
    {
        .hContext = ULongToPtr(params32->hContext),
        .dump_parameters = NULL,
    };
    NTSTATUS ret;

    if (dump_params32)
    {
        dump_params.path = ULongToPtr(dump_params32->path);
        dump_params.frame_idx = dump_params32->frame_idx;
        dump_params.frame_count = dump_params32->frame_count;
        dump_params.dump_elements_mask = dump_params32->dump_elements_mask;
        params.dump_parameters = &dump_params;
    }

    ret = xess_start_dump( &params );
    params32->result = params.result;
    return ret;
}

static NTSTATUS wow64_xess_get_profiling_data( void *args )
{
    struct xess_get_profiling_data_params32 *params32 = args;
    xess_profiling_data_t *profiling_data = NULL;
    PTR32 *profiling_data_out = ULongToPtr(params32->pProfilingData);
    struct xess_get_profiling_data_params params =
    {
        .hContext = ULongToPtr(params32->hContext),
        .pProfilingData = &profiling_data,
    };
    NTSTATUS ret;

    ret = xess_get_profiling_data( &params );
    if (ret == STATUS_SUCCESS && params.result >= XESS_RESULT_SUCCESS && profiling_data_out)
    {
        if ((ULONG_PTR)profiling_data > MAXDWORD)
        {
            WARN("xessGetProfilingData returned pointer %p above 32-bit range in WoW64.\n", profiling_data);
            params.result = XESS_RESULT_ERROR_UNSUPPORTED;
        }
        else
        {
            *profiling_data_out = (PTR32)(ULONG_PTR)profiling_data;
        }
    }
    params32->result = params.result;
    return ret;
}

static NTSTATUS wow64_xess_vk_get_required_instance_extensions( void *args )
{
    struct xess_vk_get_required_instance_extensions_params32 *params32 = args;
    struct xess_vk_get_required_instance_extensions_params params =
    {
        .instanceExtensionsCount = ULongToPtr(params32->instanceExtensionsCount),
        .instanceExtensions = ULongToPtr(params32->instanceExtensions),
        .minVkApiVersion = ULongToPtr(params32->minVkApiVersion),
    };
    NTSTATUS ret;

    ret = xess_vk_get_required_instance_extensions( &params );
    params32->result = params.result;
    return ret;
}

static NTSTATUS wow64_xess_vk_get_required_device_extensions( void *args )
{
    struct xess_vk_get_required_device_extensions_params32 *params32 = args;
    struct xess_vk_get_required_device_extensions_params params =
    {
        .instance = ULongToPtr(params32->instance),
        .physicalDevice = ULongToPtr(params32->physicalDevice),
        .deviceExtensionsCount = ULongToPtr(params32->deviceExtensionsCount),
        .deviceExtensions = ULongToPtr(params32->deviceExtensions),
    };
    NTSTATUS ret;

    ret = xess_vk_get_required_device_extensions( &params );
    params32->result = params.result;
    return ret;
}

static NTSTATUS wow64_xess_vk_get_required_device_features( void *args )
{
    struct xess_vk_get_required_device_features_params32 *params32 = args;
    struct xess_vk_get_required_device_features_params params =
    {
        .instance = ULongToPtr(params32->instance),
        .physicalDevice = ULongToPtr(params32->physicalDevice),
        .features = ULongToPtr(params32->features),
    };
    NTSTATUS ret;

    ret = xess_vk_get_required_device_features( &params );
    params32->result = params.result;
    return ret;
}

static NTSTATUS wow64_xess_vk_create_context( void *args )
{
    struct xess_vk_create_context_params32 *params32 = args;
    struct xess_vk_create_context_params params =
    {
        .instance = ULongToPtr(params32->instance),
        .physicalDevice = ULongToPtr(params32->physicalDevice),
        .device = ULongToPtr(params32->device),
        .phContext = ULongToPtr(params32->phContext),
    };
    NTSTATUS ret;

    ret = xess_vk_create_context( &params );
    params32->result = params.result;
    return ret;
}

static NTSTATUS wow64_xess_vk_build_pipelines( void *args )
{
    struct
    {
        PTR32 hContext;
        VkPipelineCache pipelineCache;
        bool blocking;
        uint32_t initFlags;
        xess_result_t result;
    } *params32 = args;
    struct xess_vk_build_pipelines_params params =
    {
        .hContext = ULongToPtr(params32->hContext),
        .pipelineCache = params32->pipelineCache,
        .blocking = params32->blocking,
        .initFlags = params32->initFlags,
    };
    NTSTATUS ret;

    ret = xess_vk_build_pipelines( &params );
    params32->result = params.result;
    return ret;
}

static NTSTATUS wow64_xess_vk_init( void *args )
{
    struct xess_vk_init_params32 *params32 = args;
    const struct xess_vk_init_params_t32 *init_params32 = ULongToPtr(params32->pInitParams);
    xess_vk_init_params_t init_params;
    struct xess_vk_init_params params =
    {
        .hContext = ULongToPtr(params32->hContext),
        .pInitParams = NULL,
    };
    NTSTATUS ret;

    if (init_params32)
    {
        init_params.outputResolution = init_params32->outputResolution;
        init_params.qualitySetting = init_params32->qualitySetting;
        init_params.initFlags = init_params32->initFlags;
        init_params.creationNodeMask = init_params32->creationNodeMask;
        init_params.visibleNodeMask = init_params32->visibleNodeMask;
        init_params.tempBufferHeap = init_params32->tempBufferHeap;
        init_params.bufferHeapOffset = init_params32->bufferHeapOffset;
        init_params.tempTextureHeap = init_params32->tempTextureHeap;
        init_params.textureHeapOffset = init_params32->textureHeapOffset;
        init_params.pipelineCache = init_params32->pipelineCache;
        params.pInitParams = &init_params;
    }

    ret = xess_vk_init( &params );
    params32->result = params.result;
    return ret;
}

static NTSTATUS wow64_xess_vk_get_init_params( void *args )
{
    struct xess_vk_get_init_params_params32 *params32 = args;
    struct xess_vk_init_params_t32 *init_params32 = ULongToPtr(params32->pInitParams);
    xess_vk_init_params_t init_params;
    struct xess_vk_get_init_params_params params =
    {
        .hContext = ULongToPtr(params32->hContext),
        .pInitParams = init_params32 ? &init_params : NULL,
    };
    NTSTATUS ret;

    ret = xess_vk_get_init_params( &params );
    if (!ret && init_params32)
    {
        init_params32->outputResolution = init_params.outputResolution;
        init_params32->qualitySetting = init_params.qualitySetting;
        init_params32->initFlags = init_params.initFlags;
        init_params32->creationNodeMask = init_params.creationNodeMask;
        init_params32->visibleNodeMask = init_params.visibleNodeMask;
        init_params32->tempBufferHeap = init_params.tempBufferHeap;
        init_params32->bufferHeapOffset = init_params.bufferHeapOffset;
        init_params32->tempTextureHeap = init_params.tempTextureHeap;
        init_params32->textureHeapOffset = init_params.textureHeapOffset;
        init_params32->pipelineCache = init_params.pipelineCache;
    }
    params32->result = params.result;
    return ret;
}

static NTSTATUS wow64_xess_vk_execute( void *args )
{
    struct xess_vk_execute_params32 *params32 = args;
    struct xess_vk_execute_params params =
    {
        .hContext = ULongToPtr(params32->hContext),
        .pCommandBuffer = ULongToPtr(params32->pCommandBuffer),
        .pExecParams = ULongToPtr(params32->pExecParams),
    };
    NTSTATUS ret;

    ret = xess_vk_execute( &params );
    params32->result = params.result;
    return ret;
}

const unixlib_entry_t __wine_unix_call_wow64_funcs[] =
{
    wow64_xess_init,
    wow64_xess_destroy_context,
    wow64_xess_get_version,
    wow64_xess_get_intel_xefx_version,
    wow64_xess_get_properties,
    wow64_xess_get_input_resolution,
    wow64_xess_get_optimal_input_resolution,
    wow64_xess_get_jitter_scale,
    wow64_xess_get_velocity_scale,
    wow64_xess_get_exposure_multiplier,
    wow64_xess_get_max_responsive_mask_value,
    wow64_xess_set_velocity_scale,
    wow64_xess_set_jitter_scale,
    wow64_xess_set_exposure_multiplier,
    wow64_xess_set_max_responsive_mask_value,
    wow64_xess_set_context_parameter_f,
    wow64_xess_set_logging_callback,
    wow64_xess_is_optimal_driver,
    wow64_xess_force_legacy_scale_factors,
    wow64_xess_get_pipeline_build_status,
    wow64_xess_select_network_model,
    wow64_xess_start_dump,
    wow64_xess_get_profiling_data,
    wow64_xess_vk_get_required_instance_extensions,
    wow64_xess_vk_get_required_device_extensions,
    wow64_xess_vk_get_required_device_features,
    wow64_xess_vk_create_context,
    wow64_xess_vk_build_pipelines,
    wow64_xess_vk_init,
    wow64_xess_vk_get_init_params,
    wow64_xess_vk_execute,
};

#endif  /* _WIN64 */
