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

#ifdef V4L_ENCODER
   #include "v4l_encoder.h"
   #include "linux/videodev2.h"
#endif

#define TEST_DEBUG(format, ...) \
{ \
    if (test_verbose) \
        fprintf(stderr, "TEST: %s:%d - %s, "#format"\n", __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__); \
}

#define WRAP_DEBUG(format, ...) \
{ \
    if (wrap_verbose) \
        fprintf(stderr, "WRAP: %s:%d - %s, "#format"\n", __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__); \
}

KHASH_MAP_INIT_STR(argvs_hash_t, char*)
KHASH_T(argvs_hash_t) *h;

int is_abort;
int wrap_verbose;
int test_verbose;

struct app_state_t app;
struct input_t input;
struct filter_t filters[MAX_FILTERS];
struct output_t outputs[MAX_OUTPUTS];

#include "test_wraps.c"
#ifdef V4L_ENCODER
    #include "test_v4l_encoder_wraps.c"
#endif

static int test_setup(void **state)
{
    *state = &app;
    memset(&app, 0, sizeof(app));
    app_set_default_state();
    app_construct();
    return 0;
}

static int test_verbose_true(void **state)
{
    app.verbose = 1;
    test_verbose = 1;
    wrap_verbose = 0;
    return 0;
}

static int test_verbose_false(void **state)
{
    app.verbose = 0;
    test_verbose = 0;
    wrap_verbose = 0;
    return 0;
}

static int test_teardown(void **state)
{
    return 0;
}

static void test_utils_init(void **state)
{
    int res = 0;
    CALL(res = app_init());

    TEST_DEBUG("res: %d", res);
    assert_int_not_equal(res, -1);

    app_cleanup();
}

#include "file.h"
extern struct file_state_t file;
static void test_file_loop(void **state)
{
    int res = 0;
    struct output_t *output = file.output;

    expect_value(__wrap_ioctl, fmt->fmt.pix.pixelformat, V4L2_PIX_FMT_YUYV);
    expect_value(__wrap_ioctl, fmt->fmt.pix.width, app.video_width);
    expect_value(__wrap_ioctl, fmt->fmt.pix.height, app.video_height);
    //will_return(__wrap_ioctl, 3);

    CALL(res = app_init(), error);
    for (int i = 0; i < 15; i++) {
        CALL(res = output->process_frame());
        if (res == -1 && errno != ETIME)
            break;            
        else
            res = 0;
    }

error:
    TEST_DEBUG("res: %d", res);
    assert_int_not_equal(res, -1);

    app_cleanup();
}

#ifdef SDL
#include "sdl.h"
extern struct sdl_state_t sdl;
static void test_sdl_loop(void **state)
{
    int res = 0;
    struct output_t *output = sdl.output;

    expect_value(__wrap_ioctl, fmt->fmt.pix.pixelformat, V4L2_PIX_FMT_YUYV);
    expect_value(__wrap_ioctl, fmt->fmt.pix.width, app.video_width);
    expect_value(__wrap_ioctl, fmt->fmt.pix.height, app.video_height);
    //will_return(__wrap_ioctl, 3);

    CALL(res = app_init(), error);
    for (int i = 0; i < 10; i++) {
        CALL(res = output->process_frame());
        if (res == -1 && errno != ETIME)
            break;            
        else
            res = 0;
    }

error:
    TEST_DEBUG("res: %d", res);
    assert_int_not_equal(res, -1);

    app_cleanup();
}
#endif //SDL

#ifdef RFB
#include "rfb.h"
extern struct rfb_state_t rfb;
static void test_rfb_loop(void **state)
{
    int res = 0;
    struct output_t *output = rfb.output;

    expect_value(__wrap_ioctl, fmt->fmt.pix.pixelformat, V4L2_PIX_FMT_YUYV);
    expect_value(__wrap_ioctl, fmt->fmt.pix.width, app.video_width);
    expect_value(__wrap_ioctl, fmt->fmt.pix.height, app.video_height);
    //will_return(__wrap_ioctl, 3);

    CALL(res = app_init(), error);
    for (int i = 0; i < 10; i++) {
        CALL(res = output->process_frame());
        if (res == -1 && errno != ETIME)
            break;            
        else
            res = 0;
    }

error:
    TEST_DEBUG("res: %d", res);
    assert_int_not_equal(res, -1);

    app_cleanup();
}
#endif //RFB

int main(int argc, char **argv)
{
    h = KH_INIT(argvs_hash_t);
    utils_parse_args(argc, argv);

    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup(test_utils_init, test_verbose_false),
        cmocka_unit_test_setup(test_file_loop, test_verbose_false),
#ifdef SDL
        cmocka_unit_test_setup(test_sdl_loop, test_verbose_false),
#endif //SDL
#ifdef RFB
        cmocka_unit_test_setup(test_rfb_loop, test_verbose_true)
#endif //RFB
    };
    int res = cmocka_run_group_tests(tests, test_setup, test_teardown);

    KH_DESTROY(argvs_hash_t, h);
    return res;
}
