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
#include "app.h"
#include "d3d.h"
#include "dxva.h"
#include "rfb.h"
#include "file.h"
#include "cuda.h"
#include "ffmpeg.h"

KHASH_MAP_INIT_STR(argvs_hash_t, char*)
KHASH_T(argvs_hash_t) *h;

int is_aborted = 0;

struct app_state_t app;
struct filter_t filters[MAX_FILTERS];

static LRESULT CALLBACK window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    //struct app_state_t* app = NULL;
    switch(message) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        case WM_CREATE:
            /*app = (struct app_state_t*)lparam;
            DEBUG_MSG("INFO: window(%d:%d) has been created!\n",
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
    printf("%s: verbose\n", VERBOSE);
}

static void signal_handler(int signal_number)
{
    DEBUG_MSG("INFO: Other signal %d\n", signal_number);
    is_aborted = 1;
}

static void main_function()
{
    setmode(fileno(stdout), O_BINARY);

    app_construct();
    CALL(app_init(), error);

#ifdef ENABLE_CUDA
    CALL(cuda_init(&app), error);
#endif //ENABLE_CUDA

    if (app.input_type == INPUT_TYPE_RFB) {
#ifdef ENABLE_RFB
        CALL(rfb_init(), error);
        CALL(rfb_connect(), error);
        CALL(rfb_handshake(), error);
#endif //ENABLE_RFB
    }
    else {
        CALL(file_init(), error);
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
    CALL(d3d_init(), error);
#endif //ENABLE_D3Ds

#ifdef ENABLE_H264
    CALL(h264_init(), error);
#endif //ENABLE_H264

    if (app.input_type == INPUT_TYPE_RFB) {
#ifdef ENABLE_RFB
        CALL(rfb_start(), error);
#endif //ENABLE_RFB
    }
    else {
        CALL(file_start(), error);
    }

    for (int i = 0; i < MAX_FILTERS && filters[i].context != NULL; i++) {
        CALL(filters[i].start(VIDEO_FORMAT_H264, VIDEO_FORMAT_GRAYSCALE), error);
    }

    ShowWindow(app.wnd, SW_SHOW);
    UpdateWindow(app.wnd);
    CALL(d3d_start(), error);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);

        if (is_aborted && app.wnd) {
            //TODO: remove direct3d first
            DEBUG_MSG("INFO: about to destroy window\n");
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

error:
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

    app_cleanup();

    DEBUG_MSG("INFO: main_exit, is_aborted: %d\n", is_aborted);

    return;
}

int main(int argc, char** argv)
{
    signal(SIGINT, signal_handler);

    h = KH_INIT(argvs_hash_t);
    utils_parse_args(argc, argv);

    unsigned verbose = KH_GET(argvs_hash_t, h, VERBOSE);
    if (verbose != KH_END(h)) {
        app.verbose = 1;
        DEBUG_MSG("Debug output has been enabled!!!");
    }
    else {
        app.verbose = 0;
    }

    app_set_default_state();

    unsigned k = KH_GET(argvs_hash_t, h, HELP);
    if (k != KH_END(h)) {
        print_help();
    }
    else {
        main_function();
    }

    KH_DESTROY(argvs_hash_t, h);
    return 0;
}