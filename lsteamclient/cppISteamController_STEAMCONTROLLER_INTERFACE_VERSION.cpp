/* This file is auto-generated, do not edit. */
#include "unix_private.h"

#if 0
#pragma makedep unix
#endif

NTSTATUS ISteamController_STEAMCONTROLLER_INTERFACE_VERSION_Init( void *args )
{
    struct ISteamController_STEAMCONTROLLER_INTERFACE_VERSION_Init_params *params = (struct ISteamController_STEAMCONTROLLER_INTERFACE_VERSION_Init_params *)args;
    struct u_ISteamController_STEAMCONTROLLER_INTERFACE_VERSION *iface = (struct u_ISteamController_STEAMCONTROLLER_INTERFACE_VERSION *)params->u_iface;
    char *u_pchAbsolutePathToControllerConfigVDF = steamclient_dos_to_unix_path( params->pchAbsolutePathToControllerConfigVDF, 0 );
    params->_ret = iface->Init( u_pchAbsolutePathToControllerConfigVDF );
    steamclient_free_path( u_pchAbsolutePathToControllerConfigVDF );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamController_STEAMCONTROLLER_INTERFACE_VERSION_Init( void *args )
{
    struct wow64_ISteamController_STEAMCONTROLLER_INTERFACE_VERSION_Init_params *params = (struct wow64_ISteamController_STEAMCONTROLLER_INTERFACE_VERSION_Init_params *)args;
    struct u_ISteamController_STEAMCONTROLLER_INTERFACE_VERSION *iface = (struct u_ISteamController_STEAMCONTROLLER_INTERFACE_VERSION *)params->u_iface;
    char *u_pchAbsolutePathToControllerConfigVDF = steamclient_dos_to_unix_path( params->pchAbsolutePathToControllerConfigVDF, 0 );
    params->_ret = iface->Init( u_pchAbsolutePathToControllerConfigVDF );
    steamclient_free_path( u_pchAbsolutePathToControllerConfigVDF );
    return 0;
}
#endif

NTSTATUS ISteamController_STEAMCONTROLLER_INTERFACE_VERSION_Shutdown( void *args )
{
    struct ISteamController_STEAMCONTROLLER_INTERFACE_VERSION_Shutdown_params *params = (struct ISteamController_STEAMCONTROLLER_INTERFACE_VERSION_Shutdown_params *)args;
    struct u_ISteamController_STEAMCONTROLLER_INTERFACE_VERSION *iface = (struct u_ISteamController_STEAMCONTROLLER_INTERFACE_VERSION *)params->u_iface;
    params->_ret = iface->Shutdown(  );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamController_STEAMCONTROLLER_INTERFACE_VERSION_Shutdown( void *args )
{
    struct wow64_ISteamController_STEAMCONTROLLER_INTERFACE_VERSION_Shutdown_params *params = (struct wow64_ISteamController_STEAMCONTROLLER_INTERFACE_VERSION_Shutdown_params *)args;
    struct u_ISteamController_STEAMCONTROLLER_INTERFACE_VERSION *iface = (struct u_ISteamController_STEAMCONTROLLER_INTERFACE_VERSION *)params->u_iface;
    params->_ret = iface->Shutdown(  );
    return 0;
}
#endif

NTSTATUS ISteamController_STEAMCONTROLLER_INTERFACE_VERSION_RunFrame( void *args )
{
    struct ISteamController_STEAMCONTROLLER_INTERFACE_VERSION_RunFrame_params *params = (struct ISteamController_STEAMCONTROLLER_INTERFACE_VERSION_RunFrame_params *)args;
    struct u_ISteamController_STEAMCONTROLLER_INTERFACE_VERSION *iface = (struct u_ISteamController_STEAMCONTROLLER_INTERFACE_VERSION *)params->u_iface;
    iface->RunFrame(  );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamController_STEAMCONTROLLER_INTERFACE_VERSION_RunFrame( void *args )
{
    struct wow64_ISteamController_STEAMCONTROLLER_INTERFACE_VERSION_RunFrame_params *params = (struct wow64_ISteamController_STEAMCONTROLLER_INTERFACE_VERSION_RunFrame_params *)args;
    struct u_ISteamController_STEAMCONTROLLER_INTERFACE_VERSION *iface = (struct u_ISteamController_STEAMCONTROLLER_INTERFACE_VERSION *)params->u_iface;
    iface->RunFrame(  );
    return 0;
}
#endif

NTSTATUS ISteamController_STEAMCONTROLLER_INTERFACE_VERSION_GetControllerState( void *args )
{
    struct ISteamController_STEAMCONTROLLER_INTERFACE_VERSION_GetControllerState_params *params = (struct ISteamController_STEAMCONTROLLER_INTERFACE_VERSION_GetControllerState_params *)args;
    struct u_ISteamController_STEAMCONTROLLER_INTERFACE_VERSION *iface = (struct u_ISteamController_STEAMCONTROLLER_INTERFACE_VERSION *)params->u_iface;
    params->_ret = iface->GetControllerState( params->unControllerIndex, params->pState );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamController_STEAMCONTROLLER_INTERFACE_VERSION_GetControllerState( void *args )
{
    struct wow64_ISteamController_STEAMCONTROLLER_INTERFACE_VERSION_GetControllerState_params *params = (struct wow64_ISteamController_STEAMCONTROLLER_INTERFACE_VERSION_GetControllerState_params *)args;
    struct u_ISteamController_STEAMCONTROLLER_INTERFACE_VERSION *iface = (struct u_ISteamController_STEAMCONTROLLER_INTERFACE_VERSION *)params->u_iface;
    params->_ret = iface->GetControllerState( params->unControllerIndex, params->pState );
    return 0;
}
#endif

NTSTATUS ISteamController_STEAMCONTROLLER_INTERFACE_VERSION_TriggerHapticPulse( void *args )
{
    struct ISteamController_STEAMCONTROLLER_INTERFACE_VERSION_TriggerHapticPulse_params *params = (struct ISteamController_STEAMCONTROLLER_INTERFACE_VERSION_TriggerHapticPulse_params *)args;
    struct u_ISteamController_STEAMCONTROLLER_INTERFACE_VERSION *iface = (struct u_ISteamController_STEAMCONTROLLER_INTERFACE_VERSION *)params->u_iface;
    iface->TriggerHapticPulse( params->unControllerIndex, params->eTargetPad, params->usDurationMicroSec );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamController_STEAMCONTROLLER_INTERFACE_VERSION_TriggerHapticPulse( void *args )
{
    struct wow64_ISteamController_STEAMCONTROLLER_INTERFACE_VERSION_TriggerHapticPulse_params *params = (struct wow64_ISteamController_STEAMCONTROLLER_INTERFACE_VERSION_TriggerHapticPulse_params *)args;
    struct u_ISteamController_STEAMCONTROLLER_INTERFACE_VERSION *iface = (struct u_ISteamController_STEAMCONTROLLER_INTERFACE_VERSION *)params->u_iface;
    iface->TriggerHapticPulse( params->unControllerIndex, params->eTargetPad, params->usDurationMicroSec );
    return 0;
}
#endif

NTSTATUS ISteamController_STEAMCONTROLLER_INTERFACE_VERSION_SetOverrideMode( void *args )
{
    struct ISteamController_STEAMCONTROLLER_INTERFACE_VERSION_SetOverrideMode_params *params = (struct ISteamController_STEAMCONTROLLER_INTERFACE_VERSION_SetOverrideMode_params *)args;
    struct u_ISteamController_STEAMCONTROLLER_INTERFACE_VERSION *iface = (struct u_ISteamController_STEAMCONTROLLER_INTERFACE_VERSION *)params->u_iface;
    iface->SetOverrideMode( params->pchMode );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamController_STEAMCONTROLLER_INTERFACE_VERSION_SetOverrideMode( void *args )
{
    struct wow64_ISteamController_STEAMCONTROLLER_INTERFACE_VERSION_SetOverrideMode_params *params = (struct wow64_ISteamController_STEAMCONTROLLER_INTERFACE_VERSION_SetOverrideMode_params *)args;
    struct u_ISteamController_STEAMCONTROLLER_INTERFACE_VERSION *iface = (struct u_ISteamController_STEAMCONTROLLER_INTERFACE_VERSION *)params->u_iface;
    iface->SetOverrideMode( params->pchMode );
    return 0;
}
#endif

