// Standard port setting for the camera component
#define MMAL_CAMERA_PREVIEW_PORT 0
#define MMAL_CAMERA_VIDEO_PORT 1
#define MMAL_CAMERA_CAPTURE_PORT 2

int mmal_get_defaults(int camera_num, char *camera_name, int *width, int *height );

int mmal_encode_buffer(app_state_t *app, char *buffer, int length);

int mmal_create(app_state_t *app);
int mmal_cleanup(app_state_t *app);

int mmal_create_h264_encoder(app_state_t *app);
int mmal_cleanup_h264_encoder(app_state_t *app);
