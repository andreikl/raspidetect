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

extern struct app_state_t app;

int tensorflow_process()
{
    TfLiteStatus status = TfLiteTensorCopyFromBuffer(app.tf.tf_input_image, app.worker_buffer_rgb, TfLiteTensorByteSize(app.tf.tf_input_image));
    if (status != kTfLiteOk) {
        fprintf(stderr, "ERROR: failed to fill input tensor, status: %x\n", status);
        return -1;
    }

    status = TfLiteInterpreterInvoke(app.tf.tf_interpreter);
    if (status != kTfLiteOk) {
        fprintf(stderr, "ERROR: failed to invoke classifier, status: %x\n", status);
        return -1;
    }

    if (app.verbose) {
        //DEBUG("the image has been classified");

        /*for (int i = 0; i < TfLiteInterpreterGetOutputTensorCount(app.tf_interpreter); i++) {
            const TfLiteTensor *tensor = TfLiteInterpreterGetOutputTensor(app.tf_interpreter, i);
            DEBUG("Output tensor %d name: %s, type: %s, dimensions: %d, size: %d",
                i,
                TfLiteTensorName(tensor),
                TfLiteTypeGetName(TfLiteTensorType(tensor)),
                TfLiteTensorNumDims(tensor),
                TfLiteTensorByteSize(tensor));

            char data[TfLiteTensorByteSize(tensor)];

            TfLiteStatus status = TfLiteTensorCopyToBuffer(tensor, data, TfLiteTensorByteSize(tensor));
            if (status != kTfLiteOk) {
                fprintf(stderr, "ERROR: failed to read output tensor, status: %x\n", status);
                return NULL;
            }

            DEBUG("tensor %d dimensions size: ", i);
            for (int j = 0; j < TfLiteTensorNumDims(tensor); j++) {
                fprintf(stderr, "%d ", TfLiteTensorDim(tensor, j));
            }
            fprintf(stderr, "\n");

            DEBUG("data %d:", i);
            float *fdata = (float *)data;
            for (int j = 0; j < TfLiteTensorByteSize(tensor) / sizeof(float); j++) {
                fprintf(stderr, "%.2f ", fdata[j]);
            }
            fprintf(stderr, "\n");
        }*/
    }

    pthread_mutex_lock(&app.buffer_mutex);
    status = TfLiteTensorCopyToBuffer(app.tf.tf_tensor_boxes, app.worker_boxes, TfLiteTensorByteSize(app.tf.tf_tensor_boxes));
    if (status != kTfLiteOk) {
        fprintf(stderr, "ERROR: failed to copy output boxes, status: %x\n", status);
        return -1;
    }

    status = TfLiteTensorCopyToBuffer(app.tf.tf_tensor_classes, app.worker_classes, TfLiteTensorByteSize(app.tf.tf_tensor_classes));
    if (status != kTfLiteOk) {
        fprintf(stderr, "ERROR: failed to copy output classes, status: %x\n", status);
        return -1;
    }

    status = TfLiteTensorCopyToBuffer(app.tf.tf_tensor_scores, app.worker_scores, TfLiteTensorByteSize(app.tf.tf_tensor_scores));
    if (status != kTfLiteOk) {
        fprintf(stderr, "ERROR: failed to copy output scores, status: %x\n", status);
        return -1;
    }

    // if (app.verbose) {
    //     DEBUG("scores:");
    //     for (int i = 0; i < app.worker_total_objects; i++) {
    //         fprintf(stderr, "%.2f ", app.worker_scores[i]);
    //     }
    //     fprintf(stderr, "\n");
    // }

    pthread_mutex_unlock(&app.buffer_mutex);

    return 0;
}

int tensorflow_create()
{
    DEBUG("TensorFlow Lite C library version %s", TfLiteVersion());

    app.tf.tf_model = TfLiteModelCreateFromFile(app.model_path);
    if (!app.tf.tf_model) {
        fprintf(stderr, "ERROR: Failed to read TFL model (path: %s)\n", app.model_path);
        return -1;
    }

    app.tf.tf_options = TfLiteInterpreterOptionsCreate();
    if (!app.tf.tf_options) {
        fprintf(stderr, "ERROR: Failed to create TFL options\n");
        return -1;
    }
    TfLiteInterpreterOptionsSetNumThreads(app.tf.tf_options, 4);
    
    app.tf.tf_interpreter = TfLiteInterpreterCreate(app.tf.tf_model, app.tf.tf_options);
    if (!app.tf.tf_interpreter) {
        fprintf(stderr, "ERROR: Failed to create TFL Interpreter\n");
        return -1;
    }
    DEBUG("Input tensors count: %d", TfLiteInterpreterGetInputTensorCount(app.tf.tf_interpreter));
    DEBUG("Output tensors count: %d", TfLiteInterpreterGetOutputTensorCount(app.tf.tf_interpreter));

    if (TfLiteInterpreterGetInputTensorCount(app.tf.tf_interpreter) != 1) {
        fprintf(stderr, "ERROR: TFL model doesn't have one input tensor\n");
        return -1;
    }

    if (TfLiteInterpreterGetOutputTensorCount(app.tf.tf_interpreter) != 4) {
        fprintf(stderr, "ERROR: TFL model doesn't have 4 output tensors\n");
        return -1;
    }

    app.tf.tf_input_image = TfLiteInterpreterGetInputTensor(app.tf.tf_interpreter, 0);
    if (TfLiteTensorNumDims(app.tf.tf_input_image) != 4) {
        fprintf(stderr, "ERROR: TFL input tensor doesn't have 4 dimensions\n");
        return -1;
    }

    for (int i = 1; i < TfLiteTensorNumDims(app.tf.tf_input_image); i++) {
        switch(i) {
            case 1: app.worker_width = TfLiteTensorDim(app.tf.tf_input_image, i); break;
            case 2: app.worker_height = TfLiteTensorDim(app.tf.tf_input_image, i); break;
            case 3: app.worker_bits_per_pixel = TfLiteTensorDim(app.tf.tf_input_image, i) * 8; break;

        }
    }

    DEBUG("Input tensor dimensions: %d", TfLiteTensorNumDims(app.tf.tf_input_image));
    DEBUG("Input tensor name: %s, type %s",
        TfLiteTensorName(app.tf.tf_input_image),
        TfLiteTypeGetName(TfLiteTensorType(app.tf.tf_input_image)));
    DEBUG("Input tensor width %d, height %d, bits per pixel %d, size: %d",
        app.worker_width,
        app.worker_height,
        app.worker_bits_per_pixel,
        TfLiteTensorByteSize(app.tf.tf_input_image));

    if (app.worker_bits_per_pixel != 24
        || app.worker_width > app.width
        || app.worker_height > app.height
        || app.worker_width * app.worker_height * 3 != TfLiteTensorByteSize(app.tf.tf_input_image)) {
        fprintf(stderr, "ERROR: Inavlid input tensor format\n");
        return -1;
    }

    TfLiteStatus status = TfLiteInterpreterAllocateTensors(app.tf.tf_interpreter);
    if (status != kTfLiteOk) {
        fprintf(stderr, "ERROR: Failed to allocate memmory for tensors, status: %x\n", status);
        return -1;
    }


    for (int i = 0; i < TfLiteInterpreterGetOutputTensorCount(app.tf.tf_interpreter); i++) {
        const TfLiteTensor *tensor = TfLiteInterpreterGetOutputTensor(app.tf.tf_interpreter, i);
        if (TfLiteTensorType(tensor) != kTfLiteFloat32) {
            fprintf(stderr, "ERROR: Inavlid output tensor format\n");
            return -1;
        }
        switch (i) {
            case 0: app.tf.tf_tensor_boxes = tensor; break;
            case 1: app.tf.tf_tensor_classes = tensor; break;
            case 2: app.tf.tf_tensor_scores = tensor; break;
            case 3: app.tf.tf_tensor_num_detections = tensor; break;
        }
    }

    /*float f_num_detections;
    status = TfLiteTensorCopyToBuffer(tf_num_detections, (char*)&f_num_detections, TfLiteTensorByteSize(tf_num_detections));
    if (status != kTfLiteOk) {
        fprintf(stderr, "ERROR: failed to read num detections tensor, status: %x\n", status);
    }
    app.tf_num_detections = (int)f_num_detections;
    DEBUG("Number of detections %d %.1f", app.tf_num_detections, f_num_detections);
    */

    return 0;
}
    
void tensorflow_destroy()
{
    if (app.tf.tf_model) {
        TfLiteModelDelete(app.tf.tf_model);
    }

    if (app.tf.tf_options) {
        TfLiteInterpreterOptionsDelete(app.tf.tf_options);
    }

    if (app.tf.tf_interpreter) {
        TfLiteInterpreterDelete(app.tf.tf_interpreter);
    }
}
