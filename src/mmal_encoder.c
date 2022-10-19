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
#include "mmal.h"

#include "bcm_host.h"

extern struct app_state_t app;
extern int is_abort;

static const char* get_mmal_message(int result)
{
    if (result == MMAL_SUCCESS) {
        return "MMAL_SUCCESS";
    }
    else if (result == MMAL_ENOMEM) {
        return "MMAL_ENOMEM: Out of memory";
    }
    else if (result == MMAL_ENOSPC) {
        return "MMAL_ENOSPC: Out of resources (other than memory)";
    }
    else if (result == MMAL_EINVAL) {
        return "MMAL_EINVAL: Argument is invalid";
    }
    else if (result == MMAL_ENOSYS) {
        return "MMAL_ENOSYS: Function not implemented";
    }
    else if (result == MMAL_ENOENT) {
        return "MMAL_ENOENT: No such file or directory";
    }
    else if (result == MMAL_ENXIO) {
        return "MMAL_ENXIO: No such device or address";
    }
    else if (result == MMAL_EIO) {
        return "MMAL_EIO: I/O error";
    }
    else if (result == MMAL_ESPIPE) {
        return "MMAL_ESPIPE: Illegal seek";
    }
    else if (result == MMAL_ECORRUPT) {
        return "MMAL_ECORRUPT: Data is corrupt";
    }
    else if (result == MMAL_ENOTREADY) {
        return "MMAL_ENOTREADY: Component is not ready";
    }
    else if (result == MMAL_ECONFIG) {
        return "MMAL_ECONFIG: Component is not configured";
    }
    else if (result == MMAL_EISCONN) {
        return "MMAL_EISCONN: Port is already connected";
    }
    else if (result == MMAL_ENOTCONN) {
        return "MMAL_ENOTCONN: Port is disconnected";
    }
    else if (result == MMAL_EAGAIN) {
        return "MMAL_EAGAIN: Resource temporarily unavailable. Try again later";
    }
    else if (result == MMAL_EFAULT) {
        return "MMAL_EFAULT: Bad address";
    }
    else {
        return "UNKNOWN";
    }
}

int mmal_get_capabilities(int camera_num, char *camera_name, int *width, int *height )
{
    MMAL_COMPONENT_T *camera_info;
    MMAL_STATUS_T status;

    bcm_host_init();

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

static void control_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
    int res;
    app_state_t *app = (app_state_t *) port->userdata;

    mmal_buffer_header_mem_lock(buffer);
    memcpy(app.openvg.video_buffer.c, buffer->data, buffer->length);
    mmal_buffer_header_mem_unlock(buffer);
    mmal_buffer_header_release(buffer);

    sem_post(&app.buffer_semaphore);

    // and send one back to the port (if still open)
    if (port->is_enabled && !is_abort) {
        MMAL_BUFFER_HEADER_T *new_buffer = mmal_queue_get(app.mmal.video_port_pool->queue);
        if (new_buffer) {
            res = mmal_port_send_buffer(port, new_buffer);
            if (res) {
                fprintf(stderr, "ERROR: mmal_port_send_buffer failed to send buffer to video port with error: %d\n", res);
            }

        }
    }
}

static void h264_input_buffer_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
    app_state_t *app = (app_state_t *) port->userdata;
    //    DEBUG("h264_input_buffer_callback: %s", __func__);
    mmal_buffer_header_release(buffer);
}

static void h264_output_buffer_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
    int res;
    MMAL_BUFFER_HEADER_T *new_buffer;
    app_state_t *app = (app_state_t *) port->userdata;
    MMAL_POOL_T *pool = app.mmal.h264_output_pool;

    res = pthread_mutex_lock(&app.mmal.h264_mutex);
    if (res)
        fprintf(stderr, "ERROR: pthread_mutex_lock failed to lock h264 buffer with code %d\n", res);

    mmal_buffer_header_mem_lock(buffer);
    memcpy(app.mmal.h264_buffer, buffer->data, buffer->length);
    app.mmal.h264_buffer_length = buffer->length;
    mmal_buffer_header_mem_unlock(buffer);

    if (app.output_type == OUTPUT_STREAM) {
        fwrite(app.mmal.h264_buffer, 1, app.mmal.h264_buffer_length, stdout);
    }

    res = pthread_mutex_unlock(&app.mmal.h264_mutex);
    if (res)
        fprintf(stderr, "ERROR: pthread_mutex_unlock failed to unlock h264 buffer with code %d\n", res);

    res = sem_post(&app.mmal.h264_semaphore);
    if (res) {
        fprintf(stderr, "ERROR: sem_post failed to increase h264 buffer semaphore\n");
    }

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

static void fill_port_buffer(MMAL_PORT_T *port, MMAL_POOL_T *pool)
{
    int num = mmal_queue_length(pool->queue);

    for (int q = 0; q < num; q++) {
        MMAL_BUFFER_HEADER_T *buffer = mmal_queue_get(pool->queue);
        if (!buffer) {
            fprintf(stderr, "ERROR: Unable to get a required buffer %d from pool queue\n", q);
        } else {
            int res = mmal_port_send_buffer(port, buffer);
            if (res) {
                fprintf(stderr, "ERROR: mmal_port_send_buffer failed to send buffer to port with error: %d\n", res);
            }
        }
    }
}

int camera_encode_buffer(char *buffer, int length)
{
    MMAL_BUFFER_HEADER_T *output_buffer = mmal_queue_get(app.mmal.h264_input_pool->queue);
    int res = -1;

    if (output_buffer) {
        memcpy(output_buffer->data, buffer, length);
        output_buffer->length = length;

        res = mmal_port_send_buffer(app.mmal.h264_input_port, output_buffer);
        if (res) {
            fprintf(stderr, "ERROR: mmal_port_send_buffer failed to send buffer to encoder with error(%d, %s)\n", res, get_mmal_message(res));
        }
    }
    else {
        fprintf(stderr, "ERROR: h264 queue returns empty buffer\n");
    }
    return res;
}

int camera_open()
{
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
        app.mmal.camera_num
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
    app.mmal.camera = camera;
    app.mmal.video_port = video_port = camera->output[MMAL_CAMERA_VIDEO_PORT];
    // -----

    // ----- set up the camera configuration
    {
        MMAL_PARAMETER_CAMERA_CONFIG_T cam_config = {
            { MMAL_PARAMETER_CAMERA_CONFIG, sizeof(cam_config) },
            .max_stills_w = app.worker_width,
            .max_stills_h = app.worker_height,
            .stills_yuv422 = 0,
            .one_shot_stills = 0,
            .max_preview_video_w = app.width,
            .max_preview_video_h = app.height,
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
    format->es->video.width = app.width;
    format->es->video.height = app.height;
    format->es->video.crop.x = 0;
    format->es->video.crop.y = 0;
    format->es->video.crop.width = app.width;
    format->es->video.crop.height = app.height;
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
    video_port->userdata = (struct MMAL_PORT_USERDATA_T *)app;

    //DEBUG("camera video buffer_size = %d", video_port->buffer_size);
    //DEBUG("camera video buffer_num = %d", video_port->buffer_num);

    app.mmal.video_port_pool = video_port_pool = (MMAL_POOL_T *) mmal_port_pool_create(video_port,
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

    DEBUG("Camera has been created");

    return 0;

error:
    if (camera)
        mmal_component_destroy(camera);

    return 1;
}

//  Destroy the camera component
void camera_cleanup()
{
    if (app.mmal.camera) {
        mmal_component_destroy(app.mmal.camera);
        app.mmal.camera = NULL;
    }
}

int camera_create_h264_encoder()
{
    int res;
    MMAL_STATUS_T status;
    MMAL_COMPONENT_T *encoder = 0;

    MMAL_PORT_T *input_port = NULL, *output_port = NULL;
    MMAL_POOL_T *input_port_pool = NULL, *output_port_pool = NULL;

    app.mmal.h264_buffer = malloc((app.width * app.height) << 1);
    if (app.mmal.h264_buffer == NULL) {
        fprintf(stderr, "ERROR: malloc failed to allocate memory for h264 buffer.\n");
        goto error;
    }

    res = pthread_mutex_init(&app.mmal.h264_mutex, NULL);
    if (res) {
        fprintf(stderr, "ERROR: pthread_mutex_init failed to init h264 buffer mutex with code: %d\n", res);
        goto error;
    } else {
        app.mmal.is_h264_mutex = 1;
    }

    res = sem_init(&app.mmal.h264_semaphore, 0, 0);
    if (res) {
	    fprintf(stderr, "ERROR: Failed to create h264 semaphore, return code: %d\n", res);
        goto error;
    } else {
        app.mmal.is_h264_semaphore = 1;
    }

    status = mmal_component_create(MMAL_COMPONENT_DEFAULT_VIDEO_ENCODER, &encoder);
    if (status != MMAL_SUCCESS) {
        fprintf(stderr, "ERROR: unable to create encoder (%x)\n", status);
        goto error;
    }

    app.mmal.h264_input_port = input_port = encoder->input[0];
    app.mmal.h264_output_port = output_port = encoder->output[0];

    mmal_format_copy(input_port->format, app.mmal.video_port->format);
    //input_port->format->encoding = MMAL_ENCODING_RGB24;
    //input_port->format->encoding_variant = MMAL_ENCODING_RGB24;
    // input_port->format->es->video.width = app.worker_width;
    // input_port->format->es->video.height = app.worker_height;
    // input_port->format->es->video.crop.x = 0;
    // input_port->format->es->video.crop.y = 0;
    // input_port->format->es->video.crop.width = app.worker_width;
    // input_port->format->es->video.crop.height = app.worker_height;
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

    //DEBUG("encoder h264 input buffer_size = %d", input_port->buffer_size);
    //DEBUG("encoder h264 input buffer_num = %d", input_port->buffer_num);
    //DEBUG("encoder h264 output buffer_size = %d", output_port->buffer_size);
    //DEBUG("encoder h264 output buffer_num = %d", output_port->buffer_num);

    app.mmal.h264_input_pool = input_port_pool = (MMAL_POOL_T *) mmal_port_pool_create(input_port,
        input_port->buffer_num,
        input_port->buffer_size);
    input_port->userdata = (struct MMAL_PORT_USERDATA_T *) app;

    status = mmal_port_enable(input_port, h264_input_buffer_callback);
    if (status != MMAL_SUCCESS) {
        fprintf(stderr, "ERROR: unable to enable encoder input port (%x)\n", status);
        goto error;
    }

    app.mmal.h264_output_pool = output_port_pool = (MMAL_POOL_T *) mmal_port_pool_create(output_port,
        output_port->buffer_num,
        output_port->buffer_size);
    output_port->userdata = (struct MMAL_PORT_USERDATA_T *) app;

    status = mmal_port_enable(output_port, h264_output_buffer_callback);
    if (status != MMAL_SUCCESS) {
        fprintf(stderr, "ERROR: unable to enable encoder h264 output port (%x)\n", status);
        goto error;
    }

    fill_port_buffer(output_port, output_port_pool);
    app.mmal.encoder_h264 = encoder;

    DEBUG("Encoder h264 has been created");
    return 0;

error:
    camera_destroy_h264_encoder();
    return 1;
}

void camera_cleanup_h264_encoder()
{
    int res = 0;

    if (app.mmal.encoder_h264) {
        res = mmal_component_destroy(app.mmal.encoder_h264);
        if (res) {
            fprintf(stderr, "ERROR: mmal_component_destroy failed to destroy h264 encoder %d\n", res);
        }
        app.mmal.encoder_h264 = NULL;
    }

    if (app.mmal.h264_buffer != NULL) {
        free(app.mmal.h264_buffer);
        app.mmal.h264_buffer = NULL;
    }

    if (app.mmal.is_h264_mutex) {
        res = pthread_mutex_destroy(&app.mmal.h264_mutex);
        if (res) {
            fprintf(stderr, "ERROR: pthread_mutex_destroy failed to destroy h264 buffer mutex with code %d\n", res);
        }
        app.mmal.is_h264_mutex = 0;
    }

    if (app.mmal.is_h264_semaphore) {
        res = sem_destroy(&app.mmal.h264_semaphore);
        if (res) {
            fprintf(stderr, "ERROR: sem_destroy failed to destroy h264 semaphore mutex with code %d\n", res);
        }
        app.mmal.is_h264_semaphore = 0;
    }
}
