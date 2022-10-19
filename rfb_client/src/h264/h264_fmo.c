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

//8.2.2 Decoding process for macroblock to slice group map

// Generates MapUnitToSliceGroupMap
// Has to be called every time a new Picture Parameter Set is used
static int h264_generate_MapUnitToSliceGroupMap()
{
    UNCOVERED_CASE(app.h264.pps.num_slice_groups_minus1, !=, 0);

    if (app.h264.MapUnitToSliceGroupMap) {
        free(app.h264.MapUnitToSliceGroupMap);
    }
    app.h264.MapUnitToSliceGroupMap = malloc(app.h264.header.PicSizeInMapUnits * sizeof(int));
    if (app.h264.MapUnitToSliceGroupMap == NULL) {
        printf("h264_generate_MapUnitToSliceGroupMap failed to allocate %lld bytes"
            "for MapUnitToSliceGroupMap\n",
            app.h264.header.PicSizeInMapUnits * sizeof(int)
        );
        return -1;
    }

    // only one slice group
    if (app.h264.pps.num_slice_groups_minus1 == 0) {
        memset(
            app.h264.MapUnitToSliceGroupMap,
            0,
            app.h264.header.PicSizeInMapUnits * sizeof(int)
        );
        return 0;
    }

    return -1;
}

// Generates MbToSliceGroupMap from MapUnitToSliceGroupMap 
static int h264_generate_MbToSliceGroupMap()
{
    if (app.h264.MbToSliceGroupMap) {
        free(app.h264.MbToSliceGroupMap);
    }

    app.h264.MbToSliceGroupMap = malloc(app.h264.header.PicSizeInMbs * sizeof(int));
    if (app.h264.MbToSliceGroupMap == NULL) {
        printf ("h264_generate_MapUnitToSliceGroupMap failed to allocate %lld bytes "
            "for MbToSliceGroupMap\n",
            app.h264.header.PicSizeInMbs * sizeof(int)
        );
        return -1;
    }

    if (app.h264.sps.frame_mbs_only_flag || app.h264.header.field_pic_flag) {
        for (int i=0; i < app.h264.header.PicSizeInMbs; i++) {
            app.h264.MbToSliceGroupMap[i] = app.h264.MapUnitToSliceGroupMap[i];
        }
    }
    else {
        if (app.h264.sps.mb_adaptive_frame_field_flag && !app.h264.header.field_pic_flag) {
            for (int i = 0; i < app.h264.header.PicSizeInMbs; i++) {
                app.h264.MbToSliceGroupMap[i] = app.h264.MapUnitToSliceGroupMap[i / 2];
            }
        }
        else {
            for (int i=0; i < app.h264.header.PicSizeInMbs; i++) {
                int index = (i / (2 * app.h264.header.PicWidthInMbs)) * 
                    app.h264.header.PicWidthInMbs + (i % app.h264.header.PicWidthInMbs);
                app.h264.MbToSliceGroupMap[i] = app.h264.MapUnitToSliceGroupMap[index];
            }
        }
    }
    return 0;
}
