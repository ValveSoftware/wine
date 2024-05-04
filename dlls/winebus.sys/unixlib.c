/*
 * Copyright 2021 Rémi Bernon for CodeWeavers
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

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winbase.h"
#include "winternl.h"
#include "ddk/hidtypes.h"

#include "wine/debug.h"
#include "wine/list.h"
#include "wine/unixlib.h"

#include "unix_private.h"

BOOL is_wine_blacklisted(WORD vid, WORD pid)
{
    if (vid == 0x056a) return TRUE; /* all Wacom devices */
    return FALSE;
}

/* logic from SDL2's SDL_ShouldIgnoreGameController */
BOOL is_sdl_blacklisted(WORD vid, WORD pid)
{
    const char *allow_virtual = getenv("SDL_GAMECONTROLLER_ALLOW_STEAM_VIRTUAL_GAMEPAD");
    const char *whitelist = getenv("SDL_GAMECONTROLLER_IGNORE_DEVICES_EXCEPT");
    const char *blacklist = getenv("SDL_GAMECONTROLLER_IGNORE_DEVICES");
    char needle[16];

    if (vid == 0x28de && pid == 0x11ff && allow_virtual && *allow_virtual &&
        *allow_virtual != '0' && strcasecmp(allow_virtual, "false"))
        return FALSE;

    sprintf(needle, "0x%04x/0x%04x", vid, pid);
    if (whitelist) return strcasestr(whitelist, needle) == NULL;
    if (blacklist) return strcasestr(blacklist, needle) != NULL;
    return FALSE;
}

BOOL is_dualshock4_gamepad(WORD vid, WORD pid)
{
    if (vid != 0x054c) return FALSE;
    if (pid == 0x05c4) return TRUE; /* DualShock 4 [CUH-ZCT1x] */
    if (pid == 0x09cc) return TRUE; /* DualShock 4 [CUH-ZCT2x] */
    if (pid == 0x0ba0) return TRUE; /* Dualshock 4 Wireless Adaptor */
    return FALSE;
}

BOOL is_dualsense_gamepad(WORD vid, WORD pid)
{
    if (vid != 0x054c) return FALSE;
    if (pid == 0x0ce6) return TRUE; /* DualSense */
    if (pid == 0x0df2) return TRUE; /* DualSense Edge */
    return FALSE;
}

BOOL is_logitech_g920(WORD vid, WORD pid)
{
    return vid == 0x046D && pid == 0xC262;
}

static BOOL is_thrustmaster_hotas(WORD vid, WORD pid)
{
    return vid == 0x044F && (pid == 0xB679 || pid == 0xB687 || pid == 0xB10A);
}

static BOOL is_simucube_wheel(WORD vid, WORD pid)
{
    if (vid != 0x16D0) return FALSE;
    if (pid == 0x0D61) return TRUE; /* Simucube 2 Sport */
    if (pid == 0x0D60) return TRUE; /* Simucube 2 Pro */
    if (pid == 0x0D5F) return TRUE; /* Simucube 2 Ultimate */
    if (pid == 0x0D5A) return TRUE; /* Simucube 1 */
    return FALSE;
}

static BOOL is_fanatec_pedals(WORD vid, WORD pid)
{
    if (vid != 0x0EB7) return FALSE;
    if (pid == 0x183B) return TRUE; /* Fanatec ClubSport Pedals v3 */
    if (pid == 0x1839) return TRUE; /* Fanatec ClubSport Pedals v1/v2 */
    return FALSE;
}

static BOOL is_vkb_controller(WORD vid, WORD pid, INT buttons)
{
    if (vid != 0x231D) return FALSE;

    /* comes with 128 buttons in the default configuration */
    if (buttons == 128) return TRUE;

    /* if customized, less than 128 buttons may be shown, decide by PID */
    if (pid == 0x0126) return TRUE; /* VKB-Sim Space Gunfighter */
    if (pid == 0x0127) return TRUE; /* VKB-Sim Space Gunfighter L */
    if (pid == 0x0200) return TRUE; /* VKBsim Gladiator EVO Right Grip */
    if (pid == 0x0201) return TRUE; /* VKBsim Gladiator EVO Left Grip */
    return FALSE;
}

static BOOL is_virpil_controller(WORD vid, WORD pid, INT buttons)
{
    if (vid != 0x3344) return FALSE;

    /* comes with 31 buttons in the default configuration, or 128 max */
    if ((buttons == 31) || (buttons == 128)) return TRUE;

    /* if customized, arbitrary amount of buttons may be shown, decide by PID */
    if (pid == 0x412f) return TRUE; /* Virpil Constellation ALPHA-R */
    return FALSE;
}

BOOL is_hidraw_enabled(WORD vid, WORD pid, INT axes, INT buttons)
{
    const char *enabled = getenv("PROTON_ENABLE_HIDRAW");
    char needle[16];

    if (is_dualshock4_gamepad(vid, pid)) return TRUE;
    if (is_dualsense_gamepad(vid, pid)) return TRUE;
    if (is_thrustmaster_hotas(vid, pid)) return TRUE;
    if (is_simucube_wheel(vid, pid)) return TRUE;
    if (is_fanatec_pedals(vid, pid)) return TRUE;
    if (is_vkb_controller(vid, pid, buttons)) return TRUE;
    if (is_virpil_controller(vid, pid, buttons)) return TRUE;

    sprintf(needle, "0x%04x/0x%04x", vid, pid);
    if (enabled) return strcasestr(enabled, needle) != NULL;
    return FALSE;
}

struct mouse_device
{
    struct unix_device unix_device;
};

static void mouse_destroy(struct unix_device *iface)
{
}

static NTSTATUS mouse_start(struct unix_device *iface)
{
    const USAGE_AND_PAGE device_usage = {.UsagePage = HID_USAGE_PAGE_GENERIC, .Usage = HID_USAGE_GENERIC_MOUSE};
    if (!hid_device_begin_report_descriptor(iface, &device_usage))
        return STATUS_NO_MEMORY;
    if (!hid_device_add_buttons(iface, HID_USAGE_PAGE_BUTTON, 1, 3))
        return STATUS_NO_MEMORY;
    if (!hid_device_end_report_descriptor(iface))
        return STATUS_NO_MEMORY;

    return STATUS_SUCCESS;
}

static void mouse_stop(struct unix_device *iface)
{
}

static NTSTATUS mouse_haptics_start(struct unix_device *iface, UINT duration,
                                    USHORT rumble_intensity, USHORT buzz_intensity,
                                    USHORT left_intensity, USHORT right_intensity)
{
    return STATUS_NOT_SUPPORTED;
}

static NTSTATUS mouse_haptics_stop(struct unix_device *iface)
{
    return STATUS_NOT_SUPPORTED;
}

static NTSTATUS mouse_physical_device_control(struct unix_device *iface, USAGE control)
{
    return STATUS_NOT_SUPPORTED;
}

static NTSTATUS mouse_physical_device_set_gain(struct unix_device *iface, BYTE percent)
{
    return STATUS_NOT_SUPPORTED;
}

static NTSTATUS mouse_physical_effect_control(struct unix_device *iface, BYTE index,
                                              USAGE control, BYTE iterations)
{
    return STATUS_NOT_SUPPORTED;
}

static NTSTATUS mouse_physical_effect_update(struct unix_device *iface, BYTE index,
                                             struct effect_params *params)
{
    return STATUS_NOT_SUPPORTED;
}

static const struct hid_device_vtbl mouse_vtbl =
{
    mouse_destroy,
    mouse_start,
    mouse_stop,
    mouse_haptics_start,
    mouse_haptics_stop,
    mouse_physical_device_control,
    mouse_physical_device_set_gain,
    mouse_physical_effect_control,
    mouse_physical_effect_update,
};

static const struct device_desc mouse_device_desc =
{
    .vid = 0x845e,
    .pid = 0x0001,
    .input = -1,
    .manufacturer = {'T','h','e',' ','W','i','n','e',' ','P','r','o','j','e','c','t',0},
    .product = {'W','i','n','e',' ','H','I','D',' ','m','o','u','s','e',0},
    .serialnumber = {'0','0','0','0',0},
};

static NTSTATUS mouse_device_create(void *args)
{
    struct device_create_params *params = args;
    params->desc = mouse_device_desc;
    params->device = (UINT_PTR)hid_device_create(&mouse_vtbl, sizeof(struct mouse_device));
    return STATUS_SUCCESS;
}

struct keyboard_device
{
    struct unix_device unix_device;
};

static void keyboard_destroy(struct unix_device *iface)
{
}

static NTSTATUS keyboard_start(struct unix_device *iface)
{
    const USAGE_AND_PAGE device_usage = {.UsagePage = HID_USAGE_PAGE_GENERIC, .Usage = HID_USAGE_GENERIC_KEYBOARD};
    if (!hid_device_begin_report_descriptor(iface, &device_usage))
        return STATUS_NO_MEMORY;
    if (!hid_device_add_buttons(iface, HID_USAGE_PAGE_KEYBOARD, 0, 101))
        return STATUS_NO_MEMORY;
    if (!hid_device_end_report_descriptor(iface))
        return STATUS_NO_MEMORY;

    return STATUS_SUCCESS;
}

static void keyboard_stop(struct unix_device *iface)
{
}

static NTSTATUS keyboard_haptics_start(struct unix_device *iface, UINT duration,
                                       USHORT rumble_intensity, USHORT buzz_intensity,
                                       USHORT left_intensity, USHORT right_intensity)
{
    return STATUS_NOT_SUPPORTED;
}

static NTSTATUS keyboard_haptics_stop(struct unix_device *iface)
{
    return STATUS_NOT_SUPPORTED;
}

static NTSTATUS keyboard_physical_device_control(struct unix_device *iface, USAGE control)
{
    return STATUS_NOT_SUPPORTED;
}

static NTSTATUS keyboard_physical_device_set_gain(struct unix_device *iface, BYTE percent)
{
    return STATUS_NOT_SUPPORTED;
}

static NTSTATUS keyboard_physical_effect_control(struct unix_device *iface, BYTE index,
                                                 USAGE control, BYTE iterations)
{
    return STATUS_NOT_SUPPORTED;
}

static NTSTATUS keyboard_physical_effect_update(struct unix_device *iface, BYTE index,
                                                struct effect_params *params)
{
    return STATUS_NOT_SUPPORTED;
}

static const struct hid_device_vtbl keyboard_vtbl =
{
    keyboard_destroy,
    keyboard_start,
    keyboard_stop,
    keyboard_haptics_start,
    keyboard_haptics_stop,
    keyboard_physical_device_control,
    keyboard_physical_device_set_gain,
    keyboard_physical_effect_control,
    keyboard_physical_effect_update,
};

static const struct device_desc keyboard_device_desc =
{
    .vid = 0x845e,
    .pid = 0x0002,
    .input = -1,
    .manufacturer = {'T','h','e',' ','W','i','n','e',' ','P','r','o','j','e','c','t',0},
    .product = {'W','i','n','e',' ','H','I','D',' ','k','e','y','b','o','a','r','d',0},
    .serialnumber = {'0','0','0','0',0},
};

static NTSTATUS keyboard_device_create(void *args)
{
    struct device_create_params *params = args;
    params->desc = keyboard_device_desc;
    params->device = (UINT_PTR)hid_device_create(&keyboard_vtbl, sizeof(struct keyboard_device));
    return STATUS_SUCCESS;
}

void *raw_device_create(const struct raw_device_vtbl *vtbl, SIZE_T size)
{
    struct unix_device *iface;

    if (!(iface = calloc(1, size))) return NULL;
    iface->vtbl = vtbl;
    iface->ref = 1;

    return iface;
}

static void unix_device_decref(struct unix_device *iface)
{
    if (!InterlockedDecrement(&iface->ref))
    {
        iface->vtbl->destroy(iface);
        free(iface);
    }
}

static ULONG unix_device_incref(struct unix_device *iface)
{
    return InterlockedIncrement(&iface->ref);
}

static NTSTATUS unix_device_remove(void *args)
{
    struct device_remove_params *params = args;
    struct unix_device *iface = (struct unix_device *)(UINT_PTR)params->device;
    iface->vtbl->stop(iface);
    unix_device_decref(iface);
    return STATUS_SUCCESS;
}

static NTSTATUS unix_device_start(void *args)
{
    struct device_start_params *params = args;
    struct unix_device *iface = (struct unix_device *)(UINT_PTR)params->device;
    return iface->vtbl->start(iface);
}

static NTSTATUS unix_device_get_report_descriptor(void *args)
{
    struct device_descriptor_params *params = args;
    struct unix_device *iface = (struct unix_device *)(UINT_PTR)params->device;
    return iface->vtbl->get_report_descriptor(iface, params->buffer, params->length, params->out_length);
}

static NTSTATUS unix_device_set_output_report(void *args)
{
    struct device_report_params *params = args;
    struct unix_device *iface = (struct unix_device *)(UINT_PTR)params->device;
    iface->vtbl->set_output_report(iface, params->packet, params->io);
    return STATUS_SUCCESS;
}

static NTSTATUS unix_device_get_feature_report(void *args)
{
    struct device_report_params *params = args;
    struct unix_device *iface = (struct unix_device *)(UINT_PTR)params->device;
    iface->vtbl->get_feature_report(iface, params->packet, params->io);
    return STATUS_SUCCESS;
}

static NTSTATUS unix_device_set_feature_report(void *args)
{
    struct device_report_params *params = args;
    struct unix_device *iface = (struct unix_device *)(UINT_PTR)params->device;
    iface->vtbl->set_feature_report(iface, params->packet, params->io);
    return STATUS_SUCCESS;
}

const unixlib_entry_t __wine_unix_call_funcs[] =
{
    sdl_bus_init,
    sdl_bus_wait,
    sdl_bus_stop,
    udev_bus_init,
    udev_bus_wait,
    udev_bus_stop,
    iohid_bus_init,
    iohid_bus_wait,
    iohid_bus_stop,
    mouse_device_create,
    keyboard_device_create,
    unix_device_remove,
    unix_device_start,
    unix_device_get_report_descriptor,
    unix_device_set_output_report,
    unix_device_get_feature_report,
    unix_device_set_feature_report,
};

C_ASSERT(ARRAYSIZE(__wine_unix_call_funcs) == unix_funcs_count);

void bus_event_cleanup(struct bus_event *event)
{
    struct unix_device *iface = (struct unix_device *)(UINT_PTR)event->device;
    if (event->type == BUS_EVENT_TYPE_NONE) return;
    unix_device_decref(iface);
}

struct bus_event_entry
{
    struct list entry;
    struct bus_event event;
};

void bus_event_queue_destroy(struct list *queue)
{
    struct bus_event_entry *entry, *next;

    LIST_FOR_EACH_ENTRY_SAFE(entry, next, queue, struct bus_event_entry, entry)
    {
        bus_event_cleanup(&entry->event);
        list_remove(&entry->entry);
        free(entry);
    }
}

BOOL bus_event_queue_device_removed(struct list *queue, struct unix_device *device)
{
    ULONG size = sizeof(struct bus_event_entry);
    struct bus_event_entry *entry = malloc(size);
    if (!entry) return FALSE;

    if (unix_device_incref(device) == 1) /* being destroyed */
    {
        free(entry);
        return FALSE;
    }

    entry->event.type = BUS_EVENT_TYPE_DEVICE_REMOVED;
    entry->event.device = (UINT_PTR)device;
    list_add_tail(queue, &entry->entry);

    return TRUE;
}

BOOL bus_event_queue_device_created(struct list *queue, struct unix_device *device, struct device_desc *desc)
{
    ULONG size = sizeof(struct bus_event_entry);
    struct bus_event_entry *entry = malloc(size);
    if (!entry) return FALSE;

    if (unix_device_incref(device) == 1) /* being destroyed */
    {
        free(entry);
        return FALSE;
    }

    entry->event.type = BUS_EVENT_TYPE_DEVICE_CREATED;
    entry->event.device = (UINT_PTR)device;
    entry->event.device_created.desc = *desc;
    list_add_tail(queue, &entry->entry);

    return TRUE;
}

BOOL bus_event_queue_input_report(struct list *queue, struct unix_device *device, BYTE *report, USHORT length)
{
    ULONG size = offsetof(struct bus_event_entry, event.input_report.buffer[length]);
    struct bus_event_entry *entry = malloc(size);
    if (!entry) return FALSE;

    if (unix_device_incref(device) == 1) /* being destroyed */
    {
        free(entry);
        return FALSE;
    }

    entry->event.type = BUS_EVENT_TYPE_INPUT_REPORT;
    entry->event.device = (UINT_PTR)device;
    entry->event.input_report.length = length;
    memcpy(entry->event.input_report.buffer, report, length);
    list_add_tail(queue, &entry->entry);

    return TRUE;
}

BOOL bus_event_queue_pop(struct list *queue, struct bus_event *event)
{
    struct list *head = list_head(queue);
    struct bus_event_entry *entry;
    ULONG size;

    if (!head) return FALSE;

    entry = LIST_ENTRY(head, struct bus_event_entry, entry);
    list_remove(&entry->entry);

    if (entry->event.type != BUS_EVENT_TYPE_INPUT_REPORT) size = sizeof(entry->event);
    else size = offsetof(struct bus_event, input_report.buffer[entry->event.input_report.length]);

    memcpy(event, &entry->event, size);
    free(entry);

    return TRUE;
}
