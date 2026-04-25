/* This file is auto-generated, do not edit. */
#include "unix_private.h"

#if 0
#pragma makedep unix
#endif

NTSTATUS ISteamMatchmakingServers_SteamMatchMakingServers001_GetServerDetails( void *args )
{
    struct ISteamMatchmakingServers_SteamMatchMakingServers001_GetServerDetails_params *params = (struct ISteamMatchmakingServers_SteamMatchMakingServers001_GetServerDetails_params *)args;
    struct u_ISteamMatchmakingServers_SteamMatchMakingServers001 *iface = (struct u_ISteamMatchmakingServers_SteamMatchMakingServers001 *)params->u_iface;
    params->_ret = iface->GetServerDetails( params->eType, params->iServer );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamMatchmakingServers_SteamMatchMakingServers001_GetServerDetails( void *args )
{
    struct wow64_ISteamMatchmakingServers_SteamMatchMakingServers001_GetServerDetails_params *params = (struct wow64_ISteamMatchmakingServers_SteamMatchMakingServers001_GetServerDetails_params *)args;
    struct u_ISteamMatchmakingServers_SteamMatchMakingServers001 *iface = (struct u_ISteamMatchmakingServers_SteamMatchMakingServers001 *)params->u_iface;
    params->_ret = iface->GetServerDetails( params->eType, params->iServer );
    return 0;
}
#endif

NTSTATUS ISteamMatchmakingServers_SteamMatchMakingServers001_CancelQuery( void *args )
{
    struct ISteamMatchmakingServers_SteamMatchMakingServers001_CancelQuery_params *params = (struct ISteamMatchmakingServers_SteamMatchMakingServers001_CancelQuery_params *)args;
    struct u_ISteamMatchmakingServers_SteamMatchMakingServers001 *iface = (struct u_ISteamMatchmakingServers_SteamMatchMakingServers001 *)params->u_iface;
    iface->CancelQuery( params->eType );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamMatchmakingServers_SteamMatchMakingServers001_CancelQuery( void *args )
{
    struct wow64_ISteamMatchmakingServers_SteamMatchMakingServers001_CancelQuery_params *params = (struct wow64_ISteamMatchmakingServers_SteamMatchMakingServers001_CancelQuery_params *)args;
    struct u_ISteamMatchmakingServers_SteamMatchMakingServers001 *iface = (struct u_ISteamMatchmakingServers_SteamMatchMakingServers001 *)params->u_iface;
    iface->CancelQuery( params->eType );
    return 0;
}
#endif

NTSTATUS ISteamMatchmakingServers_SteamMatchMakingServers001_RefreshQuery( void *args )
{
    struct ISteamMatchmakingServers_SteamMatchMakingServers001_RefreshQuery_params *params = (struct ISteamMatchmakingServers_SteamMatchMakingServers001_RefreshQuery_params *)args;
    struct u_ISteamMatchmakingServers_SteamMatchMakingServers001 *iface = (struct u_ISteamMatchmakingServers_SteamMatchMakingServers001 *)params->u_iface;
    iface->RefreshQuery( params->eType );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamMatchmakingServers_SteamMatchMakingServers001_RefreshQuery( void *args )
{
    struct wow64_ISteamMatchmakingServers_SteamMatchMakingServers001_RefreshQuery_params *params = (struct wow64_ISteamMatchmakingServers_SteamMatchMakingServers001_RefreshQuery_params *)args;
    struct u_ISteamMatchmakingServers_SteamMatchMakingServers001 *iface = (struct u_ISteamMatchmakingServers_SteamMatchMakingServers001 *)params->u_iface;
    iface->RefreshQuery( params->eType );
    return 0;
}
#endif

NTSTATUS ISteamMatchmakingServers_SteamMatchMakingServers001_IsRefreshing( void *args )
{
    struct ISteamMatchmakingServers_SteamMatchMakingServers001_IsRefreshing_params *params = (struct ISteamMatchmakingServers_SteamMatchMakingServers001_IsRefreshing_params *)args;
    struct u_ISteamMatchmakingServers_SteamMatchMakingServers001 *iface = (struct u_ISteamMatchmakingServers_SteamMatchMakingServers001 *)params->u_iface;
    params->_ret = iface->IsRefreshing( params->eType );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamMatchmakingServers_SteamMatchMakingServers001_IsRefreshing( void *args )
{
    struct wow64_ISteamMatchmakingServers_SteamMatchMakingServers001_IsRefreshing_params *params = (struct wow64_ISteamMatchmakingServers_SteamMatchMakingServers001_IsRefreshing_params *)args;
    struct u_ISteamMatchmakingServers_SteamMatchMakingServers001 *iface = (struct u_ISteamMatchmakingServers_SteamMatchMakingServers001 *)params->u_iface;
    params->_ret = iface->IsRefreshing( params->eType );
    return 0;
}
#endif

NTSTATUS ISteamMatchmakingServers_SteamMatchMakingServers001_GetServerCount( void *args )
{
    struct ISteamMatchmakingServers_SteamMatchMakingServers001_GetServerCount_params *params = (struct ISteamMatchmakingServers_SteamMatchMakingServers001_GetServerCount_params *)args;
    struct u_ISteamMatchmakingServers_SteamMatchMakingServers001 *iface = (struct u_ISteamMatchmakingServers_SteamMatchMakingServers001 *)params->u_iface;
    params->_ret = iface->GetServerCount( params->eType );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamMatchmakingServers_SteamMatchMakingServers001_GetServerCount( void *args )
{
    struct wow64_ISteamMatchmakingServers_SteamMatchMakingServers001_GetServerCount_params *params = (struct wow64_ISteamMatchmakingServers_SteamMatchMakingServers001_GetServerCount_params *)args;
    struct u_ISteamMatchmakingServers_SteamMatchMakingServers001 *iface = (struct u_ISteamMatchmakingServers_SteamMatchMakingServers001 *)params->u_iface;
    params->_ret = iface->GetServerCount( params->eType );
    return 0;
}
#endif

NTSTATUS ISteamMatchmakingServers_SteamMatchMakingServers001_RefreshServer( void *args )
{
    struct ISteamMatchmakingServers_SteamMatchMakingServers001_RefreshServer_params *params = (struct ISteamMatchmakingServers_SteamMatchMakingServers001_RefreshServer_params *)args;
    struct u_ISteamMatchmakingServers_SteamMatchMakingServers001 *iface = (struct u_ISteamMatchmakingServers_SteamMatchMakingServers001 *)params->u_iface;
    iface->RefreshServer( params->eType, params->iServer );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamMatchmakingServers_SteamMatchMakingServers001_RefreshServer( void *args )
{
    struct wow64_ISteamMatchmakingServers_SteamMatchMakingServers001_RefreshServer_params *params = (struct wow64_ISteamMatchmakingServers_SteamMatchMakingServers001_RefreshServer_params *)args;
    struct u_ISteamMatchmakingServers_SteamMatchMakingServers001 *iface = (struct u_ISteamMatchmakingServers_SteamMatchMakingServers001 *)params->u_iface;
    iface->RefreshServer( params->eType, params->iServer );
    return 0;
}
#endif

NTSTATUS ISteamMatchmakingServers_SteamMatchMakingServers001_CancelServerQuery( void *args )
{
    struct ISteamMatchmakingServers_SteamMatchMakingServers001_CancelServerQuery_params *params = (struct ISteamMatchmakingServers_SteamMatchMakingServers001_CancelServerQuery_params *)args;
    struct u_ISteamMatchmakingServers_SteamMatchMakingServers001 *iface = (struct u_ISteamMatchmakingServers_SteamMatchMakingServers001 *)params->u_iface;
    iface->CancelServerQuery( params->hServerQuery );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamMatchmakingServers_SteamMatchMakingServers001_CancelServerQuery( void *args )
{
    struct wow64_ISteamMatchmakingServers_SteamMatchMakingServers001_CancelServerQuery_params *params = (struct wow64_ISteamMatchmakingServers_SteamMatchMakingServers001_CancelServerQuery_params *)args;
    struct u_ISteamMatchmakingServers_SteamMatchMakingServers001 *iface = (struct u_ISteamMatchmakingServers_SteamMatchMakingServers001 *)params->u_iface;
    iface->CancelServerQuery( params->hServerQuery );
    return 0;
}
#endif

