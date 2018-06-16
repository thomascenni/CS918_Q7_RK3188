#ifndef _PV_AVCENC_API_H_
#define _PV_AVCENC_API_H_

#include <stdint.h>

#include "H264TestBench.h"
#include "H264Instance.h"
#include "vpu_mem.h"
//#define ENC_DEBUG

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
   int  format;
   int	intraPicRate;
   int  framerateout;
   int  profileIdc;
   int  levelIdc;
   int	reserved[3];
}AVCEncParams1;


class Rk_AvcEncoder
{
    public:
         Rk_AvcEncoder();
        ~Rk_AvcEncoder() { };
        int pv_rkavcencoder_init(AVCEncParams1 *aEncOption, uint8_t* aOutBuffer, uint32_t* aOutputLength);
        int pv_rkavcencoder_oneframe(uint8_t* aOutBuffer, uint32_t*   aOutputLength,
        uint8_t *aInBuffer,uint32_t aInBuffPhy,uint32_t*   aInBufSize,uint32_t* aOutTimeStamp, bool*  aSyncFlag );
        void pv_rkavcencoder_setconfig(AVCEncParams1* vpug);
        void pv_rkavcencoder_getconfig(AVCEncParams1* vpug);
	int H264encSetInputFormat(H264EncPictureType inputFormat);
	void H264encSetintraPeriodCnt();
	void H264encSetInputAddr(unsigned long input);
        int pv_rkavcencoder_deinit();
	void h264encSetmbperslice(int line_per_slice);
	int h264encFramerateScale(int inframerate, int outframerate);
    private:
		int AllocRes( );
        void FreeRes();
    private:

        VPUMemLinear_t pictureMem ;
        VPUMemLinear_t pictureStabMem;
        VPUMemLinear_t outbufMem;
//      H264EncInst encoder;
        commandLine_s cmdl;
        H264EncIn encIn;
        H264EncOut encOut;
        H264EncRateCtrl rc;
        h264Instance_s h264encInst;
        int intraPeriodCnt, codedFrameCnt , next , src_img_size;
        u32 frameCnt;
        i32 InframeCnt;
		i32	preframnum;
        u32 streamSize;
        u32 bitrate;
        u32 psnrSum;
        u32 psnrCnt;
        u32 psnr;
#ifdef ENC_DEBUG
        FILE *fp;
        FILE *fp1;
#endif
};
#endif

