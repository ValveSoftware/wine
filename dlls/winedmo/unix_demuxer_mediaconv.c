/*
 * Copyright 2024 Rémi Bernon for CodeWeavers
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

#if 0
#pragma makedep unix
#endif

#include "config.h"
#include "unix_private.h"

#include <pthread.h>

#include "media-converter/media-converter.h"
#include "wine/debug.h"
#include "wine/list.h"

#ifdef HAVE_FFMPEG

WINE_DEFAULT_DEBUG_CHANNEL(dmo);

#define HASH_CHUNK_SIZE (8 * 1024 * 1024) /* 8 MB. */
#define HASH_STRIDE     (HASH_CHUNK_SIZE * 6)

#define VIDEO_CONV_FOZ_TAG_VIDEODATA 0
#define VIDEO_CONV_FOZ_TAG_OGVDATA   1
#define VIDEO_CONV_FOZ_TAG_STREAM    2
#define VIDEO_CONV_FOZ_TAG_MKVDATA   3
#define VIDEO_CONV_FOZ_TAG_CODEC     4
#define VIDEO_CONV_FOZ_NUM_TAGS      5

static pthread_mutex_t fozdb_lock = PTHREAD_MUTEX_INITIALIZER;
static struct fozdb *dump_db, *read_db;
static int fozdb_users;

static int64_t mediaconv_demuxer_seek_callback( void *opaque, int64_t offset, int whence )
{
    struct stream_context *context = opaque;

    TRACE( "opaque %p, offset %#"PRIx64", whence %#x\n", opaque, offset, whence );

    if (whence == AVSEEK_SIZE) return context->length;
    if (whence == SEEK_END) offset += context->length;
    if (whence == SEEK_CUR) offset += context->position;
    if (offset > context->length) offset = context->length;

    context->position = offset;
    return offset;
}

static int mediaconv_demuxer_read_callback( void *opaque, uint8_t *buffer, int size )
{
    struct stream_context *context = opaque;
    struct fozdb_entry *entry = *(struct fozdb_entry **)context->buffer;
    size_t read_size;
    int ret;

    TRACE( "opaque %p, buffer %p, size %#x\n", opaque, buffer, size );

    if (!size) return AVERROR_EOF;

    pthread_mutex_lock( &fozdb_lock );
    ret = fozdb_read_entry_data( read_db, entry->key.tag, &entry->key.hash, context->position,
                                 buffer, size, &read_size, false );
    pthread_mutex_unlock( &fozdb_lock );

    context->position += read_size;
    if (ret < 0 && ret != CONV_ERROR_DATA_END) return AVERROR(EIO);
    return read_size ? read_size : AVERROR_EOF;
}

void mediaconv_demuxer_init(void)
{
    pthread_mutex_lock( &fozdb_lock );

    if (!fozdb_users++)
    {
        const char *dump_path, *read_path;

        dump_path = getenv( "MEDIACONV_VIDEO_DUMP_FILE" );
        read_path = getenv( "MEDIACONV_VIDEO_TRANSCODED_FILE" );
        TRACE( "dump_path %s, read_path %s\n", debugstr_a(dump_path), debugstr_a(read_path) );

        if (!read_path || fozdb_create( read_path, O_RDONLY, true, VIDEO_CONV_FOZ_NUM_TAGS, &read_db ) < 0) read_db = NULL;
        if (!dump_path || fozdb_create( dump_path, O_RDWR | O_CREAT, false, VIDEO_CONV_FOZ_NUM_TAGS, &dump_db ) < 0) dump_db = NULL;
    }

    pthread_mutex_unlock( &fozdb_lock );
}

void mediaconv_demuxer_exit(void)
{
    pthread_mutex_lock( &fozdb_lock );

    if (!fozdb_users--)
    {
        TRACE( "closing databases\n" );
        if (read_db) fozdb_release( read_db );
        if (dump_db) fozdb_release( dump_db );
        read_db = dump_db = NULL;
    }

    pthread_mutex_unlock( &fozdb_lock );
}

static int stream_hasher_read( void *opaque, uint8_t *buffer, size_t size, size_t *read_size )
{
    struct stream_context *context = opaque;
    size_t total = 0;
    int ret = 0;

    while (size)
    {
        int step;

        if (context->position < HASH_STRIDE) step = min( size, HASH_STRIDE - context->position );
        else step = min( size, HASH_CHUNK_SIZE - (context->position % HASH_CHUNK_SIZE) );

        if ((ret = unix_read_callback( context, buffer, step )) < 0) break;
        buffer += ret;
        total += ret;
        size -= ret;

        if (context->position > HASH_STRIDE && !(context->position % HASH_CHUNK_SIZE))
            unix_seek_callback( context, HASH_STRIDE - HASH_CHUNK_SIZE, SEEK_CUR );
    }

    *read_size = total;
    if (ret < 0 && ret != AVERROR_EOF) return CONV_ERROR_READ_FAILED;
    return total ? CONV_OK : CONV_ERROR_DATA_END;
}

struct hash_entry
{
    struct list entry;
    struct fozdb_hash hash;
};

static int dump_chunk_data( const void *buffer, size_t read_size, struct list *hashes )
{
    struct bytes_reader bytes_reader;
    struct hash_entry *entry;

    if (!(entry = calloc( 1, sizeof(*entry) ))) return AVERROR(ENOMEM);

    bytes_reader_init( &bytes_reader, buffer, read_size );
    murmur3_128( &bytes_reader, bytes_reader_read, HASH_SEED, &entry->hash );
    list_add_tail( hashes, &entry->entry );

    bytes_reader_init( &bytes_reader, buffer, read_size );
    return fozdb_write_entry( dump_db, VIDEO_CONV_FOZ_TAG_VIDEODATA, &entry->hash,
                              &bytes_reader, bytes_reader_read, true );
}

static int chunk_hasher_read( void *opaque, uint8_t *buffer, size_t size, size_t *read_size )
{
    struct fozdb_hash *hash = (struct fozdb_hash *)buffer;
    struct list *ptr, *hashes = opaque;
    struct hash_entry *entry;

    *read_size = 0;
    if (!size) return CONV_OK;
    if (!(ptr = list_head( hashes ))) return CONV_ERROR_DATA_END;
    if (size < sizeof(*hash)) return CONV_ERROR_INVALID_ARGUMENT;

    entry = LIST_ENTRY( ptr, struct hash_entry, entry );
    list_remove( &entry->entry );
    *read_size = sizeof(*hash);
    *hash = entry->hash;
    free( entry );

    return CONV_OK;
}

int mediaconv_demuxer_open( AVFormatContext **ctx, struct stream_context *context )
{
    struct fozdb_entry *transcoded = NULL;
    struct fozdb_hash stream_hash;
    void *buffer;
    int i, ret;

    if (*ctx)
    {
        AVCodecParameters *par;
        for (i = 0; i < (*ctx)->nb_streams; i++)
        {
            par = (*ctx)->streams[i]->codecpar;
            if (par->codec_id && !strcmp("h264", avcodec_get_name(par->codec_id)))
                    create_placeholder_file("h264-used");
            if (!par->codec_id) FIXME( "Ignoring unknown codec on stream %u\n", i );
            else if (par->codec_id != AV_CODEC_ID_OPUS && !avcodec_find_decoder( par->codec_id )) break;
        }
        if (i == (*ctx)->nb_streams) return 0;

        WARN( "Failed to find decoder for stream %u, codec %#x %s\n", i, par->codec_id, avcodec_get_name(par->codec_id) );
        avio_context_free( &(*ctx)->pb );
        avformat_free_context( *ctx );
    }

    if (!(buffer = calloc( 1, HASH_CHUNK_SIZE ))) return AVERROR(ENOMEM);

    if ((ret = unix_seek_callback( context, 0, SEEK_SET )) < 0) return ret;
    if (!murmur3_128( context, stream_hasher_read, HASH_SEED, &stream_hash )) return AVERROR(EINVAL);
    TRACE( "stream hash %s\n", debugstr_fozdb_hash( &stream_hash ) );

    pthread_mutex_lock( &fozdb_lock );

    if (read_db && (transcoded = fozdb_entry_get( &read_db->entries, VIDEO_CONV_FOZ_TAG_MKVDATA, &stream_hash )))
        TRACE( "Found mkv stream for %s\n", debugstr_fozdb_hash( &stream_hash ) );
    else if (read_db && (transcoded = fozdb_entry_get( &read_db->entries, VIDEO_CONV_FOZ_TAG_OGVDATA, &stream_hash )))
        TRACE( "Found ogv stream for %s\n", debugstr_fozdb_hash( &stream_hash ) );
    else if (!dump_db)
        TRACE( "No dump fozdb for stream %s\n", debugstr_fozdb_hash( &stream_hash ) );
    else
    {
        struct list hashes = LIST_INIT(hashes);
        TRACE( "Transcoded stream %s not found, dumping\n", debugstr_fozdb_hash( &stream_hash ) );

        unix_seek_callback( context, 0, SEEK_SET );
        while ((ret = unix_read_callback( context, buffer, HASH_CHUNK_SIZE )) > 0)
        {
            if ((ret = dump_chunk_data( buffer, ret, &hashes )) < 0)
            {
                ERR("Error writing video data to fozdb, ret %d\n.", ret);
                break;
            }
        }

        if ((ret = fozdb_write_entry( dump_db, VIDEO_CONV_FOZ_TAG_STREAM, &stream_hash,
                                      &hashes, chunk_hasher_read, true )) < 0)
            ERR("Error writing stream data to fozdb, ret %d.", ret);
    }

    pthread_mutex_unlock( &fozdb_lock );

    if (transcoded)
    {
        context->position = 0;
        context->length = transcoded->full_size;
        *(struct fozdb_entry **)context->buffer = transcoded;

        if (!(*ctx = avformat_alloc_context())) return AVERROR(ENOMEM);
        if (!((*ctx)->pb = avio_alloc_context( NULL, 0, 0, context, mediaconv_demuxer_read_callback,
                                               NULL, mediaconv_demuxer_seek_callback )))
        {
            avformat_free_context( *ctx );
            return AVERROR(ENOMEM);
        }
    }
    else
    {
        const char *blank_path;
        int ret;

        create_placeholder_file("placeholder-video-used");

        if (!(blank_path = getenv( "MEDIACONV_BLANK_VIDEO_FILE" ))) return AVERROR(ENOENT);
        if (!(*ctx = avformat_alloc_context())) return AVERROR(ENOMEM);
        if ((ret = avio_open( &(*ctx)->pb, blank_path, AVIO_FLAG_READ )) < 0)
        {
            avformat_free_context( *ctx );
            return ret;
        }
    }

    return avformat_open_input( ctx, NULL, NULL, NULL );
}

#endif /* HAVE_FFMPEG */
