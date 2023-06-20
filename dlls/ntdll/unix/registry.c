/*
 * Registry functions
 *
 * Copyright 1999 Juergen Schmied
 * Copyright 2000 Alexandre Julliard
 * Copyright 2005 Ivan Leo Puoti, Laurent Pinchart
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
 *
 */

#if 0
#pragma makedep unix
#endif

#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "winternl.h"
#include "unix_private.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(reg);

/* maximum length of a value name in bytes (without terminating null) */
#define MAX_VALUE_LENGTH (16383 * sizeof(WCHAR))


NTSTATUS open_hkcu_key( const char *path, HANDLE *key )
{
    NTSTATUS status;
    char buffer[256];
    WCHAR bufferW[256];
    DWORD_PTR sid_data[(sizeof(TOKEN_USER) + SECURITY_MAX_SID_SIZE) / sizeof(DWORD_PTR)];
    DWORD i, len = sizeof(sid_data);
    SID *sid;
    UNICODE_STRING name;
    OBJECT_ATTRIBUTES attr;

    status = NtQueryInformationToken( GetCurrentThreadEffectiveToken(), TokenUser, sid_data, len, &len );
    if (status) return status;

    sid = ((TOKEN_USER *)sid_data)->User.Sid;
    len = snprintf( buffer, sizeof(buffer), "\\Registry\\User\\S-%u-%u", sid->Revision,
                   MAKELONG( MAKEWORD( sid->IdentifierAuthority.Value[5], sid->IdentifierAuthority.Value[4] ),
                             MAKEWORD( sid->IdentifierAuthority.Value[3], sid->IdentifierAuthority.Value[2] )));
    for (i = 0; i < sid->SubAuthorityCount; i++)
        len += snprintf( buffer + len, sizeof(buffer) - len, "-%u", sid->SubAuthority[i] );
    len += snprintf( buffer + len, sizeof(buffer) - len, "\\%s", path );

    ascii_to_unicode( bufferW, buffer, len + 1 );
    init_unicode_string( &name, bufferW );
    InitializeObjectAttributes( &attr, &name, OBJ_CASE_INSENSITIVE, 0, NULL );
    return NtCreateKey( key, KEY_ALL_ACCESS, &attr, 0, NULL, 0, NULL );
}

/* dump a Unicode string with proper escaping */
int dump_strW( const WCHAR *str, data_size_t len, FILE *f, const char escape[2] )
{
    static const char escapes[32] = ".......abtnvfr.............e....";
    char buffer[256];
    char *pos = buffer;
    int count = 0;

    for (len /= sizeof(WCHAR); len; str++, len--)
    {
        if (pos > buffer + sizeof(buffer) - 8)
        {
            fwrite( buffer, pos - buffer, 1, f );
            count += pos - buffer;
            pos = buffer;
        }
        if (*str > 127)  /* hex escape */
        {
            if (len > 1 && str[1] < 128 && isxdigit( (char)str[1] ))
                pos += sprintf( pos, "\\x%04x", *str );
            else
                pos += sprintf( pos, "\\x%x", *str );
            continue;
        }
        if (*str < 32)  /* octal or C escape */
        {
            if (!*str && len == 1) continue;  /* do not output terminating NULL */
            if (escapes[*str] != '.')
                pos += sprintf( pos, "\\%c", escapes[*str] );
            else if (len > 1 && str[1] >= '0' && str[1] <= '7')
                pos += sprintf( pos, "\\%03o", *str );
            else
                pos += sprintf( pos, "\\%o", *str );
            continue;
        }
        if (*str == '\\' || *str == escape[0] || *str == escape[1]) *pos++ = '\\';
        *pos++ = *str;
    }
    fwrite( buffer, pos - buffer, 1, f );
    count += pos - buffer;
    return count;
}

struct saved_key
{
    data_size_t namelen;
    WCHAR *name;
    data_size_t classlen;
    WCHAR *class;
    int value_count;
    int subkey_count;
    unsigned int is_symlink;
    timeout_t modif;
    struct saved_key *parent;
};

/* read serialized key data */
static char *fill_saved_key( struct saved_key *key, struct saved_key *parent, char *data )
{
    key->parent = parent;
    key->namelen = *(data_size_t *)data;
    data += sizeof(data_size_t);
    key->name = (WCHAR *)data;
    data += key->namelen;
    key->classlen = *(data_size_t *)data;
    data += sizeof(data_size_t);
    key->class = (WCHAR *)data;
    data += key->classlen;
    key->value_count = *(int *)data;
    data += sizeof(int);
    key->subkey_count = *(int *)data;
    data += sizeof(int);
    key->is_symlink = *(unsigned int *)data;
    data += sizeof(unsigned int);
    key->modif = *(timeout_t *)data;
    data += sizeof(timeout_t);

    return data;
}

/* dump serialized key full path */
static char *dump_parents( char *data, FILE *f, int count )
{
    data_size_t len;
    WCHAR *name;

    len = *(data_size_t *)data;
    data += sizeof(data_size_t);
    name = (WCHAR *)data;
    data += len;

    if (count > 1)
    {
        data = dump_parents( data, f, count - 1);
        fprintf( f, "\\\\" );
    }
    dump_strW( name, len, f, "[]" );
    return data;
}

/* dump the full path of a key */
static void dump_path( const struct saved_key *key, const struct saved_key *base, FILE *f )
{
    if (key->parent && key->parent != base)
    {
        dump_path( key->parent, base, f );
        fprintf( f, "\\\\" );
    }
    dump_strW( key->name, key->namelen, f, "[]" );
}

/* dump a value to a text file */
static char *dump_value( char *data, FILE *f )
{
    unsigned int i, dw;
    int count;
    data_size_t namelen, valuelen;
    char *valuedata;
    WCHAR *name;
    unsigned int type;

    namelen = *(data_size_t *)data;
    data += sizeof(data_size_t);
    name = (WCHAR *)data;
    data += namelen;
    type = *(unsigned int *)data;
    data += sizeof(unsigned int);
    valuelen = *(data_size_t *)data;
    data += sizeof(data_size_t);
    valuedata = data;
    data += valuelen;

    if (namelen)
    {
        fputc( '\"', f );
        count = 1 + dump_strW( name, namelen, f, "\"\"" );
        count += fprintf( f, "\"=" );
    }
    else count = fprintf( f, "@=" );

    switch(type)
    {
    case REG_SZ:
    case REG_EXPAND_SZ:
    case REG_MULTI_SZ:
        /* only output properly terminated strings in string format */
        if (valuelen < sizeof(WCHAR)) break;
        if (valuelen % sizeof(WCHAR)) break;
        if (((WCHAR *)valuedata)[valuelen / sizeof(WCHAR) - 1]) break;
        if (type != REG_SZ) fprintf( f, "str(%x):", type );
        fputc( '\"', f );
        dump_strW( (WCHAR *)valuedata, valuelen, f, "\"\"" );
        fprintf( f, "\"\n" );
        return data;

    case REG_DWORD:
        if (valuelen != sizeof(dw)) break;
        memcpy( &dw, valuedata, sizeof(dw) );
        fprintf( f, "dword:%08x\n", dw );
        return data;
    }

    if (type == REG_BINARY) count += fprintf( f, "hex:" );
    else count += fprintf( f, "hex(%x):", type );
    for (i = 0; i < valuelen; i++)
    {
        count += fprintf( f, "%02x", *((unsigned char *)valuedata + i) );
        if (i < valuelen-1)
        {
            fputc( ',', f );
            if (++count > 76)
            {
                fprintf( f, "\\\n  " );
                count = 2;
            }
        }
    }
    fputc( '\n', f );
    return data;
}

/* save a registry key and all its subkeys to a text file */
static char *save_subkeys( char *data, struct saved_key *parent, struct saved_key *base, FILE *f )
{
    struct saved_key key;
    int i;

    if (!base) base = &key;
    data = fill_saved_key( &key, parent, data );

    /* save key if it has either some values or no subkeys, or needs special options */
    /* keys with no values but subkeys are saved implicitly by saving the subkeys */
    if ((key.value_count > 0) || !key.subkey_count || key.classlen || key.is_symlink)
    {
        fprintf( f, "\n[" );
        if (parent) dump_path( &key, base, f );
        fprintf( f, "] %u\n", (unsigned int)((key.modif - SECS_1601_TO_1970 * TICKSPERSEC) / TICKSPERSEC) );
        fprintf( f, "#time=%x%08x\n", (unsigned int)(key.modif >> 32), (unsigned int)key.modif );
        if (key.classlen)
        {
            fprintf( f, "#class=\"" );
            dump_strW( key.class, key.classlen, f, "\"\"" );
            fprintf( f, "\"\n" );
        }
        if (key.is_symlink) fputs( "#link\n", f );
        for (i = 0; i < key.value_count; i++) data = dump_value( data, f );
    }
    for (i = 0; i < key.subkey_count; i++) data = save_subkeys( data, &key, base, f );
    return data;
}

/* save a registry branch to a file */
static char *save_all_subkeys( char *data, FILE *f )
{
    /* Output registry format should match server/registry.c:save_all_subkeys(). */
    enum prefix_type prefix_type;
    int parent_count;

    prefix_type = *(int *)data;
    data += sizeof(int);

    parent_count = *(int *)data;
    data += sizeof(int);

    fprintf( f, "WINE REGISTRY Version 2\n" );
    fprintf( f, ";; All keys relative to " );
    data = dump_parents( data, f, parent_count );
    fprintf( f, "\n" );

    switch (prefix_type)
    {
    case PREFIX_32BIT:
        fprintf( f, "\n#arch=win32\n" );
        break;
    case PREFIX_64BIT:
        fprintf( f, "\n#arch=win64\n" );
        break;
    default:
        break;
    }
    return save_subkeys( data, NULL, NULL, f );
}


/******************************************************************************
 *              NtCreateKey  (NTDLL.@)
 */
NTSTATUS WINAPI NtCreateKey( HANDLE *key, ACCESS_MASK access, const OBJECT_ATTRIBUTES *attr,
                             ULONG index, const UNICODE_STRING *class, ULONG options, ULONG *dispos )
{
    unsigned int ret;
    data_size_t len;
    struct object_attributes *objattr;

    *key = 0;
    if (attr->Length != sizeof(OBJECT_ATTRIBUTES)) return STATUS_INVALID_PARAMETER;
    if (!attr->ObjectName->Length && !attr->RootDirectory) return STATUS_OBJECT_PATH_SYNTAX_BAD;
    if ((ret = alloc_object_attributes( attr, &objattr, &len ))) return ret;
    objattr->attributes |= OBJ_OPENIF | OBJ_CASE_INSENSITIVE;

    TRACE( "(%p,%s,%s,%x,%x,%p)\n", attr->RootDirectory, debugstr_us(attr->ObjectName),
           debugstr_us(class), options, access, key );

    SERVER_START_REQ( create_key )
    {
        req->access     = access;
        req->options    = options;
        wine_server_add_data( req, objattr, len );
        if (class) wine_server_add_data( req, class->Buffer, class->Length );
        ret = wine_server_call( req );
        *key = wine_server_ptr_handle( reply->hkey );
    }
    SERVER_END_REQ;

    if (ret == STATUS_OBJECT_NAME_EXISTS)
    {
        if (dispos) *dispos = REG_OPENED_EXISTING_KEY;
        ret = STATUS_SUCCESS;
    }
    else if (ret == STATUS_SUCCESS)
    {
        if (dispos) *dispos = REG_CREATED_NEW_KEY;
    }

    TRACE( "<- %p\n", *key );
    free( objattr );
    return ret;
}


/******************************************************************************
 *              NtCreateKeyTransacted  (NTDLL.@)
 */
NTSTATUS WINAPI NtCreateKeyTransacted( HANDLE *key, ACCESS_MASK access, const OBJECT_ATTRIBUTES *attr,
                                       ULONG index, const UNICODE_STRING *class, ULONG options,
                                       HANDLE transacted, ULONG *dispos )
{
    FIXME( "(%p,%s,%s,%x,%x,%p,%p)\n", attr->RootDirectory, debugstr_us(attr->ObjectName),
           debugstr_us(class), options, access, transacted, key );
    return STATUS_NOT_IMPLEMENTED;
}


/******************************************************************************
 *              NtOpenKeyEx  (NTDLL.@)
 */
NTSTATUS WINAPI NtOpenKeyEx( HANDLE *key, ACCESS_MASK access, const OBJECT_ATTRIBUTES *attr, ULONG options )
{
    unsigned int ret;
    ULONG attributes;

    *key = 0;
    if (attr->Length != sizeof(*attr)) return STATUS_INVALID_PARAMETER;
    if (attr->ObjectName->Length & 1) return STATUS_OBJECT_NAME_INVALID;

    TRACE( "(%p,%s,%x,%p)\n", attr->RootDirectory, debugstr_us(attr->ObjectName), access, key );

    if (options & ~REG_OPTION_OPEN_LINK) FIXME( "options %x not implemented\n", options );

    attributes = attr->Attributes | OBJ_CASE_INSENSITIVE;

    SERVER_START_REQ( open_key )
    {
        req->parent     = wine_server_obj_handle( attr->RootDirectory );
        req->access     = access;
        req->attributes = attributes;
        wine_server_add_data( req, attr->ObjectName->Buffer, attr->ObjectName->Length );
        ret = wine_server_call( req );
        *key = wine_server_ptr_handle( reply->hkey );
    }
    SERVER_END_REQ;
    TRACE("<- %p\n", *key);
    return ret;
}


/******************************************************************************
 *              NtOpenKey  (NTDLL.@)
 */
NTSTATUS WINAPI NtOpenKey( HANDLE *key, ACCESS_MASK access, const OBJECT_ATTRIBUTES *attr )
{
    return NtOpenKeyEx( key, access, attr, 0 );
}


/******************************************************************************
 *              NtOpenKeyTransactedEx  (NTDLL.@)
 */
NTSTATUS WINAPI NtOpenKeyTransactedEx( HANDLE *key, ACCESS_MASK access, const OBJECT_ATTRIBUTES *attr,
                                       ULONG options, HANDLE transaction )
{
    FIXME( "(%p %x %p %x %p) semi-stub\n", key, access, attr, options, transaction );
    return NtOpenKeyEx( key, access, attr, options );
}


/******************************************************************************
 *              NtOpenKeyTransacted  (NTDLL.@)
 */
NTSTATUS WINAPI NtOpenKeyTransacted( HANDLE *key, ACCESS_MASK access, const OBJECT_ATTRIBUTES *attr,
                                     HANDLE transaction )
{
    return NtOpenKeyTransactedEx( key, access, attr, 0, transaction );
}


/******************************************************************************
 *              NtDeleteKey  (NTDLL.@)
 */
NTSTATUS WINAPI NtDeleteKey( HANDLE key )
{
    unsigned int ret;

    TRACE( "(%p)\n", key );

    SERVER_START_REQ( delete_key )
    {
        req->hkey = wine_server_obj_handle( key );
        ret = wine_server_call( req );
    }
    SERVER_END_REQ;
    return ret;
}


/******************************************************************************
 *              NtRenameKey  (NTDLL.@)
 */
NTSTATUS WINAPI NtRenameKey( HANDLE key, UNICODE_STRING *name )
{
    unsigned int ret;

    TRACE( "(%p %s)\n", key, debugstr_us(name) );

    if (!name) return STATUS_ACCESS_VIOLATION;
    if (!name->Buffer || !name->Length) return STATUS_INVALID_PARAMETER;

    SERVER_START_REQ( rename_key )
    {
        req->hkey = wine_server_obj_handle( key );
        wine_server_add_data( req, name->Buffer, name->Length );
        ret = wine_server_call( req );
    }
    SERVER_END_REQ;
    return ret;
}


/******************************************************************************
 *     enumerate_key
 *
 * Implementation of NtQueryKey and NtEnumerateKey
 */
static NTSTATUS enumerate_key( HANDLE handle, int index, KEY_INFORMATION_CLASS info_class,
                               void *info, DWORD length, DWORD *result_len )

{
    unsigned int ret;
    void *data_ptr;
    size_t fixed_size;

    switch (info_class)
    {
    case KeyBasicInformation:  data_ptr = ((KEY_BASIC_INFORMATION *)info)->Name; break;
    case KeyFullInformation:   data_ptr = ((KEY_FULL_INFORMATION *)info)->Class; break;
    case KeyNodeInformation:   data_ptr = ((KEY_NODE_INFORMATION *)info)->Name;  break;
    case KeyNameInformation:   data_ptr = ((KEY_NAME_INFORMATION *)info)->Name;  break;
    case KeyCachedInformation: data_ptr = ((KEY_CACHED_INFORMATION *)info)+1;    break;
    default:
        FIXME( "Information class %d not implemented\n", info_class );
        return STATUS_INVALID_PARAMETER;
    }
    fixed_size = (char *)data_ptr - (char *)info;

    SERVER_START_REQ( enum_key )
    {
        req->hkey       = wine_server_obj_handle( handle );
        req->index      = index;
        req->info_class = info_class;
        if (length > fixed_size) wine_server_set_reply( req, data_ptr, length - fixed_size );
        if (!(ret = wine_server_call( req )))
        {
            switch (info_class)
            {
            case KeyBasicInformation:
            {
                KEY_BASIC_INFORMATION keyinfo;
                keyinfo.LastWriteTime.QuadPart = reply->modif;
                keyinfo.TitleIndex = 0;
                keyinfo.NameLength = reply->namelen;
                memcpy( info, &keyinfo, min( length, fixed_size ) );
            break;
            }

            case KeyFullInformation:
            {
                KEY_FULL_INFORMATION keyinfo;
                keyinfo.LastWriteTime.QuadPart = reply->modif;
                keyinfo.TitleIndex = 0;
                keyinfo.ClassLength = wine_server_reply_size(reply);
                keyinfo.ClassOffset = keyinfo.ClassLength ? fixed_size : -1;
                keyinfo.SubKeys = reply->subkeys;
                keyinfo.MaxNameLen = reply->max_subkey;
                keyinfo.MaxClassLen = reply->max_class;
                keyinfo.Values = reply->values;
                keyinfo.MaxValueNameLen = reply->max_value;
                keyinfo.MaxValueDataLen = reply->max_data;
                memcpy( info, &keyinfo, min( length, fixed_size ) );
                break;
            }

            case KeyNodeInformation:
            {
                KEY_NODE_INFORMATION keyinfo;
                keyinfo.LastWriteTime.QuadPart = reply->modif;
                keyinfo.TitleIndex = 0;
                if (reply->namelen < wine_server_reply_size(reply))
                {
                    keyinfo.ClassLength = wine_server_reply_size(reply) - reply->namelen;
                    keyinfo.ClassOffset = fixed_size + reply->namelen;
                }
                else
                {
                    keyinfo.ClassLength = 0;
                    keyinfo.ClassOffset = -1;
                }
                keyinfo.NameLength = reply->namelen;
                memcpy( info, &keyinfo, min( length, fixed_size ) );
                break;
            }

            case KeyNameInformation:
            {
                KEY_NAME_INFORMATION keyinfo;
                keyinfo.NameLength = reply->namelen;
                memcpy( info, &keyinfo, min( length, fixed_size ) );
                break;
            }

            case KeyCachedInformation:
            {
                KEY_CACHED_INFORMATION keyinfo;
                keyinfo.LastWriteTime.QuadPart = reply->modif;
                keyinfo.TitleIndex = 0;
                keyinfo.SubKeys = reply->subkeys;
                keyinfo.MaxNameLen = reply->max_subkey;
                keyinfo.Values = reply->values;
                keyinfo.MaxValueNameLen = reply->max_value;
                keyinfo.MaxValueDataLen = reply->max_data;
                keyinfo.NameLength = reply->namelen;
                memcpy( info, &keyinfo, min( length, fixed_size ) );
                break;
            }

            default:
                break;
            }
            *result_len = fixed_size + reply->total;
            if (length < fixed_size) ret = STATUS_BUFFER_TOO_SMALL;
            else if (length < *result_len) ret = STATUS_BUFFER_OVERFLOW;
        }
    }
    SERVER_END_REQ;
    return ret;
}


/******************************************************************************
 *              NtEnumerateKey  (NTDLL.@)
 */
NTSTATUS WINAPI NtEnumerateKey( HANDLE handle, ULONG index, KEY_INFORMATION_CLASS info_class,
                                void *info, DWORD length, DWORD *result_len )
{
    /* -1 means query key, so avoid it here */
    if (index == (ULONG)-1) return STATUS_NO_MORE_ENTRIES;
    return enumerate_key( handle, index, info_class, info, length, result_len );
}


/******************************************************************************
 *              NtQueryKey  (NTDLL.@)
 */
NTSTATUS WINAPI NtQueryKey( HANDLE handle, KEY_INFORMATION_CLASS info_class,
                            void *info, DWORD length, DWORD *result_len )
{
    return enumerate_key( handle, -1, info_class, info, length, result_len );
}


/******************************************************************************
 *              NtSetInformationKey  (NTDLL.@)
 */
NTSTATUS WINAPI NtSetInformationKey( HANDLE key, int class, void *info, ULONG length )
{
    FIXME( "(%p,0x%08x,%p,0x%08x) stub\n", key, class, info, length );
    return STATUS_SUCCESS;
}


/* fill the key value info structure for a specific info class */
static void copy_key_value_info( KEY_VALUE_INFORMATION_CLASS info_class, void *info,
                                 DWORD length, int type, int name_len, int data_len )
{
    switch (info_class)
    {
    case KeyValueBasicInformation:
    {
        KEY_VALUE_BASIC_INFORMATION keyinfo;
        keyinfo.TitleIndex = 0;
        keyinfo.Type       = type;
        keyinfo.NameLength = name_len;
        length = min( length, (char *)keyinfo.Name - (char *)&keyinfo );
        memcpy( info, &keyinfo, length );
        break;
    }

    case KeyValueFullInformation:
    {
        KEY_VALUE_FULL_INFORMATION keyinfo;
        keyinfo.TitleIndex = 0;
        keyinfo.Type       = type;
        keyinfo.DataOffset = (char *)keyinfo.Name - (char *)&keyinfo + name_len;
        keyinfo.DataLength = data_len;
        keyinfo.NameLength = name_len;
        length = min( length, (char *)keyinfo.Name - (char *)&keyinfo );
        memcpy( info, &keyinfo, length );
        break;
    }

    case KeyValuePartialInformation:
    {
        KEY_VALUE_PARTIAL_INFORMATION keyinfo;
        keyinfo.TitleIndex = 0;
        keyinfo.Type       = type;
        keyinfo.DataLength = data_len;
        length = min( length, (char *)keyinfo.Data - (char *)&keyinfo );
        memcpy( info, &keyinfo, length );
        break;
    }

    case KeyValuePartialInformationAlign64:
    {
        KEY_VALUE_PARTIAL_INFORMATION_ALIGN64 keyinfo;
        keyinfo.Type       = type;
        keyinfo.DataLength = data_len;
        length = min( length, (char *)keyinfo.Data - (char *)&keyinfo );
        memcpy( info, &keyinfo, length );
        break;
    }

    default:
        break;
    }
}


/******************************************************************************
 *              NtEnumerateValueKey  (NTDLL.@)
 */
NTSTATUS WINAPI NtEnumerateValueKey( HANDLE handle, ULONG index, KEY_VALUE_INFORMATION_CLASS info_class,
                                     void *info, DWORD length, DWORD *result_len )
{
    unsigned int ret;
    void *ptr;
    size_t fixed_size;

    TRACE( "(%p,%u,%d,%p,%d)\n", handle, index, info_class, info, length );

    /* compute the length we want to retrieve */
    switch (info_class)
    {
    case KeyValueBasicInformation:   ptr = ((KEY_VALUE_BASIC_INFORMATION *)info)->Name; break;
    case KeyValueFullInformation:    ptr = ((KEY_VALUE_FULL_INFORMATION *)info)->Name; break;
    case KeyValuePartialInformation: ptr = ((KEY_VALUE_PARTIAL_INFORMATION *)info)->Data; break;
    default:
        FIXME( "Information class %d not implemented\n", info_class );
        return STATUS_INVALID_PARAMETER;
    }
    fixed_size = (char *)ptr - (char *)info;

    SERVER_START_REQ( enum_key_value )
    {
        req->hkey       = wine_server_obj_handle( handle );
        req->index      = index;
        req->info_class = info_class;
        if (length > fixed_size) wine_server_set_reply( req, ptr, length - fixed_size );
        if (!(ret = wine_server_call( req )))
        {
            copy_key_value_info( info_class, info, length, reply->type, reply->namelen,
                                 wine_server_reply_size(reply) - reply->namelen );
            *result_len = fixed_size + reply->total;
            if (length < *result_len) ret = STATUS_BUFFER_OVERFLOW;
        }
    }
    SERVER_END_REQ;
    return ret;
}


/******************************************************************************
 *              NtQueryValueKey  (NTDLL.@)
 */
NTSTATUS WINAPI NtQueryValueKey( HANDLE handle, const UNICODE_STRING *name,
                                 KEY_VALUE_INFORMATION_CLASS info_class,
                                 void *info, DWORD length, DWORD *result_len )
{
    unsigned int ret;
    UCHAR *data_ptr;
    unsigned int fixed_size, min_size;

    TRACE( "(%p,%s,%d,%p,%d)\n", handle, debugstr_us(name), info_class, info, length );

    if (name->Length > MAX_VALUE_LENGTH) return STATUS_OBJECT_NAME_NOT_FOUND;

    /* compute the length we want to retrieve */
    switch(info_class)
    {
    case KeyValueBasicInformation:
    {
        KEY_VALUE_BASIC_INFORMATION *basic_info = info;
        min_size = FIELD_OFFSET(KEY_VALUE_BASIC_INFORMATION, Name);
        fixed_size = min_size + name->Length;
        if (min_size < length)
            memcpy(basic_info->Name, name->Buffer, min(length - min_size, name->Length));
        data_ptr = NULL;
        break;
    }

    case KeyValueFullInformation:
    {
        KEY_VALUE_FULL_INFORMATION *full_info = info;
        min_size = FIELD_OFFSET(KEY_VALUE_FULL_INFORMATION, Name);
        fixed_size = min_size + name->Length;
        if (min_size < length)
            memcpy(full_info->Name, name->Buffer, min(length - min_size, name->Length));
        data_ptr = (UCHAR *)full_info->Name + name->Length;
        break;
    }

    case KeyValuePartialInformation:
        min_size = fixed_size = FIELD_OFFSET(KEY_VALUE_PARTIAL_INFORMATION, Data);
        data_ptr = ((KEY_VALUE_PARTIAL_INFORMATION *)info)->Data;
        break;

    case KeyValuePartialInformationAlign64:
        min_size = fixed_size = FIELD_OFFSET(KEY_VALUE_PARTIAL_INFORMATION_ALIGN64, Data);
        data_ptr = ((KEY_VALUE_PARTIAL_INFORMATION_ALIGN64 *)info)->Data;
        break;

    default:
        FIXME( "Information class %d not implemented\n", info_class );
        return STATUS_INVALID_PARAMETER;
    }

    SERVER_START_REQ( get_key_value )
    {
        req->hkey = wine_server_obj_handle( handle );
        wine_server_add_data( req, name->Buffer, name->Length );
        if (length > fixed_size && data_ptr) wine_server_set_reply( req, data_ptr, length - fixed_size );
        if (!(ret = wine_server_call( req )))
        {
            copy_key_value_info( info_class, info, length, reply->type,
                                 name->Length, reply->total );
            *result_len = fixed_size + (info_class == KeyValueBasicInformation ? 0 : reply->total);
            if (length < min_size) ret = STATUS_BUFFER_TOO_SMALL;
            else if (length < *result_len) ret = STATUS_BUFFER_OVERFLOW;
        }
    }
    SERVER_END_REQ;
    return ret;
}


/******************************************************************************
 *              NtQueryMultipleValueKey  (NTDLL.@)
 */
NTSTATUS WINAPI NtQueryMultipleValueKey( HANDLE key, KEY_MULTIPLE_VALUE_INFORMATION *info,
                                         ULONG count, void *buffer, ULONG length, ULONG *retlen )
{
    FIXME( "(%p,%p,0x%08x,%p,0x%08x,%p) stub!\n", key, info, count, buffer, length, retlen );
    return STATUS_SUCCESS;
}


/******************************************************************************
 *              NtSetValueKey  (NTDLL.@)
 */
NTSTATUS WINAPI NtSetValueKey( HANDLE key, const UNICODE_STRING *name, ULONG index,
                               ULONG type, const void *data, ULONG count )
{
    unsigned int ret;

    TRACE( "(%p,%s,%d,%p,%d)\n", key, debugstr_us(name), type, data, count );

    if (name->Length > MAX_VALUE_LENGTH) return STATUS_INVALID_PARAMETER;

    SERVER_START_REQ( set_key_value )
    {
        req->hkey    = wine_server_obj_handle( key );
        req->type    = type;
        req->namelen = name->Length;
        wine_server_add_data( req, name->Buffer, name->Length );
        wine_server_add_data( req, data, count );
        ret = wine_server_call( req );
    }
    SERVER_END_REQ;
    return ret;
}


/******************************************************************************
 *              NtDeleteValueKey  (NTDLL.@)
 */
NTSTATUS WINAPI NtDeleteValueKey( HANDLE key, const UNICODE_STRING *name )
{
    unsigned int ret;

    TRACE( "(%p,%s)\n", key, debugstr_us(name) );

    if (name->Length > MAX_VALUE_LENGTH) return STATUS_OBJECT_NAME_NOT_FOUND;

    SERVER_START_REQ( delete_key_value )
    {
        req->hkey = wine_server_obj_handle( key );
        wine_server_add_data( req, name->Buffer, name->Length );
        ret = wine_server_call( req );
    }
    SERVER_END_REQ;
    return ret;
}


/******************************************************************************
 *              NtNotifyChangeMultipleKeys  (NTDLL.@)
 */
NTSTATUS WINAPI NtNotifyChangeMultipleKeys( HANDLE key, ULONG count, OBJECT_ATTRIBUTES *attr,
                                            HANDLE event, PIO_APC_ROUTINE apc, void *apc_context,
                                            IO_STATUS_BLOCK *io, ULONG filter, BOOLEAN subtree,
                                            void *buffer, ULONG length, BOOLEAN async )
{
    unsigned int ret;

    TRACE( "(%p,%u,%p,%p,%p,%p,%p,0x%08x, 0x%08x,%p,0x%08x,0x%08x)\n",
           key, count, attr, event, apc, apc_context, io,
           filter, async, buffer, length, subtree );

    if (count || attr || apc || apc_context || buffer || length)
        FIXME( "Unimplemented optional parameter\n" );

    if (!async)
    {
        OBJECT_ATTRIBUTES attr;
        InitializeObjectAttributes( &attr, NULL, 0, NULL, NULL );
        ret = NtCreateEvent( &event, EVENT_ALL_ACCESS, &attr, SynchronizationEvent, FALSE );
        if (ret) return ret;
    }

    SERVER_START_REQ( set_registry_notification )
    {
        req->hkey    = wine_server_obj_handle( key );
        req->event   = wine_server_obj_handle( event );
        req->subtree = subtree;
        req->filter  = filter;
        ret = wine_server_call( req );
    }
    SERVER_END_REQ;

    if (!async)
    {
        if (ret == STATUS_PENDING) ret = NtWaitForSingleObject( event, FALSE, NULL );
        NtClose( event );
    }
    return ret;
}


/******************************************************************************
 *              NtNotifyChangeKey  (NTDLL.@)
 */
NTSTATUS WINAPI NtNotifyChangeKey( HANDLE key, HANDLE event, PIO_APC_ROUTINE apc, void *apc_context,
                                   IO_STATUS_BLOCK *io, ULONG filter, BOOLEAN subtree,
                                   void *buffer, ULONG length, BOOLEAN async )
{
    return NtNotifyChangeMultipleKeys( key, 0, NULL, event, apc, apc_context,
                                       io, filter, subtree, buffer, length, async );
}

/* acquire mutex for registry flush operation */
static HANDLE get_key_flush_mutex(void)
{
    WCHAR bufferW[256];
    UNICODE_STRING name = {.Buffer = bufferW};
    OBJECT_ATTRIBUTES attr;
    char buffer[256];
    HANDLE mutex;

    snprintf( buffer, ARRAY_SIZE(buffer), "\\Sessions\\%u\\BaseNamedObjects\\__wine_regkey_flush",
              (int)NtCurrentTeb()->Peb->SessionId );
    name.Length = name.MaximumLength = (strlen(buffer) + 1) * sizeof(WCHAR);
    ascii_to_unicode( bufferW, buffer, name.Length / sizeof(WCHAR) );

    InitializeObjectAttributes( &attr, &name, OBJ_OPENIF, NULL, NULL );
    if (NtCreateMutant( &mutex, MUTEX_ALL_ACCESS, &attr, FALSE ) < 0) return NULL;
    NtWaitForSingleObject( mutex, FALSE, NULL );
    return mutex;
}

/* release registry flush mutex */
static void release_key_flush_mutex( HANDLE mutex )
{
    NtReleaseMutant( mutex, NULL );
    NtClose( mutex );
}

/* save registry branch to Wine regsitry storage file */
static NTSTATUS save_registry_branch( char **data )
{
    static const char temp_fn[] = "savereg.tmp";
    char *file_name, *path = NULL, *tmp = NULL;
    int file_name_len, path_len, fd;
    struct stat st;
    NTSTATUS ret;
    FILE *f;

    file_name_len = *(int *)*data;
    *data += sizeof(int);
    file_name = *data;
    *data += file_name_len;

    path_len = strlen( config_dir ) + 1 + file_name_len + 1;
    if (!(path = malloc( path_len ))) return STATUS_NO_MEMORY;
    sprintf( path, "%s/%s", config_dir, file_name );

    if ((fd = open( path, O_WRONLY )) != -1)
    {
        /* if file is not a regular file or has multiple links or is accessed
         * via symbolic links, write directly into it; otherwise use a temp file */
        if (!lstat( path, &st ) && (!S_ISREG(st.st_mode) || st.st_nlink > 1))
        {
            ftruncate( fd, 0 );
            goto save;
        }
        close( fd );
    }

    /* create a temp file in the same directory */
    if (!(tmp = malloc( strlen( config_dir ) + 1 + strlen( temp_fn ) + 1 )))
    {
        ret = STATUS_NO_MEMORY;
        goto done;
    }
    sprintf( tmp, "%s/%s", config_dir, temp_fn );

    if ((fd = open( tmp, O_CREAT | O_EXCL | O_WRONLY, 0666 )) == -1)
    {
        ret = errno_to_status( errno );
        goto done;
    }

save:
    if (!(f = fdopen( fd, "w" )))
    {
        ret = errno_to_status( errno );
        if (tmp) unlink( tmp );
        close( fd );
        goto done;
    }

    *data = save_all_subkeys( *data, f );

    ret = fclose( f ) ? errno_to_status( errno ) : STATUS_SUCCESS;
    if (tmp)
    {
        if (!ret && rename( tmp, path )) ret = errno_to_status( errno );
        if (ret) unlink( tmp );
    }

done:
    free( tmp );
    free( path );
    return ret;
}

/******************************************************************************
 *              NtFlushKey  (NTDLL.@)
 */
NTSTATUS WINAPI NtFlushKey( HANDLE key )
{
    abstime_t timestamp_counter;
    data_size_t size = 0;
    unsigned int ret;
    char *data = NULL, *curr_data;
    HANDLE mutex;
    int i, branch_count, branch;

    TRACE( "key=%p\n", key );

    mutex = get_key_flush_mutex();

    while (1)
    {
        SERVER_START_REQ( flush_key )
        {
            req->hkey = wine_server_obj_handle( key );
            if (size) wine_server_set_reply( req, data, size );
            ret = wine_server_call( req );
            size = reply->total;
            branch_count = reply->branch_count;
            timestamp_counter = reply->timestamp_counter;
        }
        SERVER_END_REQ;

        if (ret != STATUS_BUFFER_TOO_SMALL) break;
        free( data );
        if (!(data = malloc( size )))
        {
            ERR( "No memory.\n" );
            ret = STATUS_NO_MEMORY;
            goto done;
        }
    }
    if (ret) goto done;

    curr_data = data;
    for (i = 0; i < branch_count; ++i)
    {
        branch = *(int *)curr_data;
        curr_data += sizeof(int);
        if ((ret = save_registry_branch( &curr_data ))) goto done;

        SERVER_START_REQ( flush_key_done )
        {
            req->branch = branch;
            req->timestamp_counter = timestamp_counter;
            ret = wine_server_call( req );
        }
        SERVER_END_REQ;
        if (ret) break;
    }

done:
    release_key_flush_mutex( mutex );
    free( data );
    return ret;
}


/******************************************************************************
 *              NtLoadKey  (NTDLL.@)
 */
NTSTATUS WINAPI NtLoadKey( const OBJECT_ATTRIBUTES *attr, OBJECT_ATTRIBUTES *file )
{
    TRACE( "(%p,%p)\n", attr, file );
    return NtLoadKeyEx( attr, file, 0, 0, 0, 0, NULL, NULL );
}

/******************************************************************************
 *              NtLoadKey2  (NTDLL.@)
 */
NTSTATUS WINAPI NtLoadKey2( const OBJECT_ATTRIBUTES *attr, OBJECT_ATTRIBUTES *file, ULONG flags )
{
    return NtLoadKeyEx( attr, file, flags, 0, 0, 0, NULL, NULL );
}

/******************************************************************************
 *              NtLoadKeyEx  (NTDLL.@)
 */
NTSTATUS WINAPI NtLoadKeyEx( const OBJECT_ATTRIBUTES *attr, OBJECT_ATTRIBUTES *file, ULONG flags, HANDLE trustkey,
                             HANDLE event, ACCESS_MASK access, HANDLE *roothandle, IO_STATUS_BLOCK *iostatus )
{
    unsigned int ret;
    HANDLE key;
    data_size_t len;
    struct object_attributes *objattr;
    char *unix_name;
    UNICODE_STRING nt_name;
    OBJECT_ATTRIBUTES new_attr = *file;

    TRACE( "(%p,%p,0x%x,%p,%p,0x%x,%p,%p)\n",
           attr, file, flags, trustkey, event, access, roothandle, iostatus );

    if (flags) FIXME( "flags %x not handled\n", flags );
    if (trustkey) FIXME("trustkey parameter not supported\n");
    if (event) FIXME("event parameter not supported\n");
    if (access) FIXME("access parameter not supported\n");
    if (roothandle) FIXME("roothandle is not filled\n");
    if (iostatus) FIXME("iostatus is not filled\n");

    if (!(ret = get_nt_and_unix_names( &new_attr, &nt_name, &unix_name, FILE_OPEN, FALSE )))
    {
        ret = open_unix_file( &key, unix_name, GENERIC_READ | SYNCHRONIZE,
                              &new_attr, 0, 0, FILE_OPEN, 0, NULL, 0 );
    }
    free( unix_name );
    free( nt_name.Buffer );

    if (ret) return ret;

    if ((ret = alloc_object_attributes( attr, &objattr, &len ))) return ret;
    objattr->attributes |= OBJ_OPENIF | OBJ_CASE_INSENSITIVE;

    SERVER_START_REQ( load_registry )
    {
        req->file = wine_server_obj_handle( key );
        wine_server_add_data( req, objattr, len );
        ret = wine_server_call( req );
        if (ret == STATUS_OBJECT_NAME_EXISTS) ret = STATUS_SUCCESS;
    }
    SERVER_END_REQ;

    NtClose( key );
    free( objattr );
    return ret;
}

/******************************************************************************
 *              NtUnloadKey  (NTDLL.@)
 */
NTSTATUS WINAPI NtUnloadKey( OBJECT_ATTRIBUTES *attr )
{
    unsigned int ret;

    TRACE( "(%p)\n", attr );

    if (!attr || !attr->ObjectName) return STATUS_ACCESS_VIOLATION;
    if (attr->Length != sizeof(*attr)) return STATUS_INVALID_PARAMETER;
    if (attr->ObjectName->Length & 1) return STATUS_OBJECT_NAME_INVALID;

    SERVER_START_REQ( unload_registry )
    {
        req->parent     = wine_server_obj_handle( attr->RootDirectory );
        req->attributes = attr->Attributes;
        wine_server_add_data( req, attr->ObjectName->Buffer, attr->ObjectName->Length );
        ret = wine_server_call(req);
    }
    SERVER_END_REQ;
    return ret;
}


/******************************************************************************
 *              NtSaveKey  (NTDLL.@)
 */
NTSTATUS WINAPI NtSaveKey( HANDLE key, HANDLE file )
{
    data_size_t size = 0;
    unsigned int ret;
    char *data = NULL;
    int fd, fd2, needs_close = 0;
    FILE *f;

    TRACE( "(%p,%p)\n", key, file );

    while (1)
    {
        SERVER_START_REQ( save_registry )
        {
            req->hkey = wine_server_obj_handle( key );
            if (size) wine_server_set_reply( req, data, size );
            ret = wine_server_call( req );
            size = reply->total;
        }
        SERVER_END_REQ;

        if (!ret) break;
        free( data );
        if (ret != STATUS_BUFFER_TOO_SMALL) return ret;
        if (!(data = malloc( size )))
        {
            ERR( "No memory.\n" );
            return STATUS_NO_MEMORY;
        }
    }

    if ((ret = server_get_unix_fd( file, FILE_WRITE_DATA, &fd, &needs_close, NULL, NULL ))) goto done;
    if ((fd2 = dup( fd )) == -1)
    {
        ret = errno_to_status( errno );
        goto done;
    }
    if (!(f = fdopen( fd2, "w" )))
    {
        close( fd2 );
        ret = errno_to_status( errno );
        goto done;
    }
    save_all_subkeys( data, f );
    if (fclose(f)) ret = errno_to_status( errno );

done:
    if (needs_close) close( fd );
    free( data );
    return ret;
}


/******************************************************************************
 *              NtRestoreKey  (NTDLL.@)
 */
NTSTATUS WINAPI NtRestoreKey( HANDLE key, HANDLE file, ULONG flags )
{
    FIXME( "(%p,%p,0x%08x) stub\n", key, file, flags );
    return STATUS_SUCCESS;
}


/******************************************************************************
 *              NtReplaceKey  (NTDLL.@)
 */
NTSTATUS WINAPI NtReplaceKey( OBJECT_ATTRIBUTES *attr, HANDLE key, OBJECT_ATTRIBUTES *replace )
{
    FIXME( "(%s,%p,%s),stub!\n", debugstr_us(attr->ObjectName), key, debugstr_us(replace->ObjectName) );
    return STATUS_SUCCESS;
}

/******************************************************************************
 *              NtQueryLicenseValue  (NTDLL.@)
 *
 * NOTES
 *  On Windows all license properties are stored in a single key, but
 *  unless there is some app which explicitly depends on that, there is
 *  no good reason to reproduce that.
 */
NTSTATUS WINAPI NtQueryLicenseValue( const UNICODE_STRING *name, ULONG *type,
                                     void *data, ULONG length, ULONG *retlen )
{
    static const WCHAR nameW[] = {'\\','R','e','g','i','s','t','r','y','\\',
                                  'M','a','c','h','i','n','e','\\',
                                  'S','o','f','t','w','a','r','e','\\',
                                  'W','i','n','e','\\','L','i','c','e','n','s','e',
                                  'I','n','f','o','r','m','a','t','i','o','n',0};
    UNICODE_STRING keyW = RTL_CONSTANT_STRING( nameW );
    KEY_VALUE_PARTIAL_INFORMATION *info;
    NTSTATUS status = STATUS_OBJECT_NAME_NOT_FOUND;
    DWORD info_length, count;
    OBJECT_ATTRIBUTES attr;
    HANDLE key;

    if (!name || !name->Buffer || !name->Length || !retlen) return STATUS_INVALID_PARAMETER;

    info_length = FIELD_OFFSET( KEY_VALUE_PARTIAL_INFORMATION, Data ) + length;
    if (!(info = malloc( info_length ))) return STATUS_NO_MEMORY;

    InitializeObjectAttributes( &attr, &keyW, 0, 0, NULL );

    /* @@ Wine registry key: HKLM\Software\Wine\LicenseInformation */
    if (!NtOpenKey( &key, KEY_READ, &attr ))
    {
        status = NtQueryValueKey( key, name, KeyValuePartialInformation, info, info_length, &count );
        if (!status || status == STATUS_BUFFER_OVERFLOW)
        {
            if (type) *type = info->Type;
            *retlen = info->DataLength;
            if (status == STATUS_BUFFER_OVERFLOW)
                status = STATUS_BUFFER_TOO_SMALL;
            else
                memcpy( data, info->Data, info->DataLength );
        }
        NtClose( key );
    }

    if (status == STATUS_OBJECT_NAME_NOT_FOUND)
        FIXME( "License key %s not found\n", debugstr_w(name->Buffer) );

    free( info );
    return status;
}
