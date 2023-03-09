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

const char* cuda_get_error_message(CUresult error)
{
    const char* unknown_error = "UNKNOWN_ERROR";
    const char* error_str = NULL;
    CUresult res = cuGetErrorString(error, &error_str);
    if (res != CUDA_SUCCESS) {
        DEBUG_MSG("ERROR: Failed to get error string, res %d\n%s:%s:%d\n", res, __FILE__, __FUNCTION__, __LINE__);
        return unknown_error;
    }
    if (error_str == NULL) {
        return unknown_error;
    }
    return error_str;
}
