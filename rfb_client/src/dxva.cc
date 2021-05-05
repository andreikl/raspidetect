//#include <khash.h>

#include "main.h"

#include "dxva_helpers.cc"

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

void dxva_destroy(struct app_state_t *app)
{
    if (app->dxva.device != NULL) {
        CoTaskMemFree(app->dxva.cfg_list);
        app->dxva.cfg_count = 0;
        app->dxva.cfg_list = NULL;
    }

    if (app->dxva.decoder != NULL) {
        app->dxva.decoder->Release();
        app->dxva.decoder = NULL;
    }
    if (app->dxva.service != NULL) {
        app->dxva.service->Release();
        app->dxva.service = NULL;
    }
    if (app->dxva.device != NULL) {
        D3D_CALL(app->dxva.device_manager->CloseDeviceHandle(app->dxva.device));
    }
    if (app->dxva.device_manager != NULL) {
        app->dxva.device_manager->Release();
        app->dxva.device_manager = NULL;
    }
}

int dxva_init(struct app_state_t *app)
{
    int rslt;

    DXVA2_VideoDesc desc;
    desc.SampleWidth = app->server_width;
    desc.SampleHeight = app->server_height;
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

    unsigned reset_token;
    D3D_CALL(DXVA2CreateDirect3DDeviceManager9(
        &reset_token,
        &app->dxva.device_manager
    ), close);
    D3D_CALL(app->dxva.device_manager->ResetDevice(app->d3d.dev, reset_token), close);
    D3D_CALL(app->dxva.device_manager->OpenDeviceHandle(&app->dxva.device), close);
    D3D_CALL(app->dxva.device_manager->GetVideoService(
        app->dxva.device,
        IID_IDirectXVideoDecoderService,
        (void **)&app->dxva.service
    ), close);

    GENERAL_CALL(rslt = dxva_find_decoder(app));
    if (rslt) {
        dxva_print_guid(H264CODEC);
        goto close;
    }

    D3D_CALL(app->dxva.service->GetDecoderConfigurations(
        H264CODEC,
        &desc,
        NULL,
        &app->dxva.cfg_count,
        &app->dxva.cfg_list
    ), close);

    GENERAL_CALL(rslt = dxva_find_config(app));
    if (rslt) {
        dxva_print_config(app);
        goto close;
    }

    D3D_CALL(app->dxva.service->CreateSurface(
        app->server_width,
        app->server_height, 
        SCREEN_BUFFERS - 1, 
        H264CODEC_FORMAT, 
        D3DPOOL_DEFAULT, 
        0,
        DXVA2_VideoDecoderRenderTarget,
        app->d3d.surfaces,
        NULL
    ), close);

    D3D_CALL(app->dxva.service->CreateVideoDecoder(
        H264CODEC,
        &desc,
        app->dxva.cfg,
        app->d3d.surfaces,
        SCREEN_BUFFERS,
        &app->dxva.decoder
    ), close);

    return 0;

close:
    return -1;
}

static void dxva_fill_picture_entry(DXVA_PicEntry_H264 *pic, unsigned index, unsigned flag)
{
    pic->bPicEntry = index | (flag << 7);
}

static int dxva_fill_picture_parameters(struct app_state_t *app)
{
    struct h264_slice_header_t* header = LINKED_HASH_GET_HEAD(app->h264.headers);

    app->dxva.pic_params.wFrameWidthInMbsMinus1 = app->h264.sps.pic_width_in_mbs_minus1;
    app->dxva.pic_params.wFrameHeightInMbsMinus1 = app->h264.sps.pic_height_in_map_units_minus1;
    //TODO: to check, we are using one surface so index is 0
    dxva_fill_picture_entry(
        &app->dxva.pic_params.CurrPic,
        0,
        header->field_pic_flag && header->bottom_field_flag
    );

    app->dxva.pic_params.num_ref_frames = app->h264.sps.num_ref_frames;
    app->dxva.pic_params.field_pic_flag = header->field_pic_flag;
    app->dxva.pic_params.MbaffFrameFlag = header->MbaffFrameFlag;
    app->dxva.pic_params.residual_colour_transform_flag = 0;
    app->dxva.pic_params.sp_for_switch_flag = header->sp_for_switch_flag;
    app->dxva.pic_params.chroma_format_idc = app->h264.sps.chroma_format_idc;
    app->dxva.pic_params.RefPicFlag = app->h264.nal_ref_idc > 0? 1: 0;
    app->dxva.pic_params.constrained_intra_pred_flag = app->h264.pps.constrained_intra_pred_flag;
    app->dxva.pic_params.weighted_pred_flag = app->h264.pps.weighted_pred_flag;
    app->dxva.pic_params.weighted_bipred_idc = app->h264.pps.weighted_bipred_idc;
    app->dxva.pic_params.MbsConsecutiveFlag = 1;
    app->dxva.pic_params.frame_mbs_only_flag = app->h264.sps.frame_mbs_only_flag;
    app->dxva.pic_params.transform_8x8_mode_flag = app->h264.pps.transform_8x8_mode_flag;
    app->dxva.pic_params.MinLumaBipredSize8x8Flag = 0;
    // Specifies whether all macroblocks in the current picture have intra prediction modes. 
    app->dxva.pic_params.IntraPicFlag = 0;
    app->dxva.pic_params.bit_depth_luma_minus8 = app->h264.sps.bit_depth_luma_minus8;
    app->dxva.pic_params.bit_depth_chroma_minus8 = app->h264.sps.bit_depth_chroma_minus8;
    app->dxva.pic_params.StatusReportFeedbackNumber = ++app->dxva.status_report;
    // TODO: to check
    dxva_fill_picture_entry(&app->dxva.pic_params.RefFrameList[0], 0, 1);
    // TODO: to find
    //app->dxva.pic_params.CurrFieldOrderCnt[0] = 0;
    //app->dxva.pic_params.CurrFieldOrderCnt[1] = 0;
    // TODO: to find
    //app->dxva.pic_params.FieldOrderCntList = 0;
    app->dxva.pic_params.pic_init_qp_minus26 = app->h264.pps.pic_init_qp_minus26;
    app->dxva.pic_params.chroma_qp_index_offset = app->h264.pps.chroma_qp_index_offset;
    app->dxva.pic_params.second_chroma_qp_index_offset = app->h264.pps.second_chroma_qp_index_offset;
    app->dxva.pic_params.ContinuationFlag = 0; // truncate for now
    app->dxva.pic_params.pic_init_qs_minus26 = app->h264.pps.pic_init_qs_minus26;
    app->dxva.pic_params.num_ref_idx_l0_active_minus1 = app->h264.pps.ref_count[0] - 1;
    app->dxva.pic_params.num_ref_idx_l1_active_minus1 = app->h264.pps.ref_count[1] - 1;
    // TODO: to find
    //app->dxva.pic_params.Reserved8BitsA 
    // TODO: to find
    //app->dxva.pic_params.FrameNumList
    // TODO: to find
    // Contains two 1-bit flags for each entry in RefFrameList. For the ith entry in RefFrameList,
    // the two flags are accessed as follows:
    //app->dxva.pic_params.UsedForReferenceFlags
    // If Flagi is 1, frame number i is marked as "non-existing," as defined by the  H.264/AVC
    // specification. (Otherwise, if the flag is 0, the frame is not marked as "non-existing.") 
    app->dxva.pic_params.NonExistingFrameFlags = 0;
    app->dxva.pic_params.frame_num = header->frame_num;
    return 0;
}

static int dxva_fill_matrices(struct app_state_t *app)
{
    int i = 0;
    do {
        memcpy(&app->dxva.matrices.bScalingLists4x4[i++], default_scaling4[0], sizeof(*default_scaling4[0]));
        memcpy(&app->dxva.matrices.bScalingLists4x4[i++], default_scaling4[1], sizeof(*default_scaling4[0]));
    } while (i < 6);

    memcpy(&app->dxva.matrices.bScalingLists8x8[0], default_scaling8[0], sizeof(*default_scaling8[0]));
    memcpy(&app->dxva.matrices.bScalingLists8x8[1], default_scaling8[1], sizeof(*default_scaling8[0]));

    // check size
    // fprintf(stderr, "dxva_fill_matrices: 4: first: %d, last: %d, size %lld\n",
    //     app->dxva.matrices.bScalingLists4x4[0][0],
    //     app->dxva.matrices.bScalingLists4x4[0][15],
    //     sizeof(*default_scaling4[0]));
    // fprintf(stderr, "dxva_fill_matrices: 4: first: %d, last: %d, size %lld\n",
    //     app->dxva.matrices.bScalingLists4x4[1][0],
    //     app->dxva.matrices.bScalingLists4x4[1][15],
    //     sizeof(*default_scaling4[0]));
    // fprintf(stderr, "dxva_fill_matrices: 8: first: %d, last: %d, size %lld\n",
    //     app->dxva.matrices.bScalingLists8x8[0][0],
    //     app->dxva.matrices.bScalingLists8x8[1][0],
    //     sizeof(*default_scaling8[0]));
    // fprintf(stderr, "dxva_fill_matrices: 8: first: %d, last: %d, size %lld\n",
    //     app->dxva.matrices.bScalingLists8x8[1][0],
    //     app->dxva.matrices.bScalingLists8x8[1][0],
    //     sizeof(*default_scaling8[0]));

    return 0;
}

static int dxva_fill_slice(struct app_state_t *app, uint8_t *buffer)
{
    struct h264_slice_header_t* header = LINKED_HASH_GET_HEAD(app->h264.headers);

    app->dxva.slice.BSNALunitDataLocation = buffer - app->enc_buf;
    GENERAL_DEBUG(app->dxva.slice.BSNALunitDataLocation);
    //TODO: calculate length without 000x
    app->dxva.slice.SliceBytesInBuffer = app->enc_buf_length - 4;
    GENERAL_DEBUG(app->dxva.slice.SliceBytesInBuffer);
    app->dxva.slice.wBadSliceChopping = 0;
    GENERAL_DEBUG(app->dxva.slice.wBadSliceChopping);
    app->dxva.slice.first_mb_in_slice = header->first_mb_in_slice;
    GENERAL_DEBUG(app->dxva.slice.first_mb_in_slice);
    //...
    //TODO: should be 0 for first slice
    app->dxva.slice.NumMbsForSlice = header->PicSizeInMbs - header->first_mb_in_slice;
    GENERAL_DEBUG(app->dxva.slice.NumMbsForSlice);
    //TODO: replace to right calculation
    app->dxva.slice.BitOffsetToSliceData = 24;
    GENERAL_DEBUG(app->dxva.slice.BitOffsetToSliceData);
    app->dxva.slice.slice_type = header->slice_type_origin;
    GENERAL_DEBUG(app->dxva.slice.slice_type);
    app->dxva.slice.luma_log2_weight_denom = header->luma_log2_weight_denom;
    GENERAL_DEBUG(app->dxva.slice.luma_log2_weight_denom);
    app->dxva.slice.chroma_log2_weight_denom = header->chroma_log2_weight_denom;
    GENERAL_DEBUG(app->dxva.slice.chroma_log2_weight_denom);
    app->dxva.slice.num_ref_idx_l0_active_minus1 = header->ref_count[0] - 1;
    GENERAL_DEBUG(app->dxva.slice.num_ref_idx_l0_active_minus1);
    app->dxva.slice.num_ref_idx_l1_active_minus1 = header->ref_count[1] - 1;
    GENERAL_DEBUG(app->dxva.slice.num_ref_idx_l1_active_minus1);
    app->dxva.slice.slice_alpha_c0_offset_div2 = header->slice_alpha_c0_offset_div2;
    GENERAL_DEBUG(app->dxva.slice.slice_alpha_c0_offset_div2);
    app->dxva.slice.slice_beta_offset_div2 = header->slice_beta_offset_div2;
    GENERAL_DEBUG(app->dxva.slice.slice_beta_offset_div2);
    GENERAL_DEBUG(app->dxva.slice.Reserved8Bits);
    app->dxva.slice.slice_qs_delta = header->slice_qs_delta;
    GENERAL_DEBUG(app->dxva.slice.slice_qs_delta);
    app->dxva.slice.slice_qp_delta = header->slice_qp_delta;
    GENERAL_DEBUG(app->dxva.slice.slice_qp_delta);
    app->dxva.slice.redundant_pic_cnt = header->redundant_pic_cnt;
    GENERAL_DEBUG(app->dxva.slice.redundant_pic_cnt);
    app->dxva.slice.direct_spatial_mv_pred_flag = header->direct_spatial_mv_pred_flag;
    GENERAL_DEBUG(app->dxva.slice.direct_spatial_mv_pred_flag);
    app->dxva.slice.cabac_init_idc = header->cabac_init_idc;
    GENERAL_DEBUG(app->dxva.slice.cabac_init_idc);
    app->dxva.slice.disable_deblocking_filter_idc = header->disable_deblocking_filter_idc;
    GENERAL_DEBUG(app->dxva.slice.disable_deblocking_filter_idc);
    app->dxva.slice.slice_id = app->dxva.slice_id++;
    GENERAL_DEBUG(app->dxva.slice_id);

    //TODO: RefPicList, DXVA_PicEntry_H264 RefPicList[2][32]; - DXVA_PicEntry_H264 - char
    for (unsigned list = 0; list < 2; list++) {
        for (unsigned ref = 0; ref < ARRAY_SIZE(app->dxva.slice.RefPicList[0]); ref++) {
            if (list < header->list_count && ref < header->ref_count[list]) {
                fprintf(stderr, "ERROR: TO IMPLEMENT header->list_count %d, "
                    "%s:%d - %s\n",
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

                //app->dxva.slice.RefPicList[list][ref].bPicEntry = index | (flag << 7);

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
                    app->dxva.slice.Weights[list][ref][plane][0] = w;
                    app->dxva.slice.Weights[list][ref][plane][1] = o;
                }
            } else {
                app->dxva.slice.RefPicList[list][ref].bPicEntry = 0xff;
                for (unsigned plane = 0; plane < 3; plane++) {
                    app->dxva.slice.Weights[list][ref][plane][0] = 0;
                    app->dxva.slice.Weights[list][ref][plane][1] = 0;
                }
            }
        }
    }
    return 0;
}

static int dxva_commit_buffer(struct app_state_t *app,
    unsigned type,
    DXVA2_DecodeBufferDesc* buffer,
    const void *data,
    unsigned size)
{
    int res = -1;

    void* dxva_data;
    unsigned dxva_size;

    D3D_CALL(app->dxva.decoder->GetBuffer(type, &dxva_data, &dxva_size), close);
    if (size <= dxva_size) {
        memcpy(dxva_data, data, size);
        res = 0;
    } else {
        fprintf(
            stderr,
            "ERROR: dxva_commit_buffer(type: %d) failed, buffer to commit is too big\n",
            type
        );
    }

    D3D_CALL(app->dxva.decoder->ReleaseBuffer(type));

    memset(buffer, 0, sizeof(*buffer));
    buffer->CompressedBufferType = type;
    buffer->DataSize = size;

close:
    return res;
}

static int dxva_commit_slice(
    struct app_state_t *app,
    DXVA2_DecodeBufferDesc* buffer
) {
    static const uint8_t start_code[] = { 0, 0, 1 };

    int res = -1;

    void* dxva_data;
    unsigned dxva_size;
    D3D_CALL(
        app->dxva.decoder->GetBuffer(DXVA2_BitStreamDateBufferType, &dxva_data, &dxva_size),
        close
    );

    if (sizeof(start_code) + app->enc_buf_length - 4 <= dxva_size) {
        uint8_t* position = (uint8_t*)dxva_data;
        memcpy(position, start_code, sizeof(start_code));
        position += sizeof(start_code);
        memcpy(position, app->enc_buf + 4, app->enc_buf_length - 4);
        position += app->enc_buf_length - 4;

        app->dxva.slice.SliceBytesInBuffer = sizeof(start_code) + app->enc_buf_length - 4;
        res = 0;
    } else {
        fprintf(stderr, "ERROR: dxva_commit_slice(type: %d) failed, buffer to commit is too big\n",
            DXVA2_BitStreamDateBufferType);
        goto release_stream;
    }

    buffer->CompressedBufferType = DXVA2_BitStreamDateBufferType;
    buffer->DataSize = app->dxva.slice.SliceBytesInBuffer;
    buffer->NumMBsInBuffer = app->dxva.slice.NumMbsForSlice;

release_stream:
    D3D_CALL(app->dxva.decoder->ReleaseBuffer(DXVA2_BitStreamDateBufferType));

close:
    return res;
}

int dxva_decode(struct app_state_t *app) {
    int res = -1;

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
    DXVA2_DecodeExecuteParams params;
    DXVA2_DecodeBufferDesc buffers[4];
    unsigned buffers_size = 0;

    D3D_CALL(app->dxva.decoder->BeginFrame(app->d3d.surfaces[0], NULL), close);
    GENERAL_CALL(dxva_fill_picture_parameters(app), end_frame);
    GENERAL_CALL(dxva_commit_buffer(
        app,
        DXVA2_PictureParametersBufferType,
        buffers + buffers_size,
        &app->dxva.pic_params, sizeof(app->dxva.pic_params)
    ), end_frame);
    buffers_size++;

    GENERAL_CALL(dxva_fill_matrices(app), end_frame);
    GENERAL_CALL(dxva_commit_buffer(
        app,
        DXVA2_InverseQuantizationMatrixBufferType,
        buffers + buffers_size,
        &app->dxva.matrices, sizeof(app->dxva.matrices)
    ), end_frame);
    buffers_size++;

    GENERAL_CALL(dxva_commit_slice(app, buffers + buffers_size), end_frame);
    buffers_size++;

    GENERAL_CALL(dxva_fill_slice(app, app->enc_buf), end_frame);
    GENERAL_CALL(dxva_commit_buffer(
        app,
        DXVA2_SliceControlBufferType,
        buffers + buffers_size,
        &app->dxva.slice, sizeof(app->dxva.slice)
    ), end_frame);
    buffers_size++;

    params = {
        .NumCompBuffers = buffers_size,
        .pCompressedBuffers = buffers,
        .pExtensionData = NULL
    };
    D3D_CALL(app->dxva.decoder->Execute(&params), end_frame);

    if (app->verbose) {
        fprintf(stderr, "decoding operation has completed\n");
    }
    res = 0;

end_frame:
    D3D_CALL(app->dxva.decoder->EndFrame(NULL));

close:
    return res;
}
