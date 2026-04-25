/* This file is auto-generated, do not edit. */
#include "unix_private.h"

#if 0
#pragma makedep unix
#endif

NTSTATUS ISteamNetworkingSocketsSerialized_SteamNetworkingSocketsSerialized002_SendP2PRendezvous( void *args )
{
    struct ISteamNetworkingSocketsSerialized_SteamNetworkingSocketsSerialized002_SendP2PRendezvous_params *params = (struct ISteamNetworkingSocketsSerialized_SteamNetworkingSocketsSerialized002_SendP2PRendezvous_params *)args;
    struct u_ISteamNetworkingSocketsSerialized_SteamNetworkingSocketsSerialized002 *iface = (struct u_ISteamNetworkingSocketsSerialized_SteamNetworkingSocketsSerialized002 *)params->u_iface;
    iface->SendP2PRendezvous( params->steamIDRemote, params->unConnectionIDSrc, params->pMsgRendezvous, params->cbRendezvous );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamNetworkingSocketsSerialized_SteamNetworkingSocketsSerialized002_SendP2PRendezvous( void *args )
{
    struct wow64_ISteamNetworkingSocketsSerialized_SteamNetworkingSocketsSerialized002_SendP2PRendezvous_params *params = (struct wow64_ISteamNetworkingSocketsSerialized_SteamNetworkingSocketsSerialized002_SendP2PRendezvous_params *)args;
    struct u_ISteamNetworkingSocketsSerialized_SteamNetworkingSocketsSerialized002 *iface = (struct u_ISteamNetworkingSocketsSerialized_SteamNetworkingSocketsSerialized002 *)params->u_iface;
    iface->SendP2PRendezvous( params->steamIDRemote, params->unConnectionIDSrc, params->pMsgRendezvous, params->cbRendezvous );
    return 0;
}
#endif

NTSTATUS ISteamNetworkingSocketsSerialized_SteamNetworkingSocketsSerialized002_SendP2PConnectionFailure( void *args )
{
    struct ISteamNetworkingSocketsSerialized_SteamNetworkingSocketsSerialized002_SendP2PConnectionFailure_params *params = (struct ISteamNetworkingSocketsSerialized_SteamNetworkingSocketsSerialized002_SendP2PConnectionFailure_params *)args;
    struct u_ISteamNetworkingSocketsSerialized_SteamNetworkingSocketsSerialized002 *iface = (struct u_ISteamNetworkingSocketsSerialized_SteamNetworkingSocketsSerialized002 *)params->u_iface;
    iface->SendP2PConnectionFailure( params->steamIDRemote, params->unConnectionIDDest, params->nReason, params->pszReason );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamNetworkingSocketsSerialized_SteamNetworkingSocketsSerialized002_SendP2PConnectionFailure( void *args )
{
    struct wow64_ISteamNetworkingSocketsSerialized_SteamNetworkingSocketsSerialized002_SendP2PConnectionFailure_params *params = (struct wow64_ISteamNetworkingSocketsSerialized_SteamNetworkingSocketsSerialized002_SendP2PConnectionFailure_params *)args;
    struct u_ISteamNetworkingSocketsSerialized_SteamNetworkingSocketsSerialized002 *iface = (struct u_ISteamNetworkingSocketsSerialized_SteamNetworkingSocketsSerialized002 *)params->u_iface;
    iface->SendP2PConnectionFailure( params->steamIDRemote, params->unConnectionIDDest, params->nReason, params->pszReason );
    return 0;
}
#endif

NTSTATUS ISteamNetworkingSocketsSerialized_SteamNetworkingSocketsSerialized002_GetCertAsync( void *args )
{
    struct ISteamNetworkingSocketsSerialized_SteamNetworkingSocketsSerialized002_GetCertAsync_params *params = (struct ISteamNetworkingSocketsSerialized_SteamNetworkingSocketsSerialized002_GetCertAsync_params *)args;
    struct u_ISteamNetworkingSocketsSerialized_SteamNetworkingSocketsSerialized002 *iface = (struct u_ISteamNetworkingSocketsSerialized_SteamNetworkingSocketsSerialized002 *)params->u_iface;
    params->_ret = iface->GetCertAsync(  );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamNetworkingSocketsSerialized_SteamNetworkingSocketsSerialized002_GetCertAsync( void *args )
{
    struct wow64_ISteamNetworkingSocketsSerialized_SteamNetworkingSocketsSerialized002_GetCertAsync_params *params = (struct wow64_ISteamNetworkingSocketsSerialized_SteamNetworkingSocketsSerialized002_GetCertAsync_params *)args;
    struct u_ISteamNetworkingSocketsSerialized_SteamNetworkingSocketsSerialized002 *iface = (struct u_ISteamNetworkingSocketsSerialized_SteamNetworkingSocketsSerialized002 *)params->u_iface;
    params->_ret = iface->GetCertAsync(  );
    return 0;
}
#endif

NTSTATUS ISteamNetworkingSocketsSerialized_SteamNetworkingSocketsSerialized002_GetNetworkConfigJSON( void *args )
{
    struct ISteamNetworkingSocketsSerialized_SteamNetworkingSocketsSerialized002_GetNetworkConfigJSON_params *params = (struct ISteamNetworkingSocketsSerialized_SteamNetworkingSocketsSerialized002_GetNetworkConfigJSON_params *)args;
    struct u_ISteamNetworkingSocketsSerialized_SteamNetworkingSocketsSerialized002 *iface = (struct u_ISteamNetworkingSocketsSerialized_SteamNetworkingSocketsSerialized002 *)params->u_iface;
    params->_ret = iface->GetNetworkConfigJSON( params->buf, params->cbBuf );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamNetworkingSocketsSerialized_SteamNetworkingSocketsSerialized002_GetNetworkConfigJSON( void *args )
{
    struct wow64_ISteamNetworkingSocketsSerialized_SteamNetworkingSocketsSerialized002_GetNetworkConfigJSON_params *params = (struct wow64_ISteamNetworkingSocketsSerialized_SteamNetworkingSocketsSerialized002_GetNetworkConfigJSON_params *)args;
    struct u_ISteamNetworkingSocketsSerialized_SteamNetworkingSocketsSerialized002 *iface = (struct u_ISteamNetworkingSocketsSerialized_SteamNetworkingSocketsSerialized002 *)params->u_iface;
    params->_ret = iface->GetNetworkConfigJSON( params->buf, params->cbBuf );
    return 0;
}
#endif

NTSTATUS ISteamNetworkingSocketsSerialized_SteamNetworkingSocketsSerialized002_CacheRelayTicket( void *args )
{
    struct ISteamNetworkingSocketsSerialized_SteamNetworkingSocketsSerialized002_CacheRelayTicket_params *params = (struct ISteamNetworkingSocketsSerialized_SteamNetworkingSocketsSerialized002_CacheRelayTicket_params *)args;
    struct u_ISteamNetworkingSocketsSerialized_SteamNetworkingSocketsSerialized002 *iface = (struct u_ISteamNetworkingSocketsSerialized_SteamNetworkingSocketsSerialized002 *)params->u_iface;
    iface->CacheRelayTicket( params->pTicket, params->cbTicket );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamNetworkingSocketsSerialized_SteamNetworkingSocketsSerialized002_CacheRelayTicket( void *args )
{
    struct wow64_ISteamNetworkingSocketsSerialized_SteamNetworkingSocketsSerialized002_CacheRelayTicket_params *params = (struct wow64_ISteamNetworkingSocketsSerialized_SteamNetworkingSocketsSerialized002_CacheRelayTicket_params *)args;
    struct u_ISteamNetworkingSocketsSerialized_SteamNetworkingSocketsSerialized002 *iface = (struct u_ISteamNetworkingSocketsSerialized_SteamNetworkingSocketsSerialized002 *)params->u_iface;
    iface->CacheRelayTicket( params->pTicket, params->cbTicket );
    return 0;
}
#endif

NTSTATUS ISteamNetworkingSocketsSerialized_SteamNetworkingSocketsSerialized002_GetCachedRelayTicketCount( void *args )
{
    struct ISteamNetworkingSocketsSerialized_SteamNetworkingSocketsSerialized002_GetCachedRelayTicketCount_params *params = (struct ISteamNetworkingSocketsSerialized_SteamNetworkingSocketsSerialized002_GetCachedRelayTicketCount_params *)args;
    struct u_ISteamNetworkingSocketsSerialized_SteamNetworkingSocketsSerialized002 *iface = (struct u_ISteamNetworkingSocketsSerialized_SteamNetworkingSocketsSerialized002 *)params->u_iface;
    params->_ret = iface->GetCachedRelayTicketCount(  );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamNetworkingSocketsSerialized_SteamNetworkingSocketsSerialized002_GetCachedRelayTicketCount( void *args )
{
    struct wow64_ISteamNetworkingSocketsSerialized_SteamNetworkingSocketsSerialized002_GetCachedRelayTicketCount_params *params = (struct wow64_ISteamNetworkingSocketsSerialized_SteamNetworkingSocketsSerialized002_GetCachedRelayTicketCount_params *)args;
    struct u_ISteamNetworkingSocketsSerialized_SteamNetworkingSocketsSerialized002 *iface = (struct u_ISteamNetworkingSocketsSerialized_SteamNetworkingSocketsSerialized002 *)params->u_iface;
    params->_ret = iface->GetCachedRelayTicketCount(  );
    return 0;
}
#endif

NTSTATUS ISteamNetworkingSocketsSerialized_SteamNetworkingSocketsSerialized002_GetCachedRelayTicket( void *args )
{
    struct ISteamNetworkingSocketsSerialized_SteamNetworkingSocketsSerialized002_GetCachedRelayTicket_params *params = (struct ISteamNetworkingSocketsSerialized_SteamNetworkingSocketsSerialized002_GetCachedRelayTicket_params *)args;
    struct u_ISteamNetworkingSocketsSerialized_SteamNetworkingSocketsSerialized002 *iface = (struct u_ISteamNetworkingSocketsSerialized_SteamNetworkingSocketsSerialized002 *)params->u_iface;
    params->_ret = iface->GetCachedRelayTicket( params->idxTicket, params->buf, params->cbBuf );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamNetworkingSocketsSerialized_SteamNetworkingSocketsSerialized002_GetCachedRelayTicket( void *args )
{
    struct wow64_ISteamNetworkingSocketsSerialized_SteamNetworkingSocketsSerialized002_GetCachedRelayTicket_params *params = (struct wow64_ISteamNetworkingSocketsSerialized_SteamNetworkingSocketsSerialized002_GetCachedRelayTicket_params *)args;
    struct u_ISteamNetworkingSocketsSerialized_SteamNetworkingSocketsSerialized002 *iface = (struct u_ISteamNetworkingSocketsSerialized_SteamNetworkingSocketsSerialized002 *)params->u_iface;
    params->_ret = iface->GetCachedRelayTicket( params->idxTicket, params->buf, params->cbBuf );
    return 0;
}
#endif

NTSTATUS ISteamNetworkingSocketsSerialized_SteamNetworkingSocketsSerialized002_PostConnectionStateMsg( void *args )
{
    struct ISteamNetworkingSocketsSerialized_SteamNetworkingSocketsSerialized002_PostConnectionStateMsg_params *params = (struct ISteamNetworkingSocketsSerialized_SteamNetworkingSocketsSerialized002_PostConnectionStateMsg_params *)args;
    struct u_ISteamNetworkingSocketsSerialized_SteamNetworkingSocketsSerialized002 *iface = (struct u_ISteamNetworkingSocketsSerialized_SteamNetworkingSocketsSerialized002 *)params->u_iface;
    iface->PostConnectionStateMsg( params->pMsg, params->cbMsg );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamNetworkingSocketsSerialized_SteamNetworkingSocketsSerialized002_PostConnectionStateMsg( void *args )
{
    struct wow64_ISteamNetworkingSocketsSerialized_SteamNetworkingSocketsSerialized002_PostConnectionStateMsg_params *params = (struct wow64_ISteamNetworkingSocketsSerialized_SteamNetworkingSocketsSerialized002_PostConnectionStateMsg_params *)args;
    struct u_ISteamNetworkingSocketsSerialized_SteamNetworkingSocketsSerialized002 *iface = (struct u_ISteamNetworkingSocketsSerialized_SteamNetworkingSocketsSerialized002 *)params->u_iface;
    iface->PostConnectionStateMsg( params->pMsg, params->cbMsg );
    return 0;
}
#endif

