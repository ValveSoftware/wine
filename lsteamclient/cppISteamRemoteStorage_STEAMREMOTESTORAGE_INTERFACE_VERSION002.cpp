/* This file is auto-generated, do not edit. */
#include "unix_private.h"

#if 0
#pragma makedep unix
#endif

NTSTATUS ISteamRemoteStorage_STEAMREMOTESTORAGE_INTERFACE_VERSION002_FileWrite( void *args )
{
    struct ISteamRemoteStorage_STEAMREMOTESTORAGE_INTERFACE_VERSION002_FileWrite_params *params = (struct ISteamRemoteStorage_STEAMREMOTESTORAGE_INTERFACE_VERSION002_FileWrite_params *)args;
    struct u_ISteamRemoteStorage_STEAMREMOTESTORAGE_INTERFACE_VERSION002 *iface = (struct u_ISteamRemoteStorage_STEAMREMOTESTORAGE_INTERFACE_VERSION002 *)params->u_iface;
    params->_ret = iface->FileWrite( params->pchFile, params->pvData, params->cubData );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamRemoteStorage_STEAMREMOTESTORAGE_INTERFACE_VERSION002_FileWrite( void *args )
{
    struct wow64_ISteamRemoteStorage_STEAMREMOTESTORAGE_INTERFACE_VERSION002_FileWrite_params *params = (struct wow64_ISteamRemoteStorage_STEAMREMOTESTORAGE_INTERFACE_VERSION002_FileWrite_params *)args;
    struct u_ISteamRemoteStorage_STEAMREMOTESTORAGE_INTERFACE_VERSION002 *iface = (struct u_ISteamRemoteStorage_STEAMREMOTESTORAGE_INTERFACE_VERSION002 *)params->u_iface;
    params->_ret = iface->FileWrite( params->pchFile, params->pvData, params->cubData );
    return 0;
}
#endif

NTSTATUS ISteamRemoteStorage_STEAMREMOTESTORAGE_INTERFACE_VERSION002_GetFileSize( void *args )
{
    struct ISteamRemoteStorage_STEAMREMOTESTORAGE_INTERFACE_VERSION002_GetFileSize_params *params = (struct ISteamRemoteStorage_STEAMREMOTESTORAGE_INTERFACE_VERSION002_GetFileSize_params *)args;
    struct u_ISteamRemoteStorage_STEAMREMOTESTORAGE_INTERFACE_VERSION002 *iface = (struct u_ISteamRemoteStorage_STEAMREMOTESTORAGE_INTERFACE_VERSION002 *)params->u_iface;
    params->_ret = iface->GetFileSize( params->pchFile );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamRemoteStorage_STEAMREMOTESTORAGE_INTERFACE_VERSION002_GetFileSize( void *args )
{
    struct wow64_ISteamRemoteStorage_STEAMREMOTESTORAGE_INTERFACE_VERSION002_GetFileSize_params *params = (struct wow64_ISteamRemoteStorage_STEAMREMOTESTORAGE_INTERFACE_VERSION002_GetFileSize_params *)args;
    struct u_ISteamRemoteStorage_STEAMREMOTESTORAGE_INTERFACE_VERSION002 *iface = (struct u_ISteamRemoteStorage_STEAMREMOTESTORAGE_INTERFACE_VERSION002 *)params->u_iface;
    params->_ret = iface->GetFileSize( params->pchFile );
    return 0;
}
#endif

NTSTATUS ISteamRemoteStorage_STEAMREMOTESTORAGE_INTERFACE_VERSION002_FileRead( void *args )
{
    struct ISteamRemoteStorage_STEAMREMOTESTORAGE_INTERFACE_VERSION002_FileRead_params *params = (struct ISteamRemoteStorage_STEAMREMOTESTORAGE_INTERFACE_VERSION002_FileRead_params *)args;
    struct u_ISteamRemoteStorage_STEAMREMOTESTORAGE_INTERFACE_VERSION002 *iface = (struct u_ISteamRemoteStorage_STEAMREMOTESTORAGE_INTERFACE_VERSION002 *)params->u_iface;
    params->_ret = iface->FileRead( params->pchFile, params->pvData, params->cubDataToRead );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamRemoteStorage_STEAMREMOTESTORAGE_INTERFACE_VERSION002_FileRead( void *args )
{
    struct wow64_ISteamRemoteStorage_STEAMREMOTESTORAGE_INTERFACE_VERSION002_FileRead_params *params = (struct wow64_ISteamRemoteStorage_STEAMREMOTESTORAGE_INTERFACE_VERSION002_FileRead_params *)args;
    struct u_ISteamRemoteStorage_STEAMREMOTESTORAGE_INTERFACE_VERSION002 *iface = (struct u_ISteamRemoteStorage_STEAMREMOTESTORAGE_INTERFACE_VERSION002 *)params->u_iface;
    params->_ret = iface->FileRead( params->pchFile, params->pvData, params->cubDataToRead );
    return 0;
}
#endif

NTSTATUS ISteamRemoteStorage_STEAMREMOTESTORAGE_INTERFACE_VERSION002_FileExists( void *args )
{
    struct ISteamRemoteStorage_STEAMREMOTESTORAGE_INTERFACE_VERSION002_FileExists_params *params = (struct ISteamRemoteStorage_STEAMREMOTESTORAGE_INTERFACE_VERSION002_FileExists_params *)args;
    struct u_ISteamRemoteStorage_STEAMREMOTESTORAGE_INTERFACE_VERSION002 *iface = (struct u_ISteamRemoteStorage_STEAMREMOTESTORAGE_INTERFACE_VERSION002 *)params->u_iface;
    params->_ret = iface->FileExists( params->pchFile );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamRemoteStorage_STEAMREMOTESTORAGE_INTERFACE_VERSION002_FileExists( void *args )
{
    struct wow64_ISteamRemoteStorage_STEAMREMOTESTORAGE_INTERFACE_VERSION002_FileExists_params *params = (struct wow64_ISteamRemoteStorage_STEAMREMOTESTORAGE_INTERFACE_VERSION002_FileExists_params *)args;
    struct u_ISteamRemoteStorage_STEAMREMOTESTORAGE_INTERFACE_VERSION002 *iface = (struct u_ISteamRemoteStorage_STEAMREMOTESTORAGE_INTERFACE_VERSION002 *)params->u_iface;
    params->_ret = iface->FileExists( params->pchFile );
    return 0;
}
#endif

NTSTATUS ISteamRemoteStorage_STEAMREMOTESTORAGE_INTERFACE_VERSION002_GetFileCount( void *args )
{
    struct ISteamRemoteStorage_STEAMREMOTESTORAGE_INTERFACE_VERSION002_GetFileCount_params *params = (struct ISteamRemoteStorage_STEAMREMOTESTORAGE_INTERFACE_VERSION002_GetFileCount_params *)args;
    struct u_ISteamRemoteStorage_STEAMREMOTESTORAGE_INTERFACE_VERSION002 *iface = (struct u_ISteamRemoteStorage_STEAMREMOTESTORAGE_INTERFACE_VERSION002 *)params->u_iface;
    params->_ret = iface->GetFileCount(  );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamRemoteStorage_STEAMREMOTESTORAGE_INTERFACE_VERSION002_GetFileCount( void *args )
{
    struct wow64_ISteamRemoteStorage_STEAMREMOTESTORAGE_INTERFACE_VERSION002_GetFileCount_params *params = (struct wow64_ISteamRemoteStorage_STEAMREMOTESTORAGE_INTERFACE_VERSION002_GetFileCount_params *)args;
    struct u_ISteamRemoteStorage_STEAMREMOTESTORAGE_INTERFACE_VERSION002 *iface = (struct u_ISteamRemoteStorage_STEAMREMOTESTORAGE_INTERFACE_VERSION002 *)params->u_iface;
    params->_ret = iface->GetFileCount(  );
    return 0;
}
#endif

NTSTATUS ISteamRemoteStorage_STEAMREMOTESTORAGE_INTERFACE_VERSION002_GetFileNameAndSize( void *args )
{
    struct ISteamRemoteStorage_STEAMREMOTESTORAGE_INTERFACE_VERSION002_GetFileNameAndSize_params *params = (struct ISteamRemoteStorage_STEAMREMOTESTORAGE_INTERFACE_VERSION002_GetFileNameAndSize_params *)args;
    struct u_ISteamRemoteStorage_STEAMREMOTESTORAGE_INTERFACE_VERSION002 *iface = (struct u_ISteamRemoteStorage_STEAMREMOTESTORAGE_INTERFACE_VERSION002 *)params->u_iface;
    params->_ret = iface->GetFileNameAndSize( params->iFile, params->pnFileSizeInBytes );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamRemoteStorage_STEAMREMOTESTORAGE_INTERFACE_VERSION002_GetFileNameAndSize( void *args )
{
    struct wow64_ISteamRemoteStorage_STEAMREMOTESTORAGE_INTERFACE_VERSION002_GetFileNameAndSize_params *params = (struct wow64_ISteamRemoteStorage_STEAMREMOTESTORAGE_INTERFACE_VERSION002_GetFileNameAndSize_params *)args;
    struct u_ISteamRemoteStorage_STEAMREMOTESTORAGE_INTERFACE_VERSION002 *iface = (struct u_ISteamRemoteStorage_STEAMREMOTESTORAGE_INTERFACE_VERSION002 *)params->u_iface;
    params->_ret = iface->GetFileNameAndSize( params->iFile, params->pnFileSizeInBytes );
    return 0;
}
#endif

NTSTATUS ISteamRemoteStorage_STEAMREMOTESTORAGE_INTERFACE_VERSION002_GetQuota( void *args )
{
    struct ISteamRemoteStorage_STEAMREMOTESTORAGE_INTERFACE_VERSION002_GetQuota_params *params = (struct ISteamRemoteStorage_STEAMREMOTESTORAGE_INTERFACE_VERSION002_GetQuota_params *)args;
    struct u_ISteamRemoteStorage_STEAMREMOTESTORAGE_INTERFACE_VERSION002 *iface = (struct u_ISteamRemoteStorage_STEAMREMOTESTORAGE_INTERFACE_VERSION002 *)params->u_iface;
    params->_ret = iface->GetQuota( params->pnTotalBytes, params->puAvailableBytes );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamRemoteStorage_STEAMREMOTESTORAGE_INTERFACE_VERSION002_GetQuota( void *args )
{
    struct wow64_ISteamRemoteStorage_STEAMREMOTESTORAGE_INTERFACE_VERSION002_GetQuota_params *params = (struct wow64_ISteamRemoteStorage_STEAMREMOTESTORAGE_INTERFACE_VERSION002_GetQuota_params *)args;
    struct u_ISteamRemoteStorage_STEAMREMOTESTORAGE_INTERFACE_VERSION002 *iface = (struct u_ISteamRemoteStorage_STEAMREMOTESTORAGE_INTERFACE_VERSION002 *)params->u_iface;
    params->_ret = iface->GetQuota( params->pnTotalBytes, params->puAvailableBytes );
    return 0;
}
#endif

