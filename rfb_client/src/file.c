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

#include <unistd.h>

#include "main.h"
#include "utils.h"
#include "ffmpeg.h"
#include "d3d.h"

extern struct app_state_t app;
extern struct filter_t filters[MAX_FILTERS];
extern int is_aborted;

static void *file_function(void* data)
{
    DEBUG_MSG("INFO: File thread has been started\n");

    //wait frame from network
    if (sem_wait(&app.file.semaphore)) {
        ERROR_MSG("sem_wait failed with error: %s\n", strerror(errno));
        goto error;
    }

    while (!is_aborted) {
        if (usleep(400000)) {
            ERROR_MSG("usleep failed with error: %s\n", strerror(errno));
            goto error;
        }

        int buf_length = app.server_width * app.server_height;
        long int pos = ftell(app.file.fstream);
        if (pos == -1) {
            ERROR_MSG("ftell failed with error: %s\n", strerror(errno));
            goto error;
        }
        size_t read_length = fread(app.enc_buf, 1, buf_length, app.file.fstream);
        if (read_length < buf_length) {
            app.enc_buf[read_length] = 0;
            app.enc_buf_length = read_length;
        } else {
            app.enc_buf[buf_length] = 0;
            app.enc_buf_length = buf_length;
        }
        //DEBUG_MSG("INFO: read %lld\n", read_length);

        int start = 0, end = 0;
        CALL(find_nal(app.enc_buf, app.enc_buf_length, &start, &end), error);
        //DEBUG_MSG("INFO: start %d, end: %d\n", start, end);
        if (end < app.enc_buf_length) {
            app.enc_buf[end] = 0;
            app.enc_buf_length = end;

            if (fseek(app.file.fstream, pos + end, SEEK_SET)) {
                ERROR_MSG("fseek failed to set position: %ld, with error: : %s\n",
                    pos + end, strerror(errno));
                goto error;
            }
        }

        uint8_t *buffer = app.enc_buf;
        int len = app.enc_buf_length;
        for (int i = 0; i < MAX_FILTERS && filters[i].context != NULL; i++) {
            struct filter_t *filter = filters + i;
            if (!filter->is_started())
                CALL(filter->start(VIDEO_FORMAT_H264, VIDEO_FORMAT_GRAYSCALE), error);
            CALL(filter->process(buffer, len), error);
            buffer = filter->get_buffer(NULL, &len);
            if (len == 0) {
                DEBUG_MSG("The filter[%s] doesn't have buffer yet", filter->name);
                break;
            }
            else {
                DEBUG_MSG("buffer has been received from filter[%s], length: %d!!!", filter->name, len);
            }
            break;
        }

#ifdef ENABLE_H264
        CALL(h264_decode(), error);
#endif //ENABLE_H264

#ifdef ENABLE_D3D
        CALL(d3d_render_image(), error);
#endif //ENABLE_D3D
    }

    DEBUG_MSG("INFO: file_function is_aborted: %d\n", is_aborted);
    return NULL;

error:
    is_aborted = 1;
    return NULL;
}

void file_destroy()
{
    if (sem_destroy(&app.file.semaphore)) {
        ERROR_MSG("sem_destroy failed with code: %s\n", strerror(errno));
    }

    if (app.file.is_thread) {
        CALL(pthread_join(app.file.thread, NULL));
        app.file.is_thread = 0;
    }

    if (app.file.fstream && fclose(app.file.fstream)) {
        ERROR_MSG("fclose failed with error: %s\n", strerror(errno));
    }
    else {
        app.file.fstream = NULL;
    }
}

int file_init()
{
    app.file.fstream = fopen(app.file.file_name, "rb");
    if (app.file.fstream == NULL) {
        ERROR_MSG("fopen failed when open file, filename: %s, error: %d\n",
            app.file.file_name, errno);
        goto error;
    }

    if (sem_init(&app.file.semaphore, 0, 0)) {
        ERROR_MSG("sem_init failed with code: %s\n", strerror(errno));
        goto error;
    }

    if (pthread_create(&app.file.thread, NULL, file_function, NULL)) {
	    ERROR_MSG("pthread_create failed with code: %s\n", strerror(errno));
        goto error;
    } else {
        app.file.is_thread = 1;
    }

    return 0;

error:
    file_destroy();
    return -1;
}

int file_start()
{
    if (sem_post(&app.file.semaphore)) {
        ERROR_MSG("sem_post Failed to start rfb with error: : %s\n", strerror(errno));
        return -1;
    }
    return 0;
}