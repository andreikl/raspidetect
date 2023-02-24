#ifndef app_h
#define app_h

#include "main.h"

const char* app_get_video_format_str(int format);
const char* app_get_video_output_str(int format);
int app_get_video_output_int(const char* format);

void app_set_default_state();
void app_construct();
void app_cleanup();
int app_init();

#endif //app_h
