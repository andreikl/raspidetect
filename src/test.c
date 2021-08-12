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

#include "khash.h"
#include "main.h"
#include "utils.h"

#include <stdarg.h> //va_list
#include <setjmp.h> //jmp_buf
#include <cmocka.h>

#include "linux/videodev2.h"

KHASH_MAP_INIT_STR(map_str, char*)
khash_t(map_str) *h;

int is_abort = 0;

struct input_t input;
struct filter_t filters[MAX_OUTPUTS];
struct output_t outputs[MAX_OUTPUTS];

int __wrap___xstat(const char *pathname, struct stat *statbuf)
{
    DEBUG_INT("stat", 1);
    return 0;
}

int __wrap_open(const char *__file, int __oflag, ...)
{
    DEBUG_INT("open", 1);
    return 1;
}

int __wrap_ioctl(int fd, int request, void *arg)
{
    if (request == (int)VIDIOC_QUERYCAP) {
        DEBUG_STR("request", "VIDIOC_QUERYCAP");
        struct v4l2_capability *cap = arg;
        strncpy((char *)cap->card, "test", 32);
        cap->capabilities = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING;
        return 0;
    }
    else if (request == (int)VIDIOC_ENUM_FMT) {
        DEBUG_STR("request", "VIDIOC_ENUM_FMT");
        struct v4l2_fmtdesc *fmt = arg;
        if (fmt->index == 0) {
            fmt->pixelformat = V4L2_PIX_FMT_YUYV;
            return 0;
        }
        else {
            errno = EINVAL;
            return -1;
        }
    }
    else if (request == (int)VIDIOC_ENUM_FRAMESIZES) {
        DEBUG_STR("request", "VIDIOC_ENUM_FRAMESIZES");
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
    else {
        DEBUG_INT("request", request);
    }
    errno = EAGAIN;
    return -1;
}

int __wrap_v4l2_open(const char *__file, int __oflag, ...)
{
    DEBUG_INT("v4l2_open", 1);
    return 1;
}

int __wrap_v4l2_ioctl(int fd, int request, void *arg)
{
    if (request == (int)VIDIOC_QUERYCAP) {
        DEBUG_STR("request", "VIDIOC_QUERYCAP");
        struct v4l2_capability *cap = arg;
        strncpy((char *)cap->card, "test_encoder", 32);
        cap->capabilities = V4L2_CAP_VIDEO_M2M_MPLANE | V4L2_CAP_STREAMING;
        return 0;
    }
    else if (request == (int)VIDIOC_ENUM_FMT) {
        DEBUG_STR("request", "VIDIOC_ENUM_FMT");
        struct v4l2_fmtdesc *fmt = arg;
        if (fmt->index == 0 && fmt->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
            fmt->pixelformat = V4L2_PIX_FMT_YUYV;
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
        DEBUG_STR("request", "VIDIOC_ENUM_FRAMESIZES");
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
    else {
        DEBUG_INT("request", request);
    }
    errno = EAGAIN;
    return -1;
}

static void test_utils_init(void **state)
{
    struct app_state_t app;

    /*char xbuf[9]; // 8 + '\0'

    expect_value(__wrap_read, fd, 1);
    expect_value(__wrap_read, buf, xbuf);
    expect_value(__wrap_read, count, 8);
    will_return(__wrap_read, cast_ptr_to_largest_integral_type("xyz"));
	will_return(__wrap_read, 3);

    expect_value(__wrap_read, fd, 1);
    expect_value(__wrap_read, buf, xbuf+3);
    expect_value(__wrap_read, count,5);
    will_return(__wrap_read, cast_ptr_to_largest_integral_type("54321"));
    will_return(__wrap_read, 5);*/

    utils_set_default_state(&app);
    utils_construct(&app);

    int res = utils_init(&app);
    assert_int_not_equal(res, -1);

    for (int i = 0; outputs[i].context != NULL && i < MAX_OUTPUTS; i++)
        outputs[i].cleanup(&app);
    input.cleanup(&app);
}

int main(int argc, char **argv)
{
    h = kh_init(map_str);
    utils_parse_args(argc, argv);

    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_utils_init),
    };
    int res = cmocka_run_group_tests(tests, NULL, NULL);

    kh_destroy(map_str, h);
    return res;
}
