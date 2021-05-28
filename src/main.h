#ifndef main_h
#define main_h

#define WIDTH "-w"
#define WIDTH_DEF 640
#define HEIGHT "-h"
#define HEIGHT_DEF 480
#define PORT "-p"
#define PORT_DEF 5900
#define HELP "--help"

#define WORKER_WIDTH "-ww"
#define WORKER_WIDTH_DEF 300
#define WORKER_HEIGHT "-wh"
#define WORKER_HEIGHT_DEF 300

#define APP_NAME "raspidetect\0"
#define INPUT "-i"
#define INPUT_DEF "camera"
#define OUTPUT "-o"
#define OUTPUT_DEF "none"
#define VERBOSE "-d"
#define VERBOSE_DEF 1
#define TFL_MODEL_PATH "-m"
#define TFL_MODEL_PATH_DEF "./tflite_models/detect.tflite"
#define DN_MODEL_PATH "-m"
#define DN_MODEL_PATH_DEF "./dn_models/yolov3-tiny.weights"
#define DN_CONFIG_PATH "-c"
#define DN_CONFIG_PATH_DEF "./dn_models/yolov3-tiny.cfg"
#define ARG_STREAM "-"
#define ARG_RFB "rfb"
#define ARG_NONE "none"

#define THRESHOLD 0.5
#define MAX_STRING 256
#define MAX_DATA 1024
#define TICK_TIME 500000 //500 miliseconds

#define FONT_DIR "."
#define FONT_NAME "Vera.ttf"
#define FONT_PATH FONT_DIR"/"FONT_NAME

// Check windows
#if _WIN32 || _WIN64
    #if _WIN64
        #define ENV64BIT
    #else
        #define ENV32BIT
    #endif
#endif

// Check GCC
#if __GNUC__
    #if __x86_64__ || __ppc64__
        #define ENV64BIT
    #else
        #define ENV32BIT
    #endif
#endif

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

#define DEBUG_INT(text, value) \
{ \
    fprintf(stderr, "INFO: %s, "#text": %d\n", \
        __FUNCTION__, \
        value); \
}

#define DEBUG_POINTER(text, value) \
{ \
    fprintf(stderr, "INFO: %s, "#text": %p\n", \
        __FUNCTION__, \
        value); \
}

#define DEBUG_STR(text, value) \
{ \
    fprintf(stderr, "INFO: %s, "#text": %s\n", \
        __FUNCTION__, \
        value); \
}

#define CALL_MESSAGE(call, res) \
{ \
    fprintf(stderr, "ERROR: "#call" returned error: %s (%d)\n%s:%d - %s\n", \
        strerror(res), res, __FILE__, __LINE__, __FUNCTION__); \
}

#define CALL_2(call, error) \
{ \
    int res = call; \
    if (res) { \
        CALL_MESSAGE(call, res); \
        goto error; \
    } \
}

#define CALL_1(call) \
{ \
    int res = call; \
    if (res) { \
        CALL_MESSAGE(call, res); \
    } \
}

#define CALL_X(...) GET_3RD_ARG(__VA_ARGS__, CALL_2, CALL_1, )

#define CALL(...) CALL_X(__VA_ARGS__)(__VA_ARGS__)

#include <stdio.h>     // fprintf
#include <unistd.h>    // STDIN_FILENO
#include <time.h>      // time_t
#include <semaphore.h>
#include <errno.h>     // error codes
#include <signal.h>    // SIGUSR1
#include <string.h>    // memcpy
#include <sys/stat.h>  // stat
#include <fcntl.h>     // O_RDWR | O_NONBLOCK

struct cpu_state_t {
    float cpu;
};

struct memory_state_t {
    // memory status
    int total_size;
    int swap_size; 
    int pte_size;
    int lib_size;
    int exe_size;
    int stk_size;
    int data_size;
};

struct temperature_state_t {
    float temp;
};

#ifdef MMAL
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
#elif defined(V4L)
#include "linux/videodev2.h"
struct v4l_state_t {
    char dev_name[20];
    int dev_id;
};
#endif

#ifdef TENSORFLOW
#include "tensorflow/lite/experimental/c/c_api.h"

struct tensorflow_state_t {
    TfLiteModel *tf_model;
    TfLiteInterpreterOptions *tf_options;
    TfLiteInterpreter *tf_interpreter;
    TfLiteTensor *tf_input_image;

    const TfLiteTensor *tf_tensor_boxes;
    const TfLiteTensor *tf_tensor_classes;
    const TfLiteTensor *tf_tensor_scores;
    const TfLiteTensor *tf_tensor_num_detections;

};
#elif DARKNET
#include "include/darknet.h"

struct darknet_state_t {
    network *dn_net;
};
#endif

#ifdef OPENVG
#include "EGL/egl.h"
#include "VG/openvg.h"
#include <ft2build.h>
#include FT_FREETYPE_H
/**
 * Structure used to store an EGL client state. 
 ***********************************************************/
struct openvg_state_t {
    EGLDisplay display;
    EGLContext context;
    EGLSurface surface;
    union {
        int* i;
        char* c;
    } video_buffer;
    union {
        EGL_DISPMANX_WINDOW_T native_window;
        VGImage pixmap;
    } u;

    int egl_maj;
    int egl_min;

    FT_Library font_lib;
    void* font_data;
    size_t font_len;
};
#endif //OPENVG

#ifdef CONTROL
struct control_state_t {
    volatile unsigned *gpio;
};
#endif //CONTROL

#ifdef RFB
struct rfb_state_t {
    pthread_t thread;
    int thread_res;
    int serv_socket;
    int client_socket;
};
#endif //RFB

enum output_enum_t {
    OUTPUT_NONE,
    OUTPUT_STREAM,
    OUTPUT_RFB
};

struct app_state_t {
    // camera properties
    int camera_num;               // Camera number
    char camera_name[MAX_STRING]; // Name of the camera sensor
    int camera_max_width;         // camera max width
    int camera_max_height;        // camera max height

    // window properties
    unsigned window_width;
    unsigned window_height;

    // common properties
    int width;
    int height;
    int bits_per_pixel;
    int port;
    char *filename;                     // name of output file
    float fps;
    int verbose;                        // debug
    enum output_enum_t output_type;
    char* model_path;
    char* config_path;
    volatile unsigned *gpio;
    float rfb_fps;

    pthread_mutex_t buffer_mutex;
    sem_t buffer_semaphore;

    sem_t worker_semaphore;
    pthread_t worker_thread;
    int worker_thread_res;
    int worker_width;
    int worker_height;
    int worker_bits_per_pixel;
    char *worker_buffer_565;
    char *worker_buffer_rgb;
    int worker_objects;
    float worker_fps;
    int worker_total_objects;
    float *worker_boxes;
    float *worker_classes;
    float *worker_scores;

#ifdef CONTROL
    struct control_state_t control;
#endif

#ifdef MMAL
    struct mmal_state_t mmal;
#elif defined (V4L)
    struct v4l_state_t v4l;
#endif

#ifdef TENSORFLOW
    struct tensorflow_state_t tf;
#elif DARKNET
    struct darknet_state_t dn;
#endif

#ifdef OPENVG
    struct openvg_state_t openvg;
#endif

#ifdef RFB
	struct rfb_state_t rfb;
#endif

    struct temperature_state_t temperature;
    struct memory_state_t memory;
    struct cpu_state_t cpu;
};

#endif // main_h