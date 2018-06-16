#ifndef PV_JPEGDECAPI_H
#define PV_JPEGDECAPI_H

#include "jpegdecapi.h"

class Rk_JpegDecoder
{
    public:
         Rk_JpegDecoder(){};
        ~Rk_JpegDecoder() { };
        int pv_rkJpegdecoder_init();
        int pv_rkJpegdecoder_oneframe(unsigned char * aOutBuffer, unsigned int *aOutputLength,
        unsigned char* aInputBuf, unsigned int* aInBufSize, unsigned int out_phyaddr =0);
        int pv_rkJpegdecoder_rest();
        int pv_rkJpegdecoder_deinit();
        int32_t pv_rkJpeg_perform_seting(int cmd,void *param);
    private:
        JpegDecImageInfo imageInfo;
	    JpegDecInput jpegIn;
	    JpegDecOutput jpegOut;
	    JpegDecInst jpegInst;
        JpegDecRet jpegRet;
        VPUMemLinear_t streamMem;
        VPUMemLinear_t ppOutMem;
        
};


#endif /* PV_JPEGDECAPI_H */

