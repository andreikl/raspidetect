#include "main.h"

extern "C" {
    #include <libavutil/imgutils.h>
}


void ffmpeg_destroy(struct app_state_t* app)
{
    //av_packet_unref(&app->ffmpeg.pkt);
    if (app->ffmpeg.pkt) {
        av_packet_free(&app->ffmpeg.pkt);
        app->ffmpeg.pkt = NULL;
    }
    if (app->ffmpeg.fr) {
        av_frame_free(&app->ffmpeg.fr);
        app->ffmpeg.fr = NULL;
    }
    if (app->ffmpeg.ctx) {
        avcodec_close(app->ffmpeg.ctx);
    }
    if (app->ffmpeg.ctx->hw_device_ctx != NULL) {
        av_buffer_unref(&app->ffmpeg.ctx->hw_device_ctx);
    }
    if (app->ffmpeg.ctx) {
        avcodec_free_context(&app->ffmpeg.ctx);
        app->ffmpeg.ctx = NULL;
    }
}

int ffmpeg_init(struct app_state_t* app)
{
    int res;
    app->ffmpeg.codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!app->ffmpeg.codec) {
        fprintf(stderr, "ERROR: avcodec_find_decoder can't find decoder\n");
        goto error;
    }

    app->ffmpeg.ctx = avcodec_alloc_context3(app->ffmpeg.codec);
    if (!app->ffmpeg.ctx) {
        fprintf(stderr, "ERROR: avcodec_alloc_context3 can't allocate decoder context\n");
        goto error;
    }

    for (int i = 0;; i++) {
        const AVCodecHWConfig *config = avcodec_get_hw_config(app->ffmpeg.codec, i);
        if (!config) {
            fprintf(stderr, "Decoder %s does not support device type %s.\n",
                app->ffmpeg.codec->name, av_hwdevice_get_type_name(AV_HWDEVICE_TYPE_DXVA2));
            return -1;
        }
        fprintf(stderr, "Decoder %s supports device type %s.\n",
            app->ffmpeg.codec->name, av_hwdevice_get_type_name(config->device_type));

        if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX &&
            config->device_type == AV_HWDEVICE_TYPE_DXVA2) {
            //app->ffmpeg.ctx->pix_fmt = config->pix_fmt;
            break;
        }
    }

    app->ffmpeg.ctx->width  = app->server_width;
    app->ffmpeg.ctx->height = app->server_height;
    if (app->server_chroma == CHROMA_FORMAT_YUV422) {
        app->ffmpeg.ctx->pix_fmt = AV_PIX_FMT_DXVA2_VLD;
    }
    else {
        UNCOVERED_CASE(app->server_chroma, !=, CHROMA_FORMAT_YUV422);
    }
    app->ffmpeg.ctx->flags2 |= AV_CODEC_FLAG2_CHUNKS;
    app->ffmpeg.ctx->debug = 1;

    AVBufferRef *hw_ctx;
    res = av_hwdevice_ctx_create(&hw_ctx, AV_HWDEVICE_TYPE_DXVA2, NULL, NULL, 0);
    if (res < 0) {
        fprintf(stderr, "ERROR: av_hwdevice_ctx_create can't create specified HW device.\n");
        goto error;
    }
    app->ffmpeg.ctx->hw_device_ctx = av_buffer_ref(hw_ctx);

    res = avcodec_open2(app->ffmpeg.ctx, app->ffmpeg.codec, NULL);
    if (res < 0) {
        fprintf(stderr, "ERROR: avcodec_open2 can't open decoder\n");
        goto error;
    }

    app->ffmpeg.fr = av_frame_alloc();
    if (!app->ffmpeg.fr) {
        fprintf(stderr, "ERROR: av_frame_alloc can't allocate frame\n");
        goto error;
    }

    app->ffmpeg.pkt = av_packet_alloc();
    //av_init_packet(&app->ffmpeg.pkt);

    return 0;

error:
    ffmpeg_destroy(app);
    return -1;
}

int ffmpeg_decode(struct app_state_t* app, int start, int end)
{
    // fprintf(stderr, "INFO: ffmpeg_decode: app->enc_buf %p(%d)\n",
    //     app->enc_buf, app->enc_buf_length);
    app->ffmpeg.pkt->data = app->enc_buf + start - 4;
    app->ffmpeg.pkt->size = end - start + 4;
    // fprintf(stderr, "INFO: start %d, end: %d\n", start, end);
    // fprintf(stderr, "INFO: nal1 %X%X%X%X\n",
    //     *(app->enc_buf + start - 4),
    //     *(app->enc_buf + start - 3),
    //     *(app->enc_buf + start - 2),
    //     *(app->enc_buf + start - 1)
    // );

    int got_frame = 0;
    int res = app->ffmpeg.codec->decode(
        app->ffmpeg.ctx,
        app->ffmpeg.fr,
        &got_frame,
        app->ffmpeg.pkt
    );
    //fprintf(stderr, "INFO: app->ffmpeg.codec->decode: got_frame %d\n", got_frame);
    if (res < 0) {
        fprintf(stderr, "ERROR: app->ffmpeg.codec->decode can't decode slice, res: %d\n", res);
        goto error;
    }
    if (got_frame) {
        GENERAL_CALL(pthread_mutex_lock(&app->dec_mutex), error);

        app->dec_buf_length = av_image_copy_to_buffer(
            app->dec_buf, app->server_width * app->server_height * 4,
            (const uint8_t* const *)app->ffmpeg.fr->data,
            (const int*)app->ffmpeg.fr->linesize, app->ffmpeg.ctx->pix_fmt,
            app->ffmpeg.ctx->width, app->ffmpeg.ctx->height, 1);

        GENERAL_CALL(pthread_mutex_unlock(&app->dec_mutex), unlockm);
unlockm:

        // fprintf(stderr, "INFO: av_image_copy_to_buffer get buffer! size: %d\n",
        //     app->dec_buf_length);

        if (app->dec_buf_length < 0) {
            fprintf(stderr, "ERROR: av_image_copy_to_buffer can't get buffer, res: %d\n", res);
            goto error;
        }

#ifdef ENABLE_D3D
        GENERAL_CALL(d3d_render_image(app), error);
#endif //ENABLE_D3D
    }

    fprintf(
        stderr,
        "INFO: app->ffmpeg.ctx->codec->decode returns %d, got_frame %d\n",
        res,
        got_frame);

    return 0;
error:
    return -1;
}
