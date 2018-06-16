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
--  Abstract  :    JPEG Code Frame control
--
--------------------------------------------------------------------------------
--
--  Version control information, please leave untouched.
--
--  $RCSfile: EncJpegCodeFrame.c,v $
--  $Revision: 1.1 $
--
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    1. Include headers
------------------------------------------------------------------------------*/
#include "enccommon.h"
#include "ewl.h"
//#include "encdebug.h"

#include "EncJpegCodeFrame.h"
#include <utils/Log.h>
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
#ifndef AVS40
#undef LOGD
#define LOGD ALOGD
#undef LOGE
#define LOGE ALOGE
#endif
/*------------------------------------------------------------------------------
    2. External compiler flags
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
    3. Module defines
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    4. Local function prototypes
------------------------------------------------------------------------------*/
static void NextHeader(stream_s * stream, jpegData_s * jpeg);
static void EndRi(stream_s * stream, jpegData_s * jpeg);
static void jpegSetNewFrame(jpegInstance_s * inst);

/*------------------------------------------------------------------------------

    EncJpegCodeFrame

------------------------------------------------------------------------------*/
jpegEncodeFrame_e EncJpegCodeFrame(jpegInstance_s * inst/*, VPUMemLinear_t outMem*/)
{
    jpegEncodeFrame_e ret;
    asicData_s *asic = &inst->asic;

    /* set output stream start point in case of whole encoding mode
     * and no rst used */
    if(inst->stream.byteCnt == 0)
    {
        inst->jpeg.streamStartAddress = inst->stream.stream;
    }

    /* set new frame encoding parameters */
    jpegSetNewFrame(inst);

	if (inst->cacheflush) {
		(inst->cacheflush)(1, 0, 0x7fffffff);
	}

	//VPUMemFlush(&outMem);
    /* start hw encoding */
    JPEG_EncAsicFrameStart(inst->asic.ewl, &inst->asic.regs);
   // LOGD("mirror[2] = 0x%x ",inst->asic.regs.regMirror[2]);
    {
        /* Encode one frame */
        RK_S32 ewl_ret;

#ifndef JPEG_SIMULATION
        /* Wait for IRQ */
        {
            VPU_CMD_TYPE cmd;
            RK_S32 len;
            ewl_ret = VPUClientWaitResult(inst->asic.regs.socket, inst->asic.regs.regMirror, inst->asic.regs.register_size, &cmd, &len);
            if ((VPU_SUCCESS != ewl_ret) || (cmd != VPU_SEND_CONFIG_ACK_OK))
                ewl_ret = EWL_HW_WAIT_ERROR;

        }

#else
        return JPEGENCODE_OK;
#endif
        if(ewl_ret != EWL_OK)
        {
            if(ewl_ret == EWL_ERROR)
            {
                /* IRQ error => Stop and release HW */
                ret = JPEGENCODE_SYSTEM_ERROR;
            }
            else    /*if(ewl_ret == EWL_HW_WAIT_TIMEOUT) */
            {
                /* IRQ Timeout => Stop and release HW */
                ret = JPEGENCODE_TIMEOUT;
            }

            JPEG_EncAsicStop(asic->ewl);
            /* Release HW so that it can be used by other codecs */
            //EWLReleaseHw(asic->ewl);

        }
        else
        {
            RK_S32 status = JPEG_EncAsicCheckStatus_V2(asic);

            switch (status)
            {
            case ASIC_STATUS_ERROR:
                LOGD("%s %d, JPEG_EncAsicCheckStatus_V2 return ASIC_STATUS_ERROR\n", __func__, __LINE__);
                ret = JPEGENCODE_HW_ERROR;
                break;
            case ASIC_STATUS_BUFF_FULL:
                ret = JPEGENCODE_OK;
                inst->stream.overflow = ENCHW_YES;
                break;
            case ASIC_STATUS_HW_RESET:
                ret = JPEGENCODE_HW_RESET;
                break;
            case ASIC_STATUS_FRAME_READY:
                inst->stream.byteCnt &= (~0x07);    /* last not full 64-bit counted in HW data */
                inst->stream.byteCnt += (asic->regs.outputStrmSize / 8);
                ret = JPEGENCODE_OK;
                break;
            default:
                /* should never get here */
                ASSERT(0);
                LOGD("%s %d, JPEG_EncAsicCheckStatus_V2 failure\n", __func__, __LINE__);
                ret = JPEGENCODE_HW_ERROR;
            }
        }
    }

    /* Handle EOI */
    if(ret == JPEGENCODE_OK)
    {
        /* update mcu count */
        if(inst->jpeg.codingType == ENC_PARTIAL_FRAME)
        {
            if((inst->jpeg.mbNum + inst->jpeg.restart.Ri) <
               ((RK_U32) inst->jpeg.mbPerFrame))
            {
                inst->jpeg.mbNum += inst->jpeg.restart.Ri;
                inst->jpeg.row += inst->jpeg.sliceRows;
            }
            else
            {
                inst->jpeg.mbNum += (inst->jpeg.mbPerFrame - inst->jpeg.mbNum);
            }
        }
        else
        {
            inst->jpeg.mbNum += inst->jpeg.mbPerFrame;
        }

        EndRi(&inst->stream, &inst->jpeg);
    }

    return ret;
}

/*------------------------------------------------------------------------------

    Write the header data (frame header or restart marker) to the stream.

------------------------------------------------------------------------------*/
void NextHeader(stream_s * stream, jpegData_s * jpeg)
{
    if(jpeg->mbNum == 0)
    {
        (void) EncJpegHdr(stream, jpeg);
    }
}

/*------------------------------------------------------------------------------

    Write the end of current coding unit (RI / FRAME) into stream.

------------------------------------------------------------------------------*/
void EndRi(stream_s * stream, jpegData_s * jpeg)
{
    /* not needed anymore, ASIC generates EOI marker */
}

/*------------------------------------------------------------------------------

    Set encoding parameters at the beginning of a new frame.

------------------------------------------------------------------------------*/
void jpegSetNewFrame(jpegInstance_s * inst)
{
    regValues_s *regs = &inst->asic.regs;

    /* Write next header if needed */
    NextHeader(&inst->stream, &inst->jpeg);

    /* calculate output start point for hw */
    regs->outputStrmSize -= inst->stream.byteCnt;
    regs->outputStrmSize /= 8;  /* 64-bit addresses */
    regs->outputStrmSize &= (~0x07);    /* 8 multiple size */

    /* 64-bit aligned stream base address */
    if (!VPUMemJudgeIommu()) {
        regs->outputStrmBase += (inst->stream.byteCnt & (~0x07));
    } else {
        regs->outputStrmBase += (inst->stream.byteCnt & (~0x07)) << 10;
    }
    /* bit offset in the last 64-bit word */
    regs->firstFreeBit = (inst->stream.byteCnt & 0x07) * 8;

    /* header remainder is byte aligned, max 7 bytes = 56 bits */
    if(regs->firstFreeBit != 0)
    {
        /* 64-bit aligned stream pointer */
        RK_U8 *pTmp = (RK_U8 *) ((unsigned int) (inst->stream.stream) & (~0x07));
        RK_U32 val;

        /* Clear remaining bits */
        for (val = 6; val >= regs->firstFreeBit/8; val--)
            pTmp[val] = 0;

        val = pTmp[0] << 24;
        val |= pTmp[1] << 16;
        val |= pTmp[2] << 8;
        val |= pTmp[3];

        regs->strmStartMSB = val;  /* 32 bits to MSB */

        if(regs->firstFreeBit > 32)
        {
            val = pTmp[4] << 24;
            val |= pTmp[5] << 16;
            val |= pTmp[6] << 8;

            regs->strmStartLSB = val;
        }
        else
            regs->strmStartLSB = 0;
    }
    else
    {
        regs->strmStartMSB = regs->strmStartLSB = 0;
    }

}
