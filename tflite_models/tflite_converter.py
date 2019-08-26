import tensorflow as tf

graph_def_file = "C:/projects/other/raspberry/tensorflow_models/ssd_mobilenet_v1_coco_2018_01_28/frozen_inference_graph.pb"
#input_arrays = ["model_inputs"]
#output_arrays = ["model_outputs"]

input_arrays = ["image_tensor"]
output_arrays = ["detection_boxes", "detection_scores", "detection_classes", "num_detections"]
input_shapes = {"image_tensor": [1,640,480,3]}

converter = tf.lite.TFLiteConverter.from_frozen_graph(
  graph_def_file, input_arrays, output_arrays, input_shapes)
tflite_model = converter.convert()
open("frozen_inference_graph.tflite", "wb").write(tflite_model)
