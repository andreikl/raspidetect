/*
 * Copyright (c) 2019, NVIDIA CORPORATION. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of NVIDIA CORPORATION nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * Execution command
 * ./encode_sample raw_file.yuv (int)width (int)height encoded_file.264
 * Eg: ./encode_sample test_h264_raw.yuv 1920 1080 encoded_h264.264
 **/

#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <stdint.h>
#include <unistd.h>
#include <cstdlib>
#include <libv4l2.h>
#include <linux/videodev2.h>
#include <malloc.h>
#include <pthread.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <assert.h>

#include "nvbuf_utils.h"
#include "v4l2_nv_extensions.h"

using namespace std;

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

#define GET_3RD_ARG(arg1, arg2, arg3, ...) arg3

#define ASSERT_INT(value, condition, expectation, error) \
{ \
    if (value condition expectation) { \
        fprintf(stderr, "ERROR: assert "#value"(%d) "#condition" "#expectation"(%d)\n%s:%d - %s\n", \
            value, expectation, __FILE__, __LINE__, __FUNCTION__); \
        goto error; \
    } \
}

#define ASSERT_LNG(value, condition, expectation, error) \
{ \
    if (value condition expectation) { \
        fprintf(stderr, "ERROR: assert "#value"(%ld) "#condition" "#expectation"(%ld)\n%s:%d - %s\n", \
            value, (long int)expectation, __FILE__, __LINE__, __FUNCTION__); \
        goto error; \
    } \
}

#define ASSERT_PTR(value, condition, expectation, error) \
{ \
    if (value condition expectation) { \
        fprintf(stderr, "ERROR: assert "#value"(%p) "#condition" "#expectation"(%p)\n%s:%d - %s\n", \
            value, expectation, __FILE__, __LINE__, __FUNCTION__); \
        goto error; \
    } \
}

#define DEBUG(format, ...) \
{ \
    fprintf(stderr, "%s:%s, "#format"\n", __FILE__, __FUNCTION__, ##__VA_ARGS__); \
}

#define CALL_MESSAGE(call) \
{ \
    fprintf(stderr, "ERROR: "#call" returned error: %s (%d)\n%s:%d - %s\n", \
        strerror(errno), errno, __FILE__, __LINE__, __FUNCTION__); \
}

#define CALL_CUSTOM_MESSAGE(call, res) \
{ \
    fprintf(stderr, "ERROR: "#call" returned error: (%d)\n%s:%d - %s\n", \
        res, __FILE__, __LINE__, __FUNCTION__); \
}

#define CALL_2(call, error) \
{ \
    int __res = call; \
    if (__res == -1) { \
        CALL_MESSAGE(call); \
        goto error; \
    } \
}

#define CALL_1(call) \
{ \
    int __res = call; \
    if (__res == -1) { \
        CALL_MESSAGE(call); \
    } \
}

#define CALL_X(...) GET_3RD_ARG(__VA_ARGS__, CALL_2, CALL_1, )

#define CALL(...) CALL_X(__VA_ARGS__)(__VA_ARGS__)

#define V4L_CALL_2(call, error) \
{ \
    int __res = call; \
    if (__res != 0) { \
        CALL_MESSAGE(call); \
        goto error; \
    } \
}

#define V4L_CALL_1(call) \
{ \
    int __res = call; \
    if (__res != 0) { \
        CALL_MESSAGE(call); \
    } \
}

#define V4L_CALL_X(...) GET_3RD_ARG(__VA_ARGS__, V4L_CALL_2, V4L_CALL_1, )

#define V4L_CALL(...) V4L_CALL_X(__VA_ARGS__)(__VA_ARGS__)

#include "encoder_unit_sample.hpp"

/**
 *
 * V4L2 H264 Video Encoder Sample
 *
 * The video encoder device node is
 *     /dev/nvhost-msenc
 *
 * In this sample:
 * ## Pixel Formats
 * OUTPUT PLANE         | CAPTURE PLANE
 * :----------------:   | :----------------:
 * V4L2_PIX_FMT_YUV420M | V4L2_PIX_FMT_H264
 * V4L2_PIX_FMT_YUV444M
 *
 * ## Memory Type
 *            | OUTPUT PLANE        | CAPTURE PLANE
 * :--------: | :----------:        | :-----------:
 * MEMORY     | V4L2_MEMORY_MMAP    | V4L2_MEMORY_MMAP
 *
 * ## Supported Controls
 * - #V4L2_CID_MPEG_VIDEO_DISABLE_COMPLETE_FRAME_INPUT
 * - V4L2_CID_MIN_BUFFERS_FOR_CAPTURE (Get the minimum buffers to be allocated
 * on capture plane.
 * Read-only. Valid after #V4L2_EVENT_RESOLUTION_CHANGE)
 *
 * ## Supported Events
 * Event                         | Purpose
 * ----------------------------- | :----------------------------:
 * #V4L2_EVENT_EOS               | End of Stream detected.
 *
 * ## Opening the Encoder
 * The encoder device node is opened through the v4l2_open IOCTL call.
 * After opening the device, the application calls VIDIOC_QUERYCAP to identify
 * the driver capabilities.
 *
 * ## Subscribing events and setting up the planes
 * The application subscribes to the V4L2_EVENT_EOS event,
 * to detect the end of stream and handle the plane buffers
 * accordingly.
 * It calls VIDIOC_S_FMT to setup the formats required on
 * OUTPUT PLANE and CAPTURE PLANE for the data
 * negotiation between the former and the driver.
 * It is necessary to set capture plane format before the output plane format
 * along with the frame width and height
 *
 * ## Setting Controls
 * The application gets/sets the properties of the encoder by setting
 * the controls, calling VIDIOC_S_EXT_CTRLS, VIDIOC_G_CTRL.
 *
 * ## Buffer Management
 * Buffers are requested on the OUTPUT PLANE by the application, calling
 * VIDIOC_REQBUFS. The actual buffers allocated by the encoder are then
 * queried and exported as FD for the DMA-mapped buffer while mapped
 * for Mmaped buffer.
 * Status STREAMON is called on both planes to signal the encoder for
 * processing.
 *
 * Application continuously queues the raw data in the allocated
 * OUTPUT PLANE buffer and dequeues the next empty buffer fed into the
 * encoder.
 * The encoder encodes the raw buffer and signals a successful dequeue
 * on the capture plane, from where the data of v4l2_buffer dequeued is
 * dumped as an encoded bitstream.
 *
 * The encoding thread blocks on the DQ buffer call, which returns either after
 * a successful encoded bitstream or after a specific timeout.
 *
 * ## EOS Handling
 * For sending EOS and receiving EOS from the encoder, the application must
 * - Send EOS to the encoder by queueing on the output plane a buffer with
 * bytesused = 0 for the 0th plane (`v4l2_buffer.m.planes[0].bytesused = 0`).
 * - Dequeues buffers on the output plane until it gets a buffer with bytesused = 0
 * for the 0th plane (`v4l2_buffer.m.planes[0].bytesused == 0`)
 * - Dequeues buffers on the capture plane until it gets a buffer with bytesused = 0
 * for the 0th plane.
 * After the last buffer on the capture plane is dequeued, set STREAMOFF on both
 * planes and destroy the allocated buffers.
 *
 */

#define CHECK_ERROR(condition, error_str, label) if (condition) { \
                                                        cerr << error_str << endl; \
                                                        v4l.in_error = 1; \
                                                        goto label; }

Buffer::Buffer(enum v4l2_buf_type buf_type, enum v4l2_memory memory_type,
        uint32_t index)
        :buf_type(buf_type),
         memory_type(memory_type),
         index(index)
{
    uint32_t i;

    memset(planes, 0, sizeof(planes));

    mapped = false;
    n_planes = 1;
    for (i = 0; i < n_planes; i++)
    {
        this->planes[i].fd = -1;
        this->planes[i].data = NULL;
        this->planes[i].bytesused = 0;
        this->planes[i].mem_offset = 0;
        this->planes[i].length = 0;
        this->planes[i].fmt.sizeimage = 0;
    }
}

Buffer::Buffer(enum v4l2_buf_type buf_type, enum v4l2_memory memory_type,
        uint32_t n_planes, BufferPlaneFormat * fmt, uint32_t index)
        :buf_type(buf_type),
         memory_type(memory_type),
         index(index),
         n_planes(n_planes)
{
    uint32_t i;

    mapped = false;

    memset(planes, 0, sizeof(planes));
    for (i = 0; i < n_planes; i++)
    {
        this->planes[i].fd = -1;
        this->planes[i].fmt = fmt[i];
    }
}

Buffer::~Buffer()
{
    if (mapped)
    {
        unmap();
    }
}

int
Buffer::map()
{
    uint32_t j;

    if (memory_type != V4L2_MEMORY_MMAP)
    {
        cout << "Buffer " << index << "already mapped" << endl;
        return -1;
    }

    if (mapped)
    {
        cout << "Buffer " << index << "already mapped" << endl;
        return 0;
    }

    for (j = 0; j < n_planes; j++)
    {
        if (planes[j].fd == -1)
        {
            return -1;
        }

        planes[j].data = (unsigned char *) mmap(NULL,
                                                planes[j].length,
                                                PROT_READ | PROT_WRITE,
                                                MAP_SHARED,
                                                planes[j].fd,
                                                planes[j].mem_offset);
        if (planes[j].data == MAP_FAILED)
        {
            cout << "Could not map buffer " << index << ", plane " << j << endl;
            return -1;
        }

    }
    mapped = true;
    return 0;
}

void
Buffer::unmap()
{
    if (memory_type != V4L2_MEMORY_MMAP || !mapped)
    {
        cout << "Cannot Unmap Buffer " << index <<
                ". Only mapped MMAP buffer can be unmapped" << endl;
        return;
    }

    for (uint32_t j = 0; j < n_planes; j++)
    {
        if (planes[j].data)
        {
            munmap(planes[j].data, planes[j].length);
        }
        planes[j].data = NULL;
    }
    mapped = false;
}

int
Buffer::fill_buffer_plane_format(uint32_t *num_planes,
        Buffer::BufferPlaneFormat *planefmts,
        uint32_t width, uint32_t height, uint32_t raw_pixfmt)
{
    switch (raw_pixfmt)
    {
        case V4L2_PIX_FMT_YUV444M:
            *num_planes = 3;

            planefmts[0].width = width;
            planefmts[1].width = width;
            planefmts[2].width = width;

            planefmts[0].height = height;
            planefmts[1].height = height;
            planefmts[2].height = height;

            planefmts[0].bytesperpixel = 1;
            planefmts[1].bytesperpixel = 1;
            planefmts[2].bytesperpixel = 1;
            break;

        case V4L2_PIX_FMT_YUV420M:
            *num_planes = 3;

            planefmts[0].width = width;
            planefmts[1].width = width / 2;
            planefmts[2].width = width / 2;

            planefmts[0].height = height;
            planefmts[1].height = height / 2;
            planefmts[2].height = height / 2;

            planefmts[0].bytesperpixel = 1;
            planefmts[1].bytesperpixel = 1;
            planefmts[2].bytesperpixel = 1;
            break;

        case V4L2_PIX_FMT_NV12M:
            *num_planes = 2;

            planefmts[0].width = width;
            planefmts[1].width = width / 2;

            planefmts[0].height = height;
            planefmts[1].height = height / 2;

            planefmts[0].bytesperpixel = 1;
            planefmts[1].bytesperpixel = 2;
            break;
        default:
            cout << "Unsupported pixel format " << raw_pixfmt << endl;
            return -1;
    }
    return 0;
}

static int
//read_video_frame(ifstream * stream, Buffer & buffer)
read_video_frame(Buffer & buffer)
{
    uint32_t i, j;
    char *data;

    for (i = 0; i < buffer.n_planes; i++)
    {
        Buffer::BufferPlane &plane = buffer.planes[i];
        streamsize bytes_to_read =
            plane.fmt.bytesperpixel * plane.fmt.width;
        data = (char *) plane.data;
        plane.bytesused = 0;
        /* It is necessary to set bytesused properly,
        ** so that encoder knows how
        ** many bytes in the buffer to be read.
        */
        for (j = 0; j < plane.fmt.height; j++)
        {
            /*stream->read(data, bytes_to_read);
            if (stream->gcount() < bytes_to_read)
                return -1;*/
            data += plane.fmt.stride;
        }
        plane.bytesused = plane.fmt.stride * plane.fmt.height;
    }
    return 0;
}

static int
write_encoded_frame(ofstream * stream, Buffer * buffer)
{
    stream->write((char *)buffer->planes[0].data, buffer->planes[0].bytesused);
    return 0;
}

static int
wait_for_dqthread(context_t& v4l, uint32_t max_wait_ms)
{
    struct timespec waiting_time;
    struct timeval now;
    int ret_val = 0;
    int dq_return = 0;

    gettimeofday(&now, NULL);

    waiting_time.tv_nsec = (now.tv_usec + (max_wait_ms % 1000) * 1000L) * 1000L;
    waiting_time.tv_sec = now.tv_sec + max_wait_ms / 1000 +
        waiting_time.tv_nsec / 1000000000L;
    waiting_time.tv_nsec = waiting_time.tv_nsec % 1000000000L;

    pthread_mutex_lock(&v4l.queue_lock);
    while (v4l.dqthread_running)
    {
        dq_return = pthread_cond_timedwait(&v4l.queue_cond, &v4l.queue_lock,
            &waiting_time);
        if (dq_return == ETIMEDOUT)
        {
            ret_val = -1;
            break;
        }
    }

    pthread_mutex_unlock(&v4l.queue_lock);

    if (dq_return == 0)
    {
        pthread_join(v4l.enc_dq_thread, NULL);
        v4l.enc_dq_thread = 0;
    }
    else
    {
        cerr << "Time out waiting for dqthread" << endl;
        v4l.in_error = 1;
    }
    return ret_val;
}

static int
req_buffers_on_capture_plane(context_t * v4l, enum v4l2_buf_type buf_type,
        enum v4l2_memory mem_type, int num_buffers)
{
    struct v4l2_requestbuffers reqbuffers;
    int ret_val = 0;
    memset (&reqbuffers, 0, sizeof (struct v4l2_requestbuffers));

    reqbuffers.count = num_buffers;
    reqbuffers.memory = mem_type;
    reqbuffers.type = buf_type;

    ret_val = v4l2_ioctl (v4l->fd, VIDIOC_REQBUFS, &reqbuffers);
    if (ret_val)
        return ret_val;

    if (reqbuffers.count)
    {
        v4l->capplane_buffers = new Buffer *[reqbuffers.count];
        for (uint32_t i = 0; i < reqbuffers.count; ++i)
        {
            v4l->capplane_buffers[i] = new Buffer (buf_type, mem_type,
                v4l->capplane_num_planes, v4l->capplane_planefmts, i);
        }
    }
    else
    {
        for (uint32_t i = 0; i < v4l->capplane_num_buffers; ++i)
        {
            delete v4l->capplane_buffers[i];
        }
        delete[] v4l->capplane_buffers;
        v4l->capplane_buffers = NULL;
    }
    v4l->capplane_num_buffers = reqbuffers.count;

    return ret_val;
}

static int
req_buffers_on_output_plane(context_t * v4l, enum v4l2_buf_type buf_type,
        enum v4l2_memory mem_type, int num_buffers)
{
    struct v4l2_requestbuffers reqbuffers;
    int ret_val = 0;
    memset (&reqbuffers, 0, sizeof (struct v4l2_requestbuffers));

    reqbuffers.count = num_buffers;
    reqbuffers.memory = mem_type;
    reqbuffers.type = buf_type;

    ret_val = v4l2_ioctl (v4l->fd, VIDIOC_REQBUFS, &reqbuffers);
    if (ret_val)
        return ret_val;

    if (reqbuffers.count)
    {
        v4l->outplane_buffers = new Buffer *[reqbuffers.count];
        for (uint32_t i = 0; i < reqbuffers.count; ++i)
        {
            v4l->outplane_buffers[i] = new Buffer (buf_type, mem_type,
                v4l->outplane_num_planes, v4l->outplane_planefmts, i);
        }
    }
    else
    {
        for (uint32_t i = 0; i < v4l->outplane_num_buffers; ++i)
        {
            delete v4l->outplane_buffers[i];
        }
        delete[] v4l->outplane_buffers;
        v4l->outplane_buffers = NULL;
    }
    v4l->outplane_num_buffers = reqbuffers.count;

    return ret_val;
}

/*static int
subscribe_event(int fd, uint32_t type, uint32_t id, uint32_t flags)
{
    struct v4l2_event_subscription sub;
    int ret_val;

    memset(&sub, 0, sizeof (struct v4l2_event_subscription));

    sub.type = type;
    sub.id = id;
    sub.flags = flags;

    ret_val = v4l2_ioctl(fd, VIDIOC_SUBSCRIBE_EVENT, &sub);

    return ret_val;
}*/

static int
q_buffer(context_t * v4l, struct v4l2_buffer &v4l2_buf, Buffer * buffer,
    enum v4l2_buf_type buf_type, enum v4l2_memory memory_type, int num_planes)
{
    int ret_val;
    uint32_t j;

    pthread_mutex_lock (&v4l->queue_lock);
    v4l2_buf.type = buf_type;
    v4l2_buf.memory = memory_type;
    v4l2_buf.length = num_planes;

    switch (memory_type)
    {
        case V4L2_MEMORY_MMAP:
            for (j = 0; j < buffer->n_planes; ++j)
            {
                v4l2_buf.m.planes[j].bytesused =
                buffer->planes[j].bytesused;
            }
            break;
        case V4L2_MEMORY_DMABUF:
            break;
        default:
            pthread_cond_broadcast (&v4l->queue_cond);
            pthread_mutex_unlock (&v4l->queue_lock);
            return -1;
    }

    ret_val = v4l2_ioctl (v4l->fd, VIDIOC_QBUF, &v4l2_buf);

    if (!ret_val)
    {
        switch (v4l2_buf.type)
        {
            case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
                v4l->num_queued_outplane_buffers++;
                break;
            case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
                v4l->num_queued_capplane_buffers++;
                break;
            default:
                cerr << "Buffer Type not supported" << endl;
        }
        pthread_cond_broadcast (&v4l->queue_cond);
    }
    pthread_mutex_unlock (&v4l->queue_lock);

    return ret_val;
}

static int
dq_buffer(context_t * v4l, struct v4l2_buffer &v4l2_buf, Buffer ** buffer,
    enum v4l2_buf_type buf_type, enum v4l2_memory memory_type, uint32_t num_retries)
{
    int ret_val = 0;
    bool is_in_error = false;
    v4l2_buf.type = buf_type;
    v4l2_buf.memory = memory_type;

    do
    {
        ret_val = v4l2_ioctl (v4l->fd, VIDIOC_DQBUF, &v4l2_buf);

        if (ret_val == 0)
        {
            pthread_mutex_lock(&v4l->queue_lock);
            switch(v4l2_buf.type)
            {
                case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
                    if (buffer)
                        *buffer = v4l->outplane_buffers[v4l2_buf.index];
                    for (uint32_t j = 0; j < v4l->outplane_buffers[v4l2_buf.index]->n_planes; j++)
                    {
                        v4l->outplane_buffers[v4l2_buf.index]->planes[j].bytesused =
                        v4l2_buf.m.planes[j].bytesused;
                    }
                    v4l->num_queued_outplane_buffers--;
                    break;

                case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
                    if (buffer)
                        *buffer = v4l->capplane_buffers[v4l2_buf.index];
                    for (uint32_t j = 0; j < v4l->capplane_buffers[v4l2_buf.index]->n_planes; j++)
                    {
                        v4l->capplane_buffers[v4l2_buf.index]->planes[j].bytesused =
                        v4l2_buf.m.planes[j].bytesused;
                    }
                    v4l->num_queued_capplane_buffers--;
                    break;

                default:
                    cout << "Invaild buffer type" << endl;
            }
            pthread_cond_broadcast(&v4l->queue_cond);
            pthread_mutex_unlock(&v4l->queue_lock);
        }
        else if (errno == EAGAIN)
        {
            pthread_mutex_lock(&v4l->queue_lock);
            if (v4l2_buf.flags & V4L2_BUF_FLAG_LAST)
            {
                pthread_mutex_unlock(&v4l->queue_lock);
                break;
            }
            pthread_mutex_unlock(&v4l->queue_lock);

            if (num_retries-- == 0)
            {
                // Resource temporarily unavailable.
                cout << "Resource temporarily unavailable" << endl;
                break;
            }
        }
        else
        {
            is_in_error = true;
            break;
        }
    }
    while (ret_val && !is_in_error);

    return ret_val;
}

static void *
dq_thread(void *arg)
{
    context_t *v4l = (context_t *)arg;
    bool stop_dqthread = false;

    while (!stop_dqthread)
    {
        struct v4l2_buffer v4l2_buf;
        struct v4l2_plane planes[MAX_PLANES];
        Buffer *buffer = new Buffer(v4l->capplane_buf_type,
                v4l->capplane_mem_type, 0);
        bool ret_val;

        memset(&v4l2_buf, 0, sizeof (struct v4l2_buffer));
        memset(planes, 0, MAX_PLANES * sizeof (struct v4l2_plane));
        v4l2_buf.m.planes = planes;
        v4l2_buf.length = v4l->capplane_num_planes;

        if (dq_buffer(v4l, v4l2_buf, &buffer, v4l->capplane_buf_type,
                v4l->capplane_mem_type, -1) < 0)
        {
            if (errno != EAGAIN)
            {
                v4l->in_error = true;
            }

            if (errno != EAGAIN || v4l->capplane_streamon)
                ret_val = capture_plane_callback(NULL, NULL, v4l);

            if (!v4l->capplane_streamon)
                break;
        }
        else
        {
            ret_val = capture_plane_callback(&v4l2_buf, buffer, v4l);
        }
        if (!ret_val)
        {
            break;
        }
    }
    stop_dqthread = true;

    pthread_mutex_lock(&v4l->queue_lock);
    v4l->dqthread_running = false;
    pthread_cond_broadcast(&v4l->queue_cond);
    pthread_mutex_unlock(&v4l->queue_lock);

    return NULL;
}

static bool
capture_plane_callback(struct v4l2_buffer *v4l2_buf, Buffer * buffer, void *arg)
{
    context_t *v4l = (context_t *)arg;

    if (v4l2_buf == NULL)
    {
        cout << "Error while DQing buffer from capture plane" << endl;
        v4l->in_error = 1;
        return false;
    }

    if (buffer->planes[0].bytesused == 0)
    {
        cout << "Got 0 size buffer in capture" << endl;
        return false;
    }

    write_encoded_frame(v4l->output_file, buffer);

    if (q_buffer(v4l, *v4l2_buf, buffer, v4l->capplane_buf_type, v4l->capplane_mem_type,
            v4l->capplane_num_planes) < 0)
    {
        cerr << "Error while Qing buffer at capture plane" <<  endl;
        v4l->in_error = 1;
        return false;
    }

    return true;
}

static int
encoder_process_blocking(context_t& v4l)
{
    int ret_val = 0;
    //struct v4l2_buffer v4l2_buf;
    //struct v4l2_plane planes[MAX_PLANES];


    /* Reading input till EOS is reached.
    ** As all the output plane buffers are queued, a buffer
    ** is dequeued first before new data is read and queued back.
    */
    while (!v4l.in_error && !v4l.eos)
    {
        // Buffer *buffer =  new Buffer(v4l.outplane_buf_type, v4l.outplane_mem_type, 0);
        // memset(&v4l2_buf, 0, sizeof (v4l2_buf));
        // memset(planes, 0, sizeof (planes));
        // v4l2_buf.m.planes = planes;
        // // Dequeue the empty buffer on output plane.
        // ret_val = dq_buffer(&v4l, v4l2_buf, &buffer, v4l.outplane_buf_type,
        //             v4l.outplane_mem_type, 10);
        // if (ret_val < 0)
        // {
        //     cerr << "Error while DQing buffer at output plane" << endl;
        //     v4l.in_error = 1;
        //     break;
        // }

        struct v4l2_buffer in_buf;
        struct v4l2_plane in_planes[3];
        memset(&in_buf, 0, sizeof(in_buf));
        memset(in_planes, 0, sizeof(in_planes));
        in_buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
        in_buf.memory = V4L2_MEMORY_MMAP;
        in_buf.m.planes = in_planes;
        //pthread_mutex_lock(&v4l.queue_lock);
        V4L_CALL(v4l2_ioctl(v4l.fd, VIDIOC_DQBUF, &in_buf));
        Buffer *buffer = v4l.outplane_buffers[in_buf.index];
        for (uint32_t j = 0; j < v4l.outplane_buffers[in_buf.index]->n_planes; j++)
        {
            DEBUG("In plane[%d] - bytesused: %d", j, in_buf.m.planes[j].bytesused);
            v4l.outplane_buffers[in_buf.index]->planes[j].bytesused = 
                in_buf.m.planes[j].bytesused;
        }
        v4l.num_queued_outplane_buffers--;
        //pthread_cond_broadcast(&v4l.queue_cond);
        //pthread_mutex_unlock(&v4l.queue_lock);

        DEBUG("read_video_frame");
        // Read and enqueue the filled buffer.
        ret_val = read_video_frame(*buffer);
        if (ret_val < 0)
        {
            cerr << "Could not read complete frame from input file" << endl;
            v4l.eos = true;
            in_buf.m.planes[0].m.userptr = 0;
            in_buf.m.planes[0].bytesused =
                in_buf.m.planes[1].bytesused =
                in_buf.m.planes[2].bytesused = 0;
        }

        if (v4l.outplane_mem_type == V4L2_MEMORY_MMAP ||
                v4l.outplane_mem_type == V4L2_MEMORY_DMABUF)
        {
            for (uint32_t j = 0; j < buffer->n_planes; ++j)
            {
                ret_val = NvBufferMemSyncForDevice(buffer->planes[j].fd, j,
                    (void **)&buffer->planes[j].data);
                if (ret_val < 0)
                {
                    cerr << "Error while NvBufferMemSyncForDevice at output plane" << endl;
                    v4l.in_error = 1;
                    break;
                }
            }
        }

        ret_val = q_buffer(&v4l,  in_buf, buffer, v4l.outplane_buf_type,
            v4l.outplane_mem_type, v4l.outplane_num_planes);
        if (ret_val)
        {
            cerr << "Error while queueing buffer on output plane" << endl;
            v4l.in_error = 1;
            break;
        }

        if (in_buf.m.planes[0].bytesused == 0)
        {
            cout << "File read complete." << endl;
            v4l.eos = true;
            break;
        }

    }

    return ret_val;
}

int main (int argc, char const *argv[])
{
    context_t v4l;
    int ret = 0;
    struct v4l2_buffer outplane_v4l2_buf;
    struct v4l2_plane outputplanes[MAX_PLANES];
    struct v4l2_exportbuffer outplane_expbuf;
    struct v4l2_buffer capplane_v4l2_buf;
    struct v4l2_plane captureplanes[MAX_PLANES];
    struct v4l2_exportbuffer capplane_expbuf;

    // Initialisation.

    memset(&v4l, 0, sizeof (context_t));
    v4l.outplane_mem_type = V4L2_MEMORY_MMAP;
    v4l.capplane_mem_type = V4L2_MEMORY_MMAP;
    v4l.outplane_buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    v4l.capplane_buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    v4l.fd = -1;
    v4l.outplane_buffers = NULL;
    v4l.capplane_buffers = NULL;
    v4l.num_queued_outplane_buffers = 0;
    v4l.num_queued_capplane_buffers = 0;
    v4l.dqthread_running = false;
    v4l.enc_dq_thread = 0;
    pthread_mutex_init(&v4l.queue_lock, NULL);
    pthread_cond_init(&v4l.queue_cond, NULL);

    assert(argc == 4);
    v4l.output_file_path = argv[3];
    v4l.width = atoi(argv[1]);
    v4l.height = atoi(argv[2]);

    v4l.output_file = new ofstream(v4l.output_file_path);
    CHECK_ERROR(!v4l.output_file->is_open(),
        "Error in opening output file", cleanup);

    CALL(v4l.fd = v4l2_open(ENCODER_DEV, O_RDWR));
    struct v4l2_capability cap;
    CALL(v4l2_ioctl(v4l.fd, VIDIOC_QUERYCAP, &cap), cleanup);
    ASSERT_INT((cap.capabilities & V4L2_CAP_VIDEO_M2M_MPLANE), ==, 0, cleanup);
    ASSERT_INT((cap.capabilities & V4L2_CAP_STREAMING), ==, 0, cleanup);


    struct v4l2_format fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    fmt.fmt.pix_mp.width = v4l.width;
    fmt.fmt.pix_mp.height = v4l.height;
    fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_H264;
    fmt.fmt.pix_mp.num_planes = 1;
    CALL(v4l2_ioctl(v4l.fd, VIDIOC_S_FMT, &fmt));
    DEBUG("Out plane - stride: %d", fmt.fmt.pix_mp.plane_fmt[0].bytesperline);
    DEBUG("Out plane - sizeimage: %d", fmt.fmt.pix_mp.plane_fmt[0].sizeimage);
    DEBUG("Out plane - num_planes: %d", fmt.fmt.pix_mp.num_planes);
    v4l.capplane_num_planes = fmt.fmt.pix_mp.num_planes;
    for (uint32_t i = 0; i < v4l.capplane_num_planes; ++i)
    {
        v4l.capplane_planefmts[i].stride =
            fmt.fmt.pix_mp.plane_fmt[i].bytesperline;
        v4l.capplane_planefmts[i].sizeimage =
            fmt.fmt.pix_mp.plane_fmt[i].sizeimage;
    }

    uint32_t num_bufferplanes;
    Buffer::BufferPlaneFormat planefmts[MAX_PLANES];
    Buffer::fill_buffer_plane_format(&num_bufferplanes, planefmts, v4l.width, v4l.height,
        V4L2_PIX_FMT_YUV444M);

    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    fmt.fmt.pix_mp.width = v4l.width;
    fmt.fmt.pix_mp.height = v4l.height;
    fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_YUV444M;
    fmt.fmt.pix_mp.num_planes = 3;
    CALL(v4l2_ioctl(v4l.fd, VIDIOC_S_FMT, &fmt), cleanup);
    for (int i = 0; i < fmt.fmt.pix_mp.num_planes; i++)
    {
        DEBUG("In plane[%d] - stride: %d", i, fmt.fmt.pix_mp.plane_fmt[i].bytesperline);
        DEBUG("In Plane[%d] - sizeimage: %d", i, fmt.fmt.pix_mp.plane_fmt[i].sizeimage);
    }
    v4l.outplane_num_planes = fmt.fmt.pix_mp.num_planes;
    for (uint32_t j = 0; j < v4l.outplane_num_planes; j++)
    {
        v4l.outplane_planefmts[j] = planefmts[j];
        v4l.outplane_planefmts[j].stride = fmt.fmt.pix_mp.plane_fmt[j].bytesperline;
        v4l.outplane_planefmts[j].sizeimage = fmt.fmt.pix_mp.plane_fmt[j].sizeimage;
    }

    ret = req_buffers_on_output_plane(&v4l, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
        v4l.outplane_mem_type, 10);
    CHECK_ERROR(ret, "Error in requesting buffers on output plane", cleanup);

    /* Query the status of requested buffers
    ** For each requested buffer, export buffer
    ** and map it for MMAP memory.
    */

    for (uint32_t i = 0; i < v4l.outplane_num_buffers; ++i)
    {
        memset(&outplane_v4l2_buf, 0, sizeof (struct v4l2_buffer));
        memset(outputplanes, 0, sizeof (struct v4l2_plane));
        outplane_v4l2_buf.index = i;
        outplane_v4l2_buf.type = v4l.outplane_buf_type;
        outplane_v4l2_buf.memory = v4l.outplane_mem_type;
        outplane_v4l2_buf.m.planes = outputplanes;
        outplane_v4l2_buf.length = v4l.outplane_num_planes;

        ret = v4l2_ioctl(v4l.fd, VIDIOC_QUERYBUF, &outplane_v4l2_buf);
        CHECK_ERROR(ret, "Error in querying for "<< i <<
            "th buffer outputplane", cleanup);

        for (uint32_t j = 0; j < outplane_v4l2_buf.length; ++j)
        {
            v4l.outplane_buffers[i]->planes[j].length =
                outplane_v4l2_buf.m.planes[j].length;
            v4l.outplane_buffers[i]->planes[j].mem_offset =
                outplane_v4l2_buf.m.planes[j].m.mem_offset;
        }

        memset(&outplane_expbuf, 0, sizeof (struct v4l2_exportbuffer));
        outplane_expbuf.type = v4l.outplane_buf_type;
        outplane_expbuf.index = i;

        for (uint32_t j = 0; j < v4l.outplane_num_planes; ++j)
        {
            outplane_expbuf.plane = j;
            ret = v4l2_ioctl(v4l.fd, VIDIOC_EXPBUF, &outplane_expbuf);
            CHECK_ERROR(ret, "Error in exporting "<< i <<
                "th index buffer outputplane", cleanup);

            v4l.outplane_buffers[i]->planes[j].fd = outplane_expbuf.fd;
        }

        if (v4l.outplane_buffers[i]->map())
        {
            cerr << "Buffer mapping error on output plane" << endl;
            v4l.in_error = 1;
            goto cleanup;
        }

    }

    // Request buffers on capture plane.

    ret = req_buffers_on_capture_plane(&v4l, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
        v4l.capplane_mem_type, 6);
    CHECK_ERROR(ret, "Error in requesting buffers on capture plane", cleanup);

    /* Query the status of requested buffers
    ** For each requested buffer, export buffer
    ** and map it for MMAP memory.
    */

    for (uint32_t i = 0; i < v4l.capplane_num_buffers; ++i)
    {
        memset(&capplane_v4l2_buf, 0, sizeof (struct v4l2_buffer));
        memset(captureplanes, 0, sizeof (struct v4l2_plane));
        capplane_v4l2_buf.index = i;
        capplane_v4l2_buf.type = v4l.capplane_buf_type;
        capplane_v4l2_buf.memory = v4l.capplane_mem_type;
        capplane_v4l2_buf.m.planes = captureplanes;
        capplane_v4l2_buf.length = v4l.capplane_num_planes;

        ret = v4l2_ioctl(v4l.fd, VIDIOC_QUERYBUF, &capplane_v4l2_buf);
        CHECK_ERROR(ret, "Error in querying for "<< i <<
            "th buffer captureplane", cleanup);

        for (uint32_t j = 0; j < capplane_v4l2_buf.length; ++j)
        {
            v4l.capplane_buffers[i]->planes[j].length =
                capplane_v4l2_buf.m.planes[j].length;
            v4l.capplane_buffers[i]->planes[j].mem_offset =
                capplane_v4l2_buf.m.planes[j].m.mem_offset;
        }

        memset(&capplane_expbuf, 0, sizeof (struct v4l2_exportbuffer));
        capplane_expbuf.type = v4l.capplane_buf_type;
        capplane_expbuf.index = i;

        for (uint32_t j = 0; j < v4l.capplane_num_planes; ++j)
        {
            capplane_expbuf.plane = j;
            ret = v4l2_ioctl(v4l.fd, VIDIOC_EXPBUF, &capplane_expbuf);
            CHECK_ERROR(ret, "Error in exporting "<< i <<
                "th index buffer captureplane", cleanup);

            v4l.capplane_buffers[i]->planes[j].fd = capplane_expbuf.fd;
        }

        if (v4l.capplane_buffers[i]->map())
        {
            cerr << "Buffer mapping error on capture plane" << endl;
            v4l.in_error = 1;
            goto cleanup;
        }
    }

    /* Subscribe to EOS event, triggered
    ** when zero sized buffer is enquequed
    ** on output plane.
    */
    //ret = subscribe_event(v4l.fd, V4L2_EVENT_EOS, 0, 0);
    //CHECK_ERROR(ret, "Error in subscribing to EOS change", cleanup);

    /* Set streaming on both plane
    ** Start stream processing on output plane and capture
    ** plane by setting the streaming status ON.
    */

    enum v4l2_buf_type type;
    type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    V4L_CALL(v4l2_ioctl(v4l.fd, VIDIOC_STREAMON, &type), cleanup);
    v4l.outplane_streamon = 1;

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    V4L_CALL(v4l2_ioctl(v4l.fd, VIDIOC_STREAMON, &type), cleanup);
    CHECK_ERROR(ret, "Error in setting streaming status ON capture plane", cleanup);
    v4l.capplane_streamon = 1;

    //pthread_mutex_lock(&v4l.queue_lock);
    //pthread_create(&v4l.enc_dq_thread, NULL, dq_thread, &v4l);
    //v4l.dqthread_running = true;
    //pthread_mutex_unlock(&v4l.queue_lock);

    // First enqueue all the empty buffers on capture plane.
    /*for (uint32_t i = 0; i < v4l.capplane_num_buffers; ++i)
    {
        struct v4l2_buffer queue_cap_v4l2_buf;
        struct v4l2_plane queue_cap_planes[MAX_PLANES];
        Buffer *buffer;

        memset(&queue_cap_v4l2_buf, 0, sizeof (struct v4l2_buffer));
        memset(queue_cap_planes, 0, MAX_PLANES * sizeof (struct v4l2_plane));

        buffer = v4l.capplane_buffers[i];
        queue_cap_v4l2_buf.index = i;
        queue_cap_v4l2_buf.m.planes = queue_cap_planes;

        ret = q_buffer(&v4l, queue_cap_v4l2_buf, buffer, v4l.capplane_buf_type,
                v4l.capplane_mem_type, v4l.capplane_num_planes);
        CHECK_ERROR(ret, "Error while queueing buffer on capture plane", cleanup);
    }*/

    struct v4l2_buffer in_buf;
    struct v4l2_plane in_planes[3];
    DEBUG("v4l.outplane_num_buffers: %d", v4l.outplane_num_buffers);
    for (uint32_t i = 0; i < v4l.outplane_num_buffers; ++i) {
        Buffer* buffer = v4l.outplane_buffers[i];
        CALL(read_video_frame(*buffer));

        memset(&in_buf, 0, sizeof(in_buf));
        memset(in_planes, 0, sizeof(in_planes));
        in_buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
        in_buf.memory = V4L2_MEMORY_MMAP;
        in_buf.index = i;
        in_buf.length = 3;
        in_buf.m.planes = in_planes;
        in_buf.m.planes[0].bytesused = buffer->planes[0].bytesused;
        in_buf.m.planes[1].bytesused = buffer->planes[1].bytesused;
        in_buf.m.planes[2].bytesused = buffer->planes[2].bytesused;
        DEBUG("bytesused: %d, %d, %d", in_buf.m.planes[0].bytesused, in_buf.m.planes[1].bytesused,
            in_buf.m.planes[2].bytesused);
        for (int j = 0; j < 3; j++) {
            CALL(
                NvBufferMemSyncForDevice(buffer->planes[j].fd, j, (void **)&buffer->planes[j].data);
            );
        }
        V4L_CALL(v4l2_ioctl(v4l.fd, VIDIOC_QBUF, &in_buf), cleanup);
    }

    // for (uint32_t i = 0; i < v4l.outplane_num_buffers; ++i)
    // {
    //     struct v4l2_buffer v4l2_buf;
    //     struct v4l2_plane planes[MAX_PLANES];
    //     Buffer *buffer;

    //     memset(&v4l2_buf, 0, sizeof (v4l2_buf));
    //     memset(planes, 0, MAX_PLANES * sizeof (struct v4l2_plane));

    //     buffer = v4l.outplane_buffers[i];
    //     v4l2_buf.index = i;
    //     v4l2_buf.m.planes = planes;

    //     //ret = read_video_frame(v4l.input_file, *buffer);
    //     ret = read_video_frame(*buffer);
    //     if (ret < 0)
    //     {
    //         cerr << "Could not read complete frame from input file" << endl;
    //         v4l.eos = true;
    //         v4l2_buf.m.planes[0].m.userptr = 0;
    //         v4l2_buf.m.planes[0].bytesused =
    //             v4l2_buf.m.planes[1].bytesused =
    //             v4l2_buf.m.planes[2].bytesused = 0;
    //     }

    //     if (v4l.outplane_mem_type == V4L2_MEMORY_MMAP ||
    //             v4l.outplane_mem_type == V4L2_MEMORY_DMABUF)
    //     {
    //         for (uint32_t j = 0; j < buffer->n_planes; ++j)
    //         {
    //             ret = NvBufferMemSyncForDevice(buffer->planes[j].fd, j,
    //                 (void **)&buffer->planes[j].data);
    //             CHECK_ERROR(ret < 0,
    //                 "Error while NvBufferMemSyncForDevice at output plane", cleanup);
    //         }
    //     }

    //     /* Enqueue the buffer on output plane
    //     ** It is necessary to queue an empty buffer
    //     ** to signal EOS to the encoder.
    //     */
    //     ret = q_buffer(&v4l, v4l2_buf, buffer, v4l.outplane_buf_type,
    //         v4l.outplane_mem_type, v4l.outplane_num_planes);
    //     CHECK_ERROR(ret, "Error while queueing buffer on output plane", cleanup);

    //     if (v4l2_buf.m.planes[0].bytesused == 0)
    //     {
    //         cout << "File read complete." << endl;
    //         v4l.eos = true;
    //         break;
    //     }
    // }

    DEBUG("encoder_process_blocking");
    // Dequeue and queue loop on output plane.
    ret = encoder_process_blocking(v4l);
    CHECK_ERROR(ret < 0, "Encoder is in error", cleanup);

    /* For blocking mode, after getting EOS on output plane, wait
    ** till all the buffers are successfully from the capture plane.
    */
    wait_for_dqthread(v4l, -1);

cleanup:
    if (v4l.fd != -1)
    {

        // Stream off on both planes.

        ret = v4l2_ioctl(v4l.fd, VIDIOC_STREAMOFF, &v4l.outplane_buf_type);
        v4l.outplane_streamon = 0;
        ret = v4l2_ioctl(v4l.fd, VIDIOC_STREAMOFF, &v4l.capplane_buf_type);
        v4l.capplane_streamon = 0;

        // Unmap MMAPed buffers.

        for (uint32_t i = 0; i < v4l.outplane_num_buffers; ++i)
        {
            v4l.outplane_buffers[i]->unmap();
        }
        for (uint32_t i = 0; i < v4l.capplane_num_buffers; ++i)
        {
            v4l.capplane_buffers[i]->unmap();
        }

        // Request 0 buffers on both planes.

        ret = req_buffers_on_output_plane(&v4l, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
            v4l.outplane_mem_type, 0);
        ret = req_buffers_on_capture_plane(&v4l, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
            v4l.capplane_mem_type, 0);

        // Close the opened V4L2 device.

        ret = v4l2_close(v4l.fd);
        if (ret)
        {
            cerr << "Unable to close the device" << endl;
            v4l.in_error = 1;
        }

    }

    //v4l.input_file->close();
    v4l.output_file->close();

    //delete v4l.input_file;
    delete v4l.output_file;

    if (v4l.in_error)
    {
        cerr << "Encoder is in error << endl" << endl;
    }

    else
    {
        cout << "Encoder Run Successful" << endl;
    }

    return ret;
}