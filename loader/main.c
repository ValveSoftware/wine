/*
 * Emulator initialisation code
 *
 * Copyright 2000 Alexandre Julliard
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "config.h"

#include <stdarg.h>
#include <stddef.h>

#include "windef.h"
#include "winbase.h"
#include "winnt.h"

#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dlfcn.h>
#include <limits.h>
#ifdef HAVE_SYS_SYSCTL_H
# include <sys/sysctl.h>
#endif
#ifdef HAVE_LINK_H
# include <link.h>
#endif
#ifdef HAVE_SYS_LINK_H
# include <sys/link.h>
#endif

#include "wine/list.h"

#include "main.h"

extern char **environ;

/* the preloader will set these variables */
const struct wine_preload_info *wine_main_preload_info = NULL;
void (*wine_dl_debug_state)(void) = NULL;
struct r_debug *wine_r_debug = NULL;

#ifdef __linux__

struct link_map_entry
{
    struct link_map map;
    const void *module;
    struct list entry;
};

static struct list pe_link_map_entries = LIST_INIT( pe_link_map_entries );
static struct link_map so_link_map;
static struct link_map pe_link_map = {.l_prev = &so_link_map, .l_name = (char *)""};
static struct link_map so_link_map = {.l_name = (char *)""};
static pthread_mutex_t link_map_lock = PTHREAD_MUTEX_INITIALIZER;

static void sync_wine_link_map(void)
{
    static struct r_debug *_r_debug;
    struct link_map *next = &so_link_map, **rtld_map, **wine_map;

    if (!_r_debug) _r_debug = dlsym( RTLD_NEXT, "_r_debug" );
    rtld_map = &_r_debug->r_map;
    wine_map = &next;

    pthread_mutex_lock( &link_map_lock );

    /* unlink PE link map */
    pe_link_map.l_prev->l_next = NULL;

    while (*rtld_map)
    {
        if (!*wine_map)
        {
            if (!(*wine_map = calloc( 1, sizeof(struct link_map) ))) break;
            (*wine_map)->l_prev = pe_link_map.l_prev;
        }

        pe_link_map.l_prev = *wine_map;
        (*wine_map)->l_addr = (*rtld_map)->l_addr;
        (*wine_map)->l_name = strdup( (*rtld_map)->l_name );
        (*wine_map)->l_ld = (*rtld_map)->l_ld;
        rtld_map = &(*rtld_map)->l_next;
        wine_map = &(*wine_map)->l_next;
    }

    /* remove the remaining wine entries */
    next = *wine_map;
    *wine_map = NULL;

    while (next)
    {
        struct link_map *prev = next;
        wine_map = &next->l_next;
        next = *wine_map;
        *wine_map = NULL;
        free( prev->l_name );
        free( prev );
    }

    /* link PE link map back */
    pe_link_map.l_prev->l_next = &pe_link_map;

    pthread_mutex_unlock( &link_map_lock );

    if (wine_r_debug) wine_r_debug->r_map = &so_link_map;
    if (wine_dl_debug_state) wine_dl_debug_state();
}

static void add_dll_to_pe_link_map( const void *module, const char *unix_path )
{
    const IMAGE_DOS_HEADER *dos = (const IMAGE_DOS_HEADER *)module;
    const IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)((const BYTE *)dos + dos->e_lfanew);
    struct link_map_entry *entry;
    struct link_map *map_end;
    struct list *tail;

    if (!(entry = calloc( 1, sizeof(*entry) ))) return;

    entry->module = module;
    entry->map.l_addr = (char *)module - (char *)nt->OptionalHeader.ImageBase;
    entry->map.l_name = strdup( unix_path );

    if ((tail = list_tail( &pe_link_map_entries )))
    {
        entry->map.l_prev = &LIST_ENTRY( tail, struct link_map_entry, entry )->map;
        entry->map.l_prev->l_next = &entry->map;
    }
    else
    {
        map_end = &pe_link_map;
        while (map_end->l_next) map_end = map_end->l_next;

        map_end->l_next = &entry->map;
        map_end->l_next->l_prev = map_end;
    }

    list_add_tail( &pe_link_map_entries, &entry->entry );
}

void wine_gdb_dll_loaded( const void *module, const char *unix_path )
{
    struct link_map_entry *entry;

    pthread_mutex_lock( &link_map_lock );

    LIST_FOR_EACH_ENTRY( entry, &pe_link_map_entries, struct link_map_entry, entry )
        if (entry->module == module) break;

    if (&entry->entry == &pe_link_map_entries)
        add_dll_to_pe_link_map( module, unix_path );
    else
    {
        const IMAGE_DOS_HEADER *dos = (const IMAGE_DOS_HEADER *)module;
        const IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)((const BYTE *)dos + dos->e_lfanew);
        entry->map.l_addr = (char *)module - (char *)nt->OptionalHeader.ImageBase;
    }

    pthread_mutex_unlock( &link_map_lock );

    sync_wine_link_map();
}

void wine_gdb_dll_unload( const void *module )
{
    struct link_map_entry *entry;

    pthread_mutex_lock( &link_map_lock );

    LIST_FOR_EACH_ENTRY( entry, &pe_link_map_entries, struct link_map_entry, entry )
        if (entry->module == module) break;

    if (&entry->entry == &pe_link_map_entries)
    {
        pthread_mutex_unlock( &link_map_lock );
        return;
    }

    list_remove( &entry->entry );

    if (entry->map.l_prev) entry->map.l_prev->l_next = entry->map.l_next;
    if (entry->map.l_next) entry->map.l_next->l_prev = entry->map.l_prev;

    pthread_mutex_unlock( &link_map_lock );

    sync_wine_link_map();

    free( entry->map.l_name );
    free( entry );
}

void *dlopen( const char *file, int mode )
{
    static typeof(dlopen) *rtld_dlopen;
    void *ret;

    if (!rtld_dlopen) rtld_dlopen = dlsym( RTLD_NEXT, "dlopen" );
    ret = rtld_dlopen( file, mode );

    sync_wine_link_map();
    return ret;
}

int dlclose( void *handle )
{
    static typeof(dlclose) *rtld_dlclose;
    int ret;

    if (!rtld_dlclose) rtld_dlclose = dlsym( RTLD_NEXT, "dlclose" );
    ret = rtld_dlclose( handle );

    sync_wine_link_map();
    return ret;
}

#endif /* __linux__ */

/* canonicalize path and return its directory name */
static char *realpath_dirname( const char *name )
{
    char *p, *fullpath = realpath( name, NULL );

    if (fullpath)
    {
        p = strrchr( fullpath, '/' );
        if (p == fullpath) p++;
        if (p) *p = 0;
    }
    return fullpath;
}

/* if string ends with tail, remove it */
static char *remove_tail( const char *str, const char *tail )
{
    size_t len = strlen( str );
    size_t tail_len = strlen( tail );
    char *ret;

    if (len < tail_len) return NULL;
    if (strcmp( str + len - tail_len, tail )) return NULL;
    ret = malloc( len - tail_len + 1 );
    memcpy( ret, str, len - tail_len );
    ret[len - tail_len] = 0;
    return ret;
}

/* build a path from the specified dir and name */
static char *build_path( const char *dir, const char *name )
{
    size_t len = strlen( dir );
    char *ret = malloc( len + strlen( name ) + 2 );

    memcpy( ret, dir, len );
    if (len && ret[len - 1] != '/') ret[len++] = '/';
    strcpy( ret + len, name );
    return ret;
}

static const char *get_self_exe( char *argv0 )
{
#if defined(__linux__) || defined(__FreeBSD_kernel__) || defined(__NetBSD__)
    return "/proc/self/exe";
#elif defined (__FreeBSD__) || defined(__DragonFly__)
    static int pathname[] = { CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1 };
    size_t path_size = PATH_MAX;
    char *path = malloc( path_size );
    if (path && !sysctl( pathname, sizeof(pathname)/sizeof(pathname[0]), path, &path_size, NULL, 0 ))
        return path;
    free( path );
#endif

    if (!strchr( argv0, '/' )) /* search in PATH */
    {
        char *p, *path = getenv( "PATH" );

        if (!path || !(path = strdup(path))) return NULL;
        for (p = strtok( path, ":" ); p; p = strtok( NULL, ":" ))
        {
            char *name = build_path( p, argv0 );
            if (!access( name, X_OK ))
            {
                free( path );
                return name;
            }
            free( name );
        }
        free( path );
        return NULL;
    }
    return argv0;
}

static void *try_dlopen( const char *dir, const char *name )
{
    char *path = build_path( dir, name );
    void *handle = dlopen( path, RTLD_NOW );
    free( path );
    return handle;
}

static void *load_ntdll( char *argv0 )
{
#ifdef __i386__
#define SO_DIR "i386-unix/"
#elif defined(__x86_64__)
#define SO_DIR "x86_64-unix/"
#elif defined(__arm__)
#define SO_DIR "arm-unix/"
#elif defined(__aarch64__)
#define SO_DIR "aarch64-unix/"
#else
#define SO_DIR ""
#endif
    const char *self = get_self_exe( argv0 );
    char *path, *p;
    void *handle = NULL;

    if (self && ((path = realpath_dirname( self ))))
    {
        if ((p = remove_tail( path, "/loader" )))
        {
            handle = try_dlopen( p, "dlls/ntdll/ntdll.so" );
            free( p );
        }
        else handle = try_dlopen( path, BIN_TO_DLLDIR "/" SO_DIR "ntdll.so" );
        free( path );
    }

    if (!handle && (path = getenv( "WINEDLLPATH" )))
    {
        path = strdup( path );
        for (p = strtok( path, ":" ); p; p = strtok( NULL, ":" ))
        {
            handle = try_dlopen( p, SO_DIR "ntdll.so" );
            if (!handle) handle = try_dlopen( p, "ntdll.so" );
            if (handle) break;
        }
        free( path );
    }

    if (!handle && !self) handle = try_dlopen( DLLDIR, SO_DIR "ntdll.so" );

    return handle;
}


/**********************************************************************
 *           main
 */
int main( int argc, char *argv[] )
{
    void *handle;

    if ((handle = load_ntdll( argv[0] )))
    {
        void (*init_func)(int, char **, char **) = dlsym( handle, "__wine_main" );
        if (init_func) init_func( argc, argv, environ );
        fprintf( stderr, "wine: __wine_main function not found in ntdll.so\n" );
        exit(1);
    }

    fprintf( stderr, "wine: could not load ntdll.so: %s\n", dlerror() );
    pthread_detach( pthread_self() );  /* force importing libpthread for OpenGL */
    exit(1);
}
