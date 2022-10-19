// Raspidetect

// Copyright (C) 2021 Andrei Klimchuk <andrew.klimchuk@gmail.com>

// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 3 of the License, or (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.

// You should have received a copy of the GNU Lesser General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#include "main.h"

int overlay_create()
{
    app.overlay_stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, app.overlay_width);
    app.overlay_buffer = malloc(app.overlay_stride * app.overlay_height);
    app.cairo_surface = cairo_image_surface_create_for_data(app.overlay_buffer,
        CAIRO_FORMAT_ARGB32,
        app.overlay_width,
        app.overlay_height,
        app.overlay_stride);
    if (!app.cairo_surface) {
        fprintf(stderr, "ERROR: Failed to create cairo surface");
        return -1;
    }
    app.cairo_context = cairo_create(app.cairo_surface);
    if (!app.cairo_context) {
        fprintf(stderr, "ERROR: Failed to create cairo context");
        return -1;
    }
    DEBUG("overlay_stride %d", app.overlay_stride);

    return 0;
}

void overlay_print(const char *text)
{
    pthread_mutex_lock(&app.buffer_mutex);
    cairo_rectangle(app.cairo_context, 0.0, 0.0, app.overlay_width, app.overlay_height);
    cairo_set_source_rgba(app.cairo_context, 0.0, 0.0, 0.0, 1.0);
    cairo_fill(app.cairo_context);

    cairo_set_source_rgba(app.cairo_context, 1.0, 1.0, 1.0, 1.0);
    cairo_set_line_width(app.cairo_context, 1);
    cairo_move_to(app.cairo_context, 0.0, 10.0);
    cairo_set_font_size(app.cairo_context, 10.0);
    cairo_show_text(app.cairo_context, text);

    int n = 0;
    for (int i = 0; i < app.worker_total_objects; i++) {
        if(app.worker_scores[i] > THRESHOLD) {
            n++;
            float *box = &app.worker_boxes[i * 4];
            int x1 = (int)(box[0] * app.width);
            int y1 = (int)(box[0] * app.height);
            int x2 = (int)(box[2] * app.width);
            int y2 = (int)(box[3] * app.height);
            cairo_move_to(app.cairo_context, x1, y1);
            cairo_line_to(app.cairo_context, x2, y1);
            cairo_line_to(app.cairo_context, x2, y2);
            cairo_line_to(app.cairo_context, x1, y2);
            cairo_line_to(app.cairo_context, x1, y1);
            cairo_stroke(app.cairo_context);
        }
    }
    app.worker_objects = n;

    pthread_mutex_unlock(&app.buffer_mutex);
}

void overlay_destroy()
{
    if (app.cairo_context != NULL) {
        cairo_destroy(app.cairo_context);
    }
    if (app.cairo_surface != NULL) {
        cairo_surface_destroy(app.cairo_surface);
    }
    if (app.cairo_surface != NULL) {
        free(app.overlay_buffer);
    }
}

// void overlay_bitblt()
// {
//     int32_t* source_data = (int32_t*)buffer->data;
//     int32_t* dest_data = (int32_t*)output_buffer->data;
//     int32_t* overlay_data = (int32_t*)app.overlay_buffer;
//     int size = app.width * app.height;
//     uint32_t data_old, data_new, data_overlay, result = 0;
//     data_old = data_new = source_data[0];
//     for (int i = 0, xy_index = 0, res_index = 0, bits = 0; i < size; i++, bits += 24) {
//         data_old = data_new;
//         data_new = source_data[xy_index];
//         data_overlay = overlay_data[i];
//         int move = bits & 0B11111; //x % 32
//         switch(move) {
//             case 0: //0-24
//                 xy_index++;
//                 result = (data_new & 0x00FFFFFF) | (data_overlay & 0x00FFFFFF); //rgb
//                 break;

//             case 24: //24-48
//                 xy_index++;
//                 result = (data_old & 0xFF000000) | ((data_overlay & 0x000000FF) << 24) | result; //r
//                 dest_data[res_index++] = result;
//                 result = (data_new & 0x0000FFFF) | ((data_overlay & 0x00FFFF00) >> 8); //gb
//                 break;

//             case 16: //48-72
//                 xy_index++;
//                 result = (data_old & 0xFFFF0000) | ((data_overlay & 0x0000FFFF) << 16) | result; //rg
//                 dest_data[res_index++] = result;
//                 result = (data_new & 0x000000FF) | ((data_overlay & 0x00FF0000) >> 16); //b
//                 break;

//             case 8: //72-96
//                 result = (data_old & 0xFFFFFF00) | ((data_overlay & 0x00FFFFFF) << 8) | result; //rgb
//                 dest_data[res_index++] = result;
//                 break;
//         }
//     }
// }