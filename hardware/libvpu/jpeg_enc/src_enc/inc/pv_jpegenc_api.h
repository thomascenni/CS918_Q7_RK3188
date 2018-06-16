#ifndef PV_JPEGENCAPI_H
#define PV_JPEGENCAPI_H

#include "jpegencapi.h"
#include "hw_jpegenc.h"

typedef struct
{
   int width;
   int height;
   int rc_mode;
   int bitRate;
   int framerate;
   int	qp;
   int	enableCabac;
   int	cabacInitIdc;
   int  inputFormat;
   int  intraPicRate;
   int	reserved[6];
}EncParams1;

class Rk_JpegEncoder
{
    public:
         Rk_JpegEncoder(){};
        ~Rk_JpegEncoder() { };
        int pv_Rk_Jpegencoder_init(EncParams1 *aEncOption);
        int pv_RkJpegencoder_oneframe(uint8_t* aOutBuffer, uint32_t* aOutputLength, uint8_t *aInBuffer,
        uint32_t aInBuffPhy,uint32_t* aInBufSize,uint32_t* aOutTimeStamp,bool*  aSyncFlag);
        int pv_RkJpegencoder_deinit();

    private:
        VPUMemLinear_t pictureMem;
        JpegEncInInfo  JpegInInfo;
        JpegEncOutInfo JpegOutInfo;
        VPUMemLinear_t outbufMem;

};


#endif /* PV_JPEGENCAPI_H */

