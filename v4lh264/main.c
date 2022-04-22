#include "v4l_encoder.h"

#include <libv4l2.h>
#include <linux/videodev2.h>
#include "sys/mman.h"

struct v4l_encoder_state_t v4l;

int main (int argc, char const *argv[])
{

    v4l.in_bufs[0][0].buf = MAP_FAILED;
    v4l.in_bufs[0][0].fd = -1;
    v4l.in_bufs[0][1].buf = MAP_FAILED;
    v4l.in_bufs[0][1].fd = -1;
    v4l.in_bufs[0][2].buf = MAP_FAILED;
    v4l.in_bufs[0][2].fd = -1;
    v4l.in_bufs[1][0].buf = MAP_FAILED;
    v4l.in_bufs[1][0].fd = -1;
    v4l.in_bufs[1][1].buf = MAP_FAILED;
    v4l.in_bufs[1][1].fd = -1;
    v4l.in_bufs[1][2].buf = MAP_FAILED;
    v4l.in_bufs[1][2].fd = -1;
    v4l.in_bufs[2][0].buf = MAP_FAILED;
    v4l.in_bufs[2][0].fd = -1;
    v4l.in_bufs[2][1].buf = MAP_FAILED;
    v4l.in_bufs[2][1].fd = -1;
    v4l.in_bufs[2][2].buf = MAP_FAILED;
    v4l.in_bufs[2][2].fd = -1;
    v4l.in_bufs[3][0].buf = MAP_FAILED;
    v4l.in_bufs[3][0].fd = -1;
    v4l.in_bufs[3][1].buf = MAP_FAILED;
    v4l.in_bufs[3][1].fd = -1;
    v4l.in_bufs[3][2].buf = MAP_FAILED;
    v4l.in_bufs[3][2].fd = -1;
    v4l.in_bufs[4][0].buf = MAP_FAILED;
    v4l.in_bufs[4][0].fd = -1;
    v4l.in_bufs[4][1].buf = MAP_FAILED;
    v4l.in_bufs[4][1].fd = -1;
    v4l.in_bufs[4][2].buf = MAP_FAILED;
    v4l.in_bufs[4][2].fd = -1;
    v4l.in_bufs[5][0].buf = MAP_FAILED;
    v4l.in_bufs[5][0].fd = -1;
    v4l.in_bufs[5][1].buf = MAP_FAILED;
    v4l.in_bufs[5][1].fd = -1;
    v4l.in_bufs[5][2].buf = MAP_FAILED;
    v4l.in_bufs[5][2].fd = -1;
    v4l.in_bufs[6][0].buf = MAP_FAILED;
    v4l.in_bufs[6][0].fd = -1;
    v4l.in_bufs[6][1].buf = MAP_FAILED;
    v4l.in_bufs[6][1].fd = -1;
    v4l.in_bufs[6][2].buf = MAP_FAILED;
    v4l.in_bufs[6][2].fd = -1;
    v4l.in_bufs[7][0].buf = MAP_FAILED;
    v4l.in_bufs[7][0].fd = -1;
    v4l.in_bufs[7][1].buf = MAP_FAILED;
    v4l.in_bufs[7][1].fd = -1;
    v4l.in_bufs[7][2].buf = MAP_FAILED;
    v4l.in_bufs[7][2].fd = -1;
    v4l.in_bufs[8][0].buf = MAP_FAILED;
    v4l.in_bufs[8][0].fd = -1;
    v4l.in_bufs[8][1].buf = MAP_FAILED;
    v4l.in_bufs[8][1].fd = -1;
    v4l.in_bufs[8][2].buf = MAP_FAILED;
    v4l.in_bufs[8][2].fd = -1;
    v4l.in_bufs[9][0].buf = MAP_FAILED;
    v4l.in_bufs[9][0].fd = -1;
    v4l.in_bufs[9][1].buf = MAP_FAILED;
    v4l.in_bufs[9][1].fd = -1;
    v4l.in_bufs[9][2].buf = MAP_FAILED;
    v4l.in_bufs[9][2].fd = -1;
    v4l.out_bufs[0].buf = MAP_FAILED;
    v4l.out_bufs[0].fd = -1;
    v4l.out_bufs[1].buf = MAP_FAILED;
    v4l.out_bufs[1].fd = -1;
    v4l.out_bufs[2].buf = MAP_FAILED;
    v4l.out_bufs[2].fd = -1;
    v4l.out_bufs[3].buf = MAP_FAILED;
    v4l.out_bufs[3].fd = -1;
    v4l.out_bufs[4].buf = MAP_FAILED;
    v4l.out_bufs[4].fd = -1;
    v4l.out_bufs[5].buf = MAP_FAILED;
    v4l.out_bufs[5].fd = -1;
    v4l.in_bufs_count = V4L_MAX_IN_BUFS;
    v4l.out_bufs_count = V4L_MAX_OUT_BUFS;

    v4l.dev_id = -1;

    ASSERT_INT(argc, !=, 4, cleanup);
    v4l.output_file_path = argv[3];
    v4l.video_width = atoi(argv[1]);
    v4l.video_height = atoi(argv[2]);

    //v4l.output_file = new ofstream(v4l.output_file_path);
    //CHECK_ERROR(!v4l.output_file->is_open(), "Error in opening output file", cleanup);

    CALL(v4l.dev_id = v4l2_open(ENCODER_DEV, O_RDWR));
    struct v4l2_capability cap;
    CALL(v4l2_ioctl(v4l.dev_id, VIDIOC_QUERYCAP, &cap));
    ASSERT_INT((cap.capabilities & V4L2_CAP_VIDEO_M2M_MPLANE), ==, 0, cleanup);
    ASSERT_INT((cap.capabilities & V4L2_CAP_STREAMING), ==, 0, cleanup);

    struct v4l2_format fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    fmt.fmt.pix_mp.width = v4l.video_width;
    fmt.fmt.pix_mp.height = v4l.video_height;
    fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_H264;
    fmt.fmt.pix_mp.num_planes = 1;
    CALL(v4l2_ioctl(v4l.dev_id, VIDIOC_S_FMT, &fmt));
    v4l.out_sizeimages[0] = fmt.fmt.pix_mp.plane_fmt[0].sizeimage;
    v4l.out_strides[0] = fmt.fmt.pix_mp.plane_fmt[0].bytesperline;
    DEBUG("Out plane - size: %d, stride: %d", v4l.out_sizeimages[0], v4l.out_strides[0]);

    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    fmt.fmt.pix_mp.width = v4l.video_width;
    fmt.fmt.pix_mp.height = v4l.video_height;
    fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_YUV444M;
    fmt.fmt.pix_mp.num_planes = 3;
    CALL(v4l2_ioctl(v4l.dev_id, VIDIOC_S_FMT, &fmt));
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
    V4L_CALL(v4l2_ioctl(v4l.dev_id, VIDIOC_REQBUFS, &req));
    ASSERT_INT(req.count, >, V4L_MAX_IN_BUFS, cleanup);
    ASSERT_INT(req.count, <=, 0, cleanup);
    DEBUG("v4l.in_bufs_count before: %d", v4l.in_bufs_count);
    v4l.in_bufs_count = req.count;
    DEBUG("v4l.in_bufs_count after: %d", v4l.in_bufs_count);

    struct v4l2_buffer buf;
    struct v4l2_plane planes[3];
    struct v4l2_exportbuffer expbuf;
    for (uint32_t i = 0; i < v4l.in_bufs_count; ++i)
    {
        memset(&buf, 0, sizeof(buf));
        memset(planes, 0, sizeof(planes));
        buf.index = i;
        buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.m.planes = planes;
        buf.length = 3;
        V4L_CALL(v4l2_ioctl(v4l.dev_id, VIDIOC_QUERYBUF, &buf));
        for (int j = 0; j < buf.length; j++) {
            v4l.in_bufs[i][j].len = buf.m.planes[j].length;
            v4l.in_bufs[i][j].offset = buf.m.planes[j].m.mem_offset;
            if (i == 0) {
                DEBUG("In plane[%d] - offset: %d, length: %d", j,
                    v4l.in_bufs[i][j].offset, v4l.in_bufs[i][j].len);
            }
        }

        memset(&expbuf, 0, sizeof(expbuf));
        expbuf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
        expbuf.index = i;
        for (int j = 0; j < 3; j++) {
            expbuf.plane = j;
            V4L_CALL(v4l2_ioctl(v4l.dev_id, VIDIOC_EXPBUF, &expbuf));
            v4l.in_bufs[i][j].fd = expbuf.fd;
            //DEBUG("In plane[%d] - fd: %d", j, v4l.in_bufs[i][j].fd);
        }
        for (int j = 0; j < 3; j++) {
            v4l.in_bufs[i][j].buf = mmap(NULL,
                v4l.in_bufs[i][j].len,
                PROT_READ | PROT_WRITE,
                MAP_SHARED,
                v4l.in_bufs[i][j].fd,
                v4l.in_bufs[i][j].offset
            );
            //DEBUG("In plane[%d] - buf: %p", j, v4l.in_bufs[i][j].buf);
            if (v4l.in_bufs[i][j].buf == MAP_FAILED) {
                CALL_MESSAGE(mmap);
            }
        }
        /*if (v4l.outplane_buffers[i]->map())
        {
            cerr << "Buffer mapping error on output plane" << endl;
            v4l.in_error = 1;
            goto cleanup;
        }*/
    }

    memset(&req, 0, sizeof(req));
    req.count = v4l.out_bufs_count;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    req.memory = V4L2_MEMORY_MMAP;
    V4L_CALL(v4l2_ioctl(v4l.dev_id, VIDIOC_REQBUFS, &req));
    ASSERT_INT(req.count, >, V4L_MAX_OUT_BUFS, cleanup);
    ASSERT_INT(req.count, <=, 0, cleanup);
    DEBUG("v4l.out_bufs_count before: %d", v4l.out_bufs_count);
    v4l.out_bufs_count = req.count;
    DEBUG("v4l.out_bufs_count after: %d", v4l.out_bufs_count);

    for (int i = 0; i < v4l.out_bufs_count; i++) {
        memset(&buf, 0, sizeof(buf));
        memset(planes, 0, sizeof(planes));
        buf.index = i;
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.m.planes = planes;
        buf.length = 1;
        V4L_CALL(v4l2_ioctl(v4l.dev_id, VIDIOC_QUERYBUF, &buf), cleanup);
        v4l.out_bufs[i].len = buf.m.planes[0].length;
        v4l.out_bufs[i].offset = buf.m.planes[0].m.mem_offset;
        if (i == 0) {
            DEBUG("Out plane[%d] - offset: %d, length: %d", 0,
                v4l.out_bufs[i].offset, v4l.out_bufs[i].len);
        }
        memset(&expbuf, 0, sizeof(expbuf));
        expbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        expbuf.index = i;
        expbuf.plane = 0;
        V4L_CALL(v4l2_ioctl(v4l.dev_id, VIDIOC_EXPBUF, &expbuf), cleanup);
        v4l.out_bufs[i].fd = expbuf.fd;

        v4l.out_bufs[i].buf = mmap(NULL,
            v4l.out_bufs[i].len,
            PROT_READ | PROT_WRITE,
            MAP_SHARED,
            v4l.out_bufs[i].fd,
            v4l.out_bufs[i].offset
        );
        if (v4l.out_bufs[i].buf == MAP_FAILED) {
            CALL_MESSAGE(mmap);
            goto cleanup;
        }
    }

    enum v4l2_buf_type type;
    type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    V4L_CALL(v4l2_ioctl(v4l.dev_id, VIDIOC_STREAMON, &type), cleanup);

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    V4L_CALL(v4l2_ioctl(v4l.dev_id, VIDIOC_STREAMON, &type), cleanup);

    struct v4l2_buffer in_buf;
    struct v4l2_plane in_planes[3];
    DEBUG("v4l.out_bufs_count: %d", v4l.out_bufs_count);
    for (uint32_t i = 0; i < v4l.out_bufs_count; ++i) {
        memset(&in_buf, 0, sizeof(in_buf));
        memset(in_planes, 0, sizeof(in_planes));
        in_buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
        in_buf.memory = V4L2_MEMORY_MMAP;
        in_buf.m.planes = in_planes;
        in_buf.length = 3;
        in_buf.index = i;
        in_buf.m.planes[0].bytesused = v4l.in_strides[0] * v4l.video_height;
        in_buf.m.planes[1].bytesused = v4l.in_strides[1] * v4l.video_height;
        in_buf.m.planes[2].bytesused = v4l.in_strides[2] * v4l.video_height;
        DEBUG("bytesused: %d, %d, %d", in_buf.m.planes[0].bytesused, in_buf.m.planes[1].bytesused,
            in_buf.m.planes[2].bytesused);

        for (int j = 0; j < 3; j++) {
            CALL(NvBufferMemSyncForDevice(v4l.in_bufs[i][j].fd, j, (void **)&v4l.in_bufs[i][j].buf),
                cleanup);
        }
        V4L_CALL(v4l2_ioctl(v4l.dev_id, VIDIOC_QBUF, &in_buf), cleanup);
    }

    DEBUG("encoder_process_blocking");
    // Dequeue and queue loop on output plane.
    encoder_process_blocking();

cleanup:
    if (v4l.dev_id != -1)
    {
        enum v4l2_buf_type type;
        type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
        v4l2_ioctl(v4l.dev_id, VIDIOC_STREAMOFF, &type);
        type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        v4l2_ioctl(v4l.dev_id, VIDIOC_STREAMOFF, &type);

        /*for (uint32_t i = 0; i < v4l.outplane_num_buffers; ++i)
        {
            v4l.outplane_buffers[i]->unmap();
        }
        for (uint32_t i = 0; i < v4l.capplane_num_buffers; ++i)
        {
            v4l.capplane_buffers[i]->unmap();
        }*/

        v4l2_close(v4l.dev_id);
    }

    //v4l.output_file->close();
    //delete v4l.output_file;

    return 0;
}
