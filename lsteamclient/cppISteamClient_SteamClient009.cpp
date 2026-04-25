/* This file is auto-generated, do not edit. */
#include "unix_private.h"

#if 0
#pragma makedep unix
#endif

NTSTATUS ISteamClient_SteamClient009_CreateSteamPipe( void *args )
{
    struct ISteamClient_SteamClient009_CreateSteamPipe_params *params = (struct ISteamClient_SteamClient009_CreateSteamPipe_params *)args;
    struct u_ISteamClient_SteamClient009 *iface = (struct u_ISteamClient_SteamClient009 *)params->u_iface;
    params->_ret = iface->CreateSteamPipe(  );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamClient_SteamClient009_CreateSteamPipe( void *args )
{
    struct wow64_ISteamClient_SteamClient009_CreateSteamPipe_params *params = (struct wow64_ISteamClient_SteamClient009_CreateSteamPipe_params *)args;
    struct u_ISteamClient_SteamClient009 *iface = (struct u_ISteamClient_SteamClient009 *)params->u_iface;
    params->_ret = iface->CreateSteamPipe(  );
    return 0;
}
#endif

NTSTATUS ISteamClient_SteamClient009_BReleaseSteamPipe( void *args )
{
    struct ISteamClient_SteamClient009_BReleaseSteamPipe_params *params = (struct ISteamClient_SteamClient009_BReleaseSteamPipe_params *)args;
    struct u_ISteamClient_SteamClient009 *iface = (struct u_ISteamClient_SteamClient009 *)params->u_iface;
    params->_ret = iface->BReleaseSteamPipe( params->hSteamPipe );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamClient_SteamClient009_BReleaseSteamPipe( void *args )
{
    struct wow64_ISteamClient_SteamClient009_BReleaseSteamPipe_params *params = (struct wow64_ISteamClient_SteamClient009_BReleaseSteamPipe_params *)args;
    struct u_ISteamClient_SteamClient009 *iface = (struct u_ISteamClient_SteamClient009 *)params->u_iface;
    params->_ret = iface->BReleaseSteamPipe( params->hSteamPipe );
    return 0;
}
#endif

NTSTATUS ISteamClient_SteamClient009_ConnectToGlobalUser( void *args )
{
    struct ISteamClient_SteamClient009_ConnectToGlobalUser_params *params = (struct ISteamClient_SteamClient009_ConnectToGlobalUser_params *)args;
    struct u_ISteamClient_SteamClient009 *iface = (struct u_ISteamClient_SteamClient009 *)params->u_iface;
    params->_ret = iface->ConnectToGlobalUser( params->hSteamPipe );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamClient_SteamClient009_ConnectToGlobalUser( void *args )
{
    struct wow64_ISteamClient_SteamClient009_ConnectToGlobalUser_params *params = (struct wow64_ISteamClient_SteamClient009_ConnectToGlobalUser_params *)args;
    struct u_ISteamClient_SteamClient009 *iface = (struct u_ISteamClient_SteamClient009 *)params->u_iface;
    params->_ret = iface->ConnectToGlobalUser( params->hSteamPipe );
    return 0;
}
#endif

NTSTATUS ISteamClient_SteamClient009_CreateLocalUser( void *args )
{
    struct ISteamClient_SteamClient009_CreateLocalUser_params *params = (struct ISteamClient_SteamClient009_CreateLocalUser_params *)args;
    struct u_ISteamClient_SteamClient009 *iface = (struct u_ISteamClient_SteamClient009 *)params->u_iface;
    params->_ret = iface->CreateLocalUser( params->phSteamPipe, params->eAccountType );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamClient_SteamClient009_CreateLocalUser( void *args )
{
    struct wow64_ISteamClient_SteamClient009_CreateLocalUser_params *params = (struct wow64_ISteamClient_SteamClient009_CreateLocalUser_params *)args;
    struct u_ISteamClient_SteamClient009 *iface = (struct u_ISteamClient_SteamClient009 *)params->u_iface;
    params->_ret = iface->CreateLocalUser( params->phSteamPipe, params->eAccountType );
    return 0;
}
#endif

NTSTATUS ISteamClient_SteamClient009_ReleaseUser( void *args )
{
    struct ISteamClient_SteamClient009_ReleaseUser_params *params = (struct ISteamClient_SteamClient009_ReleaseUser_params *)args;
    struct u_ISteamClient_SteamClient009 *iface = (struct u_ISteamClient_SteamClient009 *)params->u_iface;
    iface->ReleaseUser( params->hSteamPipe, params->hUser );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamClient_SteamClient009_ReleaseUser( void *args )
{
    struct wow64_ISteamClient_SteamClient009_ReleaseUser_params *params = (struct wow64_ISteamClient_SteamClient009_ReleaseUser_params *)args;
    struct u_ISteamClient_SteamClient009 *iface = (struct u_ISteamClient_SteamClient009 *)params->u_iface;
    iface->ReleaseUser( params->hSteamPipe, params->hUser );
    return 0;
}
#endif

NTSTATUS ISteamClient_SteamClient009_GetISteamUser( void *args )
{
    struct ISteamClient_SteamClient009_GetISteamUser_params *params = (struct ISteamClient_SteamClient009_GetISteamUser_params *)args;
    struct u_ISteamClient_SteamClient009 *iface = (struct u_ISteamClient_SteamClient009 *)params->u_iface;
    params->_ret = iface->GetISteamUser( params->hSteamUser, params->hSteamPipe, params->pchVersion );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamClient_SteamClient009_GetISteamUser( void *args )
{
    struct wow64_ISteamClient_SteamClient009_GetISteamUser_params *params = (struct wow64_ISteamClient_SteamClient009_GetISteamUser_params *)args;
    struct u_ISteamClient_SteamClient009 *iface = (struct u_ISteamClient_SteamClient009 *)params->u_iface;
    params->_ret = iface->GetISteamUser( params->hSteamUser, params->hSteamPipe, params->pchVersion );
    return 0;
}
#endif

NTSTATUS ISteamClient_SteamClient009_GetISteamGameServer( void *args )
{
    struct ISteamClient_SteamClient009_GetISteamGameServer_params *params = (struct ISteamClient_SteamClient009_GetISteamGameServer_params *)args;
    struct u_ISteamClient_SteamClient009 *iface = (struct u_ISteamClient_SteamClient009 *)params->u_iface;
    params->_ret = iface->GetISteamGameServer( params->hSteamUser, params->hSteamPipe, params->pchVersion );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamClient_SteamClient009_GetISteamGameServer( void *args )
{
    struct wow64_ISteamClient_SteamClient009_GetISteamGameServer_params *params = (struct wow64_ISteamClient_SteamClient009_GetISteamGameServer_params *)args;
    struct u_ISteamClient_SteamClient009 *iface = (struct u_ISteamClient_SteamClient009 *)params->u_iface;
    params->_ret = iface->GetISteamGameServer( params->hSteamUser, params->hSteamPipe, params->pchVersion );
    return 0;
}
#endif

NTSTATUS ISteamClient_SteamClient009_SetLocalIPBinding( void *args )
{
    struct ISteamClient_SteamClient009_SetLocalIPBinding_params *params = (struct ISteamClient_SteamClient009_SetLocalIPBinding_params *)args;
    struct u_ISteamClient_SteamClient009 *iface = (struct u_ISteamClient_SteamClient009 *)params->u_iface;
    iface->SetLocalIPBinding( params->unIP, params->usPort );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamClient_SteamClient009_SetLocalIPBinding( void *args )
{
    struct wow64_ISteamClient_SteamClient009_SetLocalIPBinding_params *params = (struct wow64_ISteamClient_SteamClient009_SetLocalIPBinding_params *)args;
    struct u_ISteamClient_SteamClient009 *iface = (struct u_ISteamClient_SteamClient009 *)params->u_iface;
    iface->SetLocalIPBinding( params->unIP, params->usPort );
    return 0;
}
#endif

NTSTATUS ISteamClient_SteamClient009_GetISteamFriends( void *args )
{
    struct ISteamClient_SteamClient009_GetISteamFriends_params *params = (struct ISteamClient_SteamClient009_GetISteamFriends_params *)args;
    struct u_ISteamClient_SteamClient009 *iface = (struct u_ISteamClient_SteamClient009 *)params->u_iface;
    params->_ret = iface->GetISteamFriends( params->hSteamUser, params->hSteamPipe, params->pchVersion );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamClient_SteamClient009_GetISteamFriends( void *args )
{
    struct wow64_ISteamClient_SteamClient009_GetISteamFriends_params *params = (struct wow64_ISteamClient_SteamClient009_GetISteamFriends_params *)args;
    struct u_ISteamClient_SteamClient009 *iface = (struct u_ISteamClient_SteamClient009 *)params->u_iface;
    params->_ret = iface->GetISteamFriends( params->hSteamUser, params->hSteamPipe, params->pchVersion );
    return 0;
}
#endif

NTSTATUS ISteamClient_SteamClient009_GetISteamUtils( void *args )
{
    struct ISteamClient_SteamClient009_GetISteamUtils_params *params = (struct ISteamClient_SteamClient009_GetISteamUtils_params *)args;
    struct u_ISteamClient_SteamClient009 *iface = (struct u_ISteamClient_SteamClient009 *)params->u_iface;
    params->_ret = iface->GetISteamUtils( params->hSteamPipe, params->pchVersion );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamClient_SteamClient009_GetISteamUtils( void *args )
{
    struct wow64_ISteamClient_SteamClient009_GetISteamUtils_params *params = (struct wow64_ISteamClient_SteamClient009_GetISteamUtils_params *)args;
    struct u_ISteamClient_SteamClient009 *iface = (struct u_ISteamClient_SteamClient009 *)params->u_iface;
    params->_ret = iface->GetISteamUtils( params->hSteamPipe, params->pchVersion );
    return 0;
}
#endif

NTSTATUS ISteamClient_SteamClient009_GetISteamMatchmaking( void *args )
{
    struct ISteamClient_SteamClient009_GetISteamMatchmaking_params *params = (struct ISteamClient_SteamClient009_GetISteamMatchmaking_params *)args;
    struct u_ISteamClient_SteamClient009 *iface = (struct u_ISteamClient_SteamClient009 *)params->u_iface;
    params->_ret = iface->GetISteamMatchmaking( params->hSteamUser, params->hSteamPipe, params->pchVersion );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamClient_SteamClient009_GetISteamMatchmaking( void *args )
{
    struct wow64_ISteamClient_SteamClient009_GetISteamMatchmaking_params *params = (struct wow64_ISteamClient_SteamClient009_GetISteamMatchmaking_params *)args;
    struct u_ISteamClient_SteamClient009 *iface = (struct u_ISteamClient_SteamClient009 *)params->u_iface;
    params->_ret = iface->GetISteamMatchmaking( params->hSteamUser, params->hSteamPipe, params->pchVersion );
    return 0;
}
#endif

NTSTATUS ISteamClient_SteamClient009_GetISteamMasterServerUpdater( void *args )
{
    struct ISteamClient_SteamClient009_GetISteamMasterServerUpdater_params *params = (struct ISteamClient_SteamClient009_GetISteamMasterServerUpdater_params *)args;
    struct u_ISteamClient_SteamClient009 *iface = (struct u_ISteamClient_SteamClient009 *)params->u_iface;
    params->_ret = iface->GetISteamMasterServerUpdater( params->hSteamUser, params->hSteamPipe, params->pchVersion );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamClient_SteamClient009_GetISteamMasterServerUpdater( void *args )
{
    struct wow64_ISteamClient_SteamClient009_GetISteamMasterServerUpdater_params *params = (struct wow64_ISteamClient_SteamClient009_GetISteamMasterServerUpdater_params *)args;
    struct u_ISteamClient_SteamClient009 *iface = (struct u_ISteamClient_SteamClient009 *)params->u_iface;
    params->_ret = iface->GetISteamMasterServerUpdater( params->hSteamUser, params->hSteamPipe, params->pchVersion );
    return 0;
}
#endif

NTSTATUS ISteamClient_SteamClient009_GetISteamMatchmakingServers( void *args )
{
    struct ISteamClient_SteamClient009_GetISteamMatchmakingServers_params *params = (struct ISteamClient_SteamClient009_GetISteamMatchmakingServers_params *)args;
    struct u_ISteamClient_SteamClient009 *iface = (struct u_ISteamClient_SteamClient009 *)params->u_iface;
    params->_ret = iface->GetISteamMatchmakingServers( params->hSteamUser, params->hSteamPipe, params->pchVersion );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamClient_SteamClient009_GetISteamMatchmakingServers( void *args )
{
    struct wow64_ISteamClient_SteamClient009_GetISteamMatchmakingServers_params *params = (struct wow64_ISteamClient_SteamClient009_GetISteamMatchmakingServers_params *)args;
    struct u_ISteamClient_SteamClient009 *iface = (struct u_ISteamClient_SteamClient009 *)params->u_iface;
    params->_ret = iface->GetISteamMatchmakingServers( params->hSteamUser, params->hSteamPipe, params->pchVersion );
    return 0;
}
#endif

NTSTATUS ISteamClient_SteamClient009_GetISteamGenericInterface( void *args )
{
    struct ISteamClient_SteamClient009_GetISteamGenericInterface_params *params = (struct ISteamClient_SteamClient009_GetISteamGenericInterface_params *)args;
    struct u_ISteamClient_SteamClient009 *iface = (struct u_ISteamClient_SteamClient009 *)params->u_iface;
    params->_ret = iface->GetISteamGenericInterface( params->hSteamUser, params->hSteamPipe, params->pchVersion );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamClient_SteamClient009_GetISteamGenericInterface( void *args )
{
    struct wow64_ISteamClient_SteamClient009_GetISteamGenericInterface_params *params = (struct wow64_ISteamClient_SteamClient009_GetISteamGenericInterface_params *)args;
    struct u_ISteamClient_SteamClient009 *iface = (struct u_ISteamClient_SteamClient009 *)params->u_iface;
    params->_ret = iface->GetISteamGenericInterface( params->hSteamUser, params->hSteamPipe, params->pchVersion );
    return 0;
}
#endif

NTSTATUS ISteamClient_SteamClient009_GetISteamUserStats( void *args )
{
    struct ISteamClient_SteamClient009_GetISteamUserStats_params *params = (struct ISteamClient_SteamClient009_GetISteamUserStats_params *)args;
    struct u_ISteamClient_SteamClient009 *iface = (struct u_ISteamClient_SteamClient009 *)params->u_iface;
    params->_ret = iface->GetISteamUserStats( params->hSteamUser, params->hSteamPipe, params->pchVersion );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamClient_SteamClient009_GetISteamUserStats( void *args )
{
    struct wow64_ISteamClient_SteamClient009_GetISteamUserStats_params *params = (struct wow64_ISteamClient_SteamClient009_GetISteamUserStats_params *)args;
    struct u_ISteamClient_SteamClient009 *iface = (struct u_ISteamClient_SteamClient009 *)params->u_iface;
    params->_ret = iface->GetISteamUserStats( params->hSteamUser, params->hSteamPipe, params->pchVersion );
    return 0;
}
#endif

NTSTATUS ISteamClient_SteamClient009_GetISteamGameServerStats( void *args )
{
    struct ISteamClient_SteamClient009_GetISteamGameServerStats_params *params = (struct ISteamClient_SteamClient009_GetISteamGameServerStats_params *)args;
    struct u_ISteamClient_SteamClient009 *iface = (struct u_ISteamClient_SteamClient009 *)params->u_iface;
    params->_ret = iface->GetISteamGameServerStats( params->hSteamuser, params->hSteamPipe, params->pchVersion );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamClient_SteamClient009_GetISteamGameServerStats( void *args )
{
    struct wow64_ISteamClient_SteamClient009_GetISteamGameServerStats_params *params = (struct wow64_ISteamClient_SteamClient009_GetISteamGameServerStats_params *)args;
    struct u_ISteamClient_SteamClient009 *iface = (struct u_ISteamClient_SteamClient009 *)params->u_iface;
    params->_ret = iface->GetISteamGameServerStats( params->hSteamuser, params->hSteamPipe, params->pchVersion );
    return 0;
}
#endif

NTSTATUS ISteamClient_SteamClient009_GetISteamApps( void *args )
{
    struct ISteamClient_SteamClient009_GetISteamApps_params *params = (struct ISteamClient_SteamClient009_GetISteamApps_params *)args;
    struct u_ISteamClient_SteamClient009 *iface = (struct u_ISteamClient_SteamClient009 *)params->u_iface;
    params->_ret = iface->GetISteamApps( params->hSteamUser, params->hSteamPipe, params->pchVersion );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamClient_SteamClient009_GetISteamApps( void *args )
{
    struct wow64_ISteamClient_SteamClient009_GetISteamApps_params *params = (struct wow64_ISteamClient_SteamClient009_GetISteamApps_params *)args;
    struct u_ISteamClient_SteamClient009 *iface = (struct u_ISteamClient_SteamClient009 *)params->u_iface;
    params->_ret = iface->GetISteamApps( params->hSteamUser, params->hSteamPipe, params->pchVersion );
    return 0;
}
#endif

NTSTATUS ISteamClient_SteamClient009_GetISteamNetworking( void *args )
{
    struct ISteamClient_SteamClient009_GetISteamNetworking_params *params = (struct ISteamClient_SteamClient009_GetISteamNetworking_params *)args;
    struct u_ISteamClient_SteamClient009 *iface = (struct u_ISteamClient_SteamClient009 *)params->u_iface;
    params->_ret = iface->GetISteamNetworking( params->hSteamUser, params->hSteamPipe, params->pchVersion );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamClient_SteamClient009_GetISteamNetworking( void *args )
{
    struct wow64_ISteamClient_SteamClient009_GetISteamNetworking_params *params = (struct wow64_ISteamClient_SteamClient009_GetISteamNetworking_params *)args;
    struct u_ISteamClient_SteamClient009 *iface = (struct u_ISteamClient_SteamClient009 *)params->u_iface;
    params->_ret = iface->GetISteamNetworking( params->hSteamUser, params->hSteamPipe, params->pchVersion );
    return 0;
}
#endif

NTSTATUS ISteamClient_SteamClient009_GetISteamRemoteStorage( void *args )
{
    struct ISteamClient_SteamClient009_GetISteamRemoteStorage_params *params = (struct ISteamClient_SteamClient009_GetISteamRemoteStorage_params *)args;
    struct u_ISteamClient_SteamClient009 *iface = (struct u_ISteamClient_SteamClient009 *)params->u_iface;
    params->_ret = iface->GetISteamRemoteStorage( params->hSteamuser, params->hSteamPipe, params->pchVersion );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamClient_SteamClient009_GetISteamRemoteStorage( void *args )
{
    struct wow64_ISteamClient_SteamClient009_GetISteamRemoteStorage_params *params = (struct wow64_ISteamClient_SteamClient009_GetISteamRemoteStorage_params *)args;
    struct u_ISteamClient_SteamClient009 *iface = (struct u_ISteamClient_SteamClient009 *)params->u_iface;
    params->_ret = iface->GetISteamRemoteStorage( params->hSteamuser, params->hSteamPipe, params->pchVersion );
    return 0;
}
#endif

NTSTATUS ISteamClient_SteamClient009_RunFrame( void *args )
{
    struct ISteamClient_SteamClient009_RunFrame_params *params = (struct ISteamClient_SteamClient009_RunFrame_params *)args;
    struct u_ISteamClient_SteamClient009 *iface = (struct u_ISteamClient_SteamClient009 *)params->u_iface;
    iface->RunFrame(  );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamClient_SteamClient009_RunFrame( void *args )
{
    struct wow64_ISteamClient_SteamClient009_RunFrame_params *params = (struct wow64_ISteamClient_SteamClient009_RunFrame_params *)args;
    struct u_ISteamClient_SteamClient009 *iface = (struct u_ISteamClient_SteamClient009 *)params->u_iface;
    iface->RunFrame(  );
    return 0;
}
#endif

NTSTATUS ISteamClient_SteamClient009_GetIPCCallCount( void *args )
{
    struct ISteamClient_SteamClient009_GetIPCCallCount_params *params = (struct ISteamClient_SteamClient009_GetIPCCallCount_params *)args;
    struct u_ISteamClient_SteamClient009 *iface = (struct u_ISteamClient_SteamClient009 *)params->u_iface;
    params->_ret = iface->GetIPCCallCount(  );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamClient_SteamClient009_GetIPCCallCount( void *args )
{
    struct wow64_ISteamClient_SteamClient009_GetIPCCallCount_params *params = (struct wow64_ISteamClient_SteamClient009_GetIPCCallCount_params *)args;
    struct u_ISteamClient_SteamClient009 *iface = (struct u_ISteamClient_SteamClient009 *)params->u_iface;
    params->_ret = iface->GetIPCCallCount(  );
    return 0;
}
#endif

NTSTATUS ISteamClient_SteamClient009_SetWarningMessageHook( void *args )
{
    struct ISteamClient_SteamClient009_SetWarningMessageHook_params *params = (struct ISteamClient_SteamClient009_SetWarningMessageHook_params *)args;
    struct u_ISteamClient_SteamClient009 *iface = (struct u_ISteamClient_SteamClient009 *)params->u_iface;
    void (*U_CDECL u_pFunction)(int32_t, const char *) = manual_convert_SetWarningMessageHook_pFunction( params->pFunction );
    iface->SetWarningMessageHook( u_pFunction );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamClient_SteamClient009_SetWarningMessageHook( void *args )
{
    struct wow64_ISteamClient_SteamClient009_SetWarningMessageHook_params *params = (struct wow64_ISteamClient_SteamClient009_SetWarningMessageHook_params *)args;
    struct u_ISteamClient_SteamClient009 *iface = (struct u_ISteamClient_SteamClient009 *)params->u_iface;
    void (*U_CDECL u_pFunction)(int32_t, const char *) = manual_convert_SetWarningMessageHook_pFunction( params->pFunction );
    iface->SetWarningMessageHook( u_pFunction );
    return 0;
}
#endif

