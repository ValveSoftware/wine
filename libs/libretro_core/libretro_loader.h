#ifndef LIBRETRO_LOADER_H
#define LIBRETRO_LOADER_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <limits.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*retro_init_t)(void);
typedef void (*retro_deinit_t)(void);
typedef int  (*retro_api_version_t)(void);
struct retro_system_info; typedef const struct retro_system_info* (*retro_get_system_info_t)(void);
typedef bool (*retro_load_game_t)(const void* game);
typedef void (*retro_unload_game_t)(void);
typedef void (*retro_run_t)(void);

typedef struct libretro_core libretro_core_t;

libretro_core_t *libretro_core_load(const char *path);
void libretro_core_unload(libretro_core_t *core);
void libretro_core_init(libretro_core_t *core);
void libretro_core_run(libretro_core_t *core);

#ifdef __cplusplus
}
#endif

#endif /* LIBRETRO_LOADER_H */
