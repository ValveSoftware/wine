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
 * Nicely, both Quartz and Media Foundation allow us random access to the entire data stream. So we
 * can easily hash the entire incoming stream and substitute it with our Ogg Theora video. If there
 * is a cache miss, then we dump the entire incoming stream. In case of a cache hit, we dump
 * nothing.
 *
 * Incoming video data is stored in the video.foz Fossilize database.
 *
 * Transcoded video data is stored in the transcoded_video.foz Fossilize database.
 *
 *
 * Hashing algorithm
 * -----------------
 *
 * We use murmur3 hash with the seed given below. We use the x32 variant for 32-bit programs, and
 * the x64 variant for 64-bit programs.
 *
 * For speed when hashing, we specify a stride which will skip over chunks of the input. However,
 * we will always hash the first "stride" number of bytes, to try to avoid collisions on smaller
 * files with size between chunk and stride.
 *
 * For example, the 'H's below are hashed, the 'x's are skipped:
 *
 * int chunk = 4;
 * int stride = chunk * 3;
 * H = hashed, x = skipped
 * [HHHH HHHH HHHH HHHH xxxx xxxx HHHH xxxx xxxx HHHH xxxx] < data stream
 *  ^^^^ ^^^^ ^^^^ stride prefix, hashed
 *                 ^^^^ chunk
 *                 ^^^^ ^^^^ ^^^^ stride
 *                                ^^^^ chunk
 *                                ^^^^ ^^^^ ^^^^ stride
 *                                               ^^^^ chunk
 *                                               ^^^^ ^^^^ stride
 */

#if 0
#pragma makedep unix
#endif

#include "media-converter.h"

#include <assert.h>

#define HASH_CHUNK_SIZE (8 * 1024 * 1024) /* 8 MB. */
#define HASH_STRIDE     (HASH_CHUNK_SIZE * 6)

#define VIDEO_CONV_FOZ_TAG_VIDEODATA 0
#define VIDEO_CONV_FOZ_TAG_OGVDATA   1
#define VIDEO_CONV_FOZ_TAG_STREAM    2
#define VIDEO_CONV_FOZ_TAG_MKVDATA   3
#define VIDEO_CONV_FOZ_NUM_TAGS      4

#define DURATION_NONE (UINT64_MAX)

struct pad_reader
{
    GstPad *pad;
    size_t offset;
    uint8_t *chunk;
    size_t chunk_offset;
    size_t chunk_end;
    size_t stride; /* Set to SIZE_MAX to skip no bytes. */
};

struct hashes_reader
{
    GList *current_hash;
};

struct video_conv_state
{
    struct payload_hash transcode_hash;
    struct fozdb *read_fozdb;
    uint64_t upstream_duration;
    uint64_t our_duration;
    uint32_t transcoded_tag;
    bool has_transcoded, need_stream_start;
};

typedef struct
{
    GstElement element;
    GstPad *sink_pad, *src_pad;
    pthread_mutex_t state_mutex;
    struct video_conv_state *state;
} VideoConv;

typedef struct
{
    GstElementClass class;
} VideoConvClass;

G_DEFINE_TYPE(VideoConv, video_conv, GST_TYPE_ELEMENT);
#define VIDEO_CONV_TYPE (video_conv_get_type())
#define VIDEO_CONV(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), VIDEO_CONV_TYPE, VideoConv))
#define parent_class    (video_conv_parent_class)
GST_ELEMENT_REGISTER_DEFINE(protonvideoconverter, "protonvideoconverter",
        GST_RANK_MARGINAL, VIDEO_CONV_TYPE);

static GstStaticPadTemplate video_conv_sink_template = GST_STATIC_PAD_TEMPLATE("sink",
        GST_PAD_SINK, GST_PAD_ALWAYS,
        GST_STATIC_CAPS("video/x-ms-asf; video/x-msvideo; video/mpeg; video/quicktime;"));

static GstStaticPadTemplate video_conv_src_template = GST_STATIC_PAD_TEMPLATE("src",
        GST_PAD_SRC, GST_PAD_ALWAYS,
        GST_STATIC_CAPS("video/x-matroska; application/ogg;"));

static struct dump_fozdb dump_fozdb = {PTHREAD_MUTEX_INITIALIZER, NULL, false};

void hashes_reader_init(struct hashes_reader *reader, GList *hashes)
{
    reader->current_hash = hashes;
}

static int hashes_reader_read(void *reader, uint8_t *buffer, size_t size, size_t *read_size)
{
    struct payload_hash *hash = (struct payload_hash *)buffer;
    struct hashes_reader *hashes_reader = reader;

    if (!size)
    {
        *read_size = 0;
        return CONV_OK;
    }

    if (!hashes_reader->current_hash)
        return CONV_ERROR_DATA_END;

    *hash = *(struct payload_hash *)(hashes_reader->current_hash->data);
    hashes_reader->current_hash = hashes_reader->current_hash->next;

    *read_size = sizeof(*hash);
    return CONV_OK;
}

static int dump_fozdb_open_video(bool create)
{
    return dump_fozdb_open(&dump_fozdb, create, "MEDIACONV_VIDEO_DUMP_FILE", VIDEO_CONV_FOZ_NUM_TAGS);
}

static void dump_fozdb_discard_transcoded(void)
{
    GList *to_discard_chunks = NULL;
    struct payload_hash *stream_id;
    struct fozdb *read_fozdb;
    char *read_fozdb_path;
    GHashTableIter iter;
    int ret;

    if (dump_fozdb.already_cleaned)
        return;
    dump_fozdb.already_cleaned = true;

    if (discarding_disabled())
        return;
    if (!file_exists(getenv("MEDIACONV_VIDEO_DUMP_FILE")))
        return;

    if (dump_fozdb_open_video(false) < 0)
        return;

    if (!(read_fozdb_path = getenv("MEDIACONV_VIDEO_TRANSCODED_FILE")))
    {
        GST_ERROR("Env MEDIACONV_VIDEO_TRANSCODED_FILE not set.");
        return;
    }

    if ((ret = fozdb_create(read_fozdb_path, O_RDONLY, true /* Read-only?  */, VIDEO_CONV_FOZ_NUM_TAGS, &read_fozdb)) < 0)
    {
        GST_ERROR("Failed to create read fozdb, ret %d.", ret);
        return;
    }

    fozdb_iter_tag(dump_fozdb.fozdb, VIDEO_CONV_FOZ_TAG_STREAM, &iter);
    while (g_hash_table_iter_next(&iter, (void **)&stream_id, NULL))
    {
        struct payload_hash chunk_id;
        uint32_t chunks_size, i;
        size_t read_size;

        if (fozdb_has_entry(read_fozdb, VIDEO_CONV_FOZ_TAG_OGVDATA, stream_id))
        {
            if (fozdb_entry_size(dump_fozdb.fozdb, VIDEO_CONV_FOZ_TAG_STREAM, stream_id, &chunks_size) == CONV_OK)
            {
                uint8_t *buffer = calloc(1, chunks_size);
                if (fozdb_read_entry_data(dump_fozdb.fozdb, VIDEO_CONV_FOZ_TAG_STREAM, stream_id,
                        0, buffer, chunks_size, &read_size, true) == CONV_OK)
                {
                    for (i = 0; i < read_size / sizeof(chunk_id); ++i)
                    {
                        payload_hash_from_bytes(&chunk_id, buffer + i * sizeof(chunk_id));
                        to_discard_chunks = g_list_append(to_discard_chunks,
                                entry_name_create(VIDEO_CONV_FOZ_TAG_VIDEODATA, &chunk_id));
                    }
                }
                free(buffer);
            }

            to_discard_chunks = g_list_append(to_discard_chunks,
                    entry_name_create(VIDEO_CONV_FOZ_TAG_STREAM, stream_id));
        }
    }

    if ((ret = fozdb_discard_entries(dump_fozdb.fozdb, to_discard_chunks)) < 0)
    {
        GST_ERROR("Failed to discard entries, ret %d.", ret);
        dump_fozdb_close(&dump_fozdb);
    }

    g_list_free_full(to_discard_chunks, free);
}

struct pad_reader *pad_reader_create_with_stride(GstPad *pad, size_t stride)
{
    struct pad_reader *pad_reader;

    pad_reader = calloc(1, sizeof(*pad_reader));
    pad_reader->chunk = calloc(HASH_CHUNK_SIZE, sizeof(*pad_reader->chunk));
    pad_reader->stride = stride;
    gst_object_ref((pad_reader->pad = pad));

    return pad_reader;
}

struct pad_reader *pad_reader_create(GstPad *pad)
{
    return pad_reader_create_with_stride(pad, SIZE_MAX);
}

void pad_reader_release(struct pad_reader *reader)
{
    gst_object_unref(reader->pad);
    free(reader->chunk);
    free(reader);
}

int pad_reader_read(void *data_src, uint8_t *buffer, size_t size, size_t *read_size)
{
    struct pad_reader *reader = data_src;
    GstBuffer *gst_buffer = NULL;
    GstFlowReturn gst_ret;
    size_t to_copy;

    if (!size)
    {
        *read_size = 0;
        return CONV_OK;
    }

    if (reader->chunk_offset >= reader->chunk_end)
    {
        reader->chunk_offset = 0;
        reader->chunk_end = 0;

        if ((gst_ret = gst_pad_pull_range(reader->pad,
                reader->offset, HASH_CHUNK_SIZE, &gst_buffer)) == GST_FLOW_OK)
        {
            gsize buffer_size = gst_buffer_get_size(gst_buffer);

            if (reader->offset + buffer_size < reader->stride)
            {
                to_copy = buffer_size;
                reader->offset += to_copy;
            }
            else if (reader->offset < reader->stride)
            {
                to_copy = reader->stride - reader->offset;
                reader->offset = reader->stride;
            }
            else
            {
                to_copy = buffer_size;
                reader->offset += reader->stride;
            }

            if (size >= to_copy) /* Copy directly into out buffer and return. */
            {
                *read_size = gst_buffer_extract(gst_buffer, 0, buffer, to_copy);
                gst_buffer_unref(gst_buffer);
                return CONV_OK;
            }
            else
            {
                reader->chunk_end = gst_buffer_extract(gst_buffer, 0, reader->chunk, to_copy);
                gst_buffer_unref(gst_buffer);
            }
        }
        else if (gst_ret == GST_FLOW_EOS)
        {
            return CONV_ERROR_DATA_END;
        }
        else
        {
            GST_ERROR("Failed to pull data from %"GST_PTR_FORMAT", reason %s.",
                    reader->pad, gst_flow_get_name(gst_ret));
            return CONV_ERROR;
        }
    }

    /* Copy chunk data to output buffer. */
    to_copy = min(reader->chunk_end - reader->chunk_offset, size);
    memcpy(buffer, reader->chunk + reader->chunk_offset, to_copy);
    reader->chunk_offset += to_copy;

    *read_size = to_copy;
    return CONV_OK;
}

static int video_conv_state_create(struct video_conv_state **out)
{
    struct video_conv_state *state;
    struct fozdb *fozdb = NULL;
    char *read_fozdb_path;
    int ret;

    if ((read_fozdb_path = getenv("MEDIACONV_VIDEO_TRANSCODED_FILE")))
    {
        if ((ret = fozdb_create(read_fozdb_path, O_RDONLY, true /* Read-only? */,
                VIDEO_CONV_FOZ_NUM_TAGS, &fozdb)) < 0)
            GST_ERROR("Failed to create read fozdb from %s, ret %d.", read_fozdb_path, ret);
    }
    else
    {
        GST_ERROR("Env MEDIACONV_VIDEO_TRANSCODED_FILE is not set.");
        ret = CONV_ERROR_ENV_NOT_SET;
    }

    state = calloc(1, sizeof(*state));
    state->read_fozdb = fozdb;
    state->upstream_duration = DURATION_NONE;
    state->our_duration = DURATION_NONE;
    state->transcoded_tag = VIDEO_CONV_FOZ_TAG_MKVDATA;
    state->need_stream_start = true;

    *out = state;
    return ret;
}

static void video_conv_state_release(struct video_conv_state *state)
{
    if (state->read_fozdb)
        fozdb_release(state->read_fozdb);
    free(state);
}

/* Return true if the file is transcoded, false if not. */
bool video_conv_state_begin_transcode(struct video_conv_state *state, struct payload_hash *hash)
{
    const char *blank_video;
    uint64_t file_size = 0;
    int fd;

    GST_DEBUG("state %p, hash %s.", state, format_hash(hash));

    if (state->read_fozdb)
    {
        uint32_t entry_size;

        if (fozdb_entry_size(state->read_fozdb, VIDEO_CONV_FOZ_TAG_MKVDATA, hash, &entry_size) == CONV_OK)
        {
            GST_DEBUG("Found an MKV video for hash %s.", format_hash(hash));
            state->transcode_hash = *hash;
            state->our_duration = entry_size;
            state->transcoded_tag = VIDEO_CONV_FOZ_TAG_MKVDATA;
            state->has_transcoded = true;
            return true;
        }

        if (fozdb_entry_size(state->read_fozdb, VIDEO_CONV_FOZ_TAG_OGVDATA, hash, &entry_size) == CONV_OK)
        {
            GST_DEBUG("Found an OGV video for hash %s.", format_hash(hash));
            state->transcode_hash = *hash;
            state->our_duration = entry_size;
            state->transcoded_tag = VIDEO_CONV_FOZ_TAG_OGVDATA;
            state->has_transcoded = true;
            return true;
        }
    }

    GST_INFO("No transcoded video for %s. Substituting a blank video.", format_hash(hash));

    if (!(blank_video = getenv("MEDIACONV_BLANK_VIDEO_FILE")))
    {
        GST_ERROR("Env MEDIACONV_BLANK_VIDEO_FILE not set.");
        return false;
    }
    if (open_file(blank_video, O_RDONLY, &fd))
    {
        get_file_size(fd, &file_size);
        close(fd);
    }
    state->our_duration = file_size;
    state->has_transcoded = false;

    create_placeholder_file("placeholder-video-used");

    return false;
}

int video_conv_state_fill_buffer(struct video_conv_state *state, uint64_t offset,
        uint8_t *buffer, size_t size, size_t *fill_size)
{
    const char *blank_video;
    uint64_t file_size;
    size_t to_copy;
    bool read_ok;
    int fd, ret;

    if (state->has_transcoded)
    {
        if ((ret = fozdb_read_entry_data(state->read_fozdb, state->transcoded_tag, &state->transcode_hash,
                offset, buffer, size, fill_size, false)) < 0)
            GST_ERROR("Failed to read entry data, ret %d.", ret);
        return ret;
    }
    else /* Fill blank video data to buffer. */
    {
        if (!(blank_video = getenv("MEDIACONV_BLANK_VIDEO_FILE")))
        {
            GST_ERROR("Env MEDIACONV_BLANK_VIDEO_FILE not set.");
            return CONV_ERROR_ENV_NOT_SET;
        }
        if (!open_file(blank_video, O_RDONLY, &fd))
            return CONV_ERROR_OPEN_FAILED;
        if (!get_file_size(fd, &file_size))
        {
            close(fd);
            return CONV_ERROR;
        }

        /* Get copy size. */
        if (offset >= file_size)
        {
            close(fd);
            return CONV_OK;
        }
        to_copy = min(file_size - offset, size);

        /* Copy data. */
        if (lseek(fd, offset, SEEK_SET) < 0)
        {
            GST_ERROR("Failed to seek %s to %#"PRIx64". %s.", blank_video, offset, strerror(errno));
            close(fd);
            return CONV_ERROR;
        }
        read_ok = complete_read(fd, buffer, to_copy);
        close(fd);

        if (!read_ok)
        {
            GST_ERROR("Failed to read blank video data.");
            return CONV_ERROR_READ_FAILED;
        }
        *fill_size = to_copy;
        return CONV_OK;
    }
}

/* Call pthread_mutex_unlock() to unlock after usage. */
static struct video_conv_state *video_conv_lock_state(VideoConv *conv)
{
    pthread_mutex_lock(&conv->state_mutex);
    if (!conv->state)
        pthread_mutex_unlock(&conv->state_mutex);
    return conv->state;
}

static GstStateChangeReturn video_conv_change_state(GstElement *element, GstStateChange transition)
{
    VideoConv *conv = VIDEO_CONV(element);
    struct video_conv_state *state = NULL;
    int ret;

    GST_INFO_OBJECT(element, "State transition: %s.", gst_state_change_get_name(transition));

    switch (transition)
    {
    case GST_STATE_CHANGE_NULL_TO_READY:
        /* Do runtime setup. */
        if ((ret = video_conv_state_create(&state)) < 0)
            GST_ERROR("Failed to create video conv state, ret %d.", ret);
        pthread_mutex_lock(&conv->state_mutex);
        assert(!conv->state);
        conv->state = state;
        pthread_mutex_unlock(&conv->state_mutex);
        break;

    case GST_STATE_CHANGE_READY_TO_NULL:
        /* Do runtime teardown. */
        pthread_mutex_lock(&conv->state_mutex);
        video_conv_state_release(conv->state);
        conv->state = NULL;
        pthread_mutex_unlock(&conv->state_mutex);
        break;

    default:
        break;
    }

    return GST_ELEMENT_CLASS(parent_class)->change_state(element, transition);

    /* XXX on ReadyToNull, sodium drops state _again_ here... why? */
}

static uint64_t video_conv_duration_ours_to_upstream(VideoConv *conv, uint64_t pos)
{
    struct video_conv_state *state = conv->state;

    if (state->upstream_duration != DURATION_NONE && state->our_duration != DURATION_NONE)
        return pos * state->upstream_duration / state->our_duration;
    else
        return DURATION_NONE;
}

static void video_conv_query_upstream_duration(VideoConv *conv)
{
    gint64 duration;

    if (gst_pad_peer_query_duration(conv->sink_pad, GST_FORMAT_BYTES, &duration))
        conv->state->upstream_duration = duration;
    else
        GST_ERROR_OBJECT(conv, "Failed to query upstream duration.");
}

static bool video_conv_get_upstream_range(VideoConv *conv, uint64_t offset, uint32_t requested_size,
        uint64_t *upstream_offset, uint64_t *upstream_requested_size)
{
    struct video_conv_state *state;

    if (!(state = video_conv_lock_state(conv)))
        return false;

    if (state->upstream_duration == DURATION_NONE)
        video_conv_query_upstream_duration(conv);

    *upstream_offset = video_conv_duration_ours_to_upstream(conv, offset);
    *upstream_requested_size = video_conv_duration_ours_to_upstream(conv, requested_size);

    pthread_mutex_unlock(&conv->state_mutex);

    return true;
}

static bool video_conv_hash_upstream_data(VideoConv *conv, struct payload_hash *hash)
{
    struct pad_reader *reader;
    bool ret;

    memset(hash, 0, sizeof(*hash));

    reader = pad_reader_create_with_stride(conv->sink_pad, HASH_STRIDE);
    ret = murmur3_128(reader, pad_reader_read, HASH_SEED, hash);
    pad_reader_release(reader);

    return ret;
}

static int video_conv_dump_upstream_data(VideoConv *conv, struct payload_hash *hash)
{
    struct hashes_reader chunk_hashes_reader;
    struct pad_reader *pad_reader = NULL;
    GList *chunk_hashes = NULL;
    uint8_t *buffer = NULL;
    size_t read_size;
    int ret;

    GST_DEBUG("Dumping upstream data, hash %s.", format_hash(hash));

    if ((ret = dump_fozdb_open_video(true)) < 0)
    {
        GST_ERROR("Failed to open video dump fozdb, ret %d.", ret);
        goto done;
    }

    buffer = calloc(1, HASH_CHUNK_SIZE);
    pad_reader = pad_reader_create(conv->sink_pad);
    while ((ret = pad_reader_read(pad_reader, buffer, HASH_CHUNK_SIZE, &read_size)) == CONV_OK)
    {
        struct bytes_reader bytes_reader;
        struct payload_hash *chunk_hash;

        bytes_reader_init(&bytes_reader, buffer, read_size);
        chunk_hash = calloc(1, sizeof(*chunk_hash));
        murmur3_128(&bytes_reader, bytes_reader_read, HASH_SEED, chunk_hash);
        chunk_hashes = g_list_append(chunk_hashes, chunk_hash);

        bytes_reader_init(&bytes_reader, buffer, read_size);
        if ((ret = fozdb_write_entry(dump_fozdb.fozdb, VIDEO_CONV_FOZ_TAG_VIDEODATA, chunk_hash,
                &bytes_reader, bytes_reader_read, true)) < 0)
        {
            GST_ERROR("Error writing video data to fozdb, ret %d.", ret);
            goto done;
        }
    }

    if (ret != CONV_ERROR_DATA_END)
    {
        GST_ERROR("Failed to read data from pad reader, ret %d.", ret);
        goto done;
    }

    hashes_reader_init(&chunk_hashes_reader, chunk_hashes);
    if ((ret = fozdb_write_entry(dump_fozdb.fozdb, VIDEO_CONV_FOZ_TAG_STREAM, hash,
            &chunk_hashes_reader, hashes_reader_read, true)) < 0)
        GST_ERROR("Error writing stream data to fozdb, ret %d.", ret);

done:
    if (chunk_hashes)
        g_list_free_full(chunk_hashes, free);
    if (pad_reader)
        pad_reader_release(pad_reader);
    if (buffer)
        free(buffer);
    return ret;
}

static void video_conv_init_transcode(VideoConv *conv)
{
    struct video_conv_state *state = conv->state;
    struct payload_hash hash;
    int ret;

    if (state->has_transcoded)
        return;

    pthread_mutex_lock(&dump_fozdb.mutex);

    dump_fozdb_discard_transcoded();

    if (video_conv_hash_upstream_data(conv, &hash))
    {
        GST_INFO("Got upstream data hash: %s.", format_hash(&hash));
        if (!video_conv_state_begin_transcode(state, &hash)
                && (ret = video_conv_dump_upstream_data(conv, &hash)) < 0)
            GST_ERROR("Failed to dump upstream data, ret %d.", ret);
    }
    else
    {
        GST_ERROR("Failed to hash upstream data.");
    }

    pthread_mutex_unlock(&dump_fozdb.mutex);
}

static gboolean video_conv_sink_event(GstPad *pad, GstObject *parent, GstEvent *event)
{
    VideoConv *conv = VIDEO_CONV(parent);
    bool ret;

    GST_DEBUG_OBJECT(pad, "Got event %"GST_PTR_FORMAT".", event);

    if (event->type == GST_EVENT_CAPS)
    {
        struct video_conv_state *state;
        uint32_t transcode_tag;
        GstCaps *caps;

        /* push_event, below, can also grab state and cause a deadlock, so make sure it's
         * unlocked before calling */
        if (!(state = video_conv_lock_state(conv)))
        {
            GST_ERROR("VideoConv not yet in READY state?");
            return false;
        }

        if (!gst_pad_activate_mode(conv->sink_pad, GST_PAD_MODE_PULL, true))
        {
            GST_ERROR("Failed to activate sink pad in pull mode.");
            pthread_mutex_unlock(&conv->state_mutex);
            return false;
        }

        video_conv_init_transcode(conv);
        transcode_tag = state->transcoded_tag;

        pthread_mutex_unlock(&conv->state_mutex);

        if (transcode_tag == VIDEO_CONV_FOZ_TAG_MKVDATA)
            caps = gst_caps_from_string("video/x-matroska");
        else if (transcode_tag == VIDEO_CONV_FOZ_TAG_OGVDATA)
            caps = gst_caps_from_string("application/ogg");
        else
            return false;

        ret = push_event(conv->src_pad, gst_event_new_caps(caps));
        gst_caps_unref(caps);
        return ret;
    }

    return gst_pad_event_default(pad, parent, event);
}

static GstFlowReturn video_conv_src_get_range(GstPad *pad, GstObject *parent,
        guint64 offset, guint request_size, GstBuffer **buffer)
{
    GstBuffer *my_buffer, *upstream_buffer = NULL, *new_buffer = NULL;
    uint64_t upstream_offset, upstream_request_size;
    GstFlowReturn flow_ret = GST_FLOW_ERROR;
    VideoConv *conv = VIDEO_CONV(parent);
    struct video_conv_state *state;
    size_t fill_size;
    GstMapInfo map;
    int ret;

    if (!video_conv_get_upstream_range(conv, offset, request_size, &upstream_offset, &upstream_request_size))
        return flow_ret;

    if (!(state = video_conv_lock_state(conv)))
        return flow_ret;

    /* Read and ignore upstream bytes. */
    if ((flow_ret = gst_pad_pull_range(conv->sink_pad, upstream_offset, upstream_request_size, &upstream_buffer)) < 0)
    {
        GST_ERROR("Failed to pull upstream data from %"GST_PTR_FORMAT", offset %#"PRIx64", size %#"PRIx64", reason %s.",
                conv->sink_pad, upstream_offset, upstream_request_size, gst_flow_get_name(flow_ret));
        goto done;
    }
    gst_buffer_unref(upstream_buffer);

    /* Allocate and map buffer. */
    my_buffer = *buffer;
    if (!my_buffer)
    {
        /* XXX: can we use a buffer cache here? */
        if (!(new_buffer = gst_buffer_new_and_alloc(request_size)))
        {
            GST_ERROR("Failed to allocate buffer of %u bytes.", request_size);
            goto done;
        }
        my_buffer = new_buffer;
    }
    if (!gst_buffer_map(my_buffer, &map, GST_MAP_READWRITE))
    {
        GST_ERROR("Failed to map buffer <%"GST_PTR_FORMAT">.", my_buffer);
        goto done;
    }

    /* Fill buffer. */
    ret = video_conv_state_fill_buffer(state, offset, map.data, map.size, &fill_size);
    gst_buffer_unmap(my_buffer, &map);
    if (ret < 0)
    {
        GST_ERROR("Failed to fill buffer, ret %d.", ret);
        goto done;
    }

    if (fill_size > 0 || !gst_buffer_get_size(my_buffer))
    {
        gst_buffer_set_size(my_buffer, fill_size);
        *buffer = my_buffer;
        flow_ret = GST_FLOW_OK;
    }
    else
    {
        flow_ret = GST_FLOW_EOS;
    }

done:
    if (flow_ret < 0 && new_buffer)
        gst_buffer_unref(new_buffer);
    pthread_mutex_unlock(&conv->state_mutex);
    return flow_ret;
}

static gboolean video_conv_src_query(GstPad *pad, GstObject *parent, GstQuery *query)
{
    VideoConv *conv = VIDEO_CONV(parent);
    struct video_conv_state *state;
    GstSchedulingFlags flags;
    gint min, max, align;
    GstQuery *peer_query;
    uint64_t duration;
    GstFormat format;

    GST_DEBUG_OBJECT(pad, "Got query %"GST_PTR_FORMAT".", query);

    switch (query->type)
    {
    case GST_QUERY_SCHEDULING:
        peer_query = gst_query_new_scheduling();
        if (!gst_pad_peer_query(conv->sink_pad, peer_query))
        {
            GST_ERROR_OBJECT(conv->sink_pad, "Failed to query scheduling from peer.");
            gst_query_unref(peer_query);
            return false;
        }
        gst_query_parse_scheduling(peer_query, &flags, &min, &max, &align);
        gst_query_unref(peer_query);

        gst_query_set_scheduling(query, flags, min, max, align);
        gst_query_add_scheduling_mode(query, GST_PAD_MODE_PULL);

        return true;

    case GST_QUERY_DURATION:
        gst_query_parse_duration(query, &format, NULL);
        if (format != GST_FORMAT_BYTES)
        {
            GST_WARNING("Duration query format is not GST_FORMAT_BYTES.");
            return false;
        }

        if (!(state = video_conv_lock_state(conv)))
            return false;
        if (state->upstream_duration == DURATION_NONE)
            video_conv_query_upstream_duration(conv);
        duration = state->our_duration;
        pthread_mutex_unlock(&conv->state_mutex);

        if (duration == DURATION_NONE)
            return false;
        gst_query_set_duration(query, GST_FORMAT_BYTES, duration);
        return true;

    default:
        return gst_pad_query_default(pad, parent, query);
    }
}

static gboolean video_conv_src_active_mode(GstPad *pad, GstObject *parent, GstPadMode mode, gboolean active)
{
    VideoConv *conv = VIDEO_CONV(parent);
    struct video_conv_state *state;
    struct payload_hash hash;
    bool need_stream_start;
    bool has_transcoded;

    GST_DEBUG_OBJECT(pad, "mode %s, active %d.", gst_pad_mode_get_name(mode), active);

    if (!gst_pad_activate_mode(conv->sink_pad, mode, active))
    {
        GST_ERROR_OBJECT(conv->sink_pad, "Failed to active sink pad: mode %s, active %d.",
                gst_pad_mode_get_name(mode), active);
        return false;
    }

    if (mode != GST_PAD_MODE_PULL)
        return true;

    if (!(state = video_conv_lock_state(conv)))
    {
        GST_ERROR("VideoConv not yet in READY state?");
        return false;
    }

    video_conv_init_transcode(conv);
    hash = state->transcode_hash;
    need_stream_start = state->need_stream_start;
    has_transcoded = state->has_transcoded;

    /* push_event, below, can also grab state and cause a deadlock, so make sure it's
     * unlocked before calling */
    pthread_mutex_unlock(&conv->state_mutex);

    if (need_stream_start && active && has_transcoded)
    {
        push_event(conv->src_pad, gst_event_new_stream_start(format_hash(&hash)));

        if (!(state = video_conv_lock_state(conv)))
        {
            GST_ERROR("VideoConv not yet in READY state?");
            return false;
        }
        state->need_stream_start = false;
        pthread_mutex_unlock(&conv->state_mutex);
    }

    return true;
}


static void video_conv_finalize(GObject *object)
{
    VideoConv *conv = VIDEO_CONV(object);

    pthread_mutex_destroy(&conv->state_mutex);
    if (conv->state)
        video_conv_state_release(conv->state);

    G_OBJECT_CLASS(parent_class)->finalize(object);
}

static void video_conv_class_init(VideoConvClass * klass)
{
    GstElementClass *element_class = GST_ELEMENT_CLASS(klass);
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    gst_element_class_set_metadata(element_class,
            "Proton video converter",
            "Codec/Demuxer",
            "Converts video for Proton",
            "Andrew Eikum <aeikum@codeweavers.com>, Ziqing Hui <zhui@codeweavers.com>");

    element_class->change_state = video_conv_change_state;
    object_class->finalize = video_conv_finalize;

    gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&video_conv_sink_template));
    gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&video_conv_src_template));
}

static void video_conv_init(VideoConv *conv)
{
    GstElement *element = GST_ELEMENT(conv);

    conv->sink_pad = gst_pad_new_from_static_template(&video_conv_sink_template, "sink");
    gst_pad_set_event_function(conv->sink_pad, GST_DEBUG_FUNCPTR(video_conv_sink_event));
    gst_element_add_pad(element, conv->sink_pad);

    conv->src_pad = gst_pad_new_from_static_template(&video_conv_src_template, "src");
    gst_pad_set_getrange_function(conv->src_pad, GST_DEBUG_FUNCPTR(video_conv_src_get_range));
    gst_pad_set_query_function(conv->src_pad, GST_DEBUG_FUNCPTR(video_conv_src_query));
    gst_pad_set_activatemode_function(conv->src_pad, GST_DEBUG_FUNCPTR(video_conv_src_active_mode));
    gst_element_add_pad(element, conv->src_pad);

    pthread_mutex_init(&conv->state_mutex, NULL);
    conv->state = NULL;
}
