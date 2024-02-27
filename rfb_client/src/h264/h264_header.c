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

static int h264_read_ref_pic_list_modification()
{
    struct h264_slice_header_t* header = LINKED_HASH_GET_HEAD(app.h264.headers);
    struct h264_rbsp_t* rbsp = &app.h264.rbsp;

    for (int list = 0; list < header->list_count; list++) {
        int ref_pic_list_reordering_flag = RBSP_READ_U1(rbsp);
        //H264_RBSP_DEBUG(ref_pic_list_reordering_flag);
        if (!ref_pic_list_reordering_flag) {
            continue;
        }

        for (int i = 0; ; i++) {
            int reordering_of_pic_nums_idc = RBSP_READ_UE(rbsp);
            //H264_RBSP_DEBUG(reordering_of_pic_nums_idc);

            if (reordering_of_pic_nums_idc == 3) {
                break;
            }

            if (i >= header->ref_count[list]) {
                DEBUG_MSG("ERROR: reference count overflow.\n");
                return -1;
            }

            if (reordering_of_pic_nums_idc > 3) {
                DEBUG_MSG("ERROR: invalid reordering_of_pic_nums_idc %d.\n",
                    reordering_of_pic_nums_idc);
                return -1;
            }

            // ffmpeg: sl->ref_modifications[list][index].val
            if (reordering_of_pic_nums_idc < 2) {
                header->modifications[list][i].abs_diff_pic_num_minus1
                    = RBSP_READ_UE(rbsp);
            }
            else if (reordering_of_pic_nums_idc == 2) {
                header->modifications[list][i].long_term_pic_num
                    = RBSP_READ_UE(rbsp);
            }
            // ffmpeg: sl->ref_modifications[list][index].op
            header->modifications[list][i].reordering_of_pic_nums_idc
                 = reordering_of_pic_nums_idc;
        }
    }

    return 0;
}

static int h264_read_pred_weight_table()
{
    struct h264_slice_header_t* header = LINKED_HASH_GET_HEAD(app.h264.headers);
    struct h264_rbsp_t* rbsp = &app.h264.rbsp;

    //ffmpeg: pwt->luma_log2_weight_denom 
    header->luma_log2_weight_denom = RBSP_READ_UE(rbsp);
    //H264_RBSP_DEBUG(header->luma_log2_weight_denom);
    if (header->luma_log2_weight_denom > 7) {
        DEBUG_MSG("ERROR: luma_log2_weight_denom %d is out of range.\n",
            header->luma_log2_weight_denom);
        return -1;
    }
    int luma_def = 1 << header->luma_log2_weight_denom;

    // ffmpeg: sps->chroma_format_idc
    int chroma_def = 0;
    if (app.h264.sps.ChromaArrayType) {
        //ffmpeg: pwt->chroma_log2_weight_denom
        header->chroma_log2_weight_denom = RBSP_READ_UE(rbsp);
        //H264_RBSP_DEBUG(header->chroma_log2_weight_denom);
        if (header->chroma_log2_weight_denom > 7) {
            DEBUG_MSG("ERROR: chroma_log2_weight_denom %d is out of range\n",
                header->chroma_log2_weight_denom);
            return -1;
        }
        chroma_def = 1 << header->chroma_log2_weight_denom;
    }

    for (int list = 0; list < header->list_count; list++) {
        for (int i = 0; i < header->ref_count[0]; i++) {
            //ffmpeg: pwt->luma_weight_flag[list];
            header->weights[list][i].luma_weight_flag = RBSP_READ_U1(rbsp);
            if (header->weights[list][i].luma_weight_flag) {
                // ffmpeg: pwt->luma_weight[i][list][0], pwt->luma_weight[i][list][1]
                header->weights[list][i].luma_weight = RBSP_READ_SE(rbsp);
                header->weights[list][i].luma_offset = RBSP_READ_SE(rbsp);
                //ffmpeg: uses additional pwt->use_weight
            } else {
                //ffmpeg: copied from ffmpeg
                header->weights[list][i].luma_weight = luma_def;
                header->weights[list][i].luma_offset = 0;
            }
            // ffmpeg: sps->chroma_format_idc additional check to chroma_format_idc
            // ffmpeg: pwt->chroma_weight_flag[list]
            header->weights[list][i].chroma_weight_flag = RBSP_READ_U1(rbsp);
            if (header->weights[list][i].chroma_weight_flag) {
                for (int j = 0; j < 2; j++) {
                    // ffmpeg: pwt->chroma_weight[i][list][j][0] pwt->chroma_weight[i][list][j][1]
                    header->weights[list][i].chroma_weight[j] = RBSP_READ_SE(rbsp);
                    header->weights[list][i].chroma_offset[j] = RBSP_READ_SE(rbsp);
                    //ffmpeg: uses additional pwt->use_weight_chroma
                }
            } else {
                for (int j = 0; j < 2; j++) {
                    header->weights[list][i].chroma_weight[j] = chroma_def;
                    header->weights[list][i].chroma_offset[j] = 0;
                    //ffmpeg: uses additional pwt->use_weight_chroma
                }
            }
        }
        //ffmpeg does additional transformation if picture_structure == PICT_FRAME
        //if (picture_structure == PICT_FRAME) {
    }
    //ffmpeg set pwt->use_weight = pwt->use_weight || pwt->use_weight_chroma;
    return 0;
}

static int h264_read_dec_ref_pic_marking()
{
    struct h264_slice_header_t* header = LINKED_HASH_GET_HEAD(app.h264.headers);
    struct h264_rbsp_t* rbsp = &app.h264.rbsp;

    // 7.4.1 NAL unit semantic
    if (header->IdrPicFlag) {
        // ffmpeg: ignores field
        header->no_output_of_prior_pics_flag = RBSP_READ_U1(rbsp);
        //H264_RBSP_DEBUG(header->no_output_of_prior_pics_flag);

        // ffmpeg: doesn't save flag
        header->long_term_reference_flag = RBSP_READ_U1(rbsp);
        //H264_RBSP_DEBUG(header->long_term_reference_flag);
        if (header->long_term_reference_flag) {
            // ffmpeg: set mmco[0].opcode = MMCO_LONG, mmco[0].long_arg = 0; nb_mmco = 1;
            DEBUG_MSG("Slice is marked as 'used for reference' (long-term reference) %d",
                header->long_term_reference_flag);

            header->LongTermFrameIdx = 0;
            header->MaxLongTermFrameIdx = 0;
        } else {
            header->LongTermFrameIdx = -1;
            header->MaxLongTermFrameIdx = -1;

            DEBUG_MSG("Slice is marked as 'used for reference' (short-term reference) %d",
                header->long_term_reference_flag);
        }
        //ffmpeg: set adaptive_ref_pic_marking_mode_flag -> sl->explicit_ref_marking = 1
    } else {
        // ffmpeg: sl->explicit_ref_marking
        header->adaptive_ref_pic_marking_mode_flag = RBSP_READ_U1(rbsp);
        H264_RBSP_DEBUG(header->adaptive_ref_pic_marking_mode_flag);
        if (header->adaptive_ref_pic_marking_mode_flag) {
            int i = 0;
            do {
                if (i > H264_MAX_REFS) {
                    fprintf(stderr,
                        "ERROR: reference(%d) exceeded  H264_MAX_REFS(%d).\n",
                        i,
                        H264_MAX_REFS
                    );
                    return -1;
                }
                //ffmpeg: mmco[i].opcode
                header->mmco[i].mmco = RBSP_READ_UE(rbsp);
                if (
                    header->mmco[i].mmco == MMCO_SHORT2UNUSED ||
                    header->mmco[i].mmco == MMCO_SHORT2LONG
                ) {
                    //ffmpeg: short_picNumX -> mmco[i].short_pic_num
                    int difference_of_pic_nums_minus1 = RBSP_READ_UE(rbsp);
                     //8-39
                    header->mmco[i].short_picNumX =
                        (
                            header->curr_pic_num - difference_of_pic_nums_minus1 - 1
                        ) & (header->max_pic_num - 1);
                }
                //ffmpeg: read only mmco[i].long_arg;
                //ffmpeg: add validation range ref validation
                if (header->mmco[i].mmco == MMCO_LONG2UNUSED) {
                    header->mmco[i].long_term_pic_num = RBSP_READ_UE(rbsp);
                }
                if (
                    header->mmco[i].mmco == MMCO_SHORT2LONG ||
                    header->mmco[i].mmco == MMCO_LONG
                ) {
                    header->mmco[i].long_term_frame_idx = RBSP_READ_UE(rbsp);
                }
                if (header->mmco[i].mmco == MMCO_SET_MAX_LONG) {
                    header->mmco[i].max_long_term_frame_idx_plus1 = RBSP_READ_UE(rbsp);
                }
                i++;
            } while(header->mmco[i].mmco != 0);
            //ffmpeg: nb_mmco
            header->mmco_size = i;
        }
    }
    return 0;
}

// Slice header 7.3.3
//h264_slice.c:1725
static int h264_read_slice_header()
{
    struct h264_slice_header_t* header = LINKED_HASH_GET_HEAD(app.h264.headers);
    struct h264_rbsp_t* rbsp = &app.h264.rbsp;

    //set default values
    for(int i = 0; i < 2; i++)
        for(int j = 0; j < H264_MAX_REFS; j++) {
            header->modifications[i][j].reordering_of_pic_nums_idc = -1;
            header->modifications[i][j].abs_diff_pic_num_minus1 = -1;
            header->modifications[i][j].long_term_pic_num = -1;
        }

    // 7-1
    header->IdrPicFlag = app.h264.nal_unit_type == NAL_UNIT_TYPE_CODED_SLICE_IDR? 1: 0;
    if (header->IdrPicFlag) {
        header->PrevRefFrameNum = 0;
    } else {
        header->PrevRefFrameNum = header->frame_num;
    }

    //ffmpeg: sl->first_mb_addr
    header->first_mb_in_slice = RBSP_READ_UE(rbsp);
    //H264_RBSP_DEBUG(header->first_mb_in_slice);

    // ffmpeg: slice_type -> sl->slice_type
    // slice_type_origin != slice_type -> sl->slice_type_fixed
    header->slice_type_origin = header->slice_type = RBSP_READ_UE(rbsp);
    if (header->slice_type >= 5) {
        header->slice_type = header->slice_type - 5;
        H264_RBSP_DEBUG("slice_type: %s", h264_get_slice_type(header->slice_type));
    }
    else {
        H264_RBSP_DEBUG("slice_type: %d", header->slice_type);
    }

    // ffmpeg: copy check
    if (header->IdrPicFlag && header->slice_type != SliceTypeI) {
        DEBUG_MSG("ERROR: A non-intra slice in an IDR NAL unit.\n");
        return -1;
    }

    // ffmpeg: sl->pps_id
    header->pic_parameter_set_id = RBSP_READ_UE(rbsp);
    //TODO: create pps lists
    UNCOVERED_CASE(header->pic_parameter_set_id, !=, 0);
    //H264_RBSP_DEBUG(header->pic_parameter_set_id);

    //TODO: check h264 block-scheme
    UNCOVERED_CASE(app.h264.sps.separate_colour_plane_flag, !=, 0);

    // ffmpeg: sl->frame_num
    // TODO: to find what is h->poc.frame_num
    header->frame_num = RBSP_READ_UN(rbsp, app.h264.sps.log2_max_frame_num);
    H264_RBSP_DEBUG("frame_num: %d", header->frame_num);

    //7.4.3 Slice header semantics
    //ffmepg: picture_structure -> sl->picture_structure
    // sl->mb_field_decoding_flag, sl->curr_pic_num, sl->max_pic_num
    // ffmpeg: maxRefCount = picture_structure == PICT_FRAME ? 15 : 31;
    if (app.h264.sps.frame_mbs_only_flag) {
        header->picture_structure = H264_PICT_FRAME;
    }
    else {
        header->field_pic_flag = RBSP_READ_U1(rbsp);
        //H264_RBSP_DEBUG(header->field_pic_flag);

        if (header->field_pic_flag) {
            header->bottom_field_flag = RBSP_READ_U1(rbsp);
            header->picture_structure
                = H264_PICT_TOP_FIELD + header->bottom_field_flag;
            //H264_RBSP_DEBUG(header->bottom_field_flag);
        } else {
            header->picture_structure = H264_PICT_FRAME;
        }
    }

    // ffmpeg
    if (header->picture_structure == H264_PICT_FRAME) {
        header->curr_pic_num = header->frame_num;
        header->max_pic_num = 1 << app.h264.sps.log2_max_frame_num;
    }
    else {
        header->curr_pic_num = 2 * header->frame_num + 1;
        header->max_pic_num  = 1 << (app.h264.sps.log2_max_frame_num + 1);
    }

    // 7.4.1 NAL unit semantics
    // ffmpeg: doesn't save value
    if (header->IdrPicFlag) {
        header->idr_pic_id = RBSP_READ_UE(rbsp);
        //H264_RBSP_DEBUG(header->idr_pic_id);
    }
    //H264_RBSP_DEBUG(app.h264.sps.pic_order_cnt_type);
    // ffmpeg: sps->poc_type
    if (app.h264.sps.pic_order_cnt_type == 0) {
        //ffmpeg: sl->poc_lsb
        header->pic_order_cnt_lsb = RBSP_READ_UN(
            rbsp,
            app.h264.sps.log2_max_pic_order_cnt_lsb_minus4 + 4
        );
        //H264_RBSP_DEBUG(header->pic_order_cnt_lsb);

        //ffmpeg: pps->pic_order_present == 1 && picture_structure == PICT_FRAME
        if (
            app.h264.pps.bottom_field_pic_order_in_frame_present_flag &&
            !header->field_pic_flag
        ) {
            //ffmpeg: sl->delta_poc_bottom
            header->delta_pic_order_cnt_bottom = RBSP_READ_SE(rbsp);
            //H264_RBSP_DEBUG(header->delta_pic_order_cnt_bottom);
        }
    }
    if (
        app.h264.sps.pic_order_cnt_type == 1 &&
        !app.h264.sps.delta_pic_order_always_zero_flag
    ) {
        //ffmpeg: sl->delta_poc[0], sl->delta_poc[1]
        UNCOVERED_CASE(app.h264.sps.pic_order_cnt_type, ==, 1);
        UNCOVERED_CASE(app.h264.sps.delta_pic_order_always_zero_flag, ==, 0);
    }

    //ffmpeg: sl->redundant_pic_count
    if (app.h264.pps.redundant_pic_cnt_present_flag) {
        header->redundant_pic_cnt = RBSP_READ_UE(rbsp);
        //H264_RBSP_DEBUG(header->redundant_pic_cnt);
    } else {
        header->redundant_pic_cnt = 0;
    }
    
    if (header->slice_type == SliceTypeB) {
        //ffmpeg: sl->direct_spatial_mv_pred
        header->direct_spatial_mv_pred_flag = RBSP_READ_U1(rbsp);
        //H264_RBSP_DEBUG(header->direct_spatial_mv_pred_flag);
    }

    //TODO: check list is overflow
    //if (ref_count[0] - 1 > max[0] || (list_count == 2 && (ref_count[1] - 1 > max[1]))) {
    //    av_log(logctx, AV_LOG_ERROR, "reference overflow %u > %u or %u > %u\n",

    if (
        header->slice_type == SliceTypeP ||
        header->slice_type == SliceTypeSP ||
        header->slice_type == SliceTypeB
    ) {
        // ffmpeg: doesn't save value
        header->num_ref_idx_active_override_flag = RBSP_READ_U1(rbsp);
        if (header->num_ref_idx_active_override_flag) {
            // ffmpeg: sl->ref_count[0]. ref_count[1]
            header->ref_count[0] = RBSP_READ_UE(rbsp) + 1;
            if (header->slice_type == SliceTypeB) {
                header->ref_count[1]  = RBSP_READ_UE(rbsp) + 1;
            } else {
                header->ref_count[1]  = 1;
            }
        } else {
            header->ref_count[0] = app.h264.pps.ref_count[0];
            header->ref_count[1] = app.h264.pps.ref_count[1];
        }

        header->list_count = (header->slice_type == SliceTypeB)? 2: 1;
    } else {
        header->list_count = 0;
    }

    //TODO: to implement ref_pic_list_mvc_modification()
    UNCOVERED_CASE(app.h264.nal_unit_type, ==, 20);
    UNCOVERED_CASE(app.h264.nal_unit_type, ==, 21);

    //TODO: set ref_count to 0 if error is happened
    CALL(h264_read_ref_pic_list_modification(), error);

    //ffmpeg: ignores SP and SI and fill 0
    // sl->pwt.luma_weight_flag and sl->pwt.chroma_weight_flag[i]
    if ((app.h264.pps.weighted_pred_flag 
            && (
                header->slice_type == SliceTypeP ||
                header->slice_type == SliceTypeSP
            )
        )
        || (app.h264.pps.weighted_bipred_idc == 1 && header->slice_type == SliceTypeB)
    ) {
        CALL(h264_read_pred_weight_table(), error);
    }
    // 8.2.5.1 Sequence of operations for decoded reference picture marking process
    //ffmpeg: ref_idc
    if (app.h264.nal_ref_idc) {
        CALL(h264_read_dec_ref_pic_marking(), error);
    }

    //ffmpeg: entropy_coding_mode_flag -> cabac
    //ffmpeg: header->slice_type -> slice_type_nos
    //ffmpeg: ignores SliceTypeSI
    if (
        app.h264.pps.entropy_coding_mode_flag &&
        header->slice_type != SliceTypeI &&
        header->slice_type != SliceTypeSI
    ) {
        header->cabac_init_idc = RBSP_READ_UE(rbsp);
        if (header->cabac_init_idc > 2) {
            DEBUG_MSG("ERROR: cabac_init_idc (%d) overflow.\n",
                header->cabac_init_idc);
            return -1;
        }
    }

    //ffmpeg: qscale = init_qp + slice_qp_delta
    //ffmpeg: TODO: validates qscale > 51 + 6 * (sps->bit_depth_luma - 8)
    //ffmpeg: fills sl->chroma_qp[0] and sl->chroma_qp[1]
    header->slice_qp_delta = RBSP_READ_SE(rbsp);
    //H264_RBSP_DEBUG(header->slice_qp_delta);

    // ffmpeg: ignores if
    if (
        header->slice_type == SliceTypeSP ||
        header->slice_type == SliceTypeSI
    ) {
        // ffmpeg: ignores if and value
        if (header->slice_type == SliceTypeSP) {
            header->sp_for_switch_flag = RBSP_READ_U1(rbsp);
        }
        // ffmpeg: don't save value
        header->slice_qs_delta = RBSP_READ_SE(rbsp);
    }

    //ffmpeg: deblocking_filter_control_present_flag -> deblocking_filter_parameters_present
    if (app.h264.pps.deblocking_filter_control_present_flag) {
        //ffmpeg: validate value disable_deblocking_filter_idc > 2 -> message
        //ffmpeg: disable_deblocking_filter_idc -> deblocking_filter
        header->disable_deblocking_filter_idc = RBSP_READ_UE(rbsp);
        //H264_RBSP_DEBUG(header->disable_deblocking_filter_idc);

        if (header->disable_deblocking_filter_idc != 1) {
            //ffmpeg: caculate slice_alpha_c0_offset and slice_beta_offset values
            header->slice_alpha_c0_offset_div2 = RBSP_READ_SE(rbsp);
            //H264_RBSP_DEBUG(header->slice_alpha_c0_offset_div2);
            header->slice_beta_offset_div2 = RBSP_READ_SE(rbsp);
            //H264_RBSP_DEBUG(header->slice_beta_offset_div2);
        }
    }

    UNCOVERED_CASE(app.h264.pps.num_slice_groups_minus1, >, 0);

    header->PicWidthInMbs = app.h264.sps.pic_width_in_mbs_minus1 + 1; //7-13
    header->PicHeightInMapUnits = app.h264.sps.pic_height_in_map_units_minus1 + 1; //7-16
    header->PicSizeInMapUnits = header->PicWidthInMbs *
        header->PicHeightInMapUnits; //7-17
    header->FrameHeightInMbs = (2 - app.h264.sps.frame_mbs_only_flag) *
        header->PicHeightInMapUnits; //7-18
    header->MbaffFrameFlag = (
        app.h264.sps.mb_adaptive_frame_field_flag &&
        !header->field_pic_flag
    ); //7-25
    header->PicHeightInMbs = header->FrameHeightInMbs /
        (1 + header->field_pic_flag); //7-26
    header->PicSizeInMbs = header->PicWidthInMbs *
        header->PicHeightInMbs; //7-29

    // 8.2.5.2 Decoding process for gaps in frame_num This process is invoked when frame_num is not
    // equal to PrevRefFrameNum and is not equal to (PrevRefFrameNum + 1) % MaxFrameNum.
    // 7.4.3 Slice header semantics frame_num
    // if (header->IdrPicFlag) {
    //     UNCOVERED_CASE(header->frame_num, !=, 0);
    // } else {
    //     UNCOVERED_CASE(header->frame_num, !=, header->PrevRefFrameNum);
    // }

    if (header->list_count > 0) {
        DEBUG_MSG("list_count: %d", header->list_count);
        for (int i = 0; i < header->list_count; i++) {
            DEBUG_MSG("ref_count[i]: %d", header->ref_count[i]);
        }

    }

    return 0;

error:
    return -1;
}
