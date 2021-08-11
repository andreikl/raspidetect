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

KHASH_MAP_INIT_STR(map_str, char*)
khash_t(map_str) *h;

int is_abort = 0;

struct input_t input;
struct filter_t filters[MAX_OUTPUTS];
struct output_t outputs[MAX_OUTPUTS];

static void test_utils_init(void **state)
{
    struct app_state_t app;

	/*char xbuf[9]; // 8 + '\0'

	expect_value(__wrap_read, fd, 1);
	expect_value(__wrap_read, buf, xbuf);
	expect_value(__wrap_read, count, 8);
        will_return(__wrap_read,
		cast_ptr_to_largest_integral_type("xyz"));
	will_return(__wrap_read, 3);

	expect_value(__wrap_read, fd, 1);
	expect_value(__wrap_read, buf, xbuf+3);
	expect_value(__wrap_read, count,5);
        will_return(__wrap_read,
		cast_ptr_to_largest_integral_type("54321"));
	will_return(__wrap_read, 5);*/

	int res = utils_init(&app);
	assert_int_not_equal(res, -1);
}

int main(int argc, char **argv)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_utils_init),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
