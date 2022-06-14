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

#ifdef OPENCV
#include "opencv2/imgproc/imgproc_c.h"
#endif

KHASH_MAP_INIT_STR(argvs_hash_t, char *);
extern KHASH_T(argvs_hash_t) *h;

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

int utils_fill_buffer(const char *path, char *buffer, int buffer_size, size_t *read)
{
    FILE *fstream = fopen(path, "r");
    if (fstream == NULL) {
        CALL_MESSAGE(fopen(path, "r"));
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
        CALL_MESSAGE(fopen(path, "r"));
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

int utils_write_file(const char *path, const uint8_t *data, int len)
{
    FILE* fstream = NULL;
    if (strcmp(path, "stdout") == 0) {
        fstream = stdout;
    }
    else {
        fstream = fopen(path, "a");
        if (fstream == NULL)
            CALL_MESSAGE(fopen(path, "a"));
    }
    int written = fwrite(data, len, 1, fstream) * len;
    if (fstream && strcmp(path, "stdout") != 0) {
        fclose(fstream);
    }
    if (len <= 0 || written != len) {
        CALL_CUSTOM_MESSAGE("The file wasn't written, length: ", len);
        goto cleanup;
    }
    return 0;

cleanup:
    if (!errno) errno = EAGAIN;
    return -1;
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
            DEBUG("dx: %d, sx1: %d, sj1: %d, gfx: %2.2f, fx: %2.2f", dx, sx1, sj1, gfx, fx);
            DEBUG("p00: %d, p01: %d, p10: %d, p11: %d", p000, p001, p010, p011);
            DEBUG("b00: %d, b01: %d, b10: %d, b11: %d, blerp: %d", GET_B565(p000), GET_B565(p001),
                GET_B565(p010), GET_B565(p011), (int)blerp(GET_B565(p000), 
                GET_B565(p001), 
                GET_B565(p010),
                GET_B565(p011),
                dfx1, 
                dfy));
            DEBUG("g00: %d, g01: %d, g10: %d, g11: %d, blerp: %d", GET_G565(p000), GET_G565(p001),
                GET_G565(p010), GET_G565(p011), (int)blerp(GET_G565(p000), 
                GET_G565(p001), 
                GET_G565(p010),
                GET_G565(p011),
                dfx1, 
                dfy));
            DEBUG("r00: %d, r01: %d, r10: %d, r11: %d, blerp: %d", GET_R565(p000), GET_R565(p001),
                GET_R565(p010), GET_R565(p011), (int)blerp(GET_R565(p000), 
                GET_R565(p001),
                GET_R565(p010),
                GET_R565(p011),
                dfx1, 
                dfy));
            DEBUG("res: %d", res);
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
