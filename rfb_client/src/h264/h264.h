#ifndef H264_H
#define H264_H


//NAL ref idc codes
#define NAL_REF_IDC_PRIORITY_HIGHEST    3
#define NAL_REF_IDC_PRIORITY_HIGH       2
#define NAL_REF_IDC_PRIORITY_LOW        1
#define NAL_REF_IDC_PRIORITY_DISPOSABLE 0

//Table 7-1 NAL unit type codes
// Unspecified
#define NAL_UNIT_TYPE_UNSPECIFIED                    0
// Coded slice of a non-IDR picture, slice_layer_without_partitioning_rbsp()
#define NAL_UNIT_TYPE_CODED_SLICE_NON_IDR            1
// Coded slice data partition A, slice_data_partition_a_layer_rbsp()
#define NAL_UNIT_TYPE_CODED_SLICE_DATA_PARTITION_A   2
// Coded slice data partition B, slice_data_partition_b_layer_rbsp()
#define NAL_UNIT_TYPE_CODED_SLICE_DATA_PARTITION_B   3
// Coded slice data partition C, slice_data_partition_c_layer_rbsp()
#define NAL_UNIT_TYPE_CODED_SLICE_DATA_PARTITION_C   4
// Coded slice of an IDR picture, slice_layer_without_partitioning_rbsp()
#define NAL_UNIT_TYPE_CODED_SLICE_IDR                5
// Supplemental enhancement information (SEI), sei_rbsp()
#define NAL_UNIT_TYPE_SEI                            6
// Sequence parameter set, seq_parameter_set_rbsp() 
#define NAL_UNIT_TYPE_SPS                            7
// Picture parameter set, pic_parameter_set_rbsp()
#define NAL_UNIT_TYPE_PPS                            8
// Access unit delimiter, access_unit_delimiter_rbsp()
#define NAL_UNIT_TYPE_AUD                            9
// End of sequence, end_of_seq_rbsp()
#define NAL_UNIT_TYPE_END_OF_SEQUENCE               10
// End of stream, end_of_stream_rbsp()
#define NAL_UNIT_TYPE_END_OF_STREAM                 11
// Filler data, filler_data_rbsp()
#define NAL_UNIT_TYPE_FILLER                        12
                                             // 13..23    // Reserved
                                             // 24..31    // Unspecified

//--- block types for CABAC ----
#define LUMA_16DC       0
#define LUMA_16AC       1
#define LUMA_8x8        2
#define LUMA_8x4        3
#define LUMA_4x8        4
#define LUMA_4x4        5
#define CHROMA_DC       6
#define CHROMA_AC       7
#define CHROMA_DC_2x4   8
#define CHROMA_DC_4x4   9
#define CB_16DC         10
#define CB_16AC         11
#define CB_8x8          12
#define CB_8x4          13
#define CB_4x8          14
#define CB_4x4          15
#define CR_16DC         16
#define CR_16AC         17
#define CR_8x8          18
#define CR_8x4          19
#define CR_4x8          20
#define CR_4x4          21 
#define NUM_BLOCK_TYPES 22

struct h264_rbsp_t;

typedef unsigned (*h264_ae_value)(unsigned value);

#define H264_INIT() \
    inline void h264_rbsp_init(struct h264_rbsp_t* rbsp, uint8_t* p, int size) \
    { \
        rbsp->p = rbsp->start = p; \
        rbsp->end = rbsp->p + size; \
        rbsp->bits_left = 8; \
    } \
    inline void h264_rbsp_align(struct h264_rbsp_t* rbsp) \
    { \
        if (rbsp->bits_left != 8) { \
            rbsp->p++; rbsp->bits_left = 8; \
        } \
    } \
    inline int h264_is_more_rbsp(struct h264_rbsp_t* rbsp) \
    { \
        if (rbsp->bits_left == 1 && (*rbsp->p & 0b00000001) == 0b00000001) { \
            return 0; \
        } \
        else if (rbsp->bits_left == 2 && (*rbsp->p & 0b00000011) == 0b00000010) { \
            return 0; \
        } \
        else if (rbsp->bits_left == 3 && (*rbsp->p & 0b00000111) == 0b00000100) { \
            return 0; \
        } \
        else if (rbsp->bits_left == 4 && (*rbsp->p & 0b00001111) == 0b00001000) { \
            return 0; \
        } \
        else if (rbsp->bits_left == 5 && (*rbsp->p & 0b00011111) == 0b00010000) { \
            return 0; \
        } \
        else if (rbsp->bits_left == 6 && (*rbsp->p & 0b00111111) == 0b00100000) { \
            return 0; \
        } \
        else if (rbsp->bits_left == 7 && (*rbsp->p & 0b01111111) == 0b01000000) { \
            return 0; \
        } \
        else if (rbsp->bits_left == 8 && (*rbsp->p & 0b11111111) == 0b10000000) { \
            return 0; \
        } \
        return 1; \
    } \
    unsigned h264_rbsp_read_u1(struct h264_rbsp_t* rbsp) \
    { \
        rbsp->bits_left--; \
        unsigned r = ((*rbsp->p) >> rbsp->bits_left) & 0x01; \
        if (rbsp->bits_left == 0) { \
            rbsp->p++; rbsp->bits_left = 8; \
        } \
        if (rbsp->p > rbsp->end) { \
            fprintf(stderr, "ERROR: %s, rbsp is out of bits", __FUNCTION__); \
        } \
        return r; \
    } \
    unsigned h264_rbsp_read_un(struct h264_rbsp_t* rbsp, int n) \
    { \
        unsigned r = 0; \
        for (int i = 0; i < n; i++) { \
            r |= (RBSP_READ_U1(rbsp) << ( n - i - 1 )); \
        } \
        return r; \
    } \
    unsigned h264_rbsp_read_ue(struct h264_rbsp_t* rbsp) \
    { \
        int i = 0;\
        while ((RBSP_READ_U1(rbsp) == 0) && (i < 32) && (!RBSP_EOF(rbsp))) { \
            i++; \
        } \
        unsigned r = h264_rbsp_read_un(rbsp, i); \
        r += (1 << i) - 1; \
        return r; \
    } \
    int h264_rbsp_read_se(struct h264_rbsp_t* rbsp) \
    { \
        unsigned r = h264_rbsp_read_ue(rbsp); \
        if (r & 0x01) { \
            return (r + 1) / 2; \
        } \
        else { \
            return -(r / 2); \
        } \
    }

#define RBSP_INIT(rbsp, p, size) ( \
    h264_rbsp_init(rbsp, p, size) \
)

#define RBSP_EOF(rbsp) ( \
    ((rbsp)->p >= (rbsp)->end)? 1: 0 \
)

#define RBSP_IS_ALLIGN(rbsp) ( \
    ((rbsp)->bits_left == 8)? 1: 0 \
)

#define RBSP_IS_MORE(rbsp) ( \
    h264_is_more_rbsp(rbsp) \
)

#define RBSP_ALLIGN(rbsp) ( \
    h264_rbsp_align(rbsp) \
)

#define RBSP_READ_U1(rbsp) ( \
    h264_rbsp_read_u1(rbsp) \
)

#define RBSP_READ_UN(rbsp, n) ( \
    h264_rbsp_read_un(rbsp, n) \
)

#define RBSP_READ_UE(rbsp) (\
   h264_rbsp_read_ue(rbsp) \
)

#define RBSP_READ_SE(rbsp) (\
    h264_rbsp_read_se(rbsp) \
)

// Nal 7.3.1
struct h264_nal_t {
    union {
        struct nal_bit_header_t {
            uint8_t nal_unit_type:5;
            uint8_t nal_ref_idc:2;
            uint8_t nal_zero_bit:1;
        };
        uint8_t header;
    } u;
};

// Slice header 7.3.3
enum h264_slice_type_e {
    SliceTypeP = 0,
    SliceTypeB = 1,
    SliceTypeI = 2, // Intra
    SliceTypeSP = 3,
    SliceTypeSI = 4,
    SliceTypePOnly = 5,
    SliceTypeBOnly = 6,
    SliceTypeIOnly = 7,
    SliceTypeSPOnly = 8,
    SliceTypeSIOnly = 9
};

//Table  9-26 – Binarization for macroblock types in I slices
enum h264_i_microblock_type_e {
    H264_I_NxN         = 0,
    H264_I_16x16_0_0_0 = 1,
    H264_I_16x16_1_0_0 = 2,
    H264_I_16x16_2_0_0 = 3,
    H264_I_16x16_3_0_0 = 4,
    H264_I_16x16_0_1_0 = 5,
    H264_I_16x16_1_1_0 = 6,
    H264_I_16x16_2_1_0 = 7,
    H264_I_16x16_3_1_0 = 8,
    H264_I_16x16_0_2_0 = 9,
    H264_I_16x16_1_2_0 = 10,
    H264_I_16x16_2_2_0 = 11,
    H264_I_16x16_3_2_0 = 12,
    H264_I_16x16_0_0_1 = 13,
    H264_I_16x16_1_0_1 = 14,
    H264_I_16x16_2_0_1 = 15,
    H264_I_16x16_3_0_1 = 16,
    H264_I_16x16_0_1_1 = 17,
    H264_I_16x16_1_1_1 = 18,
    H264_I_16x16_2_1_1 = 19,
    H264_I_16x16_3_1_1 = 20,
    H264_I_16x16_0_2_1 = 21,
    H264_I_16x16_1_2_1 = 22,
    H264_I_16x16_2_2_1 = 23,
    H264_I_16x16_3_2_1 = 24,
    H264_I_PCM         = 25
};

//Table 7-14 - Macroblock type values for B slices
enum h264_b_microblock_type_e {
    H264_B_Direct_16x16 = 0,
    H264_B_L0_16x16     = 1,
    H264_B_L1_16x16     = 2,
    H264_B_Bi_16x16     = 3,
    H264_B_L0_L0_16x8   = 4,
    H264_B_L0_L0_8x16   = 5,
    H264_B_L1_L1_16x8   = 6,
    H264_B_L1_L1_8x16   = 7,
    H264_B_L0_L1_16x8   = 8,
    H264_B_L0_L1_8x16   = 9,
    H264_B_L1_L0_16x8   = 10,
    H264_B_L1_L0_8x16   = 11,
    H264_B_L0_Bi_16x8   = 12,
    H264_B_L0_Bi_8x16   = 13,
    H264_B_L1_Bi_16x8   = 14,
    H264_B_L1_Bi_8x16   = 15,
    H264_B_Bi_L0_16x8   = 16,
    H264_B_Bi_L0_8x16   = 17,
    H264_B_Bi_L1_16x8   = 18,
    H264_B_Bi_L1_8x16   = 19,
    H264_B_Bi_Bi_16x8   = 20,
    H264_B_Bi_Bi_8x16   = 21,
    H264_B_8x8          = 22
};

//Table  9-26 – Binarization for macroblock types in I slices
enum h264_unique_microblock_type_e {
    H264_U_I             = 0x8000,
    H264_U_I_NxN         = (0x8000 | H264_I_NxN),
    H264_U_I_16x16_0_0_0 = (0x8000 | H264_I_16x16_0_0_0),
    H264_U_I_16x16_1_0_0 = (0x8000 | H264_I_16x16_1_0_0),
    H264_U_I_16x16_2_0_0 = (0x8000 | H264_I_16x16_2_0_0),
    H264_U_I_16x16_3_0_0 = (0x8000 | H264_I_16x16_3_0_0),
    H264_U_I_16x16_0_1_0 = (0x8000 | H264_I_16x16_0_1_0),
    H264_U_I_16x16_1_1_0 = (0x8000 | H264_I_16x16_1_1_0),
    H264_U_I_16x16_2_1_0 = (0x8000 | H264_I_16x16_2_1_0),
    H264_U_I_16x16_3_1_0 = (0x8000 | H264_I_16x16_3_1_0),
    H264_U_I_16x16_0_2_0 = (0x8000 | H264_I_16x16_0_2_0),
    H264_U_I_16x16_1_2_0 = (0x8000 | H264_I_16x16_1_2_0),
    H264_U_I_16x16_2_2_0 = (0x8000 | H264_I_16x16_2_2_0),
    H264_U_I_16x16_3_2_0 = (0x8000 | H264_I_16x16_3_2_0),
    H264_U_I_16x16_0_0_1 = (0x8000 | H264_I_16x16_0_0_1),
    H264_U_I_16x16_1_0_1 = (0x8000 | H264_I_16x16_1_0_1),
    H264_U_I_16x16_2_0_1 = (0x8000 | H264_I_16x16_2_0_1),
    H264_U_I_16x16_3_0_1 = (0x8000 | H264_I_16x16_3_0_1),
    H264_U_I_16x16_0_1_1 = (0x8000 | H264_I_16x16_0_1_1),
    H264_U_I_16x16_1_1_1 = (0x8000 | H264_I_16x16_1_1_1),
    H264_U_I_16x16_2_1_1 = (0x8000 | H264_I_16x16_2_1_1),
    H264_U_I_16x16_3_1_1 = (0x8000 | H264_I_16x16_3_1_1),
    H264_U_I_16x16_0_2_1 = (0x8000 | H264_I_16x16_0_2_1),
    H264_U_I_16x16_1_2_1 = (0x8000 | H264_I_16x16_1_2_1),
    H264_U_I_16x16_2_2_1 = (0x8000 | H264_I_16x16_2_2_1),
    H264_U_I_16x16_3_2_1 = (0x8000 | H264_I_16x16_3_2_1),
    H264_U_I_PCM         = (0x8000 | H264_I_PCM),
    H264_U_SI            = 0x4000,
    H264_U_P             = 0x2000,
    H264_U_P_Skip        = (0x2000 | 0x0FFF),
    H264_U_B             = 0x1000,
    H264_U_B_Direct_16x16= (0x1000 | H264_B_Direct_16x16),
    H264_U_B_L0_16x16    = (0x1000 | H264_B_L0_16x16),
    H264_U_B_L1_16x16    = (0x1000 | H264_B_L1_16x16),
    H264_U_B_Bi_16x16    = (0x1000 | H264_B_Bi_16x16),
    H264_U_B_L0_L0_16x8  = (0x1000 | H264_B_L0_L0_16x8),
    H264_U_B_L0_L0_8x16  = (0x1000 | H264_B_L0_L0_8x16),
    H264_U_B_L1_L1_16x8  = (0x1000 | H264_B_L1_L1_16x8),
    H264_U_B_L1_L1_8x16  = (0x1000 | H264_B_L1_L1_8x16),
    H264_U_B_L0_L1_16x8  = (0x1000 | H264_B_L0_L1_16x8),
    H264_U_B_L0_L1_8x16  = (0x1000 | H264_B_L0_L1_8x16),
    H264_U_B_L1_L0_16x8  = (0x1000 | H264_B_L1_L0_16x8),
    H264_U_B_L1_L0_8x16  = (0x1000 | H264_B_L1_L0_8x16),
    H264_U_B_L0_Bi_16x8  = (0x1000 | H264_B_L0_Bi_16x8),
    H264_U_B_L0_Bi_8x16  = (0x1000 | H264_B_L0_Bi_8x16),
    H264_U_B_L1_Bi_16x8  = (0x1000 | H264_B_L1_Bi_16x8),
    H264_U_B_L1_Bi_8x16  = (0x1000 | H264_B_L1_Bi_8x16),
    H264_U_B_Bi_L0_16x8  = (0x1000 | H264_B_Bi_L0_16x8),
    H264_U_B_Bi_L0_8x16  = (0x1000 | H264_B_Bi_L0_8x16),
    H264_U_B_Bi_L1_16x8  = (0x1000 | H264_B_Bi_L1_16x8),
    H264_U_B_Bi_L1_8x16  = (0x1000 | H264_B_Bi_L1_8x16),
    H264_U_B_Bi_Bi_16x8  = (0x1000 | H264_B_Bi_Bi_16x8),
    H264_U_B_Bi_Bi_8x16  = (0x1000 | H264_B_Bi_Bi_8x16),
    H264_U_B_8x8         = (0x1000 | H264_B_8x8),
    H264_U_B_Skip        = (0x1000 | 0x0FFF)        
};

enum h264_part_pred_e {
    H264_Intra_16x16,
    H264_Intra_8x8,
    H264_Intra_4x4
};

void h264_destroy();
int h264_init();
int h264_decode();

#define H264_RBSP_DEBUG(format, ...) \
{ \
    fprintf(stderr, "%s:%d - %s, position: (%d:%d-%d), "#format"\n", \
        __FILE__, __LINE__, __FUNCTION__, \
        rbsp->p - rbsp->start, \
        rbsp->bits_left, \
        rbsp->end - rbsp->p, \
        ##__VA_ARGS__); \
}

#endif // H264_H