#ifndef mmal_encoder_h
#define mmal_encoder_h

#if !defined(MMAL_ENCODER_WRAP)
    #include "bcm_host.h"
    #include "interface/mmal/mmal.h"
    #include "interface/mmal/util/mmal_util.h"
    #include "interface/mmal/util/mmal_default_components.h"
#else
    #include "mmal_encoder_stubs.h"
#endif

#define MMAL_UNKNOWN "Unknown"
#define MMAL_OUT_BUFFER_SIZE 65536

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
    uint8_t *mmal_buf;
    uint8_t *out_buf;
    int mmal_buf_used;
};

void mmal_encoder_construct();

#endif //mmal_encoder_h