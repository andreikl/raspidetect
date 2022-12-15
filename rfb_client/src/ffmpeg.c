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
        DEBUG("ERROR: avcodec_find_decoder can't find decoder");
        goto error;
    }

    app.ffmpeg.ctx = avcodec_alloc_context3(app.ffmpeg.codec);
    if (!app.ffmpeg.ctx) {
        DEBUG("ERROR: avcodec_alloc_context3 can't allocate decoder context");
        goto error;
    }

    app.ffmpeg.ctx->width  = app.server_width;
    app.ffmpeg.ctx->height = app.server_height;
    if (app.server_chroma == CHROMA_FORMAT_YUV422) {
        app.ffmpeg.ctx->pix_fmt = AV_PIX_FMT_YUV422P;
    }
    //TODO: the format is ignored it is always  YUV422P
    app.ffmpeg.ctx->pix_fmt = AV_PIX_FMT_YUV422P;

    app.ffmpeg.ctx->flags2 |= AV_CODEC_FLAG2_CHUNKS;
    app.ffmpeg.ctx->debug = 1;

    res = avcodec_open2(app.ffmpeg.ctx, app.ffmpeg.codec, NULL);
    if (res < 0) {
        DEBUG("ERROR: avcodec_open2 can't open decoder");
        goto error;
    }

    app.ffmpeg.fr = av_frame_alloc();
    if (!app.ffmpeg.fr) {
        DEBUG("ERROR: av_frame_alloc can't allocate frame");
        goto error;
    }

    av_init_packet(&app.ffmpeg.pkt);
    return 0;

error:
    ffmpeg_destroy();
    return -1;
}

int ffmpeg_decode()
{
    DEBUG("ffmpeg_decode: app.enc_buf %p(%d)",
         app.enc_buf, app.enc_buf_length);

    app.ffmpeg.pkt.data = app.enc_buf;
    app.ffmpeg.pkt.size = app.enc_buf_length;
    DEBUG("nal %X%X%X%X",
        *(app.enc_buf),
        *(app.enc_buf + 1),
        *(app.enc_buf + 2),
        *(app.enc_buf + 3)
    );

    int result = 0;
    FFMPEG_CALL(result = avcodec_send_packet(app.ffmpeg.ctx, &app.ffmpeg.pkt), error);
    while (result == 0) {
        result = avcodec_receive_frame(app.ffmpeg.ctx, app.ffmpeg.fr);
        if (result == AVERROR_EOF || result == AVERROR(EAGAIN))
            goto end;
        else if (result != 0) {
            FFMPEG_CALL_MESSAGE(avcodec_receive_frame, result);
            goto error;
        }

        CALL(pthread_mutex_lock(&app.dec_mutex), error);

        // DEBUG("linesize 1: %d, pix_fmt %d", app.ffmpeg.fr->linesize[0],
        //     av_pix_fmt_desc_get(app.ffmpeg.ctx));

        for (int i = 0; i < app.ffmpeg.fr->height; i++)
            memcpy(
                app.dec_buf + (i * app.ffmpeg.fr->width),
                app.ffmpeg.fr->data[0] + (i * app.ffmpeg.fr->linesize[0]),
                app.ffmpeg.fr->width
            );
        app.dec_buf_length = app.ffmpeg.fr->width * app.ffmpeg.fr->height;

        // app.dec_buf_length = av_image_copy_to_buffer(
        //     app.dec_buf, app.server_width * app.server_height * 4,
        //     (const uint8_t* const *)app.ffmpeg.fr->data,
        //     (const int*)app.ffmpeg.fr->linesize, app.ffmpeg.ctx->pix_fmt,
        //     app.ffmpeg.ctx->width, app.ffmpeg.ctx->height, 1);

        CALL(pthread_mutex_unlock(&app.dec_mutex), unlockm);

        DEBUG("avcodec decoded: %d bytes", app.dec_buf_length);
unlockm:
        if (app.dec_buf_length < 0) {
            DEBUG("ERROR: av_image_copy_to_buffer can't get buffer, res: %d",
                app.dec_buf_length);
            goto error;
        }

#ifdef ENABLE_D3D
        CALL(d3d_render_image(app), error);
#endif //ENABLE_D3D
    }

    // int got_frame = 0;
    // int res = app.ffmpeg.codec->decode(
    //     app.ffmpeg.ctx,
    //     app.ffmpeg.fr,
    //     &got_frame,
    //     &app.ffmpeg.pkt
    // );
    // //DEBUG("app.ffmpeg.codec->decode: got_frame %d", got_frame);
    // if (res < 0) {
    //     DEBUG("ERROR: app.ffmpeg.codec->decode can't decode slice, res: %d", res);
    //     goto error;
    // }
    // if (got_frame) {
    // }

end:
    DEBUG("ffmpeg_decode finished");
    return 0;

error:
    errno = EAGAIN;
    return -1;
}