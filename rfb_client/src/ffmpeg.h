#define FFMPEG_CALL_MESSAGE(call, res) \
{ \
    fprintf(stderr, "ERROR: "#call" returned error: %s (%d)\n%s:%d - %s\n", \
        av_err2str(res), res, __FILE__, __LINE__, __FUNCTION__); \
}

#define FFMPEG_CALL_2(call, error) \
{ \
    int res__ = call; \
    if (res__ != 0) { \
        FFMPEG_CALL_MESSAGE(call, res__); \
        goto error; \
    } \
} \

#define FFMPEG_CALL_1(call) \
{ \
    int res__ = call; \
    if (res__ != 0) { \
        FFMPEG_CALL_MESSAGE(call, res__); \
    } \
} \

#define FFMPEG_CALL_X(...) GET_3RD_ARG(__VA_ARGS__, FFMPEG_CALL_2, FFMPEG_CALL_1, )

#define FFMPEG_CALL(...) FFMPEG_CALL_X(__VA_ARGS__)(__VA_ARGS__)

void ffmpeg_destroy();
int ffmpeg_init();
int ffmpeg_decode();