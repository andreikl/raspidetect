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

#include <unistd.h>

#include "main.h"
#include "utils.h"
#include "d3d.h"

extern struct app_state_t app;
extern int is_terminated;

static void *d3d_function(void* data)
{
    DEBUG("Direct3d thread has been started");

    if (sem_wait(&app.d3d.semaphore)) {
        DEBUG("ERROR: sem_wait failed with error: %s", strerror(errno));
        goto error;
    }

    while (!is_terminated) {
        if (usleep(40000)) {
            DEBUG("ERROR: usleep failed with error: %s", strerror(errno));
            goto error;
        }

        DEBUG("render");

        d3d_render_frame();
    }

    DEBUG("d3d_function is_terminated: %d", is_terminated);
    return NULL;

error:
    is_terminated = 1;
    return NULL;
}

void d3d_destroy()
{
    if (sem_destroy(&app.d3d.semaphore)) {
        DEBUG("ERROR: sem_destroy failed with code: %s", strerror(errno));
    }

    if (app.d3d.is_thread) {
        CALL(pthread_join(app.d3d.thread, NULL));
        app.d3d.is_thread = 0;
    }


    for (int i = 0; i < SCREEN_BUFFERS; i++) {
        if (app.d3d.surfaces[i]) {
            IDirect3DSurface9_Release(app.d3d.surfaces[i]);
        }
    }

    if (app.d3d.dev) {
        IDirect3DDevice9_Release(app.d3d.dev);
    }

    if (app.d3d.d3d) {
        IDirect3D9_Release(app.d3d.d3d);
    }
}

int d3d_init()
{
    HRESULT res;

    CALL(sem_init(&app.d3d.semaphore, 0, 0), close);
    CALL(pthread_create(&app.d3d.thread, NULL, d3d_function, NULL), close);
    app.d3d.is_thread = 1;

    app.d3d.d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!app.d3d.d3d) {
        DEBUG("ERROR: Can't create d3d interface");
        goto close;
    }

    D3DPRESENT_PARAMETERS d3dpp;
    memset(&d3dpp, 0, sizeof(d3dpp));
    d3dpp.Windowed = TRUE;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.hDeviceWindow = app.wnd;

    // create a device class using this information and the info from the d3dpp stuct
    res = IDirect3D9_CreateDevice(
        app.d3d.d3d,
        D3DADAPTER_DEFAULT,
        D3DDEVTYPE_HAL,
        app.wnd,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING,
        &d3dpp,
        &app.d3d.dev);
    if (FAILED(res)) {
        fprintf(
            stderr,
            "ERROR: IDirect3D9_CreateDevice failed with error %lx %s",
            res,
            d3d_get_hresult_message(res)
        );
        goto close;
    }

    for (int i = 0; i < SCREEN_BUFFERS; i++) {
        D3D_CALL(IDirect3DDevice9_CreateOffscreenPlainSurface(
            app.d3d.dev,
            app.server_width,
            app.server_height,
            //H264CODEC_FORMAT,
            D3DFMT_A8R8G8B8,
            D3DPOOL_DEFAULT,
            &app.d3d.surfaces[i],
            NULL
        ), close);
    }
    return 0;

close:
    d3d_destroy();
    errno = EAGAIN;
    return -1;
}

int d3d_render_frame()
{
    /*D3D_CALL(
        IDirect3DDevice9_Clear(app.d3d.dev,
            0,
            NULL,
            D3DCLEAR_TARGET,
            D3DCOLOR_XRGB(0, 40, 100),
            1.0f,
            0
        ),
        error
    );*/

    D3D_CALL(IDirect3DDevice9_BeginScene(app.d3d.dev), end_scene);
    IDirect3DSurface9* backbuffer = NULL;

    D3D_CALL(
        IDirect3DDevice9_GetBackBuffer(app.d3d.dev, 0, 0, D3DBACKBUFFER_TYPE_MONO, &backbuffer),
        end_scene);

    CALL(pthread_mutex_lock(&app.dec_mutex), end_scene);

    D3D_CALL(
        IDirect3DDevice9_StretchRect(app.d3d.dev,
            app.d3d.surfaces[0],
            NULL,
            backbuffer,
            NULL,
            D3DTEXF_NONE),
        end_scene);

    CALL(pthread_mutex_unlock(&app.dec_mutex), end_scene);

end_scene:
    D3D_CALL(IDirect3DDevice9_EndScene(app.d3d.dev), error);

    // displays the created frame on the screen
    D3D_CALL(IDirect3DDevice9_Present(app.d3d.dev, NULL, NULL, NULL, NULL), error);

    return 0;
error:
    return -1;
}

int d3d_render_image()
{
    int res = 0;
    // RECT rect;
    // rect.left = 0; rect.top = 0;
    // rect.right = 640; rect.bottom = 480;

    CALL(pthread_mutex_lock(&app.dec_mutex), exit);

    D3DLOCKED_RECT d3d_rect;
    HRESULT hres;
    D3D_CALL(hres = IDirect3DSurface9_LockRect(
        app.d3d.surfaces[0], &d3d_rect, NULL, D3DLOCK_NOSYSLOCK
    ), unlocdec);

    //TODO: move to shader
    // convert Grayscale to ARGB
    uint8_t* in = app.dec_buf;
    uint8_t* out = d3d_rect.pBits;
    int len = app.server_width * app.server_height;
    while(len >= 0) {
        uint8_t y = *in;
        in++;
        *out = y;
        out++;
        *out = y;
        out++;
        *out = y;
        out += 2;
        len--;
    }
    // //DEBUG("memcpy: %p(%d)", app.dec_buf, app.dec_buf_length);
    // //DEBUG("INFO:%X%X%X%X", app.dec_buf[0], app.dec_buf[1], app.dec_buf[2], app.dec_buf[3]);

    D3D_CALL(hres = IDirect3DSurface9_UnlockRect(app.d3d.surfaces[0]));
    if (FAILED(hres)) {
        res = -1;
    }

unlocdec:
    int ret = 0;
    CALL(ret = pthread_mutex_unlock(&app.dec_mutex));
    if (ret)
        res = -1;

exit:
    if (res == -1 && !errno)
        errno = EAGAIN;

    return res;
}

char* d3d_get_hresult_message(HRESULT result)
{
    if (result == D3DERR_DEVICELOST) {
        return "D3DERR_DEVICELOST";
    }
    else if (result == D3DERR_INVALIDCALL) {
        return "D3DERR_INVALIDCALL";
    }
    else if (result == D3DERR_NOTAVAILABLE) {
        return "D3DERR_NOTAVAILABLE";
    }
    else if (result == D3DERR_OUTOFVIDEOMEMORY) {
        return "D3DERR_OUTOFVIDEOMEMORY";
    }
    else if (result == D3DERR_WASSTILLDRAWING) {
        return "D3DERR_WASSTILLDRAWING";
    }
    else if (result == E_INVALIDARG) {
        return "E_INVALIDARG";
    }
    else {
        return "UNKNOWN";
    }
}

int d3d_start()
{
    CALL(sem_post(&app.d3d.semaphore), error);
    return 0;
error:
    return -1;
}