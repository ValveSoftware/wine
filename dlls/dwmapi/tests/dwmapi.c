/*
 * Copyright 2019 Gabriel Ivăncescu for CodeWeavers
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

#include <windows.h>
#include <dwmapi.h>
#include "wine/test.h"

static HWND test_wnd;
static LRESULT WINAPI test_wndproc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    return DefWindowProcA(hwnd, message, wParam, lParam);
}

static void test_DwmGetWindowAttribute(void)
{
    BOOL nc_rendering;
    RECT rc, rc2;
    HRESULT hr;

    hr = DwmGetWindowAttribute(NULL, DWMWA_NCRENDERING_ENABLED, &nc_rendering, sizeof(nc_rendering));
    ok(hr == E_HANDLE || broken(hr == E_INVALIDARG) /* Vista */, "DwmGetWindowAttribute(DWMWA_NCRENDERING_ENABLED) returned 0x%08x.\n", hr);
    hr = DwmGetWindowAttribute(test_wnd, DWMWA_NCRENDERING_ENABLED, NULL, sizeof(nc_rendering));
    ok(hr == E_INVALIDARG, "DwmGetWindowAttribute(DWMWA_NCRENDERING_ENABLED) returned 0x%08x.\n", hr);
    hr = DwmGetWindowAttribute(test_wnd, DWMWA_NCRENDERING_ENABLED, &nc_rendering, 0);
    ok(hr == E_INVALIDARG, "DwmGetWindowAttribute(DWMWA_NCRENDERING_ENABLED) returned 0x%08x.\n", hr);
    nc_rendering = FALSE;
    hr = DwmGetWindowAttribute(test_wnd, 0xdeadbeef, &nc_rendering, sizeof(nc_rendering));
    ok(hr == E_INVALIDARG, "DwmGetWindowAttribute(0xdeadbeef) returned 0x%08x.\n", hr);

    nc_rendering = 0xdeadbeef;
    hr = DwmGetWindowAttribute(test_wnd, DWMWA_NCRENDERING_ENABLED, &nc_rendering, sizeof(nc_rendering));
    ok(hr == S_OK, "DwmGetWindowAttribute(DWMWA_NCRENDERING_ENABLED) failed 0x%08x.\n", hr);
    ok(nc_rendering == FALSE || nc_rendering == TRUE, "non-boolean value 0x%x.\n", nc_rendering);

    hr = DwmGetWindowAttribute(test_wnd, DWMWA_EXTENDED_FRAME_BOUNDS, &rc, sizeof(rc) - 1);
    ok(hr == HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER) || broken(hr == E_INVALIDARG) /* Vista */,
       "DwmGetWindowAttribute(DWMWA_EXTENDED_FRAME_BOUNDS) returned 0x%08x.\n", hr);
    hr = DwmGetWindowAttribute(test_wnd, DWMWA_EXTENDED_FRAME_BOUNDS, &rc, sizeof(rc));
    if (hr != E_HANDLE && hr != DWM_E_COMPOSITIONDISABLED /* Vista */)  /* composition is on */
    {
        /* For top-level Windows, the returned rect is always at least as large as GetWindowRect */
        GetWindowRect(test_wnd, &rc2);
        ok(hr == S_OK, "DwmGetWindowAttribute(DWMWA_EXTENDED_FRAME_BOUNDS) failed 0x%08x.\n", hr);
        ok(rc.left >= rc2.left && rc.right <= rc2.right && rc.top >= rc2.top && rc.bottom <= rc2.bottom,
           "returned rect %s not enclosed in window rect %s.\n", wine_dbgstr_rect(&rc), wine_dbgstr_rect(&rc2));
    }
}

START_TEST(dwmapi)
{
    HINSTANCE inst = GetModuleHandleA(NULL);
    WNDCLASSA cls;

    cls.style = 0;
    cls.lpfnWndProc = test_wndproc;
    cls.cbClsExtra = 0;
    cls.cbWndExtra = 0;
    cls.hInstance = inst;
    cls.hIcon = 0;
    cls.hCursor = LoadCursorA(0, (LPCSTR)IDC_ARROW);
    cls.hbrBackground = GetStockObject(WHITE_BRUSH);
    cls.lpszMenuName = NULL;
    cls.lpszClassName = "Test";
    RegisterClassA(&cls);

    test_wnd = CreateWindowExA(0, "Test", "Test Window", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                               100, 100, 200, 200, 0, 0, 0, NULL);
    ok(test_wnd != NULL, "Failed to create test window.\n");

    test_DwmGetWindowAttribute();

    DestroyWindow(test_wnd);
    UnregisterClassA("Test", inst);
}
