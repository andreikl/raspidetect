#include "main.h"
#include "utils.h"

#include "sdl.h"

static struct sdl_state_t sdl = {
    .buffer = NULL,
    .buffer_length = 0,
    .window = NULL,
    .renderer = NULL,
    .surface = NULL
};
extern struct output_t outputs[VIDEO_MAX_OUTPUTS];

/* 
 * YCbCr to RGB lookup table
 *
 * Indexes are [Y][Cb][Cr]
 * Y, Cb, Cr range is 0-255
 *
 * Stored value bits:
 *   24-16 Red
 *   15-8  Green
 *   7-0   Blue
 */
uint32_t YCbCr_to_RGB[256][256][256];

static void generate_YCbCr_to_RGB_lookup()
{
    for (int y = 0; y < 256; y++) {
        for (int cb = 0; cb < 256; cb++) {
            for (int cr = 0; cr < 256; cr++) {
                double Y = (double)y;
                double Cb = (double)cb;
                double Cr = (double)cr;

                int R = (int)(Y + 1.40200 * (Cr - 0x80));
                int G = (int)(Y - 0.34414 * (Cb - 0x80) - 0.71414 * (Cr - 0x80));
                int B = (int)(Y + 1.77200 * (Cb - 0x80));

                R = MAX(0, MIN(255, R));
                G = MAX(0, MIN(255, G));
                B = MAX(0, MIN(255, B));

                YCbCr_to_RGB[y][cb][cr] = R << 16 | G << 8 | B;
            }
        }
    }
}

static void inline yuv422_to_rgb(const char* input, char *output)
{
    int y0 = input[0];
    int cb = input[1];
    int y1 = input[2];
    int cr = input[3];

    uint32_t rgb = YCbCr_to_RGB[y0][cb][cr];
    output[0] = GET_R(rgb);
    output[1] = GET_G(rgb);
    output[2] = GET_B(rgb);

    rgb = YCbCr_to_RGB[y1][cb][cr];
    output[3] = GET_R(rgb);
    output[4] = GET_G(rgb);
    output[5] = GET_B(rgb);
}

static int filter(void* data, union SDL_Event *event)
{
    return event->type == SDL_QUIT;
}

static int sdl_init(struct app_state_t *app)
{
    generate_YCbCr_to_RGB_lookup();

    SDL_CALL(SDL_Init(SDL_INIT_VIDEO), error)
        return 1;

    sdl.window = SDL_CreateWindow(
        "SDL Video viewer",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        app->video_width, app->video_height,
        SDL_WINDOW_OPENGL);
    if (sdl.window == NULL) {
        SDL_MESSAGE(SDL_CreateWindow, 0);
        goto error;
    }

    sdl.renderer = SDL_CreateRenderer(
        sdl.window,
        -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );
    if (sdl.renderer == NULL) {
        SDL_MESSAGE(SDL_CreateRenderer, 0);
        goto error;
    }

    int len = app->video_width * app->video_height * 3;
    char *data = malloc(len);
    if (data == NULL) {
        errno = ENOMEM;
        CALL_MESSAGE(malloc, 0);
        goto error;
    }
    sdl.buffer = data;
    sdl.buffer_length = len;

    sdl.surface = SDL_CreateRGBSurfaceFrom(data,
        app->video_width,
        app->video_height,
        24,
        app->video_width * 3,
        R_888_MASK, G_888_MASK, B_888_MASK, 0);
    if (sdl.surface == NULL) {
        SDL_MESSAGE(SDL_CreateRGBSurfaceFrom, 0);
        goto error;
    }

    SDL_SetEventFilter(filter, NULL);
    return 0;
error:
    errno = EAGAIN;
    return -1;
}

static int sdl_render_rgb(struct app_state_t *app, char *buffer)
{
    errno = EAGAIN;
    return -1;
}

static int sdl_render_yuv(struct app_state_t *app, char *buffer)
{
    int w = app->video_width;
    int h = app->video_height;
    int size = w * h;
    for (int i = 0; i < size; i += w)
        for (int x = 0; x < w; x += 2) {
            int  j = i + x;
            yuv422_to_rgb(buffer + (j << 1), sdl.buffer + j + (j << 1));
        }

    return sdl_render_rgb(app, buffer);
}

static void sdl_cleanup(struct app_state_t *app)
{
    if (sdl.surface) {
        SDL_FreeSurface(sdl.surface);
        sdl.surface = NULL;
    }
    if (sdl.buffer) {
        free(sdl.buffer);
        sdl.buffer = NULL;
    }
    if (sdl.renderer) {
        SDL_DestroyRenderer(sdl.renderer);
        sdl.renderer = NULL;
    }
    if (sdl.window) {
        SDL_DestroyWindow(sdl.window);
        sdl.window = NULL;
    }
    SDL_Quit();
}

void sdl_construct(struct app_state_t *app)
{
    int i = 0;
    while (outputs[i].context != NULL && i < VIDEO_MAX_OUTPUTS)
        i++;

    if (i != VIDEO_MAX_OUTPUTS) {
        outputs[i].context = &sdl;
        outputs[i].init = sdl_init;
        outputs[i].render_yuv = sdl_render_yuv;
        outputs[i].render_rgb = sdl_render_rgb;
        outputs[i].cleanup = sdl_cleanup;
    }
}
