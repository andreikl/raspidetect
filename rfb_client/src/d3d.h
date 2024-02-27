#define D3D_MESSAGE(call, hres__) \
{ \
    fprintf(stderr, "\033[1;31m"#call" returned error: %s(%ld)\n%s:%d - %s\033[0m\n", \
        d3d_get_hresult_message(hres__), hres__, __FILE__, __LINE__, __FUNCTION__); \
}

#define D3D_CALL_2(call, error) \
{ \
    HRESULT hres__ = call; \
    if (FAILED(hres__)) {\
        D3D_MESSAGE(call, hres__); \
        goto error; \
    } \
} \

#define D3D_CALL_1(call) \
{ \
    HRESULT hres__ = call; \
    if (FAILED(hres__)) {\
        D3D_MESSAGE(call, hres__); \
    } \
} \

#define D3D_CALL_X(...) GET_3RD_ARG(__VA_ARGS__, D3D_CALL_2, D3D_CALL_1, )
#define D3D_CALL(...) D3D_CALL_X(__VA_ARGS__)(__VA_ARGS__)

void d3d_destroy();
int d3d_init();

int d3d_render_frame();
int d3d_render_image();

char* d3d_get_hresult_message(HRESULT result);

int d3d_start();
