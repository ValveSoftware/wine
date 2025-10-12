/*
 * LibRetro API Integration for Goliath
 *
 * Copyright 2025 Goliath Project
 * Licensed under LGPL v2.1 or later
 */

#ifndef LIBRETRO_API_H
#define LIBRETRO_API_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* LibRetro API definitions - subset of libretro.h */
#define RETRO_API_VERSION         1

#define RETRO_DEVICE_NONE         0
#define RETRO_DEVICE_JOYPAD       1
#define RETRO_DEVICE_MOUSE        2
#define RETRO_DEVICE_KEYBOARD     3
#define RETRO_DEVICE_LIGHTGUN     4
#define RETRO_DEVICE_ANALOG       5
#define RETRO_DEVICE_POINTER      6

#define RETRO_ENVIRONMENT_SET_ROTATION  1
#define RETRO_ENVIRONMENT_GET_OVERSCAN  2
#define RETRO_ENVIRONMENT_GET_CAN_DUPE  3
#define RETRO_ENVIRONMENT_SET_MESSAGE   6
#define RETRO_ENVIRONMENT_SHUTDOWN      7
#define RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL 8
#define RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY 9
#define RETRO_ENVIRONMENT_SET_PIXEL_FORMAT 10
#define RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS 11
#define RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK 12
#define RETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE 13
#define RETRO_ENVIRONMENT_SET_HW_RENDER 14
#define RETRO_ENVIRONMENT_GET_VARIABLE 15
#define RETRO_ENVIRONMENT_SET_VARIABLES 16
#define RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE 17
#define RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME 18
#define RETRO_ENVIRONMENT_GET_LIBRETRO_PATH 19
#define RETRO_ENVIRONMENT_SET_FRAME_TIME_CALLBACK 21
#define RETRO_ENVIRONMENT_SET_AUDIO_CALLBACK 22
#define RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE 23
#define RETRO_ENVIRONMENT_GET_INPUT_DEVICE_CAPABILITIES 24
#define RETRO_ENVIRONMENT_GET_SENSOR_INTERFACE 25
#define RETRO_ENVIRONMENT_GET_CAMERA_INTERFACE 26
#define RETRO_ENVIRONMENT_GET_LOG_INTERFACE 27
#define RETRO_ENVIRONMENT_GET_PERF_INTERFACE 28
#define RETRO_ENVIRONMENT_GET_LOCATION_INTERFACE 29
#define RETRO_ENVIRONMENT_GET_CONTENT_DIRECTORY 30
#define RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY 31

enum retro_pixel_format
{
   RETRO_PIXEL_FORMAT_0RGB1555 = 0,
   RETRO_PIXEL_FORMAT_XRGB8888 = 1,
   RETRO_PIXEL_FORMAT_RGB565   = 2,
   RETRO_PIXEL_FORMAT_UNKNOWN  = INT_MAX
};

struct retro_message
{
   const char *msg;
   unsigned    frames;
};

struct retro_system_info
{
   const char *library_name;
   const char *library_version;
   const char *valid_extensions;
   bool        need_fullpath;
   bool        block_extract;
};

struct retro_game_geometry
{
   unsigned base_width;
   unsigned base_height;
   unsigned max_width;
   unsigned max_height;
   float    aspect_ratio;
};

struct retro_system_timing
{
   double fps;
   double sample_rate;
};

struct retro_system_av_info
{
   struct retro_game_geometry geometry;
   struct retro_system_timing timing;
};

struct retro_variable
{
   const char *key;
   const char *value;
};

struct retro_game_info
{
   const char *path;
   const void *data;
   size_t      size;
   const char *meta;
};

/* LibRetro core function pointers */
typedef void (*retro_init_t)(void);
typedef void (*retro_deinit_t)(void);
typedef unsigned (*retro_api_version_t)(void);
typedef void (*retro_get_system_info_t)(struct retro_system_info *info);
typedef void (*retro_get_system_av_info_t)(struct retro_system_av_info *info);
typedef void (*retro_set_environment_t)(bool (*cb)(unsigned cmd, void *data));
typedef void (*retro_set_video_refresh_t)(void (*cb)(const void *data, unsigned width, unsigned height, size_t pitch));
typedef void (*retro_set_audio_sample_t)(void (*cb)(int16_t left, int16_t right));
typedef void (*retro_set_audio_sample_batch_t)(size_t (*cb)(const int16_t *data, size_t frames));
typedef void (*retro_set_input_poll_t)(void (*cb)(void));
typedef void (*retro_set_input_state_t)(int16_t (*cb)(unsigned port, unsigned device, unsigned index, unsigned id));
typedef bool (*retro_load_game_t)(const struct retro_game_info *game);
typedef void (*retro_unload_game_t)(void);
typedef unsigned (*retro_get_region_t)(void);
typedef void *(*retro_get_memory_data_t)(unsigned id);
typedef size_t (*retro_get_memory_size_t)(unsigned id);
typedef void (*retro_run_t)(void);
typedef void (*retro_reset_t)(void);
typedef size_t (*retro_serialize_size_t)(void);
typedef bool (*retro_serialize_t)(void *data, size_t size);
typedef bool (*retro_unserialize_t)(const void *data, size_t size);

/* LibRetro core structure */
struct libretro_core {
    void *handle;
    char *path;
    char *name;
    
    /* Core function pointers */
    retro_init_t retro_init;
    retro_deinit_t retro_deinit;
    retro_api_version_t retro_api_version;
    retro_get_system_info_t retro_get_system_info;
    retro_get_system_av_info_t retro_get_system_av_info;
    retro_set_environment_t retro_set_environment;
    retro_set_video_refresh_t retro_set_video_refresh;
    retro_set_audio_sample_t retro_set_audio_sample;
    retro_set_audio_sample_batch_t retro_set_audio_sample_batch;
    retro_set_input_poll_t retro_set_input_poll;
    retro_set_input_state_t retro_set_input_state;
    retro_load_game_t retro_load_game;
    retro_unload_game_t retro_unload_game;
    retro_get_region_t retro_get_region;
    retro_get_memory_data_t retro_get_memory_data;
    retro_get_memory_size_t retro_get_memory_size;
    retro_run_t retro_run;
    retro_reset_t retro_reset;
    retro_serialize_size_t retro_serialize_size;
    retro_serialize_t retro_serialize;
    retro_unserialize_t retro_unserialize;
    
    /* Core info */
    struct retro_system_info system_info;
    struct retro_system_av_info av_info;
    bool initialized;
    bool game_loaded;
};

/* Goliath LibRetro API functions */
int libretro_init_api(void);
void libretro_cleanup_api(void);
struct libretro_core *libretro_load_core(const char *core_path);
void libretro_unload_core(struct libretro_core *core);
int libretro_load_game(struct libretro_core *core, const char *game_path);
void libretro_unload_game(struct libretro_core *core);
void libretro_run_frame(struct libretro_core *core);
int libretro_save_state(struct libretro_core *core, const char *state_path);
int libretro_load_state(struct libretro_core *core, const char *state_path);

/* Core discovery and management */
char **libretro_discover_cores(const char *cores_dir);
void libretro_free_core_list(char **cores);
const char *libretro_find_core_for_rom(const char *rom_path);

/* Callback functions */
bool libretro_environment_callback(unsigned cmd, void *data);
void libretro_video_refresh_callback(const void *data, unsigned width, unsigned height, size_t pitch);
void libretro_audio_sample_callback(int16_t left, int16_t right);
size_t libretro_audio_sample_batch_callback(const int16_t *data, size_t frames);
void libretro_input_poll_callback(void);
int16_t libretro_input_state_callback(unsigned port, unsigned device, unsigned index, unsigned id);

#endif /* LIBRETRO_API_H */