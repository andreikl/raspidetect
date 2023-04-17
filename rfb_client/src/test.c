// Raspidetect

// Copyright (C) 2023 Andrei Klimchuk <andrew.klimchuk@gmail.com>

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

// standard libraries
//#include <signal.h>
// windows
#include <stddef.h> //types like: size_t
#include <stdarg.h> //types like: va_list
#include <setjmp.h> //jmp_buf

#include <windowsx.h>
#include <cmocka.h>

// 3rd party libraries
#include "khash.h"

#include "main.h"
#include "utils.h"
#include "app.h"
#include "test.h"
// #include "d3d.h"
// #include "dxva.h"
// #include "rfb.h"
// #include "file.h"
// #include "cuda.h"

KHASH_MAP_INIT_STR(argvs_hash_t, char*)
KHASH_T(argvs_hash_t) *h;

#define VIDEO_PATH "./../h264_test_video"

int is_aborted = 0;
int wrap_verbose = 0;
int test_verbose = 0;
char buffer[MAX_STRING];
uint8_t ffmpeg_buffer[MAX_DATA];

struct app_state_t app;
struct filter_t filters[MAX_FILTERS];

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

#ifdef ENABLE_FFMPEG
#include "ffmpeg.h"

extern struct ffmpeg_state_t ffmpeg;
static void test_ffmpeg_loop(void **state)
{
    int res = 0;
    CALL(res = app_init(), error);

    struct filter_t *filter = ffmpeg.filter;

    CALL(res = filter->start(VIDEO_FORMAT_H264, VIDEO_FORMAT_GRAYSCALE), error);

    for (int i = 1; i <= 18; i++) {
        sprintf(buffer, VIDEO_PATH"/data%d.bin", i);
        size_t read = 0;
        CALL(utils_fill_buffer(
            buffer,
            ffmpeg_buffer,
            sizeof(ffmpeg_buffer),
            &read
        ));

        if (read > 4) {
            char* t = (char *)ffmpeg_buffer;
            TEST_DEBUG("H264 slice has been loaded: %d, %x %x %x %x ...",
                i, t[0], t[1], t[2], t[3]);
        }
        else {
            ERROR_MSG("H264 slice is empty: %d", read);
            res = -1;
            goto error;
        }
        filter->process(ffmpeg_buffer, read);
    }
    CALL(res = filter->stop(), error);

error:
    app_cleanup();

    TEST_DEBUG("res: %d", res);
    assert_int_not_equal(res, -1);
}
#endif //ENABLE_FFMPEG

#ifdef ENABLE_FFMPEG_DXVA2
#include "ffmpeg_dxva2.h"

extern struct ffmpeg_dxva2_state_t ffmpeg_dxva2;
static void test_ffmpeg_dxva2_loop(void **state)
{
    int res = 0;
    CALL(res = app_init(), error);

    struct filter_t *filter = ffmpeg_dxva2.filter;

    CALL(res = filter->start(VIDEO_FORMAT_H264, VIDEO_FORMAT_GRAYSCALE), error);

    for (int i = 1; i <= 18; i++) {
        sprintf(buffer, VIDEO_PATH"/data%d.bin", i);
        size_t read = 0;
        CALL(utils_fill_buffer(
            buffer,
            ffmpeg_buffer,
            sizeof(ffmpeg_buffer),
            &read
        ));

        if (read > 4) {
            char* t = (char *)ffmpeg_buffer;
            TEST_DEBUG("H264 slice has been loaded: %d, %x %x %x %x ...",
                i, t[0], t[1], t[2], t[3]);
        }
        else {
            ERROR_MSG("H264 slice is empty: %d", read);
            res = -1;
            goto error;
        }
        filter->process(ffmpeg_buffer, read);
    }
    CALL(res = filter->stop(), error);

error:
    app_cleanup();

    TEST_DEBUG("res: %d", res);
    assert_int_not_equal(res, -1);
}
#endif //ENABLE_FFMPEG_DXVA2

static void print_help()
{
    printf("rfb_client_test [options]\n");
    printf("options:\n");
    printf("%s: print help\n", HELP);
    printf("%s: verbose\n", VERBOSE);
    printf("%s: wrap verbose\n", WRAP_VERBOSE);
    exit(0);
}

int main(int argc, char **argv)
{
    int res = 0;

    h = KH_INIT(argvs_hash_t);
    utils_parse_args(argc, argv);
    app_set_default_state();

    unsigned help = KH_GET(argvs_hash_t, h, HELP);
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
    else {
        const struct CMUnitTest tests[] = {
            cmocka_unit_test_setup(test_utils_init, NULL),
#ifdef ENABLE_FFMPEG
            cmocka_unit_test_setup(test_ffmpeg_loop, NULL),
#endif // ENABLE_FFMPEG
#ifdef ENABLE_FFMPEG_DXVA2
            cmocka_unit_test_setup(test_ffmpeg_dxva2_loop, NULL),
#endif // ENABLE_FFMPEG_DXVA2
        };
        res = cmocka_run_group_tests(tests, test_setup, test_teardown);
    }

    KH_DESTROY(argvs_hash_t, h);
    return res;
}
