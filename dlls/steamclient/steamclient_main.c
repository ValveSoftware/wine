#include <stdarg.h>

#include "windef.h"
#include "winbase.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(steamclient);

static HMODULE lsteamclient;
static void (CDECL *pBreakpad_SteamMiniDumpInit)(UINT32, char *, char *);
static void (CDECL *pBreakpad_SteamSetAppID)(UINT32);
static int (CDECL *pBreakpad_SteamSetSteamID)(UINT64);
static int (CDECL *pBreakpad_SteamWriteMiniDumpSetComment)(char *);
static void (CDECL *pBreakpad_SteamWriteMiniDumpUsingExceptionInfoWithBuildId)(int, int);
static void *(CDECL *pCreateInterface)(char *, int *);
static BOOL (CDECL *pSteam_BGetCallback)(INT32, void *, INT32 *);
static BOOL (CDECL *pSteam_FreeLastCallback)(INT32);
static BOOL (CDECL *pSteam_GetAPICallResult)(INT32, UINT64, void *, int, int, BOOL *);
static void (CDECL *pSteam_ReleaseThreadLocalMemory)(int);

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, void *reserved)
{
    TRACE("(%p, %u, %p)\n", instance, reason, reserved);

    switch (reason)
    {
        case DLL_PROCESS_ATTACH:
            lsteamclient = LoadLibraryA("lsteamclient");
            if (!lsteamclient)
                return FALSE;

            pBreakpad_SteamMiniDumpInit = (void*)GetProcAddress(lsteamclient,"Breakpad_SteamMiniDumpInit");
            pBreakpad_SteamSetAppID = (void*)GetProcAddress(lsteamclient,"Breakpad_SteamSetAppID");
            pBreakpad_SteamSetSteamID = (void*)GetProcAddress(lsteamclient,"Breakpad_SteamSetSteamID");
            pBreakpad_SteamWriteMiniDumpSetComment = (void*)GetProcAddress(lsteamclient,"Breakpad_SteamWriteMiniDumpSetComment");
            pBreakpad_SteamWriteMiniDumpUsingExceptionInfoWithBuildId = (void*)GetProcAddress(lsteamclient,"Breakpad_SteamWriteMiniDumpUsingExceptionInfoWithBuildId");
            pCreateInterface = (void*)GetProcAddress(lsteamclient,"CreateInterface");
            pSteam_BGetCallback = (void*)GetProcAddress(lsteamclient,"Steam_BGetCallback");
            pSteam_FreeLastCallback = (void*)GetProcAddress(lsteamclient,"Steam_FreeLastCallback");
            pSteam_GetAPICallResult = (void*)GetProcAddress(lsteamclient,"Steam_GetAPICallResult");
            pSteam_ReleaseThreadLocalMemory = (void*)GetProcAddress(lsteamclient,"Steam_ReleaseThreadLocalMemory");

            TRACE("Forwarding DLL (lsteamclient) loaded (%p)\n", lsteamclient);
            break;
        case DLL_PROCESS_DETACH:
            FreeLibrary(lsteamclient);
            TRACE("Forwarding DLL (lsteamclient) freed\n");
            break;
    }

    return TRUE;
}

void CDECL STEAMCLIENT_Breakpad_SteamMiniDumpInit(UINT32 a, char *b, char *c)
{
    TRACE("\n");
    pBreakpad_SteamMiniDumpInit(a, b, c);
}

void CDECL STEAMCLIENT_Breakpad_SteamSetAppID(UINT32 a)
{
    TRACE("\n");
    pBreakpad_SteamSetAppID(a);
}

int CDECL STEAMCLIENT_Breakpad_SteamSetSteamID(UINT64 a)
{
    TRACE("\n");
    return pBreakpad_SteamSetSteamID(a);
}

int CDECL STEAMCLIENT_Breakpad_SteamWriteMiniDumpSetComment(char *a)
{
    TRACE("\n");
    return pBreakpad_SteamWriteMiniDumpSetComment(a);
}

void CDECL STEAMCLIENT_Breakpad_SteamWriteMiniDumpUsingExceptionInfoWithBuildId(int a, int b)
{
    TRACE("\n");
    pBreakpad_SteamWriteMiniDumpUsingExceptionInfoWithBuildId(a, b);
}

void *CDECL STEAMCLIENT_CreateInterface(char *a, int *b)
{
    TRACE("\n");
    return pCreateInterface(a, b);
}

BOOL CDECL STEAMCLIENT_Steam_BGetCallback(INT32 a, void *b, INT32 *c)
{
    TRACE("\n");
    return pSteam_BGetCallback(a, b, c);
}

BOOL CDECL STEAMCLIENT_Steam_FreeLastCallback(INT32 a)
{
    TRACE("\n");
    return pSteam_FreeLastCallback(a);
}

BOOL CDECL STEAMCLIENT_Steam_GetAPICallResult(INT32 a, UINT64 b, void *c, int d, int e, BOOL *f)
{
    TRACE("\n");
    return pSteam_GetAPICallResult(a, b, c, d, e, f);
}

void CDECL STEAMCLIENT_Steam_ReleaseThreadLocalMemory(int a)
{
    TRACE("\n");
    pSteam_ReleaseThreadLocalMemory(a);
}
