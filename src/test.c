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
#include "app.h"

#include <stdarg.h> //va_list
#include <setjmp.h> //jmp_buf
#include <cmocka.h>

#define TEST_DEBUG(format, ...) \
{ \
    /*fprintf(stderr, "INFO: %s, "#format"\n", __FUNCTION__, ##__VA_ARGS__);*/ \
}

#ifdef V4L
    #include "linux/videodev2.h"

    KHASH_MAP_INIT_STR(argvs_hash_t, char*)
    KHASH_T(argvs_hash_t) *h;

    int is_abort;

    struct input_t input;
    struct filter_t filters[MAX_FILTERS];
    struct output_t outputs[MAX_OUTPUTS];

    int __wrap___xstat(int __ver, const char *__filename, struct stat *__stat_buf)
    {
        TEST_DEBUG("stat");
        __stat_buf->st_mode = __S_IFCHR;
        return 0;
    }

    int __wrap_open(const char *__file, int __oflag, ...)
    {
        TEST_DEBUG("open");
        return 1;
    }

    int __wrap_close(int fd)
    {
        TEST_DEBUG("close");
        return 0;
    }

    int __wrap_ioctl(int fd, int request, void *arg)
    {
        if (request == (int)VIDIOC_QUERYCAP) {
            TEST_DEBUG("request: %s", "VIDIOC_QUERYCAP");
            struct v4l2_capability *cap = arg;
            strncpy((char *)cap->card, "test", 32);
            cap->capabilities = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING;
            return 0;
        }
        else if (request == (int)VIDIOC_ENUM_FMT) {
            TEST_DEBUG("request: %s", "VIDIOC_ENUM_FMT");
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
            TEST_DEBUG("request: %s", "VIDIOC_ENUM_FRAMESIZES");
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
            TEST_DEBUG("request: %d", request);
        }
        errno = EAGAIN;
        return -1;
    }

    int __wrap_v4l2_open(const char *__file, int __oflag, ...)
    {
        TEST_DEBUG("v4l2_open");
        return 1;
    }

    int __wrap_v4l2_ioctl(int fd, int request, void *arg)
    {
        if (request == (int)VIDIOC_QUERYCAP) {
            TEST_DEBUG("request: %s", "VIDIOC_QUERYCAP");
            struct v4l2_capability *cap = arg;
            strncpy((char *)cap->card, "test_encoder", 32);
            cap->capabilities = V4L2_CAP_VIDEO_M2M_MPLANE | V4L2_CAP_STREAMING;
            return 0;
        }
        else if (request == (int)VIDIOC_ENUM_FMT) {
            TEST_DEBUG("request: %s", "VIDIOC_ENUM_FMT");
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
            TEST_DEBUG("request %s", "VIDIOC_ENUM_FRAMESIZES");
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
            TEST_DEBUG("request: %d", request);
        }
        errno = EAGAIN;
        return -1;
    }
#endif


#ifdef SDL
#include <SDL.h>
static SDL_Window *sdl_window = (SDL_Window *)1;
int __wrap_SDL_Init(uint32_t flags)
{
    TEST_DEBUG("SDL_Init");
    return 0;
}

SDL_Window *__wrap_SDL_CreateWindow(
    const char *title,
    int x, int y, int w,
    int h, uint32_t flags)
{
    TEST_DEBUG("__wrap_SDL_CreateWindow");
    return sdl_window;
}

void __wrap_SDL_DestroyWindow(uint32_t flags)
{
    TEST_DEBUG("__wrap_SDL_DestroyWindow");
}

#endif

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

    app_set_default_state(&app);
    app_construct(&app);

    int res = app_init(&app);

    TEST_DEBUG("res: %d", res);

    assert_int_not_equal(res, -1);

    app_cleanup(&app);
}

int main(int argc, char **argv)
{
    h = KH_INIT(argvs_hash_t);
    utils_parse_args(argc, argv);

    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_utils_init),
    };
    int res = cmocka_run_group_tests(tests, NULL, NULL);

    KH_DESTROY(argvs_hash_t, h);
    return res;
}
