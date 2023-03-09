// Raspidetect

// Copyright (C) 2021 Andrei Klimchuk <andrew.klimchuk@gmail.com>

// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 3 of the License, or (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.

// You should have received a copy of the GNU Lesser General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.


#include "main.h"
#include "utils.h"
#include "ffmpeg.h"
#include "d3d.h"

static struct format_mapping_t ffmpeg_input_formats[] = {
    {
        .format = VIDEO_FORMAT_H264,
        .internal_format = AV_CODEC_ID_H264,
        .is_supported = 1
    }
};
static struct format_mapping_t ffmpeg_output_formats[] = {
    {
        .format = VIDEO_FORMAT_GRAYSCALE,
        .internal_format = AV_PIX_FMT_YUV422P,
        .is_supported = 1
    }
};

static struct format_mapping_t *ffmpeg_input_format = NULL;
static struct format_mapping_t *ffmpeg_output_format = NULL;
static struct ffmpeg_state_t ffmpeg = {
    .filter = NULL,
    .codec = NULL,
    .ctx = NULL,
    .fr = NULL
};

extern struct app_state_t app;
extern struct filter_t filters[MAX_FILTERS];

static int ffmpeg_stop()
{
    ASSERT_PTR(ffmpeg_input_format, !=, NULL, error);
    ASSERT_PTR(ffmpeg_output_format, !=, NULL, error);

    av_packet_unref(&ffmpeg.pkt);
    if (ffmpeg.fr) {
        av_frame_free(&ffmpeg.fr);
        ffmpeg.fr = NULL;
    }
    if (ffmpeg.ctx) {
        FFMPEG_CALL(avcodec_close(ffmpeg.ctx), error);
    }
    if (ffmpeg.ctx) {
        avcodec_free_context(&ffmpeg.ctx);
        ffmpeg.ctx = NULL;
    }
    ffmpeg_input_format = NULL;
    ffmpeg_output_format = NULL;
    return 0;

error:
    if (!errno)
        errno = EAGAIN;
    return -1;
}

static int ffmpeg_is_started()
{
    return ffmpeg_input_format != NULL && ffmpeg_output_format != NULL? 1: 0;
}

static void ffmpeg_cleanup()
{
    if (ffmpeg_is_started())
        ffmpeg_stop();
}

static int ffmpeg_start(int input_format, int output_format)
{
    int formats_len = ARRAY_SIZE(ffmpeg_input_formats);
    for (int i = 0; i < formats_len; i++) {
        struct format_mapping_t *f = ffmpeg_input_formats + i;
        if (f->format == input_format && f->is_supported) {
            ffmpeg_input_format = f;
            break;
        }
    }
    if (ffmpeg_input_format == NULL) {
        ERROR_MSG("Input format isn't supported by device or application");
        errno = EINVAL;
        goto error;
    }

    formats_len = ARRAY_SIZE(ffmpeg_output_formats);
    for (int i = 0; i < formats_len; i++) {
        struct format_mapping_t *f = ffmpeg_output_formats + i;
        if (f->format == output_format && f->is_supported) {
            ffmpeg_output_format = f;
            break;
        }
    }
    if (ffmpeg_output_format == NULL) {
        ERROR_MSG("Outoput format isn't supported by device or application");
        errno = EINVAL;
        goto error;
    }

    ffmpeg.codec = avcodec_find_decoder(ffmpeg_input_format->internal_format);
    if (!ffmpeg.codec) {
        DEBUG_MSG("ERROR: avcodec_find_decoder can't find decoder");
        goto error;
    }

    ffmpeg.ctx = avcodec_alloc_context3(ffmpeg.codec);
    if (!ffmpeg.ctx) {
        DEBUG_MSG("ERROR: avcodec_alloc_context3 can't allocate decoder context");
        goto error;
    }

    ffmpeg.ctx->width  = app.server_width;
    ffmpeg.ctx->height = app.server_height;

    //TODO: the format is ignored it is always AV_PIX_FMT_GRAY8
    ffmpeg.ctx->pix_fmt = ffmpeg_output_format->internal_format;


    //ffmpeg.ctx->flags2 |= AV_CODEC_FLAG2_CHUNKS;
    ffmpeg.ctx->debug = 1;

    int res = avcodec_open2(ffmpeg.ctx, ffmpeg.codec, NULL);
    if (res < 0) {
        DEBUG_MSG("ERROR: avcodec_open2 can't open decoder");
        goto error;
    }

    ffmpeg.fr = av_frame_alloc();
    if (!ffmpeg.fr) {
        DEBUG_MSG("ERROR: av_frame_alloc can't allocate frame");
        goto error;
    }

    av_init_packet(&ffmpeg.pkt);
    return 0;

error:
    ffmpeg_stop();
    if (!errno)
        errno = EAGAIN;
    return -1;
}

static int ffmpeg_init()
{
    return 0;
}

static int ffmpeg_process_frame(uint8_t *buffer, int length)
{
    //DEBUG_MSG("ffmpeg_decode: app.enc_buf %p(%d)", buffer, length);
    ffmpeg.pkt.data = buffer;
    ffmpeg.pkt.size = length;
    DEBUG_MSG("nal %X%X%X%X",
        *(buffer),
        *(buffer + 1),
        *(buffer + 2),
        *(buffer + 3)
    );

    int result = 0;
    FFMPEG_CALL(result = avcodec_send_packet(ffmpeg.ctx, &ffmpeg.pkt), error);
    while (result == 0) {
        result = avcodec_receive_frame(ffmpeg.ctx, ffmpeg.fr);
        if (result == AVERROR_EOF || result == AVERROR(EAGAIN))
            goto end;
        else if (result != 0) {
            FFMPEG_CALL_MESSAGE(avcodec_receive_frame, result);
            goto error;
        }

        int res = pthread_mutex_lock(&app.dec_mutex);
        if (res) {
            CALL_CUSTOM_MESSAGE(pthread_mutex_lock(&app.dec_mutex), res);
            goto error;
        }
        // DEBUG_MSG("linesize 1: %d, pix_fmt %d", ffmpeg.fr->linesize[0],
        //     av_pix_fmt_desc_get(ffmpeg.ctx));

        for (int i = 0; i < ffmpeg.fr->height; i++)
            memcpy(
                app.dec_buf + (i * ffmpeg.fr->width),
                ffmpeg.fr->data[0] + (i * ffmpeg.fr->linesize[0]),
                ffmpeg.fr->width
            );
        app.dec_buf_length = ffmpeg.fr->width * ffmpeg.fr->height;

        // app.dec_buf_length = av_image_copy_to_buffer(
        //     app.dec_buf, app.server_width * app.server_height * 4,
        //     (const uint8_t* const *)ffmpeg.fr->data,
        //     (const int*)ffmpeg.fr->linesize, ffmpeg.ctx->pix_fmt,
        //     ffmpeg.ctx->width, ffmpeg.ctx->height, 1);

        res = pthread_mutex_unlock(&app.dec_mutex);
        if (res) {
            CALL_CUSTOM_MESSAGE(pthread_mutex_unlock(&app.dec_mutex), res);
            goto unlockm;
        }

        DEBUG_MSG("avcodec decoded: %d bytes", app.dec_buf_length);
unlockm:
        if (app.dec_buf_length < 0) {
            DEBUG_MSG("ERROR: av_image_copy_to_buffer can't get buffer, res: %d",
                app.dec_buf_length);
            goto error;
        }

#ifdef ENABLE_D3D
        CALL(d3d_render_image(), error);
#endif //ENABLE_D3D
    }

    // int got_frame = 0;
    // int res = ffmpeg.codec->decode(
    //     ffmpeg.ctx,
    //     ffmpeg.fr,
    //     &got_frame,
    //     &ffmpeg.pkt
    // );
    // //DEBUG_MSG("ffmpeg.codec->decode: got_frame %d", got_frame);
    // if (res < 0) {
    //     DEBUG_MSG("ERROR: ffmpeg.codec->decode can't decode slice, res: %d", res);
    //     goto error;
    // }
    // if (got_frame) {
    // }

end:
    return 0;

error:
    errno = EAGAIN;
    return -1;
}

static uint8_t *ffmpeg_get_buffer(int *out_format, int *length)
{
    ASSERT_PTR(ffmpeg_input_format, !=, NULL, cleanup);
    ASSERT_PTR(ffmpeg_output_format, !=, NULL, cleanup);

    if (out_format)
        *out_format = ffmpeg_output_format->format;

    if (length)
        *length = app.dec_buf_length;
    
    app.dec_buf_length = 0;

    return app.dec_buf;

cleanup:
    if (errno == 0)
        errno = EOPNOTSUPP;

    return (uint8_t *)-1;
}

static int ffmpeg_get_in_formats(const struct format_mapping_t *formats[])
{
    if (formats != NULL)
        *formats = ffmpeg_input_formats;
    return ARRAY_SIZE(ffmpeg_input_formats);
}

static int ffmpeg_get_out_formats(const struct format_mapping_t *formats[])
{
    if (formats != NULL)
        *formats = ffmpeg_output_formats;
    return ARRAY_SIZE(ffmpeg_output_formats);
}

void ffmpeg_decoder_construct()
{
    int i = 0;
    while (i < MAX_FILTERS && filters[i].context != NULL)
        i++;

    if (i != MAX_FILTERS) {
        ffmpeg.filter = filters + i;
        filters[i].name = "ffmpeg_decoder";
        filters[i].context = &ffmpeg;
        filters[i].init = ffmpeg_init;
        filters[i].cleanup = ffmpeg_cleanup;
        filters[i].start = ffmpeg_start;
        filters[i].stop = ffmpeg_stop;
        filters[i].is_started = ffmpeg_is_started;
        filters[i].process_frame = ffmpeg_process_frame;
        filters[i].get_buffer = ffmpeg_get_buffer;
        filters[i].get_in_formats = ffmpeg_get_in_formats;
        filters[i].get_out_formats = ffmpeg_get_out_formats;
    }
}
