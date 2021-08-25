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

#define YUV_MEMORY_OPTIMIZATION

#include "main.h"
#include "utils.h"

#include "sdl.h"

static struct format_mapping_t sdl_formats[] = {
    {
        .format = VIDEO_FORMAT_YUV422,
        .internal_format = VIDEO_FORMAT_YUV422,
        .is_supported = 1
    }
};

static struct sdl_state_t sdl = {
    .app = NULL,
    .buffer = NULL,
    .buffer_len = 0,
    .window = NULL,
    //.renderer = NULL,
    .surface = NULL
};
extern struct output_t outputs[MAX_OUTPUTS];
extern int is_abort;

#ifdef YUV_MEMORY_OPTIMIZATION
// yuv422 to RGB lookup table
//
// Indexes are [Y][U][V]
// Y, Cb, Cr range is 0-255
//
// Stored value bits:
//   24-16 Red
//   15-8  Green
//   7-0   Blue
static int yuv422[256][256][256];

static void generate_yuv422_lookup()
{
    for (int y = 0; y < 256; y++) {
        for (int u = 0; u < 256; u++) {
            for (int v = 0; v < 256; v++) {

                int r = y + 1.370705 * (v - 128);
                int g = y - 0.698001 * (v - 128) - 0.337633 * (u - 128);
                int b = y + 1.732446 * (u - 128);
                r = MAX(0, MIN(255, r));
                g = MAX(0, MIN(255, g));
                b = MAX(0, MIN(255, b));

                int rgb = r << 16 | g << 8 | b;
                yuv422[y][u][v] = rgb;
            }
        }
    }
}
#endif //YUV_MEMORY_OPTIMIZATION

static void inline yuv422_to_rgb(const char* input, char *output)
{
    int y0 = input[0];
    int u = input[1];
    int y1 = input[2];
    int v = input[3];

#ifndef YUV_MEMORY_OPTIMIZATION
    int r0 = y0 + 1.370705 * (v - 128);
    int g0 = y0 - 0.698001 * (v - 128) - 0.337633 * (u - 128);
    int b0 = y0 + 1.732446 * (u - 128);
    output[0] = MAX(0, MIN(255, r0));
    output[1] = MAX(0, MIN(255, g0));
    output[2] = MAX(0, MIN(255, b0));

    int r1 = y1 + 1.370705 * (v - 128);
    int g1 = y1 - 0.698001 * (v - 128) - 0.337633 * (u - 128);
    int b1 = y1 + 1.732446 * (u - 128);
    output[3] = MAX(0, MIN(255, r1));
    output[4] = MAX(0, MIN(255, g1));
    output[5] = MAX(0, MIN(255, b1));
#else
    uint32_t rgb = yuv422[y0][u][v];
    output[0] = GET_R(rgb);
    output[1] = GET_G(rgb);
    output[2] = GET_B(rgb);

    rgb = yuv422[y1][u][v];
    output[3] = GET_R(rgb);
    output[4] = GET_G(rgb);
    output[5] = GET_B(rgb);
#endif
}

static int filter(void* data, union SDL_Event *event)
{
    return event->type == SDL_QUIT;
}

static int sdl_init()
{
#ifdef YUV_MEMORY_OPTIMIZATION
    generate_yuv422_lookup();
#endif //YUV_MEMORY_OPTIMIZATION

    SDL_INT_CALL(SDL_Init(SDL_INIT_VIDEO), cleanup);

    SDL_CALL(sdl.window = SDL_CreateWindow(
        "SDL Video viewer",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        sdl.app->video_width, sdl.app->video_height,
        0 //TODO: SDL_WINDOW_OPENGL
    ), cleanup);

    // SDL_CALL(sdl.renderer = SDL_CreateRenderer(
    //     sdl.window,
    //     -1,
    //     SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    // ), cleanup);

    int len = sdl.app->video_width * sdl.app->video_height * 3;
    char *data = malloc(len);
    if (data == NULL) {
        errno = ENOMEM;
        CALL_MESSAGE(malloc, 0);
        goto cleanup;
    }
    sdl.buffer = data;
    sdl.buffer_len = len;
    SDL_CALL(sdl.surface = SDL_CreateRGBSurfaceFrom(data,
        sdl.app->video_width,
        sdl.app->video_height,
        24,
        sdl.app->video_width * 3,
        R_888_MASK, G_888_MASK, B_888_MASK, 0
    ), cleanup);

    SDL_SetEventFilter(filter, NULL);
    return 0;

cleanup:
    errno = EAGAIN;
    return -1;
}

static int sdl_render()
{
    struct SDL_Surface *surface;
    SDL_CALL(surface = SDL_GetWindowSurface(sdl.window), cleanup);
    SDL_INT_CALL(
        SDL_BlitSurface(sdl.surface, NULL, surface, NULL),
        cleanup
    );
    SDL_INT_CALL(SDL_UpdateWindowSurface(sdl.window), cleanup);

    SDL_Event event;
    while (SDL_PollEvent(&event))
        if (event.type == SDL_QUIT)
            is_abort = 1;
    
    return 0;

cleanup:
    errno = EAGAIN;
    return -1;
}

static int sdl_render_yuv(char *buffer)
{
    int w = sdl.app->video_width;
    int h = sdl.app->video_height;
    int size = w * h;
    for (int i = 0; i < size; i += w)
        for (int x = 0; x < w; x += 2) {
            int  j = i + x;
            yuv422_to_rgb(buffer + (j << 1), sdl.buffer + j + (j << 1));
        }

    return sdl_render();
}

static void sdl_cleanup()
{
    if (sdl.surface) {
        SDL_FreeSurface(sdl.surface);
        sdl.surface = NULL;
    }
    if (sdl.buffer) {
        free(sdl.buffer);
        sdl.buffer = NULL;
    }
    /*if (sdl.renderer) {
        SDL_DestroyRenderer(sdl.renderer);
        sdl.renderer = NULL;
    }*/
    if (sdl.window) {
        SDL_DestroyWindow(sdl.window);
        sdl.window = NULL;
    }
    SDL_Quit();
}

static int sdl_get_formats(const struct format_mapping_t *formats[])
{
    if (formats != NULL)
        *formats = sdl_formats;
    return ARRAY_SIZE(sdl_formats);
}

void sdl_construct(struct app_state_t *app)
{
    int i = 0;
    while (outputs[i].context != NULL && i < MAX_OUTPUTS)
        i++;

    if (i != MAX_OUTPUTS) {
        sdl.app = app;
        outputs[i].name = "sdl";
        outputs[i].context = &sdl;
        outputs[i].init = sdl_init;
        outputs[i].render = sdl_render_yuv;
        outputs[i].cleanup = sdl_cleanup;
        outputs[i].get_formats = sdl_get_formats;
    }
}
