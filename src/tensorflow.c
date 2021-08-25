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

int tensorflow_process(app_state_t *state)
{
    TfLiteStatus status = TfLiteTensorCopyFromBuffer(state->tf.tf_input_image, state->worker_buffer_rgb, TfLiteTensorByteSize(state->tf.tf_input_image));
    if (status != kTfLiteOk) {
        fprintf(stderr, "ERROR: failed to fill input tensor, status: %x\n", status);
        return -1;
    }

    status = TfLiteInterpreterInvoke(state->tf.tf_interpreter);
    if (status != kTfLiteOk) {
        fprintf(stderr, "ERROR: failed to invoke classifier, status: %x\n", status);
        return -1;
    }

    if (state->verbose) {
        //DEBUG("the image has been classified");

        /*for (int i = 0; i < TfLiteInterpreterGetOutputTensorCount(state->tf_interpreter); i++) {
            const TfLiteTensor *tensor = TfLiteInterpreterGetOutputTensor(state->tf_interpreter, i);
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

    pthread_mutex_lock(&state->buffer_mutex);
    status = TfLiteTensorCopyToBuffer(state->tf.tf_tensor_boxes, state->worker_boxes, TfLiteTensorByteSize(state->tf.tf_tensor_boxes));
    if (status != kTfLiteOk) {
        fprintf(stderr, "ERROR: failed to copy output boxes, status: %x\n", status);
        return -1;
    }

    status = TfLiteTensorCopyToBuffer(state->tf.tf_tensor_classes, state->worker_classes, TfLiteTensorByteSize(state->tf.tf_tensor_classes));
    if (status != kTfLiteOk) {
        fprintf(stderr, "ERROR: failed to copy output classes, status: %x\n", status);
        return -1;
    }

    status = TfLiteTensorCopyToBuffer(state->tf.tf_tensor_scores, state->worker_scores, TfLiteTensorByteSize(state->tf.tf_tensor_scores));
    if (status != kTfLiteOk) {
        fprintf(stderr, "ERROR: failed to copy output scores, status: %x\n", status);
        return -1;
    }

    // if (state->verbose) {
    //     DEBUG("scores:");
    //     for (int i = 0; i < state->worker_total_objects; i++) {
    //         fprintf(stderr, "%.2f ", state->worker_scores[i]);
    //     }
    //     fprintf(stderr, "\n");
    // }

    pthread_mutex_unlock(&state->buffer_mutex);

    return 0;
}

int tensorflow_create(app_state_t *state)
{
    if (state->verbose) {
        DEBUG("TensorFlow Lite C library version %s", TfLiteVersion());
    }

    state->tf.tf_model = TfLiteModelCreateFromFile(state->model_path);
    if (!state->tf.tf_model) {
        fprintf(stderr, "ERROR: Failed to read TFL model (path: %s)\n", state->model_path);
        return -1;
    }

    state->tf.tf_options = TfLiteInterpreterOptionsCreate();
    if (!state->tf.tf_options) {
        fprintf(stderr, "ERROR: Failed to create TFL options\n");
        return -1;
    }
    TfLiteInterpreterOptionsSetNumThreads(state->tf.tf_options, 4);
    
    state->tf.tf_interpreter = TfLiteInterpreterCreate(state->tf.tf_model, state->tf.tf_options);
    if (!state->tf.tf_interpreter) {
        fprintf(stderr, "ERROR: Failed to create TFL Interpreter\n");
        return -1;
    }
    if (state->verbose) {
        DEBUG("Input tensors count: %d", TfLiteInterpreterGetInputTensorCount(state->tf.tf_interpreter));
        DEBUG("Output tensors count: %d", TfLiteInterpreterGetOutputTensorCount(state->tf.tf_interpreter));
    }

    if (TfLiteInterpreterGetInputTensorCount(state->tf.tf_interpreter) != 1) {
        fprintf(stderr, "ERROR: TFL model doesn't have one input tensor\n");
        return -1;
    }

    if (TfLiteInterpreterGetOutputTensorCount(state->tf.tf_interpreter) != 4) {
        fprintf(stderr, "ERROR: TFL model doesn't have 4 output tensors\n");
        return -1;
    }

    state->tf.tf_input_image = TfLiteInterpreterGetInputTensor(state->tf.tf_interpreter, 0);
    if (TfLiteTensorNumDims(state->tf.tf_input_image) != 4) {
        fprintf(stderr, "ERROR: TFL input tensor doesn't have 4 dimensions\n");
        return -1;
    }

    for (int i = 1; i < TfLiteTensorNumDims(state->tf.tf_input_image); i++) {
        switch(i) {
            case 1: state->worker_width = TfLiteTensorDim(state->tf.tf_input_image, i); break;
            case 2: state->worker_height = TfLiteTensorDim(state->tf.tf_input_image, i); break;
            case 3: state->worker_bits_per_pixel = TfLiteTensorDim(state->tf.tf_input_image, i) * 8; break;

        }
    }

    if (state->verbose) {
        DEBUG("Input tensor dimensions: %d", TfLiteTensorNumDims(state->tf.tf_input_image));
        DEBUG("Input tensor name: %s, type %s",
            TfLiteTensorName(state->tf.tf_input_image),
            TfLiteTypeGetName(TfLiteTensorType(state->tf.tf_input_image)));
        DEBUG("Input tensor width %d, height %d, bits per pixel %d, size: %d",
            state->worker_width,
            state->worker_height,
            state->worker_bits_per_pixel,
            TfLiteTensorByteSize(state->tf.tf_input_image));
    }

    if (state->worker_bits_per_pixel != 24
        || state->worker_width > state->width
        || state->worker_height > state->height
        || state->worker_width * state->worker_height * 3 != TfLiteTensorByteSize(state->tf.tf_input_image)) {
        fprintf(stderr, "ERROR: Inavlid input tensor format\n");
        return -1;
    }

    TfLiteStatus status = TfLiteInterpreterAllocateTensors(state->tf.tf_interpreter);
    if (status != kTfLiteOk) {
        fprintf(stderr, "ERROR: Failed to allocate memmory for tensors, status: %x\n", status);
        return -1;
    }


    for (int i = 0; i < TfLiteInterpreterGetOutputTensorCount(state->tf.tf_interpreter); i++) {
        const TfLiteTensor *tensor = TfLiteInterpreterGetOutputTensor(state->tf.tf_interpreter, i);
        if (TfLiteTensorType(tensor) != kTfLiteFloat32) {
            fprintf(stderr, "ERROR: Inavlid output tensor format\n");
            return -1;
        }
        switch (i) {
            case 0: state->tf.tf_tensor_boxes = tensor; break;
            case 1: state->tf.tf_tensor_classes = tensor; break;
            case 2: state->tf.tf_tensor_scores = tensor; break;
            case 3: state->tf.tf_tensor_num_detections = tensor; break;
        }
    }

    /*float f_num_detections;
    status = TfLiteTensorCopyToBuffer(tf_num_detections, (char*)&f_num_detections, TfLiteTensorByteSize(tf_num_detections));
    if (status != kTfLiteOk) {
        fprintf(stderr, "ERROR: failed to read num detections tensor, status: %x\n", status);
    }
    state->tf_num_detections = (int)f_num_detections;
    if (state->verbose) {
        DEBUG("Number of detections %d %.1f", state->tf_num_detections, f_num_detections);
    }*/

    return 0;
}
    
void tensorflow_destroy(app_state_t *state)
{
    if (state->tf.tf_model) {
        TfLiteModelDelete(state->tf.tf_model);
    }

    if (state->tf.tf_options) {
        TfLiteInterpreterOptionsDelete(state->tf.tf_options);
    }

    if (state->tf.tf_interpreter) {
        TfLiteInterpreterDelete(state->tf.tf_interpreter);
    }
}
