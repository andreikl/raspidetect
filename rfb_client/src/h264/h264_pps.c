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

// Picture parameter set 7.3.2.2
static int h264_read_pps(int start, int end)
{
    uint8_t* buffer = app.enc_buf;
    struct h264_rbsp_t* rbsp = &app.h264.rbsp;

    RBSP_INIT(&app.h264.rbsp, &buffer[start], end - start);
    app.h264.pps.pic_parameter_set_id = RBSP_READ_UE(rbsp);
    app.h264.pps.seq_parameter_set_id = RBSP_READ_UE(rbsp);
    UNCOVERED_CASE(app.h264.sps.seq_parameter_set_id, !=, app.h264.pps.seq_parameter_set_id);
    app.h264.pps.entropy_coding_mode_flag = RBSP_READ_U1(rbsp);
    //H264_RBSP_DEBUG(app.h264.pps.entropy_coding_mode_flag);
    app.h264.pps.bottom_field_pic_order_in_frame_present_flag = RBSP_READ_U1(rbsp);
    app.h264.pps.num_slice_groups_minus1 = RBSP_READ_UE(rbsp);
    UNCOVERED_CASE(app.h264.pps.num_slice_groups_minus1, !=, 0);
    app.h264.pps.ref_count[0] = RBSP_READ_UE(rbsp) + 1;
    H264_RBSP_DEBUG(app.h264.pps.ref_count[0]);    
    UNCOVERED_CASE(app.h264.pps.ref_count[0], >, H264_MAX_REFS);
    app.h264.pps.ref_count[1] = RBSP_READ_UE(rbsp) + 1;
    H264_RBSP_DEBUG(app.h264.pps.ref_count[1]);
    UNCOVERED_CASE(app.h264.pps.ref_count[1], >, H264_MAX_REFS);
    app.h264.pps.weighted_pred_flag = RBSP_READ_U1(rbsp);
    app.h264.pps.weighted_bipred_idc = RBSP_READ_UN(rbsp, 2);
    app.h264.pps.pic_init_qp_minus26 = RBSP_READ_SE(rbsp);
    app.h264.pps.pic_init_qs_minus26 = RBSP_READ_SE(rbsp);
    app.h264.pps.chroma_qp_index_offset = RBSP_READ_SE(rbsp);
    app.h264.pps.deblocking_filter_control_present_flag = RBSP_READ_U1(rbsp);
    app.h264.pps.constrained_intra_pred_flag = RBSP_READ_U1(rbsp);
    app.h264.pps.redundant_pic_cnt_present_flag = RBSP_READ_U1(rbsp);
    //H264_RBSP_DEBUG(app.h264.pps.redundant_pic_cnt_present_flag);
    if (RBSP_IS_MORE(rbsp)) {
        app.h264.pps.transform_8x8_mode_flag = RBSP_READ_U1(rbsp);
        UNCOVERED_CASE(app.h264.pps.transform_8x8_mode_flag, ==, 0);
        app.h264.pps.pic_scaling_matrix_present_flag = RBSP_READ_U1(rbsp);
        UNCOVERED_CASE(app.h264.pps.pic_scaling_matrix_present_flag, !=, 0);
        app.h264.pps.second_chroma_qp_index_offset = RBSP_READ_SE(rbsp);
    }
    CALL(h264_is_more_rbsp(rbsp), error);
    return 0;

error:
    return -1;
}
