import tensorflow as tf

gf = tf.GraphDef()
m_file = open('C:/projects/other/raspberry/tensorflow/ssd_mobilenet_v2_coco_2018_03_29/frozen_inference_graph.pb', 'rb')
gf.ParseFromString(m_file.read())

#all node names
for n in gf.node:
    print(n.name)

#first node name
print(gf.node[0].name)

