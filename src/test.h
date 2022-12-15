
#define TEST_RFB "--rfb"
#define TEST_RFB_DEF "false"
#define WRAP_VERBOSE "-wv"
#define WRAP_VERBOSE_DEF 0

#define TEST_DEBUG(format, ...) \
{ \
    if (test_verbose) \
        fprintf(stderr, "\033[0;35m: %s:%d - %s, "#format"\033[0m\n", \
            __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__); \
}

#define WRAP_DEBUG(format, ...) \
{ \
    if (wrap_verbose) \
        fprintf(stderr, "\033[1;37m%s:%d - %s, "#format"\033[0m\n", \
            __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__); \
}
