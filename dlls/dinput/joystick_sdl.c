/*  DirectInput Joystick device from SDL
 *
 * Copyright 1998,2000 Marcus Meissner
 * Copyright 1998,1999 Lionel Ulmer
 * Copyright 2000-2001 TransGaming Technologies Inc.
 * Copyright 2005 Daniel Remenak
 * Copyright 2017 CodeWeavers, Aric Stewart
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

#include "config.h"
#include "wine/port.h"

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef HAVE_SDL2_SDL_H
# include <SDL2/SDL.h>
#endif
#include <errno.h>

#include "wine/debug.h"
#include "wine/unicode.h"
#include "wine/library.h"
#include "wine/list.h"
#include "windef.h"
#include "winbase.h"
#include "winerror.h"
#include "winreg.h"
#include "devguid.h"
#include "dinput.h"

#include "dinput_private.h"
#include "device_private.h"
#include "joystick_private.h"

#ifdef HAVE_SDL2_SDL_H

WINE_DEFAULT_DEBUG_CHANNEL(dinput);

typedef struct JoystickImpl JoystickImpl;
static const IDirectInputDevice8AVtbl JoystickAvt;
static const IDirectInputDevice8WVtbl JoystickWvt;

/* implemented in effect_sdl.c */
HRESULT sdl_create_effect(SDL_Haptic *haptic, REFGUID rguid, struct list *parent_list_entry, LPDIRECTINPUTEFFECT* peff);
HRESULT sdl_input_get_info_A(SDL_Joystick *dev, REFGUID rguid, LPDIEFFECTINFOA info);
HRESULT sdl_input_get_info_W(SDL_Joystick *dev, REFGUID rguid, LPDIEFFECTINFOW info);

#define ITEM_TYPE_BUTTON 1
#define ITEM_TYPE_AXIS 2
#define ITEM_TYPE_HAT 3

struct device_state_item {
    int type;
    int idx;
    int val;
};

typedef BOOL (*enum_device_state_function)(JoystickImpl*, struct device_state_item *, int);

struct SDLDev {
    int id;
    WORD vendor_id;
    WORD product_id;
    CHAR *name;
    BOOL is_xbox_gamepad;

    int n_buttons, n_axes, n_hats;

    BOOL has_ff, is_joystick;
    int autocenter;
    int gain;
    struct list effects;
};

struct JoystickImpl
{
    struct JoystickGenericImpl generic;
    struct SDLDev              *sdldev;

    SDL_Joystick *device;
    SDL_Haptic *haptic;
    BOOL ff_paused;

    enum_device_state_function enum_device_state;
};

static inline JoystickImpl *impl_from_IDirectInputDevice8A(IDirectInputDevice8A *iface)
{
    return CONTAINING_RECORD(CONTAINING_RECORD(CONTAINING_RECORD(iface, IDirectInputDeviceImpl, IDirectInputDevice8A_iface),
           JoystickGenericImpl, base), JoystickImpl, generic);
}
static inline JoystickImpl *impl_from_IDirectInputDevice8W(IDirectInputDevice8W *iface)
{
    return CONTAINING_RECORD(CONTAINING_RECORD(CONTAINING_RECORD(iface, IDirectInputDeviceImpl, IDirectInputDevice8W_iface),
           JoystickGenericImpl, base), JoystickImpl, generic);
}

static inline IDirectInputDevice8W *IDirectInputDevice8W_from_impl(JoystickImpl *This)
{
    return &This->generic.base.IDirectInputDevice8W_iface;
}

static const GUID DInput_Wine_SDL_Joystick_GUID = { /* 001E36B7-5DBA-4C4F-A8C9-CFC8689DB403 */
  0x001E36B7, 0x5DBA, 0x4C4F, {0xA8, 0xC9, 0xCF, 0xC8, 0x68, 0x9D, 0xB4, 0x03}
};

static const GUID DInput_PIDVID_Product_GUID = { /* PIDVID-0000-0000-0000-504944564944 "PIDVID" */
  0x00000000, 0x0000, 0x0000, {0x00, 0x00, 0x50, 0x49, 0x44, 0x56, 0x49, 0x44}
};

static int have_sdldevs = -1;
static struct SDLDev *sdldevs = NULL;

static void find_sdldevs(void)
{
    int i;
    Uint16 (*pSDL_JoystickGetProduct)(SDL_Joystick * joystick) = NULL;
    Uint16 (*pSDL_JoystickGetVendor)(SDL_Joystick * joystick) = NULL;
    void *sdl_handle = NULL;

    if (InterlockedCompareExchange(&have_sdldevs, 0, -1) != -1)
        /* Someone beat us to it */
        return;

    SDL_Init(SDL_INIT_JOYSTICK|SDL_INIT_HAPTIC);
    SDL_JoystickEventState(SDL_ENABLE);

    sdl_handle = wine_dlopen(SONAME_LIBSDL2, RTLD_NOW, NULL, 0);
    if (sdl_handle) {
        pSDL_JoystickGetProduct = wine_dlsym(sdl_handle, "SDL_JoystickGetProduct", NULL, 0);
        pSDL_JoystickGetVendor = wine_dlsym(sdl_handle, "SDL_JoystickGetVendor", NULL, 0);
    }

    if(!pSDL_JoystickGetVendor){
        ERR("SDL installation is old! Please upgrade to >=2.0.6 to get accurate joystick information.\n");
    }

    for (i = 0; i < SDL_NumJoysticks(); i++)
    {
        struct SDLDev sdldev = {0};
        struct SDLDev *new_sdldevs;
        SDL_Joystick *device;
        const CHAR* name;

        sdldev.id = i;
        device = SDL_JoystickOpen(i);

        name = SDL_JoystickName(device);
        sdldev.name = HeapAlloc(GetProcessHeap(), 0, strlen(name) + 1);
        strcpy(sdldev.name, name);

        if (device_disabled_registry(sdldev.name)) {
            SDL_JoystickClose(device);
            continue;
        }

        TRACE("Found a joystick (%i) on %p: %s\n", have_sdldevs, device, sdldev.name);

        if (SDL_JoystickIsHaptic(device))
        {
            SDL_Haptic *haptic = SDL_HapticOpenFromJoystick(device);
            if (haptic)
            {
                TRACE(" ... with force feedback\n");
                sdldev.has_ff = TRUE;
                SDL_HapticClose(haptic);
            }
        }

        if(pSDL_JoystickGetVendor){
            sdldev.vendor_id = pSDL_JoystickGetVendor(device);
            sdldev.product_id = pSDL_JoystickGetProduct(device);
        }else{
            sdldev.vendor_id = 0x01;
            sdldev.product_id = SDL_JoystickInstanceID(device) + 1;
        }

        {
            SDL_JoystickType type = SDL_JoystickGetType(device);
            sdldev.is_joystick =
                type == SDL_JOYSTICK_TYPE_WHEEL ||
                type == SDL_JOYSTICK_TYPE_FLIGHT_STICK ||
                type == SDL_JOYSTICK_TYPE_THROTTLE;

            sdldev.is_xbox_gamepad = is_xinput_device(NULL, sdldev.vendor_id, sdldev.product_id);
        }

        sdldev.n_buttons = SDL_JoystickNumButtons(device);
        sdldev.n_axes = SDL_JoystickNumAxes(device);
        sdldev.n_hats = SDL_JoystickNumHats(device);

        if (!have_sdldevs)
            new_sdldevs = HeapAlloc(GetProcessHeap(), 0, sizeof(struct SDLDev));
        else
            new_sdldevs = HeapReAlloc(GetProcessHeap(), 0, sdldevs, (1 + have_sdldevs) * sizeof(struct SDLDev));

        SDL_JoystickClose(device);
        if (!new_sdldevs)
        {
            continue;
        }
        sdldevs = new_sdldevs;
        sdldevs[have_sdldevs] = sdldev;
        have_sdldevs++;
    }
}

static void fill_joystick_dideviceinstanceA(LPDIDEVICEINSTANCEA lpddi, DWORD version, int id)
{
    DWORD dwSize = lpddi->dwSize;

    TRACE("%d %p\n", dwSize, lpddi);
    memset(lpddi, 0, dwSize);

    lpddi->dwSize       = dwSize;
    lpddi->guidInstance = DInput_Wine_SDL_Joystick_GUID;
    lpddi->guidInstance.Data3 = id;
    lpddi->guidProduct = DInput_PIDVID_Product_GUID;
    lpddi->guidProduct.Data1 = MAKELONG(sdldevs[id].vendor_id, sdldevs[id].product_id);
    lpddi->guidFFDriver = GUID_NULL;

    lpddi->dwDevType = get_device_type(version, sdldevs[id].is_joystick);

    /* Assume the joystick as HID if it is attached to USB bus and has a valid VID/PID */
    if ( sdldevs[id].vendor_id && sdldevs[id].product_id)
    {
        lpddi->dwDevType |= DIDEVTYPE_HID;
        lpddi->wUsagePage = 0x01; /* Desktop */
        if (sdldevs[id].is_joystick)
            lpddi->wUsage = 0x04; /* Joystick */
        else
            lpddi->wUsage = 0x05; /* Game Pad */
    }

    strcpy(lpddi->tszInstanceName, sdldevs[id].name);
    strcpy(lpddi->tszProductName,  sdldevs[id].name);
}

static void fill_joystick_dideviceinstanceW(LPDIDEVICEINSTANCEW lpddi, DWORD version, int id)
{
    DIDEVICEINSTANCEA lpddiA;
    DWORD dwSize = lpddi->dwSize;

    lpddiA.dwSize = sizeof(lpddiA);
    fill_joystick_dideviceinstanceA(&lpddiA, version, id);

    TRACE("%d %p\n", dwSize, lpddi);
    memset(lpddi, 0, dwSize);

    /* Convert A->W */
    lpddi->dwSize = dwSize;
    lpddi->guidInstance = lpddiA.guidInstance;
    lpddi->guidProduct = lpddiA.guidProduct;
    lpddi->dwDevType = lpddiA.dwDevType;
    MultiByteToWideChar(CP_ACP, 0, lpddiA.tszInstanceName, -1, lpddi->tszInstanceName, MAX_PATH);
    MultiByteToWideChar(CP_ACP, 0, lpddiA.tszProductName, -1, lpddi->tszProductName, MAX_PATH);
    lpddi->guidFFDriver = lpddiA.guidFFDriver;
    lpddi->wUsagePage = lpddiA.wUsagePage;
    lpddi->wUsage = lpddiA.wUsage;
}

static HRESULT sdl_enum_deviceA(DWORD dwDevType, DWORD dwFlags, LPDIDEVICEINSTANCEA lpddi, DWORD version, int id)
{
  find_sdldevs();

  if (id >= have_sdldevs) {
    return E_FAIL;
  }

  if (!((dwDevType == 0) ||
        ((dwDevType == DIDEVTYPE_JOYSTICK) && (version >= 0x0300 && version < 0x0800)) ||
        (((dwDevType == DI8DEVCLASS_GAMECTRL) || (dwDevType == DI8DEVTYPE_JOYSTICK)) && (version >= 0x0800))))
    return S_FALSE;

  if (!(dwFlags & DIEDFL_FORCEFEEDBACK) || sdldevs[id].has_ff) {
    fill_joystick_dideviceinstanceA(lpddi, version, id);
    return S_OK;
  }
  return S_FALSE;
}

static HRESULT sdl_enum_deviceW(DWORD dwDevType, DWORD dwFlags, LPDIDEVICEINSTANCEW lpddi, DWORD version, int id)
{
  find_sdldevs();

  if (id >= have_sdldevs) {
    return E_FAIL;
  }

  if (!((dwDevType == 0) ||
        ((dwDevType == DIDEVTYPE_JOYSTICK) && (version >= 0x0300 && version < 0x0800)) ||
        (((dwDevType == DI8DEVCLASS_GAMECTRL) || (dwDevType == DI8DEVTYPE_JOYSTICK)) && (version >= 0x0800))))
    return S_FALSE;

  if (!(dwFlags & DIEDFL_FORCEFEEDBACK) || sdldevs[id].has_ff) {
    fill_joystick_dideviceinstanceW(lpddi, version, id);
    return S_OK;
  }
  return S_FALSE;
}

static int buttons_to_sdl_hat(int u, int r, int d, int l)
{
    if(u == d)
    {
        if(l == r)
            return SDL_HAT_CENTERED;
        if(l)
            return SDL_HAT_LEFT;
        return SDL_HAT_RIGHT;
    }
    if(u)
    {
        if(l == r)
            return SDL_HAT_UP;
        if(l)
            return SDL_HAT_LEFTUP;
        return SDL_HAT_RIGHTUP;
    }
    if(l == r)
        return SDL_HAT_DOWN;
    if(l)
        return SDL_HAT_LEFTDOWN;
    return SDL_HAT_RIGHTDOWN;
}

/* playstation controllers */
#define VID_SONY 0x054c
#define PID_SONY_DUALSHOCK_4 0x05c4
#define PID_SONY_DUALSHOCK_4_2 0x09cc
#define PID_SONY_DUALSHOCK_4_DONGLE 0x0ba0

static BOOL enum_device_state_ds4_16button(JoystickImpl *This, struct device_state_item *st, int idx)
{
    static const int button_map_ds4_16button[] = {
        /* [linux button] -> windows button */

        /* [0] -> */ 1, /* cross */
        /* [1] -> */ 2, /* circle */
        /* [2] -> */ 0, /* square */
        /* [3] -> */ 3, /* triangle */

        /* [4] -> */ 8, /* share */
        /* [5] -> */ 12, /* guide */
        /* [6] -> */ 9, /* options */

        /* [7] -> */ 10, /* L3 */
        /* [8] -> */ 11, /* R3 */

        /* [9] -> */ 4, /* L1 */
        /* [10] -> */ 5, /* R1 */
    };

    static const int axis_map_ds4_16button[] = {
        /* [linux axis] -> windows axis */

        /* [0] -> */ 0, /* left horiz */
        /* [1] -> */ 1, /* left vert */
        /* [2] -> */ 2, /* right horiz */
        /* [3] -> */ 5, /* right vert */
        /* [4] -> */ 3, /* L2 */
        /* [5] -> */ 4, /* R2 */
    };

    static const int DNP_TOUCHPAD_BUTTON = 13;
    static const int SDL_TOUCHPAD_BUTTON = 15;

    static const int DNP_L2_BUTTON = 6;
    static const int SDL_L2_AXIS = 4;

    static const int DNP_R2_BUTTON = 7;
    static const int SDL_R2_AXIS = 5;

    static const int SDL_DPAD_UP_BUTTON = 11;
    static const int SDL_DPAD_DOWN_BUTTON = 12;
    static const int SDL_DPAD_LEFT_BUTTON = 13;
    static const int SDL_DPAD_RIGHT_BUTTON = 14;

    if(idx < 11)
    {
        /* first 11 buttons */
        st->type = ITEM_TYPE_BUTTON;
        st->idx = button_map_ds4_16button[idx];
        st->val = SDL_JoystickGetButton(This->device, idx);
        return TRUE;
    }

    if(idx < 17)
    {
        /* six axes */
        idx -= 11;
        st->type = ITEM_TYPE_AXIS;
        st->idx = axis_map_ds4_16button[idx];
        st->val = SDL_JoystickGetAxis(This->device, idx);
        return TRUE;
    }

    switch(idx)
    {
    case 17:
        /* touchpad button */
        st->type = ITEM_TYPE_BUTTON;
        st->idx = DNP_TOUCHPAD_BUTTON;
        st->val = SDL_JoystickGetButton(This->device, SDL_TOUCHPAD_BUTTON);
        return TRUE;

    case 18:
        /* L2 button */
        st->type = ITEM_TYPE_BUTTON;
        st->idx = DNP_L2_BUTTON;
        /* turn button on at about 1/8 of the trigger travel */
        st->val = SDL_JoystickGetAxis(This->device, SDL_L2_AXIS) > 3 * SDL_JOYSTICK_AXIS_MIN / 4;
        return TRUE;

    case 19:
        /* R2 button */
        st->type = ITEM_TYPE_BUTTON;
        st->idx = DNP_R2_BUTTON;
        /* turn button on at about 1/8 of the trigger travel */
        st->val = SDL_JoystickGetAxis(This->device, SDL_R2_AXIS) > 3 * SDL_JOYSTICK_AXIS_MIN / 4;
        return TRUE;

    case 20:
        /* dpad buttons --> hatswitch */
        st->type = ITEM_TYPE_HAT;
        st->idx = 0;
        st->val = buttons_to_sdl_hat(
                SDL_JoystickGetButton(This->device, SDL_DPAD_UP_BUTTON),
                SDL_JoystickGetButton(This->device, SDL_DPAD_RIGHT_BUTTON),
                SDL_JoystickGetButton(This->device, SDL_DPAD_DOWN_BUTTON),
                SDL_JoystickGetButton(This->device, SDL_DPAD_LEFT_BUTTON));
        return TRUE;
    }

    return FALSE;
}

static BOOL enum_device_state_ds4_13button(JoystickImpl *This, struct device_state_item *st, int idx)
{
    static const int button_map_ds4_13button[] = {
        /* [linux button] -> windows button */

        /* [0] -> */ 1, /* cross */
        /* [1] -> */ 2, /* circle */
        /* [2] -> */ 3, /* triangle */
        /* [3] -> */ 0, /* square */

        /* [4] -> */ 4, /* L1 */
        /* [5] -> */ 5, /* R1 */
        /* [6] -> */ 6, /* L2 */
        /* [7] -> */ 7, /* R2 */
        /* [8] -> */ 8, /* share */
        /* [9] -> */ 9, /* options */

        /* [10] -> */ 12, /* guide */
        /* [11] -> */ 10, /* L3 */
        /* [12] -> */ 11, /* R3 */

        /* ps4 controller through linux event API does not support touchpad button */
    };

    static const int axis_map_ds4_13button[] = {
        /* [linux axis] -> windows axis */

        /* [0] -> */ 0, /* left horiz */
        /* [1] -> */ 1, /* left vert */
        /* [2] -> */ 3, /* L2 */
        /* [3] -> */ 2, /* right horiz */
        /* [4] -> */ 5, /* right vert */
        /* [5] -> */ 4, /* R2 */
    };

    if(idx < This->sdldev->n_buttons)
    {
        st->type = ITEM_TYPE_BUTTON;
        st->idx = button_map_ds4_13button[idx];
        st->val = SDL_JoystickGetButton(This->device, idx);
        return TRUE;
    }

    idx -= This->sdldev->n_buttons;

    if(idx < This->sdldev->n_axes)
    {
        st->type = ITEM_TYPE_AXIS;
        st->idx = axis_map_ds4_13button[idx];
        st->val = SDL_JoystickGetAxis(This->device, idx);
        return TRUE;
    }

    idx -= This->sdldev->n_axes;

    if(idx < This->sdldev->n_hats)
    {
        st->type = ITEM_TYPE_HAT;
        st->idx = idx;
        st->val = SDL_JoystickGetHat(This->device, idx);
        return TRUE;
    }

    return FALSE;
}

/* straight 1:1 mapping of SDL items and dinput items */
static BOOL enum_device_state_standard(JoystickImpl *This, struct device_state_item *st, int idx)
{
    if(idx < This->sdldev->n_buttons)
    {
        st->type = ITEM_TYPE_BUTTON;
        st->idx = idx;
        st->val = SDL_JoystickGetButton(This->device, idx);
        return TRUE;
    }

    idx -= This->sdldev->n_buttons;

    if(idx < This->sdldev->n_axes)
    {
        st->type = ITEM_TYPE_AXIS;
        st->idx = idx;
        st->val = SDL_JoystickGetAxis(This->device, idx);
        return TRUE;
    }

    idx -= This->sdldev->n_axes;

    if(idx < This->sdldev->n_hats)
    {
        st->type = ITEM_TYPE_HAT;
        st->idx = idx;
        st->val = SDL_JoystickGetHat(This->device, idx);
        return TRUE;
    }

    return FALSE;
}

static void poll_sdl_device_state(LPDIRECTINPUTDEVICE8A iface)
{
    JoystickImpl *This = impl_from_IDirectInputDevice8A(iface);
    int i = 0;
    int inst_id = 0;
    int newVal = 0;
    struct device_state_item item;

    SDL_JoystickUpdate();

    while(This->enum_device_state(This, &item, i++))
    {
        switch(item.type){
        case ITEM_TYPE_BUTTON:
        {
            int val = item.val;
            int oldVal = This->generic.js.rgbButtons[item.idx];
            newVal = val ? 0x80 : 0x0;
            This->generic.js.rgbButtons[item.idx] = newVal;
            if (oldVal != newVal)
            {
                TRACE("Button: %i val %d oldVal %d newVal %d\n",  item.idx, val, oldVal, newVal);
                inst_id = DIDFT_MAKEINSTANCE(item.idx) | DIDFT_PSHBUTTON;
                queue_event(iface, inst_id, newVal, GetCurrentTime(), This->generic.base.dinput->evsequence++);
            }
            break;
        }

        case ITEM_TYPE_AXIS:
        {
            int oldVal;
            newVal = item.val;
            newVal = joystick_map_axis(&This->generic.props[item.idx], newVal);
            switch (item.idx)
            {
                case 0: oldVal = This->generic.js.lX;
                        This->generic.js.lX  = newVal; break;
                case 1: oldVal = This->generic.js.lY;
                        This->generic.js.lY  = newVal; break;
                case 2: oldVal = This->generic.js.lZ;
                        This->generic.js.lZ  = newVal; break;
                case 3: oldVal = This->generic.js.lRx;
                        This->generic.js.lRx = newVal; break;
                case 4: oldVal = This->generic.js.lRy;
                        This->generic.js.lRy = newVal; break;
                case 5: oldVal = This->generic.js.lRz;
                        This->generic.js.lRz = newVal; break;
                case 6: oldVal = This->generic.js.rglSlider[0];
                        This->generic.js.rglSlider[0] = newVal; break;
                case 7: oldVal = This->generic.js.rglSlider[1];
                        This->generic.js.rglSlider[1] = newVal; break;
            }
            if (oldVal != newVal)
            {
                TRACE("Axis: %i oldVal %d newVal %d\n",  item.idx, oldVal, newVal);
                inst_id = DIDFT_MAKEINSTANCE(item.idx) | DIDFT_ABSAXIS;
                queue_event(iface, inst_id, newVal, GetCurrentTime(), This->generic.base.dinput->evsequence++);
            }
            break;
        }

        case ITEM_TYPE_HAT:
        {
            int oldVal = This->generic.js.rgdwPOV[item.idx];
            newVal = item.val;
            switch (newVal)
            {
                case SDL_HAT_CENTERED: newVal = -1; break;
                case SDL_HAT_UP: newVal = 0; break;
                case SDL_HAT_RIGHTUP:newVal = 4500; break;
                case SDL_HAT_RIGHT: newVal = 9000; break;
                case SDL_HAT_RIGHTDOWN: newVal = 13500; break;
                case SDL_HAT_DOWN: newVal = 18000; break;
                case SDL_HAT_LEFTDOWN: newVal = 22500; break;
                case SDL_HAT_LEFT: newVal = 27000; break;
                case SDL_HAT_LEFTUP: newVal = 31500; break;
            }
            if (oldVal != newVal)
            {
                TRACE("Hat : %i oldVal %d newVal %d\n",  item.idx, oldVal, newVal);
                This->generic.js.rgdwPOV[item.idx] = newVal;
                inst_id = DIDFT_MAKEINSTANCE(item.idx) | DIDFT_POV;
                queue_event(iface, inst_id, newVal, GetCurrentTime(), This->generic.base.dinput->evsequence++);
            }
            break;
        }
        }
    }
}

static enum_device_state_function select_enum_function(struct SDLDev *sdldev)
{
    switch(sdldev->vendor_id){
    case VID_SONY:
        switch(sdldev->product_id){
        case PID_SONY_DUALSHOCK_4:
        case PID_SONY_DUALSHOCK_4_2:
        case PID_SONY_DUALSHOCK_4_DONGLE:
            TRACE("for %04x/%04x, polling ds4 controller\n", sdldev->vendor_id, sdldev->product_id);
            if(sdldev->n_buttons >= 16)
                return enum_device_state_ds4_16button;

            TRACE("SDL only reports %u buttons for this PS4 controller. Please upgrade SDL to > 2.0.10 and/or give your user hidraw access.\n",
                    sdldev->n_buttons);
            return enum_device_state_ds4_13button;
        }
        break;
    }

    TRACE("for %04x/%04x, using no maps\n", sdldev->vendor_id, sdldev->product_id);
    return enum_device_state_standard;
}

static JoystickImpl *alloc_device(REFGUID rguid, IDirectInputImpl *dinput, unsigned short index)
{
    JoystickImpl* newDevice;
    LPDIDATAFORMAT df = NULL;
    DIDEVICEINSTANCEW ddi;
    int i,idx = 0, axis_count = 0, button_count = 0, hat_count = 0;
    struct device_state_item item;

    newDevice = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(JoystickImpl));
    if (!newDevice) return NULL;

    newDevice->generic.guidInstance = DInput_Wine_SDL_Joystick_GUID;
    newDevice->generic.guidInstance.Data3 = index;
    newDevice->generic.guidProduct = DInput_PIDVID_Product_GUID;
    newDevice->generic.guidProduct.Data1 = MAKELONG(sdldevs[index].vendor_id, sdldevs[index].product_id);
    newDevice->generic.joy_polldev = poll_sdl_device_state;
    newDevice->enum_device_state = select_enum_function(&sdldevs[index]);

    newDevice->generic.base.IDirectInputDevice8A_iface.lpVtbl = &JoystickAvt;
    newDevice->generic.base.IDirectInputDevice8W_iface.lpVtbl = &JoystickWvt;
    newDevice->generic.base.ref    = 1;
    newDevice->generic.base.guid   = *rguid;
    newDevice->generic.base.dinput = dinput;
    newDevice->sdldev              = &sdldevs[index];
    newDevice->generic.name        = (char*)newDevice->sdldev->name;
    list_init(&newDevice->sdldev->effects);
    newDevice->sdldev->autocenter = 1;
    newDevice->sdldev->gain = 100;

    InitializeCriticalSection(&newDevice->generic.base.crit);
    newDevice->generic.base.crit.DebugInfo->Spare[0] = (DWORD_PTR)(__FILE__ ": JoystickImpl*->base.crit");

    /* Open Device */
    newDevice->device = SDL_JoystickOpen(newDevice->sdldev->id);
    newDevice->haptic = SDL_HapticOpenFromJoystick(newDevice->device);

    i = 0;
    while(newDevice->enum_device_state(newDevice, &item, i++)){
        switch(item.type){
            case ITEM_TYPE_BUTTON:
                ++button_count;
                break;
            case ITEM_TYPE_AXIS:
                ++axis_count;
                break;
            case ITEM_TYPE_HAT:
                ++hat_count;
                break;
        }
    }

    newDevice->generic.devcaps.dwAxes = axis_count;
    if (newDevice->generic.devcaps.dwAxes > 8 )
    {
        WARN("Can't support %d axis. Clamping down to 8\n", newDevice->generic.devcaps.dwAxes);
        newDevice->generic.devcaps.dwAxes = 8;
    }

    for (i = 0; i < newDevice->generic.devcaps.dwAxes; i++)
    {
        newDevice->generic.props[i].lDevMin = -32768;
        newDevice->generic.props[i].lDevMax = 32767;
        newDevice->generic.props[i].lMin =  0;
        newDevice->generic.props[i].lMax =  0xffff;
        newDevice->generic.props[i].lDeadZone = 0;
        newDevice->generic.props[i].lSaturation = 0;
    }

    newDevice->generic.devcaps.dwPOVs = hat_count;
    if (newDevice->generic.devcaps.dwPOVs > 4)
    {
        WARN("Can't support %d POV. Clamping down to 4\n", newDevice->generic.devcaps.dwPOVs);
        newDevice->generic.devcaps.dwPOVs = 4;
    }

    newDevice->generic.devcaps.dwButtons = button_count;
    if (newDevice->generic.devcaps.dwButtons > 128)
    {
        WARN("Can't support %d buttons. Clamping down to 128\n", newDevice->generic.devcaps.dwButtons);
        newDevice->generic.devcaps.dwButtons = 128;
    }

    TRACE("axes %u povs %u buttons %u\n", newDevice->generic.devcaps.dwAxes, newDevice->generic.devcaps.dwPOVs, newDevice->generic.devcaps.dwButtons);

    /* Create copy of default data format */
    if (!(df = HeapAlloc(GetProcessHeap(), 0, c_dfDIJoystick2.dwSize))) goto failed;
    memcpy(df, &c_dfDIJoystick2, c_dfDIJoystick2.dwSize);

    df->dwNumObjs = newDevice->generic.devcaps.dwAxes + newDevice->generic.devcaps.dwPOVs + newDevice->generic.devcaps.dwButtons;
    if (!(df->rgodf = HeapAlloc(GetProcessHeap(), 0, df->dwNumObjs * df->dwObjSize))) goto failed;

    for (i = 0; i < newDevice->generic.devcaps.dwAxes; i++)
    {
        memcpy(&df->rgodf[idx], &c_dfDIJoystick2.rgodf[idx], df->dwObjSize);
        df->rgodf[idx].dwType = DIDFT_MAKEINSTANCE(idx) | DIDFT_ABSAXIS;
        if (newDevice->sdldev->has_ff && i < 2)
             df->rgodf[idx].dwFlags |= DIDOI_FFACTUATOR;
        ++idx;
    }

    for (i = 0; i < newDevice->generic.devcaps.dwPOVs; i++)
    {
        memcpy(&df->rgodf[idx], &c_dfDIJoystick2.rgodf[i + 8], df->dwObjSize);
        df->rgodf[idx++].dwType = DIDFT_MAKEINSTANCE(i) | DIDFT_POV;
    }

    for (i = 0; i < newDevice->generic.devcaps.dwButtons; i++)
    {
        memcpy(&df->rgodf[idx], &c_dfDIJoystick2.rgodf[i + 12], df->dwObjSize);
        df->rgodf[idx].pguid = &GUID_Button;
        df->rgodf[idx++].dwType = DIDFT_MAKEINSTANCE(i) | DIDFT_PSHBUTTON;
    }

    if (newDevice->sdldev->has_ff)
        newDevice->generic.devcaps.dwFlags |= DIDC_FORCEFEEDBACK;

    newDevice->generic.base.data_format.wine_df = df;

    /* Fill the caps */
    newDevice->generic.devcaps.dwSize = sizeof(newDevice->generic.devcaps);
    newDevice->generic.devcaps.dwFlags = DIDC_ATTACHED;

    ddi.dwSize = sizeof(ddi);
    fill_joystick_dideviceinstanceW(&ddi, newDevice->generic.base.dinput->dwVersion, index);
    newDevice->generic.devcaps.dwDevType = ddi.dwDevType;

    if (newDevice->sdldev->has_ff)
        newDevice->generic.devcaps.dwFlags |= DIDC_FORCEFEEDBACK;

    IDirectInput_AddRef(&newDevice->generic.base.dinput->IDirectInput7A_iface);

    EnterCriticalSection(&dinput->crit);
    list_add_tail(&dinput->devices_list, &newDevice->generic.base.entry);
    LeaveCriticalSection(&dinput->crit);

    return newDevice;

failed:
    if (df) HeapFree(GetProcessHeap(), 0, df->rgodf);
    HeapFree(GetProcessHeap(), 0, df);
    HeapFree(GetProcessHeap(), 0, newDevice);
    return NULL;
}

/******************************************************************************
  *     get_joystick_index : Get the joystick index from a given GUID
  */
static unsigned short get_joystick_index(REFGUID guid)
{
    GUID wine_joystick = DInput_Wine_SDL_Joystick_GUID;
    GUID dev_guid = *guid;

    wine_joystick.Data3 = 0;
    dev_guid.Data3 = 0;

    /* for the standard joystick GUID use index 0 */
    if(IsEqualGUID(&GUID_Joystick,guid)) return 0;

    /* for the wine joystick GUIDs use the index stored in Data3 */
    if(IsEqualGUID(&wine_joystick, &dev_guid)) return guid->Data3;

    return 0xffff;
}

static HRESULT sdl_create_device(IDirectInputImpl *dinput, REFGUID rguid, REFIID riid, LPVOID *pdev, int unicode)
{
    unsigned short index;

    TRACE("%p %s %s %p %i\n", dinput, debugstr_guid(rguid), debugstr_guid(riid), pdev, unicode);

    find_sdldevs();
    *pdev = NULL;

    if ((index = get_joystick_index(rguid)) < 0xffff &&
        have_sdldevs && index < have_sdldevs)
    {
        JoystickImpl *This;

        if (riid == NULL)
            ;/* nothing */
        else if (IsEqualGUID(&IID_IDirectInputDeviceA,  riid) ||
                 IsEqualGUID(&IID_IDirectInputDevice2A, riid) ||
                 IsEqualGUID(&IID_IDirectInputDevice7A, riid) ||
                 IsEqualGUID(&IID_IDirectInputDevice8A, riid))
        {
            unicode = 0;
        }
        else if (IsEqualGUID(&IID_IDirectInputDeviceW,  riid) ||
                 IsEqualGUID(&IID_IDirectInputDevice2W, riid) ||
                 IsEqualGUID(&IID_IDirectInputDevice7W, riid) ||
                 IsEqualGUID(&IID_IDirectInputDevice8W, riid))
        {
            unicode = 1;
        }
        else
        {
            WARN("no interface\n");
            return DIERR_NOINTERFACE;
        }

        This = alloc_device(rguid, dinput, index);
        TRACE("Created a Joystick device (%p)\n", This);

        if (!This) return DIERR_OUTOFMEMORY;

        if (unicode)
            *pdev = &This->generic.base.IDirectInputDevice8W_iface;
        else
            *pdev = &This->generic.base.IDirectInputDevice8A_iface;

        return DI_OK;
    }

    return DIERR_DEVICENOTREG;
}

const struct dinput_device joystick_sdl_device = {
  "Wine SDL joystick driver",
  sdl_enum_deviceA,
  sdl_enum_deviceW,
  sdl_create_device
};

static ULONG WINAPI JoystickWImpl_Release(LPDIRECTINPUTDEVICE8W iface)
{
    JoystickImpl *This = impl_from_IDirectInputDevice8W(iface);
    TRACE("(this=%p)\n", iface);
    if (This->generic.base.ref == 1 && This->device >= 0)
    {
        TRACE("Closing Joystick: %p\n",This);
        if (This->sdldev->has_ff)
            SDL_HapticClose(This->haptic);
        SDL_JoystickClose(This->device);
        This->device = NULL;
    }
    return IDirectInputDevice2WImpl_Release(iface);
}

static ULONG WINAPI JoystickAImpl_Release(LPDIRECTINPUTDEVICE8A iface)
{
    JoystickImpl *This = impl_from_IDirectInputDevice8A(iface);
    return JoystickWImpl_Release(IDirectInputDevice8W_from_impl(This));
}

/******************************************************************************
  *     GetProperty : get input device properties
  */
static HRESULT WINAPI JoystickWImpl_GetProperty(LPDIRECTINPUTDEVICE8W iface, REFGUID rguid, LPDIPROPHEADER pdiph)
{
    JoystickImpl *This = impl_from_IDirectInputDevice8W(iface);

    TRACE("(this=%p,%s,%p)\n", iface, debugstr_guid(rguid), pdiph);
    _dump_DIPROPHEADER(pdiph);

    if (!IS_DIPROP(rguid)) return DI_OK;

    switch (LOWORD(rguid)) {
        case (DWORD_PTR) DIPROP_AUTOCENTER:
        {
            LPDIPROPDWORD pd = (LPDIPROPDWORD)pdiph;

            pd->dwData = This->sdldev->autocenter ? DIPROPAUTOCENTER_ON : DIPROPAUTOCENTER_OFF;
            TRACE("autocenter(%d)\n", pd->dwData);
            break;
        }
        case (DWORD_PTR) DIPROP_FFGAIN:
        {
            LPDIPROPDWORD pd = (LPDIPROPDWORD)pdiph;

            pd->dwData = This->sdldev->gain;
            TRACE("DIPROP_FFGAIN(%d)\n", pd->dwData);
            break;
        }
        case (DWORD_PTR) DIPROP_VIDPID:
        {
            LPDIPROPDWORD pd = (LPDIPROPDWORD)pdiph;

            if (!This->sdldev->product_id || !This->sdldev->vendor_id)
                return DIERR_UNSUPPORTED;
            pd->dwData = MAKELONG(This->sdldev->vendor_id, This->sdldev->product_id);
            TRACE("DIPROP_VIDPID(%08x)\n", pd->dwData);
            break;
        }
        case (DWORD_PTR) DIPROP_JOYSTICKID:
        {
            LPDIPROPDWORD pd = (LPDIPROPDWORD)pdiph;

            pd->dwData = This->sdldev->id;
            TRACE("DIPROP_JOYSTICKID(%d)\n", pd->dwData);
            break;
        }

        case (DWORD_PTR) DIPROP_GUIDANDPATH:
        {
            static const WCHAR formatW[] = {'\\','\\','?','\\','h','i','d','#','v','i','d','_','%','0','4','x','&',
                                            'p','i','d','_','%','0','4','x','&','%','s','_','%','i',0};
            static const WCHAR miW[] = {'m','i',0};
            static const WCHAR igW[] = {'i','g',0};

            LPDIPROPGUIDANDPATH pd = (LPDIPROPGUIDANDPATH)pdiph;

            if (!This->sdldev->product_id || !This->sdldev->vendor_id)
                return DIERR_UNSUPPORTED;

            pd->guidClass = GUID_DEVCLASS_HIDCLASS;
            sprintfW(pd->wszPath, formatW, This->sdldev->vendor_id, This->sdldev->product_id,
                     This->sdldev->is_xbox_gamepad ? igW : miW, This->sdldev->id);

            TRACE("DIPROP_GUIDANDPATH(%s, %s): returning fake path\n", debugstr_guid(&pd->guidClass), debugstr_w(pd->wszPath));
            break;
        }

    default:
        return JoystickWGenericImpl_GetProperty(iface, rguid, pdiph);
    }

    return DI_OK;
}

static HRESULT WINAPI JoystickAImpl_GetProperty(LPDIRECTINPUTDEVICE8A iface, REFGUID rguid, LPDIPROPHEADER pdiph)
{
    JoystickImpl *This = impl_from_IDirectInputDevice8A(iface);
    return JoystickWImpl_GetProperty(IDirectInputDevice8W_from_impl(This), rguid, pdiph);
}

static BOOL _SetProperty(JoystickImpl *This, const GUID *prop, const DIPROPHEADER *header)
{
    int rc;

    switch(LOWORD(prop))
    {
        case (DWORD_PTR)DIPROP_AUTOCENTER:
        {
            LPCDIPROPDWORD pd = (LPCDIPROPDWORD)header;

            This->sdldev->autocenter = pd->dwData == DIPROPAUTOCENTER_ON;

            rc = SDL_HapticSetAutocenter(This->haptic, This->sdldev->autocenter * 100);
            if (rc != 0)
                ERR("SDL_HapticSetAutocenter failed: %s\n", SDL_GetError());
            break;
        }
        case (DWORD_PTR)DIPROP_FFGAIN:
        {
            LPCDIPROPDWORD pd = (LPCDIPROPDWORD)header;
            int sdl_gain = MulDiv(This->sdldev->gain, 100, 10000);

            TRACE("DIPROP_FFGAIN(%d)\n", pd->dwData);

            This->sdldev->gain = pd->dwData;

            rc = SDL_HapticSetGain(This->haptic, sdl_gain);
            if (rc != 0)
                ERR("SDL_HapticSetGain (%i -> %i) failed: %s\n", pd->dwData, sdl_gain, SDL_GetError());
            break;
        }
        default:
            return FALSE;
    }

    return TRUE;
}

static HRESULT WINAPI JoystickWImpl_SetProperty(IDirectInputDevice8W *iface,
        const GUID *prop, const DIPROPHEADER *header)
{
    JoystickImpl *This = impl_from_IDirectInputDevice8W(iface);

    TRACE("%p %s %p\n", This, debugstr_guid(prop), header);

    if (_SetProperty(This, prop, header))
        return DI_OK;
    else
        return JoystickWGenericImpl_SetProperty(iface, prop, header);
}

static HRESULT WINAPI JoystickAImpl_SetProperty(IDirectInputDevice8A *iface,
        const GUID *prop, const DIPROPHEADER *header)
{
    JoystickImpl *This = impl_from_IDirectInputDevice8A(iface);

    TRACE("%p %s %p\n", This, debugstr_guid(prop), header);

    if (_SetProperty(This, prop, header))
        return DI_OK;
    else
        return JoystickAGenericImpl_SetProperty(iface, prop, header);
}

/******************************************************************************
  *     GetDeviceInfo : get information about a device's identity
  */
static HRESULT WINAPI JoystickAImpl_GetDeviceInfo(LPDIRECTINPUTDEVICE8A iface,
                                                  LPDIDEVICEINSTANCEA pdidi)
{
    JoystickImpl *This = impl_from_IDirectInputDevice8A(iface);

    TRACE("(%p) %p\n", This, pdidi);

    if (pdidi == NULL) return E_POINTER;
    if ((pdidi->dwSize != sizeof(DIDEVICEINSTANCE_DX3A)) &&
        (pdidi->dwSize != sizeof(DIDEVICEINSTANCEA)))
        return DIERR_INVALIDPARAM;

    fill_joystick_dideviceinstanceA(pdidi, This->generic.base.dinput->dwVersion,
                                    get_joystick_index(&This->generic.base.guid));
    return DI_OK;
}

static HRESULT WINAPI JoystickWImpl_GetDeviceInfo(LPDIRECTINPUTDEVICE8W iface,
                                                  LPDIDEVICEINSTANCEW pdidi)
{
    JoystickImpl *This = impl_from_IDirectInputDevice8W(iface);

    TRACE("(%p) %p\n", This, pdidi);

    if (pdidi == NULL) return E_POINTER;
    if ((pdidi->dwSize != sizeof(DIDEVICEINSTANCE_DX3W)) &&
        (pdidi->dwSize != sizeof(DIDEVICEINSTANCEW)))
        return DIERR_INVALIDPARAM;

    fill_joystick_dideviceinstanceW(pdidi, This->generic.base.dinput->dwVersion,
                                    get_joystick_index(&This->generic.base.guid));
    return DI_OK;
}

static HRESULT WINAPI JoystickWImpl_CreateEffect(IDirectInputDevice8W *iface,
        const GUID *rguid, const DIEFFECT *lpeff, IDirectInputEffect **ppdef,
        IUnknown *pUnkOuter)
{
    JoystickImpl *This = impl_from_IDirectInputDevice8W(iface);
    HRESULT retval = DI_OK;
    effect_list_item* new_effect = NULL;

    TRACE("%p %s %p %p %p\n", iface, debugstr_guid(rguid), lpeff, ppdef, pUnkOuter);
    if (lpeff) dump_DIEFFECT(lpeff, rguid, 0);

    if(!This->sdldev->has_ff){
        TRACE("No force feedback support\n");
        *ppdef = NULL;
        return DIERR_UNSUPPORTED;
    }

    if (pUnkOuter)
        WARN("aggregation not implemented\n");

    if (This->ff_paused)
    {
        FIXME("Cannot add new effects to a paused SDL device\n");
        return DIERR_GENERIC;
    }

    if (!(new_effect = HeapAlloc(GetProcessHeap(), 0, sizeof(*new_effect))))
    return DIERR_OUTOFMEMORY;

    retval = sdl_create_effect(This->haptic, rguid, &new_effect->entry, &new_effect->ref);
    if (retval != DI_OK)
    {
        HeapFree(GetProcessHeap(), 0, new_effect);
        return retval;
    }

    if (lpeff != NULL)
    {
        retval = IDirectInputEffect_SetParameters(new_effect->ref, lpeff, 0);

        if (retval != DI_OK && retval != DI_DOWNLOADSKIPPED)
        {
            HeapFree(GetProcessHeap(), 0, new_effect);
            return retval;
        }
    }

    list_add_tail(&This->sdldev->effects, &new_effect->entry);
    *ppdef = new_effect->ref;

    TRACE("allocated effect: %p\n", new_effect);

    return DI_OK;
}

static HRESULT WINAPI JoystickAImpl_CreateEffect(IDirectInputDevice8A *iface,
        const GUID *type, const DIEFFECT *params, IDirectInputEffect **out,
        IUnknown *outer)
{
    JoystickImpl *This = impl_from_IDirectInputDevice8A(iface);

    TRACE("%p %s %p %p %p\n", iface, debugstr_guid(type), params, out, outer);

    return JoystickWImpl_CreateEffect(&This->generic.base.IDirectInputDevice8W_iface,
            type, params, out, outer);
}

static HRESULT WINAPI JoystickWImpl_EnumEffects(LPDIRECTINPUTDEVICE8W iface,
                                                LPDIENUMEFFECTSCALLBACKW lpCallback,
                                                LPVOID pvRef,
                                                DWORD dwEffType)
{
    DIEFFECTINFOW dei;
    DWORD type = DIEFT_GETTYPE(dwEffType);
    JoystickImpl* This = impl_from_IDirectInputDevice8W(iface);
    unsigned int query;

    TRACE("(this=%p,%p,%d) type=%d\n", This, pvRef, dwEffType, type);

    dei.dwSize = sizeof(DIEFFECTINFOW);
    query = SDL_HapticQuery(This->haptic);
    TRACE("Effects 0x%x\n",query);

    if ((type == DIEFT_ALL || type == DIEFT_CONSTANTFORCE)
        && (query & SDL_HAPTIC_CONSTANT))
    {
        IDirectInputDevice8_GetEffectInfo(iface, &dei, &GUID_ConstantForce);
        (*lpCallback)(&dei, pvRef);
    }

    if ((type == DIEFT_ALL || type == DIEFT_RAMPFORCE) &&
        (query & SDL_HAPTIC_RAMP))
    {
        IDirectInputDevice8_GetEffectInfo(iface, &dei, &GUID_RampForce);
        (*lpCallback)(&dei, pvRef);
    }

    if (type == DIEFT_ALL || type == DIEFT_PERIODIC)
    {
        if (query & SDL_HAPTIC_SINE)
        {
            IDirectInputDevice8_GetEffectInfo(iface, &dei, &GUID_Sine);
            (*lpCallback)(&dei, pvRef);
        }
        if (query & SDL_HAPTIC_TRIANGLE)
        {
            IDirectInputDevice8_GetEffectInfo(iface, &dei, &GUID_Triangle);
            (*lpCallback)(&dei, pvRef);
        }
        if (query & SDL_HAPTIC_SAWTOOTHUP)
        {
            IDirectInputDevice8_GetEffectInfo(iface, &dei, &GUID_SawtoothUp);
            (*lpCallback)(&dei, pvRef);
        }
        if (query & SDL_HAPTIC_SAWTOOTHDOWN)
        {
            IDirectInputDevice8_GetEffectInfo(iface, &dei, &GUID_SawtoothDown);
            (*lpCallback)(&dei, pvRef);
        }
    }

    if (type == DIEFT_ALL || type == DIEFT_CONDITION)
    {
        if (query & SDL_HAPTIC_SPRING)
        {
            IDirectInputDevice8_GetEffectInfo(iface, &dei, &GUID_Spring);
            (*lpCallback)(&dei, pvRef);
        }
        if (query & SDL_HAPTIC_DAMPER)
        {
            IDirectInputDevice8_GetEffectInfo(iface, &dei, &GUID_Damper);
            (*lpCallback)(&dei, pvRef);
        }
        if (query & SDL_HAPTIC_INERTIA)
        {
            IDirectInputDevice8_GetEffectInfo(iface, &dei, &GUID_Inertia);
            (*lpCallback)(&dei, pvRef);
        }
        if (query & SDL_HAPTIC_FRICTION)
        {
            IDirectInputDevice8_GetEffectInfo(iface, &dei, &GUID_Friction);
            (*lpCallback)(&dei, pvRef);
        }
    }

    return DI_OK;
}

static HRESULT WINAPI JoystickAImpl_EnumEffects(LPDIRECTINPUTDEVICE8A iface,
                                                LPDIENUMEFFECTSCALLBACKA lpCallback,
                                                LPVOID pvRef,
                                                DWORD dwEffType)
{
    DIEFFECTINFOA dei;
    DWORD type = DIEFT_GETTYPE(dwEffType);
    JoystickImpl* This = impl_from_IDirectInputDevice8A(iface);
    unsigned int query;

    TRACE("(this=%p,%p,%d) type=%d\n", This, pvRef, dwEffType, type);

    dei.dwSize = sizeof(DIEFFECTINFOA);
    query = SDL_HapticQuery(This->haptic);
    TRACE("Effects 0x%x\n",query);

    if ((type == DIEFT_ALL || type == DIEFT_CONSTANTFORCE)
        && (query & SDL_HAPTIC_CONSTANT))
    {
        IDirectInputDevice8_GetEffectInfo(iface, &dei, &GUID_ConstantForce);
        (*lpCallback)(&dei, pvRef);
    }

    if ((type == DIEFT_ALL || type == DIEFT_RAMPFORCE) &&
        (query & SDL_HAPTIC_RAMP))
    {
        IDirectInputDevice8_GetEffectInfo(iface, &dei, &GUID_RampForce);
        (*lpCallback)(&dei, pvRef);
    }

    if (type == DIEFT_ALL || type == DIEFT_PERIODIC)
    {
        if (query & SDL_HAPTIC_SINE)
        {
            IDirectInputDevice8_GetEffectInfo(iface, &dei, &GUID_Sine);
            (*lpCallback)(&dei, pvRef);
        }
        if (query & SDL_HAPTIC_TRIANGLE)
        {
            IDirectInputDevice8_GetEffectInfo(iface, &dei, &GUID_Triangle);
            (*lpCallback)(&dei, pvRef);
        }
        if (query & SDL_HAPTIC_SAWTOOTHUP)
        {
            IDirectInputDevice8_GetEffectInfo(iface, &dei, &GUID_SawtoothUp);
            (*lpCallback)(&dei, pvRef);
        }
        if (query & SDL_HAPTIC_SAWTOOTHDOWN)
        {
            IDirectInputDevice8_GetEffectInfo(iface, &dei, &GUID_SawtoothDown);
            (*lpCallback)(&dei, pvRef);
        }
    }

    if (type == DIEFT_ALL || type == DIEFT_CONDITION)
    {
        if (query & SDL_HAPTIC_SPRING)
        {
            IDirectInputDevice8_GetEffectInfo(iface, &dei, &GUID_Spring);
            (*lpCallback)(&dei, pvRef);
        }
        if (query & SDL_HAPTIC_DAMPER)
        {
            IDirectInputDevice8_GetEffectInfo(iface, &dei, &GUID_Damper);
            (*lpCallback)(&dei, pvRef);
        }
        if (query & SDL_HAPTIC_INERTIA)
        {
            IDirectInputDevice8_GetEffectInfo(iface, &dei, &GUID_Inertia);
            (*lpCallback)(&dei, pvRef);
        }
        if (query & SDL_HAPTIC_FRICTION)
        {
            IDirectInputDevice8_GetEffectInfo(iface, &dei, &GUID_Friction);
            (*lpCallback)(&dei, pvRef);
        }
    }

    return DI_OK;
}

static HRESULT WINAPI JoystickWImpl_GetEffectInfo(LPDIRECTINPUTDEVICE8W iface,
                                                  LPDIEFFECTINFOW pdei,
                                                  REFGUID guid)
{
    JoystickImpl* This = impl_from_IDirectInputDevice8W(iface);
    TRACE("(this=%p,%p,%s)\n", This, pdei, _dump_dinput_GUID(guid));
    return sdl_input_get_info_W(This->device, guid, pdei);
}

static HRESULT WINAPI JoystickAImpl_GetEffectInfo(LPDIRECTINPUTDEVICE8A iface,
                          LPDIEFFECTINFOA pdei,
                          REFGUID guid)
{
    JoystickImpl* This = impl_from_IDirectInputDevice8A(iface);
    TRACE("(this=%p,%p,%s)\n", This, pdei, _dump_dinput_GUID(guid));
    return sdl_input_get_info_A(This->device, guid, pdei);
}

static HRESULT WINAPI JoystickWImpl_SendForceFeedbackCommand(LPDIRECTINPUTDEVICE8W iface, DWORD dwFlags)
{
    JoystickImpl* This = impl_from_IDirectInputDevice8W(iface);
    TRACE("(this=%p,%d)\n", This, dwFlags);

    switch (dwFlags)
    {
    case DISFFC_STOPALL:
    {
        effect_list_item *itr;

        /* Stop all effects */
        LIST_FOR_EACH_ENTRY(itr, &This->sdldev->effects, effect_list_item, entry)
            IDirectInputEffect_Stop(itr->ref);
        break;
    }

    case DISFFC_RESET:
    {
        effect_list_item *itr;

        /* Stop and unload all effects. It is not true that effects are released */
        LIST_FOR_EACH_ENTRY(itr, &This->sdldev->effects, effect_list_item, entry)
        {
            IDirectInputEffect_Stop(itr->ref);
            IDirectInputEffect_Unload(itr->ref);
        }
        break;
    }
    case DISFFC_PAUSE:
        This->ff_paused = TRUE;
        if (SDL_HapticPause(This->haptic) != 0)
            ERR("SDL_HapticPause failed: %s\n",SDL_GetError());
        break;
    case DISFFC_CONTINUE:
        This->ff_paused = FALSE;
        if (SDL_HapticUnpause(This->haptic) != 0)
            ERR("SDL_HapticUnpause failed: %s\n",SDL_GetError());
        break;

    case DISFFC_SETACTUATORSOFF:
    case DISFFC_SETACTUATORSON:
        FIXME("No direct actuator control in sdl\n");
        break;

    default:
        WARN("Unknown Force Feedback Command %u!\n", dwFlags);
        return DIERR_INVALIDPARAM;
    }
    return DI_OK;
}

static HRESULT WINAPI JoystickAImpl_SendForceFeedbackCommand(LPDIRECTINPUTDEVICE8A iface, DWORD dwFlags)
{
    JoystickImpl *This = impl_from_IDirectInputDevice8A(iface);
    return JoystickWImpl_SendForceFeedbackCommand(IDirectInputDevice8W_from_impl(This), dwFlags);
}

static HRESULT WINAPI JoystickWImpl_EnumCreatedEffectObjects(LPDIRECTINPUTDEVICE8W iface,
                                                             LPDIENUMCREATEDEFFECTOBJECTSCALLBACK lpCallback,
                                                             LPVOID pvRef, DWORD dwFlags)
{
    JoystickImpl* This = impl_from_IDirectInputDevice8W(iface);
    effect_list_item *itr, *ptr;

    TRACE("(this=%p,%p,%p,%d)\n", This, lpCallback, pvRef, dwFlags);

    if (!lpCallback)
        return DIERR_INVALIDPARAM;

    if (dwFlags != 0)
        FIXME("Flags specified, but no flags exist yet (DX9)!\n");

    LIST_FOR_EACH_ENTRY_SAFE(itr, ptr, &This->sdldev->effects, effect_list_item, entry)
        (*lpCallback)(itr->ref, pvRef);

    return DI_OK;
}

static HRESULT WINAPI JoystickAImpl_EnumCreatedEffectObjects(LPDIRECTINPUTDEVICE8A iface,
                                                             LPDIENUMCREATEDEFFECTOBJECTSCALLBACK lpCallback,
                                                             LPVOID pvRef, DWORD dwFlags)
{
    JoystickImpl *This = impl_from_IDirectInputDevice8A(iface);
    return JoystickWImpl_EnumCreatedEffectObjects(IDirectInputDevice8W_from_impl(This), lpCallback, pvRef, dwFlags);
}

static const IDirectInputDevice8AVtbl JoystickAvt =
{
    IDirectInputDevice2AImpl_QueryInterface,
    IDirectInputDevice2AImpl_AddRef,
    JoystickAImpl_Release,
    JoystickAGenericImpl_GetCapabilities,
    IDirectInputDevice2AImpl_EnumObjects,
    JoystickAImpl_GetProperty,
    JoystickAImpl_SetProperty,
    IDirectInputDevice2AImpl_Acquire,
    IDirectInputDevice2AImpl_Unacquire,
    JoystickAGenericImpl_GetDeviceState,
    IDirectInputDevice2AImpl_GetDeviceData,
    IDirectInputDevice2AImpl_SetDataFormat,
    IDirectInputDevice2AImpl_SetEventNotification,
    IDirectInputDevice2AImpl_SetCooperativeLevel,
    JoystickAGenericImpl_GetObjectInfo,
    JoystickAImpl_GetDeviceInfo,
    IDirectInputDevice2AImpl_RunControlPanel,
    IDirectInputDevice2AImpl_Initialize,
    JoystickAImpl_CreateEffect,
    JoystickAImpl_EnumEffects,
    JoystickAImpl_GetEffectInfo,
    IDirectInputDevice2AImpl_GetForceFeedbackState,
    JoystickAImpl_SendForceFeedbackCommand,
    JoystickAImpl_EnumCreatedEffectObjects,
    IDirectInputDevice2AImpl_Escape,
    JoystickAGenericImpl_Poll,
    IDirectInputDevice2AImpl_SendDeviceData,
    IDirectInputDevice7AImpl_EnumEffectsInFile,
    IDirectInputDevice7AImpl_WriteEffectToFile,
    JoystickAGenericImpl_BuildActionMap,
    JoystickAGenericImpl_SetActionMap,
    IDirectInputDevice8AImpl_GetImageInfo
};

static const IDirectInputDevice8WVtbl JoystickWvt =
{
    IDirectInputDevice2WImpl_QueryInterface,
    IDirectInputDevice2WImpl_AddRef,
    JoystickWImpl_Release,
    JoystickWGenericImpl_GetCapabilities,
    IDirectInputDevice2WImpl_EnumObjects,
    JoystickWImpl_GetProperty,
    JoystickWImpl_SetProperty,
    IDirectInputDevice2WImpl_Acquire,
    IDirectInputDevice2WImpl_Unacquire,
    JoystickWGenericImpl_GetDeviceState,
    IDirectInputDevice2WImpl_GetDeviceData,
    IDirectInputDevice2WImpl_SetDataFormat,
    IDirectInputDevice2WImpl_SetEventNotification,
    IDirectInputDevice2WImpl_SetCooperativeLevel,
    JoystickWGenericImpl_GetObjectInfo,
    JoystickWImpl_GetDeviceInfo,
    IDirectInputDevice2WImpl_RunControlPanel,
    IDirectInputDevice2WImpl_Initialize,
    JoystickWImpl_CreateEffect,
    JoystickWImpl_EnumEffects,
    JoystickWImpl_GetEffectInfo,
    IDirectInputDevice2WImpl_GetForceFeedbackState,
    JoystickWImpl_SendForceFeedbackCommand,
    JoystickWImpl_EnumCreatedEffectObjects,
    IDirectInputDevice2WImpl_Escape,
    JoystickWGenericImpl_Poll,
    IDirectInputDevice2WImpl_SendDeviceData,
    IDirectInputDevice7WImpl_EnumEffectsInFile,
    IDirectInputDevice7WImpl_WriteEffectToFile,
    JoystickWGenericImpl_BuildActionMap,
    JoystickWGenericImpl_SetActionMap,
    IDirectInputDevice8WImpl_GetImageInfo
};

#else

const struct dinput_device joystick_sdl_device = {
  "Wine SDL joystick driver",
  NULL,
  NULL,
  NULL
};

#endif
