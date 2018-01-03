/*  DirectInput Joystick device from SDL
 *
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
#include "wine/list.h"
#include "windef.h"
#include "winbase.h"
#include "winerror.h"
#include "winreg.h"
#include "dinput.h"

#include "dinput_private.h"
#include "device_private.h"
#include "joystick_private.h"

#ifdef HAVE_SDL2_SDL_H

WINE_DEFAULT_DEBUG_CHANNEL(dinput);

typedef struct _SDLInputEffectImpl {
    IDirectInputEffect IDirectInputEffect_iface;
    LONG ref;

    SDL_Haptic *haptic;
    GUID guid;

    SDL_HapticEffect effect;
    int effect_id;
    BOOL first_axis_is_x;

    struct list *entry;
} SDLInputEffectImpl;

static SDLInputEffectImpl *impl_from_IDirectInputEffect(IDirectInputEffect *iface)
{
    return CONTAINING_RECORD(iface, SDLInputEffectImpl, IDirectInputEffect_iface);
}

static const IDirectInputEffectVtbl EffectVtbl;


static HRESULT WINAPI effect_QueryInterface(IDirectInputEffect *iface,
        const GUID *guid, void **out)
{
    SDLInputEffectImpl *This = impl_from_IDirectInputEffect(iface);

    TRACE("%p %s %p\n", This, debugstr_guid(guid), out);

    if(IsEqualIID(guid, &IID_IUnknown) || IsEqualIID(guid, &IID_IDirectInputEffect)){
        *out = iface;
        IDirectInputEffect_AddRef(iface);
        return DI_OK;
    }

    return E_NOINTERFACE;
}

static ULONG WINAPI effect_AddRef(IDirectInputEffect *iface)
{
    SDLInputEffectImpl *This = impl_from_IDirectInputEffect(iface);
    ULONG ref = InterlockedIncrement(&This->ref);
    TRACE("%p, ref is now: %u\n", This, ref);
    return ref;
}

static ULONG WINAPI effect_Release(IDirectInputEffect *iface)
{
    SDLInputEffectImpl *This = impl_from_IDirectInputEffect(iface);
    ULONG ref = InterlockedDecrement(&This->ref);
    TRACE("%p, ref is now: %u\n", This, ref);

    if (!ref)
    {
        list_remove(This->entry);
        if (This->effect_id >= 0)
            SDL_HapticDestroyEffect(This->haptic, This->effect_id);
        HeapFree(GetProcessHeap(), 0, LIST_ENTRY(This->entry, effect_list_item, entry));
        HeapFree(GetProcessHeap(), 0, This);
    }

    return ref;
}

static HRESULT WINAPI effect_Initialize(IDirectInputEffect *iface, HINSTANCE hinst,
        DWORD version, const GUID *guid)
{
    SDLInputEffectImpl *This = impl_from_IDirectInputEffect(iface);
    TRACE("%p %p 0x%x, %s\n", This, hinst, version, debugstr_guid(guid));
    return DI_OK;
}

static HRESULT WINAPI effect_GetEffectGuid(IDirectInputEffect *iface, GUID *out)
{
    SDLInputEffectImpl *This = impl_from_IDirectInputEffect(iface);
    TRACE("%p %p\n", This, out);
    *out = This->guid;
    return DI_OK;
}


#define GET_BASE_EFFECT_FIELD(target, field, value) {\
    if (target.type == SDL_HAPTIC_SINE || \
        target.type == SDL_HAPTIC_TRIANGLE || \
        target.type == SDL_HAPTIC_SAWTOOTHUP || \
        target.type == SDL_HAPTIC_SAWTOOTHDOWN) \
        value = (target.periodic.field); \
    else if (target.type == SDL_HAPTIC_CONSTANT) \
        value = (target.constant.field); \
    else if (target.type == SDL_HAPTIC_RAMP) \
        value = (target.ramp.field); \
    else if (target.type == SDL_HAPTIC_SPRING || \
             target.type == SDL_HAPTIC_DAMPER || \
             target.type == SDL_HAPTIC_INERTIA || \
             target.type == SDL_HAPTIC_FRICTION) \
        value = (target.condition.field); \
    else if (target.type == SDL_HAPTIC_CUSTOM) \
        value = (target.custom.field); \
    }

#define GET_EXTENDED_EFFECT_FIELD(target, field, value) {\
    if (target.type == SDL_HAPTIC_SINE || \
        target.type == SDL_HAPTIC_TRIANGLE || \
        target.type == SDL_HAPTIC_SAWTOOTHUP || \
        target.type == SDL_HAPTIC_SAWTOOTHDOWN) \
        value = (target.periodic.field); \
    else if (target.type == SDL_HAPTIC_CONSTANT) \
        value = (target.constant.field); \
    else if (target.type == SDL_HAPTIC_RAMP) \
        value = (target.ramp.field); \
    else if (target.type == SDL_HAPTIC_SPRING || \
             target.type == SDL_HAPTIC_DAMPER || \
             target.type == SDL_HAPTIC_INERTIA || \
             target.type == SDL_HAPTIC_FRICTION); \
        /* Ignored because extended fields are not preset in these effects */ \
    else if (target.type == SDL_HAPTIC_CUSTOM) \
        value = (target.custom.field); \
    }

#define SCALE(type, target_range, target_min, value, source_range, source_min) \
    (type)((((target_range)*(value + source_min))/source_range)-target_min)

static HRESULT WINAPI effect_GetParameters(IDirectInputEffect *iface,
        DIEFFECT *effect, DWORD flags)
{
    HRESULT hr = DI_OK;
    SDLInputEffectImpl *This = impl_from_IDirectInputEffect(iface);
    TRACE("%p %p 0x%x\n", This, effect, flags);

    if (flags & DIEP_AXES)
    {
        if (effect->cAxes < 2)
            hr = DIERR_MOREDATA;
        effect->cAxes = 2;
        if (hr)
            return hr;
        else
        {
            effect->rgdwAxes[0] = DIJOFS_X;
            effect->rgdwAxes[1] = DIJOFS_Y;
        }
    }

    if (flags & DIEP_DIRECTION)
    {
        if (effect->cAxes < 2)
            hr = DIERR_MOREDATA;
        effect->cAxes = 2;
        if (hr)
            return hr;
        else
        {
            if (effect->dwFlags & DIEFF_CARTESIAN)
            {
                GET_BASE_EFFECT_FIELD(This->effect, direction.dir[0], effect->rglDirection[0]);
                GET_BASE_EFFECT_FIELD(This->effect, direction.dir[1], effect->rglDirection[1]);
            }
        else
            {
                /* Polar and spherical coordinates */
                GET_BASE_EFFECT_FIELD(This->effect, direction.dir[0], effect->rglDirection[0]);
            }
        }
    }

    if (flags & DIEP_DURATION)
    {
        int sdl_length = 0;
        GET_BASE_EFFECT_FIELD(This->effect, length, sdl_length);
        if (sdl_length == SDL_HAPTIC_INFINITY)
            effect->dwDuration = INFINITE;
        else
            effect->dwDuration = (DWORD)sdl_length * 1000;
    }

    if (flags & DIEP_ENVELOPE)
    {
        int sdl_attack_length = 0;
        int sdl_attack_level = 0;
        int sdl_fade_length = 0;
        int sdl_fade_level = 0;

        GET_EXTENDED_EFFECT_FIELD(This->effect, attack_length, sdl_attack_length);
        GET_EXTENDED_EFFECT_FIELD(This->effect, attack_level, sdl_attack_level);
        GET_EXTENDED_EFFECT_FIELD(This->effect, fade_length, sdl_fade_length);
        GET_EXTENDED_EFFECT_FIELD(This->effect, fade_level, sdl_fade_level);

        if (sdl_attack_length == 0 && sdl_attack_level == 0 && sdl_fade_length == 0 && sdl_fade_level == 0)
        {
            effect->lpEnvelope = NULL;
        }
        else if (effect->lpEnvelope == NULL)
        {
            return DIERR_INVALIDPARAM;
        }
        else
        {
            effect->lpEnvelope->dwAttackLevel = sdl_attack_level;
            effect->lpEnvelope->dwAttackTime = sdl_attack_length * 1000;
            effect->lpEnvelope->dwFadeLevel = sdl_fade_level;
            effect->lpEnvelope->dwFadeTime = sdl_fade_length * 1000;
        }
    }

    if (flags & DIEP_GAIN)
        effect->dwGain = 0;

    if (flags & DIEP_SAMPLEPERIOD)
        effect->dwSamplePeriod = 0;

    if (flags & DIEP_STARTDELAY && effect->dwSize > sizeof(DIEFFECT_DX5))
    {
        GET_BASE_EFFECT_FIELD(This->effect, delay, effect->dwStartDelay);
        effect->dwStartDelay *= 1000;
    }

    if (flags & DIEP_TRIGGERBUTTON)
    {
        int trigger = 0;
        GET_BASE_EFFECT_FIELD(This->effect, button, trigger);
        effect->dwTriggerButton = DIJOFS_BUTTON(trigger);
    }

    if (flags & DIEP_TRIGGERREPEATINTERVAL)
    {
        GET_BASE_EFFECT_FIELD(This->effect, interval, effect->dwTriggerRepeatInterval);
        effect->dwTriggerRepeatInterval *= 1000;
    }

    if (flags & DIEP_TYPESPECIFICPARAMS)
    {
        if (This->effect.type == SDL_HAPTIC_SINE ||
            This->effect.type == SDL_HAPTIC_TRIANGLE ||
            This->effect.type == SDL_HAPTIC_SAWTOOTHUP ||
            This->effect.type == SDL_HAPTIC_SAWTOOTHDOWN)
        {
            DIPERIODIC *tsp = effect->lpvTypeSpecificParams;

            tsp->dwMagnitude = MulDiv(This->effect.periodic.magnitude, 10000, 32767);
            tsp->lOffset = This->effect.periodic.offset;
            tsp->dwPhase = This->effect.periodic.phase;
            tsp->dwPeriod = This->effect.periodic.period * 1000;
        }
        else if (This->effect.type == SDL_HAPTIC_CONSTANT)
        {
            LPDICONSTANTFORCE tsp = effect->lpvTypeSpecificParams;
            tsp->lMagnitude = SCALE(LONG, 20000, -10000, This->effect.constant.level, 0xffff, -32767);
        }
        else if (This->effect.type == SDL_HAPTIC_RAMP)
        {
            DIRAMPFORCE *tsp = effect->lpvTypeSpecificParams;

            tsp->lStart = SCALE(Sint16, 20000, -10000, This->effect.ramp.start, 0xffff, -32767);
            tsp->lEnd = SCALE(Sint16, 20000, -10000, This->effect.ramp.end, 0xffff, -32767);
        }
        else if (This->effect.type == SDL_HAPTIC_SPRING ||
                 This->effect.type == SDL_HAPTIC_DAMPER ||
                 This->effect.type == SDL_HAPTIC_INERTIA ||
                 This->effect.type == SDL_HAPTIC_FRICTION)
        {
            int i;
            DICONDITION *tsp = effect->lpvTypeSpecificParams;
            for (i = 0; i < 2; i++)
            {
                tsp[i].lOffset = SCALE(LONG, 20000, -10000, This->effect.condition.center[i], 0xffff, -32767);
                tsp[i].lPositiveCoefficient = SCALE(LONG, 20000, -10000, This->effect.condition.right_coeff[i], 0xffff, -32767);
                tsp[i].lNegativeCoefficient = SCALE(LONG, 10000, -20000, This->effect.condition.left_coeff[i], 0xffff, -32767);
                tsp[i].dwPositiveSaturation = SCALE(DWORD, 10000, 0, This->effect.condition.right_sat[i], 0xffff, 0);
                tsp[i].dwNegativeSaturation = SCALE(DWORD, 10000, 0, This->effect.condition.left_sat[i], 0xffff, 0);
                tsp[i].lDeadBand = SCALE(LONG, 20000, -10000, This->effect.condition.deadband[i], 0xffff, -32767);
            }
        }
        else if (This->effect.type == SDL_HAPTIC_CUSTOM)
        {
            DICUSTOMFORCE *tsp = effect->lpvTypeSpecificParams;

            tsp->cChannels = This->effect.custom.channels;
            tsp->dwSamplePeriod = This->effect.custom.period * 1000;
            tsp->cSamples = This->effect.custom.samples;
            tsp->rglForceData = (LONG*)This->effect.custom.data;
        }
    }

    return hr;
}

static HRESULT WINAPI effect_Download(IDirectInputEffect *iface)
{
    SDLInputEffectImpl *This = impl_from_IDirectInputEffect(iface);
    TRACE("%p\n", This);

    if (This->effect_id < 0)
    {
        This->effect_id = SDL_HapticNewEffect(This->haptic, &This->effect);
        if(This->effect_id < 0)
        {
            ERR("SDL_HapticNewEffect failed (Effect type %i): %s\n", This->effect.type, SDL_GetError());
            return E_FAIL;
        }
    }

    return DI_OK;
}

static HRESULT WINAPI effect_Start(IDirectInputEffect *iface, DWORD iterations,
        DWORD flags)
{
    SDLInputEffectImpl *This = impl_from_IDirectInputEffect(iface);

    TRACE("%p 0x%x 0x%x\n", This, iterations, flags);

    if (!(flags & DIES_NODOWNLOAD))
    {
        if (This->effect_id == -1)
        {
            HRESULT res = effect_Download(iface);
            if (res != DI_OK)
                return res;
        }
    }

    if (iterations == INFINITE) iterations = SDL_HAPTIC_INFINITY;

    if (SDL_HapticRunEffect(This->haptic, This->effect_id, iterations) < 0)
    {
        ERR("SDL_HapticRunEffect failed: %s\n", SDL_GetError());
        return E_FAIL;
    }
    return DI_OK;
}

#define SET_BASE_EFFECT_FIELD(target, field, value) {\
    if (target.type == SDL_HAPTIC_SINE || \
        target.type == SDL_HAPTIC_TRIANGLE || \
        target.type == SDL_HAPTIC_SAWTOOTHUP || \
        target.type == SDL_HAPTIC_SAWTOOTHDOWN) \
        (target.periodic.field) = value; \
    else if (target.type == SDL_HAPTIC_CONSTANT) \
        (target.constant.field) = value; \
    else if (target.type == SDL_HAPTIC_RAMP) \
        (target.ramp.field) = value; \
    else if (target.type == SDL_HAPTIC_SPRING || \
             target.type == SDL_HAPTIC_DAMPER || \
             target.type == SDL_HAPTIC_INERTIA || \
             target.type == SDL_HAPTIC_FRICTION) \
        (target.condition.field) = value; \
    else if (target.type == SDL_HAPTIC_CUSTOM) \
        (target.custom.field) = value; \
    }

#define SET_EXTENDED_EFFECT_FIELD(target, field, value) {\
    if (target.type == SDL_HAPTIC_SINE || \
        target.type == SDL_HAPTIC_TRIANGLE || \
        target.type == SDL_HAPTIC_SAWTOOTHUP || \
        target.type == SDL_HAPTIC_SAWTOOTHDOWN) \
        (target.periodic.field) = value; \
    else if (target.type == SDL_HAPTIC_CONSTANT) \
        (target.constant.field) = value; \
    else if (target.type == SDL_HAPTIC_RAMP) \
        (target.ramp.field) = value; \
    else if (target.type == SDL_HAPTIC_SPRING || \
             target.type == SDL_HAPTIC_DAMPER || \
             target.type == SDL_HAPTIC_INERTIA || \
             target.type == SDL_HAPTIC_FRICTION); \
        /* Ignored because extended fields are not preset in these effects */ \
    else if (target.type == SDL_HAPTIC_CUSTOM) \
        (target.custom.field) = value; \
    }

static HRESULT WINAPI effect_SetParameters(IDirectInputEffect *iface,
        const DIEFFECT *effect, DWORD flags)
{
    SDLInputEffectImpl *This = impl_from_IDirectInputEffect(iface);
    HRESULT retval = DI_OK;

    TRACE("%p %p 0x%x\n", This, effect, flags);

    dump_DIEFFECT(effect, &This->guid, flags);

    if (IsEqualGUID(&This->guid, &GUID_Sine))
        This->effect.type = SDL_HAPTIC_SINE;
    else if (IsEqualGUID(&This->guid, &GUID_Triangle))
        This->effect.type = SDL_HAPTIC_TRIANGLE;
    else if (IsEqualGUID(&This->guid, &GUID_SawtoothUp))
        This->effect.type = SDL_HAPTIC_SAWTOOTHUP;
    else if (IsEqualGUID(&This->guid, &GUID_SawtoothDown))
        This->effect.type = SDL_HAPTIC_SAWTOOTHDOWN;
    else if (IsEqualGUID(&This->guid, &GUID_ConstantForce))
        This->effect.type = SDL_HAPTIC_CONSTANT;
    else if (IsEqualGUID(&This->guid, &GUID_RampForce))
        This->effect.type = SDL_HAPTIC_RAMP;
    else if (IsEqualGUID(&This->guid, &GUID_Spring))
        This->effect.type = SDL_HAPTIC_SPRING;
    else if (IsEqualGUID(&This->guid, &GUID_Damper))
        This->effect.type = SDL_HAPTIC_DAMPER;
    else if (IsEqualGUID(&This->guid, &GUID_Inertia))
        This->effect.type = SDL_HAPTIC_INERTIA;
    else if (IsEqualGUID(&This->guid, &GUID_Friction))
        This->effect.type = SDL_HAPTIC_FRICTION;
    else if (IsEqualGUID(&This->guid, &GUID_CustomForce))
        This->effect.type = SDL_HAPTIC_CUSTOM;

    if ((flags & ~DIEP_NORESTART & ~DIEP_NODOWNLOAD & ~DIEP_START) == 0)
    {
        /* set everything */
        flags = DIEP_AXES | DIEP_DIRECTION | DIEP_DURATION | DIEP_ENVELOPE |
                DIEP_GAIN | DIEP_SAMPLEPERIOD | DIEP_STARTDELAY | DIEP_TRIGGERBUTTON |
                DIEP_TRIGGERREPEATINTERVAL | DIEP_TYPESPECIFICPARAMS;
    }

    if (flags & DIEP_AXES)
    {
        if (effect->cAxes > 2)
            return DIERR_INVALIDPARAM;
        else if (effect->cAxes < 1)
            return DIERR_INCOMPLETEEFFECT;
        This->first_axis_is_x = effect->rgdwAxes[0] == DIJOFS_X;
    }

    if (flags & DIEP_DIRECTION)
    {
        if (effect->cAxes == 1)
        {
            if (effect->dwFlags & DIEFF_CARTESIAN)
            {
               SET_BASE_EFFECT_FIELD(This->effect, direction.type, SDL_HAPTIC_CARTESIAN);
                if (flags & DIEP_AXES)
                {
                    SET_BASE_EFFECT_FIELD(This->effect, direction.dir[0], effect->rglDirection[0]);
                    SET_BASE_EFFECT_FIELD(This->effect, direction.dir[1], effect->rglDirection[1]);
                }
            } else {
                /* one-axis effects must use cartesian coords */
                return DIERR_INVALIDPARAM;
            }
        }
        /* two axes */
        else
        {
            if (effect->dwFlags & DIEFF_CARTESIAN)
            {
                LONG x, y;

                SET_BASE_EFFECT_FIELD(This->effect, direction.type, SDL_HAPTIC_CARTESIAN);

                if (This->first_axis_is_x)
                {
                    x = effect->rglDirection[0];
                    y = effect->rglDirection[1];
                }
                else
                {
                    x = effect->rglDirection[1];
                    y = effect->rglDirection[0];
                }
                SET_BASE_EFFECT_FIELD(This->effect, direction.dir[0], x);
                SET_BASE_EFFECT_FIELD(This->effect, direction.dir[1], y);
            }
            else
            {
                if (effect->dwFlags & DIEFF_POLAR)
                {
                    SET_BASE_EFFECT_FIELD(This->effect, direction.type, SDL_HAPTIC_POLAR);
                }
                if (effect->dwFlags & DIEFF_SPHERICAL)
                {
                    SET_BASE_EFFECT_FIELD(This->effect, direction.type, SDL_HAPTIC_SPHERICAL);
                }
                SET_BASE_EFFECT_FIELD(This->effect, direction.dir[0], effect->rglDirection[0]);
            }
        }
    }

    if (flags & DIEP_DURATION)
    {
        if (effect->dwDuration == INFINITE)
        {
            SET_BASE_EFFECT_FIELD(This->effect, length, SDL_HAPTIC_INFINITY);
        }
        else if(effect->dwDuration > 1000)
        {
            SET_BASE_EFFECT_FIELD(This->effect, length, effect->dwDuration / 1000);
        }
        else
        {
            SET_BASE_EFFECT_FIELD(This->effect, length, 1);
        }
    }

    if (flags & DIEP_STARTDELAY && effect->dwSize > sizeof(DIEFFECT_DX5))
    {
        SET_BASE_EFFECT_FIELD(This->effect, delay, effect->dwStartDelay / 1000);
    }

    if (flags & DIEP_TRIGGERBUTTON)
    {
        SET_BASE_EFFECT_FIELD(This->effect, button, effect->dwTriggerButton);
    }

    if (flags & DIEP_TRIGGERREPEATINTERVAL)
    {
        SET_BASE_EFFECT_FIELD(This->effect, interval, effect->dwTriggerRepeatInterval / 1000);
    }

    if (flags & DIEP_TYPESPECIFICPARAMS)
    {
        if (IsEqualGUID(&This->guid, &GUID_Sine) ||
            IsEqualGUID(&This->guid, &GUID_Triangle) ||
            IsEqualGUID(&This->guid, &GUID_SawtoothUp) ||
            IsEqualGUID(&This->guid, &GUID_SawtoothDown))
        {
            DIPERIODIC *tsp;
            if (effect->cbTypeSpecificParams != sizeof(DIPERIODIC))
                return DIERR_INVALIDPARAM;
            tsp = effect->lpvTypeSpecificParams;

            This->effect.periodic.magnitude = MulDiv(tsp->dwMagnitude, 32767, 10000);
            This->effect.periodic.offset = tsp->lOffset;
            This->effect.periodic.phase = tsp->dwPhase;
            if (tsp->dwPeriod <= 1000)
                This->effect.periodic.period = 1;
            else
                This->effect.periodic.period = tsp->dwPeriod / 1000;
        }
        else if (IsEqualGUID(&This->guid, &GUID_ConstantForce))
        {
            DICONSTANTFORCE *tsp = effect->lpvTypeSpecificParams;

            if (effect->cbTypeSpecificParams != sizeof(DICONSTANTFORCE))
                return DIERR_INVALIDPARAM;
            tsp = effect->lpvTypeSpecificParams;
            This->effect.constant.level = SCALE(Sint16, 0xffff, -32767, tsp->lMagnitude, 20000, -10000);
        }
        else if (IsEqualGUID(&This->guid, &GUID_RampForce))
        {
            DIRAMPFORCE *tsp = effect->lpvTypeSpecificParams;

            if (effect->cbTypeSpecificParams != sizeof(DIRAMPFORCE))
                return DIERR_INVALIDPARAM;
            tsp = effect->lpvTypeSpecificParams;
            This->effect.ramp.start = SCALE(Sint16, 0xffff, -32767, tsp->lStart, 20000, -10000);
            This->effect.ramp.end = SCALE(Sint16, 0xffff, -32767, tsp->lEnd, 20000, -10000);
        }
        else if (IsEqualGUID(&This->guid, &GUID_Spring) ||
            IsEqualGUID(&This->guid, &GUID_Damper) ||
            IsEqualGUID(&This->guid, &GUID_Inertia) ||
            IsEqualGUID(&This->guid, &GUID_Friction))
        {
            int sources;
            int i,j;
            DICONDITION *tsp = effect->lpvTypeSpecificParams;

            if (effect->cbTypeSpecificParams == sizeof(DICONDITION))
                sources = 1;
            else if (effect->cbTypeSpecificParams == 2 * sizeof(DICONDITION))
                sources = 2;
            else if (effect->cbTypeSpecificParams == 3 * sizeof(DICONDITION))
                sources = 3;
            else
                return DIERR_INVALIDPARAM;

            for (i = j = 0; i < 3; ++i)
            {
                This->effect.condition.right_sat[i] = SCALE(Uint16, 0xffff, 0, tsp[j].dwPositiveSaturation, 10000, 0);
                This->effect.condition.left_sat[i] = SCALE(Uint16, 0xffff, 0, tsp[j].dwNegativeSaturation, 10000, 0);
                This->effect.condition.right_coeff[i] = SCALE(Sint16, 0xffff, -32767, tsp[j].lPositiveCoefficient, 20000, -10000);
                This->effect.condition.left_coeff[i] = SCALE(Sint16, 0xffff, -32767, tsp[j].lNegativeCoefficient, 20000, -10000);
                This->effect.condition.deadband[i] = SCALE(Uint16, 0xffff, 0, tsp[j].lDeadBand, 10000, 0);
                This->effect.condition.center[i] = SCALE(Sint16, 0xffff, -32767, tsp[j].lOffset, 20000, -10000);
               if (sources-1 > j)
                j++;
            }
        }
        else if (IsEqualGUID(&This->guid, &GUID_CustomForce))
        {
            DICUSTOMFORCE *tsp = effect->lpvTypeSpecificParams;

            if (effect->cbTypeSpecificParams != sizeof(DICUSTOMFORCE))
                return DIERR_INVALIDPARAM;

            This->effect.custom.channels = tsp->cChannels;
            This->effect.custom.period = tsp->dwSamplePeriod / 1000;
            This->effect.custom.samples = tsp->cSamples;
            This->effect.custom.data = (Uint16*)tsp->rglForceData;
        }
        else
        {
            FIXME("Specific effect params for type %s no implemented.\n", debugstr_guid(&This->guid));
        }
    }

    if (flags & DIEP_ENVELOPE)
    {
        if (effect->lpEnvelope)
        {
            SET_EXTENDED_EFFECT_FIELD(This->effect, attack_length, effect->lpEnvelope->dwAttackTime / 1000);
            SET_EXTENDED_EFFECT_FIELD(This->effect, attack_level, effect->lpEnvelope->dwAttackLevel);
            SET_EXTENDED_EFFECT_FIELD(This->effect, fade_length, effect->lpEnvelope->dwFadeTime / 1000);
            SET_EXTENDED_EFFECT_FIELD(This->effect, fade_level, effect->lpEnvelope->dwFadeLevel);
        }
        else
        {
            SET_EXTENDED_EFFECT_FIELD(This->effect, attack_length, 0);
            SET_EXTENDED_EFFECT_FIELD(This->effect, attack_level, 0);
            SET_EXTENDED_EFFECT_FIELD(This->effect, fade_length, 0);
            SET_EXTENDED_EFFECT_FIELD(This->effect, fade_level, 0);
        }
    }

    if (flags & DIEP_GAIN)
        TRACE("Effect gain requested but no effect gain functionality present.\n");

    if (flags & DIEP_SAMPLEPERIOD)
        TRACE("Sample period requested but no sample period functionality present.\n");

    if (This->effect_id >= 0)
    {
        if (SDL_HapticUpdateEffect(This->haptic, This->effect_id, &This->effect) < 0)
        {
            ERR("SDL_HapticUpdateEffect failed: %s\n",SDL_GetError());
            return E_FAIL;
        }

    }

    if (!(flags & DIEP_NODOWNLOAD))
        retval = effect_Download(iface);
    if (retval != DI_OK)
        return DI_DOWNLOADSKIPPED;

    if (flags & DIEP_START)
        retval = effect_Start(iface, 1, 0);

    return DI_OK;
}

static HRESULT WINAPI effect_Stop(IDirectInputEffect *iface)
{
    SDLInputEffectImpl *This = impl_from_IDirectInputEffect(iface);
    TRACE("%p\n", This);
    if (SDL_HapticStopEffect(This->haptic, This->effect_id) < 0)
    {
        ERR("SDL_HapticStopEffect failed: %s\n", SDL_GetError());
        return E_FAIL;
    }
    return DI_OK;
}

static HRESULT WINAPI effect_GetEffectStatus(IDirectInputEffect *iface, DWORD *flags)
{
    int rc;
    SDLInputEffectImpl *This = impl_from_IDirectInputEffect(iface);
    TRACE("%p %p %p %i\n", This, flags, This->haptic, This->effect_id);

    if (!flags)
        return E_POINTER;

    if (This->effect_id == -1)
        return DIERR_NOTDOWNLOADED;

    rc = SDL_HapticGetEffectStatus(This->haptic, This->effect_id);
    switch (rc)
    {
        case 0: *flags = 0; break;
        case 1: *flags = DIEGES_PLAYING; break;
        default:
            ERR("SDL_HapticGetEffectStatus failed: %s\n", SDL_GetError());
    }
    return DI_OK;
}

static HRESULT WINAPI effect_Unload(IDirectInputEffect *iface)
{
    SDLInputEffectImpl *This = impl_from_IDirectInputEffect(iface);
    TRACE("%p\n", This);
    if (This->effect_id >= 0)
        SDL_HapticDestroyEffect(This->haptic, This->effect_id);
    This->effect_id = -1;
    return DI_OK;
}

static HRESULT WINAPI effect_Escape(IDirectInputEffect *iface, DIEFFESCAPE *escape)
{
    SDLInputEffectImpl *This = impl_from_IDirectInputEffect(iface);
    TRACE("%p %p\n", This, escape);
    return E_NOTIMPL;
}

static const IDirectInputEffectVtbl EffectVtbl = {
    effect_QueryInterface,
    effect_AddRef,
    effect_Release,
    effect_Initialize,
    effect_GetEffectGuid,
    effect_GetParameters,
    effect_SetParameters,
    effect_Start,
    effect_Stop,
    effect_GetEffectStatus,
    effect_Download,
    effect_Unload,
    effect_Escape
};

/******************************************************************************
 *      SDLInputEffect
 */

DECLSPEC_HIDDEN HRESULT sdl_create_effect(SDL_Haptic *device, REFGUID rguid, struct list *parent_list_entry, LPDIRECTINPUTEFFECT* peff)
{
    SDLInputEffectImpl *effect;

    effect = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(SDLInputEffectImpl));

    effect->IDirectInputEffect_iface.lpVtbl = &EffectVtbl;
    effect->ref = 1;
    effect->guid = *rguid;
    effect->haptic = device;
    effect->effect_id = -1;

    effect->entry = parent_list_entry;
    *peff = &effect->IDirectInputEffect_iface;

    return DI_OK;
}

DECLSPEC_HIDDEN HRESULT sdl_input_get_info_A(
        SDL_Joystick *dev,
        REFGUID rguid,
        LPDIEFFECTINFOA info)
{
    DWORD type = typeFromGUID(rguid);

    TRACE("(%p, %s, %p) type=%d\n", dev, _dump_dinput_GUID(rguid), info, type);

    if (!info) return E_POINTER;

    if (info->dwSize != sizeof(DIEFFECTINFOA)) return DIERR_INVALIDPARAM;

    info->guid = *rguid;

    info->dwEffType = type;
    /* the event device API does not support querying for all these things
     * therefore we assume that we have support for them
     * that's not as dangerous as it sounds, since drivers are allowed to
     * ignore parameters they claim to support anyway */
    info->dwEffType |= DIEFT_DEADBAND | DIEFT_FFATTACK | DIEFT_FFFADE
                    | DIEFT_POSNEGCOEFFICIENTS | DIEFT_POSNEGSATURATION
                    | DIEFT_SATURATION | DIEFT_STARTDELAY;

    /* again, assume we have support for everything */
    info->dwStaticParams = DIEP_ALLPARAMS;
    info->dwDynamicParams = info->dwStaticParams;

    /* yes, this is windows behavior (print the GUID_Name for name) */
    strcpy(info->tszName, _dump_dinput_GUID(rguid));

    return DI_OK;
}

DECLSPEC_HIDDEN HRESULT sdl_input_get_info_W(
        SDL_Joystick *dev,
        REFGUID rguid,
        LPDIEFFECTINFOW info)
{
    DWORD type = typeFromGUID(rguid);

    TRACE("(%p, %s, %p) type=%d\n", dev, _dump_dinput_GUID(rguid), info, type);

    if (!info) return E_POINTER;

    if (info->dwSize != sizeof(DIEFFECTINFOW)) return DIERR_INVALIDPARAM;

    info->guid = *rguid;

    info->dwEffType = type;
    /* the event device API does not support querying for all these things
     * therefore we assume that we have support for them
     * that's not as dangerous as it sounds, since drivers are allowed to
     * ignore parameters they claim to support anyway */
    info->dwEffType |= DIEFT_DEADBAND | DIEFT_FFATTACK | DIEFT_FFFADE
                    | DIEFT_POSNEGCOEFFICIENTS | DIEFT_POSNEGSATURATION
                    | DIEFT_SATURATION | DIEFT_STARTDELAY;

    /* again, assume we have support for everything */
    info->dwStaticParams = DIEP_ALLPARAMS;
    info->dwDynamicParams = info->dwStaticParams;

    /* yes, this is windows behavior (print the GUID_Name for name) */
    MultiByteToWideChar(CP_ACP, 0, _dump_dinput_GUID(rguid), -1,
                        info->tszName, MAX_PATH);

    return DI_OK;
}

#endif
