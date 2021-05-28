#include "main.h"

int v4l_get_capabilities(struct app_state_t *app)
{
    int res;
    struct v4l2_capability cap;
    CALL(res = ioctl(app->v4l.dev_name, VIDIOC_QUERYCAP, &cap), cleanup);
    ASSERT_INT((cap.capabilities & V4L2_CAP_VIDEO_CAPTURE), ==, 0, cleanup)
    return 0;
cleanup:
    return res;
}

int v4l_init(struct app_state_t *app)
{
    int res;
    struct stat st;
    CALL(res = stat(app->v4l.dev_name, &st), cleanup);
    ASSERT_INT(S_ISCHR(st.st_mode), ==, 0, cleanup);
    app->v4l.dev_id = open(app->v4l.dev_name, O_RDWR | O_NONBLOCK, 0);
    if (app->v4l.dev_id == -1) {
        CALL_MESSAGE(open(app->v4l.dev_name, O_RDWR | O_NONBLOCK, 0), app->v4l.dev_id);
        goto cleanup;
    }
    return 0;
cleanup:
    return res;
}

void v4l_cleanup(struct app_state_t *app)
{
    if (app->v4l.dev_id != -1)
        CALL(close(app->v4l.dev_id));
}
