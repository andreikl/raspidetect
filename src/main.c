#include <sysexits.h>// exit codes

#include "khash.h"

#include "main.h"
#include "utils.h"
#include "overlay.h"
#include "mmal.h"

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

void *worker_function(void *data)
{
    app_state_t* app = (app_state_t*) data;
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

        if (sem_wait(&app->worker_semaphore)) {
            fprintf(stderr, "ERROR: sem_wait failed to wait worker_semaphore with error (%d)\n", errno); 
        }

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
    printf("%s: width, default: %d\n", WIDTH, WIDTH_DEF);
    printf("%s: height, default: %d\n", HEIGHT, HEIGHT_DEF);
    printf("%s: port, default: %d\n", PORT, PORT_DEF);
    printf("%s: worke_width, default: %d\n", WORKER_WIDTH, WORKER_WIDTH_DEF);
    printf("%s: worker_height, default: %d\n", WORKER_HEIGHT, WORKER_HEIGHT_DEF);
    printf("%s: TFL model path, default: %s\n", TFL_MODEL_PATH, TFL_MODEL_PATH_DEF);    
    printf("%s: DN model path, default: %s\n", DN_MODEL_PATH, DN_MODEL_PATH_DEF);    
    printf("%s: DN config path, default: %s\n", DN_CONFIG_PATH, DN_CONFIG_PATH_DEF);    
    printf("%s: input, default: %s\n", INPUT, INPUT_DEF);
    printf("%s: output, default: %s\n", OUTPUT, OUTPUT_DEF);
    printf("\toptions: "ARG_STREAM" - output stream, "ARG_RFB" - output rfb, "ARG_NONE"\n");
    printf("%s: verbose, verbose: %d\n", VERBOSE, VERBOSE_DEF);
    exit(0);
}

static int main_function()
{
    int res;
    char buffer[BUFFER_SIZE];
    app_state_t app;

    bcm_host_init();

    utils_default_status(&app);

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

    int wh = app.worker_width * app.worker_height;
    app.worker_buffer_rgb = malloc((wh << 1) + wh);
    if (!app.worker_buffer_rgb) {
	    fprintf(stderr, "ERROR: Failed to allocate memory for worker buffer RGB\n");
        goto error;
    }

    app.worker_buffer_565 = malloc(wh << 1);
    if (!app.worker_buffer_565) {
	    fprintf(stderr, "ERROR: Failed to allocate memory for worker buffer 565\n");
        goto error;
    }

    app.worker_boxes = malloc(app.worker_total_objects * sizeof(float) * 4);
    if (!app.worker_boxes) {
	    fprintf(stderr, "ERROR: Failed to allocate memory for image boxes\n");
        goto error;
    }

    app.worker_classes = malloc(app.worker_total_objects * sizeof(float));
    if (!app.worker_classes) {
	    fprintf(stderr, "ERROR: Failed to allocate memory for image classes\n");
        goto error;
    }

    app.worker_scores = malloc(app.worker_total_objects * sizeof(float));
    if (!app.worker_scores) {
	    fprintf(stderr, "ERROR: Failed to allocate memory for image scores\n");
        goto error;
    }

    res = pthread_mutex_init(&app.buffer_mutex, NULL);
    if (res) {
        fprintf(stderr, "ERROR: pthread_mutex_init failed to init buffer_mutex with code: %d\n", res);
        goto error;
    }

    res = sem_init(&app.buffer_semaphore, 0, 0);
    if (res) {
	    fprintf(stderr, "ERROR: Failed to create buffer semaphore, return code: %d\n", res);
        goto error;
    }

    res = sem_init(&app.worker_semaphore, 0, 0);
    if (res) {
	    fprintf(stderr, "ERROR: Failed to create worker semaphore, return code: %d\n", res);
        goto error;
    }

    app.worker_thread_res = pthread_create(&app.worker_thread, NULL, worker_function, &app);
    if (app.worker_thread_res) {
	    fprintf(stderr,
            "ERROR: Failed to create worker thread, return code: %d\n", 
            app.worker_thread_res);
        goto error;
    }

    // Setup for sensor specific parameters
    utils_camera_get_defaults(&app);

    if (app.verbose) {
        fprintf(stderr, "INFO: camera_num: %d\n", app.mmal.camera_num);
        fprintf(stderr, "INFO: camera_name: %s\n", app.mmal.camera_name);
        fprintf(stderr, "INFO: camera max size: %d, %d\n", app.mmal.max_width, app.mmal.max_height);
        fprintf(stderr, "INFO: video size: %d, %d, %d\n", app.width, app.height, app.bits_per_pixel);
        fprintf(stderr, "INFO: display size: %d, %d\n", app.openvg.display_width, app.openvg.display_height);
        fprintf(stderr, "INFO: worker size: %d, %d\n", app.worker_width, app.worker_height);
        fprintf(stderr, "INFO: output type: %d\n", app.output_type);
    }

    if (camera_create(&app)) {
        fprintf(stderr, "ERROR: camera_create failed\n");
        goto error;
    }

    if (app.output_type == OUTPUT_STREAM) {
        if (camera_create_h264_encoder(&app)) {
            fprintf(stderr, "ERROR: camera_create_h264_encoder failed\n");
            goto error;
        }
    } else if (app.output_type == OUTPUT_RFB) {
#ifdef RFB
        if (rfb_init(&app)) {
            fprintf(stderr, "ERROR: Can't init RFB server");
            goto error;
        }
#endif //RFB    
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
            fprintf(stderr, "camera: %2.2f detect: %2.2f, rfb: %2.2f FPS, CPU: %2.1f%%, Memory: %d (%d) kb, T: %.2fC, Objs: %d\n",
                app.fps,
                app.worker_fps,
                app.rfb_fps,
                app.cpu.cpu,
                app.memory.total_size,
                app.memory.swap_size,
                app.temperature.temp,
                app.worker_objects);
        }

#ifdef OPENVG
        //wait frame from camera
        sem_wait(&app.buffer_semaphore);

        //pthread_mutex_lock(&app.buffer_mutex);
        int value, res;
        res = sem_getvalue(&app.worker_semaphore, &value);
        if (res) {
            fprintf(stderr, "ERROR: Unable to read value from worker semaphore: %d\n", errno);
            is_abort = 1;
        }
        if (!value) {
            res = utils_get_worker_buffer(&app);
            if (res) {
                fprintf(stderr, "ERROR: cannot get worker buffer, res: %d\n", res);
                is_abort = 1;
            }

            res = sem_post(&app.worker_semaphore);
            if (res) {
                fprintf(stderr, "ERROR: Unable to increase worker semaphore\n");
                is_abort = 1;
            }
        }

        vgWritePixels(  app.openvg.video_buffer.c,
                        app.width << 1,
                        VG_sRGB_565,
                        0, 0,
                        app.width, app.height);

        res = vgGetError();
        if (res != 0) {
            fprintf(stderr, "ERROR: Failed to draw image %d\n", res);
        }
        //pthread_mutex_unlock(&app.buffer_mutex);
        // ------------------------------

        VGfloat vg_colour[4] = {1.0f, 1.0f, 1.0f, 1.0f};

        res = pthread_mutex_lock(&app.buffer_mutex);
        if (res)
            fprintf(stderr, "ERROR: pthread_mutex_lock failed with code %d\n", res);

        sprintf(buffer, "camera: %2.2f detect: %2.2f, rfb: %2.2f FPS, CPU: %2.1f%%, Memory: %d kb, T: %.2fC, Objects: %d",
            app.fps,
            app.worker_fps,
            app.rfb_fps,
            app.cpu.cpu,
            app.memory.total_size,
            app.temperature.temp,
            app.worker_objects);

        res = openvg_draw_text(&app, 0, 0, buffer, strlen(buffer), 20, vg_colour);
        if (res)
            fprintf(stderr, "ERROR: failed to draw text\n");

        res = openvg_draw_boxes(&app, vg_colour);
        if (res)
            fprintf(stderr, "ERROR: failed to draw boxes\n");

        res = pthread_mutex_unlock(&app.buffer_mutex);
        if (res)
            fprintf(stderr, "ERROR: pthread_mutex_unlock failed with code %d\n", res);

        if (app.output_type == OUTPUT_STREAM) {
            openvg_read_buffer(&app);
            camera_encode_buffer(&app, app.openvg.video_buffer.c, ((app.width * app.height) << 1));
        } else if (app.output_type == OUTPUT_RFB) {
            openvg_read_buffer(&app);
        }

        EGLBoolean egl_res;
        egl_res = eglSwapBuffers(app.openvg.display, app.openvg.surface);
        if (egl_res == EGL_FALSE) {
             fprintf(stderr, "ERROR: failed to clear screan: 0x%x\n", egl_res);
        }
#endif //OPENVG

#ifdef CONTROL
        control_ssh_key(&app);
#endif // CONTROL

        //usleep(TICK_TIME);
    }
    exit_code = EX_OK;

error:
#ifdef CONTROL
    control_destroy(&app);
#endif //CONTROL

#ifdef RFB
    rfb_destroy(&app);
#endif //RFB 

    if (app.output_type == OUTPUT_STREAM) {
        if (camera_destroy_h264_encoder(&app)) {
            fprintf(stderr, "ERROR: camera_destroy_h264_encoder failed\n");
        }
    }
    if (camera_destroy(&app)) {
        fprintf(stderr, "ERROR: camera_destroy failed\n");
    }

    // destroy semaphore and mutex before stop thread to prevent blocking
    if (sem_destroy(&app.worker_semaphore)) {
        fprintf(stderr, "ERROR: sem_destroy failed to destroy worker_semaphore with code: %d\n", errno);
    }
    if (sem_destroy(&app.buffer_semaphore)) {
        fprintf(stderr, "ERROR: sem_destroy failed to destroy buffer_semaphore with code: %d\n", errno);
    }
    res = pthread_mutex_destroy(&app.buffer_mutex);
    if (res)
        fprintf(stderr, "ERROR: pthread_mutex_destroy failed to destroy buffer mutex with code %d\n", res);


    if (!app.worker_thread_res) {
        res = pthread_join(app.worker_thread, NULL);
        if (res != 0) {
            fprintf(stderr, "ERROR: Failed to close Worker thread. error: %d\n", res);
        }
    }

    if (app.worker_buffer_rgb) {
        free(app.worker_buffer_rgb);
    }

    if (app.worker_buffer_565) {
        free(app.worker_buffer_565);
    }

    if (app.worker_boxes) {
        free(app.worker_boxes);
    }

    if (app.worker_classes) {
        free(app.worker_classes);
    }

    if (app.worker_scores) {
        free(app.worker_scores);
    }

#ifdef TENSORFLOW
    tensorflow_destroy(&app);
#elif DARKNET
    darknet_destroy(&app);
#endif

#ifdef OPENVG
    openvg_destroy(&app);
    dispmanx_destroy(&app);
#endif

    return exit_code;
}

int main(int argc, char** argv)
{
    signal(SIGINT, signal_handler);

    h = kh_init(map_str);
    utils_parse_args(argc, argv);

    unsigned k = kh_get(map_str, h, HELP);
    if (k != kh_end(h)) {
        utils_print_help();
    }
    else {
        main_function();
    }

    kh_destroy(map_str, h);
    return exit_code;
}
