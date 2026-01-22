/*
 * XeSS Unix library interface
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

#ifndef __XESS_UNIXLIB_H
#define __XESS_UNIXLIB_H

#include "wine/unixlib.h"
#include "xess_wine.h"

#define XESS_UNIX_DISPATCH_ERROR XESS_RESULT_ERROR_CANT_LOAD_LIBRARY

#define XESS_WINE_UNIX_CALL(func, params) \
    do \
    { \
        NTSTATUS status__; \
        (params)->result = XESS_UNIX_DISPATCH_ERROR; \
        status__ = WINE_UNIX_CALL(func, params); \
        if (status__) \
        { \
            ERR("Unix call %s failed, status %#lx\n", #func, status__); \
            return XESS_UNIX_DISPATCH_ERROR; \
        } \
    } while (0)

enum xess_funcs
{
    unix_xess_init,
    unix_xessDestroyContext,
    unix_xessGetVersion,
    unix_xessGetIntelXeFXVersion,
    unix_xessGetProperties,
    unix_xessGetInputResolution,
    unix_xessGetOptimalInputResolution,
    unix_xessGetJitterScale,
    unix_xessGetVelocityScale,
    unix_xessGetExposureMultiplier,
    unix_xessGetMaxResponsiveMaskValue,
    unix_xessSetVelocityScale,
    unix_xessSetJitterScale,
    unix_xessSetExposureMultiplier,
    unix_xessSetMaxResponsiveMaskValue,
    unix_xessSetContextParameterF,
    unix_xessSetLoggingCallback,
    unix_xessIsOptimalDriver,
    unix_xessForceLegacyScaleFactors,
    unix_xessGetPipelineBuildStatus,
    unix_xessSelectNetworkModel,
    unix_xessStartDump,
    unix_xessGetProfilingData,
    unix_xessVKGetRequiredInstanceExtensions,
    unix_xessVKGetRequiredDeviceExtensions,
    unix_xessVKGetRequiredDeviceFeatures,
    unix_xessVKCreateContext,
    unix_xessVKBuildPipelines,
    unix_xessVKInit,
    unix_xessVKGetInitParams,
    unix_xessVKExecute,
};

struct xess_init_params
{
    xess_result_t result;
};

struct xess_destroy_context_params
{
    xess_context_handle_t hContext;
    xess_result_t result;
};

struct xess_get_version_params
{
    xess_version_t *pVersion;
    xess_result_t result;
};

struct xess_get_intel_xefx_version_params
{
    xess_context_handle_t hContext;
    xess_version_t *pVersion;
    xess_result_t result;
};

struct xess_get_properties_params
{
    xess_context_handle_t hContext;
    const xess_2d_t *pOutputResolution;
    xess_properties_t *pProperties;
    xess_result_t result;
};

struct xess_get_input_resolution_params
{
    xess_context_handle_t hContext;
    const xess_2d_t *pOutputResolution;
    xess_quality_settings_t qualitySetting;
    xess_2d_t *pInputResolution;
    xess_result_t result;
};

struct xess_get_optimal_input_resolution_params
{
    xess_context_handle_t hContext;
    const xess_2d_t *pOutputResolution;
    xess_quality_settings_t qualitySetting;
    xess_2d_t *pMinResolution;
    xess_2d_t *pMaxResolution;
    xess_2d_t *pOptimalResolution;
    xess_result_t result;
};

struct xess_get_jitter_scale_params
{
    xess_context_handle_t hContext;
    float *pX;
    float *pY;
    xess_result_t result;
};

struct xess_get_velocity_scale_params
{
    xess_context_handle_t hContext;
    float *pX;
    float *pY;
    xess_result_t result;
};

struct xess_get_exposure_multiplier_params
{
    xess_context_handle_t hContext;
    float *pScale;
    xess_result_t result;
};

struct xess_get_max_responsive_mask_value_params
{
    xess_context_handle_t hContext;
    float *pMaxValue;
    xess_result_t result;
};

struct xess_set_velocity_scale_params
{
    xess_context_handle_t hContext;
    float x;
    float y;
    xess_result_t result;
};

struct xess_set_jitter_scale_params
{
    xess_context_handle_t hContext;
    float x;
    float y;
    xess_result_t result;
};

struct xess_set_exposure_multiplier_params
{
    xess_context_handle_t hContext;
    float scale;
    xess_result_t result;
};

struct xess_set_max_responsive_mask_value_params
{
    xess_context_handle_t hContext;
    float maxValue;
    xess_result_t result;
};

struct xess_set_context_parameter_f_params
{
    xess_context_handle_t hContext;
    uint32_t param;
    float value;
    xess_result_t result;
};

struct xess_set_logging_callback_params
{
    xess_context_handle_t hContext;
    xess_logging_level_t loggingLevel;
    xess_app_log_callback_t loggingFunction;
    xess_result_t result;
};

struct xess_is_optimal_driver_params
{
    xess_context_handle_t hContext;
    xess_result_t result;
};

struct xess_force_legacy_scale_factors_params
{
    xess_context_handle_t hContext;
    bool force;
    xess_result_t result;
};

struct xess_get_pipeline_build_status_params
{
    xess_context_handle_t hContext;
    xess_result_t result;
};

struct xess_select_network_model_params
{
    xess_context_handle_t hContext;
    xess_network_model_t network;
    xess_result_t result;
};

struct xess_start_dump_params
{
    xess_context_handle_t hContext;
    const xess_dump_parameters_t *dump_parameters;
    xess_result_t result;
};

struct xess_get_profiling_data_params
{
    xess_context_handle_t hContext;
    xess_profiling_data_t **pProfilingData;
    xess_result_t result;
};

struct xess_vk_get_required_instance_extensions_params
{
    uint32_t* instanceExtensionsCount;
    const char* const** instanceExtensions;
    uint32_t* minVkApiVersion;
    xess_result_t result;
};

struct xess_vk_get_required_device_extensions_params
{
   VkInstance instance;
   VkPhysicalDevice physicalDevice;
   uint32_t* deviceExtensionsCount;
   const char* const** deviceExtensions;
   xess_result_t result;
};

struct xess_vk_get_required_device_features_params
{
   VkInstance instance;
   VkPhysicalDevice physicalDevice;
   void** features;
   xess_result_t result;
};

struct xess_vk_create_context_params
{
    VkInstance instance;
    VkPhysicalDevice physicalDevice;
    VkDevice device;
    xess_context_handle_t *phContext;
    xess_result_t result;
};

struct xess_vk_build_pipelines_params
{
    xess_context_handle_t hContext;
    VkPipelineCache pipelineCache;
    bool blocking;
    uint32_t initFlags;
    xess_result_t result;
};

struct xess_vk_init_params
{
    xess_context_handle_t hContext;
    const xess_vk_init_params_t *pInitParams;
    xess_result_t result;
};

struct xess_vk_get_init_params_params
{
    xess_context_handle_t hContext;
    xess_vk_init_params_t *pInitParams;
    xess_result_t result;
};

struct xess_vk_execute_params
{
    xess_context_handle_t hContext;
    void *pCommandBuffer;
    const xess_vk_execute_params_t *pExecParams;
    xess_result_t result;
};

#endif /* __XESS_UNIXLIB_H */
