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

#include <khash.h>

#include "main.h"
#include "utils.h"

#include "dxva.h"
#include "h264.h"

extern struct app_state_t app;

static const uint8_t default_scaling4[2][16] = {
    {  6, 13, 20, 28, 13, 20, 28, 32,
      20, 28, 32, 37, 28, 32, 37, 42 },
    { 10, 14, 20, 24, 14, 20, 24, 27,
      20, 24, 27, 30, 24, 27, 30, 34 }
};

static const uint8_t default_scaling8[2][64] = {
    {  6, 10, 13, 16, 18, 23, 25, 27,
      10, 11, 16, 18, 23, 25, 27, 29,
      13, 16, 18, 23, 25, 27, 29, 31,
      16, 18, 23, 25, 27, 29, 31, 33,
      18, 23, 25, 27, 29, 31, 33, 36,
      23, 25, 27, 29, 31, 33, 36, 38,
      25, 27, 29, 31, 33, 36, 38, 40,
      27, 29, 31, 33, 36, 38, 40, 42 },
    {  9, 13, 15, 17, 19, 21, 22, 24,
      13, 13, 17, 19, 21, 22, 24, 25,
      15, 17, 19, 21, 22, 24, 25, 27,
      17, 19, 21, 22, 24, 25, 27, 28,
      19, 21, 22, 24, 25, 27, 28, 30,
      21, 22, 24, 25, 27, 28, 30, 32,
      22, 24, 25, 27, 28, 30, 32, 33,
      24, 25, 27, 28, 30, 32, 33, 35 }
};

static const uint8_t start_code[] = { 0, 0, 1 };

#include "dxva_helpers.c"

void dxva_destroy()
{
    HRESULT res;

    if (app.dxva.device != NULL) {
        CoTaskMemFree(app.dxva.cfg_list);
        app.dxva.cfg_count = 0;
        app.dxva.cfg_list = NULL;
    }

    if (app.dxva.decoder != NULL) {
        IDirectXVideoDecoderService_Release(app.dxva.decoder);
    }
    if (app.dxva.service != NULL) {
        IDirectXVideoDecoderService_Release(app.dxva.service);
    }
    if (app.dxva.device != NULL) {
        res = IDirect3DDeviceManager9_CloseDeviceHandle(app.dxva.device_manager, app.dxva.device);
        if (FAILED(res)) {
            DEBUG_MSG("ERROR: Can't close Direct3DDevice device");
        }
    }
    if (app.dxva.device_manager != NULL) {
        IDirect3DDeviceManager9_Release(app.dxva.device_manager);
    }
}

int dxva_init()
{
    HRESULT h_res;
    int res;

    unsigned reset_token;
    h_res = DXVA2CreateDirect3DDeviceManager9(
        &reset_token,
        &app.dxva.device_manager
    );
    if (FAILED(h_res)) {
        DEBUG_MSG("ERROR: Can't create device manager");
        goto close;
    }

    h_res = IDirect3DDeviceManager9_ResetDevice(app.dxva.device_manager, app.d3d.dev, reset_token);
    if (FAILED(h_res)) {
        if (h_res == E_INVALIDARG) {
            DEBUG_MSG("ERROR: Can't reset Direct3D device, res: E_INVALIDARG");
        } else if (h_res == D3DERR_INVALIDCALL) {
            DEBUG_MSG("ERROR: Can't reset Direct3D device, res: D3DERR_INVALIDCALL");
        }
    }

    h_res = IDirect3DDeviceManager9_OpenDeviceHandle(app.dxva.device_manager, &app.dxva.device);
    if (FAILED(h_res)) {
        if (h_res == DXVA2_E_NOT_INITIALIZED) {
            DEBUG_MSG("ERROR: Can't open Direct3D device, res: DXVA2_E_NOT_INITIALIZED");
        } else {
            DEBUG_MSG("ERROR: Can't open Direct3D device, res: %ld", h_res);
        }
        goto close;
    }

    h_res = IDirect3DDeviceManager9_GetVideoService(
        app.dxva.device_manager,
        app.dxva.device,
        &IID_IDirectXVideoDecoderService,
        (void **)&app.dxva.service);
    if (FAILED(h_res)) {
        DEBUG_MSG("ERROR: Can't get Direct3D decoder service");
        goto close;
    }

    res = dxva_find_decoder();
    if (res) {
        DEBUG_MSG("ERROR: Can't find NV12 decoder");
        dxva_print_guid(H264CODEC);
        goto close;
    }

    DXVA2_VideoDesc desc;
    desc.SampleWidth = app.server_width;
    desc.SampleHeight = app.server_height;
    desc.SampleFormat.SampleFormat = DXVA2_SampleUnknown;
    desc.SampleFormat.VideoChromaSubsampling = DXVA2_VideoChromaSubsampling_Unknown;
    desc.SampleFormat.NominalRange = DXVA2_NominalRange_Unknown;
    desc.SampleFormat.VideoTransferMatrix = DXVA2_VideoTransferMatrix_Unknown;
    desc.SampleFormat.VideoLighting = DXVA2_VideoLighting_Unknown;
    desc.SampleFormat.VideoPrimaries = DXVA2_VideoPrimaries_Unknown;
    desc.SampleFormat.VideoTransferFunction = DXVA2_VideoTransFunc_Unknown;
    desc.Format = H264CODEC_FORMAT;
    desc.InputSampleFreq.Numerator = 0;
    desc.InputSampleFreq.Denominator = 0;
    desc.OutputFrameFreq.Numerator = 0;
    desc.OutputFrameFreq.Denominator = 0;
    desc.UABProtectionLevel = FALSE;

    h_res = IDirectXVideoDecoderService_GetDecoderConfigurations(
        app.dxva.service,
        &H264CODEC,
        &desc,
        NULL,
        &app.dxva.cfg_count,
        &app.dxva.cfg_list
    );
    if (FAILED(h_res)) {
        DEBUG_MSG("ERROR: Can't get video configuration");
        goto close;
    }

    res = dxva_find_config();
    if (res) {
        DEBUG_MSG("ERROR: Can't find config which support DXVA_Slice_H264_Long format");
        dxva_print_config();
        goto close;
    }

    h_res = IDirectXVideoDecoderService_CreateSurface(
        app.dxva.service,
        app.server_width,
        app.server_height, 
        SCREEN_BUFFERS - 1, 
        H264CODEC_FORMAT, 
        D3DPOOL_DEFAULT, 
        0,
        DXVA2_VideoDecoderRenderTarget,
        app.d3d.surfaces,
        NULL);

    if (FAILED(h_res)) {
        DEBUG_MSG("ERROR: Can't create video render target");
        goto close;
    }

    h_res = IDirectXVideoDecoderService_CreateVideoDecoder(
        app.dxva.service,
        &H264CODEC,
        &desc,
        app.dxva.cfg,
        app.d3d.surfaces,
        SCREEN_BUFFERS,
        &app.dxva.decoder);

    if (FAILED(h_res)) {
        DEBUG_MSG("ERROR: Can't get Direct3D decoder service");
        goto close;
    }

    return 0;

close:
    return 1;
}

static void dxva_fill_picture_entry(DXVA_PicEntry_H264 *pic, unsigned index, unsigned flag)
{
    pic->bPicEntry = index | (flag << 7);
}

static int dxva_fill_picture_parameters()
{
    struct h264_slice_header_t* header = LINKED_HASH_GET_HEAD(app.h264.headers);

    app.dxva.pic_params.wFrameWidthInMbsMinus1 = app.h264.sps.pic_width_in_mbs_minus1;
    app.dxva.pic_params.wFrameHeightInMbsMinus1 = app.h264.sps.pic_height_in_map_units_minus1;
    //TODO: if should have index of picture_parameters
    app.dxva.pic_params.CurrPic.Index7Bits = 0;
    app.dxva.pic_params.CurrPic.AssociatedFlag
        = app.h264.pps.bottom_field_pic_order_in_frame_present_flag;
        //= header->field_pic_flag && header->bottom_field_flag;
    DEBUG_MSG("bottom_field_flag: %d", app.h264.pps.bottom_field_pic_order_in_frame_present_flag);

    app.dxva.pic_params.num_ref_frames = app.h264.sps.num_ref_frames;
    DEBUG_MSG("num_ref_frames: %d", app.h264.sps.num_ref_frames);

    app.dxva.pic_params.field_pic_flag = header->field_pic_flag;
    DEBUG_MSG("field_pic_flag: %d", header->field_pic_flag);
    
    app.dxva.pic_params.MbaffFrameFlag = header->MbaffFrameFlag;
    DEBUG_MSG("MbaffFrameFlag: %d", header->MbaffFrameFlag);

    app.dxva.pic_params.residual_colour_transform_flag = app.h264.sps.separate_colour_plane_flag;
    DEBUG_MSG("residual_colour_transform_flag: %d", app.h264.sps.separate_colour_plane_flag);

    app.dxva.pic_params.sp_for_switch_flag = header->sp_for_switch_flag;
    DEBUG_MSG("sp_for_switch_flag: %d", header->sp_for_switch_flag);

    app.dxva.pic_params.chroma_format_idc = app.h264.sps.chroma_format_idc;
    DEBUG_MSG("chroma_format_idc: %d", app.h264.sps.chroma_format_idc);

    app.dxva.pic_params.RefPicFlag = app.h264.nal_ref_idc > 0? 1: 0;
    DEBUG_MSG("RefPicFlag: %d", app.h264.nal_ref_idc > 0? 1: 0);

    app.dxva.pic_params.constrained_intra_pred_flag = app.h264.pps.constrained_intra_pred_flag;
    DEBUG_MSG("constrained_intra_pred_flag: %d", app.h264.pps.constrained_intra_pred_flag);

    app.dxva.pic_params.weighted_pred_flag = app.h264.pps.weighted_pred_flag;
    DEBUG_MSG("weighted_pred_flag: %d", app.h264.pps.weighted_pred_flag);

    app.dxva.pic_params.weighted_bipred_idc = app.h264.pps.weighted_bipred_idc;
    DEBUG_MSG("weighted_bipred_idc: %d", app.h264.pps.weighted_bipred_idc);

    app.dxva.pic_params.MbsConsecutiveFlag = 1;
    app.dxva.pic_params.frame_mbs_only_flag = app.h264.sps.frame_mbs_only_flag;
    DEBUG_MSG("frame_mbs_only_flag: %d", app.h264.sps.frame_mbs_only_flag);

    app.dxva.pic_params.transform_8x8_mode_flag = app.h264.pps.transform_8x8_mode_flag;
    DEBUG_MSG("transform_8x8_mode_flag: %d", app.h264.pps.transform_8x8_mode_flag);

    app.dxva.pic_params.MinLumaBipredSize8x8Flag = app.h264.sps.level_idc >= 31;
    DEBUG_MSG("MinLumaBipredSize8x8Flag: %d", app.h264.sps.level_idc >= 31);

    // Specifies whether all macroblocks in the current picture have intra prediction modes. 
    app.dxva.pic_params.IntraPicFlag = header->slice_type == SliceTypeI;
    DEBUG_MSG("IntraPicFlag: %d", header->slice_type == SliceTypeI);

    app.dxva.pic_params.bit_depth_luma_minus8 = app.h264.sps.bit_depth_luma_minus8;
    DEBUG_MSG("bit_depth_luma_minus8: %d", app.h264.sps.bit_depth_luma_minus8);

    app.dxva.pic_params.bit_depth_chroma_minus8 = app.h264.sps.bit_depth_chroma_minus8;
    DEBUG_MSG("bit_depth_chroma_minus8: %d", app.h264.sps.bit_depth_chroma_minus8);

    app.dxva.pic_params.StatusReportFeedbackNumber = app.dxva.status_report++;
    DEBUG_MSG("StatusReportFeedbackNumber: %d", app.dxva.status_report - 1);

    for(int i = 0; i < 16; i++) {
        app.dxva.pic_params.RefFrameList[i].bPicEntry = 0xff;
        app.dxva.pic_params.FieldOrderCntList[i][0]
            = app.dxva.pic_params.FieldOrderCntList[i][1] = 0;
        app.dxva.pic_params.FrameNumList[i] = 0;
    }

    //TODO: fill all references
    int index = 0;
    if (header->long_term_reference_flag) {
        app.dxva.pic_params.FrameNumList[index] = header->LongTermFrameIdx;
    }
    else {
        app.dxva.pic_params.FrameNumList[index] = header->frame_num;
    }
    DEBUG_MSG("long_term_reference_flag: %d", header->long_term_reference_flag);
    DEBUG_MSG("LongTermFrameIdx: %d", header->LongTermFrameIdx);
    DEBUG_MSG("frame_num: %d", header->frame_num);
    // Contains two 1-bit flags for each entry in RefFrameList. For the ith entry in RefFrameList,
    // the two flags are accessed as follows:
    app.dxva.pic_params.UsedForReferenceFlags |= 1 << (2 * index);
    app.dxva.pic_params.UsedForReferenceFlags |= 1 << (2 * index + 1);
    app.dxva.pic_params.FieldOrderCntList[index][0]
        = app.dxva.pic_params.FieldOrderCntList[index][1]
        = !header->bottom_field_flag;
    app.dxva.pic_params.RefFrameList[index].Index7Bits = 1;
    app.dxva.pic_params.RefFrameList[index].AssociatedFlag = 0;

    DEBUG_MSG("header->bottom_field_flag: %d", header->bottom_field_flag);

    
    
    /*for(auto it = m_dqPoc.begin(); it != m_dqPoc.end() && iIndex < 16; iIndex++, ++it){
        m_H264PictureParams.FrameNumList[iIndex] = it->usFrameList; //done

        m_H264PictureParams.RefFrameList[iIndex].Index7Bits = it->bRefFrameList;
        m_H264PictureParams.RefFrameList[iIndex].AssociatedFlag = 0;
        m_H264PictureParams.FieldOrderCntList[iIndex][0]
            = m_H264PictureParams.FieldOrderCntList[iIndex][1]
            = it->TopFieldOrderCnt;

        // Contains two 1-bit flags for each entry in RefFrameList. For the ith entry in RefFrameList,
        // the two flags are accessed as follows:
        m_H264PictureParams.UsedForReferenceFlags |= 1 << (2 * iIndex);
        m_H264PictureParams.UsedForReferenceFlags |= 1 << (2 * iIndex + 1);
    }*/

    app.dxva.pic_params.pic_init_qp_minus26 = app.h264.pps.pic_init_qp_minus26;
    DEBUG_MSG("pic_init_qp_minus26: %d", app.h264.pps.pic_init_qp_minus26);
    app.dxva.pic_params.chroma_qp_index_offset = app.h264.pps.chroma_qp_index_offset;
    DEBUG_MSG("chroma_qp_index_offset: %d", app.h264.pps.chroma_qp_index_offset);
    app.dxva.pic_params.second_chroma_qp_index_offset = app.h264.pps.second_chroma_qp_index_offset;
    DEBUG_MSG("second_chroma_qp_index_offset: %d", app.h264.pps.second_chroma_qp_index_offset);
    app.dxva.pic_params.ContinuationFlag = 1; // copied
    app.dxva.pic_params.pic_init_qs_minus26 = app.h264.pps.pic_init_qs_minus26;
    DEBUG_MSG("pic_init_qs_minus26: %d", app.h264.pps.pic_init_qs_minus26);
    app.dxva.pic_params.num_ref_idx_l0_active_minus1 = app.h264.pps.ref_count[0] - 1;
    app.dxva.pic_params.num_ref_idx_l1_active_minus1 = app.h264.pps.ref_count[1] - 1;
    DEBUG_MSG("num_ref_idx_l0_active_minus1 and num_ref_idx_l1_active_minus1: %d %d",
        app.h264.pps.ref_count[0] - 1, app.h264.pps.ref_count[1] - 1);

    // If Flagi is 1, frame number i is marked as "non-existing," as defined by the  H.264/AVC
    // specification. (Otherwise, if the flag is 0, the frame is not marked as "non-existing.") 
    app.dxva.pic_params.NonExistingFrameFlags = 0;

    app.dxva.pic_params.frame_num = header->frame_num;
    DEBUG_MSG("frame_num: %d", header->frame_num);

    app.dxva.pic_params.log2_max_frame_num_minus4 = app.h264.sps.log2_max_frame_num - 4;
    DEBUG_MSG("log2_max_frame_num_minus4: %d", app.h264.sps.log2_max_frame_num - 4);

    app.dxva.pic_params.pic_order_cnt_type = app.h264.sps.pic_order_cnt_type;
    DEBUG_MSG("pic_order_cnt_type: %d", app.h264.sps.pic_order_cnt_type);

    app.dxva.pic_params.log2_max_pic_order_cnt_lsb_minus4
        = app.h264.sps.log2_max_pic_order_cnt_lsb_minus4;
    DEBUG_MSG("log2_max_pic_order_cnt_lsb_minus4: %d", app.h264.sps.log2_max_pic_order_cnt_lsb_minus4);

    //app.dxva.pic_params.delta_pic_order_always_zero_flag = 0;

    app.dxva.pic_params.direct_8x8_inference_flag = app.h264.sps.direct_8x8_inference_flag;
    DEBUG_MSG("direct_8x8_inference_flag: %d", app.h264.sps.direct_8x8_inference_flag);

    app.dxva.pic_params.entropy_coding_mode_flag = app.h264.pps.entropy_coding_mode_flag;
    DEBUG_MSG("entropy_coding_mode_flag: %d", app.h264.pps.entropy_coding_mode_flag);

    app.dxva.pic_params.pic_order_present_flag
        = app.h264.pps.bottom_field_pic_order_in_frame_present_flag;
    DEBUG_MSG("pic_order_present_flag: %d",
        app.h264.pps.bottom_field_pic_order_in_frame_present_flag);

    app.dxva.pic_params.num_slice_groups_minus1 = app.h264.pps.num_slice_groups_minus1;
    DEBUG_MSG("num_slice_groups_minus1: %d", app.h264.pps.num_slice_groups_minus1);
    //app.dxva.pic_params.slice_group_map_type = 0;

    app.dxva.pic_params.deblocking_filter_control_present_flag
        = app.h264.pps.deblocking_filter_control_present_flag;
    DEBUG_MSG("deblocking_filter_control_present_flag: %d",
        app.h264.pps.deblocking_filter_control_present_flag);

    app.dxva.pic_params.redundant_pic_cnt_present_flag
        = app.h264.pps.redundant_pic_cnt_present_flag;
    DEBUG_MSG("redundant_pic_cnt_present_flag: %d", app.h264.pps.redundant_pic_cnt_present_flag);

    //app.dxva.pic_params.Reserved8BitsA = 0;
    //m_H264PictureParams.Reserved8BitsB = 0;

    app.dxva.pic_params.slice_group_change_rate_minus1 = app.h264.pps.num_slice_groups_minus1;
    DEBUG_MSG("slice_group_change_rate_minus1: %d", app.h264.pps.num_slice_groups_minus1);

    // TODO: to find
    //app.dxva.pic_params.CurrFieldOrderCnt[0] = 0;
    //app.dxva.pic_params.CurrFieldOrderCnt[1] = 0;

    return 0;
}

static int dxva_fill_matrices()
{
    memset(&app.dxva.matrices, 16, sizeof(DXVA_Qmatrix_H264));

    // int i = 0;
    // do {
    //     memcpy(&app.dxva.matrices.bScalingLists4x4[i++], default_scaling4[0], sizeof(*default_scaling4[0]));
    //     memcpy(&app.dxva.matrices.bScalingLists4x4[i++], default_scaling4[1], sizeof(*default_scaling4[0]));
    // } while (i < 6);

    // memcpy(&app.dxva.matrices.bScalingLists8x8[0], default_scaling8[0], sizeof(*default_scaling8[0]));
    // memcpy(&app.dxva.matrices.bScalingLists8x8[1], default_scaling8[1], sizeof(*default_scaling8[0]));

    // check size
    // DEBUG_MSG("dxva_fill_matrices: 4: first: %d, last: %d, size %lld",
    //     app.dxva.matrices.bScalingLists4x4[0][0],
    //     app.dxva.matrices.bScalingLists4x4[0][15],
    //     sizeof(*default_scaling4[0]));
    // DEBUG_MSG("dxva_fill_matrices: 4: first: %d, last: %d, size %lld",
    //     app.dxva.matrices.bScalingLists4x4[1][0],
    //     app.dxva.matrices.bScalingLists4x4[1][15],
    //     sizeof(*default_scaling4[0]));
    // DEBUG_MSG("dxva_fill_matrices: 8: first: %d, last: %d, size %lld",
    //     app.dxva.matrices.bScalingLists8x8[0][0],
    //     app.dxva.matrices.bScalingLists8x8[1][0],
    //     sizeof(*default_scaling8[0]));
    // DEBUG_MSG("dxva_fill_matrices: 8: first: %d, last: %d, size %lld",
    //     app.dxva.matrices.bScalingLists8x8[1][0],
    //     app.dxva.matrices.bScalingLists8x8[1][0],
    //     sizeof(*default_scaling8[0]));

    return 0;
}

static int dxva_fill_slice_long(int start, int end)
{
    struct h264_slice_header_t* header = LINKED_HASH_GET_HEAD(app.h264.headers);

    app.dxva.slice_long.slice_type = header->slice_type_origin;
    app.dxva.slice_long.num_ref_idx_l0_active_minus1 = header->ref_count[0] - 1;
    app.dxva.slice_long.num_ref_idx_l1_active_minus1 = header->ref_count[1] - 1;
    app.dxva.slice_long.slice_alpha_c0_offset_div2 = header->slice_alpha_c0_offset_div2;
    app.dxva.slice_long.slice_beta_offset_div2 = header->slice_beta_offset_div2;
    app.dxva.slice_long.luma_log2_weight_denom = header->luma_log2_weight_denom;
    app.dxva.slice_long.chroma_log2_weight_denom = header->chroma_log2_weight_denom;
    app.dxva.slice_long.first_mb_in_slice = header->first_mb_in_slice;
    app.dxva.slice_long.slice_qs_delta = header->slice_qs_delta;
    app.dxva.slice_long.slice_qp_delta = header->slice_qp_delta;
    app.dxva.slice_long.redundant_pic_cnt = header->redundant_pic_cnt;
    app.dxva.slice_long.direct_spatial_mv_pred_flag = header->direct_spatial_mv_pred_flag;
    app.dxva.slice_long.cabac_init_idc = header->cabac_init_idc;
    app.dxva.slice_long.disable_deblocking_filter_idc = header->disable_deblocking_filter_idc;

    app.dxva.slice_long.NumMbsForSlice = header->PicSizeInMbs - header->first_mb_in_slice;
    //TODO: RefPicList, DXVA_PicEntry_H264 RefPicList[2][32]; - DXVA_PicEntry_H264 - char
    for (int list = 0; list < 2; list++) {
        for (int ref = 0; ref < ARRAY_SIZE(app.dxva.slice_long.RefPicList[0]); ref++) {
            if (list < header->list_count && ref < header->ref_count[list]) {
                DEBUG_MSG("ERROR: TO IMPLEMENT header->list_count %d, "
                    "%s:%d - %s",
                    header->list_count, __FILE__, __LINE__, __FUNCTION__);

                // const H264Picture *r = sl->ref_list[list][i].parent;
                // unsigned plane;
                // unsigned index;
                // if (DXVA_CONTEXT_WORKAROUND(avctx, ctx) & FF_DXVA2_WORKAROUND_INTEL_CLEARVIDEO)
                //     index = ff_dxva2_get_surface_index(avctx, ctx, r->f);
                // else
                //     index = get_refpic_index(pp, ff_dxva2_get_surface_index(avctx, ctx, r->f));
                // fill_picture_entry(&slice->RefPicList[list][i], index,
                //                    sl->ref_list[list][i].reference == PICT_BOTTOM_FIELD);

                //app.dxva.slice_long.RefPicList[list][ref].bPicEntry = index | (flag << 7);

                struct h264_weight_t* weight = header->weights[list] + ref;
                for (unsigned plane = 0; plane < 3; plane++) {
                    int w, o;
                    if (plane == 0 && weight->luma_weight_flag) {
                        w = weight->luma_weight;
                        o = weight->luma_offset;
                    } else if (plane >= 1 && weight->chroma_weight_flag) {
                        w = weight->chroma_weight[plane-1];
                        o = weight->chroma_offset[plane-1];
                    } else {
                        w = 1 << (
                            plane == 0?
                            header->luma_log2_weight_denom:
                            header->chroma_log2_weight_denom
                        );
                        o = 0;
                    }
                    app.dxva.slice_long.Weights[list][ref][plane][0] = w;
                    app.dxva.slice_long.Weights[list][ref][plane][1] = o;
                }
            } else {
                app.dxva.slice_long.RefPicList[list][ref].bPicEntry = 0xff;
                for (unsigned plane = 0; plane < 3; plane++) {
                    app.dxva.slice_long.Weights[list][ref][plane][0] = 0;
                    app.dxva.slice_long.Weights[list][ref][plane][1] = 0;
                }
            }
        }
    }

    app.dxva.slice_long.BSNALunitDataLocation = start;
    app.dxva.slice_long.SliceBytesInBuffer = end - start;
    app.dxva.slice_long.wBadSliceChopping = 0;
    app.dxva.slice_long.BitOffsetToSliceData = 0;
    app.dxva.slice_long.slice_id = app.dxva.slice_id++;

    return 0;
}

static int dxva_fill_slice_short(int start, int end)
{
    app.dxva.slice_short.BSNALunitDataLocation = 0;
    app.dxva.slice_short.SliceBytesInBuffer = end - start - 1;
    app.dxva.slice_short.wBadSliceChopping = 0;
    return 0;
}

static int dxva_commit_buffer(unsigned type,
    DXVA2_DecodeBufferDesc* buffer,
    const void *data,
    unsigned size)
{
    int ret = 1;
    HRESULT hr;

    void* dxva_data;
    unsigned dxva_size;
    hr = IDirectXVideoDecoder_GetBuffer(app.dxva.decoder, type, &dxva_data, &dxva_size);
    if (FAILED(hr)) {
        DEBUG_MSG("ERROR: dxva_commit_buffer(type: %d) failed to get dxva buffer, error %s(%lx)",
            type,
            convert_hresult_error(hr),
            hr);
        
        goto close;
    }

    if (size <= dxva_size) {
        memcpy(dxva_data, data, size);
        ret = 0;
    } else {
        DEBUG_MSG("ERROR: dxva_commit_buffer(type: %d) failed, buffer to commit is too big", type);
        goto release;
    }

release:
    hr = IDirectXVideoDecoder_ReleaseBuffer(app.dxva.decoder, type);
    if (FAILED(hr)) {
        DEBUG_MSG("ERROR: dxva_commit_buffer(type: %d) failed to release dxva buffer, error %s(%lx)",
            type,
            convert_hresult_error(hr),
            hr);
    }

    memset(buffer, 0, sizeof(*buffer));
    buffer->CompressedBufferType = type;
    buffer->DataSize = size;

close:
    return ret;
}

static int dxva_commit_slice(DXVA2_DecodeBufferDesc* buffer, int start, int end) {
    struct h264_slice_header_t* header = LINKED_HASH_GET_HEAD(app.h264.headers);

    int ret = 1;
    HRESULT hr;

    void* dxva_data;
    unsigned dxva_size;
    hr = IDirectXVideoDecoder_GetBuffer(app.dxva.decoder, DXVA2_BitStreamDateBufferType, &dxva_data, &dxva_size);
    if (FAILED(hr)) {
        DEBUG_MSG("ERROR: dxva_commit_slice failed to get dxva buffer(type: %d), error %s(%lx)",
            DXVA2_BitStreamDateBufferType,
            convert_hresult_error(hr),
            hr);
        
        goto close;
    }

    int size = end - start + sizeof(start_code) - 4;
    if (size <= dxva_size) {
        uint8_t* position = dxva_data;
        memcpy(position, start_code, sizeof(start_code));
        position += sizeof(start_code);
        memcpy(position, app.enc_buf + start + 4, size - sizeof(start_code));

        buffer->DataSize = size;
        ret = 0;

        DEBUG_MSG("Slice has been copied to dxva, size: %d", buffer->DataSize);
    } else {
        DEBUG_MSG("ERROR: dxva_commit_slice(type: %d) failed, buffer to commit is too big",
            DXVA2_BitStreamDateBufferType);
        goto release_stream;
    }

    buffer->CompressedBufferType = DXVA2_BitStreamDateBufferType;
    buffer->NumMBsInBuffer = header->PicSizeInMbs - header->first_mb_in_slice;
    DEBUG_MSG("dxva, NumMBsInBuffer: %d", buffer->NumMBsInBuffer);

release_stream:
    hr = IDirectXVideoDecoder_ReleaseBuffer(app.dxva.decoder, DXVA2_BitStreamDateBufferType);
    if (FAILED(hr)) {
        DEBUG_MSG("ERROR: dxva_commit_slice(type: %d) failed to release dxva buffer, error %s(%lx)",
            DXVA2_BitStreamDateBufferType,
            convert_hresult_error(hr),
            hr);
    }

close:
    return ret;
}

int dxva_decode(int start, int end) {
    HRESULT hr;
    int ret = 1;

    // typedef struct _DXVA2_DecodeBufferDesc {
    //     DWORD CompressedBufferType;
    //     UINT BufferIndex;
    //     UINT DataOffset;
    //     UINT DataSize;
    //     UINT FirstMBaddress;
    //     UINT NumMBsInBuffer;
    //     UINT Width;
    //     UINT Height;
    //     UINT Stride;
    //     UINT ReservedBits;
    //     PVOID pvPVPState;
    // } DXVA2_DecodeBufferDesc;
    DXVA2_DecodeBufferDesc buffers[4];
    int buffers_size = 0;

    // 1. IDirectXVideoDecoder_BeginFrame
    hr = IDirectXVideoDecoder_BeginFrame(app.dxva.decoder, app.d3d.surfaces[0], NULL);
    if (FAILED(hr)) {
        DEBUG_MSG("ERROR: IDirectXVideoDecoder_BeginFrame failed with error code %lx", hr);
        goto close;
    }

    CALL(dxva_fill_picture_parameters(), end_frame);
    CALL(dxva_commit_buffer(DXVA2_PictureParametersBufferType,
        buffers + buffers_size,
        &app.dxva.pic_params, sizeof(app.dxva.pic_params)
    ), end_frame);
    buffers_size++;

    CALL(dxva_fill_matrices(), end_frame);
    CALL(dxva_commit_buffer(DXVA2_InverseQuantizationMatrixBufferType,
        buffers + buffers_size,
        &app.dxva.matrices, sizeof(app.dxva.matrices)
    ), end_frame);
    buffers_size++;

    CALL(dxva_commit_slice(buffers + buffers_size, start, end), end_frame);
    buffers_size++;

    if (app.dxva.cfg->ConfigBitstreamRaw != 2) {
        DEBUG_MSG("fill long slice");
        CALL(dxva_fill_slice_long(start, end), end_frame);
        CALL(dxva_commit_buffer(DXVA2_SliceControlBufferType,
            buffers + buffers_size,
            &app.dxva.slice_long, sizeof(app.dxva.slice_long)
        ), end_frame);
    }
    else {
        DEBUG_MSG("fill short slice");
        CALL(dxva_fill_slice_short(start, end), end_frame);
        CALL(dxva_commit_buffer(DXVA2_SliceControlBufferType,
            buffers + buffers_size,
            &app.dxva.slice_short, sizeof(app.dxva.slice_short)
        ), end_frame);
    }
    buffers_size++;

    DXVA2_DecodeExecuteParams params = {
        .NumCompBuffers = buffers_size,
        .pCompressedBuffers = buffers,
        .pExtensionData = NULL
    };

    hr = IDirectXVideoDecoder_Execute(app.dxva.decoder, &params);
    if (FAILED(hr)) {
        DEBUG_MSG("ERROR: dxva_decode failed to execute DXVA2, error %s(%lx)",
            convert_hresult_error(hr),
            hr);

        goto end_frame;
    }

    d3d_render_frame();
    ret = 0;

    DEBUG_MSG("IDirectXVideoDecoder_Execute success");

end_frame:
    DEBUG_MSG("IDirectXVideoDecoder_EndFrame crashed");
    hr = IDirectXVideoDecoder_EndFrame(app.dxva.decoder, NULL);
    if (FAILED(hr)) {
        DEBUG_MSG("ERROR: dxva_decode failed to end DXVA2 frame, error %s(%lx)",
            convert_hresult_error(hr),
            hr);
    }
    DEBUG_MSG("dxva_decode success: %d", ret);

close:
    return ret;
}
