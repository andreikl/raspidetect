#ifndef sdl_h
#define sdl_h

#define SDL_MESSAGE(call) \
{ \
    fprintf(stderr, "\033[1;31m"#call" returned error: %s\n%s:%d - %s\033[0m\n", \
        SDL_GetError(), __FILE__, __LINE__, __FUNCTION__); \
}

#define SDL_INT_CALL(call, error) \
{ \
    int __res = call; \
    if (__res != 0) { \
        fprintf(stderr, "\033[1;31m"#call" returned error: %s (%d)\n%s:%d - %s\033[0m\n", \
            SDL_GetError(), __res, __FILE__, __LINE__, __FUNCTION__); \
        goto error; \
    } \
}

#define SDL_2(call, error) \
{ \
    void *__res = call; \
    if (__res == NULL) { \
        SDL_MESSAGE(call); \
        goto error; \
    } \
}

#define SDL_1(call) \
{ \
    void * __res = call; \
    if (__res == NULL) { \
        SDL_MESSAGE(call); \
    } \
}

#define SDL_X(...) GET_3RD_ARG(__VA_ARGS__, SDL_2, SDL_1, )

#define SDL_CALL(...) SDL_X(__VA_ARGS__)(__VA_ARGS__)

#include <SDL.h>

struct sdl_state_t {
    struct output_t *output;

    uint8_t* buffer;
    int buffer_len;
    SDL_Window *window;
    //SDL_Renderer *renderer;
    SDL_Surface *surface;
};

void sdl_construct();

#endif // sdl_h