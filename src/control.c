#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <memory.h>
#include <unistd.h>
#include <errno.h>
#include <sysexits.h>
#include <linux/input.h>

#include "main.h"

static int kb_left = 0, kb_up = 0, kb_right = 0, kb_down = 0;

int control_handle_key(APP_STATE *state) {
    int size = kbhit();
    int is_left = 0, is_up = 0, is_right = 0, is_down = 0;    
    if (size > 2) { 
        int x = getchar();
        int y = getchar();
        int z = getchar();
        if (x == 27 && y == 91 && z == 68) {
            is_left = 1;
        }
        else if (x == 27 && y == 91 && z == 65) {
            is_up = 1;
            fprintf(stderr, "INFO: Key UP\n");
        }
        else if (x == 27 && y == 91 && z == 67) {
            is_right = 1;
            fprintf(stderr, "INFO: Key RIGHT\n");
        }
        else if (x == 27 && y == 91 && z == 66) {
            is_down = 1;
            fprintf(stderr, "INFO: Key DOWN\n");
        } else {
            //fprintf(stderr, "INFO: Key %d, %d, %d\n", x, y, z);
        }
    }
    if (!kb_left && is_left) {
        kb_left = is_left;
        fprintf(stderr, "INFO: Key LEFT UP\n");
    } else if (kb_left && !is_left) {
        kb_left = is_left;
        fprintf(stderr, "INFO: Key LEFT DOWN\n");
    }
    if (!kb_up && is_up) {
        kb_up = is_up;
        fprintf(stderr, "INFO: Key UP UP\n");
    } else if (kb_up && !is_up) {
        kb_up = is_up;
        fprintf(stderr, "INFO: Key UP DOWN\n");
    }
    if (!kb_right && is_right) {
        kb_right = is_right;
        fprintf(stderr, "INFO: Key RIGHT UP\n");
    } else if (kb_right && !is_right) {
        kb_right = is_right;
        fprintf(stderr, "INFO: Key RIGHT DOWN\n");
    }
    if (!kb_down && is_down) {
        kb_down = is_down;
        fprintf(stderr, "INFO: Key DOWN UP\n");
    } else if (kb_down && !is_down) {
        kb_down = is_down;
        fprintf(stderr, "INFO: Key DOWN DOWN\n");
    }
    return 0;
}

int control_destroy(APP_STATE *state) {
    if (kb_left) {
        kb_left = 0;
        fprintf(stderr, "INFO: Key LEFT DOWN\n");
    }
    if (kb_up) {
        kb_up = 0;
        fprintf(stderr, "INFO: Key UP DOWN\n");
    }
    if (kb_right) {
        kb_right = 0;
        fprintf(stderr, "INFO: Key RIGHT DOWN\n");
    }
    if (kb_down) {
        kb_down = 0;
        fprintf(stderr, "INFO: Key DOWN DOWN\n");
    }
    return 0;
}