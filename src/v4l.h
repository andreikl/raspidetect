#ifndef v4l_h
#define v4l_h

struct v4l_state_t {
    char dev_name[20];
    int dev_id;

    uint8_t *v4l_buf;
    int v4l_buf_len;

    uint8_t *buffer;
    int buffer_len;
};

void v4l_construct();

#endif //v4l_h