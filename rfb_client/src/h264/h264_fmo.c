//8.2.2 Decoding process for macroblock to slice group map

// Generates MapUnitToSliceGroupMap
// Has to be called every time a new Picture Parameter Set is used
static int h264_generate_MapUnitToSliceGroupMap(struct app_state_t* app)
{
    UNCOVERED_CASE(app->h264.pps.num_slice_groups_minus1, !=, 0);

    if (app->h264.MapUnitToSliceGroupMap) {
        free(app->h264.MapUnitToSliceGroupMap);
    }
    app->h264.MapUnitToSliceGroupMap = malloc(app->h264.header.PicSizeInMapUnits * sizeof(int));
    if (app->h264.MapUnitToSliceGroupMap == NULL) {
        printf("h264_generate_MapUnitToSliceGroupMap failed to allocate %lld bytes"
            "for MapUnitToSliceGroupMap\n",
            app->h264.header.PicSizeInMapUnits * sizeof(int)
        );
        return -1;
    }

    // only one slice group
    if (app->h264.pps.num_slice_groups_minus1 == 0) {
        memset(
            app->h264.MapUnitToSliceGroupMap,
            0,
            app->h264.header.PicSizeInMapUnits * sizeof(int)
        );
        return 0;
    }

    return -1;
}

// Generates MbToSliceGroupMap from MapUnitToSliceGroupMap 
static int h264_generate_MbToSliceGroupMap(struct app_state_t* app)
{
    if (app->h264.MbToSliceGroupMap) {
        free(app->h264.MbToSliceGroupMap);
    }

    app->h264.MbToSliceGroupMap = malloc(app->h264.header.PicSizeInMbs * sizeof(int));
    if (app->h264.MbToSliceGroupMap == NULL) {
        printf ("h264_generate_MapUnitToSliceGroupMap failed to allocate %lld bytes "
            "for MbToSliceGroupMap\n",
            app->h264.header.PicSizeInMbs * sizeof(int)
        );
        return -1;
    }

    if (app->h264.sps.frame_mbs_only_flag || app->h264.header.field_pic_flag) {
        for (int i=0; i < app->h264.header.PicSizeInMbs; i++) {
            app->h264.MbToSliceGroupMap[i] = app->h264.MapUnitToSliceGroupMap[i];
        }
    }
    else {
        if (app->h264.sps.mb_adaptive_frame_field_flag && !app->h264.header.field_pic_flag) {
            for (int i = 0; i < app->h264.header.PicSizeInMbs; i++) {
                app->h264.MbToSliceGroupMap[i] = app->h264.MapUnitToSliceGroupMap[i / 2];
            }
        }
        else {
            for (int i=0; i < app->h264.header.PicSizeInMbs; i++) {
                int index = (i / (2 * app->h264.header.PicWidthInMbs)) * 
                    app->h264.header.PicWidthInMbs + (i % app->h264.header.PicWidthInMbs);
                app->h264.MbToSliceGroupMap[i] = app->h264.MapUnitToSliceGroupMap[index];
            }
        }
    }
    return 0;
}
