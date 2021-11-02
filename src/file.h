#ifndef file_h
#define file_h

struct file_state_t {
    struct app_state_t *app;
    struct output_t *output;
};

void file_construct(struct app_state_t *app);

#endif // file_h