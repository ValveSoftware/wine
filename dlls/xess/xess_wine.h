/*
 * XeSS DLL - Internal definitions
 * Copyright 2025 the Wine project
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#ifndef __WINE_XESS_INTERNAL_H
#define __WINE_XESS_INTERNAL_H

#include "wine/vulkan.h"
#include "wine/vulkan_driver.h"

/* stdbool must be included _before_ the XeSS headers,
 or the Wine build will complain about undefined types */
#include <stdbool.h>
#include "extern/xess.h"
#include "extern/xess_debug.h"

// HACK: disable vulkan include in xess_vk.h
// without modifying the header itself
// so the header can be copied directly from upstream
#define VULKAN_CORE_H_ 1
#include "extern/xess_vk.h"
#undef VULKAN_CORE_H_

const char* xess_result_to_string(xess_result_t result);

XESS_API xess_result_t xessSetContextParameterF(xess_context_handle_t hContext, uint32_t param, float value);

void xess_d3d12_destroy_state_tracker(xess_context_handle_t hContext);
void xess_d3d12_destroy_all_state_trackers(void);

#endif /* __WINE_XESS_INTERNAL_H */
