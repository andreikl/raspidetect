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

#include "khash.h"

#include "main.h"
#include "utils.h"
#include "ov5647_helpers.h"

#ifdef OPENCV
#include "opencv2/imgproc/imgproc_c.h"
#endif

KHASH_MAP_INIT_STR(map_str, char *)
extern khash_t(map_str) *h;

char str_buffer[BUFFER_SIZE];

int get_sensor_defaults(int camera_num, char *camera_name, int *width, int *height ) {
    MMAL_COMPONENT_T *camera_info;
    MMAL_STATUS_T status;

    // Try to get the camera name and maximum supported resolution
    status = mmal_component_create(MMAL_COMPONENT_DEFAULT_CAMERA_INFO, &camera_info);
    if (status == MMAL_SUCCESS) {
        MMAL_PARAMETER_CAMERA_INFO_T param;
        param.hdr.id = MMAL_PARAMETER_CAMERA_INFO;
        param.hdr.size = sizeof(param);
        status = mmal_port_parameter_get(camera_info->control, &param.hdr);
        if (status == MMAL_SUCCESS && param.num_cameras > camera_num) {
            // Take the parameters from the first camera listed.
            if (*width == 0)
                *width = param.cameras[camera_num].max_width;
            if (*height == 0)
                *height = param.cameras[camera_num].max_height;
            strncpy(camera_name, param.cameras[camera_num].camera_name, MMAL_PARAMETER_CAMERA_INFO_MAX_STR_LEN);
            camera_name[MMAL_PARAMETER_CAMERA_INFO_MAX_STR_LEN-1] = 0;
        }
        else {
            fprintf(stderr, "ERROR: Failed to read camera info (status: %x, num_cameras: %d)\n", status, param.num_cameras);
            return -1;
        }
        mmal_component_destroy(camera_info);
    }
    else {
        fprintf(stderr, "ERROR: Failed to create camera_info component (status: %x)\n", status);
        return -1;
    }
    return 0;
}

// void preview_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer) {
//     MMAL_STATUS_T status;
//     MMAL_BUFFER_HEADER_T *new_buffer;
//     MMAL_BUFFER_HEADER_T *output_buffer = 0;
//     APP_STATE *state = (APP_STATE *) port->userdata;
//     MMAL_POOL_T *pool = state->preview_port_pool;

//     static int frame_count = 0;

//     // ----- fps
//     // static struct timespec t1;
//     // struct timespec t2;
//     // if (frame_count == 0) {
//     //     clock_gettime(CLOCK_MONOTONIC, &t1);
//     // }
//     // clock_gettime(CLOCK_MONOTONIC, &t2);
//     // float d = (t2.tv_sec + t2.tv_nsec / 1000000000.0) - (t1.tv_sec + t1.tv_nsec / 1000000000.0);
//     // if (d > 0) {
//     //     state->fps = frame_count / d;
//     // } else {
//     //     state->fps = frame_count;
//     // }
//     // frame_count++;
//     // -----

//     if (state->verbose && frame_count % 10 == 0) {
//         fprintf(stderr, "buffer length: %d\n", buffer->length);
//     }

//     mmal_buffer_header_release(buffer);

//     //usleep(100000);

//     // and send one back to the port (if still open)
//     if (port->is_enabled) {
//         new_buffer = mmal_queue_get(pool->queue);
//         if (new_buffer) {
//             status = mmal_port_send_buffer(port, new_buffer);
//         }
//         if (!new_buffer || status != MMAL_SUCCESS) {
//             fprintf(stderr, "ERROR: Unable to return a buffer to the preview port\n");
//         }
//     }
// }

void control_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer) {
    MMAL_STATUS_T status;
    MMAL_BUFFER_HEADER_T *new_buffer;
    MMAL_BUFFER_HEADER_T *output_buffer = 0;
    APP_STATE *state = (APP_STATE *) port->userdata;
    MMAL_POOL_T *pool = state->video_port_pool;

    //static int frame_count = 0;

    // ----- fps
    // static struct timespec t1;
    // struct timespec t2;
    // if (frame_count == 0) {
    //     clock_gettime(CLOCK_MONOTONIC, &t1);
    // }
    // clock_gettime(CLOCK_MONOTONIC, &t2);
    // float d = (t2.tv_sec + t2.tv_nsec / 1000000000.0) - (t1.tv_sec + t1.tv_nsec / 1000000000.0);
    // if (d > 0) {
    //     state->fps = frame_count / d;
    // } else {
    //     state->fps = frame_count;
    // }
    // frame_count++;
    // -----

    // if (state->verbose && frame_count % 10 == 0) {
    //     fprintf(stderr, "buffer cmd: %x\n", buffer->cmd);
    //     fprintf(stderr, "buffer alloc_size: %d\n", buffer->alloc_size);
    //     fprintf(stderr, "buffer length: %d\n", buffer->length);
    //     fprintf(stderr, "buffer offset: %x\n", buffer->offset);
    //     fprintf(stderr, "buffer flags: %x\n", buffer->flags);
    // }
    output_buffer = mmal_queue_get(state->h264_input_pool->queue);
    if (output_buffer) {
        pthread_mutex_lock(&state->buffer_mutex);

        mmal_buffer_header_mem_lock(buffer);

        int value, res;
        res = sem_getvalue(&state->worker_semaphore, &value);
        if (res) {
            fprintf(stderr, "ERROR: Unable to read value from worker semaphore: %d\n", errno);
        }
        if (!value) {
#ifdef OPENCV
            struct timespec imgt1;
            struct timespec imgt2;
            CvMat img1, img2;
            clock_gettime(CLOCK_MONOTONIC, &imgt1);
            cvInitMatHeader(&img1, state->width, state->height, CV_8UC3, buffer->data, state->width * 3);
            cvInitMatHeader(&img2, state->worker_width, state->worker_height, CV_8UC3, state->worker_buffer, state->worker_width * 3);
            cvResize(&img1, &img2, CV_INTER_LINEAR);
            clock_gettime(CLOCK_MONOTONIC, &imgt2);
            state->rtime = imgt2.tv_nsec - imgt1.tv_nsec;
#elif
            // convert to tensorflow 300x300
            if (state->worker_width != state->width) {
                // TODO: implement model which width is equal image width
                int b_index = 0;
                char *worker_buffer = (char*)state->worker_buffer;
                for (int i = 0, w_index = 0; i < state->worker_height; i++) {
                    b_index = i * state->width * 3;
                    for (int j = 0; j < state->worker_width; j++) {
                        char r = buffer->data[b_index++];
                        char g = buffer->data[b_index++];
                        char b = buffer->data[b_index++];
                        worker_buffer[w_index++] = r;
                        worker_buffer[w_index++] = g;
                        worker_buffer[w_index++] = b;
                    }
                }
            } else {
                // convert to darknet 416x416
                memcpy(state->worker_buffer,
                    buffer->data,
                    state->worker_width * state->worker_height * state->worker_pixel_bytes);
            }
#endif
            res = sem_post(&state->worker_semaphore);
            if (res) {
                fprintf(stderr, "ERROR: Unable to increase worker semaphore \n");
            }
        }

        // 4bytes array | 3bytes array to 4bytes array (optimized)
        // block below increase cpu to 40% which is based on RGB24 and 416x416 resolution on arm61
#ifdef ENV32BIT
        int32_t* source_data = (int32_t*)buffer->data;
        int32_t* dest_data = (int32_t*)output_buffer->data;
        int32_t* overlay_data = (int32_t*)state->overlay_buffer;
        int size = state->width * state->height;
        uint32_t data_old, data_new, data_overlay, result = 0;
        data_old = data_new = source_data[0];
        for (int i = 0, xy_index = 0, res_index = 0, bits = 0; i < size; i++, bits += 24) {
            data_old = data_new;
            data_new = source_data[xy_index];
            data_overlay = overlay_data[i];
            int move = bits & 0B11111; //x % 32
            switch(move) {
                case 0: //0-24
                    xy_index++;
                    result = (data_new & 0x00FFFFFF) | (data_overlay & 0x00FFFFFF); //rgb
                    break;

                case 24: //24-48
                    xy_index++;
                    result = (data_old & 0xFF000000) | ((data_overlay & 0x000000FF) << 24) | result; //r
                    dest_data[res_index++] = result;
                    result = (data_new & 0x0000FFFF) | ((data_overlay & 0x00FFFF00) >> 8); //gb
                    break;

                case 16: //48-72
                    xy_index++;
                    result = (data_old & 0xFFFF0000) | ((data_overlay & 0x0000FFFF) << 16) | result; //rg
                    dest_data[res_index++] = result;
                    result = (data_new & 0x000000FF) | ((data_overlay & 0x00FF0000) >> 16); //b
                    break;

                case 8: //72-96
                    result = (data_old & 0xFFFFFF00) | ((data_overlay & 0x00FFFFFF) << 8) | result; //rgb
                    dest_data[res_index++] = result;
                    break;
            }
        }
#elif
        fprintf(stderr, "ERROR: 64bit overlay isn't implemented\n");
        memcpy(output_buffer->data, buffer->data, buffer->length);
#endif

        // 4bytes array | 3bytes array to 4bytes array (not optimized)
        // int8_t* source_data = (int8_t*)buffer->data;
        // int8_t* dest_data = (int8_t*)output_buffer->data;
        // int8_t* overlay_data = (int8_t*)state->overlay_buffer;
        // int size = state->width * state->height * 3;
        // int buffer_index = 0;
        // int overlay_index = 0;
        // while (buffer_index < size) {
        //     uint32_t rx = source_data[buffer_index];
        //     uint8_t ry = overlay_data[overlay_index++];
        //     dest_data[buffer_index++] = rx | ry;
        //     uint8_t gx = source_data[buffer_index];
        //     uint8_t gy = overlay_data[overlay_index++];
        //     dest_data[buffer_index++] = gx | gy;
        //     uint8_t bx = source_data[buffer_index];
        //     uint8_t by = overlay_data[overlay_index++];
        //     dest_data[buffer_index++] = bx | by;
        //     overlay_index++;
        // }

        // 4bytes array | 4bytes array to 4bytes array
        /*int32_t* source_data = (int32_t*)buffer->data;
        int32_t* dest_data = (int32_t*)output_buffer->data;
        int32_t* overlay_data = (int32_t*)state->overlay_buffer;
        int size = buffer->length >> 2;
        for (int i = 0; i < size; i++) {
            uint32_t x = source_data[i];
            uint32_t y = overlay_data[i];
            dest_data[i] = x | (y & 0X00FFFFFF);
        }*/

        // 1byte array | 4bytes array to 1bytes array
        // block below increase cpu to 27% which is based on YUV420 and 416x416 resolution on one arm61
// #ifdef ENV32BIT
//         int32_t* source_data = (int32_t*)buffer->data;
//         int32_t* dest_data = (int32_t*)output_buffer->data;
//         int32_t* overlay_data = (int32_t*)state->overlay_buffer;
//         int size = (state->width * state->height) >> 2; // size / 4
//         int i = 0;
//         int j = 0;
//         while (i < size) {
//             uint32_t x = source_data[i];
//             uint32_t y1 = overlay_data[j++];
//             uint32_t y2 = overlay_data[j++];
//             uint32_t y3 = overlay_data[j++];
//             uint32_t y4 = overlay_data[j++];
//             x = x | (y1 & 0x000000FF) | ((y2 & 0x000000FF) << 8) | ((y3 & 0x000000FF) << 16) | ((y4 & 0x000000FF) << 24);
//             dest_data[i++] = x;
//         }
//         size = i << 2;
//         memcpy(&output_buffer->data[size], &buffer->data[size], buffer->length - size);
// #elif
//         fprintf(stderr, "ERROR: 64bit overlay isn't implemented\n");
//         memcpy(output_buffer->data, buffer->data, buffer->length);
// #endif

        //copy without overlay
        // memcpy(output_buffer->data, buffer->data, buffer->length);

        output_buffer->length = buffer->length;

        mmal_buffer_header_mem_unlock(buffer);
        pthread_mutex_unlock(&state->buffer_mutex);

        if (mmal_port_send_buffer(state->h264_input_port, output_buffer) != MMAL_SUCCESS) {
            fprintf(stderr, "ERROR: Unable to send buffer \n");
        }
    }
    else {
        fprintf(stderr, "ERROR: mmal_queue_get returns empty buffer\n");
    }

    mmal_buffer_header_release(buffer);

    // and send one back to the port (if still open)
    if (port->is_enabled) {
        new_buffer = mmal_queue_get(pool->queue);
        if (new_buffer) {
            status = mmal_port_send_buffer(port, new_buffer);
        }

        if (!new_buffer || status != MMAL_SUCCESS) {
            fprintf(stderr, "ERROR: Unable to return a buffer to the video port\n");
        }
    }
}

void h264_input_buffer_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer) {
    APP_STATE *state = (APP_STATE *) port->userdata;
    if (state->verbose) {
    //    fprintf(stderr, "h264_input_buffer_callback: %s\n", __func__);
    }
    mmal_buffer_header_release(buffer);
}

void h264_output_buffer_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer) {
    MMAL_BUFFER_HEADER_T *new_buffer;
    APP_STATE *state = (APP_STATE *) port->userdata;
    MMAL_POOL_T *pool = state->h264_output_pool;

    if (state->verbose) {
    //    fprintf(stderr, "h264_output_buffer_callback: %s\n", __func__);
    }

    mmal_buffer_header_mem_lock(buffer);
    fwrite(buffer->data, 1, buffer->length, stdout);
    mmal_buffer_header_mem_unlock(buffer);

    mmal_buffer_header_release(buffer);
    if (port->is_enabled) {
        MMAL_STATUS_T status;

        new_buffer = mmal_queue_get(pool->queue);

        if (new_buffer) {
            status = mmal_port_send_buffer(port, new_buffer);
        }

        if (!new_buffer || status != MMAL_SUCCESS) {
            fprintf(stderr, "ERROR: Unable to return a buffer to the video port\n");
        }
    }
}

// void rgb_input_buffer_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer) {
//     APP_STATE *state = (APP_STATE *) port->userdata;
//     if (state->verbose) {
//     //    fprintf(stderr, "rgb_input_buffer_callback: %s\n", __func__);
//     }
//     mmal_buffer_header_release(buffer);
// }

// void rgb_output_buffer_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer) {
//     MMAL_STATUS_T status;
//     MMAL_BUFFER_HEADER_T *new_buffer;
//     APP_STATE *state = (APP_STATE *) port->userdata;
//     MMAL_POOL_T *pool = state->rgb_output_pool;

//     static int frame_count = 0;

//     // ----- fps
//     static struct timespec t1;
//     struct timespec t2;
//     if (frame_count == 0) {
//         clock_gettime(CLOCK_MONOTONIC, &t1);
//     }
//     clock_gettime(CLOCK_MONOTONIC, &t2);
//     float d = (t2.tv_sec + t2.tv_nsec / 1000000000.0) - (t1.tv_sec + t1.tv_nsec / 1000000000.0);
//     if (d > 0) {
//         state->fps = frame_count / d;
//     } else {
//         state->fps = frame_count;
//     }
//     frame_count++;
//     // -----

//     if (state->verbose && frame_count % 10 == 0) {
//         fprintf(stderr, "buffer length: %d\n", buffer->length);
//     }

//     mmal_buffer_header_release(buffer);

//     // and send one back to the port (if still open)
//     if (port->is_enabled) {
//         new_buffer = mmal_queue_get(pool->queue);
//         if (new_buffer) {
//             status = mmal_port_send_buffer(port, new_buffer);
//         }

//         if (!new_buffer || status != MMAL_SUCCESS) {
//             fprintf(stderr, "ERROR: Unable to return a buffer to the preview port\n");
//         }
//     }

// }

void fill_port_buffer(MMAL_PORT_T *port, MMAL_POOL_T *pool) {
    int q;
    int num = mmal_queue_length(pool->queue);

    for (q = 0; q < num; q++) {
        MMAL_BUFFER_HEADER_T *buffer = mmal_queue_get(pool->queue);
        if (!buffer) {
            fprintf(stderr, "ERROR: Unable to get a required buffer %d from pool queue\n", q);
        }

        if (mmal_port_send_buffer(port, buffer) != MMAL_SUCCESS) {
            fprintf(stderr, "ERROR: Unable to send a buffer to port (%d)\n", q);
        }
    }
}

int create_camera_component(APP_STATE *state) {
    MMAL_PORT_T *video_port = NULL;//, *preview_port = NULL, *still_port = NULL;
    MMAL_POOL_T *video_port_pool = NULL;//, *preview_port_pool = NULL;
    MMAL_ES_FORMAT_T *format;
    MMAL_COMPONENT_T *camera = 0;
    MMAL_STATUS_T status;

    // ----- Create the component
    status = mmal_component_create(MMAL_COMPONENT_DEFAULT_CAMERA, &camera);
    if (status != MMAL_SUCCESS) {
        fprintf(stderr, "ERROR: Failed to create camera component (status: %x)\n", status);
        goto error;
    }
    // -----

    // ----- set camera number
    MMAL_PARAMETER_INT32_T camera_num = {
        { MMAL_PARAMETER_CAMERA_NUM, sizeof(camera_num) },
        state->camera_num
    };
    status = mmal_port_parameter_set(camera->control, &camera_num.hdr);
    if (status != MMAL_SUCCESS) {
        fprintf(stderr, "ERROR: Could not select camera (status: %x)\n", status);
        goto error;
    }
    // -----

    // ----- vaidate camera response
    if (!camera->output_num) {
        status = MMAL_ENOSYS;
        fprintf(stderr, "ERROR: Camera doesn't have output ports\n");
        goto error;
    }
    // -----

    // ----- save camera response
    state->camera = camera;
    //state->preview_port = preview_port = camera->output[MMAL_CAMERA_PREVIEW_PORT];
    state->video_port = video_port = camera->output[MMAL_CAMERA_VIDEO_PORT];
    //state->still_port = still_port = camera->output[MMAL_CAMERA_CAPTURE_PORT];
    // -----

    // ----- set up the camera configuration
    {
        MMAL_PARAMETER_CAMERA_CONFIG_T cam_config = {
            { MMAL_PARAMETER_CAMERA_CONFIG, sizeof(cam_config) },
            .max_stills_w = state->worker_width,
            .max_stills_h = state->worker_height,
            .stills_yuv422 = 0,
            .one_shot_stills = 1,
            .max_preview_video_w = state->width,
            .max_preview_video_h = state->height,
            .num_preview_video_frames = 3,
            .stills_capture_circular_buffer_height = 0,
            .fast_preview_resume = 0,
            .use_stc_timestamp = MMAL_PARAM_TIMESTAMP_MODE_RESET_STC
        };
        status = mmal_port_parameter_set(camera->control, &cam_config.hdr);
        if (status != MMAL_SUCCESS && status != MMAL_ENOSYS) {
            fprintf(stderr, "ERROR: unable to set camera port parameters (%x)\n", status);
            goto error;
        }
    }

    // ------ Setup camera video port format
    format = video_port->format;
    //fastest 10% cpu on arm6 with 30FPS
    //format->encoding = MMAL_ENCODING_I420;
    //15% cpu on arm6 with 30FPS
    format->encoding = MMAL_ENCODING_RGB24;
    //format->encoding = MMAL_ENCODING_RGBA;
    //format->encoding_variant = MMAL_ENCODING_I420;
    format->es->video.width = state->width;
    format->es->video.height = state->height;
    format->es->video.crop.x = 0;
    format->es->video.crop.y = 0;
    format->es->video.crop.width = state->width;
    format->es->video.crop.height = state->height;
    format->es->video.frame_rate.num = 10;
    format->es->video.frame_rate.den = 1;
    status = mmal_port_format_commit(video_port);
    if (status != MMAL_SUCCESS) {
        fprintf(stderr, "ERROR: unable to commit camera video port format (status: %x)\n", status);
        goto error;
    }

    video_port->buffer_size = video_port->buffer_size_recommended;
    video_port->buffer_num = video_port->buffer_num_recommended;;
    video_port->userdata = (struct MMAL_PORT_USERDATA_T *)state;

    if (state->verbose) {
        fprintf(stderr, "INFO: camera video buffer_size = %d\n", video_port->buffer_size);
        fprintf(stderr, "INFO: camera video buffer_num = %d\n", video_port->buffer_num);
    }
    // ------

    // ----- Setup camera preview port format
    // format = preview_port->format;
    // //fastest 10% cpu on arm6
    // //format->encoding = MMAL_ENCODING_I420;
    // //15% cpu on arm6
    // format->encoding = MMAL_ENCODING_RGB24;
    // //format->encoding = MMAL_ENCODING_RGBA;
    // //format->encoding_variant = MMAL_ENCODING_I420;
    // format->es->video.width = state->worker_width;
    // format->es->video.height = state->worker_height;
    // format->es->video.crop.x = 0;
    // format->es->video.crop.y = 0;
    // format->es->video.crop.width = state->worker_width;
    // format->es->video.crop.height = state->worker_height;
    // format->es->video.frame_rate.num = 10;
    // format->es->video.frame_rate.den = 1;
    // status = mmal_port_format_commit(preview_port);
    // if (status != MMAL_SUCCESS) {
    //     fprintf(stderr, "ERROR: unable to commit camera preview port format (status: %x)\n", status);
    //     goto error;
    // }

    // preview_port->buffer_size = preview_port->buffer_size_recommended;
    // preview_port->buffer_num = 2;
    // preview_port->userdata = (struct MMAL_PORT_USERDATA_T *)state;

    // if (state->verbose) {
    //     fprintf(stderr, "INFO: camera preview buffer_size = %d\n", preview_port->buffer_size);
    //     fprintf(stderr, "INFO: camera preview buffer_num = %d\n", preview_port->buffer_num);
    // }
    // -----

    state->video_port_pool = video_port_pool = (MMAL_POOL_T *) mmal_port_pool_create(video_port,
        video_port->buffer_num,
        video_port->buffer_size);

    // state->preview_port_pool = preview_port_pool = (MMAL_POOL_T *) mmal_port_pool_create(preview_port,
    //     preview_port->buffer_num,
    //     preview_port->buffer_size);

    // Enable video port
    status = mmal_port_enable(video_port, control_callback);
    if (status != MMAL_SUCCESS) {
        fprintf(stderr, "ERROR: Unable to set control_callback (status: %x)\n", status);
        goto error;
    }


    // Enable preview port
    // status = mmal_port_enable(preview_port, preview_callback);
    // if (status != MMAL_SUCCESS) {
    //     fprintf(stderr, "ERROR: Unable to set preview_callback (status: %x)\n", status);
    //     goto error;
    // }

    // Enable video component
    status = mmal_component_enable(camera);
    if (status != MMAL_SUCCESS) {
        fprintf(stderr, "camera component couldn't be enabled (status: %x)\n", status);
        goto error;
    }

    fill_port_buffer(video_port, video_port_pool);
    // fill_port_buffer(preview_port, preview_port_pool);

    status = mmal_port_parameter_set_boolean(video_port, MMAL_PARAMETER_CAPTURE, 1);
    if (status != MMAL_SUCCESS) {
        fprintf(stderr, "ERROR: Failed to start video port (status: %x)\n", status);
        goto error;
    }

    // status = mmal_port_parameter_set_boolean(preview_port, MMAL_PARAMETER_CAPTURE, 1);
    // if (status != MMAL_SUCCESS) {
    //     fprintf(stderr, "ERROR: Failed to start preview port (status: %x)\n", status);
    //     goto error;
    // }

    if (state->verbose) {
        fprintf(stderr, "INFO: Camera has been created\n");
    }

    return 0;

error:
    if (camera)
        mmal_component_destroy(camera);

    return 1;
}

int create_encoder_h264(APP_STATE *state) {
    MMAL_STATUS_T status;
    MMAL_COMPONENT_T *encoder = 0;

    MMAL_PORT_T *input_port = NULL, *output_port = NULL;
    MMAL_POOL_T *input_port_pool = NULL, *output_port_pool = NULL;

    status = mmal_component_create(MMAL_COMPONENT_DEFAULT_VIDEO_ENCODER, &encoder);
    if (status != MMAL_SUCCESS) {
        fprintf(stderr, "ERROR: unable to create encoder (%x)\n", status);
        goto error;
    }

    state->h264_input_port = input_port = encoder->input[0];
    state->h264_output_port = output_port = encoder->output[0];

    mmal_format_copy(input_port->format, state->video_port->format);
    input_port->buffer_size = input_port->buffer_size_recommended;
    input_port->buffer_num = input_port->buffer_num_recommended;

    // Commit the port changes to the input port
    status = mmal_port_format_commit(input_port);
    if (status != MMAL_SUCCESS) {
        fprintf(stderr, "ERROR: unable to commit encoder input port format (%x)\n", status);
        goto error;
    }

    mmal_format_copy(output_port->format, input_port->format);
    output_port->format->encoding = MMAL_ENCODING_H264;
    //output_port->format->bitrate = 2000000;
    //output_port->format->bitrate = 0;

    output_port->buffer_size = output_port->buffer_size_recommended;
    output_port->buffer_num = output_port->buffer_num_recommended;

    // Commit the port changes to the output port
    status = mmal_port_format_commit(output_port);
    if (status != MMAL_SUCCESS) {
        fprintf(stderr, "ERROR: unable to commit encoder output port format (%x)\n", status);
        goto error;
    }

    if (state->verbose) {
        fprintf(stderr, "INFO: encoder h264 input buffer_size = %d\n", input_port->buffer_size);
        fprintf(stderr, "INFO: encoder h264 input buffer_num = %d\n", input_port->buffer_num);

        fprintf(stderr, "INFO: encoder h264 output buffer_size = %d\n", output_port->buffer_size);
        fprintf(stderr, "INFO: encoder h264 output buffer_num = %d\n", output_port->buffer_num);
    }

    state->h264_input_pool = input_port_pool = (MMAL_POOL_T *) mmal_port_pool_create(input_port,
        input_port->buffer_num,
        input_port->buffer_size);
    input_port->userdata = (struct MMAL_PORT_USERDATA_T *) state;

    status = mmal_port_enable(input_port, h264_input_buffer_callback);
    if (status != MMAL_SUCCESS) {
        fprintf(stderr, "ERROR: unable to enable encoder input port (%x)\n", status);
        goto error;
    }

    state->h264_output_pool = output_port_pool = (MMAL_POOL_T *) mmal_port_pool_create(output_port,
        output_port->buffer_num,
        output_port->buffer_size);
    output_port->userdata = (struct MMAL_PORT_USERDATA_T *) state;

    status = mmal_port_enable(output_port, h264_output_buffer_callback);
    if (status != MMAL_SUCCESS) {
        fprintf(stderr, "ERROR: unable to enable encoder h264 output port (%x)\n", status);
        goto error;
    }

    fill_port_buffer(output_port, output_port_pool);
    state->encoder_h264 = encoder;

    if (state->verbose) {
        fprintf(stderr, "INFO: Encoder h264 has been created\n");
    }

    return 0;

error:
    if (encoder)
        mmal_component_destroy(encoder);

    return 1;
}

// int create_encoder_rgb(APP_STATE *state) {
//     MMAL_STATUS_T status;
//     MMAL_COMPONENT_T *encoder = 0;

//     MMAL_PORT_T *input_port = NULL, *output_port = NULL;
//     MMAL_POOL_T *input_port_pool = NULL, *output_port_pool = NULL;

//     status = mmal_component_create(MMAL_COMPONENT_DEFAULT_VIDEO_ENCODER, &encoder);
//     if (status != MMAL_SUCCESS) {
//         fprintf(stderr, "ERROR: unable to create encoder RGB (%x)\n", status);
//         goto error;
//     }

//     state->rgb_input_port = input_port = encoder->input[0];
//     state->rgb_output_port = output_port = encoder->output[0];

//     mmal_format_copy(input_port->format, state->video_port->format);
//     input_port->buffer_size = input_port->buffer_size_recommended;
//     input_port->buffer_num = input_port->buffer_num_recommended;

//     // Commit the port changes to the input port
//     status = mmal_port_format_commit(input_port);
//     if (status != MMAL_SUCCESS) {
//         fprintf(stderr, "ERROR: unable to commit encoder RGB input port format (%x)\n", status);
//         goto error;
//     }

//     mmal_format_copy(output_port->format, state->preview_port->format);
//     output_port->buffer_size = output_port->buffer_size_recommended;
//     output_port->buffer_num = output_port->buffer_num_recommended;

//     // Commit the port changes to the output port
//     status = mmal_port_format_commit(output_port);
//     if (status != MMAL_SUCCESS) {
//         fprintf(stderr, "ERROR: unable to commit encoder RGB output port format (%x)\n", status);
//         goto error;
//     }

//     if (state->verbose) {
//         fprintf(stderr, "INFO: encoder input buffer_size = %d\n", input_port->buffer_size);
//         fprintf(stderr, "INFO: encoder input buffer_num = %d\n", input_port->buffer_num);

//         fprintf(stderr, "INFO: encoder output buffer_size = %d\n", output_port->buffer_size);
//         fprintf(stderr, "INFO: encoder output buffer_num = %d\n", output_port->buffer_num);
//     }

//     state->rgb_input_pool = input_port_pool = (MMAL_POOL_T *) mmal_port_pool_create(input_port,
//         input_port->buffer_num,
//         input_port->buffer_size);
//     input_port->userdata = (struct MMAL_PORT_USERDATA_T *) state;

//     status = mmal_port_enable(input_port, rgb_input_buffer_callback);
//     if (status != MMAL_SUCCESS) {
//         fprintf(stderr, "ERROR: unable to enable RGB encoder input port (%x)\n", status);
//         goto error;
//     }

//     state->rgb_output_pool = output_port_pool = (MMAL_POOL_T *) mmal_port_pool_create(output_port,
//         output_port->buffer_num,
//         output_port->buffer_size);
//     output_port->userdata = (struct MMAL_PORT_USERDATA_T *) state;

//     status = mmal_port_enable(output_port, rgb_output_buffer_callback);
//     if (status != MMAL_SUCCESS) {
//         fprintf(stderr, "ERROR: unable to enable encoder RGB output port (%x)\n", status);
//         goto error;
//     }

//     fill_port_buffer(output_port, output_port_pool);
//     state->encoder_rgb = encoder;

//     if (state->verbose) {
//         fprintf(stderr, "INFO: Encoder RGB has been created\n");
//     }

//     return 0;

// error:
//     if (encoder)
//         mmal_component_destroy(encoder);

//     return 1;
// }

// int create_preview_component(APP_STATE *state) {
//     MMAL_STATUS_T status;
//     MMAL_COMPONENT_T *preview = 0;
//     MMAL_CONNECTION_T *connection = 0;
//     MMAL_PORT_T *input_port;

//     status = mmal_component_create(MMAL_COMPONENT_DEFAULT_VIDEO_RENDERER, &preview);
//     if (status != MMAL_SUCCESS) {
//         fprintf(stderr, "ERROR: unable to create preview (%x)\n", status);
//         goto error;
//     }

//     state->preview = preview;
//     input_port = preview->input[0];
//     {
//         MMAL_DISPLAYREGION_T param;
//         param.hdr.id = MMAL_PARAMETER_DISPLAYREGION;
//         param.hdr.size = sizeof (MMAL_DISPLAYREGION_T);
//         param.set = MMAL_DISPLAY_SET_LAYER;
//         param.layer = 0;
//         param.set |= MMAL_DISPLAY_SET_FULLSCREEN;
//         param.fullscreen = 1;
//         status = mmal_port_parameter_set(input_port, &param.hdr);
//         if (status != MMAL_SUCCESS && status != MMAL_ENOSYS) {
//             fprintf(stderr, "ERROR: unable to set preview port parameters (%x)\n", status);
//             goto error;
//         }
//     }

//     status = mmal_connection_create(&connection, state->preview_port, input_port, MMAL_CONNECTION_FLAG_TUNNELLING | MMAL_CONNECTION_FLAG_ALLOCATION_ON_INPUT);
//     if (status != MMAL_SUCCESS) {
//         fprintf(stderr, "ERROR: unable to create connection (%x)\n", status);
//         goto error;
//     }

//     status = mmal_connection_enable(connection);
//     if (status != MMAL_SUCCESS) {
//         fprintf(stderr, "ERROR: unable to enable connection (%u)\n", status);
//         goto error;
//     }

//     if (state->verbose) {
//         fprintf(stderr, "INFO: Preview has been created\n");
//     }
//     return 0;

// error:
//     if (preview)
//         mmal_component_destroy(preview);

//     return 1;
// }

/**
 * Destroy the camera component
 *
 * @param state Pointer to state control struct
 *
 */
void destroy_components(APP_STATE *state) {
    if (state->encoder_h264) {
        mmal_component_destroy(state->encoder_h264);
        state->encoder_h264 = NULL;
    }

    // if (state->encoder_rgb) {
    //     mmal_component_destroy(state->encoder_rgb);
    //     state->encoder_rgb = NULL;
    // }

    if (state->camera) {
        mmal_component_destroy(state->camera);
        state->camera = NULL;
    }
}
