/* This file is auto-generated, do not edit. */
#include "steamclient_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(steamclient);

DEFINE_THISCALL_WRAPPER(winISteamAppTicket_STEAMAPPTICKET_INTERFACE_VERSION001_GetAppOwnershipTicketData, 32)

uint32_t __thiscall winISteamAppTicket_STEAMAPPTICKET_INTERFACE_VERSION001_GetAppOwnershipTicketData(struct w_iface *_this, uint32_t nAppID, void *pvBuffer, uint32_t cbBufferLength, uint32_t *piAppId, uint32_t *piSteamId, uint32_t *piSignature, uint32_t *pcbSignature)
{
    struct ISteamAppTicket_STEAMAPPTICKET_INTERFACE_VERSION001_GetAppOwnershipTicketData_params params =
    {
        .u_iface = _this->u_iface,
        .nAppID = nAppID,
        .pvBuffer = pvBuffer,
        .cbBufferLength = cbBufferLength,
        .piAppId = piAppId,
        .piSteamId = piSteamId,
        .piSignature = piSignature,
        .pcbSignature = pcbSignature,
    };
    TRACE("%p\n", _this);
    STEAMCLIENT_CALL( ISteamAppTicket_STEAMAPPTICKET_INTERFACE_VERSION001_GetAppOwnershipTicketData, &params );
    return params._ret;
}

extern vtable_ptr winISteamAppTicket_STEAMAPPTICKET_INTERFACE_VERSION001_vtable;

DEFINE_RTTI_DATA0(winISteamAppTicket_STEAMAPPTICKET_INTERFACE_VERSION001, 0, ".?AVISteamAppTicket@@")

__ASM_BLOCK_BEGIN(winISteamAppTicket_STEAMAPPTICKET_INTERFACE_VERSION001_vtables)
    __ASM_VTABLE(winISteamAppTicket_STEAMAPPTICKET_INTERFACE_VERSION001,
        VTABLE_ADD_FUNC(winISteamAppTicket_STEAMAPPTICKET_INTERFACE_VERSION001_GetAppOwnershipTicketData)
    );
__ASM_BLOCK_END

struct w_iface *create_winISteamAppTicket_STEAMAPPTICKET_INTERFACE_VERSION001( struct u_iface u_iface )
{
    struct w_iface *r = alloc_mem_for_iface(sizeof(struct w_iface), "STEAMAPPTICKET_INTERFACE_VERSION001");
    TRACE("-> %p\n", r);
    r->vtable = alloc_vtable(&winISteamAppTicket_STEAMAPPTICKET_INTERFACE_VERSION001_vtable, 1, "STEAMAPPTICKET_INTERFACE_VERSION001");
    r->u_iface = u_iface;
    return r;
}

void init_winISteamAppTicket_rtti( char *base )
{
#if defined(__x86_64__) || defined(__aarch64__)
    init_winISteamAppTicket_STEAMAPPTICKET_INTERFACE_VERSION001_rtti( base );
#endif /* defined(__x86_64__) || defined(__aarch64__) */
}
