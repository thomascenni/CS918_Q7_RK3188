#ifndef _PV_VP8DECODER_H_
#define _PV_VP8DECODER_H_

#include "framemanager.h"
#include "vp8decoder.h"
#include "vpu.h"
#define OUT_TEM_NUM 6

class Rk_Vp8Decoder
{
    public:
         Rk_Vp8Decoder(){};
        ~Rk_Vp8Decoder() { };
        int pv_rkvp8decoder_init();
        int pv_rkvp8decoder_oneframe(uint8_t* aOutBuffer, uint32_t *aOutputLength,uint8_t* aInputBuf, uint32_t* aInBufSize);
        int pv_rkvp8decoder_deinit();
        int pv_rkvp8decoder_reset();
        int32_t pv_rkvp8_perform_seting(int cmd,void *param);
    private:
        vp8decoder       *vp8_dec;
#ifdef FRAME_COPY
       VPUMemLinear_t TESTMem[OUT_TEM_NUM];
       uint32_t FrameCount;
       uint32_t MemAllocFlag;
#endif
};


#endif


