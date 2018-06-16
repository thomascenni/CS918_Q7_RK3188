/*------------------------------------------------------------------------------
--                                                                            --
--       This software is confidential and proprietary and may be used        --
--        only as expressly authorized by a licensing agreement from          --
--                                                                            --
--                            Hantro Products Oy.                             --
--                                                                            --
--                   (C) COPYRIGHT 2006 HANTRO PRODUCTS OY                    --
--                            ALL RIGHTS RESERVED                             --
--                                                                            --
--                 The entire notice above must be reproduced                 --
--                  on all copies and should not be removed.                  --
--                                                                            --
--------------------------------------------------------------------------------
--
--  Abstract : VP8 Encoder testbench for linux
--
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    1. Include headers
------------------------------------------------------------------------------*/

/* For SW/HW shared memory allocation */
#include "ewl.h"

/* For accessing the EWL instance inside the encoder */
//#include "vp8instance.h"

/* For compiler flags, test data, debug and tracing */
#include "enccommon.h"

/* For Hantro VP8 encoder */
#include "vp8encapi.h"

/* For printing and file IO */
#include <stdio.h>

/* For dynamic memory allocation */
#include <stdlib.h>

/* For memset, strcpy and strlen */
#include <string.h>

#include "pv_vp8enc_api.h"
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

/*------------------------------------------------------------------------------
    2. External compiler flags
--------------------------------------------------------------------------------

NO_OUTPUT_WRITE: Output stream is not written to file. This should be used
                 when running performance simulations.
NO_INPUT_READ:   Input frames are not read from file. This should be used
                 when running performance simulations.
PSNR:            Enable PSNR calculation with --psnr option, only works with
                 system model

--------------------------------------------------------------------------------
    3. Module defines
------------------------------------------------------------------------------*/

#define VP8ERR_OUTPUT stdout

/* Value for parameter to use API default */
#define DEFAULT -100

/* Intermediate Video File Format */
#define IVF_HDR_BYTES       32
#define IVF_FRM_BYTES       12

#ifdef PSNR
    float log10f(float x);
    float roundf(float x);
#endif

/*------------------------------------------------------------------------------
    4. Local function prototypes
------------------------------------------------------------------------------*/


Rk_Vp8Encoder:: Rk_Vp8Encoder()
{
	intraPeriodCnt = 0;
	codedFrameCnt = 0;
	src_img_size = 0;
	next = 0;
	encoder = NULL;

	memset(&cmdl, 0, sizeof(cmdl));
	memset(&encIn, 0, sizeof(encIn));
}

int Rk_Vp8Encoder::pv_rkvp8encoder_init(EncParams1 *aEncOption, uint8_t* aOutBuffer, uint32_t* aOutputLength)
{
    i32 ret;
	testbench_s *cml = &cmdl;

	//ALOGE("pv_rkvp8encoder_init");

     /* Parse command line parameters */
    if(Parameter(cml) != 0)
    {
        fprintf(VP8ERR_OUTPUT, "Input parameter error\n");
        return -1;
    }

	//input width and height
	cml->lumWidthSrc		= aEncOption->width;
	cml->lumHeightSrc		= aEncOption->height;
	cml->width				= aEncOption->width;
	cml->height 			= aEncOption->height;
	cml->bitPerSecond		= aEncOption->bitRate;					 //Bitrate, 10000..levelMax [64000]\n"
	cml->outputRateNumer	= aEncOption->framerate;
	cml->inputRateNumer 	= aEncOption->framerate;

    /* Encoder initialization */
    if((ret = OpenEncoder(cml, &encoder)) != 0)
    {
        return -ret;    /* Return positive value for test scripts */
    }

    /* Set the test ID for internal testing,
     * the SW must be compiled with testing flags */
    VP8EncSetTestId(encoder, cml->testId);

    /* Allocate input and output buffers */
    if(AllocRes(cml, encoder) != 0)
    {
        fprintf(VP8ERR_OUTPUT, "Failed to allocate the external resources!\n");
        FreeRes(cml, encoder);
        CloseEncoder(encoder);
        return -VP8ENC_MEMORY_ERROR;
    }

	encIn.pOutBuf = cml->outbufMem.vir_addr;
	encIn.busOutBuf = cml->outbufMem.phy_addr;
	encIn.outBufSize = cml->outbufMem.size;

	IvfHeader(cml, aOutBuffer);
	cml->streamSize = IVF_HDR_BYTES;
	PrintTitle(cml);

	/* First frame is always intra with zero time increment */
	encIn.codingType = VP8ENC_INTRA_FRAME;
	encIn.timeIncrement = 0;
	encIn.busLumaStab = cml->pictureStabMem.phy_addr;

	*aOutputLength = cml->streamSize;

	//LOGE("pv_rkvp8encoder_init end %d", cml->streamSize);
	return 0;
}

int Rk_Vp8Encoder::pv_rkvp8encoder_oneframe(uint8_t* aOutBuffer, uint32_t* aOutputLength,
 uint8_t *aInBuffer, uint32_t aInBuffPhy,uint32_t*	aInBufSize,uint32_t* aOutTimeStamp, bool*  aSyncFlag)
{
	VP8EncRet ret;
	testbench_s *cml = &cmdl;

	//ALOGE("pv_rkvp8encoder_oneframe");

/* Set the window length for bitrate moving average calculation */
    if(!aInBuffPhy){
        memcpy((u8 *)cml->pictureMem.vir_addr, aInBuffer,  cml->lumWidthSrc*cml->lumHeightSrc *3/2);
        VPUMemClean(&cml->pictureMem);
    }
	cml->ma.pos = cml->ma.count = 0;
	cml->ma.frameRateNumer = cml->outputRateNumer;
	cml->ma.frameRateDenom = cml->outputRateDenom;
	if (cml->outputRateDenom)
		cml->ma.length = MAX(1, MIN(cml->outputRateNumer / cml->outputRateDenom,
								MOVING_AVERAGE_FRAMES));
	else
		cml->ma.length = MOVING_AVERAGE_FRAMES;

	encIn.pOutBuf = cml->outbufMem.vir_addr;
	encIn.busOutBuf = cml->outbufMem.phy_addr;
	encIn.outBufSize = cml->outbufMem.size;

	/* Source Image Size */
	if(cml->inputFormat <= 1)
	{
		src_img_size = cml->lumWidthSrc * cml->lumHeightSrc +
			2 * (((cml->lumWidthSrc + 1) >> 1) *
				 ((cml->lumHeightSrc + 1) >> 1));
	}
	else if((cml->inputFormat <= 9) || (cml->inputFormat == 14))
	{
		/* 422 YUV or 16-bit RGB */
		src_img_size = cml->lumWidthSrc * cml->lumHeightSrc * 2;
	}
	else
	{
		/* 32-bit RGB */
		src_img_size = cml->lumWidthSrc * cml->lumHeightSrc * 4;
	}

	ALOGV("Reading input from file <%s>, frame size %d bytes.\n",
			cml->input, src_img_size);

	//cml->streamSize = 0;

	VP8EncGetRateCtrl(encoder, &cml->rc);

	/* Setup encoder input */
	{
		u32 w = (cml->lumWidthSrc + 15) & (~0x0f);
        if(aInBuffPhy){
		    encIn.busLuma = aInBuffPhy;//cml->pictureMem.phy_addr;
        }else{
            encIn.busLuma = cml->pictureMem.phy_addr;
        }
		encIn.busChromaU = encIn.busLuma + (w * cml->lumHeightSrc);
		encIn.busChromaV = encIn.busChromaU +
			(((w + 1) >> 1) * ((cml->lumHeightSrc + 1) >> 1));
	}


	/* Main encoding loop */
	/* Select frame type */
	if((cml->intraPicRate != 0) && (intraPeriodCnt >= cml->intraPicRate))
		encIn.codingType = VP8ENC_INTRA_FRAME;
	else
		encIn.codingType = VP8ENC_PREDICTED_FRAME;

	if(encIn.codingType == VP8ENC_INTRA_FRAME)
		intraPeriodCnt = 0;

	/* This applies for PREDICTED frames only. By default always predict
	 * from the previous frame only. */
	encIn.ipf = VP8ENC_REFERENCE_AND_REFRESH;
	encIn.grf = encIn.arf = VP8ENC_REFERENCE;

	/* Force odd frames to be coded as droppable. */
	if (cml->droppable && cml->frameCnt&1) {
		encIn.codingType = VP8ENC_PREDICTED_FRAME;
		encIn.ipf = encIn.grf = encIn.arf = VP8ENC_REFERENCE;
	}

	//SetFrameInternalTest(encoder, cml);
	VPUMemInvalidate(&cml->outbufMem);

	/* Encode one frame */
	ret = VP8EncStrmEncode(encoder, &encIn, &cml->encOut, cml);

	//ALOGE("RET=%d", ret);

	VP8EncGetRateCtrl(encoder, &cml->rc);

	if (cml->encOut.frameSize)
		cml->streamSize += (IVF_FRM_BYTES + cml->encOut.frameSize);
	MaAddFrame(&cml->ma, cml->encOut.frameSize*8);

	switch (ret)
	{
	case VP8ENC_FRAME_READY:
		PrintFrame(cml, encoder, next, ret);
		if(cml->encOut.codingType == VP8ENC_INTRA_FRAME)
            *aSyncFlag = 1;
        else
        	*aSyncFlag = 0;

		VPUMemInvalidate(&cml->outbufMem);
		*aOutputLength = 0;
		memcpy(aOutBuffer, cml->encOut.pOutBuf[0], cml->encOut.streamSize[0]);
		aOutBuffer += cml->encOut.streamSize[0];
		*aOutputLength += cml->encOut.streamSize[0];
		memcpy(aOutBuffer, cml->encOut.pOutBuf[1], cml->encOut.streamSize[1]);
		aOutBuffer += cml->encOut.streamSize[1];
		*aOutputLength += cml->encOut.streamSize[1];
		memcpy(aOutBuffer, cml->encOut.pOutBuf[2], cml->encOut.streamSize[2]);
		aOutBuffer += cml->encOut.streamSize[2];
		*aOutputLength += cml->encOut.streamSize[2];

		ALOGV("streamSize=%d, %d, %d", cml->encOut.streamSize[0], cml->encOut.streamSize[1], cml->encOut.streamSize[2]);
		break;

	case VP8ENC_OUTPUT_BUFFER_OVERFLOW:
		PrintFrame(cml, encoder, next, ret);
		break;

	default:
		PrintErrorValue("VP8EncStrmEncode() failed.", ret);
		PrintFrame(cml, encoder, next, ret);
		break;
	}

	encIn.timeIncrement = cml->outputRateDenom;

	cml->frameCnt++;
	cml->frameCntTotal++;

	if (cml->encOut.codingType != VP8ENC_NOTCODED_FRAME) {
		intraPeriodCnt++; codedFrameCnt++;
	}
	/* End of main encoding loop */

	return ret;
}


int Rk_Vp8Encoder::pv_rkvp8encoder_deinit()
{
	u8 out[64];
	testbench_s *cml = &cmdl;


	/* Print information about encoded frames */
	ALOGV("\nBitrate target %d bps, actual %d bps (%d%%).\n",
			cml->rc.bitPerSecond, cml->bitrate,
			(cml->rc.bitPerSecond) ? cml->bitrate*100/cml->rc.bitPerSecond : 0);
	ALOGV("Total of %llu frames processed, %d frames encoded, %d bytes.\n",
			cml->frameCntTotal, codedFrameCnt, cml->streamSize);

	if (cml->psnrCnt)
		ALOGV("Average PSNR %d.%02d\n",
			(cml->psnrSum/cml->psnrCnt)/100, (cml->psnrSum/cml->psnrCnt)%100);
	/* Rewrite ivf header with correct frame count */
	IvfHeader(cml, out);

    FreeRes(cml, encoder);

    CloseEncoder(encoder);

	//ALOGE("pv_rkvp8encoder_deinit end");

	return 0;
}

void Rk_Vp8Encoder::VP8encSetintraPeriodCnt()
{
	intraPeriodCnt = cmdl.intraPicRate;
	
	return;
}

int Rk_Vp8Encoder::VP8encSetInputFormat(VP8EncPictureType inputFormat)
{    
	VP8EncPreProcessingCfg preProcCfg;
	VP8EncRet ret;
	
	if((ret = VP8EncGetPreProcessing(encoder, &preProcCfg)) != VP8ENC_OK)    
	{     
		PrintErrorValue("VP8EncGetPreProcessing() failed.", ret);        
		return -1;    
	}    
	if(inputFormat!=preProcCfg.inputType)
		intraPeriodCnt = cmdl.intraPicRate;
	preProcCfg.inputType = (VP8EncPictureType)inputFormat;                
	if((ret = VP8EncSetPreProcessing(encoder, &preProcCfg)) != VP8ENC_OK)
	{        
		PrintErrorValue("VP8EncSetPreProcessing() failed.", ret);        
		return -1;    
	}    
	
	return 0;
}

void Rk_Vp8Encoder::pv_rkvp8encoder_getconfig(EncParams1 *vpug)
{
	vpug->width 	= cmdl.lumWidthSrc;
	vpug->height	= cmdl.lumHeightSrc;
	vpug->bitRate 	= cmdl.bitPerSecond;                   //Bitrate, 10000..levelMax [64000]\n"
    vpug->rc_mode 	= cmdl.picRc;                           //0=OFF, 1=ON Picture rate control. [1]\n"
    vpug->framerate = cmdl.outputRateNumer ;
	vpug->qp 		= cmdl.fixedIntraQp;
	
	memset(vpug->reserved, 0 ,sizeof(vpug->reserved));
	
	ALOGV("pv_rkavcencoder_getconfig bitrate w %d h %d %d rc %d fps %d",
		vpug->width,vpug->height,vpug->bitRate,vpug->rc_mode,vpug->framerate);

	return ;
}

void Rk_Vp8Encoder::pv_rkvp8encoder_setconfig(EncParams1 *vpug)
{
    u32 testid = 0;
	
   // ALOGE("pv_rkavcencoder_setconfig");

	cmdl.bitPerSecond  = vpug->bitRate;					 //Bitrate, 10000..levelMax [64000]\n"
    cmdl.picRc = vpug->rc_mode;                           //0=OFF, 1=ON Picture rate control. [1]\n"

    cmdl.outputRateNumer = vpug->framerate;
    cmdl.inputRateNumer = vpug->framerate;
    
    VP8EncGetRateCtrl(encoder, &cmdl.rc);
	
    if(cmdl.rc.bitPerSecond != cmdl.bitPerSecond)
    {
        cmdl.rc.bitPerSecond = cmdl.bitPerSecond;
        testid = 1;
    }
    
    if(cmdl.rc.pictureRc != cmdl.picRc)
    {
        cmdl.rc.pictureRc = cmdl.picRc;
        testid = 1;
    }
    
    if(cmdl.rc.outRateNum != cmdl.outputRateNumer)
    {
        cmdl.rc.outRateNum = cmdl.outputRateNumer;
        testid = 1;
    }
	
    int ret;
	
    if(testid)
        ret = VP8EncSetRateCtrl(encoder, &cmdl.rc);
	
	ALOGV("pv_rkavcencoder_setconfig bitrate w %d h %d bitrate %d rc %d fps %d  qphdr %d qpmax %d qpmin %d intraqp %d ret %d fixintraqp %d testid %d",
	vpug->width,vpug->height,vpug->bitRate,vpug->rc_mode,vpug->framerate,cmdl.qpHdr,cmdl.qpMin,cmdl.qpMax,cmdl.intraQpDelta ,ret,
    cmdl.fixedIntraQp,testid);

	return ;
}

/*------------------------------------------------------------------------------

    AllocRes

    Allocation of the physical memories used by both SW and HW:
    the input pictures and the output stream buffer.

    NOTE! The implementation uses the EWL instance from the encoder
          for OS independence. This is not recommended in final environment
          because the encoder will release the EWL instance in case of error.
          Instead, the memories should be allocated from the OS the same way
          as inside EWLMallocLinear().

------------------------------------------------------------------------------*/
int Rk_Vp8Encoder::AllocRes(testbench_s * cml, VP8EncInst enc)
{
    i32 ret;
    u32 pictureSize;
    u32 outbufSize;

    if(cml->inputFormat <= 1)
    {
        /* Input picture in planar YUV 4:2:0 format */
        pictureSize =
            ((cml->lumWidthSrc + 15) & (~15)) * cml->lumHeightSrc * 3 / 2;
    }
    else if((cml->inputFormat <= 9) || (cml->inputFormat == 14))
    {
        /* Input picture in YUYV 4:2:2 or 16-bit RGB format */
        pictureSize =
            ((cml->lumWidthSrc + 15) & (~15)) * cml->lumHeightSrc * 2;
    }
    else
    {
        /* Input picture in 32-bit RGB format */
        pictureSize =
            ((cml->lumWidthSrc + 15) & (~15)) * cml->lumHeightSrc * 4;
    }

    ALOGV("Input %dx%d encoding at %dx%d\n", cml->lumWidthSrc,
           cml->lumHeightSrc, cml->width, cml->height);

    cml->pictureMem.vir_addr = NULL;
    cml->outbufMem.vir_addr = NULL;
    cml->pictureStabMem.vir_addr = NULL;

    /* Here we use the EWL instance directly from the encoder
     * because it is the easiest way to allocate the linear memories */
    ret = VPUMallocLinear(&cml->pictureMem, pictureSize);
    if (ret != EWL_OK)
    {
        fprintf(VP8ERR_OUTPUT, "Failed to allocate input picture!\n");
        cml->pictureMem.vir_addr = NULL;
        return 1;
    }

    if(cml->videoStab > 0)
    {
        ret = VPUMallocLinear(&cml->pictureStabMem, pictureSize);
        if (ret != EWL_OK)
        {
            fprintf(VP8ERR_OUTPUT, "Failed to allocate stab input picture!\n");
            cml->pictureStabMem.vir_addr = NULL;
            return 1;
        }
    }
	outbufSize = cml->pictureMem.size/2;	//output buffer half of pictureMem, that is enough

    ret = VPUMallocLinear(&cml->outbufMem, outbufSize);
    if (ret != EWL_OK)
    {
        fprintf(VP8ERR_OUTPUT, "Failed to allocate output buffer!\n");
        cml->outbufMem.vir_addr = NULL;
        return 1;
    }

    ALOGV("Input buffer size:          %d bytes\n", cml->pictureMem.size);
    ALOGV("Input buffer bus address:   0x%08x\n", cml->pictureMem.phy_addr);
    ALOGV("Input buffer user address:  0x%08x\n",
                                        (u32) cml->pictureMem.vir_addr);
    ALOGV("Output buffer size:         %d bytes\n", cml->outbufMem.size);
    ALOGV("Output buffer bus address:  0x%08x\n", cml->outbufMem.phy_addr);
    ALOGV("Output buffer user address: 0x%08x\n",
                                        (u32) cml->outbufMem.vir_addr);

    return 0;
}

/*------------------------------------------------------------------------------

    FreeRes

    Release all resources allcoated byt AllocRes()

------------------------------------------------------------------------------*/
void Rk_Vp8Encoder::FreeRes(testbench_s * cml, VP8EncInst enc)
{
    if(cml->pictureMem.vir_addr != NULL)
        VPUFreeLinear(&cml->pictureMem);
    if(cml->pictureStabMem.vir_addr != NULL)
        VPUFreeLinear(&cml->pictureStabMem);
    if(cml->outbufMem.vir_addr != NULL)
        VPUFreeLinear(&cml->outbufMem);
}


/*------------------------------------------------------------------------------

    main

------------------------------------------------------------------------------*/
/*int main(int argc, char *argv[])
{
	return 0;
}
*/

void SetFrameInternalTest(VP8EncInst encoder, testbench_s *cml)
{
    /* Set some values outside the product API for internal testing purposes. */

#ifdef INTERNAL_TEST
    if (cml->intra16Favor)
        ((vp8Instance_s *)encoder)->asic.regs.intra16Favor = cml->intra16Favor;
    if (cml->intraPenalty)
        ((vp8Instance_s *)encoder)->asic.regs.interFavor = cml->intraPenalty;
    if (cml->traceRecon)
        ((vp8Instance_s *)encoder)->asic.traceRecon = 1;
#endif
}


/*------------------------------------------------------------------------------

    OpenEncoder
        Create and configure an encoder instance.

    Params:
        cml     - processed comand line options
        pEnc    - place where to save the new encoder instance
    Return:
        0   - for success
        -1  - error

------------------------------------------------------------------------------*/
int OpenEncoder(testbench_s * cml, VP8EncInst * pEnc)
{
    VP8EncRet ret;
    VP8EncConfig cfg;
    VP8EncCodingCtrl codingCfg;
    VP8EncRateCtrl rcCfg;
    VP8EncPreProcessingCfg preProcCfg;

    VP8EncInst encoder;

    /* Default resolution, try parsing input file name */
    if(cml->lumWidthSrc == DEFAULT || cml->lumHeightSrc == DEFAULT)
    {
        if (GetResolution(cml->input, &cml->lumWidthSrc, &cml->lumHeightSrc))
        {
            /* No dimensions found in filename, using default QCIF */
            cml->lumWidthSrc = 176;
            cml->lumHeightSrc = 144;
        }
    }

    /* input resolution == encoded resolution if not defined */
    if(cml->width == DEFAULT)
        cml->width = cml->lumWidthSrc;
    if(cml->height == DEFAULT)
        cml->height = cml->lumHeightSrc;

    /* output frame rate == input frame rate if not defined */
    if(cml->outputRateNumer == DEFAULT)
        cml->outputRateNumer = cml->inputRateNumer;
    if(cml->outputRateDenom == DEFAULT)
        cml->outputRateDenom = cml->inputRateDenom;

    if(cml->rotation) {
        cfg.width = cml->height;
        cfg.height = cml->width;
    } else {
        cfg.width = cml->width;
        cfg.height = cml->height;
    }

    cfg.frameRateDenom = cml->outputRateDenom;
    cfg.frameRateNum = cml->outputRateNumer;
    cfg.refFrameAmount = cml->refFrameAmount;

    ALOGV("Init config: size %dx%d   %d/%d fps  %d refFrames\n",
         cfg.width, cfg.height, cfg.frameRateNum, cfg.frameRateDenom,
         cfg.refFrameAmount);

    if((ret = VP8EncInit(&cfg, pEnc)) != VP8ENC_OK)
    {
        PrintErrorValue("VP8EncInit() failed.", ret);
        return ret;
    }

    encoder = *pEnc;

    /* Encoder setup: rate control */
    if((ret = VP8EncGetRateCtrl(encoder, &rcCfg)) != VP8ENC_OK)
    {
        PrintErrorValue("VP8EncGetRateCtrl() failed.", ret);
        CloseEncoder(encoder);
        return -1;
    }
    else
    {
        ALOGV("Get rate control: qp=%2d [%2d..%2d] %8d bps,"
                " picRc=%d gop=%d\n",
                rcCfg.qpHdr, rcCfg.qpMin, rcCfg.qpMax, rcCfg.bitPerSecond,
                rcCfg.pictureRc, rcCfg.bitrateWindow);

        if (cml->picRc != DEFAULT)
            rcCfg.pictureRc = cml->picRc;
        if (cml->picSkip != DEFAULT)
            rcCfg.pictureSkip = cml->picSkip;
        if (cml->qpHdr != DEFAULT)
            rcCfg.qpHdr = cml->qpHdr;
        if (cml->qpMin != DEFAULT)
            rcCfg.qpMin = cml->qpMin;
        if (cml->qpMax != DEFAULT)
            rcCfg.qpMax = cml->qpMax;
        if (cml->bitPerSecond != DEFAULT)
            rcCfg.bitPerSecond = cml->bitPerSecond;
        if (cml->gopLength != DEFAULT)
            rcCfg.bitrateWindow = cml->gopLength;
        if (cml->intraQpDelta != DEFAULT)
            rcCfg.intraQpDelta = cml->intraQpDelta;
        if (cml->fixedIntraQp != DEFAULT)
            rcCfg.fixedIntraQp = cml->fixedIntraQp;

        ALOGV("Set rate control: qp=%2d [%2d..%2d] %8d bps,"
                " picRc=%d gop=%d\n",
                rcCfg.qpHdr, rcCfg.qpMin, rcCfg.qpMax, rcCfg.bitPerSecond,
                rcCfg.pictureRc, rcCfg.bitrateWindow);

        if((ret = VP8EncSetRateCtrl(encoder, &rcCfg)) != VP8ENC_OK)
        {
            PrintErrorValue("VP8EncSetRateCtrl() failed.", ret);
            CloseEncoder(encoder);
            return -1;
        }
    }

    /* Encoder setup: coding control */
    if((ret = VP8EncGetCodingCtrl(encoder, &codingCfg)) != VP8ENC_OK)
    {
        PrintErrorValue("VP8EncGetCodingCtrl() failed.", ret);
        CloseEncoder(encoder);
        return -1;
    }
    else
    {
        if (cml->dctPartitions != DEFAULT)
            codingCfg.dctPartitions = cml->dctPartitions;
        if (cml->errorResilient != DEFAULT)
            codingCfg.errorResilient = cml->errorResilient;
        if (cml->ipolFilter != DEFAULT)
            codingCfg.interpolationFilter = cml->ipolFilter;
        if (cml->filterType != DEFAULT)
            codingCfg.filterType = cml->filterType;
        if (cml->filterLevel != DEFAULT)
            codingCfg.filterLevel = cml->filterLevel;
        if (cml->filterSharpness != DEFAULT)
            codingCfg.filterSharpness = cml->filterSharpness;
        if (cml->quarterPixelMv != DEFAULT)
            codingCfg.quarterPixelMv = cml->quarterPixelMv;
        if (cml->splitMv != DEFAULT)
            codingCfg.splitMv = cml->splitMv;

        codingCfg.cirStart = cml->cirStart;
        codingCfg.cirInterval = cml->cirInterval;
        codingCfg.intraArea.enable = cml->intraAreaEnable;
        codingCfg.intraArea.top = cml->intraAreaTop;
        codingCfg.intraArea.left = cml->intraAreaLeft;
        codingCfg.intraArea.bottom = cml->intraAreaBottom;
        codingCfg.intraArea.right = cml->intraAreaRight;
        codingCfg.roi1Area.enable = cml->roi1AreaEnable;
        codingCfg.roi1Area.top = cml->roi1AreaTop;
        codingCfg.roi1Area.left = cml->roi1AreaLeft;
        codingCfg.roi1Area.bottom = cml->roi1AreaBottom;
        codingCfg.roi1Area.right = cml->roi1AreaRight;
        codingCfg.roi2Area.enable = cml->roi2AreaEnable;
        codingCfg.roi2Area.top = cml->roi2AreaTop;
        codingCfg.roi2Area.left = cml->roi2AreaLeft;
        codingCfg.roi2Area.bottom = cml->roi2AreaBottom;
        codingCfg.roi2Area.right = cml->roi2AreaRight;
        codingCfg.roi1DeltaQp = cml->roi1DeltaQp;
        codingCfg.roi2DeltaQp = cml->roi2DeltaQp;

        ALOGV("Set coding control: dctPartitions=%d ipolFilter=%d"
                " errorResilient=%d\n"
                " filterType=%d filterLevel=%d filterSharpness=%d quarterPixelMv=%d"
                " splitMv=%d\n",
                codingCfg.dctPartitions, codingCfg.interpolationFilter,
                codingCfg.errorResilient, codingCfg.filterType,
                codingCfg.filterLevel, codingCfg.filterSharpness,
                codingCfg.quarterPixelMv, codingCfg.splitMv);

        if (codingCfg.cirInterval)
            ALOGV("  CIR: %d %d\n",
                codingCfg.cirStart, codingCfg.cirInterval);

        if (codingCfg.intraArea.enable)
            ALOGV("  IntraArea: %dx%d-%dx%d\n",
                codingCfg.intraArea.left, codingCfg.intraArea.top,
                codingCfg.intraArea.right, codingCfg.intraArea.bottom);

        if (codingCfg.roi1Area.enable)
            ALOGV("  ROI 1: %d  %dx%d-%dx%d\n", codingCfg.roi1DeltaQp,
                codingCfg.roi1Area.left, codingCfg.roi1Area.top,
                codingCfg.roi1Area.right, codingCfg.roi1Area.bottom);

        if (codingCfg.roi2Area.enable)
            ALOGV("  ROI 2: %d  %dx%d-%dx%d\n", codingCfg.roi2DeltaQp,
                codingCfg.roi2Area.left, codingCfg.roi2Area.top,
                codingCfg.roi2Area.right, codingCfg.roi2Area.bottom);


        if((ret = VP8EncSetCodingCtrl(encoder, &codingCfg)) != VP8ENC_OK)
        {
            PrintErrorValue("VP8EncSetCodingCtrl() failed.", ret);
            CloseEncoder(encoder);
            return -1;
        }
    }

    /* PreP setup */
    if((ret = VP8EncGetPreProcessing(encoder, &preProcCfg)) != VP8ENC_OK)
    {
        PrintErrorValue("VP8EncGetPreProcessing() failed.", ret);
        CloseEncoder(encoder);
        return -1;
    }
    ALOGV
        ("Get PreP: input %4dx%d, offset %4dx%d, format=%d rotation=%d"
           " stab=%d cc=%d\n",
         preProcCfg.origWidth, preProcCfg.origHeight, preProcCfg.xOffset,
         preProcCfg.yOffset, preProcCfg.inputType, preProcCfg.rotation,
         preProcCfg.videoStabilization, preProcCfg.colorConversion.type);

    preProcCfg.inputType = (VP8EncPictureType)cml->inputFormat;
    preProcCfg.rotation = (VP8EncPictureRotation)cml->rotation;

    preProcCfg.origWidth =  cml->lumWidthSrc;
    preProcCfg.origHeight = cml->lumHeightSrc;

    if(cml->horOffsetSrc != DEFAULT)
        preProcCfg.xOffset = cml->horOffsetSrc;
    if(cml->verOffsetSrc != DEFAULT)
        preProcCfg.yOffset = cml->verOffsetSrc;
    if(cml->videoStab != DEFAULT)
        preProcCfg.videoStabilization = cml->videoStab;
    if(cml->colorConversion != DEFAULT)
        preProcCfg.colorConversion.type =
                        (VP8EncColorConversionType)cml->colorConversion;
    if(preProcCfg.colorConversion.type == VP8ENC_RGBTOYUV_USER_DEFINED)
    {
        preProcCfg.colorConversion.coeffA = 20000;
        preProcCfg.colorConversion.coeffB = 44000;
        preProcCfg.colorConversion.coeffC = 5000;
        preProcCfg.colorConversion.coeffE = 35000;
        preProcCfg.colorConversion.coeffF = 38000;
    }

    ALOGV
        ("Set PreP: input %4dx%d, offset %4dx%d, format=%d rotation=%d"
           " stab=%d cc=%d\n",
         preProcCfg.origWidth, preProcCfg.origHeight, preProcCfg.xOffset,
         preProcCfg.yOffset, preProcCfg.inputType, preProcCfg.rotation,
         preProcCfg.videoStabilization, preProcCfg.colorConversion.type);

    if((ret = VP8EncSetPreProcessing(encoder, &preProcCfg)) != VP8ENC_OK)
    {
        PrintErrorValue("VP8EncSetPreProcessing() failed.", ret);
        CloseEncoder(encoder);
        return ret;
    }

    return 0;
}

/*------------------------------------------------------------------------------

    CloseEncoder
       Release an encoder insatnce.

   Params:
        encoder - the instance to be released
------------------------------------------------------------------------------*/
void CloseEncoder(VP8EncInst encoder)
{
    VP8EncRet ret;

    if((ret = VP8EncRelease(encoder)) != VP8ENC_OK)
    {
        PrintErrorValue("VP8EncRelease() failed.", ret);
    }
	ALOGE("VP8EncRelease ret=%d", ret);
}

/*------------------------------------------------------------------------------
------------------------------------------------------------------------------*/
int ParseDelim(char *optArg, char delim)
{
    i32 i;

    for (i = 0; i < (i32)strlen(optArg); i++)
        if (optArg[i] == delim)
        {
            optArg[i] = 0;
            return i;
        }

    return -1;
}
/*------------------------------------------------------------------------------

    Parameter
        Process the testbench calling arguments.

    Params:
        argc    - number of arguments to the application
        argv    - argument list as provided to the application
        cml     - processed comand line options
    Return:
        0   - for success
        -1  - error

------------------------------------------------------------------------------*/
int Parameter(testbench_s * cml)
{
    i32 ret, i;

    memset(cml, 0, sizeof(testbench_s));

    /* Default setting tries to parse resolution from file name */
    cml->width              = 176;  //Width of encoded output image. [--lumWidthSrc]
    cml->height             = 144;  //Height of encoded output image. [--lumHeightSrc]
    cml->lumWidthSrc        = 176;  //Width of source image. [176]
    cml->lumHeightSrc       = 144;  //Height of source image. [144]
    cml->horOffsetSrc       = 0;  //Output image horizontal cropping offset. [0]
    cml->verOffsetSrc       = 0;  //Output image vertical cropping offset. [0]

    cml->inputFormat        = VP8ENC_YUV420_SEMIPLANAR;    //Input YUV format. [1]
    cml->input              = NULL; //Read input video sequence from file. [input.yuv]
    cml->output             = NULL; //Write output VP8 stream to file. [stream.ivf]
    cml->firstPic           = 0;        //First picture of input file to encode. [0]
    cml->lastPic            = 0xffff;   //Last picture of input file to encode. [100]
    cml->inputRateNumer     = 30;   //1..1048575 Input picture rate numerator. [30]
    cml->inputRateDenom     = 1;    //1..1048575 Input picture rate denominator. [1]
    cml->outputRateNumer    = 30;   //1..1048575 Output picture rate numerator. [--inputRateNumer]
    cml->outputRateDenom    = 1;    //1..1048575 Output picture rate denominator. [--inputRateDenom]
    cml->refFrameAmount     = 1;    //1..3 Amount of buffered reference frames. [1]

    /* Default settings are get from API and not changed in testbench */
    cml->colorConversion    = VP8ENC_RGBTOYUV_BT601;    //RGB to YCbCr color conversion type. [0]
    cml->videoStab          = 0;    //Enable video stabilization or scene change detection. [0]
    cml->rotation           = 0;    //Rotate input image. [0]

    cml->qpHdr              = 36;   //-1..127, Initial QP used for the first frame. [36]
    cml->qpMin              = 10;   //0..127, Minimum frame header QP. [10]
    cml->qpMax              = 127;   //0..127, Maximum frame header QP. [51]
    cml->bitPerSecond       = 1000000;  //10000..60000000, Target bitrate for rate control [1000000]
    cml->picRc              = 0;    //0=OFF, 1=ON, Picture rate control enable. [1]
    cml->picSkip            = 0;    //0=OFF, 1=ON, Picture skip rate control. [0]
    cml->intraQpDelta       = 0;    //-12..12, Intra QP delta. [0]
    cml->fixedIntraQp       = 0;    //0..127, Fixed Intra QP, 0 = disabled. [0]

    cml->intraPicRate       = 30;    //Intra picture rate in frames. [0]
    cml->gopLength          = cml->intraPicRate;   //1..300, Group Of Pictures length in frames. [--intraPicRate]
    cml->dctPartitions      = 0;    //0=1, 1=2, 2=4, 3=8, Amount of DCT partitions to create
    cml->errorResilient     = 0;    //Enable error resilient stream mode. [0]
    cml->ipolFilter         = 1;    //0=Bicubic, 1=Bilinear, 2=None, Interpolation filter mode. [1]
    cml->filterType         = 0;    //0=Normal, 1=Simple, Type of in-loop deblocking filter. [0]
    cml->filterLevel        = 64;   //0..64, 64=auto, Filter strength level for deblocking. [64]
    cml->filterSharpness    = 8;    //0..8, 8=auto, Filter sharpness for deblocking. [8]
    cml->quarterPixelMv     = 1;    //0=OFF, 1=Adaptive, 2=ON, use 1/4 pixel MVs. [1]
    cml->splitMv            = 1;    //0=OFF, 1=Adaptive, 2=ON, allowed to to use more than 1 MV/MB. [1]

    cml->cirStart           = 0;    //start:interval for Cyclic Intra Refresh, forces MBs intra.
    cml->cirInterval        = 0;    //start:interval for Cyclic Intra Refresh, forces MBs intra.
    cml->intraAreaLeft      = 0;    //left:top:right:bottom macroblock coordinates
    cml->intraAreaTop       = 0;    //left:top:right:bottom macroblock coordinates
    cml->intraAreaRight     = 0;    //left:top:right:bottom macroblock coordinates
    cml->intraAreaBottom    = 0;    //left:top:right:bottom macroblock coordinates
    cml->intraAreaEnable    = 0;    //left:top:right:bottom macroblock coordinates

    cml->roi1AreaLeft      = 0;    //left:top:right:bottom macroblock coordinates
    cml->roi1AreaTop       = 0;    //left:top:right:bottom macroblock coordinates
    cml->roi1AreaRight     = 0;    //left:top:right:bottom macroblock coordinates
    cml->roi1AreaBottom    = 0;    //left:top:right:bottom macroblock coordinates
    cml->roi1AreaEnable    = 0;    //left:top:right:bottom macroblock coordinates
    cml->roi2AreaLeft      = 0;    //left:top:right:bottom macroblock coordinates
    cml->roi2AreaTop       = 0;    //left:top:right:bottom macroblock coordinates
    cml->roi2AreaRight     = 0;    //left:top:right:bottom macroblock coordinates
    cml->roi2AreaBottom    = 0;    //left:top:right:bottom macroblock coordinates
    cml->roi2AreaEnable    = 0;    //left:top:right:bottom macroblock coordinates

    cml->roi1DeltaQp    = 0;    //QP delta value for 1st Region-Of-Interest. [-50,0]
    cml->roi2DeltaQp    = 0;    //QP delta value for 2nd Region-Of-Interest. [-50,0]
    cml->roi1AreaEnable = cml->roi1DeltaQp;
    cml->roi2AreaEnable = cml->roi2DeltaQp;

    cml->psnrSum        = 0;    //Enables PSNR calculation for each frame. [0]
    cml->psnrCnt        = 0;    //Enables PSNR calculation for each frame. [0]

    cml->mvOutput           = 0;    //Enable MV writing in <mv.txt> [0]
    cml->testId             = 0;    //Internal test ID. [0]
    //cml->traceFileConfig    = 0;    //File holding names of trace files to create. []

    //cml->firstTracePic      = 0;    //First picture for tracing. [0]
    cml->intra16Favor       = 0;    //Favor value for I16x16 mode in I16/I4
    cml->intraPenalty       = 0;    //Penalty value for intra mode in intra/inter
    cml->traceRecon         = 0;    //Enable writing sw_recon_internal.yuv
    cml->droppable          = 0;    //Code all odd frames as droppable.

    return 0;
}

/*------------------------------------------------------------------------------

    ReadPic

    Read raw YUV or RGB image data from file

    Params:
        image   - buffer where the image will be saved
        size    - amount of image data to be read
        nro     - picture number to be read
        name    - name of the file to read
        width   - image width in pixels
        height  - image height in pixels
        format  - image format (YUV 420/422/RGB16/RGB32)

    Returns:
        0 - for success
        non-zero error code
------------------------------------------------------------------------------*/
int ReadPic(testbench_s * cml, u8 * image, i32 size, i32 nro)
{
    int ret = 0;
    FILE *readFile = NULL;
    i32 width = cml->lumWidthSrc;
    i32 height = cml->lumHeightSrc;

    if(cml->yuvFile == NULL)
    {
        cml->yuvFile = fopen(cml->input, "rb");
        cml->yuvFileName = cml->input;

        if(cml->yuvFile == NULL)
        {
            fprintf(VP8ERR_OUTPUT, "\nUnable to open YUV file: %s\n", cml->input);
            ret = -1;
            goto end;
        }

        fseeko(cml->yuvFile, 0, SEEK_END);
        cml->file_size = ftello(cml->yuvFile);
    }

    readFile = cml->yuvFile;

    /* Stop if over last frame of the file */
    if((off_t)size * (nro + 1) > cml->file_size)
    {
        ALOGV("\nCan't read frame, EOF\n");
        ret = -1;
        goto end;
    }

    if(fseeko(readFile, (off_t)size * nro, SEEK_SET) != 0)
    {
        fprintf(VP8ERR_OUTPUT, "\nI can't seek frame no: %i from file: %s\n",
                nro, cml->yuvFileName);
        ret = -1;
        goto end;
    }

    if((width & 0x0f) == 0)
        fread(image, 1, size, readFile);
    else
    {
        i32 i;
        u8 *buf = image;
        i32 scan = (width + 15) & (~0x0f);

        /* Read the frame so that scan (=stride) is multiple of 16 pixels */

        if(cml->inputFormat == 0) /* YUV 4:2:0 planar */
        {
            /* Y */
            for(i = 0; i < height; i++)
            {
                fread(buf, 1, width, readFile);
                buf += scan;
            }
            /* Cb */
            for(i = 0; i < (height / 2); i++)
            {
                fread(buf, 1, width / 2, readFile);
                buf += scan / 2;
            }
            /* Cr */
            for(i = 0; i < (height / 2); i++)
            {
                fread(buf, 1, width / 2, readFile);
                buf += scan / 2;
            }
        }
        else if(cml->inputFormat == 1)    /* YUV 4:2:0 semiplanar */
        {
            /* Y */
            for(i = 0; i < height; i++)
            {
                fread(buf, 1, width, readFile);
                buf += scan;
            }
            /* CbCr */
            for(i = 0; i < (height / 2); i++)
            {
                fread(buf, 1, width, readFile);
                buf += scan;
            }
        }
        else if(cml->inputFormat <= 9)   /* YUV 4:2:2 interleaved or 16-bit RGB */
        {
            for(i = 0; i < height; i++)
            {
                fread(buf, 1, width * 2, readFile);
                buf += scan * 2;
            }
        }
        else    /* 32-bit RGB */
        {
            for(i = 0; i < height; i++)
            {
                fread(buf, 1, width * 4, readFile);
                buf += scan * 4;
            }
        }

    }

  end:

    return ret;
}

/*------------------------------------------------------------------------------
    IvfHeader
------------------------------------------------------------------------------*/
void IvfHeader(testbench_s *tb, u8 *out)
{
    u8 data[IVF_HDR_BYTES] = {0};

    /* File header signature */
    data[0] = 'D';
    data[1] = 'K';
    data[2] = 'I';
    data[3] = 'F';

    /* File format version and file header size */
    data[6] = 32;

    /* Video data FourCC */
    data[8] = 'V';
    data[9] = 'P';
    data[10] = '8';
    data[11] = '0';

    /* Video Image width and height */
    data[12] = tb->width & 0xff;
    data[13] = (tb->width >> 8) & 0xff;
    data[14] = tb->height & 0xff;
    data[15] = (tb->height >> 8) & 0xff;

    /* Frame rate rate */
    data[16] = tb->outputRateNumer & 0xff;
    data[17] = (tb->outputRateNumer >> 8) & 0xff;
    data[18] = (tb->outputRateNumer >> 16) & 0xff;
    data[19] = (tb->outputRateNumer >> 24) & 0xff;

    /* Frame rate scale */
    data[20] = tb->outputRateDenom & 0xff;
    data[21] = (tb->outputRateDenom >> 8) & 0xff;
    data[22] = (tb->outputRateDenom >> 16) & 0xff;
    data[23] = (tb->outputRateDenom >> 24) & 0xff;

    /* Video length in frames */
    data[24] = tb->frameCntTotal & 0xff;
    data[25] = (tb->frameCntTotal >> 8) & 0xff;
    data[26] = (tb->frameCntTotal >> 16) & 0xff;
    data[27] = (tb->frameCntTotal >> 24) & 0xff;

    memcpy(out, data, IVF_HDR_BYTES);
}

/*------------------------------------------------------------------------------
    IvfFrame
------------------------------------------------------------------------------*/
void IvfFrame(testbench_s *tb, u8 *out)
{
    i32 byteCnt = 0;
    u8 data[IVF_FRM_BYTES];

    /* Frame size (4 bytes) */
    byteCnt = tb->encOut.frameSize;
    data[0] =  byteCnt        & 0xff;
    data[1] = (byteCnt >> 8)  & 0xff;
    data[2] = (byteCnt >> 16) & 0xff;
    data[3] = (byteCnt >> 24) & 0xff;

    /* Timestamp (8 bytes) */
    data[4]  =  (tb->frameCntTotal)        & 0xff;
    data[5]  = ((tb->frameCntTotal) >> 8)  & 0xff;
    data[6]  = ((tb->frameCntTotal) >> 16) & 0xff;
    data[7]  = ((tb->frameCntTotal) >> 24) & 0xff;
    data[8]  = ((tb->frameCntTotal) >> 32) & 0xff;
    data[9]  = ((tb->frameCntTotal) >> 40) & 0xff;
    data[10] = ((tb->frameCntTotal) >> 48) & 0xff;
    data[11] = ((tb->frameCntTotal) >> 56) & 0xff;

    memcpy(out, data, IVF_FRM_BYTES);
}

/*------------------------------------------------------------------------------
    WriteMotionVectors
        Write motion vectors into a file
------------------------------------------------------------------------------*/
void WriteMotionVectors(FILE *file, i8 *mvs, i32 frame, i32 width, i32 height)
{
    i32 i;
    i32 mbPerRow = (width+15)/16;
    i32 mbPerCol = (height+15)/16;
    i32 mbPerFrame = mbPerRow * mbPerCol;
    u8 *sads = (u8 *)mvs;

    if (!file || !mvs)
        return;

    fprintf(file,
            "\nFrame=%d  motion vector X,Y for %d macroblocks (%dx%d)\n",
            frame, mbPerFrame, mbPerRow, mbPerCol);

    for (i = 0; i < mbPerFrame; i++)
    {
        fprintf(file, "  %3d,%3d", mvs[i*4], mvs[i*4+1]);
        if ((i % mbPerRow) == mbPerRow-1)
            fprintf(file, "\n");
    }

    fprintf(file,
            "\nFrame=%d  SAD\n", frame);

    for (i = 0; i < mbPerFrame; i++)
    {
        fprintf(file, " %5d", (u32)(sads[i*4+2] << 8) | sads[i*4+3]);
        if ((i % mbPerRow) == mbPerRow-1)
            fprintf(file, "\n");
    }
}

/*------------------------------------------------------------------------------

    NextPic

    Function calculates next input picture depending input and output frame
    rates.

    Input   inputRateNumer  (input.yuv) frame rate numerator.
            inputRateDenom  (input.yuv) frame rate denominator
            outputRateNumer (stream.mpeg4) frame rate numerator.
            outputRateDenom (stream.mpeg4) frame rate denominator.
            frameCnt        Frame counter.
            firstPic        The first picture of input.yuv sequence.

    Return  next    The next picture of input.yuv sequence.

------------------------------------------------------------------------------*/
i32 NextPic(i32 inputRateNumer, i32 inputRateDenom, i32 outputRateNumer,
            i32 outputRateDenom, i32 frameCnt, i32 firstPic)
{
    u32 sift;
    u32 skip;
    u32 numer;
    u32 denom;
    u32 next;

    numer = (u32) inputRateNumer *(u32) outputRateDenom;
    denom = (u32) inputRateDenom *(u32) outputRateNumer;

    if(numer >= denom)
    {
        sift = 9;
        do
        {
            sift--;
        }
        while(((numer << sift) >> sift) != numer);
    }
    else
    {
        sift = 17;
        do
        {
            sift--;
        }
        while(((numer << sift) >> sift) != numer);
    }
    skip = (numer << sift) / denom;
    next = (((u32) frameCnt * skip) >> sift) + (u32) firstPic;

    return (i32) next;
}

/*------------------------------------------------------------------------------

    API tracing
        Tracing as defined by the API.
    Params:
        msg - null terminated tracing message
------------------------------------------------------------------------------*/
void VP8EncTrace(const char *msg)
{
    static FILE *fp = NULL;

    if(fp == NULL)
        fp = fopen("api.trc", "wt");

    if(fp)
        fprintf(fp, "%s\n", msg);
}

/*------------------------------------------------------------------------------
    Print title for per frame prints
------------------------------------------------------------------------------*/
void PrintTitle(testbench_s *cml)
{
    ALOGV("\n");
    ALOGV("Input | Pic |  QP | Type | IP GR AR |   BR avg    MA(%3d) | ByteCnt (inst) |",
            cml->ma.length);

    if (cml->printPsnr)
        ALOGV(" PSNR  |");

    ALOGV("\n");
    ALOGV("------------------------------------------------------------------------------------\n");

    ALOGV("      |     | %3d | HDR  |          |                     | %7i %6i |",
            cml->rc.qpHdr, cml->streamSize, IVF_HDR_BYTES);

    if (cml->printPsnr)
        ALOGV("       |");
    ALOGV("\n");
}

/*------------------------------------------------------------------------------
    Print information per frame
------------------------------------------------------------------------------*/
void PrintFrame(testbench_s *cml, VP8EncInst encoder, u32 frameNumber,
        VP8EncRet ret)
{
    u32 psnr = 0;

    if((cml->frameCntTotal+1) && cml->outputRateDenom)
    {
        /* Using 64-bits to avoid overflow */
        u64 tmp = cml->streamSize / (cml->frameCntTotal+1);
        tmp *= (u32) cml->outputRateNumer;

        cml->bitrate = (u32) (8 * (tmp / (u32) cml->outputRateDenom));
    }

    ALOGV("%5i | %3llu | %3d | ",
        frameNumber, cml->frameCntTotal, cml->rc.qpHdr);

    ALOGV("%s",
        (ret == VP8ENC_OUTPUT_BUFFER_OVERFLOW) ? "lost" :
        (cml->encOut.codingType == VP8ENC_INTRA_FRAME) ? " I  " :
        (cml->encOut.codingType == VP8ENC_PREDICTED_FRAME) ? " P  " : "skip");

    /* Print reference frame usage */
    ALOGV(" | %c%c %c%c %c%c",
        cml->encOut.ipf & VP8ENC_REFERENCE ? 'R' : ' ',
        cml->encOut.ipf & VP8ENC_REFRESH   ? 'W' : ' ',
        cml->encOut.grf & VP8ENC_REFERENCE ? 'R' : ' ',
        cml->encOut.grf & VP8ENC_REFRESH   ? 'W' : ' ',
        cml->encOut.arf & VP8ENC_REFERENCE ? 'R' : ' ',
        cml->encOut.arf & VP8ENC_REFRESH   ? 'W' : ' ');

    /* Print bitrate statistics and frame size */
    ALOGV(" | %9u %9u | %7i %6i | ",
        cml->bitrate, Ma(&cml->ma), cml->streamSize, cml->encOut.frameSize);

#ifdef PSNR
    if (cml->printPsnr)
        psnr = PrintPSNR((u8 *)
            (((vp8Instance_s *)encoder)->asic.regs.inputLumBase +
            ((vp8Instance_s *)encoder)->asic.regs.inputLumaBaseOffset),
            (u8 *)
            (((vp8Instance_s *)encoder)->asic.regs.internalImageLumBaseW),
            cml->lumWidthSrc, cml->width, cml->height, cml->rotation);

    if (psnr) {
        cml->psnrSum += psnr;
        cml->psnrCnt++;
    }
#endif

    /* Print size of each partition in bytes */
    ALOGV("%d %d %d %d\n", cml->encOut.frameSize ? IVF_FRM_BYTES : 0,
            cml->encOut.streamSize[0],
            cml->encOut.streamSize[1], cml->encOut.streamSize[2]);

    /* Check that partition sizes match frame size */
    if (cml->encOut.frameSize != (cml->encOut.streamSize[0]+
                cml->encOut.streamSize[1]+cml->encOut.streamSize[2]))
        ALOGV("ERROR: Frame size doesn't match partition sizes!\n");
}

/*------------------------------------------------------------------------------
    PrintErrorValue
        Print return error value
------------------------------------------------------------------------------*/
void PrintErrorValue(const char *errorDesc, u32 retVal)
{
    char * str;

    switch (retVal)
    {
        case VP8ENC_ERROR: str = "VP8ENC_ERROR"; break;
        case VP8ENC_NULL_ARGUMENT: str = "VP8ENC_NULL_ARGUMENT"; break;
        case VP8ENC_INVALID_ARGUMENT: str = "VP8ENC_INVALID_ARGUMENT"; break;
        case VP8ENC_MEMORY_ERROR: str = "VP8ENC_MEMORY_ERROR"; break;
        case VP8ENC_EWL_ERROR: str = "VP8ENC_EWL_ERROR"; break;
        case VP8ENC_EWL_MEMORY_ERROR: str = "VP8ENC_EWL_MEMORY_ERROR"; break;
        case VP8ENC_INVALID_STATUS: str = "VP8ENC_INVALID_STATUS"; break;
        case VP8ENC_OUTPUT_BUFFER_OVERFLOW: str = "VP8ENC_OUTPUT_BUFFER_OVERFLOW"; break;
        case VP8ENC_HW_BUS_ERROR: str = "VP8ENC_HW_BUS_ERROR"; break;
        case VP8ENC_HW_DATA_ERROR: str = "VP8ENC_HW_DATA_ERROR"; break;
        case VP8ENC_HW_TIMEOUT: str = "VP8ENC_HW_TIMEOUT"; break;
        case VP8ENC_HW_RESERVED: str = "VP8ENC_HW_RESERVED"; break;
        case VP8ENC_SYSTEM_ERROR: str = "VP8ENC_SYSTEM_ERROR"; break;
        case VP8ENC_INSTANCE_ERROR: str = "VP8ENC_INSTANCE_ERROR"; break;
        case VP8ENC_HRD_ERROR: str = "VP8ENC_HRD_ERROR"; break;
        case VP8ENC_HW_RESET: str = "VP8ENC_HW_RESET"; break;
        default: str = "UNDEFINED";
    }

    //fprintf(VP8ERR_OUTPUT, "%s Return value: %s\n", errorDesc, str);
    ALOGV("%s, %s", errorDesc, str);
}

/*------------------------------------------------------------------------------
    PrintPSNR
        Calculate and print frame PSNR
------------------------------------------------------------------------------*/
u32 PrintPSNR(u8 *a, u8 *b, i32 scanline, i32 wdh, i32 hgt, i32 rotation)
{
#ifdef PSNR
        float mse = 0.0;
        u32 tmp, i, j;

        if (!rotation)
        {
            for (j = 0 ; j < hgt; j++) {
                    for (i = 0; i < wdh; i++) {
                            tmp = a[i] - b[i];
                            tmp *= tmp;
                            mse += tmp;
                    }
                    a += scanline;
                    b += wdh;
            }
            mse /= wdh * hgt;
        }

        if (mse == 0.0) {
                ALOGV("--.-- | ");
        } else {
                mse = 10.0 * log10f(65025.0 / mse);
                ALOGV("%5.2f | ", mse);
        }

        return (u32)roundf(mse*100);
#else
        ALOGV("xx.xx | ");
        return 0;
#endif
}

/*------------------------------------------------------------------------------
    GetResolution
        Parse image resolution from file name
------------------------------------------------------------------------------*/
u32 GetResolution(char *filename, i32 *pWidth, i32 *pHeight)
{
    i32 i;
    u32 w, h;
    i32 len = strlen(filename);
    i32 filenameBegin = 0;

    /* Find last '/' in the file name, it marks the beginning of file name */
    for (i = len-1; i; --i)
        if (filename[i] == '/') {
            filenameBegin = i+1;
            break;
        }

    /* If '/' found, it separates trailing path from file name */
    for (i = filenameBegin; i <= len-3; ++i)
    {
        if ((strncmp(filename+i, "subqcif", 7) == 0) ||
            (strncmp(filename+i, "sqcif", 5) == 0))
        {
            *pWidth = 128;
            *pHeight = 96;
            ALOGV("Detected resolution SubQCIF (128x96) from file name.\n");
            return 0;
        }
        if (strncmp(filename+i, "qcif", 4) == 0)
        {
            *pWidth = 176;
            *pHeight = 144;
            ALOGV("Detected resolution QCIF (176x144) from file name.\n");
            return 0;
        }
        if (strncmp(filename+i, "4cif", 4) == 0)
        {
            *pWidth = 704;
            *pHeight = 576;
            ALOGV("Detected resolution 4CIF (704x576) from file name.\n");
            return 0;
        }
        if (strncmp(filename+i, "cif", 3) == 0)
        {
            *pWidth = 352;
            *pHeight = 288;
            ALOGV("Detected resolution CIF (352x288) from file name.\n");
            return 0;
        }
        if (strncmp(filename+i, "qqvga", 5) == 0)
        {
            *pWidth = 160;
            *pHeight = 120;
            ALOGV("Detected resolution QQVGA (160x120) from file name.\n");
            return 0;
        }
        if (strncmp(filename+i, "qvga", 4) == 0)
        {
            *pWidth = 320;
            *pHeight = 240;
            ALOGV("Detected resolution QVGA (320x240) from file name.\n");
            return 0;
        }
        if (strncmp(filename+i, "vga", 3) == 0)
        {
            *pWidth = 640;
            *pHeight = 480;
            ALOGV("Detected resolution VGA (640x480) from file name.\n");
            return 0;
        }
        if (strncmp(filename+i, "720p", 4) == 0)
        {
            *pWidth = 1280;
            *pHeight = 720;
            ALOGV("Detected resolution 720p (1280x720) from file name.\n");
            return 0;
        }
        if (strncmp(filename+i, "1080p", 5) == 0)
        {
            *pWidth = 1920;
            *pHeight = 1080;
            ALOGV("Detected resolution 1080p (1920x1080) from file name.\n");
            return 0;
        }
        if (filename[i] == 'x')
        {
            if (sscanf(filename+i-4, "%ux%u", &w, &h) == 2)
            {
                *pWidth = w;
                *pHeight = h;
                ALOGV("Detected resolution %dx%d from file name.\n", w, h);
                return 0;
            }
            else if (sscanf(filename+i-3, "%ux%u", &w, &h) == 2)
            {
                *pWidth = w;
                *pHeight = h;
                ALOGV("Detected resolution %dx%d from file name.\n", w, h);
                return 0;
            }
            else if (sscanf(filename+i-2, "%ux%u", &w, &h) == 2)
            {
                *pWidth = w;
                *pHeight = h;
                ALOGV("Detected resolution %dx%d from file name.\n", w, h);
                return 0;
            }
        }
        if (filename[i] == 'w')
        {
            if (sscanf(filename+i, "w%uh%u", &w, &h) == 2)
            {
                *pWidth = w;
                *pHeight = h;
                ALOGV("Detected resolution %dx%d from file name.\n", w, h);
                return 0;
            }
        }
    }

    return 1;   /* Error - no resolution found */
}

/*------------------------------------------------------------------------------
    Add new frame bits for moving average bitrate calculation
------------------------------------------------------------------------------*/
void MaAddFrame(ma_s *ma, i32 frameSizeBits)
{
    ma->frame[ma->pos++] = frameSizeBits;

    if (ma->pos == ma->length)
        ma->pos = 0;

    if (ma->count < ma->length)
        ma->count++;
}

/*------------------------------------------------------------------------------
    Calculate average bitrate of moving window
------------------------------------------------------------------------------*/
i32 Ma(ma_s *ma)
{
    i32 i;
    u64 sum = 0;     /* Using 64-bits to avoid overflow */

    for (i = 0; i < ma->count; i++)
        sum += ma->frame[i];

    if (!ma->frameRateDenom)
        return 0;

    sum = sum / ma->length;

    return sum * ma->frameRateNumer / ma->frameRateDenom;
}

extern "C"
void *  get_class_RkVp8Encoder(void)
{
    return (void*)new Rk_Vp8Encoder();
}

extern "C"
void  destroy_class_RkVp8Encoder(void * Vp8Encoder)
{
    delete (Rk_Vp8Encoder *)Vp8Encoder;
    Vp8Encoder = NULL;
}

extern "C"
int init_class_RkVp8Encoder(void * Vp8Encoder, EncParams1 *aEncOption, uint8_t* aOutBuffer,uint32_t* aOutputLength)
{
	Rk_Vp8Encoder * Vp8enc = (Rk_Vp8Encoder *)Vp8Encoder;
	return Vp8enc->pv_rkvp8encoder_init(aEncOption, aOutBuffer, aOutputLength);
}

extern "C"
int deinit_class_RkVp8Encoder(void * Vp8Encoder)
{
	Rk_Vp8Encoder * Vp8enc = (Rk_Vp8Encoder *)Vp8Encoder;
	return Vp8enc->pv_rkvp8encoder_deinit();
}

extern "C"
int enc_oneframe_class_RkVp8Encoder(void * Vp8Encoder, uint8_t* aOutBuffer, uint32_t* aOutputLength,
                                     uint8_t *aInBuffer,uint32_t aInBuffPhy,uint32_t* aInBufSize,uint32_t* aOutTimeStamp,bool*  aSyncFlag)
{
	int ret;

	Rk_Vp8Encoder * Vp8enc = (Rk_Vp8Encoder *)Vp8Encoder;

	ret = Vp8enc->pv_rkvp8encoder_oneframe(aOutBuffer, aOutputLength,aInBuffer,
                                            aInBuffPhy, aInBufSize, aOutTimeStamp, aSyncFlag);
	return ret;
}



