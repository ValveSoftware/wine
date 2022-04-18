/*
 * Copyright 2022 Zhiyi Zhang for CodeWeavers
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */
#define COBJMACROS
#include <stdarg.h>

#include "windef.h"
#include "winbase.h"
#include "winerror.h"
#include "winstring.h"

#include "initguid.h"
#include "roapi.h"

#define WIDL_using_Windows_Foundation_Metadata
#include "windows.foundation.metadata.h"

#include "wine/test.h"

static HRESULT (WINAPI *pRoActivateInstance)(HSTRING, IInspectable **);
static HRESULT (WINAPI *pRoGetActivationFactory)(HSTRING, REFIID, void **);
static HRESULT (WINAPI *pRoInitialize)(RO_INIT_TYPE);
static void    (WINAPI *pRoUninitialize)(void);
static HRESULT (WINAPI *pWindowsCreateString)(LPCWSTR, UINT32, HSTRING *);
static HRESULT (WINAPI *pWindowsDeleteString)(HSTRING);
static WCHAR * (WINAPI *pWindowsGetStringRawBuffer)(HSTRING, UINT32 *);

static void test_IApiInformationStatics(void)
{
    static const WCHAR *class_name = L"Windows.Foundation.Metadata.ApiInformation";
    IAgileObject *agile_object = NULL, *tmp_agile_object = NULL;
    IInspectable *inspectable = NULL, *tmp_inspectable = NULL;
    IApiInformationStatics *statics = NULL;
    IActivationFactory *factory = NULL;
    HSTRING str, str2;
    BOOLEAN ret;
    HRESULT hr;

    hr = pRoInitialize(RO_INIT_MULTITHREADED);
    ok(hr == S_OK, "RoInitialize failed, hr %#lx.\n", hr);

    hr = pWindowsCreateString(class_name, wcslen(class_name), &str);
    ok(hr == S_OK, "WindowsCreateString failed, hr %#lx.\n", hr);

    hr = pRoGetActivationFactory(str, &IID_IActivationFactory, (void **)&factory);
    ok(hr == S_OK || broken(hr == REGDB_E_CLASSNOTREG), "RoGetActivationFactory failed, hr %#lx.\n", hr);
    pWindowsDeleteString(str);
    if (hr == REGDB_E_CLASSNOTREG)
    {
        win_skip("%s runtimeclass not registered, skipping tests.\n", wine_dbgstr_w(class_name));
        pRoUninitialize();
        return;
    }

    hr = IActivationFactory_QueryInterface(factory, &IID_IInspectable, (void **)&inspectable);
    ok(hr == S_OK, "QueryInterface IID_IInspectable failed, hr %#lx.\n", hr);

    hr = IActivationFactory_QueryInterface(factory, &IID_IAgileObject, (void **)&agile_object);
    ok(hr == S_OK, "QueryInterface IID_IAgileObject failed, hr %#lx.\n", hr);

    hr = IActivationFactory_QueryInterface(factory, &IID_IApiInformationStatics, (void **)&statics);
    ok(hr == S_OK, "QueryInterface IID_IApiInformationStatics failed, hr %#lx.\n", hr);

    hr = IApiInformationStatics_QueryInterface(statics, &IID_IInspectable, (void **)&tmp_inspectable);
    ok(hr == S_OK, "QueryInterface IID_IInspectable failed, hr %#lx.\n", hr);
    ok(tmp_inspectable == inspectable, "QueryInterface IID_IInspectable returned %p, expected %p.\n",
            tmp_inspectable, inspectable);
    IInspectable_Release(tmp_inspectable);

    hr = IApiInformationStatics_QueryInterface(statics, &IID_IAgileObject, (void **)&tmp_agile_object);
    ok(hr == S_OK, "QueryInterface IID_IAgileObject failed, hr %#lx.\n", hr);
    ok(tmp_agile_object == agile_object, "QueryInterface IID_IAgileObject returned %p, expected %p.\n",
            tmp_agile_object, agile_object);
    IAgileObject_Release(tmp_agile_object);

    /* IsTypePresent() */
    hr = pWindowsCreateString(L"Windows.Foundation.FoundationContract",
            wcslen(L"Windows.Foundation.FoundationContract"), &str);
    ok(hr == S_OK, "WindowsCreateString failed, hr %#lx.\n", hr);

    hr = IApiInformationStatics_IsTypePresent(statics, NULL, &ret);
    ok(hr == E_INVALIDARG, "IsTypePresent failed, hr %#lx.\n", hr);

#if 0 /* Crash on Windows */
    hr = IApiInformationStatics_IsTypePresent(statics, str, NULL);
    ok(hr == E_INVALIDARG, "IsTypePresent failed, hr %#lx.\n", hr);
#endif

    ret = FALSE;
    hr = IApiInformationStatics_IsTypePresent(statics, str, &ret);
    todo_wine
    ok(hr == S_OK, "IsTypePresent failed, hr %#lx.\n", hr);
    todo_wine
    ok(ret == TRUE, "IsTypePresent returned FALSE.\n");

    pWindowsDeleteString(str);

    /* IsMethodPresent() */
    hr = pWindowsCreateString(L"Windows.Foundation.Metadata.IApiInformationStatics",
            wcslen(L"Windows.Foundation.Metadata.IApiInformationStatics"), &str);
    ok(hr == S_OK, "WindowsCreateString failed, hr %#lx.\n", hr);
    hr = pWindowsCreateString(L"IsTypePresent", wcslen(L"IsTypePresent"), &str2);
    ok(hr == S_OK, "WindowsCreateString failed, hr %#lx.\n", hr);

    hr = IApiInformationStatics_IsMethodPresent(statics, NULL, str2, &ret);
    ok(hr == E_INVALIDARG, "IsMethodPresent failed, hr %#lx.\n", hr);

    ret = TRUE;
    hr = IApiInformationStatics_IsMethodPresent(statics, str, NULL, &ret);
    todo_wine
    ok(hr == S_OK, "IsMethodPresent failed, hr %#lx.\n", hr);
    todo_wine
    ok(ret == FALSE, "IsMethodPresent returned TRUE.\n");

#if 0 /* Crash on Windows */
    hr = IApiInformationStatics_IsMethodPresent(statics, str, str2, NULL);
    ok(hr == E_INVALIDARG, "IsMethodPresent failed, hr %#lx.\n", hr);
#endif

    ret = FALSE;
    hr = IApiInformationStatics_IsMethodPresent(statics, str, str2, &ret);
    todo_wine
    ok(hr == S_OK, "IsMethodPresent failed, hr %#lx.\n", hr);
    todo_wine
    ok(ret == TRUE, "IsMethodPresent returned FALSE.\n");

    /* IsMethodPresentWithArity() */
    hr = IApiInformationStatics_IsMethodPresentWithArity(statics, NULL, str2, 1, &ret);
    ok(hr == E_INVALIDARG, "IsMethodPresentWithArity failed, hr %#lx.\n", hr);

    ret = TRUE;
    hr = IApiInformationStatics_IsMethodPresentWithArity(statics, str, NULL, 1, &ret);
    todo_wine
    ok(hr == S_OK, "IsMethodPresentWithArity failed, hr %#lx.\n", hr);
    todo_wine
    ok(ret == FALSE, "IsMethodPresentWithArity returned FALSE.\n");

    ret = TRUE;
    hr = IApiInformationStatics_IsMethodPresentWithArity(statics, str, str2, 0, &ret);
    todo_wine
    ok(hr == S_OK, "IsMethodPresentWithArity failed, hr %#lx.\n", hr);
    todo_wine
    ok(ret == FALSE, "IsMethodPresentWithArity returned FALSE.\n");

    ret = TRUE;
    hr = IApiInformationStatics_IsMethodPresentWithArity(statics, str, str2, 2, &ret);
    todo_wine
    ok(hr == S_OK, "IsMethodPresentWithArity failed, hr %#lx.\n", hr);
    todo_wine
    ok(ret == FALSE, "IsMethodPresentWithArity returned FALSE.\n");

#if 0 /* Crash on Windows */
    hr = IApiInformationStatics_IsMethodPresentWithArity(statics, str, str2, 1, NULL);
    ok(hr == E_INVALIDARG, "IsMethodPresentWithArity failed, hr %#lx.\n", hr);
#endif

    ret = FALSE;
    hr = IApiInformationStatics_IsMethodPresentWithArity(statics, str, str2, 1, &ret);
    todo_wine
    ok(hr == S_OK, "IsMethodPresentWithArity failed, hr %#lx.\n", hr);
    todo_wine
    ok(ret == TRUE, "IsMethodPresentWithArity returned FALSE.\n");

    pWindowsDeleteString(str2);
    pWindowsDeleteString(str);

    /* IsEventPresent() */
    hr = pWindowsCreateString(L"Windows.Devices.Enumeration.IDeviceWatcher",
            wcslen(L"Windows.Devices.Enumeration.IDeviceWatcher"), &str);
    ok(hr == S_OK, "WindowsCreateString failed, hr %#lx.\n", hr);
    hr = pWindowsCreateString(L"Added", wcslen(L"Added"), &str2);
    ok(hr == S_OK, "WindowsCreateString failed, hr %#lx.\n", hr);

    hr = IApiInformationStatics_IsEventPresent(statics, NULL, str2, &ret);
    ok(hr == E_INVALIDARG, "IsEventPresent failed, hr %#lx.\n", hr);

    ret = TRUE;
    hr = IApiInformationStatics_IsEventPresent(statics, str, NULL, &ret);
    todo_wine
    ok(hr == S_OK, "IsEventPresent failed, hr %#lx.\n", hr);
    todo_wine
    ok(ret == FALSE, "IsEventPresent returned FALSE.\n");

#if 0 /* Crash on Windows */
    hr = IApiInformationStatics_IsEventPresent(statics, str, str2, NULL);
    ok(hr == E_INVALIDARG, "IsEventPresent failed, hr %#lx.\n", hr);
#endif

    ret = FALSE;
    hr = IApiInformationStatics_IsEventPresent(statics, str, str2, &ret);
    todo_wine
    ok(hr == S_OK, "IsEventPresent failed, hr %#lx.\n", hr);
    todo_wine
    ok(ret == TRUE, "IsEventPresent returned FALSE.\n");

    pWindowsDeleteString(str2);
    pWindowsDeleteString(str);

    /* IsPropertyPresent() */
    hr = pWindowsCreateString(L"Windows.Devices.Enumeration.IDeviceWatcher",
            wcslen(L"Windows.Devices.Enumeration.IDeviceWatcher"), &str);
    ok(hr == S_OK, "WindowsCreateString failed, hr %#lx.\n", hr);
    hr = pWindowsCreateString(L"Status", wcslen(L"Status"), &str2);
    ok(hr == S_OK, "WindowsCreateString failed, hr %#lx.\n", hr);

    hr = IApiInformationStatics_IsPropertyPresent(statics, NULL, str2, &ret);
    ok(hr == E_INVALIDARG, "IsPropertyPresent failed, hr %#lx.\n", hr);

    ret = TRUE;
    hr = IApiInformationStatics_IsPropertyPresent(statics, str, NULL, &ret);
    todo_wine
    ok(hr == S_OK, "IsPropertyPresent failed, hr %#lx.\n", hr);
    todo_wine
    ok(ret == FALSE, "IsPropertyPresent returned TRUE.\n");

#if 0 /* Crash on Windows */
    hr = IApiInformationStatics_IsPropertyPresent(statics, str, str2, NULL);
    ok(hr == E_INVALIDARG, "IsPropertyPresent failed, hr %#lx.\n", hr);
#endif

    ret = FALSE;
    hr = IApiInformationStatics_IsPropertyPresent(statics, str, str2, &ret);
    todo_wine
    ok(hr == S_OK, "IsPropertyPresent failed, hr %#lx.\n", hr);
    todo_wine
    ok(ret == TRUE, "IsPropertyPresent returned FALSE.\n");

    pWindowsDeleteString(str2);
    pWindowsDeleteString(str);

    /* IsReadOnlyPropertyPresent() */
    hr = pWindowsCreateString(L"Windows.Devices.Enumeration.IDeviceWatcher",
            wcslen(L"Windows.Devices.Enumeration.IDeviceWatcher"), &str);
    ok(hr == S_OK, "WindowsCreateString failed, hr %#lx.\n", hr);
    hr = pWindowsCreateString(L"Id", wcslen(L"Id"), &str2);
    ok(hr == S_OK, "WindowsCreateString failed, hr %#lx.\n", hr);

    hr = IApiInformationStatics_IsReadOnlyPropertyPresent(statics, NULL, str2, &ret);
    ok(hr == E_INVALIDARG, "IsReadOnlyPropertyPresent failed, hr %#lx.\n", hr);

    ret = TRUE;
    hr = IApiInformationStatics_IsReadOnlyPropertyPresent(statics, str, NULL, &ret);
    todo_wine
    ok(hr == S_OK, "IsReadOnlyPropertyPresent failed, hr %#lx.\n", hr);
    todo_wine
    ok(ret == FALSE, "IsReadOnlyPropertyPresent returned TRUE.\n");

#if 0 /* Crash on Windows */
    hr = IApiInformationStatics_IsReadOnlyPropertyPresent(statics, str, str2, NULL);
    ok(hr == E_INVALIDARG, "IsReadOnlyPropertyPresent failed, hr %#lx.\n", hr);
#endif

    ret = TRUE;
    hr = IApiInformationStatics_IsReadOnlyPropertyPresent(statics, str, str2, &ret);
    todo_wine
    ok(hr == S_OK, "IsReadOnlyPropertyPresent failed, hr %#lx.\n", hr);
    todo_wine
    ok(ret == FALSE, "IsReadOnlyPropertyPresent returned TRUE.\n");

    pWindowsDeleteString(str2);
    pWindowsDeleteString(str);

    /* IsWriteablePropertyPresent() */
    hr = pWindowsCreateString(L"Windows.Gaming.Input.ForceFeedback.IForceFeedbackEffect",
            wcslen(L"Windows.Gaming.Input.ForceFeedback.IForceFeedbackEffect"), &str);
    ok(hr == S_OK, "WindowsCreateString failed, hr %#lx.\n", hr);
    hr = pWindowsCreateString(L"Gain", wcslen(L"Gain"), &str2);
    ok(hr == S_OK, "WindowsCreateString failed, hr %#lx.\n", hr);

    hr = IApiInformationStatics_IsWriteablePropertyPresent(statics, NULL, str2, &ret);
    ok(hr == E_INVALIDARG, "IsWriteablePropertyPresent failed, hr %#lx.\n", hr);

    ret = TRUE;
    hr = IApiInformationStatics_IsWriteablePropertyPresent(statics, str, NULL, &ret);
    todo_wine
    ok(hr == S_OK, "IsWriteablePropertyPresent failed, hr %#lx.\n", hr);
    todo_wine
    ok(ret == FALSE, "IsWriteablePropertyPresent returned TRUE.\n");

#if 0 /* Crash on Windows */
    hr = IApiInformationStatics_IsWriteablePropertyPresent(statics, str, str2, NULL);
    ok(hr == E_INVALIDARG, "IsWriteablePropertyPresent failed, hr %#lx.\n", hr);
#endif

    ret = FALSE;
    hr = IApiInformationStatics_IsWriteablePropertyPresent(statics, str, str2, &ret);
    todo_wine
    ok(hr == S_OK, "IsWriteablePropertyPresent failed, hr %#lx.\n", hr);
    todo_wine
    ok(ret == TRUE || broken(ret == FALSE) /* Win10 1507 */,
            "IsWriteablePropertyPresent returned FALSE.\n");

    pWindowsDeleteString(str2);
    pWindowsDeleteString(str);

    /* IsEnumNamedValuePresent */
    hr = pWindowsCreateString(L"Windows.Foundation.Metadata.GCPressureAmount",
            wcslen(L"Windows.Foundation.Metadata.GCPressureAmount"), &str);
    ok(hr == S_OK, "WindowsCreateString failed, hr %#lx.\n", hr);
    hr = pWindowsCreateString(L"Low", wcslen(L"Low"), &str2);
    ok(hr == S_OK, "WindowsCreateString failed, hr %#lx.\n", hr);

    hr = IApiInformationStatics_IsEnumNamedValuePresent(statics, NULL, str2, &ret);
    ok(hr == E_INVALIDARG, "IsEnumNamedValuePresent failed, hr %#lx.\n", hr);

    ret = TRUE;
    hr = IApiInformationStatics_IsEnumNamedValuePresent(statics, str, NULL, &ret);
    todo_wine
    ok(hr == S_OK, "IsEnumNamedValuePresent failed, hr %#lx.\n", hr);
    todo_wine
    ok(ret == FALSE, "IsEnumNamedValuePresent returned TRUE.\n");

#if 0 /* Crash on Windows */
    hr = IApiInformationStatics_IsEnumNamedValuePresent(statics, str, str2, NULL);
    ok(hr == E_INVALIDARG, "IsEnumNamedValuePresent failed, hr %#lx.\n", hr);
#endif

    ret = FALSE;
    hr = IApiInformationStatics_IsEnumNamedValuePresent(statics, str, str2, &ret);
    todo_wine
    ok(hr == S_OK, "IsEnumNamedValuePresent failed, hr %#lx.\n", hr);
    todo_wine
    ok(ret == TRUE, "IsEnumNamedValuePresent returned FALSE.\n");

    ret = TRUE;
    hr = IApiInformationStatics_IsEnumNamedValuePresent(statics, str, str, &ret);
    todo_wine
    ok(hr == S_OK, "IsEnumNamedValuePresent failed, hr %#lx.\n", hr);
    todo_wine
    ok(ret == FALSE, "IsEnumNamedValuePresent returned TRUE.\n");

    pWindowsDeleteString(str2);
    pWindowsDeleteString(str);

    /* IsApiContractPresentByMajor */
    hr = pWindowsCreateString(L"Windows.Foundation.FoundationContract",
            wcslen(L"Windows.Foundation.FoundationContract"), &str);
    ok(hr == S_OK, "WindowsCreateString failed, hr %#lx.\n", hr);

    hr = IApiInformationStatics_IsApiContractPresentByMajor(statics, NULL, 1, &ret);
    ok(hr == E_INVALIDARG, "IsApiContractPresentByMajor failed, hr %#lx.\n", hr);

#if 0 /* Crash on Windows */
    hr = IApiInformationStatics_IsApiContractPresentByMajor(statics, str, 1, NULL);
    ok(hr == E_INVALIDARG, "IsApiContractPresentByMajor failed, hr %#lx.\n", hr);
#endif

    ret = FALSE;
    hr = IApiInformationStatics_IsApiContractPresentByMajor(statics, str, 1, &ret);
    ok(hr == S_OK, "IsApiContractPresentByMajor failed, hr %#lx.\n", hr);
    todo_wine
    ok(ret == TRUE, "IsApiContractPresentByMajor returned FALSE.\n");

    ret = FALSE;
    hr = IApiInformationStatics_IsApiContractPresentByMajor(statics, str, 0, &ret);
    ok(hr == S_OK, "IsApiContractPresentByMajor failed, hr %#lx.\n", hr);
    todo_wine
    ok(ret == TRUE, "IsApiContractPresentByMajor returned FALSE.\n");

    ret = TRUE;
    hr = IApiInformationStatics_IsApiContractPresentByMajor(statics, str, 999, &ret);
    ok(hr == S_OK, "IsApiContractPresentByMajor failed, hr %#lx.\n", hr);
    ok(ret == FALSE, "IsApiContractPresentByMajor returned TRUE.\n");

    pWindowsDeleteString(str);

    /* IsApiContractPresentByMajorAndMinor */
    hr = pWindowsCreateString(L"Windows.Foundation.FoundationContract",
            wcslen(L"Windows.Foundation.FoundationContract"), &str);
    ok(hr == S_OK, "WindowsCreateString failed, hr %#lx.\n", hr);

    hr = IApiInformationStatics_IsApiContractPresentByMajorAndMinor(statics, NULL, 1, 0, &ret);
    ok(hr == E_INVALIDARG, "IsApiContractPresentByMajorAndMinor failed, hr %#lx.\n", hr);

#if 0 /* Crash on Windows */
    hr = IApiInformationStatics_IsApiContractPresentByMajorAndMinor(statics, str, 1, 0, NULL);
    ok(hr == E_INVALIDARG, "IsApiContractPresentByMajorAndMinor failed, hr %#lx.\n", hr);
#endif

    ret = FALSE;
    hr = IApiInformationStatics_IsApiContractPresentByMajorAndMinor(statics, str, 1, 0, &ret);
    todo_wine
    ok(hr == S_OK, "IsApiContractPresentByMajorAndMinor failed, hr %#lx.\n", hr);
    todo_wine
    ok(ret == TRUE, "IsApiContractPresentByMajorAndMinor returned FALSE.\n");

    ret = FALSE;
    hr = IApiInformationStatics_IsApiContractPresentByMajorAndMinor(statics, str, 0, 999, &ret);
    todo_wine
    ok(hr == S_OK, "IsApiContractPresentByMajorAndMinor failed, hr %#lx.\n", hr);
    todo_wine
    ok(ret == TRUE, "IsApiContractPresentByMajorAndMinor returned FALSE.\n");

    ret = FALSE;
    hr = IApiInformationStatics_IsApiContractPresentByMajorAndMinor(statics, str, 1, 999, &ret);
    todo_wine
    ok(hr == S_OK, "IsApiContractPresentByMajorAndMinor failed, hr %#lx.\n", hr);
    todo_wine
    ok(ret == TRUE || broken(ret == FALSE) /* Win10 1507 */,
            "IsApiContractPresentByMajorAndMinor returned FALSE.\n");

    ret = TRUE;
    hr = IApiInformationStatics_IsApiContractPresentByMajorAndMinor(statics, str, 999, 999, &ret);
    todo_wine
    ok(hr == S_OK, "IsApiContractPresentByMajorAndMinor failed, hr %#lx.\n", hr);
    todo_wine
    ok(ret == FALSE, "IsApiContractPresentByMajorAndMinor returned TRUE.\n");

    pWindowsDeleteString(str);

    IApiInformationStatics_Release(statics);
    IAgileObject_Release(agile_object);
    IInspectable_Release(inspectable);
    IActivationFactory_Release(factory);
    pRoUninitialize();
}

START_TEST(wintypes)
{
    HMODULE combase;

    if (!(combase = LoadLibraryW(L"combase.dll")))
    {
        win_skip("Failed to load combase.dll, skipping tests.\n");
        return;
    }

#define LOAD_FUNCPTR(x)                                                      \
    if (!(p##x = (void *)GetProcAddress(combase, #x)))                       \
    {                                                                        \
        win_skip("Failed to find %s in combase.dll, skipping tests.\n", #x); \
        return;                                                              \
    }

    LOAD_FUNCPTR(RoActivateInstance)
    LOAD_FUNCPTR(RoGetActivationFactory)
    LOAD_FUNCPTR(RoInitialize)
    LOAD_FUNCPTR(RoUninitialize)
    LOAD_FUNCPTR(WindowsCreateString)
    LOAD_FUNCPTR(WindowsDeleteString)
    LOAD_FUNCPTR(WindowsGetStringRawBuffer)
#undef LOAD_FUNCPTR

    test_IApiInformationStatics();
}
