#ifndef v4l_encoder_h
#define v4l_encoder_h

#define V4L_H264_ENCODER "/dev/nvhost-msenc"
#define V4L_MAX_IN_BUFS 4
#define V4L_MAX_OUT_BUFS 3

#define V4L_CALL_2(call, error) \
{ \
    int __res = call; \
    if (__res != 0) { \
        CALL_MESSAGE(call); \
        goto error; \
    } \
}

#define V4L_CALL_1(call) \
{ \
    int __res = call; \
    if (__res != 0) { \
        CALL_MESSAGE(call); \
    } \
}

#define V4L_CALL_X(...) GET_3RD_ARG(__VA_ARGS__, V4L_CALL_2, V4L_CALL_1, )

#define V4L_CALL(...) V4L_CALL_X(__VA_ARGS__)(__VA_ARGS__)

struct v4l_encoder_plane_t {
    uint8_t *buf;
    int len;
    int offset;
    int sizeimage;
    int stride;
    int fd;
};

struct v4l_encoder_state_t {
    struct app_state_t *app;

    char dev_name[20];
    int dev_id;

    struct v4l_encoder_plane_t in_bufs[V4L_MAX_IN_BUFS][3];
    struct v4l_encoder_plane_t out_bufs[V4L_MAX_OUT_BUFS];
    int in_bufs_count;
    int out_bufs_count;
    int in_curr_buf;
};

void v4l_encoder_construct(struct app_state_t *app);

#endif //v4l_encoder_h