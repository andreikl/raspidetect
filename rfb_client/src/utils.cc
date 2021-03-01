#include <khash.h>

#include "main.h"

KHASH_MAP_INIT_STR(map_str, char *)
extern khash_t(map_str) *h;

void utils_parse_args(int argc, char** argv)
{
    int ret;
    unsigned k;

    for (int i = 0; i < argc; i++) {
        if (argv[i][0] == '-') {
            k = kh_put(map_str, h, argv[i], &ret);
            kh_val(h, k) = (i + 1 < argc) ? argv[i + 1] : NULL;
        }
    }
}

const char *utils_read_str_value(const char* name, const char* def_value)
{
    unsigned k = kh_get(map_str, h, name);
    if (k != kh_end(h)) {
        return kh_val(h, k);
    }
    return def_value;
}

int utils_read_int_value(const char name[], int def_value)
{
    unsigned k = kh_get(map_str, h, name);
    if (k != kh_end(h)) {
        const char* value = kh_val(h, k);
        return atoi(value);
    }
    return def_value;
}

/*int utils_fill_buffer(const char *path, char *buffer, int buffer_size, size_t *read)
{
    FILE *fstream = fopen(path, "r");

    if (fstream == NULL) {
        fprintf(stderr, "ERROR: opening the file. (filename: %s)\n", path);
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
}*/

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

const char* convert_general_error(int error) {
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