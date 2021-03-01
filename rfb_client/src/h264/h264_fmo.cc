//8.2.2 Decoding process for macroblock to slice group map

// Generates MapUnitToSliceGroupMap
// Has to be called every time a new Picture Parameter Set is used
static int h264_generate_MapUnitToSliceGroupMap(struct app_state_t* app)
{
    UNCOVERED_CASE(app->h264.pps.num_slice_groups_minus1, !=, 0);
    struct h264_slice_header_t* header = LINKED_HASH_GET_HEAD(app->h264.headers);

    if (app->h264.MapUnitToSliceGroupMap) {
        free(app->h264.MapUnitToSliceGroupMap);
    }
    app->h264.MapUnitToSliceGroupMap = (unsigned*)malloc(header->PicSizeInMapUnits * sizeof(int));
    if (app->h264.MapUnitToSliceGroupMap == NULL) {
        printf("h264_generate_MapUnitToSliceGroupMap failed to allocate %lld bytes"
            "for MapUnitToSliceGroupMap\n",
            header->PicSizeInMapUnits * sizeof(int)
        );
        return -1;
    }

    // only one slice group
    if (app->h264.pps.num_slice_groups_minus1 == 0) {
        memset(
            app->h264.MapUnitToSliceGroupMap,
            0,
            header->PicSizeInMapUnits * sizeof(int)
        );
        return 0;
    }

    return -1;
}

// Generates MbToSliceGroupMap from MapUnitToSliceGroupMap 
static int h264_generate_MbToSliceGroupMap(struct app_state_t* app)
{
    struct h264_slice_header_t* header = LINKED_HASH_GET_HEAD(app->h264.headers);

    if (app->h264.MbToSliceGroupMap) {
        free(app->h264.MbToSliceGroupMap);
    }

    app->h264.MbToSliceGroupMap = (unsigned*)malloc(header->PicSizeInMbs * sizeof(int));
    if (app->h264.MbToSliceGroupMap == NULL) {
        printf ("h264_generate_MapUnitToSliceGroupMap failed to allocate %lld bytes "
            "for MbToSliceGroupMap\n",
            header->PicSizeInMbs * sizeof(int)
        );
        return -1;
    }

    if (app->h264.sps.frame_mbs_only_flag || header->field_pic_flag) {
        for (unsigned i = 0; i < header->PicSizeInMbs; i++) {
            app->h264.MbToSliceGroupMap[i] = app->h264.MapUnitToSliceGroupMap[i];
        }
    }
    else {
        if (app->h264.sps.mb_adaptive_frame_field_flag && !header->field_pic_flag) {
            for (unsigned i = 0; i < header->PicSizeInMbs; i++) {
                app->h264.MbToSliceGroupMap[i] = app->h264.MapUnitToSliceGroupMap[i / 2];
            }
        }
        else {
            for (unsigned i = 0; i < header->PicSizeInMbs; i++) {
                unsigned index = (i / (2 * header->PicWidthInMbs)) * 
                    header->PicWidthInMbs + (i % header->PicWidthInMbs);
                app->h264.MbToSliceGroupMap[i] = app->h264.MapUnitToSliceGroupMap[index];
            }
        }
    }
    return 0;
}
