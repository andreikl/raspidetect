#ifndef file_h
#define file_h

struct file_state_t {
    struct output_t *output;
    int is_started;
};

void file_construct();

#endif // file_h