#ifndef MAIN_H
#define MAIN_H

#define MAX_BUFFER 255
#define MAX_NAME_SIZE 16

// number of processing surfaces
#define SCREEN_BUFFERS 2

#define APP_NAME "rfb_client\0"

#define HELP "--help\0"
#define INPUT_TYPE "-i\0"
#define INPUT_TYPE_FILE_STR "file\0"
#define INPUT_TYPE_RFB_STR "rfb\0"
#define INPUT_TYPE_DEF "file\0"
#define INPUT_TYPE_RFB 1
#define INPUT_TYPE_FILE 0

#define FILE_NAME "-f\0"
#define FILE_NAME_DEF "t.h264\0"
#define SERVER "-s\0"
//#define SERVER_DEF "58.107.224.90\0"
#define SERVER_DEF "192.168.2.101\0"
#define PORT "-p\0"
//#define PORT_DEF "5906\0"
#define PORT_DEF "5900\0"
#define VERBOSE "-d\0"
#define VERBOSE_DEF 1

// Check windows
#if _WIN32 || _WIN64
    #if _WIN64
        #define ENV64BIT
    #else
        #define ENV32BIT
    #endif
#endif

// Check GCC
#if __GNUC__
    #if __x86_64__ || __ppc64__
        #define ENV64BIT
    #else
        #define ENV32BIT
    #endif
#endif

#define CHROMA_FORMAT_YUV400 0 //monochrome
#define CHROMA_FORMAT_YUV420 1
#define CHROMA_FORMAT_YUV422 2 // 2 bytes
#define CHROMA_FORMAT_YUV444 3

#define H264CODEC_FORMAT (D3DFORMAT)MAKEFOURCC('N', 'V', '1', '2')
//NV12 – 12 bits per pixel planar format with Y plane and interleaved UV plane

#include <stdint.h> // types like uint8_t
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <windows.h>
#include <fcntl.h> // O_BINARY
#include <time.h> // nanosleep

#include "utils.h"

#define GET_3RD_ARG(arg1, arg2, arg3, ...) arg3

#define UNCOVERED_CASE(value, condition, expectation) \
{ \
    if (value condition expectation) { \
        fprintf(stderr, "ERROR: uncovered case "#value"(%d) "#condition" "#expectation"(%d)\n%s:%d - %s\n", \
            value, expectation, __FILE__, __LINE__, __FUNCTION__); \
        return -1; \
    } \
}

#define GENERAL_CALL_2(call, error) \
{ \
    int res = call; \
    if (res) { \
        fprintf(stderr, "ERROR: "#call" returned error: %s(%d)\n%s:%d - %s\n", \
            convert_general_error(res), res, __FILE__, __LINE__, __FUNCTION__); \
        goto error; \
    } \
} \

#define GENERAL_CALL_1(call) \
{ \
    int res = call; \
    if (res) { \
        fprintf(stderr, "ERROR: "#call" returned error: %s(%d)\n%s:%d - %s\n", \
            convert_general_error(res), res, __FILE__, __LINE__, __FUNCTION__); \
    } \
} \

#define GENERAL_CALL_X(...) GET_3RD_ARG(__VA_ARGS__, GENERAL_CALL_2, GENERAL_CALL_1, )

#define GENERAL_CALL(...) GENERAL_CALL_X(__VA_ARGS__)(__VA_ARGS__)

#define STANDARD_CALL_2(call, error) \
{ \
    int res = call; \
    if (res) { \
        fprintf(stderr, "ERROR: "#call" returned error: %s\n%s:%d - %s\n", \
            strerror(errno), __FILE__, __LINE__, __FUNCTION__); \
        goto error; \
    } \
} \

#define STANDARD_CALL_1(call) \
{ \
    int res = call; \
    if (res) { \
        fprintf(stderr, "ERROR: "#call" returned error: %s\n%s:%d - %s\n", \
            strerror(errno), __FILE__, __LINE__, __FUNCTION__); \
    } \
} \

#define STANDARD_CALL_X(...) GET_3RD_ARG(__VA_ARGS__, STANDARD_CALL_2, STANDARD_CALL_1, )
#define STANDARD_CALL(...) STANDARD_CALL_X(__VA_ARGS__)(__VA_ARGS__)

#define ARRAY_SIZE(a) ((unsigned)(sizeof(a) / sizeof(a[0])))

#define GENERAL_DEBUG(value) \
{ \
    fprintf(stderr, "INFO: %s, "#value": %d\n", \
        __FUNCTION__, \
        value); \
}

#define GENERAL_DEBUG_INT(text, value) \
{ \
    fprintf(stderr, "INFO: %s, "#text": %d\n", \
        __FUNCTION__, \
        value); \
}

#define GENERAL_DEBUG_LONG(text, value) \
{ \
    fprintf(stderr, "INFO: %s, "#text": %lld\n", \
        __FUNCTION__, \
        value); \
}

#define NETWORK_IO_CALL(call, error) \
{ \
    int res = call; \
    if (res < 0) { \
        fprintf(stderr, "ERROR: "#call" returned error: %d\n%s:%d - %s\n", \
            WSAGetLastError(), __FILE__, __LINE__, __FUNCTION__); \
        goto error; \
    } \
}

#define NETWORK_CALL_2(call, error) \
{ \
    int res = call; \
    if (res) { \
        fprintf(stderr, "ERROR: "#call" returned error: %d\n%s:%d - %s\n", \
            WSAGetLastError(), __FILE__, __LINE__, __FUNCTION__); \
        goto error; \
    } \
}

#define NETWORK_CALL_1(call) \
{ \
    int res = call; \
    if (res) { \
        fprintf(stderr, "ERROR: "#call" returned error: %d\n%s:%d - %s\n", \
            WSAGetLastError(), __FILE__, __LINE__, __FUNCTION__); \
    } \
}

#define NETWORK_CALL_X(...) GET_3RD_ARG(__VA_ARGS__, NETWORK_CALL_2, NETWORK_CALL_1, )
#define NETWORK_CALL(...) NETWORK_CALL_X(__VA_ARGS__)(__VA_ARGS__)

#include "linked_hash.h"

#ifdef ENABLE_RFB
    #include "rfb.h"

    struct rfb_state_t
    {
        int socket;
        pthread_t thread;
        int is_thread;
        sem_t semaphore;
    };
#endif //ENABLE_RFB

#ifdef ENABLE_D3D
    #include <d3d9.h> //Direct3d9 API
    #include "d3d.h"

    struct d3d_state_t {
        IDirect3D9* d3d;
        IDirect3DDevice9* dev;
        IDirect3DSurface9* surfaces[SCREEN_BUFFERS];

        pthread_t thread;
        int is_thread;
        sem_t semaphore;
    };
#endif //ENABLE_D3D

#ifdef ENABLE_DXVA
    //#include <initguid.h> //DEFINE_GUID macros
    #include <dxva2api.h> //dxva2 API
    #include <dxva.h> //DXVA_Slice_H264_Long
    #include "dxva.h"

    //#define H264CODEC DXVA2_ModeH264_A
    //#define H264CODEC DXVA2_ModeH264_B
    //#define H264CODEC DXVA2_ModeH264_C
    //#define H264CODEC DXVA2_ModeH264_D
    #define H264CODEC DXVA2_ModeH264_E // DXVA2_ModeH264_VLD_NoFGT
    //#define H264CODEC DXVA2_ModeH264_F
    //#define H264CODEC DXVA2_ModeH264_VLD_Stereo_Progressive_NoFGT
    //#define H264CODEC DXVA2_ModeH264_VLD_Stereo_NoFGT

    //#ifndef DXVA_NoEncrypt
    //DEFINE_GUID(DXVA_NoEncrypt, 0x1b81beD0, 0xa0c7, 0x11d3, 0xb9, 0x84, 0x00, 0xc0, 0x4f, 0x2e, 0x73, 0xc5);
    //#endif

    struct dxva_state_t
    {
        IDirect3DDeviceManager9 *device_manager;
        IDirectXVideoDecoderService *service;
        IDirectXVideoDecoder *decoder;
        HANDLE device;

        unsigned cfg_count;
        DXVA2_ConfigPictureDecode *cfg_list;
        DXVA2_ConfigPictureDecode *cfg;

        DXVA_PicParams_H264 pic_params;
        DXVA_Qmatrix_H264 matrices;
        DXVA_Slice_H264_Long slice;

        unsigned status_report;
        unsigned slice_id;
    };
#endif //ENABLE_DXVA

#ifdef ENABLE_H264
    #include "h264.h"

    #define H264_PICT_TOP_FIELD     1
    #define H264_PICT_BOTTOM_FIELD  2
    #define H264_PICT_FRAME         3

    #define MMCO_END                1
    #define MMCO_SHORT2UNUSED       2
    #define MMCO_LONG2UNUSED        3
    #define MMCO_SHORT2LONG         4
    #define MMCO_SET_MAX_LONG       5
    #define MMCO_RESET              6
    #define MMCO_LONG               7

    #define H264_MAX_REFS 32
    #define MAX_SLICES 256
    
    // Sequence parameter set 7.3.2.1
    struct h264_sps_t
    {
        unsigned profile_idc;
        unsigned constraint_set;
        unsigned level_idc;

        unsigned seq_parameter_set_id;
        unsigned chroma_format_idc;
        unsigned separate_colour_plane_flag;
        unsigned bit_depth_luma_minus8;
        unsigned bit_depth_chroma_minus8;
        unsigned qpprime_y_zero_transform_bypass_flag;
        unsigned seq_scaling_matrix_present_flag;
        unsigned log2_max_frame_num;
        unsigned pic_order_cnt_type;
        unsigned log2_max_pic_order_cnt_lsb_minus4;
        unsigned delta_pic_order_always_zero_flag;
        int offset_for_non_ref_pic;
        int offset_for_top_to_bottom_field;
        unsigned num_ref_frames_in_pic_order_cnt_cycle;
        int offset_for_ref_frame[H264_MAX_REFS];
        unsigned num_ref_frames;
        unsigned gaps_in_frame_num_value_allowed_flag;
        unsigned pic_width_in_mbs_minus1;
        unsigned pic_height_in_map_units_minus1;
        unsigned frame_mbs_only_flag;
        unsigned mb_adaptive_frame_field_flag;
        unsigned direct_8x8_inference_flag;
        unsigned frame_cropping_flag;
        unsigned frame_crop_left_offset;
        unsigned frame_crop_right_offset;
        unsigned frame_crop_top_offset;
        unsigned frame_crop_bottom_offset;

        unsigned vui_parameters_present_flag;
        unsigned aspect_ratio_info_present_flag;
        unsigned overscan_info_present_flag;
        unsigned video_signal_type_present_flag;
        unsigned chroma_loc_info_present_flag;
        unsigned timing_info_present_flag;
        unsigned nal_hrd_parameters_present_flag;
        unsigned vcl_hrd_parameters_present_flag;
        unsigned pic_struct_present_flag;
        unsigned bitstream_restriction_flag;
        unsigned motion_vectors_over_pic_boundaries_flag;
        unsigned max_bytes_per_pic_denom;
        unsigned max_bits_per_mb_denom;
        unsigned log2_max_mv_length_horizontal;
        unsigned log2_max_mv_length_vertical;
        unsigned max_num_reorder_frames;
        unsigned max_dec_frame_buffering;

        unsigned ChromaArrayType;
    };

    // Picture parameter set 7.3.2.2
    struct h264_pps_t
    {
        unsigned pic_parameter_set_id;
        unsigned seq_parameter_set_id;
        unsigned entropy_coding_mode_flag;
        unsigned bottom_field_pic_order_in_frame_present_flag;
        unsigned num_slice_groups_minus1;
        unsigned weighted_pred_flag;
        unsigned weighted_bipred_idc;
        int pic_init_qp_minus26;
        int pic_init_qs_minus26;
        int chroma_qp_index_offset;
        unsigned deblocking_filter_control_present_flag;
        unsigned constrained_intra_pred_flag;
        unsigned redundant_pic_cnt_present_flag;
        unsigned transform_8x8_mode_flag;
        unsigned pic_scaling_matrix_present_flag;
        int second_chroma_qp_index_offset;

        //num_ref_idx_l0_default_active, num_ref_idx_l1_default_active;
        unsigned ref_count[2];
    };


    // 8 bytes alignment
    struct h264_weight_t
    {
        // luma_weight_l0_flag, luma_weight_l0, luma_offset_l0, chroma_weight_l0_flag,
        // chroma_weight_l0, chroma_offset_l0
        // luma_weight_l1_flag, luma_weight_l1, luma_offset_l1, chroma_weight_l1_flag;
        // chroma_weight_l1, chroma_offset_l1
        char luma_weight_flag;
        char luma_weight;
        char luma_offset;
        char chroma_weight_flag;
        char chroma_weight[2];
        char chroma_offset[2];
    };

    struct h264_ref_pic_list_modification_t
    {
        int reordering_of_pic_nums_idc;
        int abs_diff_pic_num_minus1;
        int long_term_pic_num;
    };

    struct h264_memory_management_control_operation_t
    {
        //memory_management_control_operation
        unsigned mmco;
        unsigned short_picNumX;

        unsigned long_term_pic_num;
        unsigned long_term_frame_idx;
        unsigned max_long_term_frame_idx_plus1;
    };

    // Slice header 7.3.3
    struct h264_slice_header_t
    {
        unsigned first_mb_in_slice;
        unsigned slice_type;
        unsigned slice_type_origin;
        unsigned pic_parameter_set_id;
        unsigned frame_num;
        unsigned field_pic_flag;
        unsigned bottom_field_flag;
        unsigned idr_pic_id;
        unsigned pic_order_cnt_lsb;
        int delta_pic_order_cnt_bottom;
        unsigned redundant_pic_cnt;
        unsigned direct_spatial_mv_pred_flag;
        unsigned num_ref_idx_active_override_flag;
        //unsigned ref_pic_list_reordering_flag_l0;
        //unsigned reordering_of_pic_nums_idc;
        //unsigned abs_diff_pic_num_minus1;
        //unsigned ref_pic_list_reordering_flag_l1;
        unsigned luma_log2_weight_denom;
        unsigned chroma_log2_weight_denom;
        struct h264_weight_t weights[2][H264_MAX_REFS];
        struct h264_ref_pic_list_modification_t modifications[2][H264_MAX_REFS];
        struct h264_memory_management_control_operation_t mmco[H264_MAX_REFS];
        unsigned mmco_size;

        unsigned long_term_reference_flag;
        unsigned no_output_of_prior_pics_flag;
        unsigned adaptive_ref_pic_marking_mode_flag;
        unsigned cabac_init_idc;
        int slice_qp_delta;
        unsigned sp_for_switch_flag;
        unsigned slice_qs_delta;
        unsigned disable_deblocking_filter_idc;
        unsigned slice_alpha_c0_offset_div2;
        unsigned slice_beta_offset_div2;

        //num_ref_idx_l0_active, num_ref_idx_l1_active;
        unsigned ref_count[2];
        unsigned list_count;

        unsigned IdrPicFlag; //7-1
        unsigned PicWidthInMbs; //7-13
        unsigned PicHeightInMapUnits; //7-16
        unsigned PicSizeInMapUnits; ////7-17
        unsigned FrameHeightInMbs; //7-18
        unsigned MbaffFrameFlag; //7-25
        unsigned PicHeightInMbs;//7-26
        unsigned PicSizeInMbs; //7-29

        // 8.2.5.1 Sequence of operations for decoded reference picture marking process
        int LongTermFrameIdx;
        int MaxLongTermFrameIdx;

        unsigned PrevRefFrameNum;

        //ffmpeg
        unsigned picture_structure;
        unsigned curr_pic_num;
        unsigned max_pic_num;
    };

    DEFINE_LINKED_HASH(struct h264_slice_header_t, header)

    struct h264_rbsp_t {
        uint8_t* p;
        uint8_t* start;
        uint8_t* end;
        int bits_left;
    };

#ifdef ENABLE_H264_SLICE
    // 7.3.5.3.3 Residual block CABAC syntax
    struct h264_coeff_t {
        unsigned significant_coeff_flag;
        unsigned last_significant_coeff_flag;
        int coeff_abs_level_minus1;
        unsigned coeff_sign_flag;
        int value;
    };

    struct h264_coeff_level_t {
        unsigned coded_block_flag;
        unsigned maxNumCoeff;
        unsigned numDecodAbsLevelEq1;
        unsigned numDecodAbsLevelGt1; 
        struct h264_coeff_t* coeff;
    };

    // 7.3.4 Slice data
    struct h264_macroblock_t {
        unsigned mb_field_decoding_flag;
        unsigned mb_skip_flag;
        unsigned mb_skip_run;
        unsigned pcm_sample_luma[256];
        unsigned pcm_sample_chroma[512];
        unsigned end_of_slice_flag;

        unsigned mb_type_origin;
        unsigned mb_type;
        unsigned transform_size_8x8_flag;
        int intra4x4_pred_mode[16];
        unsigned intra_chroma_pred_mode;
        unsigned coded_block_pattern;
        unsigned CodedBlockPatternLuma;
        unsigned CodedBlockPatternChroma;
        unsigned mb_qp_delta;

        int MbPartPredMode;

        struct h264_coeff_level_t Intra16x16DCLevel;
        struct h264_coeff_level_t Intra16x16ACLevel[16];
        struct h264_coeff_level_t LumaLevel4x4[16];
        struct h264_coeff_level_t LumaLevel8x8[16];
        struct h264_coeff_level_t ChromaDCLevel[4];
        struct h264_coeff_level_t ChromaACLevel[2][4];
    };

    struct h264_slice_data_t
    {
        // Macroblock variables
        signed CurrMbAddr;
        signed PrevMbAddr;

        struct h264_macroblock_t* macroblocks;
        struct h264_macroblock_t* curr_mb;
        
    };

    struct h264_cabac_t
    {
        unsigned codIOffset;
        unsigned codIRange;
    };

    struct h264_context_t
    {
        unsigned pStateIdx;
        int valMPS;
    };
#endif //ENABLE_H264_SLICE

    struct h264_state_t {
        unsigned nal_unit_type;
        unsigned nal_ref_idc;
        unsigned setup_finished;
        struct h264_sps_t sps;
        struct h264_pps_t pps;

        LINKED_HASH(header) headers;

        struct h264_rbsp_t rbsp;

#ifdef ENABLE_H264_SLICE
        struct h264_slice_data_t data;
        struct h264_cabac_t cabac;
        struct h264_context_t contexts[1024];
        unsigned* MbToSliceGroupMap;
        unsigned* MapUnitToSliceGroupMap;
#endif //ENABLE_H264_SLICE
    };
#endif //ENABLE_H264

#ifdef ENABLE_FFMPEG

extern "C" {
    #include <libavcodec/avcodec.h>
}

#include "ffmpeg.h"
    struct ffmpeg_state_t {
        const AVCodec* codec;
        AVCodecContext* ctx;
        AVFrame* fr;
        AVPacket* pkt;
    };
#endif //ENABLE_FFMPEG

#ifdef ENABLE_CUDA
    #include <cuviddec.h>
    #include "cuda.h"

    struct cuda_state_t {
        char name[MAX_NAME_SIZE + 1];
        CUdevice device;
        CUcontext context;
        CUvideodecoder decoder;
        CUVIDDECODECREATEINFO info;
    };
#endif //ENABLE_CUDA

#include "file.h"

struct file_state_t {
    const char* file_name;
    FILE* fstream;
    sem_t semaphore;

    pthread_t thread;
    int is_thread;
};

struct app_state_t {
    HWND wnd;
    HINSTANCE instance;
    unsigned input_type;
    const char* server_host;
    const char* server_port;
    int server_width;
    int server_height;
    int server_chroma;
    int verbose;

    uint8_t *enc_buf;
    int enc_buf_length;

    uint8_t *dec_buf;
    int dec_buf_length;
    pthread_mutex_t dec_mutex;
    int is_dec_mutex;

    struct file_state_t file;

#ifdef ENABLE_RFB
    struct rfb_state_t rfb;
#endif //ENABLE_RFB

#ifdef ENABLE_D3D
    struct d3d_state_t d3d;
#endif //ENABLE_D3d

#ifdef ENABLE_DXVA
    struct dxva_state_t dxva;
#endif //ENABLE_DXVA

#ifdef ENABLE_H264
    struct h264_state_t h264;
#endif //ENABLE_H264

#ifdef ENABLE_FFMPEG
    struct ffmpeg_state_t ffmpeg;
#endif //ENABLE_FFMPEG

#ifdef ENABLE_CUDA
    struct cuda_state_t cuda;
#endif //ENABLE_CUDA

    // it isn't aligned
    char server_name[MAX_NAME_SIZE + 1];
};
#endif // MAIN_H