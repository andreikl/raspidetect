#ifndef control_h
#define control_h

//#define GPIO_DEVICE "/dev/gpiomem"
#define GPIO_DEVICE "/dev/mem"

struct control_state_t {
    char dev_name[20];

    struct extension_t *extension;
    int is_started;
    volatile unsigned *gpio;
};

void control_construct();

#endif // control_h