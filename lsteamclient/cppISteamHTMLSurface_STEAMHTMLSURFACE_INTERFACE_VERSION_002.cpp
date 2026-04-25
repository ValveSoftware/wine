/* This file is auto-generated, do not edit. */
#include "unix_private.h"

#if 0
#pragma makedep unix
#endif

NTSTATUS ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_Init( void *args )
{
    struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_Init_params *params = (struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_Init_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *)params->u_iface;
    params->_ret = iface->Init(  );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_Init( void *args )
{
    struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_Init_params *params = (struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_Init_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *)params->u_iface;
    params->_ret = iface->Init(  );
    return 0;
}
#endif

NTSTATUS ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_Shutdown( void *args )
{
    struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_Shutdown_params *params = (struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_Shutdown_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *)params->u_iface;
    params->_ret = iface->Shutdown(  );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_Shutdown( void *args )
{
    struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_Shutdown_params *params = (struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_Shutdown_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *)params->u_iface;
    params->_ret = iface->Shutdown(  );
    return 0;
}
#endif

NTSTATUS ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_CreateBrowser( void *args )
{
    struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_CreateBrowser_params *params = (struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_CreateBrowser_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *)params->u_iface;
    params->_ret = iface->CreateBrowser( params->pchUserAgent, params->pchUserCSS );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_CreateBrowser( void *args )
{
    struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_CreateBrowser_params *params = (struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_CreateBrowser_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *)params->u_iface;
    params->_ret = iface->CreateBrowser( params->pchUserAgent, params->pchUserCSS );
    return 0;
}
#endif

NTSTATUS ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_RemoveBrowser( void *args )
{
    struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_RemoveBrowser_params *params = (struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_RemoveBrowser_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *)params->u_iface;
    iface->RemoveBrowser( params->unBrowserHandle );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_RemoveBrowser( void *args )
{
    struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_RemoveBrowser_params *params = (struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_RemoveBrowser_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *)params->u_iface;
    iface->RemoveBrowser( params->unBrowserHandle );
    return 0;
}
#endif

NTSTATUS ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_LoadURL( void *args )
{
    struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_LoadURL_params *params = (struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_LoadURL_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *)params->u_iface;
    char *u_pchURL = steamclient_dos_to_unix_path( params->pchURL, 1 );
    iface->LoadURL( params->unBrowserHandle, u_pchURL, params->pchPostData );
    steamclient_free_path( u_pchURL );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_LoadURL( void *args )
{
    struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_LoadURL_params *params = (struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_LoadURL_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *)params->u_iface;
    char *u_pchURL = steamclient_dos_to_unix_path( params->pchURL, 1 );
    iface->LoadURL( params->unBrowserHandle, u_pchURL, params->pchPostData );
    steamclient_free_path( u_pchURL );
    return 0;
}
#endif

NTSTATUS ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_SetSize( void *args )
{
    struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_SetSize_params *params = (struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_SetSize_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *)params->u_iface;
    iface->SetSize( params->unBrowserHandle, params->unWidth, params->unHeight );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_SetSize( void *args )
{
    struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_SetSize_params *params = (struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_SetSize_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *)params->u_iface;
    iface->SetSize( params->unBrowserHandle, params->unWidth, params->unHeight );
    return 0;
}
#endif

NTSTATUS ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_StopLoad( void *args )
{
    struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_StopLoad_params *params = (struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_StopLoad_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *)params->u_iface;
    iface->StopLoad( params->unBrowserHandle );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_StopLoad( void *args )
{
    struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_StopLoad_params *params = (struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_StopLoad_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *)params->u_iface;
    iface->StopLoad( params->unBrowserHandle );
    return 0;
}
#endif

NTSTATUS ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_Reload( void *args )
{
    struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_Reload_params *params = (struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_Reload_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *)params->u_iface;
    iface->Reload( params->unBrowserHandle );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_Reload( void *args )
{
    struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_Reload_params *params = (struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_Reload_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *)params->u_iface;
    iface->Reload( params->unBrowserHandle );
    return 0;
}
#endif

NTSTATUS ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_GoBack( void *args )
{
    struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_GoBack_params *params = (struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_GoBack_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *)params->u_iface;
    iface->GoBack( params->unBrowserHandle );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_GoBack( void *args )
{
    struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_GoBack_params *params = (struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_GoBack_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *)params->u_iface;
    iface->GoBack( params->unBrowserHandle );
    return 0;
}
#endif

NTSTATUS ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_GoForward( void *args )
{
    struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_GoForward_params *params = (struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_GoForward_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *)params->u_iface;
    iface->GoForward( params->unBrowserHandle );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_GoForward( void *args )
{
    struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_GoForward_params *params = (struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_GoForward_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *)params->u_iface;
    iface->GoForward( params->unBrowserHandle );
    return 0;
}
#endif

NTSTATUS ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_AddHeader( void *args )
{
    struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_AddHeader_params *params = (struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_AddHeader_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *)params->u_iface;
    iface->AddHeader( params->unBrowserHandle, params->pchKey, params->pchValue );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_AddHeader( void *args )
{
    struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_AddHeader_params *params = (struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_AddHeader_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *)params->u_iface;
    iface->AddHeader( params->unBrowserHandle, params->pchKey, params->pchValue );
    return 0;
}
#endif

NTSTATUS ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_ExecuteJavascript( void *args )
{
    struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_ExecuteJavascript_params *params = (struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_ExecuteJavascript_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *)params->u_iface;
    iface->ExecuteJavascript( params->unBrowserHandle, params->pchScript );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_ExecuteJavascript( void *args )
{
    struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_ExecuteJavascript_params *params = (struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_ExecuteJavascript_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *)params->u_iface;
    iface->ExecuteJavascript( params->unBrowserHandle, params->pchScript );
    return 0;
}
#endif

NTSTATUS ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_MouseUp( void *args )
{
    struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_MouseUp_params *params = (struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_MouseUp_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *)params->u_iface;
    iface->MouseUp( params->unBrowserHandle, params->eMouseButton );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_MouseUp( void *args )
{
    struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_MouseUp_params *params = (struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_MouseUp_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *)params->u_iface;
    iface->MouseUp( params->unBrowserHandle, params->eMouseButton );
    return 0;
}
#endif

NTSTATUS ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_MouseDown( void *args )
{
    struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_MouseDown_params *params = (struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_MouseDown_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *)params->u_iface;
    iface->MouseDown( params->unBrowserHandle, params->eMouseButton );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_MouseDown( void *args )
{
    struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_MouseDown_params *params = (struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_MouseDown_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *)params->u_iface;
    iface->MouseDown( params->unBrowserHandle, params->eMouseButton );
    return 0;
}
#endif

NTSTATUS ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_MouseDoubleClick( void *args )
{
    struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_MouseDoubleClick_params *params = (struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_MouseDoubleClick_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *)params->u_iface;
    iface->MouseDoubleClick( params->unBrowserHandle, params->eMouseButton );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_MouseDoubleClick( void *args )
{
    struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_MouseDoubleClick_params *params = (struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_MouseDoubleClick_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *)params->u_iface;
    iface->MouseDoubleClick( params->unBrowserHandle, params->eMouseButton );
    return 0;
}
#endif

NTSTATUS ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_MouseMove( void *args )
{
    struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_MouseMove_params *params = (struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_MouseMove_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *)params->u_iface;
    iface->MouseMove( params->unBrowserHandle, params->x, params->y );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_MouseMove( void *args )
{
    struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_MouseMove_params *params = (struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_MouseMove_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *)params->u_iface;
    iface->MouseMove( params->unBrowserHandle, params->x, params->y );
    return 0;
}
#endif

NTSTATUS ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_MouseWheel( void *args )
{
    struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_MouseWheel_params *params = (struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_MouseWheel_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *)params->u_iface;
    iface->MouseWheel( params->unBrowserHandle, params->nDelta );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_MouseWheel( void *args )
{
    struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_MouseWheel_params *params = (struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_MouseWheel_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *)params->u_iface;
    iface->MouseWheel( params->unBrowserHandle, params->nDelta );
    return 0;
}
#endif

NTSTATUS ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_KeyDown( void *args )
{
    struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_KeyDown_params *params = (struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_KeyDown_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *)params->u_iface;
    uint32_t u_nNativeKeyCode = manual_convert_nNativeKeyCode( params->nNativeKeyCode );
    iface->KeyDown( params->unBrowserHandle, u_nNativeKeyCode, params->eHTMLKeyModifiers );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_KeyDown( void *args )
{
    struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_KeyDown_params *params = (struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_KeyDown_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *)params->u_iface;
    uint32_t u_nNativeKeyCode = manual_convert_nNativeKeyCode( params->nNativeKeyCode );
    iface->KeyDown( params->unBrowserHandle, u_nNativeKeyCode, params->eHTMLKeyModifiers );
    return 0;
}
#endif

NTSTATUS ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_KeyUp( void *args )
{
    struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_KeyUp_params *params = (struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_KeyUp_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *)params->u_iface;
    uint32_t u_nNativeKeyCode = manual_convert_nNativeKeyCode( params->nNativeKeyCode );
    iface->KeyUp( params->unBrowserHandle, u_nNativeKeyCode, params->eHTMLKeyModifiers );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_KeyUp( void *args )
{
    struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_KeyUp_params *params = (struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_KeyUp_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *)params->u_iface;
    uint32_t u_nNativeKeyCode = manual_convert_nNativeKeyCode( params->nNativeKeyCode );
    iface->KeyUp( params->unBrowserHandle, u_nNativeKeyCode, params->eHTMLKeyModifiers );
    return 0;
}
#endif

NTSTATUS ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_KeyChar( void *args )
{
    struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_KeyChar_params *params = (struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_KeyChar_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *)params->u_iface;
    iface->KeyChar( params->unBrowserHandle, params->cUnicodeChar, params->eHTMLKeyModifiers );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_KeyChar( void *args )
{
    struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_KeyChar_params *params = (struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_KeyChar_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *)params->u_iface;
    iface->KeyChar( params->unBrowserHandle, params->cUnicodeChar, params->eHTMLKeyModifiers );
    return 0;
}
#endif

NTSTATUS ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_SetHorizontalScroll( void *args )
{
    struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_SetHorizontalScroll_params *params = (struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_SetHorizontalScroll_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *)params->u_iface;
    iface->SetHorizontalScroll( params->unBrowserHandle, params->nAbsolutePixelScroll );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_SetHorizontalScroll( void *args )
{
    struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_SetHorizontalScroll_params *params = (struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_SetHorizontalScroll_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *)params->u_iface;
    iface->SetHorizontalScroll( params->unBrowserHandle, params->nAbsolutePixelScroll );
    return 0;
}
#endif

NTSTATUS ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_SetVerticalScroll( void *args )
{
    struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_SetVerticalScroll_params *params = (struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_SetVerticalScroll_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *)params->u_iface;
    iface->SetVerticalScroll( params->unBrowserHandle, params->nAbsolutePixelScroll );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_SetVerticalScroll( void *args )
{
    struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_SetVerticalScroll_params *params = (struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_SetVerticalScroll_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *)params->u_iface;
    iface->SetVerticalScroll( params->unBrowserHandle, params->nAbsolutePixelScroll );
    return 0;
}
#endif

NTSTATUS ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_SetKeyFocus( void *args )
{
    struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_SetKeyFocus_params *params = (struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_SetKeyFocus_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *)params->u_iface;
    iface->SetKeyFocus( params->unBrowserHandle, params->bHasKeyFocus );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_SetKeyFocus( void *args )
{
    struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_SetKeyFocus_params *params = (struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_SetKeyFocus_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *)params->u_iface;
    iface->SetKeyFocus( params->unBrowserHandle, params->bHasKeyFocus );
    return 0;
}
#endif

NTSTATUS ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_ViewSource( void *args )
{
    struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_ViewSource_params *params = (struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_ViewSource_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *)params->u_iface;
    iface->ViewSource( params->unBrowserHandle );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_ViewSource( void *args )
{
    struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_ViewSource_params *params = (struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_ViewSource_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *)params->u_iface;
    iface->ViewSource( params->unBrowserHandle );
    return 0;
}
#endif

NTSTATUS ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_CopyToClipboard( void *args )
{
    struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_CopyToClipboard_params *params = (struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_CopyToClipboard_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *)params->u_iface;
    iface->CopyToClipboard( params->unBrowserHandle );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_CopyToClipboard( void *args )
{
    struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_CopyToClipboard_params *params = (struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_CopyToClipboard_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *)params->u_iface;
    iface->CopyToClipboard( params->unBrowserHandle );
    return 0;
}
#endif

NTSTATUS ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_PasteFromClipboard( void *args )
{
    struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_PasteFromClipboard_params *params = (struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_PasteFromClipboard_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *)params->u_iface;
    iface->PasteFromClipboard( params->unBrowserHandle );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_PasteFromClipboard( void *args )
{
    struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_PasteFromClipboard_params *params = (struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_PasteFromClipboard_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *)params->u_iface;
    iface->PasteFromClipboard( params->unBrowserHandle );
    return 0;
}
#endif

NTSTATUS ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_Find( void *args )
{
    struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_Find_params *params = (struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_Find_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *)params->u_iface;
    iface->Find( params->unBrowserHandle, params->pchSearchStr, params->bCurrentlyInFind, params->bReverse );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_Find( void *args )
{
    struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_Find_params *params = (struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_Find_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *)params->u_iface;
    iface->Find( params->unBrowserHandle, params->pchSearchStr, params->bCurrentlyInFind, params->bReverse );
    return 0;
}
#endif

NTSTATUS ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_StopFind( void *args )
{
    struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_StopFind_params *params = (struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_StopFind_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *)params->u_iface;
    iface->StopFind( params->unBrowserHandle );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_StopFind( void *args )
{
    struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_StopFind_params *params = (struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_StopFind_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *)params->u_iface;
    iface->StopFind( params->unBrowserHandle );
    return 0;
}
#endif

NTSTATUS ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_GetLinkAtPosition( void *args )
{
    struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_GetLinkAtPosition_params *params = (struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_GetLinkAtPosition_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *)params->u_iface;
    iface->GetLinkAtPosition( params->unBrowserHandle, params->x, params->y );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_GetLinkAtPosition( void *args )
{
    struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_GetLinkAtPosition_params *params = (struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_GetLinkAtPosition_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *)params->u_iface;
    iface->GetLinkAtPosition( params->unBrowserHandle, params->x, params->y );
    return 0;
}
#endif

NTSTATUS ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_SetCookie( void *args )
{
    struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_SetCookie_params *params = (struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_SetCookie_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *)params->u_iface;
    iface->SetCookie( params->pchHostname, params->pchKey, params->pchValue, params->pchPath, params->nExpires, params->bSecure, params->bHTTPOnly );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_SetCookie( void *args )
{
    struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_SetCookie_params *params = (struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_SetCookie_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *)params->u_iface;
    iface->SetCookie( params->pchHostname, params->pchKey, params->pchValue, params->pchPath, params->nExpires, params->bSecure, params->bHTTPOnly );
    return 0;
}
#endif

NTSTATUS ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_SetPageScaleFactor( void *args )
{
    struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_SetPageScaleFactor_params *params = (struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_SetPageScaleFactor_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *)params->u_iface;
    iface->SetPageScaleFactor( params->unBrowserHandle, params->flZoom, params->nPointX, params->nPointY );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_SetPageScaleFactor( void *args )
{
    struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_SetPageScaleFactor_params *params = (struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_SetPageScaleFactor_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *)params->u_iface;
    iface->SetPageScaleFactor( params->unBrowserHandle, params->flZoom, params->nPointX, params->nPointY );
    return 0;
}
#endif

NTSTATUS ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_AllowStartRequest( void *args )
{
    struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_AllowStartRequest_params *params = (struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_AllowStartRequest_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *)params->u_iface;
    iface->AllowStartRequest( params->unBrowserHandle, params->bAllowed );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_AllowStartRequest( void *args )
{
    struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_AllowStartRequest_params *params = (struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_AllowStartRequest_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *)params->u_iface;
    iface->AllowStartRequest( params->unBrowserHandle, params->bAllowed );
    return 0;
}
#endif

NTSTATUS ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_JSDialogResponse( void *args )
{
    struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_JSDialogResponse_params *params = (struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_JSDialogResponse_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *)params->u_iface;
    iface->JSDialogResponse( params->unBrowserHandle, params->bResult );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_JSDialogResponse( void *args )
{
    struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_JSDialogResponse_params *params = (struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_JSDialogResponse_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *)params->u_iface;
    iface->JSDialogResponse( params->unBrowserHandle, params->bResult );
    return 0;
}
#endif

NTSTATUS ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_FileLoadDialogResponse( void *args )
{
    struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_FileLoadDialogResponse_params *params = (struct ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_FileLoadDialogResponse_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *)params->u_iface;
    const char **u_pchSelectedFiles = steamclient_dos_to_unix_path_array( params->pchSelectedFiles );
    iface->FileLoadDialogResponse( params->unBrowserHandle, u_pchSelectedFiles );
    steamclient_free_path_array( u_pchSelectedFiles );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_FileLoadDialogResponse( void *args )
{
    struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_FileLoadDialogResponse_params *params = (struct wow64_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002_FileLoadDialogResponse_params *)args;
    struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *iface = (struct u_ISteamHTMLSurface_STEAMHTMLSURFACE_INTERFACE_VERSION_002 *)params->u_iface;
    const char **u_pchSelectedFiles = wow64_steamclient_dos_to_unix_path_array( params->pchSelectedFiles );
    iface->FileLoadDialogResponse( params->unBrowserHandle, u_pchSelectedFiles );
    steamclient_free_path_array( u_pchSelectedFiles );
    return 0;
}
#endif

