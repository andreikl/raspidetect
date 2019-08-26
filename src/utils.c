#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <signal.h>

#include "khash.h"

#include "main.h"
#include "utils.h"

KHASH_MAP_INIT_STR(map_str, char *)
extern khash_t(map_str) *h;

void parse_args(int argc, char** argv) {
    int ret;
    unsigned k;

    for (int i = 0; i < argc; i++) {
        if (argv[i][0] == '-') {
            k = kh_put(map_str, h, argv[i], &ret);
            kh_val(h, k) = (i + 1 < argc) ? argv[i + 1] : NULL;
        }
    }
}

void print_help(void) {
    printf("ov5647 [options]\n");
    printf("options:\n");
    printf("\n");
    printf("%s: help\n", HELP);
    printf("%s: width, default: %d\n", WIDTH, WIDTH_DEF);
    printf("%s: height, default: %d\n", HEIGHT, HEIGHT_DEF);
    printf("%s: worke_width, default: %d\n", WORKER_WIDTH, WORKER_WIDTH_DEF);
    printf("%s: worker_height, default: %d\n", WORKER_HEIGHT, WORKER_HEIGHT_DEF);
    printf("%s: TFL model path, default: %s\n", TFL_MODEL_PATH, TFL_MODEL_PATH_DEF);    
    printf("%s: DN model path, default: %s\n", DN_MODEL_PATH, DN_MODEL_PATH_DEF);    
    printf("%s: DN config path, default: %s\n", DN_CONFIG_PATH, DN_CONFIG_PATH_DEF);    
    printf("%s: input, default: %s, -: input stream\n", INPUT, INPUT_DEF);
    printf("%s: output, default: %s, -: output stream\n", OUTPUT, OUTPUT_DEF);
    printf("%s: verbose, verbose: %d\n", VERBOSE, VERBOSE_DEF);
    exit(0);
}

void default_status(APP_STATE *state) {
    memset(state, 0, sizeof(APP_STATE));

    strncpy(state->camera_name, "Unknown", MMAL_PARAMETER_CAMERA_INFO_MAX_STR_LEN);
    int width = read_int_value(WIDTH, WIDTH_DEF);
    int height = read_int_value(HEIGHT, HEIGHT_DEF);
    state->overlay_width = state->width = (width / 32 * 32) + (width % 32? 32: 0);
    state->overlay_height = state->height = (height / 16 * 16) + (height % 16? 16: 0);
    state->worker_width = read_int_value(WORKER_WIDTH, WORKER_WIDTH_DEF);
    state->worker_height = read_int_value(WORKER_HEIGHT, WORKER_HEIGHT_DEF);
    state->worker_pixel_bytes = 3;
    state->worker_total_objects = 10;
    state->verbose = read_int_value(VERBOSE, VERBOSE_DEF);
#ifdef TENSORFLOW
    state->model_path = read_str_value(TFL_MODEL_PATH, TFL_MODEL_PATH_DEF);
#elif DARKNET
    state->model_path = read_str_value(DN_MODEL_PATH, DN_MODEL_PATH_DEF);
    state->config_path = read_str_value(DN_CONFIG_PATH, DN_CONFIG_PATH_DEF);
#endif
}

/**
 * Handler for sigint signals
 *
 * @param signal_number ID of incoming signal.
 *
 */
void default_signal_handler(int signal_number) {
    if (signal_number == SIGUSR1) {
        // Aborting as well
        fprintf(stderr, "SIGUSR1\n"); 
        exit(130);
    } else {
        // Going to abort on all other signals
        fprintf(stderr, "Aborting program\n");
        exit(130);
    }
}

char *read_str_value(const char *name, char *def_value) {
    unsigned k = kh_get(map_str, h, name);
    if (k != kh_end(h)) {
        return kh_val(h, k);
    }
    return def_value;
}

int read_int_value(const char name[], int def_value) {
    unsigned k = kh_get(map_str, h, name);
    if (k != kh_end(h)) {
        const char* value = kh_val(h, k);
        return atoi(value);
    }
    return def_value;
}

int fill_buffer(const char *path, char *buffer, int buffer_size, size_t *read) {
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
}

// unsigned char * read_file(const char *path, int *size) {
//     unsigned char buffer[BUFFER_SIZE];
//     FILE *fstream;
//     size_t read;

//     #ifdef DEBUG
//     clock_t start_time = clock();
//     #endif

//     if (path[0] == '-') {
//         fstream = stdin;
//     }
//     else {
//         fstream = fopen(path, "r");
//     }

//     unsigned char *data = NULL; *size = 0;
//     do {
//         read = fread(buffer, sizeof(buffer[0]), BUFFER_SIZE, fstream);
//         if (read > 0) {
//             if (data == NULL) {
//                 data = malloc(read);
//             }
//             else {
//                 data = realloc(data, *size + read);
//             }
//             memcpy(data + *size, buffer, read);
//             *size += read;
//         }
//     } while (read == BUFFER_SIZE);

//     if (path[0] != '-') {
//          fclose(fstream);
//     }

// #ifdef DEBUG
//     double diff = (double)(clock() - start_time) / CLOCKS_PER_SEC;
//     fprintf(stderr, "INFO: elapsed %f ms\n", diff);
// #endif
    
//     return data;
// }

// void write_file(const char *path, unsigned char *data, int width, int height) {
//     FILE* fstream;
//     size_t written;
//     unsigned char buffer[BUFFER_SIZE];
//     int i = 0, j = 0, size = width * height;
//     unsigned char b;

// #ifdef DEBUG
//     clock_t start_time = clock();
// #endif

//     if (path[0] == '-') {
//         fstream = stdout;
//     }
//     else {
//         fstream = fopen(path, "w");
//     }


//     fprintf(fstream, "P6\n%d %d\n255\n", width, height);
//     while (j < size) {
//         if (i + 3 >= BUFFER_SIZE) {
//             written += fwrite(buffer, 1, i, fstream);
//             i = 0;
//         }

//         b = data[j];
// #ifdef DEBUG
//         if (b == 255) {
//             buffer[i] = b;
//             buffer[i + 1] = (unsigned char)0;
//             buffer[i + 2] = (unsigned char)0;
//         }
//         else {
//             buffer[i] = b;
//             buffer[i + 1] = b;
//             buffer[i + 2] = b;
//         }
// #else
//         buffer[i] = b;
//         buffer[i + 1] = b;
//         buffer[i + 2] = b;
// #endif
//         i += 3; j++;
//     }
//     if (i > 0) {
//         written += fwrite(buffer, 1, i, fstream);
//     }

//     if (path[0] != '-') {
//         fclose(fstream);
//     }

// #ifdef DEBUG
//     double diff = (double)(clock() - start_time) / CLOCKS_PER_SEC;
//     fprintf(stderr, "INFO: elapsed %f ms\n", diff);
// #endif
// }

void get_cpu_load(char * buffer, CPU_STATE *state) {
    fill_buffer("/proc/stat", buffer, BUFFER_SIZE, NULL);

    int user, nice, system, idle;
    sscanf(&buffer[4], "%d %d %d %d", &user, &nice, &system, &idle);

    int load = user + nice + system, all = load + idle;
    static int last_load = 0, last_all = 0;
    float cpu = (load - last_load) / (float)(all - last_all) * 100;

    last_load = user + nice + system;
    last_all = load + idle;

    state->cpu = cpu;
}


void get_memory_load(char * buffer, MEMORY_STATE *state) {
//  VmPeak                      peak virtual memory size
//  VmSize                      total program size
//  VmLck                       locked memory size
//  VmHWM                       peak resident set size ("high water mark")
//  VmRSS                       size of memory portions
//  VmData                      size of data, stack, and text segments
//  VmStk                       size of data, stack, and text segments
//  VmExe                       size of text segment
//  VmLib                       size of shared library code
//  VmPTE                       size of page table entries
//  VmSwap                      size of swap usage (the number of referred swapents)    
    fill_buffer("/proc/self/status", buffer, BUFFER_SIZE, NULL);
    char * line = buffer;
    while (line) {
        char * next_line = strchr(line, '\n');
        int line_len = next_line ? next_line - line : strlen(line);
        if (line_len > 1 && line[0] == 'V' && line[1] == 'm') {
            line[line_len] = 0;
            char * value_line = strchr(line, ':');
            value_line[0] = 0;
            value_line++;

            if (line[2] == 'S' && line[3] == 'i') {
                state->total_size = atoi(value_line);
            } else if (line[2] == 'S' && line[3] == 'w') {
                state->swap_size = atoi(value_line);
            } else if (line[2] == 'P' && line[3] == 'T') {
                state->pte_size = atoi(value_line);
            } else if (line[2] == 'L' && line[3] == 'i') {
                state->lib_size = atoi(value_line);
            } else if (line[2] == 'E' && line[3] == 'x') {
                state->exe_size = atoi(value_line);
            } else if (line[2] == 'S' && line[3] == 't') {
                state->stk_size = atoi(value_line);
            } else if (line[2] == 'D' && line[3] == 'a') {
                state->data_size = atoi(value_line);
            }
        }
        line = next_line ? next_line + 1: NULL;
    }
}

void get_temperature(char * buffer, TEMPERATURE_STATE *state) {
    fill_buffer("/sys/class/thermal/thermal_zone0/temp", buffer, BUFFER_SIZE, NULL);

    state->temp = (float)(atoi(buffer)) / 1000;
}
