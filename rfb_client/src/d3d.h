void d3d_destroy(struct app_state_t *app);
int d3d_init(struct app_state_t *app);

int d3d_render_frame(struct app_state_t *app);
int d3d_render_image(struct app_state_t *app);

char* d3d_get_hresult_message(HRESULT result);

int d3d_start(struct app_state_t* app);

#define D3D_CALL(call, error) \
{ \
    HRESULT hres = call; \
    if (FAILED(hres)) {\
        fprintf(stderr, "ERROR: "#call" returned error: %s(%ld)\n%s:%d - %s\n", \
            d3d_get_hresult_message(hres), hres, __FILE__, __LINE__, __FUNCTION__); \
        goto error; \
    } \
} \

