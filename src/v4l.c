#include "main.h"
#include "utils.h"

static int ioctl_wait(int fd, int request, void *arg)
{
    struct timespec rem;
    struct timespec req = {
        .tv_sec = 0,
        .tv_nsec = 10000,
    };
    int res = ioctl(fd, request, arg);
    while (res == -1 && errno == EINTR) {
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

int v4l_verify_capabilities(struct app_state_t *app)
{
    int res = 0;
    struct v4l2_capability cap;
    CALL(ioctl_wait(app->v4l.dev_id, VIDIOC_QUERYCAP, &cap), cleanup);
    strncpy(app->camera_name, (const char *)cap.card, 32);
    ASSERT_INT((cap.capabilities & V4L2_CAP_VIDEO_CAPTURE), ==, 0, cleanup);
    ASSERT_INT((cap.capabilities & V4L2_CAP_STREAMING), ==, 0, cleanup);

    int is_found = 0;
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    struct v4l2_fmtdesc fmt = {
        .index = 0, .type = type
    };
    CALL(res = ioctl_enum(app->v4l.dev_id, VIDIOC_ENUM_FMT, &fmt), cleanup);
    while (res >= 0 && !is_found) {
        if (app->verbose)
            fprintf(stderr, "INFO: pixelformat %c %c %c %c\n", GET_B(fmt.pixelformat),
                GET_G(fmt.pixelformat), GET_R(fmt.pixelformat), GET_A(fmt.pixelformat));

        if (app->video_format == VIDEO_FORMAT_YUV422 && fmt.pixelformat == V4L2_PIX_FMT_YUYV) {
            struct v4l2_frmsizeenum frmsize = {
                .pixel_format = fmt.pixelformat, .index = 0
            };
            CALL(res = ioctl_enum(app->v4l.dev_id, VIDIOC_ENUM_FRAMESIZES, &frmsize), cleanup);
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
                else if (frmsize.type == V4L2_FRMSIZE_TYPE_STEPWISE) {
                    if (
                        frmsize.discrete.width == app->video_width &&
                        frmsize.discrete.height == app->video_height
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
                CALL(res = ioctl_enum(app->v4l.dev_id, VIDIOC_ENUM_FRAMESIZES, &frmsize), cleanup);
            }
        }

        fmt.index++;
        CALL(res = ioctl_enum(app->v4l.dev_id, VIDIOC_ENUM_FMT, &fmt), cleanup);
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

int v4l_init(struct app_state_t *app)
{
    struct stat st;
    CALL(stat(app->v4l.dev_name, &st), cleanup);
    ASSERT_INT(S_ISCHR(st.st_mode), ==, 0, cleanup);
    app->v4l.dev_id = open(app->v4l.dev_name, O_RDWR | O_NONBLOCK, 0);
    if (app->v4l.dev_id == -1) {
        CALL_MESSAGE(open(app->v4l.dev_name, O_RDWR | O_NONBLOCK, 0), app->v4l.dev_id);
        goto cleanup;
    }
    return 0;
cleanup:
    if (errno == 0)
        errno = EAGAIN;
    return -1;
}

void v4l_cleanup(struct app_state_t *app)
{
    if (app->v4l.dev_id != -1)
        CALL(close(app->v4l.dev_id));
}
