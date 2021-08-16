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
#include "klist.h"

#include "main.h"
#include "utils.h"

#ifdef OPENCV
#include "opencv2/imgproc/imgproc_c.h"
#endif

KHASH_MAP_INIT_STR(argvs_hash_t, char *)
extern KHASH_T(argvs_hash_t) *h;

extern struct input_t input;
extern struct filter_t filters[MAX_FILTERS];
extern struct output_t outputs[MAX_OUTPUTS];

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

const char *utils_read_str_value(const char *name, char *def_value)
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

const char *video_formats[] = {
    VIDEO_FORMAT_UNKNOWN_STR,
    VIDEO_FORMAT_YUV422_STR
};

const char* utils_get_video_format_str(int format)
{
    int size = ARRAY_SIZE(video_formats);
    ASSERT_INT(format, <, 0, error);
    ASSERT_INT(format, >=, size, error);
    return video_formats[format];

error:
    errno = EOVERFLOW;
    return NULL;
}

int utils_get_video_format_int(const char* format)
{
    int size = ARRAY_SIZE(video_formats);
    for (int i = 1; i < size; i++) {
        if (!strcmp(video_formats[i], format)) {
            return i;
        }
    }
    return 0;    
}

const char *video_outputs[] = {
    VIDEO_OUTPUT_NULL_STR,
    VIDEO_OUTPUT_STDOUT_STR,
    VIDEO_OUTPUT_SDL_STR,
    VIDEO_OUTPUT_STDOUT_STR","VIDEO_OUTPUT_SDL_STR,
    VIDEO_OUTPUT_RFB_STR,
    VIDEO_OUTPUT_STDOUT_STR","VIDEO_OUTPUT_RFB_STR,
    VIDEO_OUTPUT_SDL_STR","VIDEO_OUTPUT_RFB_STR,
    VIDEO_OUTPUT_STDOUT_STR","VIDEO_OUTPUT_SDL_STR","VIDEO_OUTPUT_RFB_STR,
};

const char* utils_get_video_output_str(int output)
{

    int size = ARRAY_SIZE(video_outputs);
    ASSERT_INT(output, <, 0, error);
    ASSERT_INT(output, >=, size, error);
    return video_outputs[output];

error:
    errno = EOVERFLOW;
    return NULL;
}

int utils_get_video_output_int(const char* output)
{
    ASSERT_PTR(output, ==, NULL, error);
    ASSERT_INT((int)strlen(output), >, MAX_STRING, error);

    int res = 0;
    const char coma = ',';
    const char *next_start = output;
    const char *next_end = strchr(next_start, coma);
    do {
        int len = next_end != NULL? next_end - next_start: strlen(next_start);
        if (strncmp(VIDEO_OUTPUT_STDOUT_STR, next_start, len) == 0)
            res |= VIDEO_OUTPUT_STDOUT;
        else if (strncmp(VIDEO_OUTPUT_SDL_STR, next_start, len) == 0)
            res |= VIDEO_OUTPUT_SDL;
        else if (strncmp(VIDEO_OUTPUT_RFB_STR, next_start, len) == 0)
            res |= VIDEO_OUTPUT_RFB;
    } while (next_end != NULL);
    return res;

error:
    errno = EOVERFLOW;
    return -1;
}

void utils_set_default_state(struct app_state_t *app)
{
    memset(app, 0, sizeof(struct app_state_t));
    app->video_width = utils_read_int_value(VIDEO_WIDTH, VIDEO_WIDTH_DEF);
    app->video_height = utils_read_int_value(VIDEO_HEIGHT, VIDEO_HEIGHT_DEF);
    const char* format = utils_read_str_value(VIDEO_FORMAT, VIDEO_FORMAT_DEF);
    app->video_format = utils_get_video_format_int(format);
    const char *output = utils_read_str_value(VIDEO_OUTPUT, VIDEO_OUTPUT_DEF);
    app->video_output = utils_get_video_output_int(output);

    app->port = utils_read_int_value(PORT, PORT_DEF);
    app->worker_width = utils_read_int_value(WORKER_WIDTH, WORKER_WIDTH_DEF);
    app->worker_height = utils_read_int_value(WORKER_HEIGHT, WORKER_HEIGHT_DEF);
    app->worker_total_objects = 10;
    app->worker_thread_res = -1;
    app->verbose = utils_read_int_value(VERBOSE, VERBOSE_DEF);
#ifdef RFB
    app->rfb.thread_res = -1;
#endif
#ifdef TENSORFLOW
    app->model_path = utils_read_str_value(TFL_MODEL_PATH, TFL_MODEL_PATH_DEF);
#elif DARKNET
    app->model_path = utils_read_str_value(DN_MODEL_PATH, DN_MODEL_PATH_DEF);
    app->config_path = utils_read_str_value(DN_CONFIG_PATH, DN_CONFIG_PATH_DEF);
#endif
}

void utils_construct(struct app_state_t *app)
{
#ifdef V4L
    v4l_construct(app);
#endif //V4L

#ifdef V4L_ENCODER
    v4l_encoder_construct(app);
#elif MMAL_ENCODER
    mmal_encoder_construct(app);
#endif

#ifdef SDL
    if ((app->video_output & VIDEO_OUTPUT_SDL) == VIDEO_OUTPUT_SDL)
        sdl_construct(app);
#endif //SDL

#ifdef RFB
    if ((app->video_output & VIDEO_OUTPUT_RFB) == VIDEO_OUTPUT_RFB)
        return rfb_construct(&app);
#endif //RFB
}

#define BFS_NODE_FREE(x)
struct bfs_node_t {
    int index;
    int distance;
};
KLIST_INIT(bfs_qeue_t, struct bfs_node_t, BFS_NODE_FREE)

static int find_path(
    const struct format_mapping_t* in_f,
    const struct format_mapping_t* out_f,
    int filters_len,
    struct output_t* output)
{
    int matrix_len = filters_len + 2;
    int bfs_adjacency_matrix[MAX_FILTERS + 2][MAX_FILTERS + 2];
    memset(*bfs_adjacency_matrix, 0, sizeof(bfs_adjacency_matrix));
    printf("bfs_adjacency_matrix: %lu", sizeof(bfs_adjacency_matrix));

    if (in_f->format == out_f->format) {
        bfs_adjacency_matrix[0][matrix_len - 1] = in_f->format;
    }

    for (int i = 0; filters[i].context != NULL && i < MAX_FILTERS; i++) {
        const struct format_mapping_t* fin_fs = NULL;
        int fin_fs_len = filters[i].get_in_formats(&fin_fs);
        const struct format_mapping_t* fout_fs = NULL;
        int fout_fs_len = filters[i].get_out_formats(&fout_fs);

        for (int ii = 0; ii < fin_fs_len; ii++) {
            const struct format_mapping_t* fin_f = fin_fs + ii;
            if (!fin_f->is_supported)
                continue;
            if (fin_f->format == in_f->format) {
                bfs_adjacency_matrix[0][i + 1] = in_f->format;
            }

            //try to connect another output format of filter(j) to current input format of filter(i)
            for (int j = 0; filters[j].context != NULL && j < MAX_FILTERS; j++) {
                if (j == i)
                    continue; //skip current filter
                const struct format_mapping_t* ffout_fs = NULL;
                int ffout_fs_len = filters[i].get_out_formats(&ffout_fs);
                for (int jj = 0; jj < ffout_fs_len; jj++) {
                    const struct format_mapping_t* ffout_f = ffout_fs + jj;
                    if (!ffout_f->is_supported)
                        continue;

                    if (fin_f->format == ffout_f->format) {
                        bfs_adjacency_matrix[j + 1][i + 1] = ffout_f->format;
                    }
                }
            }
        }

        for (int ii = 0; ii < fout_fs_len; ii++) {
            const struct format_mapping_t* fout_f = fout_fs + ii;
            if (!fout_f->is_supported)
                continue;
            if (out_f->format == fout_f->format) {
                bfs_adjacency_matrix[i + 1][matrix_len - 1] = out_f->format;
            }

            //try to connect current output format of filter(i) to another input format of filter(j)
            for (int j = 0; filters[j].context != NULL && j < MAX_FILTERS; j++) {
                if (j == i)
                    continue; //skip current filter
                const struct format_mapping_t* ffin_fs = NULL;
                int ffin_fs_len = filters[i].get_in_formats(&ffin_fs);
                for (int jj = 0; jj < ffin_fs_len; jj++) {
                    const struct format_mapping_t* ffin_f = ffin_fs + jj;
                    if (!ffin_fs->is_supported)
                        continue;

                    if (fout_f->format == ffin_f->format) {
                        bfs_adjacency_matrix[i + 1][j + 1] = fout_f->format;
                    }
                }
            }
        }
    }

    //Breadth-First-Search (BFS)
    int distance = -1;
    KLIST_T(bfs_qeue_t) *bfs_qeue = KL_INIT(bfs_qeue_t);
    struct bfs_node_t node = {
        .index = 0,
        .distance = 0
    };
    *KL_PUSHP(bfs_qeue_t, bfs_qeue) = node;
    int bfs_path[MAX_FILTERS + 2];
    memset(bfs_path, 0, sizeof(bfs_path));
    bfs_path[node.index] = distance;

    while (bfs_qeue->size > 0 && distance < 0) {
        KL_SHIFT(bfs_qeue_t, bfs_qeue, &node);
        for (int i = 0; i < matrix_len; i++) {
            if (bfs_adjacency_matrix[node.index][i] > 0) {
                if (bfs_path[i])
                    continue;

                if (i == matrix_len - 1) {
                    int d = distance = node.distance;
                    if (d > 0) {
                        d--;
                        output->filters[d].out_format = bfs_adjacency_matrix[node.index][i];
                        output->filters[d].index = node.index - 1;
                    }
                    while (d > 0) {
                        // find previous filter
                        for (int j = 1; j < ARRAY_SIZE(bfs_path); j++) {
                            if (bfs_path[j] == d + 1) {
                                output->filters[d - 1].out_format
                                    = bfs_adjacency_matrix[node.index][i];
                                output->filters[d-1].index = j - 1;
                                break;
                            }
                        }
                        d--;
                    }
                    output->start_format = in_f->format;
                    break;
                }

                struct bfs_node_t next = {
                    .index = i,
                    .distance = node.distance + 1
                };
                *KL_PUSHP(bfs_qeue_t, bfs_qeue) = next;
                bfs_path[i] = next.distance;
            }
        }
    }

    DEBUG_INT("distance", distance);
    KL_DESTROY(bfs_qeue_t, bfs_qeue);
    return distance;
}

int utils_init(struct app_state_t *app)
{
    CALL(input.init(), error);
    for (int i = 0; outputs[i].context != NULL && i < MAX_OUTPUTS; i++)
        CALL(outputs[i].init(&app), error);

    int filters_len = 0;
    for (int i = 0; filters[i].context != NULL && i < MAX_FILTERS; i++) {
        CALL(filters[i].init(&app), error);
        filters_len++;
    }
    DEBUG_INT("filters count", filters_len);

    const struct format_mapping_t* in_fs = NULL;
    int in_fs_len = input.get_formats(&in_fs);
    DEBUG_INT("input formats count", in_fs_len);
    for (int i = 0; outputs[i].context != NULL && i < MAX_OUTPUTS; i++) {
        const struct format_mapping_t* out_fs = NULL;
        int out_fs_len = outputs[i].get_formats(&out_fs);
        DEBUG_INT("output formats count", out_fs_len);
        
        for (int ii = 0; ii < out_fs_len; ii++) {
            const struct format_mapping_t* out_f = out_fs + ii;
            if (!out_f->is_supported)
                continue;

            for (int jj = 0; jj < in_fs_len; jj++) {
                const struct format_mapping_t* in_f = in_fs + jj;

                if (!out_f->is_supported)
                    continue;

                if (find_path(in_f, out_f, filters_len, outputs + i) >= 0) {
                    fprintf(stderr,
                        "INFO: input -> %d\n",
                        outputs[i].start_format);
                    for (int k = 0; k < MAX_FILTERS && outputs[i].filters[k].out_format; k++) {
                        int index = outputs[i].filters[k].index;
                        fprintf(stderr,
                            "INFO: filter(%s) -> %d\n",
                            filters[index].name,
                            outputs[i].filters[k].out_format);
                    }
                }
            }
        }
    }

    return 0;
error:
    if (errno == 0)
        errno = EAGAIN;
    return -1;
}

int utils_fill_buffer(const char *path, char *buffer, int buffer_size, size_t *read)
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
}

/*static unsigned char * read_file(const char *path, int *size)
{
    unsigned char buffer[BUFFER_SIZE];
    FILE *fstream;
    size_t read;

    if (path[0] == '-') {
        fstream = stdin;
    }
    else {
        fstream = fopen(path, "r");
    }

    unsigned char *data = NULL; *size = 0;
    do {
        read = fread(buffer, sizeof(buffer[0]), BUFFER_SIZE, fstream);
        if (read > 0) {
            if (data == NULL) {
                data = malloc(read);
            }
            else {
                data = realloc(data, *size + read);
            }
            memcpy(data + *size, buffer, read);
            *size += read;
        }
    } while (read == BUFFER_SIZE);

    if (path[0] != '-') {
         fclose(fstream);
    }

    return data;
}*/

void *utils_read_file(const char *path, size_t *len)
{
    FILE *fstream = NULL;

    if (path[0] == '-') {
        fstream = stdin;
    }
    else {
        fstream = fopen(path, "r");
    }
    if (fstream == NULL) {
        fprintf(stderr, "ERROR: Failed to open file. path: %s", path);
        goto fail_open;
    }

    fseek(fstream, 0, SEEK_END);
    size_t len_p = ftell(fstream);
    fseek(fstream, 0, SEEK_SET);

    unsigned char *data = malloc(len_p);
    if (data == NULL) {
        fprintf(stderr, "ERROR: Failed to allocate memory. path: %s", path);
        goto fail_memory;
    }

    size_t read = fread(data, 1, len_p, fstream);
    if (read != len_p) {
        fprintf(stderr, "ERROR: Failed to read file. path: %s", path);
        goto fail_read;
    }

    *len = len_p;
    return data;

fail_read:
    free(data);

fail_memory:
    if (path[0] != '-' && fstream != NULL) {
         fclose(fstream);
    }

fail_open:
    return NULL;
}

void utils_write_file(const char *path, unsigned char *data, int width, int height)
{
    FILE* fstream;
    size_t written;
    unsigned char buffer[MAX_DATA];
    int i = 0, j = 0, size = width * height;
    unsigned char b;

#ifdef DEBUG
    clock_t start_time = clock();
#endif

    if (path[0] == '-') {
        fstream = stdout;
    }
    else {
        fstream = fopen(path, "w");
    }


    fprintf(fstream, "P6\n%d %d\n255\n", width, height);
    while (j < size) {
        if (i + 3 >= MAX_DATA) {
            written += fwrite(buffer, 1, i, fstream);
            i = 0;
        }

        b = data[j];
#ifdef DEBUG
        if (b == 255) {
            buffer[i] = b;
            buffer[i + 1] = (unsigned char)0;
            buffer[i + 2] = (unsigned char)0;
        }
        else {
            buffer[i] = b;
            buffer[i + 1] = b;
            buffer[i + 2] = b;
        }
#else
        buffer[i] = b;
        buffer[i + 1] = b;
        buffer[i + 2] = b;
#endif
        i += 3; j++;
    }
    if (i > 0) {
        written += fwrite(buffer, 1, i, fstream);
    }

    if (path[0] != '-') {
        fclose(fstream);
    }

#ifdef DEBUG
    double diff = (double)(clock() - start_time) / CLOCKS_PER_SEC;
    fprintf(stderr, "INFO: elapsed %f ms\n", diff);
#endif
}

void utils_get_cpu_load(char * buffer, struct cpu_state_t *cpu)
{
    utils_fill_buffer("/proc/stat", buffer, MAX_DATA, NULL);

    int user, nice, system, idle;
    sscanf(&buffer[4], "%d %d %d %d", &user, &nice, &system, &idle);

    int load = user + nice + system, all = load + idle;
    static int last_load = 0, last_all = 0;
    float fcpu = (load - last_load) / (float)(all - last_all) * 100;

    last_load = user + nice + system;
    last_all = load + idle;

    cpu->cpu = fcpu;
}


void utils_get_memory_load(char * buffer, struct memory_state_t *memory)
{
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
    utils_fill_buffer("/proc/self/status", buffer, MAX_DATA, NULL);
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
                memory->total_size = atoi(value_line);
            } else if (line[2] == 'S' && line[3] == 'w') {
                memory->swap_size = atoi(value_line);
            } else if (line[2] == 'P' && line[3] == 'T') {
                memory->pte_size = atoi(value_line);
            } else if (line[2] == 'L' && line[3] == 'i') {
                memory->lib_size = atoi(value_line);
            } else if (line[2] == 'E' && line[3] == 'x') {
                memory->exe_size = atoi(value_line);
            } else if (line[2] == 'S' && line[3] == 't') {
                memory->stk_size = atoi(value_line);
            } else if (line[2] == 'D' && line[3] == 'a') {
                memory->data_size = atoi(value_line);
            }
        }
        line = next_line ? next_line + 1: NULL;
    }
}

void utils_get_temperature(char * buffer, struct temperature_state_t *temperature)
{
    utils_fill_buffer("/sys/class/thermal/thermal_zone0/temp", buffer, MAX_DATA, NULL);

    temperature->temp = (float)(atoi(buffer)) / 1000;
}

static float lerpf(float s, float e, float t)
{
    return s + (e - s) * t;
}

static float lerp(int s, int e, float t)
{
    return s + (e - s) * t;
}

static float blerp(int c00, int c10, int c01, int c11, float tx, float ty)
{
    return lerpf(lerp(c00, c10, tx), lerp(c01, c11, tx), ty);
}

static int resize_li_16(int *src, int src_width, int src_height, int *dst, int dst_width, int dst_height)
{
#if defined(ENV32BIT)
    int dw = dst_width >> 1;
    int sw = src_width >> 1;

    float fx = (sw - 1) / ((float)dst_width);
    float fy = (src_height - 1) / ((float)dst_height);
    float gfx = 0.0f, gfy = 0.0f;
    float dfx1 = 0.0f, dfx2 = 0.0f;
    float dfy = 0.0f;
    int dx = 0, di = 0;
    int sx1 = 0, sx2 = 0;
    int sy = 0, si = 0;
    int sj1 = 0, sj2 = 0;

    int32_t p000;
    int32_t p001;
    int32_t p010;
    int32_t p011;
    int32_t p100;
    int32_t p101;
    int32_t p110;
    int32_t p111;
    int32_t res;
    int dsize = dw * dst_height;
    while (di < dsize) {
        if ((sj1 & 0b1) == 0) {
            int32_t t1 = src[sj1];
            int32_t t3 = src[sj1 + sw];
            p000 = t1 & MASK1_565;
            p001 = t1 & MASK2_565 >> 16;
            p010 = t3 & MASK1_565;
            p011 = t3 & MASK2_565 >> 16;
        }
        else {
            int32_t t1 = src[sj1];
            int32_t t2 = src[sj1 + 1];
            int32_t t3 = src[sj1 + sw];
            int32_t t4 = src[sj1 + sw + 1];
            p000 = t1 & MASK2_565 >> 16;
            p001 = t2 & MASK1_565;
            p010 = t3 & MASK2_565 >> 16;
            p011 = t4 & MASK1_565;
        }
        if ((sj2 & 0b1) == 0) {
            int32_t t1 = src[sj2];
            int32_t t3 = src[sj2 + sw];
            p100 = t1 & MASK1_565;
            p101 = t1 & MASK2_565 >> 16;
            p110 = t3 & MASK1_565;
            p111 = t3 & MASK2_565 >> 16;
        }
        else {
            int32_t t1 = src[sj2];
            int32_t t2 = src[sj2 + 1];
            int32_t t3 = src[sj2 + sw];
            int32_t t4 = src[sj2 + sw + 1];
            p100 = t1 & MASK2_565 >> 16;
            p101 = t2 & MASK1_565;
            p110 = t3 & MASK2_565 >> 16;
            p111 = t4 & MASK1_565;
        }

        res = SET_R5651((int)blerp(GET_R5651(p000), 
                    GET_R5651(p001), 
                    GET_R5651(p010),
                    GET_R5651(p011),
                    dfx1, 
                    dfy));

        res |= SET_G5651((int)blerp(GET_G5651(p000), 
                    GET_G5651(p001), 
                    GET_G5651(p010),
                    GET_G5651(p011),
                    dfx1, 
                    dfy));

        res |= SET_B5651((int)blerp(GET_B5651(p000), 
                    GET_B5651(p001), 
                    GET_B5651(p010),
                    GET_B5651(p011),
                    dfx1, 
                    dfy));

        res |= SET_R5652((int)blerp(GET_R5651(p100), 
                    GET_R5651(p101), 
                    GET_R5651(p110),
                    GET_R5651(p111),
                    dfx2, 
                    dfy));

        res |= SET_G5652((int)blerp(GET_G5651(p100), 
                    GET_G5651(p101), 
                    GET_G5651(p110),
                    GET_G5651(p111),
                    dfx2, 
                    dfy));

        res |= SET_B5652((int)blerp(GET_B5651(p100), 
                    GET_B5651(p101), 
                    GET_B5651(p110),
                    GET_B5651(p111),
                    dfx2, 
                    dfy));

        dst[di] = res;

        /*if (di == 75) {
            fprintf(stderr, "INFO: dx: %d, sx1: %d, sj1: %d, gfx: %2.2f, fx: %2.2f\n", dx, sx1, sj1, gfx, fx);
            fprintf(stderr, "INFO: p00: %d, p01: %d, p10: %d, p11: %d\n", p000, p001, p010, p011);
            fprintf(stderr, "INFO: b00: %d, b01: %d, b10: %d, b11: %d, blerp: %d\n", GET_B565(p000), GET_B565(p001), GET_B565(p010), GET_B565(p011), (int)blerp(GET_B565(p000), 
                    GET_B565(p001), 
                    GET_B565(p010),
                    GET_B565(p011),
                    dfx1, 
                    dfy));
            fprintf(stderr, "INFO: g00: %d, g01: %d, g10: %d, g11: %d, blerp: %d\n", GET_G565(p000), GET_G565(p001), GET_G565(p010), GET_G565(p011), (int)blerp(GET_G565(p000), 
                    GET_G565(p001), 
                    GET_G565(p010),
                    GET_G565(p011),
                    dfx1, 
                    dfy));
            fprintf(stderr, "INFO: r00: %d, r01: %d, r10: %d, r11: %d, blerp: %d\n", GET_R565(p000), GET_R565(p001), GET_R565(p010), GET_R565(p011), (int)blerp(GET_R565(p000), 
                    GET_R565(p001),
                    GET_R565(p010),
                    GET_R565(p011),
                    dfx1, 
                    dfy));
            fprintf(stderr, "INFO: res: %d\n", res);
        }*/

        gfx += fx;
        sx1 = (int)gfx;
        dfx1 = gfx - sx1;
        sj1 = si + sx1;

        gfx += fx;
        sx2 = (int)gfx;
        dfx2 = gfx - sx2;
        sj2 = si + sx2;
        dx++; di++;

        if (dx >= dw) {
            gfx = 0;
            dx = 0;
            gfy += fy;
            sy = (int)gfy;
            dfy = gfy - sy;
            si = sw * sy;
        }
    }
    return 0;
#else
    fprintf(stderr, "ERROR: 64 bits aren't supported\n");
    return -1;
#endif
}

int utils_get_worker_buffer(struct app_state_t *app)
{
#ifdef OPENCV
    CvMat *img1 = cvCreateMatHeader(app->width,
                                    app->height,
                                    CV_8UC2);
    cvSetData(img1, app->openvg.video_buffer, app->width << 1);

    CvMat *img2 = cvCreateMatHeader(app->worker_width,
                                    app->worker_height,
                                    CV_8UC2);
    cvSetData(img2, app->worker_buffer_565, app->worker_width << 1);

    cvResize(img1, img2, CV_INTER_LINEAR);

    cvRelease(&img1);
    cvRelease(&img2);
#elif OPENVG
    int res = resize_li_16(app->openvg.video_buffer.i,
                app->width,
                app->height,
                (int*)app->worker_buffer_565,
                app->worker_width,
                app->worker_height);
    if (res != 0) {
        fprintf(stderr, "ERROR: Failed to resize image %d\n", res);
        return -1;
    }
#endif
    // openvg implementation
    // vgImageSubData(app->openvg.video_image,
    //             app->openvg.video_buffer,
    //             app->width << 1,
    //             VG_sRGB_565,
    //             0, 0,
    //             app->width, app->height);

    // vgSeti(VG_MATRIX_MODE, VG_MATRIX_IMAGE_USER_TO_SURFACE);
    // vgLoadIdentity();
    // vgScale(((float)app->worker_width) / app->width, ((float)app->worker_height) / app->height);
    // vgDrawImage(app->openvg.video_image);

    // vgReadPixels(   app->worker_buffer_565,
    //                 app->width << 1,
    //                 VG_sRGB_565,
    //                 0, 0,
    //                 app->worker_width, app->worker_height);
#if defined(ENV32BIT)
    int32_t *buffer_565 = (int32_t *)app->worker_buffer_565;
    int32_t *buffer_rgb = (int32_t *)app->worker_buffer_rgb;
    int i = 0, j = 0;
    int l = app->worker_width * app->worker_height >> 1;
    int32_t vs1, vs2, vd1, vd2, vd3;
    while(i < l) {
        vs1 = buffer_565[i++];
        vs2 = buffer_565[i++];
        vd1 = GET_R5652(vs1) << (24 + 3) |
            GET_B5651(vs1) << (16 + 3) |
            GET_G5651(vs1) << (8 + 2) |
            GET_R5651(vs1) << (3);

        vd2 = GET_G5651(vs1) << (24 + 3) |
            GET_R5651(vs2) << (16 + 3) |
            GET_B5652(vs1) << (8 + 3) |
            GET_G5652(vs1) << (2);

        vd3 = GET_B5651(vs1) << (24 + 3) |
            GET_G5651(vs2) << (16 + 2) |
            GET_R5652(vs1) << (8 + 3) |
            GET_B5651(vs1) << (3);

        buffer_rgb[j++] = vd1;
        buffer_rgb[j++] = vd2;
        buffer_rgb[j++] = vd3;
    }
#else
    fprintf(stderr, "ERROR: 64 bits aren't supported\n");
    return -1;
#endif
    return 0;
}
