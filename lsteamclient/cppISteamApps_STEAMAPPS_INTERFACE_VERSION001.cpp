/* This file is auto-generated, do not edit. */
#include "unix_private.h"

#if 0
#pragma makedep unix
#endif

NTSTATUS ISteamApps_STEAMAPPS_INTERFACE_VERSION001_GetAppData( void *args )
{
    struct ISteamApps_STEAMAPPS_INTERFACE_VERSION001_GetAppData_params *params = (struct ISteamApps_STEAMAPPS_INTERFACE_VERSION001_GetAppData_params *)args;
    struct u_ISteamApps_STEAMAPPS_INTERFACE_VERSION001 *iface = (struct u_ISteamApps_STEAMAPPS_INTERFACE_VERSION001 *)params->u_iface;
    params->_ret = iface->GetAppData( params->nAppID, params->pchKey, params->pchValue, params->cchValueMax );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamApps_STEAMAPPS_INTERFACE_VERSION001_GetAppData( void *args )
{
    struct wow64_ISteamApps_STEAMAPPS_INTERFACE_VERSION001_GetAppData_params *params = (struct wow64_ISteamApps_STEAMAPPS_INTERFACE_VERSION001_GetAppData_params *)args;
    struct u_ISteamApps_STEAMAPPS_INTERFACE_VERSION001 *iface = (struct u_ISteamApps_STEAMAPPS_INTERFACE_VERSION001 *)params->u_iface;
    params->_ret = iface->GetAppData( params->nAppID, params->pchKey, params->pchValue, params->cchValueMax );
    return 0;
}
#endif

