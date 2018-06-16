#ifndef VPU_API_PRIVATE_H_
#define VPU_API_PRIVATE_H_

#include "vpu_global.h"
#include "vpu_api.h"
#include <stdint.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>

#define MAX_STREAM_LENGHT  1024*1024  //for encoder out put stream
#define DEFAULT_FRAME_BUFFER_DEPTH 4

/* interlace filed mode support */
typedef enum {
    DEINTER_INPUT_EOS_REACHED   = 0x01,
}DEINTER_FLAG_MAP;

typedef enum DEINTER_CMD {
    DEINT_GET_PRE_PROCESS_COUNT,
    DEINT_GET_FRAME_COUNT,
    DEINT_SET_INPUT_EOS_REACH,
} DEINTER_CMD;

typedef enum DEINTER_STATUS_TYPE {
    DEINTER_NOT_INIT            = -1,
    DEINTER_OK                  = 0,
    DEINTER_INIT_FAIL           = 1,
    DEINTER_ERR_FATAL_THREAD    = 2,
    DEINTER_ERR_OPEN_DEV        = 3,
    DEINTER_ERR_TEST_DEV        = 4,
    DEINTER_ERR_LIST_PRE_DEINT  = 5,
    DEINTER_ERR_ERR_LIST_STREAM = 6,

    DEINTER_ERR_UNKNOW          = 100,
    DEINTER_BUTT,
}DEINTER_STATUS_TYPE;

typedef struct DeinterContext {
    VPU_FRAME  deintBuf;
    VPU_FRAME  originBuf;
    int32_t    poll_flag;
}DeinterContext_t;

typedef enum {
    NEED_FLUSH_RK_DPB           = 0x01,
    ENABLE_DEINTERLACE_SUPPORT  = 0x02,
    INTERLACE_FIELD_MODE        = 0x04,

}VPU_API_PRIVATE_FLAG_MAP;

// trick: put vpu_mem at head of node and make stream destroy simple
typedef struct {
    uint8_t* buf;
    uint32_t size;
    int64_t timestamp;
    int32_t usePts;
} stream_packet;

typedef enum VPU_CODEC_CMD {
    VPU_GET_STREAM_COUNT,
    VPU_GET_FRAME_COUNT,
} VPU_CODEC_CMD;

typedef struct Stream_describe
{
    uint8_t                 flag[4];
    uint32_t                size;
    uint32_t                timel;
    uint32_t                timeh;
    uint32_t                type;
    uint32_t                slices;
    uint32_t                retFlag;
    uint32_t                res[1];
    uint32_t                slicesize[128];
}Stream_describe;

/* Rk decoder Input structure */
typedef struct RkDecInput
{
    uint8_t *pStream;               /* Pointer to the input */
    uint32_t streamBusAddress;      /* DMA bus address of the input stream */
    uint32_t dataLen;               /* Number of bytes to be decoded         */
    uint32_t picId;                 /* Identifier for the picture to be decoded */
    uint32_t skipNonReference;      /* Flag to enable decoder skip non-reference
                                    * frames to reduce processor load */
    int64_t  timeUs;
    int32_t usePts;
}RkDecInput_t;

/* Rk decoder Output structure */
typedef struct RkDecOutput
{
    uint32_t dataLeft;              /* how many bytes left undecoded */
}RkDecOutput_t;

typedef struct H264HwStatus {
    int32_t  errFrmNum;
    int32_t  tolerance;
}H264HwStatus_t;


typedef struct RkDecExt
{
    int32_t headSend;
    int32_t reserver[2];
    H264HwStatus_t  h264HwStatus;
}RkDecExt_t;

typedef enum CODEC_CFG_VALUE {
    CFG_VALUE_UNKNOW,
    CFG_VALUE_SUPPORT,
    CFG_VALUE_NOT_SUPPORT
} CODEC_CFG_VALUE;

typedef struct RkCodecConfigure {
    CODEC_TYPE codecType;
    OMX_RK_VIDEO_CODINGTYPE videoCoding;
    uint8_t old_cfg[10];
    const char* cfg;
    CODEC_CFG_VALUE value;
}RkCodecConfigure_t;

typedef struct CfgFile {
    FILE*       file;
    uint32_t    file_size;
    uint32_t    read_pos;
}CfgFile_t;

typedef struct VpuApiCodecCfg {
    CfgFile_t   cfg_file;
    uint8_t*    buf;
}VpuApiCodecCfg_t;

typedef struct VpuApiTimeStampTrace {
    int64_t     pre_timeUs;
    uint32_t    same_cnt;
    uint32_t    init;
}VpuApiTimeStampTrace_t;

#ifdef __cplusplus
extern "C"
{
#endif

extern void*  get_class_RkAvcDecoder(void);
extern int    init_class_RkAvcDecoder(void * AvcDecoder, int32_t tsFlag);
extern void   destroy_class_RkAvcDecoder(void * AvcDecoder);
extern int    deinit_class_RkAvcDecoder(void * AvcDecoder);
extern int    dec_oneframe_class_RkAvcDecoder(void * AvcDecoder, uint8_t* aOutBuffer, uint32_t *aOutputLength, uint8_t* aInputBuf, uint32_t* aInBufSize, int64_t *InputTimestamp);
extern int    reset_class_RkAvcDecoder(void * AvcDecoder);
extern int    flush_oneframe_in_dpb_class_RkAvcDecoder(void *decoder, uint8_t* aOutBuffer, uint32_t *aOutputLength);
extern int    perform_seting_class_RkAvcDecoder(void * AvcDecoder,VPU_API_CMD cmd,void *param);

extern void*  get_class_RkM4vDecoder(void);
extern void   destroy_class_RkM4vDecoder(void * M4vDecoder);
extern int    init_class_RkM4vDecoder(void * M4vDecoder, VPU_GENERIC *vpug);
extern int    deinit_class_RkM4vDecoder(void * M4vDecoder);
extern int    reset_class_RkM4vDecoder(void * M4vDecoder);
extern int    dec_oneframe_class_RkM4vDecoder(void * M4vDecoder,uint8_t* aOutBuffer, uint32_t *aOutputLength, uint8_t* aInputBuf, uint32_t* aInBufSize);
extern int    flush_oneframe_in_dpb_class_RkM4vDecoder(void *decoder, uint8_t* aOutBuffer, uint32_t *aOutputLength);
extern int    perform_seting_class_RkM4vDecoder(void * M4vDecoder,VPU_API_CMD cmd,void *param);

extern void*  get_class_RkVp8Decoder(void);
extern void   destroy_class_RkVp8Decoder(void * Vp8Decoder);
extern int    init_class_RkVp8Decoder(void * Vp8Decoder);
extern int    deinit_class_RkVp8Decoder(void * Vp8Decoder);
extern int    reset_class_RkVp8Decoder(void * Vp8Decoder);
extern int    dec_oneframe_class_RkVp8Decoder(void * Vp8Decoder,uint8_t* aOutBuffer, uint32_t *aOutputLength, uint8_t* aInputBuf, uint32_t* aInBufSize);
extern int    perform_seting_class_RkVp8Decoder(void * Vp8Decoder,VPU_API_CMD cmd,void *param);

extern void*  get_class_RkJpegDecoder(void);
extern void   destroy_class_RkJpegDecoder(void * JpegDecoder);
extern int    init_class_RkJpegDecoder(void * JpegDecoder);
extern int    deinit_class_RkJpegDecoder(void * JpegDecoder);
extern int    reset_class_RkJpegDecoder(void * JpegDecoder);
extern int    dec_oneframe_class_RkJpegDecoder(void * JpegDecoder, unsigned char* aOutBuffer, unsigned int *aOutputLength, unsigned char* aInputBuf, unsigned int* aInBufSize);
extern int    perform_seting_class_RkJpegDecoder(void * JpegDecoder,VPU_API_CMD cmd,void *param);

extern void*  get_class_RkAvcEncoder(void);
extern void   destroy_class_RkAvcEncoder(void * AvcEncoder);
extern int    init_class_RkAvcEncoder(void * AvcEncoder, EncParameter_t *aEncOption, uint8_t* aOutBuffer,uint32_t * aOutputLength);
extern int    deinit_class_RkAvcEncoder(void * AvcEncoder);
extern int    enc_oneframe_class_RkAvcEncoder(void * AvcEncoder, uint8_t* aOutBuffer, uint32_t * aOutputLength, uint8_t* aInputBuf,uint32_t  aInBuffPhy,uint32_t *aInBufSize,uint32_t * aOutTimeStamp, int* aSyncFlag);
extern void   set_config_class_RkAvcEncoder(void * AvcEncoder,EncParameter_t *vpug);
extern void   get_config_class_RkAvcEncoder(void * AvcEncoder,EncParameter_t *vpug);
extern int    set_idrframe_class_RkAvcEncoder(void * AvcEncoder);
extern int    set_inputformat_class_RkAvcEncoder(void * AvcEncoder,H264EncPictureType inputFormat);

extern void*  get_class_RkVp8Encoder(void);
extern void   destroy_class_RkVp8Encoder(void * Vp8Encoder);
extern int    init_class_RkVp8Encoder(void * Vp8Encoder, EncParameter_t *aEncOption, uint8_t* aOutBuffer,uint32_t* aOutputLength);
extern int    deinit_class_RkVp8Encoder(void * Vp8Encoder);
extern int    enc_oneframe_class_RkVp8Encoder(void * Vp8Encoder, uint8_t* aOutBuffer, uint32_t* aOutputLength,
                                     uint8_t *aInBuffer,uint32_t aInBuffPhy,uint32_t* aInBufSize,uint32_t* aOutTimeStamp, int *aSyncFlag);

extern void*  get_class_RkMjpegEncoder(void);
extern void   destroy_class_RkMjpegEncoder(void * MjpegEncoder);
extern int    init_class_RkMjpegEncoder(void * MjpegEncoder, EncParameter_t *aEncOption, unsigned char* aOutBuffer,unsigned int* aOutputLength);
extern int    deinit_class_RkMjpegEncoder(void * MjpegEncoder);
extern int    enc_oneframe_class_RkMjpegEncoder(void *MjpegEncoder, unsigned char* aOutBuffer, unsigned int* aOutputLength,
                                     unsigned char *aInBuffer,unsigned int aInBuffPhy,unsigned int* aInBufSize,unsigned int* aOutTimeStamp, int *aSyncFlag);
#ifdef __cplusplus
}
#endif

typedef struct tag_VPU_API {
    void* (*         get_class_RkDecoder)(void);
    void  (*     destroy_class_RkDecoder)(void *decoder);
    int   (*      deinit_class_RkDecoder)(void *decoder);
    int   (*        init_class_RkDecoder)(void *decoder);
    int   (*        init_class_RkDecoder_with_extra)(void *decoder, uint8_t* extra_data, uint32_t extra_size);
    int   (*        init_class_RkDecoder_M4V)(void *decoder, VPU_GENERIC *vpug);
    int   (*        init_class_RkDecoder_AVC)(void *decoder,int tsFlag);
    int   (*       reset_class_RkDecoder)(void *decoder);
    int   (*dec_oneframe_class_RkDecoder)(void *decoder, uint8_t* aOutBuffer, uint32_t *aOutputLength, uint8_t* aInputBuf, uint32_t* aInBufSize);
    int   (*dec_oneframe_class_RkDecoder_WithTimeStamp)(void *decoder, uint8_t* aOutBuffer, uint32_t *aOutputLength, uint8_t* aInputBuf, uint32_t* aInBufSize, int64_t* InputTimestamp);
    int   (*get_oneframe_class_RkDecoder)(void *decoder, uint8_t* aOutBuffer, uint32_t* aOutputLength);
    int   (*flush_oneframe_in_dpb_class_RkDecoder)(void *decoder, uint8_t* aOutBuffer, uint32_t *aOutputLength);
    int   (*perform_seting_class_RkDecoder)(void *decoder,VPU_API_CMD cmd,void *param);

	void* (*         get_class_RkEncoder)(void);
    void  (*     destroy_class_RkEncoder)(void *encoder);
    int   (*      deinit_class_RkEncoder)(void *encoder);
    int   (*        init_class_RkEncoder)(void *encoder, EncParameter_t *aEncOption, uint8_t *aOutBuffer, uint32_t* aOutputLength);
    int   (*enc_oneframe_class_RkEncoder)(void *encoder, uint8_t* aOutBuffer, uint32_t * aOutputLength,uint8_t* aInputBuf,uint32_t  aInBuffPhy,uint32_t *aInBufSize,uint32_t * aOutTimeStamp, int* aSyncFlag);
    void  (*enc_getconfig_class_RkEncoder)(void * AvcEncoder,EncParameter_t* vpug);
    void  (*enc_setconfig_class_RkEncoder)(void * AvcEncoder,EncParameter_t* vpug);
    int   (*enc_setInputFormat_class_RkEncoder)(void * AvcEncoder,H264EncPictureType inputFormat);
    int   (*enc_setIdrframe_class_RkEncoder)(void * AvcEncoder);
} VPU_API;

class VpuApiPrivate
{
public:
    VpuApiPrivate();
    ~VpuApiPrivate();

    static int32_t vpu_api_init(VPU_API *rk_api, uint32_t video_coding_type);

    int32_t video_fill_hdr(uint8_t *dst,uint8_t *src, uint32_t size,
            int64_t time, uint32_t type, uint32_t num);

    inline int32_t get_line(VpuApiCodecCfg_t *s, uint8_t *buf, uint32_t maxlen);
    inline int32_t read_chomp_line(VpuApiCodecCfg_t *s, uint8_t *buf, uint32_t maxlen);

    int32_t read_user_config();
    int32_t parseStagefrightCfgFile(VpuApiCodecCfg_t* codecCfg);

    RkCodecConfigure_t* getStagefrightCfgByCoding(CODEC_TYPE codec_type, uint32_t video_coding_type);
    RkCodecConfigure_t* getStagefrightCfgByName(CODEC_TYPE codec_type, uint8_t* name);
    int32_t isSupportUnderCfg(uint32_t video_coding_type);

    int32_t openDebugFile(const char *path);
    int32_t writeToDebugFile(uint8_t* data, uint32_t size);
    void    setVideoCoding(uint32_t coding);
    int32_t postProcessTimeStamp(int64_t *inTimeUs, int64_t *outTimeUs);
    void    resetIfNecessary();

    static uint8_t mHaveParseCfg;

private:
    VpuApiCodecCfg_t  mCodecCfg;
    uint32_t mCoding;
    FILE* mDebugFile;
    VpuApiTimeStampTrace_t mTimeTrace;
};

#endif

