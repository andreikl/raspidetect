#include "main.h"

int dispmanx_init(app_state_t *state);
void dispmanx_destroy(app_state_t *state);
int openvg_init(app_state_t *state);
void openvg_destroy(app_state_t *state);

int openvg_draw_boxes(app_state_t *state, VGfloat colour[4]);
int openvg_draw_text(app_state_t *state,
                  float x,
                  float y,
                  const char *text,
                  uint32_t text_length,
                  uint32_t text_size,
                  VGfloat vg_colour[4]);

int openvg_read_buffer(app_state_t *state);