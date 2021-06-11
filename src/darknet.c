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

int darknet_process(app_state_t *state)
{
    fprintf(stderr, "DEBUG: darknet start\n");

    image im = make_image(state->worker_width, state->worker_height, 3);

#ifdef ENV32BIT
    int size = state->worker_width * state->worker_height >> 2; // = size / 4
    int32_t *buffer = (int32_t *)state->worker_buffer;
    int32_t x = 0;
    int i = 0, j = 0;

    while (i < size) {
        x = buffer[i++];
        im.data[j++] = (x & 0x000000FF);
        im.data[j++] = ((x & 0x0000FF00) >> 8);
        im.data[j++] = ((x & 0x00FF0000) >> 16);
        im.data[j++] = ((x & 0xFF000000) >> 24);
    }
#elif
    fprintf(stderr, "ERROR: 64bit darknet conversion isn't implemented\n");
 #endif

    fprintf(stderr, "DEBUG: darknet about to detect\n");

    network_predict(state->dn.dn_net, im.data);
    fprintf(stderr, "DEBUG: darknet detected\n");

    detection *dets = get_network_boxes(state->dn.dn_net, 1, 1, THRESHOLD, 0, 0, 0, &state->worker_objects);
    fprintf(stderr, "DEBUG: darknet about to freed\n");

    free_detections(dets, state->worker_objects);
    free_image(im);
    return 0;
}

int darknet_create(app_state_t *state)
{
    state->dn.dn_net = load_network(state->config_path, state->model_path, 0);
    if (!state->dn.dn_net) {
        fprintf(stderr, "ERROR: Failed to load Darknet network (config_path: %s, model_path: %s)\n", state->config_path, state->model_path);
        return -1;
    }
    if (state->verbose) {
        fprintf(stderr, "INFO: net width: %d, height: %d\n",
        state->dn.dn_net->w,
        state->dn.dn_net->h);
    }
    set_batch_network(state->dn.dn_net, 1);

    return 0;
}

void darknet_destroy(app_state_t *state)
{
    if (state->dn.dn_net) {
        free_network(state->dn.dn_net);
    }
}
