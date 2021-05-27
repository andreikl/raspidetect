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
#define TEXT_SIZE 256
#define BUFFER_SIZE 1024
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

#include <stdio.h>     // fprintf
#include <time.h>      // time_t
#include <semaphore.h>
#include <errno.h>     // error codes
#include <signal.h>    // SIGUSR1

typedef struct {
    float cpu;
} cpu_state_t;

typedef struct {
    // memory status
    int total_size;
    int swap_size; 
    int pte_size;
    int lib_size;
    int exe_size;
    int stk_size;
    int data_size;
} memory_state_t;

typedef struct {
    float temp;
} temperature_state_t;

#ifdef MMAL
#include "interface/mmal/mmal.h"
#include "interface/mmal/mmal_logging.h"
#include "interface/mmal/mmal_buffer.h"
#include "interface/mmal/util/mmal_util.h"
#include "interface/mmal/util/mmal_util_params.h"
#include "interface/mmal/util/mmal_default_components.h"
#include "interface/mmal/util/mmal_connection.h"
#include "interface/mmal/mmal_parameters_camera.h"

typedef struct {
    // camera properties
    char camera_name[TEXT_SIZE]; // Name of the camera sensor
    int max_width; // camera max width
    int max_height; // camera max height
    int camera_num; // Camera number
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

} mmal_state_t;
#endif //MMAL

#ifdef TENSORFLOW
#include "tensorflow/lite/experimental/c/c_api.h"

typedef struct {
    TfLiteModel *tf_model;
    TfLiteInterpreterOptions *tf_options;
    TfLiteInterpreter *tf_interpreter;
    TfLiteTensor *tf_input_image;

    const TfLiteTensor *tf_tensor_boxes;
    const TfLiteTensor *tf_tensor_classes;
    const TfLiteTensor *tf_tensor_scores;
    const TfLiteTensor *tf_tensor_num_detections;

} tensorflow_state_t;
#elif DARKNET
#include "include/darknet.h"

typedef struct {
    network *dn_net;
} darknet_state_t;
#endif

#ifdef OPENVG
#include "EGL/egl.h"
#include "VG/openvg.h"
#include <ft2build.h>
#include FT_FREETYPE_H
/**
 * Structure used to store an EGL client state. 
 ***********************************************************/
typedef struct {
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

    unsigned int display_width;
    unsigned int display_height;

    FT_Library font_lib;
    void* font_data;
    size_t font_len;
} openvg_state_t;
#endif //OPENVG

#ifdef CONTROL
typedef struct {
    volatile unsigned *gpio;
} control_state_t;
#endif //CONTROL

#ifdef RFB
typedef struct {
    pthread_t thread;
    int thread_res;
    int serv_socket;
    int client_socket;
} rfb_state_t;
#endif //RFB

typedef enum {
    OUTPUT_NONE,
    OUTPUT_STREAM,
    OUTPUT_RFB
} output_enum_t;

typedef struct {
    // common properties
    int width;
    int height;
    int bits_per_pixel;
    int port;
    char *filename;                     // name of output file
    float fps;
    int verbose;                        // debug
    output_enum_t output_type;
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
    control_state_t control;
#endif

#ifdef MMAL
    mmal_state_t mmal;
#endif

#ifdef TENSORFLOW
    tensorflow_state_t tf;
#elif DARKNET
    darknet_state_t dn;
#endif

#ifdef OPENVG
    openvg_state_t openvg;
#endif

#ifdef RFB
	rfb_state_t rfb;
#endif

    temperature_state_t temperature;
    memory_state_t memory;
    cpu_state_t cpu;
} app_state_t;

#endif // main_h