// Raspidetect

// Copyright (C) 2022 Andrei Klimchuk <andrew.klimchuk@gmail.com>

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
#include "utils.h"
#include "mmal_encoder_stubs.h"
#include "mmal_encoder.h"

extern struct app_state_t app;
extern struct input_t input;
extern struct filter_t filters[MAX_FILTERS];
extern struct output_t outputs[MAX_OUTPUTS];
extern int is_abort;

static uint8_t mmal_buffer[640*480*2];
static MMAL_ES_SPECIFIC_FORMAT_T mmal_specific_format;
static MMAL_ES_FORMAT_T mmal_format = {
    .es = &mmal_specific_format
};
static MMAL_PORT_T mmal_port = {
    .buffer_num = 1,
    .format = &mmal_format,
    .buffer_size_recommended = MMAL_OUT_BUFFER_SIZE,
    .buffer_num_recommended = 1
};
static MMAL_POOL_T mmal_pool;
static MMAL_BUFFER_HEADER_T mmal_header = {
    .data = mmal_buffer
};
static MMAL_COMPONENT_T mmal_component = {
    .input[0] = &mmal_port,
    .output[0] = &mmal_port
};
static MMAL_PORT_BH_CB_T mmal_callback;

MMAL_STATUS_T mmal_component_create(const char *name, MMAL_COMPONENT_T **component)
{
    if (component) {
        *component = &mmal_component;
    }
    return MMAL_SUCCESS;
}

MMAL_STATUS_T mmal_component_destroy(MMAL_COMPONENT_T* component)
{
    return MMAL_SUCCESS;
}

MMAL_STATUS_T mmal_port_format_commit(MMAL_PORT_T *port)
{
    return MMAL_SUCCESS;
}

MMAL_STATUS_T mmal_port_enable(MMAL_PORT_T *port, MMAL_PORT_BH_CB_T cb)
{
    mmal_callback = cb;
    return MMAL_SUCCESS;
}

MMAL_STATUS_T mmal_port_disable(MMAL_PORT_T* port)
{
    return MMAL_SUCCESS;
}

MMAL_POOL_T *mmal_port_pool_create(MMAL_PORT_T *port, unsigned int headers, uint32_t payload_size)
{
    return &mmal_pool;
}

unsigned int mmal_queue_length(MMAL_QUEUE_T *queue)
{
    return 1;
}

MMAL_BUFFER_HEADER_T* mmal_queue_get(MMAL_QUEUE_T* queue)
{
    return &mmal_header;
}

MMAL_STATUS_T mmal_port_send_buffer(MMAL_PORT_T* port, MMAL_BUFFER_HEADER_T* buffer)
{
    if (mmal_callback != NULL) {
        mmal_header.length = 200;
        mmal_callback(&mmal_port, &mmal_header);
    }
    return MMAL_SUCCESS;
}

MMAL_STATUS_T mmal_buffer_header_mem_lock(MMAL_BUFFER_HEADER_T *header)
{
    return MMAL_SUCCESS;
}

void mmal_buffer_header_mem_unlock(MMAL_BUFFER_HEADER_T *header)
{
}

void mmal_buffer_header_release(MMAL_BUFFER_HEADER_T* header)
{
}
