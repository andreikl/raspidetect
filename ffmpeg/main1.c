#include "libavutil/adler32.h"
#include "libavcodec/avcodec.h"
//#include "libavformat/avformat.h"
#include "libavutil/imgutils.h"
#include "libavutil/timestamp.h"

#define BUFFER_SIZE 100 * 1024
static int video_decode_example(const char *input_filename)
{
    AVCodec *codec = NULL;
    AVCodecContext *ctx= NULL;
    //AVCodecParameters *origin_par = NULL;
    AVFrame *fr = NULL;
    uint8_t *byte_buffer = NULL;
    AVPacket pkt;
    //AVFormatContext *fmt_ctx = NULL;
    int number_of_written_bytes;
    //int video_stream;
    int got_frame = 0;
    int byte_buffer_size;
    int i = 0;
    int result;
    int end_of_stream = 0;

    FILE *file = NULL;
    char* nal = av_malloc(BUFFER_SIZE);

    /*result = avformat_open_input(&fmt_ctx, input_filename, NULL, NULL);
    if (result < 0) {
        av_log(NULL, AV_LOG_ERROR, "Can't open file\n");
        return result;
    }*/

    /*result = avformat_find_stream_info(fmt_ctx, NULL);
    if (result < 0) {
        av_log(NULL, AV_LOG_ERROR, "Can't get stream info\n");
        return result;
    }*/

    /*video_stream = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (video_stream < 0) {
      av_log(NULL, AV_LOG_ERROR, "Can't find video stream in input file\n");
      return -1;
    }

    origin_par = fmt_ctx->streams[video_stream]->codecpar;*/

    codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!codec) {
        av_log(NULL, AV_LOG_ERROR, "Can't find decoder\n");
        return -1;
    }

    ctx = avcodec_alloc_context3(codec);
    if (!ctx) {
        av_log(NULL, AV_LOG_ERROR, "Can't allocate decoder context\n");
        return AVERROR(ENOMEM);
    }

    ctx->width  = 640;
    ctx->height = 480;
    ctx->pix_fmt = AV_PIX_FMT_YUV422P;

    //c->flags2 |= AV_CODEC_FLAG2_CHUNKS;
    //c->thread_type = FF_THREAD_SLICE;
    //c->thread_count = threads;

    /*result = avcodec_parameters_to_context(ctx, origin_par);
    if (result) {
        av_log(NULL, AV_LOG_ERROR, "Can't copy decoder context\n");
        return result;
    }*/

    result = avcodec_open2(ctx, codec, NULL);
    if (result < 0) {
        av_log(ctx, AV_LOG_ERROR, "Can't open decoder\n");
        return result;
    }

    fr = av_frame_alloc();
    if (!fr) {
        av_log(NULL, AV_LOG_ERROR, "Can't allocate frame\n");
        return AVERROR(ENOMEM);
    }

    byte_buffer_size = av_image_get_buffer_size(ctx->pix_fmt, ctx->width, ctx->height, 16);
    byte_buffer = av_malloc(byte_buffer_size);
    if (!byte_buffer) {
        av_log(NULL, AV_LOG_ERROR, "Can't allocate buffer\n");
        return AVERROR(ENOMEM);
    }

    //printf("#tb %d: %d/%d\n", video_stream, fmt_ctx->streams[video_stream]->time_base.num, fmt_ctx->streams[video_stream]->time_base.den);
    i = 0;
    av_init_packet(&pkt);
    if (!(file = fopen(input_filename, "rb"))) {
        fprintf(stderr, "Couldn't open file: %s\n", input_filename);
        return AVERROR(ENOMEM);
    }
            
    pkt.data = nal;
    pkt.size = fread(nal, 1, BUFFER_SIZE, file);

    do {
        /*if (!end_of_stream)
            if (av_read_frame(fmt_ctx, &pkt) < 0)
                end_of_stream = 1;
        if (end_of_stream) {
            pkt.data = NULL;
            pkt.size = 0;
        }*/
        //if (pkt.stream_index == video_stream || end_of_stream) {
            got_frame = 0;
            if (pkt.pts == AV_NOPTS_VALUE)
                pkt.pts = pkt.dts = i;
            result = avcodec_decode_video2(ctx, fr, &got_frame, &pkt);
            if (result < 0) {
                av_log(NULL, AV_LOG_ERROR, "Error decoding frame\n");
                return result;
            }
            if (got_frame) {
                number_of_written_bytes = av_image_copy_to_buffer(byte_buffer, byte_buffer_size,
                                        (const uint8_t* const *)fr->data, (const int*) fr->linesize,
                                        ctx->pix_fmt, ctx->width, ctx->height, 1);
                if (number_of_written_bytes < 0) {
                    av_log(NULL, AV_LOG_ERROR, "Can't copy image to buffer\n");
                    return number_of_written_bytes;
                }
//                printf("%d, %s, %s, %8"PRId64", %8d, 0x%08lx\n", video_stream,
//                       av_ts2str(fr->pts), av_ts2str(fr->pkt_dts), fr->pkt_duration,
//                       number_of_written_bytes, av_adler32_update(0, (const uint8_t*)byte_buffer, number_of_written_bytes));
                printf("%s, %s, %8"PRId64", %8d, 0x%08lx\n",
                       av_ts2str(fr->pts), av_ts2str(fr->pkt_dts), fr->pkt_duration,
                       number_of_written_bytes, av_adler32_update(0, (const uint8_t*)byte_buffer, number_of_written_bytes));

            }
            av_packet_unref(&pkt);
            av_init_packet(&pkt);
        //}
        i++;
    } while (!end_of_stream || got_frame);

    av_packet_unref(&pkt);
    av_frame_free(&fr);
    avcodec_close(ctx);
    //avformat_close_input(&fmt_ctx);
    avcodec_free_context(&ctx);
    av_freep(&byte_buffer);

    if (nal)
        av_free(nal);
    if (file)
        fclose(file);

    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        av_log(NULL, AV_LOG_ERROR, "Incorrect input\n");
        return 1;
    }

    if (video_decode_example(argv[1]) != 0)
        return 1;

    return 0;
}
