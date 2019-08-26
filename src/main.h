#ifndef main_h
#define main_h

#define WIDTH "-w"
#define WIDTH_DEF 640
#define HEIGHT "-h"
#define HEIGHT_DEF 480
#define HELP "--help"

#define WORKER_WIDTH "-ww"
#define WORKER_WIDTH_DEF 300
#define WORKER_HEIGHT "-wh"
#define WORKER_HEIGHT_DEF 300

#define INPUT "-i"
#define INPUT_DEF "-"
#define OUTPUT "-o"
#define OUTPUT_DEF "-"
#define VERBOSE "-d"
#define VERBOSE_DEF 1
#define TFL_MODEL_PATH "-m"
#define TFL_MODEL_PATH_DEF "./tflite_models/detect.tflite"
#define DN_MODEL_PATH "-m"
#define DN_MODEL_PATH_DEF "./dn_models/yolov3-tiny.weights"
#define DN_CONFIG_PATH "-c"
#define DN_CONFIG_PATH_DEF "./dn_models/yolov3-tiny.cfg"

#define THRESHOLD 0.3

#define BUFFER_SIZE 1024
#define TICK_TIME 500000 //500 miliseconds

#include "semaphore.h"

#include "cairo/cairo.h"

#include "interface/mmal/mmal.h"
#include "interface/mmal/mmal_logging.h"
#include "interface/mmal/mmal_buffer.h"
#include "interface/mmal/util/mmal_util.h"
#include "interface/mmal/util/mmal_util_params.h"
#include "interface/mmal/util/mmal_default_components.h"
#include "interface/mmal/util/mmal_connection.h"
#include "interface/mmal/mmal_parameters_camera.h"

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

typedef struct {
    float cpu;
} CPU_STATE;

typedef struct {
    // memory status
    int total_size;
    int swap_size; 
    int pte_size;
    int lib_size;
    int exe_size;
    int stk_size;
    int data_size;
} MEMORY_STATE;

typedef struct {
    float temp;
} TEMPERATURE_STATE;

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

} TENSORFLOW_STATE;
#elif DARKNET
#include "include/darknet.h"

typedef struct {
    network *dn_net;
} DARKNET_STATE;
#endif

typedef struct {
    // common properties
    int width;                          // Requested width of image
    int height;                         // requested height of image
    char *filename;                     // filename of output file
    int camera_num;                     // Camera number
    int verbose;                        // debug
    float fps;
    char* model_path;
    char* config_path;
    // debug running time value
    int rtime;

    pthread_mutex_t buffer_mutex;

    // camera properties
    char camera_name[MMAL_PARAMETER_CAMERA_INFO_MAX_STR_LEN]; // Name of the camera sensor
    // camera width
    int max_width;
    // camera height
    int max_height;
    MMAL_COMPONENT_T *camera;
    MMAL_COMPONENT_T *encoder_h264;
    //MMAL_COMPONENT_T *encoder_rgb;

    MMAL_PORT_T *video_port;
    MMAL_POOL_T *video_port_pool;
    //MMAL_PORT_T *preview_port;
    //MMAL_POOL_T *preview_port_pool;
    //MMAL_PORT_T *still_port;
    MMAL_PORT_T *h264_input_port;
    MMAL_POOL_T *h264_input_pool;
    MMAL_PORT_T *h264_output_port;
    MMAL_POOL_T *h264_output_pool;
    /*MMAL_PORT_T *rgb_input_port;
    MMAL_POOL_T *rgb_input_pool;
    MMAL_PORT_T *rgb_output_port;
    MMAL_POOL_T *rgb_output_pool;*/

    // overlay properties
    cairo_surface_t *cairo_surface;
    cairo_t *cairo_context;
    uint8_t *overlay_buffer;
    int overlay_width;
    int overlay_height;
    int overlay_stride;

    sem_t worker_semaphore;
    pthread_t worker_thread;
    int worker_width;
    int worker_height;
    int worker_pixel_bytes;
    char *worker_buffer;
    int worker_objects;
    int worker_total_objects;
    float *worker_boxes;
    float *worker_classes;
    float *worker_scores;

#ifdef TENSORFLOW
    TENSORFLOW_STATE tf;
#elif DARKNET
    DARKNET_STATE dn;
#endif

    TEMPERATURE_STATE temperature;
    MEMORY_STATE memory;
    CPU_STATE cpu;
} APP_STATE;

#endif // main_h