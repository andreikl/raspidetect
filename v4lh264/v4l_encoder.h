#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

#define GET_3RD_ARG(arg1, arg2, arg3, ...) arg3

#define ASSERT_INT(value, condition, expectation, error) \
{ \
    if (value condition expectation) { \
        fprintf(stderr, "ERROR: assert "#value"(%d) "#condition" "#expectation"(%d)\n%s:%d - %s\n", \
            value, expectation, __FILE__, __LINE__, __FUNCTION__); \
        goto error; \
    } \
}

#define ASSERT_LNG(value, condition, expectation, error) \
{ \
    if (value condition expectation) { \
        fprintf(stderr, "ERROR: assert "#value"(%ld) "#condition" "#expectation"(%ld)\n%s:%d - %s\n", \
            value, (long int)expectation, __FILE__, __LINE__, __FUNCTION__); \
        goto error; \
    } \
}

#define ASSERT_PTR(value, condition, expectation, error) \
{ \
    if (value condition expectation) { \
        fprintf(stderr, "ERROR: assert "#value"(%p) "#condition" "#expectation"(%p)\n%s:%d - %s\n", \
            value, expectation, __FILE__, __LINE__, __FUNCTION__); \
        goto error; \
    } \
}

#define DEBUG(format, ...) \
{ \
    fprintf(stderr, "%s:%s, "#format"\n", __FILE__, __FUNCTION__, ##__VA_ARGS__); \
}

#define CALL_MESSAGE(call) \
{ \
    fprintf(stderr, "ERROR: "#call" returned error: %s (%d)\n%s:%d - %s\n", \
        strerror(errno), errno, __FILE__, __LINE__, __FUNCTION__); \
}

#define CALL_CUSTOM_MESSAGE(call, res) \
{ \
    fprintf(stderr, "ERROR: "#call" returned error: (%d)\n%s:%d - %s\n", \
        res, __FILE__, __LINE__, __FUNCTION__); \
}

#define CALL_2(call, error) \
{ \
    int __res = call; \
    if (__res == -1) { \
        CALL_MESSAGE(call); \
        goto error; \
    } \
}

#define CALL_1(call) \
{ \
    int __res = call; \
    if (__res == -1) { \
        CALL_MESSAGE(call); \
    } \
}

#define CALL_X(...) GET_3RD_ARG(__VA_ARGS__, CALL_2, CALL_1, )

#define CALL(...) CALL_X(__VA_ARGS__)(__VA_ARGS__)

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

#include <stdint.h>    //uint32_t
#include <stdio.h>     // fprintf
#include <stdlib.h>    // malloc, free
//#include <unistd.h>  // STDIN_FILENO, usleep
#include <time.h>      // time_t, timespec
#include <semaphore.h>
#include <errno.h>     // error codes
#include <signal.h>    // SIGUSR1
#include <string.h>    // memcpy
#include <sys/stat.h>  // stat
#include <sys/select.h> //select
#include <fcntl.h>     // O_RDWR | O_NONBLOCK

#define ENCODER_DEV "/dev/nvhost-msenc"
#define V4L_MAX_IN_BUFS 10
#define V4L_MAX_OUT_BUFS 6

struct v4l_encoder_plane_t {
    void *buf;
    int len;
    int offset;
    int fd;
};

struct v4l_encoder_state_t {
    ///-------------- to delete -------

    //uint32_t encode_pixfmt;
    //uint32_t raw_pixfmt;

    //uint32_t num_queued_outplane_buffers;
    //uint32_t num_queued_capplane_buffers;

    //enum v4l2_memory outplane_mem_type;
    //enum v4l2_memory capplane_mem_type;
    //enum v4l2_buf_type outplane_buf_type;
    //enum v4l2_buf_type capplane_buf_type;

    //Buffer **capplane_buffers;

    //string input_file_path;
    //ifstream *input_file;

    const char* output_file_path;
    //ofstream *output_file;

    /*pthread_mutex_t queue_lock;
    pthread_cond_t queue_cond;
    pthread_t enc_dq_thread;*/

    uint32_t in_error;
    uint32_t eos;
    //bool dqthread_running;

    ///-------------- to delete -------
    int dev_id;
    uint32_t video_width;
    uint32_t video_height;

    int out_sizeimages[1];
    int out_strides[1];
    int in_sizeimages[3];
    int in_strides[3];

    struct v4l_encoder_plane_t in_bufs[V4L_MAX_IN_BUFS][3];
    struct v4l_encoder_plane_t out_bufs[V4L_MAX_OUT_BUFS];
    int in_bufs_count;
    int out_bufs_count;
};

int encoder_process_blocking();
