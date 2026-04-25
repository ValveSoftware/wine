#ifndef __STEAMCLIENT_STRUCTS_H
#define __STEAMCLIENT_STRUCTS_H

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <assert.h>

#include <windef.h>
#include <winbase.h>

#ifdef __cplusplus
#include <array>
#endif /* __cplusplus */

#ifdef __cplusplus
#define U64_ARRAY( type, count, name ) std::array<type, count> name
#define U32_ARRAY( type, count, name ) std::array<type, count> name
#define W64_ARRAY( type, count, name ) std::array<type, count> name
#define W32_ARRAY( type, count, name ) std::array<type, count> name
#else
#define U64_ARRAY( type, count, name ) type name[count]
#define U32_ARRAY( type, count, name ) type name[count]
#define W64_ARRAY( type, count, name ) type name[count]
#define W32_ARRAY( type, count, name ) type name[count]
#endif

#define W_CDECL   __cdecl
#define W_STDCALL __stdcall

#if defined(WINE_UNIX_LIB)
#define U_CDECL   __attribute__((sysv_abi))
#define U_STDCALL __attribute__((sysv_abi))
#else
#define U_CDECL
#define U_STDCALL
#endif

#if defined(__cplusplus)
template< typename T >
#endif /* __cplusplus */
struct ptr32
{
    uint32_t value;

#if defined(__cplusplus)
    template< typename U > struct pointee { using type = U; };
    template< typename U > struct pointee< U** > { using type = struct ptr32< U* >*; };
    using Type = typename pointee< T >::type;

    struct ptr32& operator=( const Type ptr )
    {
        assert( (UINT64)ptr == (UINT_PTR)ptr );
        this->value = (UINT_PTR)ptr;
        return *this;
    }
    operator Type() const
    {
        return (Type)(UINT_PTR)this->value;
    }
#endif /* __cplusplus */
};

#ifdef __i386__
#define U64_PTR( decl, name, type ) uint64_t name
#define U32_PTR( decl, name, type ) decl
#define W64_PTR( decl, name, type ) uint64_t name
#define W32_PTR( decl, name, type ) decl
#endif

#if defined(__x86_64__) || defined(__aarch64__)
#define U64_PTR( decl, name, type ) decl
#define U32_PTR( decl, name, type ) uint32_t name
#define W64_PTR( decl, name, type ) decl
#if defined(__cplusplus)
#define W32_PTR( decl, name, type ) struct ptr32< type > name
#else /* __cplusplus */
#define W32_PTR( decl, name, type ) struct ptr32 name
#endif /* __cplusplus */
#endif

typedef struct { uint8_t _[8]; } CSteamID;
typedef struct { uint8_t _[8]; } CGameID;
typedef struct { uint8_t _[20]; } SteamIPAddress_t;
typedef struct { uint8_t _[120]; } ScePadTriggerEffectParam;
typedef char *SteamNetworkingErrMsg;

typedef struct SteamDatagramRelayAuthTicket SteamDatagramRelayAuthTicket;
typedef struct SteamDatagramHostedAddress SteamDatagramHostedAddress;
typedef struct SteamDatagramGameCoordinatorServerLogin SteamDatagramGameCoordinatorServerLogin;

#include "steamclient_structs_generated.h"

#define PATH_MAX 4096
extern char *g_tmppath;

struct u_iface
{
    UINT64 handle;
#ifdef __cplusplus
    struct u_iface &operator=( const void* value ) { this->handle = (UINT_PTR)value; return *this; }
    template< typename T > operator T*() const { return (T*)(UINT_PTR)this->handle; }
#endif /* __cplusplus */
};

struct u_buffer
{
    UINT64 ptr;
    UINT64 len;
#ifdef __cplusplus
    struct u_buffer &operator=(const char* value) { this->ptr = (UINT_PTR)value; this->len = value ? strlen( value ) + 1 : 0; return *this; }
    template< typename T > struct u_buffer &operator=(const T* value) { this->ptr = (UINT_PTR)value; this->len = value ? sizeof( *value ) : 0; return *this; }
    operator char*() const { return (char*)(UINT_PTR)this->ptr; }
#endif /* __cplusplus */
};

struct u_request
{
    UINT64 handle;
#ifdef __cplusplus
    struct u_request &operator=(const void* value) { this->handle = (UINT_PTR)value; return *this; }
    operator void*() const { return (void*)(UINT_PTR)this->handle; }
#endif /* __cplusplus */
};

struct u_response
{
    UINT64 handle;
#ifdef __cplusplus
    struct u_response &operator=(const void* value) { this->handle = (UINT_PTR)value; return *this; }
    template< typename T > operator T*() const { return (T*)(UINT_PTR)this->handle; }
#endif /* __cplusplus */
};

#endif /* __STEAMCLIENT_STRUCTS_H */
