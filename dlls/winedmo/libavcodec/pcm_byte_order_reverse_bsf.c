/*
 * PCM byte order reverse byte stream format filter
 * Copyright (c) 2025 Conor McCarthy for CodeWeavers
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

#ifdef HAVE_FFMPEG

#define IS_EMPTY(pkt) (!(pkt)->data && !(pkt)->side_data_elems)

struct AVBSFInternal {
    AVPacket *buffer_pkt;
    int eof;
};

/* From FFmpeg */
int ff_bsf_get_packet(AVBSFContext *ctx, AVPacket **pkt)
{
    AVBSFInternal *bsfi = ctx->internal;
    AVPacket *tmp_pkt;

    if (bsfi->eof)
        return AVERROR_EOF;

    if (IS_EMPTY(bsfi->buffer_pkt))
        return AVERROR(EAGAIN);

    tmp_pkt = av_packet_alloc();
    if (!tmp_pkt)
        return AVERROR(ENOMEM);

    *pkt = bsfi->buffer_pkt;
    bsfi->buffer_pkt = tmp_pkt;

    return 0;
}

static enum AVCodecID reverse_codec_id(enum AVCodecID codec_id)
{
    switch (codec_id)
    {
    case AV_CODEC_ID_PCM_S16BE: return AV_CODEC_ID_PCM_S16LE;
    case AV_CODEC_ID_PCM_S24BE: return AV_CODEC_ID_PCM_S24LE;
    case AV_CODEC_ID_PCM_S32BE: return AV_CODEC_ID_PCM_S32LE;
    case AV_CODEC_ID_PCM_S64BE: return AV_CODEC_ID_PCM_S64LE;
    case AV_CODEC_ID_PCM_F32BE: return AV_CODEC_ID_PCM_F32LE;
    case AV_CODEC_ID_PCM_F64BE: return AV_CODEC_ID_PCM_F64LE;
    case AV_CODEC_ID_PCM_S16LE: return AV_CODEC_ID_PCM_S16BE;
    case AV_CODEC_ID_PCM_S24LE: return AV_CODEC_ID_PCM_S24BE;
    case AV_CODEC_ID_PCM_S32LE: return AV_CODEC_ID_PCM_S32BE;
    case AV_CODEC_ID_PCM_S64LE: return AV_CODEC_ID_PCM_S64BE;
    case AV_CODEC_ID_PCM_F32LE: return AV_CODEC_ID_PCM_F32BE;
    case AV_CODEC_ID_PCM_F64LE: return AV_CODEC_ID_PCM_F64BE;
    default: return codec_id;
    }
}

static int init(AVBSFContext *ctx)
{
    if (ctx->par_in->channels <= 0 || ctx->par_in->sample_rate <= 0)
        return AVERROR(EINVAL);
    if (ctx->par_in->bits_per_coded_sample % 8u)
        return AVERROR(EINVAL);

    ctx->par_out->codec_id = reverse_codec_id(ctx->par_in->codec_id);

    return 0;
}

static int byte_order_reverse_filter(AVBSFContext *ctx, AVPacket *pkt)
{
    unsigned int bytes_per_sample;
    const uint8_t *buf_end;
    const uint8_t *buf;
    unsigned int i;
    AVPacket *in;
    uint8_t *out;
    int ret;

    ret = ff_bsf_get_packet(ctx, &in);
    if (ret < 0)
        return ret;

    buf = in->data;
    buf_end = in->data + in->size;

    ret = av_new_packet(pkt, in->size);
    if (ret < 0)
        return ret;
    out = pkt->data;

    ret = av_packet_copy_props(pkt, in);
    if (ret < 0)
        goto fail;

    bytes_per_sample = ctx->par_in->bits_per_coded_sample / 8u;

    while (buf < buf_end)
    {
        for (i = 0; i < bytes_per_sample; ++i)
            out[i] = buf[bytes_per_sample - i - 1];
        buf += bytes_per_sample;
        out += bytes_per_sample;
    }

fail:
    if (ret < 0)
        av_packet_unref(pkt);
    av_packet_free(&in);
    return ret;
}

static const enum AVCodecID codec_ids[] = {
    AV_CODEC_ID_PCM_S8,
    AV_CODEC_ID_PCM_S16LE,
    AV_CODEC_ID_PCM_S16BE,
    AV_CODEC_ID_PCM_S24LE,
    AV_CODEC_ID_PCM_S24BE,
    AV_CODEC_ID_PCM_S32LE,
    AV_CODEC_ID_PCM_S32BE,
    AV_CODEC_ID_PCM_S64LE,
    AV_CODEC_ID_PCM_S64BE,
    AV_CODEC_ID_PCM_F32LE,
    AV_CODEC_ID_PCM_F32BE,
    AV_CODEC_ID_PCM_F64LE,
    AV_CODEC_ID_PCM_F64BE,
    AV_CODEC_ID_NONE,
};

const AVBitStreamFilter ff_pcm_byte_order_reverse_bsf = {
    .name           = "pcm_byte_order_reverse",
    .filter         = byte_order_reverse_filter,
    .init           = init,
    .codec_ids      = codec_ids,
};

#endif /* HAVE_FFMPEG */
