/* This file is auto-generated, do not edit. */
#include "unix_private.h"

#if 0
#pragma makedep unix
#endif

NTSTATUS ISteamTimeline_STEAMTIMELINE_INTERFACE_V001_SetTimelineStateDescription( void *args )
{
    struct ISteamTimeline_STEAMTIMELINE_INTERFACE_V001_SetTimelineStateDescription_params *params = (struct ISteamTimeline_STEAMTIMELINE_INTERFACE_V001_SetTimelineStateDescription_params *)args;
    struct u_ISteamTimeline_STEAMTIMELINE_INTERFACE_V001 *iface = (struct u_ISteamTimeline_STEAMTIMELINE_INTERFACE_V001 *)params->u_iface;
    iface->SetTimelineStateDescription( params->pchDescription, params->flTimeDelta );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamTimeline_STEAMTIMELINE_INTERFACE_V001_SetTimelineStateDescription( void *args )
{
    struct wow64_ISteamTimeline_STEAMTIMELINE_INTERFACE_V001_SetTimelineStateDescription_params *params = (struct wow64_ISteamTimeline_STEAMTIMELINE_INTERFACE_V001_SetTimelineStateDescription_params *)args;
    struct u_ISteamTimeline_STEAMTIMELINE_INTERFACE_V001 *iface = (struct u_ISteamTimeline_STEAMTIMELINE_INTERFACE_V001 *)params->u_iface;
    iface->SetTimelineStateDescription( params->pchDescription, params->flTimeDelta );
    return 0;
}
#endif

NTSTATUS ISteamTimeline_STEAMTIMELINE_INTERFACE_V001_ClearTimelineStateDescription( void *args )
{
    struct ISteamTimeline_STEAMTIMELINE_INTERFACE_V001_ClearTimelineStateDescription_params *params = (struct ISteamTimeline_STEAMTIMELINE_INTERFACE_V001_ClearTimelineStateDescription_params *)args;
    struct u_ISteamTimeline_STEAMTIMELINE_INTERFACE_V001 *iface = (struct u_ISteamTimeline_STEAMTIMELINE_INTERFACE_V001 *)params->u_iface;
    iface->ClearTimelineStateDescription( params->flTimeDelta );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamTimeline_STEAMTIMELINE_INTERFACE_V001_ClearTimelineStateDescription( void *args )
{
    struct wow64_ISteamTimeline_STEAMTIMELINE_INTERFACE_V001_ClearTimelineStateDescription_params *params = (struct wow64_ISteamTimeline_STEAMTIMELINE_INTERFACE_V001_ClearTimelineStateDescription_params *)args;
    struct u_ISteamTimeline_STEAMTIMELINE_INTERFACE_V001 *iface = (struct u_ISteamTimeline_STEAMTIMELINE_INTERFACE_V001 *)params->u_iface;
    iface->ClearTimelineStateDescription( params->flTimeDelta );
    return 0;
}
#endif

NTSTATUS ISteamTimeline_STEAMTIMELINE_INTERFACE_V001_AddTimelineEvent( void *args )
{
    struct ISteamTimeline_STEAMTIMELINE_INTERFACE_V001_AddTimelineEvent_params *params = (struct ISteamTimeline_STEAMTIMELINE_INTERFACE_V001_AddTimelineEvent_params *)args;
    struct u_ISteamTimeline_STEAMTIMELINE_INTERFACE_V001 *iface = (struct u_ISteamTimeline_STEAMTIMELINE_INTERFACE_V001 *)params->u_iface;
    iface->AddTimelineEvent( params->pchIcon, params->pchTitle, params->pchDescription, params->unPriority, params->flStartOffsetSeconds, params->flDurationSeconds, params->ePossibleClip );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamTimeline_STEAMTIMELINE_INTERFACE_V001_AddTimelineEvent( void *args )
{
    struct wow64_ISteamTimeline_STEAMTIMELINE_INTERFACE_V001_AddTimelineEvent_params *params = (struct wow64_ISteamTimeline_STEAMTIMELINE_INTERFACE_V001_AddTimelineEvent_params *)args;
    struct u_ISteamTimeline_STEAMTIMELINE_INTERFACE_V001 *iface = (struct u_ISteamTimeline_STEAMTIMELINE_INTERFACE_V001 *)params->u_iface;
    iface->AddTimelineEvent( params->pchIcon, params->pchTitle, params->pchDescription, params->unPriority, params->flStartOffsetSeconds, params->flDurationSeconds, params->ePossibleClip );
    return 0;
}
#endif

NTSTATUS ISteamTimeline_STEAMTIMELINE_INTERFACE_V001_SetTimelineGameMode( void *args )
{
    struct ISteamTimeline_STEAMTIMELINE_INTERFACE_V001_SetTimelineGameMode_params *params = (struct ISteamTimeline_STEAMTIMELINE_INTERFACE_V001_SetTimelineGameMode_params *)args;
    struct u_ISteamTimeline_STEAMTIMELINE_INTERFACE_V001 *iface = (struct u_ISteamTimeline_STEAMTIMELINE_INTERFACE_V001 *)params->u_iface;
    iface->SetTimelineGameMode( params->eMode );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamTimeline_STEAMTIMELINE_INTERFACE_V001_SetTimelineGameMode( void *args )
{
    struct wow64_ISteamTimeline_STEAMTIMELINE_INTERFACE_V001_SetTimelineGameMode_params *params = (struct wow64_ISteamTimeline_STEAMTIMELINE_INTERFACE_V001_SetTimelineGameMode_params *)args;
    struct u_ISteamTimeline_STEAMTIMELINE_INTERFACE_V001 *iface = (struct u_ISteamTimeline_STEAMTIMELINE_INTERFACE_V001 *)params->u_iface;
    iface->SetTimelineGameMode( params->eMode );
    return 0;
}
#endif

