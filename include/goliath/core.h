#ifndef GOLIATH_CORE_H
#define GOLIATH_CORE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Binary types that Goliath can handle */
typedef enum {
    GOLIATH_BINARY_UNKNOWN = 0,
    GOLIATH_BINARY_PE,      /* Windows PE/COFF */
    GOLIATH_BINARY_MACHO,   /* macOS Mach-O */
    GOLIATH_BINARY_ELF,     /* Linux ELF */
    GOLIATH_BINARY_DEX,     /* Android Dalvik */
    GOLIATH_BINARY_RETRO    /* libretro core */
} goliath_binary_type_t;

/* Runtime identification */
typedef enum {
    GOLIATH_RUNTIME_NONE = 0,
    GOLIATH_RUNTIME_WINE,    /* Windows runtime */
    GOLIATH_RUNTIME_DARLING, /* macOS runtime */
    GOLIATH_RUNTIME_WSL,     /* Linux runtime */
    GOLIATH_RUNTIME_ATL,     /* Android runtime */
    GOLIATH_RUNTIME_LIBRETRO /* Game runtime */
} goliath_runtime_t;

/* Process handle */
typedef struct goliath_process goliath_process_t;

/* Process creation flags */
typedef enum {
    GOLIATH_PROCESS_DEFAULT = 0,
    GOLIATH_PROCESS_SUSPENDED = 1 << 0,
    GOLIATH_PROCESS_INHERIT_ENV = 1 << 1
} goliath_process_flags_t;

/* Process creation info */
typedef struct {
    const char *path;           /* Path to executable */
    char **argv;               /* Argument vector */
    char **envp;               /* Environment */
    goliath_runtime_t runtime; /* Preferred runtime, or NONE for auto */
    uint32_t flags;           /* Creation flags */
} goliath_process_info_t;

/* Initialize the Goliath core system */
int goliath_init(void);

/* Clean up and shut down */
void goliath_cleanup(void);

/* Create a new process */
goliath_process_t *goliath_process_create(const goliath_process_info_t *info);

/* Start a suspended process */
int goliath_process_start(goliath_process_t *process);

/* Wait for process exit */
int goliath_process_wait(goliath_process_t *process, int *exit_code);

/* Get process runtime type */
goliath_runtime_t goliath_process_get_runtime(const goliath_process_t *process);

/* Get binary type */
goliath_binary_type_t goliath_get_binary_type(const char *path);

#ifdef __cplusplus
}
#endif

#endif /* GOLIATH_CORE_H */
