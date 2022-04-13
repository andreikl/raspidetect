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

static struct v4l_encoder_state_t v4l;


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
        this->planes[i].dev_id = -1;
        this->planes[i].data = NULL;
        this->planes[i].bytesused = 0;
        this->planes[i].mem_offset = 0;
        this->planes[i].length = 0;
    }
}

Buffer::Buffer(enum v4l2_buf_type buf_type, enum v4l2_memory memory_type,
        uint32_t n_planes, uint32_t index)
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
        this->planes[i].dev_id = -1;
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
        if (planes[j].dev_id == -1)
        {
            return -1;
        }

        planes[j].data = (unsigned char *) mmap(NULL,
                                                planes[j].length,
                                                PROT_READ | PROT_WRITE,
                                                MAP_SHARED,
                                                planes[j].dev_id,
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

static int
read_video_frame(Buffer & buffer)
{
    uint32_t i, j;
    char *data;

    for (i = 0; i < buffer.n_planes; i++)
    {
        Buffer::BufferPlane &plane = buffer.planes[i];
        streamsize bytes_to_read = v4l.in_strides[i];
        data = (char *) plane.data;
        plane.bytesused = 0;
        /* It is necessary to set bytesused properly,
        ** so that encoder knows how
        ** many bytes in the buffer to be read.
        */
        for (j = 0; j < v4l.height; j++)
        {
            /*stream->read(data, bytes_to_read);
            if (stream->gcount() < bytes_to_read)
                return -1;*/
            data += v4l.in_strides[i];
        }
        plane.bytesused = v4l.in_strides[i] * v4l.height;
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
encoder_process_blocking()
{
    int ret_val = 0;
    while (!v4l.in_error && !v4l.eos)
    {
        struct v4l2_buffer in_buf;
        struct v4l2_plane in_planes[3];
        memset(&in_buf, 0, sizeof(in_buf));
        memset(in_planes, 0, sizeof(in_planes));
        in_buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
        in_buf.memory = V4L2_MEMORY_MMAP;
        in_buf.m.planes = in_planes;
        V4L_CALL(v4l2_ioctl(v4l.dev_id, VIDIOC_DQBUF, &in_buf));
        Buffer *buffer = v4l.outplane_buffers[in_buf.index];
        for (uint32_t j = 0; j < v4l.outplane_buffers[in_buf.index]->n_planes; j++)
        {
            DEBUG("In plane[%d] - bytesused: %d", j, in_buf.m.planes[j].bytesused);
            v4l.outplane_buffers[in_buf.index]->planes[j].bytesused = 
                in_buf.m.planes[j].bytesused;
        }
        v4l.num_queued_outplane_buffers--;

        DEBUG("read_video_frame");
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
                ret_val = NvBufferMemSyncForDevice(buffer->planes[j].dev_id, j,
                    (void **)&buffer->planes[j].data);
                if (ret_val < 0)
                {
                    cerr << "Error while NvBufferMemSyncForDevice at output plane" << endl;
                    v4l.in_error = 1;
                    break;
                }
            }
        }

        in_buf.m.planes[0].bytesused = buffer->planes[0].bytesused;
        in_buf.m.planes[1].bytesused = buffer->planes[1].bytesused;
        in_buf.m.planes[2].bytesused = buffer->planes[2].bytesused;
        DEBUG("bytesused: %d, %d, %d", in_buf.m.planes[0].bytesused, in_buf.m.planes[1].bytesused,
            in_buf.m.planes[2].bytesused);
        for (int j = 0; j < 3; j++) {
            CALL(
                NvBufferMemSyncForDevice(buffer->planes[j].dev_id, j, (void **)&buffer->planes[j].data);
            );
        }
        V4L_CALL(v4l2_ioctl(v4l.dev_id, VIDIOC_QBUF, &in_buf));

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

    v4l.in_bufs[0][0].buf = (uint8_t*)MAP_FAILED;
    v4l.in_bufs[0][0].fd = -1;
    v4l.in_bufs[0][1].buf = (uint8_t*)MAP_FAILED;
    v4l.in_bufs[0][1].fd = -1;
    v4l.in_bufs[0][2].buf = (uint8_t*)MAP_FAILED;
    v4l.in_bufs[0][2].fd = -1;
    v4l.in_bufs[1][0].buf = (uint8_t*)MAP_FAILED;
    v4l.in_bufs[1][0].fd = -1;
    v4l.in_bufs[1][1].buf = (uint8_t*)MAP_FAILED;
    v4l.in_bufs[1][1].fd = -1;
    v4l.in_bufs[1][2].buf = (uint8_t*)MAP_FAILED;
    v4l.in_bufs[1][2].fd = -1;
    v4l.in_bufs[2][0].buf = (uint8_t*)MAP_FAILED;
    v4l.in_bufs[2][0].fd = -1;
    v4l.in_bufs[2][1].buf = (uint8_t*)MAP_FAILED;
    v4l.in_bufs[2][1].fd = -1;
    v4l.in_bufs[2][2].buf = (uint8_t*)MAP_FAILED;
    v4l.in_bufs[2][2].fd = -1;
    v4l.in_bufs[3][0].buf = (uint8_t*)MAP_FAILED;
    v4l.in_bufs[3][0].fd = -1;
    v4l.in_bufs[3][1].buf = (uint8_t*)MAP_FAILED;
    v4l.in_bufs[3][1].fd = -1;
    v4l.in_bufs[3][2].buf = (uint8_t*)MAP_FAILED;
    v4l.in_bufs[3][2].fd = -1;
    v4l.in_bufs[4][0].buf = (uint8_t*)MAP_FAILED;
    v4l.in_bufs[4][0].fd = -1;
    v4l.in_bufs[4][1].buf = (uint8_t*)MAP_FAILED;
    v4l.in_bufs[4][1].fd = -1;
    v4l.in_bufs[4][2].buf = (uint8_t*)MAP_FAILED;
    v4l.in_bufs[4][2].fd = -1;
    v4l.in_bufs[5][0].buf = (uint8_t*)MAP_FAILED;
    v4l.in_bufs[5][0].fd = -1;
    v4l.in_bufs[5][1].buf = (uint8_t*)MAP_FAILED;
    v4l.in_bufs[5][1].fd = -1;
    v4l.in_bufs[5][2].buf = (uint8_t*)MAP_FAILED;
    v4l.in_bufs[5][2].fd = -1;
    v4l.in_bufs[6][0].buf = (uint8_t*)MAP_FAILED;
    v4l.in_bufs[6][0].fd = -1;
    v4l.in_bufs[6][1].buf = (uint8_t*)MAP_FAILED;
    v4l.in_bufs[6][1].fd = -1;
    v4l.in_bufs[6][2].buf = (uint8_t*)MAP_FAILED;
    v4l.in_bufs[6][2].fd = -1;
    v4l.in_bufs[7][0].buf = (uint8_t*)MAP_FAILED;
    v4l.in_bufs[7][0].fd = -1;
    v4l.in_bufs[7][1].buf = (uint8_t*)MAP_FAILED;
    v4l.in_bufs[7][1].fd = -1;
    v4l.in_bufs[7][2].buf = (uint8_t*)MAP_FAILED;
    v4l.in_bufs[7][2].fd = -1;
    v4l.in_bufs[8][0].buf = (uint8_t*)MAP_FAILED;
    v4l.in_bufs[8][0].fd = -1;
    v4l.in_bufs[8][1].buf = (uint8_t*)MAP_FAILED;
    v4l.in_bufs[8][1].fd = -1;
    v4l.in_bufs[8][2].buf = (uint8_t*)MAP_FAILED;
    v4l.in_bufs[8][2].fd = -1;
    v4l.in_bufs[9][0].buf = (uint8_t*)MAP_FAILED;
    v4l.in_bufs[9][0].fd = -1;
    v4l.in_bufs[9][1].buf = (uint8_t*)MAP_FAILED;
    v4l.in_bufs[9][1].fd = -1;
    v4l.in_bufs[9][2].buf = (uint8_t*)MAP_FAILED;
    v4l.in_bufs[9][2].fd = -1;
    v4l.out_bufs[0].buf = (uint8_t*)MAP_FAILED;
    v4l.out_bufs[0].fd = -1;
    v4l.out_bufs[1].buf = (uint8_t*)MAP_FAILED;
    v4l.out_bufs[1].fd = -1;
    v4l.out_bufs[2].buf = (uint8_t*)MAP_FAILED;
    v4l.out_bufs[2].fd = -1;
    v4l.out_bufs[3].buf = (uint8_t*)MAP_FAILED;
    v4l.out_bufs[3].fd = -1;
    v4l.out_bufs[4].buf = (uint8_t*)MAP_FAILED;
    v4l.out_bufs[4].fd = -1;
    v4l.out_bufs[5].buf = (uint8_t*)MAP_FAILED;
    v4l.out_bufs[5].fd = -1;
    v4l.in_bufs_count = V4L_MAX_IN_BUFS;
    v4l.out_bufs_count = V4L_MAX_OUT_BUFS;

    v4l.outplane_mem_type = V4L2_MEMORY_MMAP;
    v4l.capplane_mem_type = V4L2_MEMORY_MMAP;
    v4l.outplane_buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    v4l.capplane_buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    v4l.dev_id = -1;
    v4l.outplane_buffers = NULL;
    v4l.capplane_buffers = NULL;
    v4l.num_queued_outplane_buffers = 0;
    v4l.num_queued_capplane_buffers = 0;
    v4l.dqthread_running = false;
    v4l.enc_dq_thread = 0;

    int ret = 0;
    struct v4l2_buffer outplane_v4l2_buf;
    struct v4l2_plane outputplanes[MAX_PLANES];
    struct v4l2_exportbuffer outplane_expbuf;
    struct v4l2_buffer capplane_v4l2_buf;
    struct v4l2_plane captureplanes[MAX_PLANES];
    struct v4l2_exportbuffer capplane_expbuf;

    assert(argc == 4);
    v4l.output_file_path = argv[3];
    v4l.width = atoi(argv[1]);
    v4l.height = atoi(argv[2]);

    v4l.output_file = new ofstream(v4l.output_file_path);
    CHECK_ERROR(!v4l.output_file->is_open(),
        "Error in opening output file", cleanup);

    CALL(v4l.dev_id = v4l2_open(ENCODER_DEV, O_RDWR));
    struct v4l2_capability cap;
    CALL(v4l2_ioctl(v4l.dev_id, VIDIOC_QUERYCAP, &cap), cleanup);
    ASSERT_INT((cap.capabilities & V4L2_CAP_VIDEO_M2M_MPLANE), ==, 0, cleanup);
    ASSERT_INT((cap.capabilities & V4L2_CAP_STREAMING), ==, 0, cleanup);


    struct v4l2_format fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    fmt.fmt.pix_mp.width = v4l.width;
    fmt.fmt.pix_mp.height = v4l.height;
    fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_H264;
    fmt.fmt.pix_mp.num_planes = 1;
    CALL(v4l2_ioctl(v4l.dev_id, VIDIOC_S_FMT, &fmt));
    v4l.out_sizeimages[0] = fmt.fmt.pix_mp.plane_fmt[0].sizeimage;
    v4l.out_strides[0] = fmt.fmt.pix_mp.plane_fmt[0].bytesperline;
    DEBUG("Out plane - size: %d, stride: %d", v4l.out_sizeimages[0], v4l.out_strides[0]);

    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    fmt.fmt.pix_mp.width = v4l.width;
    fmt.fmt.pix_mp.height = v4l.height;
    fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_YUV444M;
    fmt.fmt.pix_mp.num_planes = 3;
    CALL(v4l2_ioctl(v4l.dev_id, VIDIOC_S_FMT, &fmt), cleanup);
    for (int i = 0; i < fmt.fmt.pix_mp.num_planes; i++) {
        v4l.in_sizeimages[i] = fmt.fmt.pix_mp.plane_fmt[i].sizeimage;
        v4l.in_strides[i] = fmt.fmt.pix_mp.plane_fmt[i].bytesperline;
        DEBUG("In Plane[%d] - size: %d, stride: %d", i, v4l.in_sizeimages[i], v4l.in_strides[i]);
    }

    struct v4l2_requestbuffers req;
    memset(&req, 0, sizeof(req));
    req.count = V4L_MAX_IN_BUFS;
    req.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    req.memory = V4L2_MEMORY_MMAP;
    V4L_CALL(v4l2_ioctl(v4l.dev_id, VIDIOC_REQBUFS, &req), cleanup);
    ASSERT_INT(req.count, >, V4L_MAX_IN_BUFS, cleanup);
    ASSERT_INT(req.count, <=, 0, cleanup);
    DEBUG("v4l.in_bufs_count: %d, v4l.outplane_num_buffers %d", req.count, v4l.outplane_num_buffers);
    v4l.outplane_buffers = new Buffer *[req.count];
    for (uint32_t i = 0; i < req.count; ++i)
    {
        v4l.outplane_buffers[i] = new Buffer (V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, V4L2_MEMORY_MMAP,
            3, i);
    }
    v4l.outplane_num_buffers = req.count;

    for (uint32_t i = 0; i < v4l.outplane_num_buffers; ++i)
    {
        memset(&outplane_v4l2_buf, 0, sizeof (struct v4l2_buffer));
        memset(outputplanes, 0, sizeof (struct v4l2_plane));
        outplane_v4l2_buf.index = i;
        outplane_v4l2_buf.type = v4l.outplane_buf_type;
        outplane_v4l2_buf.memory = v4l.outplane_mem_type;
        outplane_v4l2_buf.m.planes = outputplanes;
        outplane_v4l2_buf.length = 3;

        ret = v4l2_ioctl(v4l.dev_id, VIDIOC_QUERYBUF, &outplane_v4l2_buf);
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

        for (uint32_t j = 0; j < 3; ++j)
        {
            outplane_expbuf.plane = j;
            ret = v4l2_ioctl(v4l.dev_id, VIDIOC_EXPBUF, &outplane_expbuf);
            CHECK_ERROR(ret, "Error in exporting "<< i <<
                "th index buffer outputplane", cleanup);

            v4l.outplane_buffers[i]->planes[j].dev_id = outplane_expbuf.fd;
        }

        if (v4l.outplane_buffers[i]->map())
        {
            cerr << "Buffer mapping error on output plane" << endl;
            v4l.in_error = 1;
            goto cleanup;
        }

    }

    memset(&req, 0, sizeof(req));
    req.count = V4L_MAX_OUT_BUFS;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    req.memory = V4L2_MEMORY_MMAP;
    V4L_CALL(v4l2_ioctl(v4l.dev_id, VIDIOC_REQBUFS, &req), cleanup);
    ASSERT_INT(req.count, >, V4L_MAX_OUT_BUFS, cleanup);
    ASSERT_INT(req.count, <=, 0, cleanup);
    DEBUG("v4l.out_bufs_count: %d", req.count);
    v4l.capplane_buffers = new Buffer *[req.count];
    for (uint32_t i = 0; i < req.count; ++i)
    {
        v4l.capplane_buffers[i] = new Buffer (V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, V4L2_MEMORY_MMAP,
            1, i);
    }
    v4l.capplane_num_buffers = req.count;


    for (uint32_t i = 0; i < v4l.capplane_num_buffers; ++i)
    {
        memset(&capplane_v4l2_buf, 0, sizeof (struct v4l2_buffer));
        memset(captureplanes, 0, sizeof (struct v4l2_plane));
        capplane_v4l2_buf.index = i;
        capplane_v4l2_buf.type = v4l.capplane_buf_type;
        capplane_v4l2_buf.memory = v4l.capplane_mem_type;
        capplane_v4l2_buf.m.planes = captureplanes;
        capplane_v4l2_buf.length = 1;

        ret = v4l2_ioctl(v4l.dev_id, VIDIOC_QUERYBUF, &capplane_v4l2_buf);
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

        for (uint32_t j = 0; j < 1; ++j)
        {
            capplane_expbuf.plane = j;
            ret = v4l2_ioctl(v4l.dev_id, VIDIOC_EXPBUF, &capplane_expbuf);
            CHECK_ERROR(ret, "Error in exporting "<< i <<
                "th index buffer captureplane", cleanup);

            v4l.capplane_buffers[i]->planes[j].dev_id = capplane_expbuf.fd;
        }

        if (v4l.capplane_buffers[i]->map())
        {
            cerr << "Buffer mapping error on capture plane" << endl;
            v4l.in_error = 1;
            goto cleanup;
        }
    }

    enum v4l2_buf_type type;
    type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    V4L_CALL(v4l2_ioctl(v4l.dev_id, VIDIOC_STREAMON, &type), cleanup);
    v4l.outplane_streamon = 1;

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    V4L_CALL(v4l2_ioctl(v4l.dev_id, VIDIOC_STREAMON, &type), cleanup);
    CHECK_ERROR(ret, "Error in setting streaming status ON capture plane", cleanup);
    v4l.capplane_streamon = 1;

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
                NvBufferMemSyncForDevice(buffer->planes[j].dev_id, j, (void **)&buffer->planes[j].data);
            );
        }
        V4L_CALL(v4l2_ioctl(v4l.dev_id, VIDIOC_QBUF, &in_buf), cleanup);
    }

    DEBUG("encoder_process_blocking");
    // Dequeue and queue loop on output plane.
    ret = encoder_process_blocking();
    CHECK_ERROR(ret < 0, "Encoder is in error", cleanup);

cleanup:
    if (v4l.dev_id != -1)
    {

        // Stream off on both planes.

        ret = v4l2_ioctl(v4l.dev_id, VIDIOC_STREAMOFF, &v4l.outplane_buf_type);
        v4l.outplane_streamon = 0;
        ret = v4l2_ioctl(v4l.dev_id, VIDIOC_STREAMOFF, &v4l.capplane_buf_type);
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

        ret = v4l2_close(v4l.dev_id);
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