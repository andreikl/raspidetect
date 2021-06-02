#include "main.h"
#include "utils.h"

#include "sdl.h"

static int sdl_filter(void* data, union SDL_Event *event)
{
    return event->type == SDL_QUIT;
}

int sdl_init(struct app_state_t *app)
{
    SDL_CALL(SDL_Init(SDL_INIT_VIDEO), error)
        return 1;

    app->sdl.window = SDL_CreateWindow(
        "SDL Video viewer",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        app->video_width, app->video_height,
        SDL_WINDOW_OPENGL);
    if (app->sdl.window == NULL) {
        SDL_MESSAGE(SDL_CreateWindow, 0);
        goto error;
    }

    app->sdl.renderer = SDL_CreateRenderer(
        app->sdl.window,
        -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );
    if (app->sdl.renderer == NULL) {
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
    app->sdl.buffer = data;
    app->sdl.buffer_length = len;

    app->sdl.surface = SDL_CreateRGBSurfaceFrom(data,
        app->video_width,
        app->video_height,
        24,
        app->video_width * 3,
        R_888_MASK, G_888_MASK, B_888_MASK, 0);
    if (app->sdl.surface == NULL) {
        SDL_MESSAGE(SDL_CreateRGBSurfaceFrom, 0);
        goto error;
    }

    SDL_SetEventFilter(sdl_filter, NULL);
    return 0;
error:
    errno = EAGAIN;
    return -1;
}

void sdl_cleanup(struct app_state_t *app)
{
    if (app->sdl.surface) {
        SDL_FreeSurface(app->sdl.surface);
        app->sdl.surface = NULL;
    }
    if (app->sdl.buffer) {
        free(app->sdl.buffer);
        app->sdl.buffer = NULL;
    }
    if (app->sdl.renderer) {
        SDL_DestroyRenderer(app->sdl.renderer);
        app->sdl.renderer = NULL;
    }
    if (app->sdl.window) {
        SDL_DestroyWindow(app->sdl.window);
        app->sdl.window = NULL;
    }
    SDL_Quit();
}