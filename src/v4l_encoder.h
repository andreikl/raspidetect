#ifndef v4l_encoder_h
#define v4l_encoder_h

#define V4L_H264_ENCODER "/dev/nvhost-msenc"
#define V4L_PLANES_SIZE 3

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

    struct v4l_encoder_plane_t in_planes[3];
    struct v4l_encoder_plane_t out_plane;
};

void v4l_encoder_construct(struct app_state_t *app);

#endif //v4l_encoder_h