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
#include "utils.h"

#include "file.h"

static struct format_mapping_t file_formats[] = {
    {
        .format = VIDEO_FORMAT_H264,
        .internal_format = VIDEO_FORMAT_H264,
        .is_supported = 1
    }
};

struct file_state_t file = {
    .output = NULL,
    .is_started = 0
};

extern struct app_state_t app;
extern struct input_t input;
extern struct filter_t filters[MAX_FILTERS];
extern struct output_t outputs[MAX_OUTPUTS];
extern int is_abort;

static int file_stop()
{
    ASSERT_INT(file.is_started, ==, 1, cleanup);
    file.is_started = 0;
    DEBUG("output[%s] has been stopped", file.output->name);
    return 0;

cleanup:
    if (!errno) errno = EAGAIN;
    return -1;
}

static int file_is_started()
{
    return file.is_started;
}

static void file_cleanup()
{
    if (file_is_started())
        file_stop();
}

static int file_init()
{
    return 0;
}

static int file_start()
{
    ASSERT_INT(file.is_started, ==, 0, cleanup);
    file.is_started = 1;
    DEBUG("output[%s] has been started", file.output->name);
    return 0;

cleanup:
    if (!errno) errno = EAGAIN;
    return -1;
}

static int file_process_frame()
{
    int res = 0;

    struct output_t *output = file.output;
    if (!output->is_started()) CALL(output->start(), cleanup);

    int in_format = output->start_format;
    int out_format = output->start_format;
    if (!input.is_started()) CALL(input.start(in_format), cleanup);
    CALL(res = input.process_frame(), cleanup);
    int length = 0;
    uint8_t *buffer = input.get_buffer(NULL, &length);
    for (int k = 0; k < MAX_FILTERS && output->filters[k].out_format; k++) {
        struct filter_t *filter = filters + output->filters[k].index;
        in_format = out_format;
        out_format = output->filters[k].out_format;
        if (!filter->is_started())
            CALL(filter->start(in_format, out_format), cleanup);
        CALL(filter->process_frame(buffer), cleanup);
        buffer = filter->get_buffer(NULL, &length);
        if (!length)
            break;
    }
    if (length) {
        return utils_write_file(app.output_path, buffer, length);
    }
    return 0;

cleanup:
    if (!errno) errno = EAGAIN;
    return -1;
}

static int file_get_formats(const struct format_mapping_t *formats[])
{
    if (formats != NULL)
        *formats = file_formats;
    return ARRAY_SIZE(file_formats);
}

void file_construct()
{
    int i = 0;
    while (i < MAX_OUTPUTS && outputs[i].context != NULL)
        i++;

    if (i != MAX_OUTPUTS) {
        file.output = outputs + i;
        outputs[i].name = "file";
        outputs[i].context = &file;
        outputs[i].init = file_init;
        outputs[i].cleanup = file_cleanup;
        outputs[i].is_started = file_is_started;
        outputs[i].start = file_start;
        outputs[i].stop = file_stop;
        outputs[i].process_frame = file_process_frame;
        outputs[i].get_formats = file_get_formats;
    }
}
