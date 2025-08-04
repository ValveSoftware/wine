/*
 * Copyright 2010 Maarten Lankhorst for CodeWeavers
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

#include "wine/test.h"

#define COBJMACROS

#ifdef STANDALONE
#include "initguid.h"
#endif

#include "unknwn.h"
#include "uuids.h"
#include "mmdeviceapi.h"
#include "devpkey.h"
#include "ks.h"
#include "ksmedia.h"
#include "mmreg.h"
#include "propvarutil.h"

static BOOL (WINAPI *pIsWow64Process)(HANDLE, BOOL *);

static const WCHAR software_renderW[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\MMDevices\\Audio\\Render";


static void test_propertystore(IPropertyStore *store)
{
    const WAVEFORMATEXTENSIBLE *format;
    HRESULT hr;
    PROPVARIANT pv;
    char temp[128];
    temp[sizeof(temp)-1] = 0;

    pv.vt = VT_EMPTY;
    hr = IPropertyStore_GetValue(store, &PKEY_AudioEndpoint_GUID, &pv);
    ok(hr == S_OK, "Failed with %08lx\n", hr);
    ok(pv.vt == VT_LPWSTR, "Value should be %i, is %i\n", VT_LPWSTR, pv.vt);
    WideCharToMultiByte(CP_ACP, 0, pv.pwszVal, -1, temp, sizeof(temp)-1, NULL, NULL);
    trace("guid: %s\n", temp);
    PropVariantClear(&pv);

    pv.vt = VT_EMPTY;
    hr = IPropertyStore_GetValue(store, (const PROPERTYKEY*)&DEVPKEY_DeviceInterface_FriendlyName, &pv);
    ok(hr == S_OK, "Failed with %08lx\n", hr);
    ok(pv.vt == VT_LPWSTR && pv.pwszVal, "FriendlyName value had wrong type: 0x%x or was NULL\n", pv.vt);
    PropVariantClear(&pv);

    pv.vt = VT_EMPTY;
    hr = IPropertyStore_GetValue(store, (const PROPERTYKEY*)&DEVPKEY_DeviceInterface_Enabled, &pv);
    ok(hr == S_OK, "Failed with %08lx\n", hr);
    ok(pv.vt == VT_EMPTY, "Key should not be found\n");
    PropVariantClear(&pv);

    pv.vt = VT_EMPTY;
    hr = IPropertyStore_GetValue(store, (const PROPERTYKEY*)&DEVPKEY_DeviceInterface_ClassGuid, &pv);
    ok(hr == S_OK, "Failed with %08lx\n", hr);
    ok(pv.vt == VT_EMPTY, "Key should not be found\n");
    PropVariantClear(&pv);

    pv.vt = VT_EMPTY;
    hr = IPropertyStore_GetValue(store, (const PROPERTYKEY *)&PKEY_AudioEngine_DeviceFormat, &pv);
    ok(hr == S_OK, "Failed with %08lx\n", hr);
    ok(pv.vt == VT_BLOB, "Got type %u\n", pv.vt);
    ok(pv.blob.cbSize == sizeof(WAVEFORMATEXTENSIBLE), "Got size %lu\n", pv.blob.cbSize);
    format = (const void *)pv.blob.pBlobData;
    ok(format->Format.wFormatTag == WAVE_FORMAT_EXTENSIBLE, "Got format tag %#x\n", format->Format.wFormatTag);
    ok(format->Format.cbSize == sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX),
            "Got extra size %u\n", format->Format.cbSize);
    todo_wine ok(format->Format.wBitsPerSample == 16, "Got bit depth %u\n", format->Format.wBitsPerSample);
    todo_wine ok(IsEqualGUID(&format->SubFormat, &KSDATAFORMAT_SUBTYPE_PCM),
            "Got subformat %s\n", debugstr_guid(&format->SubFormat));
    PropVariantClear(&pv);
}

static void test_deviceinterface(IPropertyStore *store)
{
    HRESULT hr;
    PROPVARIANT pv;

    static const PROPERTYKEY deviceinterface_key = {
        {0x233164c8, 0x1b2c, 0x4c7d, {0xbc, 0x68, 0xb6, 0x71, 0x68, 0x7a, 0x25, 0x67}}, 1
    };

    pv.vt = VT_EMPTY;
    hr = IPropertyStore_GetValue(store, &deviceinterface_key, &pv);
    ok(hr == S_OK, "GetValue failed: %08lx\n", hr);
    ok(pv.vt == VT_LPWSTR, "Got wrong variant type: 0x%x\n", pv.vt);
    trace("device interface: %s\n", wine_dbgstr_w(pv.pwszVal));
    PropVariantClear(&pv);
}

static void test_getat(IPropertyStore *store)
{
    HRESULT hr;
    DWORD propcount;
    DWORD prop;
    PROPERTYKEY pkey;
    BOOL found_name = FALSE;
    BOOL found_desc = FALSE;
    char temp[128];
    temp[sizeof(temp)-1] = 0;

    hr = IPropertyStore_GetCount(store, &propcount);

    ok(hr == S_OK, "Failed with %08lx\n", hr);
    ok(propcount > 0, "Propcount %ld should be greater than zero\n", propcount);

    for (prop = 0; prop < propcount; prop++) {
	hr = IPropertyStore_GetAt(store, prop, &pkey);
	ok(hr == S_OK, "Failed with %08lx\n", hr);
	if (IsEqualPropertyKey(pkey, DEVPKEY_Device_FriendlyName))
	    found_name = TRUE;
	if (IsEqualPropertyKey(pkey, DEVPKEY_Device_DeviceDesc))
	    found_desc = TRUE;
    }
    ok(found_name ||
            broken(!found_name) /* vista */, "DEVPKEY_Device_FriendlyName not found\n");
    ok(found_desc, "DEVPKEY_Device_DeviceDesc not found\n");
}

static const VARIANT_BOOL vt_bool_vals[] = { VARIANT_TRUE, VARIANT_FALSE };
static const GUID vt_clsid_vals[] =  { { 1 }, { 2 } };
static const BYTE vt_blob_val[] = { 1, 2, 3, 4, 5, 6 };
static const WCHAR vt_bstr_val[] = L"Test1";

static ULONG set_propvariant_for_vt(PROPVARIANT *pv, VARTYPE vt, const void *val, DWORD size)
{
    ULONG elems = 1;

    PropVariantInit(pv);
    pv->vt = vt;
    switch (vt)
    {
        case VT_BOOL:
            pv->boolVal = ((const VARIANT_BOOL *)val)[0];
            break;

        case VT_BOOL | VT_VECTOR:
            pv->cabool.cElems = size / sizeof(*pv->cabool.pElems);
            pv->cabool.pElems = CoTaskMemAlloc(size);
            memcpy(pv->cabool.pElems, val, size);
            elems = pv->cabool.cElems;
            break;

        case VT_CLSID:
            pv->puuid = CoTaskMemAlloc(sizeof(*pv->puuid));
            *pv->puuid = ((const GUID *)val)[0];
            break;

        case VT_CLSID | VT_VECTOR:
            pv->cauuid.cElems = size / sizeof(*pv->cauuid.pElems);
            pv->cauuid.pElems = CoTaskMemAlloc(size);
            memcpy(pv->cauuid.pElems, val, size);
            elems = pv->cauuid.cElems;
            break;

        case VT_BSTR:
            pv->bstrVal = SysAllocString((const WCHAR *)val);
            break;

        case VT_BLOB:
            pv->blob.cbSize = size;
            pv->blob.pBlobData = CoTaskMemAlloc(size);
            memcpy(pv->blob.pBlobData, val, size);
            break;
    }

    return elems;
}

static BOOL compare_propvariant(PROPVARIANT *pv, PROPVARIANT *pv2)
{
    unsigned int i;

    if (pv->vt != pv2->vt)
        return FALSE;

    switch (pv->vt)
    {
        case VT_BSTR:
        case VT_CLSID:
            return !PropVariantCompareEx(pv, pv2, 0, 0);

        case VT_BOOL:
            return pv->boolVal == pv2->boolVal;

        case VT_BOOL | VT_VECTOR:
            if (pv->cabool.cElems != pv2->cabool.cElems)
                return FALSE;

            for (i = 0; i < pv->cabool.cElems; i++)
            {
                if (pv->cabool.pElems[i] != pv2->cabool.pElems[i])
                    return FALSE;
            }
            return TRUE;

        case VT_CLSID | VT_VECTOR:
            if (pv->cauuid.cElems != pv2->cauuid.cElems)
                return FALSE;

            for (i = 0; i < pv->cauuid.cElems; i++)
            {
                if (memcmp(&pv->cauuid.pElems[i], &pv2->cauuid.pElems[i], sizeof(*pv->cauuid.pElems)))
                    return FALSE;
            }
            return TRUE;

        case VT_BLOB:
            if (pv->blob.cbSize != pv2->blob.cbSize)
                return FALSE;

            return !memcmp(pv->blob.pBlobData, pv2->blob.pBlobData, pv->blob.cbSize);
    }

    return FALSE;
}

struct reg_serialized {
    VARTYPE vt;
    WORD unk; /* Seems like mostly uninitialized memory... */
    ULONG elems;
    BYTE data[];
};

static void test_setvalue_on_wow64(IPropertyStore *store)
{
    static const struct
    {
        VARTYPE vt;
        const void *data;
        DWORD data_size;

        HRESULT expected_hr;
        DWORD expected_reg_type;
        DWORD expected_size;
        BOOL todo_hr;
        BOOL todo_data;
    } propvar_tests[] =
    {
        { VT_BOOL, vt_bool_vals, sizeof(vt_bool_vals) / 2, S_OK, REG_BINARY, 0xa },
        { VT_BOOL | VT_VECTOR, vt_bool_vals, sizeof(vt_bool_vals), S_OK, REG_BINARY, 0xc },
        { VT_CLSID, vt_clsid_vals, sizeof(vt_clsid_vals) / 2, S_OK, REG_BINARY, 0x18 },
        { VT_CLSID | VT_VECTOR, vt_clsid_vals, sizeof(vt_clsid_vals), S_OK, REG_BINARY, 0x28 },
        { VT_BSTR, vt_bstr_val, sizeof(vt_bstr_val), S_OK, REG_BINARY, 0x14, TRUE, TRUE },
        { VT_BLOB, vt_blob_val, sizeof(vt_blob_val), S_OK, REG_BINARY, 0xe, .todo_data = TRUE },
    };
    PROPVARIANT pv, pv2;
    HRESULT hr;
    LONG ret;
    WCHAR *guidW;
    HKEY root, props, devkey;
    DWORD type, regval, size;
    unsigned int i;
    BYTE buf[256];

    static const PROPERTYKEY PKEY_Bogus = {
        {0x1da5d803, 0xd492, 0x4edd, {0x8c, 0x23, 0xe0, 0xc0, 0xff, 0xee, 0x7f, 0x00}}, 0x7f
    };
    static const PROPERTYKEY PKEY_Bogus2 = {
        {0x1da5d803, 0xd492, 0x4edd, {0x8c, 0x23, 0xe0, 0xc0, 0xff, 0xee, 0x7f, 0x00}}, 0x80
    };
    static const WCHAR bogusW[] = L"{1DA5D803-D492-4EDD-8C23-E0C0FFEE7F00},127";
    static const WCHAR bogus2W[] = L"{1DA5D803-D492-4EDD-8C23-E0C0FFEE7F00},128";

    PropVariantInit(&pv);

    pv.vt = VT_EMPTY;
    hr = IPropertyStore_GetValue(store, &PKEY_AudioEndpoint_GUID, &pv);
    ok(hr == S_OK, "Failed to get Endpoint GUID: %08lx\n", hr);

    guidW = pv.pwszVal;

    pv.vt = VT_UI4;
    pv.ulVal = 0xAB;

    hr = IPropertyStore_SetValue(store, &PKEY_Bogus, &pv);
    ok(hr == S_OK || hr == E_ACCESSDENIED, "SetValue failed: %08lx\n", hr);
    if (hr != S_OK)
    {
        win_skip("Missing permission to write to registry\n");
        return;
    }

    pv.ulVal = 0x00;

    hr = IPropertyStore_GetValue(store, &PKEY_Bogus, &pv);
    ok(hr == S_OK, "GetValue failed: %08lx\n", hr);
    ok(pv.ulVal == 0xAB, "Got wrong value: 0x%lx\n", pv.ulVal);

    /* should find the key in 64-bit view */
    ret = RegOpenKeyExW(HKEY_LOCAL_MACHINE, software_renderW, 0, KEY_READ|KEY_WOW64_64KEY, &root);
    ok(ret == ERROR_SUCCESS, "Couldn't open mmdevices Render key: %lu\n", ret);

    ret = RegOpenKeyExW(root, guidW, 0, KEY_READ|KEY_WOW64_64KEY, &devkey);
    ok(ret == ERROR_SUCCESS, "Couldn't open mmdevice guid key: %lu\n", ret);

    ret = RegOpenKeyExW(devkey, L"Properties", 0, KEY_READ|KEY_WOW64_64KEY, &props);
    ok(ret == ERROR_SUCCESS, "Couldn't open mmdevice property key: %lu\n", ret);

    /* Note: the registry key exists even without calling IPropStore::Commit */
    size = sizeof(regval);
    ret = RegQueryValueExW(props, bogusW, NULL, &type, (LPBYTE)&regval, &size);
    ok(ret == ERROR_SUCCESS, "Couldn't get bogus propertykey value: %lu\n", ret);
    ok(type == REG_DWORD, "Got wrong value type: %lu\n", type);
    ok(regval == 0xAB, "Got wrong value: 0x%lx\n", regval);

    for (i = 0; i < ARRAY_SIZE(propvar_tests); i++)
    {
        struct reg_serialized *reg_val;
        ULONG expected_elems;

        winetest_push_context("Test %u", i);

        expected_elems = set_propvariant_for_vt(&pv, propvar_tests[i].vt, propvar_tests[i].data, propvar_tests[i].data_size);
        hr = IPropertyStore_SetValue(store, &PKEY_Bogus2, &pv);
        todo_wine_if(propvar_tests[i].todo_hr) ok(hr == propvar_tests[i].expected_hr, "Unexpected hr %#lx.\n", hr);
        if (FAILED(hr))
        {
            winetest_pop_context();
            PropVariantClear(&pv);
            continue;
        }

        ret = RegQueryValueExW(props, bogus2W, NULL, &type, NULL, &size);
        ok(ret == ERROR_SUCCESS, "Couldn't get bogus propertykey value: %lu.\n", ret);
        ok(type == propvar_tests[i].expected_reg_type, "Unexpected registry value type %lu.\n", type);
        todo_wine_if(propvar_tests[i].todo_data) ok(size >= propvar_tests[i].expected_size, "Unexpected registry value size 0x%lx.\n", size);

        ret = RegQueryValueExW(props, bogus2W, NULL, &type, buf, &size);
        ok(ret == ERROR_SUCCESS, "Couldn't get bogus propertykey value: %lu.\n", ret);
        reg_val = (struct reg_serialized *)buf;
        todo_wine_if(propvar_tests[i].todo_data)
            ok(reg_val->vt == propvar_tests[i].vt, "Unexpected vt: %#x.\n", reg_val->vt);
        todo_wine_if(propvar_tests[i].todo_data)
            ok(reg_val->elems == expected_elems, "Unexpected elems: %lu.\n", reg_val->elems);
        todo_wine_if(propvar_tests[i].todo_data)
            ok(!memcmp(reg_val->data, propvar_tests[i].data, propvar_tests[i].data_size), "Unexpected data.\n");

        hr = IPropertyStore_GetValue(store, &PKEY_Bogus2, &pv2);
        ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);
        ok(compare_propvariant(&pv, &pv2), "Propvariant mismatch.\n");

        PropVariantClear(&pv);
        PropVariantClear(&pv2);
        winetest_pop_context();
    }

    RegCloseKey(props);
    RegCloseKey(devkey);
    RegCloseKey(root);

    CoTaskMemFree(guidW);

    /* should NOT find the key in 32-bit view */
    ret = RegOpenKeyExW(HKEY_LOCAL_MACHINE, software_renderW, 0, KEY_READ, &root);
    todo_wine
    ok(ret == 0 || broken(ret == ERROR_FILE_NOT_FOUND /* win10 < 2004 */),
       "Wrong error when opening mmdevices Render key: %lu\n", ret);
}

START_TEST(propstore)
{
    HRESULT hr;
    IMMDeviceCollection *collection;
    IMMDeviceEnumerator *mme = NULL;
    IPropertyStore *store;
    BOOL is_wow64 = FALSE;
    HMODULE hk32 = GetModuleHandleA("kernel32.dll");
    unsigned int i, count;

    pIsWow64Process = (void *)GetProcAddress(hk32, "IsWow64Process");

    if (pIsWow64Process)
        pIsWow64Process(GetCurrentProcess(), &is_wow64);

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_INPROC_SERVER, &IID_IMMDeviceEnumerator, (void**)&mme);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);

    hr = IMMDeviceEnumerator_EnumAudioEndpoints(mme, eRender, DEVICE_STATE_ACTIVE, &collection);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    hr = IMMDeviceCollection_GetCount(collection, &count);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);

    for (i = 0; i < count; ++i)
    {
        IMMDevice *dev;

        hr = IMMDeviceCollection_Item(collection, i, &dev);
        ok(hr == S_OK, "Got hr %#lx.\n", hr);

        store = NULL;
        hr = IMMDevice_OpenPropertyStore(dev, 3, &store);
        ok(hr == E_INVALIDARG, "Wrong hr returned: %08lx\n", hr);
        /* It seems on windows returning with E_INVALIDARG doesn't
         * set store to NULL, so just don't set store to non-null
         * before calling this function
         */
        ok(!store, "Got unexpected store %p\n", store);

        hr = IMMDevice_OpenPropertyStore(dev, STGM_READ, NULL);
        ok(hr == E_POINTER, "Wrong hr returned: %08lx\n", hr);

        store = NULL;
        hr = IMMDevice_OpenPropertyStore(dev, STGM_READWRITE, &store);
        if (hr == E_ACCESSDENIED)
            hr = IMMDevice_OpenPropertyStore(dev, STGM_READ, &store);
        ok(hr == S_OK, "Opening valid store returned %08lx\n", hr);

        test_propertystore(store);
        test_deviceinterface(store);
        test_getat(store);
        if (is_wow64)
            test_setvalue_on_wow64(store);

        IPropertyStore_Release(store);
        IMMDevice_Release(dev);
    }

    IMMDeviceCollection_Release(collection);
    IMMDeviceEnumerator_Release(mme);
    CoUninitialize();
}
