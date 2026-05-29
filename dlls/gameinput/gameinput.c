/*
 * Copyright 2024 Rémi Bernon for CodeWeavers
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

#include <stddef.h>
#include <stdarg.h>

#define COBJMACROS
#include "windef.h"
#include "winbase.h"
#include "winreg.h"
#include "devpropdef.h"
#include "devfiltertypes.h"
#include "devquery.h"
#include "hidusage.h"
#include "ddk/hidsdi.h"

#include "initguid.h"
#include "gameinput.h"
#include "devpkey.h"
#include "ddk/hidclass.h"

#include "wine/list.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(ginput);

DEFINE_GUID( GUID_DEVINTERFACE_WINEXINPUT,0x6c53d5fd,0x6480,0x440f,0xb6,0x18,0x47,0x67,0x50,0xc5,0xe1,0xa6 );

static CRITICAL_SECTION game_input_cs;
static CRITICAL_SECTION_DEBUG game_input_cs_debug =
{
    0, 0, &game_input_cs,
    { &game_input_cs_debug.ProcessLocksList, &game_input_cs_debug.ProcessLocksList },
      0, 0, { (DWORD_PTR)(__FILE__ ": game_input_cs") }
};
static CRITICAL_SECTION game_input_cs = { &game_input_cs_debug, -1, 0, 0, 0, 0 };
static struct game_input *game_input;

struct device
{
    IGameInputDevice_v0 IGameInputDevice_v0_iface;
    LONG refcount;
    WCHAR path[MAX_PATH];
    struct list entry;

    GameInputDeviceStatus status;
    GameInputDeviceInfo_v0 info_v0;
};

static struct device *device_from_IGameInputDevice_v0( IGameInputDevice_v0 *iface )
{
    return CONTAINING_RECORD( iface, struct device, IGameInputDevice_v0_iface );
}

static HRESULT WINAPI game_input_device_v0_QueryInterface( IGameInputDevice_v0 *iface, REFIID iid, void **out )
{
    struct device *device = device_from_IGameInputDevice_v0( iface );

    TRACE( "device %p, iid %s, out %p.\n", device, debugstr_guid( iid ), out );

    if (IsEqualGUID( iid, &IID_IGameInputDevice_v0 ) ||
        IsEqualGUID( iid, &IID_IUnknown ))
    {
        IGameInputDevice_v0_AddRef( &device->IGameInputDevice_v0_iface );
        *out = &device->IGameInputDevice_v0_iface;
        return S_OK;
    }

    *out = NULL;
    FIXME( "%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid( iid ) );
    return E_NOINTERFACE;
}

static ULONG WINAPI game_input_device_v0_AddRef( IGameInputDevice_v0 *iface )
{
    struct device *device = device_from_IGameInputDevice_v0( iface );
    ULONG ref = InterlockedIncrement( &device->refcount );
    TRACE( "device %p increasing refcount to %lu.\n", device, ref );
    return ref;
}

static ULONG WINAPI game_input_device_v0_Release( IGameInputDevice_v0 *iface )
{
    struct device *device = device_from_IGameInputDevice_v0( iface );
    ULONG ref = InterlockedDecrement( &device->refcount );
    TRACE( "device %p decreasing refcount to %lu.\n", device, ref );
    if (!ref) free( device );
    return ref;
};

static const GameInputDeviceInfo_v0 *WINAPI game_input_device_v0_GetDeviceInfo( IGameInputDevice_v0 *iface )
{
    struct device *device = device_from_IGameInputDevice_v0( iface );
    FIXME( "device %p stub!\n", device );
    return &device->info_v0;
}

static GameInputDeviceStatus WINAPI game_input_device_v0_GetDeviceStatus( IGameInputDevice_v0 *iface )
{
    FIXME( "device %p stub!\n", device_from_IGameInputDevice_v0( iface ) );
    return 0;
}

static void WINAPI game_input_device_v0_GetBatteryState( IGameInputDevice_v0 *iface, GameInputBatteryState *state )
{
    FIXME( "device %p, state %p stub!\n", device_from_IGameInputDevice_v0( iface ), state );
}

static HRESULT WINAPI game_input_device_v0_CreateForceFeedbackEffect( IGameInputDevice_v0 *iface, uint32_t index, const GameInputForceFeedbackParams *params,
                                                                  IGameInputForceFeedbackEffect_v0 **effect )
{
    FIXME( "device %p, index %u, params %p, effect %p stub!\n", device_from_IGameInputDevice_v0( iface ), index, params, effect );
    return E_NOTIMPL;
}

static bool WINAPI game_input_device_v0_IsForceFeedbackMotorPoweredOn( IGameInputDevice_v0 *iface, uint32_t index )
{
    FIXME( "device %p, index %u stub!\n", device_from_IGameInputDevice_v0( iface ), index );
    return FALSE;
}

static void WINAPI game_input_device_v0_SetForceFeedbackMotorGain( IGameInputDevice_v0 *iface, uint32_t index, float gain )
{
    FIXME( "device %p, index %u, gain %f stub!\n", device_from_IGameInputDevice_v0( iface ), index, gain );
}

static HRESULT WINAPI game_input_device_v0_SetHapticMotorState( IGameInputDevice_v0 *iface, uint32_t index, const GameInputHapticFeedbackParams *params )
{
    FIXME( "device %p, index %u, params %p stub!\n", device_from_IGameInputDevice_v0( iface ), index, params );
    return E_NOTIMPL;
}

static void WINAPI game_input_device_v0_SetRumbleState( IGameInputDevice_v0 *iface, const GameInputRumbleParams *params )
{
    FIXME( "device %p, params %p stub!\n", device_from_IGameInputDevice_v0( iface ), params );
}

static void WINAPI game_input_device_v0_SetInputSynchronizationState( IGameInputDevice_v0 *iface, bool enabled )
{
    FIXME( "device %p, enabled %d stub!\n", device_from_IGameInputDevice_v0( iface ), enabled );
}

static void WINAPI game_input_device_v0_SendInputSynchronizationHint( IGameInputDevice_v0 *iface )
{
    FIXME( "device %p stub!\n", device_from_IGameInputDevice_v0( iface ) );
}

static void WINAPI game_input_device_v0_PowerOff( IGameInputDevice_v0 *iface )
{
    FIXME( "device %p stub!\n", device_from_IGameInputDevice_v0( iface ) );
}

static HRESULT WINAPI game_input_device_v0_CreateRawDeviceReport( IGameInputDevice_v0 *iface, uint32_t report_id, GameInputRawDeviceReportKind report_kind,
                                                              IGameInputRawDeviceReport_v0 **report )
{
    FIXME( "device %p, report_id %u, report_kind %#x, report %p stub!\n", device_from_IGameInputDevice_v0( iface ), report_id, report_kind, report );
    return E_NOTIMPL;
}

static HRESULT WINAPI game_input_device_v0_GetRawDeviceFeature( IGameInputDevice_v0 *iface, uint32_t report_id, IGameInputRawDeviceReport_v0 **report )
{
    FIXME( "device %p, report_id %u, report %p stub!\n", device_from_IGameInputDevice_v0( iface ), report_id, report );
    return E_NOTIMPL;
}

static HRESULT WINAPI game_input_device_v0_SetRawDeviceFeature( IGameInputDevice_v0 *iface, IGameInputRawDeviceReport_v0 *report )
{
    FIXME( "device %p, report %p stub!\n", device_from_IGameInputDevice_v0( iface ), report );
    return E_NOTIMPL;
}

static HRESULT WINAPI game_input_device_v0_SendRawDeviceOutput( IGameInputDevice_v0 *iface, IGameInputRawDeviceReport_v0 *report )
{
    FIXME( "device %p, report %p stub!\n", device_from_IGameInputDevice_v0( iface ), report );
    return E_NOTIMPL;
}

static HRESULT WINAPI game_input_device_v0_SendRawDeviceOutputWithResponse( IGameInputDevice_v0 *iface, IGameInputRawDeviceReport_v0 *request_report,
                                                                        IGameInputRawDeviceReport_v0 **response_report )
{
    FIXME( "device %p, request_report %p, response_report %p stub!\n", device_from_IGameInputDevice_v0( iface ), request_report, response_report );
    return E_NOTIMPL;
}

static HRESULT WINAPI game_input_device_v0_ExecuteRawDeviceIoControl( IGameInputDevice_v0 *iface, uint32_t control_code, size_t input_buffer_size, const void *input_buffer,
                                                                  size_t output_buffer_size, void *output_buffer, size_t *output_size )
{
    FIXME( "device %p, control_code %u, input_buffer_size %Iu, input_buffer %p, output_buffer_size %Iu, output_buffer %p, output_size %p stub!\n",
           device_from_IGameInputDevice_v0( iface ), control_code, input_buffer_size, input_buffer, output_buffer_size, output_buffer, output_size );
    return E_NOTIMPL;
}

static bool WINAPI game_input_device_v0_AcquireExclusiveRawDeviceAccess( IGameInputDevice_v0 *iface, uint64_t timeout_us )
{
    FIXME( "device %p, timeout_us %I64u stub!\n", device_from_IGameInputDevice_v0( iface ), timeout_us );
    return FALSE;
}

static void WINAPI game_input_device_v0_ReleaseExclusiveRawDeviceAccess( IGameInputDevice_v0 *iface )
{
    FIXME( "device %p stub!\n", device_from_IGameInputDevice_v0( iface ) );
}

static const IGameInputDevice_v0Vtbl game_input_device_v0_vtbl =
{
    game_input_device_v0_QueryInterface,
    game_input_device_v0_AddRef,
    game_input_device_v0_Release,
    game_input_device_v0_GetDeviceInfo,
    game_input_device_v0_GetDeviceStatus,
    game_input_device_v0_GetBatteryState,
    game_input_device_v0_CreateForceFeedbackEffect,
    game_input_device_v0_IsForceFeedbackMotorPoweredOn,
    game_input_device_v0_SetForceFeedbackMotorGain,
    game_input_device_v0_SetHapticMotorState,
    game_input_device_v0_SetRumbleState,
    game_input_device_v0_SetInputSynchronizationState,
    game_input_device_v0_SendInputSynchronizationHint,
    game_input_device_v0_PowerOff,
    game_input_device_v0_CreateRawDeviceReport,
    game_input_device_v0_GetRawDeviceFeature,
    game_input_device_v0_SetRawDeviceFeature,
    game_input_device_v0_SendRawDeviceOutput,
    game_input_device_v0_SendRawDeviceOutputWithResponse,
    game_input_device_v0_ExecuteRawDeviceIoControl,
    game_input_device_v0_AcquireExclusiveRawDeviceAccess,
    game_input_device_v0_ReleaseExclusiveRawDeviceAccess,
};

static BOOL matches_device_interface( const DEV_OBJECT *object, const GUID *iid )
{
    for (UINT i = 0; i < object->cPropertyCount; i++)
    {
        const DEVPROPERTY *prop = object->pProperties + i;
        if (memcmp( &DEVPKEY_DeviceInterface_ClassGuid, &prop->CompKey.Key, sizeof(prop->CompKey.Key) )) continue;
        return IsEqualGUID( prop->Buffer, iid );
    }

    return FALSE;
}

static struct device *device_create( struct list *devices, const DEV_OBJECT *object )
{
    GameInputDeviceFamily family = GameInputFamilyHid;
    const WCHAR *device_path = object->pszObjectId;
    PHIDP_PREPARSED_DATA preparsed;
    struct device *device = NULL;
    HIDD_ATTRIBUTES attr;
    HIDP_CAPS caps;
    HANDLE file;
    WCHAR *tmp;

    if (!matches_device_interface( object, &GUID_DEVINTERFACE_HID ) &&
        !matches_device_interface( object, &GUID_DEVINTERFACE_WINEXINPUT ))
        return NULL;

    if ((tmp = wcschr( device_path + 8, '#' )) && !wcsnicmp( tmp - 6, L"&IG_", 4 )) return NULL;
    if (tmp && !wcsnicmp( tmp - 6, L"&XI_", 4 )) family = GameInputFamilyXbox360;

    TRACE( "device_path %s\n", debugstr_w( device_path ) );

    file = CreateFileW(device_path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                         NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED | FILE_FLAG_NO_BUFFERING, NULL);
    if (file == INVALID_HANDLE_VALUE) return NULL;

    HidD_GetAttributes( file, &attr );
    HidD_GetPreparsedData( file, &preparsed );
    HidP_GetCaps( preparsed, &caps );
    HidD_FreePreparsedData( preparsed );

    if (caps.UsagePage != HID_USAGE_PAGE_GENERIC || caps.Usage != HID_USAGE_GENERIC_GAMEPAD) goto done;

    if (!(device = calloc( 1, sizeof(*device) ))) goto done;
    device->IGameInputDevice_v0_iface.lpVtbl = &game_input_device_v0_vtbl;
    device->refcount = 1;
    wcscpy( device->path, device_path );

    device->info_v0.infoSize = sizeof(device->info_v0.infoSize);
    device->info_v0.vendorId = attr.VendorID;
    device->info_v0.productId = attr.ProductID;
    device->info_v0.usage.page = caps.UsagePage;
    device->info_v0.usage.id = caps.Usage;
    device->info_v0.deviceFamily = family;
    device->info_v0.capabilities = GameInputDeviceCapabilityNone;
    device->info_v0.supportedInput = GameInputKindUiNavigation_v0 | GameInputKindGamepad | GameInputKindController;
    device->info_v0.supportedRumbleMotors = GameInputRumbleNone;
    device->info_v0.controllerAxisCount = 6;
    device->info_v0.controllerButtonCount = caps.NumberInputButtonCaps;
    device->info_v0.controllerSwitchCount = 1;
    device->info_v0.supportedSystemButtons = GameInputSystemButtonGuide;

    list_add_tail( devices, &device->entry );

    TRACE( "created device %p\n", device );
done:
    CloseHandle( file );
    return device;
}

struct device_callback
{
    struct list entry;

    IGameInputDevice_v0 *device;
    GameInputKind input_kind;
    GameInputDeviceStatus status_filter;

    void *context;
    GameInputDeviceCallback_v0 callback;
};

static void device_callback_notify( struct device_callback *entry, IGameInputDevice_v0 *device, GameInputKind input_kind,
                                    GameInputDeviceStatus new_status, GameInputDeviceStatus old_status )
{
    if (entry->device && entry->device != device) return;
    if (!(entry->input_kind & input_kind)) return;
    if (!(entry->status_filter & (new_status | old_status))) return;
    entry->callback( (UINT_PTR)entry, entry->context, device, 0, new_status, old_status );
}

static HRESULT device_callback_create( IGameInputDevice_v0 *device, GameInputKind input_kind, GameInputDeviceStatus status_filter,
                                       void *context, GameInputDeviceCallback_v0 callback, struct device_callback **out )
{
    struct device_callback *entry;

    if (!(entry = calloc( 1, sizeof(*entry) ))) return E_OUTOFMEMORY;
    if ((entry->device = device)) IGameInputDevice_v0_AddRef( entry->device );
    entry->input_kind = input_kind;
    entry->status_filter = status_filter;
    entry->context = context;
    entry->callback = callback;

    *out = entry;
    return S_OK;
}

struct game_input
{
    IGameInput_v0 IGameInput_v0_iface;
    LONG refcount;

    HDEVQUERY query;
    HANDLE initialized;
    struct list devices;
    struct list callbacks;
};

static struct game_input *impl_from_v0_IGameInput( IGameInput_v0 *iface )
{
    return CONTAINING_RECORD( iface, struct game_input, IGameInput_v0_iface );
}

static HRESULT WINAPI game_input_v0_QueryInterface( IGameInput_v0 *iface, REFIID iid, void **out )
{
    struct game_input *impl = impl_from_v0_IGameInput( iface );

    TRACE( "impl %p, iid %s, out %p.\n", impl, debugstr_guid( iid ), out );

    if (IsEqualGUID( iid, &IID_IUnknown ) ||
        IsEqualGUID( iid, &IID_IGameInput_v0 ))
    {
        *out = &impl->IGameInput_v0_iface;
        IGameInput_v0_AddRef( &impl->IGameInput_v0_iface );
        return S_OK;
    }

    FIXME( "%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid( iid ) );
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI game_input_v0_AddRef( IGameInput_v0 *iface )
{
    struct game_input *impl = impl_from_v0_IGameInput( iface );
    ULONG ref;

    EnterCriticalSection( &game_input_cs );
    ref = ++impl->refcount;
    LeaveCriticalSection( &game_input_cs );

    TRACE( "impl %p increasing refcount to %lu.\n", impl, ref );

    return ref;
}

static ULONG WINAPI game_input_v0_Release( IGameInput_v0 *iface )
{
    struct game_input *impl = impl_from_v0_IGameInput( iface );
    ULONG ref;

    EnterCriticalSection( &game_input_cs );
    if (!(ref = --impl->refcount)) game_input = NULL;
    LeaveCriticalSection( &game_input_cs );

    TRACE( "impl %p decreasing refcount to %lu.\n", impl, ref );

    if (!ref)
    {
        struct list *ptr;

        while ((ptr = list_head( &impl->callbacks )))
        {
            struct device_callback *callback = LIST_ENTRY(ptr, struct device_callback, entry);
            list_remove( &callback->entry );
            free( callback );
        }

        while ((ptr = list_head( &impl->devices )))
        {
            struct device *device = LIST_ENTRY(ptr, struct device, entry);
            list_remove( &device->entry );
            IGameInputDevice_v0_Release( &device->IGameInputDevice_v0_iface );
        }

        DevCloseObjectQuery( impl->query );
        CloseHandle( impl->initialized );
        free( impl );
    }

    return ref;
}

static uint64_t WINAPI game_input_v0_GetCurrentTimestamp( IGameInput_v0 *iface )
{
    FIXME( "impl %p stub!\n", impl_from_v0_IGameInput( iface ) );
    return 0;
}

static HRESULT WINAPI game_input_v0_GetCurrentReading( IGameInput_v0 *iface, GameInputKind input_kind,
                                                       IGameInputDevice_v0 *device_iface, IGameInputReading_v0 **reading )
{
    FIXME( "impl %p, input_kind %#x, device %p, reading %p\n", impl_from_v0_IGameInput( iface ), input_kind, device_iface, reading );
    return E_NOTIMPL;
}

static HRESULT WINAPI game_input_v0_GetNextReading( IGameInput_v0 *iface, IGameInputReading_v0 *reference_reading, GameInputKind input_kind,
                                                    IGameInputDevice_v0 *device_iface, IGameInputReading_v0 **reading )
{
    FIXME( "impl %p, reference_reading %p, input_kind %#x, device %p, reading %p\n", impl_from_v0_IGameInput( iface ), reference_reading, input_kind, device_iface, reading );
    return E_NOTIMPL;
}

static HRESULT WINAPI game_input_v0_GetPreviousReading( IGameInput_v0 *iface, IGameInputReading_v0 *reference_reading,
                                                        GameInputKind input_kind, IGameInputDevice_v0 *device,
                                                        IGameInputReading_v0 **reading )
{
    FIXME( "impl %p, reference_reading %p, input_kind %#x, device %p, reading %p stub!\n",
           impl_from_v0_IGameInput( iface ), reference_reading, input_kind, device, reading );
    return E_NOTIMPL;
}

static HRESULT WINAPI game_input_v0_GetTemporalReading( IGameInput_v0 *iface, uint64_t timestamp, IGameInputDevice_v0 *device, IGameInputReading_v0 **reading )
{
    FIXME( "impl %p, timestamp %I64u, device %p, reading %p stub!\n", impl_from_v0_IGameInput( iface ), timestamp, device, reading );
    return E_NOTIMPL;
}

static HRESULT WINAPI game_input_v0_RegisterReadingCallback( IGameInput_v0 *iface, IGameInputDevice_v0 *device, GameInputKind input_kind, float analog_threshold,
                                                             void *context, GameInputReadingCallback_v0 callback, GameInputCallbackToken *token )
{
    FIXME( "impl %p, device %p, input_kind %#x, analog_threshold %f, context %p, callback %p, token %p stub!\n",
           impl_from_v0_IGameInput( iface ), device, input_kind, analog_threshold, context, callback, token );
    return E_NOTIMPL;
}

static HRESULT WINAPI game_input_v0_RegisterDeviceCallback( IGameInput_v0 *iface, IGameInputDevice_v0 *device_iface, GameInputKind input_kind, GameInputDeviceStatus status_filter,
                                                            GameInputEnumerationKind enumeration_kind, void *context, GameInputDeviceCallback_v0 callback,
                                                            GameInputCallbackToken *token )
{
    struct game_input *impl = CONTAINING_RECORD( iface, struct game_input, IGameInput_v0_iface );
    struct device_callback *entry;
    struct device *device;
    HRESULT hr;

    FIXME( "impl %p device_iface %p input_kind %#x status_filter %#x enumeration_kind %#x, context %p callback %p token %p stub!\n",
           impl, device_iface, input_kind, status_filter, enumeration_kind, context, callback, token );

    if (FAILED(hr = device_callback_create( device_iface, input_kind, status_filter, context, callback, &entry ))) return hr;

    EnterCriticalSection( &game_input_cs );
    list_add_tail( &impl->callbacks, &entry->entry );

    LIST_FOR_EACH_ENTRY( device, &impl->devices, struct device, entry )
        device_callback_notify( entry, &device->IGameInputDevice_v0_iface, GameInputKindGamepad,
                                device->status, GameInputDeviceNoStatus );

    LeaveCriticalSection( &game_input_cs );

    *token = (UINT_PTR)entry;

    return hr;
}

static HRESULT WINAPI game_input_v0_RegisterSystemButtonCallback( IGameInput_v0 *iface, IGameInputDevice_v0 *device,
                                                                  GameInputSystemButtons button_filter, void *context,
                                                                  GameInputSystemButtonCallback_v0 callback, GameInputCallbackToken *token )
{
    FIXME( "impl %p, device %p, button_filter %#x, context %p, callback %p, token %p stub!\n",
           impl_from_v0_IGameInput( iface ), device, button_filter, context, callback, token );
    return E_NOTIMPL;
}

static HRESULT WINAPI game_input_v0_RegisterKeyboardLayoutCallback( IGameInput_v0 *iface, IGameInputDevice_v0 *device, void *context,
                                                                    GameInputKeyboardLayoutCallback_v0 callback, GameInputCallbackToken *token )
{
    FIXME( "impl %p, device %p, context %p, callback %p, token %p stub!\n",
           impl_from_v0_IGameInput( iface ), device, context, callback, token );
    return E_NOTIMPL;
}

static void WINAPI game_input_v0_StopCallback( IGameInput_v0 *iface, GameInputCallbackToken token )
{
    FIXME( "impl %p, token %I64u stub!\n", impl_from_v0_IGameInput( iface ), token );
}

static bool WINAPI game_input_v0_UnregisterCallback( IGameInput_v0 *iface, GameInputCallbackToken token, uint64_t timeout_us )
{
    FIXME( "impl %p, token %I64u, timeout_us %I64u stub!\n", impl_from_v0_IGameInput( iface ), token, timeout_us );
    return FALSE;
}

static HRESULT WINAPI game_input_v0_CreateDispatcher( IGameInput_v0 *iface, IGameInputDispatcher **dispatcher )
{
    FIXME( "impl %p, dispatcher %p stub!\n", impl_from_v0_IGameInput( iface ), dispatcher );
    return E_NOTIMPL;
}

static HRESULT WINAPI game_input_v0_CreateAggregateDevice( IGameInput_v0 *iface, GameInputKind input_kind, IGameInputDevice_v0 **device )
{
    FIXME( "impl %p, input_kind %#x, device %p stub!\n", impl_from_v0_IGameInput( iface ), input_kind, device );
    return E_NOTIMPL;
}

static HRESULT WINAPI game_input_v0_FindDeviceFromId( IGameInput_v0 *iface, const APP_LOCAL_DEVICE_ID *value, IGameInputDevice_v0 **device )
{
    FIXME( "impl %p, value %p, device %p stub!\n", impl_from_v0_IGameInput( iface ), value, device );
    return E_NOTIMPL;
}

static HRESULT WINAPI game_input_v0_FindDeviceFromObject( IGameInput_v0 *iface, IUnknown *value, IGameInputDevice_v0 **device )
{
    FIXME( "impl %p, value %p, device %p stub!\n", impl_from_v0_IGameInput( iface ), value, device );
    return E_NOTIMPL;
}

static HRESULT WINAPI game_input_v0_FindDeviceFromPlatformHandle( IGameInput_v0 *iface, HANDLE value, IGameInputDevice_v0 **device )
{
    FIXME( "impl %p, value %p, device %p stub!\n", impl_from_v0_IGameInput( iface ), value, device );
    return E_NOTIMPL;
}

static HRESULT WINAPI game_input_v0_FindDeviceFromPlatformString( IGameInput_v0 *iface, const WCHAR *value, IGameInputDevice_v0 **device )
{
    FIXME( "impl %p, value %s, device %p stub!\n", impl_from_v0_IGameInput( iface ), debugstr_w(value), device );
    return E_NOTIMPL;
}

static HRESULT WINAPI game_input_v0_EnableOemDeviceSupport( IGameInput_v0 *iface, uint16_t vendor_id, uint16_t product_id,
                                                            uint8_t interface_number, uint8_t collection_number )
{
    FIXME( "impl %p, vendor_id %u, product_id %u, interface_number %u, collection_number %u stub!\n",
           impl_from_v0_IGameInput( iface ), vendor_id, product_id, interface_number, collection_number );
    return E_NOTIMPL;
}

static void WINAPI game_input_v0_SetFocusPolicy( IGameInput_v0 *iface, GameInputFocusPolicy policy )
{
    FIXME( "impl %p, policy %#x stub!\n", impl_from_v0_IGameInput( iface ), policy );
}

static const IGameInput_v0Vtbl game_input_v0_vtbl =
{
    game_input_v0_QueryInterface,
    game_input_v0_AddRef,
    game_input_v0_Release,
    game_input_v0_GetCurrentTimestamp,
    game_input_v0_GetCurrentReading,
    game_input_v0_GetNextReading,
    game_input_v0_GetPreviousReading,
    game_input_v0_GetTemporalReading,
    game_input_v0_RegisterReadingCallback,
    game_input_v0_RegisterDeviceCallback,
    game_input_v0_RegisterSystemButtonCallback,
    game_input_v0_RegisterKeyboardLayoutCallback,
    game_input_v0_StopCallback,
    game_input_v0_UnregisterCallback,
    game_input_v0_CreateDispatcher,
    game_input_v0_CreateAggregateDevice,
    game_input_v0_FindDeviceFromId,
    game_input_v0_FindDeviceFromObject,
    game_input_v0_FindDeviceFromPlatformHandle,
    game_input_v0_FindDeviceFromPlatformString,
    game_input_v0_EnableOemDeviceSupport,
    game_input_v0_SetFocusPolicy,
};

static struct device *find_device( struct list *devices, const DEV_OBJECT *object )
{
    struct device *device;

    LIST_FOR_EACH_ENTRY( device, devices, struct device, entry )
        if (!wcscmp( device->path, object->pszObjectId )) return device;
    return NULL;
}

static void WINAPI device_query_cb( HDEVQUERY devquery, void *context, const DEV_QUERY_RESULT_ACTION_DATA *action )
{
    struct game_input *impl = context;
    struct device *device;

    switch (action->Action)
    {
    case DevQueryResultStateChange:
        TRACE( "impl %p, DevQueryResultStateChange %u\n", impl, action->Data.State );
        SetEvent( impl->initialized );
        break;

    case DevQueryResultAdd:
        TRACE( "impl %p, action %u type %u id %s props %lu\n", impl, action->Action, action->Data.DeviceObject.ObjectType,
               debugstr_w(action->Data.DeviceObject.pszObjectId), action->Data.DeviceObject.cPropertyCount );

        EnterCriticalSection( &game_input_cs );
        if ((device = find_device( &impl->devices, &action->Data.DeviceObject )) ||
            (device = device_create( &impl->devices, &action->Data.DeviceObject )))
        {
            GameInputDeviceStatus old_status = device->status;
            struct device_callback *entry;
            device->status = GameInputDeviceConnected;

            LIST_FOR_EACH_ENTRY( entry, &impl->callbacks, struct device_callback, entry )
                device_callback_notify( entry, &device->IGameInputDevice_v0_iface, GameInputKindGamepad,
                                        device->status, old_status );
        }
        LeaveCriticalSection( &game_input_cs );

        break;

    case DevQueryResultRemove:
        TRACE( "impl %p, action %u type %u id %s props %lu\n", impl, action->Action, action->Data.DeviceObject.ObjectType,
               debugstr_w(action->Data.DeviceObject.pszObjectId), action->Data.DeviceObject.cPropertyCount );

        EnterCriticalSection( &game_input_cs );
        if ((device = find_device( &impl->devices, &action->Data.DeviceObject )))
        {
            GameInputDeviceStatus old_status = device->status;
            struct device_callback *entry;
            device->status = GameInputDeviceNoStatus;

            LIST_FOR_EACH_ENTRY( entry, &impl->callbacks, struct device_callback, entry )
                device_callback_notify( entry, &device->IGameInputDevice_v0_iface, GameInputKindGamepad,
                                        device->status, old_status );
        }
        LeaveCriticalSection( &game_input_cs );

        break;

    case DevQueryResultUpdate:
        TRACE( "impl %p, action %u type %u id %s props %lu\n", impl, action->Action, action->Data.DeviceObject.ObjectType,
               debugstr_w(action->Data.DeviceObject.pszObjectId), action->Data.DeviceObject.cPropertyCount );
        break;
    }
}

static struct game_input *game_input_create(void)
{
    struct game_input *impl;

    if (!(impl = calloc( 1, sizeof(*impl) ))) return NULL;
    impl->IGameInput_v0_iface.lpVtbl = &game_input_v0_vtbl;
    impl->refcount = 1;
    list_init( &impl->devices );
    list_init( &impl->callbacks );

    if (!(impl->initialized = CreateEventW( NULL, TRUE, FALSE, NULL )) ||
        FAILED(DevCreateObjectQuery( DevObjectTypeDeviceInterface, DevQueryFlagUpdateResults | DevQueryFlagAllProperties,
                                     0, NULL, 0, NULL, device_query_cb, impl, &impl->query )))
    {
        if (impl->initialized) CloseHandle( impl->initialized );
        ERR( "Failed to enumerate devices\n" );
        free( impl );
        return NULL;
    }

    TRACE( "created GameInput %p\n", impl );
    return impl;
}

HRESULT WINAPI GameInputCreate( IGameInput_v0 **out )
{
    struct game_input *impl;

    const char *sgi, *sd;
    if ((!(sgi = getenv( "SteamGameId" )) || strcmp(sgi, "1771300") /* Kingdom Come Deliverance II */)) goto failed;
    if ((sd = getenv( "SteamDeck" )) && atoi( sd )) goto failed; /* not on Steam Deck */

    TRACE( "out %p\n", out );

    EnterCriticalSection( &game_input_cs );
    if ((impl = game_input)) impl->refcount++;
    else impl = game_input = game_input_create();
    LeaveCriticalSection( &game_input_cs );
    if (!impl) return E_OUTOFMEMORY;

    WaitForSingleObject( impl->initialized, INFINITE );
    *out = &impl->IGameInput_v0_iface;
    return S_OK;

failed:
    FIXME( "out %p, stub!\n", out );
    return E_NOTIMPL;
}

HRESULT WINAPI DllGetClassObject( REFCLSID clsid, REFIID riid, void **out )
{
    FIXME( "clsid %s, riid %s, out %p stub!\n", debugstr_guid(clsid), debugstr_guid(riid), out );
    return CLASS_E_CLASSNOTAVAILABLE;
}
