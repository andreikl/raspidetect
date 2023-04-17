// Raspidetect

// Copyright (C) 2023 Andrei Klimchuk <andrew.klimchuk@gmail.com>

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

#include <libavutil/pixdesc.h>

#include "main.h"
#include "utils.h"
#include "ffmpeg_dxva2.h"
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
        .internal_format = AV_PIX_FMT_DXVA2_VLD,
        .is_supported = 1
    }
};

static struct format_mapping_t *ffmpeg_input_format = NULL;
static struct format_mapping_t *ffmpeg_output_format = NULL;
struct ffmpeg_dxva2_state_t ffmpeg_dxva2 = {
    .filter = NULL,
    .codec = NULL,
    .ctx = NULL,
    .fr = NULL
};

extern struct app_state_t app;
extern struct filter_t filters[MAX_FILTERS];

static int ffmpeg_dxva2_stop()
{
    ASSERT_PTR(ffmpeg_input_format, !=, NULL, error);
    ASSERT_PTR(ffmpeg_output_format, !=, NULL, error);

    av_packet_unref(&ffmpeg_dxva2.pkt);
    if (ffmpeg_dxva2.hw_fr) {
        av_frame_free(&ffmpeg_dxva2.hw_fr);
        ffmpeg_dxva2.hw_fr = NULL;
    }
    if (ffmpeg_dxva2.fr) {
        av_frame_free(&ffmpeg_dxva2.fr);
        ffmpeg_dxva2.fr = NULL;
    }
    if (ffmpeg_dxva2.ctx) {
        FFMPEG_CALL(avcodec_close(ffmpeg_dxva2.ctx), error);
    }
    if (ffmpeg_dxva2.ctx->hw_device_ctx != NULL) {
        av_buffer_unref(&ffmpeg_dxva2.ctx->hw_device_ctx);
    }
    if (ffmpeg_dxva2.ctx) {
        avcodec_free_context(&ffmpeg_dxva2.ctx);
        ffmpeg_dxva2.ctx = NULL;
    }
    ffmpeg_input_format = NULL;
    ffmpeg_output_format = NULL;
    return 0;

error:
    if (!errno)
        errno = EAGAIN;
    return -1;
}

static int ffmpeg_dxva2_is_started()
{
    return ffmpeg_input_format != NULL && ffmpeg_output_format != NULL? 1: 0;
}

static void ffmpeg_dxva2_cleanup()
{
    if (ffmpeg_dxva2_is_started())
        ffmpeg_dxva2_stop();
}

static int ffmpeg_dxva2_start(int input_format, int output_format)
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

    ffmpeg_dxva2.codec = avcodec_find_decoder(ffmpeg_input_format->internal_format);
    if (!ffmpeg_dxva2.codec) {
        CALL_CUSTOM_MESSAGE(avcodec_find_decoder(...), -1);
        goto error;
    }

    ffmpeg_dxva2.ctx = avcodec_alloc_context3(ffmpeg_dxva2.codec);
    if (!ffmpeg_dxva2.ctx) {
        CALL_CUSTOM_MESSAGE(avcodec_alloc_context3(ffmpeg_dxva2.codec), -1);
        goto error;
    }

    for (int i = 0;; i++) {
        const AVCodecHWConfig *config = avcodec_get_hw_config(ffmpeg_dxva2.codec, i);
        if (!config) {
            ERROR_MSG("Decoder %s does not support %s.",
                ffmpeg_dxva2.codec->name, av_hwdevice_get_type_name(AV_HWDEVICE_TYPE_DXVA2));
            goto error;
        }
        DEBUG_MSG("Decoder %s supports %s.",
            ffmpeg_dxva2.codec->name, av_hwdevice_get_type_name(config->device_type));

        if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX &&
            config->device_type == AV_HWDEVICE_TYPE_DXVA2) {
            DEBUG_MSG("%s pixel format %d",
                av_hwdevice_get_type_name(config->device_type), config->pix_fmt);
            break;
        }
    }

    ffmpeg_dxva2.ctx->width  = app.server_width;
    ffmpeg_dxva2.ctx->height = app.server_height;

    //TODO: the format is ignored it is always AV_PIX_FMT_GRAY8
    ffmpeg_dxva2.ctx->pix_fmt = ffmpeg_output_format->internal_format;


    //ffmpeg_dxva2.ctx->flags2 |= AV_CODEC_FLAG2_CHUNKS;
    ffmpeg_dxva2.ctx->debug = 1;

    AVBufferRef *hw_ctx;
    int res = av_hwdevice_ctx_create(&hw_ctx, AV_HWDEVICE_TYPE_DXVA2, NULL, NULL, 0);
    if (res < 0) {
        CALL_CUSTOM_MESSAGE(av_hwdevice_ctx_create(...), -1);
        goto error;
    }
    ffmpeg_dxva2.ctx->hw_device_ctx = av_buffer_ref(hw_ctx);

    res = avcodec_open2(ffmpeg_dxva2.ctx, ffmpeg_dxva2.codec, NULL);
    if (res < 0) {
        CALL_CUSTOM_MESSAGE(avcodec_open2(...), -1);
        goto error;
    }

    ffmpeg_dxva2.fr = av_frame_alloc();
    if (!ffmpeg_dxva2.fr) {
        CALL_CUSTOM_MESSAGE(av_frame_alloc(...), -1);
        goto error;
    }

    ffmpeg_dxva2.hw_fr = av_frame_alloc();
    if (!ffmpeg_dxva2.hw_fr) {
        CALL_CUSTOM_MESSAGE(av_frame_alloc(...), -1);
        goto error;
    }

    av_init_packet(&ffmpeg_dxva2.pkt);
    return 0;

error:
    ffmpeg_dxva2_stop();
    if (!errno)
        errno = EAGAIN;
    return -1;
}

static int ffmpeg_dxva2_init()
{
    return 0;
}

static void yuv_to_rgb(int y, int u, int v, uint8_t* rgb)
{
    int r = y + (1.370705 * (v - 128)); 
    // or fast integer computing with a small approximation
    // rTmp = yValue + (351*(vValue-128))>>8;
    int g = y - (0.698001 * (v - 128)) - (0.337633 * (u - 128)); 
    // gTmp = yValue - (179*(vValue-128) + 86*(uValue-128))>>8;
    int b = y + (1.732446 * (u - 128));
    // bTmp = yValue + (443*(uValue-128))>>8;
    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (b > 255) b = 255;
    if (r < 0) r = 0;
    if (g < 0) g = 0;
    if (b < 0) b = 0;
    *rgb = r; rgb++;
    *rgb = g; rgb++;
    *rgb = b; rgb++;
}

static int ffmpeg_dxva2_process_slice(uint8_t *buffer, int length)
{
    DEBUG_MSG("ffmpeg_dxva2_decode: app.enc_buf %p(%d)", buffer, length);
    ffmpeg_dxva2.pkt.data = buffer;
    ffmpeg_dxva2.pkt.size = length;
    DEBUG_MSG("nal %X%X%X%X",
        *(buffer),
        *(buffer + 1),
        *(buffer + 2),
        *(buffer + 3)
    );

    int result = 0;
    FFMPEG_CALL(result = avcodec_send_packet(ffmpeg_dxva2.ctx, &ffmpeg_dxva2.pkt), error);
    while (result == 0) {
        result = avcodec_receive_frame(ffmpeg_dxva2.ctx, ffmpeg_dxva2.fr);
        if (result == AVERROR_EOF || result == AVERROR(EAGAIN))
            goto end;
        else if (result != 0) {
            FFMPEG_CALL_MESSAGE(avcodec_receive_frame, result);
            goto error;
        }

        result = av_hwframe_transfer_data(ffmpeg_dxva2.hw_fr, ffmpeg_dxva2.fr, 0);
        if (result < 0) {
            FFMPEG_CALL_MESSAGE(av_hwframe_transfer_data, result);
            goto error;
        }

        result = pthread_mutex_lock(&app.dec_mutex);
        if (result) {
            CALL_CUSTOM_MESSAGE(pthread_mutex_lock(&app.dec_mutex), result);
            goto error;
        }
        const struct AVPixFmtDescriptor* format = av_pix_fmt_desc_get(ffmpeg_dxva2.fr->format);
        //DEBUG_MSG("linesize 0: %d, pix_fmt %s", ffmpeg_dxva2.fr->linesize[0], format->name);

        format = av_pix_fmt_desc_get(ffmpeg_dxva2.hw_fr->format);
        DEBUG_MSG("linesize 0: %d, pix_fmt %s", ffmpeg_dxva2.hw_fr->linesize[0], format->name);
        DEBUG_MSG("linesize 1: %d, pix_fmt %s", ffmpeg_dxva2.hw_fr->linesize[1], format->name);
        DEBUG_MSG("linesize 2: %d, pix_fmt %s", ffmpeg_dxva2.hw_fr->linesize[2], format->name);
        DEBUG_MSG("linesize 3: %d, pix_fmt %s", ffmpeg_dxva2.hw_fr->linesize[3], format->name);


        // for (int i = 0; i < ffmpeg_dxva2.hw_fr->height; i++)
        //     memcpy(
        //         app.dec_buf + (i * ffmpeg_dxva2.hw_fr->width),
        //         ffmpeg_dxva2.hw_fr->data[0] + (i * ffmpeg_dxva2.hw_fr->linesize[0]),
        //         ffmpeg_dxva2.hw_fr->width
        //     );
        // app.dec_buf_length = ffmpeg_dxva2.hw_fr->width * ffmpeg_dxva2.hw_fr->height;
        //DEBUG_MSG("memcpy: %p(%d)", app.dec_buf, app.dec_buf_length);


        // nv21
        uint8_t* yb = ffmpeg_dxva2.hw_fr->data[0];
        uint8_t* uvb = ffmpeg_dxva2.hw_fr->data[1];
        uint8_t * dest = app.dec_buf;
        int half = ffmpeg_dxva2.hw_fr->linesize[0] / 2;
        for (int i = 0; i < ffmpeg_dxva2.hw_fr->height; i++)
        {
            for (int j = 0; j < half; j++)
            {
                int y1 = *yb; yb++;
                int y2 = *yb; yb++;
                int u = *uvb; uvb++;
                int v = *uvb; uvb++;
                yuv_to_rgb(y1, u, v, dest);
                dest += 3;
                yuv_to_rgb(y2, u, v, dest);
                dest += 3;
            }
            if ((i & 0x1) == 1) {
                uvb -= ffmpeg_dxva2.hw_fr->linesize[0];
            }
        }
        app.dec_buf_length = ffmpeg_dxva2.hw_fr->width * ffmpeg_dxva2.hw_fr->height * 3;


        // app.dec_buf_length = av_image_copy_to_buffer(
        //     app.dec_buf, app.server_width * app.server_height * 4,
        //     (const uint8_t* const *)ffmpeg_dxva2.fr->data,
        //     (const int*)ffmpeg_dxva2.fr->linesize, ffmpeg_dxva2.ctx->pix_fmt,
        //     ffmpeg_dxva2.ctx->width, ffmpeg_dxva2.ctx->height, 1);

        result = pthread_mutex_unlock(&app.dec_mutex);
        if (result) {
            CALL_CUSTOM_MESSAGE(pthread_mutex_unlock(&app.dec_mutex), result);
            goto unlockm;
        }

        DEBUG_MSG("avcodec decoded: %d bytes", app.dec_buf_length);
unlockm:
        if (app.dec_buf_length < 0) {
            DEBUG_MSG("ERROR: av_image_copy_to_buffer can't get buffer, res: %d",
                app.dec_buf_length);
            goto error;
        }
    }

    // int got_frame = 0;
    // int res = ffmpeg_dxva2.codec->decode(
    //     ffmpeg_dxva2.ctx,
    //     ffmpeg_dxva2.fr,
    //     &got_frame,
    //     &ffmpeg_dxva2.pkt
    // );
    // //DEBUG_MSG("ffmpeg_dxva2.codec->decode: got_frame %d", got_frame);
    // if (res < 0) {
    //     DEBUG_MSG("ERROR: ffmpeg_dxva2.codec->decode can't decode slice, res: %d", res);
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

static uint8_t *ffmpeg_dxva2_get_buffer(int *out_format, int *length)
{
    ASSERT_PTR(ffmpeg_input_format, !=, NULL, cleanup);
    ASSERT_PTR(ffmpeg_output_format, !=, NULL, cleanup);

    if (out_format)
        *out_format = ffmpeg_output_format->format;

    if (length)
        *length = app.dec_buf_length;
    
    //app.dec_buf_length = 0;

    return app.dec_buf;

cleanup:
    if (errno == 0)
        errno = EOPNOTSUPP;

    return (uint8_t *)-1;
}

static int ffmpeg_dxva2_get_in_formats(const struct format_mapping_t *formats[])
{
    if (formats != NULL)
        *formats = ffmpeg_input_formats;
    return ARRAY_SIZE(ffmpeg_input_formats);
}

static int ffmpeg_dxva2_get_out_formats(const struct format_mapping_t *formats[])
{
    if (formats != NULL)
        *formats = ffmpeg_output_formats;
    return ARRAY_SIZE(ffmpeg_output_formats);
}

void ffmpeg_dxva2_decoder_construct()
{
    int i = 0;
    while (i < MAX_FILTERS && filters[i].context != NULL)
        i++;

    if (i != MAX_FILTERS) {
        ffmpeg_dxva2.filter = filters + i;
        ffmpeg_dxva2.filter->name = "ffmpeg_dxva2_decoder";
        ffmpeg_dxva2.filter->context = &ffmpeg_dxva2;
        ffmpeg_dxva2.filter->init = ffmpeg_dxva2_init;
        ffmpeg_dxva2.filter->cleanup = ffmpeg_dxva2_cleanup;
        ffmpeg_dxva2.filter->start = ffmpeg_dxva2_start;
        ffmpeg_dxva2.filter->stop = ffmpeg_dxva2_stop;
        ffmpeg_dxva2.filter->is_started = ffmpeg_dxva2_is_started;
        ffmpeg_dxva2.filter->process = ffmpeg_dxva2_process_slice;
        ffmpeg_dxva2.filter->get_buffer = ffmpeg_dxva2_get_buffer;
        ffmpeg_dxva2.filter->get_in_formats = ffmpeg_dxva2_get_in_formats;
        ffmpeg_dxva2.filter->get_out_formats = ffmpeg_dxva2_get_out_formats;
    }
}
