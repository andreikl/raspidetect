#ifndef v4l_encoder_h
#define v4l_encoder_h

struct v4l_encoder_state_t {
    char dev_name[20];
    int dev_id;

    char *v4l_buf;
    int v4l_buf_length;

    char *buffer;
    int buffer_length;
};

void v4l_encoder_construct(struct app_state_t *app);

#endif //v4l_encoder_h