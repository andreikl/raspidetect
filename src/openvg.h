#include "main.h"

int dispmanx_init();
void dispmanx_destroy();
int openvg_init();
void openvg_destroy();

int openvg_draw_boxes(VGfloat colour[4]);
int openvg_draw_text(float x, float y,
                  const char *text,
                  uint32_t text_length,
                  uint32_t text_size,
                  VGfloat vg_colour[4]);

int openvg_read_buffer();