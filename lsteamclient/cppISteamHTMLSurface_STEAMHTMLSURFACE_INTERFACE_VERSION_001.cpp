/* This file is auto-generated, do not edit. */
#include "unix_private.h"

#if 0
#pragma makedep unix
#endif

NTSTATUS ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_Init( void *args )
{
    struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_Init_params *params = (struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_Init_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *)params->u_iface;
    params->_ret = iface->Init(  );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_Init( void *args )
{
    struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_Init_params *params = (struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_Init_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *)params->u_iface;
    params->_ret = iface->Init(  );
    return 0;
}
#endif

NTSTATUS ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_Shutdown( void *args )
{
    struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_Shutdown_params *params = (struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_Shutdown_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *)params->u_iface;
    params->_ret = iface->Shutdown(  );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_Shutdown( void *args )
{
    struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_Shutdown_params *params = (struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_Shutdown_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *)params->u_iface;
    params->_ret = iface->Shutdown(  );
    return 0;
}
#endif

NTSTATUS ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_CreateBrowser( void *args )
{
    struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_CreateBrowser_params *params = (struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_CreateBrowser_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *)params->u_iface;
    params->_ret = iface->CreateBrowser( params->pchUserAgent, params->pchUserCSS );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_CreateBrowser( void *args )
{
    struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_CreateBrowser_params *params = (struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_CreateBrowser_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *)params->u_iface;
    params->_ret = iface->CreateBrowser( params->pchUserAgent, params->pchUserCSS );
    return 0;
}
#endif

NTSTATUS ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_RemoveBrowser( void *args )
{
    struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_RemoveBrowser_params *params = (struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_RemoveBrowser_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *)params->u_iface;
    iface->RemoveBrowser( params->unBrowserHandle );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_RemoveBrowser( void *args )
{
    struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_RemoveBrowser_params *params = (struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_RemoveBrowser_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *)params->u_iface;
    iface->RemoveBrowser( params->unBrowserHandle );
    return 0;
}
#endif

NTSTATUS ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_LoadURL( void *args )
{
    struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_LoadURL_params *params = (struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_LoadURL_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *)params->u_iface;
    char *u_pchURL = steamclient_dos_to_unix_path( params->pchURL, 1 );
    iface->LoadURL( params->unBrowserHandle, u_pchURL, params->pchPostData );
    steamclient_free_path( u_pchURL );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_LoadURL( void *args )
{
    struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_LoadURL_params *params = (struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_LoadURL_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *)params->u_iface;
    char *u_pchURL = steamclient_dos_to_unix_path( params->pchURL, 1 );
    iface->LoadURL( params->unBrowserHandle, u_pchURL, params->pchPostData );
    steamclient_free_path( u_pchURL );
    return 0;
}
#endif

NTSTATUS ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_SetSize( void *args )
{
    struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_SetSize_params *params = (struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_SetSize_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *)params->u_iface;
    iface->SetSize( params->unBrowserHandle, params->unWidth, params->unHeight );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_SetSize( void *args )
{
    struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_SetSize_params *params = (struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_SetSize_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *)params->u_iface;
    iface->SetSize( params->unBrowserHandle, params->unWidth, params->unHeight );
    return 0;
}
#endif

NTSTATUS ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_StopLoad( void *args )
{
    struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_StopLoad_params *params = (struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_StopLoad_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *)params->u_iface;
    iface->StopLoad( params->unBrowserHandle );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_StopLoad( void *args )
{
    struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_StopLoad_params *params = (struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_StopLoad_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *)params->u_iface;
    iface->StopLoad( params->unBrowserHandle );
    return 0;
}
#endif

NTSTATUS ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_Reload( void *args )
{
    struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_Reload_params *params = (struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_Reload_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *)params->u_iface;
    iface->Reload( params->unBrowserHandle );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_Reload( void *args )
{
    struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_Reload_params *params = (struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_Reload_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *)params->u_iface;
    iface->Reload( params->unBrowserHandle );
    return 0;
}
#endif

NTSTATUS ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_GoBack( void *args )
{
    struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_GoBack_params *params = (struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_GoBack_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *)params->u_iface;
    iface->GoBack( params->unBrowserHandle );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_GoBack( void *args )
{
    struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_GoBack_params *params = (struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_GoBack_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *)params->u_iface;
    iface->GoBack( params->unBrowserHandle );
    return 0;
}
#endif

NTSTATUS ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_GoForward( void *args )
{
    struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_GoForward_params *params = (struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_GoForward_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *)params->u_iface;
    iface->GoForward( params->unBrowserHandle );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_GoForward( void *args )
{
    struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_GoForward_params *params = (struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_GoForward_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *)params->u_iface;
    iface->GoForward( params->unBrowserHandle );
    return 0;
}
#endif

NTSTATUS ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_AddHeader( void *args )
{
    struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_AddHeader_params *params = (struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_AddHeader_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *)params->u_iface;
    iface->AddHeader( params->unBrowserHandle, params->pchKey, params->pchValue );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_AddHeader( void *args )
{
    struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_AddHeader_params *params = (struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_AddHeader_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *)params->u_iface;
    iface->AddHeader( params->unBrowserHandle, params->pchKey, params->pchValue );
    return 0;
}
#endif

NTSTATUS ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_ExecuteJavascript( void *args )
{
    struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_ExecuteJavascript_params *params = (struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_ExecuteJavascript_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *)params->u_iface;
    iface->ExecuteJavascript( params->unBrowserHandle, params->pchScript );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_ExecuteJavascript( void *args )
{
    struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_ExecuteJavascript_params *params = (struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_ExecuteJavascript_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *)params->u_iface;
    iface->ExecuteJavascript( params->unBrowserHandle, params->pchScript );
    return 0;
}
#endif

NTSTATUS ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_MouseUp( void *args )
{
    struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_MouseUp_params *params = (struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_MouseUp_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *)params->u_iface;
    iface->MouseUp( params->unBrowserHandle, params->eMouseButton );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_MouseUp( void *args )
{
    struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_MouseUp_params *params = (struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_MouseUp_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *)params->u_iface;
    iface->MouseUp( params->unBrowserHandle, params->eMouseButton );
    return 0;
}
#endif

NTSTATUS ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_MouseDown( void *args )
{
    struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_MouseDown_params *params = (struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_MouseDown_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *)params->u_iface;
    iface->MouseDown( params->unBrowserHandle, params->eMouseButton );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_MouseDown( void *args )
{
    struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_MouseDown_params *params = (struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_MouseDown_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *)params->u_iface;
    iface->MouseDown( params->unBrowserHandle, params->eMouseButton );
    return 0;
}
#endif

NTSTATUS ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_MouseDoubleClick( void *args )
{
    struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_MouseDoubleClick_params *params = (struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_MouseDoubleClick_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *)params->u_iface;
    iface->MouseDoubleClick( params->unBrowserHandle, params->eMouseButton );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_MouseDoubleClick( void *args )
{
    struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_MouseDoubleClick_params *params = (struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_MouseDoubleClick_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *)params->u_iface;
    iface->MouseDoubleClick( params->unBrowserHandle, params->eMouseButton );
    return 0;
}
#endif

NTSTATUS ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_MouseMove( void *args )
{
    struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_MouseMove_params *params = (struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_MouseMove_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *)params->u_iface;
    iface->MouseMove( params->unBrowserHandle, params->x, params->y );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_MouseMove( void *args )
{
    struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_MouseMove_params *params = (struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_MouseMove_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *)params->u_iface;
    iface->MouseMove( params->unBrowserHandle, params->x, params->y );
    return 0;
}
#endif

NTSTATUS ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_MouseWheel( void *args )
{
    struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_MouseWheel_params *params = (struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_MouseWheel_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *)params->u_iface;
    iface->MouseWheel( params->unBrowserHandle, params->nDelta );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_MouseWheel( void *args )
{
    struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_MouseWheel_params *params = (struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_MouseWheel_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *)params->u_iface;
    iface->MouseWheel( params->unBrowserHandle, params->nDelta );
    return 0;
}
#endif

NTSTATUS ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_KeyDown( void *args )
{
    struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_KeyDown_params *params = (struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_KeyDown_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *)params->u_iface;
    uint32_t u_nNativeKeyCode = manual_convert_nNativeKeyCode( params->nNativeKeyCode );
    iface->KeyDown( params->unBrowserHandle, u_nNativeKeyCode, params->eHTMLKeyModifiers );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_KeyDown( void *args )
{
    struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_KeyDown_params *params = (struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_KeyDown_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *)params->u_iface;
    uint32_t u_nNativeKeyCode = manual_convert_nNativeKeyCode( params->nNativeKeyCode );
    iface->KeyDown( params->unBrowserHandle, u_nNativeKeyCode, params->eHTMLKeyModifiers );
    return 0;
}
#endif

NTSTATUS ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_KeyUp( void *args )
{
    struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_KeyUp_params *params = (struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_KeyUp_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *)params->u_iface;
    uint32_t u_nNativeKeyCode = manual_convert_nNativeKeyCode( params->nNativeKeyCode );
    iface->KeyUp( params->unBrowserHandle, u_nNativeKeyCode, params->eHTMLKeyModifiers );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_KeyUp( void *args )
{
    struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_KeyUp_params *params = (struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_KeyUp_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *)params->u_iface;
    uint32_t u_nNativeKeyCode = manual_convert_nNativeKeyCode( params->nNativeKeyCode );
    iface->KeyUp( params->unBrowserHandle, u_nNativeKeyCode, params->eHTMLKeyModifiers );
    return 0;
}
#endif

NTSTATUS ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_KeyChar( void *args )
{
    struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_KeyChar_params *params = (struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_KeyChar_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *)params->u_iface;
    iface->KeyChar( params->unBrowserHandle, params->cUnicodeChar, params->eHTMLKeyModifiers );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_KeyChar( void *args )
{
    struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_KeyChar_params *params = (struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_KeyChar_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *)params->u_iface;
    iface->KeyChar( params->unBrowserHandle, params->cUnicodeChar, params->eHTMLKeyModifiers );
    return 0;
}
#endif

NTSTATUS ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_SetHorizontalScroll( void *args )
{
    struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_SetHorizontalScroll_params *params = (struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_SetHorizontalScroll_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *)params->u_iface;
    iface->SetHorizontalScroll( params->unBrowserHandle, params->nAbsolutePixelScroll );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_SetHorizontalScroll( void *args )
{
    struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_SetHorizontalScroll_params *params = (struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_SetHorizontalScroll_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *)params->u_iface;
    iface->SetHorizontalScroll( params->unBrowserHandle, params->nAbsolutePixelScroll );
    return 0;
}
#endif

NTSTATUS ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_SetVerticalScroll( void *args )
{
    struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_SetVerticalScroll_params *params = (struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_SetVerticalScroll_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *)params->u_iface;
    iface->SetVerticalScroll( params->unBrowserHandle, params->nAbsolutePixelScroll );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_SetVerticalScroll( void *args )
{
    struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_SetVerticalScroll_params *params = (struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_SetVerticalScroll_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *)params->u_iface;
    iface->SetVerticalScroll( params->unBrowserHandle, params->nAbsolutePixelScroll );
    return 0;
}
#endif

NTSTATUS ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_SetKeyFocus( void *args )
{
    struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_SetKeyFocus_params *params = (struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_SetKeyFocus_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *)params->u_iface;
    iface->SetKeyFocus( params->unBrowserHandle, params->bHasKeyFocus );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_SetKeyFocus( void *args )
{
    struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_SetKeyFocus_params *params = (struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_SetKeyFocus_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *)params->u_iface;
    iface->SetKeyFocus( params->unBrowserHandle, params->bHasKeyFocus );
    return 0;
}
#endif

NTSTATUS ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_ViewSource( void *args )
{
    struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_ViewSource_params *params = (struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_ViewSource_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *)params->u_iface;
    iface->ViewSource( params->unBrowserHandle );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_ViewSource( void *args )
{
    struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_ViewSource_params *params = (struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_ViewSource_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *)params->u_iface;
    iface->ViewSource( params->unBrowserHandle );
    return 0;
}
#endif

NTSTATUS ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_CopyToClipboard( void *args )
{
    struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_CopyToClipboard_params *params = (struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_CopyToClipboard_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *)params->u_iface;
    iface->CopyToClipboard( params->unBrowserHandle );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_CopyToClipboard( void *args )
{
    struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_CopyToClipboard_params *params = (struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_CopyToClipboard_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *)params->u_iface;
    iface->CopyToClipboard( params->unBrowserHandle );
    return 0;
}
#endif

NTSTATUS ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_PasteFromClipboard( void *args )
{
    struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_PasteFromClipboard_params *params = (struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_PasteFromClipboard_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *)params->u_iface;
    iface->PasteFromClipboard( params->unBrowserHandle );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_PasteFromClipboard( void *args )
{
    struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_PasteFromClipboard_params *params = (struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_PasteFromClipboard_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *)params->u_iface;
    iface->PasteFromClipboard( params->unBrowserHandle );
    return 0;
}
#endif

NTSTATUS ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_Find( void *args )
{
    struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_Find_params *params = (struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_Find_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *)params->u_iface;
    iface->Find( params->unBrowserHandle, params->pchSearchStr, params->bCurrentlyInFind, params->bReverse );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_Find( void *args )
{
    struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_Find_params *params = (struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_Find_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *)params->u_iface;
    iface->Find( params->unBrowserHandle, params->pchSearchStr, params->bCurrentlyInFind, params->bReverse );
    return 0;
}
#endif

NTSTATUS ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_StopFind( void *args )
{
    struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_StopFind_params *params = (struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_StopFind_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *)params->u_iface;
    iface->StopFind( params->unBrowserHandle );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_StopFind( void *args )
{
    struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_StopFind_params *params = (struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_StopFind_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *)params->u_iface;
    iface->StopFind( params->unBrowserHandle );
    return 0;
}
#endif

NTSTATUS ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_GetLinkAtPosition( void *args )
{
    struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_GetLinkAtPosition_params *params = (struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_GetLinkAtPosition_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *)params->u_iface;
    iface->GetLinkAtPosition( params->unBrowserHandle, params->x, params->y );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_GetLinkAtPosition( void *args )
{
    struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_GetLinkAtPosition_params *params = (struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_GetLinkAtPosition_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *)params->u_iface;
    iface->GetLinkAtPosition( params->unBrowserHandle, params->x, params->y );
    return 0;
}
#endif

NTSTATUS ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_AllowStartRequest( void *args )
{
    struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_AllowStartRequest_params *params = (struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_AllowStartRequest_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *)params->u_iface;
    iface->AllowStartRequest( params->unBrowserHandle, params->bAllowed );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_AllowStartRequest( void *args )
{
    struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_AllowStartRequest_params *params = (struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_AllowStartRequest_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *)params->u_iface;
    iface->AllowStartRequest( params->unBrowserHandle, params->bAllowed );
    return 0;
}
#endif

NTSTATUS ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_JSDialogResponse( void *args )
{
    struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_JSDialogResponse_params *params = (struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_JSDialogResponse_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *)params->u_iface;
    iface->JSDialogResponse( params->unBrowserHandle, params->bResult );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_JSDialogResponse( void *args )
{
    struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_JSDialogResponse_params *params = (struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_JSDialogResponse_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *)params->u_iface;
    iface->JSDialogResponse( params->unBrowserHandle, params->bResult );
    return 0;
}
#endif

NTSTATUS ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_FileLoadDialogResponse( void *args )
{
    struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_FileLoadDialogResponse_params *params = (struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_FileLoadDialogResponse_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *)params->u_iface;
    const char **u_pchSelectedFiles = steamclient_dos_to_unix_path_array( params->pchSelectedFiles );
    iface->FileLoadDialogResponse( params->unBrowserHandle, u_pchSelectedFiles );
    steamclient_free_path_array( u_pchSelectedFiles );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_FileLoadDialogResponse( void *args )
{
    struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_FileLoadDialogResponse_params *params = (struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001_FileLoadDialogResponse_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_001 *)params->u_iface;
    const char **u_pchSelectedFiles = wow64_steamclient_dos_to_unix_path_array( params->pchSelectedFiles );
    iface->FileLoadDialogResponse( params->unBrowserHandle, u_pchSelectedFiles );
    steamclient_free_path_array( u_pchSelectedFiles );
    return 0;
}
#endif

