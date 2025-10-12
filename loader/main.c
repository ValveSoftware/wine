#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dlfcn.h>

// --- WSL subsystem entry point ---
int wsl_main(int argc, char *argv[]) {
    // Load the WSL library and call its main function
    void *wsl_handle = dlopen("../dlls/wsl/libwsl.so", RTLD_NOW);
    if (!wsl_handle) {
        // Try alternative paths
        wsl_handle = dlopen("./dlls/wsl/libwsl.so", RTLD_NOW);
        if (!wsl_handle) {
            wsl_handle = dlopen("libwsl.so", RTLD_NOW);
        }
    }
    
    if (wsl_handle) {
        // Get the wsl_main function from the library
        int (*wsl_main_func)(int, char**) = dlsym(wsl_handle, "wsl_main");
        if (wsl_main_func) {
            int result = wsl_main_func(argc, argv);
            dlclose(wsl_handle);
            return result;
        } else {
            fprintf(stderr, "goliath: wsl_main function not found in WSL library\n");
            dlclose(wsl_handle);
        }
    }
    
    // Fallback: direct execution for basic WSL functionality
    fprintf(stderr, "[Goliath/WSL] Using fallback WSL implementation\n");
    
    if (argc < 2) {
        fprintf(stderr, "[Goliath/WSL] Usage: %s <linux-elf-binary> [args...]\n", argv[0]);
        return 1;
    }

    // Basic environment setup
    setenv("PATH", "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin", 1);
    setenv("WSL_DISTRO_NAME", "GoliathWSL", 1);
    
    // Execute the Linux binary directly
    pid_t pid = fork();
    if (pid == 0) {
        // Child: exec the Linux ELF binary
        execv(argv[1], &argv[1]);
        perror("[Goliath/WSL] execv failed");
        exit(127);
    } else if (pid > 0) {
        // Parent: wait for child
        int status = 0;
        waitpid(pid, &status, 0);
        return WIFEXITED(status) ? WEXITSTATUS(status) : 127;
    } else {
        perror("[Goliath/WSL] fork failed");
        return 127;
    }
}
// --- Goliath subsystem entry points (stubs for now) ---

int wine_main(int argc, char *argv[]) {
    // Original Wine loader logic
    void *handle = NULL;
    // Use the original load_ntdll logic
#ifdef __i386__
#define SO_DIR "i386-unix/"
#elif defined(__x86_64__)
#define SO_DIR "x86_64-unix/"
#elif defined(__arm__)
#define SO_DIR "arm-unix/"
#elif defined(__aarch64__)
#define SO_DIR "aarch64-unix/"
#else
#define SO_DIR ""
#endif
    const char *self = argv[0];
    char *path, *p;

    // Try to find ntdll.so as in the original loader
    if ((path = realpath(self, NULL))) {
        p = strrchr(path, '/');
        if (p) *p = 0;
        if ((p = strstr(path, "/loader"))) *p = 0;
        char ntdll_path[PATH_MAX];
        snprintf(ntdll_path, sizeof(ntdll_path), "%s/dlls/ntdll/ntdll.so", path);
        handle = dlopen(ntdll_path, RTLD_NOW);
        if (!handle) {
            snprintf(ntdll_path, sizeof(ntdll_path), "%s/wine/%sntdll.so", path, SO_DIR);
            handle = dlopen(ntdll_path, RTLD_NOW);
        }
        free(path);
    }
    if (!handle && (path = getenv("WINEDLLPATH"))) {
        path = strdup(path);
        for (p = strtok(path, ":"); p; p = strtok(NULL, ":")) {
            char ntdll_path[PATH_MAX];
            snprintf(ntdll_path, sizeof(ntdll_path), "%s/%sntdll.so", p, SO_DIR);
            handle = dlopen(ntdll_path, RTLD_NOW);
            if (!handle) {
                snprintf(ntdll_path, sizeof(ntdll_path), "%s/ntdll.so", p);
                handle = dlopen(ntdll_path, RTLD_NOW);
            }
            if (handle) break;
        }
        free(path);
    }
    if (!handle) {
        handle = dlopen(LIBDIR "/wine/" SO_DIR "ntdll.so", RTLD_NOW);
    }
    if (handle) {
        void (*init_func)(int, char **) = dlsym(handle, "__wine_main");
        if (init_func) {
            init_func(argc, argv);
            return 0;
        }
        fprintf(stderr, "goliath: __wine_main function not found in ntdll.so\n");
        return 1;
    }
    fprintf(stderr, "goliath: could not load ntdll.so: %s\n", dlerror());
    return 1;
}


#include <sys/types.h>
#include <sys/wait.h>

int darling_main(int argc, char *argv[]) {
    // For now, exec the Mach-O loader binary from Darling as a placeholder for deep integration
    // In a real integration, this would call the Mach-O loader logic directly
    const char *mach_loader = "../libs/darling/src/startup/mldr/mldr";
    char **new_argv = malloc(sizeof(char*) * (argc + 1));
    if (!new_argv) {
        fprintf(stderr, "goliath: out of memory\n");
        return 1;
    }
    new_argv[0] = (char*)mach_loader;
    for (int i = 1; i < argc; ++i) new_argv[i] = argv[i];
    new_argv[argc] = NULL;

    pid_t pid = fork();
    if (pid == 0) {
        // Child: exec Mach-O loader
        execv(mach_loader, new_argv);
        perror("goliath: execv failed for Mach-O loader");
        exit(127);
    } else if (pid > 0) {
        // Parent: wait for child
        int status = 0;
        waitpid(pid, &status, 0);
        free(new_argv);
        return WIFEXITED(status) ? WEXITSTATUS(status) : 127;
    } else {
        perror("goliath: fork failed");
        free(new_argv);
        return 127;
    }
}
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

    // 1 = Windows ELF, 2 = Mach-O, 3 = Linux ELF, 0 = Unknown
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
        lseek(fd, 0, SEEK_SET);
        char ident[16] = {0};
        read(fd, ident, 16);
        close(fd);
        // ELF: check OS ABI field
        if (!memcmp(&magic, ELF_MAGIC, 4)) {
            // ident[7] is OS ABI: 0 = System V, 3 = Linux, 6 = Solaris, 9 = FreeBSD
            if (ident[7] == 3) return 3; // Linux ELF
            else return 1; // Default to Windows ELF
        }
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
            // Windows ELF: dispatch to Wine
            return wine_main(argc, argv);
        } else if (type == 2) {
            // Mach-O: dispatch to Darling
            return darling_main(argc, argv);
        } else if (type == 3) {
            // Linux ELF: dispatch to WSL
            return wsl_main(argc, argv);
        } else {
            fprintf(stderr, "Unknown or unsupported binary format: %s\n", argv[1]);
            return 2;
        }
    }
    if (len < tail_len) return NULL;
