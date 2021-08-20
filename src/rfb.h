#ifndef rfb_h
#define rfb_h

struct rfb_state_t {
    struct app_state_t *app;

    pthread_t thread;
    int thread_res;
    int serv_socket;
    int client_socket;
};

void rfb_construct(struct app_state_t *app);

#endif // rfb_h