static const char* h264_get_nal_unit_type(int nal_unit_type)
{
    switch (nal_unit_type) {
        case NAL_UNIT_TYPE_CODED_SLICE_NON_IDR:
            return "Coded slice of a non-IDR picture";

        case NAL_UNIT_TYPE_CODED_SLICE_IDR:
            return "Coded slice of an IDR picture (it is used for reference)";

        case NAL_UNIT_TYPE_SPS:
            return "Sequence parameter set";

        case NAL_UNIT_TYPE_PPS:
            return "Picture parameter set";

        default:
            return "Unknown";
    };
}

static const char* h264_get_nal_ref_idc(int nal_ref_idc)
{
    switch (nal_ref_idc) {
        case NAL_REF_IDC_PRIORITY_LOW:
            return "NAL_REF_IDC_PRIORITY_LOW";

        default:
            return "Unknown";
    };
}

static const char* h264_get_slice_type(int slice_type)
{
    if (slice_type == SliceTypeP) {
        return "P (0)";
    }
    else if (slice_type == SliceTypeB) {
        return "B (1)";
    }
    else if (slice_type == SliceTypeI) {
        return "I (2)";
    }
    else if (slice_type == SliceTypeSP) {
        return "SP (3)";
    }
    else if (slice_type == SliceTypeSI) {
        return "SI (4)";
    }
    else if (slice_type == SliceTypePOnly) {
        return "P (5)";
    }
    else if (slice_type == SliceTypeBOnly) {
        return "B (6)";
    }
    else if (slice_type == SliceTypeIOnly) {
        return "I (7)";
    }
    else if (slice_type == SliceTypeSPOnly) {
        return "SP (8)";
    }
    else if (slice_type == SliceTypeSIOnly) {
        return "SI (9)";
    }
    return "Unknown";
}


inline int h264_clip(int x, int y, int z)
{
    if (z < x) {
        return x;
    } else if (z > y) {
        return y;
    } else {
        return z;
    }
}

inline int h264_max(int x, int y)
{
    return (x > y)? x: y;
}

inline int h264_min(int x, int y)
{
    return (x < y)? x: y;
}

#ifdef ENABLE_H264_SLICE
static int h264_InverseRasterScan(int a, int b, int c, int d, int e) //(5-11)
{
    if (e == 0) {
        return (a % (d / b)) * b;
    }
    else if (e == 1) {
        return (a / (d / b)) * c;
    }
    UNCOVERED_CASE(e, !=, 0);
    UNCOVERED_CASE(e, !=, 1);
    return -1;
} 

//Table 7-11 - Macroblock types for I slices
static int h264_MbPartPredMode(struct h264_macroblock_t *mb)
{
    if (mb->mb_type == H264_U_I_NxN) {
        mb->MbPartPredMode = (mb->transform_size_8x8_flag == 1)?
            H264_Intra_8x8: H264_Intra_4x4;
            return 0;
    }
    else if (mb->mb_type == H264_U_I_PCM) {
        UNCOVERED_CASE(mb->mb_type, ==, H264_U_I_PCM);
        return -1;
    }
    else if ((mb->mb_type & H264_U_I) == H264_U_I) {
        mb->MbPartPredMode = H264_Intra_16x16;
        return 0;
    }
    fprintf(stderr, "ERROR: TO IMPLEMENT h264_MbPartPredMode for other slices %d, %s - %s:%d\n",
        mb->mb_type, __FILE__, __FUNCTION__, __LINE__);
    return -1;
}

int h264_NumMbPart(int mb_type) {
    fprintf(stderr, "ERROR: TO IMPLEMENT h264_MbPartPredMode for other slices %d, %s - %s:%d\n",
        mb_type, __FILE__, __FUNCTION__, __LINE__);
    return -1;
}

//Table 7-14 - Macroblock type values for B slices
static int h264_is_inter_pred_mode(struct h264_macroblock_t *mb)
{
    if ((mb->mb_type & H264_U_B) == H264_U_B) {
        return 1;
    }
    return 0;
}

//Table 7-11 - Macroblock types for I slices
static int h264_is_intra_pred_mode(struct h264_macroblock_t *mb)
{
    if ((mb->mb_type & H264_U_I) == H264_U_I) {
        return 1;
    }
    return 0;
}

// Table 6-1 – SubWidthC, and SubHeightC values derived from chroma_format_idc
static int h264_get_chroma_variables(
    struct app_state_t *app,
    int* SubWidthC,
    int* SubHeightC)
{
    if (app->h264.sps.chroma_format_idc == CHROMA_FORMAT_YUV400) {
        *SubWidthC = -1;
        *SubHeightC = -1;
        return 0;
    }
    else if (app->h264.sps.chroma_format_idc == CHROMA_FORMAT_YUV420) {
        *SubWidthC = 2;
        *SubHeightC = 2;
        return 0;
    }
    else if (app->h264.sps.chroma_format_idc == CHROMA_FORMAT_YUV422) {
        *SubWidthC = 2;
        *SubHeightC = 1;
        return 0;
    }
    else if (app->h264.sps.chroma_format_idc == CHROMA_FORMAT_YUV444) {
        if (!app->h264.sps.separate_colour_plane_flag) {
            *SubWidthC = 1;
            *SubHeightC = 1;
            return 0;
        } else {
            *SubWidthC = -1;
            *SubHeightC = -1;
            return 0;
        }
    }
    UNCOVERED_CASE(app->h264.sps.chroma_format_idc, !=, CHROMA_FORMAT_YUV400);
    UNCOVERED_CASE(app->h264.sps.chroma_format_idc, !=, CHROMA_FORMAT_YUV420);
    UNCOVERED_CASE(app->h264.sps.chroma_format_idc, !=, CHROMA_FORMAT_YUV422);
    UNCOVERED_CASE(app->h264.sps.chroma_format_idc, !=, CHROMA_FORMAT_YUV444);
    return -1;
}

// 6.4.8 Derivation process of the availability for macroblock addresses
unsigned h264_is_MbAddr_available(struct app_state_t* app, int mbAddr)
{
    if (mbAddr < 0) {
        return 0;
    }
    if (mbAddr > app->h264.data.CurrMbAddr) {
        return 0;
    }
    return 1;
}

// 6.4.9 Derivation process for neighbouring macroblock addresses and their availability
int h264_MbAddrA(struct app_state_t* app)
{
    struct h264_slice_header_t* header = LINKED_HASH_GET_HEAD(app->h264.headers);
    UNCOVERED_CASE(header->MbaffFrameFlag, !=, 0);

    int mbAddrA = app->h264.data.CurrMbAddr - 1;
    if (h264_is_MbAddr_available(app, mbAddrA)) {
        return (app->h264.data.CurrMbAddr % header->PicWidthInMbs != 0)? mbAddrA: -1;
    }
    else {
        return -1;
    }
}

// 6.4.9 Derivation process for neighbouring macroblock addresses and their availability
int h264_MbAddrB(struct app_state_t* app)
{
    struct h264_slice_header_t* header = LINKED_HASH_GET_HEAD(app->h264.headers);
    UNCOVERED_CASE(header->MbaffFrameFlag, !=, 0);

    int mbAddrB = app->h264.data.CurrMbAddr - header->PicWidthInMbs;
    if (h264_is_MbAddr_available(app, mbAddrB)) {
        return mbAddrB;
    }
    else {
        return -1;
    }
}

// 6.4.9 Derivation process for neighbouring macroblock addresses and their availability
int h264_MbAddrC(struct app_state_t* app)
{
    struct h264_slice_header_t* header = LINKED_HASH_GET_HEAD(app->h264.headers);
    UNCOVERED_CASE(header->MbaffFrameFlag, !=, 0);

    int mbAddrC = app->h264.data.CurrMbAddr - header->PicWidthInMbs + 1;
    if (h264_is_MbAddr_available(app, mbAddrC)) {
        return ((app->h264.data.CurrMbAddr + 1) % header->PicWidthInMbs != 0)?
            mbAddrC: -1;;
    }
    else {
        return -1;
    }
}

// 6.4.9 Derivation process for neighbouring macroblock addresses and their availability
int h264_MbAddrD(struct app_state_t* app)
{
    struct h264_slice_header_t* header = LINKED_HASH_GET_HEAD(app->h264.headers);
    UNCOVERED_CASE(header->MbaffFrameFlag, !=, 0);

    int mbAddrD = app->h264.data.CurrMbAddr - header->PicWidthInMbs - 1;
    if (h264_is_MbAddr_available(app, mbAddrD)) {
        return (app->h264.data.CurrMbAddr % header->PicWidthInMbs != 0)? mbAddrD: -1;
    }
    else {
        return -1;
    }
}
#endif //ENABLE_H264_SLICE