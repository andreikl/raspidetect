#ifndef mmal_encoder_h
#define mmal_encoder_h

// Standard port setting for the camera component
/*#define MMAL_CAMERA_PREVIEW_PORT 0
#define MMAL_CAMERA_VIDEO_PORT 1
#define MMAL_CAMERA_CAPTURE_PORT 2

#include "interface/mmal/mmal.h"
#include "interface/mmal/mmal_logging.h"
#include "interface/mmal/mmal_buffer.h"
#include "interface/mmal/util/mmal_util.h"
#include "interface/mmal/util/mmal_util_params.h"
#include "interface/mmal/util/mmal_default_components.h"
#include "interface/mmal/util/mmal_connection.h"
#include "interface/mmal/mmal_parameters_camera.h"

struct mmal_state_t {
    MMAL_COMPONENT_T *camera;
    MMAL_COMPONENT_T *encoder_h264;

    MMAL_PORT_T *video_port;
    MMAL_POOL_T *video_port_pool;
    MMAL_PORT_T *h264_input_port;
    MMAL_POOL_T *h264_input_pool;
    MMAL_PORT_T *h264_output_port;
    MMAL_POOL_T *h264_output_pool;

    char *h264_buffer;
    int32_t h264_buffer_length;
    pthread_mutex_t h264_mutex;
    int is_h264_mutex;

    sem_t h264_semaphore;
    int is_h264_semaphore;

};

int mmal_get_capabilities(int camera_num, char *camera_name, int *width, int *height );
int camera_encode_buffer(char *buffer, int length);
int camera_open();
void camera_cleanup();
int camera_create_h264_encoder();
int camera_cleanup_h264_encoder();*/

#if !defined(MMAL_ENCODER_WRAP)
    #include "mmal.h"
    #include "bcm_host.h"
#else
    #include "mmal_encoder_stubs.h"
#endif

#define MMAL_UNKNOWN "Unknown"
#define MMAL_OUT_BUFFER_SIZE 51200

#define MMAL_STR_ERROR(res) \
({ \
    char* str = MMAL_UNKNOWN; \
    if (res >= 0 && res < ARRAY_SIZE(mmal_errors)) \
        str = mmal_errors[res]; \
    str; \
})

#define MMAL_MESSAGE(call, res) \
{ \
    fprintf(stderr, "\033[1;31m"#call" returned error: %s (%d)\n%s:%d - %s\033[0m\n", \
        MMAL_STR_ERROR(res), res, __FILE__, __LINE__, __FUNCTION__); \
}

#define MMAL_2(call, error) \
{ \
    int __res = call; \
    if (__res != 0) { \
        MMAL_MESSAGE(call, __res); \
        goto error; \
    } \
}

#define MMAL_1(call) \
{ \
    int __res = call; \
    if (__res != 0) { \
        MMAL_MESSAGE(call, __res); \
    } \
}

#define MMAL_X(...) GET_3RD_ARG(__VA_ARGS__, MMAL_2, MMAL_1, )

#define MMAL_CALL(...) MMAL_X(__VA_ARGS__)(__VA_ARGS__)

struct mmal_encoder_state_t {
    MMAL_COMPONENT_T *encoder;
    MMAL_PORT_T *input_port;
    MMAL_POOL_T *input_pool;
    MMAL_PORT_T *output_port;
    MMAL_POOL_T *output_pool;
    pthread_mutex_t mutex;
    int is_mutex;
    sem_t semaphore;
    int is_semaphore;
    uint8_t *out_buf;
    int out_buf_used;
};

void mmal_encoder_construct();

#endif //mmal_encoder_h