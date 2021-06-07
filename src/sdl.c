#include "main.h"
#include "utils.h"

#include "sdl.h"

static struct sdl_state_t sdl = {
    .buffer = NULL,
    .buffer_length = 0,
    .window = NULL,
    //.renderer = NULL,
    .surface = NULL
};
extern struct output_t outputs[VIDEO_MAX_OUTPUTS];

// yuv422 to RGB lookup table
//
// Indexes are [Y][U][V]
// Y, Cb, Cr range is 0-255
//
// Stored value bits:
//   24-16 Red
//   15-8  Green
//   7-0   Blue
static int yuv422[256][16][16];

static void generate_yuv422_lookup()
{
    for (int y = 0; y < 256; y++) {
        for (int u = 0; u < 16; u++) {
            for (int v = 0; v < 16; v++) {

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

static void inline yuv422_to_rgb(const char* input, char *output)
{
    int y0 = input[0];
    int u = input[1];
    int y1 = input[2];
    int v = input[3];

    uint32_t rgb = yuv422[y0][GET_F_HI(u)][GET_F_HI(v)];
    output[0] = GET_R(rgb);
    output[1] = GET_G(rgb);
    output[2] = GET_B(rgb);

    rgb = yuv422[y1][GET_F_LO(u)][GET_F_LO(v)];
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
    generate_yuv422_lookup();

    SDL_INT_CALL(SDL_Init(SDL_INIT_VIDEO), cleanup);

    SDL_CALL(sdl.window = SDL_CreateWindow(
        "SDL Video viewer",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        app->video_width, app->video_height,
        0 //TODO: SDL_WINDOW_OPENGL
    ), cleanup);

    // SDL_CALL(sdl.renderer = SDL_CreateRenderer(
    //     sdl.window,
    //     -1,
    //     SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    // ), cleanup);

    int len = app->video_width * app->video_height * 3;
    char *data = malloc(len);
    if (data == NULL) {
        errno = ENOMEM;
        CALL_MESSAGE(malloc, 0);
        goto cleanup;
    }
    sdl.buffer = data;
    sdl.buffer_length = len;
    SDL_CALL(sdl.surface = SDL_CreateRGBSurfaceFrom(data,
        app->video_width,
        app->video_height,
        24,
        app->video_width * 3,
        R_888_MASK, G_888_MASK, B_888_MASK, 0
    ), cleanup);

    SDL_SetEventFilter(filter, NULL);
    return 0;

cleanup:
    errno = EAGAIN;
    return -1;
}

static int sdl_render(struct app_state_t *app)
{
    struct SDL_Surface *surface;
    SDL_CALL(surface = SDL_GetWindowSurface(sdl.window), cleanup);
    SDL_INT_CALL(
        SDL_BlitSurface(sdl.surface, NULL, surface, NULL),
        cleanup
    );
    SDL_INT_CALL(SDL_UpdateWindowSurface(sdl.window), cleanup);
    return 0;

cleanup:
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

    return sdl_render(app);
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

void sdl_construct(struct app_state_t *app)
{
    int i = 0;
    while (outputs[i].context != NULL && i < VIDEO_MAX_OUTPUTS)
        i++;

    if (i != VIDEO_MAX_OUTPUTS) {
        outputs[i].context = &sdl;
        outputs[i].init = sdl_init;
        outputs[i].render_yuv = sdl_render_yuv;
        outputs[i].cleanup = sdl_cleanup;
    }
}
