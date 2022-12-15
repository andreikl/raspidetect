#ifndef rfb_h
#define rfb_h

// doesn't show error if rfb is closed but thread is still running
#define RFB_FUNC_CALL(call, error) \
{ \
    int __res = call; \
    if (__res == -1 && (errno == 9 || errno == 104) ) { \
        goto error; \
    } \
    if (__res == -1) { \
        CALL_MESSAGE(call); \
        goto error; \
    } \
}

struct rfb_state_t {
    struct output_t *output;

    pthread_t thread;
    int thread_res;
    int server_socket;

    int client_socket;
    sem_t client_semaphore;
    int client_semaphore_res;
};

void rfb_construct();

#endif // rfb_h