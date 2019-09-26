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
#include "ov5647.h"

#ifdef OPENCV
#include "opencv2/imgproc/imgproc_c.h"
#endif

KHASH_MAP_INIT_STR(map_str, char *)
extern khash_t(map_str) *h;

char str_buffer[BUFFER_SIZE];

extern int is_abort;

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

void control_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer) {
    MMAL_STATUS_T status;
    APP_STATE *state = (APP_STATE *) port->userdata;

    mmal_buffer_header_mem_lock(buffer);
    memcpy(state->openvg.video_buffer, buffer->data, buffer->length);
    mmal_buffer_header_mem_unlock(buffer);
    mmal_buffer_header_release(buffer);

    sem_post(&state->buffer_semaphore);

    // and send one back to the port (if still open)
    if (port->is_enabled && !is_abort) {
        MMAL_BUFFER_HEADER_T *new_buffer = mmal_queue_get(state->mmal.video_port_pool->queue);
        if (new_buffer) {
            status = mmal_port_send_buffer(port, new_buffer);
        }

        if (!new_buffer || status != MMAL_SUCCESS) {
            fprintf(stderr, "ERROR: can't return a buffer to the video port\n");
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
    MMAL_POOL_T *pool = state->mmal.h264_output_pool;

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

void encode_buffer(APP_STATE *state, char *buffer, int length) {
    MMAL_BUFFER_HEADER_T *output_buffer = mmal_queue_get(state->mmal.h264_input_pool->queue);
    if (output_buffer) {
        memcpy(output_buffer->data, buffer, length);
        output_buffer->length = length;

        if (mmal_port_send_buffer(state->mmal.h264_input_port, output_buffer) != MMAL_SUCCESS) {
            fprintf(stderr, "ERROR: Unable to send buffer \n");
        }
    }
    else {
        fprintf(stderr, "ERROR: h264 queue returns empty buffer\n");
    }
}

int create_camera_component(APP_STATE *state) {
    MMAL_PORT_T *video_port = NULL;
    MMAL_POOL_T *video_port_pool = NULL;
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
        state->mmal.camera_num
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
    state->mmal.camera = camera;
    state->mmal.video_port = video_port = camera->output[MMAL_CAMERA_VIDEO_PORT];
    // -----

    // ----- set up the camera configuration
    {
        MMAL_PARAMETER_CAMERA_CONFIG_T cam_config = {
            { MMAL_PARAMETER_CAMERA_CONFIG, sizeof(cam_config) },
            .max_stills_w = state->worker_width,
            .max_stills_h = state->worker_height,
            .stills_yuv422 = 0,
            .one_shot_stills = 0,
            .max_preview_video_w = state->video_width,
            .max_preview_video_h = state->video_height,
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
    //format->encoding = MMAL_ENCODING_RGB24;
    format->encoding = MMAL_ENCODING_RGB16;
    //format->encoding = MMAL_ENCODING_RGBA;
    format->encoding_variant = MMAL_ENCODING_RGB16;
    format->es->video.width = state->video_width;
    format->es->video.height = state->video_height;
    format->es->video.crop.x = 0;
    format->es->video.crop.y = 0;
    format->es->video.crop.width = state->video_width;
    format->es->video.crop.height = state->video_height;
    format->es->video.frame_rate.num = 10;
    format->es->video.frame_rate.den = 1;
    status = mmal_port_format_commit(video_port);
    if (status != MMAL_SUCCESS) {
        fprintf(stderr, "ERROR: unable to commit camera video port format (status: %x)\n", status);
        goto error;
    }

    // Enable video port
    status = mmal_port_enable(video_port, control_callback);
    if (status != MMAL_SUCCESS) {
        fprintf(stderr, "ERROR: Unable to set control_callback (status: %x)\n", status);
        goto error;
    }

    video_port->buffer_size = video_port->buffer_size_recommended;
    video_port->buffer_num = video_port->buffer_num_recommended;;
    video_port->userdata = (struct MMAL_PORT_USERDATA_T *)state;

    // if (state->verbose) {
    //     fprintf(stderr, "INFO: camera video buffer_size = %d\n", video_port->buffer_size);
    //     fprintf(stderr, "INFO: camera video buffer_num = %d\n", video_port->buffer_num);
    // }
    // ------

    state->mmal.video_port_pool = video_port_pool = (MMAL_POOL_T *) mmal_port_pool_create(video_port,
        video_port->buffer_num,
        video_port->buffer_size);

    // Enable video component
    status = mmal_component_enable(camera);
    if (status != MMAL_SUCCESS) {
        fprintf(stderr, "camera component couldn't be enabled (status: %x)\n", status);
        goto error;
    }

    status = mmal_port_parameter_set_boolean(video_port, MMAL_PARAMETER_CAPTURE, 1);
    if (status != MMAL_SUCCESS) {
        fprintf(stderr, "ERROR: Failed to start video port (status: %x)\n", status);
        goto error;
    }

    fill_port_buffer(video_port, video_port_pool);

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

    state->mmal.h264_input_port = input_port = encoder->input[0];
    state->mmal.h264_output_port = output_port = encoder->output[0];

    mmal_format_copy(input_port->format, state->mmal.video_port->format);
    //input_port->format->encoding = MMAL_ENCODING_RGB24;
    //input_port->format->encoding_variant = MMAL_ENCODING_RGB24;
    // input_port->format->es->video.width = state->worker_width;
    // input_port->format->es->video.height = state->worker_height;
    // input_port->format->es->video.crop.x = 0;
    // input_port->format->es->video.crop.y = 0;
    // input_port->format->es->video.crop.width = state->worker_width;
    // input_port->format->es->video.crop.height = state->worker_height;
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

    // if (state->verbose) {
    //     fprintf(stderr, "INFO: encoder h264 input buffer_size = %d\n", input_port->buffer_size);
    //     fprintf(stderr, "INFO: encoder h264 input buffer_num = %d\n", input_port->buffer_num);

    //     fprintf(stderr, "INFO: encoder h264 output buffer_size = %d\n", output_port->buffer_size);
    //     fprintf(stderr, "INFO: encoder h264 output buffer_num = %d\n", output_port->buffer_num);
    // }

    state->mmal.h264_input_pool = input_port_pool = (MMAL_POOL_T *) mmal_port_pool_create(input_port,
        input_port->buffer_num,
        input_port->buffer_size);
    input_port->userdata = (struct MMAL_PORT_USERDATA_T *) state;

    status = mmal_port_enable(input_port, h264_input_buffer_callback);
    if (status != MMAL_SUCCESS) {
        fprintf(stderr, "ERROR: unable to enable encoder input port (%x)\n", status);
        goto error;
    }

    state->mmal.h264_output_pool = output_port_pool = (MMAL_POOL_T *) mmal_port_pool_create(output_port,
        output_port->buffer_num,
        output_port->buffer_size);
    output_port->userdata = (struct MMAL_PORT_USERDATA_T *) state;

    status = mmal_port_enable(output_port, h264_output_buffer_callback);
    if (status != MMAL_SUCCESS) {
        fprintf(stderr, "ERROR: unable to enable encoder h264 output port (%x)\n", status);
        goto error;
    }

    fill_port_buffer(output_port, output_port_pool);
    state->mmal.encoder_h264 = encoder;

    if (state->verbose) {
        fprintf(stderr, "INFO: Encoder h264 has been created\n");
    }

    return 0;

error:
    if (encoder)
        mmal_component_destroy(encoder);

    return 1;
}



//  Destroy the camera component
 void destroy_components(APP_STATE *state) {
    if (state->mmal.encoder_h264) {
        mmal_component_destroy(state->mmal.encoder_h264);
        state->mmal.encoder_h264 = NULL;
    }

    if (state->mmal.camera) {
        mmal_component_destroy(state->mmal.camera);
        state->mmal.camera = NULL;
    }
}
