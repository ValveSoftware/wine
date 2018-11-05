/*
 * Desktop window class.
 *
 * Copyright 1994 Alexandre Julliard
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

#include "config.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "winnls.h"
#include "controls.h"
#include "wine/unicode.h"

static HBRUSH hbrushPattern;
static HBITMAP hbitmapWallPaper;
static SIZE bitmapSize;
static BOOL fTileWallPaper;


/*********************************************************************
 * desktop class descriptor
 */
const struct builtin_class_descr DESKTOP_builtin_class =
{
    (LPCWSTR)DESKTOP_CLASS_ATOM, /* name */
    CS_DBLCLKS,           /* style */
    WINPROC_DESKTOP,      /* proc */
    0,                    /* extra */
    0,                    /* cursor */
    (HBRUSH)(COLOR_BACKGROUND+1)    /* brush */
};


/***********************************************************************
 *           DESKTOP_LoadBitmap
 */
static HBITMAP DESKTOP_LoadBitmap( const WCHAR *filename )
{
    HBITMAP hbitmap;

    if (!filename[0]) return 0;
    hbitmap = LoadImageW( 0, filename, IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION | LR_LOADFROMFILE );
    if (!hbitmap)
    {
        WCHAR buffer[MAX_PATH];
        UINT len = GetWindowsDirectoryW( buffer, MAX_PATH - 2 );
        if (buffer[len - 1] != '\\') buffer[len++] = '\\';
        lstrcpynW( buffer + len, filename, MAX_PATH - len );
        hbitmap = LoadImageW( 0, buffer, IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION | LR_LOADFROMFILE );
    }
    return hbitmap;
}

/***********************************************************************
 *           init_wallpaper
 */
static void init_wallpaper( const WCHAR *wallpaper )
{
    HBITMAP hbitmap = DESKTOP_LoadBitmap( wallpaper );

    if (hbitmapWallPaper) DeleteObject( hbitmapWallPaper );
    hbitmapWallPaper = hbitmap;
    if (hbitmap)
    {
	BITMAP bmp;
	GetObjectA( hbitmap, sizeof(bmp), &bmp );
	bitmapSize.cx = (bmp.bmWidth != 0) ? bmp.bmWidth : 1;
	bitmapSize.cy = (bmp.bmHeight != 0) ? bmp.bmHeight : 1;
        fTileWallPaper = GetProfileIntA( "desktop", "TileWallPaper", 0 );
    }
}

/***********************************************************************
 *           DesktopWndProc
 */
LRESULT WINAPI DesktopWndProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
    static const WCHAR display_device_guid_propW[] = {
        '_','_','w','i','n','e','_','d','i','s','p','l','a','y','_',
        'd','e','v','i','c','e','_','g','u','i','d',0 };
    static const WCHAR guid_formatW[] = {
        '%','0','8','x','-','%','0','4','x','-','%','0','4','x','-','%','0','2','x','%','0','2','x','-',
        '%','0','2','x','%','0','2','x','%','0','2','x','%','0','2','x','%','0','2','x','%','0','2','x',0};

    switch (message)
    {
    case WM_NCCREATE:
    {
        CREATESTRUCTW *cs = (CREATESTRUCTW *)lParam;
        const GUID *guid = cs->lpCreateParams;

        if (guid)
        {
            ATOM atom;
            WCHAR buffer[37];

            if (GetAncestor( hwnd, GA_PARENT )) return FALSE;  /* refuse to create non-desktop window */

            sprintfW( buffer, guid_formatW, guid->Data1, guid->Data2, guid->Data3,
                      guid->Data4[0], guid->Data4[1], guid->Data4[2], guid->Data4[3],
                      guid->Data4[4], guid->Data4[5], guid->Data4[6], guid->Data4[7] );
            atom = GlobalAddAtomW( buffer );
            SetPropW( hwnd, display_device_guid_propW, ULongToHandle( atom ) );
        }
        return TRUE;
    }
    case WM_NCCALCSIZE:
        return 0;
    default:
        return DefWindowProcW( hwnd, message, wParam, lParam );
    }
}

/***********************************************************************
 *           PaintDesktop   (USER32.@)
 *
 */
BOOL WINAPI PaintDesktop(HDC hdc)
{
    HWND hwnd = GetDesktopWindow();

    /* check for an owning thread; otherwise don't paint anything (non-desktop mode) */
    if (GetWindowThreadProcessId( hwnd, NULL ))
    {
        RECT rect;

        GetClientRect( hwnd, &rect );

        /* Paint desktop pattern (only if wall paper does not cover everything) */

        if (!hbitmapWallPaper ||
            (!fTileWallPaper && ((bitmapSize.cx < rect.right) || (bitmapSize.cy < rect.bottom))))
        {
            HBRUSH brush = hbrushPattern;
            if (!brush) brush = (HBRUSH)GetClassLongPtrW( hwnd, GCLP_HBRBACKGROUND );
            /* Set colors in case pattern is a monochrome bitmap */
            SetBkColor( hdc, RGB(0,0,0) );
            SetTextColor( hdc, GetSysColor(COLOR_BACKGROUND) );
            FillRect( hdc, &rect, brush );
        }

        /* Paint wall paper */

        if (hbitmapWallPaper)
        {
            INT x, y;
            HDC hMemDC = CreateCompatibleDC( hdc );

            SelectObject( hMemDC, hbitmapWallPaper );

            if (fTileWallPaper)
            {
                for (y = 0; y < rect.bottom; y += bitmapSize.cy)
                    for (x = 0; x < rect.right; x += bitmapSize.cx)
                        BitBlt( hdc, x, y, bitmapSize.cx, bitmapSize.cy, hMemDC, 0, 0, SRCCOPY );
            }
            else
            {
                x = (rect.left + rect.right - bitmapSize.cx) / 2;
                y = (rect.top + rect.bottom - bitmapSize.cy) / 2;
                if (x < 0) x = 0;
                if (y < 0) y = 0;
                BitBlt( hdc, x, y, bitmapSize.cx, bitmapSize.cy, hMemDC, 0, 0, SRCCOPY );
            }
            DeleteDC( hMemDC );
        }
    }
    return TRUE;
}

/***********************************************************************
 *           SetDeskWallPaper   (USER32.@)
 *
 * FIXME: is there a unicode version?
 */
BOOL WINAPI SetDeskWallPaper( LPCSTR filename )
{
    return SystemParametersInfoA( SPI_SETDESKWALLPAPER, MAX_PATH, (void *)filename, SPIF_UPDATEINIFILE );
}


/***********************************************************************
 *           update_wallpaper
 */
BOOL update_wallpaper( const WCHAR *wallpaper, const WCHAR *pattern )
{
    int pat[8];

    if (hbrushPattern) DeleteObject( hbrushPattern );
    hbrushPattern = 0;
    memset( pat, 0, sizeof(pat) );
    if (pattern)
    {
        char buffer[64];
        WideCharToMultiByte( CP_ACP, 0, pattern, -1, buffer, sizeof(buffer), NULL, NULL );
        if (sscanf( buffer, " %d %d %d %d %d %d %d %d",
                    &pat[0], &pat[1], &pat[2], &pat[3],
                    &pat[4], &pat[5], &pat[6], &pat[7] ))
        {
            WORD ptrn[8];
            HBITMAP hbitmap;
            int i;

            for (i = 0; i < 8; i++) ptrn[i] = pat[i] & 0xffff;
            hbitmap = CreateBitmap( 8, 8, 1, 1, ptrn );
            hbrushPattern = CreatePatternBrush( hbitmap );
            DeleteObject( hbitmap );
        }
    }
    init_wallpaper( wallpaper );
    RedrawWindow( GetDesktopWindow(), 0, 0, RDW_INVALIDATE | RDW_ERASE | RDW_NOCHILDREN );
    return TRUE;
}
