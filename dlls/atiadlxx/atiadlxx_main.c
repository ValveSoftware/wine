#include <stdarg.h>

#include "windef.h"
#include "winbase.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(atiadlxx);

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, void *reserved)
{
    TRACE("(%p, %u, %p)\n", instance, reason, reserved);

    switch (reason)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(instance);
        break;
    }

    return TRUE;
}

typedef void *(CALLBACK *ADL_MAIN_MALLOC_CALLBACK)(int);
typedef void *ADL_CONTEXT_HANDLE;

typedef struct ADLVersionsInfo
{
    char strDriverVer[256];
    char strCatalystVersion[256];
    char strCatalystWebLink[256];
} ADLVersionsInfo, *LPADLVersionsInfo;

typedef struct ADLVersionsInfoX2
{
    char strDriverVer[256];
    char strCatalystVersion[256];
    char strCrimsonVersion[256];
    char strCatalystWebLink[256];
} ADLVersionsInfoX2, *LPADLVersionsInfoX2;

static const ADLVersionsInfo version = {
    "16.11.2",
    "16.11.2",
    "",
};

static const ADLVersionsInfoX2 version2 = {
    "16.11.2",
    "16.11.2",
    "16.11.2",
    "",
};

int WINAPI ADL2_Main_Control_Create(ADL_MAIN_MALLOC_CALLBACK cb, int arg, ADL_CONTEXT_HANDLE *ptr)
{
    FIXME("cb %p, arg %d, ptr %p stub!\n", cb, arg, ptr);
    return 0;
}

int WINAPI ADL_Main_Control_Create(ADL_MAIN_MALLOC_CALLBACK cb, int arg)
{
    FIXME("cb %p, arg %d stub!\n", cb, arg);
    return 0;
}

int WINAPI ADL_Main_Control_Destroy(void)
{
    FIXME("stub!\n");
    return 0;
}

int WINAPI ADL2_Adapter_NumberOfAdapters_Get(ADL_CONTEXT_HANDLE *ptr, int *count)
{
    FIXME("ptr %p, count %p stub!\n", ptr, count);
    *count = 0;
    return 0;
}

int WINAPI ADL2_Graphics_VersionsX2_Get(ADL_CONTEXT_HANDLE *ptr, ADLVersionsInfoX2 *ver)
{
    FIXME("ptr %p, ver %p stub!\n", ptr, ver);
    memcpy(ver, &version2, sizeof(version2));
    return 0;
}

int WINAPI ADL_Graphics_Versions_Get(ADLVersionsInfo *ver)
{
    FIXME("ver %p stub!\n", ver);
    memcpy(ver, &version, sizeof(version));
    return 0;
}
