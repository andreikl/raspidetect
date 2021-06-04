#ifndef mmal_h
#define mmal_h

// Standard port setting for the camera component
#define MMAL_CAMERA_PREVIEW_PORT 0
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
int camera_encode_buffer(app_state_t *app, char *buffer, int length);
int camera_open(app_state_t *app);
void camera_cleanup(app_state_t *app);
int camera_create_h264_encoder(app_state_t *app);
int camera_cleanup_h264_encoder(app_state_t *app);

#endif //mmal_h