// Raspidetect

// Copyright (C) 2023 Andrei Klimchuk <andrew.klimchuk@gmail.com>

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

#include "klist.h"

#include "main.h"
#include "utils.h"

extern struct app_state_t app;
extern struct filter_t filters[MAX_FILTERS];

void app_set_default_state()
{
    memset(&app, 0, sizeof(struct app_state_t));
    char* input_type = utils_read_str_value(INPUT_TYPE, INPUT_TYPE_DEF);
    DEBUG_MSG("INFO: input_type %s\n", input_type);
    if (!strcmp(input_type, INPUT_TYPE_FILE_STR)) {
        app.input_type = INPUT_TYPE_FILE;
    }
    else {
        app.input_type = INPUT_TYPE_RFB;
    }   
    app.file.file_name = utils_read_str_value(FILE_NAME, FILE_NAME_DEF);
    app.server_port = utils_read_str_value(PORT, PORT_DEF);
    app.server_host = utils_read_str_value(SERVER, SERVER_DEF);
    app.server_width = 640;
    app.server_height = 480;
    app.server_chroma = CHROMA_FORMAT_YUV422;
    app.verbose = utils_read_int_value(VERBOSE, VERBOSE_DEF);
}

void app_construct()
{
#ifdef FFMPEG
    ffmpeg_construct();
#endif //FFMPEG
}

void app_cleanup()
{
    for (int i = 0; i < MAX_FILTERS && filters[i].context != NULL; i++) {
        if (filters[i].is_started()) {
            CALL(filters[i].stop())
        }
        filters[i].cleanup();
    }
}


int app_init()
{
    for (int i = 0; i < MAX_FILTERS && filters[i].context != NULL; i++) {
        //DEBUG("filters[%s].init...", filters[i].name);
        CALL(filters[i].init(), error);
    }
    return 0;
error:
    if (errno == 0)
        errno = EAGAIN;
    return -1;
}
