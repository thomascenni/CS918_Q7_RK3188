#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <stdint.h>
#include <unistd.h>

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "log.h"
#include <sys/timeb.h>
#define LOG_TAG "Jpeg_api"
#include "log.h"

#include "jpegdecapi.h"
#include "dwl.h"
#include "jpegdeccontainer.h"

#include "deccfg.h"
#include "regdrv.h"

#include "hw_jpegdecapi.h"
#include "pv_jpegdec_api.h"

#include "vpu.h"
#include "vpu_mem.h"
#include "vpu_global.h"

#ifdef AVS40
#undef ALOGV
#define ALOGV LOGV

#undef ALOGD
#define ALOGD LOGD

#undef ALOGI
#define ALOGI LOGI

#undef ALOGW
#define ALOGW LOGW

#undef ALOGE
#define ALOGE LOGE

#endif

extern void PrintRet(JpegDecRet * pJpegRet);

#define DEFAULT -1
#define JPEG_INPUT_BUFFER 0x5120

#define PP_IN_FORMAT_YUV422INTERLAVE            0
#define PP_IN_FORMAT_YUV420SEMI                 1
#define PP_IN_FORMAT_YUV420PLANAR               2
#define PP_IN_FORMAT_YUV400                     3
#define PP_IN_FORMAT_YUV422SEMI                 4
#define PP_IN_FORMAT_YUV420SEMITIELED           5
#define PP_IN_FORMAT_YUV440SEMI                 6
#define PP_IN_FORMAT_YUV444_SEMI                7
#define PP_IN_FORMAT_YUV411_SEMI                8

#define PP_OUT_FORMAT_RGB565                    0
#define PP_OUT_FORMAT_ARGB                      1
#define PP_OUT_FORMAT_YUV422INTERLAVE           3   
#define PP_OUT_FORMAT_YUV420INTERLAVE           5

#define LocalPrint ALOG
#define LocalPrint(...)
int Rk_JpegDecoder::pv_rkJpegdecoder_init()
{
    int ret;
    JpegDecApiVersion decVer;
    JpegDecBuild decBuild;

    jpegIn.streamBuffer.pVirtualAddress = BLANK;
    jpegIn.streamBuffer.busAddress = 0;
    jpegIn.streamLength = 0;
    jpegIn.pictureBufferY.pVirtualAddress = BLANK;
    jpegIn.pictureBufferY.busAddress = 0;
    jpegIn.pictureBufferCbCr.pVirtualAddress = BLANK;
    jpegIn.pictureBufferCbCr.busAddress = 0;

    /* reset output */
    jpegOut.outputPictureY.pVirtualAddress = BLANK;
    jpegOut.outputPictureY.busAddress = 0;
    jpegOut.outputPictureCbCr.pVirtualAddress = BLANK;
    jpegOut.outputPictureCbCr.busAddress = 0;
    jpegOut.outputPictureCr.pVirtualAddress = BLANK;
    jpegOut.outputPictureCr.busAddress = 0;

    /* reset imageInfo */
    imageInfo.displayWidth = 0;
    imageInfo.displayHeight = 0;
    imageInfo.outputWidth = 0;
    imageInfo.outputHeight = 0;
    imageInfo.version = 0;
    imageInfo.units = 0;
    imageInfo.xDensity = 0;
    imageInfo.yDensity = 0;
    imageInfo.outputFormat = 0;
    imageInfo.thumbnailType = 0;
    imageInfo.displayWidthThumb = 0;
    imageInfo.displayHeightThumb = 0;
    imageInfo.outputWidthThumb = 0;
    imageInfo.outputHeightThumb = 0;
    imageInfo.outputFormatThumb = 0;

    /* set default */
    jpegIn.sliceMbSet = 0;
    jpegIn.bufferSize = 0;

	memset(&ppOutMem, 0, sizeof(ppOutMem));
	
    streamMem.vir_addr = BLANK;
    streamMem.phy_addr = 0;

    /* allocate memory for stream buffer. if unsuccessful -> exit */
	ret = VPUMallocLinear(&streamMem, 1024*100);
    if(ret != DWL_OK)
    {
    	return -1;
    }
    decVer = JpegGetAPIVersion();
    decBuild = JpegDecGetBuild();
	/* Jpeg initialization */
	jpegRet = JpegDecInit(&jpegInst);
	if(jpegRet != JPEGDEC_OK)
	{
		 LocalPrint(LOG_DEBUG,"jpeghw",("JpegDecInit Fail!!!\n"));
		PrintRet(&jpegRet);
		return -1;
	}    

    return 0;  
}

int Rk_JpegDecoder::pv_rkJpegdecoder_oneframe(unsigned char * aOutBuffer, unsigned int *aOutputLength,
        unsigned char* aInputBuf, unsigned int* aInBufSize, unsigned int out_phyaddr)
{
	JpegDecContainer *jpegC;
    int ret;
    
	int ppInputFomart = 0;
	RK_U32 bufsize;  /* input stream buffer size in bytes */
	RK_U32 imageInfoLength = 0;
	RK_U32 getDataLength = 0;
	RK_U32 mcuSizeDivider = 0,amountOfMCUs = 0, mcuInRow = 0;
	PostProcessInfo ppInfo;
	RK_U32 ppScaleW = 640, ppScaleH = 480;
	
	if(*aInBufSize >  1024*100)
	{
        VPUFreeLinear(&streamMem);
        ret = VPUMallocLinear(&streamMem, *aInBufSize);
        if(ret != DWL_OK)
        {
        	goto j_end;
        }
	}

    memcpy((RK_U8 *)streamMem.vir_addr, aInputBuf, *aInBufSize);
    VPUMemClean(&streamMem);
    VPUMemInvalidate(&streamMem);
	
    jpegIn.streamLength = *aInBufSize;
    jpegIn.bufferSize = *aInBufSize;
    jpegIn.pstreamMem = &streamMem;

	/* Pointer to the input JPEG */
	jpegIn.streamBuffer.pVirtualAddress = (RK_U32 *) streamMem.vir_addr;
	jpegIn.streamBuffer.busAddress = streamMem.phy_addr;
	/* Get image information of the JPEG image */
	jpegRet = JpegDecGetImageInfo(jpegInst, &jpegIn, &imageInfo);
	
	if(jpegRet != JPEGDEC_OK)
	{
	   LocalPrint(LOG_DEBUG,"jpeghw","JpegDecGetImageInfo fail error code is  %d\n", (int)jpegRet);
		goto j_end;
	}
	/* set pp info */
	memset(&ppInfo, 0, sizeof(ppInfo));
	ppInfo.outFomart = 5;	//PP_OUT_FORMAT_YUV420INTERLAVE
	ppScaleW = imageInfo.outputWidth;
	ppScaleH = imageInfo.outputHeight;
	if(ppScaleW > 1920) {// || ppScaleH > 1920) {
		ppScaleW = (ppScaleW + 15) & (~15);
		ppScaleH = (ppScaleH + 15) & (~15);
	} else {
		ppScaleW = (ppScaleW + 7) & (~7); // pp dest width must be dividable by 8
		ppScaleH = (ppScaleH + 1) & (~1); // must be dividable by 2.in pp downscaling ,the output lines always equal (desire lines - 1);
	}
	LocalPrint(LOG_DEBUG,"jpeghw","ppScalew:%d,ppSh:%d",ppScaleW,ppScaleH);
	
	jpegC = (JpegDecContainer*)jpegInst;
	jpegC->ppInstance = (void *)1;

	if(jpegC->ppInstance != NULL)
	{
		int timeout = 0;
		
		if(ppOutMem.phy_addr)
            VPUFreeLinear(&ppOutMem);

    	while(1)
    	{
            if (jpegC->vpumem_ctx) {
                ret = VPUMallocLinearFromRender(&ppOutMem, (ppScaleW*ppScaleH*2), jpegC->vpumem_ctx);
                if (ret !=DWL_OK) {
                    return (JPEGDEC_MEMFAIL);
                }
            } else {
                ret = VPUMallocLinear(&ppOutMem, (ppScaleW*ppScaleH*2));
            }
            if (ret ==DWL_OK) {
                break;
            }

            usleep(5000);
            timeout++;
            if(timeout > 0xFF)
            {
                ALOGE("MJPEG Malloc ppoutmem Failed \n");
    		    return (JPEGDEC_MEMFAIL);
            }
    	}
	}

    if(imageInfo.outputFormat)
    {
        switch (imageInfo.outputFormat)
        {
        case JPEGDEC_YCbCr400:
            LocalPrint(LOG_DEBUG,"jpeghw",(
                    "\t\t-JPEG: FULL RESOLUTION OUTPUT: JPEGDEC_YCbCr400\n"));
			ppInputFomart = PP_IN_FORMAT_YUV400;
            break;
        case JPEGDEC_YCbCr420_SEMIPLANAR:

			ppInputFomart = PP_IN_FORMAT_YUV420SEMI;
            break;
        case JPEGDEC_YCbCr422_SEMIPLANAR:
			ppInputFomart = PP_IN_FORMAT_YUV422SEMI;
            break;
        case JPEGDEC_YCbCr440:
            LocalPrint(LOG_DEBUG,"jpeghw",(
                    "\t\t-JPEG: FULL RESOLUTION OUTPUT: JPEGDEC_YCbCr440\n"));
			ppInputFomart = PP_IN_FORMAT_YUV440SEMI;
            break;
        case JPEGDEC_YCbCr411_SEMIPLANAR:
            LocalPrint(LOG_DEBUG,"jpeghw",(
                    "\t\t-JPEG: FULL RESOLUTION OUTPUT: JPEGDEC_YCbCr411_SEMIPLANAR\n"));
			ppInputFomart = PP_IN_FORMAT_YUV411_SEMI;
            break;
        case JPEGDEC_YCbCr444_SEMIPLANAR:
            LocalPrint(LOG_DEBUG,"jpeghw",(
                    "\t\t-JPEG: FULL RESOLUTION OUTPUT: JPEGDEC_YCbCr444_SEMIPLANAR\n"));
			ppInputFomart = PP_IN_FORMAT_YUV444_SEMI;
            break;
        }
    }
	
	LocalPrint("Jpeg set post process\n");
	SetPostProcessor(jpegC->jpegRegs, &ppOutMem, imageInfo.outputWidth, imageInfo.outputHeight,
					ppScaleW, ppScaleH, ppInputFomart, &ppInfo, out_phyaddr);
	LocalPrint(LOG_DEBUG,"jpeghw"," ppScaleW is %d, ppScaleH is %d. inInfo->ppInfo.outFomart is %d.\n", ppScaleW, ppScaleH, ppInfo.outFomart);
	
	/* Slice mode */
	jpegIn.sliceMbSet = 0; 	

	/* calculate MCU's */
	if(imageInfo.outputFormat == JPEGDEC_YCbCr400 ||
	   imageInfo.outputFormat == JPEGDEC_YCbCr444_SEMIPLANAR)
	{
		amountOfMCUs =
			((imageInfo.outputWidth * imageInfo.outputHeight) / 64);
		mcuInRow = (imageInfo.outputWidth / 8);
	}
	else if(imageInfo.outputFormat == JPEGDEC_YCbCr420_SEMIPLANAR)
	{
		/* 265 is the amount of luma samples in MB for 4:2:0 */
		amountOfMCUs =
			((imageInfo.outputWidth * imageInfo.outputHeight) / 256);
		mcuInRow = (imageInfo.outputWidth / 16);
	}
	else if(imageInfo.outputFormat == JPEGDEC_YCbCr422_SEMIPLANAR)
	{
		/* 128 is the amount of luma samples in MB for 4:2:2 */
		amountOfMCUs =
			((imageInfo.outputWidth * imageInfo.outputHeight) / 128);
		mcuInRow = (imageInfo.outputWidth / 16);
	}
	else if(imageInfo.outputFormat == JPEGDEC_YCbCr440)
	{
		/* 128 is the amount of luma samples in MB for 4:4:0 */
		amountOfMCUs =
			((imageInfo.outputWidth * imageInfo.outputHeight) / 128);
		mcuInRow = (imageInfo.outputWidth / 8);
	}
	else if(imageInfo.outputFormat == JPEGDEC_YCbCr411_SEMIPLANAR)
	{
		amountOfMCUs =
			((imageInfo.outputWidth * imageInfo.outputHeight) / 256);
		mcuInRow = (imageInfo.outputWidth / 32);
	}
	
	/* set mcuSizeDivider for slice size count */
	if(imageInfo.outputFormat == JPEGDEC_YCbCr400 ||
	   imageInfo.outputFormat == JPEGDEC_YCbCr440 ||
	   imageInfo.outputFormat == JPEGDEC_YCbCr444_SEMIPLANAR)
		mcuSizeDivider = 2;
	else
		mcuSizeDivider = 1;

	/* 9190 and over 16M ==> force to slice mode */
	if((jpegIn.sliceMbSet == 0) &&
	   ((imageInfo.outputWidth * imageInfo.outputHeight) >
		JPEGDEC_MAX_PIXEL_AMOUNT))
	{
		do
		{
			jpegIn.sliceMbSet++;
		}
		while(((jpegIn.sliceMbSet * (mcuInRow / mcuSizeDivider)) +
			   (mcuInRow / mcuSizeDivider)) <
			  JPEGDEC_MAX_SLICE_SIZE_8190);
		LocalPrint(LOG_DEBUG,"jpeghw","Force to slice mode (over 16M) ==> Decoder Slice MB Set %d\n",(int)jpegIn.sliceMbSet);
	}

	jpegIn.decImageType = 0; // FULL MODEs
	/* Decode JPEG */
	LocalPrint("Here Start The Real Jpeg Hardware Decoding\n");
	do
	{		
		jpegRet = JpegDecDecode(jpegInst, &jpegIn, &jpegOut);
		PrintRet(&jpegRet);

		if((imageInfo.outputFormat == JPEGDEC_YCbCr422_SEMIPLANAR)&&(jpegRet == JPEGDEC_FRAME_READY))
		{
			JpegDecContainer *jpegdec = (JpegDecContainer *)jpegInst;
			RK_U32 width = ((jpegdec->info.X+ 15) & 0xfffffff0);
			RK_U32 height = ((jpegdec->info.Y + 15) & 0xfffffff0);
			RK_U8 *chromabuff = (RK_U8 *)jpegdec->asicBuff.outLumaBuffer.vir_addr + width*height;
		}
		
		if( jpegRet == JPEGDEC_FRAME_READY)
		{
			if(out_phyaddr)
			{
				VPU_FRAME *frame  = (VPU_FRAME *)aOutBuffer;
	            
	            frame->FrameBusAddr[0] = out_phyaddr;
	            frame->FrameBusAddr[1] = frame->FrameBusAddr[0] +((ppScaleW+ 15) & 0xfffffff0) * ((ppScaleH + 15) & 0xfffffff0);

	            frame->DisplayWidth  = ppScaleW;
	            frame->DisplayHeight = ppScaleH;

	            *aOutputLength = sizeof(VPU_FRAME);
			}else
			{
	            VPU_FRAME *frame  = (VPU_FRAME *)aOutBuffer;
	            
	            frame->FrameBusAddr[0] = ppOutMem.phy_addr;
	            frame->FrameBusAddr[1] = frame->FrameBusAddr[0] +((ppScaleW+ 15) & 0xfffffff0) * ((ppScaleH + 15) & 0xfffffff0);

	            frame->DisplayWidth  = ppScaleW;
	            frame->DisplayHeight = ppScaleH;
	            
	            VPUMemDuplicate(&frame->vpumem,&ppOutMem);
	            *aOutputLength = sizeof(VPU_FRAME);
			}
		}
		else if( jpegRet == JPEGDEC_STRM_ERROR)
		{
			goto j_end;
		}
		else
		{
			LocalPrint(LOG_DEBUG,"jpeghw","DECODE ERROR\n");
			goto j_end;
		}
	}
	while(jpegRet != JPEGDEC_FRAME_READY );

j_end:
    if (jpegRet == JPEGDEC_UNSUPPORTED) {
        return JPEGDEC_UNSUPPORTED;
    }
	return 0;
    
}

int Rk_JpegDecoder::pv_rkJpegdecoder_rest()
{
    return 0;
}

int Rk_JpegDecoder::pv_rkJpegdecoder_deinit()
{
    if(streamMem.vir_addr != NULL)
        VPUFreeLinear(&streamMem);
	
	if(ppOutMem.vir_addr != NULL)
        VPUFreeLinear(&ppOutMem);
	
    hw_jpeg_release(jpegInst);
    
    return 0;
}

int32_t Rk_JpegDecoder::pv_rkJpeg_perform_seting(int cmd,void *param)
{
    int32_t ret = 0;
    JpegDecContainer *jpegC = (JpegDecContainer*)jpegInst;
    switch (cmd) {
        case VPU_API_SET_VPUMEM_CONTEXT:{
            if(jpegC != NULL){
                jpegC->vpumem_ctx = param;
            }else{
                ALOGE("MJPEG set contxt fail");
                ret = -1;
            }
        }break;
        default : {
            ret = -EINVAL;
        } break;
    }
    return ret;
}

extern "C"
void *  get_class_RkJpegDecoder(void)
{
    return (void*)new Rk_JpegDecoder();
}
extern "C"
void  destroy_class_RkJpegDecoder(void * JpegDecoder)
{
    delete (Rk_JpegDecoder *)JpegDecoder;
}
extern "C"
int init_class_RkJpegDecoder(void * JpegDecoder)
{
    Rk_JpegDecoder * Jpegdec = (Rk_JpegDecoder *)JpegDecoder;
    return Jpegdec->pv_rkJpegdecoder_init();
}
extern "C"
int deinit_class_RkJpegDecoder(void * JpegDecoder)
{
    Rk_JpegDecoder * Jpegdec = (Rk_JpegDecoder *)JpegDecoder;
    return Jpegdec->pv_rkJpegdecoder_deinit();
}
extern "C"
int reset_class_RkJpegDecoder(void * JpegDecoder)
{
    Rk_JpegDecoder * Jpegdec = (Rk_JpegDecoder *)JpegDecoder;
    return Jpegdec->pv_rkJpegdecoder_rest();
}
extern "C"
int dec_oneframe_RkJpegDecoder(void * JpegDecoder,uint8_t* aOutBuffer, uint32_t *aOutputLength,
        uint8_t* aInputBuf, uint32_t* aInBufSize, uint32_t out_phyaddr)
{
    int ret;
    Rk_JpegDecoder * Jpegdec = (Rk_JpegDecoder *)JpegDecoder;
    ret = Jpegdec->pv_rkJpegdecoder_oneframe(aOutBuffer,aOutputLength,
                    aInputBuf,aInBufSize, out_phyaddr);
    return ret;
}

extern "C"
int dec_oneframe_class_RkJpegDecoder(void * JpegDecoder,uint8_t* aOutBuffer, uint32_t *aOutputLength,
        uint8_t* aInputBuf, uint32_t* aInBufSize)
{
    int ret;
    Rk_JpegDecoder * Jpegdec = (Rk_JpegDecoder *)JpegDecoder;
    ret = Jpegdec->pv_rkJpegdecoder_oneframe(aOutBuffer,aOutputLength,
                    aInputBuf,aInBufSize, 0);
    return ret;
}
extern "C"
int perform_seting_class_RkJpegDecoder(void * JpegDecoder,VPU_API_CMD cmd,void *param){
    Rk_JpegDecoder * Jpegdec = (Rk_JpegDecoder *)JpegDecoder;
    int ret = 0;
    if(Jpegdec != NULL){
        ret = Jpegdec->pv_rkJpeg_perform_seting(cmd, param);
    }else{
        ret = -1;
    }
    return 0;
}
