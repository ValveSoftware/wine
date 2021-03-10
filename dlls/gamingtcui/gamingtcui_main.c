#include "config.h"

#include <stdarg.h>

#include "windef.h"
#include "winbase.h"
#include "hstring.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(gamingtcui);

BOOL WINAPI DllMain(HINSTANCE hInstDll, DWORD fdwReason, LPVOID lpvReserved)
{
    TRACE("(%p, %d, %p)\n", hInstDll, fdwReason, lpvReserved);

    switch (fdwReason)
    {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hInstDll);
            break;
    }

    return TRUE;
}

typedef void (WINAPI *GameUICompletionRoutine)(HRESULT returnCode, void *context);

HRESULT WINAPI ShowProfileCardUI(HSTRING targetUserXuid, GameUICompletionRoutine completionRoutine, void *context)
{
    FIXME("%p %p %p: stub\n", targetUserXuid, completionRoutine, context);

    if (completionRoutine)
        completionRoutine(S_OK, context);

    return S_OK;
}

typedef void (WINAPI *PlayerPickerUICompletionRoutine)(HRESULT returnCode, void *context,
        const HSTRING *selectedXuids, size_t selectedXuidsCount);

HRESULT WINAPI ShowPlayerPickerUI(HSTRING promptDisplayText, const HSTRING *xuids, size_t xuidsCount,
        const HSTRING *preSelectedXuids, size_t preSelectedXuidsCount, size_t minSelectionCount,
        size_t maxSelectionCount, PlayerPickerUICompletionRoutine completionRoutine, void *context)
{
    FIXME("%p %p %lu %p %lu %lu %lu %p %p stub.\n", promptDisplayText, xuids, xuidsCount, preSelectedXuids,
            preSelectedXuidsCount, minSelectionCount, maxSelectionCount, completionRoutine, context);

    if (completionRoutine)
        completionRoutine(S_OK, context, preSelectedXuids, preSelectedXuidsCount);

    return S_OK;
}

HRESULT WINAPI ProcessPendingGameUI(BOOL waitForCompletion)
{
    FIXME("%d: stub\n", waitForCompletion);

    return S_OK;
}
