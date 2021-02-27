static void dxva_print_guid(GUID guid)
{
    if (IsEqualGUID(&guid, &DXVA2_ModeH264_A)) {
        fprintf(stderr, "DXVA2_ModeH264_MoComp_NoFGT\n");
    }
    else if (IsEqualGUID(&guid, &DXVA2_ModeH264_B)) {
        fprintf(stderr, "DXVA2_ModeH264_MoComp_FGT\n");
    }
    else if (IsEqualGUID(&guid, &DXVA2_ModeH264_C)) {
        fprintf(stderr, "DXVA2_ModeH264_IDCT_NoFGT\n");
    }
    else if (IsEqualGUID(&guid, &DXVA2_ModeH264_D)) {
        fprintf(stderr, "DXVA2_ModeH264_IDCT_FGT\n");
    }
    else if (IsEqualGUID(&guid, &DXVA2_ModeH264_E)) {
        fprintf(stderr, "DXVA2_ModeH264_VLD_NoFGT\n");
    }
    else if (IsEqualGUID(&guid, &DXVA2_ModeH264_F)) {
        fprintf(stderr, "DXVA2_ModeH264_VLD_FGT\n");
    }
    // else if (IsEqualGUID(&guid, &DXVA2_ModeH264_VLD_Stereo_Progressive_NoFGT)) {
    //     fprintf(stderr, "DXVA2_ModeH264_VLD_Stereo_Progressive_NoFGT\n");
    // }
    // else if (IsEqualGUID(&guid, &DXVA2_ModeH264_VLD_Stereo_NoFGT)) {
    //     fprintf(stderr, "DXVA2_ModeH264_VLD_Stereo_NoFGT\n");
    // }
    // else if (IsEqualGUID(&guid, &DXVA2_ModeHEVC_VLD_Main)) {
    //     fprintf(stderr, "DXVA2_ModeHEVC_VLD_Main\n");
    // }
    // else if (IsEqualGUID(&guid, &DXVA2_ModeHEVC_VLD_Main10)) {
    //     fprintf(stderr, "DXVA2_ModeHEVC_VLD_Main10\n");
    // }
    else if (IsEqualGUID(&guid, &DXVA2_ModeMPEG2_IDCT)) {
        fprintf(stderr, "DXVA2_ModeMPEG2_IDCT\n");
    }
    else if (IsEqualGUID(&guid, &DXVA2_ModeMPEG2_MoComp)) {
        fprintf(stderr, "DXVA2_ModeMPEG2_MoComp\n");
    }
    else if (IsEqualGUID(&guid, &DXVA2_ModeMPEG2_VLD)) {
        fprintf(stderr, "DXVA2_ModeMPEG2_VLD\n");
    }
    // else if (IsEqualGUID(&guid, &DXVA2_ModeVC1_PostProc)) {
    //     fprintf(stderr, "DXVA2_ModeVC1_PostProc\n");
    // }
    // else if (IsEqualGUID(&guid, &DXVA2_ModeVC1_MoComp)) {
    //     fprintf(stderr, "DXVA2_ModeVC1_MoComp\n");
    // }
    // else if (IsEqualGUID(&guid, &DXVA2_ModeVC1_IDCT)) {
    //     fprintf(stderr, "DXVA2_ModeVC1_IDCT\n");
    // }
    // else if (IsEqualGUID(&guid, &DXVA2_ModeVC1_VLD)) {
    //     fprintf(stderr, "DXVA2_ModeVC1_VLD\n");
    // }
    // else if (IsEqualGUID(&guid, &DXVA2_ModeWMV8_PostProc)) {
    //     fprintf(stderr, "DXVA2_ModeWMV8_PostProc\n");
    // }
    // else if (IsEqualGUID(&guid, &DXVA2_ModeWMV8_MoComp)) {
    //     fprintf(stderr, "DXVA2_ModeWMV8_MoComp\n");
    // }
    // else if (IsEqualGUID(&guid, &DXVA2_ModeWMV9_PostProc)) {
    //     fprintf(stderr, "DXVA2_ModeWMV9_PostProc\n");
    // }
    // else if (IsEqualGUID(&guid, &DXVA2_ModeWMV9_MoComp)) {
    //     fprintf(stderr, "DXVA2_ModeWMV9_MoComp\n");
    // }
    // else if (IsEqualGUID(&guid, &DXVA2_ModeWMV9_IDCT)) {
    //     fprintf(stderr, "DXVA2_ModeWMV9_IDCT\n");
    // }
    // else if (IsEqualGUID(&guid, &DXVA2_ModeMPEG2and1_VLD)) {
    //     fprintf(stderr, "DXVA2_ModeMPEG2and1_VLD\n");
    // }
    // else if (IsEqualGUID(&guid, &DXVA2_ModeMPEG1_VLD)) {
    //     fprintf(stderr, "DXVA2_ModeMPEG1_VLD\n");
    // }
    // else if (IsEqualGUID(&guid, &DXVA2_ModeVC1_D2010)) {
    //     fprintf(stderr, "DXVA2_ModeVC1_D2010\n");
    // }
    // else if (IsEqualGUID(&guid, &DXVA2_ModeMPEG4pt2_VLD_Simple)) {
    //     fprintf(stderr, "DXVA2_ModeMPEG4pt2_VLD_Simple\n");
    // }
    // else if (IsEqualGUID(&guid, &DXVA2_ModeMPEG4pt2_VLD_AdvSimple_NoGMC)) {
    //     fprintf(stderr, "DXVA2_ModeMPEG4pt2_VLD_AdvSimple_NoGMC\n");
    // }
    // else if (IsEqualGUID(&guid, &DXVA2_ModeMPEG4pt2_VLD_AdvSimple_GMC)) {
    //     fprintf(stderr, "DXVA2_ModeMPEG4pt2_VLD_AdvSimple_GMC\n");
    // }
    else if (IsEqualGUID(&guid, &DXVA_NoEncrypt)) {
        fprintf(stderr, "DXVA_NoEncrypt\n");
    }
    else {
        fprintf(stderr, "{%08lX-%04hX-%04hX-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX}\n", 
            guid.Data1, guid.Data2, guid.Data3, 
            guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
            guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
    }
}

static void dxva_print_format(D3DFORMAT format)
{
    if (format == D3DFMT_UYVY) {
        fprintf(stderr, "D3DFMT_UYVY\n");
    } 
    else if (format == D3DFMT_R8G8_B8G8) {
        fprintf(stderr, "D3DFMT_UYVY\n");
    }
    else if (format == D3DFMT_YUY2) {
        fprintf(stderr, "D3DFMT_YUY2\n");
    }
    else if (format == D3DFMT_G8R8_G8B8) {
        fprintf(stderr, "D3DFMT_G8R8_G8B8\n");
    }
    else if (format == D3DFMT_DXT1) {
        fprintf(stderr, "D3DFMT_DXT1\n");
    }
    else if (format == D3DFMT_DXT2) {
        fprintf(stderr, "D3DFMT_DXT2\n");
    }
    else if (format == D3DFMT_DXT3) {
        fprintf(stderr, "D3DFMT_DXT3\n");
    }
    else if (format == D3DFMT_DXT4) {
        fprintf(stderr, "D3DFMT_DXT3\n");
    }
    else if (format == (D3DFORMAT)MAKEFOURCC('N', 'V', '1', '2')) {
        fprintf(stderr, "D3DFMT_NV12\n");
    }
    else if (format == (D3DFORMAT)MAKEFOURCC('Y', 'V', '1', '2')) {
        fprintf(stderr, "D3DFMT_YV12\n");
    }
    else {
        fprintf(stderr, "%x\n", format);
    }
    
}

static void dxva_print_config(struct app_state_t *app)
{
    for (int i = 0; i < app->dxva.cfg_count; i++) {
        DXVA2_ConfigPictureDecode *cfg = &app->dxva.cfg_list[i];
        fprintf(stderr, "%d guidConfigBitstreamEncryption: ", i); dxva_print_guid(cfg->guidConfigBitstreamEncryption);
        fprintf(stderr, "%d guidConfigMBcontrolEncryption: ", i); dxva_print_guid(cfg->guidConfigMBcontrolEncryption);
        fprintf(stderr, "%d guidConfigResidDiffEncryption: ", i); dxva_print_guid(cfg->guidConfigResidDiffEncryption);
        fprintf(stderr, "%d ConfigBitstreamRaw: %d\n", i, cfg->ConfigBitstreamRaw);
        fprintf(stderr, "%d ConfigMBcontrolRasterOrder: %d\n", i, cfg->ConfigMBcontrolRasterOrder);
        fprintf(stderr, "%d ConfigResidDiffHost: %d\n", i, cfg->ConfigResidDiffHost);
        fprintf(stderr, "%d ConfigSpatialResid8: %d\n", i, cfg->ConfigSpatialResid8);
        fprintf(stderr, "%d ConfigResid8Subtraction: %d\n", i, cfg->ConfigResid8Subtraction);
        fprintf(stderr, "%d ConfigSpatialHost8or9Clipping: %d\n", i, cfg->ConfigSpatialHost8or9Clipping);
        fprintf(stderr, "%d ConfigSpatialResidInterleaved: %d\n", i, cfg->ConfigSpatialResidInterleaved);
        fprintf(stderr, "%d ConfigIntraResidUnsigned: %d\n", i, cfg->ConfigIntraResidUnsigned);
        fprintf(stderr, "%d ConfigResidDiffAccelerator: %d\n", i, cfg->ConfigResidDiffAccelerator);
        fprintf(stderr, "%d ConfigHostInverseScan: %d\n", i, cfg->ConfigHostInverseScan);
        fprintf(stderr, "%d ConfigSpecificIDCT: %d\n", i, cfg->ConfigSpecificIDCT);
        fprintf(stderr, "%d Config4GroupedCoefs: %d\n", i, cfg->Config4GroupedCoefs);
        fprintf(stderr, "%d ConfigMinRenderTargetBuffCount: %d\n", i, cfg->ConfigMinRenderTargetBuffCount);
        fprintf(stderr, "%d ConfigDecoderSpecific: %d\n", i, cfg->ConfigDecoderSpecific);
    }
}

static int dxva_find_config(struct app_state_t *app)
{
    for (int i = 0; i < app->dxva.cfg_count; i++) {
        DXVA2_ConfigPictureDecode *cfg = &app->dxva.cfg_list[i];
        if (cfg->ConfigBitstreamRaw == 1) {
            app->dxva.cfg = cfg;
            break;
        }
    }
    if (app->dxva.cfg == NULL) {
        return 1;
    }
    return 0;
}

static int dxva_find_decoder(struct app_state_t *app)
{
    int res = 1;
    HRESULT h_res;
    D3DFORMAT *formats = NULL;

    unsigned guid_count;
    GUID *guids = NULL;
    h_res = IDirectXVideoDecoderService_GetDecoderDeviceGuids(app->dxva.service, &guid_count, &guids);
    if (FAILED(h_res)) {
        fprintf(stderr, "ERROR: Can't get list of decoders\n");
        return 0;
    }

    for (int i = 0; i < guid_count; i++) {
        //dxva_print_guid(guids[i]);
        if (IsEqualGUID(&guids[i], &H264CODEC)) {
            unsigned target_count;
            h_res = IDirectXVideoDecoderService_GetDecoderRenderTargets(app->dxva.service, &guids[i], &target_count, &formats);
            if (FAILED(h_res)) {
                fprintf(stderr, "ERROR: Can't get render target for ");
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

