/* Minimal libretro core loader for Goliath
 * This is a small wrapper that loads a libretro core via dlopen
 * and resolves the common entry points. It intentionally avoids
 * importing the entire libretro-common tree to keep integration small.
 *
 * Upstream: https://github.com/libretro
 */

#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>

#include "libretro_loader.h"

struct libretro_core {
    void *handle;
    retro_init_t init;
    retro_deinit_t deinit;
    retro_api_version_t api_version;
    retro_get_system_info_t get_system_info;
    retro_load_game_t load_game;
    retro_unload_game_t unload_game;
    retro_run_t run;
};

libretro_core_t *libretro_core_load(const char *path)
{
    if (!path) return NULL;
    void *h = dlopen(path, RTLD_NOW);
    if (!h) {
        fprintf(stderr, "libretro: dlopen('%s') failed: %s\n", path, dlerror());
        return NULL;
    }

    libretro_core_t *core = calloc(1, sizeof(*core));
    if (!core) { dlclose(h); return NULL; }
    core->handle = h;

    core->init = (retro_init_t)dlsym(h, "retro_init");
    core->deinit = (retro_deinit_t)dlsym(h, "retro_deinit");
    core->api_version = (retro_api_version_t)dlsym(h, "retro_api_version");
    core->get_system_info = (retro_get_system_info_t)dlsym(h, "retro_get_system_info");
    core->load_game = (retro_load_game_t)dlsym(h, "retro_load_game");
    core->unload_game = (retro_unload_game_t)dlsym(h, "retro_unload_game");
    core->run = (retro_run_t)dlsym(h, "retro_run");

    /* Basic sanity: require init and run at minimum */
    if (!core->init || !core->run) {
        fprintf(stderr, "libretro: core missing required symbols\n");
        dlclose(h);
        free(core);
        return NULL;
    }

    return core;
}

void libretro_core_unload(libretro_core_t *core)
{
    if (!core) return;
    if (core->handle) dlclose(core->handle);
    free(core);
}

void libretro_core_init(libretro_core_t *core)
{
    if (!core || !core->init) return;
    core->init();
}

void libretro_core_run(libretro_core_t *core)
{
    if (!core || !core->run) return;
    core->run();
}
