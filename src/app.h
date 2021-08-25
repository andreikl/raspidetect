#ifndef app_h
#define app_h

#include "main.h"

const char* app_get_video_format_str(int format);
int app_get_video_format_int(const char* format);
const char* app_get_video_output_str(int format);
int app_get_video_output_int(const char* format);

void app_set_default_state(struct app_state_t *app);
void app_construct(struct app_state_t *app);
void app_cleanup(struct app_state_t *app);
int app_init(struct app_state_t *app);

#endif //app_h
