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
    fprintf(stderr, "TEST: %s:%s, "#format"\n", __FILE__, __FUNCTION__, ##__VA_ARGS__); \
}

KHASH_MAP_INIT_STR(argvs_hash_t, char*)
KHASH_T(argvs_hash_t) *h;

int is_abort;

struct input_t input;
struct filter_t filters[MAX_FILTERS];
struct output_t outputs[MAX_OUTPUTS];

#include "test_wraps.c"

static int test_setup(void **state)
{
    struct app_state_t *app = *state = malloc(sizeof(*app));
    memset(app, 0, sizeof(*app));
    app_set_default_state(app);
    app_construct(app);
    return 0;
}

static int test_verbose_true(void **state)
{
    struct app_state_t *app = *state;
    app->verbose = 1;
    return 0;
}

static int test_verbose_false(void **state)
{
    struct app_state_t *app = *state;
    app->verbose = 0;
    return 0;
}

static int test_teardown(void **state)
{
    free(*state);
    return 0;
}

static void test_utils_init(void **state)
{
    int res = 0;
    struct app_state_t *app = *state;
    CALL(res = app_init(app));

    TEST_DEBUG("res: %d", res);
    assert_int_not_equal(res, -1);

    app_cleanup(app);
}

#ifdef SDL
#include "sdl.h"
extern struct sdl_state_t sdl;
static void test_sdl_loop(void **state)
{
    int res = 0;
    struct app_state_t *app = *state;
    struct output_t *output = sdl.output;

    expect_value(__wrap_ioctl, fmt->fmt.pix.pixelformat, V4L2_PIX_FMT_YUYV);
    //will_return(__wrap_ioctl, 3);

    CALL(res = app_init(app), error);
    CALL(res = input.start(output->start_format), error);
    for (int i = 0; i < 10; i++) {
        CALL(res = input.process_frame());
        if (res == -1 && errno != ETIME)
            break;            
        else
            res = 0;
        char* buffer = input.get_buffer();
        CALL(res = output->render(buffer));
    }

error:
    TEST_DEBUG("res: %d", res);
    assert_int_not_equal(res, -1);

    CALL(input.stop());
    app_cleanup(app);
}
#endif //SDL

int main(int argc, char **argv)
{
    h = KH_INIT(argvs_hash_t);
    utils_parse_args(argc, argv);

    const struct CMUnitTest tests[] = {
        //cmocka_unit_test_setup(test_utils_init, test_verbose_false),
#ifdef SDL
        cmocka_unit_test_setup(test_sdl_loop, test_verbose_true)
#endif //SDL
    };
    int res = cmocka_run_group_tests(tests, test_setup, test_teardown);

    KH_DESTROY(argvs_hash_t, h);
    return res;
}
