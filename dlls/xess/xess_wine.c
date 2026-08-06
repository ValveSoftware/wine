/*
 * XeSS DLL - Main implementation
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

#include <stdarg.h>

#include "windef.h"
#include "winbase.h"
#include "wine/debug.h"
#include "xess_wine.h"
#include "unixlib.h"

WINE_DEFAULT_DEBUG_CHANNEL(xess);

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, void *reserved)
{
    struct xess_init_params params = {0};
    NTSTATUS status;

    TRACE("(%p, %lu, %p)\n", instance, reason, reserved);

    switch (reason)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(instance);
        status = __wine_init_unix_call();
        if (status)
        {
            ERR("Failed to initialize Unix library interface. NTSTATUS %#lx\n", status);
            return FALSE;
        }
        if (WINE_UNIX_CALL(unix_xess_init, &params ))
        {
            ERR("Failed to initialize XeSS Unix library\n");
            return FALSE;
        }
        if (params.result != XESS_RESULT_SUCCESS)
            TRACE("XeSS native library not available\n");
        else
            TRACE("XeSS Unix library initialized successfully\n");
        break;
    case DLL_PROCESS_DETACH:
        if (reserved) break; // process is terminating, no need to clean up
        xess_d3d12_destroy_all_state_trackers();
        break;
    }

    return TRUE;
}

XESS_API xess_result_t xessDestroyContext(xess_context_handle_t hContext)
{
    struct xess_destroy_context_params params = { hContext };
    TRACE("(%p)\n", hContext);
    /* Destroy unix-side context first while d3d12_device ref keeps VkDevice alive. */
    XESS_WINE_UNIX_CALL(unix_xessDestroyContext, &params);
    xess_d3d12_destroy_state_tracker(hContext);
    TRACE("xessDestroyContext result: %s (0x%x)\n", xess_result_to_string(params.result), params.result);
    return params.result;
}

XESS_API xess_result_t xessGetVersion(xess_version_t *pVersion)
{
    struct xess_get_version_params params = { pVersion };
    TRACE("(%p)\n", pVersion);
    XESS_WINE_UNIX_CALL(unix_xessGetVersion, &params);
    return params.result;
}

XESS_API xess_result_t xessGetIntelXeFXVersion(xess_context_handle_t hContext, xess_version_t *pVersion)
{
    struct xess_get_intel_xefx_version_params params = { hContext, pVersion };
    TRACE("(%p, %p)\n", hContext, pVersion);
    XESS_WINE_UNIX_CALL(unix_xessGetIntelXeFXVersion, &params);
    return params.result;
}

XESS_API xess_result_t xessGetProperties(xess_context_handle_t hContext, const xess_2d_t *pOutputResolution, xess_properties_t *pProperties)
{
    struct xess_get_properties_params params = { hContext, pOutputResolution, pProperties };
    TRACE("(%p, %p, %p)\n", hContext, pOutputResolution, pProperties);
    XESS_WINE_UNIX_CALL(unix_xessGetProperties, &params);
    return params.result;
}

XESS_API xess_result_t xessGetInputResolution(xess_context_handle_t hContext, const xess_2d_t *pOutputResolution,
                                                     xess_quality_settings_t qualitySetting, xess_2d_t *pInputResolution)
{
    struct xess_get_input_resolution_params params = { hContext, pOutputResolution, qualitySetting, pInputResolution };
    TRACE("(%p, %p, %u, %p)\n", hContext, pOutputResolution, qualitySetting, pInputResolution);
    XESS_WINE_UNIX_CALL(unix_xessGetInputResolution, &params);
    return params.result;
}

XESS_API xess_result_t xessGetOptimalInputResolution(xess_context_handle_t hContext, const xess_2d_t *pOutputResolution,
                                                            xess_quality_settings_t qualitySetting, xess_2d_t *pMinResolution,
                                                            xess_2d_t *pMaxResolution, xess_2d_t *pOptimalResolution)
{
    struct xess_get_optimal_input_resolution_params params = { hContext, pOutputResolution, qualitySetting, pMinResolution, pMaxResolution, pOptimalResolution };
    TRACE("(%p, %p, %u, %p, %p, %p)\n", hContext, pOutputResolution, qualitySetting, pMinResolution, pMaxResolution, pOptimalResolution);
    XESS_WINE_UNIX_CALL(unix_xessGetOptimalInputResolution, &params);
    return params.result;
}

XESS_API xess_result_t xessGetJitterScale(xess_context_handle_t hContext, float *pX, float *pY)
{
    struct xess_get_jitter_scale_params params = { hContext, pX, pY };
    TRACE("(%p, %p, %p)\n", hContext, pX, pY);
    XESS_WINE_UNIX_CALL(unix_xessGetJitterScale, &params);
    return params.result;
}

XESS_API xess_result_t xessGetVelocityScale(xess_context_handle_t hContext, float *pX, float *pY)
{
    struct xess_get_velocity_scale_params params = { hContext, pX, pY };
    TRACE("(%p, %p, %p)\n", hContext, pX, pY);
    XESS_WINE_UNIX_CALL(unix_xessGetVelocityScale, &params);
    return params.result;
}

XESS_API xess_result_t xessSetJitterScale(xess_context_handle_t hContext, float x, float y)
{
    struct xess_set_jitter_scale_params params = { hContext, x, y };
    TRACE("(%p, %f, %f)\n", hContext, x, y);
    XESS_WINE_UNIX_CALL(unix_xessSetJitterScale, &params);
    return params.result;
}

XESS_API xess_result_t xessSetVelocityScale(xess_context_handle_t hContext, float x, float y)
{
    struct xess_set_velocity_scale_params params = { hContext, x, y };
    TRACE("(%p, %f, %f)\n", hContext, x, y);
    XESS_WINE_UNIX_CALL(unix_xessSetVelocityScale, &params);
    return params.result;
}

XESS_API xess_result_t xessSetExposureMultiplier(xess_context_handle_t hContext, float scale)
{
    struct xess_set_exposure_multiplier_params params = { hContext, scale };
    TRACE("(%p, %f)\n", hContext, scale);
    XESS_WINE_UNIX_CALL(unix_xessSetExposureMultiplier, &params);
    return params.result;
}

XESS_API xess_result_t xessGetExposureMultiplier(xess_context_handle_t hContext, float *pScale)
{
    struct xess_get_exposure_multiplier_params params = { hContext, pScale };
    TRACE("(%p, %p)\n", hContext, pScale);
    XESS_WINE_UNIX_CALL(unix_xessGetExposureMultiplier, &params);
    return params.result;
}

XESS_API xess_result_t xessSetMaxResponsiveMaskValue(xess_context_handle_t hContext, float maxValue)
{
    struct xess_set_max_responsive_mask_value_params params = { hContext, maxValue };
    TRACE("(%p, %f)\n", hContext, maxValue);
    XESS_WINE_UNIX_CALL(unix_xessSetMaxResponsiveMaskValue, &params);
    return params.result;
}

XESS_API xess_result_t xessSetContextParameterF(xess_context_handle_t hContext, uint32_t param, float value)
{
    struct xess_set_context_parameter_f_params params = { hContext, param, value };
    TRACE("(%p, %#x, %f)\n", hContext, param, value);
    XESS_WINE_UNIX_CALL(unix_xessSetContextParameterF, &params);
    return params.result;
}

XESS_API xess_result_t xessGetMaxResponsiveMaskValue(xess_context_handle_t hContext, float *pMaxValue)
{
    struct xess_get_max_responsive_mask_value_params params = { hContext, pMaxValue };
    TRACE("(%p, %p)\n", hContext, pMaxValue);
    XESS_WINE_UNIX_CALL(unix_xessGetMaxResponsiveMaskValue, &params);
    return params.result;
}

XESS_API xess_result_t xessSetLoggingCallback(xess_context_handle_t hContext, xess_logging_level_t loggingLevel, xess_app_log_callback_t loggingFunction)
{
    TRACE("(%p, %d, %p)\n", hContext, loggingLevel, loggingFunction);
    // There is no known Wine mechanism to handle callbacks from the Unix side to the Windows side, so this function is not implemented.
    return XESS_RESULT_ERROR_NOT_IMPLEMENTED;
}

XESS_API xess_result_t xessIsOptimalDriver(xess_context_handle_t hContext)
{
    struct xess_is_optimal_driver_params params = { hContext };
    TRACE("(%p)\n", hContext);
    XESS_WINE_UNIX_CALL(unix_xessIsOptimalDriver, &params);
    return params.result;
}

XESS_API xess_result_t xessForceLegacyScaleFactors(xess_context_handle_t hContext, bool force)
{
    struct xess_force_legacy_scale_factors_params params = { hContext, force };
    TRACE("(%p, %u)\n", hContext, force);
    XESS_WINE_UNIX_CALL(unix_xessForceLegacyScaleFactors, &params);
    return params.result;
}

XESS_API xess_result_t xessGetPipelineBuildStatus(xess_context_handle_t hContext)
{
    struct xess_get_pipeline_build_status_params params = { hContext };
    TRACE("(%p)\n", hContext);
    XESS_WINE_UNIX_CALL(unix_xessGetPipelineBuildStatus, &params);
    return params.result;
}

XESS_API xess_result_t xessSelectNetworkModel(xess_context_handle_t hContext, xess_network_model_t network)
{
    struct xess_select_network_model_params params = { hContext, network };
    TRACE("(%p, %u)\n", hContext, network);
    XESS_WINE_UNIX_CALL(unix_xessSelectNetworkModel, &params);
    return params.result;
}

XESS_API xess_result_t xessStartDump(xess_context_handle_t hContext, const xess_dump_parameters_t *dump_parameters)
{
    struct xess_start_dump_params params = { hContext, dump_parameters };
    TRACE("(%p, %p)\n", hContext, dump_parameters);
    XESS_WINE_UNIX_CALL(unix_xessStartDump, &params);
    return params.result;
}

XESS_API xess_result_t xessGetProfilingData(xess_context_handle_t hContext, xess_profiling_data_t **pProfilingData)
{
    struct xess_get_profiling_data_params params = { hContext, pProfilingData };
    TRACE("(%p, %p)\n", hContext, pProfilingData);
    XESS_WINE_UNIX_CALL(unix_xessGetProfilingData, &params);
    return params.result;
}

const char* xess_result_to_string(xess_result_t result)
{
    switch (result)
    {
        case XESS_RESULT_WARNING_NONEXISTING_FOLDER: return "WARNING_NONEXISTING_FOLDER";
        case XESS_RESULT_WARNING_OLD_DRIVER: return "WARNING_OLD_DRIVER";
        case XESS_RESULT_SUCCESS: return "SUCCESS";
        case XESS_RESULT_ERROR_UNSUPPORTED_DEVICE: return "ERROR_UNSUPPORTED_DEVICE";
        case XESS_RESULT_ERROR_UNSUPPORTED_DRIVER: return "ERROR_UNSUPPORTED_DRIVER";
        case XESS_RESULT_ERROR_UNINITIALIZED: return "ERROR_UNINITIALIZED";
        case XESS_RESULT_ERROR_INVALID_ARGUMENT: return "ERROR_INVALID_ARGUMENT";
        case XESS_RESULT_ERROR_DEVICE_OUT_OF_MEMORY: return "ERROR_DEVICE_OUT_OF_MEMORY";
        case XESS_RESULT_ERROR_DEVICE: return "ERROR_DEVICE";
        case XESS_RESULT_ERROR_NOT_IMPLEMENTED: return "ERROR_NOT_IMPLEMENTED";
        case XESS_RESULT_ERROR_INVALID_CONTEXT: return "ERROR_INVALID_CONTEXT";
        case XESS_RESULT_ERROR_OPERATION_IN_PROGRESS: return "ERROR_OPERATION_IN_PROGRESS";
        case XESS_RESULT_ERROR_UNSUPPORTED: return "ERROR_UNSUPPORTED";
        case XESS_RESULT_ERROR_CANT_LOAD_LIBRARY: return "ERROR_CANT_LOAD_LIBRARY";
        case XESS_RESULT_ERROR_WRONG_CALL_ORDER: return "ERROR_WRONG_CALL_ORDER";
        case XESS_RESULT_ERROR_UNKNOWN: return "ERROR_UNKNOWN";
        default: return "UNKNOWN_RESULT";
    }
}
