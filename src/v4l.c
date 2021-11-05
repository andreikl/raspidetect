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
#include "v4l.h"

#include "linux/videodev2.h"

static struct format_mapping_t v4l_formats[] = {
    {
        .format = VIDEO_FORMAT_YUV422,
        .internal_format = V4L2_PIX_FMT_YUYV,
        .is_supported = 0
    }
};

static struct format_mapping_t *v4l_format = NULL;

static struct v4l_state_t v4l = {
    .app = NULL,
    .dev_id = -1,
    .v4l_buf = NULL,
    .v4l_buf_len = -1,
    .buffer = NULL,
    .buffer_len = -1,
};

extern struct input_t input;

static int ioctl_wait(int fd, int request, void *arg)
{
    struct timespec rem;
    struct timespec req = {
        .tv_sec = 0,
        .tv_nsec = 10000,
    };
    int res = ioctl(fd, request, arg);
    while (res == -1 && (errno == EINTR) ) {
        CALL(nanosleep(&req, &rem), cleanup);
        res = ioctl(fd, request, arg);
    }
    return res;

cleanup:
    return -1;
}

static int ioctl_enum(int fd, int request, void *arg)
{
    int res = ioctl_wait(fd, request, arg);
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
    sprintf(v4l.dev_name, "/dev/video%d", v4l.app->camera_num);

    int len = v4l.app->video_width * v4l.app->video_height * 2;
    uint8_t *data = malloc(len);
    if (data == NULL) {
        errno = ENOMEM;
        CALL_MESSAGE(malloc);
        goto cleanup;
    }
    v4l.v4l_buf = data;
    v4l.v4l_buf_len = len;

    data = malloc(len);
    if (data == NULL) {
        errno = ENOMEM;
        CALL_MESSAGE(malloc);
        goto cleanup;
    }
    v4l.buffer = data;
    v4l.buffer_len = len;

    struct stat st = {
        .st_mode = 1
    };
    CALL(stat(v4l.dev_name, &st), cleanup);

    ASSERT_INT(S_ISCHR(st.st_mode), ==, 0, cleanup);
    CALL(v4l.dev_id = open(v4l.dev_name, O_RDWR | O_NONBLOCK, 0), cleanup);

    struct v4l2_capability cap;
    CALL(ioctl_wait(v4l.dev_id, VIDIOC_QUERYCAP, &cap), cleanup);
    strncpy(v4l.app->camera_name, (const char *)cap.card, 32);
    ASSERT_INT((cap.capabilities & V4L2_CAP_VIDEO_CAPTURE), ==, 0, cleanup);
    ASSERT_INT((cap.capabilities & V4L2_CAP_STREAMING), ==, 0, cleanup);
    //ASSERT_INT((cap.capabilities & V4L2_CAP_READWRITE), ==, 0, cleanup);

    int is_found = 0;
    int formats_len = ARRAY_SIZE(v4l_formats);
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    struct v4l2_fmtdesc fmt = {
        .index = 0, .type = type
    };
    CALL(res = ioctl_enum(v4l.dev_id, VIDIOC_ENUM_FMT, &fmt), cleanup);
    while (res >= 0 && !is_found) {
        DEBUG("pixelformat %c %c %c %c", GET_B(fmt.pixelformat),
            GET_G(fmt.pixelformat), GET_R(fmt.pixelformat), GET_A(fmt.pixelformat));
        for (int i = 0; i < formats_len; i++) {
            struct format_mapping_t *f = v4l_formats + i;
            if (f->internal_format == fmt.pixelformat) {
                CALL(f->is_supported = v4l_is_supported_resolution(fmt.pixelformat),
                    cleanup);

                if (f->is_supported) {
                    if (v4l.app->verbose)
                        DEBUG("input pixel format(%c%c%c%c) and resolution(%dx%d) have been found!",
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

static int v4l_start(int format)
{
    ASSERT_PTR(v4l_format, !=, NULL, cleanup);
    int formats_len = ARRAY_SIZE(v4l_formats);
    for (int i = 0; i < formats_len; i++) {
        struct format_mapping_t *f = v4l_formats + i;
        if (f->format == format && f->is_supported) {
            v4l_format = f;
            break;
        }
    }
    ASSERT_PTR(v4l_format, ==, NULL, cleanup);

    struct v4l2_format fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = v4l_format->internal_format;
    fmt.fmt.pix.width = v4l.app->video_width;
    fmt.fmt.pix.height = v4l.app->video_height;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;
    CALL(ioctl_enum(v4l.dev_id, VIDIOC_S_FMT, &fmt), cleanup);

    struct v4l2_requestbuffers req;
    memset(&req, 0, sizeof(req));
    req.count = 1;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_USERPTR;
    CALL(ioctl_wait(v4l.dev_id, VIDIOC_REQBUFS, &req), cleanup);

    struct v4l2_buffer buf;
    memset(&buf, 0, sizeof(buf));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_USERPTR;
    buf.index = 0;
    buf.m.userptr = (unsigned long)v4l.v4l_buf;
    buf.length = v4l.v4l_buf_len;
    CALL(ioctl_wait(v4l.dev_id, VIDIOC_QBUF, &buf), cleanup);

    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    CALL(ioctl_wait(v4l.dev_id, VIDIOC_STREAMON, &type), cleanup);

    return 0;
cleanup:
    v4l_format = NULL;
    if (errno == 0)
        errno = EAGAIN;
    return -1;
}

static int v4l_is_started()
{
    return v4l_format != NULL? 1: 0;
}

static int v4l_process_frame()
{
    ASSERT_PTR(v4l_format, ==, NULL, cleanup);

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
    CALL(ioctl_wait(v4l.dev_id, VIDIOC_DQBUF, &buf), cleanup);

    memcpy(v4l.buffer, v4l.v4l_buf, v4l.v4l_buf_len);
    v4l.buffer_len = v4l.v4l_buf_len;

    buf.index = 0;
    buf.m.userptr = (unsigned long)v4l.v4l_buf;
    buf.length = v4l.v4l_buf_len;
    CALL(ioctl_wait(v4l.dev_id, VIDIOC_QBUF, &buf), cleanup);
    return 0;

cleanup:
    if (errno == 0)
        errno = EAGAIN;
    return -1;
}

static int v4l_stop()
{
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    CALL(ioctl_wait(v4l.dev_id, VIDIOC_STREAMOFF, &type), cleanup);

    v4l_format = NULL;

    return 0;

cleanup:
    if (errno == 0)
        errno = EAGAIN;
    return -1;
}

static uint8_t *v4l_get_buffer(int *format, int *length)
{
    ASSERT_PTR(v4l_format, ==, NULL, cleanup);
    if (format)
        *format = v4l_format->format;
    if (length)
        *length = v4l.buffer_len;
    return v4l.buffer;

cleanup:
    if (errno == 0)
        errno = EOPNOTSUPP;
    return (uint8_t *)-1;
}

static int v4l_get_formats(const struct format_mapping_t *formats[])
{
    if (formats != NULL)
        *formats = v4l_formats;
    return ARRAY_SIZE(v4l_formats);
}

void v4l_construct(struct app_state_t *app)
{
    v4l.app = app;
    input.name = "v4l";
    input.context = &v4l;
    input.init = v4l_init;
    input.cleanup = v4l_cleanup;
    input.start = v4l_start;
    input.is_started = v4l_is_started;
    input.stop = v4l_stop;
    input.process_frame = v4l_process_frame;

    input.get_buffer = v4l_get_buffer;
    input.get_formats = v4l_get_formats;
}

