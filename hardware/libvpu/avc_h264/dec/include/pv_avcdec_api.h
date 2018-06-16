#ifndef _PV_AVCDEC_API_H_
#define _PV_AVCDEC_API_H_

#include <android/Errors.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

namespace android {

typedef struct decContainer decContainer_t;
typedef struct VPUMem VPUMemLinear_t;

class Rk_AvcDecoder
{
public:
     Rk_AvcDecoder();
    ~Rk_AvcDecoder();
    int32_t pv_rkavcdecoder_init(uint32_t ts_en);
    int32_t pv_rkavcdecoder_oneframe(uint8_t* aOutBuffer, uint32_t *aOutputLength,
                                      uint8_t* aInputBuf,  uint32_t* aInBufSize,
                                      int64_t &InputTimestamp);
    int32_t pv_rkavc_flushOneFrameInDpb(uint8_t* aOutBuffer, uint32_t *aOutputLength);
    int32_t pv_rkavcdecoder_deinit();
    void    pv_rkavcdecoder_rest();
    int32_t pv_rkavcdecoder_perform(int32_t cmd, void *data);

private:
    int32_t isFlashPlayer;
    int32_t getOneFrame(uint8_t* aOutBuffer, uint32_t *aOutputLength, int64_t& InputTimestamp);
    int32_t prepareStream(uint8_t* aInputBuf, uint32_t aInBufSize);
    uint32_t status;
    uint32_t streamSize;
    VPUMemLinear_t *streamMem;
    decContainer_t *H264deccont;

//#define STREAM_DEBUG
#ifdef  STREAM_DEBUG
    FILE *fpStream;
#endif
//#define OUTPUT_DEBUG
#ifdef  OUTPUT_DEBUG
    FILE *fpOut;
#endif
//#define PTS_DEBUG
#ifdef  PTS_DEBUG
    FILE *pts_in;
    FILE *pts_out;
#endif
#define FLASH_DROP_FRAME
#ifdef FLASH_DROP_FRAME
	int32_t dropThreshold;
	int32_t curFrameNum;
#endif
};

}

#endif

