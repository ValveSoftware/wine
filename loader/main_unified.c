/*
 * Goliath Unified Loader: Dispatches to ELF (Wine), Mach-O (Darling), APK (ATL), or ROM (LibRetro) loader
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
#include "../libretro/rom_detection.h"
#include "../libretro/libretro_api.h"

#define ELF_MAGIC "\x7fELF"
#define MACHO_MAGIC_32 0xfeedface
#define MACHO_MAGIC_64 0xfeedfacf
#define MACHO_CIGAM_32 0xcefaedfe
#define MACHO_CIGAM_64 0xcffaedfe
#define ZIP_MAGIC 0x04034b50  /* ZIP local file header signature */
#define APK_BUFFER_SIZE 1024

/* Binary types */
typedef enum {
    BINARY_TYPE_UNKNOWN = 0,
    BINARY_TYPE_ELF = 1,
    BINARY_TYPE_MACHO = 2,
    BINARY_TYPE_ROM = 3,
    BINARY_TYPE_APK = 4
} binary_type_t;

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
    if (!ext || strcasecmp(ext, ".apk") != 0) return 0;
    
    /* Verify it's a ZIP file by checking magic number */
    int fd = open(path, O_RDONLY);
    if (fd < 0) return 0;
    
    uint32_t magic = 0;
    if (read(fd, &magic, sizeof(magic)) != sizeof(magic)) {
        close(fd);
        return 0;
    }
    close(fd);
    
    return (magic == ZIP_MAGIC || magic == (ZIP_MAGIC >> 24 | ZIP_MAGIC << 24));
}

static binary_type_t detect_binary_type(const char *path) {
    /* First check if it's a ROM file */
    if (is_rom_file(path)) {
        return BINARY_TYPE_ROM;
    }
    
    /* Check for APK files */
    if (is_apk_file(path)) {
        return BINARY_TYPE_APK;
    }
    
    /* Check binary magic numbers */
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        perror("open");
        return BINARY_TYPE_UNKNOWN;
    }
    
    uint32_t magic = 0;
    if (read(fd, &magic, sizeof(magic)) != sizeof(magic)) {
        close(fd);
        return BINARY_TYPE_UNKNOWN;
    }
    close(fd);
    
    if (!memcmp(&magic, ELF_MAGIC, 4)) return BINARY_TYPE_ELF;
    if (magic == MACHO_MAGIC_32 || magic == MACHO_MAGIC_64 ||
        magic == MACHO_CIGAM_32 || magic == MACHO_CIGAM_64) return BINARY_TYPE_MACHO;
    
    return BINARY_TYPE_UNKNOWN;
}

static int run_libretro_game(const char *rom_path) {
    printf("Running ROM file with LibRetro: %s\n", rom_path);
    
    /* Analyze the ROM */
    struct rom_info *info = analyze_rom_file(rom_path);
    if (!info) {
        fprintf(stderr, "Failed to analyze ROM file: %s\n", rom_path);
        return 1;
    }
    
    if (!info->valid) {
        fprintf(stderr, "Invalid ROM file: %s\n", rom_path);
        free_rom_info(info);
        return 1;
    }
    
    printf("ROM Info:\n");
    printf("  Type: %s\n", rom_type_to_string(info->type));
    printf("  Title: %s\n", info->title ? info->title : "Unknown");
    printf("  Region: %s\n", info->region ? info->region : "Unknown");
    printf("  Size: %zu bytes\n", info->size);
    printf("  CRC32: 0x%08X\n", info->crc32);
    printf("  Suggested Core: %s\n", info->suggested_core ? info->suggested_core : "None");
    
    /* Find and load the appropriate LibRetro core */
    const char *core_name = info->suggested_core;
    if (!core_name) {
        fprintf(stderr, "No suitable LibRetro core found for ROM type: %s\n", 
                rom_type_to_string(info->type));
        free_rom_info(info);
        return 1;
    }
    
    /* Try to find the core in common locations */
    char core_path[1024];
    const char *core_dirs[] = {
        "/usr/lib/libretro",
        "/usr/local/lib/libretro",
        "/opt/libretro/cores",
        getenv("LIBRETRO_CORE_PATH"),
        NULL
    };
    
    struct libretro_core *core = NULL;
    for (int i = 0; core_dirs[i]; i++) {
        if (!core_dirs[i]) continue;
        
        snprintf(core_path, sizeof(core_path), "%s/%s.so", core_dirs[i], core_name);
        
        /* Check if core file exists */
        if (access(core_path, R_OK) == 0) {
            printf("Loading LibRetro core: %s\n", core_path);
            core = libretro_load_core(core_path);
            if (core) break;
        }
    }
    
    if (!core) {
        fprintf(stderr, "Failed to load LibRetro core: %s\n", core_name);
        free_rom_info(info);
        return 1;
    }
    
    /* Load the game */
    if (libretro_load_game(core, rom_path) != 0) {
        fprintf(stderr, "Failed to load game into LibRetro core\n");
        libretro_unload_core(core);
        free_rom_info(info);
        return 1;
    }
    
    printf("Game loaded successfully. Starting emulation...\n");
    printf("Press Ctrl+C to exit.\n");
    
    /* Simple game loop - run for a few seconds as demonstration */
    for (int frame = 0; frame < 300; frame++) { /* ~5 seconds at 60fps */
        libretro_run_frame(core);
        usleep(16667); /* ~60fps */
    }
    
    printf("Emulation finished.\n");
    
    /* Cleanup */
    libretro_unload_game(core);
    libretro_unload_core(core);
    free_rom_info(info);
    
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <binary|rom|apk> [args...]\n", argv[0]);
        fprintf(stderr, "Goliath Unified Compatibility Layer\n");
        fprintf(stderr, "Supports: Windows (ELF/Wine), macOS (Mach-O/Darling), Android (APK/ATL), Legacy ROMs (LibRetro)\n");
        return 1;
    }
    
    binary_type_t type = detect_binary_type(argv[1]);
    
    switch (type) {
        case BINARY_TYPE_ELF:
            printf("Detected ELF binary, dispatching to Wine...\n");
            return wine_main(argc, argv);
            
        case BINARY_TYPE_MACHO:
            printf("Detected Mach-O binary, dispatching to Darling...\n");
            return darling_main(argc, argv);
            
        case BINARY_TYPE_ROM:
            printf("Detected ROM file, dispatching to LibRetro...\n");
            return run_libretro_game(argv[1]);
            
        case BINARY_TYPE_APK:
            printf("Detected APK file, dispatching to ATL...\n");
            return atl_main(argc, argv);
            
        default:
            fprintf(stderr, "Unknown or unsupported file format: %s\n", argv[1]);
            fprintf(stderr, "Supported formats:\n");
            fprintf(stderr, "  - Windows executables (.exe, ELF format)\n");
            fprintf(stderr, "  - macOS applications (Mach-O format)\n");
            fprintf(stderr, "  - Android packages (.apk)\n");
            fprintf(stderr, "  - Legacy ROMs (.nes, .snes, .md, .gb, .gba, .n64, etc.)\n");
            return 2;
    }
    
    return 0;
}