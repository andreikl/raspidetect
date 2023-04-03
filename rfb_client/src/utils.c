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

#include <khash.h>

#include "main.h"
#include "utils.h"

KHASH_MAP_INIT_STR(argvs_hash_t, char *)
extern KHASH_T(argvs_hash_t) *h;

extern struct app_state_t app;
static char utils_buffer[MAX_STRING];

void utils_parse_args(int argc, char** argv)
{
    int ret;
    unsigned k;

    for (int i = 0; i < argc; i++) {
        if (argv[i][0] == '-') {
            k = KH_PUT(argvs_hash_t, h, argv[i], &ret);
            KH_VAL(h, k) = (i + 1 < argc) ? argv[i + 1] : NULL;
        }
    }
}

char *utils_read_str_value(const char *name, char *def_value)
{
    unsigned k = KH_GET(argvs_hash_t, h, name);
    if (k != KH_END(h)) {
        return KH_VAL(h, k);
    }
    return def_value;
}

int utils_read_int_value(const char name[], int def_value)
{
    unsigned k = KH_GET(argvs_hash_t, h, name);
    if (k != KH_END(h)) {
        const char* value = KH_VAL(h, k);
        return atoi(value);
    }
    return def_value;
}

int utils_fill_buffer(const char *path, uint8_t *buffer, int buffer_size, size_t *read)
{
    FILE *fstream = fopen(path, "r");

    if (fstream == NULL) {
        DEBUG_MSG("ERROR: opening the file. (filename: %s)\n", path);
        return EXIT_FAILURE;
    }

    size_t read_ = fread(buffer, 1, buffer_size, fstream);
    if (read_ < buffer_size) {
        buffer[read_] = 0;
    } else {
        buffer[buffer_size - 1] = 0;
    }

    if (read != NULL) {
        *read = read_;
    }

    fclose(fstream);

    return 0;
}

int find_nal(uint8_t *buf, int buf_size, int *nal_start, int *nal_end)
{
    int start = *nal_end + 4, end = *nal_end + 4, size = buf_size - 4;

    if (start >= size) {
        return -1;
    }
    
    while ((buf[end] != 0 || buf[end + 1] != 0 || buf[end + 2] != 0 || buf[end + 3] != 1) && end < size) {
        end++;
    }
    if (end >= size) {
        end += 4;
    }

    *nal_start = start;
    *nal_end = end;

    return 0;
}

char* convert_general_error(int error) {
    switch (error) {
        case EDEADLK:
            return "EDEADLK";
        case EINVAL:
            return "EINVAL";
        case ESRCH:
            return "ESRCH";
        default:
            return "Unknown";
    }
}

char* convert_hresult_error(HRESULT err) {
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM,
        NULL,
        err,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        utils_buffer,
        sizeof(utils_buffer),
        NULL);
    return utils_buffer;
}
