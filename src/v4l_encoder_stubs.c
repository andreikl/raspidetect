// Raspidetect

// Copyright (C) 2022 Andrei Klimchuk <andrew.klimchuk@gmail.com>

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

extern struct app_state_t app;
extern struct input_t input;
extern struct filter_t filters[MAX_FILTERS];
extern struct output_t outputs[MAX_OUTPUTS];
extern int is_abort;

// wrapper to allow to run test on platform where h264 Jetson encoder isn't available
#ifdef V4L_ENCODER_WRAP
    int NvBufferMemSyncForDevice(int fd, unsigned int plane, void **addr)
    {
        DEBUG("wrap NvBufferMemSyncForDevice, fd: %d, plane: %d", fd, plane);
        return -1;
    }
#endif
