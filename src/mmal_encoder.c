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

#include "mmal_encoder.h"

static char* mmal_errors[16] = {
    "MMAL_SUCCESS",
    "MMAL_ENOMEM",
    "MMAL_ENOSPC",
    "MMAL_EINVAL",
    "MMAL_ENOSYS",
    "MMAL_ENOENT",
    "MMAL_ENXIO",
    "MMAL_EIO",
    "MMAL_ESPIPE",
    "MMAL_ECORRUPT",
    "MMAL_ENOTREADY",
    "MMAL_ECONFIG",
    "MMAL_EISCONN",
    "MMAL_ENOTCONN",
    "MMAL_EAGAIN",
    "MMAL_EFAULT"
};

static struct format_mapping_t mmal_input_formats[] = {
    {
        .format = VIDEO_FORMAT_YUV422,
        .internal_format = MMAL_ENCODING_I422,
        .is_supported = 1
    }
};
static struct format_mapping_t mmal_output_formats[] = {
    {
        .format = VIDEO_FORMAT_H264,
        .internal_format = MMAL_ENCODING_H264,
        .is_supported = 1
    }
};

static struct format_mapping_t *mmal_input_format = NULL;
static struct format_mapping_t *mmal_output_format = NULL;
static struct mmal_encoder_state_t mmal = {
    .encoder = NULL,
    .out_buf = NULL,
    .mmal_buf = NULL,
    .mmal_buf_used = 0,
    .input_port = NULL,
    .input_pool = NULL,
    .output_port = NULL,
    .output_pool = NULL,
    .is_mutex = 0
};

extern struct app_state_t app;
extern struct filter_t filters[MAX_FILTERS];

static void input_buffer_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
    mmal_buffer_header_release(buffer);
}

static void output_buffer_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
    int res = pthread_mutex_lock(&mmal.mutex);
    if (res) {
        CALL_CUSTOM_MESSAGE(pthread_mutex_lock(&mmal.mutex), res);
        goto error;
    }
    MMAL_CALL(mmal_buffer_header_mem_lock(buffer), buffer_unlock);

    ASSERT_INT(mmal.mmal_buf_used + buffer->length,
        <,
        MMAL_OUT_BUFFER_SIZE,
        mmal_unlock);
    
    // DEBUG("copy buffer, source: %p, dest: %p, length: %d",
    //     buffer->data, mmal.out_buf, buffer->length);
    memcpy(mmal.mmal_buf + mmal.mmal_buf_used, buffer->data, buffer->length);
    mmal.mmal_buf_used += buffer->length;

    mmal_buffer_header_mem_unlock(buffer);

    res = pthread_mutex_unlock(&mmal.mutex);
    if (res) {
        CALL_CUSTOM_MESSAGE(pthread_mutex_unlock(&mmal.mutex), res);
        goto error;
    }

    mmal_buffer_header_release(buffer);

    if (mmal.output_pool && port->is_enabled) {
        MMAL_QUEUE_T *queue = mmal.output_pool->queue;

        MMAL_BUFFER_HEADER_T *buffer = mmal_queue_get(queue);
        if (!buffer) {
            MMAL_MESSAGE(mmal_queue_get(queue), MMAL_EAGAIN);
            goto error;
        }
        MMAL_CALL(mmal_port_send_buffer(port, buffer), error);
    }

    // unlock encoder to get new buffer;
    CALL(sem_post(&mmal.semaphore), error);
    return;

mmal_unlock:
    mmal_buffer_header_mem_unlock(buffer);

buffer_unlock:
    res = pthread_mutex_unlock(&mmal.mutex);
    if (res) {
        CALL_CUSTOM_MESSAGE(pthread_mutex_unlock(&mmal.mutex), res);
    }
error:
    return;
}

static int fill_port_buffer(MMAL_PORT_T *port, MMAL_POOL_T *pool)
{
    int num = mmal_queue_length(pool->queue);
    for (int q = 0; q < num; q++) {
        MMAL_BUFFER_HEADER_T *buffer = mmal_queue_get(pool->queue);
        if (!buffer) {
            MMAL_MESSAGE(mmal_queue_get(pool->queue), MMAL_EAGAIN);
            goto cleanup;
        }
        MMAL_CALL(mmal_port_send_buffer(port, buffer), cleanup);
    }
    return 0;

cleanup:
    if (errno == 0)
        errno = EAGAIN;
    return -1;
}

static int mmal_stop()
{
    if (mmal_input_format) {
        MMAL_CALL(mmal_port_disable(mmal.input_port), cleanup);
        mmal_input_format = NULL;
        mmal.input_port = NULL;
        mmal.input_pool = NULL;
    }

    if (mmal_output_format) {
        MMAL_CALL(mmal_port_disable(mmal.output_port), cleanup);
        mmal_output_format = NULL;
        mmal.output_port = NULL;
        mmal.output_pool = NULL;
    }

    if (mmal.mmal_buf) {
        free(mmal.mmal_buf);
        mmal.mmal_buf = NULL;
    }

    if (mmal.out_buf) {
        free(mmal.out_buf);
        mmal.out_buf = NULL;
    }

    return 0;

cleanup:
    if (errno == 0)
        errno = EAGAIN;
    return -1;
}

void mmal_cleanup()
{
    mmal_stop();

    if (mmal.encoder) {
        MMAL_CALL(mmal_component_destroy(mmal.encoder));
        mmal.encoder = NULL;
    }

    if (mmal.is_mutex) {
        int res = pthread_mutex_destroy(&mmal.mutex);
        if (res) {
            errno = res;
            CALL_MESSAGE(pthread_mutex_destroy(&mmal.mutex));
        }
        mmal.is_mutex = 0;
    }

    if (mmal.is_semaphore) {
        CALL(sem_destroy(&mmal.semaphore));
        mmal.is_semaphore = 0;
    }
}

static int mmal_start(int input_format, int output_format)
{
    ASSERT_PTR(mmal.mmal_buf, !=, NULL, cleanup);
    ASSERT_PTR(mmal.out_buf, !=, NULL, cleanup)
    ASSERT_PTR(mmal.encoder, !=, NULL, cleanup);
    ASSERT_INT(mmal.encoder->output[0]->buffer_num, >, 0, cleanup);

    ASSERT_PTR(mmal_input_format, ==, NULL, cleanup);
    int formats_len = ARRAY_SIZE(mmal_input_formats);
    for (int i = 0; i < formats_len; i++) {
        struct format_mapping_t *f = mmal_input_formats + i;
        if (f->format == input_format && f->is_supported) {
            mmal_input_format = f;
            break;
        }
    }
    ASSERT_PTR(mmal_input_format, !=, NULL, cleanup);

    ASSERT_PTR(mmal_output_format, ==, NULL, cleanup);
    formats_len = ARRAY_SIZE(mmal_output_formats);
    for (int i = 0; i < formats_len; i++) {
        struct format_mapping_t *f = mmal_output_formats + i;
        if (f->format == output_format && f->is_supported) {
            mmal_output_format = f;
            break;
        }
    }
    ASSERT_PTR(mmal_output_format, !=, NULL, cleanup);

    mmal.input_port = mmal.encoder->input[0];
    mmal.input_port->format->encoding = mmal_input_format->internal_format;
    mmal.input_port->format->es->video.width = app.video_width;
    mmal.input_port->format->es->video.height = app.video_height;
    mmal.input_port->format->es->video.crop.x = 0;
    mmal.input_port->format->es->video.crop.y = 0;
    mmal.input_port->format->es->video.crop.width = app.video_width;
    mmal.input_port->format->es->video.crop.height = app.video_height;
    mmal.input_port->buffer_size = mmal.input_port->buffer_size_recommended;
    mmal.input_port->buffer_num = mmal.input_port->buffer_num_recommended;
    MMAL_CALL(mmal_port_format_commit(mmal.input_port), cleanup);

    mmal.output_port = mmal.encoder->output[0];
    mmal.output_port->format->encoding =  mmal_output_format->internal_format;
    mmal.output_port->format->es->video.width = app.video_width;
    mmal.output_port->format->es->video.height = app.video_height;
    mmal.output_port->format->es->video.crop.x = 0;
    mmal.output_port->format->es->video.crop.y = 0;
    mmal.output_port->format->es->video.crop.width = app.video_width;
    mmal.output_port->format->es->video.crop.height = app.video_height;
    mmal.output_port->buffer_size = mmal.output_port->buffer_size_recommended;
    mmal.output_port->buffer_num = mmal.output_port->buffer_num_recommended;
    MMAL_CALL(mmal_port_format_commit(mmal.output_port), cleanup);

    ASSERT_INT(mmal.output_port->buffer_size, <=, MMAL_OUT_BUFFER_SIZE, cleanup);
    DEBUG("mmal.input_port->buffer_num: %d", mmal.input_port->buffer_num);

    mmal.input_pool = mmal_port_pool_create(mmal.input_port,
        mmal.input_port->buffer_num,
        mmal.input_port->buffer_size);
    if (!mmal.input_pool) {
        MMAL_MESSAGE(mmal_port_pool_create(mmal.input_port, ...), MMAL_EAGAIN);
        goto cleanup;
    }

    mmal.output_pool = mmal_port_pool_create(mmal.output_port,
        mmal.output_port->buffer_num,
        mmal.output_port->buffer_size);
    if (!mmal.output_port) {
        MMAL_MESSAGE(mmal_port_pool_create(mmal.output_port, ...), MMAL_EAGAIN);
        goto cleanup;
    }

    MMAL_CALL(mmal_port_enable(mmal.input_port, input_buffer_callback), cleanup);
    MMAL_CALL(mmal_port_enable(mmal.output_port, output_buffer_callback), cleanup);

    CALL(fill_port_buffer(mmal.output_port, mmal.output_pool), cleanup);

    DEBUG("h264 mmal encoder has been started");
    return 0;

cleanup:
    mmal_input_format = NULL;
    mmal_output_format = NULL;

    if (errno == 0)
        errno = EAGAIN;
    return -1;
}

static int mmal_init()
{
    MMAL_CALL(mmal_component_create(MMAL_COMPONENT_DEFAULT_VIDEO_ENCODER, &mmal.encoder), cleanup);

    ASSERT_INT(mmal.is_mutex, ==, 0, cleanup)
    int res = pthread_mutex_init(&mmal.mutex, NULL);
    if (res) {
        CALL_CUSTOM_MESSAGE(pthread_mutex_init(&mmal.mutex), res);
        goto cleanup;
    }
    mmal.is_mutex = 1;

    ASSERT_INT(mmal.is_semaphore, ==, 0, cleanup)
    CALL(sem_init(&mmal.semaphore, 0, 0), cleanup);
    mmal.is_semaphore = 1;

    ASSERT_PTR(mmal.mmal_buf, ==, NULL, cleanup)
    mmal.mmal_buf = malloc(MMAL_OUT_BUFFER_SIZE);
    if (!mmal.mmal_buf) {
        CALL_MESSAGE(malloc(MMAL_OUT_BUFFER_SIZE));
        goto cleanup;
    }

    ASSERT_PTR(mmal.out_buf, ==, NULL, cleanup)
    mmal.out_buf = malloc(MMAL_OUT_BUFFER_SIZE);
    if (!mmal.out_buf) {
        CALL_MESSAGE(malloc(MMAL_OUT_BUFFER_SIZE));
        goto cleanup;
    }
    return 0;

cleanup:
    mmal_cleanup();
    if (errno == 0)
        errno = EAGAIN;
    return -1;
}

static int mmal_is_started()
{
    return mmal_input_format != NULL && mmal_output_format != NULL? 1: 0;
}

static int mmal_process_frame(uint8_t *buffer)
{
    ASSERT_PTR(mmal_input_format, !=, NULL, cleanup);
    ASSERT_PTR(mmal_output_format, !=, NULL, cleanup);

    MMAL_BUFFER_HEADER_T *mmal_buffer = mmal_queue_get(mmal.input_pool->queue);
    if (!mmal_buffer) {
        MMAL_MESSAGE(mmal_queue_get(pool->queue), MMAL_EAGAIN);
        goto cleanup;
    }

    mmal_buffer->length = app.video_width * app.video_height * 2;
    //DEBUG("copy buffer, data: %p, length: %d", mmal_buffer->data, mmal_buffer->length);
    memcpy(mmal_buffer->data, buffer, mmal_buffer->length);

    MMAL_CALL(mmal_port_send_buffer(mmal.input_port, mmal_buffer), cleanup);

    // wait when encoder encode buffer to process next one
    int value = 0;
    CALL(sem_getvalue(&mmal.semaphore, &value), cleanup);
    while (value > 0) {
        CALL(sem_wait(&mmal.semaphore), cleanup);
        CALL(sem_getvalue(&mmal.semaphore, &value), cleanup);
    }
    CALL(sem_wait(&mmal.semaphore), cleanup);
    return 0;

cleanup:
    if (errno == 0)
        errno = EAGAIN;
    return -1;
}

static uint8_t *mmal_get_buffer(int *out_format, int *length)
{
    ASSERT_PTR(mmal_input_format, !=, NULL, cleanup);
    ASSERT_PTR(mmal_output_format, !=, NULL, cleanup);

    if (out_format)
        *out_format = mmal_output_format->format;

    int res = pthread_mutex_lock(&mmal.mutex);
    if (res) {
        CALL_CUSTOM_MESSAGE(pthread_mutex_lock(&mmal.mutex), res);
        goto cleanup;
    }

    if (length)
        *length = mmal.mmal_buf_used;
    
    memcpy(mmal.out_buf, mmal.mmal_buf, mmal.mmal_buf_used);
    mmal.mmal_buf_used = 0;

    res = pthread_mutex_unlock(&mmal.mutex);
    if (res) {
        CALL_CUSTOM_MESSAGE(pthread_mutex_unlock(&mmal.mutex), res);
        goto cleanup;
    }

    return mmal.out_buf;

cleanup:
    if (errno == 0)
        errno = EOPNOTSUPP;
    return (uint8_t *)-1;
}

/*int mmal_get_capabilities(int camera_num, char *camera_name, int *width, int *height )
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
}*/

static int mmal_get_in_formats(const struct format_mapping_t *formats[])
{
    if (mmal_input_formats != NULL)
        *formats = mmal_input_formats;
    return ARRAY_SIZE(mmal_input_formats);
}

static int mmal_get_out_formats(const struct format_mapping_t *formats[])
{
    if (mmal_output_formats != NULL)
        *formats = mmal_output_formats;
    return ARRAY_SIZE(mmal_output_formats);
}

void mmal_encoder_construct()
{
    int i = 0;
    while (i < MAX_FILTERS && filters[i].context != NULL)
        i++;

    if (i != MAX_FILTERS) {
        filters[i].name = "mmal_encoder";
        filters[i].context = &mmal;
        filters[i].stop = mmal_stop;
        filters[i].cleanup = mmal_cleanup;
        filters[i].init = mmal_init;
        filters[i].start = mmal_start;
        filters[i].is_started = mmal_is_started;
        filters[i].process_frame = mmal_process_frame;

        filters[i].get_buffer = mmal_get_buffer;
        filters[i].get_in_formats = mmal_get_in_formats;
        filters[i].get_out_formats = mmal_get_out_formats;
    }
}
