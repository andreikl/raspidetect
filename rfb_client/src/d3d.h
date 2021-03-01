void d3d_destroy(struct app_state_t *app);
int d3d_init(struct app_state_t *app);

int d3d_render_frame(struct app_state_t *app);
int d3d_render_image(struct app_state_t *app);

const char* d3d_get_hresult_message(HRESULT result);

int d3d_start(struct app_state_t* app);

#define D3D_CALL_2(call, error) \
{ \
    HRESULT hres = call; \
    if (FAILED(hres)) {\
        fprintf(stderr, "ERROR: "#call" returned error: %s(%ld)\n%s:%d - %s\n", \
            d3d_get_hresult_message(hres), hres, __FILE__, __LINE__, __FUNCTION__); \
        goto error; \
    } \
}

#define D3D_CALL_1(call) \
{ \
    HRESULT hres = call; \
    if (FAILED(hres)) {\
        fprintf(stderr, "ERROR: "#call" returned error: %s(%ld)\n%s:%d - %s\n", \
            d3d_get_hresult_message(hres), hres, __FILE__, __LINE__, __FUNCTION__); \
    } \
} \

#define D3D_CALL_X(...) GET_3RD_ARG(__VA_ARGS__, D3D_CALL_2, D3D_CALL_1, )
#define D3D_CALL(...) D3D_CALL_X(__VA_ARGS__)(__VA_ARGS__)
