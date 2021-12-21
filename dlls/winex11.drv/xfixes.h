/*
 * Wine X11DRV Xfixes interface
 *
 * Copyright 2021 Rémi Bernon for CodeWeavers
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
#ifndef __WINE_XFIXES_H
#define __WINE_XFIXES_H

#ifndef __WINE_CONFIG_H
# error You must include config.h to use this header
#endif

#ifdef SONAME_LIBXFIXES
#include <X11/extensions/Xfixes.h>
#define MAKE_FUNCPTR(f) extern typeof(f) * p##f DECLSPEC_HIDDEN;
MAKE_FUNCPTR(XFixesQueryExtension)
MAKE_FUNCPTR(XFixesQueryVersion)
MAKE_FUNCPTR(XFixesSelectSelectionInput)
#undef MAKE_FUNCPTR
#endif /* defined(SONAME_LIBXFIXES) */

#endif /* __WINE_XFIXES_H */
