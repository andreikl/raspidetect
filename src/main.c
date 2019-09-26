#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <memory.h>
#include <unistd.h>
#include <errno.h>
#include <sysexits.h>
#include <assert.h>
#include <signal.h>
#include <string.h>

#include <semaphore.h>
#include <math.h>
#include <pthread.h>
#include <time.h>

#include "bcm_host.h"

#include "khash.h"

#include "main.h"
#include "utils.h"
#include "overlay.h"
#include "ov5647.h"

#ifdef OPENVG
#include "openvg.h"
#endif //OPENVG

#define TICK_TIME 500000 //500 miliseconds

KHASH_MAP_INIT_STR(map_str, char*)
khash_t(map_str) *h;

int is_abort = 0;
static int exit_code = EX_OK;

void *worker_function(void *data) {
    APP_STATE* state = (APP_STATE*) data;
    if (state->verbose) {
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
            state->fps = frame_count / d;
        } else {
            state->fps = frame_count;
        }
        frame_count++;
        // -----

        sem_wait(&state->worker_semaphore);

#ifdef TENSORFLOW
        tensorflow_process(state);
#elif DARKNET
        darknet_process(state);
#endif
    }
    return NULL;
}

int main_function() {
    int res;
    char buffer[BUFFER_SIZE];
    APP_STATE state;

    bcm_host_init();

    default_status(&state);

#ifdef OPENVG
    if (dispmanx_init(&state)) {
        fprintf(stderr, "ERROR: Failed to initialise dispmanx window\n");
        exit_code = EX_SOFTWARE;
        goto error;
    }
    if (openvg_init(&state)) {
        fprintf(stderr, "ERROR: Failed to initialise OpenVG\n");
        exit_code = EX_SOFTWARE;
        goto error;
    }
#endif //OPENVG

#ifdef TENSORFLOW
    if (tensorflow_create(&state)) {
        fprintf(stderr, "ERROR: Failed to create tensorflow\n");
        exit_code = EX_SOFTWARE;
        goto error;
    }
#elif DARKNET
    if (darknet_create(&state)) {
        fprintf(stderr, "ERROR: Failed to create darknet\n");
        exit_code = EX_SOFTWARE;
        goto error;
    }
#endif

    int wh = state.worker_width * state.worker_height;
    state.worker_buffer_rgb = malloc((wh << 1) + wh);
    if (!state.worker_buffer_rgb) {
	    fprintf(stderr, "ERROR: Failed to allocate memory for worker buffer RGB\n");
        goto error;
    }

    state.worker_buffer_565 = malloc(wh << 1);
    if (!state.worker_buffer_565) {
	    fprintf(stderr, "ERROR: Failed to allocate memory for worker buffer 565\n");
        goto error;
    }

    state.worker_boxes = malloc(state.worker_total_objects * sizeof(float) * 4);
    if (!state.worker_boxes) {
	    fprintf(stderr, "ERROR: Failed to allocate memory for image boxes\n");
        goto error;
    }

    state.worker_classes = malloc(state.worker_total_objects * sizeof(float));
    if (!state.worker_classes) {
	    fprintf(stderr, "ERROR: Failed to allocate memory for image classes\n");
        goto error;
    }

    state.worker_scores = malloc(state.worker_total_objects * sizeof(float));
    if (!state.worker_scores) {
	    fprintf(stderr, "ERROR: Failed to allocate memory for image scores\n");
        goto error;
    }

    res = pthread_mutex_init(&state.buffer_mutex, NULL);
    if (res != 0) {
        fprintf(stderr, "ERROR: Failed to create buffer_mutex, return code: %d\n", res);
        goto error;
    }

    res = sem_init(&state.buffer_semaphore, 0, 0);
    if (res) {
	    fprintf(stderr, "ERROR: Failed to create buffer semaphore, return code: %d\n", res);
        goto error;
    }

    res = sem_init(&state.worker_semaphore, 0, 0);
    if (res) {
	    fprintf(stderr, "ERROR: Failed to create worker semaphore, return code: %d\n", res);
        goto error;
    }

    res = pthread_create(&state.worker_thread, NULL, worker_function, &state);
    if (res) {
	    fprintf(stderr, "ERROR: Failed to create worker thread, return code: %d\n", res);
        goto error;
    }

    // Setup for sensor specific parameters
    get_sensor_defaults(state.mmal.camera_num,
        state.mmal.camera_name,
        &state.mmal.max_width,
        &state.mmal.max_height);

    if (state.verbose) {
        fprintf(stderr, "INFO: camera_num: %d\n", state.mmal.camera_num);
        fprintf(stderr, "INFO: camera_name: %s\n", state.mmal.camera_name);
        fprintf(stderr, "INFO: camera max size: %d, %d\n", state.mmal.max_width, state.mmal.max_height);
        fprintf(stderr, "INFO: video size: %d, %d\n", state.video_width, state.video_height);
        fprintf(stderr, "INFO: display size: %d, %d\n", state.openvg.display_width, state.openvg.display_height);
        fprintf(stderr, "INFO: worker size: %d, %d\n", state.worker_width, state.worker_height);
        fprintf(stderr, "INFO: output type: %d\n", state.output_type);
    }

    if (create_camera_component(&state)) {
        fprintf(stderr, "ERROR: Failed to create camera component");
        exit_code = EX_SOFTWARE;
        goto error;
    }

    if (state.output_type != OUTPUT_NONE && create_encoder_h264(&state)) {
        fprintf(stderr, "ERROR: Failed to create H264 encoder component");
        exit_code = EX_SOFTWARE;
        goto error;
    }

    while (!is_abort) {
        // ----- fps
        static int frame_count = 0;
        frame_count++;
        // -----

        get_cpu_load(buffer, &state.cpu);
        get_memory_load(buffer, &state.memory);
        get_temperature(buffer, &state.temperature);

        // every 8th frame
        if ((frame_count & 0b1111) == 0) {
            fprintf(stderr, "%.2f FPS, CPU: %.1f%%, Memory: %d kb, Swap: %d kb, T: %.2fC, Objs: %d\n",
                state.fps,
                state.cpu.cpu,
                state.memory.total_size,
                state.memory.swap_size,
                state.temperature.temp,
                state.worker_objects);
        }

#ifdef OPENVG
        //wait frame from camera
        sem_wait(&state.buffer_semaphore);

        //pthread_mutex_lock(&state.buffer_mutex);
        int value, res;
        res = sem_getvalue(&state.worker_semaphore, &value);
        if (res) {
            fprintf(stderr, "ERROR: Unable to read value from worker semaphore: %d\n", errno);
            is_abort = 1;
        }
        if (!value) {
            res = get_worker_buffer(&state);
            if (res) {
                fprintf(stderr, "ERROR: cannot get worker buffer, res: %d\n", res);
                is_abort = 1;
            }

            res = sem_post(&state.worker_semaphore);
            if (res) {
                fprintf(stderr, "ERROR: Unable to increase worker semaphore\n");
                is_abort = 1;
            }
        }

        vgWritePixels(  state.openvg.video_buffer,
                        state.video_width << 1,
                        VG_sRGB_565,
                        0, 0,
                        state.video_width, state.video_height);

        res = vgGetError();
        if (res != 0) {
            fprintf(stderr, "ERROR: Failed to draw image %d\n", res);
        }
        //pthread_mutex_unlock(&state.buffer_mutex);
        // ------------------------------

        VGfloat vg_colour[4] = {1.0f, 1.0f, 1.0f, 1.0f};

        pthread_mutex_lock(&state.buffer_mutex);
        sprintf(buffer, "%.2f FPS, CPU: %.1f%%, Memory: %d kb, T: %.2fC, Objects: %d",
            state.fps,
            state.cpu.cpu,
            state.memory.total_size,
            state.temperature.temp,
            state.worker_objects);

        res = openvg_draw_text(&state, 0, 0, buffer, strlen(buffer), 20, vg_colour);
        if (res)
            fprintf(stderr, "ERROR: failed to draw text\n");

        res = openvg_draw_boxes(&state, vg_colour);
        if (res)
            fprintf(stderr, "ERROR: failed to draw boxes\n");
        pthread_mutex_unlock(&state.buffer_mutex);

        if (state.output_type != OUTPUT_NONE) {
            vgReadPixels(state.openvg.video_buffer,
                        state.video_width << 1,
                        VG_sRGB_565,
                        0, 0,
                        state.video_width, state.video_height);

            int size = state.video_width * state.video_height;
            encode_buffer(&state, state.openvg.video_buffer, (size << 1));
            //encode_buffer(&state, state.worker_buffer_rgb, (size << 1) + size);
            //encode_buffer(&state, state.worker_buffer_565, (size << 1));
        } 

        EGLBoolean egl_res;
        egl_res = eglSwapBuffers(state.openvg.display, state.openvg.surface);
        if (egl_res == EGL_FALSE) {
             fprintf(stderr, "ERROR: failed to clear screan: 0x%x\n", egl_res);
        }
#endif
        control_handle_key(&state);

        //usleep(TICK_TIME);
    }

error:
    control_destroy(&state);
    destroy_components(&state);

    pthread_join(state.worker_thread, NULL);
    sem_destroy(&state.worker_semaphore);
    sem_destroy(&state.buffer_semaphore);
    pthread_mutex_destroy(&state.buffer_mutex);

    if (state.worker_buffer_rgb) {
        free(state.worker_buffer_rgb);
    }

    if (state.worker_buffer_565) {
        free(state.worker_buffer_565);
    }

    if (state.worker_boxes) {
        free(state.worker_boxes);
    }

    if (state.worker_classes) {
        free(state.worker_classes);
    }

    if (state.worker_scores) {
        free(state.worker_scores);
    }

#ifdef TENSORFLOW
    tensorflow_destroy(&state);
#elif DARKNET
    darknet_destroy(&state);
#endif

#ifdef OPENVG
    openvg_destroy(&state);
    dispmanx_destroy(&state);
#endif

    return exit_code;
}

// Handler for sigint signals
static void signal_handler(int signal_number) {
    if (signal_number == SIGUSR1) {
        fprintf(stderr, "INFO: SIGUSR1\n"); 
    } else {
        fprintf(stderr, "INFO: Other signal %d\n", signal_number);
    }
    is_abort = 1;
}

int main(int argc, char** argv) {
    signal(SIGINT, signal_handler);

    h = kh_init(map_str);
    parse_args(argc, argv);

    unsigned k = kh_get(map_str, h, HELP);
    if (k != kh_end(h)) {
        print_help();
    }
    else {
        main_function();
    }

    kh_destroy(map_str, h);
    return 0;
}
