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

#include "klist.h"

#include "main.h"
#include "utils.h"

extern struct input_t input;
extern struct filter_t filters[MAX_FILTERS];
extern struct output_t outputs[MAX_OUTPUTS];

const char *video_formats[] = {
    VIDEO_FORMAT_UNKNOWN_STR,
    VIDEO_FORMAT_YUV422_STR,
    VIDEO_FORMAT_YUV444_STR,
    VIDEO_FORMAT_H264_STR
};

const char *video_outputs[] = {
    VIDEO_OUTPUT_NULL_STR,
    VIDEO_OUTPUT_FILE_STR,
    VIDEO_OUTPUT_SDL_STR,
    VIDEO_OUTPUT_FILE_STR","VIDEO_OUTPUT_SDL_STR,
    VIDEO_OUTPUT_RFB_STR,
    VIDEO_OUTPUT_FILE_STR","VIDEO_OUTPUT_RFB_STR,
    VIDEO_OUTPUT_SDL_STR","VIDEO_OUTPUT_RFB_STR,
    VIDEO_OUTPUT_FILE_STR","VIDEO_OUTPUT_SDL_STR","VIDEO_OUTPUT_RFB_STR,
};

const char* app_get_video_format_str(int format)
{
    int size = ARRAY_SIZE(video_formats);
    ASSERT_INT(format, <, 0, error);
    ASSERT_INT(format, >=, size, error);
    return video_formats[format];

error:
    errno = EOVERFLOW;
    return NULL;
}

const char* app_get_video_output_str(int output)
{
    int size = ARRAY_SIZE(video_outputs);
    ASSERT_INT(output, <, 0, error);
    ASSERT_INT(output, >=, size, error);
    return video_outputs[output];

error:
    errno = EOVERFLOW;
    return NULL;
}

int app_get_video_format_int(const char* format)
{
    int size = ARRAY_SIZE(video_formats);
    for (int i = 1; i < size; i++) {
        if (!strcmp(video_formats[i], format)) {
            return i;
        }
    }
    return 0;    
}

int app_get_video_output_int(const char* output)
{
    ASSERT_PTR(output, ==, NULL, error);
    ASSERT_INT((int)strlen(output), >, MAX_STRING, error);

    int res = 0;
    const char coma = ',';
    const char *next_start = output;
    const char *next_end = strchr(next_start, coma);
    do {
        int len = next_end != NULL? next_end - next_start: strlen(next_start);
        if (strncmp(VIDEO_OUTPUT_FILE_STR, next_start, len) == 0)
            res |= VIDEO_OUTPUT_FILE;
        else if (strncmp(VIDEO_OUTPUT_SDL_STR, next_start, len) == 0)
            res |= VIDEO_OUTPUT_SDL;
        else if (strncmp(VIDEO_OUTPUT_RFB_STR, next_start, len) == 0)
            res |= VIDEO_OUTPUT_RFB;

        if (next_end == NULL)
            break;

        next_start = next_end + 1;
        next_end = strchr(next_start, coma);
    } while (1);
    return res;

error:
    errno = EOVERFLOW;
    return -1;
}

void app_set_default_state(struct app_state_t *app)
{
    memset(app, 0, sizeof(struct app_state_t));
    app->video_width = utils_read_int_value(VIDEO_WIDTH, VIDEO_WIDTH_DEF);
    app->video_height = utils_read_int_value(VIDEO_HEIGHT, VIDEO_HEIGHT_DEF);
    const char* format = utils_read_str_value(VIDEO_FORMAT, VIDEO_FORMAT_DEF);
    app->video_format = app_get_video_format_int(format);
    const char *output = utils_read_str_value(VIDEO_OUTPUT, VIDEO_OUTPUT_DEF);
    app->video_output = app_get_video_output_int(output);

    app->port = utils_read_int_value(PORT, PORT_DEF);
    app->worker_width = utils_read_int_value(WORKER_WIDTH, WORKER_WIDTH_DEF);
    app->worker_height = utils_read_int_value(WORKER_HEIGHT, WORKER_HEIGHT_DEF);
    app->worker_total_objects = 10;
    app->worker_thread_res = -1;
    app->verbose = utils_read_int_value(VERBOSE, VERBOSE_DEF);
    app->output_path = utils_read_str_value(OUTPUT_PATH, OUTPUT_PATH_DEF);
#ifdef TENSORFLOW
    app->model_path = utils_read_str_value(TFL_MODEL_PATH, TFL_MODEL_PATH_DEF);
#elif DARKNET
    app->model_path = utils_read_str_value(DN_MODEL_PATH, DN_MODEL_PATH_DEF);
    app->config_path = utils_read_str_value(DN_CONFIG_PATH, DN_CONFIG_PATH_DEF);
#endif
}

void app_construct(struct app_state_t *app)
{
#ifdef V4L
    v4l_construct(app);
#endif //V4L

#ifdef V4L_ENCODER
    v4l_encoder_construct(app);
#elif MMAL_ENCODER
    mmal_encoder_construct(app);
#endif

    if ((app->video_output & VIDEO_OUTPUT_FILE) == VIDEO_OUTPUT_FILE)
        file_construct(app);

#ifdef SDL
    if ((app->video_output & VIDEO_OUTPUT_SDL) == VIDEO_OUTPUT_SDL)
        sdl_construct(app);
#endif //SDL

#ifdef RFB
    if ((app->video_output & VIDEO_OUTPUT_RFB) == VIDEO_OUTPUT_RFB)
        rfb_construct(&app);
#endif //RFB
}

void app_cleanup(struct app_state_t *app)
{
    for (int i = 0; i < MAX_OUTPUTS && outputs[i].context != NULL; i++) {
        outputs[i].cleanup(&app);
    }
    for (int i = 0; i < MAX_FILTERS && filters[i].context != NULL; i++) {
        if (filters[i].is_started) CALL(filters[i].stop())
        filters[i].cleanup(&app);
    }
    if (input.is_started) CALL(input.stop());
    input.cleanup(&app);
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
    //DEBUG("bfs_adjacency_matrix: %lu", sizeof(bfs_adjacency_matrix));

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

    //DEBUG("distance: %d", distance);
    KL_DESTROY(bfs_qeue_t, bfs_qeue);
    return distance;
}

int app_init(struct app_state_t *app)
{
    CALL(input.init(), error);
    for (int i = 0; i < MAX_OUTPUTS && outputs[i].context != NULL; i++) {
        CALL(outputs[i].init(), error);
    }
        

    int filters_len = 0;
    for (int i = 0; i < MAX_FILTERS && filters[i].context != NULL; i++) {
        CALL(filters[i].init(&app), error);
        filters_len++;
    }

    const struct format_mapping_t* in_fs = NULL;
    int in_fs_len = input.get_formats(&in_fs);
    if (app->verbose) {
        char buffer1[MAX_STRING];
        char buffer2[MAX_STRING];

        buffer1[0] = '\0';
        for (int i = 0, k = 0; i < in_fs_len; i++) {
            if (in_fs[i].is_supported) {
                if (k > 0)
                    strcat(buffer1, COMMA);
                strcat(buffer1, app_get_video_format_str(in_fs[i].format));
                k++;
            }
        }
        DEBUG("input[%s]: %s", input.name, buffer1);

        for (int i = 0; filters[i].context != NULL && i < MAX_FILTERS; i++) {
            buffer1[0] = '\0';
            const struct format_mapping_t* fin_fs = NULL;
            int fin_fs_len = filters[i].get_in_formats(&fin_fs);
            for (int j = 0, k = 0; j < fin_fs_len; j++) {
                if (fin_fs[j].is_supported) {
                    if (k > 0)
                        strcat(buffer1, COMMA);
                    strcat(buffer1, app_get_video_format_str(fin_fs[j].format));
                    k++;
                }
            }
            buffer2[0] = '\0';
            const struct format_mapping_t* fout_fs = NULL;
            int fout_fs_len = filters[i].get_out_formats(&fout_fs);
            for (int j = 0, k = 0; j < fout_fs_len; j++) {
                if (fout_fs[j].is_supported) {
                    if (k > 0)
                        strcat(buffer2, COMMA);
                    strcat(buffer2, app_get_video_format_str(fout_fs[j].format));
                    k++;
                }
            }
            DEBUG("filter[%s]: %s -> %s",
                filters[i].name,
                buffer1,
                buffer2);
        }

        for (int i = 0; i < MAX_OUTPUTS && outputs[i].context != NULL; i++) {
            buffer1[0] = '\0';
            const struct format_mapping_t* out_fs = NULL;
            int out_fs_len = outputs[i].get_formats(&out_fs);
            for (int j = 0, k = 0; j < out_fs_len; j++) {
                if (out_fs[j].is_supported) {
                    if (k > 0)
                        strcat(buffer1, COMMA);
                    strcat(buffer1, app_get_video_format_str(out_fs[j].format));
                    k++;
                }
            }
            DEBUG("otput[%s]: %s", outputs[i].name, buffer1);
        }
    }

    for (int i = 0; i < MAX_OUTPUTS && outputs[i].context != NULL; i++) {
        const struct format_mapping_t* out_fs = NULL;
        int out_fs_len = outputs[i].get_formats(&out_fs);

        for (int ii = 0; ii < out_fs_len; ii++) {
            const struct format_mapping_t* out_f = out_fs + ii;
            if (!out_f->is_supported)
                continue;

            for (int jj = 0; jj < in_fs_len; jj++) {
                const struct format_mapping_t* in_f = in_fs + jj;

                if (!out_f->is_supported)
                    continue;

                find_path(in_f, out_f, filters_len, outputs + i);
            }
        }
    }
    if (app->verbose) {
        char buffer[MAX_STRING];
        for (int i = 0; i < MAX_OUTPUTS && outputs[i].context != NULL; i++) {
            if (outputs[i].start_format != 0) {
                buffer[0] = '\0';
                for (int k = 0; k < MAX_FILTERS && outputs[i].filters[k].out_format; k++) {
                    int index = outputs[i].filters[k].index;
                    strcat(buffer, " -> ");
                    strcat(buffer, filters[index].name);
                    strcat(buffer, "[");
                    strcat(buffer, app_get_video_format_str(outputs[i].filters[k].out_format));
                    strcat(buffer, "]");
                }
                DEBUG("path for %s, %s[%s]%s",
                    outputs[i].name,
                    input.name,
                    app_get_video_format_str(outputs[i].start_format),
                    buffer);
            } else {
                DEBUG("path for %s doesn't exist", outputs[i].name);
            }
        }
    }


    return 0;
error:
    if (errno == 0)
        errno = EAGAIN;
    return -1;
}
