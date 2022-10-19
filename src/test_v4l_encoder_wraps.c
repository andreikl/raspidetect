// Raspidetect

// Copyright (C) 2022 Andrei Klimchuk <andrew.klimchuk@gmail.com>

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

#include "khash.h"
#include "main.h"
#include "utils.h"
#include "app.h"
#include "test.h"

#include <stdarg.h> //va_list
#include <setjmp.h> //jmp_buf
#include <cmocka.h>

#include "v4l_encoder.h"
#include "linux/videodev2.h"

#define VIDEO_PATH "./h264_test_video"

extern int wrap_verbose;
extern int test_verbose;

static int encoder_width = 0;
static int encoder_height = 0;
static int encoder_buffer = -1;
static void* buffers[V4L_MAX_IN_BUFS + V4L_MAX_OUT_BUFS];
static int out_buffers[V4L_MAX_OUT_BUFS];
char buffer[MAX_STRING];

int __wrap_v4l2_open(const char * file, int oflag, ...)
{
    WRAP_DEBUG("v4l2_open, file: %s", file);
    memset(buffers, 0, sizeof(buffers));
    memset(out_buffers, 0, sizeof(out_buffers));
    return 1;
}

int __wrap_v4l2_ioctl(int fd, int request, void * arg)
{
    if (request == (int)VIDIOC_QUERYCAP) {
        WRAP_DEBUG("request: %s", "VIDIOC_QUERYCAP");
        struct v4l2_capability *cap = arg;
        strncpy((char *)cap->card, "test_encoder", 32);
        cap->capabilities = V4L2_CAP_VIDEO_M2M_MPLANE | V4L2_CAP_STREAMING;
        return 0;
    }
    else if (request == (int)VIDIOC_ENUM_FMT) {
        WRAP_DEBUG("request: %s", "VIDIOC_ENUM_FMT");
        struct v4l2_fmtdesc *fmt = arg;
        if (fmt->index == 0 && fmt->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
            fmt->pixelformat = V4L2_PIX_FMT_YUV444M;
            return 0;
        }
        if (fmt->index == 0 && fmt->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
            fmt->pixelformat = V4L2_PIX_FMT_H264;
            return 0;
        }
        else {
            errno = EINVAL;
            return -1;
        }
    }
    else if (request == (int)VIDIOC_ENUM_FRAMESIZES) {
        WRAP_DEBUG("request %s", "VIDIOC_ENUM_FRAMESIZES");
        struct v4l2_frmsizeenum *frmsize = arg;
        if (frmsize->index == 0) {
            frmsize->type = V4L2_FRMSIZE_TYPE_STEPWISE;
            frmsize->stepwise.step_width = 16;
            frmsize->stepwise.step_height = 16;
            frmsize->stepwise.min_width = 320;
            frmsize->stepwise.min_height = 256;
            frmsize->stepwise.max_width = 1024;
            frmsize->stepwise.max_height = 768;
            return 0;
        }
        else {
            errno = EINVAL;
            return -1;
        }
    }
    else if (request == (int)VIDIOC_S_FMT) {
        WRAP_DEBUG("request: %s", "VIDIOC_S_FMT");
        struct v4l2_format *buf = arg;
        if (buf->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
            assert_int_equal(buf->fmt.pix_mp.num_planes, 3);
            encoder_width = buf->fmt.pix_mp.width;
            encoder_height = buf->fmt.pix_mp.height;
            for (int i = 0; i < buf->fmt.pix_mp.num_planes; i++) {
                buf->fmt.pix_mp.plane_fmt[i].sizeimage = encoder_width * encoder_height;
                buf->fmt.pix_mp.plane_fmt[i].bytesperline = encoder_width;
            }
        }
        else 
            if (buf->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
                assert_int_equal(buf->fmt.pix_mp.num_planes, 1);
            }
        return 0;
    }
    else if (request == (int)VIDIOC_REQBUFS) {
        WRAP_DEBUG("request: %s", "VIDIOC_REQBUFS");
        struct v4l2_requestbuffers *buf = arg;
        if (buf->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
            buf->count = V4L_MAX_IN_BUFS;
        else
            if (buf->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
                buf->count = V4L_MAX_OUT_BUFS;

        return 0;
    }
    else if (request == (int)VIDIOC_QUERYBUF) {
        WRAP_DEBUG("request: %s", "VIDIOC_QUERYBUF");
        struct v4l2_buffer *buf = arg;
        if (buf->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
            assert_int_equal(buf->length, 3);
            for (int i = 0; i < buf->length; i++) {
                buf->m.planes[i].length = encoder_width * encoder_height;
                buf->m.planes[i].m.mem_offset = i * encoder_width * encoder_height;
            }
        }
        else 
            if (buf->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
                assert_int_equal(buf->length, 1);
                buf->m.planes[0].length = encoder_width * encoder_height;
                buf->m.planes[0].m.mem_offset = 0;
            }
        return 0;
    }
    else if (request == (int)VIDIOC_EXPBUF) {
        WRAP_DEBUG("request: %s", "VIDIOC_EXPBUF");
        struct v4l2_exportbuffer* buf = arg;
        if (buf->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
            assert_in_range(buf->index, 0, V4L_MAX_IN_BUFS);
            buf->fd = buf->index + 10;
        }
        else 
            if (buf->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
                assert_in_range(buf->index, 0, V4L_MAX_OUT_BUFS);
                buf->fd = buf->index + 20;
            }
        return 0;
    }
    else if (request == (int)VIDIOC_QBUF) {
        WRAP_DEBUG("request: %s", "VIDIOC_QBUF");
        struct v4l2_buffer *buf = arg;
        if (buf->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
            assert_int_equal(buf->length, 3);
            assert_in_range(buf->length, 0, V4L_MAX_IN_BUFS);
        }
        else
            if (buf->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
                assert_int_equal(buf->length, 1);
                assert_in_range(buf->index, 0, V4L_MAX_OUT_BUFS);
                assert_int_equal(out_buffers[buf->index], 0);
                out_buffers[buf->index] = 1;
            }

        return 0;
    }
    else if (request == (int)VIDIOC_STREAMON) {
        WRAP_DEBUG("request: %s", "VIDIOC_STREAMON");
        encoder_buffer = 1;
        return 0;
    }
    else if (request == (int)VIDIOC_STREAMOFF) {
        WRAP_DEBUG("request: %s", "VIDIOC_STREAMOFF");
        return 0;
    }
    else if (request == (int)VIDIOC_DQBUF) {
        WRAP_DEBUG("request: %s", "VIDIOC_DQBUF");
        struct v4l2_buffer *buf = arg;
        if (buf->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
            assert_int_equal(buf->length, 3);
        else
            if (buf->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
                assert_int_equal(buf->length, 1);
                int index = 0;
                while (index < V4L_MAX_OUT_BUFS && out_buffers[index] == 0)
                    index++;
                assert_in_range(index, 0, V4L_MAX_OUT_BUFS);
                assert_in_range(encoder_buffer, 0, 19);

                size_t read = 0;
                //CALL(getcwd(buffer, MAX_STRING));
                //fprintf(stderr, "current directory: %s\n", buffer);
                sprintf(buffer, VIDEO_PATH"/data%d.bin", encoder_buffer++);
                //fprintf(stderr, "path: %s\n", buffer);
                CALL(utils_fill_buffer(
                    buffer,
                    buffers[V4L_MAX_IN_BUFS + index],
                    encoder_width * encoder_height,
                    &read
                ));

                char* t = (char*)buffers[V4L_MAX_IN_BUFS + index];
                TEST_DEBUG("File has been loaded: %d, %x %x %x %x ...",
                    V4L_MAX_IN_BUFS + index,
                    t[0],
                    t[1],
                    t[2],
                    t[3]);
                
                buf->index = index;
                buf->m.planes[0].bytesused = read;
                out_buffers[buf->index] = 0;
            }
        return 0;
    }
    else {
        WRAP_DEBUG("request: %d", request);
    }
    errno = EAGAIN;
    return -1;
}

void * __wrap_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset)
{
    int index = (fd >= 20)? V4L_MAX_IN_BUFS + (fd - 20): (fd - 10);
    WRAP_DEBUG("mmap, index: %d", index);
    assert_int_equal(length, encoder_width * encoder_height);
    assert_in_range(index, 0, V4L_MAX_IN_BUFS + V4L_MAX_OUT_BUFS);
    if (buffers[index] == NULL)
        buffers[index] = malloc(encoder_width * encoder_height);
    assert_ptr_not_equal(buffers[index], NULL);
    return buffers[index];
}

void * __wrap_munmap(void *addr, size_t length)
{
    assert_ptr_not_equal(addr, NULL);
    void** buffer = NULL;
    for (int i = 0; i < V4L_MAX_IN_BUFS + V4L_MAX_OUT_BUFS; i++)
        if (buffers[i] == addr)
            buffer = buffers + i;

    assert_ptr_not_equal(buffer, NULL);
    free(*buffer);
    *buffer = NULL;

    return *buffer;
}

int __wrap_NvBufferMemSyncForDevice(int fd, unsigned int plane, void **addr)
{
    WRAP_DEBUG("NvBufferMemSyncForDevice, fd: %d, index: %d", fd, plane);

    if (fd >= 10 && fd < 20) // ignores input buffers
        return 0;

    int index = (fd >= 20)? V4L_MAX_IN_BUFS + (fd - 20): (fd - 10);
    assert_in_range(index, 0, V4L_MAX_IN_BUFS + V4L_MAX_OUT_BUFS);
    assert_ptr_not_equal(buffers[index], NULL);

    char* t = (char*)buffers[index];
    TEST_DEBUG("buffer about to be copied: %d, %x %x %x %x ...",
        index,
        t[0],
        t[1],
        t[2],
        t[3]);

    memcpy(*addr, buffers[index], encoder_width * encoder_height);
    return 0;
}
