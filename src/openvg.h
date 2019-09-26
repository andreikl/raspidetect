#include "EGL/egl.h"
#include "VG/openvg.h"

int dispmanx_init(APP_STATE *state);
void dispmanx_destroy(APP_STATE *state);
int openvg_init(APP_STATE *state);
void openvg_destroy(APP_STATE *state);

int openvg_draw_boxes(APP_STATE *state, VGfloat colour[4]);
int openvg_draw_text(APP_STATE *state,
                  float x,
                  float y,
                  const char *text,
                  uint32_t text_length,
                  uint32_t text_size,
                  VGfloat vg_colour[4]);
