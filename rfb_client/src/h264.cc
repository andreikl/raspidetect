#include "main.h"
#include "h264.h"

H264_INIT()

void h264_destroy(struct app_state_t *app)
{
#ifdef ENABLE_DXVA
    dxva_destroy(app);
#endif //ENABLE_DXVA

    LINKED_HASH_DESTROY(app->h264.headers, header);
}

int h264_init(struct app_state_t *app)
{
    LINKED_HASH_INIT(app->h264.headers, MAX_SLICES);

#ifdef ENABLE_DXVA
    GENERAL_CALL(dxva_init(app), error);
#endif //ENABLE_DXVA

    return 0;
error:
    return -1;
}

#include "h264_helpers.cc"
#include "h264_sps.cc"
#include "h264_pps.cc"
#include "h264_header.cc"

#ifdef ENABLE_H264_SLICE 
#include "h264_cabac.cc"
#include "h264_slice.cc"
#endif // ENABLE_H264_SLICE

//h264_slice.c:2090
static int h264_slice_layer_without_partitioning_rbsp(struct app_state_t *app, int start, int end)
{
    struct h264_slice_header_t* header = LINKED_HASH_GET_HEAD(app->h264.headers);
    struct h264_rbsp_t* rbsp = &app->h264.rbsp;
    uint8_t* buffer = app->enc_buf;

    RBSP_INIT(rbsp, &buffer[start], end - start);


    GENERAL_CALL(h264_read_slice_header(app), error);

    //ffmpeg: ignores redundant_pic_count but has check
    UNCOVERED_CASE(header->redundant_pic_cnt, !=, 0);

    //ffmpeg: check if slice is first but setup isn't finished
    if (header->first_mb_in_slice != 0 && app->h264.setup_finished == 0) {
        UNCOVERED_CASE(header->first_mb_in_slice, !=, 0);
        UNCOVERED_CASE(app->h264.setup_finished, ==, 0);
    }

    if (app->h264.setup_finished == 0) {
        //ffmpeg: ff_h264_execute_ref_pic_marking
        if (!header->adaptive_ref_pic_marking_mode_flag && !header->IdrPicFlag) {
            UNCOVERED_CASE(header->adaptive_ref_pic_marking_mode_flag, ==, 0);
            UNCOVERED_CASE(header->IdrPicFlag, ==, 0);
        }

        //TODO:
        //h264_field_start
        //h264_slice_init

        app->h264.setup_finished = 1;
    }

    for (unsigned i = 0; i < header->mmco_size; i++) {
        if (
            header->mmco[i].mmco == MMCO_SHORT2UNUSED ||
            header->mmco[i].mmco == MMCO_SHORT2LONG
        ) {
            unsigned pic_num = header->mmco[i].short_picNumX;
            if (header->picture_structure != H264_PICT_FRAME) {
                pic_num >>= 1;
            }
        }
    }

#ifdef ENABLE_H264_SLICE 
    GENERAL_CALL(h264_read_slice_data(app), error);
    H264_RBSP_DEBUG(*app->h264.rbsp.p);
    GENERAL_CALL(!h264_is_more_rbsp(&app->h264.rbsp), error);
#endif //ENABLE_H264_SLICE

#ifdef ENABLE_DXVA
    GENERAL_CALL(dxva_decode(app), error);
#endif //ENABLE_DXVA

    return 0;
error:
    return -1;
}

int h264_decode(struct app_state_t *app)
{
    //TODO: to delete
    //fwrite(app->h264.buf, 1, app->h264.buf_length, stdout);

    int start = 0, end = 0;
    while(!find_nal(app->enc_buf, app->enc_buf_length, &start, &end)) {
        struct h264_nal_t* header = (struct h264_nal_t*)&app->enc_buf[start++];

        app->h264.nal_unit_type = header->u.h_bits.nal_unit_type;
        app->h264.nal_ref_idc = header->u.h_bits.nal_ref_idc;

        UNCOVERED_CASE(app->h264.nal_unit_type, ==, 14);
        UNCOVERED_CASE(app->h264.nal_unit_type, ==, 20);
        UNCOVERED_CASE(app->h264.nal_unit_type, ==, 21);

        fprintf(stderr, "INFO: nal_unit_type (%d, %s)\n", app->h264.nal_unit_type, h264_get_nal_unit_type(app->h264.nal_unit_type));
        fprintf(stderr, "INFO: nal_ref_idc (%d, %s)\n", app->h264.nal_ref_idc, h264_get_nal_ref_idc(app->h264.nal_ref_idc));

        switch (app->h264.nal_unit_type) {
            case NAL_UNIT_TYPE_SPS:
                h264_read_sps(app, start, end);
                break;

            case NAL_UNIT_TYPE_PPS:
                h264_read_pps(app, start, end);
                break;

            case NAL_UNIT_TYPE_CODED_SLICE_IDR:
            case NAL_UNIT_TYPE_CODED_SLICE_NON_IDR:
                h264_slice_layer_without_partitioning_rbsp(app, start, end);
                break;

        }

        //char* rbsp = &app->buffer[start + 1];
        //int rbsp_size = end - start - 1;

        //fprintf(stderr, "INFO: rbsp package start (%d) end (%d)\n", start, end);
    }

    return 0;
}
