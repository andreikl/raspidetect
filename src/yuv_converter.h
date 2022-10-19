#ifndef yuv_converter_h
#define yuv_converter_h

struct yuv_converter_state_t {
    uint8_t *buffer;
    int buffer_length;
};

void yuv_converter_construct();

#endif //yuv_converter_h