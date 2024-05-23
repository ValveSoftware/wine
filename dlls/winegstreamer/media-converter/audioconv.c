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

/* Algorithm
 * ---------
 *
 * The application feeds encoded audio into XAudio2 in chunks. Since we don't have access to all
 * chunks in a stream on initialization (as we do with the video converter), we continuously hash
 * the stream as it is sent to us. Each "chunk" is identified as the hash of the entire stream up
 * to that chunk.
 *
 * Since chunks are small (~2 kilobytes), this leads to a significant possibility of two different
 * streams having identical intro chunks (imagine two streams that start with several seconds of
 * silence). This means we need a tree of chunks. Suppose two incoming streams with chunks that
 * hash as shown (i.e. identical intro chunks that diverge later):
 *
 * Stream 1: [AA BB CC DD]
 *
 * Stream 2: [AA BB YY ZZ]
 *
 * We record a tree and the transcoder will walk it depth-first in order to reconstruct each unique
 * stream:
 *
 * AA => aa.ptna
 * AA+BB => bb.ptna
 * AA+BB+CC => cc.ptna
 * AA+BB+CC+DD => dd.ptna
 * AA+BB+YY => yy.ptna
 * AA+BB+YY+ZZ => zz.ptna
 *
 * Upon playback, we chain each transcoded stream chunk together as the packets come in:
 *
 * AA -> start stream with aa.ptna
 * BB -> play bb.ptna
 * CC -> play cc.ptna
 * DD -> play dd.ptna
 *
 * or:
 *
 * AA -> start stream with aa.ptna
 * BB -> play bb.ptna
 * YY -> play yy.ptna
 * ZZ -> play zz.ptna
 *
 * or:
 *
 * AA -> start stream with aa.ptna
 * NN -> not recognized, instead play blank.ptna and mark this stream as needs-transcoding
 * OO -> play blank.ptna
 * PP -> play blank.ptna
 * When the Stream is destroyed, we'll record AA+NN+OO+PP into the needs-transcode database
 * for the transcoder to convert later.
 *
 *
 * Physical Format
 * ---------------
 *
 * All stored values are little-endian.
 *
 * Transcoded audio is stored in the "transcoded" Fossilize database under the
 * AUDIO_CONV_FOZ_TAG_PTNADATA tag. Each chunk is stored in one entry with as many of the following
 * "Proton Audio" (ptna) packets as are required to store the entire transcoded chunk:
 *
 *     uint32_t packet_header: Information about the upcoming packet, see bitmask:
 *        MSB [FFFF PPPP PPPP PPPP PPPP LLLL LLLL LLLL] LSB
 *        L: Number of _bytes_ in this packet following this header.
 *        P: Number of _samples_ at the end of this packet which are padding and should be skipped.
 *        F: Flag bits:
 *           0x1: This packet is an Opus header
 *           0x2, 0x4, 0x8: Reserved for future use.
 *
 *     If the Opus header flag is set:
 *        Following packet is an Opus identification header, as defined in RFC 7845 "Ogg
 *        Encapsulation for the Opus Audio Codec" Section 5.1.
 *        <https://tools.ietf.org/html/rfc7845#section-5.1>
 *
 *     If the header flag is not set:
 *        Following packet is raw Opus data to be sent to an Opus decoder.
 *
 *
 * If we encounter a stream which needs transcoding, we record the buffers and metadata in
 * a Fossilize database. The database has three tag types:
 *
 * AUDIO_CONV_FOZ_TAG_STREAM: This identifies each unique stream of buffers. For example:
 *   [hash(AA+BB+CC+DD)] -> [AA, BB, CC, DD]
 *   [hash(AA+BB+XX+YY)] -> [AA, BB, XX, YY]
 *
 * AUDIO_CONV_FOZ_TAG_AUDIODATA: This contans the actual encoded audio data. For example:
 *   [AA] -> [AA's buffer data]
 *   [BB] -> [BB's buffer data]
 *
 * AUDIO_CONV_FOZ_TAG_CODECINFO: This contans the codec data required to decode the buffer. Only
 * the "head" of each stream is recorded. For example:
 *   [AA] -> [
 *     uint32_t wmaversion                 (from WAVEFORMATEX.wFormatTag)
 *     uint32_t bitrate                    (from WAVEFORMATEX.nAvgBytesPerSec)
 *     uint32_t channels                   (WAVEFORMATEX.nChannels)
 *     uint32_t rate                       (WAVEFORMATEX.nSamplesPerSec)
 *     uint32_t block_align                (WAVEFORMATEX.nBlockAlign)
 *     uint32_t depth                      (WAVEFORMATEX.wBitsPerSample)
 *     char[remainder of entry] codec_data (codec data which follows WAVEFORMATEX)
 *   ]
 *
 */

#if 0
#pragma makedep unix
#endif

#include "media-converter.h"

#include <gst/audio/audio.h>
#include <errno.h>
#include <string.h>
#include <assert.h>

GST_DEBUG_CATEGORY_EXTERN(media_converter_debug);
#undef GST_CAT_DEFAULT
#define GST_CAT_DEFAULT media_converter_debug

#define AUDIO_CONV_ENCODED_LENGTH_MASK  0x00000fff /* 4kB fits in here. */
#define AUDIO_CONV_PADDING_LENGTH_MASK  0x0ffff000 /* 120ms of samples at 48kHz fits in here. */
#define AUDIO_CONV_PADDING_LENGTH_SHIFT 12
#define AUDIO_CONV_FLAG_MASK            0xf0000000
#define AUDIO_CONV_FLAG_HEADER          0x10000000 /* This chunk is the Opus header. */
#define _AUDIO_CONV_FLAG_RESERVED1      0x20000000 /* Not yet used */
#define _AUDIO_CONV_FLAG_RESERVED2      0x40000000 /* Not yet used */
#define _AUDIO_CONV_FLAG_V2             0x80000000 /* Indicates a "version 2" header, process somehow differently (TBD). */

/* Properties of the "blank" audio file. */
#define _BLANK_AUDIO_FILE_LENGTH_MS 10.0
#define _BLANK_AUDIO_FILE_RATE      48000.0

#define AUDIO_CONV_FOZ_TAG_STREAM    0
#define AUDIO_CONV_FOZ_TAG_CODECINFO 1
#define AUDIO_CONV_FOZ_TAG_AUDIODATA 2
#define AUDIO_CONV_FOZ_TAG_PTNADATA  3
#define AUDIO_CONV_FOZ_NUM_TAGS      4

typedef enum
{
    NO_LOOP,
    LOOPING,
    LOOP_ENDED,
    LOOP_ERROR,
} loop_state;

struct buffer_entry
{
    struct payload_hash hash;
    GstBuffer *buffer;
};

/* Followed by codec_data_size bytes codec data. */
struct need_transcode_head
{
    size_t codec_data_size;
    uint32_t wmaversion;
    uint32_t bitrate;
    uint32_t channels;
    uint32_t rate;
    uint32_t block_align;
    uint32_t depth;
} __attribute__((packed));

/* Represents a Stream, a sequence of buffers. */
struct stream_state
{
    struct murmur3_128_state hash_state;
    struct payload_hash current_hash;
    GList *buffers;      /* Entry type: struct buffer_entry. */
    GList *loop_buffers; /* Entry type: struct buffer_entry. */
    struct need_transcode_head *codec_info;
    bool needs_dump;
};

struct stream_state_serializer
{
    struct stream_state *state;
    int index;
};

struct audio_conv_state
{
    bool sent_header;
    struct need_transcode_head *codec_data;
    struct murmur3_128_state hash_state;
    struct murmur3_128_state loop_hash_state;
    struct stream_state *stream_state;
    struct fozdb *read_fozdb;
};

typedef struct
{
    GstElement element;
    GstPad *sink_pad, *src_pad;
    pthread_mutex_t state_mutex;
    struct audio_conv_state *state;
} AudioConv;

typedef struct
{
    GstElementClass class;
} AudioConvClass;

G_DEFINE_TYPE(AudioConv, audio_conv, GST_TYPE_ELEMENT);
#define AUDIO_CONV_TYPE (audio_conv_get_type())
#define AUDIO_CONV(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), AUDIO_CONV_TYPE, AudioConv))
#define parent_class    (audio_conv_parent_class)
GST_ELEMENT_REGISTER_DEFINE(protonaudioconverter, "protonaudioconverter",
        GST_RANK_MARGINAL, AUDIO_CONV_TYPE);

static GstStaticPadTemplate audio_conv_sink_template = GST_STATIC_PAD_TEMPLATE("sink",
        GST_PAD_SINK, GST_PAD_ALWAYS,
        GST_STATIC_CAPS("audio/x-wma;"));

static GstStaticPadTemplate audio_conv_src_template = GST_STATIC_PAD_TEMPLATE("src",
        GST_PAD_SRC, GST_PAD_ALWAYS,
        GST_STATIC_CAPS("audio/x-opus;"));

static struct dump_fozdb dump_fozdb = {PTHREAD_MUTEX_INITIALIZER, NULL, false};

static struct buffer_entry *buffer_entry_create(struct payload_hash *hash, GstBuffer *buffer)
{
    struct buffer_entry *entry = calloc(1, sizeof(*entry));
    entry->hash = *hash;
    entry->buffer = gst_buffer_ref(buffer);
    return entry;
}

static void buffer_entry_release(void *arg)
{
    struct buffer_entry *entry = arg;
    gst_buffer_unref(entry->buffer);
    free(entry);
}

static bool dumping_disabled(void)
{
    return option_enabled("MEDIACONV_AUDIO_DONT_DUMP");
}

static bool hash_data(const uint8_t *data, size_t size, struct murmur3_128_state *hash_state, struct payload_hash *hash)
{
    struct bytes_reader reader;
    bytes_reader_init(&reader, data, size);
    return murmur3_128_full(&reader, bytes_reader_read, hash_state, hash);
}

static int dump_fozdb_open_audio(bool create)
{
    return dump_fozdb_open(&dump_fozdb, create, "MEDIACONV_AUDIO_DUMP_FILE", AUDIO_CONV_FOZ_NUM_TAGS);
}

static void dump_fozdb_discard_transcoded(void)
{
    GList *chunks_to_discard = NULL, *chunks_to_keep = NULL, *chunks = NULL, *list_iter;
    struct payload_hash chunk_id, *stream_id;
    struct fozdb *read_fozdb;
    char *read_fozdb_path;
    GHashTableIter iter;
    int ret;

    if (dump_fozdb.already_cleaned)
        return;
    dump_fozdb.already_cleaned = true;

    if (discarding_disabled())
        return;
    if (!file_exists(getenv("MEDIACONV_AUDIO_DUMP_FILE")))
        return;

    if ((dump_fozdb_open_audio(false)) < 0)
        return;

    if (!(read_fozdb_path = getenv("MEDIACONV_AUDIO_TRANSCODED_FILE")))
    {
        GST_ERROR("Env MEDIACONV_AUDIO_TRANSCODED_FILE not set.");
        return;
    }

    if ((ret = fozdb_create(read_fozdb_path, O_RDONLY, true /* Read-only?  */, AUDIO_CONV_FOZ_NUM_TAGS, &read_fozdb)) < 0)
    {
        GST_ERROR("Failed to create read fozdb, ret %d.", ret);
        return;
    }

    fozdb_iter_tag(dump_fozdb.fozdb, AUDIO_CONV_FOZ_TAG_STREAM, &iter);
    while (g_hash_table_iter_next(&iter, (void *)&stream_id, NULL))
    {
        uint32_t chunks_size, i;
        size_t read_size;

        if (fozdb_entry_size(dump_fozdb.fozdb, AUDIO_CONV_FOZ_TAG_STREAM, stream_id, &chunks_size) == CONV_OK)
        {
            uint8_t *buffer = calloc(1, chunks_size);
            if (fozdb_read_entry_data(dump_fozdb.fozdb, AUDIO_CONV_FOZ_TAG_STREAM, stream_id,
                    0, buffer, chunks_size, &read_size, true) == CONV_OK)
            {
                GList *stream_chunks = NULL;
                bool has_all = true;

                for (i = 0; i < read_size / sizeof(chunk_id); ++i)
                {
                    payload_hash_from_bytes(&chunk_id, buffer + i * sizeof(chunk_id));
                    if (!fozdb_has_entry(read_fozdb, AUDIO_CONV_FOZ_TAG_PTNADATA, &chunk_id))
                    {
                        has_all = false;
                        break;
                    }
                    stream_chunks = g_list_append(stream_chunks,
                            entry_name_create(AUDIO_CONV_FOZ_TAG_AUDIODATA, &chunk_id));
                }

                for (list_iter = stream_chunks; list_iter; list_iter = list_iter->next)
                {
                    struct entry_name *entry = list_iter->data;
                    if (has_all)
                    {
                        chunks_to_discard = g_list_append(chunks_to_discard,
                                entry_name_create(entry->tag, &entry->hash));
                        chunks_to_discard = g_list_append(chunks_to_discard,
                                entry_name_create(AUDIO_CONV_FOZ_TAG_CODECINFO, &entry->hash));
                    }
                    else
                    {
                        chunks_to_keep = g_list_append(chunks_to_keep,
                                entry_name_create(entry->tag, &entry->hash));
                        chunks_to_keep = g_list_append(chunks_to_keep,
                                entry_name_create(AUDIO_CONV_FOZ_TAG_CODECINFO, &entry->hash));
                    }
                }

                if (has_all)
                    chunks_to_discard = g_list_append(chunks_to_discard,
                            entry_name_create(AUDIO_CONV_FOZ_TAG_STREAM, stream_id));

                g_list_free_full(stream_chunks, free);
            }
            free(buffer);
        }
    }

    for (list_iter = chunks_to_discard; list_iter; list_iter = list_iter->next)
    {
        struct entry_name *entry = list_iter->data;
        if (!g_list_find_custom(chunks_to_keep, entry, entry_name_compare))
            chunks = g_list_append(chunks, entry_name_create(entry->tag, &entry->hash));
    }

    if ((ret = fozdb_discard_entries(dump_fozdb.fozdb, chunks)) < 0)
    {
        GST_ERROR("Failed to discard entries, ret %d.", ret);
        dump_fozdb_close(&dump_fozdb);
    }

    g_list_free_full(chunks, free);
    g_list_free_full(chunks_to_keep, free);
    g_list_free_full(chunks_to_discard, free);
}

static bool need_transcode_head_create_from_caps(GstCaps *caps, struct need_transcode_head **out)
{
    int wmaversion, bitrate, channels, rate, block_align, depth;
    const GstStructure *structure = gst_caps_get_structure(caps, 0);
    struct need_transcode_head *head;
    const GValue *codec_data_value;
    GstBuffer *codec_data_buffer;
    gsize codec_data_size;

    GST_DEBUG("caps %"GST_PTR_FORMAT", out %p.", caps, out);

    if (!gst_structure_get_int(structure, "wmaversion", &wmaversion))
    {
        GST_ERROR("Caps have no wmaversion field.");
        return false;
    }
    if (!gst_structure_get_int(structure, "bitrate", &bitrate))
    {
        GST_ERROR("Caps have no bitrate field.");
        return false;
    }
    if (!gst_structure_get_int(structure, "channels", &channels))
    {
        GST_ERROR("Caps have no channels field.");
        return false;
    }
    if (!gst_structure_get_int(structure, "rate", &rate))
    {
        GST_ERROR("Caps have no rate field.");
        return false;
    }
    if (!gst_structure_get_int(structure, "block_align", &block_align))
    {
        GST_ERROR("Caps have no block_align field.");
        return false;
    }
    if (!gst_structure_get_int(structure, "depth", &depth))
    {
        GST_ERROR("Caps have no depth field.");
        return false;
    }
    if (!(codec_data_value = gst_structure_get_value(structure, "codec_data"))
             || !(codec_data_buffer = gst_value_get_buffer(codec_data_value)))
    {
        GST_ERROR("Caps have no codec_data field.");
        return false;
    }

    codec_data_size = gst_buffer_get_size(codec_data_buffer);
    head = calloc(1, sizeof(*head) + codec_data_size);
    head->codec_data_size = codec_data_size;
    head->wmaversion = wmaversion;
    head->bitrate = bitrate;
    head->channels = channels;
    head->rate = rate;
    head->block_align = block_align;
    head->depth = depth;
    gst_buffer_extract(codec_data_buffer, 0, head + 1, codec_data_size);

    *out = head;
    return true;
}

static struct need_transcode_head *need_transcode_head_dup(struct need_transcode_head *head)
{
    size_t size = sizeof(*head) + head->codec_data_size;
    struct need_transcode_head *dup;

    dup = calloc(1, size);
    memcpy(dup, head, size);

    return dup;
}


static void need_transcode_head_serialize(struct need_transcode_head *head,
        uint8_t *buffer, size_t buffer_size, size_t *out_size)
{
    *out_size = sizeof(*head) - sizeof(head->codec_data_size) + head->codec_data_size;

    if (buffer_size < *out_size)
    {
        GST_ERROR("Buffer too small: buffer size %zu, out size %zu.", buffer_size, *out_size);
        return;
    }
    memcpy(buffer, &head->wmaversion, *out_size);
}

static void stream_state_serializer_init(struct stream_state_serializer *serializer, struct stream_state *state)
{
    serializer->state = state;
    serializer->index = 0;
}

static int stream_state_serializer_read(void *data_reader, uint8_t *buffer, size_t size, size_t *read_size)
{
    struct stream_state_serializer *serializer = data_reader;
    struct buffer_entry *entry;

    if (!size)
    {
        *read_size = 0;
        return CONV_OK;
    }

    if (serializer->index >= g_list_length(serializer->state->buffers))
        return CONV_ERROR_DATA_END;

    entry = g_list_nth_data(serializer->state->buffers, serializer->index++);
    memcpy(buffer, &entry->hash, sizeof(entry->hash));

    *read_size = sizeof(entry->hash);
    return CONV_OK;
}

static struct stream_state *stream_state_create(void)
{
    struct stream_state *state;
    state = calloc(1, sizeof(*state));
    murmur3_128_state_init(&state->hash_state, HASH_SEED);
    return state;
}

static void stream_state_release(struct stream_state *state)
{
    g_list_free_full(state->buffers, buffer_entry_release);
    g_list_free_full(state->loop_buffers, buffer_entry_release);
    if (state->codec_info)
        free(state->codec_info);
    free(state);
}

static void stream_state_reset(struct stream_state *state)
{
    murmur3_128_state_reset(&state->hash_state);
    memset(&state->current_hash, 0, sizeof(state->current_hash));
    g_list_free_full(g_steal_pointer(&state->buffers), buffer_entry_release);
    g_list_free_full(g_steal_pointer(&state->loop_buffers), buffer_entry_release);
    free(state->codec_info);
    state->codec_info = NULL;
    state->needs_dump = false;
}

static loop_state stream_state_record_buffer(struct stream_state *state, struct payload_hash *buffer_hash,
        struct payload_hash *loop_hash, GstBuffer *buffer, struct need_transcode_head *codec_info)
{
    if (!state->codec_info && codec_info)
        state->codec_info = need_transcode_head_dup(codec_info);

    if (g_list_length(state->loop_buffers) < g_list_length(state->buffers))
    {
        struct buffer_entry *entry = g_list_nth_data(state->buffers, g_list_length(state->loop_buffers));
        if (!memcmp(&entry->hash, loop_hash, sizeof(*loop_hash)))
        {
            state->loop_buffers = g_list_append(state->loop_buffers,
                    buffer_entry_create(buffer_hash /* Not loop_hash! */, buffer));
            if (g_list_length(state->loop_buffers) == g_list_length(state->buffers))
            {
                /* Full loop, just drop them. */
                g_list_free_full(g_steal_pointer(&state->loop_buffers), buffer_entry_release);
                return LOOP_ENDED;
            }

            return LOOPING;
        }
    }

    /* Partial loop, track them and then continue */
    if (state->loop_buffers)
        state->buffers = g_list_concat(state->buffers, g_steal_pointer(&state->loop_buffers));

    state->buffers = g_list_append(state->buffers, buffer_entry_create(buffer_hash, buffer));

    if (!hash_data((const uint8_t *)buffer_hash, sizeof(*buffer_hash), &state->hash_state, &state->current_hash))
        return LOOP_ERROR;

    return NO_LOOP;
}

static bool stream_state_is_stream_subset(struct stream_state *state, struct fozdb *db, struct payload_hash *stream_id)
{
    uint64_t offset = 0;
    GList *list_iter;

    for (list_iter = state->buffers; list_iter; list_iter = list_iter->next)
    {
        struct buffer_entry *entry = list_iter->data;
        struct payload_hash buffer_id;
        size_t read_size;

        if ((fozdb_read_entry_data(db, AUDIO_CONV_FOZ_TAG_STREAM, stream_id,
                offset, (uint8_t *)&buffer_id, sizeof(buffer_id), &read_size, true)) < 0
                || read_size != sizeof(buffer_id))
            return false;

        if (memcmp(&buffer_id, &entry->hash, sizeof(buffer_id)) != 0)
            return false;

        offset += sizeof(buffer_id);
    }

    GST_LOG("Stream id %s is a subset of %s, so not recording stream.",
            format_hash(&state->current_hash), format_hash(stream_id));

    return true;
}

static int stream_state_write_to_foz(struct stream_state *state)
{
    struct stream_state_serializer serializer;
    struct buffer_entry *entry;
    struct bytes_reader reader;
    uint8_t buffer[1024];
    size_t header_size;
    GList *list_iter;
    bool found;
    int ret;

    GST_DEBUG("state %p, current hash %s.", state, format_hash(&state->current_hash));

    if (!state->needs_dump || !state->buffers)
        return CONV_OK;

    pthread_mutex_lock(&dump_fozdb.mutex);

    if ((ret = dump_fozdb_open_audio(true)) < 0)
    {
        GST_ERROR("Failed to open audio dump fozdb, ret %d.", ret);
        pthread_mutex_unlock(&dump_fozdb.mutex);
        return ret;
    }

    found = fozdb_has_entry(dump_fozdb.fozdb, AUDIO_CONV_FOZ_TAG_STREAM, &state->current_hash);
    if (!found)
    {
        /* Are there any recorded streams of which this stream is a subset? */
        struct payload_hash *stream_id;
        GHashTableIter stream_ids;

        fozdb_iter_tag(dump_fozdb.fozdb, AUDIO_CONV_FOZ_TAG_STREAM, &stream_ids);
        while (g_hash_table_iter_next(&stream_ids, (void **)&stream_id, NULL))
        {
            if (stream_state_is_stream_subset(state, dump_fozdb.fozdb, stream_id))
            {
                found = true;
                break;
            }
        }
    }

    if (!found)
    {
        if (dumping_disabled())
        {
            GST_LOG("Dumping disabled, so not recording stream id %s.", format_hash(&state->current_hash));
        }
        else
        {
            GST_LOG("Recording stream id %s.", format_hash(&state->current_hash));

            need_transcode_head_serialize(state->codec_info, buffer, sizeof(buffer), &header_size);
            bytes_reader_init(&reader, buffer, header_size);
            entry = state->buffers->data;
            if ((ret = fozdb_write_entry(dump_fozdb.fozdb, AUDIO_CONV_FOZ_TAG_CODECINFO, &entry->hash,
                    &reader, bytes_reader_read, true)) < 0)
            {
                GST_ERROR("Unable to write stream header, ret %d.\n", ret);
                pthread_mutex_unlock(&dump_fozdb.mutex);
                return ret;
            }

            stream_state_serializer_init(&serializer, state);
            if ((ret = fozdb_write_entry(dump_fozdb.fozdb, AUDIO_CONV_FOZ_TAG_STREAM, &state->current_hash,
                    &serializer, stream_state_serializer_read, true)) < 0)
            {
                GST_ERROR("Unable to write stream, ret %d.\n", ret);
                pthread_mutex_unlock(&dump_fozdb.mutex);
                return ret;
            }

            for (list_iter = state->buffers; list_iter; list_iter = list_iter->next)
            {
                struct gst_buffer_reader buffer_reader;

                entry = list_iter->data;
                gst_buffer_reader_init(&buffer_reader, entry->buffer);
                if ((ret = fozdb_write_entry(dump_fozdb.fozdb, AUDIO_CONV_FOZ_TAG_AUDIODATA, &entry->hash,
                        &buffer_reader, gst_buffer_reader_read, true)) < 0)
                {
                    GST_ERROR("Unable to write audio data, ret %d.\n", ret);
                    pthread_mutex_unlock(&dump_fozdb.mutex);
                    return ret;
                }
            }
        }
    }

    pthread_mutex_unlock(&dump_fozdb.mutex);
    return CONV_OK;
}

static int audio_conv_state_create(struct audio_conv_state **out)
{
    struct audio_conv_state *state;
    struct fozdb *fozdb = NULL;
    char *read_fozdb_path;
    int ret;

    if ((read_fozdb_path = getenv("MEDIACONV_AUDIO_TRANSCODED_FILE")))
    {
        if ((ret = fozdb_create(read_fozdb_path, O_RDONLY, true /* Read-only? */,
                AUDIO_CONV_FOZ_NUM_TAGS, &fozdb)) < 0)
            GST_ERROR("Failed to create fozdb from %s, ret %d.", read_fozdb_path, ret);
    }
    else
    {
        GST_ERROR("Env MEDIACONV_AUDIO_TRANSCODED_FILE is not set!");
        ret = CONV_ERROR_ENV_NOT_SET;
    }

    state = calloc(1, sizeof(*state));
    murmur3_128_state_init(&state->hash_state, HASH_SEED);
    murmur3_128_state_init(&state->loop_hash_state, HASH_SEED);
    state->stream_state = stream_state_create();
    state->read_fozdb = fozdb;

    *out = state;
    return ret;
}

static void audio_conv_state_release(struct audio_conv_state *state)
{
    free(state->codec_data);
    stream_state_release(state->stream_state);
    if (state->read_fozdb)
        fozdb_release(state->read_fozdb);
    free(state);
}

static void audio_conv_state_reset(struct audio_conv_state *state)
{
    if (stream_state_write_to_foz(state->stream_state) < 0)
        GST_ERROR("Failed to write stream to dump fozdb.");

    stream_state_reset(state->stream_state);
    murmur3_128_state_reset(&state->hash_state);
    murmur3_128_state_reset(&state->loop_hash_state);
}

/* Allocate a buffer on success, free it after usage. */
static int audio_conv_state_open_transcode_file(struct audio_conv_state *state, GstBuffer *buffer,
        uint8_t **out_data, size_t *out_size)
{
    struct payload_hash hash, loop_hash;
    uint32_t transcoded_size;
    const char *blank_audio;
    bool try_loop, hash_ok;
    GstMapInfo map_info;
    uint64_t file_size;
    loop_state loop;
    uint8_t *data;
    size_t size;
    int fd;

    /* Hash buffer. */
    if (!gst_buffer_map(buffer, &map_info, GST_MAP_READ))
        return CONV_ERROR;
    hash_ok = hash_data(map_info.data, map_info.size, &state->hash_state, &hash)
            && hash_data(map_info.data, map_info.size, &state->loop_hash_state, &loop_hash);
    gst_buffer_unmap(buffer, &map_info);
    if (!hash_ok)
    {
        GST_ERROR("Failed to hash buffer.");
        return CONV_ERROR;
    }

    loop = stream_state_record_buffer(state->stream_state, &hash, &loop_hash, buffer, state->codec_data);
    gst_buffer_unref(buffer); /* Buffer has been recorded, so unref it. */
    switch (loop)
    {
    case NO_LOOP:
        murmur3_128_state_reset(&state->loop_hash_state);
        try_loop = false;
        break;

    case LOOP_ENDED:
        murmur3_128_state_reset(&state->loop_hash_state);
    case LOOPING:
        try_loop = true;
        break;

    case LOOP_ERROR:
    default:
        return CONV_ERROR;
    }

    if (try_loop)
        GST_INFO("Buffer hash: %s (Loop: %s).", format_hash(&hash), format_hash(&loop_hash));
    else
        GST_INFO("Buffer hash: %s.", format_hash(&hash));

    /* Try to read transcoded data. */
    if (state->read_fozdb)
    {
        if (fozdb_entry_size(state->read_fozdb,
                AUDIO_CONV_FOZ_TAG_PTNADATA, &hash, &transcoded_size) == CONV_OK)
        {
            data = calloc(1, transcoded_size);
            if (fozdb_read_entry_data(state->read_fozdb, AUDIO_CONV_FOZ_TAG_PTNADATA, &hash, 0,
                    data, transcoded_size, &size, false) == CONV_OK)
            {
                *out_data = data;
                *out_size = size;
                return CONV_OK;
            }
            free(data);
        }

        if (try_loop && fozdb_entry_size(state->read_fozdb,
                AUDIO_CONV_FOZ_TAG_PTNADATA, &loop_hash, &transcoded_size) == CONV_OK)
        {
            data = calloc(1, transcoded_size);
            if (fozdb_read_entry_data(state->read_fozdb, AUDIO_CONV_FOZ_TAG_PTNADATA, &loop_hash, 0,
                    data, transcoded_size, &size, false) == CONV_OK)
            {
                *out_data = data;
                *out_size = size;
                return CONV_OK;
            }
            free(data);
        }
    }

    /* If we can't, return the blank file */
    state->stream_state->needs_dump = true;
    if (!(blank_audio = getenv("MEDIACONV_BLANK_AUDIO_FILE")))
    {
        GST_ERROR("Env MEDIACONV_BLANK_AUDIO_FILE not set.");
        return CONV_ERROR_ENV_NOT_SET;
    }
    if (!open_file(blank_audio, O_RDONLY, &fd))
        return CONV_ERROR_OPEN_FAILED;
    if (!get_file_size(fd, &file_size))
    {
        close(fd);
        return CONV_ERROR;
    }
    data = calloc(1, file_size);
    if (!complete_read(fd, data, file_size))
    {
        free(data);
        close(fd);
        return CONV_ERROR_READ_FAILED;
    }

    create_placeholder_file("placeholder-audio-used");

    *out_data = data;
    *out_size = file_size;

    return CONV_OK;
}

/* Call pthread_mutex_unlock() to unlock after usage. */
static struct audio_conv_state *audio_conv_lock_state(AudioConv *conv)
{
    pthread_mutex_lock(&conv->state_mutex);
    if (!conv->state)
        pthread_mutex_unlock(&conv->state_mutex);
    return conv->state;
}

static GstStateChangeReturn audio_conv_change_state(GstElement *element, GstStateChange transition)
{
    AudioConv *conv = AUDIO_CONV(element);
    struct audio_conv_state *state;
    int ret;

    GST_DEBUG_OBJECT(element, "State transition %s.", gst_state_change_get_name(transition));

    switch (transition)
    {
    case GST_STATE_CHANGE_NULL_TO_READY:
        /* Do runtime setup. */

        /* Open fozdb here; this is the right place to fail
        * and opening may be expensive. */
        pthread_mutex_lock(&dump_fozdb.mutex);
        dump_fozdb_discard_transcoded();
        ret = dump_fozdb_open_audio(true);
        pthread_mutex_unlock(&dump_fozdb.mutex);
        if (ret < 0)
        {
            GST_ERROR("Failed to open dump fozdb, ret %d.", ret);
            return GST_STATE_CHANGE_FAILURE;
        }

        /* Create audio conv state. */
        if ((ret = audio_conv_state_create(&state)) < 0)
            GST_ERROR("Failed to create audio conv state, ret %d.", ret);
        pthread_mutex_lock(&conv->state_mutex);
        assert(!conv->state);
        conv->state = state;
        pthread_mutex_unlock(&conv->state_mutex);
        break;

    case GST_STATE_CHANGE_READY_TO_NULL:
        /* Do runtime teardown. */
        pthread_mutex_lock(&conv->state_mutex);
        state = conv->state;
        conv->state = NULL;
        pthread_mutex_unlock(&conv->state_mutex);

        if (state && (ret = stream_state_write_to_foz(state->stream_state)) < 0)
            GST_WARNING("Error writing out stream data, ret %d.", ret);
        audio_conv_state_release(state);
        break;

    default:
        break;
    }

    return GST_ELEMENT_CLASS(parent_class)->change_state(element, transition);

    /* XXX on ReadyToNull, sodium drops state _again_ here... why? */
}

static gboolean audio_conv_sink_event(GstPad *pad, GstObject *parent, GstEvent *event)
{
    AudioConv *conv = AUDIO_CONV(parent);
    struct audio_conv_state *state;
    GstCaps *caps;
    bool ret;

    GST_DEBUG_OBJECT(pad, "Got sink event %"GST_PTR_FORMAT".", event);

    switch (event->type)
    {
    case GST_EVENT_CAPS:
        if ((state = audio_conv_lock_state(conv)))
        {
            gst_event_parse_caps(event, &caps);
            if (!need_transcode_head_create_from_caps(caps, &state->codec_data))
            {
                GST_ERROR("Invalid WMA caps!");
                pthread_mutex_unlock(&conv->state_mutex);
                return false;
            }
            pthread_mutex_unlock(&conv->state_mutex);
        }

        caps = gst_caps_from_string("audio/x-opus, channel-mapping-family=0");
        ret = push_event(conv->src_pad, gst_event_new_caps(caps));
        gst_caps_unref(caps);
        return ret;

    case GST_EVENT_FLUSH_STOP:
        if ((state = audio_conv_lock_state(conv)))
        {
            audio_conv_state_reset(state);
            pthread_mutex_unlock(&conv->state_mutex);
        }
        return gst_pad_event_default(pad, parent, event);

    default:
        return gst_pad_event_default(pad, parent, event);
    }
}

static GstFlowReturn audio_conv_chain(GstPad *pad, GstObject *parent, GstBuffer *buffer)
{
    size_t ptna_data_size, offset, encoded_len;
    GstFlowReturn flow_ret = GST_FLOW_ERROR;
    AudioConv *conv = AUDIO_CONV(parent);
    struct audio_conv_state *state;
    uint8_t *ptna_data = NULL;
    int ret;

    GST_LOG_OBJECT(pad, "Handling buffer <%"GST_PTR_FORMAT">.", buffer);

    if (!(state = audio_conv_lock_state(conv)))
        return flow_ret;

    if ((ret = audio_conv_state_open_transcode_file(state, buffer, &ptna_data, &ptna_data_size)) < 0)
    {
        GST_ERROR("Failed to read transcoded audio, ret %d. Things will go badly...", ret);
        goto done;
    }

    for (offset = 0; offset < ptna_data_size; offset += encoded_len)
    {
        uint32_t packet_header, flags, padding_len;
        GstBuffer *new_buffer;
        bool packet_is_header;

        if (offset + 4 >= ptna_data_size)
        {
            GST_WARNING( "Short read on ptna header?");
            break;
        }

        packet_header = bytes_to_uint32(&ptna_data[offset]);
        offset += 4;

        flags = packet_header & AUDIO_CONV_FLAG_MASK,
        padding_len = (packet_header & AUDIO_CONV_PADDING_LENGTH_MASK) >> AUDIO_CONV_PADDING_LENGTH_SHIFT;
        encoded_len = packet_header & AUDIO_CONV_ENCODED_LENGTH_MASK;

        if (offset + encoded_len > ptna_data_size)
        {
            GST_WARNING("Short read on ptna data?");
            break;
        }

        packet_is_header = flags & AUDIO_CONV_FLAG_HEADER;
        if (packet_is_header && state->sent_header)
            continue; /* Only send one header. */

        /* TODO: can we use a GstBuffer cache here? */
        new_buffer = gst_buffer_new_and_alloc(encoded_len);
        if (!packet_is_header && padding_len > 0)
            gst_buffer_add_audio_clipping_meta(new_buffer, GST_FORMAT_DEFAULT, 0, padding_len);
        gst_buffer_fill(new_buffer, 0, ptna_data + offset, encoded_len);

        GST_LOG("Pushing one packet of len %zu.", encoded_len);
        if ((flow_ret = gst_pad_push(conv->src_pad, new_buffer)) < 0)
        {
            GST_ERROR("Failed to push buffer <%"GST_PTR_FORMAT"> to src pad %"GST_PTR_FORMAT,
                    new_buffer, conv->src_pad);
            goto done;
        }

        if (packet_is_header)
            state->sent_header = true;
    }

    flow_ret = GST_FLOW_OK;

done:
    if (ptna_data)
        free(ptna_data);
    pthread_mutex_unlock(&conv->state_mutex);
    return flow_ret;
}

static gboolean audio_conv_src_query(GstPad *pad, GstObject *parent, GstQuery *query)
{
    AudioConv *conv = AUDIO_CONV(parent);
    GstSchedulingFlags flags;
    gint min, max, align;
    GstQuery *peer_query;

    GST_DEBUG_OBJECT(pad, "Got query %"GST_PTR_FORMAT".", query);

    switch (query->type)
    {
    case GST_QUERY_SCHEDULING:
        peer_query = gst_query_new_scheduling();
        if (!gst_pad_peer_query(conv->sink_pad, peer_query))
        {
            gst_query_unref(peer_query);
            return false;
        }
        gst_query_parse_scheduling(peer_query, &flags, &min, &max, &align);
        gst_query_unref(peer_query);
        gst_query_set_scheduling(query, flags, min, max, align);
        return true;

    default:
        return gst_pad_query_default(pad, parent, query);
    }

}

static gboolean audio_conv_active_mode(GstPad *pad, GstObject *parent, GstPadMode mode, gboolean active)
{
    AudioConv *conv = AUDIO_CONV(parent);
    return gst_pad_activate_mode(conv->sink_pad, mode, active);
}

static void audio_conv_finalize(GObject *object)
{
    AudioConv *conv = AUDIO_CONV(object);

    pthread_mutex_destroy(&conv->state_mutex);
    if (conv->state)
        audio_conv_state_release(conv->state);

    G_OBJECT_CLASS(parent_class)->finalize(object);
}

static void audio_conv_class_init(AudioConvClass *klass)
{
    GstElementClass *element_class = GST_ELEMENT_CLASS(klass);
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    gst_element_class_set_metadata(element_class,
            "Proton audio converter",
            "Codec/Demuxer",
            "Converts audio for Proton",
            "Andrew Eikum <aeikum@codeweavers.com>, Ziqing Hui <zhui@codeweavers.com>");

    element_class->change_state = audio_conv_change_state;
    object_class->finalize = audio_conv_finalize;

    gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&audio_conv_sink_template));
    gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&audio_conv_src_template));
}

static void audio_conv_init(AudioConv *conv)
{
    GstElement *element = GST_ELEMENT(conv);

    conv->sink_pad = gst_pad_new_from_static_template(&audio_conv_sink_template, "sink");
    gst_pad_set_event_function(conv->sink_pad, GST_DEBUG_FUNCPTR(audio_conv_sink_event));
    gst_pad_set_chain_function(conv->sink_pad, GST_DEBUG_FUNCPTR(audio_conv_chain));
    gst_element_add_pad(element, conv->sink_pad);

    conv->src_pad = gst_pad_new_from_static_template(&audio_conv_src_template, "src");
    gst_pad_set_query_function(conv->src_pad, GST_DEBUG_FUNCPTR(audio_conv_src_query));
    gst_pad_set_activatemode_function(conv->src_pad, GST_DEBUG_FUNCPTR(audio_conv_active_mode));
    gst_element_add_pad(element, conv->src_pad);

    pthread_mutex_init(&conv->state_mutex, NULL);
    conv->state = NULL;
}
