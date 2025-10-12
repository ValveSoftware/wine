/*
 * LibRetro API Implementation for Goliath
 *
 * Copyright 2025 Goliath Project
 * Licensed under LGPL v2.1 or later
 */

#include "libretro_api.h"
#include "rom_detection.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

/* Global state */
static bool libretro_initialized = false;
static struct libretro_core *current_core = NULL;

/* Video/Audio buffers */
static uint16_t *video_buffer = NULL;
static size_t video_buffer_size = 0;
static int16_t *audio_buffer = NULL;
static size_t audio_buffer_size = 0;

/* Callback implementations */
bool libretro_environment_callback(unsigned cmd, void *data) {
    switch (cmd) {
        case RETRO_ENVIRONMENT_GET_OVERSCAN: {
            bool *overscan = (bool*)data;
            *overscan = false;
            return true;
        }
        case RETRO_ENVIRONMENT_GET_CAN_DUPE: {
            bool *can_dupe = (bool*)data;
            *can_dupe = true;
            return true;
        }
        case RETRO_ENVIRONMENT_SET_MESSAGE: {
            struct retro_message *msg = (struct retro_message*)data;
            printf("LibRetro Message: %s\n", msg->msg);
            return true;
        }
        case RETRO_ENVIRONMENT_SHUTDOWN: {
            printf("LibRetro core requested shutdown\n");
            return true;
        }
        case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT: {
            enum retro_pixel_format *format = (enum retro_pixel_format*)data;
            printf("LibRetro pixel format: %d\n", *format);
            return true;
        }
        case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY: {
            const char **dir = (const char**)data;
            *dir = "/usr/share/libretro/system";
            return true;
        }
        case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY: {
            const char **dir = (const char**)data;
            *dir = getenv("HOME") ? getenv("HOME") : "/tmp";
            return true;
        }
        case RETRO_ENVIRONMENT_GET_VARIABLE: {
            struct retro_variable *var = (struct retro_variable*)data;
            /* For now, return NULL for all variables */
            var->value = NULL;
            return false;
        }
        case RETRO_ENVIRONMENT_SET_VARIABLES: {
            /* Accept variable definitions but don't store them yet */
            return true;
        }
        case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE: {
            bool *updated = (bool*)data;
            *updated = false;
            return true;
        }
        default:
            printf("Unhandled environment callback: %u\n", cmd);
            return false;
    }
}

void libretro_video_refresh_callback(const void *data, unsigned width, unsigned height, size_t pitch) {
    if (!data) return;
    
    /* Allocate video buffer if needed */
    size_t needed_size = width * height * sizeof(uint16_t);
    if (video_buffer_size < needed_size) {
        video_buffer = realloc(video_buffer, needed_size);
        video_buffer_size = needed_size;
    }
    
    if (video_buffer) {
        /* Copy frame data - assuming RGB565 format for now */
        const uint16_t *src = (const uint16_t*)data;
        for (unsigned y = 0; y < height; y++) {
            memcpy(video_buffer + y * width, 
                   (const uint8_t*)src + y * pitch, 
                   width * sizeof(uint16_t));
        }
    }
    
    /* TODO: Send frame to display system */
    printf("Video frame: %ux%u, pitch: %zu\n", width, height, pitch);
}

void libretro_audio_sample_callback(int16_t left, int16_t right) {
    /* TODO: Send audio sample to audio system */
    /* For now, just accumulate in buffer */
    static size_t audio_pos = 0;
    
    if (!audio_buffer) {
        audio_buffer_size = 44100 * 2 * sizeof(int16_t); /* 1 second stereo buffer */
        audio_buffer = malloc(audio_buffer_size);
        audio_pos = 0;
    }
    
    if (audio_buffer && audio_pos < audio_buffer_size / sizeof(int16_t) - 2) {
        audio_buffer[audio_pos++] = left;
        audio_buffer[audio_pos++] = right;
    }
}

size_t libretro_audio_sample_batch_callback(const int16_t *data, size_t frames) {
    /* TODO: Send audio batch to audio system */
    printf("Audio batch: %zu frames\n", frames);
    return frames;
}

void libretro_input_poll_callback(void) {
    /* TODO: Poll input devices */
}

int16_t libretro_input_state_callback(unsigned port, unsigned device, unsigned index, unsigned id) {
    /* TODO: Return input state */
    /* For now, return no input */
    return 0;
}

int libretro_init_api(void) {
    if (libretro_initialized) return 0;
    
    /* Initialize video buffer */
    video_buffer_size = 1024 * 1024 * sizeof(uint16_t); /* 1MB initial buffer */
    video_buffer = malloc(video_buffer_size);
    
    /* Initialize audio buffer */
    audio_buffer_size = 44100 * 2 * sizeof(int16_t); /* 1 second stereo buffer */
    audio_buffer = malloc(audio_buffer_size);
    
    if (!video_buffer || !audio_buffer) {
        libretro_cleanup_api();
        return -1;
    }
    
    libretro_initialized = true;
    return 0;
}

void libretro_cleanup_api(void) {
    if (current_core) {
        libretro_unload_core(current_core);
        current_core = NULL;
    }
    
    free(video_buffer);
    video_buffer = NULL;
    video_buffer_size = 0;
    
    free(audio_buffer);
    audio_buffer = NULL;
    audio_buffer_size = 0;
    
    libretro_initialized = false;
}

struct libretro_core *libretro_load_core(const char *core_path) {
    if (!libretro_initialized) {
        if (libretro_init_api() != 0) return NULL;
    }
    
    struct libretro_core *core = calloc(1, sizeof(struct libretro_core));
    if (!core) return NULL;
    
    /* Load the core library */
    core->handle = dlopen(core_path, RTLD_LAZY);
    if (!core->handle) {
        printf("Failed to load core %s: %s\n", core_path, dlerror());
        free(core);
        return NULL;
    }
    
    core->path = strdup(core_path);
    
    /* Load core functions */
    #define LOAD_FUNC(name) \
        core->name = dlsym(core->handle, #name); \
        if (!core->name) { \
            printf("Failed to load function %s from core\n", #name); \
            libretro_unload_core(core); \
            return NULL; \
        }
    
    LOAD_FUNC(retro_init);
    LOAD_FUNC(retro_deinit);
    LOAD_FUNC(retro_api_version);
    LOAD_FUNC(retro_get_system_info);
    LOAD_FUNC(retro_get_system_av_info);
    LOAD_FUNC(retro_set_environment);
    LOAD_FUNC(retro_set_video_refresh);
    LOAD_FUNC(retro_set_audio_sample);
    LOAD_FUNC(retro_set_audio_sample_batch);
    LOAD_FUNC(retro_set_input_poll);
    LOAD_FUNC(retro_set_input_state);
    LOAD_FUNC(retro_load_game);
    LOAD_FUNC(retro_unload_game);
    LOAD_FUNC(retro_get_region);
    LOAD_FUNC(retro_get_memory_data);
    LOAD_FUNC(retro_get_memory_size);
    LOAD_FUNC(retro_run);
    LOAD_FUNC(retro_reset);
    LOAD_FUNC(retro_serialize_size);
    LOAD_FUNC(retro_serialize);
    LOAD_FUNC(retro_unserialize);
    
    #undef LOAD_FUNC
    
    /* Check API version */
    unsigned api_version = core->retro_api_version();
    if (api_version != RETRO_API_VERSION) {
        printf("Core API version mismatch: expected %d, got %u\n", RETRO_API_VERSION, api_version);
        libretro_unload_core(core);
        return NULL;
    }
    
    /* Get system info */
    core->retro_get_system_info(&core->system_info);
    
    /* Set up callbacks */
    core->retro_set_environment(libretro_environment_callback);
    core->retro_set_video_refresh(libretro_video_refresh_callback);
    core->retro_set_audio_sample(libretro_audio_sample_callback);
    core->retro_set_audio_sample_batch(libretro_audio_sample_batch_callback);
    core->retro_set_input_poll(libretro_input_poll_callback);
    core->retro_set_input_state(libretro_input_state_callback);
    
    /* Initialize the core */
    core->retro_init();
    core->initialized = true;
    
    printf("Loaded LibRetro core: %s v%s\n", 
           core->system_info.library_name, 
           core->system_info.library_version);
    
    return core;
}

void libretro_unload_core(struct libretro_core *core) {
    if (!core) return;
    
    if (core->game_loaded) {
        libretro_unload_game(core);
    }
    
    if (core->initialized && core->retro_deinit) {
        core->retro_deinit();
    }
    
    if (core->handle) {
        dlclose(core->handle);
    }
    
    free(core->path);
    free(core->name);
    free(core);
}

int libretro_load_game(struct libretro_core *core, const char *game_path) {
    if (!core || !core->initialized) return -1;
    
    struct retro_game_info game_info = {0};
    
    /* Check if core needs full path or data */
    if (core->system_info.need_fullpath) {
        game_info.path = game_path;
        game_info.data = NULL;
        game_info.size = 0;
    } else {
        /* Load game data into memory */
        FILE *file = fopen(game_path, "rb");
        if (!file) {
            printf("Failed to open game file: %s\n", game_path);
            return -1;
        }
        
        fseek(file, 0, SEEK_END);
        long size = ftell(file);
        fseek(file, 0, SEEK_SET);
        
        void *data = malloc(size);
        if (!data) {
            fclose(file);
            return -1;
        }
        
        if (fread(data, 1, size, file) != size) {
            free(data);
            fclose(file);
            return -1;
        }
        fclose(file);
        
        game_info.path = game_path;
        game_info.data = data;
        game_info.size = size;
    }
    
    /* Load the game */
    if (!core->retro_load_game(&game_info)) {
        if (game_info.data) free((void*)game_info.data);
        printf("Failed to load game: %s\n", game_path);
        return -1;
    }
    
    /* Get AV info after loading game */
    core->retro_get_system_av_info(&core->av_info);
    core->game_loaded = true;
    
    printf("Loaded game: %s\n", game_path);
    printf("Resolution: %ux%u (max: %ux%u)\n", 
           core->av_info.geometry.base_width,
           core->av_info.geometry.base_height,
           core->av_info.geometry.max_width,
           core->av_info.geometry.max_height);
    printf("FPS: %.2f, Sample Rate: %.2f\n",
           core->av_info.timing.fps,
           core->av_info.timing.sample_rate);
    
    /* Clean up temporary data */
    if (game_info.data) free((void*)game_info.data);
    
    return 0;
}

void libretro_unload_game(struct libretro_core *core) {
    if (!core || !core->game_loaded) return;
    
    core->retro_unload_game();
    core->game_loaded = false;
    
    printf("Unloaded game\n");
}

void libretro_run_frame(struct libretro_core *core) {
    if (!core || !core->game_loaded) return;
    
    core->retro_run();
}

int libretro_save_state(struct libretro_core *core, const char *state_path) {
    if (!core || !core->game_loaded) return -1;
    
    size_t state_size = core->retro_serialize_size();
    if (state_size == 0) {
        printf("Core doesn't support save states\n");
        return -1;
    }
    
    void *state_data = malloc(state_size);
    if (!state_data) return -1;
    
    if (!core->retro_serialize(state_data, state_size)) {
        free(state_data);
        printf("Failed to serialize state\n");
        return -1;
    }
    
    FILE *file = fopen(state_path, "wb");
    if (!file) {
        free(state_data);
        printf("Failed to open state file for writing: %s\n", state_path);
        return -1;
    }
    
    if (fwrite(state_data, 1, state_size, file) != state_size) {
        fclose(file);
        free(state_data);
        printf("Failed to write state file\n");
        return -1;
    }
    
    fclose(file);
    free(state_data);
    
    printf("Saved state to: %s\n", state_path);
    return 0;
}

int libretro_load_state(struct libretro_core *core, const char *state_path) {
    if (!core || !core->game_loaded) return -1;
    
    FILE *file = fopen(state_path, "rb");
    if (!file) {
        printf("Failed to open state file: %s\n", state_path);
        return -1;
    }
    
    fseek(file, 0, SEEK_END);
    long state_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    void *state_data = malloc(state_size);
    if (!state_data) {
        fclose(file);
        return -1;
    }
    
    if (fread(state_data, 1, state_size, file) != state_size) {
        free(state_data);
        fclose(file);
        printf("Failed to read state file\n");
        return -1;
    }
    fclose(file);
    
    if (!core->retro_unserialize(state_data, state_size)) {
        free(state_data);
        printf("Failed to deserialize state\n");
        return -1;
    }
    
    free(state_data);
    printf("Loaded state from: %s\n", state_path);
    return 0;
}

char **libretro_discover_cores(const char *cores_dir) {
    DIR *dir = opendir(cores_dir);
    if (!dir) return NULL;
    
    char **cores = NULL;
    int core_count = 0;
    int core_capacity = 16;
    
    cores = malloc(core_capacity * sizeof(char*));
    if (!cores) {
        closedir(dir);
        return NULL;
    }
    
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type != DT_REG) continue;
        
        /* Check for .so extension */
        const char *ext = strrchr(entry->d_name, '.');
        if (!ext || strcmp(ext, ".so") != 0) continue;
        
        /* Check if it's a libretro core */
        if (strstr(entry->d_name, "_libretro") == NULL) continue;
        
        /* Expand array if needed */
        if (core_count >= core_capacity - 1) {
            core_capacity *= 2;
            char **new_cores = realloc(cores, core_capacity * sizeof(char*));
            if (!new_cores) {
                libretro_free_core_list(cores);
                closedir(dir);
                return NULL;
            }
            cores = new_cores;
        }
        
        /* Add full path to core */
        size_t path_len = strlen(cores_dir) + strlen(entry->d_name) + 2;
        cores[core_count] = malloc(path_len);
        if (!cores[core_count]) {
            libretro_free_core_list(cores);
            closedir(dir);
            return NULL;
        }
        
        snprintf(cores[core_count], path_len, "%s/%s", cores_dir, entry->d_name);
        core_count++;
    }
    
    closedir(dir);
    
    /* Null-terminate the array */
    cores[core_count] = NULL;
    
    return cores;
}

void libretro_free_core_list(char **cores) {
    if (!cores) return;
    
    for (int i = 0; cores[i]; i++) {
        free(cores[i]);
    }
    free(cores);
}

const char *libretro_find_core_for_rom(const char *rom_path) {
    rom_type_t rom_type = detect_rom_type(rom_path);
    return get_suggested_core_for_rom_type(rom_type);
}