/*
 * Win32 builtin dlls support
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
#include "wine/port.h"

#include <assert.h>
#include <ctype.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif
#ifdef HAVE_SYS_RESOURCE_H
# include <sys/resource.h>
#endif
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef __APPLE__
#include <crt_externs.h>
#define environ (*_NSGetEnviron())
#include <CoreFoundation/CoreFoundation.h>
#define LoadResource MacLoadResource
#define GetCurrentThread MacGetCurrentThread
#include <CoreServices/CoreServices.h>
#undef LoadResource
#undef GetCurrentThread
#include <pthread.h>
#include <mach-o/getsect.h>
#else
extern char **environ;
#endif

#ifdef __ANDROID__
#include <jni.h>
#endif

#define NONAMELESSUNION
#define NONAMELESSSTRUCT
#include "windef.h"
#include "winbase.h"
#include "wine/library.h"

/* argc/argv for the Windows application */
int __wine_main_argc = 0;
char **__wine_main_argv = NULL;
WCHAR **__wine_main_wargv = NULL;
char **__wine_main_environ = NULL;

struct dll_path_context
{
    unsigned int index; /* current index in the dll path list */
    char *buffer;       /* buffer used for storing path names */
    char *name;         /* start of file name part in buffer (including leading slash) */
    int   namelen;      /* length of file name without .so extension */
    int   win16;        /* 16-bit dll search */
};

#define MAX_DLLS 100

static struct
{
    const IMAGE_NT_HEADERS *nt;           /* NT header */
    const char             *filename;     /* DLL file name */
} builtin_dlls[MAX_DLLS];

static int nb_dlls;

static const IMAGE_NT_HEADERS *main_exe;

static load_dll_callback_t load_dll_callback;

static const char *build_dir;
static const char *default_dlldir;
static const char **dll_paths;
static unsigned int nb_dll_paths;
static int dll_path_maxlen;

extern void mmap_init(void);
extern const char *get_dlldir( const char **default_dlldir );

/* build the dll load path from the WINEDLLPATH variable */
static void build_dll_path(void)
{
    int len, count = 0, modcount = 0;
    char *p, *path = getenv( "WINEDLLPATH" ), *modpath = getenv( "WINEMODPATH" );
    const char *dlldir = get_dlldir( &default_dlldir );

    if (path)
    {
        /* count how many path elements we need */
        path = strdup(path);
        p = path;
        while (*p)
        {
            while (*p == ':') p++;
            if (!*p) break;
            count++;
            while (*p && *p != ':') p++;
        }
    }

    if (modpath)
    {
        /* count how many path elements we need */
        modpath = strdup(modpath);
        p = modpath;
        while (*p)
        {
            while (*p == ':') p++;
            if (!*p) break;
            modcount++;
            while (*p && *p != ':') p++;
        }
    }

    dll_paths = malloc( (count+modcount+2) * sizeof(*dll_paths) );
    nb_dll_paths = 0;

    if (dlldir)
    {
        dll_path_maxlen = strlen(dlldir);
    }
    else if ((build_dir = wine_get_build_dir()))
    {
        dll_path_maxlen = strlen(build_dir) + sizeof("/programs");
    }

    if (modcount)
    {
        p = modpath;
        while (*p)
        {
            while (*p == ':') *p++ = 0;
            if (!*p) break;
            dll_paths[nb_dll_paths] = p;
            while (*p && *p != ':') p++;
            if (p - dll_paths[nb_dll_paths] > dll_path_maxlen)
                dll_path_maxlen = p - dll_paths[nb_dll_paths];
            nb_dll_paths++;
        }
    }

    if (dlldir)
    {
        dll_paths[nb_dll_paths++] = dlldir;
    }

    if (count)
    {
        p = path;
        while (*p)
        {
            while (*p == ':') *p++ = 0;
            if (!*p) break;
            dll_paths[nb_dll_paths] = p;
            while (*p && *p != ':') p++;
            if (p - dll_paths[nb_dll_paths] > dll_path_maxlen)
                dll_path_maxlen = p - dll_paths[nb_dll_paths];
            nb_dll_paths++;
        }
    }

    /* append default dll dir (if not empty) to path */
    if ((len = strlen(default_dlldir)) > 0)
    {
        if (len > dll_path_maxlen) dll_path_maxlen = len;
        dll_paths[nb_dll_paths++] = default_dlldir;
    }
}

/* check if the library is the correct architecture */
/* only returns false for a valid library of the wrong arch */
static int check_library_arch( int fd )
{
#ifdef __APPLE__
    struct  /* Mach-O header */
    {
        unsigned int magic;
        unsigned int cputype;
    } header;

    if (read( fd, &header, sizeof(header) ) != sizeof(header)) return 1;
    if (header.magic != 0xfeedface) return 1;
    if (sizeof(void *) == sizeof(int)) return !(header.cputype >> 24);
    else return (header.cputype >> 24) == 1; /* CPU_ARCH_ABI64 */
#else
    struct  /* ELF header */
    {
        unsigned char magic[4];
        unsigned char class;
        unsigned char data;
        unsigned char version;
    } header;

    if (read( fd, &header, sizeof(header) ) != sizeof(header)) return 1;
    if (memcmp( header.magic, "\177ELF", 4 )) return 1;
    if (header.version != 1 /* EV_CURRENT */) return 1;
#ifdef WORDS_BIGENDIAN
    if (header.data != 2 /* ELFDATA2MSB */) return 1;
#else
    if (header.data != 1 /* ELFDATA2LSB */) return 1;
#endif
    if (sizeof(void *) == sizeof(int)) return header.class == 1; /* ELFCLASS32 */
    else return header.class == 2; /* ELFCLASS64 */
#endif
}

/* check if a given file can be opened */
static inline int file_exists( const char *name )
{
    int ret = 0;
    int fd = open( name, O_RDONLY );
    if (fd != -1)
    {
        ret = check_library_arch( fd );
        close( fd );
    }
    return ret;
}

static inline char *prepend( char *buffer, const char *str, size_t len )
{
    return memcpy( buffer - len, str, len );
}

/* get a filename from the next entry in the dll path */
static char *next_dll_path( struct dll_path_context *context )
{
    unsigned int index = context->index++;
    int namelen = context->namelen;
    char *path = context->name;

    switch(index)
    {
    case 0:  /* try dlls dir with subdir prefix */
        if (namelen > 4 && !memcmp( context->name + namelen - 4, ".dll", 4 )) namelen -= 4;
        if (!context->win16) path = prepend( path, context->name, namelen );
        path = prepend( path, "/dlls", sizeof("/dlls") - 1 );
        path = prepend( path, build_dir, strlen(build_dir) );
        return path;
    case 1:  /* try programs dir with subdir prefix */
        if (!context->win16)
        {
            if (namelen > 4 && !memcmp( context->name + namelen - 4, ".exe", 4 )) namelen -= 4;
            path = prepend( path, context->name, namelen );
            path = prepend( path, "/programs", sizeof("/programs") - 1 );
            path = prepend( path, build_dir, strlen(build_dir) );
            return path;
        }
        context->index++;
        /* fall through */
    default:
        index -= 2;
        if (index >= nb_dll_paths) return NULL;
        path = prepend( path, dll_paths[index], strlen( dll_paths[index] ));
        return path;
    }
}


/* get a filename from the first entry in the dll path */
static char *first_dll_path( const char *name, int win16, struct dll_path_context *context )
{
    char *p;
    int namelen = strlen( name );
    const char *ext = win16 ? "16" : ".so";

    context->buffer = malloc( dll_path_maxlen + 2 * namelen + strlen(ext) + 3 );
    context->index = build_dir ? 0 : 2;  /* if no build dir skip all the build dir magic cases */
    context->name = context->buffer + dll_path_maxlen + namelen + 1;
    context->namelen = namelen + 1;
    context->win16 = win16;

    /* store the name at the end of the buffer, followed by extension */
    p = context->name;
    *p++ = '/';
    memcpy( p, name, namelen );
    strcpy( p + namelen, ext );
    return next_dll_path( context );
}


/* free the dll path context created by first_dll_path */
static inline void free_dll_path( struct dll_path_context *context )
{
    free( context->buffer );
}


/* open a library for a given dll, searching in the dll path
 * 'name' must be the Windows dll name (e.g. "kernel32.dll") */
static void *dlopen_dll( const char *name, char *error, int errorsize,
                         int test_only, int *exists )
{
    struct dll_path_context context;
    char *path;
    void *ret = NULL;

    *exists = 0;
    for (path = first_dll_path( name, 0, &context ); path; path = next_dll_path( &context ))
    {
        if (!test_only && (ret = wine_dlopen( path, RTLD_NOW, error, errorsize ))) break;
        if ((*exists = file_exists( path ))) break; /* exists but cannot be loaded, return the error */
    }
    free_dll_path( &context );
    return ret;
}


/* adjust an array of pointers to make them into RVAs */
static inline void fixup_rva_ptrs( void *array, BYTE *base, unsigned int count )
{
    void **src = (void **)array;
    DWORD *dst = (DWORD *)array;
    while (count--)
    {
        *dst++ = *src ? (BYTE *)*src - base : 0;
        src++;
    }
}

/* fixup an array of RVAs by adding the specified delta */
static inline void fixup_rva_dwords( DWORD *ptr, int delta, unsigned int count )
{
    while (count--)
    {
        if (*ptr) *ptr += delta;
        ptr++;
    }
}


/* fixup an array of name/ordinal RVAs by adding the specified delta */
static inline void fixup_rva_names( UINT_PTR *ptr, int delta )
{
    while (*ptr)
    {
        if (!(*ptr & IMAGE_ORDINAL_FLAG)) *ptr += delta;
        ptr++;
    }
}


/* fixup RVAs in the import directory */
static void fixup_imports( IMAGE_IMPORT_DESCRIPTOR *dir, BYTE *base, int delta )
{
    while (dir->Name)
    {
        fixup_rva_dwords( &dir->u.OriginalFirstThunk, delta, 1 );
        fixup_rva_dwords( &dir->Name, delta, 1 );
        fixup_rva_dwords( &dir->FirstThunk, delta, 1 );
        if (dir->u.OriginalFirstThunk) fixup_rva_names( (UINT_PTR *)(base + dir->u.OriginalFirstThunk), delta );
        if (dir->FirstThunk) fixup_rva_names( (UINT_PTR *)(base + dir->FirstThunk), delta );
        dir++;
    }
}


/* fixup RVAs in the export directory */
static void fixup_exports( IMAGE_EXPORT_DIRECTORY *dir, BYTE *base, int delta )
{
    fixup_rva_dwords( &dir->Name, delta, 1 );
    fixup_rva_dwords( &dir->AddressOfFunctions, delta, 1 );
    fixup_rva_dwords( &dir->AddressOfNames, delta, 1 );
    fixup_rva_dwords( &dir->AddressOfNameOrdinals, delta, 1 );
    fixup_rva_dwords( (DWORD *)(base + dir->AddressOfNames), delta, dir->NumberOfNames );
    fixup_rva_ptrs( (base + dir->AddressOfFunctions), base, dir->NumberOfFunctions );
}


/* fixup RVAs in the resource directory */
static void fixup_resources( IMAGE_RESOURCE_DIRECTORY *dir, BYTE *root, int delta )
{
    IMAGE_RESOURCE_DIRECTORY_ENTRY *entry;
    int i;

    entry = (IMAGE_RESOURCE_DIRECTORY_ENTRY *)(dir + 1);
    for (i = 0; i < dir->NumberOfNamedEntries + dir->NumberOfIdEntries; i++, entry++)
    {
        void *ptr = root + entry->u2.s2.OffsetToDirectory;
        if (entry->u2.s2.DataIsDirectory) fixup_resources( ptr, root, delta );
        else
        {
            IMAGE_RESOURCE_DATA_ENTRY *data = ptr;
            fixup_rva_dwords( &data->OffsetToData, delta, 1 );
        }
    }
}


/* map a builtin dll in memory and fixup RVAs */
static void *map_dll( const IMAGE_NT_HEADERS *nt_descr )
{
#ifdef HAVE_MMAP
    IMAGE_DATA_DIRECTORY *dir;
    IMAGE_DOS_HEADER *dos;
    IMAGE_NT_HEADERS *nt;
    IMAGE_SECTION_HEADER *sec;
    BYTE *addr;
    DWORD code_start, code_end, data_start, data_end;
    const size_t page_size = sysconf( _SC_PAGESIZE );
    const size_t page_mask = page_size - 1;
    int delta, nb_sections = 2;  /* code + data */
    unsigned int i;
#ifdef __APPLE__
    Dl_info dli;
    unsigned long data_size;
#endif

    size_t size = (sizeof(IMAGE_DOS_HEADER)
                   + sizeof(IMAGE_NT_HEADERS)
                   + nb_sections * sizeof(IMAGE_SECTION_HEADER));

    assert( size <= page_size );

    /* module address must be aligned on 64K boundary */
    addr = (BYTE *)((nt_descr->OptionalHeader.ImageBase + 0xffff) & ~0xffff);
    if (wine_anon_mmap( addr, page_size, PROT_READ|PROT_WRITE, MAP_FIXED ) != addr) return NULL;

    dos    = (IMAGE_DOS_HEADER *)addr;
    nt     = (IMAGE_NT_HEADERS *)(dos + 1);
    sec    = (IMAGE_SECTION_HEADER *)(nt + 1);

    /* Build the DOS and NT headers */

    dos->e_magic    = IMAGE_DOS_SIGNATURE;
    dos->e_cblp     = 0x90;
    dos->e_cp       = 3;
    dos->e_cparhdr  = (sizeof(*dos)+0xf)/0x10;
    dos->e_minalloc = 0;
    dos->e_maxalloc = 0xffff;
    dos->e_ss       = 0x0000;
    dos->e_sp       = 0x00b8;
    dos->e_lfarlc   = sizeof(*dos);
    dos->e_lfanew   = sizeof(*dos);

    *nt = *nt_descr;

    delta      = (const BYTE *)nt_descr - addr;
    code_start = page_size;
    data_start = delta & ~page_mask;
#ifdef __APPLE__
    /* Need the mach_header, not the PE header, to give to getsegmentdata(3) */
    dladdr(addr, &dli);
    code_end   = getsegmentdata(dli.dli_fbase, "__DATA", &data_size) - addr;
    data_end   = (code_end + data_size + page_mask) & ~page_mask;
#else
    code_end   = data_start;
    data_end   = (nt->OptionalHeader.SizeOfImage + delta + page_mask) & ~page_mask;
#endif

    fixup_rva_ptrs( &nt->OptionalHeader.AddressOfEntryPoint, addr, 1 );

    nt->FileHeader.NumberOfSections                = nb_sections;
    nt->OptionalHeader.BaseOfCode                  = code_start;
#ifndef _WIN64
    nt->OptionalHeader.BaseOfData                  = data_start;
#endif
    nt->OptionalHeader.SizeOfCode                  = code_end - code_start;
    nt->OptionalHeader.SizeOfInitializedData       = data_end - data_start;
    nt->OptionalHeader.SizeOfUninitializedData     = 0;
    nt->OptionalHeader.SizeOfImage                 = data_end;
    nt->OptionalHeader.ImageBase                   = (ULONG_PTR)addr;

    /* Build the code section */

    memcpy( sec->Name, ".text", sizeof(".text") );
    sec->SizeOfRawData = code_end - code_start;
    sec->Misc.VirtualSize = sec->SizeOfRawData;
    sec->VirtualAddress   = code_start;
    sec->PointerToRawData = code_start;
    sec->Characteristics  = (IMAGE_SCN_CNT_CODE | IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_MEM_READ);
    sec++;

    /* Build the data section */

    memcpy( sec->Name, ".data", sizeof(".data") );
    sec->SizeOfRawData = data_end - data_start;
    sec->Misc.VirtualSize = sec->SizeOfRawData;
    sec->VirtualAddress   = data_start;
    sec->PointerToRawData = data_start;
    sec->Characteristics  = (IMAGE_SCN_CNT_INITIALIZED_DATA |
                             IMAGE_SCN_MEM_WRITE | IMAGE_SCN_MEM_READ);
    sec++;

    for (i = 0; i < nt->OptionalHeader.NumberOfRvaAndSizes; i++)
        fixup_rva_dwords( &nt->OptionalHeader.DataDirectory[i].VirtualAddress, delta, 1 );

    /* Build the import directory */

    dir = &nt->OptionalHeader.DataDirectory[IMAGE_FILE_IMPORT_DIRECTORY];
    if (dir->Size)
    {
        IMAGE_IMPORT_DESCRIPTOR *imports = (void *)(addr + dir->VirtualAddress);
        fixup_imports( imports, addr, delta );
    }

    /* Build the resource directory */

    dir = &nt->OptionalHeader.DataDirectory[IMAGE_FILE_RESOURCE_DIRECTORY];
    if (dir->Size)
    {
        void *ptr = (void *)(addr + dir->VirtualAddress);
        fixup_resources( ptr, ptr, delta );
    }

    /* Build the export directory */

    dir = &nt->OptionalHeader.DataDirectory[IMAGE_FILE_EXPORT_DIRECTORY];
    if (dir->Size)
    {
        IMAGE_EXPORT_DIRECTORY *exports = (void *)(addr + dir->VirtualAddress);
        fixup_exports( exports, addr, delta );
    }
    return addr;
#else  /* HAVE_MMAP */
    return NULL;
#endif  /* HAVE_MMAP */
}


/***********************************************************************
 *           __wine_get_main_environment
 *
 * Return an environment pointer to work around lack of environ variable.
 * Only exported on Mac OS.
 */
char **__wine_get_main_environment(void)
{
    return environ;
}


/***********************************************************************
 *           __wine_dll_register
 *
 * Register a built-in DLL descriptor.
 */
void __wine_dll_register( const IMAGE_NT_HEADERS *header, const char *filename )
{
    if (load_dll_callback) load_dll_callback( map_dll(header), filename );
    else
    {
        if (!(header->FileHeader.Characteristics & IMAGE_FILE_DLL))
            main_exe = header;
        else
        {
            assert( nb_dlls < MAX_DLLS );
            builtin_dlls[nb_dlls].nt = header;
            builtin_dlls[nb_dlls].filename = filename;
            nb_dlls++;
        }
    }
}


/***********************************************************************
 *           wine_dll_set_callback
 *
 * Set the callback function for dll loading, and call it
 * for all dlls that were implicitly loaded already.
 */
void wine_dll_set_callback( load_dll_callback_t load )
{
    int i;
    load_dll_callback = load;
    for (i = 0; i < nb_dlls; i++)
    {
        const IMAGE_NT_HEADERS *nt = builtin_dlls[i].nt;
        if (!nt) continue;
        builtin_dlls[i].nt = NULL;
        load_dll_callback( map_dll(nt), builtin_dlls[i].filename );
    }
    nb_dlls = 0;
    if (main_exe) load_dll_callback( map_dll(main_exe), "" );
}


/***********************************************************************
 *           wine_dll_load
 *
 * Load a builtin dll.
 */
void *wine_dll_load( const char *filename, char *error, int errorsize, int *file_exists )
{
    int i;

    /* callback must have been set already */
    assert( load_dll_callback );

    /* check if we have it in the list */
    /* this can happen when initializing pre-loaded dlls in wine_dll_set_callback */
    for (i = 0; i < nb_dlls; i++)
    {
        if (!builtin_dlls[i].nt) continue;
        if (!strcmp( builtin_dlls[i].filename, filename ))
        {
            const IMAGE_NT_HEADERS *nt = builtin_dlls[i].nt;
            builtin_dlls[i].nt = NULL;
            load_dll_callback( map_dll(nt), builtin_dlls[i].filename );
            *file_exists = 1;
            return (void *)1;
        }
    }
    return dlopen_dll( filename, error, errorsize, 0, file_exists );
}


/***********************************************************************
 *           wine_dll_unload
 *
 * Unload a builtin dll.
 */
void wine_dll_unload( void *handle )
{
    if (handle != (void *)1)
	wine_dlclose( handle, NULL, 0 );
}


/***********************************************************************
 *           wine_dll_load_main_exe
 *
 * Try to load the .so for the main exe.
 */
void *wine_dll_load_main_exe( const char *name, char *error, int errorsize,
                              int test_only, int *file_exists )
{
    return dlopen_dll( name, error, errorsize, test_only, file_exists );
}


/***********************************************************************
 *           wine_dll_enum_load_path
 *
 * Enumerate the dll load path.
 */
const char *wine_dll_enum_load_path( unsigned int index )
{
    if (index >= nb_dll_paths) return NULL;
    return dll_paths[index];
}


/***********************************************************************
 *           wine_dll_get_owner
 *
 * Retrieve the name of the 32-bit owner dll for a 16-bit dll.
 * Return 0 if OK, -1 on error.
 */
int wine_dll_get_owner( const char *name, char *buffer, int size, int *exists )
{
    int ret = -1;
    char *path;
    struct dll_path_context context;

    *exists = 0;

    for (path = first_dll_path( name, 1, &context ); path; path = next_dll_path( &context ))
    {
        int fd = open( path, O_RDONLY );
        if (fd != -1)
        {
            int res = read( fd, buffer, size - 1 );
            while (res > 0 && (buffer[res-1] == '\n' || buffer[res-1] == '\r')) res--;
            buffer[res] = 0;
            close( fd );
            *exists = 1;
            ret = 0;
            break;
        }
    }
    free_dll_path( &context );
    return ret;
}


/***********************************************************************
 *           set_max_limit
 *
 * Set a user limit to the maximum allowed value.
 */
static void set_max_limit( int limit )
{
#ifdef HAVE_SETRLIMIT
    struct rlimit rlimit;

    if (!getrlimit( limit, &rlimit ))
    {
        rlimit.rlim_cur = rlimit.rlim_max;
        if (setrlimit( limit, &rlimit ) != 0)
        {
#if defined(__APPLE__) && defined(RLIMIT_NOFILE) && defined(OPEN_MAX)
            /* On Leopard, setrlimit(RLIMIT_NOFILE, ...) fails on attempts to set
             * rlim_cur above OPEN_MAX (even if rlim_max > OPEN_MAX). */
            if (limit == RLIMIT_NOFILE && rlimit.rlim_cur > OPEN_MAX)
            {
                rlimit.rlim_cur = OPEN_MAX;
                setrlimit( limit, &rlimit );
            }
#endif
        }
    }
#endif
}


#ifdef __APPLE__
struct apple_stack_info
{
    void *stack;
    size_t desired_size;
};

/***********************************************************************
 *           apple_alloc_thread_stack
 *
 * Callback for wine_mmap_enum_reserved_areas to allocate space for
 * the secondary thread's stack.
 */
#ifndef _WIN64
static int apple_alloc_thread_stack( void *base, size_t size, void *arg )
{
    struct apple_stack_info *info = arg;

    /* For mysterious reasons, putting the thread stack at the very top
     * of the address space causes subsequent execs to fail, even on the
     * child side of a fork.  Avoid the top 16MB. */
    char * const limit = (char*)0xff000000;
    if ((char *)base >= limit) return 0;
    if (size > limit - (char*)base)
        size = limit - (char*)base;
    if (size < info->desired_size) return 0;
    info->stack = wine_anon_mmap( (char *)base + size - info->desired_size,
                                  info->desired_size, PROT_READ|PROT_WRITE, MAP_FIXED );
    return (info->stack != (void *)-1);
}
#endif

/***********************************************************************
 *           apple_create_wine_thread
 *
 * Spin off a secondary thread to complete Wine initialization, leaving
 * the original thread for the Mac frameworks.
 *
 * Invoked as a CFRunLoopSource perform callback.
 */
static void apple_create_wine_thread( void *init_func )
{
    int success = 0;
    pthread_t thread;
    pthread_attr_t attr;

    if (!pthread_attr_init( &attr ))
    {
#ifndef _WIN64
        struct apple_stack_info info;

        /* Try to put the new thread's stack in the reserved area.  If this
         * fails, just let it go wherever.  It'll be a waste of space, but we
         * can go on. */
        if (!pthread_attr_getstacksize( &attr, &info.desired_size ) &&
            wine_mmap_enum_reserved_areas( apple_alloc_thread_stack, &info, 1 ))
        {
            wine_mmap_remove_reserved_area( info.stack, info.desired_size, 0 );
            pthread_attr_setstackaddr( &attr, (char*)info.stack + info.desired_size );
        }
#endif

        if (!pthread_attr_setdetachstate( &attr, PTHREAD_CREATE_JOINABLE ) &&
            !pthread_create( &thread, &attr, init_func, NULL ))
            success = 1;

        pthread_attr_destroy( &attr );
    }

    /* Failure is indicated by returning from wine_init().  Stopping
     * the run loop allows apple_main_thread() and thus wine_init() to
     * return. */
    if (!success)
        CFRunLoopStop( CFRunLoopGetCurrent() );
}


/***********************************************************************
 *           apple_main_thread
 *
 * Park the process's original thread in a Core Foundation run loop for
 * use by the Mac frameworks, especially receiving and handling
 * distributed notifications.  Spin off a new thread for the rest of the
 * Wine initialization.
 */
static void apple_main_thread( void (*init_func)(void) )
{
    CFRunLoopSourceContext source_context = { 0 };
    CFRunLoopSourceRef source;

    if (!pthread_main_np())
    {
        init_func();
        return;
    }

    /* Multi-processing Services can get confused about the main thread if the
     * first time it's used is on a secondary thread.  Use it here to make sure
     * that doesn't happen. */
    MPTaskIsPreemptive(MPCurrentTaskID());

    /* Give ourselves the best chance of having the distributed notification
     * center scheduled on this thread's run loop.  In theory, it's scheduled
     * in the first thread to ask for it. */
    CFNotificationCenterGetDistributedCenter();

    /* We use this run loop source for two purposes.  First, a run loop exits
     * if it has no more sources scheduled.  So, we need at least one source
     * to keep the run loop running.  Second, although it's not critical, it's
     * preferable for the Wine initialization to not proceed until we know
     * the run loop is running.  So, we signal our source immediately after
     * adding it and have its callback spin off the Wine thread. */
    source_context.info = init_func;
    source_context.perform = apple_create_wine_thread;
    source = CFRunLoopSourceCreate( NULL, 0, &source_context );

    if (source)
    {
        CFRunLoopAddSource( CFRunLoopGetCurrent(), source, kCFRunLoopCommonModes );
        CFRunLoopSourceSignal( source );
        CFRelease( source );

        CFRunLoopRun(); /* Should never return, except on error. */
    }

    /* If we get here (i.e. return), that indicates failure to our caller. */
}
#endif


#ifdef __ANDROID__

#ifndef WINE_JAVA_CLASS
#define WINE_JAVA_CLASS "org/winehq/wine/WineActivity"
#endif

static JavaVM *java_vm;
static jobject java_object;

/* return the Java VM that was used for JNI initialisation */
JavaVM *wine_get_java_vm(void)
{
    return java_vm;
}

/* return the Java object that called the wine_init method */
jobject wine_get_java_object(void)
{
    return java_object;
}

/* main Wine initialisation */
static jstring wine_init_jni( JNIEnv *env, jobject obj, jobjectArray cmdline, jobjectArray environment )
{
    char **argv;
    char *str;
    char error[1024];
    int i, argc, length;

    /* get the command line array */

    argc = (*env)->GetArrayLength( env, cmdline );
    for (i = length = 0; i < argc; i++)
    {
        jobject str_obj = (*env)->GetObjectArrayElement( env, cmdline, i );
        length += (*env)->GetStringUTFLength( env, str_obj ) + 1;
    }

    argv = malloc( (argc + 1) * sizeof(*argv) + length );
    str = (char *)(argv + argc + 1);
    for (i = 0; i < argc; i++)
    {
        jobject str_obj = (*env)->GetObjectArrayElement( env, cmdline, i );
        length = (*env)->GetStringUTFLength( env, str_obj );
        (*env)->GetStringUTFRegion( env, str_obj, 0,
                                    (*env)->GetStringLength( env, str_obj ), str );
        argv[i] = str;
        str[length] = 0;
        str += length + 1;
    }
    argv[argc] = NULL;

    /* set the environment variables */

    if (environment)
    {
        int count = (*env)->GetArrayLength( env, environment );
        for (i = 0; i < count - 1; i += 2)
        {
            jobject var_obj = (*env)->GetObjectArrayElement( env, environment, i );
            jobject val_obj = (*env)->GetObjectArrayElement( env, environment, i + 1 );
            const char *var = (*env)->GetStringUTFChars( env, var_obj, NULL );

            if (val_obj)
            {
                const char *val = (*env)->GetStringUTFChars( env, val_obj, NULL );
                setenv( var, val, 1 );
                if (!strcmp( var, "LD_LIBRARY_PATH" ))
                {
                    void (*update_func)( const char * ) = dlsym( RTLD_DEFAULT,
                                                                 "android_update_LD_LIBRARY_PATH" );
                    if (update_func) update_func( val );
                }
                else if (!strcmp( var, "WINEDEBUGLOG" ))
                {
                    int fd = open( val, O_WRONLY | O_CREAT | O_APPEND, 0666 );
                    if (fd != -1)
                    {
                        dup2( fd, 2 );
                        close( fd );
                    }
                }
                (*env)->ReleaseStringUTFChars( env, val_obj, val );
            }
            else unsetenv( var );

            (*env)->ReleaseStringUTFChars( env, var_obj, var );
        }
    }

    java_object = (*env)->NewGlobalRef( env, obj );

#ifdef __i386__
    {
        unsigned short java_fs = wine_get_fs();
        wine_set_fs( 0 );
        wine_init( argc, argv, error, sizeof(error) );
        wine_set_fs( java_fs );
    }
#else
    wine_init( argc, argv, error, sizeof(error) );
#endif
    return (*env)->NewStringUTF( env, error );
}

jint JNI_OnLoad( JavaVM *vm, void *reserved )
{
    static const JNINativeMethod method =
    {
        "wine_init", "([Ljava/lang/String;[Ljava/lang/String;)Ljava/lang/String;", wine_init_jni
    };

    JNIEnv *env;
    jclass class;

    java_vm = vm;
    if ((*vm)->AttachCurrentThread( vm, &env, NULL ) != JNI_OK) return JNI_ERR;
    if (!(class = (*env)->FindClass( env, WINE_JAVA_CLASS ))) return JNI_ERR;
    (*env)->RegisterNatives( env, class, &method, 1 );
    return JNI_VERSION_1_6;
}

#endif  /* __ANDROID__ */

/***********************************************************************
 *           wine_init
 *
 * Main Wine initialisation.
 */
void wine_init( int argc, char *argv[], char *error, int error_size )
{
    struct dll_path_context context;
    char *path;
    void *ntdll = NULL;
    void (*init_func)(void);

    /* force a few limits that are set too low on some platforms */
#ifdef RLIMIT_NOFILE
    set_max_limit( RLIMIT_NOFILE );
#endif
#ifdef RLIMIT_AS
    set_max_limit( RLIMIT_AS );
#endif

    wine_init_argv0_path( argv[0] );
    build_dll_path();
    __wine_main_argc = argc;
    __wine_main_argv = argv;
    __wine_main_environ = __wine_get_main_environment();
    mmap_init();

    for (path = first_dll_path( "ntdll.dll", 0, &context ); path; path = next_dll_path( &context ))
    {
        if ((ntdll = wine_dlopen( path, RTLD_NOW, error, error_size )))
        {
            /* if we didn't use the default dll dir, remove it from the search path */
            if (default_dlldir[0] && context.index < nb_dll_paths + 2) nb_dll_paths--;
            break;
        }
    }
    free_dll_path( &context );

    if (!ntdll) return;
    if (!(init_func = wine_dlsym( ntdll, "__wine_process_init", error, error_size ))) return;
#ifdef __APPLE__
    apple_main_thread( init_func );
#else
    init_func();
#endif
}


/*
 * These functions provide wrappers around dlopen() and associated
 * functions.  They work around a bug in glibc 2.1.x where calling
 * a dl*() function after a previous dl*() function has failed
 * without a dlerror() call between the two will cause a crash.
 * They all take a pointer to a buffer that
 * will receive the error description (from dlerror()).  This
 * parameter may be NULL if the error description is not required.
 */

#ifndef RTLD_FIRST
#define RTLD_FIRST 0
#endif

/***********************************************************************
 *		wine_dlopen
 */
void *wine_dlopen( const char *filename, int flag, char *error, size_t errorsize )
{
#ifdef HAVE_DLOPEN
    void *ret;
    const char *s;

#ifdef __APPLE__
    /* the Mac OS loader pretends to be able to load PE files, so avoid them here */
    unsigned char magic[2];
    int fd = open( filename, O_RDONLY );
    if (fd != -1)
    {
        if (pread( fd, magic, 2, 0 ) == 2 && magic[0] == 'M' && magic[1] == 'Z')
        {
            if (error && errorsize)
            {
                static const char msg[] = "MZ format";
                size_t len = min( errorsize, sizeof(msg) );
                memcpy( error, msg, len );
                error[len - 1] = 0;
            }
            close( fd );
            return NULL;
        }
        close( fd );
    }
#endif
    dlerror(); dlerror();
#ifdef __sun
    if (strchr( filename, ':' ))
    {
        char path[PATH_MAX];
        /* Solaris' brain damaged dlopen() treats ':' as a path separator */
        realpath( filename, path );
        ret = dlopen( path, flag | RTLD_FIRST );
    }
    else
#endif
    ret = dlopen( filename, flag | RTLD_FIRST );
    s = dlerror();
    if (error && errorsize)
    {
        if (s)
        {
            size_t len = strlen(s);
            if (len >= errorsize) len = errorsize - 1;
            memcpy( error, s, len );
            error[len] = 0;
        }
        else error[0] = 0;
    }
    dlerror();
    return ret;
#else
    if (error)
    {
        static const char msg[] = "dlopen interface not detected by configure";
        size_t len = min( errorsize, sizeof(msg) );
        memcpy( error, msg, len );
        error[len - 1] = 0;
    }
    return NULL;
#endif
}

/***********************************************************************
 *		wine_dlsym
 */
void *wine_dlsym( void *handle, const char *symbol, char *error, size_t errorsize )
{
#ifdef HAVE_DLOPEN
    void *ret;
    const char *s;
    dlerror(); dlerror();
    ret = dlsym( handle, symbol );
    s = dlerror();
    if (error && errorsize)
    {
        if (s)
        {
            size_t len = strlen(s);
            if (len >= errorsize) len = errorsize - 1;
            memcpy( error, s, len );
            error[len] = 0;
        }
        else error[0] = 0;
    }
    dlerror();
    return ret;
#else
    if (error)
    {
        static const char msg[] = "dlopen interface not detected by configure";
        size_t len = min( errorsize, sizeof(msg) );
        memcpy( error, msg, len );
        error[len - 1] = 0;
    }
    return NULL;
#endif
}

/***********************************************************************
 *		wine_dlclose
 */
int wine_dlclose( void *handle, char *error, size_t errorsize )
{
#ifdef HAVE_DLOPEN
    int ret;
    const char *s;
    dlerror(); dlerror();
    ret = dlclose( handle );
    s = dlerror();
    if (error && errorsize)
    {
        if (s)
        {
            size_t len = strlen(s);
            if (len >= errorsize) len = errorsize - 1;
            memcpy( error, s, len );
            error[len] = 0;
        }
        else error[0] = 0;
    }
    dlerror();
    return ret;
#else
    if (error)
    {
        static const char msg[] = "dlopen interface not detected by configure";
        size_t len = min( errorsize, sizeof(msg) );
        memcpy( error, msg, len );
        error[len - 1] = 0;
    }
    return 1;
#endif
}
