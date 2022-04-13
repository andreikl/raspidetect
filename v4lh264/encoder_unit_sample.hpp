/*
 * Copyright (c) 2019, NVIDIA CORPORATION. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of NVIDIA CORPORATION nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * Specifies the encoder device node.
 */
#define ENCODER_DEV "/dev/nvhost-msenc"
/**
 * Specifies the maximum number of planes a buffer can contain.
 */
#define MAX_PLANES 3

/**
 * @brief Class representing a buffer.
 *
 * The Buffer class is modeled on the basis of the @c v4l2_buffer
 * structure. The buffer has @c buf_type @c v4l2_buf_type, @c
 * memory_type @c v4l2_memory, and an index. It contains an
 * BufferPlane array similar to the array of @c v4l2_plane
 * structures in @c v4l2_buffer.m.planes. It also contains a
 * corresponding BufferPlaneFormat array that describes the
 * format of each of the planes.
 *
 * In the case of a V4L2 MMAP, this class provides convenience methods
 * for mapping or unmapping the contents of the buffer to or from
 * memory, allocating or deallocating software memory depending on its
 * format.
 */
class Buffer
{
public:
    /**
     * Holds the buffer plane format.
     */
    /*typedef struct
    {
        uint32_t width;
        uint32_t height;

        uint32_t bytesperpixel;
        uint32_t stride;
        uint32_t sizeimage;
    } BufferPlaneFormat;*/

    /**
     * Holds the buffer plane parameters.
     */
    typedef struct
    {
        //BufferPlaneFormat fmt;

        unsigned char *data;        /**< Holds a pointer to the plane memory. */
        uint32_t bytesused;         /**< Holds the number of valid bytes in the plane. */

        int dev_id;                     /**< Holds the file descriptor (FD) of the plane of the
                                      exported buffer, in the case of V4L2 MMAP buffers. */
        uint32_t mem_offset;        /**< Holds the offset of the first valid byte
                                      from the data pointer. */
        uint32_t length;            /**< Holds the size of the buffer in bytes. */
    } BufferPlane;

    Buffer(enum v4l2_buf_type buf_type, enum v4l2_memory memory_type,
        uint32_t index);

    Buffer(enum v4l2_buf_type buf_type, enum v4l2_memory memory_type,
           uint32_t n_planes, uint32_t index);

     ~Buffer();

    /**
     * Maps the contents of the buffer to memory.
     *
     * This method maps the file descriptor (FD) of the planes to
     * a data pointer of @c planes. (MMAP buffers only.)
     */
    int map();
    /**
     * Unmaps the contents of the buffer from memory. (MMAP buffers only.)
     *
     */
    void unmap();

    enum v4l2_buf_type buf_type;    /**< Type of the buffer. */
    enum v4l2_memory memory_type;   /**< Type of memory associated
                                        with the buffer. */

    uint32_t index;                 /**< Holds the buffer index. */

    uint32_t n_planes;              /**< Holds the number of planes in the buffer. */
    BufferPlane planes[MAX_PLANES]; /**< Holds the data pointer, plane file
                                        descriptor (FD), plane format, etc. */

private:

    bool mapped;

};

#define V4L_MAX_IN_BUFS 10
#define V4L_MAX_OUT_BUFS 6

struct v4l_encoder_plane_t {
    uint8_t *buf;
    int len;
    int offset;
    int fd;
};

struct v4l_encoder_state_t {
    ///-------------- to delete -------

    uint32_t encode_pixfmt;
    uint32_t raw_pixfmt;
    uint32_t width;
    uint32_t height;
    uint32_t capplane_num_planes;
    uint32_t capplane_num_buffers;
    uint32_t outplane_num_buffers;

    uint32_t num_queued_outplane_buffers;
    uint32_t num_queued_capplane_buffers;

    enum v4l2_memory outplane_mem_type;
    enum v4l2_memory capplane_mem_type;
    enum v4l2_buf_type outplane_buf_type;
    enum v4l2_buf_type capplane_buf_type;
    //Buffer::BufferPlaneFormat outplane_planefmts[MAX_PLANES];
    //Buffer::BufferPlaneFormat capplane_planefmts[MAX_PLANES];

    Buffer **outplane_buffers;
    Buffer **capplane_buffers;

    string input_file_path;
    ifstream *input_file;

    string output_file_path;
    ofstream *output_file;

    pthread_mutex_t queue_lock;
    pthread_cond_t queue_cond;
    pthread_t enc_dq_thread;

    bool in_error;
    bool eos;
    bool dqthread_running;
    bool outplane_streamon;
    bool capplane_streamon;
    int dev_id;

    ///-------------- to delete -------

    int out_sizeimages[1];
    int out_strides[1];
    int in_sizeimages[3];
    int in_strides[3];

    struct v4l_encoder_plane_t in_bufs[V4L_MAX_IN_BUFS][3];
    struct v4l_encoder_plane_t out_bufs[V4L_MAX_OUT_BUFS];
    int in_bufs_count;
    int out_bufs_count;
};

/**
 * @brief Reads the raw data from a file to the buffer structure.
 *
 * Helper function to read a raw frame.
 * This function reads data from the file into the buffer plane-by-plane
 * while taking care of the stride of the plane.
 *
 * @param stream : Input stream
 * @param buffer : Buffer class pointer
 */
static int read_video_frame(ifstream * stream, Buffer & buffer);

/**
 * @brief Writes an elementary encoded frame from the buffer to a file.
 *
 * This function writes data into the file from the buffer plane-by-plane.
 *
 * @param[in] stream A pointer to the output file stream.
 * @param[in] buffer Buffer class pointer
 * @return 0 for success, -1 otherwise.
 */
static int write_encoded_frame(ofstream * stream, Buffer * buffer);

/**
 * @brief Encode processing function for blocking mode.
 *
 * Function loop to DQ and EnQ buffers on output plane
 * till eos is signalled
 *
 * @param[in] ctx Reference to the encoder context struct created.
 * @return If the application implementing this call returns TRUE,
 *         EOS is detected by the encoder and all the buffers are dequeued;
 *         else the encode process continues running.
 */
static int encoder_process_blocking();
