#include "main.h"
#include "utils.h"
#include "v4l.h"

#include "linux/videodev2.h"

static struct v4l_state_t v4l = {
    .dev_id = -1,
    .buffer = NULL,
    .buffer_length = -1
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

static void v4l_cleanup(struct app_state_t *app)
{
    if (v4l.dev_id != -1) {
        CALL(close(v4l.dev_id));
        v4l.dev_id = -1;
    }
    if (v4l.buffer) {
        free(v4l.buffer);
        v4l.buffer = NULL;
    }
}

static int v4l_verify_capabilities(struct app_state_t *app)
{
    int res = 0;
    struct v4l2_capability cap;
    CALL(ioctl_wait(v4l.dev_id, VIDIOC_QUERYCAP, &cap), cleanup);
    strncpy(app->camera_name, (const char *)cap.card, 32);
    ASSERT_INT((cap.capabilities & V4L2_CAP_VIDEO_CAPTURE), ==, 0, cleanup);
    ASSERT_INT((cap.capabilities & V4L2_CAP_STREAMING), ==, 0, cleanup);
    //ASSERT_INT((cap.capabilities & V4L2_CAP_READWRITE), ==, 0, cleanup);

    int is_found = 0;
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    struct v4l2_fmtdesc fmt = {
        .index = 0, .type = type
    };
    CALL(res = ioctl_enum(v4l.dev_id, VIDIOC_ENUM_FMT, &fmt), cleanup);
    while (res >= 0 && !is_found) {
        if (app->verbose)
            fprintf(stderr, "INFO: pixelformat %c %c %c %c\n", GET_B(fmt.pixelformat),
                GET_G(fmt.pixelformat), GET_R(fmt.pixelformat), GET_A(fmt.pixelformat));

        if (app->video_format == VIDEO_FORMAT_YUV422 && fmt.pixelformat == V4L2_PIX_FMT_YUYV) {
            struct v4l2_frmsizeenum frmsize = {
                .pixel_format = fmt.pixelformat, .index = 0
            };
            CALL(res = ioctl_enum(v4l.dev_id, VIDIOC_ENUM_FRAMESIZES, &frmsize), cleanup);
            while (res >= 0) {
                if (frmsize.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
                    if (
                        frmsize.discrete.width == app->video_width &&
                        frmsize.discrete.height == app->video_height
                    ) {
                        is_found = 1;
                    }
                    if (
                        frmsize.discrete.width > app->camera_max_width || 
                        frmsize.discrete.height > app->camera_max_height
                    ) {
                        app->camera_max_width = frmsize.discrete.width;
                        app->camera_max_height = frmsize.discrete.height;
                    }
                    if (app->verbose)
                        fprintf(stderr, "INFO: V4L2_FRMSIZE_TYPE_DISCRETE %dx%d\n",
                            frmsize.discrete.width, frmsize.discrete.height);
                }
                else if (
                    frmsize.type == V4L2_FRMSIZE_TYPE_STEPWISE ||
                    frmsize.type == V4L2_FRMSIZE_TYPE_CONTINUOUS
                ) {
                    if (
                        app->video_width % frmsize.stepwise.step_width == 0 &&
                        app->video_height % frmsize.stepwise.step_height == 0 &&
                        app->video_width >= frmsize.stepwise.min_width &&
                        app->video_height >= frmsize.stepwise.min_height &&
                        app->video_width <= frmsize.stepwise.max_width &&
                        app->video_height <= frmsize.stepwise.max_height
                    ) {
                        is_found = 1;
                    }
                    if (
                        frmsize.stepwise.max_width > app->camera_max_width || 
                        frmsize.stepwise.max_height > app->camera_max_height
                    ) {
                        app->camera_max_width = frmsize.stepwise.max_width;
                        app->camera_max_height = frmsize.stepwise.max_height;
                    }
                    if (app->verbose)
                        fprintf(stderr, "INFO: V4L2_FRMSIZE_TYPE_STEPWISE %dx%d\n",
                            frmsize.stepwise.max_width, frmsize.stepwise.max_height);
                }
                frmsize.index++;
                CALL(res = ioctl_enum(v4l.dev_id, VIDIOC_ENUM_FRAMESIZES, &frmsize), cleanup);
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
    if (errno == 0)
        errno = EAGAIN;
    return -1;
}

static int v4l_init(struct app_state_t *app)
{
    sprintf(v4l.dev_name, "/dev/video%d", app->camera_num);

    int len = app->video_width * app->video_height * 4;
    char *data = malloc(len);
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
    v4l.dev_id = open(v4l.dev_name, O_RDWR | O_NONBLOCK, 0);
    if (v4l.dev_id == -1) {
        CALL_MESSAGE(open(v4l.dev_name, O_RDWR | O_NONBLOCK, 0), v4l.dev_id);
        goto cleanup;
    }
    return 0;
cleanup:

    v4l_cleanup(app);
    if (errno == 0)
        errno = EAGAIN;
    return -1;
}

static int v4l_open(struct app_state_t *app)
{
    struct v4l2_format fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = app->video_width;
    fmt.fmt.pix.height = app->video_height;
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
    buf.m.userptr = (unsigned long)v4l.buffer;
    buf.length = v4l.buffer_length;
    CALL(ioctl_wait(v4l.dev_id, VIDIOC_QBUF, &buf), cleanup);

    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    CALL(ioctl_wait(v4l.dev_id, VIDIOC_STREAMON, &type), cleanup);

    return 0;
cleanup:
    if (errno == 0)
        errno = EAGAIN;
    return -1;
}

static int v4l_get_frame(struct app_state_t *app)
{
    struct timeval tv;
    fd_set rfds;

    FD_ZERO(&rfds);
    FD_SET(v4l.dev_id, &rfds);

    tv.tv_sec = 1;
    tv.tv_usec = 0;
    CALL(select(1, &rfds, NULL, NULL, &tv), cleanup);

    struct v4l2_buffer buf;
    memset(&buf, 0, sizeof(buf));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_USERPTR;
    CALL(ioctl_wait(v4l.dev_id, VIDIOC_DQBUF, &buf), cleanup);

    //TODO: process

    buf.index = 0;
    buf.m.userptr = (unsigned long)v4l.buffer;
    buf.length = v4l.buffer_length;
    CALL(ioctl_wait(v4l.dev_id, VIDIOC_QBUF, &buf), cleanup);
    return 0;

cleanup:
    if (errno == 0)
        errno = EAGAIN;
    return -1;
}

int v4l_close(struct app_state_t *app)
{
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    CALL(ioctl_wait(v4l.dev_id, VIDIOC_STREAMOFF, &type), cleanup);

    return 0;
cleanup:
    if (errno == 0)
        errno = EAGAIN;
    return -1;
}

void v4l_construct(struct app_state_t *app)
{
    input.context = &v4l;
    input.verify_capabilities = v4l_verify_capabilities;
    input.init = v4l_init;
    input.open = v4l_open;
    input.get_frame = v4l_get_frame;
    input.close = v4l_close;
    input.cleanup = v4l_cleanup;
}

