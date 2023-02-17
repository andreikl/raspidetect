// Raspidetect

// Copyright (C) 2021 Andrei Klimchuk <andrew.klimchuk@gmail.com>

// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 3 of the License, or (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.

// You should have received a copy of the GNU Lesser General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#include "main.h"
#include "utils.h"

#include "rfb.h"

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

struct rfb_pixel_format_t {
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
};

// server messages ------------------------------
struct rfb_security_message_t {
    uint8_t types_count;
    uint8_t types;
};

struct rfb_server_init_message_t {
    uint16_t framebuffer_width;
    uint16_t framebuffer_height;
    struct rfb_pixel_format_t pixel_format;
    uint32_t name_length;
    char name[16];
};

struct rfb_buffer_update_message_t {
    uint8_t message_type;
    uint8_t padding;
    uint16_t number_of_rectangles;
    uint16_t x;
    uint16_t y;
    uint16_t width;
    uint16_t height;
    uint32_t encoding_type;
};

// client messages ------------------------------
struct rfb_type_request_message_t {
    uint8_t message_type;
    uint8_t temp;
};

struct rfb_pixel_format_request_message_t {
    char padding[2];
    struct rfb_pixel_format_t f;
};

struct rfb_buffer_update_request_message_t {
    uint16_t x;
    uint16_t y;
    uint16_t width;
    uint16_t height;
};

struct rfb_set_encoding_request_message_t {
    uint16_t number_of_encodings;
};
// -----------------------------------------------

static struct format_mapping_t rfb_formats[] = {
    {
        .format = VIDEO_FORMAT_H264,
        .internal_format = VIDEO_FORMAT_H264,
        .is_supported = 1
    }
};

struct rfb_state_t rfb = {
    .output = NULL,
    .server_socket = -1,
    .client_socket = -1,
    .thread_res = -1,
    .client_semaphore_res = -1
};

extern struct app_state_t app;
extern struct input_t input;
extern struct filter_t filters[MAX_FILTERS];
extern struct output_t outputs[MAX_OUTPUTS];
extern int is_abort;

static struct rfb_buffer_update_message_t update_message;

static int rfb_is_started()
{
    return rfb.server_socket != -1? 1: 0;
}

static void *rfb_function(void *data)
{
    int res;
    const int one = 1;

    while (!is_abort && rfb_is_started()) {
        struct sockaddr_in client_addr;
        int addres_len = sizeof(client_addr);

        DEBUG("Waiting for clients connection to port: %d", app.port);
        RFB_FUNC_CALL(rfb.client_socket = accept(
            rfb.server_socket,
            (struct sockaddr *)&client_addr, 
            (socklen_t *)&addres_len
        ), fatal_error);

        RFB_FUNC_CALL(setsockopt(rfb.client_socket,
            IPPROTO_TCP,
            TCP_NODELAY,
            (char *)&one,
            sizeof(one)), rfb_error);

        char server_rfb_version[12];
        strcpy(server_rfb_version, "RFB 003.008\0");
        RFB_FUNC_CALL(send(rfb.client_socket, server_rfb_version, sizeof(server_rfb_version), 0),
            rfb_error);

        char client_rfb_version[12];
        RFB_FUNC_CALL(recv(rfb.client_socket, client_rfb_version, sizeof(client_rfb_version), 0),
            rfb_error);

        DEBUG("Client rfb version. %s", client_rfb_version);

        struct rfb_security_message_t security = {
            .types_count = 1,
            .types = RFB_SECURITY_NONE
        };
        RFB_FUNC_CALL(send(rfb.client_socket, (char *)&security , sizeof(security), 0), rfb_error);

        uint8_t client_security_type = 0;
        RFB_FUNC_CALL(recv(
            rfb.client_socket,
            (char *)&client_security_type,
            sizeof(client_security_type), 0
        ), rfb_error);

        DEBUG("Client rfb security type. %d", client_security_type);
    
        uint32_t success = 0;
        RFB_FUNC_CALL(send(rfb.client_socket, (char *)&success, sizeof(success), 0), rfb_error);

        uint8_t shared_flag;
        RFB_FUNC_CALL(recv(rfb.client_socket, (char *)&shared_flag, sizeof(shared_flag), 0), rfb_error);

        DEBUG("Client rfb shared flag. %d", shared_flag);

        struct rfb_server_init_message_t init_message = {
            .framebuffer_width = htons(app.video_width),
            .framebuffer_height = htons(app.video_height),
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
        if (app.video_format == VIDEO_FORMAT_YUV422) {
            init_message.pixel_format.bpp = 16;
            init_message.pixel_format.depth = 16;
        }
        else {
            DEBUG("Video format isn't supported %d", app.video_format);
            errno = EINVAL;
            goto rfb_error;
        }
        memset(init_message.name, 0, sizeof(init_message.name));
        memcpy(init_message.name, APP_NAME, strlen(APP_NAME));

        // DEBUG("il: %d, nl: %d", sizeof(init_message.name), sizeof(init_message));
        RFB_FUNC_CALL(send(rfb.client_socket, (char *)&init_message, sizeof(init_message), 0), rfb_error);

        // if (camera_create_h264_encoder(app)) {
        //     fprintf(stderr, "ERROR: camera_create_h264_encoder failed\n");
        //     goto rfb_error;
        // }

        struct rfb_buffer_update_request_message_t buffer_update;
        do {
            struct rfb_type_request_message_t type;
            RFB_FUNC_CALL(
                res = recv(rfb.client_socket, (char *)&type, sizeof(type), 0),
                rfb_error
            );
            if (type.message_type == RFBSetPixelFormat)  {
                struct rfb_pixel_format_request_message_t format;
                RFB_FUNC_CALL(recv(rfb.client_socket, (char *)&format, sizeof(format), 0),
                    rfb_error);
                DEBUG("RFBSetPixelFormat message.");
                DEBUG("bpp: %d, depth: %d, big_endian: %d, true_color %d.",
                    format.f.bpp, format.f.depth, format.f.big_endian, format.f.true_color);
                DEBUG("red_max: %d, green_max: %d, blue_max: %d.", ntohs(format.f.red_max),
                    ntohs(format.f.green_max), ntohs(format.f.blue_max));
                DEBUG("red_shift: %d, green_shift: %d, blue_shift: %d.", format.f.red_shift,
                    format.f.green_shift, format.f.blue_shift);
            }
            else if (type.message_type == RFBSetEncodings) {
                DEBUG("RFBSetEncodings message.");
                struct rfb_set_encoding_request_message_t encoding;
                RFB_FUNC_CALL(recv(rfb.client_socket, (char *)&encoding, sizeof(encoding), 0), rfb_error);

                DEBUG("RFBSetEncodings message, encodings: %d",
                    ntohs(encoding.number_of_encodings));
                int32_t e;
                for (int i = 0; i < ntohs(encoding.number_of_encodings); i++) {
                    RFB_FUNC_CALL(recv(rfb.client_socket, (char *)&e, sizeof(e), 0), rfb_error);
                    fprintf(stderr, "%d ", ntohl(e));
                }
                fprintf(stderr, "\n");
            } else if (type.message_type == RFBFramebufferUpdateRequest) {
                RFB_FUNC_CALL(
                    recv(rfb.client_socket, (char *)&buffer_update, sizeof(buffer_update), 0),
                    rfb_error
                );

                DEBUG("Server received message: RFBFramebufferUpdateRequest, waiting for frame");
                CALL(sem_post(&rfb.client_semaphore), rfb_error);
            } else if (type.message_type == RFBKeyEvent) {
                DEBUG("RFBKeyEvent message.");
                uint8_t downFlag;
                uint16_t padding;
                uint32_t key;
                RFB_FUNC_CALL(recv(rfb.client_socket, (char *)&downFlag, sizeof(downFlag), 0),
                    rfb_error);
                RFB_FUNC_CALL(recv(rfb.client_socket, (char *)&padding, sizeof(padding), 0),
                    rfb_error);
                RFB_FUNC_CALL(recv(rfb.client_socket, (char *)&key, sizeof(key), 0), rfb_error);
            } else if (type.message_type == RFBPointerEvent) {
                DEBUG("RFBPointerEvent message.");
                uint8_t buttonMask;
                uint16_t xPos;
                uint16_t yPos;
                RFB_FUNC_CALL(recv(rfb.client_socket, (char *)&buttonMask, sizeof(buttonMask), 0),
                    rfb_error);
                RFB_FUNC_CALL(recv(rfb.client_socket, (char *)&xPos, sizeof(xPos), 0), rfb_error);
                RFB_FUNC_CALL(recv(rfb.client_socket, (char *)&yPos, sizeof(yPos), 0), rfb_error);
            } else if (type.message_type == RFBClientCutText) {
                DEBUG("RFBClientCutText message.");
                char padding[3];
                uint32_t length;
                RFB_FUNC_CALL(recv(rfb.client_socket, padding, sizeof(padding), 0), rfb_error);
                RFB_FUNC_CALL(recv(rfb.client_socket, (char *)&length, sizeof(length), 0),
                    rfb_error);
                uint8_t *text = (uint8_t *)malloc(length);
                RFB_FUNC_CALL(recv(rfb.client_socket, (char *)text, length, 0), rfb_error);
            } else {
                fprintf(stderr, "WARNING: Unknown message %d.\n", type.message_type);
            }
        } while (!is_abort && rfb_is_started());

rfb_error:
        if (errno == 104) {
            DEBUG("Client has closed the connection");
        }
        // stop the input and all filters
        CALL(input.stop(), fatal_error);
        struct output_t *output = rfb.output;
        for (int k = 0; k < MAX_FILTERS && output->filters[k].out_format; k++) {
            struct filter_t *filter = filters + output->filters[k].index;
            CALL(filter->stop(), fatal_error);
        }
        // int val = 0;
        // CALL(sem_getvalue(&rfb.client_semaphore, &val));
        // for (int i = 0; i < val; i++) {
        //     CALL(sem_post(&rfb.client_semaphore), rfb_error);
        // }

        // TODO: to delete
        // if (camera_destroy_h264_encoder(app)) {
        //     fprintf(stderr, "ERROR: camera_destroy_h264_encoder failed\n");
        // }

        if (rfb.client_socket > 0) {
            CALL(close(rfb.client_socket))
            rfb.client_socket = -1;
        }
    }

fatal_error:    
    return NULL;
}

static int rfb_init()
{
    update_message.message_type = RFBFramebufferUpdate;
    update_message.padding = 0;
    update_message.number_of_rectangles = htons(1);
    update_message.x = htons(0);
    update_message.y = htons(0);
    update_message.width = htons(app.video_width);
    update_message.height = htons(app.video_height);
    update_message.encoding_type = htonl(RFBEncodingH264);
    return 0;
}

static int rfb_start()
{
    DEBUG("Port to listen: %d", app.port);

    ASSERT_INT(rfb.client_socket, ==, -1, cleanup);
    ASSERT_INT(rfb.server_socket, ==, -1, cleanup);

    CALL(rfb.server_socket = socket(AF_INET, SOCK_STREAM, 0), cleanup);

    const int one = 1;
    CALL(setsockopt(
        rfb.server_socket,
        SOL_SOCKET,
        SO_REUSEADDR,
        (char *)&one,
        sizeof(one)
    ), cleanup);

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(app.port);
    serv_addr.sin_family = AF_INET;
    CALL(bind(rfb.server_socket, (struct sockaddr *)&serv_addr, sizeof(serv_addr)), cleanup);
    CALL(listen(rfb.server_socket, RFB_MAX_CONNECTIONS), cleanup);

    rfb.thread_res = pthread_create(&rfb.thread, NULL, rfb_function, NULL);
    if (rfb.thread_res) {
        CALL_CUSTOM_MESSAGE(pthread_create, rfb.thread_res);
        goto cleanup;
    }

    rfb.client_semaphore_res = sem_init(&rfb.client_semaphore, 0, 0);
    if (rfb.client_semaphore_res) {
        CALL_CUSTOM_MESSAGE(sem_init, rfb.client_semaphore_res);
        goto cleanup;
    }
    return 0;

cleanup:
    errno = EAGAIN;
    return -1;
}

int rfb_process_frame()
{
    int res = 0;

    struct output_t *output = rfb.output;
    if (!output->is_started()) CALL(output->start(), cleanup);

    CALL(sem_wait(&rfb.client_semaphore), cleanup);

    int in_format = output->start_format;
    int out_format = output->start_format;
    if (!input.is_started()) CALL(input.start(in_format), cleanup);
    CALL(res = input.process_frame(), cleanup);
    int length = 0;
    uint8_t *buffer = input.get_buffer(NULL, &length);
    DEBUG("buffer has been received from input[%s], length: %d!!!", input.name, length);
    for (int k = 0; k < MAX_FILTERS && output->filters[k].out_format; k++) {
        struct filter_t *filter = filters + output->filters[k].index;
        in_format = out_format;
        out_format = output->filters[k].out_format;
        if (!filter->is_started()) CALL(filter->start(in_format, out_format), cleanup);
        CALL(filter->process_frame(buffer), cleanup);
        buffer = filter->get_buffer(NULL, &length);
        if (length == 0) {
            DEBUG("The filter[%s] doesn't have buffer yet", filter->name);
            break;
        }
        else {
            DEBUG("buffer has been received from filter[%s], length: %d!!!", filter->name, length);
        }
    }

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
        app.rfb_fps = frame_count / d;
    } else {
        app.rfb_fps = frame_count;
    }
    frame_count++;
    // -----

    if (length != 0) {
        uint32_t length_send = htonl(length);
        CALL(send(rfb.client_socket, (char *)&update_message, sizeof(update_message), 0), cleanup);
        CALL(send(rfb.client_socket, (char *)&length_send, sizeof(length_send), 0), cleanup);

        DEBUG("Bytes to send: %d, %x %x %x %x ...",
            length,
            buffer[0],
            buffer[1],
            buffer[2],
            buffer[3]);
        CALL(send(rfb.client_socket, (char *)buffer, length, 0), cleanup);

        DEBUG("r: %d, x: %d, y: %d, w: %d, h: %d", ntohs(update_message.number_of_rectangles),
            ntohs(update_message.x), ntohs(update_message.y), ntohs(update_message.width),
            ntohs(update_message.height));
        #if defined(ENV64BIT)
            DEBUG("t: %d, e: %d, size: %ld, len: %d", update_message.message_type,
                ntohl(update_message.encoding_type), sizeof(update_message), length);
        #else
            DEBUG("t: %d, e: %d, size: %d, len: %d", update_message.message_type,
                ntohl(update_message.encoding_type), sizeof(update_message), length);
        #endif

    }
    else {
        DEBUG("Unblock semaphore until buffer is received");
        CALL(sem_post(&rfb.client_semaphore), cleanup);
    }
    return 0;

cleanup:
    errno = EAGAIN;
    return -1;
}

static int rfb_stop()
{
    DEBUG("rfb is stopping...");
    // shutdown the server socket terminates accept call to wait incoming connections
    if (rfb.server_socket > 0) {
        int res = shutdown(rfb.server_socket, SHUT_RDWR);
        if (res == -1 && errno != ENOTCONN) {
            CALL_MESSAGE(shutdown(rfb.server_socket, SHUT_RDWR));
            goto stop_error;
        }
    }

    if (rfb.client_socket > 0) {
        CALL(close(rfb.client_socket), stop_error)
        rfb.client_socket = -1;
    }

    if (rfb.server_socket > 0) {
        CALL(close(rfb.server_socket), stop_error);
        rfb.server_socket = -1;
    }

    if (!rfb.thread_res) {
        int res = pthread_join(rfb.thread, NULL);
        if (res != 0) {
            CALL_CUSTOM_MESSAGE(pthread_join, res);
            goto stop_error;
        }
        else
            rfb.thread_res = -1;
    }

    if (!rfb.client_semaphore_res) {
        int res = sem_destroy(&rfb.client_semaphore);
        if (res) {
            CALL_CUSTOM_MESSAGE(sem_destroy, res);
            goto stop_error;
        }
        else
            rfb.client_semaphore_res = -1;
    }
    DEBUG("rfb has been stopped");
    return 0;

stop_error:
    errno = EAGAIN;
    return -1;
}

static void rfb_cleanup()
{
    rfb_stop();
}

static int rfb_get_formats(const struct format_mapping_t *formats[])
{
    if (formats != NULL)
        *formats = rfb_formats;
    return ARRAY_SIZE(rfb_formats);
}

void rfb_construct(struct app_state_t *app)
{
    int i = 0;
    while (i < MAX_OUTPUTS && outputs[i].context != NULL)
        i++;

    if (i != MAX_OUTPUTS) {
        rfb.output = outputs + i;
        outputs[i].name = "rfb";
        outputs[i].context = &rfb;
        outputs[i].init = rfb_init;
        outputs[i].start = rfb_start;
        outputs[i].is_started = rfb_is_started;
        outputs[i].process_frame = rfb_process_frame;
        outputs[i].stop = rfb_stop;        
        outputs[i].get_formats = rfb_get_formats;
        outputs[i].cleanup = rfb_cleanup;
    }
}
