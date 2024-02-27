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

#include "h264_cabac.h"

//9.3.1.1 Initialization process for context variables
//The processes in clauses 9.3.1.1 and 9.3.1.2 are invoked when starting the parsing of
//the slice data of a slice in clause 7.3.4.
//The process in clause 9.3.1.2 is also invoked after decoding any
static int h264_cabac_init_context()
{
    int QpBdOffsety = 6 * app.h264.sps.bit_depth_luma_minus8; // (7-4)
    int SliceQPy = 26 + app.h264.pps.pic_init_qp_minus26 + 
        h264_clip(-QpBdOffsety, 51, app.h264.header.slice_qp_delta); // (7-30)

    UNCOVERED_CASE(app.h264.header.slice_type, !=, SliceTypeI);

    for (int i = 0; i < 1024; i++) {
        int preCtxState = h264_clip(1, 126, (cabac_context_init_I[i][0] * 
            h264_clip(0, 51, SliceQPy) >> 4) + cabac_context_init_I[i][1]); // (9-5)
        if (preCtxState <= 63) {
            app.h264.contexts[i].pStateIdx = 63 - preCtxState;
            app.h264.contexts[i].valMPS = 0;
        } else {
            app.h264.contexts[i].pStateIdx = preCtxState - 64;
            app.h264.contexts[i].valMPS = 1;
        }
    }
    return 0;
}

//9.3.1.2 Initialization process for the arithmetic decoding engine
static int h264_cabac_init_decoder()
{
    app.h264.cabac.codIOffset = RBSP_READ_UN(&app.h264.rbsp, 9);
    app.h264.cabac.codIRange = 510;

    DEBUG_MSG("INFO: h264_cabac_init_decoder codIOffset (%d)\n", app.h264.cabac.codIOffset);

    return 0;
}

static int h264_cabac_init()
{
    int res;
    res = h264_cabac_init_context();
    if (res) {
        return res;
    }
    res = h264_cabac_init_decoder();
    if (res) {
        return res;
    }
    return 0;
}

//Figure 9-4 – Flowchart of renormalization
void h264_RenormD() {
    struct h264_rbsp_t* rbsp = &app.h264.rbsp;

    //Figure 9-4 – Flowchart of renormalization
    while (app.h264.cabac.codIRange < 256) {
        //H264_RBSP_DEBUG(app.h264.cabac.codIRange);
        app.h264.cabac.codIRange <<= 1;
        app.h264.cabac.codIOffset <<= 1;
        app.h264.cabac.codIOffset |= RBSP_READ_U1(rbsp);
    }
}

//9.3.3.2.1 Arithmetic decoding process for a binary decision
unsigned h264_DecodeDecision(unsigned ctxIdx)
{
    unsigned binVal;
    struct h264_context_t* ctx = &app.h264.contexts[ctxIdx];

    //Figure 9-3 Flowchart for decoding a decision
    unsigned qCodIRangeIdx = (app.h264.cabac.codIRange >> 6) & 0x03; //(9-25)
    unsigned codIRangeLPS  = rangeTabLPS[ctx->pStateIdx][qCodIRangeIdx]; //(9-26)
    app.h264.cabac.codIRange -= codIRangeLPS;

    if (app.h264.cabac.codIOffset >= app.h264.cabac.codIRange) {
        binVal = !ctx->valMPS;
        app.h264.cabac.codIOffset -= app.h264.cabac.codIRange;
        app.h264.cabac.codIRange = codIRangeLPS;
        if (ctx->pStateIdx == 0) {
            ctx->valMPS = 1 - ctx->valMPS;
        }
        ctx->pStateIdx = transIdxLPS[ctx->pStateIdx];
    } else {
        binVal = ctx->valMPS;
        ctx->pStateIdx = transIdxMPS[ctx->pStateIdx];
    }

    h264_RenormD();

    return binVal? 1: 0;
}

//Figure 9-5 – Flowchart of bypass decoding process
unsigned h264_DecodeBypass()
{
    int binVal;
    app.h264.cabac.codIOffset <<= 1;
    app.h264.cabac.codIOffset |= RBSP_READ_U1(&app.h264.rbsp);
    if (app.h264.cabac.codIOffset >= app.h264.cabac.codIRange) {
        app.h264.cabac.codIOffset -= app.h264.cabac.codIRange;
        binVal = 1;
    } else {
        binVal = 0;
    }
    return binVal;
}

//Figure 9-6 – Flowchart of decoding a decision before termination
unsigned h264_DecodeTerminate()
{
    int binVal;
    app.h264.cabac.codIRange -= 2;
    if (app.h264.cabac.codIOffset >= app.h264.cabac.codIRange) {
        binVal = 1;
    } else {
        binVal = 0;
        h264_RenormD();
    }
    return binVal;
}

unsigned h264_DecodeBin(unsigned ctxIdx, unsigned bypassFlag)
{
    if (bypassFlag) {
        return h264_DecodeBypass();
    }
    else {
        if (ctxIdx == 276) {
            return h264_DecodeTerminate();
        }
        else {
            return h264_DecodeDecision(ctxIdx);
        }
    }
}

// 6.4.12.1 Specification for neighbouring locations in fields and non-MBAFF frames
int h264_get_MbAddr_type(int* MbAddrN, int xN, int yN, int maxW, int maxH)
{
    UNCOVERED_CASE(app.h264.header.MbaffFrameFlag, !=, 0);
    //Table 6-3 – Specification of mbAddrN
    if (xN < 0 && yN < 0) {
        *MbAddrN = h264_MbAddrD();
        //DEBUG_MSG("INFO: MbAddrD (%d)\n", *MbAddrN);
    } else if (xN < 0 && yN >= 0 && yN < maxH) {
        *MbAddrN = h264_MbAddrA();
        //DEBUG_MSG("INFO: MbAddrA (%d)\n", *MbAddrN);
    } else if (xN >= 0 && xN < maxW && yN < 0) {
        *MbAddrN = h264_MbAddrB();
        //DEBUG_MSG("INFO: MbAddrB (%d)\n", *MbAddrN);
    } else if (xN >= 0 && xN < maxW && yN >= 0 && yN < maxH) {
        *MbAddrN = app.h264.data.CurrMbAddr;
        //DEBUG_MSG("INFO: CurrMbAddr (%d)\n", *MbAddrN);
    } else if (xN >= maxW && yN < 0) {
        *MbAddrN = h264_MbAddrC();
        //DEBUG_MSG("INFO: MbAddrC (%d)\n", *MbAddrN);
    } else if (xN >= maxW  && yN >= 0 && yN < maxH) {
        *MbAddrN = -1;
        //DEBUG_MSG("INFO: MbAddr: not available\n");
    } else if (yN >= maxH) {
        *MbAddrN = -1;
    } else {
        return -1;
        DEBUG_MSG("ERROR: uncovered case xN(%d) yN(%d)\n%s:%d - %s\n",
            xN, yN, __FILE__, __LINE__, __FUNCTION__);
    }
    return 0;
}

int h264_get_neighbouring_macroblocks(int xD, int yD,
    int* MbAddrN)
{
    //6.4.11.1 Derivation process for neighbouring macroblocks
    int xN = xD;
    int yN = yD;
    // 6.4.12 Derivation process for neighbouring locations
    int SubWidthC = -1;
    int SubHeightC = -1;
    CALL(
        h264_get_chroma_variables(&SubWidthC, &SubHeightC),
        error
    );
    int maxW = SubWidthC; // (6-32)
    int maxH = SubHeightC; // (6-33)
    CALL(
        h264_get_MbAddr_type(MbAddrN, xN, yN, maxW, maxH),
        error
    );
    return 0;
error:
    return -1;
}

int h264_get_neighbouring_8x8luma(int luma8x8BlkIdx, int xD, int yD,
    int* MbAddrN, int* luma8x8BlkIdxN)
{
    //6.4.11.2 Derivation process for neighbouring 8x8 luma block
    int xN = (luma8x8BlkIdx % 2) * 8 + xD; // (6-23)
    int yN = (luma8x8BlkIdx / 2) * 8 + yD; // (6-24)
    // 6.4.12 Derivation process for neighbouring locations
    int maxW = 16; // (6-31)
    int maxH = 16; // (6-31)
    CALL(
        h264_get_MbAddr_type(MbAddrN, xN, yN, maxW, maxH),
        error
    );
    if (h264_is_MbAddr_available(*MbAddrN)) {
        int xW = (xN + maxW) % maxW; // (6-34)
        int yW = (yN + maxH) % maxH; // (6-35)
        // 6.4.13.3 Derivation process for 8x8 luma block indices
        *luma8x8BlkIdxN = 2 * (yW / 8) + (xW / 8); //(6-40)
    }
    return 0;
error:
    return -1;
}

int h264_get_neighbouring_4x4chroma(
    struct app_state_t* app,
    int chroma4x4BlkIdx, int xD, int yD, 
    int* MbAddrN, int* chroma4x4BlkIdxN)
{
    //6.4.11.5 Derivation process for neighbouring 4x4 chroma blocks
    UNCOVERED_CASE(app.h264.sps.ChromaArrayType, ==, 3);
    int x = h264_InverseRasterScan(chroma4x4BlkIdx, 4, 4, 8, 0); //(6-21)
    int y = h264_InverseRasterScan(chroma4x4BlkIdx, 4, 4, 8, 1); //(6-22)
    int xN = x + xD; //(6-27)
    int yN = y + yD; //(6-28)
    // 6.4.12 Derivation process for neighbouring locations
    int SubWidthC = -1;
    int SubHeightC = -1;
    CALL(
        h264_get_chroma_variables(&SubWidthC, &SubHeightC),
        error
    );
    int maxW = SubWidthC; // (6-32)
    int maxH = SubHeightC; // (6-33)
    CALL(
        h264_get_MbAddr_type(MbAddrN, xN, yN, maxW, maxH),
        error
    );
    if (h264_is_MbAddr_available(*MbAddrN)) {
        int xW = (xN + maxW) % maxW; // (6-34)
        int yW = (yN + maxH) % maxH; // (6-35)
        // 6.4.13.2 Derivation process for 4x4 chroma block indices
        *chroma4x4BlkIdxN = 2 * (yW / 4) + (xW / 4); //(6-39)
    }
    return 0;
error:
    return -1;
}

// 9.3.3.1.1.3 Derivation process of ctxIdxInc for the syntax element mb_type
int h264_cabac_derive_mb_type(
    struct app_state_t* app,
    int xD, int yD,
    int ctxIdxOffset,
    int* condTermFlagN)
{
    //6.4.11.1 Derivation process for neighbouring macroblocks
    int MbAddrN = -1;
    CALL(
        h264_get_neighbouring_macroblocks(xD, yD, &MbAddrN),
        error
    );

    struct h264_macroblock_t *mb_addr_n = h264_is_MbAddr_available(MbAddrN)?
        &app.h264.data.macroblocks[MbAddrN]:
        NULL;

    if (
        mb_addr_n == NULL ||
        (ctxIdxOffset == 0 && mb_addr_n->mb_type == H264_U_SI) ||
        (ctxIdxOffset == 3 && mb_addr_n->mb_type == H264_U_I_NxN) ||
        (ctxIdxOffset == 27 && (
            mb_addr_n->mb_type == H264_U_B_Skip ||
            mb_addr_n->mb_type == H264_U_B_Direct_16x16
        ))
    ) {
        *condTermFlagN = 0;
    }
    else {
        *condTermFlagN = 1;
    }
    return 0;

error:
    return -1;
}

int h264_cabac_read_mb_type(struct app_state_t* app)
{
    //Table 9-34 – Syntax elements and associated types of binarization, maxBinIdxCtx
    //and ctxIdxOffset
    UNCOVERED_CASE(app.h264.header.slice_type, !=, SliceTypeI);
    int ctxIdxOffset = 3;

    // 9.3.3.1.1.3 Derivation process of ctxIdxInc for the syntax element mb_type
    int condTermFlagA, condTermFlagB;
    CALL(
        h264_cabac_derive_mb_type(-1, 0, ctxIdxOffset, &condTermFlagA),
        error
    );
    CALL(
        h264_cabac_derive_mb_type(0, -1, ctxIdxOffset, &condTermFlagB),
        error
    );
    int ctxIdxInc = condTermFlagA + condTermFlagB;
    //DEBUG_MSG("INFO: h264_cabac_read_mb_type ctxIdxInc(%d)\n", ctxIdxInc);

    //Table 9-36 – Binarization for macroblock types in I slices
    int bin = h264_DecodeDecision(ctxIdxOffset + ctxIdxInc); //ctxIdxOffset(3): 0 - 0, 1, 2
    if (bin == 0) {
        //DEBUG_MSG("INFO: h264_cabac_read_mb_type H264_I_NxN\n");
        app.h264.data.curr_mb->mb_type_origin = H264_I_NxN;
        return 0;
    }
    else { // 16x16 Intra
        int bin = h264_DecodeTerminate(); //ctxIdxOffset(3): 1 - 276
        if(bin == 1) {
            //DEBUG_MSG("INFO: h264_cabac_read_mb_type H264_I_PCM\n");
            app.h264.data.curr_mb->mb_type_origin = H264_I_PCM;
            return 0;
        }
        else {
            app.h264.data.curr_mb->mb_type_origin = 1; // H264_I_16x16_1_0_0
            app.h264.data.curr_mb->mb_type_origin +=
                12 * h264_DecodeDecision(ctxIdxOffset + 3); //ctxIdxOffset(3): 2 - 3
            int bin3 = h264_DecodeDecision(ctxIdxOffset + 4); //ctxIdxOffset(3): 3 - 4
            app.h264.data.curr_mb->mb_type_origin += 4 * bin3;
            if (bin3) {
                //ctxIdxOffset(3): 4 - bin3? 5: 6 (9.3.3.1.2)
                app.h264.data.curr_mb->mb_type_origin +=
                    4 * h264_DecodeDecision(ctxIdxOffset + 5);
                //ctxIdxOffset(3): 5 - bin3? 6: 7 (9.3.3.1.2)
                app.h264.data.curr_mb->mb_type_origin +=
                    2 * h264_DecodeDecision(ctxIdxOffset + 6);
                app.h264.data.curr_mb->mb_type_origin +=
                    h264_DecodeDecision(ctxIdxOffset + 7); //ctxIdxOffset(3): 6 - 7
            }
            else {
                //ctxIdxOffset(3): 4 - bin3? 5: 6 (9.3.3.1.2)
                app.h264.data.curr_mb->mb_type_origin +=
                    2 * h264_DecodeDecision(ctxIdxOffset + 6);
                //ctxIdxOffset(3): 5 - bin3? 6: 7 (9.3.3.1.2) 
                app.h264.data.curr_mb->mb_type_origin +=
                    h264_DecodeDecision(ctxIdxOffset + 7);
            }
            return 0;
        }
    }
error:
    return -1;
}

// 9.3.3.1.1.10 Derivation process of ctxIdxInc for the syntax element transform_size_8x8_flag
int h264_cabac_derive_size_8x8_flag(
    struct app_state_t* app,
    int xD, int yD,
    int* condTermFlagN)
{
    //6.4.11.1 Derivation process for neighbouring macroblocks
    int MbAddrN = -1;
    CALL(
        h264_get_neighbouring_macroblocks(xD, yD, &MbAddrN),
        error
    );

    struct h264_macroblock_t *mb_addr_n = h264_is_MbAddr_available(MbAddrN)?
        &app.h264.data.macroblocks[MbAddrN]:
        NULL;

    if (
        mb_addr_n == NULL ||
        mb_addr_n->transform_size_8x8_flag == 0
    ) {
        *condTermFlagN = 0;
    }
    else {
        *condTermFlagN = 1;
    }
    return 0;

error:
    return -1;
}

int h264_cabac_read_transform_size_8x8_flag(struct app_state_t* app)
{
    //Table 9-34 – Syntax elements and associated types of binarization, maxBinIdxCtx
    //and ctxIdxOffset
    int ctxIdxOffset = 399;

    // 9.3.3.1.1.10 Derivation process of ctxIdxInc for the syntax element transform_size_8x8_flag
    int condTermFlagA, condTermFlagB;
    CALL(
        h264_cabac_derive_size_8x8_flag(-1, 0, &condTermFlagA),
        error
    );
    CALL(
        h264_cabac_derive_size_8x8_flag(0, -1, &condTermFlagB),
        error
    );
    int ctxIdxInc = condTermFlagA + condTermFlagB;
    //DEBUG_MSG("INFO: h264_cabac_read_transform_size_8x8_flag ctxIdxInc(%d)\n", ctxIdxInc);    

    app.h264.data.curr_mb->transform_size_8x8_flag =
        h264_DecodeDecision(ctxIdxOffset + ctxIdxInc);
    return 0;

error:
    return -1;
}

// prev_intra4x4_pred_mode_flag and rem_intra4x4_pred_mode
int h264_cabac_read_intra4x4_pred_mode(struct app_state_t* app, int i)
{
    //Table 9-11 – Association of ctxIdx and syntax elements for each slice type in the
    //initialization process prev_intra4x4_pred_mode_flag - 68, rem_intra4x4_pred_mode - 69

    int bin = h264_DecodeDecision(68);
    if (bin) {
        app.h264.data.curr_mb->intra4x4_pred_mode[i] = -1;
        return 0;
    } else {
        // rem_intra4x4_pred_mode
        bin |= h264_DecodeDecision(69);
        bin |= h264_DecodeDecision(69) << 1;
        bin |= h264_DecodeDecision(69) << 1;
        app.h264.data.curr_mb->intra4x4_pred_mode[i] = bin;
        return 0;
    }
}

// 9.3.3.1.1.8 Derivation process of ctxIdxInc for the syntax element intra_chroma
int h264_cabac_derive_intra_chroma_pred_mode(
    struct app_state_t* app,
    int xD, int yD,
    int* condTermFlagN)
{
    //6.4.11.1 Derivation process for neighbouring macroblocks
    int MbAddrN = -1;
    CALL(
        h264_get_neighbouring_macroblocks(xD, yD, &MbAddrN),
        error
    );

    struct h264_macroblock_t *mb_addr_n = h264_is_MbAddr_available(MbAddrN)?
        &app.h264.data.macroblocks[MbAddrN]:
        NULL;

    if (
        mb_addr_n == NULL ||
        h264_is_inter_pred_mode(mb_addr_n) ||
        mb_addr_n->mb_type == H264_U_I_PCM ||
        mb_addr_n->intra_chroma_pred_mode == 0
    ) {
        *condTermFlagN = 0;
    }
    else {
        *condTermFlagN = 1;
    }
    return 0;

error:
    return -1;
}

int h264_cabac_read_intra_chroma_pred_mode(struct app_state_t* app)
{
    //Table 9-11 Association of ctxIdx and syntax elements for each slice type in
    // the initialization process 64..67
    //Table 9-34 Syntax elements and associated types of binarization, maxBinIdxCtx, 
    //and ctxIdxOffset TU, cMax=3

    // Table 9-39 Assignment of ctxIdxInc to binIdx for all ctxIdxOffset
    int ctxIdxOffset = 64;
    // 9.3.3.1.1.8 Derivation process of ctxIdxInc for the syntax element intra_chroma_pred_mode
    int condTermFlagA, condTermFlagB;
    CALL(
        h264_cabac_derive_size_8x8_flag(-1, 0, &condTermFlagA),
        error
    );
    CALL(
        h264_cabac_derive_size_8x8_flag(0, -1, &condTermFlagB),
        error
    );
    int ctxIdxInc = condTermFlagA + condTermFlagB;
    // DEBUG_MSG("INFO: h264_cabac_read_intra_chroma_pred_mode ctxIdxInc(%d)\n", ctxIdxInc);    

    // 64: 0 - 0,1,2 (clause 9.3.3.1.1.8)
    if (!h264_DecodeDecision(ctxIdxOffset + ctxIdxInc)) {
        app.h264.data.curr_mb->intra_chroma_pred_mode = 0;
        return 0;
    }
    if (!h264_DecodeDecision(ctxIdxOffset + 3)) { // 64: 1 - 3
        app.h264.data.curr_mb->intra_chroma_pred_mode = 1;
        return 0;
    }
    if (!h264_DecodeDecision(ctxIdxOffset + 3)) { // 64: 2 - 3
        app.h264.data.curr_mb->intra_chroma_pred_mode = 2;
        return 0;
    }
    app.h264.data.curr_mb->intra_chroma_pred_mode = 3;
    return 0;

error:
    return -1;
}

//9.3.3.1.1.4 Derivation process of ctxIdxInc for the syntax element coded_block_pattern
int h264_cabac_derive_coded_block_pattern_luma(struct app_state_t* app,
    int xD, int yD,
    int binIdx,
    unsigned* condTermFlagN)
{
    int MbAddrN = -1;
    int luma8x8BlkIdxN = -1;
    CALL(
        h264_get_neighbouring_8x8luma(binIdx, xD, yD, &MbAddrN, &luma8x8BlkIdxN),
        error
    );

    struct h264_macroblock_t *mb_addr_n = h264_is_MbAddr_available(MbAddrN)?
        &app.h264.data.macroblocks[MbAddrN]:
        NULL;

    if (
        mb_addr_n == NULL ||
        mb_addr_n->mb_type == H264_U_I_PCM ||
        (
            mb_addr_n != app.h264.data.curr_mb &&
            (
                mb_addr_n->mb_type == H264_U_P_Skip ||
                mb_addr_n->mb_type == H264_U_B_Skip
            ) && 
            (mb_addr_n->CodedBlockPatternLuma >> luma8x8BlkIdxN & 1) != 0
        ) ||
        (
            mb_addr_n == app.h264.data.curr_mb && 
            mb_addr_n->CodedBlockPatternLuma != 0
        )
    ) {
        *condTermFlagN = 0;
    }
    else {
        *condTermFlagN = 1;
    }
    return 0;

error:
    return -1;    
}

// 9.3.2.6: FL binarization of CodedBlockPatternLuma with cMax = 15
//(fixedLength = Ceil(Log2(cMax + 1)) -> fixedLength = 4)
int h264_cabac_read_coded_block_pattern_luma(struct app_state_t* app)
{
    UNCOVERED_CASE(app.h264.header.slice_type, !=, SliceTypeI);

    //Table 9-11 – Association of ctxIdx and syntax elements for each slice type in
    //the initialization process coded_block_pattern (luma) 73..76
    int ctxIdxOffset = 73;

    int fixedLength = 4;
    for (int binIdx = 0; binIdx < fixedLength; binIdx++) {
        unsigned condTermFlagA;
        CALL(
            h264_cabac_derive_coded_block_pattern_luma(app,
                -1, 0,
                binIdx,
                &condTermFlagA),
            error
        );

        unsigned condTermFlagB;
        CALL(
            h264_cabac_derive_coded_block_pattern_luma(app,
                0, -1,
                binIdx,
                &condTermFlagB),
            error
        );

        int ctxIdxInc = condTermFlagA + 2 * condTermFlagB;
        // fprintf(stderr,
        //     "INFO: h264_cabac_read_coded_block_pattern_luma: binIdx(%d) ctxIdxInc(%d)\n",
        //     binIdx,
        //     ctxIdxInc);
   
        unsigned bin = h264_DecodeDecision(ctxIdxOffset + ctxIdxInc);
        app.h264.data.curr_mb->CodedBlockPatternLuma |= bin << binIdx;
    }
    return 0;

error:
    return -1;
}

//9.3.3.1.1.4 Derivation process of ctxIdxInc for the syntax element coded_block_pattern
// TODO: luma8x8BlkIdxN is not used
int h264_cabac_derive_coded_block_pattern_chroma(
    struct app_state_t* app,
    int xD, int yD,
    int binIdx,
    unsigned* condTermFlagN)
{
    int MbAddrN = -1;
    int luma8x8BlkIdxN = -1;
    CALL(h264_get_neighbouring_8x8luma(binIdx, xD, yD, &MbAddrN, &luma8x8BlkIdxN),
        error);

    struct h264_macroblock_t *mb_addr_n = h264_is_MbAddr_available(MbAddrN)?
        &app.h264.data.macroblocks[MbAddrN]:
        NULL;

    if (
        mb_addr_n != NULL && 
        mb_addr_n->mb_type == H264_U_I_PCM
    ) {
        *condTermFlagN = 1;
    }
    else if (
        mb_addr_n == NULL ||
        (
            mb_addr_n->mb_type == H264_U_P_Skip ||
            mb_addr_n->mb_type == H264_U_B_Skip
        ) ||
        (binIdx == 0 && mb_addr_n->CodedBlockPatternChroma == 0) ||
        (binIdx == 1 && mb_addr_n->CodedBlockPatternChroma != 2)
    ) {
        *condTermFlagN = 0;
    }
    else {
        *condTermFlagN = 1;
    }
    return 0;

error:
    return -1;
}

// 9.3.2.6: TU binarization of CodedBlockPatternChroma with cMax = 2
int h264_cabac_read_coded_block_pattern_chroma(struct app_state_t* app)
{
    UNCOVERED_CASE(app.h264.header.slice_type, !=, SliceTypeI);

    //Table 9-11 – Association of ctxIdx and syntax elements for each slice type in
    //the initialization process coded_block_pattern (chroma) 77..84
    int ctxIdxOffset = 77;

    int binIdx = 0;
    while (app.h264.data.curr_mb->CodedBlockPatternChroma < 2) {
        
        unsigned condTermFlagA;
        CALL(
            h264_cabac_derive_coded_block_pattern_chroma(app,
                -1, 0,
                binIdx,
                &condTermFlagA),
            error
        );

        unsigned condTermFlagB;
        CALL(
            h264_cabac_derive_coded_block_pattern_chroma(app,
                0, -1,
                binIdx,
                &condTermFlagB),
            error
        );

        int ctxIdxInc = condTermFlagA + 2 * condTermFlagB + (binIdx == 1? 4: 0);
        // fprintf(stderr,
        //     "INFO: h264_cabac_read_coded_block_pattern_chroma: binIdx(%d) ctxIdxInc(%d)\n",
        //     binIdx,
        //     ctxIdxInc);

        int bin = h264_DecodeDecision(ctxIdxOffset + ctxIdxInc);
        app.h264.data.curr_mb->CodedBlockPatternChroma |= bin << binIdx;
        if (bin == 0) {
            break;
        }
        binIdx++;
    }
    return 0;

error:
    return -1;
}

// 9.3.2.6: Binarization process for coded block pattern
int h264_cabac_read_coded_block_pattern()
{
    CALL(h264_cabac_read_coded_block_pattern_luma(), error);
    if (app.h264.sps.ChromaArrayType != 0 || app.h264.sps.ChromaArrayType != 3) {
        CALL(h264_cabac_read_coded_block_pattern_chroma(), error);
    }

    app.h264.data.curr_mb->coded_block_pattern =
        app.h264.data.curr_mb->CodedBlockPatternLuma |
        (app.h264.data.curr_mb->CodedBlockPatternChroma << 4);
    return 0;

error:
    return -1;
}

int h264_cabac_read_mb_qp_delta(struct app_state_t* app)
{
    // Table 9-11 Association of ctxIdx and syntax elements for each slice
    // type in the initialization process
    // 60..63
    // Table 9-34 Syntax elements and associated types of binarization
    // ctxIdxOffset 60
    // maxBinIdxCtx 2
    int ctxIdxOffset = 60;

    // 9.3.3.1.1.5 Derivation process of ctxIdxInc for the syntax element mb_qp_delta
    struct h264_macroblock_t *prev_mb = h264_is_MbAddr_available(app.h264.data.PrevMbAddr)?
        &app.h264.data.macroblocks[app.h264.data.PrevMbAddr]:
        NULL;

    int ctxIdxInc = 
    (
        prev_mb == NULL || 
        prev_mb->mb_type == H264_U_P_Skip ||
        prev_mb->mb_type == H264_U_B_Skip ||
        prev_mb->mb_type == H264_U_I_PCM ||
        (
            prev_mb->MbPartPredMode != H264_Intra_16x16 &&
            prev_mb->CodedBlockPatternLuma == 0 &&
            prev_mb->CodedBlockPatternChroma == 0
        ) ||
        prev_mb->mb_qp_delta == 0
    )? 0: 1;

    // fprintf(stderr,
    //     "INFO: h264_cabac_read_mb_qp_delta: ctxIdxInc(%d)\n",
    //     ctxIdxInc);

    // 9.3.2.7 Binarization process for mb_qp_delta, U binarization
    // Table 9-39 Assignment of ctxIdxInc to binIdx for all ctxIdxOffset values - 
    int bin = h264_DecodeDecision(ctxIdxOffset + ctxIdxInc);
    if (bin != 0) {
        bin += h264_DecodeDecision(ctxIdxOffset + 2);
    }

    // signed value of mb_qp_delta and its mapped value is given as specified in Table 9-3.
    if (bin == 1) {
        app.h264.data.curr_mb->mb_qp_delta = 1;
    }
    else if (bin == 2) {
        app.h264.data.curr_mb->mb_qp_delta = -1;
    } else {
        app.h264.data.curr_mb->mb_qp_delta = bin;
    }
    return 0;
}

//Table 9-42 - Specification of ctxBlockCat for the different block
int h264_cabac_coded_block_flag_ctxBlockCat(struct app_state_t* app,
    int type,
    int* maxNumCoeff,
    int* ctxBlockCat)
{
    if (type == LUMA_16DC) {
        *maxNumCoeff = 16;
        *ctxBlockCat = 0;
    }
    else if (type == LUMA_16AC) {
        *maxNumCoeff = 15;
        *ctxBlockCat = 1;
    }
    else if (type == LUMA_4x4) {
        *maxNumCoeff = 16;
        *ctxBlockCat = 2;
    }
    else if (type == CHROMA_DC) {
        UNCOVERED_CASE(app.h264.sps.ChromaArrayType, ==, 3);

        int SubWidthC = -1;
        int SubHeightC = -1;
        CALL(
            h264_get_chroma_variables(&SubWidthC, &SubHeightC),
            error
        );
        int NumC8x8 = 4 / (SubWidthC * SubHeightC);

        *maxNumCoeff = 4 * NumC8x8;
        *ctxBlockCat = 3;
    }
    else if (type == CHROMA_AC) {
        UNCOVERED_CASE(app.h264.sps.ChromaArrayType, ==, 3);
        *maxNumCoeff = 15;
        *ctxBlockCat = 4;
    }
    else if (type == LUMA_8x8) {
        *maxNumCoeff = 64;
        *ctxBlockCat = 5;
    }
    else if (type == CB_16DC) {
        UNCOVERED_CASE(app.h264.sps.ChromaArrayType, !=, 3);
        *maxNumCoeff = 16;
        *ctxBlockCat = 6;        
    }
    else if (type == CB_16AC) {
        UNCOVERED_CASE(app.h264.sps.ChromaArrayType, !=, 3);
        *maxNumCoeff = 15;
        *ctxBlockCat = 7;        
    }
    else if (type == CB_4x4) {
        UNCOVERED_CASE(app.h264.sps.ChromaArrayType, !=, 3);
        *maxNumCoeff = 16;
        *ctxBlockCat = 8;        
    }
    else if (type == CB_8x8) {
        UNCOVERED_CASE(app.h264.sps.ChromaArrayType, !=, 3);
        *maxNumCoeff = 64;
        *ctxBlockCat = 9;        
    }
    else if (type == CR_16DC) {
        UNCOVERED_CASE(app.h264.sps.ChromaArrayType, !=, 3);
        *maxNumCoeff = 16;
        *ctxBlockCat = 10;        
    }
    else if (type == CR_16AC) {
        UNCOVERED_CASE(app.h264.sps.ChromaArrayType, !=, 3);
        *maxNumCoeff = 15;
        *ctxBlockCat = 11;        
    }
    else if (type == CR_4x4) {
        UNCOVERED_CASE(app.h264.sps.ChromaArrayType, !=, 3);
        *maxNumCoeff = 16;
        *ctxBlockCat = 12;        
    }
    else if (type == CR_8x8) {
        UNCOVERED_CASE(app.h264.sps.ChromaArrayType, !=, 3);
        *maxNumCoeff = 64;
        *ctxBlockCat = 13;        
    }
    else {
        return -1;
    }
    return 0;

error:
    return -1;
}

// ctxBlockCat==3 CHROMA_DC
int h264_cabac_derive_coded_block_flag_chroma_dc(
    struct app_state_t* app,
    int xD, int yD,
    struct h264_coeff_level_t* chroma_blocks,
    int* condTermFlagN)
{
    //6.4.11.1 Derivation process for neighbouring macroblocks
    int MbAddrN = -1;
    CALL(
        h264_get_neighbouring_macroblocks(xD, yD, &MbAddrN),
        error
    );

    struct h264_macroblock_t *mb_addr_n = h264_is_MbAddr_available(MbAddrN)?
        &app.h264.data.macroblocks[MbAddrN]:
        NULL;

    struct h264_coeff_level_t* transBlockN = NULL;
    if (
        mb_addr_n != NULL &&
        mb_addr_n->mb_type != H264_U_I_PCM &&
        mb_addr_n->mb_type != H264_U_P_Skip &&
        mb_addr_n->mb_type != H264_U_B_Skip &&
        mb_addr_n->CodedBlockPatternChroma != 0
    ) {
        int offset = (char*)chroma_blocks - (char*)app.h264.data.curr_mb;
        transBlockN = (struct h264_coeff_level_t*)((char*)mb_addr_n + offset);
    }

    //9.3.3.1.1.9 Derivation process of ctxIdxInc for the syntax element coded_block_flag
    if (
        (mb_addr_n == NULL && h264_is_inter_pred_mode(app.h264.data.curr_mb)) ||
        (
            mb_addr_n != NULL &&
            transBlockN == NULL &&
            mb_addr_n->mb_type != H264_U_I_PCM
        ) ||
        (
            h264_is_intra_pred_mode(app.h264.data.curr_mb) &&
            app.h264.pps.constrained_intra_pred_flag == 1 &&
            mb_addr_n != NULL &&
            h264_is_inter_pred_mode(mb_addr_n) &&
            (
                app.h264.nal_unit_type == NAL_UNIT_TYPE_CODED_SLICE_DATA_PARTITION_A ||
                app.h264.nal_unit_type == NAL_UNIT_TYPE_CODED_SLICE_DATA_PARTITION_B ||
                app.h264.nal_unit_type == NAL_UNIT_TYPE_CODED_SLICE_DATA_PARTITION_C
            )
        )
    ) {
        *condTermFlagN = 0;
    }
    else if (
        (mb_addr_n == NULL && h264_is_intra_pred_mode(app.h264.data.curr_mb)) ||
        mb_addr_n->mb_type == H264_U_I_PCM
    ) {
        *condTermFlagN = 1;
    }
    else {
        *condTermFlagN = transBlockN->coded_block_flag;
    }
    return 0;
error:
    return -1;
}

// ctxBlockCat==4 CHROMA_AC
int h264_cabac_derive_coded_block_flag_chroma_ac(
    struct app_state_t* app,
    int xD, int yD,
    struct h264_coeff_level_t* chroma_blocks,
    int chroma4x4BlkIdx,
    int* condTermFlagN)
{
    //6.4.11.1 Derivation process for neighbouring macroblocks
    int MbAddrN = -1;
    int chroma4x4BlkIdxN = -1;
    CALL(
        h264_get_neighbouring_4x4chroma(chroma4x4BlkIdx, xD, yD, &MbAddrN, &chroma4x4BlkIdxN),
        error
    );
    UNCOVERED_CASE(chroma4x4BlkIdxN, >=, 4);

    struct h264_macroblock_t *mb_addr_n = h264_is_MbAddr_available(MbAddrN)?
        &app.h264.data.macroblocks[MbAddrN]:
        NULL;

    struct h264_coeff_level_t* transBlockN = NULL;
    if (
        mb_addr_n != NULL &&
        mb_addr_n->mb_type != H264_U_I_PCM &&
        mb_addr_n->mb_type != H264_U_P_Skip &&
        mb_addr_n->mb_type != H264_U_B_Skip &&
        mb_addr_n->CodedBlockPatternChroma == 2
    ) {
        int offset = (char*)chroma_blocks - (char*)app.h264.data.curr_mb;
        transBlockN = (struct h264_coeff_level_t*)((char*)mb_addr_n + offset) + chroma4x4BlkIdxN;
    }

    //9.3.3.1.1.9 Derivation process of ctxIdxInc for the syntax element coded_block_flag
    if (
        (mb_addr_n == NULL && h264_is_inter_pred_mode(app.h264.data.curr_mb)) ||
        (
            mb_addr_n != NULL &&
            transBlockN == NULL &&
            mb_addr_n->mb_type != H264_U_I_PCM
        ) ||
        (
            h264_is_intra_pred_mode(app.h264.data.curr_mb) &&
            app.h264.pps.constrained_intra_pred_flag == 1 &&
            mb_addr_n != NULL &&
            h264_is_inter_pred_mode(mb_addr_n) &&
            (
                app.h264.nal_unit_type == NAL_UNIT_TYPE_CODED_SLICE_DATA_PARTITION_A ||
                app.h264.nal_unit_type == NAL_UNIT_TYPE_CODED_SLICE_DATA_PARTITION_B ||
                app.h264.nal_unit_type == NAL_UNIT_TYPE_CODED_SLICE_DATA_PARTITION_C
            )
        )
    ) {
        *condTermFlagN = 0;
    }
    else if (
        (mb_addr_n == NULL && h264_is_intra_pred_mode(app.h264.data.curr_mb)) ||
        mb_addr_n->mb_type == H264_U_I_PCM
    ) {
        *condTermFlagN = 1;
    }
    else {
        *condTermFlagN = transBlockN->coded_block_flag;
    }
    return 0;
error:
    return -1;
}

int h264_cabac_read_coded_block_flag(struct app_state_t* app,
    int type,
    struct h264_coeff_level_t* blocks,
    int blkIdx)
{
    UNCOVERED_CASE(app.h264.header.slice_type, !=, SliceTypeI);

    int maxNumCoeff = -1;
    int ctxBlockCat = -1;
    CALL(
        h264_cabac_coded_block_flag_ctxBlockCat(type, &maxNumCoeff, &ctxBlockCat),
        error
    );

    // Table 9-11 Association of ctxIdx and syntax elements for each slice
    // type in the initialization process
    // 85..104, 460..483, 1012..1023
    // Table 9-34 Syntax elements and associated types of binarization
    // Type of binarization - FL, cMax=1, maxBinIdxCtx = 0
    UNCOVERED_CASE(ctxBlockCat, <, 0);
    UNCOVERED_CASE(ctxBlockCat, >, 13);
    int ctxIdxOffset = -1;
    if (ctxBlockCat < 5) {
        ctxIdxOffset = 85;
    }
    else if (ctxBlockCat > 5 && ctxBlockCat < 9) {
        ctxIdxOffset = 460;
    }
    else if (ctxBlockCat > 9 && ctxBlockCat < 13) {
        ctxIdxOffset = 472;
    }
    else if (ctxBlockCat == 5 || ctxBlockCat == 9 || ctxBlockCat == 13) {
        ctxIdxOffset = 1012;
    }
    UNCOVERED_CASE(ctxIdxOffset, <, 0);

    int ctxIdxInc = -1;
    int condTermFlagA = -1, condTermFlagB = -1;
    //9.3.3.1.1.9 Derivation process of ctxIdxInc for the syntax element coded_block_flag
    if (ctxBlockCat == 3) { //CHROMA_DC - ChromaDCLevel as described in clause 7.4.5.3
        //6.4.11.1 Derivation process for neighbouring macroblocks
        CALL(
            h264_cabac_derive_coded_block_flag_chroma_dc(app,
                -1, 0,
                blocks + blkIdx,
                &condTermFlagA),
            error
        );
        CALL(
            h264_cabac_derive_coded_block_flag_chroma_dc(app,
                0, -1,
                blocks + blkIdx,
                &condTermFlagB),
            error
        );
    } else {
        //6.4.11.1 Derivation process for neighbouring macroblocks
        CALL(
            h264_cabac_derive_coded_block_flag_chroma_ac(app,
                -1, 0,
                blocks, blkIdx,
                &condTermFlagA),
            error
        );
        CALL(
            h264_cabac_derive_coded_block_flag_chroma_ac(app,
                0, -1,
                blocks, blkIdx,
                &condTermFlagB),
            error
        );
    }

    UNCOVERED_CASE(condTermFlagA, <, 0);
    UNCOVERED_CASE(condTermFlagB, <, 0);
    ctxIdxInc = condTermFlagA + 2 * condTermFlagB;
    blocks[blkIdx].coded_block_flag = h264_DecodeDecision(ctxIdxOffset + ctxIdxInc);
    return 0;

error:
    return -1;
}

int h264_cabac_read_significant_coeff_flag(struct app_state_t* app,
    int type,
    struct h264_coeff_level_t* coeffLevel,
    int levelListIdx)
{
    UNCOVERED_CASE(coeffLevel->maxNumCoeff, <=,  levelListIdx);

    int maxNumCoeff = -1;
    int ctxBlockCat = -1;
    CALL(
        h264_cabac_coded_block_flag_ctxBlockCat(type, &maxNumCoeff, &ctxBlockCat),
        error
    );

    // Table 9-11 Association of ctxIdx and syntax elements for each slice
    // type in the initialization process
    // 105..165, 277..337, 402..416, 436..450, 484..571, 776..863, 660..689,
    // 718..747
    // Table 9-34 Syntax elements and associated types of binarization
    // Type of binarization - FL, cMax=1
    UNCOVERED_CASE(ctxBlockCat, <, 0);
    UNCOVERED_CASE(ctxBlockCat, >, 13);
    int ctxIdxOffset = -1;
    if (ctxBlockCat < 5) {
        ctxIdxOffset = 105; //277
    }
    else if (ctxBlockCat == 5) {
        ctxIdxOffset = 402; //436
    }
    else if (ctxBlockCat > 5 && ctxBlockCat < 9) {
        ctxIdxOffset = 484; //776
    }
    else if (ctxBlockCat == 9) {
        ctxIdxOffset = 660; //675
    }
    else if (ctxBlockCat > 9 && ctxBlockCat < 13) {
        ctxIdxOffset = 528; //820
    }
    else if (ctxBlockCat == 13) {
        ctxIdxOffset = 718; //733
    }
    UNCOVERED_CASE(ctxIdxOffset, <, 0);

    // 9.3.3.1.3 Assignment  process  of  ctxIdxInc  for  syntax elements  significant_coeff_flag,
    // last_significant_coeff_flag, and coeff_abs_level_minus1
    int ctxIdxInc = -1;
    if (ctxBlockCat == 5 || ctxBlockCat == 9 || ctxBlockCat == 13) {
        ctxIdxInc = significant_coeff_flag_ctxIdxInc[levelListIdx];
    }
    else if (ctxBlockCat == 3) {
        int SubWidthC = -1;
        int SubHeightC = -1;
        CALL(
            h264_get_chroma_variables(&SubWidthC, &SubHeightC),
            error
        );
        int NumC8x8 = 4 / (SubWidthC * SubHeightC);

        ctxIdxInc = h264_min(levelListIdx / NumC8x8, 2);
    }
    else {
        ctxIdxInc = levelListIdx;
    }
    UNCOVERED_CASE(ctxIdxInc, <, 0);

    coeffLevel->coeff[levelListIdx].significant_coeff_flag = 
        h264_DecodeDecision(ctxIdxOffset + ctxIdxInc);
    return 0;

error:
    return -1;
}

int h264_cabac_read_last_significant_coeff_flag(struct app_state_t* app,
    int type,
    struct h264_coeff_level_t* coeffLevel,
    int levelListIdx)
{
    UNCOVERED_CASE(coeffLevel->maxNumCoeff, <=,  levelListIdx);

    int maxNumCoeff = -1;
    int ctxBlockCat = -1;
    CALL(
        h264_cabac_coded_block_flag_ctxBlockCat(type, &maxNumCoeff, &ctxBlockCat),
        error
    );

    // Table 9-11 Association of ctxIdx and syntax elements for each slice
    // type in the initialization process
    // 166..226 338..398 417..425 451..459 572..659 864..951 690..707
    // 748..765
    // Table 9-34 Syntax elements and associated types of binarization
    // Type of binarization - FL, cMax=1
    UNCOVERED_CASE(ctxBlockCat, <, 0);
    UNCOVERED_CASE(ctxBlockCat, >, 13);
    int ctxIdxOffset = -1;
    if (ctxBlockCat < 5) {
        ctxIdxOffset = 166; //338
    }
    else if (ctxBlockCat == 5) {
        ctxIdxOffset = 417; //451
    }
    else if (ctxBlockCat > 5 && ctxBlockCat < 9) {
        ctxIdxOffset = 572; //864
    }
    else if (ctxBlockCat == 9) {
        ctxIdxOffset = 690; //699
    }
    else if (ctxBlockCat > 9 && ctxBlockCat < 13) {
        ctxIdxOffset = 616; //908
    }
    else if (ctxBlockCat == 13) {
        ctxIdxOffset = 748; //757
    }
    UNCOVERED_CASE(ctxIdxOffset, <, 0);

    // 9.3.3.1.3 Assignment  process  of  ctxIdxInc  for  syntax elements  significant_coeff_flag,
    // last_significant_coeff_flag, and coeff_abs_level_minus1
    int ctxIdxInc = -1;
    if (ctxBlockCat == 5 || ctxBlockCat == 9 || ctxBlockCat == 13) {
        ctxIdxInc = last_significant_coeff_flag_ctxIdxInc[levelListIdx];
    }
    else if (ctxBlockCat == 3) {
        int SubWidthC = -1;
        int SubHeightC = -1;
        CALL(
            h264_get_chroma_variables(&SubWidthC, &SubHeightC),
            error
        );
        int NumC8x8 = 4 / (SubWidthC * SubHeightC);

        ctxIdxInc = h264_min(levelListIdx / NumC8x8, 2);
    }
    else {
        ctxIdxInc = levelListIdx;
    }
    UNCOVERED_CASE(ctxIdxInc, <, 0);

    coeffLevel->coeff[levelListIdx].last_significant_coeff_flag =
        h264_DecodeDecision(ctxIdxOffset + ctxIdxInc);
    return 0;

error:
    return -1;
}

int h264_cabac_read_coeff_abs_level_minus1(struct app_state_t* app,
    int type,
    struct h264_coeff_level_t* coeffLevel,
    int levelListIdx)
{
    UNCOVERED_CASE(coeffLevel->maxNumCoeff, <=,  levelListIdx);

    int maxNumCoeff = -1;
    int ctxBlockCat = -1;
    CALL(
        h264_cabac_coded_block_flag_ctxBlockCat(type, &maxNumCoeff, &ctxBlockCat),
        error
    );

    // Table 9-11 Association of ctxIdx and syntax elements for each slice
    // type in the initialization process
    // 227..275 426..435 952..1011 708..717 766..775
    // Table 9-34 Syntax elements and associated types of binarization
    // Type of binarization - prefix and suffix as given by UEG0 with signedValFlag=0, uCoff=14,
    // prefix: 1, suffix: na, (uses DecodeBypass)
    // prefix: TU with cMax = uCoff
    UNCOVERED_CASE(ctxBlockCat, <, 0);
    UNCOVERED_CASE(ctxBlockCat, >, 13);
    int ctxIdxOffset = -1;
    if (ctxBlockCat < 5) {
        ctxIdxOffset = 227;
    }
    else if (ctxBlockCat == 5) {
        ctxIdxOffset = 426;
    }
    else if (ctxBlockCat > 5 && ctxBlockCat < 9) {
        ctxIdxOffset = 952;
    }
    else if (ctxBlockCat == 9) {
        ctxIdxOffset = 708;
    }
    else if (ctxBlockCat > 9 && ctxBlockCat < 13) {
        ctxIdxOffset = 982;
    }
    else if (ctxBlockCat == 13) {
        ctxIdxOffset = 766;
    }
    UNCOVERED_CASE(ctxIdxOffset, <, 0);

    //9.3.2.3 Concatenated unary/ k-th order Exp-Golomb (UEGk) binarization process
    int prefix = 0;
    while (prefix <= 14) {
        // 9.3.3.1.3 Assignment  process  of  ctxIdxInc  for  syntax elements
        // significant_coeff_flag, last_significant_coeff_flag, and coeff_abs_level_minus1
        int ctxIdxInc;
        if (prefix == 0) {
            ctxIdxInc = ((coeffLevel->numDecodAbsLevelGt1 != 0)?
                0:
                h264_min(4, 1 + coeffLevel->numDecodAbsLevelEq1)); //(9-23)
        } else {
            ctxIdxInc = 5 + h264_min(
                4 - (ctxBlockCat == 3? 1: 0),
                coeffLevel->numDecodAbsLevelGt1
            ); //(9-24)
        }
        UNCOVERED_CASE(ctxIdxInc, <, 0);

        if (!h264_DecodeDecision(ctxIdxOffset + ctxIdxInc)) {
            break;
        }
        prefix++;
    }
    if (prefix < 15) {
        coeffLevel->coeff[levelListIdx].coeff_abs_level_minus1 = prefix;
        return 0;
    }
    DEBUG_MSG("ERROR: TO IMPLEMENT ctxBlockCat %d, type %d, %s - %s:%d\n",
        ctxBlockCat, type, __FILE__, __FUNCTION__, __LINE__);
    return -1;

error:
    return -1;
}

int h264_cabac_read_coeff_sign_flag(struct app_state_t* app,
    struct h264_coeff_level_t* coeffLevel,
    int levelListIdx)
{
    UNCOVERED_CASE(coeffLevel->maxNumCoeff, <=,  levelListIdx);
    // Table 9-11 Association of ctxIdx and syntax elements for each slice
    // type in the initialization process
    // na
    // Table 9-34 Syntax elements and associated types of binarization
    // Type of binarization FL, cMax=1 uses DecodeBypass
    coeffLevel->coeff[levelListIdx].coeff_sign_flag = h264_DecodeBypass();
    return 0;
}

int h264_cabac_end_of_slice_flag(struct app_state_t* app)
{
    // Table 9-34 Syntax elements and associated types of binarization, maxBinIdxCtx
    // and ctxIdxOffset, Type of binarizationm FL, cMax=1, ctxIdxOffset=276

    app.h264.data.curr_mb->end_of_slice_flag = h264_DecodeTerminate();
    return 0;
}
