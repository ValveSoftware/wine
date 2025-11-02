/* Android Translation Layer support */
static const char* find_atl_runtime() {
    /* Try to find ATL runtime in common paths */
    static const char* atl_paths[] = {
        "/usr/local/lib/atl/atl_runtime",
        "/usr/lib/atl/atl_runtime",
        "../libs/atl_runtime/atl_runtime",
        "./libs/atl_runtime/atl_runtime",
        NULL
    };
    
    for (int i = 0; atl_paths[i]; i++) {
        if (access(atl_paths[i], X_OK) == 0) {
            return atl_paths[i];
        }
    }
    return NULL;
}

int atl_main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "goliath: atl_main requires at least one argument\n");
        return 1;
    }

    const char *app_path = argv[1];
    const char *runtime = find_atl_runtime();
    
    if (!runtime) {
        fprintf(stderr, "goliath: ATL runtime not found\n");
        return 127;
    }

    /* Build the command line for ATL */
    char **new_argv = malloc(sizeof(char*) * (argc + 2));
    if (!new_argv) {
        fprintf(stderr, "goliath: out of memory\n");
        return 1;
    }
    
    new_argv[0] = (char*)runtime;
    for (int i = 1; i < argc; i++) {
        new_argv[i] = argv[i];
    }
    new_argv[argc] = NULL;

    pid_t pid = fork();
    if (pid == 0) {
        /* Child process */
        execv(runtime, new_argv);
        perror("goliath: execv failed for ATL runtime");
        exit(127);
    } else if (pid > 0) {
        /* Parent process */
        int status;
        waitpid(pid, &status, 0);
        free(new_argv);
        return WIFEXITED(status) ? WEXITSTATUS(status) : 127;
    }

    perror("goliath: fork failed");
    free(new_argv);
    return 127;
}
