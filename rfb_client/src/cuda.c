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

#include "main.h"
#include "utils.h"
#include "cuda.h"

#include "cuda_helpers.c"

int cuda_destroy(app_state_t* app)
{
    if (app->cuda.context) {
        CUDA_API_CALL(cuCtxDestroy(app->cuda.context), error);
        app->cuda.context = NULL;
    }
    return 0;
error:
    return -1;
}

int cuda_init(app_state_t* app)
{
    CUDA_API_CALL(cuInit(0), error);

    int gpu_count = 0;
    CUDA_API_CALL(cuDeviceGetCount(&gpu_count), error);

    if (gpu_count <= 0) {
        DEBUG_MSG("ERROR: Device doesn't have cuda\n%s:%s:%d\n", __FILE__, __FUNCTION__, __LINE__);
        goto error;
    }

    CUDA_API_CALL(cuDeviceGet(&app->cuda.device, 0), error);
    CUDA_API_CALL(cuDeviceGetName(app->cuda.name, sizeof(app->cuda.name), app->cuda.device), error);

    if (app->verbose) {
        DEBUG_MSG("INFO: Cuda device: %s\n%s:%s:%d\n", app->cuda.name, __FILE__, __FUNCTION__, __LINE__);
    }
   
    CUDA_API_CALL(cuCtxCreate(&app->cuda.context, 0, app->cuda.device), error);

    // IN: Coded sequence width in pixels
    //app->cuda.info.ulWidth = app->server_width;
    app->cuda.info.ulWidth = 640;
    // IN: Coded sequence height in pixels
    //app->cuda.info.ulHeight = app->server_height;
    app->cuda.info.ulHeight = 480;
    // IN: Maximum number of internal decode surfaces 
    //app->cuda.info.ulNumDecodeSurfaces = SCREEN_BUFFERS;
    app->cuda.info.ulNumDecodeSurfaces = 2;
    // IN: cudaVideoCodec_XXX
    //cudaVideoCodec_H264_SVC - newest and beter network transmission, cudaVideoCodec_H264_MVC ???
    app->cuda.info.CodecType = cudaVideoCodec_H264_SVC;
    // IN: cudaVideoChromaFormat_XXX
    //app->cuda.info.ChromaFormat = app->server_chroma;
    app->cuda.info.ChromaFormat = cudaVideoChromaFormat_422;
    // TODO: ??? IN: Decoder creation flags (cudaVideoCreateFlags_XXX)
    app->cuda.info.ulCreationFlags = 0;
    // TODO: ??? IN: The value "BitDepth minus 8
    app->cuda.info.bitDepthMinus8 = 0;
    // IN: Set 1 only if video has all intra frames (default value is 0).
    // This will optimize video memory for Intra frames only decoding. The support is limited
    // to specific codecs - H264, HEVC, VP9, the flag will be ignored for codecs which
    // are not supported. However decoding might fail if the flag is enabled in case
    // of supported codecs for regular bit streams having P and/or B frames.    
    app->cuda.info.ulIntraDecodeOnly = 0;
    // IN: Coded sequence max width in pixels used with reconfigure Decoder
    //app->cuda.info.ulMaxWidth = app->server_width;
    app->cuda.info.ulMaxWidth = 640;
    // IN: Coded sequence max height in pixels used with reconfigure Decoder
    //app->cuda.info.ulMaxHeight = app->server_height;
    app->cuda.info.ulMaxHeight = 480;
    // IN: area of the frame that should be displayed
    app->cuda.info.display_area.left = 0;
    app->cuda.info.display_area.top = 0;
    //app->cuda.info.display_area.right = app->server_width;
    app->cuda.info.display_area.right = 640;
    //app->cuda.info.display_area.bottom = app->server_height;
    app->cuda.info.display_area.bottom = 480;
    // IN: cudaVideoSurfaceFormat_XXX
    //app->cuda.info.OutputFormat = app->server_chroma;
    app->cuda.info.OutputFormat = cudaVideoChromaFormat_422;
    // IN: cudaVideoDeinterlaceMode_XXX
    app->cuda.info.DeinterlaceMode = cudaVideoDeinterlaceMode_Adaptive;
    // IN: Post-processed output width (Should be aligned to 2)
    //app->cuda.info.ulTargetWidth = app->server_width;
    app->cuda.info.ulTargetWidth = 640;
    // IN: Post-processed output height (Should be aligned to 2)
    //app->cuda.info.ulTargetHeight = app->server_height;
    app->cuda.info.ulTargetHeight = 480;
    // IN: Maximum number of output surfaces simultaneously mapped
    app->cuda.info.ulNumOutputSurfaces = 1;
    // TODO: ??? IN: If non-NULL, context lock used for synchronizing ownership of
    // the cuda context. Needed for cudaVideoCreate_PreferCUDA decode
    app->cuda.info.vidLock = NULL;
    // IN: target rectangle in the output frame (for aspect ratio conversion)
    // if a null rectangle is specified, {0,0,ulTargetWidth,ulTargetHeight} will be used
    app->cuda.info.target_rect.left = 0;
    app->cuda.info.target_rect.top = 0;
    //app->cuda.info.target_rect.right = app->server_width;
    app->cuda.info.target_rect.right = 640;
    //app->cuda.info.target_rect.bottom = app->server_height;
    app->cuda.info.target_rect.bottom = 480;

    CUDA_API_CALL(cuvidCreateDecoder(&app->cuda.decoder, &app->cuda.info), error);
    return 0;

error:
    cuda_destroy(app);
    return -1;
}
