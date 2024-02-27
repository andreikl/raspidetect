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

#ifdef ENABLE_FFMPEG
#include "ffmpeg.h"
#endif //ENABLE_FFMPEG

#ifdef ENABLE_FFMPEG_DXVA2
#include "ffmpeg_dxva2.h"
#endif //ENABLE_FFMPEG_DXVA2

extern struct app_state_t app;
extern struct filter_t filters[MAX_FILTERS];

void app_set_default_state()
{
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
}

void app_construct()
{
#ifdef ENABLE_FFMPEG
    ffmpeg_decoder_construct();
#endif //ENABLE_FFMPEG
#ifdef ENABLE_FFMPEG_DXVA2
    ffmpeg_dxva2_decoder_construct();
#endif //ENABLE_FFMPEG_DXVA2
}

void app_cleanup()
{
    for (int i = 0; i < MAX_FILTERS && filters[i].context != NULL; i++) {
        if (filters[i].is_started()) {
            CALL(filters[i].stop())
        }
        filters[i].cleanup();
    }

    if (app.is_dec_mutex) {
        int res = pthread_mutex_destroy(&app.dec_mutex);
        if (res) {
            fprintf(
                stderr,
                "ERROR: pthread_mutex_destroy can't destroy decoder mutex, res %d\n",
                res);
        }
        app.is_dec_mutex = 0;
    }

    if (app.dec_buf != NULL) {
        free(app.dec_buf);
        app.dec_buf = NULL;
    }

    if (app.enc_buf != NULL) {
        free(app.enc_buf);
        app.enc_buf = NULL;
    }
}


int app_init()
{
    app.enc_buf_length = app.server_width * app.server_height + 1;
    app.enc_buf = malloc(app.enc_buf_length);
    if (app.enc_buf == NULL) {
        fprintf(
            stderr,
            "ERROR: malloc can't allocate memory for encoding buffer, size: %d\n",
            app.enc_buf_length);
        goto error;
    }

    app.dec_buf_length = app.server_width * app.server_height * 4 + 1;
    app.dec_buf = malloc(app.dec_buf_length);
    if (app.dec_buf == NULL) {
        fprintf(
            stderr,
            "ERROR: malloc can't allocate memory for decoding buffer, size: %d\n",
            app.dec_buf_length);
        goto error;
    }

    CALL(pthread_mutex_init(&app.dec_mutex, NULL), error);
    app.is_dec_mutex = 1;

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
