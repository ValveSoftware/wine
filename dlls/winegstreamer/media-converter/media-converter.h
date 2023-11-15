/*
 * Copyright 2024 Ziqing Hui for CodeWeavers
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

#ifndef __MEDIA_CONVERTER_H__
#define __MEDIA_CONVERTER_H__

#define _FILE_OFFSET_BITS 64

#include <string.h>
#include <utime.h>
#include <stdarg.h>
#include <inttypes.h>

#include "unix_private.h"

GST_DEBUG_CATEGORY_EXTERN(media_converter_debug);
#undef GST_CAT_DEFAULT
#define GST_CAT_DEFAULT media_converter_debug

typedef int (*data_read_callback)(void *data_reader, uint8_t *buffer, size_t size, size_t *read_size);

/* Changing this will invalidate the cache. You MUST clear it. */
#define HASH_SEED 0x4AA61F63

enum conv_ret
{
    CONV_OK = 0,
    CONV_ERROR = -1,
    CONV_ERROR_NOT_IMPLEMENTED = -2,
    CONV_ERROR_INVALID_ARGUMENT = -3,
    CONV_ERROR_OPEN_FAILED = -4,
    CONV_ERROR_READ_FAILED = -5,
    CONV_ERROR_WRITE_FAILED = -6,
    CONV_ERROR_SEEK_FAILED = -7,
    CONV_ERROR_CORRUPT_DATABASE = -8,
    CONV_ERROR_WRONG_CHECKSUM = -9,
    CONV_ERROR_ENTRY_NOT_FOUND = -10,
    CONV_ERROR_ENV_NOT_SET = -11,
    CONV_ERROR_PATH_NOT_FOUND = -12,
    CONV_ERROR_INVALID_TAG = -13,
    CONV_ERROR_DATA_END = -14,
};

struct murmur3_x64_128_state
{
    uint32_t seed;
    uint64_t h1;
    uint64_t h2;
    size_t processed;
};

struct murmur3_x86_128_state
{
    uint32_t seed;
    uint32_t h1;
    uint32_t h2;
    uint32_t h3;
    uint32_t h4;
    size_t processed;
};

struct bytes_reader
{
    const uint8_t *data;
    size_t size;
    size_t offset;
};

struct gst_buffer_reader
{
    GstBuffer *buffer; /* No ref here, no need to unref. */
    size_t offset;
};

struct payload_hash
{
    uint32_t hash[4];
};

struct entry_name
{
    uint32_t tag;
    struct payload_hash hash;
};

struct dump_fozdb
{
    pthread_mutex_t mutex;
    struct fozdb *fozdb;
    bool already_cleaned;
};

struct fozdb
{
    const char *file_name;
    int file;
    bool read_only;
    uint64_t write_pos;
    GHashTable **seen_blobs;
    uint32_t num_tags;
};

/* lib.c. */
extern bool open_file(const char *file_name, int open_flags, int *out_fd);
extern bool get_file_size(int fd, uint64_t *file_size);
extern bool complete_read(int file, void *buffer, size_t size);
extern bool complete_write(int file, const void *buffer, size_t size);
extern uint32_t crc32(uint32_t crc, const uint8_t *ptr, size_t buf_len);
extern int create_placeholder_file(const char *file_name);
extern int dump_fozdb_open(struct dump_fozdb *db, bool create, const char *file_path_env, int num_tags);
extern void dump_fozdb_close(struct dump_fozdb *db);

/* murmur3.c. */
extern void murmur3_x64_128_state_init(struct murmur3_x64_128_state *state, uint32_t seed);
extern void murmur3_x64_128_state_reset(struct murmur3_x64_128_state *state);
extern bool murmur3_x64_128_full(void *data_src, data_read_callback read_callback,
        struct murmur3_x64_128_state* state, void *out);
extern bool murmur3_x64_128(void *data_src, data_read_callback read_callback, uint32_t seed, void *out);
extern void murmur3_x86_128_state_init(struct murmur3_x86_128_state *state, uint32_t seed);
extern void murmur3_x86_128_state_reset(struct murmur3_x86_128_state *state);
extern bool murmur3_x86_128_full(void *data_src, data_read_callback read_callback,
        struct murmur3_x86_128_state* state, void *out);
extern bool murmur3_x86_128(void *data_src, data_read_callback read_callback, uint32_t seed, void *out);
#ifdef __x86_64__
#define murmur3_128_state       murmur3_x64_128_state
#define murmur3_128_state_init  murmur3_x64_128_state_init
#define murmur3_128_state_reset murmur3_x64_128_state_reset
#define murmur3_128_full        murmur3_x64_128_full
#define murmur3_128             murmur3_x64_128
#elif defined(__i386__)
#define murmur3_128_state       murmur3_x86_128_state
#define murmur3_128_state_init  murmur3_x86_128_state_init
#define murmur3_128_state_reset murmur3_x86_128_state_reset
#define murmur3_128_full        murmur3_x86_128_full
#define murmur3_128             murmur3_x86_128
#endif /* __x86_64__ */

/* fossilize.c. */
extern int fozdb_create(const char *file_name, int open_flags, bool read_only, uint32_t num_tags, struct fozdb **out);
extern void fozdb_release(struct fozdb *db);
extern int fozdb_prepare(struct fozdb *db);
extern bool fozdb_has_entry(struct fozdb *db, uint32_t tag, struct payload_hash *hash);
extern int fozdb_entry_size(struct fozdb *db, uint32_t tag, struct payload_hash *hash, uint32_t *size);
extern void fozdb_iter_tag(struct fozdb *db, uint32_t tag, GHashTableIter *iter);
extern int fozdb_read_entry_data(struct fozdb *db, uint32_t tag, struct payload_hash *hash,
        uint64_t offset, uint8_t *buffer, size_t size, size_t *read_size, bool with_crc);
extern int fozdb_write_entry(struct fozdb *db, uint32_t tag, struct payload_hash *hash,
        void *data_src, data_read_callback read_callback, bool with_crc);
extern int fozdb_discard_entries(struct fozdb *db, GList *to_discard_entries);

static inline bool option_enabled(const char *env)
{
    const char *env_var;

    if (!(env_var = getenv(env)))
        return false;

    return strcmp(env_var, "0") != 0;
}

static inline bool discarding_disabled(void)
{
    return option_enabled("MEDIACONV_DONT_DISCARD");
}

static inline const char *format_hash(struct payload_hash *hash)
{
    int hash_str_size = 2 + sizeof(*hash) * 2 + 1;
    static char buffer[1024] = {};
    static int offset = 0;
    char *ret;

    if (offset + hash_str_size > sizeof(buffer))
        offset = 0;

    ret = buffer + offset;
    sprintf(ret, "0x%08x%08x%08x%08x", hash->hash[3], hash->hash[2], hash->hash[1], hash->hash[0]);
    offset += hash_str_size;

    return ret;
}

static inline void bytes_reader_init(struct bytes_reader *reader, const uint8_t *data, size_t size)
{
    reader->data = data;
    reader->size = size;
    reader->offset = 0;
}

static inline int bytes_reader_read(void *data_reader, uint8_t *buffer, size_t size, size_t *read_size)
{
    struct bytes_reader *reader = data_reader;
    size_t data_size, to_copy;

    if (!size)
    {
        *read_size = 0;
        return CONV_OK;
    }

    if (!(data_size = reader->size - reader->offset))
        return CONV_ERROR_DATA_END;

    to_copy = min(data_size, size);
    memcpy(buffer, reader->data + reader->offset, to_copy);
    reader->offset += to_copy;

    *read_size = to_copy;
    return CONV_OK;
}

static inline void gst_buffer_reader_init(struct gst_buffer_reader *reader, GstBuffer *buffer)
{
    reader->buffer = buffer; /* No ref here, so no need to unref. */
    reader->offset = 0;
}

static inline int gst_buffer_reader_read(void *data_reader, uint8_t *buffer, size_t size, size_t *read_size)
{
    struct gst_buffer_reader *reader = data_reader;

    if (!size)
    {
        *read_size = 0;
        return CONV_OK;
    }

    *read_size = gst_buffer_extract(reader->buffer, reader->offset, buffer, size);
    reader->offset += *read_size;
    if (!*read_size)
        return CONV_ERROR_DATA_END;

    return CONV_OK;
}

static inline bool file_exists(const char *file_path)
{
    if (!file_path)
        return false;
    return access(file_path, F_OK) == 0;
}

static inline struct entry_name *entry_name_create(uint32_t tag, struct payload_hash *hash)
{
    struct entry_name *entry = calloc(1, sizeof(*entry));
    entry->tag = tag;
    entry->hash = *hash;
    return entry;
}

static inline gint entry_name_compare(const void *a, const void *b)
{
    return memcmp(a, b, sizeof(struct entry_name));
}

static inline uint32_t bytes_to_uint32(const uint8_t *bytes)
{
    return ((uint32_t)bytes[0] << 0)
            | ((uint32_t)bytes[1] << 8)
            | ((uint32_t)bytes[2] << 16)
            | ((uint32_t)bytes[3] << 24);
}

static inline void payload_hash_from_bytes(struct payload_hash *hash, uint8_t *bytes)
{
    hash->hash[0] = bytes_to_uint32(bytes + 0);
    hash->hash[1] = bytes_to_uint32(bytes + 4);
    hash->hash[2] = bytes_to_uint32(bytes + 8);
    hash->hash[3] = bytes_to_uint32(bytes + 12);
}

#endif /* __MEDIA_CONVERTER_H__ */
