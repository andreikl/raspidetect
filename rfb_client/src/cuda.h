#define CUDA_API_CALL(call, error) \
{ \
    CUresult res = call; \
    if (res != CUDA_SUCCESS) { \
        fprintf(stderr, "ERROR: "#call" returned error '%s'(%d)\n%s:%s:%d\n", cuda_get_error_message(res), res, __FILE__, __FUNCTION__, __LINE__); \
        goto error; \
    } \
} \

int cuda_destroy(struct app_state_t *app);
int cuda_init(struct app_state_t *app);