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

extern int is_terminated;

static void *file_function(void* data)
{
    struct app_state_t * app = (struct app_state_t*) data;
    if (app->verbose) {
        fprintf(stderr, "INFO: File thread has been started\n");
    }

    //wait frame from network
    if (sem_wait(&app->file.semaphore)) {
        fprintf(stderr, "ERROR: sem_wait failed with error: %s\n", strerror(errno));
        goto error;
    }

    while (!is_terminated) {
        if (usleep(400000)) {
            fprintf(stderr, "ERROR: usleep failed with error: %s\n", strerror(errno));
            goto error;
        }

        int buf_length = app->server_width * app->server_height;
        long int pos = ftell(app->file.fstream);
        if (pos == -1) {
            fprintf(stderr, "ERROR: ftell failed with error: %s\n", strerror(errno));
            goto error;
        }
        size_t read_length = fread(app->enc_buf, 1, buf_length, app->file.fstream);
        if (read_length < buf_length) {
            app->enc_buf[read_length] = 0;
            app->enc_buf_length = read_length;
        } else {
            app->enc_buf[buf_length] = 0;
            app->enc_buf_length = buf_length;
        }
        //fprintf(stderr, "INFO: read %lld\n", read_length);

        int start = 0, end = 0;
        GENERAL_CALL(find_nal(app->enc_buf, app->enc_buf_length, &start, &end), error);
        //fprintf(stderr, "INFO: start %d, end: %d\n", start, end);
        if (end < app->enc_buf_length) {
            app->enc_buf[end] = 0;
            app->enc_buf_length = end;

            if (fseek(app->file.fstream, pos + end, SEEK_SET)) {
                fprintf(stderr, "ERROR: fseek failed to set position: %ld, with error: : %s\n",
                    pos + end, strerror(errno));
                goto error;
            }
        }

#ifdef ENABLE_H264
        GENERAL_CALL(h264_decode(app), error);
#endif //ENABLE_H264

#ifdef ENABLE_FFMPEG
        GENERAL_CALL(ffmpeg_decode(app, start, end), error);
#endif //ENABLE_FFMPEG
    }

    fprintf(stderr, "INFO: file_function is_terminated: %d\n", is_terminated);
    return NULL;

error:
    is_terminated = 1;
    return NULL;
}

void file_destroy(struct app_state_t* app)
{
    if (sem_destroy(&app->file.semaphore)) {
        fprintf(stderr, "ERROR: sem_destroy failed with code: %s\n", strerror(errno));
    }

    if (app->file.is_thread) {
        GENERAL_CALL(pthread_join(app->file.thread, NULL));
        app->file.is_thread = 0;
    }

    if (app->file.fstream && fclose(app->file.fstream)) {
        fprintf(stderr, "ERROR: fclose failed with error: %s\n", strerror(errno));
    }
    else {
        app->file.fstream = NULL;
    }
}

int file_init(struct app_state_t* app)
{
    app->file.fstream = fopen(app->file.file_name, "rb");
    if (app->file.fstream == NULL) {
        fprintf(stderr, "ERROR: fopen failed when open file, filename: %s, error: %d\n",
            app->file.file_name, errno);
        goto error;
    }

    if (sem_init(&app->file.semaphore, 0, 0)) {
        fprintf(stderr, "ERROR: sem_init failed with code: %s\n", strerror(errno));
        goto error;
    }

    if (pthread_create(&app->file.thread, NULL, file_function, app)) {
	    fprintf(stderr, "ERROR: pthread_create failed with code: %s\n", strerror(errno));
        goto error;
    } else {
        app->file.is_thread = 1;
    }

    return 0;

error:
    file_destroy(app);
    return -1;
}

int file_start(struct app_state_t* app)
{
    if (sem_post(&app->file.semaphore)) {
        fprintf(stderr, "ERROR: sem_post Failed to start rfb with error: : %s\n", strerror(errno));
        return -1;
    }
    return 0;
}