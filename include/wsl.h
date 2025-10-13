#ifndef _WSL_H_
#define _WSL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>

// WSL compatibility layer for Goliath
// Provides Linux environment emulation similar to Microsoft's WSL

// WSL configuration structure
typedef struct {
    char *distro_name;
    char *default_user;
    char *home_dir;
    char *root_dir;
    int enable_interop;
    int enable_drive_mounting;
} wsl_config_t;

// WSL API functions
int wsl_main(int argc, char *argv[]);
int wsl_init_config(void);
int wsl_setup_environment(void);
int wsl_setup_filesystem(void);
int wsl_setup_signals(void);
char* wsl_convert_path(const char* path);
void wsl_cleanup(void);

// WSL detection and utility functions
int wsl_is_available(void);
int wsl_is_running_in_wsl(void);
const char* wsl_get_distro_name(void);
const char* wsl_get_version(void);

// Path conversion utilities
char* wsl_windows_to_linux_path(const char* windows_path);
char* wsl_linux_to_windows_path(const char* linux_path);

// Process management
pid_t wsl_launch_process(const char* binary, char* const argv[], char* const envp[]);
int wsl_wait_for_process(pid_t pid);

// Environment management
int wsl_set_environment_variable(const char* name, const char* value);
const char* wsl_get_environment_variable(const char* name);

// File system operations
int wsl_mount_windows_drives(void);
int wsl_create_linux_directories(void);

// Interoperability features
int wsl_enable_windows_interop(void);
int wsl_disable_windows_interop(void);

// Configuration management
int wsl_load_config(const char* config_file);
int wsl_save_config(const char* config_file);
wsl_config_t* wsl_get_config(void);

// Error handling
const char* wsl_get_last_error(void);
void wsl_set_error(const char* error_message);

// Version information
#define WSL_VERSION_MAJOR 1
#define WSL_VERSION_MINOR 0
#define WSL_VERSION_PATCH 0
#define WSL_VERSION_STRING "1.0.0"

// WSL feature flags
#define WSL_FEATURE_INTEROP         0x01
#define WSL_FEATURE_DRIVE_MOUNTING  0x02
#define WSL_FEATURE_SIGNAL_HANDLING 0x04
#define WSL_FEATURE_PATH_CONVERSION 0x08

// Default configuration values
#define WSL_DEFAULT_DISTRO_NAME "GoliathWSL"
#define WSL_DEFAULT_ROOT_DIR "/"
#define WSL_DEFAULT_SHELL "/bin/bash"

#ifdef __cplusplus
}
#endif

#endif // _WSL_H_
