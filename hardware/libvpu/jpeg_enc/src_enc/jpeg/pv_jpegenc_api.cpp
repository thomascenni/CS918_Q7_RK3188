#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/timeb.h>


#include "jpegencapi.h"
#include "ewl.h"
#include "pv_jpegenc_api.h"
#include "vpu.h"
#include "vpu_mem.h"
#include "vpu_global.h"
#include <utils/Log.h>


#define DEFAULT -1
#define JPEG_INPUT_BUFFER 0x5120

#define PP_IN_FORMAT_YUV422INTERLAVE                0
#define PP_IN_FORMAT_YUV420SEMI                     1
#define PP_IN_FORMAT_YUV420PLANAR                   2
#define PP_IN_FORMAT_YUV400                         3
#define PP_IN_FORMAT_YUV422SEMI                     4
#define PP_IN_FORMAT_YUV420SEMITIELED               5
#define PP_IN_FORMAT_YUV440SEMI                     6
#define PP_IN_FORMAT_YUV444_SEMI                    7
#define PP_IN_FORMAT_YUV411_SEMI                    8

#define PP_OUT_FORMAT_RGB565                        0
#define PP_OUT_FORMAT_ARGB                          1
#define PP_OUT_FORMAT_YUV422INTERLAVE               3
#define PP_OUT_FORMAT_YUV420INTERLAVE               5

#define LocalPrint LOG


int Rk_JpegEncoder::pv_Rk_Jpegencoder_init(EncParams1 *aEncOption)
{

     memset((uint8_t*)(&JpegInInfo),0,sizeof(JpegEncInInfo));
	JpegInInfo.frameHeader = 1;
	JpegInInfo.rotateDegree = DEGREE_0;

	JpegInInfo.inputW = aEncOption->width;
	JpegInInfo.inputH = aEncOption->height;
	JpegInInfo.type =(JpegEncType)aEncOption->inputFormat;
	JpegInInfo.qLvl = aEncOption->qp;
	RK_U32 outbufSize =  1024*768*2;
	outbufMem.vir_addr = 0;
	memset(&outbufMem,0,sizeof(VPUMemLinear_t));
	int  ret = VPUMallocLinear(&outbufMem, outbufSize);
    if (ret != EWL_OK)
    {

        outbufMem.vir_addr = 0;
        return 1;
    }

    memset(&pictureMem,0,sizeof(VPUMemLinear_t));
    if(JpegInInfo.type <= 3)
    	ret = VPUMallocLinear(&pictureMem, (JpegInInfo.inputW * JpegInInfo.inputH*3)/2);
    else
        ret = VPUMallocLinear(&pictureMem, JpegInInfo.inputW * JpegInInfo.inputH*4);
    if (ret != EWL_OK)
    {
        pictureMem.vir_addr = 0;
        return 1;
    }

   return 0;
}

VPUMemLinear_t *gMem = 0;

int flush(int buf_type, int offset, int len){
	VPUMemFlush(gMem);
	return 0;
}


int Rk_JpegEncoder::pv_RkJpegencoder_oneframe(uint8_t* aOutBuffer, uint32_t* aOutputLength, uint8_t *aInBuffer,
        uint32_t aInBuffPhy,uint32_t* aInBufSize,uint32_t* aOutTimeStamp,bool*  aSyncFlag)
{

			int ret =1 ;
            if(!aInBuffPhy){
                if(JpegInInfo.type <= 3){
                	memcpy((uint8_t*)pictureMem.vir_addr, aInBuffer, JpegInInfo.inputW*JpegInInfo.inputH*3/2);
                }else{
                    memcpy((uint8_t*)pictureMem.vir_addr, aInBuffer, JpegInInfo.inputW*JpegInInfo.inputH*4);
                }
                VPUMemClean(&pictureMem);
                JpegInInfo.y_rgb_addr     = pictureMem.phy_addr;
			    JpegInInfo.uv_addr        = pictureMem.phy_addr + JpegInInfo.inputW*JpegInInfo.inputH;
            }else{
                JpegInInfo.y_rgb_addr     = aInBuffPhy;
			    JpegInInfo.uv_addr        = aInBuffPhy + JpegInInfo.inputW*JpegInInfo.inputH;
            }

			gMem = (VPUMemLinear_t *)&outbufMem;
            JpegOutInfo.outBufPhyAddr = outbufMem.phy_addr;
			JpegOutInfo.outBufVirAddr = (unsigned char*)outbufMem.vir_addr;
			JpegOutInfo.outBuflen     = 1024*768;

			JpegOutInfo.cacheflush    = flush;
			ret = hw_jpeg_encode(&JpegInInfo, &JpegOutInfo) ;

			if(ret < 0 || JpegOutInfo.jpegFileLen <= 0)
			{
				return -1 ;
			}
			else
			{
				*aOutputLength = JpegOutInfo.jpegFileLen;
				VPUMemInvalidate(&outbufMem);
				memcpy(aOutBuffer,outbufMem.vir_addr,JpegOutInfo.jpegFileLen);

			}
  return 0;
}


int Rk_JpegEncoder::pv_RkJpegencoder_deinit()
{
    if(outbufMem.phy_addr > 0)
        VPUFreeLinear(&outbufMem);
    if(pictureMem.phy_addr > 0)
        VPUFreeLinear(&pictureMem);

    return 0;
}

extern "C"
void *  get_class_RkMjpegEncoder(void)
{
    return (void*)new Rk_JpegEncoder();
}

extern "C"
void  destroy_class_RkMjpegEncoder(void * MjpegEncoder)
{
    delete (Rk_JpegEncoder *)MjpegEncoder;
    MjpegEncoder = NULL;
}

extern "C"
int init_class_RkMjpegEncoder(void * MjpegEncoder, EncParams1 *aEncOption, uint8_t* aOutBuffer,uint32_t* aOutputLength)
{
	Rk_JpegEncoder * Mjpegenc = (Rk_JpegEncoder *)MjpegEncoder;
	return Mjpegenc->pv_Rk_Jpegencoder_init(aEncOption);
}

extern "C"
int deinit_class_RkMjpegEncoder(void * MjpegEncoder)
{
	Rk_JpegEncoder * Mjpegenc = (Rk_JpegEncoder *)MjpegEncoder;
	return Mjpegenc->pv_RkJpegencoder_deinit();
}

extern "C"
int enc_oneframe_class_RkMjpegEncoder(void * MjpegEncoder, uint8_t* aOutBuffer, uint32_t* aOutputLength,
                                     uint8_t *aInBuffer,uint32_t aInBuffPhy,uint32_t* aInBufSize,uint32_t* aOutTimeStamp,bool*  aSyncFlag)
{
	int ret;

	Rk_JpegEncoder * Mjpegenc = (Rk_JpegEncoder *)MjpegEncoder;

	ret = Mjpegenc->pv_RkJpegencoder_oneframe(aOutBuffer, aOutputLength,aInBuffer,
                                            aInBuffPhy, aInBufSize, aOutTimeStamp, aSyncFlag);
	return ret;
}

