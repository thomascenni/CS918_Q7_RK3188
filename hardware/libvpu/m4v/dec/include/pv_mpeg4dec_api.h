#ifndef _PV_MPEG4DECODER_H_
#define _PV_MPEG4DECODER_H_

#include "mpeg4_rk_decoder.h"

class Rk_Mpeg4Decoder
{
    public:
         Rk_Mpeg4Decoder(){};
        ~Rk_Mpeg4Decoder() { };
        int pv_rkmpeg4decoder_init(VPU_GENERIC *vpug);
        int pv_rkmpeg4decoder_oneframe(uint8_t* aOutBuffer, uint32_t *aOutputLength,
        uint8_t* aInputBuf, uint32_t* aInBufSize);
        int pv_rkmpeg4_flushOneFrameInDpb(uint8_t* aOutBuffer, uint32_t *aOutputLength);
        int pv_rkmpeg4decoder_rest();
        int pv_rkmpeg4decoder_deinit();
		int32_t pv_rkmpeg4_perform_seting(int cmd,void *param);
    private:
        Mpeg4_Decoder       *mpeg4_dec;
#ifdef FRAME_COPY
       VPUMemLinear_t TESTMem[3];
       uint32_t FrameCount;
       uint32_t MemAllocFlag;
#endif
};


#endif


