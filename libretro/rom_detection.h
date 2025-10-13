/*
 * ROM File Detection for Goliath LibRetro Integration
 *
 * Copyright 2025 Goliath Project
 * Licensed under LGPL v2.1 or later
 */

#ifndef ROM_DETECTION_H
#define ROM_DETECTION_H

#include <stdint.h>
#include <stdbool.h>

/* ROM file types */
typedef enum {
    ROM_TYPE_UNKNOWN = 0,
    ROM_TYPE_NES,
    ROM_TYPE_SNES,
    ROM_TYPE_GENESIS,
    ROM_TYPE_GAMEBOY,
    ROM_TYPE_GAMEBOY_COLOR,
    ROM_TYPE_GAMEBOY_ADVANCE,
    ROM_TYPE_N64,
    ROM_TYPE_PSX,
    ROM_TYPE_ATARI_2600,
    ROM_TYPE_ATARI_7800,
    ROM_TYPE_MASTER_SYSTEM,
    ROM_TYPE_GAME_GEAR,
    ROM_TYPE_LYNX,
    ROM_TYPE_NEO_GEO,
    ROM_TYPE_ARCADE,
    ROM_TYPE_MAX
} rom_type_t;

/* ROM information structure */
struct rom_info {
    rom_type_t type;
    char *filename;
    char *title;
    char *region;
    size_t size;
    uint32_t crc32;
    bool valid;
    const char *suggested_core;
};

/* ROM file magic numbers and signatures */
struct rom_signature {
    rom_type_t type;
    const char *extension;
    const uint8_t *magic;
    size_t magic_size;
    size_t magic_offset;
    const char *description;
    const char *suggested_core;
};

/* Function prototypes */
rom_type_t detect_rom_type(const char *filename);
rom_type_t detect_rom_type_by_content(const char *filename);
rom_type_t detect_rom_type_by_extension(const char *filename);
struct rom_info *analyze_rom_file(const char *filename);
void free_rom_info(struct rom_info *info);
bool is_rom_file(const char *filename);
const char *rom_type_to_string(rom_type_t type);
const char *get_suggested_core_for_rom_type(rom_type_t type);
uint32_t calculate_rom_crc32(const char *filename);

/* ROM validation functions */
bool validate_nes_rom(const char *filename);
bool validate_snes_rom(const char *filename);
bool validate_genesis_rom(const char *filename);
bool validate_gameboy_rom(const char *filename);
bool validate_n64_rom(const char *filename);

/* Header parsing functions */
int parse_nes_header(const uint8_t *data, size_t size, struct rom_info *info);
int parse_snes_header(const uint8_t *data, size_t size, struct rom_info *info);
int parse_genesis_header(const uint8_t *data, size_t size, struct rom_info *info);
int parse_gameboy_header(const uint8_t *data, size_t size, struct rom_info *info);
int parse_n64_header(const uint8_t *data, size_t size, struct rom_info *info);

/* Utility functions */
const char *get_file_extension(const char *filename);
bool file_has_extension(const char *filename, const char *extension);
bool file_has_any_extension(const char *filename, const char **extensions);

#endif /* ROM_DETECTION_H */