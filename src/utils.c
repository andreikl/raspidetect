#include "khash.h"

#include "main.h"
#include "utils.h"

#ifdef OPENCV
#include "opencv2/imgproc/imgproc_c.h"
#endif

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

void utils_print_help(void)
{
    printf("ov5647 [options]\n");
    printf("options:\n");
    printf("\n");
    printf("%s: help\n", HELP);
    printf("%s: width, default: %d\n", WIDTH, WIDTH_DEF);
    printf("%s: height, default: %d\n", HEIGHT, HEIGHT_DEF);
    printf("%s: port, default: %d\n", PORT, PORT_DEF);
    printf("%s: worke_width, default: %d\n", WORKER_WIDTH, WORKER_WIDTH_DEF);
    printf("%s: worker_height, default: %d\n", WORKER_HEIGHT, WORKER_HEIGHT_DEF);
    printf("%s: TFL model path, default: %s\n", TFL_MODEL_PATH, TFL_MODEL_PATH_DEF);    
    printf("%s: DN model path, default: %s\n", DN_MODEL_PATH, DN_MODEL_PATH_DEF);    
    printf("%s: DN config path, default: %s\n", DN_CONFIG_PATH, DN_CONFIG_PATH_DEF);    
    printf("%s: input, default: %s\n", INPUT, INPUT_DEF);
    printf("%s: output, default: %s\n", OUTPUT, OUTPUT_DEF);
    printf("\toptions: "ARG_STREAM" - output stream, "ARG_RFB" - output rfb, "ARG_NONE"\n");
    printf("%s: verbose, verbose: %d\n", VERBOSE, VERBOSE_DEF);
    exit(0);
}

void utils_default_status(app_state_t *state)
{
    memset(state, 0, sizeof(app_state_t));

    int width = utils_read_int_value(WIDTH, WIDTH_DEF);
    int height = utils_read_int_value(HEIGHT, HEIGHT_DEF);
    state->width = (width / 32 * 32) + (width % 32? 32: 0);
    state->height = (height / 16 * 16) + (height % 16? 16: 0);
    state->bits_per_pixel = 16;
    state->port = utils_read_int_value(PORT, PORT_DEF);
    state->worker_width = utils_read_int_value(WORKER_WIDTH, WORKER_WIDTH_DEF);
    state->worker_height = utils_read_int_value(WORKER_HEIGHT, WORKER_HEIGHT_DEF);
    state->worker_bits_per_pixel = 24;
    state->worker_total_objects = 10;
    state->worker_thread_res = -1;
    state->verbose = utils_read_int_value(VERBOSE, VERBOSE_DEF);
#ifdef RFB
    state->rfb.thread_res = -1;
#endif
#ifdef TENSORFLOW
    state->model_path = utils_read_str_value(TFL_MODEL_PATH, TFL_MODEL_PATH_DEF);
#elif DARKNET
    state->model_path = utils_read_str_value(DN_MODEL_PATH, DN_MODEL_PATH_DEF);
    state->config_path = utils_read_str_value(DN_CONFIG_PATH, DN_CONFIG_PATH_DEF);
#endif
    char *output = utils_read_str_value(OUTPUT, OUTPUT_DEF);
    if (strcmp(output, ARG_STREAM) == 0) {
        state->output_type = OUTPUT_STREAM;
    } else if (strcmp(output, ARG_RFB) == 0) {
        state->output_type = OUTPUT_RFB;
    }
}

char *utils_read_str_value(const char *name, char *def_value)
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
    unsigned char buffer[BUFFER_SIZE];
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
        if (i + 3 >= BUFFER_SIZE) {
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

void utils_get_cpu_load(char * buffer, cpu_state_t *state)
{
    utils_fill_buffer("/proc/stat", buffer, BUFFER_SIZE, NULL);

    int user, nice, system, idle;
    sscanf(&buffer[4], "%d %d %d %d", &user, &nice, &system, &idle);

    int load = user + nice + system, all = load + idle;
    static int last_load = 0, last_all = 0;
    float cpu = (load - last_load) / (float)(all - last_all) * 100;

    last_load = user + nice + system;
    last_all = load + idle;

    state->cpu = cpu;
}


void utils_get_memory_load(char * buffer, memory_state_t *state)
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
    utils_fill_buffer("/proc/self/status", buffer, BUFFER_SIZE, NULL);
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

void utils_get_temperature(char * buffer, temperature_state_t *state)
{
    utils_fill_buffer("/sys/class/thermal/thermal_zone0/temp", buffer, BUFFER_SIZE, NULL);

    state->temp = (float)(atoi(buffer)) / 1000;
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

int utils_get_worker_buffer(app_state_t *state)
{
#ifdef OPENCV
    CvMat *img1 = cvCreateMatHeader(state->width,
                                    state->height,
                                    CV_8UC2);
    cvSetData(img1, state->openvg.video_buffer, state->width << 1);

    CvMat *img2 = cvCreateMatHeader(state->worker_width,
                                    state->worker_height,
                                    CV_8UC2);
    cvSetData(img2, state->worker_buffer_565, state->worker_width << 1);

    cvResize(img1, img2, CV_INTER_LINEAR);

    cvRelease(&img1);
    cvRelease(&img2);
#else
    int res = resize_li_16(state->openvg.video_buffer.i,
                state->width,
                state->height,
                (int*)state->worker_buffer_565,
                state->worker_width,
                state->worker_height);
    if (res != 0) {
        fprintf(stderr, "ERROR: Failed to resize image %d\n", res);
        return -1;
    }
#endif
    // openvg implementation
    // vgImageSubData(state->openvg.video_image,
    //             state->openvg.video_buffer,
    //             state->width << 1,
    //             VG_sRGB_565,
    //             0, 0,
    //             state->width, state->height);

    // vgSeti(VG_MATRIX_MODE, VG_MATRIX_IMAGE_USER_TO_SURFACE);
    // vgLoadIdentity();
    // vgScale(((float)state->worker_width) / state->width, ((float)state->worker_height) / state->height);
    // vgDrawImage(state->openvg.video_image);

    // vgReadPixels(   state->worker_buffer_565,
    //                 state->width << 1,
    //                 VG_sRGB_565,
    //                 0, 0,
    //                 state->worker_width, state->worker_height);
#if defined(ENV32BIT)
    int32_t *buffer_565 = (int32_t *)state->worker_buffer_565;
    int32_t *buffer_rgb = (int32_t *)state->worker_buffer_rgb;
    int i = 0, j = 0;
    int l = state->worker_width * state->worker_height >> 1;
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

char* get_mmal_message(int result)
{
    if (result == MMAL_SUCCESS) {
        return "MMAL_SUCCESS";
    }
    else if (result == MMAL_ENOMEM) {
        return "MMAL_ENOMEM: Out of memory";
    }
    else if (result == MMAL_ENOSPC) {
        return "MMAL_ENOSPC: Out of resources (other than memory)";
    }
    else if (result == MMAL_EINVAL) {
        return "MMAL_EINVAL: Argument is invalid";
    }
    else if (result == MMAL_ENOSYS) {
        return "MMAL_ENOSYS: Function not implemented";
    }
    else if (result == MMAL_ENOENT) {
        return "MMAL_ENOENT: No such file or directory";
    }
    else if (result == MMAL_ENXIO) {
        return "MMAL_ENXIO: No such device or address";
    }
    else if (result == MMAL_EIO) {
        return "MMAL_EIO: I/O error";
    }
    else if (result == MMAL_ESPIPE) {
        return "MMAL_ESPIPE: Illegal seek";
    }
    else if (result == MMAL_ECORRUPT) {
        return "MMAL_ECORRUPT: Data is corrupt";
    }
    else if (result == MMAL_ENOTREADY) {
        return "MMAL_ENOTREADY: Component is not ready";
    }
    else if (result == MMAL_ECONFIG) {
        return "MMAL_ECONFIG: Component is not configured";
    }
    else if (result == MMAL_EISCONN) {
        return "MMAL_EISCONN: Port is already connected";
    }
    else if (result == MMAL_ENOTCONN) {
        return "MMAL_ENOTCONN: Port is disconnected";
    }
    else if (result == MMAL_EAGAIN) {
        return "MMAL_EAGAIN: Resource temporarily unavailable. Try again later";
    }
    else if (result == MMAL_EFAULT) {
        return "MMAL_EFAULT: Bad address";
    }
    else {
        return "UNKNOWN";
    }
}
