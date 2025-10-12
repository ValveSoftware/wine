/*
 * Example program demonstrating Goliath WSL API usage
 * 
 * This program shows how to use the WSL compatibility layer
 * to launch Linux applications and manage the WSL environment.
 * 
 * Compile with: gcc -o wsl_example wsl_example.c -lwsl
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

// Include Goliath WSL header
#include "wsl.h"

int main(int argc, char *argv[]) {
    printf("Goliath WSL API Example\n");
    printf("======================\n\n");

    // Initialize WSL configuration
    printf("1. Initializing WSL configuration...\n");
    if (wsl_init_config() != 0) {
        fprintf(stderr, "Failed to initialize WSL configuration\n");
        return 1;
    }
    printf("   ✓ WSL configuration initialized\n\n");

    // Set up WSL environment
    printf("2. Setting up WSL environment...\n");
    if (wsl_setup_environment() != 0) {
        fprintf(stderr, "Failed to set up WSL environment\n");
        return 1;
    }
    printf("   ✓ WSL environment configured\n\n");

    // Set up filesystem structure
    printf("3. Setting up filesystem structure...\n");
    wsl_setup_filesystem();
    printf("   ✓ Basic Linux directories created\n\n");

    // Demonstrate path conversion
    printf("4. Testing path conversion...\n");
    const char *test_paths[] = {
        "C:\\Windows\\System32",
        "D:\\Users\\test\\Documents",
        "/usr/bin/ls",
        "relative/path/file.txt",
        NULL
    };

    for (int i = 0; test_paths[i]; i++) {
        char *converted = wsl_convert_path(test_paths[i]);
        if (converted) {
            printf("   %s -> %s\n", test_paths[i], converted);
            free(converted);
        }
    }
    printf("\n");

    // Launch a Linux application if provided
    if (argc > 1) {
        printf("5. Launching Linux application: %s\n", argv[1]);
        
        // Prepare arguments
        char **app_argv = malloc(sizeof(char*) * argc);
        if (!app_argv) {
            fprintf(stderr, "Out of memory\n");
            return 1;
        }
        
        for (int i = 1; i < argc; i++) {
            app_argv[i-1] = argv[i];
        }
        app_argv[argc-1] = NULL;

        // Launch the process
        pid_t pid = wsl_launch_process(argv[1], app_argv, NULL);
        if (pid > 0) {
            printf("   ✓ Process launched with PID %d\n", pid);
            
            // Wait for the process to complete
            int status = wsl_wait_for_process(pid);
            printf("   ✓ Process completed with exit code %d\n", status);
        } else {
            fprintf(stderr, "   ✗ Failed to launch process\n");
        }
        
        free(app_argv);
    } else {
        printf("5. No application specified to launch\n");
        printf("   Usage: %s <linux-binary> [args...]\n", argv[0]);
        printf("   Example: %s /bin/echo \"Hello WSL!\"\n", argv[0]);
    }

    // Display WSL information
    printf("\n6. WSL Information:\n");
    printf("   Distro Name: %s\n", wsl_get_distro_name());
    printf("   WSL Version: %s\n", wsl_get_version());
    printf("   Running in WSL: %s\n", wsl_is_running_in_wsl() ? "Yes" : "No");

    // Cleanup
    printf("\n7. Cleaning up...\n");
    wsl_cleanup();
    printf("   ✓ WSL cleanup completed\n");

    printf("\nGoliath WSL example completed successfully!\n");
    return 0;
}