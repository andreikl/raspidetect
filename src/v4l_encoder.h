#ifndef v4l_encoder_h
#define v4l_encoder_h

#define V4L_H264_ENCODER "/dev/nvhost-msenc"

struct v4l_encoder_state_t {
    struct app_state_t *app;

    char dev_name[20];
    int dev_id;

    uint8_t *v4l_buf;
    int v4l_buf_length;

    uint8_t *buffer;
    int buffer_length;
};

void v4l_encoder_construct(struct app_state_t *app);

#endif //v4l_encoder_h