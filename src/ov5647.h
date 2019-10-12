// Standard port setting for the camera component
#define MMAL_CAMERA_PREVIEW_PORT 0
#define MMAL_CAMERA_VIDEO_PORT 1
#define MMAL_CAMERA_CAPTURE_PORT 2

int get_sensor_defaults(int camera_num, char *camera_name, int *width, int *height );

void encode_buffer(app_state_t *state, char *buffer, int length);

int create_camera_component(app_state_t *state);
int create_encoder_h264(app_state_t *state);

void destroy_components(app_state_t *state);
