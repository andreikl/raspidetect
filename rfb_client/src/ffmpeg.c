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

extern struct app_state_t app;

void ffmpeg_destroy()
{
    av_packet_unref(&app.ffmpeg.pkt);
    if (app.ffmpeg.fr) {
        av_frame_free(&app.ffmpeg.fr);
        app.ffmpeg.fr = NULL;
    }
    if (app.ffmpeg.ctx) {
        avcodec_close(app.ffmpeg.ctx);
    }
    if (app.ffmpeg.ctx) {
        avcodec_free_context(&app.ffmpeg.ctx);
        app.ffmpeg.ctx = NULL;
    }
}

int ffmpeg_init()
{
    int res;
    app.ffmpeg.codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!app.ffmpeg.codec) {
        DEBUG("ERROR: avcodec_find_decoder can't find decoder\n");
        goto error;
    }

    app.ffmpeg.ctx = avcodec_alloc_context3(app.ffmpeg.codec);
    if (!app.ffmpeg.ctx) {
        DEBUG("ERROR: avcodec_alloc_context3 can't allocate decoder context\n");
        goto error;
    }

    app.ffmpeg.ctx->width  = app.server_width;
    app.ffmpeg.ctx->height = app.server_height;
    if (app.server_chroma == CHROMA_FORMAT_YUV422) {
        app.ffmpeg.ctx->pix_fmt = AV_PIX_FMT_YUV422P;
    }
    app.ffmpeg.ctx->flags2 |= AV_CODEC_FLAG2_CHUNKS;
    app.ffmpeg.ctx->debug = 1;

    res = avcodec_open2(app.ffmpeg.ctx, app.ffmpeg.codec, NULL);
    if (res < 0) {
        DEBUG("ERROR: avcodec_open2 can't open decoder\n");
        goto error;
    }

    app.ffmpeg.fr = av_frame_alloc();
    if (!app.ffmpeg.fr) {
        DEBUG("ERROR: av_frame_alloc can't allocate frame\n");
        goto error;
    }

    av_init_packet(&app.ffmpeg.pkt);
    return 0;

error:
    ffmpeg_destroy();
    return -1;
}

int ffmpeg_decode(int start, int end)
{
    DEBUG("INFO: ffmpeg_decode: app.enc_buf %p(%d)\n",
         app.enc_buf, app.enc_buf_length);

    app.ffmpeg.pkt.data = app.enc_buf + start - 4;
    app.ffmpeg.pkt.size = end - start + 4;
    DEBUG("INFO: start %d, end: %d\n", start, end);
    // DEBUG("INFO: nal1 %X%X%X%X\n",
    //     *(app.enc_buf + start - 4),
    //     *(app.enc_buf + start - 3),
    //     *(app.enc_buf + start - 2),
    //     *(app.enc_buf + start - 1)
    // );

    int result = 0;
    result = avcodec_send_packet(app.ffmpeg.ctx, &app.ffmpeg.pkt);
    if (result < 0) {
        CALL_CUSTOM_MESSAGE_STR(avcodec_send_packet, av_err2str(result));
        goto error;
    }

    while (result >= 0) {
        DEBUG("avcodec_receive_frame\n");
        result = avcodec_receive_frame(app.ffmpeg.ctx, app.ffmpeg.fr);
        if (result == AVERROR_EOF)
            goto end;
        /*else if (result == AVERROR(EAGAIN)) {
            result = 0;
            break;
        }*/ else if (result < 0) {
            CALL_CUSTOM_MESSAGE(avcodec_receive_frame, result);
            goto error;
        }

        DEBUG("pthread_mutex_lock\n");
        GENERAL_CALL(pthread_mutex_lock(&app.dec_mutex), error);

        DEBUG("av_image_copy_to_buffer\n");
        app.dec_buf_length = av_image_copy_to_buffer(
            app.dec_buf, app.server_width * app.server_height * 4,
            (const uint8_t* const *)app.ffmpeg.fr->data,
            (const int*)app.ffmpeg.fr->linesize, app.ffmpeg.ctx->pix_fmt,
            app.ffmpeg.ctx->width, app.ffmpeg.ctx->height, 1);

        DEBUG("pthread_mutex_unlock\n");
        GENERAL_CALL(pthread_mutex_unlock(&app.dec_mutex), unlockm);
unlockm:
        if (app.dec_buf_length < 0) {
            DEBUG("ERROR: av_image_copy_to_buffer can't get buffer, res: %d\n",
                app.dec_buf_length);
            goto error;
        }

#ifdef ENABLE_D3D
        GENERAL_CALL(d3d_render_image(app), error);
#endif //ENABLE_D3D
    }

    // int got_frame = 0;
    // int res = app.ffmpeg.codec->decode(
    //     app.ffmpeg.ctx,
    //     app.ffmpeg.fr,
    //     &got_frame,
    //     &app.ffmpeg.pkt
    // );
    // //DEBUG("INFO: app.ffmpeg.codec->decode: got_frame %d\n", got_frame);
    // if (res < 0) {
    //     DEBUG("ERROR: app.ffmpeg.codec->decode can't decode slice, res: %d\n", res);
    //     goto error;
    // }
    // if (got_frame) {
    // }

end:
    STANDARD_MESSAGE("ffmpeg_decode failed!!!");

    return 0;
error:
    return -1;
}