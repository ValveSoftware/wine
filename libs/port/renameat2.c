/*
 * renameat2 function
 *
 * Copyright 2015-2019 Erich E. Hoover
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
#include "wine/port.h"

#ifdef HAVE_SYS_SYSCALL_H
# include <sys/syscall.h>
#endif

#include <errno.h>
#include <stdio.h>

#ifndef HAVE_RENAMEAT
int renameat( int olddirfd, const char *oldpath, int newdirfd, const char *newpath )
{
    errno = ENOSYS;
    return -1;
}
#endif

#ifndef HAVE_RENAMEAT2
int renameat2( int olddirfd, const char *oldpath, int newdirfd, const char *newpath,
               unsigned int flags )
{
    if (flags == 0)
        return renameat( olddirfd, oldpath, newdirfd, newpath );
#if defined(__NR_renameat2)
    return syscall( __NR_renameat2, olddirfd, oldpath, newdirfd, newpath, flags );
#elif defined(RENAME_SWAP)
    return renameatx_np(olddirfd, oldpath, newdirfd, newpath,
                        (flags & RENAME_EXCHANGE ? RENAME_SWAP : 0));
#else
    errno = ENOSYS;
    return -1;
#endif
}
#endif /* HAVE_RENAMEAT2 */
