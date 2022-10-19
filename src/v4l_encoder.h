#ifndef v4l_encoder_h
#define v4l_encoder_h

#define V4L_H264_ENCODER "/dev/nvhost-msenc"
#define V4L_MAX_IN_BUFS 10
#define V4L_MAX_OUT_BUFS 3 //3 is the minimum nubmer of buffers
#define V4L_OUT_BUFFER_SIZE 10240

struct v4l_encoder_plane_t {
    uint8_t *buf;
    int fd;
};

struct v4l_encoder_state_t {
    char dev_name[20];
    int dev_id;

    int in_offsets[3];
    int in_lengths[3];
    int in_sizeimages[3];
    int in_strides[3];
    int out_sizeimages[1];
    int out_strides[1];
    int out_offsets[1];
    int out_lengths[1];

    struct v4l_encoder_plane_t in_bufs[V4L_MAX_IN_BUFS][3];
    struct v4l_encoder_plane_t out_bufs[V4L_MAX_OUT_BUFS];
    int in_bufs_count;
    int out_bufs_count;
    int in_curr_buf;

    uint8_t *out_buf;
    int out_buf_used;
};

void v4l_encoder_construct();

#endif //v4l_encoder_h