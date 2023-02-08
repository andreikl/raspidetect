#ifndef mmal_encoder_stubs_h
#define mmal_encoder_stubs_h

#define MMAL_FOURCC(a,b,c,d) ((a) | (b << 8) | (c << 16) | (d << 24))


#define MMAL_ENCODING_I422 MMAL_FOURCC('I','4','2','2')
#define MMAL_ENCODING_H264 MMAL_FOURCC('H','2','6','4')
#define MMAL_COMPONENT_DEFAULT_VIDEO_ENCODER "avcodec.video_encode"

typedef enum
{
    MMAL_SUCCESS = 0,
    MMAL_ENOMEM,
    MMAL_ENOSPC,
    MMAL_EINVAL,
    MMAL_ENOSYS,
    MMAL_ENOENT,
    MMAL_ENXIO,
    MMAL_EIO,
    MMAL_ESPIPE,
    MMAL_ECORRUPT,
    MMAL_ENOTREADY,
    MMAL_ECONFIG,
    MMAL_EISCONN,
    MMAL_ENOTCONN,
    MMAL_EAGAIN,
    MMAL_EFAULT
} MMAL_STATUS_T;

typedef struct MMAL_QUEUE_T MMAL_QUEUE_T;

typedef struct
 {
    int32_t x;
    int32_t y;
    int32_t width;
    int32_t height;
 } MMAL_RECT_T;
 
typedef struct
{
    int32_t num;
    int32_t den;
} MMAL_RATIONAL_T;

typedef uint32_t MMAL_FOURCC_T;

typedef enum {
    MMAL_ES_TYPE_UNKNOWN,
    MMAL_ES_TYPE_CONTROL,
    MMAL_ES_TYPE_AUDIO,
    MMAL_ES_TYPE_VIDEO,
    MMAL_ES_TYPE_SUBPICTURE
} MMAL_ES_TYPE_T;

typedef struct MMAL_BUFFER_HEADER_T
{
    struct MMAL_BUFFER_HEADER_T *next;
    struct MMAL_BUFFER_HEADER_PRIVATE_T *priv;
    uint32_t cmd;
    uint8_t  *data;
    uint32_t alloc_size;
    uint32_t length;
    uint32_t offset;
    uint32_t flags;
    int64_t  pts;
    int64_t  dts;
    //MMAL_BUFFER_HEADER_TYPE_SPECIFIC_T *type;
    void *user_data;
} MMAL_BUFFER_HEADER_T;

typedef struct
{
    uint32_t        width;
    uint32_t        height;
    MMAL_RECT_T     crop;
    MMAL_RATIONAL_T frame_rate;
    MMAL_RATIONAL_T par;
    MMAL_FOURCC_T   color_space;
} MMAL_VIDEO_FORMAT_T;
 
typedef struct MMAL_AUDIO_FORMAT_T
{
    uint32_t channels;
    uint32_t sample_rate;
    uint32_t bits_per_sample;
    uint32_t block_align;
} MMAL_AUDIO_FORMAT_T;
 
typedef struct
{
    uint32_t x_offset;
    uint32_t y_offset;
} MMAL_SUBPICTURE_FORMAT_T;

typedef union
{
    MMAL_AUDIO_FORMAT_T      audio;
    MMAL_VIDEO_FORMAT_T      video;
    MMAL_SUBPICTURE_FORMAT_T subpicture;
} MMAL_ES_SPECIFIC_FORMAT_T;

typedef struct MMAL_ES_FORMAT_T
{
    MMAL_ES_TYPE_T type;
    MMAL_FOURCC_T encoding;
    MMAL_FOURCC_T encoding_variant;
    MMAL_ES_SPECIFIC_FORMAT_T *es;
    uint32_t bitrate;
    uint32_t flags;
    uint32_t extradata_size;
    uint8_t  *extradata;
} MMAL_ES_FORMAT_T;

typedef struct MMAL_POOL_T
{
    MMAL_QUEUE_T *queue;
    uint32_t headers_num;
    MMAL_BUFFER_HEADER_T **header;
} MMAL_POOL_T;

typedef struct MMAL_PORT_T
{
    MMAL_ES_FORMAT_T *format;
    uint32_t buffer_num_recommended;
    uint32_t buffer_size_recommended;
    uint32_t buffer_num;
    uint32_t buffer_size;
} MMAL_PORT_T;

typedef struct MMAL_COMPONENT_T {
    MMAL_PORT_T* input[1];
    MMAL_PORT_T* output[1];
} MMAL_COMPONENT_T;

typedef void (*MMAL_PORT_BH_CB_T)(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer);

MMAL_STATUS_T mmal_component_create(const char *name, MMAL_COMPONENT_T **component);
MMAL_STATUS_T mmal_component_destroy(MMAL_COMPONENT_T* component);
MMAL_STATUS_T mmal_port_format_commit(MMAL_PORT_T *port);
MMAL_STATUS_T mmal_port_enable(MMAL_PORT_T *port, MMAL_PORT_BH_CB_T cb);
MMAL_STATUS_T mmal_port_disable(MMAL_PORT_T* port);
MMAL_POOL_T *mmal_port_pool_create(MMAL_PORT_T *port, unsigned int headers, uint32_t payload_size);
unsigned int mmal_queue_length(MMAL_QUEUE_T *queue);
MMAL_BUFFER_HEADER_T* mmal_queue_get(MMAL_QUEUE_T *queue);
MMAL_STATUS_T mmal_port_send_buffer(MMAL_PORT_T* port, MMAL_BUFFER_HEADER_T* buffer);
MMAL_STATUS_T mmal_buffer_header_mem_lock(MMAL_BUFFER_HEADER_T *header);
void mmal_buffer_header_mem_unlock(MMAL_BUFFER_HEADER_T *header);
void mmal_buffer_header_release(MMAL_BUFFER_HEADER_T* header);

#endif //mmal_encoder_stubs_h