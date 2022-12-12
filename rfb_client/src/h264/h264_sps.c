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

// Sequence parameter set 7.3.2.1
static int h264_read_sps(int start, int end)
{
    uint8_t* buffer = app.enc_buf;
    struct h264_rbsp_t* rbsp = &app.h264.rbsp;

    RBSP_INIT(rbsp, &buffer[start], end - start);
    app.h264.sps.profile_idc = RBSP_READ_UN(rbsp, 8);
    app.h264.sps.constraint_set = RBSP_READ_UN(rbsp, 8);
    app.h264.sps.level_idc = RBSP_READ_UN(rbsp, 8);
    app.h264.sps.seq_parameter_set_id = RBSP_READ_UE(rbsp);
    if (app.h264.sps.profile_idc == 100 ||
        app.h264.sps.profile_idc == 110 ||
        app.h264.sps.profile_idc == 122 ||
        app.h264.sps.profile_idc == 244 ||
        app.h264.sps.profile_idc == 44 ||
        app.h264.sps.profile_idc == 83 ||
        app.h264.sps.profile_idc == 86 ||
        app.h264.sps.profile_idc == 118 ||
        app.h264.sps.profile_idc == 128 ||
        app.h264.sps.profile_idc == 138 || 
        app.h264.sps.profile_idc == 139 ||
        app.h264.sps.profile_idc == 134 ||
        app.h264.sps.profile_idc ==  135) {

        app.h264.sps.chroma_format_idc = RBSP_READ_UE(rbsp);
        //H264_RBSP_DEBUG(app.h264.sps.chroma_format_idc);
        if (app.h264.sps.chroma_format_idc == CHROMA_FORMAT_YUV444) {
            app.h264.sps.separate_colour_plane_flag = RBSP_READ_U1(rbsp);
            //H264_RBSP_DEBUG(app.h264.sps.separate_colour_plane_flag);
        }
        app.h264.sps.bit_depth_luma_minus8 = RBSP_READ_UE(rbsp);
        //H264_RBSP_DEBUG(app.h264.sps.bit_depth_luma_minus8);
        app.h264.sps.bit_depth_chroma_minus8 = RBSP_READ_UE(rbsp);
        //H264_RBSP_DEBUG(app.h264.sps.bit_depth_chroma_minus8);
        app.h264.sps.qpprime_y_zero_transform_bypass_flag = RBSP_READ_U1(rbsp);
        app.h264.sps.seq_scaling_matrix_present_flag = RBSP_READ_U1(rbsp);
        UNCOVERED_CASE(app.h264.sps.seq_scaling_matrix_present_flag, !=, 0);
    } else {
        // 7.4.2.1.1
        app.h264.sps.chroma_format_idc = CHROMA_FORMAT_YUV420;
    }
    // 7.4.2.1.1Sequence parameter set data semantics
    // Depending on the value of separate_colour_plane_flag, the value of the variable
    // ChromaArrayType is assigned as follows: –If separate_colour_plane_flag is equal to 0,
    // ChromaArrayType is set equal to chroma_format_idc.
    // –Otherwise (separate_colour_plane_flag is equal to 1), ChromaArrayType is set equal to 0.
    app.h264.sps.ChromaArrayType = (
        app.h264.sps.separate_colour_plane_flag == 0
    )? app.h264.sps.chroma_format_idc: 0;
    //H264_RBSP_DEBUG(app.h264.sps.ChromaArrayType);

    app.h264.sps.log2_max_frame_num = RBSP_READ_UE(rbsp) + 4;
    //H264_RBSP_DEBUG(app.h264.sps.log2_max_frame_num);
    app.h264.sps.pic_order_cnt_type = RBSP_READ_UE(rbsp);
    //H264_RBSP_DEBUG(app.h264.sps.pic_order_cnt_type);
    if (app.h264.sps.pic_order_cnt_type == 0) {
        app.h264.sps.log2_max_pic_order_cnt_lsb_minus4 = RBSP_READ_UE(rbsp);
        //H264_RBSP_DEBUG(app.h264.sps.log2_max_pic_order_cnt_lsb_minus4);
    }
    else if(app.h264.sps.pic_order_cnt_type == 1) {
        app.h264.sps.delta_pic_order_always_zero_flag = RBSP_READ_U1(rbsp);
        app.h264.sps.offset_for_non_ref_pic = RBSP_READ_SE(rbsp);
        app.h264.sps.offset_for_top_to_bottom_field = RBSP_READ_SE(rbsp);
        app.h264.sps.num_ref_frames_in_pic_order_cnt_cycle = RBSP_READ_UE(rbsp);
        UNCOVERED_CASE(app.h264.sps.num_ref_frames_in_pic_order_cnt_cycle, >, H264_MAX_REFS);
        for(int i = 0; i < app.h264.sps.num_ref_frames_in_pic_order_cnt_cycle; i++) {
            app.h264.sps.offset_for_ref_frame[i] = RBSP_READ_SE(rbsp);
        }
    }
    app.h264.sps.num_ref_frames = RBSP_READ_UE(rbsp);
    //H264_RBSP_DEBUG(app.h264.sps.num_ref_frames);
    app.h264.sps.gaps_in_frame_num_value_allowed_flag = RBSP_READ_U1(rbsp);
    //H264_RBSP_DEBUG(app.h264.sps.gaps_in_frame_num_value_allowed_flag);

    app.h264.sps.pic_width_in_mbs_minus1 = RBSP_READ_UE(rbsp);
    app.h264.sps.pic_height_in_map_units_minus1 = RBSP_READ_UE(rbsp);
    app.h264.sps.frame_mbs_only_flag = RBSP_READ_U1(rbsp);
    if (!app.h264.sps.frame_mbs_only_flag) {
        app.h264.sps.mb_adaptive_frame_field_flag = RBSP_READ_U1(rbsp);
    }
    app.h264.sps.direct_8x8_inference_flag = RBSP_READ_U1(rbsp);
    app.h264.sps.frame_cropping_flag = RBSP_READ_U1(rbsp);
    if (app.h264.sps.frame_cropping_flag) {
        app.h264.sps.frame_crop_left_offset = RBSP_READ_UE(rbsp);
        app.h264.sps.frame_crop_right_offset = RBSP_READ_UE(rbsp);
        app.h264.sps.frame_crop_top_offset = RBSP_READ_UE(rbsp);
        app.h264.sps.frame_crop_bottom_offset = RBSP_READ_UE(rbsp);
    }
    app.h264.sps.vui_parameters_present_flag = RBSP_READ_U1(rbsp);
    if (app.h264.sps.vui_parameters_present_flag) {
        app.h264.sps.aspect_ratio_info_present_flag = RBSP_READ_U1(rbsp);
        UNCOVERED_CASE(app.h264.sps.aspect_ratio_info_present_flag, !=, 0);
        app.h264.sps.overscan_info_present_flag = RBSP_READ_U1(rbsp);
        UNCOVERED_CASE(app.h264.sps.overscan_info_present_flag, !=, 0);
        app.h264.sps.video_signal_type_present_flag = RBSP_READ_U1(rbsp);
        UNCOVERED_CASE(app.h264.sps.video_signal_type_present_flag, !=, 0);
        app.h264.sps.chroma_loc_info_present_flag = RBSP_READ_U1(rbsp);
        UNCOVERED_CASE(app.h264.sps.chroma_loc_info_present_flag, !=, 0);
        app.h264.sps.timing_info_present_flag = RBSP_READ_U1(rbsp);
        UNCOVERED_CASE(app.h264.sps.timing_info_present_flag, !=, 0);
        app.h264.sps.nal_hrd_parameters_present_flag = RBSP_READ_U1(rbsp);
        UNCOVERED_CASE(app.h264.sps.nal_hrd_parameters_present_flag, !=, 0);
        app.h264.sps.vcl_hrd_parameters_present_flag = RBSP_READ_U1(rbsp);
        UNCOVERED_CASE(app.h264.sps.vcl_hrd_parameters_present_flag, !=, 0);
        app.h264.sps.pic_struct_present_flag = RBSP_READ_U1(rbsp);
        app.h264.sps.bitstream_restriction_flag = RBSP_READ_U1(rbsp);
        if (app.h264.sps.bitstream_restriction_flag) {
            app.h264.sps.motion_vectors_over_pic_boundaries_flag = RBSP_READ_U1(rbsp);
            app.h264.sps.max_bytes_per_pic_denom = RBSP_READ_UE(rbsp);
            app.h264.sps.max_bits_per_mb_denom = RBSP_READ_UE(rbsp);
            app.h264.sps.log2_max_mv_length_horizontal = RBSP_READ_UE(rbsp);
            app.h264.sps.log2_max_mv_length_vertical = RBSP_READ_UE(rbsp);
            app.h264.sps.max_num_reorder_frames = RBSP_READ_UE(rbsp);
            app.h264.sps.max_dec_frame_buffering = RBSP_READ_UE(rbsp);
        }
    }
    CALL(h264_is_more_rbsp(rbsp), error);
    return 0;

error:
    return -1;
}

