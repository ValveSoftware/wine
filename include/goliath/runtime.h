#ifndef GOLIATH_RUNTIME_H
#define GOLIATH_RUNTIME_H

#include "goliath/core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Runtime operations interface */
typedef struct {
    /* Create a new process in this runtime */
    void *(*create_process)(const goliath_process_info_t *info);

    /* Start a process */
    int (*start_process)(void *process);

    /* Wait for process completion */
    int (*wait_process)(void *process, int *exit_code);

    /* Runtime cleanup */
    void (*cleanup)(void);
} goliath_runtime_ops_t;

/* Runtime registration (internal use) */
void goliath_register_runtime(goliath_runtime_t type, const goliath_runtime_ops_t *ops);

/* Wine runtime interface */
#ifdef HAVE_WINE
int wine_runtime_init(void);
const goliath_runtime_ops_t *wine_runtime_get_ops(void);
#endif

/* Darling runtime interface */
#ifdef HAVE_DARLING
int darling_runtime_init(void);
const goliath_runtime_ops_t *darling_runtime_get_ops(void);
#endif

/* WSL runtime interface */
#ifdef HAVE_WSL
int wsl_runtime_init(void);
const goliath_runtime_ops_t *wsl_runtime_get_ops(void);
#endif

/* ATL runtime interface */
#ifdef HAVE_ATL
int atl_runtime_init(void);
const goliath_runtime_ops_t *atl_runtime_get_ops(void);
#endif

/* libretro runtime interface */
#ifdef HAVE_LIBRETRO
int libretro_runtime_init(void);
const goliath_runtime_ops_t *libretro_runtime_get_ops(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* GOLIATH_RUNTIME_H */
