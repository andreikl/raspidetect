#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <memory.h>
#include <unistd.h>
#include <errno.h>
#include <sysexits.h>

#include <semaphore.h>
#include <math.h>
#include <pthread.h>
#include <time.h>

#include "bcm_host.h"

#include "main.h"
#include "utils.h"
#include "overlay.h"
#include "ov5647_helpers.h"

#define TICK_TIME 500000 //500 miliseconds

void *worker_function(void *data) {
    APP_STATE* state = (APP_STATE*) data;
    if (state->verbose) {
        fprintf(stderr, "INFO: Worker thread has been started\n");
    }
    while (1) {
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
}

int ov5647_main(void) {
    int exit_code = EX_OK;
    char buffer[BUFFER_SIZE];
    APP_STATE state;

    bcm_host_init();

    default_status(&state);

#ifdef TENSORFLOW
    if (tensorflow_create(&state)) {
        fprintf(stderr, "ERROR: Failed to create tensorflow");
        exit_code = EX_SOFTWARE;
        goto error;
    }
#elif DARKNET
    if (darknet_create(&state)) {
        fprintf(stderr, "ERROR: Failed to create darknet");
        exit_code = EX_SOFTWARE;
        goto error;
    }
#endif

    state.worker_buffer = malloc(state.worker_width * state.worker_height * 3);
    if (!state.worker_buffer) {
	    fprintf(stderr, "ERROR: Failed to allocate memory for worker buffer\n");
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

    int res = pthread_mutex_init(&state.buffer_mutex, NULL);
    if (res != 0) {
        fprintf(stderr, "ERROR: Failed to create buffer_mutex, return code: %d\n", res);
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
    get_sensor_defaults(state.camera_num,
        state.camera_name,
        &state.max_width,
        &state.max_height);

    if (state.verbose) {
        fprintf(stderr, "INFO: camera_num: %d\n", state.camera_num);
        fprintf(stderr, "INFO: camera_name: %s\n", state.camera_name);
        fprintf(stderr, "INFO: camera_max_width: %d\n", state.max_width);
        fprintf(stderr, "INFO: camera_max_height: %d\n", state.max_height);
        fprintf(stderr, "INFO: camera_width: %d\n", state.width);
        fprintf(stderr, "INFO: camera_height: %d\n", state.height);
        fprintf(stderr, "INFO: overlay_width: %d\n", state.overlay_width);
        fprintf(stderr, "INFO: overlay_height: %d\n", state.overlay_height);
        fprintf(stderr, "INFO: worker_width: %d\n", state.worker_width);
        fprintf(stderr, "INFO: worker_height: %d\n", state.worker_height);
    }

    if (overlay_create(&state)) {
        fprintf(stderr, "ERROR: Failed to create cairo overlay");
        exit_code = EX_SOFTWARE;
        goto error;
    }

    if (create_camera_component(&state)) {
        fprintf(stderr, "ERROR: Failed to create camera component");
        exit_code = EX_SOFTWARE;
        goto error;
    }

    if (create_encoder_h264(&state)) {
        fprintf(stderr, "ERROR: Failed to create H264 encoder component");
        exit_code = EX_SOFTWARE;
        goto error;
    }

    // if (create_encoder_rgb(&state)) {
    //     fprintf(stderr, "ERROR: Failed to create RGB encoder component");
    //     exit_code = EX_SOFTWARE;
    //     goto error;
    // }

    while (1) {
        get_cpu_load(buffer, &state.cpu);
        get_memory_load(buffer, &state.memory);
        get_temperature(buffer, &state.temperature);

        sprintf(buffer, "%.2f FPS, CPU: %.1f%%, Memory: %d kb, T: %.2fC, Objects: %d",
            state.fps,
            state.cpu.cpu,
            state.memory.total_size,
            state.temperature.temp,
            state.worker_objects);
        overlay_print(&state, buffer);


        fprintf(stderr, "%.2f FPS, CPU: %.1f%%, Memory: %d kb, Swap: %d kb, T: %.2fC, Objs: %d, Debug: %d\n",
            state.fps,
            state.cpu.cpu,
            state.memory.total_size,
            state.memory.swap_size,
            state.temperature.temp,
            state.worker_objects,
            state.rtime);

        usleep(TICK_TIME);
    }

error:
    destroy_components(&state);

    overlay_destroy(&state);

    pthread_join(state.worker_thread, NULL);
    sem_destroy(&state.worker_semaphore);
    pthread_mutex_destroy(&state.buffer_mutex);

    if (state.worker_buffer) {
        free(state.worker_buffer);
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

    return exit_code;
}
