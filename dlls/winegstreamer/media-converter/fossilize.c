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
 *
 * Based on "Fossilize," which is
 * Copyright (c) 2018-2019 Hans-Kristian Arntzen
 * https://github.com/ValveSoftware/Fossilize/
 */

/* This is a read/write implementation of the Fossilize database format.
 *
 * https://github.com/ValveSoftware/Fossilize/
 *
 * That C++ implementation is specific to Vulkan, while this one tries to be generic to store any
 * type of data.
 */

#if 0
#pragma makedep unix
#endif

#include "media-converter.h"

/* Fossilize StreamArchive database format version 6:
 *
 * The file consists of a header, followed by an unlimited series of "entries".
 *
 * All multi-byte entities are little-endian.
 *
 * The file header is as follows:
 *
 * Field           Type           Description
 * -----           ----           -----------
 * magic_number    uint8_t[12]    Constant value: "\x81""FOSSILIZEDB"
 * version         uint32_t       StreamArchive version: 6
 *
 *
 * Each entry follows this format:
 *
 * Field           Type                    Description
 * -----           ----                    -----------
 * name            unsigned char[40]       Application-defined entry identifier, stored in hexadecimal big-endian
 *                                         ASCII. Usually N-char tag followed by (40 - N)-char hash.
 * size            uint32_t                Size of the payload as stored in this file.
 * flags           uint32_t                Flags for this entry (e.g. compression). See below.
 * crc32           uint32_t                CRC32 of the payload as stored in this file.
 * full_size       uint32_t                Size of this payload after decompression.
 * payload         uint8_t[stored_size]    Entry data.
 *
 * The flags field may contain:
 *     0x1: No compression.
 *     0x2: Deflate compression.
 */

#define FOZDB_MIN_COMPAT_VERSION  5
#define FOZDB_VERSION             6

#define FOZDB_COMPRESSION_NONE    1
#define FOZDB_COMPRESSION_DEFLATE 2

#define ENTRY_NAME_SIZE  40

#define BUFFER_COPY_BYTES (8 * 1024 * 1024) /* Tuneable. */

static const uint8_t FOZDB_MAGIC[] = {0x81, 'F', 'O', 'S', 'S', 'I', 'L', 'I', 'Z', 'E', 'D', 'B'};

struct file_header
{
    uint8_t magic[12];
    uint8_t unused1;
    uint8_t unused2;
    uint8_t unused3;
    uint8_t version;
} __attribute__((packed));

struct payload_header
{
    uint32_t size;
    uint32_t compression;
    uint32_t crc;
    uint32_t full_size;
} __attribute__((packed));

struct payload_entry
{
    struct payload_hash hash;
    struct payload_header header;
    uint64_t offset;
};

static guint hash_func(gconstpointer key)
{
    const struct payload_hash *payload_hash = key;

    return payload_hash->hash[0]
            ^ payload_hash->hash[1]
            ^ payload_hash->hash[2]
            ^ payload_hash->hash[3];
}

static gboolean hash_equal(gconstpointer a, gconstpointer b)
{
    return memcmp(a, b, sizeof(struct payload_hash)) == 0;
}

static bool tag_from_ascii_bytes(uint32_t *tag, const uint8_t *ascii_bytes)
{
    char str[sizeof(*tag) * 2 + 1] = {};

    memcpy(str, ascii_bytes, sizeof(*tag) * 2);

    *tag = strtoul(str, NULL, 16);
    if (errno != 0)
    {
        GST_ERROR("Failed to convert string \"%s\" to tag. %s.", str, strerror(errno));
        return false;
    }

    return true;
}

static bool hash_from_ascii_bytes(struct payload_hash *hash, const uint8_t *ascii_bytes)
{
    unsigned int i;

    for (i = 0; i < ARRAY_SIZE(hash->hash); ++i)
    {
        uint32_t *hash_part = &hash->hash[ARRAY_SIZE(hash->hash) - 1 - i];
        char str[sizeof(hash_part) * 2 + 1] = {};

        memcpy(str, ascii_bytes + sizeof(*hash_part) * 2 * i, sizeof(*hash_part) * 2);

        *hash_part = strtoul(str, NULL, 16);

        if (errno != 0)
        {
            GST_ERROR("Failed to convert string \"%s\" to hash part %u. %s.", str, 4 - i, strerror(errno));
            return false;
        }
    }

    return true;
}

static void tag_to_ascii_bytes(uint32_t tag, uint8_t *ascii_bytes)
{
    char buffer[sizeof(tag) * 2 + 1];
    sprintf(buffer, "%08x", tag);
    memcpy(ascii_bytes, buffer, sizeof(tag) * 2);
}

static void hash_to_ascii_bytes(const struct payload_hash *hash, uint8_t *ascii_bytes)
{
    char buffer[sizeof(*hash) * 2 + 1];
    sprintf(buffer, "%08x%08x%08x%08x", hash->hash[3], hash->hash[2], hash->hash[1], hash->hash[0]);
    memcpy(ascii_bytes, buffer, sizeof(*hash) * 2);
}

static void payload_header_from_bytes(struct payload_header *header, const uint8_t *bytes)
{
    header->size        = bytes_to_uint32(bytes);
    header->compression = bytes_to_uint32(bytes + 4);
    header->crc         = bytes_to_uint32(bytes + 8);
    header->full_size   = bytes_to_uint32(bytes + 12);
}

static int fozdb_read_file_header(struct fozdb *db)
{
    struct file_header header;

    if (!complete_read(db->file, &header, sizeof(header)))
    {
        GST_ERROR("Failed to read file header.");
        return CONV_ERROR_READ_FAILED;
    }
    if (memcmp(&header.magic, FOZDB_MAGIC, sizeof(FOZDB_MAGIC)) != 0)
    {
        GST_ERROR("Bad magic.");
        return CONV_ERROR_CORRUPT_DATABASE;
    }
    if (header.version < FOZDB_MIN_COMPAT_VERSION || header.version > FOZDB_VERSION)
    {
        GST_ERROR("Incompatible version %u.", header.version);
        return CONV_ERROR_CORRUPT_DATABASE;
    }

    return CONV_OK;
}

static int fozdb_read_entry_tag_hash_header(struct fozdb *db,
        uint32_t *out_tag, struct payload_hash *out_hash, struct payload_header *out_header)
{
    uint8_t entry_name_and_header[ENTRY_NAME_SIZE + sizeof(struct payload_header)];
    struct payload_hash hash;
    uint32_t tag;

    if (!complete_read(db->file, entry_name_and_header, sizeof(entry_name_and_header)))
    {
        GST_ERROR("Failed to read entry name and header.");
        return CONV_ERROR_READ_FAILED;
    }

    if (!tag_from_ascii_bytes(&tag, entry_name_and_header)
            || !hash_from_ascii_bytes(&hash, entry_name_and_header + sizeof(tag) * 2))
        return CONV_ERROR_CORRUPT_DATABASE;

    payload_header_from_bytes(out_header, entry_name_and_header + ENTRY_NAME_SIZE);

    *out_tag = tag;
    *out_hash = hash;
    return CONV_OK;
}

static bool fozdb_seek_to_next_entry(struct fozdb *db, struct payload_header *header, bool *truncated)
{
    uint64_t file_size = 0, data_offset = lseek(db->file, 0, SEEK_CUR);

    if (truncated)
        *truncated = false;

    get_file_size(db->file, &file_size);

    if (lseek(db->file, header->size, SEEK_CUR) < 0)
    {
        GST_ERROR("Failed to seek to next entry. %s. "
                "Entry data offset %#"PRIx64", size %#x, file size %#"PRIx64".",
                strerror(errno), data_offset, header->size, file_size);
        return false;
    }

    if (file_size && data_offset + header->size > file_size)
    {
        /* Truncated chunk is not fatal. */
        GST_WARNING("Entry data larger than file, truncating database here. "
            "Entry data offset %#"PRIx64", size %#x, file size %#"PRIx64".",
                data_offset, header->size, file_size);
        if (truncated)
            *truncated = true;
    }

    return true;
}

static bool fozdb_write_entry_name(struct fozdb *db, uint32_t tag, struct payload_hash *hash)
{
    uint8_t entry_name[ENTRY_NAME_SIZE];

    tag_to_ascii_bytes(tag, entry_name);
    hash_to_ascii_bytes(hash, entry_name + sizeof(tag) * 2);

    if (!complete_write(db->file, entry_name, sizeof(entry_name)))
    {
        GST_ERROR("Failed to write entry name.");
        return false;
    }

    return true;
}

/* Copy an entry to write_pos. */
static int fozdb_copy_entry(struct fozdb *db,
        struct entry_name *name, struct payload_header *header, uint64_t entry_data_offset)
{
    uint64_t read_offset, entry_end = entry_data_offset + header->size;
    ssize_t read_size;
    uint8_t *buffer;

    if (lseek(db->file, db->write_pos, SEEK_SET) < 0)
    {
        GST_ERROR("Failed to seek file to write_pos.");
        return CONV_ERROR_SEEK_FAILED;
    }

    /* Write entry name. */
    if (!fozdb_write_entry_name(db, name->tag, &name->hash))
        return CONV_ERROR_WRITE_FAILED;
    db->write_pos += ENTRY_NAME_SIZE;

    /* Write entry header. */
    if (!complete_write(db->file, header, sizeof(*header)))
    {
        GST_ERROR("Failed to write entry header.");
        return CONV_ERROR_WRITE_FAILED;
    }
    db->write_pos += sizeof(*header);

    /* Copy entry data. */
    buffer = calloc(1, BUFFER_COPY_BYTES);
    for (read_offset = entry_data_offset; read_offset < entry_end; read_offset += read_size)
    {
        size_t to_read = min(entry_end - read_offset, BUFFER_COPY_BYTES);

        /* Read data from entry. */
        if (lseek(db->file, read_offset, SEEK_SET) < 0)
        {
            GST_ERROR("Failed to seek to read offset. %s.", strerror(errno));
            free(buffer);
            return CONV_ERROR_SEEK_FAILED;
        }
        if ((read_size = read(db->file, buffer, to_read)) < 0)
        {
            GST_ERROR("Failed to read entry data. %s.", strerror(errno));
            free(buffer);
            return CONV_ERROR_READ_FAILED;
        }
        if (read_size == 0)
            break;

        /* Write data to write_pos. */
        if (lseek(db->file, db->write_pos, SEEK_SET) < 0)
        {
            GST_ERROR("Failed to seek to write_pos. %s.", strerror(errno));
            free(buffer);
            return CONV_ERROR_SEEK_FAILED;
        }
        if (!complete_write(db->file, buffer, read_size))
        {
            GST_ERROR("Failed to write entry data to write_pos.");
            free(buffer);
            return CONV_ERROR_WRITE_FAILED;
        }
        db->write_pos += read_size;
    }
    free(buffer);

    if (lseek(db->file, read_offset, SEEK_SET) < 0)
    {
        GST_ERROR("Failed to seek to read offset. %s.", strerror(errno));
        return CONV_ERROR_SEEK_FAILED;
    }

    return CONV_OK;
}

int fozdb_create(const char *file_name, int open_flags, bool read_only, uint32_t num_tags, struct fozdb **out)
{
    struct fozdb *db;
    size_t i;
    int ret;

    GST_DEBUG("file_name %s, open_flags %d, read_only %d, num_tags %u, out %p.",
            file_name, open_flags, read_only, num_tags, out);

    db = calloc(1, sizeof(*db));

    if (!open_file(file_name, open_flags, &db->file))
    {
        free(db);
        return CONV_ERROR_OPEN_FAILED;
    }

    db->file_name = file_name;
    db->num_tags = num_tags;
    db->read_only = read_only;

    /* Create entry hash tables. */
    db->seen_blobs = calloc(num_tags, sizeof(*db->seen_blobs));
    for (i = 0; i < num_tags; ++i)
        db->seen_blobs[i] = g_hash_table_new_full(hash_func, hash_equal, NULL, free);

    /* Load entries. */
    if ((ret = fozdb_prepare(db)) < 0)
    {
        GST_ERROR("Failed to prepare fozdb, ret %d.", ret);
        fozdb_release(db);
        return ret;
    }

    GST_INFO("Created fozdb %p from %s.", db, file_name);

    *out = db;
    return CONV_OK;
}

void fozdb_release(struct fozdb *db)
{
    int i;

    GST_DEBUG("db %p.", db);

    for (i = 0; i < db->num_tags; ++i)
        g_hash_table_destroy(db->seen_blobs[i]);
    free(db->seen_blobs);
    close(db->file);
    free(db);
}

int fozdb_prepare(struct fozdb *db)
{
    uint64_t file_size;
    int ret;

    GST_DEBUG("db %p, file_name %s, read_only %d, num_tags %u.",
            db, db->file_name, db->read_only, db->num_tags);

    db->write_pos = lseek(db->file, 0, SEEK_SET);
    if (!get_file_size(db->file, &file_size))
        return CONV_ERROR;

    /* New file, write foz header. */
    if (!file_size)
    {
        struct file_header file_header;

        memcpy(file_header.magic, FOZDB_MAGIC, sizeof(FOZDB_MAGIC));
        file_header.unused1 = 0;
        file_header.unused2 = 0;
        file_header.unused3 = 0;
        file_header.version = FOZDB_VERSION;

        if (!complete_write(db->file, &file_header, sizeof(file_header)))
        {
            GST_ERROR("Failed to write file header.");
            return CONV_ERROR_WRITE_FAILED;
        }
        db->write_pos = sizeof(file_header);

        return CONV_OK;
    }

    /* Read file header. */
    if ((ret = fozdb_read_file_header(db)) < 0)
        return ret;
    db->write_pos = lseek(db->file, 0, SEEK_CUR);

    /* Read entries to seen_blobs. */
    while (db->write_pos < file_size)
    {
        struct payload_entry entry, *table_entry;
        uint32_t tag;

        /* Read an entry. */
        if ((ret = fozdb_read_entry_tag_hash_header(db, &tag, &entry.hash, &entry.header) < 0))
            return ret;
        entry.offset = lseek(db->file, 0, SEEK_CUR);

        if (!fozdb_seek_to_next_entry(db, &entry.header, NULL))
            return CONV_ERROR_SEEK_FAILED;
        db->write_pos = lseek(db->file, 0, SEEK_CUR);

        GST_INFO("Got entry: tag %u, hash %s, offset %#"PRIx64", size %#x, crc %#x.",
                tag, format_hash(&entry.hash), entry.offset, entry.header.size, entry.header.crc);

        /* Insert entry to hash table. */
        if (tag >= db->num_tags)
        {
            GST_WARNING("Invalid tag %u.", tag);

            /* Ignore unknown tags for read-only DBs. */
            if (db->read_only)
                continue;
            else
                return CONV_ERROR_INVALID_TAG;
        }
        table_entry = calloc(1, sizeof(*table_entry));
        *table_entry = entry;
        g_hash_table_insert(db->seen_blobs[tag], &table_entry->hash, table_entry);
    }

    return CONV_OK;
}

bool fozdb_has_entry(struct fozdb *db, uint32_t tag, struct payload_hash *hash)
{
    if (tag >= db->num_tags)
        return false;
    return g_hash_table_contains(db->seen_blobs[tag], hash);
}

int fozdb_entry_size(struct fozdb *db, uint32_t tag, struct payload_hash *hash, uint32_t *size)
{
    struct payload_entry *entry;

    if (tag >= db->num_tags)
        return CONV_ERROR_INVALID_TAG;
    if (!(entry = g_hash_table_lookup(db->seen_blobs[tag], hash)))
        return CONV_ERROR_ENTRY_NOT_FOUND;

    *size = entry->header.full_size;

    return CONV_OK;
}

void fozdb_iter_tag(struct fozdb *db, uint32_t tag, GHashTableIter *iter)
{
    if (tag > db->num_tags)
    {
        GST_ERROR("Invalid tag %u.", tag);
        return;
    }
    g_hash_table_iter_init(iter, db->seen_blobs[tag]);
}

int fozdb_read_entry_data(struct fozdb *db, uint32_t tag, struct payload_hash *hash,
        uint64_t offset, uint8_t *buffer, size_t size, size_t *read_size, bool with_crc)
{
    struct payload_entry *entry;
    size_t to_copy;

    GST_DEBUG("db %p, file_name %s, tag %u, hash %s, offset %#"PRIx64", buffer %p, size %zu, read_size %p, with_crc %d.",
            db, db->file_name, tag, format_hash(hash), offset, buffer, size, read_size, with_crc);

    if (tag >= db->num_tags)
        return CONV_ERROR_INVALID_TAG;
    if (!(entry = g_hash_table_lookup(db->seen_blobs[tag], hash)))
        return CONV_ERROR_ENTRY_NOT_FOUND;

    if (entry->header.compression != FOZDB_COMPRESSION_NONE)
        return CONV_ERROR_NOT_IMPLEMENTED;

    if (offset >= entry->header.full_size)
        return CONV_OK;

    if (lseek(db->file, entry->offset + offset, SEEK_SET) < 0)
        return CONV_ERROR_SEEK_FAILED;

    to_copy = min(entry->header.full_size - offset, size);
    if (!complete_read(db->file, buffer, to_copy))
    {
        GST_ERROR("Failed to read entry data.");
        return CONV_ERROR_READ_FAILED;
    }
    *read_size = to_copy;

    if (entry->header.crc != 0 && with_crc && entry->header.crc != crc32(0, buffer, to_copy))
    {
        GST_ERROR("Wrong check sum.");
        return CONV_ERROR_WRONG_CHECKSUM;
    }

    return CONV_OK;
}

int fozdb_write_entry(struct fozdb *db, uint32_t tag, struct payload_hash *hash,
        void *data_src, data_read_callback read_callback, bool with_crc)
{
    struct payload_header header;
    struct payload_entry *entry;
    off_t header_offset;
    uint32_t size = 0;
    size_t read_size;
    uint8_t *buffer;
    uint64_t offset;
    int ret;

    GST_DEBUG("db %p, file_name %s, tag %u, hash %s, data_src %p, read_callback %p, with_crc %d.",
            db, db->file_name, tag, format_hash(hash), data_src, read_callback, with_crc);

    if (tag >= db->num_tags)
    {
        GST_ERROR("Invalid tag %u.", tag);
        return CONV_ERROR_INVALID_TAG;
    }
    if (fozdb_has_entry(db, tag, hash))
        return CONV_OK;

    if (lseek(db->file, db->write_pos, SEEK_SET) < 0)
    {
        GST_ERROR("Failed to seek file to write_pos %#"PRIx64".", db->write_pos);
        return CONV_ERROR_SEEK_FAILED;
    }

    /* Write entry name. */
    if (!fozdb_write_entry_name(db, tag, hash))
        return CONV_ERROR_WRITE_FAILED;

    /* Write payload header first. */
    header_offset = lseek(db->file, 0, SEEK_CUR);
    header.size = UINT32_MAX;      /* Will be filled later. */
    header.compression = FOZDB_COMPRESSION_NONE;
    header.crc = 0;                /* Will be filled later. */
    header.full_size = UINT32_MAX; /* Will be filled later. */
    if (!complete_write(db->file, &header, sizeof(header)))
    {
        GST_ERROR("Failed to write entry header.");
        return CONV_ERROR_WRITE_FAILED;
    }
    offset = lseek(db->file, 0, SEEK_CUR);

    /* Write data. */
    buffer = calloc(1, BUFFER_COPY_BYTES);
    while ((ret = read_callback(data_src, buffer, BUFFER_COPY_BYTES, &read_size)) == CONV_OK)
    {
        if (size + read_size > UINT32_MAX)
        {
            GST_ERROR("Data too large. Fossilize format only supports 4 GB entries.");
            free(buffer);
            return CONV_ERROR;
        }

        size += read_size;
        if (!complete_write(db->file, buffer, read_size))
        {
            GST_ERROR("Failed to write entry data.");
            free(buffer);
            return CONV_ERROR_WRITE_FAILED;
        }

        if (with_crc)
            header.crc = crc32(header.crc, buffer, read_size);
    }
    db->write_pos = lseek(db->file, 0, SEEK_CUR);
    free(buffer);
    if (ret != CONV_ERROR_DATA_END)
    {
        GST_ERROR("Failed to read data from data src, ret %d.", ret);
        return ret;
    }

    /* Seek back and fill in the size to header. */
    if (lseek(db->file, header_offset, SEEK_SET) < 0)
    {
        GST_ERROR("Failed to seek back to entry header. %s.", strerror(errno));
        return CONV_ERROR_SEEK_FAILED;
    }
    header.size = size;
    header.full_size = size;
    if (!complete_write(db->file, &header, sizeof(header)))
    {
        GST_ERROR("Failed to write entry header.");
        return CONV_ERROR_WRITE_FAILED;
    }

    /* Success. Record entry and exit. */
    entry = calloc(1, sizeof(*entry));
    entry->header = header;
    entry->hash = *hash;
    entry->offset = offset;
    g_hash_table_insert(db->seen_blobs[tag], &entry->hash, entry);

    GST_INFO("Wrote entry: tag %u, hash %s, offset %#"PRIx64", size %#x, crc %#x.",
            tag, format_hash(&entry->hash), entry->offset, entry->header.size, entry->header.crc);

    return CONV_OK;
}

/* Rewrites the database, discarding entries listed. */
int fozdb_discard_entries(struct fozdb *db, GList *to_discard_names)
{
    uint8_t entry_name_and_header[ENTRY_NAME_SIZE + sizeof(struct payload_header)];
    uint64_t file_size;
    int i, ret;

    GST_DEBUG("db %p, file_name %s, to_discard_entries %p.", db, db->file_name, to_discard_names);

    /* Rewind the file and clear the entry tables. */
    if (lseek(db->file, 0, SEEK_SET) < 0)
    {
        GST_ERROR("Failed to seek to file start. %s.", strerror(errno));
        return CONV_ERROR_SEEK_FAILED;
    }
    for (i = 0; i < db->num_tags; ++i)
        g_hash_table_remove_all(db->seen_blobs[i]);

    /* Read file header. */
    if ((ret = fozdb_read_file_header(db)) < 0)
        return ret;
    db->write_pos = lseek(db->file, 0, SEEK_CUR);

    /* Read each entry and see if it should be discarded. */
    if (!get_file_size(db->file, &file_size))
        return CONV_ERROR;
    while (lseek(db->file, 0, SEEK_CUR) < file_size)
    {
        struct payload_header header;
        uint64_t entry_data_offset;
        struct entry_name name;
        bool truncated;

        if ((ret = fozdb_read_entry_tag_hash_header(db, &name.tag, &name.hash, &header) < 0))
            return CONV_ERROR_READ_FAILED;
        entry_data_offset = lseek(db->file, 0, SEEK_CUR);

        /* Check if entry should be discarded. */
        if (g_list_find_custom(to_discard_names, &name, entry_name_compare))
        {
            if (!fozdb_seek_to_next_entry(db, &header, &truncated))
                return CONV_ERROR_SEEK_FAILED;
            if (truncated)
                break;
        }
        else
        {
            if (db->write_pos == entry_data_offset - sizeof(entry_name_and_header))
            {
                /* If we haven't dropped any chunks, we can just skip it rather than rewrite it. */
                if (!fozdb_seek_to_next_entry(db, &header, &truncated))
                    return CONV_ERROR_SEEK_FAILED;
                if (truncated)
                    break;
                db->write_pos = lseek(db->file, 0, SEEK_CUR);
            }
            else
            {
                /* We're offset, so we have to rewrite. */
                if ((ret = fozdb_copy_entry(db, &name, &header, entry_data_offset)) < 0)
                    return ret;
            }
        }
    }

    if (ftruncate(db->file, db->write_pos) < 0)
    {
        GST_ERROR("Failed to truncate file. %s.", strerror(errno));
        return CONV_ERROR;
    }

    return fozdb_prepare(db);
}
