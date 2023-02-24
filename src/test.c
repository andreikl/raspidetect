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
#include "v4l.h"
#include "v4l_encoder.h"
#include "test.h"

#include <stdarg.h> //va_list
#include <setjmp.h> //jmp_buf
#include <cmocka.h>

#include "linux/videodev2.h"

KHASH_MAP_INIT_STR(argvs_hash_t, char*);
KHASH_T(argvs_hash_t) *h;

int is_abort = 0;
int wrap_verbose = 0;
int test_verbose = 0;

struct app_state_t app;

struct input_t input;
struct filter_t filters[MAX_FILTERS];
struct output_t outputs[MAX_OUTPUTS];

#include "test_wraps.c"

static int test_setup(void **state)
{
    *state = &app;
    app_construct();
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
static void test_rfb(void **state)
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
    assert_int_not_equal(res, -1);

    app_cleanup();
}
#endif //RFB

static void print_help()
{
    printf("raspidetect_test [options]\n");
    printf("options:\n");
    printf("%s: print help\n", HELP);
    printf("%s: rfb test, default: %s\n", TEST_RFB, TEST_RFB_DEF);
    printf("%s: verbose, default: %d\n", VERBOSE, VERBOSE_DEF);
    printf("%s: wrap verbose, default: %d\n", WRAP_VERBOSE, WRAP_VERBOSE_DEF);
    exit(0);
}

int main(int argc, char **argv)
{
    int res = 0;

    h = KH_INIT(argvs_hash_t);
    utils_parse_args(argc, argv);
    app_set_default_state();

    unsigned help = KH_GET(argvs_hash_t, h, HELP);
    unsigned rfb = KH_GET(argvs_hash_t, h, TEST_RFB);
    unsigned verbose = KH_GET(argvs_hash_t, h, VERBOSE);
    unsigned w_verbose = KH_GET(argvs_hash_t, h, WRAP_VERBOSE);
    if (verbose != KH_END(h)) {
        app.verbose = 1;
        test_verbose = 1;
        TEST_DEBUG("Debug output has been enabled!!!");
    }
    if (w_verbose != KH_END(h)) {
        wrap_verbose = 1;
        WRAP_DEBUG("Wrap Debug output has been enabled!!!");
    }

    if (help != KH_END(h)) {
        print_help();
    }
    else if (rfb != KH_END(h)) {

        const struct CMUnitTest tests[] = {
            #ifdef RFB
                cmocka_unit_test_setup(test_rfb, NULL)
            #endif //RFB
        };
        res = cmocka_run_group_tests(tests, test_setup, test_teardown);
    }
    else {
        const struct CMUnitTest tests[] = {
            cmocka_unit_test_setup(test_utils_init, NULL),
            cmocka_unit_test_setup(test_file_loop, NULL),
            #ifdef SDL
                cmocka_unit_test_setup(test_sdl_loop, NULL),
            #endif //SDL
        };
        res = cmocka_run_group_tests(tests, test_setup, test_teardown);
    }

    KH_DESTROY(argvs_hash_t, h);
    return res;
}
