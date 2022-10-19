// #include <iostream>
// #include <string>
// #include <sstream>
// #include <fstream>
// #include <stdint.h>
// #include <unistd.h>
// #include <cstdlib>
// #include <libv4l2.h>
// #include <linux/videodev2.h>
// #include <malloc.h>
// #include <pthread.h>
// #include <string.h>
// #include <fcntl.h>
// #include <sys/mman.h>
// #include <errno.h>
// #include <assert.h>

// #include "nvbuf_utils.h"
// #include "v4l2_nv_extensions.h"

#include "v4l_encoder.h"

#include <libv4l2.h>
#include <linux/videodev2.h>
#include "sys/mman.h"

#define CHECK_ERROR(condition, error_str, label) if (condition) { \
                                                        cerr << error_str << endl; \
                                                        v4l.in_error = 1; \
                                                        goto label; }

extern struct v4l_encoder_state_t v4l;

// static int
// write_encoded_frame(ofstream * stream, Buffer * buffer)
// {
//     stream->write((char *)buffer->planes[0].data, buffer->planes[0].bytesused);
//     return 0;
// }

int encoder_process_blocking()
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
        int cb = in_buf.index;
        for (uint32_t j = 0; j < 3; j++)
        {
            DEBUG("In plane[%d] - bytesused: %d", j, in_buf.m.planes[j].bytesused);
        }
        for (uint32_t j = 0; j < 3; ++j)
        {
            ret_val = NvBufferMemSyncForDevice(v4l.in_bufs[cb][j].fd, j, 
                (void **)&v4l.in_bufs[cb][j].buf);
            if (ret_val < 0)
            {
                DEBUG("Error while NvBufferMemSyncForDevice at output plane");
                v4l.in_error = 1;
                break;
            }
        }
        in_buf.m.planes[0].bytesused = v4l.in_strides[0] * v4l.video_height;
        in_buf.m.planes[1].bytesused = v4l.in_strides[1] * v4l.video_height;
        in_buf.m.planes[2].bytesused = v4l.in_strides[2] * v4l.video_height;
        DEBUG("bytesused: %d, %d, %d", in_buf.m.planes[0].bytesused, in_buf.m.planes[1].bytesused,
            in_buf.m.planes[2].bytesused);
        for (int j = 0; j < 3; j++) {
            CALL(
                NvBufferMemSyncForDevice(v4l.in_bufs[cb][j].fd, j, (void **)&v4l.in_bufs[cb][j].buf);
            );
        }
        V4L_CALL(v4l2_ioctl(v4l.dev_id, VIDIOC_QBUF, &in_buf));

        if (in_buf.m.planes[0].bytesused == 0)
        {
            DEBUG("File read complete.");
            v4l.eos = 1;
            break;
        }

    }

    return ret_val;
}

