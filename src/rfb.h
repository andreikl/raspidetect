#ifndef rfb_h
#define rfb_h

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