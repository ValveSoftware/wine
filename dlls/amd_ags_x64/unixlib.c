/*
 * Unix library for amd_ags_x64 functions
 *
 * Copyright 2023 Paul Gofman for CodeWeavers
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
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <xf86drm.h>
#include <amdgpu_drm.h>
#include <amdgpu.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winternl.h"

#include "wine/debug.h"

#include "unixlib.h"

WINE_DEFAULT_DEBUG_CHANNEL(amd_ags);

#define MAX_DEVICE_COUNT 64

static unsigned int device_count;
static struct drm_amdgpu_info_device *amd_info;

static NTSTATUS init( void *args )
{
    drmDevicePtr devices[MAX_DEVICE_COUNT];
    amdgpu_device_handle h;
    uint32_t major, minor;
    int i, count, fd, ret;

    device_count = 0;

    if ((count = drmGetDevices(devices, MAX_DEVICE_COUNT)) <= 0)
    {
        ERR("drmGetDevices failed, err %d.\n", count);
        return STATUS_UNSUCCESSFUL;
    }
    TRACE("Got %d devices.\n", count);
    for (i = 0; i < count; ++i)
    {
        if (!devices[i] || !devices[i]->nodes[DRM_NODE_RENDER])
        {
            TRACE("No render node, skipping.\n");
            continue;
        }
        if ((fd = open(devices[i]->nodes[DRM_NODE_RENDER], O_RDONLY | O_CLOEXEC)) < 0)
        {
            ERR("Failed to open device %s, errno %d.\n", devices[i]->nodes[DRM_NODE_RENDER], errno);
            continue;
        }
        if ((ret = amdgpu_device_initialize(fd, &major, &minor, &h)))
        {
            WARN("Failed to initialize amdgpu device bustype %d, %04x:%04x, err %d.\n", devices[i]->bustype,
                    devices[i]->deviceinfo.pci->vendor_id, devices[i]->deviceinfo.pci->device_id, ret);
            close(fd);
            continue;
        }
        amd_info = realloc(amd_info, (device_count + 1) * sizeof(*amd_info));
        /* amdgpu_query_info() doesn't fail on short buffer (filling in the available buffer size). So older or
         * newer DRM version should be fine but zero init the structure to avoid random values. */
        memset(&amd_info[device_count], 0, sizeof(*amd_info));
        if (!(ret = amdgpu_query_info(h, AMDGPU_INFO_DEV_INFO, sizeof(*amd_info), &amd_info[device_count])))
        {
            TRACE("Got amdgpu info for device id %04x, family %#x, external_rev %#x, chip_rev %#x.\n",
                    amd_info[device_count].device_id, amd_info[device_count].family, amd_info[device_count].external_rev,
                    amd_info[device_count].chip_rev);
            ++device_count;
        }
        else
        {
            ERR("amdgpu_query_info failed, ret %d.\n", ret);
        }
        amdgpu_device_deinitialize(h);
        close(fd);
    }
    drmFreeDevices(devices, count);
    return STATUS_SUCCESS;
}

const unixlib_entry_t __wine_unix_call_funcs[] =
{
    init,
};
