#include "main.h"

#include <netinet/in.h> //sockaddr_in
#include <netinet/tcp.h> //TCP_NODELAY

#define RFB_MAX_CONNECTIONS 1
#define RFB_SECURITY_NONE 1

enum rfb_request_enum {
    RFBSetPixelFormat = 0,
    RFBSetEncodings = 2,
    RFBFramebufferUpdateRequest = 3,
    RFBKeyEvent = 4,
    RFBPointerEvent = 5,
    RFBClientCutText = 6
};

enum rfb_response_enum {
    RFBFramebufferUpdate = 0,
    RFBSetColorMapEntries = 1,
    RFBBell = 2,
    RFBServerCutText = 3
};

enum rfb_encoding_enum {
    RFBRaw = 0,
    RFBCopyRect = 1,
    RFBRRE = 2,
    RFBHextile = 5,
    RFBTRLE = 15,
    RFBZRLE = 16,
    RFBCursorPseudoEncoding = -239,
    RFBDesktopSizePseudoEncoding = -223,
    RFBEncodingH264 = 0x48323634
};

typedef struct rfb_pixel_format_t {
    uint8_t bpp;
    uint8_t depth;
    uint8_t big_endian;
    uint8_t true_color;
    uint16_t red_max;
    uint16_t green_max;
    uint16_t blue_max;
    uint8_t red_shift;
    uint8_t green_shift;
    uint8_t blue_shift;
    char padding[3];
} rfb_pixel_format_t;

// server messages ------------------------------
typedef struct rfb_security_message_t {
    uint8_t types_count;
    uint8_t types;
} rfb_security_message_t;

typedef struct rfb_server_init_message_t {
    uint16_t framebuffer_width;
    uint16_t framebuffer_height;
    rfb_pixel_format_t pixel_format;
    uint32_t name_length;
    char name[16];
} rfb_server_init_message_t;

typedef struct rfb_buffer_update_message_t {
    uint8_t message_type;
    uint8_t padding;
    uint16_t number_of_rectangles;
    uint16_t x;
    uint16_t y;
    uint16_t width;
    uint16_t height;
    uint32_t encoding_type;
} rfb_buffer_update_message_t;

// client messages ------------------------------
typedef struct rfb_type_request_message_t {
    uint8_t message_type;
    uint8_t temp;
} rfb_type_request_message_t;

typedef struct rfb_pixel_format_request_message_t {
    char padding[2];
    rfb_pixel_format_t f;
} rfb_pixel_format_request_message_t;

typedef struct rfb_buffer_update_request_message_t {
    uint16_t x;
    uint16_t y;
    uint16_t width;
    uint16_t height;
} rfb_buffer_update_request_message_t;

typedef struct rfb_set_encoding_request_message_t {
    uint16_t number_of_encodings;
} rfb_set_encoding_request_message_t;
// -----------------------------------------------

extern int is_abort;

static rfb_buffer_update_message_t update_message;

static void *rfb_function(void *data)
{
    int res;

    const int one = 1;    
    struct app_state_t * app = (struct app_state_t*) data;
    if (app->verbose) {
        fprintf(stderr, "INFO: RFB thread has been started\n");
    }
    while (!is_abort) {
        struct sockaddr_in client_addr;
        int addres_len = sizeof(client_addr);

        app->rfb.client_socket = accept(app->rfb.serv_socket,
            (struct sockaddr *)&client_addr, 
            (socklen_t *)&addres_len);
        // stop RFB thread
        if (is_abort) {
            return NULL;
        }
        if (app->rfb.client_socket < 0) {
            fprintf(stderr, "ERROR: Can't accept RFB client socket. res: %d\n", errno);
            continue;
        }

        if (setsockopt(app->rfb.client_socket, IPPROTO_TCP, TCP_NODELAY, (char *)&one, sizeof(one)) < 0) {
            fprintf(stderr, "ERROR: Can't set RFB client socket options. res: %d\n", errno);
            goto rfb_error;
        }

        char* rfb_version = "RFB 003.008\n\0";
        size_t rfb_version_length = strlen(rfb_version);
        if (send(app->rfb.client_socket, rfb_version, rfb_version_length, 0) < 0) {
            fprintf(stderr, "ERROR: Can't send RFB version message. res: %d\n", errno);
            goto rfb_error;
        }

        char client_rfb_version[12];
        if (recv(app->rfb.client_socket, client_rfb_version, sizeof(client_rfb_version), 0) < 0) {
            fprintf(stderr, "ERROR: Can't receive RFB version message. res: %d\n", errno);
            goto rfb_error;
        }

        if (app->verbose) {
            fprintf(stderr, "INFO: Client rfb version. %s\n", client_rfb_version);
        }

        rfb_security_message_t security = {
            .types_count = 1,
            .types = RFB_SECURITY_NONE
        };
        if (send(app->rfb.client_socket, (char *)&security , sizeof(security), 0) < 0) {
            fprintf(stderr, "ERROR: Can't send RFB security message. res: %d\n", errno);
            goto rfb_error;
        }

        uint8_t client_security_type = 0;
        if (recv(app->rfb.client_socket, (char *)&client_security_type, sizeof(client_security_type), 0) < 0) {
            fprintf(stderr, "ERROR: Can't receive RFB security message. res: %d\n", errno);
            goto rfb_error;
        }

        if (app->verbose) {
            fprintf(stderr, "INFO: Client rfb security type. %d\n", client_security_type);
        }
    
        uint32_t success = 0;
        if (send(app->rfb.client_socket, (char *)&success, sizeof(success), 0) < 0) {
            fprintf(stderr, "ERROR: Can't send RFB success message. res: %d\n", errno);
            goto rfb_error;
        }

        uint8_t shared_flag;
        if (recv(app->rfb.client_socket, (char *)&shared_flag, sizeof(shared_flag), 0) < 0) {
            fprintf(stderr, "ERROR: Can't receive RFB shared flag message. res: %d\n", errno);
            goto rfb_error;
        }

        if (app->verbose) {    
            fprintf(stderr, "INFO: Client rfb shared flag. %d\n", shared_flag);
        }
    
        rfb_server_init_message_t init_message = {
            .framebuffer_width = htons(app->width),
            .framebuffer_height = htons(app->height),
            .pixel_format.bpp = app->bits_per_pixel,
            .pixel_format.depth = app->bits_per_pixel,
            .pixel_format.big_endian = 1,
            .pixel_format.true_color = 1,
            .pixel_format.red_max = htons(31),
            .pixel_format.green_max = htons(63),
            .pixel_format.blue_max = htons(31),
            .pixel_format.red_shift = 11,
            .pixel_format.green_shift = 5,
            .pixel_format.blue_shift = 0,
            .name_length = htonl(sizeof(init_message.name))
        };
        memset(init_message.name, 0, sizeof(init_message.name));
        memcpy(init_message.name, APP_NAME, strlen(APP_NAME));

        //fprintf(stderr, "INFO: il: %d, nl: %d\n", sizeof(init_message.name), sizeof(init_message));
        if (send(app->rfb.client_socket, (char *)&init_message, sizeof(init_message), 0) < 0) {
            fprintf(stderr, "ERROR: send failed to send RFB init message. res: %d\n", errno);
            goto rfb_error;
        }

        if (camera_create_h264_encoder(app)) {
            fprintf(stderr, "ERROR: camera_create_h264_encoder failed\n");
            goto rfb_error;
        }

        rfb_buffer_update_request_message_t buffer_update;
        do {
            rfb_type_request_message_t type;
            res = recv(app->rfb.client_socket, (char *)&type, sizeof(type), 0);
            if (res < 0) {
                fprintf(stderr, "ERROR: Can't get message_type message. res: %d\n", errno);
                goto rfb_error;
            } if (res == 0) {
                if (app->verbose) {
                    fprintf(stderr, "INFO: Client has closed the connection\n");
                }
                break;
            }

            if (type.message_type == RFBSetPixelFormat)  {
                rfb_pixel_format_request_message_t format;
                if (recv(app->rfb.client_socket, (char *)&format, sizeof(format), 0) < 0) {
                    fprintf(stderr, "ERROR: Can't get RFBSetPixelFormat message. res: %d\n", errno);
                    goto rfb_error;
                }
                fprintf(stderr, "INFO: RFBSetPixelFormat message.\n");
                fprintf(stderr, "INFO: bpp: %d, depth: %d, big_endian: %d, true_color %d.\n", format.f.bpp, format.f.depth, format.f.big_endian, format.f.true_color);
                fprintf(stderr, "INFO: red_max: %d, green_max: %d, blue_max: %d.\n", ntohs(format.f.red_max), ntohs(format.f.green_max), ntohs(format.f.blue_max));
                fprintf(stderr, "INFO: red_shift: %d, green_shift: %d, blue_shift: %d.\n", format.f.red_shift, format.f.green_shift, format.f.blue_shift);
                
            } else if (type.message_type == RFBSetEncodings) {
                fprintf(stderr, "INFO: RFBSetEncodings message.\n");

                rfb_set_encoding_request_message_t encoding;
                if (recv(app->rfb.client_socket, (char *)&encoding, sizeof(encoding), 0) < 0) {
                    fprintf(stderr, "ERROR: Can't get RFBSetEncodings message. res: %d\n", errno);
                    goto rfb_error;
                }
                fprintf(stderr, "INFO: RFBSetEncodings message, encodings: %d\n", ntohs(encoding.number_of_encodings));
                int32_t e;
                for (int i = 0; i < ntohs(encoding.number_of_encodings); i++) {
                    if (recv(app->rfb.client_socket, (char *)&e, sizeof(e), 0) < 0) {
                        fprintf(stderr, "ERROR: Can't get RFBSetEncodings encoding. res: %d\n", errno);
                        goto rfb_error;
                    }
                    fprintf(stderr, "%d ", ntohl(e));
                }
                fprintf(stderr, "\n");
            } else if (type.message_type == RFBFramebufferUpdateRequest) {
                if (recv(app->rfb.client_socket, (char *)&buffer_update, sizeof(buffer_update), 0) < 0) {
                    fprintf(stderr, 
                        "ERROR: Can't get RFBFramebufferUpdateRequest message. res: %d\n", 
                        errno);
                    goto rfb_error;
                }
                if (camera_encode_buffer(app, app->openvg.video_buffer.c, ((app->width * app->height) << 1))) {
                    fprintf(stderr, 
                        "ERROR: camera_encode_buffer failed to encode h264 buffer.\n");
                    goto rfb_error;
                }
                if (rfb_send_frame(app)) {
                    fprintf(stderr, 
                        "ERROR: rfb_send_frame failed to send h264 buffer.\n");
                    goto rfb_error;
                }
            } else if (type.message_type == RFBKeyEvent) {
                fprintf(stderr, "INFO: RFBKeyEvent message.\n");
                uint8_t downFlag;
                uint16_t padding;
                uint32_t key;
                if (recv(app->rfb.client_socket, (char *)&downFlag, sizeof(downFlag), 0) < 0)
                    goto rfb_error;
                if (recv(app->rfb.client_socket, (char *)&padding, sizeof(padding), 0) < 0)
                    goto rfb_error;
                if (recv(app->rfb.client_socket, (char *)&key, sizeof(key), 0) < 0)
                    goto rfb_error;
            } else if (type.message_type == RFBPointerEvent) {
                fprintf(stderr, "INFO: RFBPointerEvent message.\n");
                uint8_t buttonMask;
                uint16_t xPos;
                uint16_t yPos;
                if (recv(app->rfb.client_socket, (char *)&buttonMask, sizeof(buttonMask), 0) < 0)
                    goto rfb_error;
                if (recv(app->rfb.client_socket, (char *)&xPos, sizeof(xPos), 0) < 0)
                    goto rfb_error;
                if (recv(app->rfb.client_socket, (char *)&yPos, sizeof(yPos), 0) < 0)
                    goto rfb_error;
            } else if (type.message_type == RFBClientCutText) {
                fprintf(stderr, "INFO: RFBClientCutText message.\n");
                char padding[3];
                uint32_t length;
                if (recv(app->rfb.client_socket, padding, sizeof(padding), 0) < 0)
                    goto rfb_error;
                if (recv(app->rfb.client_socket, (char *)&length, sizeof(length), 0) < 0)
                    goto rfb_error;
                uint8_t *text = (uint8_t *)malloc(length);
                if (recv(app->rfb.client_socket, (char *)text, length, 0) < 0)
                    goto rfb_error;
            } else {
                fprintf(stderr, "WARNING: Unknown message %d.\n", type.message_type);
            }
        } while (1);

rfb_error:
        if (camera_destroy_h264_encoder(app)) {
            fprintf(stderr, "ERROR: camera_destroy_h264_encoder failed\n");
        }

        if (app->rfb.client_socket != 0 && close(app->rfb.client_socket) != 0) {
            fprintf(stderr, "ERROR: Can't close client RFB socket. error: %d\n", errno);
        }
        app->rfb.client_socket = 0;
    }
    return NULL;
}

int rfb_init(struct app_state_t *app)
{
    update_message.message_type = RFBFramebufferUpdate;
    update_message.padding = 0;
    update_message.number_of_rectangles = htons(1);
    update_message.x = htons(0);
    update_message.y = htons(0);
    update_message.width = htons(app->width);
    update_message.height = htons(app->height);
    update_message.encoding_type = htonl(RFBEncodingH264);

    app->rfb.serv_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (app->rfb.serv_socket < 0) {
        fprintf(stderr, "ERROR: Can't create RFB server socket. res: %d\n", errno);
        goto exit;
    }

    const int one = 1;
    if (setsockopt(app->rfb.serv_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&one, sizeof(one)) < 0) {
        fprintf(stderr, "ERROR: Can't set options for RFB server socket. res: %d\n", errno);
        goto error;
    }

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(app->port);
    serv_addr.sin_family = AF_INET;
    if (bind(app->rfb.serv_socket, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) != 0) {
        fprintf(stderr, "ERROR: Can't bind socket for RFB server. res: %d\n", errno);
        goto error;
    }

    if (listen(app->rfb.serv_socket, RFB_MAX_CONNECTIONS) != 0) {
        fprintf(stderr, "ERROR: Can't listen socket for RFB server. res: %d\n", errno);
        goto error;
    }

    app->rfb.thread_res = pthread_create(&app->rfb.thread, NULL, rfb_function, app);
    if (app->rfb.thread_res) {
	    fprintf(stderr, "ERROR: Failed to create rfb thread, return code: %d\n", app->rfb.thread_res);
        goto error;
    }

    return 0;
error:
   rfb_destroy(app); 
exit:
    return -1; 
}

int rfb_send_frame(struct app_state_t *app)
{
    int res;

    // ----- fps
    static int frame_count = 0;
    static struct timespec t1;
    struct timespec t2;
    if (frame_count == 0) {
        clock_gettime(CLOCK_MONOTONIC, &t1);
    }
    clock_gettime(CLOCK_MONOTONIC, &t2);
    float d = (t2.tv_sec + t2.tv_nsec / 1000000000.0) - (t1.tv_sec + t1.tv_nsec / 1000000000.0);
    if (d > 0) {
        app->rfb_fps = frame_count / d;
    } else {
        app->rfb_fps = frame_count;
    }
    frame_count++;
    // -----

    if (sem_wait(&app->mmal.h264_semaphore)) {
        fprintf(stderr, "ERROR: sem_wait failed to wait worker_semaphore with error (%d)\n", errno); 
        return -1;
    }

    if (send(app->rfb.client_socket, (char *)&update_message, sizeof(update_message), 0) < 0) {
        fprintf(stderr, "ERROR: Can't send frame message. res: %d\n", errno);
        return -1;
    }

    //fprintf(stderr, "INFO: r: %d, x: %d, y: %d, w: %d, h: %d\n", ntohs(update_message.number_of_rectangles), ntohs(update_message.x), ntohs(update_message.y), ntohs(update_message.width), ntohs(update_message.height));
    //fprintf(stderr, "INFO: t: %d, e: %d, size: %d, len: %d\n", update_message.message_type, ntohl(update_message.encoding_type), sizeof(update_message), length);

    res = pthread_mutex_lock(&app->mmal.h264_mutex);
    if (res) {
        fprintf(stderr, "ERROR: pthread_mutex_lock failed to lock h264 buffer with code %d\n", res);
        return -1;
    }

    int32_t length = htonl(app->mmal.h264_buffer_length);
    if (send(app->rfb.client_socket, (char *)&length, sizeof(length), 0) < 0) {
        fprintf(stderr, "ERROR: send failed to send H264 header. res: %d\n", errno);
        return -1;
    }

    if (send(app->rfb.client_socket, app->mmal.h264_buffer, app->mmal.h264_buffer_length, 0) < 0) {
        fprintf(stderr, "ERROR: send failed to send frame. res: %d\n", errno);
        return -1;
    }

    res = pthread_mutex_unlock(&app->mmal.h264_mutex);
    if (res) {
        fprintf(stderr, "ERROR: pthread_mutex_unlock failed to unlock h264 buffer with code %d\n", res);
        return -1;
    }

    return 0;
}

int rfb_destroy(struct app_state_t *app)
{
    int res;

    // shutdown the server socket terminates accept call to wait incoming connections
    if (app->rfb.serv_socket != 0 && shutdown(app->rfb.serv_socket, SHUT_RD) != 0) {
        fprintf(stderr, "ERROR: shutdown failed with error: %d\n", errno);
    }

    if (app->rfb.serv_socket != 0 && close(app->rfb.serv_socket) != 0) {
        fprintf(stderr, "ERROR: Can't close server RFB socket. res: %d\n", errno);
    }

    if (!app->rfb.thread_res) {
        res = pthread_join(app->rfb.thread, NULL);
        if (res != 0) {
            fprintf(stderr, "ERROR: Can't close RFB thread. res: %d\n", res);
        }
    }

    return 0;
}
