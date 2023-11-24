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

#ifndef AMDGPU_VRAM_TYPE_DDR5
#   define AMDGPU_VRAM_TYPE_DDR5  10
#endif
#ifndef AMDGPU_VRAM_TYPE_LPDDR4
#   define AMDGPU_VRAM_TYPE_LPDDR4 11
#endif
#ifndef AMDGPU_VRAM_TYPE_LPDDR5
#   define AMDGPU_VRAM_TYPE_LPDDR5 12
#endif

/* From Mesa source. */
static uint32_t memory_ops_per_clock(uint32_t vram_type)
{
   /* Based on MemoryOpsPerClockTable from PAL. */
   switch (vram_type) {
   case AMDGPU_VRAM_TYPE_GDDR1:
   case AMDGPU_VRAM_TYPE_GDDR3: /* last in low-end Evergreen */
   case AMDGPU_VRAM_TYPE_GDDR4: /* last in R7xx, not used much */
   case AMDGPU_VRAM_TYPE_UNKNOWN:
   default:
      return 0;
   case AMDGPU_VRAM_TYPE_DDR2:
   case AMDGPU_VRAM_TYPE_DDR3:
   case AMDGPU_VRAM_TYPE_DDR4:
   case AMDGPU_VRAM_TYPE_LPDDR4:
   case AMDGPU_VRAM_TYPE_HBM: /* same for HBM2 and HBM3 */
      return 2;
   case AMDGPU_VRAM_TYPE_DDR5:
   case AMDGPU_VRAM_TYPE_LPDDR5:
   case AMDGPU_VRAM_TYPE_GDDR5: /* last in Polaris and low-end Navi14 */
      return 4;
   case AMDGPU_VRAM_TYPE_GDDR6:
      return 16;
   }
}

typedef enum AsicFamily
{
    AsicFamily_Unknown,                                         ///< Unknown architecture, potentially from another IHV. Check \ref AGSDeviceInfo::vendorId
    AsicFamily_PreGCN,                                          ///< Pre GCN architecture.
    AsicFamily_GCN1,                                            ///< AMD GCN 1 architecture: Oland, Cape Verde, Pitcairn & Tahiti.
    AsicFamily_GCN2,                                            ///< AMD GCN 2 architecture: Hawaii & Bonaire.  This also includes APUs Kaveri and Carrizo.
    AsicFamily_GCN3,                                            ///< AMD GCN 3 architecture: Tonga & Fiji.
    AsicFamily_GCN4,                                            ///< AMD GCN 4 architecture: Polaris.
    AsicFamily_Vega,                                            ///< AMD Vega architecture, including Raven Ridge (ie AMD Ryzen CPU + AMD Vega GPU).
    AsicFamily_RDNA,                                            ///< AMD RDNA architecture
    AsicFamily_RDNA2,                                           ///< AMD RDNA2 architecture
    AsicFamily_RDNA3,                                           ///< AMD RDNA3 architecture
} AsicFamily;

/* Constants from Mesa source. */
#define FAMILY_UNKNOWN 0x00
#define FAMILY_TN      0x69 /* # 105 / Trinity APUs */
#define FAMILY_SI      0x6E /* # 110 / Southern Islands: Tahiti, Pitcairn, CapeVerde, Oland, Hainan */
#define FAMILY_CI      0x78 /* # 120 / Sea Islands: Bonaire, Hawaii */
#define FAMILY_KV      0x7D /* # 125 / Kaveri APUs: Spectre, Spooky, Kalindi, Godavari */
#define FAMILY_VI      0x82 /* # 130 / Volcanic Islands: Iceland, Tonga, Fiji */
#define FAMILY_POLARIS 0x82 /* # 130 / Polaris: 10, 11, 12 */
#define FAMILY_CZ      0x87 /* # 135 / Carrizo APUs: Carrizo, Stoney */
#define FAMILY_AI      0x8D /* # 141 / Vega: 10, 20 */
#define FAMILY_RV      0x8E /* # 142 / Raven */
#define FAMILY_NV      0x8F /* # 143 / Navi: 10 */
#define FAMILY_VGH     0x90 /* # 144 / Van Gogh */
#define FAMILY_NV3     0x91 /* # 145 / Navi: 3x */
#define FAMILY_RMB     0x92 /* # 146 / Rembrandt */
#define FAMILY_RPL     0x95 /* # 149 / Raphael */
#define FAMILY_GFX1103 0x94
#define FAMILY_GFX1150 0x96
#define FAMILY_MDN     0x97 /* # 151 / Mendocino */

#define ROUND_DIV(value, div) (((value) + (div) / 2) / (div))

static void fill_device_info(struct drm_amdgpu_info_device *info, struct get_device_info_params *out)
{
    uint32_t erev = info->external_rev;

    out->asic_family = AsicFamily_Unknown;
    switch (info->family)
    {
        case FAMILY_AI:
        case FAMILY_RV:
            out->asic_family = AsicFamily_Vega;
            break;

        /* Treat pre-Polaris cards as Polaris. */
        case FAMILY_CZ:
        case FAMILY_SI:
        case FAMILY_CI:
        case FAMILY_KV:
        case FAMILY_POLARIS:
            out->asic_family = AsicFamily_GCN4;
            break;

        case FAMILY_NV:
            if (erev >= 0x01 && erev < 0x28)
                out->asic_family = AsicFamily_RDNA;
            else if (erev >= 0x28 && erev < 0x50)
                out->asic_family = AsicFamily_RDNA2;
            break;

        case FAMILY_RMB:
        case FAMILY_RPL:
        case FAMILY_MDN:
        case FAMILY_VGH:
            out->asic_family = AsicFamily_RDNA2;
            break;

        case FAMILY_NV3:
        case FAMILY_GFX1103:
        case FAMILY_GFX1150:
            out->asic_family = AsicFamily_RDNA3;
            break;
    }
    TRACE("family %u, erev %#x -> asicFamily %d.\n", info->family, erev, out->asic_family);
    if (out->asic_family == AsicFamily_Unknown && info->family != FAMILY_UNKNOWN)
    {
        if (info->family > FAMILY_GFX1150)
            out->asic_family = AsicFamily_RDNA3;
        else
            out->asic_family = AsicFamily_GCN4;

        FIXME("Unrecognized family %u, erev %#x -> defaulting to %d.\n", info->family, erev,
                out->asic_family);
    }

    out->num_cu = info->cu_active_number;
    out->num_wgp = out->asic_family >= AsicFamily_RDNA ? out->num_cu / 2 : 0;
    out->num_rops = info->num_rb_pipes * 4;
    TRACE("num_cu %d, num_wgp %d, num_rops %d.\n", out->num_cu, out->num_wgp, out->num_rops);
    out->core_clock = ROUND_DIV(info->max_engine_clock, 1000);
    out->memory_clock = ROUND_DIV(info->max_memory_clock, 1000);
    out->memory_bandwidth = ROUND_DIV(info->max_memory_clock * memory_ops_per_clock(info->vram_type)
            * info->vram_bit_width / 8, 1000);
    TRACE("core_clock %uMHz, memory_clock %uMHz, memory_bandwidth %u.\n",
            out->core_clock, out->memory_clock, out->memory_bandwidth);
    out->teraflops = 1e-9f * info->max_engine_clock * info->cu_active_number * 64 * 2;
    TRACE("teraflops %.2f.\n", out->teraflops);
}

static NTSTATUS get_device_info( void *args )
{
    struct get_device_info_params *params = args;
    unsigned int i;

    for (i = 0; i < device_count; ++i)
    {
        if (amd_info[i].device_id != params->device_id)
            continue;
        TRACE("device %04x found.\n", params->device_id);
        fill_device_info(&amd_info[i], params);
        return STATUS_SUCCESS;
    }
    TRACE("Device %04x not found.\n", params->device_id);
    return STATUS_NOT_FOUND;
}

const unixlib_entry_t __wine_unix_call_funcs[] =
{
    init,
    get_device_info,
};
