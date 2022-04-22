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

#include "v4l_encoder.h"

#include <stdarg.h> //va_list
#include <setjmp.h> //jmp_buf
#include <cmocka.h>

#define TEST_DEBUG(format, ...) \
{ \
    if (test_verbose) \
        fprintf(stderr, "TEST: %s:%s, "#format"\n", __FILE__, __FUNCTION__, ##__VA_ARGS__); \
}


#include "test_wraps.c"

static int test_setup(void **state)
{
    struct v4l_encoder_state_t *app = *state = malloc(sizeof(*app));
    memset(app, 0, sizeof(*app));
    return 0;
}

static int test_verbose_true(void **state)
{
    struct v4l_encoder_state_t *app = *state;
    test_verbose = 1;
    wrap_verbose = 1;
    return 0;
}

static int test_verbose_false(void **state)
{
    struct v4l_encoder_state_t *app = *state;
    test_verbose = 0;
    wrap_verbose = 0;
    return 0;
}

static int test_teardown(void **state)
{
    free(*state);
    return 0;
}

static void test_v4l(void **state)
{
    int res = 0;
    struct v4l_encoder_state_t *app = *state;
}

int main(int argc, char **argv)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup(test_v4l, test_verbose_true),
    };
    int res = cmocka_run_group_tests(tests, test_setup, test_teardown);
    return res;
}
