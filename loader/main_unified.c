/*
 * Goliath Unified Loader: Dispatches to ELF (Wine) or Mach-O (Darling) loader
 *
 * Copyright 2025 Goliath Project
 *
 * Licensed under LGPL v2.1 or later
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>

#define ELF_MAGIC "\x7fELF"
#define MACHO_MAGIC_32 0xfeedface
#define MACHO_MAGIC_64 0xfeedfacf
#define MACHO_CIGAM_32 0xcefaedfe
#define MACHO_CIGAM_64 0xcffaedfe

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
