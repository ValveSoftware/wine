#include "goliath/core.h"
#include "goliath/runtime.h"
#include <stdlib.h>
#include <string.h>

/* Internal process structure */
struct goliath_process {
    char *path;                 /* Executable path */
    char **argv;               /* Arguments */
    char **envp;               /* Environment */
    goliath_runtime_t runtime; /* Active runtime */
    void *runtime_data;        /* Runtime-specific data */
    int running;               /* Process state */
    int exit_code;            /* Exit code if terminated */
};

/* Runtime registry */
static struct {
    goliath_runtime_ops_t *runtimes[GOLIATH_RUNTIME_LIBRETRO + 1];
    int initialized;
} g_goliath = {0};

int goliath_init(void)
{
    if (g_goliath.initialized)
        return 0;

    /* Initialize runtime registry */
    memset(g_goliath.runtimes, 0, sizeof(g_goliath.runtimes));

    /* Register built-in runtimes */
#ifdef HAVE_WINE
    if (wine_runtime_init())
        g_goliath.runtimes[GOLIATH_RUNTIME_WINE] = wine_runtime_get_ops();
#endif

#ifdef HAVE_DARLING
    if (darling_runtime_init())
        g_goliath.runtimes[GOLIATH_RUNTIME_DARLING] = darling_runtime_get_ops();
#endif

#ifdef HAVE_WSL
    if (wsl_runtime_init())
        g_goliath.runtimes[GOLIATH_RUNTIME_WSL] = wsl_runtime_get_ops();
#endif

#ifdef HAVE_ATL 
    if (atl_runtime_init())
        g_goliath.runtimes[GOLIATH_RUNTIME_ATL] = atl_runtime_get_ops();
#endif

#ifdef HAVE_LIBRETRO
    if (libretro_runtime_init())
        g_goliath.runtimes[GOLIATH_RUNTIME_LIBRETRO] = libretro_runtime_get_ops();
#endif

    g_goliath.initialized = 1;
    return 0;
}

void goliath_cleanup(void)
{
    if (!g_goliath.initialized)
        return;

    /* Cleanup runtimes in reverse order */
    for (int i = GOLIATH_RUNTIME_LIBRETRO; i >= 0; i--) {
        if (g_goliath.runtimes[i] && g_goliath.runtimes[i]->cleanup)
            g_goliath.runtimes[i]->cleanup();
    }

    memset(g_goliath.runtimes, 0, sizeof(g_goliath.runtimes));
    g_goliath.initialized = 0;
}

goliath_process_t *goliath_process_create(const goliath_process_info_t *info)
{
    if (!info || !info->path)
        return NULL;

    goliath_process_t *process = calloc(1, sizeof(*process));
    if (!process)
        return NULL;

    process->path = strdup(info->path);
    if (!process->path)
        goto error;

    /* Detect binary type and runtime if not specified */
    goliath_binary_type_t bin_type = goliath_get_binary_type(info->path);
    if (info->runtime == GOLIATH_RUNTIME_NONE) {
        /* Auto-select runtime based on binary type */
        switch (bin_type) {
            case GOLIATH_BINARY_PE:
                process->runtime = GOLIATH_RUNTIME_WINE;
                break;
            case GOLIATH_BINARY_MACHO:
                process->runtime = GOLIATH_RUNTIME_DARLING;
                break;
            case GOLIATH_BINARY_DEX:
                process->runtime = GOLIATH_RUNTIME_ATL;
                break;
            case GOLIATH_BINARY_ELF:
                process->runtime = GOLIATH_RUNTIME_WSL;
                break;
            case GOLIATH_BINARY_RETRO:
                process->runtime = GOLIATH_RUNTIME_LIBRETRO;
                break;
            default:
                goto error;
        }
    } else {
        process->runtime = info->runtime;
    }

    /* Initialize runtime */
    if (!g_goliath.runtimes[process->runtime])
        goto error;

    process->runtime_data = g_goliath.runtimes[process->runtime]->create_process(info);
    if (!process->runtime_data)
        goto error;

    return process;

error:
    if (process) {
        free(process->path);
        free(process);
    }
    return NULL;
}

int goliath_process_start(goliath_process_t *process)
{
    if (!process || !g_goliath.runtimes[process->runtime])
        return -1;

    return g_goliath.runtimes[process->runtime]->start_process(process->runtime_data);
}

int goliath_process_wait(goliath_process_t *process, int *exit_code)
{
    if (!process || !g_goliath.runtimes[process->runtime])
        return -1;

    return g_goliath.runtimes[process->runtime]->wait_process(process->runtime_data, exit_code);
}

goliath_runtime_t goliath_process_get_runtime(const goliath_process_t *process)
{
    return process ? process->runtime : GOLIATH_RUNTIME_NONE;
}

/* Basic binary type detection */
goliath_binary_type_t goliath_get_binary_type(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return GOLIATH_BINARY_UNKNOWN;

    unsigned char magic[4];
    if (fread(magic, 1, sizeof(magic), f) != sizeof(magic)) {
        fclose(f);
        return GOLIATH_BINARY_UNKNOWN;
    }
    fclose(f);

    /* Check magic numbers */
    if (magic[0] == 'M' && magic[1] == 'Z')  /* DOS/PE */
        return GOLIATH_BINARY_PE;
    
    if (magic[0] == 0xCF && magic[1] == 0xFA) /* Mach-O */
        return GOLIATH_BINARY_MACHO;
    
    if (magic[0] == 0x7F && magic[1] == 'E' && 
        magic[2] == 'L' && magic[3] == 'F')   /* ELF */
        return GOLIATH_BINARY_ELF;

    if (magic[0] == 'd' && magic[1] == 'e' &&
        magic[2] == 'x' && magic[3] == '\n')  /* DEX */
        return GOLIATH_BINARY_DEX;

    /* libretro cores are usually ELF .so files with specific symbols */
    return GOLIATH_BINARY_UNKNOWN;
}
