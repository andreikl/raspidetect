// Standard port setting for the camera component
#define MMAL_CAMERA_PREVIEW_PORT 0
#define MMAL_CAMERA_VIDEO_PORT 1
#define MMAL_CAMERA_CAPTURE_PORT 2

int mmal_get_capabilities(int camera_num, char *camera_name, int *width, int *height );
int camera_encode_buffer(app_state_t *app, char *buffer, int length);
int camera_open(app_state_t *app);
void camera_cleanup(app_state_t *app);
int camera_create_h264_encoder(app_state_t *app);
int camera_cleanup_h264_encoder(app_state_t *app);
