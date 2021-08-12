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

#include <sysexits.h>// exit codes

#include "khash.h"

#include "main.h"
#include "utils.h"
#include "overlay.h"

#ifdef OPENVG
#include "openvg.h"
#endif //OPENVG

#ifdef CONTROL
#include "control.h"
#endif //CONTROL

#ifdef RFB
#include "rfb.h"
#endif //RFB

#define TICK_TIME 500000 //500 miliseconds

KHASH_MAP_INIT_STR(map_str, char*)
khash_t(map_str) *h;

int is_abort = 0;
static int exit_code = EX_SOFTWARE;

struct input_t input;
struct filter_t filters[MAX_OUTPUTS];
struct output_t outputs[MAX_OUTPUTS];

void *worker_function(void *data)
{
    struct app_state_t* app = (struct app_state_t*) data;
    if (app->verbose) {
        fprintf(stderr, "INFO: Worker thread has been started\n");
    }
    while (!is_abort) {
        // ----- fps
        static int frame_count = 0;
        static struct timespec t1;
        struct timespec t2;
        if (frame_count == 0) {
            clock_gettime(CLOCK_MONOTONIC, &t1);
        }
        clock_gettime(CLOCK_MONOTONIC, &t2);
        float d = (t2.tv_sec + t2.tv_nsec / 1000000000.0) - (t1.tv_sec + t1.tv_nsec / 1000000000.0);
        if (d > 0) {
            app->worker_fps = frame_count / d;
        } else {
            app->worker_fps = frame_count;
        }
        frame_count++;
        // -----

    
        CALL(sem_wait(&app->worker_semaphore));

#ifdef TENSORFLOW
        tensorflow_process(app);
#elif DARKNET
        darknet_process(app);
#endif
    }
    return NULL;
}

// Handler for sigint signals
static void signal_handler(int signal_number)
{
    if (signal_number == SIGUSR1) {
        fprintf(stderr, "INFO: SIGUSR1\n"); 
    } else {
        fprintf(stderr, "INFO: Other signal %d\n", signal_number);
    }
    is_abort = 1;
}

static void print_help()
{
    printf("ov5647 [options]\n");
    printf("options:\n");
    printf("\n");
    printf("%s: help\n", HELP);
    printf("%s: video width, default: %d\n", VIDEO_WIDTH, VIDEO_WIDTH_DEF);
    printf("%s: video height, default: %d\n", VIDEO_HEIGHT, VIDEO_HEIGHT_DEF);
    printf("%s: video format, default: %s\n", VIDEO_FORMAT, VIDEO_FORMAT_DEF);
    printf("\toptions: "VIDEO_FORMAT_YUV422_STR"\n");
    printf("%s: output, default: %s\n", VIDEO_OUTPUT, VIDEO_OUTPUT_DEF);
    printf("\toptions: "VIDEO_OUTPUT_NULL_STR", "VIDEO_OUTPUT_STDOUT_STR", "
        VIDEO_OUTPUT_SDL_STR", "VIDEO_OUTPUT_RFB_STR"\n");

    printf("%s: port, default: %d\n", PORT, PORT_DEF);
    printf("%s: worker_width, default: %d\n", WORKER_WIDTH, WORKER_WIDTH_DEF);
    printf("%s: worker_height, default: %d\n", WORKER_HEIGHT, WORKER_HEIGHT_DEF);
    printf("%s: TFL model path, default: %s\n", TFL_MODEL_PATH, TFL_MODEL_PATH_DEF);    
    printf("%s: DN model path, default: %s\n", DN_MODEL_PATH, DN_MODEL_PATH_DEF);    
    printf("%s: DN config path, default: %s\n", DN_CONFIG_PATH, DN_CONFIG_PATH_DEF);    
    printf("%s: verbose, verbose: %d\n", VERBOSE, VERBOSE_DEF);
    exit(0);
}

static int main_function()
{
    int res;
    char buffer[MAX_DATA];
    struct app_state_t app;

    utils_set_default_state(&app);
    utils_construct(&app);
    CALL(utils_init(&app), error);

    if (app.verbose) {
        fprintf(stderr, "INFO: camera_num: %d\n", app.camera_num);
        fprintf(stderr, "INFO: camera_name: %s\n", app.camera_name);
        fprintf(stderr, "INFO: camera max size: %d, %d\n", app.camera_max_width,
            app.camera_max_height);
        fprintf(stderr, "INFO: video size: %d, %d\n", app.video_width, app.video_height);
        fprintf(stderr, "INFO: video format: %s\n", utils_get_video_format_str(app.video_format));
        fprintf(stderr, "INFO: video output: %s\n", utils_get_video_output_str(app.video_output));

        fprintf(stderr, "INFO: window size: %d, %d\n", app.window_width, app.window_height);
        fprintf(stderr, "INFO: worker size: %d, %d\n", app.worker_width, app.worker_height);
    }

#ifdef CONTROL
    if (control_init(&app)) {
        fprintf(stderr, "ERROR: Failed to initialise control gpio\n");
        goto error;
    }
#endif // CONTROL

#ifdef OPENVG
    if (dispmanx_init(&app)) {
        fprintf(stderr, "ERROR: Failed to initialise dispmanx window\n");
        goto error;
    }
    if (openvg_init(&app)) {
        fprintf(stderr, "ERROR: Failed to initialise OpenVG\n");
        goto error;
    }
#endif //OPENVG

#ifdef TENSORFLOW
    if (tensorflow_create(&app)) {
        fprintf(stderr, "ERROR: Failed to create tensorflow\n");
        goto error;
    }
#elif DARKNET
    if (darknet_create(&app)) {
        fprintf(stderr, "ERROR: Failed to create darknet\n");
        goto error;
    }
#endif

    //TODO: cause segmentation fault
    // int wh = app.worker_width * app.worker_height;
    // app.worker_buffer_rgb = malloc((wh << 1) + wh);
    // if (!app.worker_buffer_rgb) {
    //     fprintf(stderr, "ERROR: Failed to allocate memory for worker buffer RGB\n");
    //     goto error;
    // }
    // app.worker_buffer_565 = malloc(wh << 1);
    // if (!app.worker_buffer_565) {
    //     fprintf(stderr, "ERROR: Failed to allocate memory for worker buffer 565\n");
    //     goto error;
    // }
    // app.worker_boxes = malloc(app.worker_total_objects * sizeof(float) * 4);
    // if (!app.worker_boxes) {
    //     fprintf(stderr, "ERROR: Failed to allocate memory for image boxes\n");
    //     goto error;
    // }
    // app.worker_classes = malloc(app.worker_total_objects * sizeof(float));
    // if (!app.worker_classes) {
    //     fprintf(stderr, "ERROR: Failed to allocate memory for image classes\n");
    //     goto error;
    // }
    // app.worker_scores = malloc(app.worker_total_objects * sizeof(float));
    // if (!app.worker_scores) {
	//     fprintf(stderr, "ERROR: Failed to allocate memory for image scores\n");
    //     goto error;
    // }

    res = pthread_mutex_init(&app.buffer_mutex, NULL);
    if (res) {
        fprintf(stderr, "ERROR: pthread_mutex_init failed to init buffer_mutex with code: %d\n", res);
        goto error;
    }

    CALL(sem_init(&app.buffer_semaphore, 0, 0), error);
    CALL(sem_init(&app.worker_semaphore, 0, 0), error);

    app.worker_thread_res = pthread_create(&app.worker_thread, NULL, worker_function, &app);
    if (app.worker_thread_res) {
	    fprintf(stderr,
            "ERROR: Failed to create worker thread, return code: %d\n", 
            app.worker_thread_res);
        goto error;
    }

    //CALL(input.start(), error);

    //TODO: move to something like camara start
    // if (app.video_output == VIDEO_OUTPUT_STDOUT) {
    //     CALL(utils_camera_create_h264_encoder(&app), error);
    // }

    while (!is_abort) {
        // for debug
        // usleep(TICK_TIME);
        // fprintf(stdout, "is_abort: %d\n", is_abort);

        CALL(res = input.process_frame());
        if (res != 0) {
            if (errno == ETIME)
                continue;
            else
                goto error;
        }
        for (int i = 0; outputs[i].context != NULL && i < MAX_OUTPUTS; i++)
            CALL(outputs[i].render(input.get_buffer()), error);

        //----- fps
        static int frame_count = 0;
        static struct timespec t1;
        struct timespec t2;
        if (frame_count == 0) {
            clock_gettime(CLOCK_MONOTONIC, &t1);
        }
        clock_gettime(CLOCK_MONOTONIC, &t2);
        float d = (t2.tv_sec + t2.tv_nsec / 1000000000.0) - (t1.tv_sec + t1.tv_nsec / 1000000000.0);
        if (d > 0) {
            app.fps = frame_count / d;
        } else {
            app.fps = frame_count;
        }
        frame_count++;
        // -----

        utils_get_cpu_load(buffer, &app.cpu);
        utils_get_memory_load(buffer, &app.memory);
        utils_get_temperature(buffer, &app.temperature);

        // every 8th frame
        if ((frame_count & 0b1111) == 0) {
            fprintf(stdout, "\rFPS: %2.2f %2.2f %2.2f, CPU: %2.1f%%, Mem: %d kb, T: %.2fC, Objs: %d"
                "          ",
                app.fps,
                app.rfb_fps,
                app.worker_fps,
                app.cpu.cpu,
                app.memory.total_size,
                app.temperature.temp,
                app.worker_objects);
            fflush(stdout);
        }

//TODO: move to open vg
#ifdef OPENVG
        // //wait frame from camera
        // sem_wait(&app.buffer_semaphore);

        // //pthread_mutex_lock(&app.buffer_mutex);
        // int value, res;
        // res = sem_getvalue(&app.worker_semaphore, &value);
        // if (res) {
        //     fprintf(stderr, "ERROR: Unable to read value from worker semaphore: %d\n", errno);
        //     is_abort = 1;
        // }
        // if (!value) {
        //     res = utils_get_worker_buffer(&app);
        //     if (res) {
        //         fprintf(stderr, "ERROR: cannot get worker buffer, res: %d\n", res);
        //         is_abort = 1;
        //     }

        //     res = sem_post(&app.worker_semaphore);
        //     if (res) {
        //         fprintf(stderr, "ERROR: Unable to increase worker semaphore\n");
        //         is_abort = 1;
        //     }
        // }

        // vgWritePixels(  app.openvg.video_buffer.c,
        //                 app.width << 1,
        //                 VG_sRGB_565,
        //                 0, 0,
        //                 app.width, app.height);

        // res = vgGetError();
        // if (res != 0) {
        //     fprintf(stderr, "ERROR: Failed to draw image %d\n", res);
        // }
        // //pthread_mutex_unlock(&app.buffer_mutex);
        // // ------------------------------

        // VGfloat vg_colour[4] = {1.0f, 1.0f, 1.0f, 1.0f};

        // res = pthread_mutex_lock(&app.buffer_mutex);
        // if (res)
        //     fprintf(stderr, "ERROR: pthread_mutex_lock failed with code %d\n", res);

        // sprintf(buffer, "camera: %2.2f detect: %2.2f, rfb: %2.2f FPS, CPU: %2.1f%%, Memory: %d kb, T: %.2fC, Objects: %d",
        //     app.fps,
        //     app.worker_fps,
        //     app.rfb_fps,
        //     app.cpu.cpu,
        //     app.memory.total_size,
        //     app.temperature.temp,
        //     app.worker_objects);

        // res = openvg_draw_text(&app, 0, 0, buffer, strlen(buffer), 20, vg_colour);
        // if (res)
        //     fprintf(stderr, "ERROR: failed to draw text\n");

        // res = openvg_draw_boxes(&app, vg_colour);
        // if (res)
        //     fprintf(stderr, "ERROR: failed to draw boxes\n");

        // res = pthread_mutex_unlock(&app.buffer_mutex);
        // if (res)
        //     fprintf(stderr, "ERROR: pthread_mutex_unlock failed with code %d\n", res);

        // if (app.output_type == OUTPUT_STREAM) {
        //     openvg_read_buffer(&app);
        //     camera_encode_buffer(&app, app.openvg.video_buffer.c, ((app.width * app.height) << 1));
        // } else if (app.output_type == OUTPUT_RFB) {
        //     openvg_read_buffer(&app);
        // }

        // EGLBoolean egl_res;
        // egl_res = eglSwapBuffers(app.openvg.display, app.openvg.surface);
        // if (egl_res == EGL_FALSE) {
        //      fprintf(stderr, "ERROR: failed to clear screan: 0x%x\n", egl_res);
        // }
#endif //OPENVG

#ifdef CONTROL
        control_ssh_key(&app);
#endif // CONTROL
    }
    fprintf(stdout, "\n");
    CALL(input.stop(&app), error);

    exit_code = EX_OK;

error:
    is_abort = 1;

#ifdef CONTROL
    control_destroy(&app);
#endif //CONTROL

    //TODO: move to something like camara start
    // if (app.video_output == VIDEO_OUTPUT_STDOUT) {
    //    CALL(res = utils_camera_cleanup_h264_encoder(&app));
    for (int i = 0; outputs[i].context != NULL && i < MAX_OUTPUTS; i++)
        outputs[i].cleanup(&app);
    input.cleanup(&app);

    fprintf(stderr, "worker_semaphore\n");
    CALL(sem_post(&app.worker_semaphore));
    CALL(sem_destroy(&app.worker_semaphore));

    fprintf(stderr, "buffer_semaphore\n");
    CALL(sem_post(&app.buffer_semaphore));
    CALL(sem_destroy(&app.buffer_semaphore));

    res = pthread_mutex_destroy(&app.buffer_mutex);
    if (res) {
        errno = res;
        CALL_MESSAGE(pthread_mutex_destroy(&app.buffer_mutex), -1);
    }

    fprintf(stderr, "worker_thread\n");
    if (!app.worker_thread_res) {
        res = pthread_join(app.worker_thread, NULL);
        if (res != 0) {
            fprintf(stderr, "ERROR: Failed to close Worker thread. error: %d\n", res);
        }
    }

    // if (app.worker_buffer_rgb) {
    //     free(app.worker_buffer_rgb);
    // }

    // if (app.worker_buffer_565) {
    //     free(app.worker_buffer_565);
    // }

    // if (app.worker_boxes) {
    //     free(app.worker_boxes);
    // }

    // if (app.worker_classes) {
    //     free(app.worker_classes);
    // }

    // if (app.worker_scores) {
    //     free(app.worker_scores);
    // }

#ifdef TENSORFLOW
    tensorflow_destroy(&app);
#elif DARKNET
    darknet_destroy(&app);
#endif

#ifdef OPENVG
    openvg_destroy(&app);
    dispmanx_destroy(&app);
#endif
    fprintf(stderr, "exit\n");

    return exit_code;
}

int main(int argc, char** argv)
{
    signal(SIGINT, signal_handler);

    h = kh_init(map_str);
    utils_parse_args(argc, argv);

    unsigned k = kh_get(map_str, h, HELP);
    if (k != kh_end(h)) {
        print_help();
    }
    else {
        main_function();
    }

    kh_destroy(map_str, h);
    return exit_code;
}
