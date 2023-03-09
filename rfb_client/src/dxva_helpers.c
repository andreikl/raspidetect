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

static void dxva_print_guid(GUID guid)
{
    if (IsEqualGUID(&guid, &DXVA2_ModeH264_A)) {
        DEBUG_MSG("DXVA2_ModeH264_MoComp_NoFGT\n");
    }
    else if (IsEqualGUID(&guid, &DXVA2_ModeH264_B)) {
        DEBUG_MSG("DXVA2_ModeH264_MoComp_FGT\n");
    }
    else if (IsEqualGUID(&guid, &DXVA2_ModeH264_C)) {
        DEBUG_MSG("DXVA2_ModeH264_IDCT_NoFGT\n");
    }
    else if (IsEqualGUID(&guid, &DXVA2_ModeH264_D)) {
        DEBUG_MSG("DXVA2_ModeH264_IDCT_FGT\n");
    }
    else if (IsEqualGUID(&guid, &DXVA2_ModeH264_E)) {
        DEBUG_MSG("DXVA2_ModeH264_VLD_NoFGT\n");
    }
    else if (IsEqualGUID(&guid, &DXVA2_ModeH264_F)) {
        DEBUG_MSG("DXVA2_ModeH264_VLD_FGT\n");
    }
    // else if (IsEqualGUID(&guid, &DXVA2_ModeH264_VLD_Stereo_Progressive_NoFGT)) {
    //     DEBUG_MSG("DXVA2_ModeH264_VLD_Stereo_Progressive_NoFGT\n");
    // }
    // else if (IsEqualGUID(&guid, &DXVA2_ModeH264_VLD_Stereo_NoFGT)) {
    //     DEBUG_MSG("DXVA2_ModeH264_VLD_Stereo_NoFGT\n");
    // }
    // else if (IsEqualGUID(&guid, &DXVA2_ModeHEVC_VLD_Main)) {
    //     DEBUG_MSG("DXVA2_ModeHEVC_VLD_Main\n");
    // }
    // else if (IsEqualGUID(&guid, &DXVA2_ModeHEVC_VLD_Main10)) {
    //     DEBUG_MSG("DXVA2_ModeHEVC_VLD_Main10\n");
    // }
    else if (IsEqualGUID(&guid, &DXVA2_ModeMPEG2_IDCT)) {
        DEBUG_MSG("DXVA2_ModeMPEG2_IDCT\n");
    }
    else if (IsEqualGUID(&guid, &DXVA2_ModeMPEG2_MoComp)) {
        DEBUG_MSG("DXVA2_ModeMPEG2_MoComp\n");
    }
    else if (IsEqualGUID(&guid, &DXVA2_ModeMPEG2_VLD)) {
        DEBUG_MSG("DXVA2_ModeMPEG2_VLD\n");
    }
    // else if (IsEqualGUID(&guid, &DXVA2_ModeVC1_PostProc)) {
    //     DEBUG_MSG("DXVA2_ModeVC1_PostProc\n");
    // }
    // else if (IsEqualGUID(&guid, &DXVA2_ModeVC1_MoComp)) {
    //     DEBUG_MSG("DXVA2_ModeVC1_MoComp\n");
    // }
    // else if (IsEqualGUID(&guid, &DXVA2_ModeVC1_IDCT)) {
    //     DEBUG_MSG("DXVA2_ModeVC1_IDCT\n");
    // }
    // else if (IsEqualGUID(&guid, &DXVA2_ModeVC1_VLD)) {
    //     DEBUG_MSG("DXVA2_ModeVC1_VLD\n");
    // }
    // else if (IsEqualGUID(&guid, &DXVA2_ModeWMV8_PostProc)) {
    //     DEBUG_MSG("DXVA2_ModeWMV8_PostProc\n");
    // }
    // else if (IsEqualGUID(&guid, &DXVA2_ModeWMV8_MoComp)) {
    //     DEBUG_MSG("DXVA2_ModeWMV8_MoComp\n");
    // }
    // else if (IsEqualGUID(&guid, &DXVA2_ModeWMV9_PostProc)) {
    //     DEBUG_MSG("DXVA2_ModeWMV9_PostProc\n");
    // }
    // else if (IsEqualGUID(&guid, &DXVA2_ModeWMV9_MoComp)) {
    //     DEBUG_MSG("DXVA2_ModeWMV9_MoComp\n");
    // }
    // else if (IsEqualGUID(&guid, &DXVA2_ModeWMV9_IDCT)) {
    //     DEBUG_MSG("DXVA2_ModeWMV9_IDCT\n");
    // }
    // else if (IsEqualGUID(&guid, &DXVA2_ModeMPEG2and1_VLD)) {
    //     DEBUG_MSG("DXVA2_ModeMPEG2and1_VLD\n");
    // }
    // else if (IsEqualGUID(&guid, &DXVA2_ModeMPEG1_VLD)) {
    //     DEBUG_MSG("DXVA2_ModeMPEG1_VLD\n");
    // }
    // else if (IsEqualGUID(&guid, &DXVA2_ModeVC1_D2010)) {
    //     DEBUG_MSG("DXVA2_ModeVC1_D2010\n");
    // }
    // else if (IsEqualGUID(&guid, &DXVA2_ModeMPEG4pt2_VLD_Simple)) {
    //     DEBUG_MSG("DXVA2_ModeMPEG4pt2_VLD_Simple\n");
    // }
    // else if (IsEqualGUID(&guid, &DXVA2_ModeMPEG4pt2_VLD_AdvSimple_NoGMC)) {
    //     DEBUG_MSG("DXVA2_ModeMPEG4pt2_VLD_AdvSimple_NoGMC\n");
    // }
    // else if (IsEqualGUID(&guid, &DXVA2_ModeMPEG4pt2_VLD_AdvSimple_GMC)) {
    //     DEBUG_MSG("DXVA2_ModeMPEG4pt2_VLD_AdvSimple_GMC\n");
    // }
    else if (IsEqualGUID(&guid, &DXVA_NoEncrypt)) {
        DEBUG_MSG("DXVA_NoEncrypt\n");
    }
    else {
        DEBUG_MSG("{%08lX-%04hX-%04hX-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX}\n", 
            guid.Data1, guid.Data2, guid.Data3, 
            guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
            guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
    }
}

static void dxva_print_format(D3DFORMAT format)
{
    if (format == D3DFMT_UYVY) {
        DEBUG_MSG("D3DFMT_UYVY\n");
    } 
    else if (format == D3DFMT_R8G8_B8G8) {
        DEBUG_MSG("D3DFMT_UYVY\n");
    }
    else if (format == D3DFMT_YUY2) {
        DEBUG_MSG("D3DFMT_YUY2\n");
    }
    else if (format == D3DFMT_G8R8_G8B8) {
        DEBUG_MSG("D3DFMT_G8R8_G8B8\n");
    }
    else if (format == D3DFMT_DXT1) {
        DEBUG_MSG("D3DFMT_DXT1\n");
    }
    else if (format == D3DFMT_DXT2) {
        DEBUG_MSG("D3DFMT_DXT2\n");
    }
    else if (format == D3DFMT_DXT3) {
        DEBUG_MSG("D3DFMT_DXT3\n");
    }
    else if (format == D3DFMT_DXT4) {
        DEBUG_MSG("D3DFMT_DXT3\n");
    }
    else if (format == (D3DFORMAT)MAKEFOURCC('N', 'V', '1', '2')) {
        DEBUG_MSG("D3DFMT_NV12\n");
    }
    else if (format == (D3DFORMAT)MAKEFOURCC('Y', 'V', '1', '2')) {
        DEBUG_MSG("D3DFMT_YV12\n");
    }
    else {
        DEBUG_MSG("%x\n", format);
    }
    
}

static void dxva_print_config()
{
    for (int i = 0; i < app.dxva.cfg_count; i++) {
        DXVA2_ConfigPictureDecode *cfg = &app.dxva.cfg_list[i];
        DEBUG_MSG("%d guidConfigBitstreamEncryption: ", i); dxva_print_guid(cfg->guidConfigBitstreamEncryption);
        DEBUG_MSG("%d guidConfigMBcontrolEncryption: ", i); dxva_print_guid(cfg->guidConfigMBcontrolEncryption);
        DEBUG_MSG("%d guidConfigResidDiffEncryption: ", i); dxva_print_guid(cfg->guidConfigResidDiffEncryption);
        DEBUG_MSG("%d ConfigBitstreamRaw: %d\n", i, cfg->ConfigBitstreamRaw);
        DEBUG_MSG("%d ConfigMBcontrolRasterOrder: %d\n", i, cfg->ConfigMBcontrolRasterOrder);
        DEBUG_MSG("%d ConfigResidDiffHost: %d\n", i, cfg->ConfigResidDiffHost);
        DEBUG_MSG("%d ConfigSpatialResid8: %d\n", i, cfg->ConfigSpatialResid8);
        DEBUG_MSG("%d ConfigResid8Subtraction: %d\n", i, cfg->ConfigResid8Subtraction);
        DEBUG_MSG("%d ConfigSpatialHost8or9Clipping: %d\n", i, cfg->ConfigSpatialHost8or9Clipping);
        DEBUG_MSG("%d ConfigSpatialResidInterleaved: %d\n", i, cfg->ConfigSpatialResidInterleaved);
        DEBUG_MSG("%d ConfigIntraResidUnsigned: %d\n", i, cfg->ConfigIntraResidUnsigned);
        DEBUG_MSG("%d ConfigResidDiffAccelerator: %d\n", i, cfg->ConfigResidDiffAccelerator);
        DEBUG_MSG("%d ConfigHostInverseScan: %d\n", i, cfg->ConfigHostInverseScan);
        DEBUG_MSG("%d ConfigSpecificIDCT: %d\n", i, cfg->ConfigSpecificIDCT);
        DEBUG_MSG("%d Config4GroupedCoefs: %d\n", i, cfg->Config4GroupedCoefs);
        DEBUG_MSG("%d ConfigMinRenderTargetBuffCount: %d\n", i, cfg->ConfigMinRenderTargetBuffCount);
        DEBUG_MSG("%d ConfigDecoderSpecific: %d\n", i, cfg->ConfigDecoderSpecific);
    }
}

static int dxva_find_config()
{
    for (int i = 0; i < app.dxva.cfg_count; i++) {
        DXVA2_ConfigPictureDecode *cfg = &app.dxva.cfg_list[i];
        // determines DXVA_Slice_H264_Long or DXVA_Slice_H264_Short structure is used
        // for slice control data
        if (cfg->ConfigBitstreamRaw == 1 || cfg->ConfigBitstreamRaw == 2) {
            app.dxva.cfg = cfg;
            break;
        }
    }
    if (app.dxva.cfg == NULL) {
        return 1;
    }
    return 0;
}

static int dxva_find_decoder()
{
    int res = 1;
    HRESULT h_res;
    D3DFORMAT *formats = NULL;

    unsigned guid_count;
    GUID *guids = NULL;
    h_res = IDirectXVideoDecoderService_GetDecoderDeviceGuids(app.dxva.service, &guid_count, &guids);
    if (FAILED(h_res)) {
        DEBUG_MSG("ERROR: Can't get list of decoders\n");
        return 0;
    }

    for (int i = 0; i < guid_count; i++) {
        //dxva_print_guid(guids[i]);
        if (IsEqualGUID(&guids[i], &H264CODEC)) {
            unsigned target_count;
            h_res = IDirectXVideoDecoderService_GetDecoderRenderTargets(app.dxva.service, &guids[i], &target_count, &formats);
            if (FAILED(h_res)) {
                DEBUG_MSG("ERROR: Can't get render target for ");
                dxva_print_guid(guids[i]);
                break;            
            }
            for (int j = 0; j < target_count; j++) {
                //dxva_print_format(formats[j]);
                if (formats[j] == H264CODEC_FORMAT) {
                    res = 0;
                    goto close;
                }
            }
        }
    }

close:
    if (formats != NULL) {
        CoTaskMemFree(formats);
    }
    if (guids != NULL) {
        CoTaskMemFree(guids);
    }
    return res;
}

