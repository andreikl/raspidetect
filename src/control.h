#ifndef control_h
#define control_h

struct control_state_t {
    struct extension_t *extension;
    int is_started;
    volatile unsigned *gpio;
};

void control_construct();

#endif // control_h