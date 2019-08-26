// Standard port setting for the camera component
#define MMAL_CAMERA_PREVIEW_PORT 0
#define MMAL_CAMERA_VIDEO_PORT 1
#define MMAL_CAMERA_CAPTURE_PORT 2

int get_sensor_defaults(int camera_num, char *camera_name, int *width, int *height );

int create_camera_component(APP_STATE *state);
int create_encoder_h264(APP_STATE *state);
//int create_encoder_rgb(APP_STATE *state);
//int create_preview_component(APP_STATE *state);

void destroy_components(APP_STATE *state);
