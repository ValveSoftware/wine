/*
 * ROM File Detection for Goliath LibRetro Integration
 *
 * Copyright 2025 Goliath Project
 * Licensed under LGPL v2.1 or later
 */

#include "rom_detection.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <ctype.h>

/* ROM file signatures */
static const struct rom_signature rom_signatures[] = {
    /* NES */
    {ROM_TYPE_NES, ".nes", (const uint8_t*)"NES\x1a", 4, 0, "Nintendo Entertainment System", "nestopia_libretro"},
    {ROM_TYPE_NES, ".unf", NULL, 0, 0, "UNIF NES ROM", "nestopia_libretro"},
    {ROM_TYPE_NES, ".unif", NULL, 0, 0, "UNIF NES ROM", "nestopia_libretro"},
    
    /* SNES */
    {ROM_TYPE_SNES, ".smc", NULL, 0, 0, "Super Nintendo ROM", "snes9x_libretro"},
    {ROM_TYPE_SNES, ".sfc", NULL, 0, 0, "Super Famicom ROM", "snes9x_libretro"},
    {ROM_TYPE_SNES, ".fig", NULL, 0, 0, "SNES ROM", "snes9x_libretro"},
    
    /* Genesis/Mega Drive */
    {ROM_TYPE_GENESIS, ".md", (const uint8_t*)"SEGA", 4, 0x100, "Sega Genesis/Mega Drive", "genesis_plus_gx_libretro"},
    {ROM_TYPE_GENESIS, ".gen", (const uint8_t*)"SEGA", 4, 0x100, "Sega Genesis", "genesis_plus_gx_libretro"},
    {ROM_TYPE_GENESIS, ".bin", (const uint8_t*)"SEGA", 4, 0x100, "Genesis Binary", "genesis_plus_gx_libretro"},
    
    /* Game Boy */
    {ROM_TYPE_GAMEBOY, ".gb", NULL, 0, 0, "Game Boy ROM", "gambatte_libretro"},
    {ROM_TYPE_GAMEBOY_COLOR, ".gbc", NULL, 0, 0, "Game Boy Color ROM", "gambatte_libretro"},
    {ROM_TYPE_GAMEBOY_ADVANCE, ".gba", NULL, 0, 0, "Game Boy Advance ROM", "mgba_libretro"},
    
    /* Nintendo 64 */
    {ROM_TYPE_N64, ".n64", NULL, 0, 0, "Nintendo 64 ROM", "mupen64plus_next_libretro"},
    {ROM_TYPE_N64, ".v64", NULL, 0, 0, "Nintendo 64 ROM", "mupen64plus_next_libretro"},
    {ROM_TYPE_N64, ".z64", NULL, 0, 0, "Nintendo 64 ROM", "mupen64plus_next_libretro"},
    
    /* PlayStation */
    {ROM_TYPE_PSX, ".bin", NULL, 0, 0, "PlayStation CD Image", "pcsx_rearmed_libretro"},
    {ROM_TYPE_PSX, ".cue", NULL, 0, 0, "PlayStation Cue Sheet", "pcsx_rearmed_libretro"},
    {ROM_TYPE_PSX, ".img", NULL, 0, 0, "PlayStation Image", "pcsx_rearmed_libretro"},
    
    /* Atari */
    {ROM_TYPE_ATARI_2600, ".a26", NULL, 0, 0, "Atari 2600 ROM", "stella_libretro"},
    {ROM_TYPE_ATARI_7800, ".a78", NULL, 0, 0, "Atari 7800 ROM", "prosystem_libretro"},
    
    /* Sega 8-bit */
    {ROM_TYPE_MASTER_SYSTEM, ".sms", NULL, 0, 0, "Sega Master System ROM", "genesis_plus_gx_libretro"},
    {ROM_TYPE_GAME_GEAR, ".gg", NULL, 0, 0, "Sega Game Gear ROM", "genesis_plus_gx_libretro"},
    
    /* Other systems */
    {ROM_TYPE_LYNX, ".lnx", NULL, 0, 0, "Atari Lynx ROM", "handy_libretro"},
    {ROM_TYPE_NEO_GEO, ".neo", NULL, 0, 0, "Neo Geo ROM", "fbneo_libretro"},
    
    /* Terminator */
    {ROM_TYPE_UNKNOWN, NULL, NULL, 0, 0, NULL, NULL}
};

/* CRC32 table for checksum calculation */
static uint32_t crc32_table[256];
static bool crc32_table_initialized = false;

static void init_crc32_table(void) {
    if (crc32_table_initialized) return;
    
    for (int i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc >>= 1;
            }
        }
        crc32_table[i] = crc;
    }
    crc32_table_initialized = true;
}

uint32_t calculate_rom_crc32(const char *filename) {
    init_crc32_table();
    
    FILE *file = fopen(filename, "rb");
    if (!file) return 0;
    
    uint32_t crc = 0xFFFFFFFF;
    uint8_t buffer[4096];
    size_t bytes_read;
    
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        for (size_t i = 0; i < bytes_read; i++) {
            crc = crc32_table[(crc ^ buffer[i]) & 0xFF] ^ (crc >> 8);
        }
    }
    
    fclose(file);
    return crc ^ 0xFFFFFFFF;
}

const char *get_file_extension(const char *filename) {
    const char *dot = strrchr(filename, '.');
    if (!dot || dot == filename) return "";
    return dot;
}

bool file_has_extension(const char *filename, const char *extension) {
    const char *file_ext = get_file_extension(filename);
    return strcasecmp(file_ext, extension) == 0;
}

bool file_has_any_extension(const char *filename, const char **extensions) {
    const char *file_ext = get_file_extension(filename);
    for (int i = 0; extensions[i]; i++) {
        if (strcasecmp(file_ext, extensions[i]) == 0) {
            return true;
        }
    }
    return false;
}

rom_type_t detect_rom_type_by_extension(const char *filename) {
    const char *ext = get_file_extension(filename);
    if (!ext || !*ext) return ROM_TYPE_UNKNOWN;
    
    for (int i = 0; rom_signatures[i].type != ROM_TYPE_UNKNOWN; i++) {
        if (rom_signatures[i].extension && 
            strcasecmp(ext, rom_signatures[i].extension) == 0) {
            return rom_signatures[i].type;
        }
    }
    
    return ROM_TYPE_UNKNOWN;
}

rom_type_t detect_rom_type_by_content(const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (!file) return ROM_TYPE_UNKNOWN;
    
    uint8_t buffer[1024];
    size_t bytes_read = fread(buffer, 1, sizeof(buffer), file);
    fclose(file);
    
    if (bytes_read < 4) return ROM_TYPE_UNKNOWN;
    
    for (int i = 0; rom_signatures[i].type != ROM_TYPE_UNKNOWN; i++) {
        const struct rom_signature *sig = &rom_signatures[i];
        if (!sig->magic || sig->magic_size == 0) continue;
        
        if (sig->magic_offset + sig->magic_size <= bytes_read) {
            if (memcmp(buffer + sig->magic_offset, sig->magic, sig->magic_size) == 0) {
                return sig->type;
            }
        }
    }
    
    return ROM_TYPE_UNKNOWN;
}

rom_type_t detect_rom_type(const char *filename) {
    /* First try content-based detection */
    rom_type_t type = detect_rom_type_by_content(filename);
    if (type != ROM_TYPE_UNKNOWN) return type;
    
    /* Fall back to extension-based detection */
    return detect_rom_type_by_extension(filename);
}

bool is_rom_file(const char *filename) {
    return detect_rom_type(filename) != ROM_TYPE_UNKNOWN;
}

const char *rom_type_to_string(rom_type_t type) {
    switch (type) {
        case ROM_TYPE_NES: return "Nintendo Entertainment System";
        case ROM_TYPE_SNES: return "Super Nintendo Entertainment System";
        case ROM_TYPE_GENESIS: return "Sega Genesis/Mega Drive";
        case ROM_TYPE_GAMEBOY: return "Game Boy";
        case ROM_TYPE_GAMEBOY_COLOR: return "Game Boy Color";
        case ROM_TYPE_GAMEBOY_ADVANCE: return "Game Boy Advance";
        case ROM_TYPE_N64: return "Nintendo 64";
        case ROM_TYPE_PSX: return "Sony PlayStation";
        case ROM_TYPE_ATARI_2600: return "Atari 2600";
        case ROM_TYPE_ATARI_7800: return "Atari 7800";
        case ROM_TYPE_MASTER_SYSTEM: return "Sega Master System";
        case ROM_TYPE_GAME_GEAR: return "Sega Game Gear";
        case ROM_TYPE_LYNX: return "Atari Lynx";
        case ROM_TYPE_NEO_GEO: return "Neo Geo";
        case ROM_TYPE_ARCADE: return "Arcade";
        default: return "Unknown";
    }
}

const char *get_suggested_core_for_rom_type(rom_type_t type) {
    for (int i = 0; rom_signatures[i].type != ROM_TYPE_UNKNOWN; i++) {
        if (rom_signatures[i].type == type && rom_signatures[i].suggested_core) {
            return rom_signatures[i].suggested_core;
        }
    }
    return NULL;
}

struct rom_info *analyze_rom_file(const char *filename) {
    struct rom_info *info = calloc(1, sizeof(struct rom_info));
    if (!info) return NULL;
    
    info->filename = strdup(filename);
    info->type = detect_rom_type(filename);
    info->suggested_core = get_suggested_core_for_rom_type(info->type);
    info->crc32 = calculate_rom_crc32(filename);
    
    /* Get file size */
    struct stat st;
    if (stat(filename, &st) == 0) {
        info->size = st.st_size;
    }
    
    /* Validate ROM based on type */
    switch (info->type) {
        case ROM_TYPE_NES:
            info->valid = validate_nes_rom(filename);
            break;
        case ROM_TYPE_SNES:
            info->valid = validate_snes_rom(filename);
            break;
        case ROM_TYPE_GENESIS:
            info->valid = validate_genesis_rom(filename);
            break;
        case ROM_TYPE_GAMEBOY:
        case ROM_TYPE_GAMEBOY_COLOR:
            info->valid = validate_gameboy_rom(filename);
            break;
        case ROM_TYPE_N64:
            info->valid = validate_n64_rom(filename);
            break;
        default:
            info->valid = (info->type != ROM_TYPE_UNKNOWN);
            break;
    }
    
    return info;
}

void free_rom_info(struct rom_info *info) {
    if (!info) return;
    free(info->filename);
    free(info->title);
    free(info->region);
    free(info);
}

bool validate_nes_rom(const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (!file) return false;
    
    uint8_t header[16];
    if (fread(header, 1, 16, file) != 16) {
        fclose(file);
        return false;
    }
    fclose(file);
    
    /* Check NES header magic */
    return memcmp(header, "NES\x1a", 4) == 0;
}

bool validate_snes_rom(const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (!file) return false;
    
    /* SNES ROMs don't have a standard header, so we check file size */
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fclose(file);
    
    /* SNES ROMs are typically multiples of 32KB */
    return (size > 0 && (size % 32768) == 0) || (size % 32768) == 512; /* 512 byte header */
}

bool validate_genesis_rom(const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (!file) return false;
    
    uint8_t header[0x200];
    if (fread(header, 1, sizeof(header), file) != sizeof(header)) {
        fclose(file);
        return false;
    }
    fclose(file);
    
    /* Check for SEGA signature at offset 0x100 */
    return memcmp(header + 0x100, "SEGA", 4) == 0;
}

bool validate_gameboy_rom(const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (!file) return false;
    
    uint8_t header[0x150];
    if (fread(header, 1, sizeof(header), file) != sizeof(header)) {
        fclose(file);
        return false;
    }
    fclose(file);
    
    /* Check Nintendo logo checksum at 0x104-0x133 */
    uint8_t checksum = 0;
    for (int i = 0x104; i <= 0x133; i++) {
        checksum = checksum - header[i] - 1;
    }
    
    return checksum == header[0x14D];
}

bool validate_n64_rom(const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (!file) return false;
    
    uint8_t header[64];
    if (fread(header, 1, 64, file) != 64) {
        fclose(file);
        return false;
    }
    fclose(file);
    
    /* Check for N64 ROM signature (big-endian format) */
    uint32_t magic = (header[0] << 24) | (header[1] << 16) | (header[2] << 8) | header[3];
    return magic == 0x80371240; /* N64 ROM magic number */
}

int parse_nes_header(const uint8_t *data, size_t size, struct rom_info *info) {
    if (size < 16 || memcmp(data, "NES\x1a", 4) != 0) return -1;
    
    /* Extract basic info from NES header */
    int prg_size = data[4] * 16384; /* PRG ROM size in 16KB units */
    int chr_size = data[5] * 8192;  /* CHR ROM size in 8KB units */
    
    /* Create a simple title based on filename */
    const char *basename = strrchr(info->filename, '/');
    if (basename) basename++;
    else basename = info->filename;
    
    info->title = strdup(basename);
    info->region = strdup("NTSC"); /* Default assumption */
    
    return 0;
}

int parse_snes_header(const uint8_t *data, size_t size, struct rom_info *info) {
    /* SNES header parsing is complex due to different formats */
    /* For now, just extract filename as title */
    const char *basename = strrchr(info->filename, '/');
    if (basename) basename++;
    else basename = info->filename;
    
    info->title = strdup(basename);
    info->region = strdup("NTSC");
    
    return 0;
}

int parse_genesis_header(const uint8_t *data, size_t size, struct rom_info *info) {
    if (size < 0x200) return -1;
    
    /* Genesis ROM title is at offset 0x150-0x18F */
    char title[64];
    memcpy(title, data + 0x150, 48);
    title[48] = '\0';
    
    /* Trim whitespace */
    char *end = title + strlen(title) - 1;
    while (end > title && isspace(*end)) *end-- = '\0';
    
    info->title = strdup(title);
    
    /* Region info at 0x1F0 */
    char region_code = data[0x1F0];
    switch (region_code) {
        case 'J': info->region = strdup("Japan"); break;
        case 'U': info->region = strdup("USA"); break;
        case 'E': info->region = strdup("Europe"); break;
        default: info->region = strdup("Unknown"); break;
    }
    
    return 0;
}

int parse_gameboy_header(const uint8_t *data, size_t size, struct rom_info *info) {
    if (size < 0x150) return -1;
    
    /* Game title at 0x134-0x143 */
    char title[17];
    memcpy(title, data + 0x134, 16);
    title[16] = '\0';
    
    /* Trim null bytes */
    for (int i = 0; i < 16; i++) {
        if (title[i] == 0) {
            title[i] = '\0';
            break;
        }
    }
    
    info->title = strdup(title);
    info->region = strdup("Worldwide");
    
    return 0;
}

int parse_n64_header(const uint8_t *data, size_t size, struct rom_info *info) {
    if (size < 64) return -1;
    
    /* N64 ROM name at offset 0x20-0x33 */
    char title[21];
    memcpy(title, data + 0x20, 20);
    title[20] = '\0';
    
    /* Trim whitespace */
    char *end = title + strlen(title) - 1;
    while (end > title && isspace(*end)) *end-- = '\0';
    
    info->title = strdup(title);
    
    /* Region code at 0x3E */
    char region_code = data[0x3E];
    switch (region_code) {
        case 'D': info->region = strdup("Germany"); break;
        case 'E': info->region = strdup("USA"); break;
        case 'F': info->region = strdup("France"); break;
        case 'I': info->region = strdup("Italy"); break;
        case 'J': info->region = strdup("Japan"); break;
        case 'P': info->region = strdup("Europe"); break;
        case 'S': info->region = strdup("Spain"); break;
        case 'U': info->region = strdup("Australia"); break;
        default: info->region = strdup("Unknown"); break;
    }
    
    return 0;
}