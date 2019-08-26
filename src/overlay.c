#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <memory.h>
#include <unistd.h>
#include <errno.h>
#include <sysexits.h>

#include "main.h"

int overlay_create(APP_STATE *state) {
    state->overlay_stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, state->overlay_width);
    state->overlay_buffer = malloc(state->overlay_stride * state->overlay_height);
    state->cairo_surface = cairo_image_surface_create_for_data(state->overlay_buffer,
        CAIRO_FORMAT_ARGB32,
        state->overlay_width,
        state->overlay_height,
        state->overlay_stride);
    if (!state->cairo_surface) {
        fprintf(stderr, "ERROR: Failed to create cairo surface");
        return -1;
    }
    state->cairo_context = cairo_create(state->cairo_surface);
    if (!state->cairo_context) {
        fprintf(stderr, "ERROR: Failed to create cairo context");
        return -1;
    }
    if (state->verbose) {
        fprintf(stderr, "INFO: overlay_stride %d\n", state->overlay_stride);
    }

    return 0;
}

void overlay_print(APP_STATE *state, const char *text) {
    pthread_mutex_lock(&state->buffer_mutex);
    cairo_rectangle(state->cairo_context, 0.0, 0.0, state->overlay_width, state->overlay_height);
    cairo_set_source_rgba(state->cairo_context, 0.0, 0.0, 0.0, 1.0);
    cairo_fill(state->cairo_context);

    cairo_set_source_rgba(state->cairo_context, 1.0, 1.0, 1.0, 1.0);
    cairo_set_line_width(state->cairo_context, 1);
    cairo_move_to(state->cairo_context, 0.0, 10.0);
    cairo_set_font_size(state->cairo_context, 10.0);
    cairo_show_text(state->cairo_context, text);

    int n = 0;
    for (int i = 0; i < state->worker_total_objects; i++) {
        if(state->worker_scores[i] > THRESHOLD) {
            n++;
            float *box = &state->worker_boxes[i * 4];
            int x1 = (int)(box[0] * state->width);
            int y1 = (int)(box[0] * state->height);
            int x2 = (int)(box[2] * state->width);
            int y2 = (int)(box[3] * state->height);
            cairo_move_to(state->cairo_context, x1, y1);
            cairo_line_to(state->cairo_context, x2, y1);
            cairo_line_to(state->cairo_context, x2, y2);
            cairo_line_to(state->cairo_context, x1, y2);
            cairo_line_to(state->cairo_context, x1, y1);
            cairo_stroke(state->cairo_context);
        }
    }
    state->worker_objects = n;

    pthread_mutex_unlock(&state->buffer_mutex);
}

void overlay_destroy(APP_STATE *state) {
    if (state->cairo_context != NULL) {
        cairo_destroy(state->cairo_context);
    }
    if (state->cairo_surface != NULL) {
        cairo_surface_destroy(state->cairo_surface);
    }
    if (state->cairo_surface != NULL) {
        free(state->overlay_buffer);
    }
}
