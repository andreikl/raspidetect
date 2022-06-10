#include "main.h"
#include "utils.h"

#include "v4l_encoder.h"

#include "linux/videodev2.h"
#include "sys/mman.h"

// wrapper to allow to run test on platform where h264 Jetson encoder isn't available
#ifdef V4L_ENCODER_WRAP
    int NvBufferMemSyncForDevice(int dmabuf_fd, unsigned int plane, void **pVirtAddr)
    {
        DEBUG("wrap NvBufferMemSyncForDevice, fd: %d, plane: %d", dmabuf_fd, plane);
        return 0;
    }
#endif

static struct format_mapping_t v4l_input_formats[] = {
    {
        .format = VIDEO_FORMAT_YUV422,
        .internal_format = V4L2_PIX_FMT_YUV444M,
        .is_supported = 0
    }
};
static struct format_mapping_t v4l_output_formats[] = {
    {
        .format = VIDEO_FORMAT_H264,
        .internal_format = V4L2_PIX_FMT_H264,
        .is_supported = 0
    }
};

static struct format_mapping_t *v4l_input_format = NULL;
static struct format_mapping_t *v4l_output_format = NULL;
static struct v4l_encoder_state_t v4l = {
    .app = NULL,
    .dev_id = -1,
    .in_bufs = {
        REP(0, 0, 10, { {MAP_FAILED, -1}, {MAP_FAILED, -1}, {MAP_FAILED, -1} })
    },
    .out_bufs = { REP(0, 0, 3, { MAP_FAILED, -1 }) },
    .in_bufs_count = V4L_MAX_IN_BUFS,
    .out_bufs_count = V4L_MAX_OUT_BUFS,
    .in_curr_buf = 0,
    .out_buf = NULL,
    .out_buf_used = 0
};

extern struct filter_t filters[MAX_FILTERS];

static int ioctl_enum(int fd, int request, void *arg)
{
    int res = v4l2_ioctl(fd, request, arg);
    if (res != 0) {
        if (errno == EINVAL) {
            res = -2;
        } else 
            res = -1;
    } 
    return res;
}

static int ioctl_again(int fd, int request, void *arg)
{
    int res = v4l2_ioctl(fd, request, arg);
    if (res == -1) {
        if (errno == EAGAIN) {
            res = -2;
        } else 
            res = -1;
    } 
    return res;
}

static void v4l2_clean_memory() {
    if (v4l.out_bufs[0].buf != MAP_FAILED) {
        CALL(munmap(v4l.out_bufs[0].buf, v4l.out_lengths[0]));
        v4l.out_bufs[0].buf = MAP_FAILED;
    }
    for (int i = 0; i < V4L_MAX_IN_BUFS; i++)
        for (int j = 0; i < 3; i++) {
            if (v4l.in_bufs[i][j].buf != MAP_FAILED) {
                CALL(munmap(v4l.in_bufs[i][j].buf, v4l.in_lengths[j]));
                v4l.in_bufs[i][j].buf = MAP_FAILED;
            }
        }
}

static void v4l_cleanup()
{
    if (v4l.dev_id != -1) {
        CALL(close(v4l.dev_id));
        v4l.dev_id = -1;
    }

    if (v4l.out_buf) {
        free(v4l.out_buf);
    }

    v4l2_clean_memory();
}

static int v4l_is_supported_resolution(int format)
{
    int res = 0, is_found = 1;
    struct v4l2_frmsizeenum frmsize = {
        .pixel_format = format, .index = 0
    };

    CALL(res = ioctl_enum(v4l.dev_id, VIDIOC_ENUM_FRAMESIZES, &frmsize), cleanup);
    while (res >= 0) {
        if (frmsize.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
            if (
                frmsize.discrete.width == v4l.app->video_width &&
                frmsize.discrete.height == v4l.app->video_height
            ) {
                is_found = 1;
            }
            if (
                frmsize.discrete.width > v4l.app->camera_max_width || 
                frmsize.discrete.height > v4l.app->camera_max_height
            ) {
                v4l.app->camera_max_width = frmsize.discrete.width;
                v4l.app->camera_max_height = frmsize.discrete.height;
            }
            // DEBUG("V4L2_FRMSIZE_TYPE_DISCRETE %dx%d",
            //     frmsize.discrete.width, frmsize.discrete.height);
        }
        else if (
            frmsize.type == V4L2_FRMSIZE_TYPE_STEPWISE ||
            frmsize.type == V4L2_FRMSIZE_TYPE_CONTINUOUS
        ) {
            if (
                v4l.app->video_width % frmsize.stepwise.step_width == 0 &&
                v4l.app->video_height % frmsize.stepwise.step_height == 0 &&
                v4l.app->video_width >= frmsize.stepwise.min_width &&
                v4l.app->video_height >= frmsize.stepwise.min_height &&
                v4l.app->video_width <= frmsize.stepwise.max_width &&
                v4l.app->video_height <= frmsize.stepwise.max_height
            ) {
                is_found = 1;
            }
            if (
                frmsize.stepwise.max_width > v4l.app->camera_max_width || 
                frmsize.stepwise.max_height > v4l.app->camera_max_height
            ) {
                v4l.app->camera_max_width = frmsize.stepwise.max_width;
                v4l.app->camera_max_height = frmsize.stepwise.max_height;
            }
            // DEBUG("V4L2_FRMSIZE_TYPE_STEPWISE %dx%d",
            //     frmsize.stepwise.max_width, frmsize.stepwise.max_height);
        }
        frmsize.index++;
        CALL(res = ioctl_enum(v4l.dev_id, VIDIOC_ENUM_FRAMESIZES, &frmsize), cleanup);
    }
    return is_found;

cleanup:
    if (errno == 0)
        errno = EAGAIN;
    return -1;
}

static int v4l_init()
{
    ASSERT_INT(v4l.dev_id, ==, -1, cleanup);

    int res = 0;
    sprintf(v4l.dev_name, V4L_H264_ENCODER);

    struct stat st;
    CALL(stat(v4l.dev_name, &st), cleanup);
    ASSERT_INT(S_ISCHR(st.st_mode), !=, 0, cleanup);

    // O_NONBLOCK - nv sample opens in block mode
    CALL(v4l.dev_id = v4l2_open(v4l.dev_name, O_RDWR), cleanup);
    struct v4l2_capability cap;
    GEN_CALL(v4l2_ioctl(v4l.dev_id, VIDIOC_QUERYCAP, &cap), cleanup);
    //DEBUG("cap.capabilities: %d", cap.capabilities);

    ASSERT_INT((cap.capabilities & V4L2_CAP_VIDEO_M2M_MPLANE), !=, 0, cleanup);
    ASSERT_INT((cap.capabilities & V4L2_CAP_STREAMING), !=, 0, cleanup);

    int is_found = 0;
    int formats_len = ARRAY_SIZE(v4l_input_formats);
    struct v4l2_fmtdesc fmt = {
        .index = 0, .type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE
    };
    CALL(res = ioctl_enum(v4l.dev_id, VIDIOC_ENUM_FMT, &fmt), cleanup);
    while (res >= 0 && !is_found) {
        // DEBUG("pixelformat %c %c %c %c", GET_B(fmt.pixelformat),
        //     GET_G(fmt.pixelformat), GET_R(fmt.pixelformat), GET_A(fmt.pixelformat));
        for (int i = 0; i < formats_len; i++) {
            struct format_mapping_t *f = v4l_input_formats + i;
            if (f->internal_format == fmt.pixelformat) {
                CALL(
                    f->is_supported = v4l_is_supported_resolution(fmt.pixelformat),
                    cleanup);

                if (f->is_supported) {
                    if (v4l.app->verbose)
                        DEBUG("%s(%c%c%c%c) and resolution(%dx%d) have been found!",
                            "encoder input pixel format",
                            GET_B(fmt.pixelformat),
                            GET_G(fmt.pixelformat),
                            GET_R(fmt.pixelformat),
                            GET_A(fmt.pixelformat),
                            v4l.app->video_width,
                            v4l.app->video_height);
                    is_found = 1;
                }
                break;
            }
        }

        fmt.index++;
        CALL(res = ioctl_enum(v4l.dev_id, VIDIOC_ENUM_FMT, &fmt), cleanup);
    }
    if (!is_found) {
        errno = EOPNOTSUPP;
        return -1;
    }

    is_found = 0;
    formats_len = ARRAY_SIZE(v4l_output_formats);
    fmt.index = 0;
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    CALL(res = ioctl_enum(v4l.dev_id, VIDIOC_ENUM_FMT, &fmt), cleanup);
    while (res >= 0 && !is_found) {
        // DEBUG("pixelformat %c %c %c %c", GET_B(fmt.pixelformat),
        //     GET_G(fmt.pixelformat), GET_R(fmt.pixelformat), GET_A(fmt.pixelformat));
        for (int i = 0; i < formats_len; i++) {
            struct format_mapping_t *f = v4l_output_formats + i;
            if (f->internal_format == fmt.pixelformat) {
                CALL(
                    f->is_supported = v4l_is_supported_resolution(fmt.pixelformat),
                    cleanup);

                if (f->is_supported) {
                    if (v4l.app->verbose)
                        DEBUG("%s(%c%c%c%c) and resolution(%dx%d) have been found!",
                            "encoder output pixel format",
                            GET_B(fmt.pixelformat),
                            GET_G(fmt.pixelformat),
                            GET_R(fmt.pixelformat),
                            GET_A(fmt.pixelformat),
                            v4l.app->video_width,
                            v4l.app->video_height);
                    is_found = 1;
                }
                break;
            }
        }

        fmt.index++;
        CALL(res = ioctl_enum(v4l.dev_id, VIDIOC_ENUM_FMT, &fmt), cleanup);

        v4l.out_buf = malloc(V4L_OUT_BUFFER_SIZE);
        if (!v4l.out_buf) {
            CALL_MESSAGE(malloc(V4L_OUT_BUFFER_SIZE));
            goto cleanup;
        }
    }
    if (!is_found) {
        errno = EOPNOTSUPP;
        return -1;
    }
    return 0;

cleanup:
    v4l_cleanup();
    if (errno == 0)
        errno = EAGAIN;
    return -1;
}

static int v4l_start(int input_format, int output_format)
{
    ASSERT_PTR(v4l_input_format, ==, NULL, cleanup);
    int formats_len = ARRAY_SIZE(v4l_input_formats);
    for (int i = 0; i < formats_len; i++) {
        struct format_mapping_t *f = v4l_input_formats + i;
        if (f->format == input_format && f->is_supported) {
            v4l_input_format = f;
            break;
        }
    }
    ASSERT_PTR(v4l_input_format, !=, NULL, cleanup);

    ASSERT_PTR(v4l_output_format, ==, NULL, cleanup);
    formats_len = ARRAY_SIZE(v4l_output_formats);
    for (int i = 0; i < formats_len; i++) {
        struct format_mapping_t *f = v4l_output_formats + i;
        if (f->format == output_format && f->is_supported) {
            v4l_output_format = f;
            break;
        }
    }
    ASSERT_PTR(v4l_output_format, !=, NULL, cleanup);

    struct v4l2_format fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    fmt.fmt.pix_mp.width = v4l.app->video_width;
    fmt.fmt.pix_mp.height = v4l.app->video_height;
    fmt.fmt.pix_mp.pixelformat = v4l_output_format->internal_format;
    fmt.fmt.pix_mp.num_planes = 1;
    GEN_CALL(v4l2_ioctl(v4l.dev_id, VIDIOC_S_FMT, &fmt), cleanup);
    v4l.out_sizeimages[0] = fmt.fmt.pix_mp.plane_fmt[0].sizeimage;
    v4l.out_strides[0] = fmt.fmt.pix_mp.plane_fmt[0].bytesperline;

    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    fmt.fmt.pix_mp.width = v4l.app->video_width;
    fmt.fmt.pix_mp.height = v4l.app->video_height;
    fmt.fmt.pix_mp.pixelformat = v4l_input_format->internal_format;
    fmt.fmt.pix_mp.num_planes = 3;
    // DEBUG("fmt.type[%d]", fmt.type);
    // DEBUG("fmt.fmt.pix_mp.width[%d]", fmt.fmt.pix_mp.width);
    // DEBUG("fmt.fmt.pix_mp.height[%d]", fmt.fmt.pix_mp.height);
    // DEBUG("fmt.fmt.pix_mp.pixelformat[%d]", fmt.fmt.pix_mp.pixelformat);
    // DEBUG("fmt.fmt.pix_mp.num_planes[%d]", fmt.fmt.pix_mp.num_planes);
    GEN_CALL(v4l2_ioctl(v4l.dev_id, VIDIOC_S_FMT, &fmt), cleanup);
    for (int i = 0; i < fmt.fmt.pix_mp.num_planes; i++) {
        v4l.in_sizeimages[i] = fmt.fmt.pix_mp.plane_fmt[i].sizeimage;
        v4l.in_strides[i] = fmt.fmt.pix_mp.plane_fmt[i].bytesperline;
    }
    //https://docs.nvidia.com/jetson/l4t-multimedia/group__V4L2Enc.html
    // only V4L2_MEMORY_MMAP is supported
    // MEMORY               OUTPUT PLANE 	CAPTURE PLANE
    // V4L2_MEMORY_MMAP     Y           	    Y
    // V4L2_MEMORY_DMABUF   Y           	    N
    // V4L2_MEMORY_USERPTR  N           	    N
    struct v4l2_requestbuffers req;
    memset(&req, 0, sizeof(req));
    req.count = v4l.in_bufs_count;
    req.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    req.memory = V4L2_MEMORY_MMAP;
    GEN_CALL(v4l2_ioctl(v4l.dev_id, VIDIOC_REQBUFS, &req), cleanup);
    ASSERT_INT(req.count, <=, V4L_MAX_IN_BUFS, cleanup);
    ASSERT_INT(req.count, >, 0, cleanup);
    DEBUG("v4l.in_bufs_count: %d", req.count);
    v4l.in_bufs_count = req.count;

    struct v4l2_buffer buf;
    struct v4l2_plane planes[3];
    struct v4l2_exportbuffer expbuf;
    for (int i = 0; i < v4l.in_bufs_count; i++) {
        memset(&buf, 0, sizeof(buf));
        memset(planes, 0, sizeof(planes));
        buf.index = i;
        buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.m.planes = planes;
        buf.length = 3;
        GEN_CALL(v4l2_ioctl(v4l.dev_id, VIDIOC_QUERYBUF, &buf), cleanup);

        if (i == 0) {
            for (int j = 0; j < buf.length; j++) {
                v4l.in_lengths[j] = buf.m.planes[j].length;
                v4l.in_offsets[j] = buf.m.planes[j].m.mem_offset;
            }
        }

        memset(&expbuf, 0, sizeof(expbuf));
        expbuf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
        expbuf.index = i;
        for (int j = 0; j < 3; j++) {
            expbuf.plane = j;
            GEN_CALL(v4l2_ioctl(v4l.dev_id, VIDIOC_EXPBUF, &expbuf), cleanup);
            v4l.in_bufs[i][j].fd = expbuf.fd;
        }
        for (int j = 0; j < 3; j++) {
            v4l.in_bufs[i][j].buf = mmap(NULL,
                v4l.in_lengths[j],
                PROT_READ | PROT_WRITE,
                MAP_SHARED,
                v4l.in_bufs[i][j].fd,
                v4l.in_offsets[j]
            );
            if (v4l.in_bufs[i][j].buf == MAP_FAILED) {
                CALL_MESSAGE(mmap);
                goto cleanup;
            }
        }
    }

    DEBUG("In Planes: %d - sizes: %d, %d, %d - strides: %d, %d, %d",
        fmt.fmt.pix_mp.num_planes,
        v4l.in_sizeimages[0], v4l.in_sizeimages[1], v4l.in_sizeimages[2],
        v4l.in_strides[0], v4l.in_strides[1], v4l.in_strides[2]);

    DEBUG("In Planes: %d - offsets: %d, %d, %d - sizes: %d, %d, %d",
        fmt.fmt.pix_mp.num_planes,
        v4l.in_offsets[0], v4l.in_offsets[1], v4l.in_offsets[2],
        v4l.in_lengths[0], v4l.in_lengths[1], v4l.in_lengths[2]);

    memset(&req, 0, sizeof(req));
    req.count = v4l.out_bufs_count;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    req.memory = V4L2_MEMORY_MMAP;
    GEN_CALL(v4l2_ioctl(v4l.dev_id, VIDIOC_REQBUFS, &req), cleanup);
    ASSERT_INT(req.count, <=, V4L_MAX_OUT_BUFS, cleanup);
    ASSERT_INT(req.count, >, 0, cleanup);
    DEBUG("v4l.out_bufs_count: %d", req.count);
    v4l.out_bufs_count = req.count;

    for (int i = 0; i < v4l.out_bufs_count; i++) {
        memset(&buf, 0, sizeof(buf));
        memset(planes, 0, sizeof(planes));
        buf.index = i;
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.m.planes = planes;
        buf.length = 1;
        GEN_CALL(v4l2_ioctl(v4l.dev_id, VIDIOC_QUERYBUF, &buf), cleanup);
        if (i == 0) {
            v4l.out_lengths[0] = buf.m.planes[0].length;
            v4l.out_offsets[0] = buf.m.planes[0].m.mem_offset;
        }
        memset(&expbuf, 0, sizeof(expbuf));
        expbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        expbuf.index = i;
        expbuf.plane = 0;
        GEN_CALL(v4l2_ioctl(v4l.dev_id, VIDIOC_EXPBUF, &expbuf), cleanup);
        v4l.out_bufs[i].fd = expbuf.fd;

        v4l.out_bufs[i].buf = mmap(NULL,
            v4l.out_lengths[0],
            PROT_READ | PROT_WRITE,
            MAP_SHARED,
            v4l.out_bufs[i].fd,
            v4l.out_offsets[0]
        );
        if (v4l.out_bufs[i].buf == MAP_FAILED) {
            CALL_MESSAGE(mmap);
            goto cleanup;
        }
    }

    DEBUG("Out plane - size: %d, stride: %d", v4l.out_sizeimages[0], v4l.out_strides[0]);
    DEBUG("Out plane - offset: %d, length: %d", v4l.out_offsets[0], v4l.out_lengths[0]);

    enum v4l2_buf_type type;
    type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    GEN_CALL(v4l2_ioctl(v4l.dev_id, VIDIOC_STREAMON, &type), cleanup);

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    GEN_CALL(v4l2_ioctl(v4l.dev_id, VIDIOC_STREAMON, &type), cleanup);

    struct v4l2_buffer out_buf;
    struct v4l2_plane out_plane;
    for (int i = 0; i < v4l.out_bufs_count; i++) {
        memset(&out_buf, 0, sizeof(out_buf));
        memset(&out_plane, 0, sizeof(out_plane));
        out_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        out_buf.memory = V4L2_MEMORY_MMAP;
        out_buf.index = i;
        out_buf.length = 1;
        out_buf.m.planes = &out_plane;
        GEN_CALL(v4l2_ioctl(v4l.dev_id, VIDIOC_QBUF, &out_buf), cleanup);
    }

    return 0;

cleanup:
    v4l_input_format = NULL;
    v4l_output_format = NULL;

    v4l2_clean_memory();

    if (errno == 0)
        errno = EAGAIN;
    return -1;
}

static int v4l_is_started()
{
    return v4l_input_format != NULL && v4l_output_format != NULL? 1: 0;
}

static int v4l_process_frame(uint8_t *buffer)
{
    ASSERT_PTR(v4l_input_format, !=, NULL, cleanup);
    ASSERT_PTR(v4l_output_format, !=, NULL, cleanup);

    int res = 0;
    struct v4l2_buffer in_buf;
    struct v4l2_plane in_planes[3];
    memset(&in_buf, 0, sizeof(in_buf));
    memset(in_planes, 0, sizeof(in_planes));
    in_buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    in_buf.memory = V4L2_MEMORY_MMAP;
    in_buf.length = 3;
    in_buf.m.planes = in_planes;

    struct v4l2_buffer out_buf;
    struct v4l2_plane out_plane;
    memset(&out_buf, 0, sizeof(out_buf));
    memset(&out_plane, 0, sizeof(out_plane));
    out_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    out_buf.memory = V4L2_MEMORY_MMAP;
    out_buf.length = 1;
    out_buf.m.planes = &out_plane;

    int cb = v4l.in_curr_buf + 1;
    if (cb >= v4l.in_bufs_count) {
        GEN_CALL(v4l2_ioctl(v4l.dev_id, VIDIOC_DQBUF, &in_buf), cleanup);
        cb = in_buf.index;
    }
    else
        v4l.in_curr_buf++;

    DEBUG("queue input frame, current buf %d", cb);
    uint8_t *yi = v4l.in_bufs[cb][0].buf;
    uint8_t *ui = v4l.in_bufs[cb][1].buf;
    uint8_t *vi = v4l.in_bufs[cb][2].buf;
    for (int i = 0, y = 0; y < v4l.app->video_height; y++) {
        for (int x = 0; x < v4l.app->video_width; x += 2, i += 4) {
            uint8_t y0 = buffer[i];
            uint8_t u01 = buffer[i + 1];
            uint8_t y1 = buffer[i + 2];
            uint8_t v01 = buffer[i + 3];
            yi[x] = y0;
            ui[x] = u01 & 0xF;
            vi[x] = v01 & 0xF;
            yi[x + 1] = y1;
            ui[x + 1] = (u01 & 0xF0) >> 4;
            vi[x + 1] = (v01 & 0xF0) >> 4;
        }
        yi = v4l.in_bufs[cb][0].buf + v4l.in_strides[0] * y;
        ui = v4l.in_bufs[cb][1].buf + v4l.in_strides[1] * y;
        vi = v4l.in_bufs[cb][2].buf + v4l.in_strides[2] * y;
    }

    in_buf.index = cb;
    in_buf.m.planes[0].bytesused = v4l.in_strides[0] * v4l.app->video_height;
    in_buf.m.planes[1].bytesused = v4l.in_strides[1] * v4l.app->video_height;
    in_buf.m.planes[2].bytesused = v4l.in_strides[2] * v4l.app->video_height;
    for (int i = 0; i < 3; i++) {
        CALL(NvBufferMemSyncForDevice(v4l.in_bufs[cb][i].fd, i, (void **)&v4l.in_bufs[cb][i].buf),
            cleanup);
    }
    GEN_CALL(v4l2_ioctl(v4l.dev_id, VIDIOC_QBUF, &in_buf), cleanup);
   
    CALL(res = ioctl_again(v4l.dev_id, VIDIOC_DQBUF, &out_buf), cleanup);
    if (res != -2) {
        ASSERT_INT(out_buf.m.planes[0].bytesused, !=, 0, cleanup);
        CALL(NvBufferMemSyncForDevice(
            v4l.out_bufs[out_buf.index].fd,
            0,
            (void **)&v4l.out_bufs[out_buf.index].buf
        ), cleanup);

        ASSERT_INT(v4l.out_buf_used + out_buf.m.planes[0].bytesused,
            <,
            V4L_OUT_BUFFER_SIZE,
            cleanup);
        
        memcpy(v4l.out_buf, v4l.out_bufs[out_buf.index].buf, out_buf.m.planes[0].bytesused);
        v4l.out_buf_used += out_buf.m.planes[0].bytesused;

        /*char buffer[MAX_STRING];
        static int index = 0;
        sprintf(buffer, "data%d", index++);
        utils_write_file(buffer, v4l.out_bufs[out_buf.index].buf, out_buf.m.planes[0].bytesused);*/

        out_buf.m.planes[0].bytesused = 0;
        GEN_CALL(v4l2_ioctl(v4l.dev_id, VIDIOC_QBUF, &out_buf), cleanup);
    }
    return 0;

cleanup:
    if (errno == 0)
        errno = EAGAIN;
    return -1;
}

static int v4l_stop()
{
    v4l_input_format = NULL;
    v4l_output_format = NULL;

    v4l2_clean_memory();

    return 0;
}

static uint8_t *v4l_get_buffer(int *in_format, int *out_format, int *length)
{
    ASSERT_PTR(v4l_input_format, !=, NULL, cleanup);
    ASSERT_PTR(v4l_output_format, !=, NULL, cleanup);
    if (in_format)
        *in_format = v4l_input_format->format;
    if (out_format)
        *out_format = v4l_output_format->format;
    if (length)
        *length = v4l.out_buf_used;
    v4l.out_buf_used = 0;
    return v4l.out_buf;

cleanup:
    if (errno == 0)
        errno = EOPNOTSUPP;
    return (uint8_t *)-1;
}

static int v4l_get_in_formats(const struct format_mapping_t *formats[])
{
    if (v4l_input_formats != NULL)
        *formats = v4l_input_formats;
    return ARRAY_SIZE(v4l_input_formats);
}

static int v4l_get_out_formats(const struct format_mapping_t *formats[])
{
    if (v4l_output_formats != NULL)
        *formats = v4l_output_formats;
    return ARRAY_SIZE(v4l_output_formats);
}

void v4l_encoder_construct(struct app_state_t *app)
{
    int i = 0;
    while (i < MAX_FILTERS && filters[i].context != NULL)
        i++;

    if (i != MAX_FILTERS) {
        v4l.app = app;

        filters[i].name = "v4l_encoder";
        filters[i].context = &v4l;
        filters[i].init = v4l_init;
        filters[i].cleanup = v4l_cleanup;
        filters[i].start = v4l_start;
        filters[i].is_started = v4l_is_started;
        filters[i].stop = v4l_stop;
        filters[i].process_frame = v4l_process_frame;

        filters[i].get_buffer = v4l_get_buffer;
        filters[i].get_in_formats = v4l_get_in_formats;
        filters[i].get_out_formats = v4l_get_out_formats;
    }
}

