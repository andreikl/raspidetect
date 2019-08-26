#include <string.h>
#include <stdio.h>
#include <signal.h>

#include "khash.h"

#include "utils.h"
#include "main.h"

KHASH_MAP_INIT_STR(map_str, char*)
khash_t(map_str) *h;

int main(int argc, char** argv) {
    signal(SIGINT, default_signal_handler);

    h = kh_init(map_str);
    parse_args(argc, argv);

    unsigned k = kh_get(map_str, h, HELP);
    if (k != kh_end(h)) {
        print_help();
    }
    else {
        ov5647_main();
    }

    kh_destroy(map_str, h);
    return 0;
}
