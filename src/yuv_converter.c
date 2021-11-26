#include "main.h"
#include "utils.h"

#include "yuv_converter.h"

static struct format_mapping_t yuv_input_formats[] = {
    {
        .format = VIDEO_FORMAT_YUV422,
        .internal_format = VIDEO_FORMAT_YUV422,
        .is_supported = 1
    }
};
static struct format_mapping_t yuv_output_formats[] = {
    {
        .format = VIDEO_FORMAT_YUV444,
        .internal_format = VIDEO_FORMAT_YUV444,
        .is_supported = 1
    }
};

static struct yuv_converter_state_t yuv = {
    .app = NULL,
    .buffer = NULL,
    .buffer_length = -1
};

extern struct filter_t filters[MAX_FILTERS];

static void yuv_cleanup()
{
    if (yuv.buffer) {
        free(yuv.buffer);
        yuv.buffer = NULL;
    }
}

static int yuv_init()
{
    ASSERT_PTR(yuv.buffer, !=, NULL, cleanup);

    int len = yuv.app->video_width * yuv.app->video_height * 3;
    uint8_t *data = malloc(len);
    data = malloc(len);
    if (data == NULL) {
        errno = ENOMEM;
        CALL_MESSAGE(malloc);
        goto cleanup;
    }
    yuv.buffer = data;
    yuv.buffer_length = len;
    return 0;

cleanup:
    yuv_cleanup();
    if (errno == 0)
        errno = EAGAIN;
    return -1;
}

static int yuv_start(int input_format, int output_format)
{
    return 0;
}

static int yuv_is_started()
{
    return yuv.buffer != NULL? 1: 0;
}

static int yuv_process_frame(uint8_t *buffer)
{
    ASSERT_PTR(yuv.buffer, ==, NULL, cleanup);
    int plane = yuv.app->video_width * yuv.app->video_height;
    int planes = plane * 2;
    uint8_t * res = yuv.buffer;
    for (int i = 0, j = 0; i < planes; i += 4, j += 2) {
        uint8_t y0 = buffer[i];
        uint8_t u01 = buffer[i + 1];
        uint8_t y1 = buffer[i + 2];
        uint8_t v01 = buffer[i + 3];
        res[j] = y0;
        res[j + plane] = u01 & 0xF;
        res[j + planes] = v01 & 0xF;
        res[j + 1] = y1;
        res[j + 1 + plane] = (u01 & 0xF0) >> 4;
        res[j + 1 + planes] = (v01 & 0xF0) >> 4;
    }
    return 0;

cleanup:
    if (errno == 0)
        errno = EAGAIN;
    return -1;
}

static int yuv_stop()
{
    return 0;
}

static uint8_t *yuv_get_buffer(int *in_format, int *out_format, int *length)
{
    if (in_format)
        *in_format = yuv_input_formats[0].format;
    if (out_format)
        *out_format = yuv_output_formats[0].format;
    return yuv.buffer;
}

static int yuv_get_in_formats(const struct format_mapping_t *formats[])
{
    if (yuv_input_formats != NULL)
        *formats = yuv_input_formats;
    return ARRAY_SIZE(yuv_input_formats);
}

static int yuv_get_out_formats(const struct format_mapping_t *formats[])
{
    if (yuv_output_formats != NULL)
        *formats = yuv_output_formats;
    return ARRAY_SIZE(yuv_output_formats);
}

void yuv_converter_construct(struct app_state_t *app)
{
    int i = 0;
    while (i < MAX_FILTERS && filters[i].context != NULL)
        i++;

    if (i != MAX_FILTERS) {
        yuv.app = app;

        filters[i].name = "yuv_converter";
        filters[i].context = &app;
        filters[i].init = yuv_init;
        filters[i].cleanup = yuv_cleanup;
        filters[i].start = yuv_start;
        filters[i].is_started = yuv_is_started;
        filters[i].stop = yuv_stop;
        filters[i].process_frame = yuv_process_frame;

        filters[i].get_buffer = yuv_get_buffer;
        filters[i].get_in_formats = yuv_get_in_formats;
        filters[i].get_out_formats = yuv_get_out_formats;
    }
}

