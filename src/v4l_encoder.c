#include "main.h"
#include "utils.h"

#include "v4l_encoder.h"

#include "linux/videodev2.h"

static struct format_mapping_t v4l_input_formats[] = {
    {
        .format = VIDEO_FORMAT_YUV422,
        .internal_format = V4L2_PIX_FMT_YUYV,
        .is_supported = 0
    },
    {
        .format = VIDEO_FORMAT_YUV444,
        .internal_format = V4L2_PIX_FMT_YUV444M,
        .is_supported = 0
    }
};
static struct format_mapping_t v4l_output_formats[] = {
    {
        .format = VIDEO_FORMAT_H264,
        .internal_format = V4L2_PIX_FMT_H264,
        .is_supported = 0
    }
};

static struct format_mapping_t *v4l_input_format = NULL;
static struct format_mapping_t *v4l_output_format = NULL;

static struct v4l_encoder_state_t v4l = {
    .app = NULL,
    .dev_id = -1,
    .v4l_buf = NULL,
    .v4l_buf_length = -1,
    .buffer = NULL,
    .buffer_length = -1
};

extern struct filter_t filters[MAX_FILTERS];

static int ioctl_enum(int fd, int request, void *arg)
{
    int res = v4l2_ioctl(fd, request, arg);
    if (res == -1 && errno == EINVAL) {
        res = -2;
    }
    return res;
}

static void v4l_cleanup()
{
    if (v4l.dev_id != -1) {
        CALL(close(v4l.dev_id));
        v4l.dev_id = -1;
    }
    if (v4l.buffer) {
        free(v4l.buffer);
        v4l.buffer = NULL;
    }
    if (v4l.v4l_buf) {
        free(v4l.v4l_buf);
        v4l.v4l_buf = NULL;
    }
}

static int v4l_is_supported_resolution(int format)
{
    int res = 0, is_found = 1;
    struct v4l2_frmsizeenum frmsize = {
        .pixel_format = format, .index = 0
    };

    CALL(res = ioctl_enum(v4l.dev_id, VIDIOC_ENUM_FRAMESIZES, &frmsize), cleanup);
    while (res >= 0) {
        if (frmsize.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
            if (
                frmsize.discrete.width == v4l.app->video_width &&
                frmsize.discrete.height == v4l.app->video_height
            ) {
                is_found = 1;
            }
            if (
                frmsize.discrete.width > v4l.app->camera_max_width || 
                frmsize.discrete.height > v4l.app->camera_max_height
            ) {
                v4l.app->camera_max_width = frmsize.discrete.width;
                v4l.app->camera_max_height = frmsize.discrete.height;
            }
            // DEBUG("V4L2_FRMSIZE_TYPE_DISCRETE %dx%d",
            //     frmsize.discrete.width, frmsize.discrete.height);
        }
        else if (
            frmsize.type == V4L2_FRMSIZE_TYPE_STEPWISE ||
            frmsize.type == V4L2_FRMSIZE_TYPE_CONTINUOUS
        ) {
            if (
                v4l.app->video_width % frmsize.stepwise.step_width == 0 &&
                v4l.app->video_height % frmsize.stepwise.step_height == 0 &&
                v4l.app->video_width >= frmsize.stepwise.min_width &&
                v4l.app->video_height >= frmsize.stepwise.min_height &&
                v4l.app->video_width <= frmsize.stepwise.max_width &&
                v4l.app->video_height <= frmsize.stepwise.max_height
            ) {
                is_found = 1;
            }
            if (
                frmsize.stepwise.max_width > v4l.app->camera_max_width || 
                frmsize.stepwise.max_height > v4l.app->camera_max_height
            ) {
                v4l.app->camera_max_width = frmsize.stepwise.max_width;
                v4l.app->camera_max_height = frmsize.stepwise.max_height;
            }
            // DEBUG("V4L2_FRMSIZE_TYPE_STEPWISE %dx%d",
            //     frmsize.stepwise.max_width, frmsize.stepwise.max_height);
        }
        frmsize.index++;
        CALL(res = ioctl_enum(v4l.dev_id, VIDIOC_ENUM_FRAMESIZES, &frmsize), cleanup);
    }
    return is_found;

cleanup:
    if (errno == 0)
        errno = EAGAIN;
    return -1;
}

static int v4l_init()
{
    ASSERT_INT(v4l.dev_id, !=, -1, cleanup);

    int res = 0;
    sprintf(v4l.dev_name, "/dev/nvhost-msenc");

    int len = v4l.app->video_width * v4l.app->video_height * 2;
    char *data = malloc(len);
    if (data == NULL) {
        errno = ENOMEM;
        CALL_MESSAGE(malloc, 0);
        goto cleanup;
    }
    v4l.v4l_buf = data;
    v4l.v4l_buf_length = len;

    data = malloc(len);
    if (data == NULL) {
        errno = ENOMEM;
        CALL_MESSAGE(malloc, 0);
        goto cleanup;
    }
    v4l.buffer = data;
    v4l.buffer_length = len;

    struct stat st;
    CALL(stat(v4l.dev_name, &st), cleanup);
    ASSERT_INT(S_ISCHR(st.st_mode), ==, 0, cleanup);

    CALL(v4l.dev_id = v4l2_open(v4l.dev_name, O_RDWR | O_NONBLOCK, 0), cleanup);
    struct v4l2_capability cap;
    CALL(v4l2_ioctl(v4l.dev_id, VIDIOC_QUERYCAP, &cap), cleanup);
    strncpy(v4l.app->camera_name, (const char *)cap.card, 32);
    // DEBUG("cap.capabilities: %d", cap.capabilities);

    ASSERT_INT((cap.capabilities & V4L2_CAP_VIDEO_M2M_MPLANE), ==, 0, cleanup);
    ASSERT_INT((cap.capabilities & V4L2_CAP_STREAMING), ==, 0, cleanup);


    int is_found = 0;
    int formats_len = ARRAY_SIZE(v4l_input_formats);
    struct v4l2_fmtdesc fmt = {
        .index = 0, .type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE
    };
    CALL(res = ioctl_enum(v4l.dev_id, VIDIOC_ENUM_FMT, &fmt), cleanup);
    while (res >= 0 && !is_found) {
        // DEBUG("pixelformat %c %c %c %c", GET_B(fmt.pixelformat),
        //     GET_G(fmt.pixelformat), GET_R(fmt.pixelformat), GET_A(fmt.pixelformat));
        for (int i = 0; i < formats_len; i++) {
            struct format_mapping_t *f = v4l_input_formats + i;
            if (f->internal_format == fmt.pixelformat) {
                CALL(
                    f->is_supported = v4l_is_supported_resolution(fmt.pixelformat),
                    cleanup);

                if (f->is_supported) {
                    if (v4l.app->verbose)
                        DEBUG("%s(%c%c%c%c) and resolution(%dx%d) have been found!",
                            "encoder input pixel format",
                            GET_B(fmt.pixelformat),
                            GET_G(fmt.pixelformat),
                            GET_R(fmt.pixelformat),
                            GET_A(fmt.pixelformat),
                            v4l.app->video_width,
                            v4l.app->video_height);
                    is_found = 1;
                }
                break;
            }
        }

        fmt.index++;
        CALL(res = ioctl_enum(v4l.dev_id, VIDIOC_ENUM_FMT, &fmt), cleanup);
    }
    if (!is_found) {
        errno = EOPNOTSUPP;
        return -1;
    }

    is_found = 0;
    formats_len = ARRAY_SIZE(v4l_output_formats);
    fmt.index = 0;
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    CALL(res = ioctl_enum(v4l.dev_id, VIDIOC_ENUM_FMT, &fmt), cleanup);
    while (res >= 0 && !is_found) {
        // DEBUG("pixelformat %c %c %c %c", GET_B(fmt.pixelformat),
        //     GET_G(fmt.pixelformat), GET_R(fmt.pixelformat), GET_A(fmt.pixelformat));
        for (int i = 0; i < formats_len; i++) {
            struct format_mapping_t *f = v4l_output_formats + i;
            if (f->internal_format == fmt.pixelformat) {
                CALL(
                    f->is_supported = v4l_is_supported_resolution(fmt.pixelformat),
                    cleanup);

                if (f->is_supported) {
                    if (v4l.app->verbose)
                        DEBUG("%s(%c%c%c%c) and resolution(%dx%d) have been found!",
                            "encoder output pixel format",
                            GET_B(fmt.pixelformat),
                            GET_G(fmt.pixelformat),
                            GET_R(fmt.pixelformat),
                            GET_A(fmt.pixelformat),
                            v4l.app->video_width,
                            v4l.app->video_height);
                    is_found = 1;
                }
                break;
            }
        }

        fmt.index++;
        CALL(res = ioctl_enum(v4l.dev_id, VIDIOC_ENUM_FMT, &fmt), cleanup);
    }
    if (!is_found) {
        errno = EOPNOTSUPP;
        return -1;
    }
    return 0;
cleanup:

    v4l_cleanup();
    if (errno == 0)
        errno = EAGAIN;
    return -1;
}

static int v4l_start(int input_format, int output_format)
{
    ASSERT_PTR(v4l_input_format, !=, NULL, cleanup);
    int formats_len = ARRAY_SIZE(v4l_input_formats);
    for (int i = 0; i < formats_len; i++) {
        struct format_mapping_t *f = v4l_input_formats + i;
        if (f->format == input_format && f->is_supported) {
            v4l_input_format = f;
            break;
        }
    }
    ASSERT_PTR(v4l_input_format, ==, NULL, cleanup);

    ASSERT_PTR(v4l_output_format, !=, NULL, cleanup);
    formats_len = ARRAY_SIZE(v4l_output_formats);
    for (int i = 0; i < formats_len; i++) {
        struct format_mapping_t *f = v4l_output_formats + i;
        if (f->format == output_format && f->is_supported) {
            v4l_output_format = f;
            break;
        }
    }
    ASSERT_PTR(v4l_output_format, ==, NULL, cleanup);

    return 0;
cleanup:
    v4l_input_format = NULL;
    v4l_output_format = NULL;

    if (errno == 0)
        errno = EAGAIN;
    return -1;
}

static int v4l_process(char * buffer)
{
    ASSERT_PTR(v4l_input_format, ==, NULL, cleanup);
    ASSERT_PTR(v4l_output_format, ==, NULL, cleanup);
    
    int res;
    struct timeval tv;
    fd_set rfds;

    FD_ZERO(&rfds);
    FD_SET(v4l.dev_id, &rfds);

    tv.tv_sec = 2;
    tv.tv_usec = 0;
    CALL(res = select(v4l.dev_id + 1, &rfds, NULL, NULL, &tv), cleanup);
    if (res == 0) {
        errno = ETIME;
        goto cleanup;
    }

    struct v4l2_buffer buf;
    memset(&buf, 0, sizeof(buf));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_USERPTR;
    CALL(v4l2_ioctl(v4l.dev_id, VIDIOC_DQBUF, &buf), cleanup);

    memcpy(v4l.buffer, v4l.v4l_buf, v4l.v4l_buf_length);

    buf.index = 0;
    buf.m.userptr = (unsigned long)v4l.v4l_buf;
    buf.length = v4l.v4l_buf_length;
    CALL(v4l2_ioctl(v4l.dev_id, VIDIOC_QBUF, &buf), cleanup);
    return 0;

cleanup:
    if (errno == 0)
        errno = EAGAIN;
    return -1;
}

static int v4l_stop()
{
    v4l_input_format = NULL;
    v4l_output_format = NULL;

    return 0;
}

static char *v4l_get_buffer()
{
    ASSERT_PTR(v4l_input_format, ==, NULL, cleanup);
    ASSERT_PTR(v4l_output_format, ==, NULL, cleanup);
    return v4l.buffer;

cleanup:
    if (errno == 0)
        errno = EOPNOTSUPP;
    return (char *)-1;
}

static int v4l_get_in_formats(const struct format_mapping_t *formats[])
{
    if (v4l_input_formats != NULL)
        *formats = v4l_input_formats;
    return ARRAY_SIZE(v4l_input_formats);
}

static int v4l_get_out_formats(const struct format_mapping_t *formats[])
{
    if (v4l_output_formats != NULL)
        *formats = v4l_output_formats;
    return ARRAY_SIZE(v4l_output_formats);
}

void v4l_encoder_construct(struct app_state_t *app)
{
    int i = 0;
    while (filters[i].context != NULL && i < MAX_FILTERS)
        i++;

    if (i != MAX_FILTERS) {
        v4l.app = app;

        filters[i].name = "v4l_encoder";
        filters[i].context = &v4l;
        filters[i].init = v4l_init;
        filters[i].start = v4l_start;
        filters[i].process = v4l_process;
        filters[i].stop = v4l_stop;
        filters[i].cleanup = v4l_cleanup;

        filters[i].get_buffer = v4l_get_buffer;
        filters[i].get_in_formats = v4l_get_in_formats;
        filters[i].get_out_formats = v4l_get_out_formats;
    }
}

