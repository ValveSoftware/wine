#include "unix_private.h"

#if 0
#pragma makedep unix
#endif

WINE_DEFAULT_DEBUG_CHANNEL(steamclient);

#if defined(__x86_64__) || defined(__aarch64__)
w32_RemoteStorageUpdatePublishedFileRequest_t::operator u64_RemoteStorageUpdatePublishedFileRequest_t() const
{
    u64_RemoteStorageUpdatePublishedFileRequest_t ret;
    ret.m_unPublishedFileId = this->m_unPublishedFileId;
    ret.m_pchFile = this->m_pchFile;
    ret.m_pchPreviewFile = this->m_pchPreviewFile;
    ret.m_pchTitle = this->m_pchTitle;
    ret.m_pchDescription = this->m_pchDescription;
    ret.m_eVisibility = this->m_eVisibility;
    ret.m_bUpdateFile = this->m_bUpdateFile;
    ret.m_bUpdatePreviewFile = this->m_bUpdatePreviewFile;
    ret.m_bUpdateTitle = this->m_bUpdateTitle;
    ret.m_bUpdateDescription = this->m_bUpdateDescription;
    ret.m_bUpdateVisibility = this->m_bUpdateVisibility;
    ret.m_bUpdateTags = this->m_bUpdateTags;
    return ret;
}
#endif

template< typename Iface, typename Params >
static NTSTATUS ISteamRemoteStorage_UpdatePublishedFile( Iface *iface, Params *params, bool wow64 )
{
    u_RemoteStorageUpdatePublishedFileRequest_t u_updatePublishedFileRequest = params->updatePublishedFileRequest;
    u_updatePublishedFileRequest.m_pTags = new u_SteamParamStringArray_t( *params->updatePublishedFileRequest.m_pTags );
    params->_ret = iface->UpdatePublishedFile( u_updatePublishedFileRequest );
    delete u_updatePublishedFileRequest.m_pTags;
    return 0;
}

LSTEAMCLIENT_UNIX_IMPL( ISteamRemoteStorage, STEAMREMOTESTORAGE_INTERFACE_VERSION005, UpdatePublishedFile );
