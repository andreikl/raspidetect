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

#include <winsock2.h>
#include <ws2tcpip.h>

#include "main.h"
#include "utils.h"
#include "rfb.h"
#include "ffmpeg.h"
#include "d3d.h"

#define RFB_SECURITY_NONE 1

extern struct app_state_t app;
extern struct filter_t filters[MAX_FILTERS];
extern int is_aborted;

enum rfb_request_enum {
    RFBSetPixelFormat = 0,
    RFBSetEncodings = 2,
    RFBFramebufferUpdateRequest = 3,
    RFBKeyEvent = 4,
    RFBPointerEvent = 5,
    RFBClientCutText = 6
};

enum rfb_response_enum {
    RFBFramebufferUpdateResponse = 0,
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

// RFB init messages
struct rfb_security_message_t {
    uint8_t types_count;
    uint8_t types;
};

struct rfb_server_init_message_t {
    uint16_t framebuffer_width;
    uint16_t framebuffer_height;
    struct rfb_pixel_format_t pixel_format;
    uint32_t name_length;
    char name[MAX_NAME_SIZE];
};

// RFB requests messages
struct rfb_buffer_update_request_message_t {
    uint8_t type;
    uint8_t temp;
    uint16_t x;
    uint16_t y;
    uint16_t width;
    uint16_t height;
} ;

// RFB responses messages
struct rfb_buffer_update_response_message_t {
    uint8_t message_type;
    uint8_t padding;
    uint16_t number_of_rectangles;
    uint16_t x;
    uint16_t y;
    uint16_t width;
    uint16_t height;
    uint32_t encoding_type;
};

static void *rfb_function(void* data)
{
    DEBUG_MSG("INFO: RFB thread has been started\n");

    //wait frame from network
    CALL(sem_wait(&app.rfb.semaphore), error);

    struct rfb_buffer_update_request_message_t update_request = {
        .type = RFBFramebufferUpdateRequest,
        .temp = 0,
        .x = htons(0),
        .y = htons(0),
        .width = htons(app.server_width),
        .height = htons(app.server_height)
    };

    struct rfb_buffer_update_response_message_t update_response;
    
    while (!is_aborted) {
        // DEBUG_MSG("new slice has been requiested: %d:%d",
        //     ntohs(update_request.width), ntohs(update_request.height));

        NETWORK_IO_CALL(
            send(app.rfb.socket, (char *)&update_request, sizeof(update_request), 0),
            error);

        NETWORK_IO_CALL(
            recv(app.rfb.socket, (char *)&update_response, sizeof(update_response), 0),
            error);

        // validate
        if (update_response.message_type != RFBFramebufferUpdateResponse
            || ntohs(update_response.number_of_rectangles) != 1
            || ntohs(update_response.x) != 0
            || ntohs(update_response.y) != 0
            || ntohs(update_response.width) != app.server_width
            || ntohs(update_response.height) != app.server_height
            || ntohl(update_response.encoding_type) != RFBEncodingH264) {

            DEBUG_MSG("ERROR: RFBFramebufferUpdate response isn't valid: message_type: %d,"
                " width: %d, height: %d, encoding_type: %lx\n",
                update_response.message_type,
                ntohs(update_response.width),
                ntohs(update_response.height),
                ntohl(update_response.encoding_type));

            goto error;
        }

        uint32_t length;
        NETWORK_IO_CALL(
            recv(app.rfb.socket, (char *)&length, sizeof(length), 0),
            error);

        // validate
        if (ntohl(length) >= app.server_width * app.server_height) {
            fprintf(stderr,
                "ERROR: H264 buffer is too big: length: %ld\n",
                ntohl(length));

            goto error;
        }

        app.enc_buf_length = ntohl(length);

        int res = 0;
        NETWORK_IO_CALL(
            res = recv(app.rfb.socket, (char *)app.enc_buf, app.enc_buf_length, MSG_WAITALL),
            error);

        // uint8_t * t = app.enc_buf;
        // uint8_t t1 = t[0];
        // uint8_t t2 = t[1];
        // uint8_t t3 = t[2];
        // uint8_t t4 = t[3];
        // DEBUG_MSG("Bytes received: %d, %x %x %x %x ...",
        //     res,
        //     t1,
        //     t2,
        //     t3,
        //     t4);

        uint8_t *buffer = app.enc_buf;
        int len = app.enc_buf_length;
        for (int i = 0; i < MAX_FILTERS && filters[i].context != NULL; i++) {
            struct filter_t *filter = filters + i;
            if (!filter->is_started())
                CALL(filter->start(VIDEO_FORMAT_H264, VIDEO_FORMAT_GRAYSCALE), error);
            CALL(filter->process(buffer, len), error);
            buffer = filter->get_buffer(NULL, &len);
            if (len == 0) {
                DEBUG_MSG("The filter[%s] doesn't have buffer yet", filter->name);
                break;
            }
            else {
                DEBUG_MSG("buffer has been received from filter[%s], length: %d!!!", filter->name, len);
            }
            break;
        }
#ifdef ENABLE_H264
        CALL(h264_decode(), error);
#endif //ENABLE_H264

#ifdef ENABLE_D3D
        CALL(d3d_render_image(), error);
#endif //ENABLE_D3D
    }

    DEBUG_MSG("INFO: rfb_function is_aborted: %d\n", is_aborted);
    return NULL;

error:
    is_aborted = 1;
    ERROR_MSG("decoding is aborted!!!");

    return NULL;
}

void rfb_destroy()
{
    int res = 0;
    char buffer[MAX_DATA];

    if (app.rfb.socket > 0) {
        // shutdown the connection since no more data will be sent
        NETWORK_CALL(shutdown(app.rfb.socket, SD_SEND), close);
        do {
            NETWORK_IO_CALL(res = recv(app.rfb.socket, buffer, MAX_DATA, 0), close);
            if (res == 0 && app.verbose) {
                DEBUG_MSG("INFO: Connection closed\n");
            }
        } while (res > 0);

close:
        NETWORK_CALL(closesocket(app.rfb.socket))
    }

    CALL(sem_destroy(&app.rfb.semaphore));

    if (app.rfb.is_thread) {
        CALL(pthread_join(app.rfb.thread, NULL));
        app.rfb.is_thread = 0;
    }

    NETWORK_CALL(WSACleanup());
}

int rfb_init()
{
    WSADATA wsa;
    NETWORK_CALL(WSAStartup(MAKEWORD(2,2), &wsa), error);

    CALL(sem_init(&app.rfb.semaphore, 0, 0), error);

    CALL(pthread_create(&app.rfb.thread, NULL, rfb_function, NULL), error);
    app.rfb.is_thread = 1;

    return 0;

error:
    rfb_destroy();
    return -1;
}

int rfb_handshake()
{
    char server_rfb_version[12];
    NETWORK_IO_CALL(recv(app.rfb.socket, server_rfb_version, sizeof(server_rfb_version), 0), error);

    char client_rfb_version[12];
    strcpy(client_rfb_version, "RFB 003.008\0");
    NETWORK_IO_CALL(send(app.rfb.socket, client_rfb_version, sizeof(client_rfb_version), 0), error);
    if (app.verbose) {
        DEBUG_MSG("Server rfb version. %s\n", server_rfb_version);
    }

    struct rfb_security_message_t security = {
        .types_count = 1,
        .types = RFB_SECURITY_NONE
    };
    NETWORK_IO_CALL(recv(app.rfb.socket, (char *)&security , sizeof(security), 0), error);

    uint8_t client_security_type = RFB_SECURITY_NONE;
    NETWORK_IO_CALL(
        send(app.rfb.socket, (char *)&client_security_type, sizeof(client_security_type), 0),
        error);
    if (app.verbose) {
        DEBUG_MSG("INFO: Server rfb security types(%d) 0:%d\n", security.types_count, security.types);
    }

    uint32_t success = 0;
    NETWORK_IO_CALL(recv(app.rfb.socket, (char *)&success, sizeof(success), 0), error);
    if (success) {
        DEBUG_MSG("ERROR: RFB handshake isn't successful\n");
        return -1;
    }

    uint8_t shared_flag = 0;
    NETWORK_IO_CALL(send(app.rfb.socket, (char *)&shared_flag, sizeof(shared_flag), 0), error);

    struct rfb_server_init_message_t init_message;
    NETWORK_IO_CALL(recv(app.rfb.socket, (char *)&init_message, sizeof(init_message), 0), error);
    strncpy(app.server_name, init_message.name, MAX_NAME_SIZE);
    app.server_width = ntohs(init_message.framebuffer_width);
    app.server_height = ntohs(init_message.framebuffer_height);
    if (app.verbose) {
        DEBUG_MSG("server_width: %d, server_height: %d, server_name: %s\n",
            app.server_width, app.server_height, app.server_name);
    }
    return 0;

error:
    return -1;
}

int rfb_connect()
{
    int client_socket = 0, res = 0, ret = -1;
    struct addrinfo *addr_info = NULL, *ptr = NULL, hints = {
        .ai_family = AF_UNSPEC,
        .ai_socktype = SOCK_STREAM,
        .ai_protocol = IPPROTO_TCP
    };
    
    NETWORK_CALL(
        getaddrinfo(app.server_host, app.server_port, &hints, &addr_info),
        cleanup
    );

    for (ptr = addr_info; ptr != NULL; ptr=ptr->ai_next) {
        NETWORK_IO_CALL(
            client_socket = socket(ptr->ai_family, ptr->ai_socktype,  ptr->ai_protocol),
            cleanup
        );

        DEBUG_MSG("trying connect to %s:%s", app.server_host, app.server_port);
        NETWORK_CALL(res = connect(client_socket, ptr->ai_addr, (int)ptr->ai_addrlen));
        if (res == -1) {
            NETWORK_CALL(closesocket(client_socket), cleanup);
            client_socket = 0;
            continue;
        }
        break;
    }

    if (client_socket > 0) {
        app.rfb.socket = client_socket;
        ret = 0;
    }

cleanup:
    if (addr_info) {
        freeaddrinfo(addr_info);
    }
    return ret;
}

int rfb_start()
{
    if (sem_post(&app.rfb.semaphore)) {
        DEBUG_MSG("ERROR: sem_post Failed to start rfb with error: %d\n", errno);
        return -1;
    }
    return 0;
}