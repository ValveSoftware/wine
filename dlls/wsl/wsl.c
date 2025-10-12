
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <fcntl.h>
#include <signal.h>
#include <pwd.h>
#include <grp.h>

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

static wsl_config_t g_wsl_config = {
    .distro_name = "GoliathWSL",
    .default_user = NULL,
    .home_dir = NULL,
    .root_dir = "/",
    .enable_interop = 1,
    .enable_drive_mounting = 1
};

// Initialize WSL configuration
static int wsl_init_config(void) {
    // Get current user information
    struct passwd *pw = getpwuid(getuid());
    if (pw) {
        g_wsl_config.default_user = strdup(pw->pw_name);
        g_wsl_config.home_dir = strdup(pw->pw_dir);
    } else {
        g_wsl_config.default_user = strdup("goliath");
        g_wsl_config.home_dir = strdup("/tmp");
    }
    
    return 0;
}

// Set up WSL environment variables
static int wsl_setup_environment(void) {
    // Core Linux environment
    setenv("PATH", "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin", 1);
    
    if (g_wsl_config.home_dir) {
        setenv("HOME", g_wsl_config.home_dir, 1);
    }
    
    if (g_wsl_config.default_user) {
        setenv("USER", g_wsl_config.default_user, 1);
        setenv("LOGNAME", g_wsl_config.default_user, 1);
    }
    
    // WSL-specific environment variables
    setenv("WSL_DISTRO_NAME", g_wsl_config.distro_name, 1);
    setenv("WSL_INTEROP", g_wsl_config.enable_interop ? "/run/WSL/interop" : "", 1);
    
    // Terminal and locale settings
    setenv("TERM", "xterm-256color", 0);
    setenv("LANG", "C.UTF-8", 0);
    setenv("LC_ALL", "C.UTF-8", 0);
    
    // Shell settings
    setenv("SHELL", "/bin/bash", 0);
    
    return 0;
}

// Convert Windows paths to Linux paths (basic implementation)
static char* wsl_convert_path(const char* path) {
    if (!path) return NULL;
    
    // If it's already a Linux path, return as-is
    if (path[0] == '/') {
        return strdup(path);
    }
    
    // Handle Windows drive letters (C:\path -> /mnt/c/path)
    if (strlen(path) >= 3 && path[1] == ':' && (path[2] == '\\' || path[2] == '/')) {
        char drive = path[0];
        if (drive >= 'A' && drive <= 'Z') drive += 32; // Convert to lowercase
        
        char *linux_path = malloc(strlen(path) + 10);
        if (!linux_path) return NULL;
        
        sprintf(linux_path, "/mnt/%c", drive);
        
        // Convert remaining path, replacing backslashes with forward slashes
        const char *src = path + 2;
        char *dst = linux_path + strlen(linux_path);
        
        while (*src) {
            if (*src == '\\') {
                *dst++ = '/';
            } else {
                *dst++ = *src;
            }
            src++;
        }
        *dst = '\0';
        
        return linux_path;
    }
    
    // For relative paths, return as-is
    return strdup(path);
}

// Set up basic Linux filesystem structure if needed
static int wsl_setup_filesystem(void) {
    // Create basic directories if they don't exist
    const char *dirs[] = {
        "/tmp", "/var", "/var/tmp", "/var/log", "/run", "/dev", "/proc", "/sys", NULL
    };
    
    for (int i = 0; dirs[i]; i++) {
        struct stat st;
        if (stat(dirs[i], &st) != 0) {
            if (mkdir(dirs[i], 0755) != 0 && errno != EEXIST) {
                fprintf(stderr, "[Goliath/WSL] Warning: Could not create directory %s: %s\n", 
                        dirs[i], strerror(errno));
            }
        }
    }
    
    return 0;
}

// Signal handler for proper cleanup
static void wsl_signal_handler(int sig) {
    // Forward signal to child processes
    // This is a simplified implementation
    signal(sig, SIG_DFL);
    raise(sig);
}

// Set up signal handling
static int wsl_setup_signals(void) {
    signal(SIGINT, wsl_signal_handler);
    signal(SIGTERM, wsl_signal_handler);
    signal(SIGQUIT, wsl_signal_handler);
    return 0;
}

// Main WSL entry point
int wsl_main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "[Goliath/WSL] Usage: %s <linux-elf-binary> [args...]\n", argv[0]);
        fprintf(stderr, "[Goliath/WSL] WSL compatibility layer for running Linux applications\n");
        return 1;
    }

    // Initialize WSL configuration
    if (wsl_init_config() != 0) {
        fprintf(stderr, "[Goliath/WSL] Failed to initialize WSL configuration\n");
        return 1;
    }

    // Set up WSL environment
    if (wsl_setup_environment() != 0) {
        fprintf(stderr, "[Goliath/WSL] Failed to set up WSL environment\n");
        return 1;
    }

    // Set up basic filesystem structure
    wsl_setup_filesystem();

    // Set up signal handling
    wsl_setup_signals();

    // Convert the binary path if needed
    char *binary_path = wsl_convert_path(argv[1]);
    if (!binary_path) {
        fprintf(stderr, "[Goliath/WSL] Failed to convert path: %s\n", argv[1]);
        return 1;
    }

    // Check if the binary exists and is executable
    if (access(binary_path, X_OK) != 0) {
        fprintf(stderr, "[Goliath/WSL] Binary not found or not executable: %s\n", binary_path);
        free(binary_path);
        return 1;
    }

    printf("[Goliath/WSL] Launching Linux application: %s\n", binary_path);

    // Prepare arguments for exec
    char **new_argv = malloc(sizeof(char*) * argc);
    if (!new_argv) {
        fprintf(stderr, "[Goliath/WSL] Out of memory\n");
        free(binary_path);
        return 1;
    }

    new_argv[0] = binary_path;
    for (int i = 2; i < argc; i++) {
        new_argv[i-1] = argv[i];
    }
    new_argv[argc-1] = NULL;

    // Fork and execute the Linux binary
    pid_t pid = fork();
    if (pid == 0) {
        // Child process: execute the Linux ELF binary
        execv(binary_path, new_argv);
        perror("[Goliath/WSL] execv failed");
        exit(127);
    } else if (pid > 0) {
        // Parent process: wait for child and handle signals
        int status = 0;
        int result = waitpid(pid, &status, 0);
        
        free(binary_path);
        free(new_argv);
        
        if (result == -1) {
            perror("[Goliath/WSL] waitpid failed");
            return 127;
        }
        
        if (WIFEXITED(status)) {
            return WEXITSTATUS(status);
        } else if (WIFSIGNALED(status)) {
            fprintf(stderr, "[Goliath/WSL] Process terminated by signal %d\n", WTERMSIG(status));
            return 128 + WTERMSIG(status);
        } else {
            return 127;
        }
    } else {
        perror("[Goliath/WSL] fork failed");
        free(binary_path);
        free(new_argv);
        return 127;
    }
}

// Cleanup function
void wsl_cleanup(void) {
    if (g_wsl_config.default_user) {
        free(g_wsl_config.default_user);
        g_wsl_config.default_user = NULL;
    }
    if (g_wsl_config.home_dir) {
        free(g_wsl_config.home_dir);
        g_wsl_config.home_dir = NULL;
    }
}
