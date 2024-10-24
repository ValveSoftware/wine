/*
 * Copyright 2024 Paul Gofman for CodeWeavers
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

#include "icu.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(icu);

const char * U_EXPORT2 u_errorName(UErrorCode code)
{
    FIXME( "code %d stub.\n", code );

    return "ICU_UNKNOWN_ERROR";
}

int32_t U_EXPORT2 ucal_getTimeZoneIDForWindowsID( const UChar *win_id, int32_t len, const char *region, UChar *id,
                                                  int32_t ret_len, UErrorCode *status )
{
    FIXME( "win_id %s, len %d, region %s, id %p, ret_len %d, status %p stub.\n", debugstr_w(win_id), len,
           debugstr_a(region), id, ret_len, status );

    *id = 0;
    *status = U_UNSUPPORTED_ERROR;
    return 0;
}
