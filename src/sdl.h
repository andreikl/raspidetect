#ifndef sdl_h
#define sdl_h

#define SDL_MESSAGE(call, res) \
{ \
    fprintf(stderr, "ERROR: "#call" returned error: %s (%d)\n%s:%d - %s\n", \
        SDL_GetError(), res, __FILE__, __LINE__, __FUNCTION__); \
}

#define SDL_2(call, error) \
{ \
    int __res = call; \
    if (__res < 0) { \
        SDL_MESSAGE(call, __res); \
        goto error; \
    } \
}

#define SDL_1(call) \
{ \
    int __res = call; \
    if (__res < 0) { \
        SDL_MESSAGE(call, __res); \
    } \
}

#define SDL_X(...) GET_3RD_ARG(__VA_ARGS__, SDL_2, SDL_1, )

#define SDL_CALL(...) SDL_X(__VA_ARGS__)(__VA_ARGS__)

#include <SDL.h>

struct sdl_state_t {
    char* buffer;
    int buffer_length;
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Surface *surface;
};

void sdl_construct(struct app_state_t *app);

#endif // main_h