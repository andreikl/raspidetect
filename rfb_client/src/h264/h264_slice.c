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

#include "h264_fmo.c"

static int h264_init_slice_data()
{
    DEBUG_MSG("INFO: app.h264.data.PicSizeInMbs(%d)\n", app.h264.header.PicSizeInMbs);

    int SubWidthC = -1;
    int SubHeightC = -1;
    CALL(
        h264_get_chroma_variables(&SubWidthC, &SubHeightC),
        error
    );
    int NumC8x8 = 4 / (SubWidthC * SubHeightC);

    if (app.h264.data.macroblocks) {
        for (int i = 0; i < app.h264.header.PicSizeInMbs; i++) {
            struct h264_macroblock_t* mb = app.h264.data.macroblocks + i;
            free(mb->Intra16x16DCLevel.coeff);
            for (int j = 0; j < ARRAY_SIZE(mb->Intra16x16ACLevel); j++) {
                free(mb->Intra16x16ACLevel[j].coeff);
            }
            for (int j = 0; j < ARRAY_SIZE(mb->LumaLevel4x4); j++) {
                free(mb->LumaLevel4x4[j].coeff);
            }
            for (int j = 0; j < ARRAY_SIZE(mb->LumaLevel8x8); j++) {
                free(mb->LumaLevel8x8[j].coeff);
            }
            for (int j = 0; j < ARRAY_SIZE(mb->ChromaDCLevel); j++) {
                free(mb->ChromaDCLevel[j].coeff);
            } 
            for (int j = 0; j < ARRAY_SIZE(mb->ChromaACLevel); j++) {
                for (int k = 0; k < ARRAY_SIZE(*mb->ChromaACLevel); k++) {
                    free(mb->ChromaACLevel[j][k].coeff);                    
                }
            }                                                   
        }
        free(app.h264.data.macroblocks);
        app.h264.data.macroblocks = NULL;
    }

    app.h264.data.macroblocks = malloc(app.h264.header.PicSizeInMbs * 
        sizeof(struct h264_macroblock_t));
    for (int i = 0; i < app.h264.header.PicSizeInMbs; i++) {
        struct h264_macroblock_t* mb = app.h264.data.macroblocks + i;
        mb->Intra16x16DCLevel.coeff = malloc(sizeof(struct h264_coeff_t) * 16);
        for (int j = 0; j < ARRAY_SIZE(mb->Intra16x16ACLevel); j++) {
            mb->Intra16x16ACLevel[j].coeff = malloc(sizeof(struct h264_coeff_t) * 15);
            mb->Intra16x16ACLevel[j].maxNumCoeff = 15;
            mb->Intra16x16ACLevel[j].coded_block_flag = 1;
            mb->Intra16x16ACLevel[j].numDecodAbsLevelEq1 = 0;
            mb->Intra16x16ACLevel[j].numDecodAbsLevelGt1 = 0;
        }
        for (int j = 0; j < ARRAY_SIZE(mb->LumaLevel4x4); j++) {
            mb->LumaLevel4x4[j].coeff = malloc(sizeof(struct h264_coeff_t) * 16);
            mb->LumaLevel4x4[j].maxNumCoeff = 16;
            mb->LumaLevel4x4[j].coded_block_flag = 1;
            mb->LumaLevel4x4[j].numDecodAbsLevelEq1 = 0;
            mb->LumaLevel4x4[j].numDecodAbsLevelGt1 = 0;
        }
        for (int j = 0; j < ARRAY_SIZE(mb->LumaLevel8x8); j++) {
            mb->LumaLevel8x8[j].coeff = malloc(sizeof(struct h264_coeff_t) * 64);
            mb->LumaLevel8x8[j].maxNumCoeff = 64;
            mb->LumaLevel8x8[j].coded_block_flag = 1;
            mb->LumaLevel8x8[j].numDecodAbsLevelEq1 = 0;
            mb->LumaLevel8x8[j].numDecodAbsLevelGt1 = 0;
        }
        for (int j = 0; j < ARRAY_SIZE(mb->ChromaDCLevel); j++) {
            mb->ChromaDCLevel[j].coeff = malloc(sizeof(struct h264_coeff_t) * NumC8x8 * 4);
            mb->ChromaDCLevel[j].maxNumCoeff = NumC8x8 * 4;
            mb->ChromaDCLevel[j].coded_block_flag = 1;
            mb->ChromaDCLevel[j].numDecodAbsLevelEq1 = 0;
            mb->ChromaDCLevel[j].numDecodAbsLevelGt1 = 0;
        }
        for (int j = 0; j < ARRAY_SIZE(mb->ChromaACLevel); j++) {
            for (int k = 0; k < ARRAY_SIZE(*mb->ChromaACLevel); k++) {
                mb->ChromaACLevel[j][k].coeff = malloc(sizeof(struct h264_coeff_t) * 15);
                mb->ChromaACLevel[j][k].maxNumCoeff = 15;
                mb->ChromaACLevel[j][k].coded_block_flag = 1;
                mb->ChromaACLevel[j][k].numDecodAbsLevelEq1 = 0;
                mb->ChromaACLevel[j][k].numDecodAbsLevelGt1 = 0;
            }
        }                            
    }

    CALL(h264_generate_MapUnitToSliceGroupMap(), error);
    CALL(h264_generate_MbToSliceGroupMap(), error);
    return 0;

error:
    return -1;
}

static int h264_read_mb_type(struct app_state_t* app)
{
    struct h264_rbsp_t* rbsp = &app.h264.rbsp;

    if (app.h264.pps.entropy_coding_mode_flag) {
        UNCOVERED_CASE(app.h264.header.slice_type, !=, SliceTypeI);
        CALL(h264_cabac_read_mb_type(), error);
	    app.h264.data.curr_mb->mb_type = (H264_U_I | app.h264.data.curr_mb->mb_type_origin);
    } else {
        app.h264.data.curr_mb->mb_type_origin = RBSP_READ_UE(rbsp);
        app.h264.data.curr_mb->mb_type = (H264_U_I | app.h264.data.curr_mb->mb_type_origin);
    }
    //H264_RBSP_DEBUG(app.h264.data.curr_mb->mb_type_origin);

    h264_MbPartPredMode(app.h264.data.curr_mb);
    return 0;

error:
    return -1;
}

static int h264_mb_pred(struct app_state_t* app)
{
    UNCOVERED_CASE(app.h264.pps.entropy_coding_mode_flag, ==, 0);
    struct h264_rbsp_t* rbsp = &app.h264.rbsp;

    if (app.h264.data.curr_mb->MbPartPredMode == H264_Intra_4x4
        || app.h264.data.curr_mb->MbPartPredMode == H264_Intra_8x8
        || app.h264.data.curr_mb->MbPartPredMode == H264_Intra_16x16) {
        if (app.h264.data.curr_mb->MbPartPredMode == H264_Intra_4x4) {
            for (int luma4x4BlkIdx = 0; luma4x4BlkIdx < 16; luma4x4BlkIdx++) {
                if (app.h264.pps.entropy_coding_mode_flag) {
                    CALL(h264_cabac_read_intra4x4_pred_mode(luma4x4BlkIdx), error);
                } else {
                    app.h264.data.curr_mb->intra4x4_pred_mode[luma4x4BlkIdx] =
                        RBSP_READ_U1(rbsp);
                }
                //H264_RBSP_DEBUG(app.h264.data.curr_mb->intra4x4_pred_mode[luma4x4BlkIdx]);
            }
            if (app.h264.sps.ChromaArrayType == 1 || app.h264.sps.ChromaArrayType == 2) {
                if (app.h264.pps.entropy_coding_mode_flag) {
                    CALL(h264_cabac_read_intra_chroma_pred_mode(), error);
                } else {
                    app.h264.data.curr_mb->intra_chroma_pred_mode = RBSP_READ_UE(rbsp);
                }
                //H264_RBSP_DEBUG(app.h264.data.curr_mb->intra_chroma_pred_mode);
            }
        }
        UNCOVERED_CASE(app.h264.data.curr_mb->MbPartPredMode, ==, H264_Intra_8x8);
    }
    else {
        UNCOVERED_CASE(app.h264.data.curr_mb->MbPartPredMode, !=, H264_Intra_4x4);
    }
    return 0;
error:
    return -1;
}

int h264_residual_block(struct app_state_t* app,
    int type,
    struct h264_coeff_level_t* blocks,
    int blkIdx,
    int startIdx,
    int endIdx,
    int maxNumCoeff)
{
    //DEBUG_MSG("INFO: h264_residual_block, blkIdx (%d)\n", blkIdx);
    //struct h264_rbsp_t* rbsp = &app.h264.rbsp;
    struct h264_coeff_level_t* coeffLevel = blocks + blkIdx;

    UNCOVERED_CASE(app.h264.pps.entropy_coding_mode_flag, ==, 0);

    if (maxNumCoeff != 64 || app.h264.sps.ChromaArrayType == 3) {
        CALL(
            h264_cabac_read_coded_block_flag(app,
                type,
                blocks,
                blkIdx),
            error
        );
        //H264_RBSP_DEBUG(coeffLevel->coded_block_flag);
    }
    for (int i = 0; i < maxNumCoeff; i++ ) {
        coeffLevel->coeff[i].value = 0;
    }

    if (coeffLevel->coded_block_flag) {
        int numCoeff = endIdx + 1;
        int i = startIdx;
        while (i < numCoeff - 1) {
            CALL(
                h264_cabac_read_significant_coeff_flag(app,
                    type,
                    coeffLevel,
                    i),
                error
            );
            //H264_RBSP_DEBUG(coeffLevel->coeff[i].significant_coeff_flag);
            if (coeffLevel->coeff[i].significant_coeff_flag) {
                CALL(
                    h264_cabac_read_last_significant_coeff_flag(app,
                        type,
                        coeffLevel,
                        i),
                    error
                );
                //H264_RBSP_DEBUG(coeffLevel->coeff[i].last_significant_coeff_flag);

                if (coeffLevel->coeff[i].last_significant_coeff_flag) {
                    numCoeff = i + 1;
                }
            }
            i++;        
        }
        struct h264_coeff_t* coeff = coeffLevel->coeff + (numCoeff - 1);
        CALL(
            h264_cabac_read_coeff_abs_level_minus1(app,
                type,
                coeffLevel,
                numCoeff - 1),
            error
        );
        //H264_RBSP_DEBUG(coeff->coeff_abs_level_minus1);

        CALL(
            h264_cabac_read_coeff_sign_flag(app,
                coeffLevel,
                numCoeff - 1),
            error
        );
        //H264_RBSP_DEBUG(coeff->coeff_sign_flag);

        coeff->value = (coeff->coeff_abs_level_minus1 + 1) *
            (1 - 2 * coeff->coeff_sign_flag);
        if (coeff->value == 1) {
            coeffLevel->numDecodAbsLevelEq1++;
        } else if (coeff->value > 1) {
            coeffLevel->numDecodAbsLevelGt1++;
        }

        for (int i = numCoeff - 2; i >= startIdx; i--) {
            coeff = coeffLevel->coeff + i;
            if (coeff->significant_coeff_flag) {
                CALL(
                    h264_cabac_read_coeff_abs_level_minus1(app,
                        type,
                        coeffLevel,
                        i),
                    error
                );
                //H264_RBSP_DEBUG(coeff->coeff_abs_level_minus1);

                CALL(
                    h264_cabac_read_coeff_sign_flag(app,
                        coeffLevel,
                        i),
                    error
                );
                //H264_RBSP_DEBUG(coeff->coeff_sign_flag);

                coeff->value = (coeff->coeff_abs_level_minus1 + 1) *
                    (1 - 2 * coeff->coeff_sign_flag);
                if (coeff->value == 1) {
                    coeffLevel->numDecodAbsLevelEq1++;
                } else if (coeff->value > 1) {
                    coeffLevel->numDecodAbsLevelGt1++;
                }
            }
        }
    }
    return 0;

error:
    return -1;
}

static int h264_residual_luma(struct app_state_t* app,
    int startIdx,
    int endIdx)
{
    if (startIdx == 0 && app.h264.data.curr_mb->MbPartPredMode == H264_Intra_16x16) {
        h264_residual_block(app,
            LUMA_16DC,
            &app.h264.data.curr_mb->Intra16x16DCLevel,
            0,
            0,
            15,
            16);
    }
    for (int i8x8 = 0; i8x8 < 4; i8x8++) {
        if (!app.h264.pps.transform_8x8_mode_flag || !app.h264.pps.entropy_coding_mode_flag) {
            for (int i4x4 = 0; i4x4 < 4; i4x4++) {
                int blkIdx = i8x8 * 4 + i4x4;

                if (app.h264.data.curr_mb->CodedBlockPatternLuma & (1 << i8x8)) {
                    if (app.h264.data.curr_mb->MbPartPredMode == H264_Intra_16x16) {
                        h264_residual_block(app,
                            LUMA_16AC,
                            app.h264.data.curr_mb->Intra16x16ACLevel,
                            blkIdx,
                            h264_max(0, startIdx - 1),
                            endIdx - 1,
                            15);
                    }
                    else {
                        h264_residual_block(app,
                            LUMA_4x4,
                            app.h264.data.curr_mb->LumaLevel4x4,
                            blkIdx,
                            startIdx,
                            endIdx,
                            16);
                    }
                }
                else if (app.h264.data.curr_mb->MbPartPredMode == H264_Intra_16x16) {
                    for (int i = 0; i < 15; i++) {
                        app.h264.data.curr_mb->Intra16x16ACLevel[blkIdx]
                            .coeff[i].value = 0;
                    }
                }
                else {
                    for (int i = 0; i < 16; i++) {
                        app.h264.data.curr_mb->LumaLevel4x4[blkIdx]
                            .coeff[i].value = 0;
                    }                    
                }

                //if (
                //    !app.h264.pps.entropy_coding_mode_flag &&
                //    app.h264.pps.transform_8x8_mode_flag
                //) {
                UNCOVERED_CASE(app.h264.pps.entropy_coding_mode_flag, ==, 0);
            }
        }
        else if (app.h264.data.curr_mb->CodedBlockPatternLuma & (1 << i8x8)) {
            h264_residual_block(app,
                LUMA_8x8,
                app.h264.data.curr_mb->LumaLevel8x8,
                i8x8,
                4 * startIdx,
                4 * endIdx + 3,
                64);
        }
        else {
            for(int i = 0; i < 64; i++) {
                app.h264.data.curr_mb->LumaLevel8x8[i8x8].coeff[i].value = 0;
            }          
        }
    }
    return 0;
}

static int h264_residual(struct app_state_t* app, int startIdx, int endIdx)
{
    h264_residual_luma(startIdx, endIdx);

    if (app.h264.sps.ChromaArrayType == 1 || app.h264.sps.ChromaArrayType == 2) {
        UNCOVERED_CASE(app.h264.sps.chroma_format_idc, !=, CHROMA_FORMAT_YUV420);

        int SubWidthC = -1;
        int SubHeightC = -1;
        CALL(
            h264_get_chroma_variables(&SubWidthC, &SubHeightC),
            error
        );

        int NumC8x8 = 4 / (SubWidthC * SubHeightC);
        for (int iCbCr = 0; iCbCr < 2; iCbCr++) {
            //chroma DC residual present
            if ((app.h264.data.curr_mb->CodedBlockPatternChroma & 3) && startIdx == 0) {
                h264_residual_block(app,
                    CHROMA_DC,
                    app.h264.data.curr_mb->ChromaDCLevel,
                    iCbCr,
                    0,
                    4 * NumC8x8 - 1,
                    4 * NumC8x8);
            }
            else {
                for (int i = 0; i < 4 * NumC8x8; i++)
                    app.h264.data.curr_mb->ChromaDCLevel[iCbCr].coeff[i].value = 0;                   
            }
        }
        for (int iCbCr = 0; iCbCr < 2; iCbCr++) {
            for (int i8x8 = 0; i8x8 < NumC8x8; i8x8++) {
                for (int i4x4 = 0; i4x4 < 4; i4x4++) {
                    int blkIdx = i8x8 * 4 + i4x4;
                    UNCOVERED_CASE(blkIdx, >=, 4);
                    // chroma AC residual present
                    if (app.h264.data.curr_mb->CodedBlockPatternChroma & 2) {
                        h264_residual_block(app,
                            CHROMA_AC,
                            &app.h264.data.curr_mb->ChromaACLevel[iCbCr][0],
                            blkIdx,
                            h264_max(0, startIdx - 1),
                            endIdx - 1,
                            15);
                    }
                    else {
                        for(int i = 0; i < 15; i++) {
                            app.h264.data.curr_mb->ChromaACLevel[iCbCr][blkIdx]
                                .coeff[i].value = 0;
                        }
                    }
                }
            }
        }
    }
    UNCOVERED_CASE(app.h264.sps.ChromaArrayType, ==, 3);
    return 0;

error:
    return -1;
}

static int h264_macroblock_layer(struct app_state_t* app)
{
    struct h264_rbsp_t* rbsp = &app.h264.rbsp;

    CALL(h264_read_mb_type(), error);

    if (app.h264.data.curr_mb->mb_type == H264_U_I_PCM) {
        DEBUG("ERROR: h264_macroblock_layer to implement pcm_sample_luma,"
            " mb_type H264_U_I_PCM\n");

        //TODO: initialize decoding engine
        //RBSP_ALLIGN(rbsp);
        while (!RBSP_IS_ALLIGN(rbsp)) {
            int rbsp_alignment_zero_bit = RBSP_READ_U1(rbsp);
            DEBUG_MSG("INFO: h264_macroblock_layer, rbsp_alignment_zero_bit (%lld:%d) %d\n",
                rbsp->p - rbsp->start, rbsp->bits_left, rbsp_alignment_zero_bit);
            if (rbsp_alignment_zero_bit) {
                DEBUG_MSG("ERROR: h264_macroblock_layer failed to read,"
                    " rbsp_alignment_zero_bit != 0\n");
                //return -1;
            }
        }
        for (int i = 0; i < 256; i++) {
            app.h264.data.curr_mb->pcm_sample_luma[i] = RBSP_READ_UN(rbsp, 8);
        }
        UNCOVERED_CASE(app.h264.sps.chroma_format_idc, !=, CHROMA_FORMAT_YUV420);

        int SubWidthC = -1;
        int SubHeightC = -1;
        CALL(
            h264_get_chroma_variables(&SubWidthC, &SubHeightC),
            error
        );
        int MbWidthC = 16 / SubWidthC;
        int MbHeightC = 16 / SubHeightC;
        for(int i = 0; i < 2 * MbWidthC * MbHeightC; i++ ) {
            app.h264.data.curr_mb->pcm_sample_chroma[i] = RBSP_READ_UN(rbsp, 8);
        }
    }
    else {
        int noSubMbPartSizeLessThan8x8Flag = 1;
        if (app.h264.data.curr_mb->mb_type != H264_U_I_NxN &&
            app.h264.data.curr_mb->MbPartPredMode != H264_Intra_16x16 &&
            h264_NumMbPart(app.h264.data.curr_mb->mb_type) == 4) {
            DEBUG_MSG("ERROR: TO IMPLEMENT mb_type %d, %s - %s:%d\n",
                app.h264.data.curr_mb->mb_type, __FILE__, __FUNCTION__, __LINE__);
            return -1;
        } else {
            if (
                app.h264.pps.transform_8x8_mode_flag &&
                app.h264.data.curr_mb->mb_type == H264_U_I_NxN
            ) {
                if (app.h264.pps.entropy_coding_mode_flag) {
                    CALL(h264_cabac_read_transform_size_8x8_flag(), error);
                } else {
                    app.h264.data.curr_mb->transform_size_8x8_flag =
                        RBSP_READ_U1(&app.h264.rbsp);
                }
                //H264_RBSP_DEBUG(app.h264.data.curr_mb->transform_size_8x8_flag);
            }
            h264_mb_pred();
        }

        if (app.h264.data.curr_mb->MbPartPredMode != H264_Intra_16x16) {
            UNCOVERED_CASE(app.h264.pps.entropy_coding_mode_flag, ==, 0);
            CALL(h264_cabac_read_coded_block_pattern(), error);
            //H264_RBSP_DEBUG(app.h264.data.curr_mb->coded_block_pattern);

            if (app.h264.data.curr_mb->CodedBlockPatternLuma > 0 &&
                app.h264.data.curr_mb->transform_size_8x8_flag &&
                app.h264.data.curr_mb->mb_type != H264_U_I_NxN &&
                noSubMbPartSizeLessThan8x8Flag &&
                (
                    app.h264.data.curr_mb->mb_type != H264_U_B_Direct_16x16 ||
                    app.h264.sps.direct_8x8_inference_flag
                )) {

                DEBUG_MSG("ERROR: TO IMPLEMENT transform_size_8x8_flag, %s - %s:%d\n",
                    __FILE__, __FUNCTION__, __LINE__);
            }

            if (app.h264.data.curr_mb->CodedBlockPatternLuma > 0
                || app.h264.data.curr_mb->CodedBlockPatternChroma > 0
                || app.h264.data.curr_mb->MbPartPredMode == H264_Intra_16x16
            ) {
                if (app.h264.pps.entropy_coding_mode_flag) {
                    CALL(h264_cabac_read_mb_qp_delta(), error);
                } else {
                    app.h264.data.curr_mb->mb_qp_delta = RBSP_READ_SE(&app.h264.rbsp);
                }
                //H264_RBSP_DEBUG(app.h264.data.curr_mb->mb_qp_delta);
                CALL(h264_residual(0, 15), error);
            }
        }
    }
    return 0;

error:
    return -1;
}

int h264_NextMbAddress(struct app_state_t* app, int n)
{
    int i = n + 1;
    while (
        i < app.h264.header.PicSizeInMbs &&
        app.h264.MbToSliceGroupMap[i] != app.h264.MbToSliceGroupMap[n]
    ) {
        i++;
    }
    return i; 
}

// Slice data 7.3.4
static int h264_read_slice_data(struct app_state_t* app)
{
    struct h264_rbsp_t* rbsp = &app.h264.rbsp;

    CALL(h264_init_slice_data(), error);

    if (app.h264.pps.entropy_coding_mode_flag) {
        H264_RBSP_DEBUG(*rbsp->p);
        RBSP_ALLIGN(rbsp);
        H264_RBSP_DEBUG(*rbsp->p);

        CALL(h264_cabac_init(), error);
    }

    app.h264.data.PrevMbAddr = -1;
    app.h264.data.CurrMbAddr = app.h264.header.first_mb_in_slice *
        (1 + app.h264.header.MbaffFrameFlag);
    app.h264.data.curr_mb = app.h264.data.macroblocks + app.h264.data.CurrMbAddr;
    int moreDataFlag = 1;
    int prevMbSkipped = 1;
    do {
        if (
            app.h264.header.slice_type != SliceTypeI &&
            app.h264.header.slice_type != SliceTypeSI
        ) {
            UNCOVERED_CASE(app.h264.pps.entropy_coding_mode_flag, ==, 0);

            //fixedLength:Ceil(Log2(2)) = 1
            app.h264.data.curr_mb->mb_skip_flag = RBSP_READ_U1(rbsp);
            DEBUG_MSG("ERROR: h264_read_slice_data, mb_skip_flag (%lld:%d, %d)\n",
                rbsp->p - rbsp->start,
                rbsp->bits_left,
                app.h264.data.curr_mb->mb_skip_flag);
            moreDataFlag = !app.h264.data.curr_mb->mb_skip_flag;
            return -1;
        }
        if (moreDataFlag) {
            if (
                app.h264.header.MbaffFrameFlag &&
                (
                    app.h264.data.CurrMbAddr % 2 == 0 ||
                    (app.h264.data.CurrMbAddr % 2 == 1 && prevMbSkipped
                )
            )) {
                //fixedLength:Ceil(Log2(2)) = 1
                app.h264.data.curr_mb->mb_field_decoding_flag = RBSP_READ_U1(rbsp);
                fprintf(stderr,
                    "ERROR: h264_read_slice_data, mb_field_decoding_flag (%lld:%d) %d\n",
                    rbsp->p - rbsp->start,
                    rbsp->bits_left,
                    app.h264.data.curr_mb->mb_field_decoding_flag);
            }
            CALL(h264_macroblock_layer(), error);
        }
        if (!app.h264.pps.entropy_coding_mode_flag) {
            moreDataFlag = !RBSP_EOF(rbsp);
        } else {
            if (
                app.h264.header.slice_type != SliceTypeI &&
                app.h264.header.slice_type != SliceTypeSI
            ) {
                prevMbSkipped = app.h264.data.curr_mb->mb_skip_flag;
            }
            if (app.h264.header.MbaffFrameFlag && app.h264.data.CurrMbAddr % 2 == 0) {
                moreDataFlag = 1;
            } else {
                CALL(h264_cabac_end_of_slice_flag(), error);
                //H264_RBSP_DEBUG(app.h264.data.curr_mb->end_of_slice_flag);
                moreDataFlag = !app.h264.data.curr_mb->end_of_slice_flag;
            }
        }
        app.h264.data.PrevMbAddr = app.h264.data.CurrMbAddr;
        app.h264.data.CurrMbAddr = h264_NextMbAddress(app.h264.data.CurrMbAddr);
        app.h264.data.curr_mb = &app.h264.data.macroblocks[app.h264.data.CurrMbAddr];
        //H264_RBSP_DEBUG(app.h264.data.CurrMbAddr);
    } while (moreDataFlag);

    return 0;

error:
    return -1;
}
