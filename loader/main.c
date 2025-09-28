/*
 * Emulator initialisation code
 *
 * Copyright 2000 Alexandre Julliard
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

#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dlfcn.h>
#include <limits.h>
#ifdef HAVE_SYS_SYSCTL_H
# include <sys/sysctl.h>
#endif

#include "main.h"

#if defined(__APPLE__) && defined(__x86_64__) && !defined(HAVE_WINE_PRELOADER)

/* Not using the preloader on x86_64:
 * Reserve the same areas as the preloader does, but using zero-fill sections
 * (the only way to prevent system frameworks from using them, including allocations
 * before main() runs).
 */
__asm__(".zerofill WINE_RESERVE,WINE_RESERVE");
static char __wine_reserve[0x1fffff000] __attribute__((section("WINE_RESERVE, WINE_RESERVE")));

__asm__(".zerofill WINE_TOP_DOWN,WINE_TOP_DOWN");
static char __wine_top_down[0x001ff0000] __attribute__((section("WINE_TOP_DOWN, WINE_TOP_DOWN")));

static const struct wine_preload_info preload_info[] =
{
    { __wine_reserve,  sizeof(__wine_reserve)  }, /*         0x1000 -    0x200000000: low 8GB */
    { __wine_top_down, sizeof(__wine_top_down) }, /* 0x7ff000000000 - 0x7ff001ff0000: top-down allocations + virtual heap */

    #include <stdint.h>
    #include <stdbool.h>
    #include <errno.h>

    #define ELF_MAGIC "\x7fELF"
    #define MACHO_MAGIC_32 0xfeedface
    #define MACHO_MAGIC_64 0xfeedfacf
    #define MACHO_CIGAM_32 0xcefaedfe
    #define MACHO_CIGAM_64 0xcffaedfe

    // Forward declarations for loader entry points
    extern int wine_main(int argc, char *argv[]);
    extern int darling_main(int argc, char *argv[]);

    static int detect_binary_type(const char *path) {
        int fd = open(path, O_RDONLY);
        if (fd < 0) {
            perror("open");
            return -1;
        }
        uint32_t magic = 0;
        if (read(fd, &magic, sizeof(magic)) != sizeof(magic)) {
            close(fd);
            return -1;
        }
        close(fd);
        if (!memcmp(&magic, ELF_MAGIC, 4)) return 1; // ELF
        if (magic == MACHO_MAGIC_32 || magic == MACHO_MAGIC_64 ||
            magic == MACHO_CIGAM_32 || magic == MACHO_CIGAM_64) return 2; // Mach-O
        return 0; // Unknown
    }

    int main(int argc, char *argv[]) {
        if (argc < 2) {
            fprintf(stderr, "Usage: %s <binary> [args...]\n", argv[0]);
            return 1;
        }
        int type = detect_binary_type(argv[1]);
        if (type == 1) {
            // ELF: dispatch to Wine
            return wine_main(argc, argv);
        } else if (type == 2) {
            // Mach-O: dispatch to Darling
            return darling_main(argc, argv);
        } else {
            fprintf(stderr, "Unknown or unsupported binary format: %s\n", argv[1]);
            return 2;
        }
    }
    if (len < tail_len) return NULL;
