/* This file is auto-generated, do not edit. */
#include "unix_private.h"

#if 0
#pragma makedep unix
#endif

NTSTATUS ISteamNetworkingFakeUDPPort_SteamNetworkingFakeUDPPort001_DestroyFakeUDPPort( void *args )
{
    struct ISteamNetworkingFakeUDPPort_SteamNetworkingFakeUDPPort001_DestroyFakeUDPPort_params *params = (struct ISteamNetworkingFakeUDPPort_SteamNetworkingFakeUDPPort001_DestroyFakeUDPPort_params *)args;
    struct u_ISteamNetworkingFakeUDPPort_SteamNetworkingFakeUDPPort001 *iface = (struct u_ISteamNetworkingFakeUDPPort_SteamNetworkingFakeUDPPort001 *)params->u_iface;
    iface->DestroyFakeUDPPort(  );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamNetworkingFakeUDPPort_SteamNetworkingFakeUDPPort001_DestroyFakeUDPPort( void *args )
{
    struct wow64_ISteamNetworkingFakeUDPPort_SteamNetworkingFakeUDPPort001_DestroyFakeUDPPort_params *params = (struct wow64_ISteamNetworkingFakeUDPPort_SteamNetworkingFakeUDPPort001_DestroyFakeUDPPort_params *)args;
    struct u_ISteamNetworkingFakeUDPPort_SteamNetworkingFakeUDPPort001 *iface = (struct u_ISteamNetworkingFakeUDPPort_SteamNetworkingFakeUDPPort001 *)params->u_iface;
    iface->DestroyFakeUDPPort(  );
    return 0;
}
#endif

NTSTATUS ISteamNetworkingFakeUDPPort_SteamNetworkingFakeUDPPort001_SendMessageToFakeIP( void *args )
{
    struct ISteamNetworkingFakeUDPPort_SteamNetworkingFakeUDPPort001_SendMessageToFakeIP_params *params = (struct ISteamNetworkingFakeUDPPort_SteamNetworkingFakeUDPPort001_SendMessageToFakeIP_params *)args;
    struct u_ISteamNetworkingFakeUDPPort_SteamNetworkingFakeUDPPort001 *iface = (struct u_ISteamNetworkingFakeUDPPort_SteamNetworkingFakeUDPPort001 *)params->u_iface;
    params->_ret = iface->SendMessageToFakeIP( params->remoteAddress, params->pData, params->cbData, params->nSendFlags );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamNetworkingFakeUDPPort_SteamNetworkingFakeUDPPort001_SendMessageToFakeIP( void *args )
{
    struct wow64_ISteamNetworkingFakeUDPPort_SteamNetworkingFakeUDPPort001_SendMessageToFakeIP_params *params = (struct wow64_ISteamNetworkingFakeUDPPort_SteamNetworkingFakeUDPPort001_SendMessageToFakeIP_params *)args;
    struct u_ISteamNetworkingFakeUDPPort_SteamNetworkingFakeUDPPort001 *iface = (struct u_ISteamNetworkingFakeUDPPort_SteamNetworkingFakeUDPPort001 *)params->u_iface;
    params->_ret = iface->SendMessageToFakeIP( params->remoteAddress, params->pData, params->cbData, params->nSendFlags );
    return 0;
}
#endif

NTSTATUS ISteamNetworkingFakeUDPPort_SteamNetworkingFakeUDPPort001_ScheduleCleanup( void *args )
{
    struct ISteamNetworkingFakeUDPPort_SteamNetworkingFakeUDPPort001_ScheduleCleanup_params *params = (struct ISteamNetworkingFakeUDPPort_SteamNetworkingFakeUDPPort001_ScheduleCleanup_params *)args;
    struct u_ISteamNetworkingFakeUDPPort_SteamNetworkingFakeUDPPort001 *iface = (struct u_ISteamNetworkingFakeUDPPort_SteamNetworkingFakeUDPPort001 *)params->u_iface;
    iface->ScheduleCleanup( params->remoteAddress );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamNetworkingFakeUDPPort_SteamNetworkingFakeUDPPort001_ScheduleCleanup( void *args )
{
    struct wow64_ISteamNetworkingFakeUDPPort_SteamNetworkingFakeUDPPort001_ScheduleCleanup_params *params = (struct wow64_ISteamNetworkingFakeUDPPort_SteamNetworkingFakeUDPPort001_ScheduleCleanup_params *)args;
    struct u_ISteamNetworkingFakeUDPPort_SteamNetworkingFakeUDPPort001 *iface = (struct u_ISteamNetworkingFakeUDPPort_SteamNetworkingFakeUDPPort001 *)params->u_iface;
    iface->ScheduleCleanup( params->remoteAddress );
    return 0;
}
#endif

