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

int __wrap___xstat(int ver, const char * filename, struct stat * stat_buf)
{
#ifndef V4L_ENCODER_WRAP
    if (strcmp(filename, V4L_H264_ENCODER) == 0) {
        WRAP_DEBUG("real xstat, filename: %s", filename);
        return __real___xstat(ver, filename, stat_buf);
    }
#endif
    WRAP_DEBUG("xstat, filename: %s", filename);
    stat_buf->st_mode = __S_IFCHR;
    return 0;
}

int __wrap_open(const char * file, int oflag, ...)
{
    WRAP_DEBUG("open, file: %s", file);
    return 1;
}

int __wrap_close(int fd)
{
#ifndef V4L_ENCODER_WRAP
    if (fd != 1) {
        WRAP_DEBUG("real close, fd: %d", fd);
        return __real_close(fd);
    }
#endif
    WRAP_DEBUG("close, fd: %d", fd);
    return 0;
}

int __wrap_select(int nfds,
    fd_set *readfds,
    fd_set *writefds,
    fd_set *exceptfds,
    struct timeval *timeout)
{
#ifndef V4L_ENCODER_WRAP
    if (__FDS_BITS (readfds)[0] != 2) {
        WRAP_DEBUG("real select, fd: %ld", (__FDS_BITS (readfds)[0] >> 1));
        return __real_select(nfds, readfds, writefds, exceptfds, timeout);
    }
#endif
    WRAP_DEBUG("select, fd: %ld", (__FDS_BITS (readfds)[0] >> 1));
    return 1;
}

#ifdef V4L

static uint8_t *image = NULL;
static int image_width = 0;
static int image_height = 0;
int __wrap_ioctl(int fd, int request, void *arg)
{
    if (request == (int)VIDIOC_QUERYCAP) {
        WRAP_DEBUG("request: %s", "VIDIOC_QUERYCAP");
        struct v4l2_capability *cap = arg;
        strncpy((char *)cap->card, "test", 32);
        cap->capabilities = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING;
        return 0;
    }
    else if (request == (int)VIDIOC_ENUM_FMT) {
        WRAP_DEBUG("request: %s", "VIDIOC_ENUM_FMT");
        struct v4l2_fmtdesc *fmt = arg;
        if (fmt->index == 0) {
            fmt->pixelformat = V4L2_PIX_FMT_YUYV;
            return 0;
        }
        else {
            errno = EINVAL;
            return -1;
        }
    }
    else if (request == (int)VIDIOC_ENUM_FRAMESIZES) {
        WRAP_DEBUG("request: %s", "VIDIOC_ENUM_FRAMESIZES");
        struct v4l2_frmsizeenum *frmsize = arg;
        if (frmsize->index == 0) {
            frmsize->type = V4L2_FRMSIZE_TYPE_STEPWISE;
            frmsize->stepwise.step_width = 16;
            frmsize->stepwise.step_height = 16;
            frmsize->stepwise.min_width = 320;
            frmsize->stepwise.min_height = 256;
            frmsize->stepwise.max_width = 1024;
            frmsize->stepwise.max_height = 768;
            return 0;
        }
        else {
            errno = EINVAL;
            return -1;
        }
    }
    else if (request == (int)VIDIOC_S_FMT) {
        WRAP_DEBUG("request: %s", "VIDIOC_S_FMT");
        struct v4l2_format *fmt = arg;
        check_expected(fmt->fmt.pix.pixelformat);
        check_expected(fmt->fmt.pix.width);
        check_expected(fmt->fmt.pix.height);
        image_width = fmt->fmt.pix.width;
        image_height = fmt->fmt.pix.height;
        return 0;
    }
    else if (request == (int)VIDIOC_REQBUFS) {
        WRAP_DEBUG("request: %s", "VIDIOC_REQBUFS");
        return 0;
    }
    else if (request == (int)VIDIOC_QBUF) {
        WRAP_DEBUG("request: %s", "VIDIOC_QBUF");
        struct v4l2_buffer *buf = arg;
        assert_int_equal(buf->length, image_width * image_height * 2);
        image = (uint8_t *)buf->m.userptr;
        return 0;
    }
    else if (request == (int)VIDIOC_STREAMON) {
        WRAP_DEBUG("request: %s", "VIDIOC_STREAMON");
        return 0;
    }
    else if (request == (int)VIDIOC_STREAMOFF) {
        WRAP_DEBUG("request: %s", "VIDIOC_STREAMOFF");
        return 0;
    }
    else if (request == (int)VIDIOC_DQBUF) {
        WRAP_DEBUG("request: %s", "VIDIOC_DQBUF");
        uint8_t *index = image;
        int row_length = image_width << 1;
        for (int i = 0; i < image_height; i++)
            for (int j = 0; j < row_length; j += 2, index += 2) {
                *index = (i & 0x8) == 0? 0: 255;
            }
        return 0;
    }
    else {
        WRAP_DEBUG("request: %d", request);
    }
    errno = EAGAIN;
    return -1;
}
#endif //V4L

#ifdef SDL
#include <SDL.h>
static SDL_Window *sdl_window = (SDL_Window *)1;
static SDL_Surface *sdl_surface = (SDL_Surface *)1;
int __wrap_SDL_Init(uint32_t flags)
{
    WRAP_DEBUG("SDL_Init");
    return 0;
}

SDL_Window *__wrap_SDL_CreateWindow(
    const char *title,
    int x, int y, int w,
    int h, uint32_t flags)
{
    WRAP_DEBUG("__wrap_SDL_CreateWindow");
    return sdl_window;
}

void __wrap_SDL_DestroyWindow(uint32_t flags)
{
    WRAP_DEBUG("__wrap_SDL_DestroyWindow");
}

SDL_Surface *__wrap_SDL_GetWindowSurface(SDL_Window * window)
{
    WRAP_DEBUG("__wrap_SDL_GetWindowSurface");
    return sdl_surface;
}

SDL_Surface *__wrap_SDL_CreateRGBSurfaceFrom(void * pixels,
    int width,
    int height,
    int depth,
    int pitch,
    Uint32 Rmask,
    Uint32 Gmask,
    Uint32 Bmask,
    Uint32 Amask)
{
    WRAP_DEBUG("__wrap_SDL_CreateRGBSurfaceFrom");
    return sdl_surface;
}

void __wrap_SDL_FreeSurface(SDL_Surface * surface)
{
    WRAP_DEBUG("__wrap_SDL_FreeSurface");
}

// SDL_BlitSurface
int __wrap_SDL_UpperBlit(
    SDL_Surface * src,
    const SDL_Rect * srcrect,
    SDL_Surface * dst,
    SDL_Rect * dstrect)
{
    WRAP_DEBUG("__wrap_SDL_BlitSurface");
    return 0;
}

int __wrap_SDL_UpdateWindowSurface(SDL_Window * window)
{
    WRAP_DEBUG("__wrap_SDL_UpdateWindowSurface");
    return 0;
}
#endif

