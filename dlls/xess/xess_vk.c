/*
 * XeSS DLL - Vulkan API implementation
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

XESS_API xess_result_t xessVKGetRequiredInstanceExtensions(uint32_t* instanceExtensionsCount, const char* const** instanceExtensions, uint32_t* minVkApiVersion)
{
    struct xess_vk_get_required_instance_extensions_params params = { instanceExtensionsCount, instanceExtensions, minVkApiVersion };
    TRACE("(%p, %p, %p)\n", instanceExtensionsCount, instanceExtensions, minVkApiVersion);
    XESS_WINE_UNIX_CALL(unix_xessVKGetRequiredInstanceExtensions, &params);
    return params.result;
}

XESS_API xess_result_t xessVKGetRequiredDeviceExtensions(VkInstance instance, VkPhysicalDevice physicalDevice,
   uint32_t* deviceExtensionsCount, const char* const** deviceExtensions)
{
    struct xess_vk_get_required_device_extensions_params params = { instance, physicalDevice, deviceExtensionsCount, deviceExtensions };
    TRACE("(%p, %p, %p, %p)\n", instance, physicalDevice, deviceExtensionsCount, deviceExtensions);
    XESS_WINE_UNIX_CALL(unix_xessVKGetRequiredDeviceExtensions, &params);
    return params.result;
}

XESS_API xess_result_t xessVKGetRequiredDeviceFeatures(VkInstance instance, VkPhysicalDevice physicalDevice, void** features)
{
    struct xess_vk_get_required_device_features_params params = { instance, physicalDevice, features };
    TRACE("(%p, %p, %p)\n", instance, physicalDevice, features);
    XESS_WINE_UNIX_CALL(unix_xessVKGetRequiredDeviceFeatures, &params);
    return params.result;
}

XESS_API xess_result_t xessVKCreateContext(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device, xess_context_handle_t* phContext)
{
    struct xess_vk_create_context_params params = { instance, physicalDevice, device, phContext };
    TRACE("(%p, %p, %p, %p)\n", instance, physicalDevice, device, phContext);
    XESS_WINE_UNIX_CALL(unix_xessVKCreateContext, &params);
    return params.result;
}

XESS_API xess_result_t xessVKBuildPipelines(xess_context_handle_t hContext, VkPipelineCache pipelineCache, bool blocking, uint32_t initFlags)
{
    struct xess_vk_build_pipelines_params params = { hContext, pipelineCache, blocking, initFlags };
    // TRACE("(%p, %p, %u, %u)\n", hContext, pipelineCache, blocking, initFlags);
    XESS_WINE_UNIX_CALL(unix_xessVKBuildPipelines, &params);
    return params.result;
}

XESS_API xess_result_t xessVKInit(xess_context_handle_t hContext, const xess_vk_init_params_t* pInitParams)
{
    struct xess_vk_init_params params = { hContext, pInitParams };
    TRACE("(%p, %p)\n", hContext, pInitParams);
    XESS_WINE_UNIX_CALL(unix_xessVKInit, &params);
    return params.result;
}

XESS_API xess_result_t xessVKGetInitParams(xess_context_handle_t hContext, xess_vk_init_params_t *pInitParams)
{
    struct xess_vk_get_init_params_params params = { hContext, pInitParams };
    TRACE("(%p, %p)\n", hContext, pInitParams);
    XESS_WINE_UNIX_CALL(unix_xessVKGetInitParams, &params);
    return params.result;
}

XESS_API xess_result_t xessVKExecute(xess_context_handle_t hContext, VkCommandBuffer commandBuffer, const xess_vk_execute_params_t* pExecParams)
{
    struct xess_vk_execute_params params = { hContext, commandBuffer, pExecParams };
    TRACE("(%p, %p, %p)\n", hContext, commandBuffer, pExecParams);
    XESS_WINE_UNIX_CALL(unix_xessVKExecute, &params);
    return params.result;
}
