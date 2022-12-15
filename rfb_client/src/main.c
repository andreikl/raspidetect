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

// standard libraries
#include <signal.h>
// windows
#include <windowsx.h>

// 3rd party libraries
#include "khash.h"

#include "main.h"
#include "utils.h"
#include "d3d.h"
#include "dxva.h"
#include "rfb.h"
#include "cuda.h"

KHASH_MAP_INIT_STR(map_str, char*)
KHASH_T(map_str) *h;

struct app_state_t app;
int is_terminated = 0;

static LRESULT CALLBACK window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    //struct app_state_t* app = NULL;
    switch(message) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        case WM_CREATE:
            /*app = (struct app_state_t*)lparam;
            DEBUG("INFO: window(%d:%d) has been created!\n",
                app->server_width, app->server_height);*/
            return 0;
    }

    return DefWindowProc(hwnd, message, wparam, lparam);
}

static void print_help(void)
{
    printf("%s [options]\n", APP_NAME);
    printf("options:\n");
    printf("\n");
    printf("%s: help\n", HELP);
    printf("%s: input type, (%s, %s) default: %s\n", INPUT_TYPE, INPUT_TYPE_FILE_STR,
        INPUT_TYPE_RFB_STR, INPUT_TYPE_DEF);
    printf("%s: file name, default: %s\n", FILE_NAME, FILE_NAME_DEF);
    printf("%s: server, default: %s\n", SERVER, SERVER_DEF);
    printf("%s: port, default: %s\n", PORT, PORT_DEF);
    printf("%s: verbose, verbose: %d\n", VERBOSE, VERBOSE_DEF);
}

static void signal_handler(int signal_number)
{
    DEBUG("INFO: Other signal %d\n", signal_number);
    is_terminated = 1;
}

static void main_function()
{
    setmode(fileno(stdout), O_BINARY);

    memset(&app, 0, sizeof(struct app_state_t));
    char* input_type = utils_read_str_value(INPUT_TYPE, INPUT_TYPE_DEF);
    DEBUG("INFO: input_type %s\n", input_type);
    if (!strcmp(input_type, INPUT_TYPE_FILE_STR)) {
        app.input_type = INPUT_TYPE_FILE;
    }
    else {
        app.input_type = INPUT_TYPE_RFB;
    }   
    app.file.file_name = utils_read_str_value(FILE_NAME, FILE_NAME_DEF);
    app.server_port = utils_read_str_value(PORT, PORT_DEF);
    app.server_host = utils_read_str_value(SERVER, SERVER_DEF);
    app.server_width = 640;
    app.server_height = 480;
    app.server_chroma = CHROMA_FORMAT_YUV422;
    app.verbose = utils_read_int_value(VERBOSE, VERBOSE_DEF);

    app.enc_buf_length = app.server_width * app.server_height + 1;
    app.enc_buf = malloc(app.enc_buf_length);
    if (app.enc_buf == NULL) {
        fprintf(
            stderr,
            "ERROR: malloc can't allocate memory for encoding buffer, size: %d\n",
            app.enc_buf_length);
        goto exit;
    }

    app.dec_buf_length = app.server_width * app.server_height * 4 + 1;
    app.dec_buf = malloc(app.dec_buf_length);
    if (app.dec_buf == NULL) {
        fprintf(
            stderr,
            "ERROR: malloc can't allocate memory for decoding buffer, size: %d\n",
            app.dec_buf_length);
        goto exit;
    }

    CALL(pthread_mutex_init(&app.dec_mutex, NULL), exit);
    app.is_dec_mutex = 1;

#ifdef ENABLE_CUDA
    CALL(cuda_init(&app), exit);
#endif //ENABLE_CUDA

    if (app.input_type == INPUT_TYPE_RFB) {
#ifdef ENABLE_RFB
        CALL(rfb_init(), exit);
        CALL(rfb_connect(), exit);
        CALL(rfb_handshake(), exit);
#endif //ENABLE_RFB
    }
    else {
        CALL(file_init(), exit);
    }

    app.instance = GetModuleHandle(NULL);

    WNDCLASSEX wc;
    memset(&wc, 0, sizeof(WNDCLASSEX));
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.hbrBackground = (HBRUSH)COLOR_WINDOW + 1;
    wc.lpfnWndProc = window_proc;
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.hInstance = app.instance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = "D3D";

    RegisterClassEx(&wc);

    app.wnd = CreateWindowEx(0,
        "D3D",
        app.server_name,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        app.server_width, app.server_height,
        NULL,
        NULL,
        app.instance,
        &app);

#ifdef ENABLE_D3D
    CALL(d3d_init(), exit);
#endif //ENABLE_D3Ds

#ifdef ENABLE_H264
    CALL(h264_init(), exit);
#endif //ENABLE_H264

#ifdef ENABLE_FFMPEG
    CALL(ffmpeg_init(), exit);
#endif //ENABLE_FFMPEG

    if (app.input_type == INPUT_TYPE_RFB) {
#ifdef ENABLE_RFB
        DEBUG("RFB init");
        CALL(rfb_start(), exit);
#endif //ENABLE_RFB
    }
    else {
        CALL(file_start(), exit);
    }

    ShowWindow(app.wnd, SW_SHOW);
    UpdateWindow(app.wnd);
    CALL(d3d_start(), exit);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);

        if (is_terminated && app.wnd) {
            //TODO: remove direct3d first
            DEBUG("INFO: about to destroy window\n");
            if (!DestroyWindow(app.wnd)) {
                fprintf(stderr,
                    "ERROR: DestroyWindow failed with code. res: %ld\n",
                    GetLastError());
            }
            else {
                app.wnd = NULL;
            }
            break;
        }
    }

exit:
    DEBUG("INFO: exit\n");

#ifdef ENABLE_FFMPEG
    ffmpeg_destroy();
#endif //ENABLE_FFMPEG

#ifdef ENABLE_H264
    h264_destroy();
#endif //ENABLE_H264

#ifdef ENABLE_CUVID
    cuda_destroy(&app);
#endif //ENABLE_CUVID

#ifdef ENABLE_D3D
    d3d_destroy();
#endif // ENABLE_D3D

    if (app.input_type == INPUT_TYPE_RFB) {
#ifdef ENABLE_RFB
        rfb_destroy();
#endif //ENABLE_RFB
    }
    else {
        file_destroy();
    }

    if (app.is_dec_mutex) {
        int res = pthread_mutex_destroy(&app.dec_mutex);
        if (res) {
            fprintf(
                stderr,
                "ERROR: pthread_mutex_destroy can't destroy decoder mutex, res %d\n",
                res);
        }
        app.is_dec_mutex = 0;
    }

    if (app.dec_buf != NULL) {
        free(app.dec_buf);
        app.dec_buf = NULL;
    }

    if (app.enc_buf != NULL) {
        free(app.enc_buf);
        app.enc_buf = NULL;
    }

    DEBUG("INFO: main_exit, is_terminated: %d\n", is_terminated);

    return;
}

int main(int argc, char** argv)
{
    signal(SIGINT, signal_handler);

    h = KH_INIT(map_str);
    utils_parse_args(argc, argv);

    unsigned k = KH_GET(map_str, h, HELP);
    if (k != KH_END(h)) {
        print_help();
    }
    else {
        main_function();
    }

    KH_DESTROY(map_str, h);
    return 0;
}