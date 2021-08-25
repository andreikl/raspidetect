#ifndef main_h
#define main_h

#define VIDEO_FORMAT_UNKNOWN_STR "Unknown"
#define VIDEO_FORMAT_YUV422_STR  "YUV422"
#define VIDEO_FORMAT_UNKNOWN 0
#define VIDEO_FORMAT_YUV422  1
#define VIDEO_FORMAT_YUV444M 2 //three separate planes - Y, Cb, Cr
#define VIDEO_FORMAT_H264    3

#define VIDEO_OUTPUT_NULL_STR   "null"
#define VIDEO_OUTPUT_STDOUT_STR "stdout"
#define VIDEO_OUTPUT_SDL_STR    "sdl"
#define VIDEO_OUTPUT_RFB_STR    "rfb"

#define MAX_OUTPUTS   3
#define MAX_FILTERS   3
#define VIDEO_OUTPUT_NULL   0
#define VIDEO_OUTPUT_STDOUT 1
#define VIDEO_OUTPUT_SDL    2
#define VIDEO_OUTPUT_RFB    4

#define VIDEO_WIDTH "-w"
#define VIDEO_WIDTH_DEF 640
#define VIDEO_HEIGHT "-h"
#define VIDEO_HEIGHT_DEF 480
#define VIDEO_FORMAT "-f"
#define VIDEO_FORMAT_DEF VIDEO_FORMAT_YUV422_STR
#define VIDEO_OUTPUT "-o"
#define VIDEO_OUTPUT_DEF VIDEO_OUTPUT_SDL_STR","VIDEO_OUTPUT_RFB_STR

#define PORT "-p"
#define PORT_DEF 5900
#define HELP "--help"

#define WORKER_WIDTH "-ww"
#define WORKER_WIDTH_DEF 300
#define WORKER_HEIGHT "-wh"
#define WORKER_HEIGHT_DEF 300

#define APP_NAME "raspidetect\0"
#define VERBOSE "-d"
#define VERBOSE_DEF 1
#define TFL_MODEL_PATH "-m"
#define TFL_MODEL_PATH_DEF "./tflite_models/detect.tflite"
#define DN_MODEL_PATH "-m"
#define DN_MODEL_PATH_DEF "./dn_models/yolov3-tiny.weights"
#define DN_CONFIG_PATH "-c"
#define DN_CONFIG_PATH_DEF "./dn_models/yolov3-tiny.cfg"

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
    fprintf(stderr, "INFO: %s, "#format"\n", __FUNCTION__, ##__VA_ARGS__); \
}

#define CALL_MESSAGE(call, res) \
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
        CALL_MESSAGE(call, errno); \
        goto error; \
    } \
}

#define CALL_1(call) \
{ \
    int __res = call; \
    if (__res == -1) { \
        CALL_MESSAGE(call, errno); \
    } \
}

#define CALL_X(...) GET_3RD_ARG(__VA_ARGS__, CALL_2, CALL_1, )

#define CALL(...) CALL_X(__VA_ARGS__)(__VA_ARGS__)

#define LAMBDA(LAMBDA$_ret, LAMBDA$_args, LAMBDA$_body) \
({ \
    LAMBDA$_ret LAMBDA$__anon$ LAMBDA$_args \
    LAMBDA$_body \
    LAMBDA$__anon$; \
})

#include <stdint.h>    //uint32_t
#include <stdio.h>     // fprintf
#include <stdlib.h>    // malloc, free
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

struct format_mapping_t {
    int format;
    int internal_format;
    int is_supported;
};

struct filter_reference_t {
    int out_format;
    int index;
};

struct input_t {
    char* name;
    void *context;

    int (*init)();
    int (*start)(int format);
    int (*process_frame)();
    int (*stop)();
    void (*cleanup)();

    char* (*get_buffer)();
    int (*get_formats)(const struct format_mapping_t *formats[]);
};

struct filter_t {
    char* name;
    void *context;
    int (*init)();
    int (*start)(int input_format, int output_format);
    int (*process)(char *buffer);
    int (*stop)();    
    void (*cleanup)();

    char* (*get_buffer)();
    int (*get_in_formats)(const struct format_mapping_t *formats[]);
    int (*get_out_formats)(const struct format_mapping_t *formats[]);
};

struct output_t {
    char* name;
    void *context;

    int start_format;
    struct filter_reference_t filters[MAX_FILTERS];

    int (*init)();
    int (*render)(char *buffer);
    void (*cleanup)();
    int (*get_formats)(const struct format_mapping_t *formats[]);
};

struct app_state_t {
    // camera properties
    int camera_num;               // Camera number
    char camera_name[MAX_STRING]; // Name of the camera sensor
    int camera_max_width;         // camera max width
    int camera_max_height;        // camera max height

    // common properties
    int video_width;
    int video_height;
    int video_format;
    int video_output;

    int port;
    char *filename;                     // name of output file
    float fps;
    int verbose;                        // debug
    char* model_path;
    char* config_path;
    volatile unsigned *gpio;
    float rfb_fps;

    // window properties
    unsigned window_width;
    unsigned window_height;

    pthread_mutex_t buffer_mutex;
    sem_t buffer_semaphore;

    sem_t worker_semaphore;
    pthread_t worker_thread;
    int worker_thread_res;
    int worker_width;
    int worker_height;
    char *worker_buffer_565;
    char *worker_buffer_rgb;
    int worker_objects;
    float worker_fps;
    int worker_total_objects;
    float *worker_boxes;
    float *worker_classes;
    float *worker_scores;

    struct input_t input;

#ifdef CONTROL
    struct control_state_t control;
#endif

#ifdef TENSORFLOW
    struct tensorflow_state_t tf;
#elif DARKNET
    struct darknet_state_t dn;
#endif

#ifdef OPENVG
    struct openvg_state_t openvg;
#endif

    struct temperature_state_t temperature;
    struct memory_state_t memory;
    struct cpu_state_t cpu;
};

#endif // main_h