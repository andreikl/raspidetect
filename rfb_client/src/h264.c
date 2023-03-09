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
#include "dxva.h"

#include "h264.h"

extern struct app_state_t app;

H264_INIT()

void h264_destroy()
{
#ifdef ENABLE_DXVA
    dxva_destroy();
#endif //ENABLE_DXVA

    LINKED_HASH_DESTROY(app.h264.headers, header);
}

int h264_init()
{
    LINKED_HASH_INIT(app.h264.headers, MAX_SLICES);

#ifdef ENABLE_DXVA
    CALL(dxva_init(), error);
#endif //ENABLE_DXVA

    return 0;
error:
    return -1;
}

#include "h264_helpers.c"
#include "h264_sps.c"
#include "h264_pps.c"
#include "h264_header.c"

#ifdef ENABLE_H264_SLICE 
#include "h264_cabac.c"
#include "h264_slice.c"
#endif // ENABLE_H264_SLICE

//h264_slice.c:2090
static int h264_slice_layer_without_partitioning_rbsp(int start, int end)
{
    struct h264_slice_header_t* header = LINKED_HASH_GET_HEAD(app.h264.headers);
    struct h264_rbsp_t* rbsp = &app.h264.rbsp;
    uint8_t* buffer = app.enc_buf;

    RBSP_INIT(rbsp, &buffer[start], end - start);

    CALL(h264_read_slice_header(), error);

    //ffmpeg: ignores redundant_pic_count but has check
    UNCOVERED_CASE(header->redundant_pic_cnt, !=, 0);

    //ffmpeg: check if slice is first but setup isn't finished
    if (header->first_mb_in_slice != 0 && app.h264.setup_finished == 0) {
        UNCOVERED_CASE(header->first_mb_in_slice, !=, 0);
        UNCOVERED_CASE(app.h264.setup_finished, ==, 0);
    }

    if (app.h264.setup_finished == 0) {
        //ffmpeg: ff_h264_execute_ref_pic_marking
        if (!header->adaptive_ref_pic_marking_mode_flag && !header->IdrPicFlag) {
            UNCOVERED_CASE(header->adaptive_ref_pic_marking_mode_flag, ==, 0);
            UNCOVERED_CASE(header->IdrPicFlag, ==, 0);
        }

        //TODO:
        //h264_field_start
        //h264_slice_init

        app.h264.setup_finished = 1;
    }

    for (int i = 0; i < header->mmco_size; i++) {
        if (
            header->mmco[i].mmco == MMCO_SHORT2UNUSED ||
            header->mmco[i].mmco == MMCO_SHORT2LONG
        ) {
            int pic_num = header->mmco[i].short_picNumX;
            if (header->picture_structure != H264_PICT_FRAME) {
                pic_num >>= 1;
            }
        }
    }

#ifdef ENABLE_H264_SLICE 
    CALL(h264_read_slice_data(), error);
    H264_RBSP_DEBUG_MSG(*app.h264.rbsp.p);
    CALL(!h264_is_more_rbsp(&app.h264.rbsp), error);
#endif //ENABLE_H264_SLICE

    return 0;
error:
    return -1;
}

int h264_decode()
{
    //TODO: to delete
    //fwrite(app.h264.buf, 1, app.h264.buf_length, stdout);

    int start = 0, end = 0;
    while(!find_nal(app.enc_buf, app.enc_buf_length, &start, &end)) {
        struct h264_nal_t* header = (struct h264_nal_t*)&app.enc_buf[start++];

        app.h264.nal_unit_type = header->u.nal_unit_type;
        app.h264.nal_ref_idc = header->u.nal_ref_idc;

        UNCOVERED_CASE(app.h264.nal_unit_type, ==, 14);
        UNCOVERED_CASE(app.h264.nal_unit_type, ==, 20);
        UNCOVERED_CASE(app.h264.nal_unit_type, ==, 21);

        DEBUG_MSG("INFO: nal_unit_type %d(%s), size %d",
            app.h264.nal_unit_type,
            h264_get_nal_unit_type(app.h264.nal_unit_type),
            end - start);
        DEBUG_MSG("INFO: nal_ref_idc (%d, %s)", app.h264.nal_ref_idc, h264_get_nal_ref_idc(app.h264.nal_ref_idc));

        switch (app.h264.nal_unit_type) {
            case NAL_UNIT_TYPE_SPS:
                h264_read_sps(start, end);
                break;

            case NAL_UNIT_TYPE_PPS:
                h264_read_pps(start, end);
                break;

            case NAL_UNIT_TYPE_CODED_SLICE_IDR:
            case NAL_UNIT_TYPE_CODED_SLICE_NON_IDR:
            {
                h264_slice_layer_without_partitioning_rbsp(start, end);
                #ifdef ENABLE_DXVA
                    CALL(dxva_decode(start, end), cleanup);
                #endif //ENABLE_DXVA
                break;
            }

            default:
                //return -1;
        }

        //char* rbsp = &app.buffer[start + 1];
        //int rbsp_size = end - start - 1;

        DEBUG_MSG("INFO: rbsp package has processed: start (%d) end (%d)", start, end);
    }
    return 0;

cleanup:
    DEBUG_MSG("h264_decode cleanup")
    if (errno == 0)
        errno = EAGAIN;
    return -1;
}
