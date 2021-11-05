#ifndef yuv_converter_h
#define yuv_converter_h

struct yuv_converter_state_t {
    struct app_state_t *app;

    uint8_t *buffer;
    int buffer_length;
};

void yuv_converter_construct(struct app_state_t *app);

#endif //yuv_converter_h