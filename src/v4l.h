#ifndef v4l_h
#define v4l_h

struct v4l_state_t {
    char dev_name[20];
    int dev_id;

    char *buffer;
    int buffer_length;
};

#endif //v4l_h