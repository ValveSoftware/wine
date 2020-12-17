/*
 * Plug and Play support for hid devices found through udev
 *
 * Copyright 2016 CodeWeavers, Aric Stewart
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

#define _GNU_SOURCE
#include "config.h"
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef HAVE_POLL_H
# include <poll.h>
#endif
#ifdef HAVE_SYS_POLL_H
# include <sys/poll.h>
#endif
#ifdef HAVE_LIBUDEV_H
# include <libudev.h>
#endif
#ifdef HAVE_LINUX_HIDRAW_H
# include <linux/hidraw.h>
#endif
#ifdef HAVE_SYS_IOCTL_H
# include <sys/ioctl.h>
#endif
#ifdef HAVE_SYS_INOTIFY_H
# include <sys/inotify.h>
#endif

#ifdef HAVE_LINUX_INPUT_H
# include <linux/input.h>
# undef SW_MAX
# if defined(EVIOCGBIT) && defined(EV_ABS) && defined(BTN_PINKIE)
#  define HAS_PROPER_INPUT_HEADER
# endif
# ifndef SYN_DROPPED
#  define SYN_DROPPED 3
# endif
#endif

#define NONAMELESSUNION

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winbase.h"
#include "winnls.h"
#include "winternl.h"
#include "ddk/wdm.h"
#include "ddk/hidtypes.h"
#include "wine/debug.h"
#include "wine/heap.h"
#include "wine/unicode.h"

#ifdef HAS_PROPER_INPUT_HEADER
# include "hidusage.h"
#endif

#ifdef WORDS_BIGENDIAN
#define LE_WORD(x) RtlUshortByteSwap(x)
#define LE_DWORD(x) RtlUlongByteSwap(x)
#else
#define LE_WORD(x) (x)
#define LE_DWORD(x) (x)
#endif

#include "controller.h"
#include "bus.h"

WINE_DEFAULT_DEBUG_CHANNEL(plugplay);

#ifdef HAVE_UDEV

WINE_DECLARE_DEBUG_CHANNEL(hid_report);

static struct udev *udev_context = NULL;
static DWORD bypass_udevd = 0;
static DWORD disable_hidraw = 0;
static DWORD disable_input = 0;
static HANDLE deviceloop_handle;
static int deviceloop_control[2];
static HANDLE steam_overlay_event;

static const WCHAR hidraw_busidW[] = {'H','I','D','R','A','W',0};
static const WCHAR lnxev_busidW[] = {'L','N','X','E','V',0};

struct vidpid {
    WORD vid, pid;
};

/* the kernel is a great place to learn about these DS4 quirks */
#define QUIRK_DS4_BT 0x1

struct platform_private
{
    const platform_vtbl *vtbl;
    struct udev_device *udev_device;
    int device_fd;

    HANDLE report_thread;
    int control_pipe[2];

    struct vidpid vidpid;

    DWORD quirks;

    DWORD bus_type;
};

static inline struct platform_private *impl_from_DEVICE_OBJECT(DEVICE_OBJECT *device)
{
    return (struct platform_private *)get_platform_private(device);
}

#ifdef HAS_PROPER_INPUT_HEADER

static const BYTE REPORT_ABS_AXIS_TAIL[] = {
    0x17, 0x00, 0x00, 0x00, 0x00,  /* LOGICAL_MINIMUM (0) */
    0x27, 0xff, 0x00, 0x00, 0x00,  /* LOGICAL_MAXIMUM (0xff) */
    0x37, 0x00, 0x00, 0x00, 0x00,  /* PHYSICAL_MINIMUM (0) */
    0x47, 0xff, 0x00, 0x00, 0x00,  /* PHYSICAL_MAXIMUM (256) */
    0x75, 0x20,                    /* REPORT_SIZE (32) */
    0x95, 0x00,                    /* REPORT_COUNT (2) */
    0x81, 0x02,                    /* INPUT (Data,Var,Abs) */
};
#define IDX_ABS_LOG_MINIMUM 1
#define IDX_ABS_LOG_MAXIMUM 6
#define IDX_ABS_PHY_MINIMUM 11
#define IDX_ABS_PHY_MAXIMUM 16
#define IDX_ABS_AXIS_COUNT 23

static const BYTE ABS_TO_HID_MAP[][2] = {
    {HID_USAGE_PAGE_GENERIC, HID_USAGE_GENERIC_X},              /*ABS_X*/
    {HID_USAGE_PAGE_GENERIC, HID_USAGE_GENERIC_Y},              /*ABS_Y*/
    {HID_USAGE_PAGE_GENERIC, HID_USAGE_GENERIC_Z},              /*ABS_Z*/
    {HID_USAGE_PAGE_GENERIC, HID_USAGE_GENERIC_RX},             /*ABS_RX*/
    {HID_USAGE_PAGE_GENERIC, HID_USAGE_GENERIC_RY},             /*ABS_RY*/
    {HID_USAGE_PAGE_GENERIC, HID_USAGE_GENERIC_RZ},             /*ABS_RZ*/
    {HID_USAGE_PAGE_SIMULATION, HID_USAGE_SIMULATION_THROTTLE}, /*ABS_THROTTLE*/
    {HID_USAGE_PAGE_SIMULATION, HID_USAGE_SIMULATION_RUDDER},   /*ABS_RUDDER*/
    {HID_USAGE_PAGE_GENERIC, HID_USAGE_GENERIC_WHEEL},          /*ABS_WHEEL*/
    {HID_USAGE_PAGE_SIMULATION, HID_USAGE_SIMULATION_ACCELERATOR}, /*ABS_GAS*/
    {HID_USAGE_PAGE_SIMULATION, HID_USAGE_SIMULATION_BRAKE},    /*ABS_BRAKE*/
    {0,0},                                                      /*ABS_HAT0X*/
    {0,0},                                                      /*ABS_HAT0Y*/
    {0,0},                                                      /*ABS_HAT1X*/
    {0,0},                                                      /*ABS_HAT1Y*/
    {0,0},                                                      /*ABS_HAT2X*/
    {0,0},                                                      /*ABS_HAT2Y*/
    {0,0},                                                      /*ABS_HAT3X*/
    {0,0},                                                      /*ABS_HAT3Y*/
    {HID_USAGE_PAGE_DIGITIZER, HID_USAGE_DIGITIZER_TIP_PRESSURE}, /*ABS_PRESSURE*/
    {0, 0},                                                     /*ABS_DISTANCE*/
    {HID_USAGE_PAGE_DIGITIZER, HID_USAGE_DIGITIZER_X_TILT},     /*ABS_TILT_X*/
    {HID_USAGE_PAGE_DIGITIZER, HID_USAGE_DIGITIZER_Y_TILT},     /*ABS_TILT_Y*/
    {0, 0},                                                     /*ABS_TOOL_WIDTH*/
    {0, 0},
    {0, 0},
    {0, 0},
    {HID_USAGE_PAGE_CONSUMER, HID_USAGE_CONSUMER_VOLUME}        /*ABS_VOLUME*/
};
#define HID_ABS_MAX (ABS_VOLUME+1)
#define TOP_ABS_PAGE (HID_USAGE_PAGE_DIGITIZER+1)

static const BYTE REL_TO_HID_MAP[][2] = {
    {HID_USAGE_PAGE_GENERIC, HID_USAGE_GENERIC_X},     /* REL_X */
    {HID_USAGE_PAGE_GENERIC, HID_USAGE_GENERIC_Y},     /* REL_Y */
    {HID_USAGE_PAGE_GENERIC, HID_USAGE_GENERIC_Z},     /* REL_Z */
    {HID_USAGE_PAGE_GENERIC, HID_USAGE_GENERIC_RX},    /* REL_RX */
    {HID_USAGE_PAGE_GENERIC, HID_USAGE_GENERIC_RY},    /* REL_RY */
    {HID_USAGE_PAGE_GENERIC, HID_USAGE_GENERIC_RZ},    /* REL_RZ */
    {0, 0},                                            /* REL_HWHEEL */
    {HID_USAGE_PAGE_GENERIC, HID_USAGE_GENERIC_DIAL},  /* REL_DIAL */
    {HID_USAGE_PAGE_GENERIC, HID_USAGE_GENERIC_WHEEL}, /* REL_WHEEL */
    {0, 0}                                             /* REL_MISC */
};

#define HID_REL_MAX (REL_MISC+1)
#define TOP_REL_PAGE (HID_USAGE_PAGE_CONSUMER+1)

struct wine_input_absinfo {
    struct input_absinfo info;
    BYTE report_index;
};

struct wine_input_private {
    struct platform_private base;

    int buffer_length;
    BYTE *last_report_buffer;
    BYTE *current_report_buffer;
    enum { FIRST, NORMAL, DROPPED } report_state;

    int report_descriptor_size;
    BYTE *report_descriptor;

    int button_start;
    BYTE button_map[KEY_MAX];
    BYTE rel_map[HID_REL_MAX];
    BYTE hat_map[8];
    int hat_values[8];
    struct wine_input_absinfo abs_map[HID_ABS_MAX];
};

#define test_bit(arr,bit) (((BYTE*)(arr))[(bit)>>3]&(1<<((bit)&7)))

static BYTE *add_axis_block(BYTE *report_ptr, BYTE count, BYTE page, BYTE *usages, BOOL absolute, const struct wine_input_absinfo *absinfo)
{
    int i;
    memcpy(report_ptr, REPORT_AXIS_HEADER, sizeof(REPORT_AXIS_HEADER));
    report_ptr[IDX_AXIS_PAGE] = page;
    report_ptr += sizeof(REPORT_AXIS_HEADER);
    for (i = 0; i < count; i++)
    {
        memcpy(report_ptr, REPORT_AXIS_USAGE, sizeof(REPORT_AXIS_USAGE));
        report_ptr[IDX_AXIS_USAGE] = usages[i];
        report_ptr += sizeof(REPORT_AXIS_USAGE);
    }
    if (absolute)
    {
        memcpy(report_ptr, REPORT_ABS_AXIS_TAIL, sizeof(REPORT_ABS_AXIS_TAIL));
        if (absinfo)
        {
            *((int*)&report_ptr[IDX_ABS_LOG_MINIMUM]) = LE_DWORD(absinfo->info.minimum);
            *((int*)&report_ptr[IDX_ABS_LOG_MAXIMUM]) = LE_DWORD(absinfo->info.maximum);
            *((int*)&report_ptr[IDX_ABS_PHY_MINIMUM]) = LE_DWORD(absinfo->info.minimum);
            *((int*)&report_ptr[IDX_ABS_PHY_MAXIMUM]) = LE_DWORD(absinfo->info.maximum);
        }
        report_ptr[IDX_ABS_AXIS_COUNT] = count;
        report_ptr += sizeof(REPORT_ABS_AXIS_TAIL);
    }
    else
    {
        memcpy(report_ptr, REPORT_REL_AXIS_TAIL, sizeof(REPORT_REL_AXIS_TAIL));
        report_ptr[IDX_REL_AXIS_COUNT] = count;
        report_ptr += sizeof(REPORT_REL_AXIS_TAIL);
    }
    return report_ptr;
}

/* Minimal compatibility with code taken from steam-runtime-tools */
typedef int gboolean;
#define g_debug(fmt, ...) TRACE(fmt "\n", ## __VA_ARGS__)
#define G_N_ELEMENTS(arr) (sizeof(arr)/sizeof(arr[0]))

typedef enum
{
  SRT_INPUT_DEVICE_TYPE_FLAGS_JOYSTICK = (1 << 0),
  SRT_INPUT_DEVICE_TYPE_FLAGS_ACCELEROMETER = (1 << 1),
  SRT_INPUT_DEVICE_TYPE_FLAGS_KEYBOARD = (1 << 2),
  SRT_INPUT_DEVICE_TYPE_FLAGS_HAS_KEYS = (1 << 3),
  SRT_INPUT_DEVICE_TYPE_FLAGS_MOUSE = (1 << 4),
  SRT_INPUT_DEVICE_TYPE_FLAGS_TOUCHPAD = (1 << 5),
  SRT_INPUT_DEVICE_TYPE_FLAGS_TOUCHSCREEN = (1 << 6),
  SRT_INPUT_DEVICE_TYPE_FLAGS_TABLET = (1 << 7),
  SRT_INPUT_DEVICE_TYPE_FLAGS_TABLET_PAD = (1 << 8),
  SRT_INPUT_DEVICE_TYPE_FLAGS_POINTING_STICK = (1 << 9),
  SRT_INPUT_DEVICE_TYPE_FLAGS_SWITCH = (1 << 10),
  SRT_INPUT_DEVICE_TYPE_FLAGS_NONE = 0
} SrtInputDeviceTypeFlags;

#define BITS_PER_LONG           (sizeof (unsigned long) * CHAR_BIT)
#define LONGS_FOR_BITS(x)       ((((x)-1)/BITS_PER_LONG)+1)
typedef struct
{
  unsigned long ev[LONGS_FOR_BITS (EV_MAX)];
  unsigned long keys[LONGS_FOR_BITS (KEY_MAX)];
  unsigned long abs[LONGS_FOR_BITS (ABS_MAX)];
  unsigned long rel[LONGS_FOR_BITS (REL_MAX)];
  unsigned long ff[LONGS_FOR_BITS (FF_MAX)];
  unsigned long props[LONGS_FOR_BITS (INPUT_PROP_MAX)];
} SrtEvdevCapabilities;

static gboolean
_srt_get_caps_from_evdev (int fd,
                          unsigned int type,
                          unsigned long *bitmask,
                          size_t bitmask_len_longs)
{
  size_t bitmask_len_bytes = bitmask_len_longs * sizeof (*bitmask);

  memset (bitmask, 0, bitmask_len_bytes);

  if (ioctl (fd, EVIOCGBIT (type, bitmask_len_bytes), bitmask) < 0)
    return FALSE;

  return TRUE;
}

static gboolean
_srt_evdev_capabilities_set_from_evdev (SrtEvdevCapabilities *caps,
                                        int fd)
{
  if (_srt_get_caps_from_evdev (fd, 0, caps->ev, G_N_ELEMENTS (caps->ev)))
    {
      _srt_get_caps_from_evdev (fd, EV_KEY, caps->keys, G_N_ELEMENTS (caps->keys));
      _srt_get_caps_from_evdev (fd, EV_ABS, caps->abs, G_N_ELEMENTS (caps->abs));
      _srt_get_caps_from_evdev (fd, EV_REL, caps->rel, G_N_ELEMENTS (caps->rel));
      _srt_get_caps_from_evdev (fd, EV_FF, caps->ff, G_N_ELEMENTS (caps->ff));
      ioctl (fd, EVIOCGPROP (sizeof (caps->props)), caps->props);
      return TRUE;
    }

  memset (caps, 0, sizeof (*caps));
  return FALSE;
}

#define JOYSTICK_ABS_AXES \
  ((1 << ABS_X) | (1 << ABS_Y) \
   | (1 << ABS_RX) | (1 << ABS_RY) \
   | (1 << ABS_THROTTLE) | (1 << ABS_RUDDER) \
   | (1 << ABS_WHEEL) | (1 << ABS_GAS) | (1 << ABS_BRAKE) \
   | (1 << ABS_HAT0X) | (1 << ABS_HAT0Y) \
   | (1 << ABS_HAT1X) | (1 << ABS_HAT1Y) \
   | (1 << ABS_HAT2X) | (1 << ABS_HAT2Y) \
   | (1 << ABS_HAT3X) | (1 << ABS_HAT3Y))

static const unsigned int first_mouse_button = BTN_MOUSE;
static const unsigned int last_mouse_button = BTN_JOYSTICK - 1;

static const unsigned int first_joystick_button = BTN_JOYSTICK;
static const unsigned int last_joystick_button = BTN_GAMEPAD - 1;

static const unsigned int first_gamepad_button = BTN_GAMEPAD;
static const unsigned int last_gamepad_button = BTN_DIGI - 1;

static const unsigned int first_dpad_button = BTN_DPAD_UP;
static const unsigned int last_dpad_button = BTN_DPAD_RIGHT;

static const unsigned int first_extra_joystick_button = BTN_TRIGGER_HAPPY;
static const unsigned int last_extra_joystick_button = BTN_TRIGGER_HAPPY40;

SrtInputDeviceTypeFlags
_srt_evdev_capabilities_guess_type (const SrtEvdevCapabilities *caps)
{
  SrtInputDeviceTypeFlags flags = SRT_INPUT_DEVICE_TYPE_FLAGS_NONE;
  unsigned int i;
  gboolean has_joystick_axes = FALSE;
  gboolean has_joystick_buttons = FALSE;

  /* Some properties let us be fairly sure about a device */
  if (test_bit (caps->props, INPUT_PROP_ACCELEROMETER))
    {
      g_debug ("INPUT_PROP_ACCELEROMETER => is accelerometer");
      flags |= SRT_INPUT_DEVICE_TYPE_FLAGS_ACCELEROMETER;
    }

  if (test_bit (caps->props, INPUT_PROP_POINTING_STICK))
    {
      g_debug ("INPUT_PROP_POINTING_STICK => is pointing stick");
      flags |= SRT_INPUT_DEVICE_TYPE_FLAGS_POINTING_STICK;
    }

  if (test_bit (caps->props, INPUT_PROP_BUTTONPAD)
      || test_bit (caps->props, INPUT_PROP_TOPBUTTONPAD))
    {
      g_debug ("INPUT_PROP_[TOP]BUTTONPAD => is touchpad");
      flags |= SRT_INPUT_DEVICE_TYPE_FLAGS_TOUCHPAD;
    }

  /* Devices with a stylus or pen are assumed to be graphics tablets */
  if (test_bit (caps->keys, BTN_STYLUS)
      || test_bit (caps->keys, BTN_TOOL_PEN))
    {
      g_debug ("Stylus or pen => is tablet");
      flags |= SRT_INPUT_DEVICE_TYPE_FLAGS_TABLET;
    }

  /* Devices that accept a finger touch are assumed to be touchpads or
   * touchscreens.
   *
   * In Steam we mostly only care about these as a way to
   * reject non-joysticks, so we're not very precise here yet.
   *
   * SDL assumes that TOUCH means a touchscreen and FINGER
   * means a touchpad. */
  if (flags == SRT_INPUT_DEVICE_TYPE_FLAGS_NONE
      && (test_bit (caps->keys, BTN_TOOL_FINGER)
          || test_bit (caps->keys, BTN_TOUCH)
          || test_bit (caps->props, INPUT_PROP_SEMI_MT)))
    {
      g_debug ("Finger or touch or semi-MT => is touchpad or touchscreen");

      if (test_bit (caps->props, INPUT_PROP_POINTER))
        flags |= SRT_INPUT_DEVICE_TYPE_FLAGS_TOUCHPAD;
      else
        flags |= SRT_INPUT_DEVICE_TYPE_FLAGS_TOUCHSCREEN;
    }

  /* Devices with mouse buttons are ... probably mice? */
  if (flags == SRT_INPUT_DEVICE_TYPE_FLAGS_NONE)
    {
      for (i = first_mouse_button; i <= last_mouse_button; i++)
        {
          if (test_bit (caps->keys, i))
            {
              g_debug ("Mouse button => mouse");
              flags |= SRT_INPUT_DEVICE_TYPE_FLAGS_MOUSE;
            }
        }
    }

  if (flags == SRT_INPUT_DEVICE_TYPE_FLAGS_NONE)
    {
      for (i = ABS_X; i < ABS_Z; i++)
        {
          if (!test_bit (caps->abs, i))
            break;
        }

      /* If it has 3 axes and no buttons it's probably an accelerometer. */
      if (i == ABS_Z && !test_bit (caps->ev, EV_KEY))
        {
          g_debug ("3 left axes and no buttons => accelerometer");
          flags |= SRT_INPUT_DEVICE_TYPE_FLAGS_ACCELEROMETER;
        }

      /* Same for RX..RZ (e.g. Wiimote) */
      for (i = ABS_RX; i < ABS_RZ; i++)
        {
          if (!test_bit (caps->abs, i))
            break;
        }

      if (i == ABS_RZ && !test_bit (caps->ev, EV_KEY))
        {
          g_debug ("3 right axes and no buttons => accelerometer");
          flags |= SRT_INPUT_DEVICE_TYPE_FLAGS_ACCELEROMETER;
        }
    }

  /* Bits 1 to 31 are ESC, numbers and Q to D, which SDL and udev both
   * consider to be enough to count as a fully-functioned keyboard. */
  if ((caps->keys[0] & 0xfffffffe) == 0xfffffffe)
    {
      g_debug ("First few keys => keyboard");
      flags |= SRT_INPUT_DEVICE_TYPE_FLAGS_KEYBOARD;
    }

  /* If we have *any* keys, consider it to be something a bit
   * keyboard-like. Bits 0 to 63 are all keyboard keys.
   * Make sure we stop before reaching KEY_UP which is sometimes
   * used on game controller mappings, e.g. for the Wiimote. */
  for (i = 0; i < (64 / BITS_PER_LONG); i++)
    {
      if (caps->keys[i] != 0)
        flags |= SRT_INPUT_DEVICE_TYPE_FLAGS_HAS_KEYS;
    }

  if (caps->abs[0] & JOYSTICK_ABS_AXES)
    has_joystick_axes = TRUE;

  /* Flight stick buttons */
  for (i = first_joystick_button; i <= last_joystick_button; i++)
    {
      if (test_bit (caps->keys, i))
        has_joystick_buttons = TRUE;
    }

  /* Gamepad buttons (Xbox, PS3, etc.) */
  for (i = first_gamepad_button; i <= last_gamepad_button; i++)
    {
      if (test_bit (caps->keys, i))
        has_joystick_buttons = TRUE;
    }

  /* Gamepad digital dpad */
  for (i = first_dpad_button; i <= last_dpad_button; i++)
    {
      if (test_bit (caps->keys, i))
        has_joystick_buttons = TRUE;
    }

  /* Steering wheel gear-change buttons */
  for (i = BTN_GEAR_DOWN; i <= BTN_GEAR_UP; i++)
    {
      if (test_bit (caps->keys, i))
        has_joystick_buttons = TRUE;
    }

  /* Reserved space for extra game-controller buttons, e.g. on Corsair
   * gaming keyboards */
  for (i = first_extra_joystick_button; i <= last_extra_joystick_button; i++)
    {
      if (test_bit (caps->keys, i))
        has_joystick_buttons = TRUE;
    }

  if (test_bit (caps->keys, last_mouse_button))
    {
      /* Mice with a very large number of buttons can apparently
       * overflow into the joystick-button space, but they're still not
       * joysticks. */
      has_joystick_buttons = FALSE;
    }

  /* TODO: Do we want to consider BTN_0 up to BTN_9 to be joystick buttons?
   * libmanette and SDL look for BTN_1, udev does not.
   *
   * They're used by some game controllers, like BTN_1 and BTN_2 for the
   * Wiimote, BTN_1..BTN_9 for the SpaceTec SpaceBall and BTN_0..BTN_3
   * for Playstation dance pads, but they're also used by
   * non-game-controllers like Logitech mice. For now we entirely ignore
   * these buttons: they are not evidence that it's a joystick, but
   * neither are they evidence that it *isn't* a joystick. */

  /* We consider it to be a joystick if there is some evidence that it is,
   * and no evidence that it's something else.
   *
   * Unlike SDL, we accept devices with only axes and no buttons as a
   * possible joystick, unless they have X/Y/Z axes in which case we
   * assume they're accelerometers. */
  if ((has_joystick_buttons || has_joystick_axes)
      && (flags == SRT_INPUT_DEVICE_TYPE_FLAGS_NONE))
    {
      g_debug ("Looks like a joystick");
      flags |= SRT_INPUT_DEVICE_TYPE_FLAGS_JOYSTICK;
    }

  /* If we have *any* keys below BTN_MISC, consider it to be something
   * a bit keyboard-like, but don't rule out *also* being considered
   * to be a joystick (again for e.g. the Wiimote). */
  for (i = 0; i < (BTN_MISC / BITS_PER_LONG); i++)
    {
      if (caps->keys[i] != 0)
        flags |= SRT_INPUT_DEVICE_TYPE_FLAGS_HAS_KEYS;
    }

  /* Also non-exclusive: don't rule out a device being a joystick and
   * having a switch */
  if (test_bit (caps->ev, EV_SW))
    flags |= SRT_INPUT_DEVICE_TYPE_FLAGS_SWITCH;

  return flags;
}

static const BYTE* what_am_I(struct udev_device *dev,
                             int fd)
{
    static const BYTE Unknown[2]     = {HID_USAGE_PAGE_GENERIC, 0};
    static const BYTE Mouse[2]       = {HID_USAGE_PAGE_GENERIC, HID_USAGE_GENERIC_MOUSE};
    static const BYTE Keyboard[2]    = {HID_USAGE_PAGE_GENERIC, HID_USAGE_GENERIC_KEYBOARD};
    static const BYTE Gamepad[2]     = {HID_USAGE_PAGE_GENERIC, HID_USAGE_GENERIC_GAMEPAD};
    static const BYTE Keypad[2]      = {HID_USAGE_PAGE_GENERIC, HID_USAGE_GENERIC_KEYPAD};
    static const BYTE Tablet[2]      = {HID_USAGE_PAGE_DIGITIZER, HID_USAGE_DIGITIZER_PEN};
    static const BYTE Touchscreen[2] = {HID_USAGE_PAGE_DIGITIZER, HID_USAGE_DIGITIZER_TOUCH_SCREEN};
    static const BYTE Touchpad[2]    = {HID_USAGE_PAGE_DIGITIZER, HID_USAGE_DIGITIZER_TOUCH_PAD};
    SrtEvdevCapabilities caps;

    struct udev_device *parent = dev;

    /* Look to the parents until we get a clue */
    while (parent)
    {
        if (udev_device_get_property_value(parent, "ID_INPUT_MOUSE"))
            return Mouse;
        else if (udev_device_get_property_value(parent, "ID_INPUT_KEYBOARD"))
            return Keyboard;
        else if (udev_device_get_property_value(parent, "ID_INPUT_JOYSTICK"))
            return Gamepad;
        else if (udev_device_get_property_value(parent, "ID_INPUT_KEY"))
            return Keypad;
        else if (udev_device_get_property_value(parent, "ID_INPUT_TOUCHPAD"))
            return Touchpad;
        else if (udev_device_get_property_value(parent, "ID_INPUT_TOUCHSCREEN"))
            return Touchscreen;
        else if (udev_device_get_property_value(parent, "ID_INPUT_TABLET"))
            return Tablet;

        parent = udev_device_get_parent_with_subsystem_devtype(parent, "input", NULL);
    }

    /* In a container, udev properties might not be available. Fall back to deriving the device
     * type from the fd's evdev capabilities. */
    if (_srt_evdev_capabilities_set_from_evdev (&caps, fd))
    {
        SrtInputDeviceTypeFlags guessed_type;

        guessed_type = _srt_evdev_capabilities_guess_type (&caps);

        if (guessed_type & (SRT_INPUT_DEVICE_TYPE_FLAGS_MOUSE
                            | SRT_INPUT_DEVICE_TYPE_FLAGS_POINTING_STICK))
            return Mouse;
        else if (guessed_type & SRT_INPUT_DEVICE_TYPE_FLAGS_KEYBOARD)
            return Keyboard;
        else if (guessed_type & SRT_INPUT_DEVICE_TYPE_FLAGS_JOYSTICK)
            return Gamepad;
        else if (guessed_type & SRT_INPUT_DEVICE_TYPE_FLAGS_HAS_KEYS)
            return Keypad;
        else if (guessed_type & SRT_INPUT_DEVICE_TYPE_FLAGS_TOUCHPAD)
            return Touchpad;
        else if (guessed_type & SRT_INPUT_DEVICE_TYPE_FLAGS_TOUCHSCREEN)
            return Touchscreen;
        else if (guessed_type & SRT_INPUT_DEVICE_TYPE_FLAGS_TABLET)
            return Tablet;

        /* Mapped to Unknown: ACCELEROMETER, TABLET_PAD, SWITCH. */
    }

    return Unknown;
}

static void set_button_value(int index, int value, BYTE* buffer)
{
    int bindex = index / 8;
    int b = index % 8;
    BYTE mask;

    mask = 1<<b;
    if (value)
        buffer[bindex] = buffer[bindex] | mask;
    else
    {
        mask = ~mask;
        buffer[bindex] = buffer[bindex] & mask;
    }
}

static void set_abs_axis_value(struct wine_input_private *ext, int code, int value)
{
    int index;
    /* check for hatswitches */
    if (code <= ABS_HAT3Y && code >= ABS_HAT0X)
    {
        index = code - ABS_HAT0X;
        ext->hat_values[index] = value;
        if ((code - ABS_HAT0X) % 2)
            index--;
        /* 8 1 2
         * 7 0 3
         * 6 5 4 */
        if (ext->hat_values[index] == 0)
        {
            if (ext->hat_values[index+1] == 0)
                value = 0;
            else if (ext->hat_values[index+1] < 0)
                value = 1;
            else
                value = 5;
        }
        else if (ext->hat_values[index] > 0)
        {
            if (ext->hat_values[index+1] == 0)
                value = 3;
            else if (ext->hat_values[index+1] < 0)
                value = 2;
            else
                value = 4;
        }
        else
        {
            if (ext->hat_values[index+1] == 0)
                value = 7;
            else if (ext->hat_values[index+1] < 0)
                value = 8;
            else
                value = 6;
        }
        ext->current_report_buffer[ext->hat_map[index]] = value;
    }
    else if (code < HID_ABS_MAX && ABS_TO_HID_MAP[code][0] != 0)
    {
        index = ext->abs_map[code].report_index;
        *((DWORD*)&ext->current_report_buffer[index]) = LE_DWORD(value);
    }
}

static void set_rel_axis_value(struct wine_input_private *ext, int code, int value)
{
    int index;
    if (code < HID_REL_MAX && REL_TO_HID_MAP[code][0] != 0)
    {
        index = ext->rel_map[code];
        if (value > 127) value = 127;
        if (value < -127) value = -127;
        ext->current_report_buffer[index] = value;
    }
}

static INT count_buttons(int device_fd, BYTE *map)
{
    int i;
    int button_count = 0;
    BYTE keybits[(KEY_MAX+7)/8];

    if (ioctl(device_fd, EVIOCGBIT(EV_KEY, sizeof(keybits)), keybits) == -1)
    {
        WARN("ioctl(EVIOCGBIT, EV_KEY) failed: %d %s\n", errno, strerror(errno));
        return FALSE;
    }

    for (i = BTN_MISC; i < KEY_MAX; i++)
    {
        if (test_bit(keybits, i))
        {
            if (map) map[i] = button_count;
            button_count++;
        }
    }
    return button_count;
}

static INT count_abs_axis(int device_fd)
{
    BYTE absbits[(ABS_MAX+7)/8];
    int abs_count = 0;
    int i;

    if (ioctl(device_fd, EVIOCGBIT(EV_ABS, sizeof(absbits)), absbits) == -1)
    {
        WARN("ioctl(EVIOCGBIT, EV_ABS) failed: %d %s\n", errno, strerror(errno));
        return 0;
    }

    for (i = 0; i < HID_ABS_MAX; i++)
        if (test_bit(absbits, i) &&
            (ABS_TO_HID_MAP[i][1] >= HID_USAGE_GENERIC_X &&
             ABS_TO_HID_MAP[i][1] <= HID_USAGE_GENERIC_WHEEL))
                abs_count++;
    return abs_count;
}

static BOOL build_report_descriptor(struct wine_input_private *ext, struct udev_device *dev)
{
    int abs_pages[TOP_ABS_PAGE][HID_ABS_MAX+1];
    int rel_pages[TOP_REL_PAGE][HID_REL_MAX+1];
    BYTE absbits[(ABS_MAX+7)/8];
    BYTE relbits[(REL_MAX+7)/8];
    BYTE *report_ptr;
    INT i, descript_size;
    INT report_size;
    INT button_count, abs_count, rel_count, hat_count;
    const BYTE *device_usage = what_am_I(dev, ext->base.device_fd);

    if (ioctl(ext->base.device_fd, EVIOCGBIT(EV_REL, sizeof(relbits)), relbits) == -1)
    {
        WARN("ioctl(EVIOCGBIT, EV_REL) failed: %d %s\n", errno, strerror(errno));
        return FALSE;
    }
    if (ioctl(ext->base.device_fd, EVIOCGBIT(EV_ABS, sizeof(absbits)), absbits) == -1)
    {
        WARN("ioctl(EVIOCGBIT, EV_ABS) failed: %d %s\n", errno, strerror(errno));
        return FALSE;
    }

    descript_size = sizeof(REPORT_HEADER) + sizeof(REPORT_TAIL);
    report_size = 0;

    abs_count = 0;
    memset(abs_pages, 0, sizeof(abs_pages));
    for (i = 0; i < HID_ABS_MAX; i++)
        if (test_bit(absbits, i))
        {
            abs_pages[ABS_TO_HID_MAP[i][0]][0]++;
            abs_pages[ABS_TO_HID_MAP[i][0]][abs_pages[ABS_TO_HID_MAP[i][0]][0]] = i;

            ioctl(ext->base.device_fd, EVIOCGABS(i), &(ext->abs_map[i]));
        }
    /* Skip page 0, aka HID_USAGE_PAGE_UNDEFINED */
    for (i = 1; i < TOP_ABS_PAGE; i++)
        if (abs_pages[i][0] > 0)
        {
            int j;
            descript_size += sizeof(REPORT_AXIS_USAGE) * abs_pages[i][0];
            for (j = 1; j <= abs_pages[i][0]; j++)
            {
                ext->abs_map[abs_pages[i][j]].report_index = report_size;
                report_size+=4;
            }
            abs_count++;
        }
    descript_size += sizeof(REPORT_AXIS_HEADER) * abs_count;
    descript_size += sizeof(REPORT_ABS_AXIS_TAIL) * abs_count;

    rel_count = 0;
    memset(rel_pages, 0, sizeof(rel_pages));
    for (i = 0; i < HID_REL_MAX; i++)
        if (test_bit(relbits, i))
        {
            rel_pages[REL_TO_HID_MAP[i][0]][0]++;
            rel_pages[REL_TO_HID_MAP[i][0]][rel_pages[REL_TO_HID_MAP[i][0]][0]] = i;
        }
    /* Skip page 0, aka HID_USAGE_PAGE_UNDEFINED */
    for (i = 1; i < TOP_REL_PAGE; i++)
        if (rel_pages[i][0] > 0)
        {
            int j;
            descript_size += sizeof(REPORT_AXIS_USAGE) * rel_pages[i][0];
            for (j = 1; j <= rel_pages[i][0]; j++)
            {
                ext->rel_map[rel_pages[i][j]] = report_size;
                report_size++;
            }
            rel_count++;
        }
    descript_size += sizeof(REPORT_AXIS_HEADER) * rel_count;
    descript_size += sizeof(REPORT_REL_AXIS_TAIL) * rel_count;

    /* For now lump all buttons just into incremental usages, Ignore Keys */
    ext->button_start = report_size;
    button_count = count_buttons(ext->base.device_fd, ext->button_map);
    if (button_count)
    {
        descript_size += sizeof(REPORT_BUTTONS);
        if (button_count % 8)
            descript_size += sizeof(REPORT_PADDING);
        report_size += (button_count + 7) / 8;
    }

    hat_count = 0;
    for (i = ABS_HAT0X; i <=ABS_HAT3X; i+=2)
        if (test_bit(absbits, i))
        {
            ext->hat_map[i - ABS_HAT0X] = report_size;
            ext->hat_values[i - ABS_HAT0X] = 0;
            ext->hat_values[i - ABS_HAT0X + 1] = 0;
            report_size++;
            hat_count++;
        }
    if (hat_count > 0)
        descript_size += sizeof(REPORT_HATSWITCH);

    TRACE("Report Descriptor will be %i bytes\n", descript_size);
    TRACE("Report will be %i bytes\n", report_size);

    ext->report_descriptor = HeapAlloc(GetProcessHeap(), 0, descript_size);
    if (!ext->report_descriptor)
    {
        ERR("Failed to alloc report descriptor\n");
        return FALSE;
    }
    report_ptr = ext->report_descriptor;

    memcpy(report_ptr, REPORT_HEADER, sizeof(REPORT_HEADER));
    report_ptr[IDX_HEADER_PAGE] = device_usage[0];
    report_ptr[IDX_HEADER_USAGE] = device_usage[1];
    report_ptr += sizeof(REPORT_HEADER);
    if (abs_count)
    {
        for (i = 1; i < TOP_ABS_PAGE; i++)
        {
            if (abs_pages[i][0])
            {
                BYTE usages[HID_ABS_MAX];
                int j;
                for (j = 0; j < abs_pages[i][0]; j++)
                    usages[j] = ABS_TO_HID_MAP[abs_pages[i][j+1]][1];
                report_ptr = add_axis_block(report_ptr, abs_pages[i][0], i, usages, TRUE, &ext->abs_map[abs_pages[i][1]]);
            }
        }
    }
    if (rel_count)
    {
        for (i = 1; i < TOP_REL_PAGE; i++)
        {
            if (rel_pages[i][0])
            {
                BYTE usages[HID_REL_MAX];
                int j;
                for (j = 0; j < rel_pages[i][0]; j++)
                    usages[j] = REL_TO_HID_MAP[rel_pages[i][j+1]][1];
                report_ptr = add_axis_block(report_ptr, rel_pages[i][0], i, usages, FALSE, NULL);
            }
        }
    }
    if (button_count)
    {
        report_ptr = add_button_block(report_ptr, 1, button_count);
        if (button_count % 8)
        {
            BYTE padding = 8 - (button_count % 8);
            report_ptr = add_padding_block(report_ptr, padding);
        }
    }
    if (hat_count)
        report_ptr = add_hatswitch(report_ptr, hat_count);

    memcpy(report_ptr, REPORT_TAIL, sizeof(REPORT_TAIL));

    ext->report_descriptor_size = descript_size;
    ext->buffer_length = report_size;
    ext->current_report_buffer = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, report_size);
    if (ext->current_report_buffer == NULL)
    {
        ERR("Failed to alloc report buffer\n");
        HeapFree(GetProcessHeap(), 0, ext->report_descriptor);
        return FALSE;
    }
    ext->last_report_buffer = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, report_size);
    if (ext->last_report_buffer == NULL)
    {
        ERR("Failed to alloc report buffer\n");
        HeapFree(GetProcessHeap(), 0, ext->report_descriptor);
        HeapFree(GetProcessHeap(), 0, ext->current_report_buffer);
        return FALSE;
    }
    ext->report_state = FIRST;

    /* Initialize axis in the report */
    for (i = 0; i < HID_ABS_MAX; i++)
        if (test_bit(absbits, i))
            set_abs_axis_value(ext, i, ext->abs_map[i].info.value);

    return TRUE;
}

static BOOL set_report_from_event(struct wine_input_private *ext, struct input_event *ie)
{
    switch(ie->type)
    {
#ifdef EV_SYN
        case EV_SYN:
            switch (ie->code)
            {
                case SYN_REPORT:
                    if (ext->report_state == NORMAL)
                    {
                        memcpy(ext->last_report_buffer, ext->current_report_buffer, ext->buffer_length);
                        return TRUE;
                    }
                    else
                    {
                        if (ext->report_state == DROPPED)
                            memcpy(ext->current_report_buffer, ext->last_report_buffer, ext->buffer_length);
                        ext->report_state = NORMAL;
                    }
                    break;
                case SYN_DROPPED:
                    TRACE_(hid_report)("received SY_DROPPED\n");
                    ext->report_state = DROPPED;
            }
            return FALSE;
#endif
#ifdef EV_MSC
        case EV_MSC:
            return FALSE;
#endif
        case EV_KEY:
            set_button_value(ext->button_start + ext->button_map[ie->code], ie->value, ext->current_report_buffer);
            return FALSE;
        case EV_ABS:
            set_abs_axis_value(ext, ie->code, ie->value);
            return FALSE;
        case EV_REL:
            set_rel_axis_value(ext, ie->code, ie->value);
            return FALSE;
        default:
            ERR("TODO: Process Report (%i, %i)\n",ie->type, ie->code);
            return FALSE;
    }
}
#endif

static inline WCHAR *strdupAtoW(const char *src)
{
    WCHAR *dst;
    DWORD len;
    if (!src) return NULL;
    len = MultiByteToWideChar(CP_UNIXCP, 0, src, -1, NULL, 0);
    if ((dst = HeapAlloc(GetProcessHeap(), 0, len * sizeof(WCHAR))))
        MultiByteToWideChar(CP_UNIXCP, 0, src, -1, dst, len);
    return dst;
}

static WCHAR *get_sysattr_string(struct udev_device *dev, const char *sysattr)
{
    const char *attr = udev_device_get_sysattr_value(dev, sysattr);
    if (!attr)
    {
        WARN("Could not get %s from device\n", sysattr);
        return NULL;
    }
    return strdupAtoW(attr);
}

static void parse_uevent_info(const char *uevent, DWORD *bus_type, DWORD *vendor_id,
                             DWORD *product_id, WORD *input, WCHAR **serial_number, WCHAR **product)
{
    char *tmp;
    char *saveptr = NULL;
    char *line;
    char *key;
    char *value;

    tmp = heap_alloc(strlen(uevent) + 1);
    strcpy(tmp, uevent);
    line = strtok_r(tmp, "\n", &saveptr);
    while (line != NULL)
    {
        /* line: "KEY=value" */
        key = line;
        value = strchr(line, '=');
        if (!value)
        {
            goto next_line;
        }
        *value = '\0';
        value++;

        if (strcmp(key, "HID_ID") == 0)
        {
            /**
             *        type vendor   product
             * HID_ID=0003:000005AC:00008242
             **/
            sscanf(value, "%x:%x:%x", bus_type, vendor_id, product_id);
        }
        else if (strcmp(key, "HID_UNIQ") == 0)
        {
            /* The caller has to free the serial number */
            if (*value)
            {
                *serial_number = strdupAtoW(value);
            }
        }
        else if (product && strcmp(key, "HID_NAME") == 0)
        {
            /* The caller has to free the product name */
            if (*value)
            {
                *product = strdupAtoW(value);
            }
        }
        else if (strcmp(key, "HID_PHYS") == 0)
        {
            const char *input_no = strstr(value, "input");
            if (input_no)
                *input = atoi(input_no+5 );
        }

next_line:
        line = strtok_r(NULL, "\n", &saveptr);
    }

    heap_free(tmp);
}

static int compare_platform_device(DEVICE_OBJECT *device, void *platform_dev)
{
    struct udev_device *dev1 = impl_from_DEVICE_OBJECT(device)->udev_device;
    struct udev_device *dev2 = platform_dev;
    return strcmp(udev_device_get_syspath(dev1), udev_device_get_syspath(dev2));
}

static NTSTATUS hidraw_get_reportdescriptor(DEVICE_OBJECT *device, BYTE *buffer, DWORD length, DWORD *out_length)
{
#ifdef HAVE_LINUX_HIDRAW_H
    struct hidraw_report_descriptor descriptor;
    struct platform_private *private = impl_from_DEVICE_OBJECT(device);

    if (ioctl(private->device_fd, HIDIOCGRDESCSIZE, &descriptor.size) == -1)
    {
        WARN("ioctl(HIDIOCGRDESCSIZE) failed: %d %s\n", errno, strerror(errno));
        return STATUS_UNSUCCESSFUL;
    }

    *out_length = descriptor.size;

    if (length < descriptor.size)
        return STATUS_BUFFER_TOO_SMALL;
    if (!descriptor.size)
        return STATUS_SUCCESS;

    if (ioctl(private->device_fd, HIDIOCGRDESC, &descriptor) == -1)
    {
        WARN("ioctl(HIDIOCGRDESC) failed: %d %s\n", errno, strerror(errno));
        return STATUS_UNSUCCESSFUL;
    }

    memcpy(buffer, descriptor.value, descriptor.size);
    return STATUS_SUCCESS;
#else
    return STATUS_NOT_IMPLEMENTED;
#endif
}

static NTSTATUS hidraw_get_string(DEVICE_OBJECT *device, DWORD index, WCHAR *buffer, DWORD length)
{
    struct udev_device *usbdev, *hiddev;
    struct platform_private *private = impl_from_DEVICE_OBJECT(device);
    WCHAR *str = NULL;

    hiddev = udev_device_get_parent_with_subsystem_devtype(private->udev_device, "hid", NULL);
    usbdev = udev_device_get_parent_with_subsystem_devtype(private->udev_device, "usb", "usb_device");

    if (private->bus_type == BUS_BLUETOOTH && hiddev)
    {
        DWORD bus_type, vid, pid;
        WORD input;
        WCHAR *serial = NULL, *product = NULL;

        /* udev doesn't report this info, so we have to extract it from uevent property */

        parse_uevent_info(udev_device_get_sysattr_value(hiddev, "uevent"),
                &bus_type, &vid, &pid, &input, &serial, &product);

        switch (index)
        {
            case HID_STRING_ID_IPRODUCT:
                str = product;
                HeapFree(GetProcessHeap(), 0, serial);
                break;
            case HID_STRING_ID_IMANUFACTURER:
                /* TODO */
                break;
            case HID_STRING_ID_ISERIALNUMBER:
                str = serial;
                HeapFree(GetProcessHeap(), 0, product);
                break;
            default:
                ERR("Unhandled string index %08x\n", index);
                return STATUS_NOT_IMPLEMENTED;
        }
    }
    else if (usbdev)
    {
        switch (index)
        {
            case HID_STRING_ID_IPRODUCT:
                str = get_sysattr_string(usbdev, "product");
                break;
            case HID_STRING_ID_IMANUFACTURER:
                str = get_sysattr_string(usbdev, "manufacturer");
                break;
            case HID_STRING_ID_ISERIALNUMBER:
                str = get_sysattr_string(usbdev, "serial");
                break;
            default:
                ERR("Unhandled string index %08x\n", index);
                return STATUS_NOT_IMPLEMENTED;
        }
    }
    else
    {
#ifdef HAVE_LINUX_HIDRAW_H
        switch (index)
        {
            case HID_STRING_ID_IPRODUCT:
            {
                char buf[MAX_PATH];
                if (ioctl(private->device_fd, HIDIOCGRAWNAME(MAX_PATH), buf) == -1)
                    WARN("ioctl(HIDIOCGRAWNAME) failed: %d %s\n", errno, strerror(errno));
                else
                    str = strdupAtoW(buf);
                break;
            }
            case HID_STRING_ID_IMANUFACTURER:
                break;
            case HID_STRING_ID_ISERIALNUMBER:
                break;
            default:
                ERR("Unhandled string index %08x\n", index);
                return STATUS_NOT_IMPLEMENTED;
        }
#else
        return STATUS_NOT_IMPLEMENTED;
#endif
    }

    if (!str)
    {
        if (!length) return STATUS_BUFFER_TOO_SMALL;
        buffer[0] = 0;
        return STATUS_SUCCESS;
    }

    if (length <= strlenW(str))
    {
        HeapFree(GetProcessHeap(), 0, str);
        return STATUS_BUFFER_TOO_SMALL;
    }

    strcpyW(buffer, str);
    HeapFree(GetProcessHeap(), 0, str);
    return STATUS_SUCCESS;
}

static DWORD CALLBACK device_report_thread(void *args)
{
    DEVICE_OBJECT *device = (DEVICE_OBJECT*)args;
    struct platform_private *private = impl_from_DEVICE_OBJECT(device);
    struct pollfd plfds[2];

    plfds[0].fd = private->device_fd;
    plfds[0].events = POLLIN;
    plfds[0].revents = 0;
    plfds[1].fd = private->control_pipe[0];
    plfds[1].events = POLLIN;
    plfds[1].revents = 0;

    while (1)
    {
        int size;
        BYTE report_buffer[1024];
        BOOL overlay_enabled = FALSE;

        if (poll(plfds, 2, -1) <= 0) continue;
        if (plfds[1].revents)
            break;

        if (WaitForSingleObject(steam_overlay_event, 0) == WAIT_OBJECT_0)
            overlay_enabled = TRUE;

        size = read(plfds[0].fd, report_buffer, sizeof(report_buffer));
        if (size == -1)
            TRACE_(hid_report)("Read failed. Likely an unplugged device %d %s\n", errno, strerror(errno));
        else if (size == 0)
            TRACE_(hid_report)("Failed to read report\n");
        else if (overlay_enabled)
            TRACE_(hid_report)("Overlay is enabled, dropping report\n");
        else
        {
            if(private->quirks & QUIRK_DS4_BT)
            {
                /* Following the kernel example, report 17 is the only type we care about for
                 * DS4 over bluetooth. but it has two extra header bytes, so skip those. */
                if(report_buffer[0] == 0x11)
                {
                    /* update report number to match windows */
                    report_buffer[2] = 1;
                    process_hid_report(device, report_buffer + 2, size - 2);
                }
            }else
                process_hid_report(device, report_buffer, size);
        }
    }
    return 0;
}

static NTSTATUS begin_report_processing(DEVICE_OBJECT *device)
{
    struct platform_private *private = impl_from_DEVICE_OBJECT(device);

    if (private->report_thread)
        return STATUS_SUCCESS;

    if (pipe(private->control_pipe) != 0)
    {
        ERR("Control pipe creation failed\n");
        return STATUS_UNSUCCESSFUL;
    }

    private->report_thread = CreateThread(NULL, 0, device_report_thread, device, 0, NULL);
    if (!private->report_thread)
    {
        ERR("Unable to create device report thread\n");
        close(private->control_pipe[0]);
        close(private->control_pipe[1]);
        return STATUS_UNSUCCESSFUL;
    }
    else
        return STATUS_SUCCESS;
}

static NTSTATUS hidraw_set_output_report(DEVICE_OBJECT *device, UCHAR id, BYTE *report, DWORD length, ULONG_PTR *written)
{
    struct platform_private* ext = impl_from_DEVICE_OBJECT(device);
    int rc;

    if (id != 0)
        rc = write(ext->device_fd, report, length);
    else
    {
        BYTE report_buffer[1024];

        if (length + 1 > sizeof(report_buffer))
        {
            ERR("Output report buffer too small\n");
            return STATUS_UNSUCCESSFUL;
        }

        report_buffer[0] = 0;
        memcpy(&report_buffer[1], report, length);
        rc = write(ext->device_fd, report_buffer, length + 1);
    }
    if (rc > 0)
    {
        *written = rc;
        return STATUS_SUCCESS;
    }
    else
    {
        TRACE("write failed: %d %d %s\n", rc, errno, strerror(errno));
        *written = 0;
        return STATUS_UNSUCCESSFUL;
    }
}

static NTSTATUS hidraw_get_feature_report(DEVICE_OBJECT *device, UCHAR id, BYTE *report, DWORD length, ULONG_PTR *read)
{
#if defined(HAVE_LINUX_HIDRAW_H) && defined(HIDIOCGFEATURE)
    int rc;
    struct platform_private* ext = impl_from_DEVICE_OBJECT(device);
    report[0] = id;
    length = min(length, 0x1fff);
    rc = ioctl(ext->device_fd, HIDIOCGFEATURE(length), report);
    if (rc >= 0)
    {
        *read = rc;
        return STATUS_SUCCESS;
    }
    else
    {
        TRACE_(hid_report)("ioctl(HIDIOCGFEATURE(%d)) failed: %d %s\n", length, errno, strerror(errno));
        *read = 0;
        return STATUS_UNSUCCESSFUL;
    }
#else
    *read = 0;
    return STATUS_NOT_IMPLEMENTED;
#endif
}

static NTSTATUS hidraw_set_feature_report(DEVICE_OBJECT *device, UCHAR id, BYTE *report, DWORD length, ULONG_PTR *written)
{
#if defined(HAVE_LINUX_HIDRAW_H) && defined(HIDIOCSFEATURE)
    int rc;
    struct platform_private* ext = impl_from_DEVICE_OBJECT(device);
    BYTE *feature_buffer;
    BYTE buffer[8192];

    if (id == 0)
    {
        if (length + 1 > sizeof(buffer))
        {
            ERR("Output feature buffer too small\n");
            return STATUS_UNSUCCESSFUL;
        }
        buffer[0] = 0;
        memcpy(&buffer[1], report, length);
        feature_buffer = buffer;
        length = length + 1;
    }
    else
        feature_buffer = report;
    length = min(length, 0x1fff);
    rc = ioctl(ext->device_fd, HIDIOCSFEATURE(length), feature_buffer);
    if (rc >= 0)
    {
        *written = rc;
        return STATUS_SUCCESS;
    }
    else
    {
        TRACE_(hid_report)("ioctl(HIDIOCSFEATURE(%d)) failed: %d %s\n", length, errno, strerror(errno));
        *written = 0;
        return STATUS_UNSUCCESSFUL;
    }
#else
    *written = 0;
    return STATUS_NOT_IMPLEMENTED;
#endif
}

static const platform_vtbl hidraw_vtbl =
{
    compare_platform_device,
    hidraw_get_reportdescriptor,
    hidraw_get_string,
    begin_report_processing,
    hidraw_set_output_report,
    hidraw_get_feature_report,
    hidraw_set_feature_report,
};

#ifdef HAS_PROPER_INPUT_HEADER

static inline struct wine_input_private *input_impl_from_DEVICE_OBJECT(DEVICE_OBJECT *device)
{
    return (struct wine_input_private*)get_platform_private(device);
}

static NTSTATUS lnxev_get_reportdescriptor(DEVICE_OBJECT *device, BYTE *buffer, DWORD length, DWORD *out_length)
{
    struct wine_input_private *ext = input_impl_from_DEVICE_OBJECT(device);

    *out_length = ext->report_descriptor_size;

    if (length < ext->report_descriptor_size)
        return STATUS_BUFFER_TOO_SMALL;

    memcpy(buffer, ext->report_descriptor, ext->report_descriptor_size);

    return STATUS_SUCCESS;
}

static NTSTATUS lnxev_get_string(DEVICE_OBJECT *device, DWORD index, WCHAR *buffer, DWORD length)
{
    struct wine_input_private *ext = input_impl_from_DEVICE_OBJECT(device);
    char str[255];

    str[0] = 0;
    switch (index)
    {
        case HID_STRING_ID_IPRODUCT:
            ioctl(ext->base.device_fd, EVIOCGNAME(sizeof(str)), str);
            break;
        case HID_STRING_ID_IMANUFACTURER:
            strcpy(str,"evdev");
            break;
        case HID_STRING_ID_ISERIALNUMBER:
            ioctl(ext->base.device_fd, EVIOCGUNIQ(sizeof(str)), str);
            break;
        default:
            ERR("Unhandled string index %i\n", index);
    }

    MultiByteToWideChar(CP_ACP, 0, str, -1, buffer, length);
    return STATUS_SUCCESS;
}

static DWORD CALLBACK lnxev_device_report_thread(void *args)
{
    DEVICE_OBJECT *device = (DEVICE_OBJECT*)args;
    struct wine_input_private *private = input_impl_from_DEVICE_OBJECT(device);
    struct pollfd plfds[2];

    plfds[0].fd = private->base.device_fd;
    plfds[0].events = POLLIN;
    plfds[0].revents = 0;
    plfds[1].fd = private->base.control_pipe[0];
    plfds[1].events = POLLIN;
    plfds[1].revents = 0;

    while (1)
    {
        int size;
        struct input_event ie;

        if (poll(plfds, 2, -1) <= 0) continue;
        if (plfds[1].revents || !private->current_report_buffer || private->buffer_length == 0)
            break;
        size = read(plfds[0].fd, &ie, sizeof(ie));
        if (size == -1)
            TRACE_(hid_report)("Read failed. Likely an unplugged device\n");
        else if (size == 0)
            TRACE_(hid_report)("Failed to read report\n");
        else if (set_report_from_event(private, &ie))
            process_hid_report(device, private->current_report_buffer, private->buffer_length);
    }
    return 0;
}

static NTSTATUS lnxev_begin_report_processing(DEVICE_OBJECT *device)
{
    struct wine_input_private *private = input_impl_from_DEVICE_OBJECT(device);

    if (private->base.report_thread)
        return STATUS_SUCCESS;

    if (pipe(private->base.control_pipe) != 0)
    {
        ERR("Control pipe creation failed\n");
        return STATUS_UNSUCCESSFUL;
    }

    private->base.report_thread = CreateThread(NULL, 0, lnxev_device_report_thread, device, 0, NULL);
    if (!private->base.report_thread)
    {
        ERR("Unable to create device report thread\n");
        close(private->base.control_pipe[0]);
        close(private->base.control_pipe[1]);
        return STATUS_UNSUCCESSFUL;
    }
    return STATUS_SUCCESS;
}

static NTSTATUS lnxev_set_output_report(DEVICE_OBJECT *device, UCHAR id, BYTE *report, DWORD length, ULONG_PTR *written)
{
    *written = 0;
    return STATUS_NOT_IMPLEMENTED;
}

static NTSTATUS lnxev_get_feature_report(DEVICE_OBJECT *device, UCHAR id, BYTE *report, DWORD length, ULONG_PTR *read)
{
    *read = 0;
    return STATUS_NOT_IMPLEMENTED;
}

static NTSTATUS lnxev_set_feature_report(DEVICE_OBJECT *device, UCHAR id, BYTE *report, DWORD length, ULONG_PTR *written)
{
    *written = 0;
    return STATUS_NOT_IMPLEMENTED;
}

static const platform_vtbl lnxev_vtbl = {
    compare_platform_device,
    lnxev_get_reportdescriptor,
    lnxev_get_string,
    lnxev_begin_report_processing,
    lnxev_set_output_report,
    lnxev_get_feature_report,
    lnxev_set_feature_report,
};
#endif

/* Return 0 to stop enumeration if @device's canonical path in /sys is @context. */
static int stop_if_syspath_equals(DEVICE_OBJECT *device, void *context)
{
    struct platform_private *private = impl_from_DEVICE_OBJECT(device);
    const char *want_syspath = context;
    const char *syspath = udev_device_get_syspath(private->udev_device);

    if (!syspath)
        return 1;

    if (strcmp(syspath, want_syspath) == 0)
    {
        TRACE("Found device %p with syspath %s\n", private, debugstr_a(want_syspath));
        return 0;
    }

    return 1;
}

static DWORD a_to_bcd(const char *s)
{
    DWORD r = 0;
    const char *c;
    int shift = strlen(s) - 1;
    for (c = s; *c; ++c)
    {
        r |= (*c - '0') << (shift * 4);
        --shift;
    }
    return r;
}

static int check_for_vidpid(DEVICE_OBJECT *device, void* context)
{
    struct vidpid *vidpid = context;
    struct platform_private *dev = impl_from_DEVICE_OBJECT(device);
    return !(dev->vidpid.vid == vidpid->vid &&
        dev->vidpid.pid == vidpid->pid);
}

BOOL is_already_opened_by_hidraw(DWORD vid, DWORD pid)
{
    struct vidpid vidpid = {vid, pid};
    return bus_enumerate_hid_devices(&hidraw_vtbl, check_for_vidpid, &vidpid) != NULL;
}

static BOOL is_in_sdl_blacklist(DWORD vid, DWORD pid)
{
    char needle[16];
    const char *blacklist = getenv("SDL_GAMECONTROLLER_IGNORE_DEVICES");
    const char *whitelist = getenv("SDL_GAMECONTROLLER_IGNORE_DEVICES_EXCEPT");

    if (whitelist)
    {
        sprintf(needle, "0x%04x/0x%04x", vid, pid);

        return strcasestr(whitelist, needle) == NULL;
    }

    if (!blacklist)
        return FALSE;

    sprintf(needle, "0x%04x/0x%04x", vid, pid);

    return strcasestr(blacklist, needle) != NULL;
}

static void set_quirks(struct platform_private *private)
{
#define VID_SONY 0x054c
#define PID_SONY_DUALSHOCK_4 0x05c4
#define PID_SONY_DUALSHOCK_4_2 0x09cc
#define PID_SONY_DUALSHOCK_4_DONGLE 0x0ba0

    private->quirks = 0;

    switch(private->vidpid.vid)
    {
    case VID_SONY:
        switch(private->vidpid.pid)
        {
        case PID_SONY_DUALSHOCK_4:
        case PID_SONY_DUALSHOCK_4_2:
        case PID_SONY_DUALSHOCK_4_DONGLE:
            if(private->bus_type == BUS_BLUETOOTH)
                private->quirks |= QUIRK_DS4_BT;
            break;
        }
        break;
    }

    TRACE("for %04x/%04x, quirks set to: 0x%x\n", private->vidpid.vid,
            private->vidpid.pid, private->quirks);
}

static void try_add_device(struct udev_device *dev,
                           int fd)
{
    DWORD vid = 0, pid = 0, version = 0, bus_type = 0;
    struct udev_device *hiddev = NULL, *walk_device;
    DEVICE_OBJECT *device = NULL;
    DEVICE_OBJECT *dup = NULL;
    const char *subsystem;
    const char *devnode;
    WCHAR *serial = NULL;
    BOOL is_gamepad = FALSE;
    WORD input = -1;
    static const CHAR *base_serial = "0000";
    const platform_vtbl *vtbl = NULL;
#ifdef HAS_PROPER_INPUT_HEADER
    const platform_vtbl *other_vtbl = NULL;
#endif
    const char *syspath;

    if (!(devnode = udev_device_get_devnode(dev)))
    {
        if (fd >= 0)
            close(fd);

        return;
    }

    if (fd < 0)
    {

        if ((fd = open(devnode, O_RDWR)) == -1)
        {
            WARN("Unable to open udev device %s: %s\n", debugstr_a(devnode), strerror(errno));
            return;
        }
    }

    syspath = udev_device_get_syspath(dev);
    subsystem = udev_device_get_subsystem(dev);

    if (strcmp(subsystem, "hidraw") == 0)
    {
        vtbl = &hidraw_vtbl;
#ifdef HAS_PROPER_INPUT_HEADER
        other_vtbl = &lnxev_vtbl;
#endif
    }
#ifdef HAS_PROPER_INPUT_HEADER
    else if (strcmp(subsystem, "input") == 0)
    {
        vtbl = &lnxev_vtbl;
        other_vtbl = &hidraw_vtbl;
    }
#endif
    else
    {
        WARN("Unexpected subsystem %s for %s\n", debugstr_a(subsystem), debugstr_a(devnode));
        close(fd);
        return;
    }

    dup = bus_enumerate_hid_devices(vtbl, stop_if_syspath_equals, (void *) syspath);
    if (dup)
    {
        TRACE("Duplicate %s device (%p) found, not adding the new one\n",
              debugstr_a(syspath), dup);
        close(fd);
        return;
    }

    hiddev = udev_device_get_parent_with_subsystem_devtype(dev, "hid", NULL);
    if (hiddev)
    {
        const char *bcdDevice = NULL;
#ifdef HAS_PROPER_INPUT_HEADER
        if (other_vtbl)
            dup = bus_enumerate_hid_devices(other_vtbl, stop_if_syspath_equals, (void *) syspath);
        if (dup)
        {
            TRACE("Duplicate cross bus device %s (%p) found, not adding the new one\n",
                  debugstr_a(syspath), dup);
            close(fd);
            return;
        }
#endif
        parse_uevent_info(udev_device_get_sysattr_value(hiddev, "uevent"),
                          &bus_type, &vid, &pid, &input, &serial, NULL);
        if (serial == NULL)
            serial = strdupAtoW(base_serial);

        if(bus_type != BUS_BLUETOOTH)
        {
            walk_device = dev;
            while (walk_device && !bcdDevice)
            {
                bcdDevice = udev_device_get_sysattr_value(walk_device, "bcdDevice");
                walk_device = udev_device_get_parent(walk_device);
            }
            if (bcdDevice)
            {
                version = a_to_bcd(bcdDevice);
            }
        }
    }
#ifdef HAS_PROPER_INPUT_HEADER
    else
    {
        struct input_id device_id = {0};
        char device_uid[255];

        if (ioctl(fd, EVIOCGID, &device_id) < 0)
            WARN("ioctl(EVIOCGID) failed: %d %s\n", errno, strerror(errno));
        device_uid[0] = 0;
        if (ioctl(fd, EVIOCGUNIQ(254), device_uid) >= 0 && device_uid[0])
            serial = strdupAtoW(device_uid);

        vid = device_id.vendor;
        pid = device_id.product;
        version = device_id.version;
        bus_type = device_id.bustype;
    }
#else
    else
        WARN("Could not get device to query VID, PID, Version and Serial\n");
#endif

    if (is_steam_controller(vid, pid) || is_in_sdl_blacklist(vid, pid))
    {
        /* this device is being used as a virtual Steam controller */
        TRACE("hidraw %s: ignoring device %04x/%04x with virtual Steam controller\n", debugstr_a(devnode), vid, pid);
        close(fd);
        return;
    }

    if (is_xbox_gamepad(vid, pid))
    {
        /* SDL handles xbox (and steam) controllers */
        TRACE("hidraw %s: ignoring xinput device %04x/%04x\n", debugstr_a(devnode), vid, pid);
        close(fd);
        return;
    }
#ifdef HAS_PROPER_INPUT_HEADER
    else
    {
        int axes=0, buttons=0;
        axes = count_abs_axis(fd);
        buttons = count_buttons(fd, NULL);
        is_gamepad = (axes == 6  && buttons >= 14);
    }
#endif
    if (input == (WORD)-1 && is_gamepad)
        input = 0;


    TRACE("Found udev device %s (vid %04x, pid %04x, version %u, serial %s)\n",
          debugstr_a(devnode), vid, pid, version, debugstr_w(serial));

    if (vtbl == &hidraw_vtbl)
    {
        device = bus_create_hid_device(hidraw_busidW, vid, pid, input, version, 0, serial, is_gamepad,
                                       vtbl, sizeof(struct platform_private), FALSE);
    }
#ifdef HAS_PROPER_INPUT_HEADER
    else if (vtbl == &lnxev_vtbl)
    {
        device = bus_create_hid_device(lnxev_busidW, vid, pid, input, version, 0, serial, is_gamepad,
                                       vtbl, sizeof(struct wine_input_private), FALSE);
    }
#endif

    if (device)
    {
        struct platform_private *private = impl_from_DEVICE_OBJECT(device);
        private->udev_device = udev_device_ref(dev);
        private->device_fd = fd;
        private->vidpid.vid = vid;
        private->vidpid.pid = pid;
        private->bus_type = bus_type;
        private->vtbl = vtbl;
        set_quirks(private);

#ifdef HAS_PROPER_INPUT_HEADER
        if (private->vtbl == &lnxev_vtbl)
            if (!build_report_descriptor((struct wine_input_private*)private, dev))
            {
                ERR("Building report descriptor failed, removing device\n");
                close(fd);
                udev_device_unref(dev);
                bus_unlink_hid_device(device);
                bus_remove_hid_device(device);
                HeapFree(GetProcessHeap(), 0, serial);
                return;
            }
#endif
        IoInvalidateDeviceRelations(bus_pdo, BusRelations);
    }
    else
    {
        WARN("Ignoring device %s with subsystem %s\n", debugstr_a(devnode), subsystem);
        close(fd);
    }

    HeapFree(GetProcessHeap(), 0, serial);
}

/* Return 0 to stop enumeration if @device's canonical path in /dev is @context. */
static int stop_if_devnode_equals(DEVICE_OBJECT *device, void *context)
{
    struct platform_private *private = impl_from_DEVICE_OBJECT(device);
    const char *want_devnode = context;
    const char *devnode = udev_device_get_devnode(private->udev_device);

    if (!devnode)
        return 1;

    if (strcmp(devnode, want_devnode) == 0)
    {
        TRACE("Found device %p with devnode %s\n", private, debugstr_a(want_devnode));
        return 0;
    }

    return 1;
}

static void try_remove_device_by_devnode(const char *devnode)
{
    DEVICE_OBJECT *device = NULL;
    struct platform_private* private;
    struct udev_device *dev;

    TRACE("Removing device if present: %s\n", debugstr_a(devnode));
    device = bus_enumerate_hid_devices(&hidraw_vtbl, stop_if_devnode_equals, (void *) devnode);

#ifdef HAS_PROPER_INPUT_HEADER
    if (device == NULL)
        device = bus_enumerate_hid_devices(&lnxev_vtbl, stop_if_devnode_equals, (void *) devnode);
#endif
    if (!device) return;

    private = impl_from_DEVICE_OBJECT(device);
    TRACE("Removing %s device: devnode %s udev_device %p -> %p\n",
          (private->vtbl == &hidraw_vtbl ? "hidraw" : "evdev"),
          debugstr_a(devnode), private->udev_device, private);

    bus_unlink_hid_device(device);
    IoInvalidateDeviceRelations(bus_pdo, BusRelations);

    if (private->report_thread)
    {
        write(private->control_pipe[1], "q", 1);
        WaitForSingleObject(private->report_thread, INFINITE);
        close(private->control_pipe[0]);
        close(private->control_pipe[1]);
        CloseHandle(private->report_thread);
#ifdef HAS_PROPER_INPUT_HEADER
        if (private->vtbl == &lnxev_vtbl)
        {
            HeapFree(GetProcessHeap(), 0, ((struct wine_input_private*)private)->current_report_buffer);
            HeapFree(GetProcessHeap(), 0, ((struct wine_input_private*)private)->last_report_buffer);
        }
#endif
    }

#ifdef HAS_PROPER_INPUT_HEADER
    if (private->vtbl == &lnxev_vtbl)
    {
        struct wine_input_private *ext = (struct wine_input_private*)private;
        HeapFree(GetProcessHeap(), 0, ext->report_descriptor);
    }
#endif

    dev = private->udev_device;
    close(private->device_fd);
    bus_remove_hid_device(device);
    udev_device_unref(dev);
}

static void try_remove_device(struct udev_device *dev)
{
    const char *devnode = udev_device_get_devnode(dev);

    /* If it didn't have a device node, then we wouldn't be tracking it anyway */
    if (!devnode)
        return;

    try_remove_device_by_devnode(devnode);
}

/* inotify watch descriptors for create_monitor_direct() */
#ifdef HAVE_SYS_INOTIFY_H
static int dev_watch = -1;
static int devinput_watch = -1;
#endif

static int str_has_prefix(const char *str,
                          const char *prefix)
{
    return (strncmp(str, prefix, strlen(prefix)) == 0);
}

static int str_is_integer(const char *str)
{
    const char *p;

    if (*str == '\0')
        return 0;

    for (p = str; *p != '\0'; p++)
    {
        if (*p < '0' || *p > '9')
            return 0;
    }

    return 1;
}

static void maybe_add_devnode(const platform_vtbl *vtbl, const char *base, const char *dir,
                              const char *base_should_be, const char *subsystem)
{
    const char *udev_devnode;
    char devnode[MAX_PATH];
    char syslink[MAX_PATH];
    char *syspath = NULL;
    struct udev_device *dev = NULL;
    int fd = -1;

    TRACE("Considering %s/%s...\n", dir, base);

    if (!str_has_prefix(base, base_should_be))
        return;

    if (!str_is_integer(base + strlen(base_should_be)))
        return;

    snprintf(devnode, sizeof(devnode), "%s/%s", dir, base);
    fd = open(devnode, O_RDWR);

    if (fd < 0)
    {
        /* When using inotify monitoring, quietly ignore device nodes that we cannot read,
         * without emitting a warning.
         *
         * We can expect that a significant number of device nodes will be permanently
         * unreadable, such as the device nodes for keyboards and mice. We can also expect
         * that joysticks and game controllers will be temporarily unreadable until udevd
         * chmods them; we'll get another chance to open them when their attributes change. */
        TRACE("Unable to open %s, ignoring: %s\n", debugstr_a(devnode), strerror(errno));
        goto out;
    }

    snprintf(syslink, sizeof(syslink), "/sys/class/%s/%s", subsystem, base);
    TRACE("Resolving real path to %s\n", debugstr_a(syslink));
    syspath = realpath(syslink, NULL);

    if (!syspath)
    {
        WARN("Unable to resolve path \"%s\" for \"%s/%s\": %s\n",
             debugstr_a(syslink), dir, base, strerror(errno));
        goto out;
    }

    TRACE("Creating udev_device for %s\n", syspath);
    dev = udev_device_new_from_syspath(udev_context, syspath);
    udev_devnode = udev_device_get_devnode(dev);

    if (udev_devnode == NULL || strcmp(devnode, udev_devnode) != 0)
    {
        WARN("Tried to get udev device for \"%s\" but device node of \"%s\" -> \"%s\" is \"%s\"\n",
             debugstr_a(devnode), debugstr_a(syslink), debugstr_a(syspath),
             debugstr_a(udev_devnode));
        goto out;
    }

    TRACE("Adding device for %s\n", syspath);
    try_add_device(dev, fd);
    /* ownership was taken */
    fd = -1;

out:
    if (fd >= 0)
        close(fd);
    if (dev)
        udev_device_unref(dev);
    free(syspath);
}

static void build_initial_deviceset_direct(void)
{
    DIR *dir;
    struct dirent *dent;

    if (!disable_hidraw)
    {
        TRACE("Initial enumeration of /dev/hidraw*\n");
        dir = opendir("/dev");

        if (dir)
        {
            for (dent = readdir(dir); dent; dent = readdir(dir))
                maybe_add_devnode(&hidraw_vtbl, dent->d_name, "/dev", "hidraw", "hidraw");

            closedir(dir);
        }
        else
        {
            WARN("Unable to open /dev: %s\n", strerror(errno));
        }
    }

#ifdef HAS_PROPER_INPUT_HEADER
    if (!disable_input)
    {
        TRACE("Initial enumeration of /dev/input/event*\n");
        dir = opendir("/dev/input");

        if (dir)
        {
            for (dent = readdir(dir); dent; dent = readdir(dir))
                maybe_add_devnode(&lnxev_vtbl, dent->d_name, "/dev/input", "event", "input");

            closedir(dir);
        }
        else
        {
            WARN("Unable to open /dev/input: %s\n", strerror(errno));
        }
    }
#endif
}

static void build_initial_deviceset_udevd(void)
{
    struct udev_enumerate *enumerate;
    struct udev_list_entry *devices, *dev_list_entry;

    enumerate = udev_enumerate_new(udev_context);
    if (!enumerate)
    {
        WARN("Unable to create udev enumeration object\n");
        return;
    }

    if (!disable_hidraw)
        if (udev_enumerate_add_match_subsystem(enumerate, "hidraw") < 0)
            WARN("Failed to add subsystem 'hidraw' to enumeration\n");
#ifdef HAS_PROPER_INPUT_HEADER
    if (!disable_input)
    {
        if (udev_enumerate_add_match_subsystem(enumerate, "input") < 0)
            WARN("Failed to add subsystem 'input' to enumeration\n");
    }
#endif

    if (udev_enumerate_scan_devices(enumerate) < 0)
        WARN("Enumeration scan failed\n");

    devices = udev_enumerate_get_list_entry(enumerate);
    udev_list_entry_foreach(dev_list_entry, devices)
    {
        struct udev_device *dev;
        const char *path;

        path = udev_list_entry_get_name(dev_list_entry);
        if ((dev = udev_device_new_from_syspath(udev_context, path)))
        {
            try_add_device(dev, -1);
            udev_device_unref(dev);
        }
    }

    udev_enumerate_unref(enumerate);
}

static void create_inotify(struct pollfd *pfd)
{
#ifdef HAVE_SYS_INOTIFY_H
    int systems = 0;

    pfd->revents = 0;
    pfd->fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);

    if (pfd->fd < 0)
    {
        WARN("Unable to get inotify fd\n");
        goto error;
    }

    if (!disable_hidraw)
    {
        /* We need to watch for attribute changes in addition to
         * creation, because when a device is first created, it has
         * permissions that we can't read. When udev chmods it to
         * something that we maybe *can* read, we'll get an
         * IN_ATTRIB event to tell us. */
        dev_watch = inotify_add_watch(pfd->fd, "/dev",
                                      IN_CREATE | IN_DELETE | IN_MOVE | IN_ATTRIB);
        if (dev_watch < 0)
            WARN("Unable to initialize inotify for /dev: %s\n",
                 strerror(errno));
        else
            systems++;
    }
#ifdef HAS_PROPER_INPUT_HEADER
    if (!disable_input)
    {
        devinput_watch = inotify_add_watch(pfd[0].fd, "/dev/input",
                                           IN_CREATE | IN_DELETE | IN_MOVE | IN_ATTRIB);
        if (devinput_watch < 0)
            WARN("Unable to initialize inotify for /dev/input: %s\n",
                 strerror(errno));
        else
            systems++;
    }
#endif
    if (systems == 0)
    {
        WARN("No subsystems added to monitor\n");
        goto error;
    }

    pfd->events = POLLIN;
    return;

error:
    WARN("Failed to start monitoring\n");
    if (pfd->fd >= 0)
        close(pfd->fd);
    pfd->fd = -1;
#else
    WARN("Compiled without inotify support, cannot watch for new input devices\n");
    pfd->fd = -1;
#endif
}

static struct udev_monitor *create_monitor(struct pollfd *pfd)
{
    struct udev_monitor *monitor;
    int systems = 0;

    monitor = udev_monitor_new_from_netlink(udev_context, "udev");
    if (!monitor)
    {
        WARN("Unable to get udev monitor object\n");
        return NULL;
    }

    if (!disable_hidraw)
    {
        if (udev_monitor_filter_add_match_subsystem_devtype(monitor, "hidraw", NULL) < 0)
            WARN("Failed to add 'hidraw' subsystem to monitor\n");
        else
            systems++;
    }
#ifdef HAS_PROPER_INPUT_HEADER
    if (!disable_input)
    {
        if (udev_monitor_filter_add_match_subsystem_devtype(monitor, "input", NULL) < 0)
            WARN("Failed to add 'input' subsystem to monitor\n");
        else
            systems++;
    }
#endif
    if (systems == 0)
    {
        WARN("No subsystems added to monitor\n");
        goto error;
    }

    if (udev_monitor_enable_receiving(monitor) < 0)
        goto error;

    if ((pfd->fd = udev_monitor_get_fd(monitor)) >= 0)
    {
        pfd->events = POLLIN;
        return monitor;
    }

error:
    WARN("Failed to start monitoring\n");
    udev_monitor_unref(monitor);
    return NULL;
}

static void maybe_remove_devnode(const char *base, const char *dir, const char *base_should_be)
{
    char path[MAX_PATH];

    TRACE("Considering %s/%s...\n", dir, base);

    if (!str_has_prefix(base, base_should_be))
        return;

    if (!str_is_integer(base + strlen(base_should_be)))
        return;

    snprintf(path, sizeof(path), "%s/%s", dir, base);
    try_remove_device_by_devnode(path);
}

static void process_inotify_event(int fd)
{
#ifdef HAVE_SYS_INOTIFY_H
    union
    {
        struct inotify_event event;
        char storage[4096];
        char enough_for_inotify[sizeof(struct inotify_event) + NAME_MAX + 1];
    } buf;
    ssize_t bytes;
    size_t remain = 0;
    size_t len;

    bytes = read(fd, &buf, sizeof(buf));

    if (bytes > 0)
        remain = (size_t) bytes;

    while (remain > 0)
    {
        if (buf.event.len > 0)
        {
            if (buf.event.wd == dev_watch)
            {
                if (buf.event.mask & (IN_CREATE | IN_MOVED_TO | IN_ATTRIB))
                    maybe_add_devnode(&hidraw_vtbl, buf.event.name, "/dev", "hidraw", "hidraw");
                else if (buf.event.mask & (IN_DELETE | IN_MOVED_FROM))
                    maybe_remove_devnode(buf.event.name, "/dev", "hidraw");
            }
#ifdef HAS_PROPER_INPUT_HEADER
            else if (buf.event.wd == devinput_watch)
            {
                if (buf.event.mask & (IN_CREATE | IN_MOVED_TO | IN_ATTRIB))
                    maybe_add_devnode(&lnxev_vtbl, buf.event.name, "/dev/input", "event", "input");
                else if (buf.event.mask & (IN_DELETE | IN_MOVED_FROM))
                    maybe_remove_devnode(buf.event.name, "/dev/input", "event");
            }
#endif
        }

        len = sizeof(struct inotify_event) + buf.event.len;
        remain -= len;

        if (remain != 0)
            memmove(&buf.storage[0], &buf.storage[len], remain);
    }
#endif
}

static void process_monitor_event(struct udev_monitor *monitor)
{
    struct udev_device *dev;
    const char *action;

    dev = udev_monitor_receive_device(monitor);
    if (!dev)
    {
        FIXME("Failed to get device that has changed\n");
        return;
    }

    action = udev_device_get_action(dev);
    TRACE("Received action %s for udev device %s\n", debugstr_a(action),
          debugstr_a(udev_device_get_devnode(dev)));

    if (!action)
        WARN("No action received\n");
    else if (strcmp(action, "remove") == 0)
        try_remove_device(dev);
    else
        try_add_device(dev, -1);

    udev_device_unref(dev);
}

static DWORD CALLBACK deviceloop_thread(void *args)
{
    struct udev_monitor *monitor = NULL;
    HANDLE init_done = args;
    struct pollfd pfd[2];

    pfd[1].fd = deviceloop_control[0];
    pfd[1].events = POLLIN;
    pfd[1].revents = 0;

    if (bypass_udevd)
    {
        create_inotify(&pfd[0]);
        build_initial_deviceset_direct();
    }
    else
    {
        monitor = create_monitor(&pfd[0]);
        build_initial_deviceset_udevd();
    }

    SetEvent(init_done);

    while (pfd[0].fd >= 0)
    {
        if (poll(pfd, 2, -1) <= 0) continue;
        if (pfd[1].revents) break;

        if (monitor)
            process_monitor_event(monitor);
        else
            process_inotify_event(pfd[0].fd);
    }

    TRACE("Monitor thread exiting\n");
    if (monitor)
        udev_monitor_unref(monitor);
    return 0;
}

static int device_unload(DEVICE_OBJECT *device, void *context)
{
    try_remove_device(impl_from_DEVICE_OBJECT(device)->udev_device);
    return 1;
}

void udev_driver_unload( void )
{
    TRACE("Unload Driver\n");

    if (!deviceloop_handle)
        return;

    write(deviceloop_control[1], "q", 1);
    WaitForSingleObject(deviceloop_handle, INFINITE);
    close(deviceloop_control[0]);
    close(deviceloop_control[1]);
    CloseHandle(deviceloop_handle);

    bus_enumerate_hid_devices(&hidraw_vtbl, device_unload, NULL);
#ifdef HAS_PROPER_INPUT_HEADER
    bus_enumerate_hid_devices(&lnxev_vtbl, device_unload, NULL);
#endif

    CloseHandle(steam_overlay_event);
}

NTSTATUS udev_driver_init(void)
{
    HANDLE events[2];
    DWORD result;
    static const WCHAR disable_udevdW[] = {'D','i','s','a','b','l','e','U','d','e','v','d',0};
    static const UNICODE_STRING disable_udevd = {sizeof(disable_udevdW) - sizeof(WCHAR), sizeof(disable_udevdW), (WCHAR*)disable_udevdW};
    static const WCHAR hidraw_disabledW[] = {'D','i','s','a','b','l','e','H','i','d','r','a','w',0};
    static const UNICODE_STRING hidraw_disabled = {sizeof(hidraw_disabledW) - sizeof(WCHAR), sizeof(hidraw_disabledW), (WCHAR*)hidraw_disabledW};
    static const WCHAR input_disabledW[] = {'D','i','s','a','b','l','e','I','n','p','u','t',0};
    static const UNICODE_STRING input_disabled = {sizeof(input_disabledW) - sizeof(WCHAR), sizeof(input_disabledW), (WCHAR*)input_disabledW};

    steam_overlay_event = CreateEventA(NULL, TRUE, FALSE, "__wine_steamclient_GameOverlayActivated");

    if (pipe(deviceloop_control) != 0)
    {
        ERR("Control pipe creation failed\n");
        return STATUS_UNSUCCESSFUL;
    }

    if (!(udev_context = udev_new()))
    {
        ERR("Can't create udev object\n");
        goto error;
    }

    if (access ("/run/pressure-vessel", R_OK)
        || access ("/.flatpak-info", R_OK))
    {
        TRACE("Container detected, bypassing udevd by default\n");
        bypass_udevd = 1;
    }

    bypass_udevd = check_bus_option(&disable_udevd, bypass_udevd);
    if (bypass_udevd)
        TRACE("udev disabled, falling back to inotify\n");

    disable_hidraw = check_bus_option(&hidraw_disabled, 0);
    if (disable_hidraw)
        TRACE("UDEV hidraw devices disabled in registry\n");

#ifdef HAS_PROPER_INPUT_HEADER
    disable_input = check_bus_option(&input_disabled, 1);
    if (disable_input)
        TRACE("UDEV input devices disabled in registry or by default\n");
#endif

    if (!(events[0] = CreateEventW(NULL, TRUE, FALSE, NULL)))
        goto error;
    if (!(events[1] = CreateThread(NULL, 0, deviceloop_thread, events[0], 0, NULL)))
    {
        CloseHandle(events[0]);
        goto error;
    }

    result = WaitForMultipleObjects(2, events, FALSE, INFINITE);
    CloseHandle(events[0]);
    if (result == WAIT_OBJECT_0)
    {
        deviceloop_handle = events[1];
        TRACE("Initialization successful\n");
        return STATUS_SUCCESS;
    }
    CloseHandle(events[1]);

error:
    ERR("Failed to initialize udev device thread\n");
    close(deviceloop_control[0]);
    close(deviceloop_control[1]);
    if (udev_context)
    {
        udev_unref(udev_context);
        udev_context = NULL;
    }
    return STATUS_UNSUCCESSFUL;
}

#else

NTSTATUS udev_driver_init(void)
{
    return STATUS_NOT_IMPLEMENTED;
}

void udev_driver_unload( void )
{
    TRACE("Stub: Unload Driver\n");
}

#endif /* HAVE_UDEV */
