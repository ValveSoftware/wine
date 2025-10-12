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
#include <sys/wait.h>

#define ELF_MAGIC "\x7fELF"
#define MACHO_MAGIC_32 0xfeedface
#define MACHO_MAGIC_64 0xfeedfacf
#define MACHO_CIGAM_32 0xcefaedfe
#define MACHO_CIGAM_64 0xcffaedfe
#define ZIP_MAGIC 0x04034b50  /* ZIP local file header signature */
#define APK_BUFFER_SIZE 1024

extern int wine_main(int argc, char *argv[]);
extern int darling_main(int argc, char *argv[]);

/* ATL integration function */
static int atl_main(int argc, char *argv[]) {
    /* For now, we'll exec the atl command directly */
    /* In a more integrated approach, this could link to ATL libraries */
    char **new_argv = malloc((argc + 1) * sizeof(char*));
    if (!new_argv) {
        perror("malloc");
        return 1;
    }
    
    new_argv[0] = "atl";
    for (int i = 1; i < argc; i++) {
        new_argv[i] = argv[i];
    }
    new_argv[argc] = NULL;
    
    execvp("atl", new_argv);
    
    /* If we get here, exec failed */
    perror("Failed to execute ATL");
    free(new_argv);
    return 1;
}

/* Check if file is an Android APK */
static int is_apk_file(const char *path) {
    /* Simple check: APK files are ZIP archives with .apk extension */
    const char *ext = strrchr(path, '.');
    if (!ext) return 0;
    
    if (strcmp(ext, ".apk") != 0) return 0;
    
    /* Verify it's a ZIP file by checking magic number */
    int fd = open(path, O_RDONLY);
    if (fd < 0) return 0;
    
    uint32_t magic = 0;
    if (read(fd, &magic, sizeof(magic)) != sizeof(magic)) {
        close(fd);
        return 0;
    }
    close(fd);
    
    return (magic == ZIP_MAGIC);
}

static int detect_binary_type(const char *path) {
    /* First check for APK files */
    if (is_apk_file(path)) {
        return 3; /* APK/Android */
    }
    
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
    if (!memcmp(&magic, ELF_MAGIC, 4)) return 1; /* ELF */
    if (magic == MACHO_MAGIC_32 || magic == MACHO_MAGIC_64 ||
        magic == MACHO_CIGAM_32 || magic == MACHO_CIGAM_64) return 2; /* Mach-O */
    return 0; /* Unknown */
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <binary> [args...]\n", argv[0]);
        return 1;
    }
    int type = detect_binary_type(argv[1]);
    if (type == 1) {
        /* ELF: dispatch to Wine */
        return wine_main(argc, argv);
    } else if (type == 2) {
        /* Mach-O: dispatch to Darling */
        return darling_main(argc, argv);
    } else if (type == 3) {
        /* APK: dispatch to ATL */
        return atl_main(argc, argv);
    } else {
        fprintf(stderr, "Unknown or unsupported binary format: %s\n", argv[1]);
        return 2;
    }
}
