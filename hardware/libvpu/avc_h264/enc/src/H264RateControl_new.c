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
--  Description : Rate control
--
------------------------------------------------------------------------------*/
/*
 * Using only one leaky bucket (Multible buckets is supported by std).
 * Constant bit rate (CBR) operation, ie. leaky bucket drain rate equals
 * average rate of the stream, is enabled if RC_CBR_HRD = 1. Frame skiping and
 * filler data are minimum requirements for the CBR conformance.
 *
 * Constant HRD parameters:
 *   low_delay_hrd_flag = 0, assumes constant delay mode.
 *       cpb_cnt_minus1 = 0, only one leaky bucket used.
 *         (cbr_flag[0] = RC_CBR_HRD, CBR mode.)
 */
#include "H264RateControl.h"
#include "H264Slice.h"
#ifdef ROI_SUPPORT
#include "H264RoiModel.h"
#endif

/*------------------------------------------------------------------------------
  Module defines
------------------------------------------------------------------------------*/

/* Define this if strict bitrate control is needed, each window has a strict
 * bit budget. Otherwise a moving window is used for smoother quality.
#define RC_WINDOW_STRICT*/

#ifdef TRACE_RC
#include <stdio.h>
FILE *fpRcTrc = NULL;
/* Select debug output: fpRcTrc or stdout */
#define DBGOUTPUT fpRcTrc
/* Select debug level: 0 = minimum, 2 = maximum */
#define DBG_LEVEL 2
#define DBG(l, str) if (l <= DBG_LEVEL) fprintf str
#else
#define DBG(l, str)
#endif

#define INITIAL_BUFFER_FULLNESS   60    /* Decoder Buffer in procents */
#define MIN_PIC_SIZE              50    /* Procents from picPerPic */

#define DIV(a, b)       ((b) ? ((a) + (SIGN(a) * (b)) / 2) / (b) : (a))
#define DSCY                      32 /* n * 32 */
#define I32_MAX           2147483647 /* 2 ^ 31 - 1 */
#define QP_DELTA          2
#define QP_DELTA_LIMIT    10
#define INTRA_QP_DELTA    (0)
#define WORD_CNT_MAX      65535

/*------------------------------------------------------------------------------
  Local structures
------------------------------------------------------------------------------*/
/* q_step values scaled up by 4 and evenly rounded */
i32 q_step[53] = { 3, 3, 3, 4, 4, 5, 5, 6, 7, 7, 8, 9, 10, 11,
    13, 14, 16, 18, 20, 23, 25, 28, 32, 36, 40, 45, 51, 57, 64, 72, 80, 90,
    101, 114, 128, 144, 160, 180, 203, 228, 256, 288, 320, 360, 405, 456,
    513, 577, 640, 720, 810, 896};
    
/*------------------------------------------------------------------------------
  Local function prototypes
------------------------------------------------------------------------------*/

static i32 InitialQp_new(i32 bits, i32 pels);
static void MbQuant_new(h264RateControl_s * rc);
static void LinearModel_new(h264RateControl_s * rc);
static void AdaptiveModel_new(h264RateControl_s * rc);
static void SourceParameter_new(h264RateControl_s * rc, i32 nonZeroCnt);
static void PicSkip_new(h264RateControl_s * rc);
static void PicQuantLimit_new(h264RateControl_s * rc);
static i32 VirtualBuffer_new(h264VirtualBuffer_s *vb, i32 timeInc, true_e hrd);
static void PicQuant_new(h264RateControl_s * rc);
static i32 avg_rc_error_new(linReg_s *p);
static void update_rc_error_new(linReg_s *p, i32 bits);
static i32 gop_avg_qp_new(h264RateControl_s *rc);
static i32 new_pic_quant_new(linReg_s *p, i32 bits, true_e useQpDeltaLimit);
static i32 get_avg_bits_new(linReg_s *p, i32 n);
static void update_tables_new(linReg_s *p, i32 qp, i32 bits);
static void update_model_new(linReg_s *p);
static i32 lin_sy_new(i32 *qp, i32 *r, i32 n);
static i32 lin_sx_new(i32 *qp, i32 n);
static i32 lin_sxy_new(i32 *qp, i32 *r, i32 n);
static i32 lin_nsxx_new(i32 *qp, i32 n);

/*------------------------------------------------------------------------------

  H264InitRc() Initialize rate control.

------------------------------------------------------------------------------*/
bool_e H264InitRc_new(h264RateControl_s * rc)
{
    h264VirtualBuffer_s *vb = &rc->virtualBuffer;

    if((rc->qpMax > 51))
    {
        return ENCHW_NOK;
    }
    
    /* QP -1: Initial QP estimation done by RC */
    if (rc->qpHdr == -1) {
        i32 tmp = H264Calculate(vb->bitRate, rc->outRateDenom, rc->outRateNum);
        rc->qpHdr = InitialQp_new(tmp, rc->mbPerPic*16*16);
        PicQuantLimit_new(rc);
    }

    if((rc->qpHdr > rc->qpMax) || (rc->qpHdr < rc->qpMin))
    {
        return ENCHW_NOK;
    }

    rc->mbQpAdjustment = MIN(7, MAX(-8, rc->mbQpAdjustment));

    /* HRD needs frame RC and macroblock RC*/
    if (rc->hrd == ENCHW_YES) {
        rc->picRc = ENCHW_YES;
        rc->mbRc = ENCHW_YES;
    }

    /* Macroblock RC needs frame RC */
    if (rc->mbRc == ENCHW_YES)
        rc->picRc = ENCHW_YES;

    /* mbQpAdjustment disables macroblock RC */
    if (rc->mbQpAdjustment && rc->mbRc)
        rc->mbRc = ENCHW_NO;

    rc->coeffCntMax = rc->mbPerPic * 24 * 16;
    rc->frameCoded = ENCHW_YES;
    rc->sliceTypeCur = ISLICE;
    rc->sliceTypePrev = PSLICE;

    rc->qpHdrPrev  = rc->qpHdr;
    rc->fixedQp    = rc->qpHdr;

    vb->bitPerPic = H264Calculate(vb->bitRate, rc->outRateDenom, rc->outRateNum);

    /* Check points are smootly distributed except last one */
    {
        h264QpCtrl_s *qpCtrl = &rc->qpCtrl;

        qpCtrl->checkPoints = MIN(rc->mbRows - 1, CHECK_POINTS_MAX);

        if(rc->mbRc == ENCHW_YES)
        {
            qpCtrl->checkPointDistance =
                rc->mbPerPic / (qpCtrl->checkPoints + 1);
        }
        else
        {
            qpCtrl->checkPointDistance = 0;
        }
    }

#if defined(TRACE_RC) && (DBGOUTPUT == fpRcTrc)
    if (!fpRcTrc) fpRcTrc = fopen("rc.trc", "wt");
#endif

    /* new rate control algorithm */
    update_rc_error_new(&rc->rError, 0x7fffffff);
    update_rc_error_new(&rc->intraError, 0x7fffffff);
    rc->frameCnt = 0;
    rc->linReg.pos = 0;
    rc->linReg.len = 0;
    rc->linReg.a1  = 0;
    rc->linReg.a2  = 0;
    rc->linReg.qs[0]   = q_step[51];
    rc->linReg.bits[0] = 0;
    rc->linReg.qp_prev = rc->qpHdr;
    rc->intra.pos = 0;
    rc->intra.len = 0;
    rc->intra.a1  = 0;
    rc->intra.a2  = 0;
    rc->intra.qs[0]   = q_step[51];
    rc->intra.bits[0] = 0;
    rc->intra.qp_prev = rc->qpHdr;
    rc->gopQpSum = 0;
    rc->gopQpDiv = 0;
    /* API parameter is named gopLen but the actual usage is rate controlling
     * window in frames. RC tries to match the target bitrate inside the
     * window. Each window can contain multiple GOPs and the RC adapts to the
     * intra rate by calculating intraInterval. */
    rc->windowLen = rc->gopLen;
    vb->windowRem = rc->gopLen;
    rc->intraIntervalCtr = rc->intraInterval = rc->gopLen;
    rc->targetPicSize = 0;
    rc->frameBitCnt = 0;
	//printf("rc->gopLen=%d\n", rc->gopLen);
    DBG(0, (DBGOUTPUT, "\nInitRc: picRc\t\t%i  hrd\t%i  PicSkip_new\t%i\n",
                     rc->picRc, rc->hrd, rc->PicSkip_new));
    DBG(0, (DBGOUTPUT, "  mbRc\t\t\t%i  qpHdr\t%i  Min,Max\t%i,%i\n",
                     rc->mbRc, rc->qpHdr, rc->qpMin, rc->qpMax));
    DBG(0, (DBGOUTPUT, "  checkPointDistance\t%i\n",
                     rc->qpCtrl.checkPointDistance));

    DBG(0, (DBGOUTPUT, "  CPBsize\t%i\n BitRate\t%i\n BitPerPic\t%i\n",
            vb->bufferSize, vb->bitRate, vb->bitPerPic));

    rc->sei.hrd = rc->hrd;

    if(rc->hrd)
    {
        vb->bucketFullness =
            H264Calculate(vb->bufferSize, INITIAL_BUFFER_FULLNESS, 100);
        rc->gDelaySum = H264Calculate(90000, vb->bufferSize, vb->bitRate);
        rc->gInitialDelay = H264Calculate(90000, vb->bucketFullness, vb->bitRate);
        rc->gInitialDoffs = rc->gDelaySum - rc->gInitialDelay;
        vb->bucketFullness = vb->bufferSize - vb->bucketFullness;
        /* Because is the first frame. Avoids if clauses in VirtualBuffer_new() */
        vb->bucketFullness += vb->bitPerPic;
#ifdef TRACE_RC
        rc->gBufferMin = vb->bufferSize;
        rc->gBufferMax = 0;
#endif
        rc->sei.icrd = (u32)rc->gInitialDelay;
        rc->sei.icrdo = (u32)rc->gInitialDoffs;
    
        DBG(1, (DBGOUTPUT, "\n InitialDelay\t%i\n Offset\t\t%i\n",
                rc->gInitialDelay, rc->gInitialDoffs));
    }
    return ENCHW_OK;
}

/*------------------------------------------------------------------------------

  InitialQp_new()  Returns sequence initial quantization parameter.

------------------------------------------------------------------------------*/
static i32 InitialQp_new(i32 bits, i32 pels)
{
    const i32 qp_tbl[2][9] = {
                    {27, 44, 72, 119, 192, 314, 453, 653, 0x7FFFFFFF},
                    /*{26, 38, 59, 96, 173, 305, 545, 0x7FFFFFFF},*/
                    {49, 45, 41, 37, 33, 29, 25, 21, 17}};
    const i32 upscale = 8000;
    i32 i = -1;

    /* prevents overflow, QP would anyway be 17 with this high bitrate
       for all resolutions under and including 1920x1088 */
    if (bits > 1000000)
        return 17;

    /* Make room for multiplication */
    pels >>= 8;
    bits >>= 5;

    /* Adjust the bits value for the current resolution */
    bits *= pels + 250;
    ASSERT(pels > 0);
    ASSERT(bits > 0);
    bits /= 350 + (3 * pels) / 4;
    bits = H264Calculate(bits, upscale, pels << 6);

    while (qp_tbl[0][++i] < bits);

    DBG(0, (DBGOUTPUT, "BPP\t\t%d\n", bits));

    return qp_tbl[1][i];
}

/*------------------------------------------------------------------------------
  VirtualBuffer_new()  Return difference of target and real buffer fullness.
  Virtual buffer and real bit count grow until one second.  After one second
  output bit rate per second is removed from virtualBitCnt and realBitCnt. Bit
  drifting has been taken care.

  If the leaky bucket in VBR mode becomes empty (e.g. underflow), those R * T_e
  bits are lost and must be decremented from virtualBitCnt. (NOTE: Drift
  calculation will mess virtualBitCnt up, so the loss is added to realBitCnt)
------------------------------------------------------------------------------*/
static i32 VirtualBuffer_new(h264VirtualBuffer_s *vb, i32 timeInc, true_e hrd)
{
    i32 drift, target, bitPerPic = vb->bitPerPic;
    if (hrd) {
#if RC_CBR_HRD
        /* In CBR mode, bucket _must not_ underflow. Insert filler when
         * needed. */
        vb->bucketFullness -= bitPerPic;
#else
        if (vb->bucketFullness >= bitPerPic) {
            vb->bucketFullness -= bitPerPic;
        } else {
            vb->realBitCnt += (bitPerPic - vb->bucketFullness);
            vb->bucketFullness = 0;
        }
#endif
    }

    /* Saturate realBitCnt, this is to prevent overflows caused by much greater
       bitrate setting than is really possible to reach */
    if (vb->realBitCnt > 0x1FFFFFFF)
        vb->realBitCnt = 0x1FFFFFFF;
    if (vb->realBitCnt < -0x1FFFFFFF)
        vb->realBitCnt = -0x1FFFFFFF;

    vb->picTimeInc    += timeInc;
    vb->virtualBitCnt += H264Calculate(vb->bitRate, timeInc, vb->timeScale);
    target = vb->virtualBitCnt - vb->realBitCnt;

    /* Saturate target, prevents rc going totally out of control.
       This situation should never happen. */
    if (target > 0x1FFFFFFF)
        target = 0x1FFFFFFF;
    if (target < -0x1FFFFFFF)
        target = -0x1FFFFFFF;

    /* picTimeInc must be in range of [0, timeScale) */
    while (vb->picTimeInc >= vb->timeScale) {
        vb->picTimeInc    -= vb->timeScale;
        vb->virtualBitCnt -= vb->bitRate;
        vb->realBitCnt    -= vb->bitRate;
    }
    drift = H264Calculate(vb->bitRate, vb->picTimeInc, vb->timeScale);
    drift -= vb->virtualBitCnt;
    vb->virtualBitCnt += drift;

    DBG(1, (DBGOUTPUT, "virtualBitCnt:\t\t%6i  realBitCnt: %i  ", 
                vb->virtualBitCnt, vb->realBitCnt));
    DBG(1, (DBGOUTPUT, "target: %i  timeInc: %i\n", target, timeInc));
    return target;
}

/*------------------------------------------------------------------------------
  H264AfterPicRc()  Update source model, bit rate target error and linear 
  regression model for frame QP calculation. If HRD enabled, check leaky bucket
  status and return RC_OVERFLOW if coded frame must be skipped. Otherwise
  returns number of required filler payload bytes.
------------------------------------------------------------------------------*/
i32 H264AfterPicRc_new(h264RateControl_s * rc, u32 nonZeroCnt, u32 byteCnt,
        u32 qpSum)
{
    h264VirtualBuffer_s *vb = &rc->virtualBuffer;
    i32 bitPerPic = rc->virtualBuffer.bitPerPic;
    i32 tmp, stat, bitCnt = (i32)byteCnt * 8;

    (void) bitPerPic;
    rc->qpSum = (i32)qpSum;
    rc->frameBitCnt = bitCnt;
    rc->nonZeroCnt = nonZeroCnt;
    rc->gopBitCnt += bitCnt;
    rc->frameCnt++;

    if (rc->targetPicSize) {
        tmp = ((bitCnt - rc->targetPicSize) * 100) /
                rc->targetPicSize;
    } else {
        tmp = -1;
    }

    DBG(0, (DBGOUTPUT, "\nAFTER PIC RC:\n"));
    DBG(0, (DBGOUTPUT, "BitCnt %3d  BitErr/avg %3d%%  ", bitCnt,
                ((bitCnt - bitPerPic) * 100) / (bitPerPic+1)));
    DBG(1, (DBGOUTPUT, "BitErr/target %3i%%  qpHdr %2i  avgQp %4i\n", 
                tmp, rc->qpHdr, rc->qpSum / rc->mbPerPic));

    /* Calculate the source parameter only for INTER frames */
    if (rc->sliceTypeCur != ISLICE && rc->sliceTypeCur != ISLICES)
        SourceParameter_new(rc, rc->nonZeroCnt);

    /* Store the error between target and actual frame size */
    if (rc->sliceTypeCur != ISLICE && rc->sliceTypeCur != ISLICES) {
        /* Saturate the error to avoid inter frames with
         * mostly intra MBs to affect too much */
        update_rc_error_new(&rc->rError,
                MIN(bitCnt - rc->targetPicSize, 2*rc->targetPicSize));
    } else {
        update_rc_error_new(&rc->intraError, bitCnt - rc->targetPicSize);
    }

    /* Update number of bits used for residual, inter or intra */
    if (rc->sliceTypeCur != ISLICE && rc->sliceTypeCur != ISLICES) {
        update_tables_new(&rc->linReg, rc->qpHdrPrev,
                   H264Calculate(bitCnt, 256, rc->mbPerPic));
        update_model_new(&rc->linReg);
    } else {
        update_tables_new(&rc->intra, rc->qpHdrPrev,
                   H264Calculate(bitCnt, 256, rc->mbPerPic));
        update_model_new(&rc->intra);
    }

    /* Post-frame skip if HRD buffer overflow */
    if ((rc->hrd == ENCHW_YES) && (bitCnt > (vb->bufferSize - vb->bucketFullness))) {
        DBG(1, (DBGOUTPUT, "Be: %7i  ", vb->bucketFullness));
        DBG(1, (DBGOUTPUT, "fillerBits %5i  ", 0));
        DBG(1, (DBGOUTPUT, "bitCnt %d  spaceLeft %d  ",
                bitCnt, (vb->bufferSize - vb->bucketFullness)));
        DBG(1, (DBGOUTPUT, "bufSize %d  bucketFullness %d  bitPerPic %d\n", 
                vb->bufferSize, vb->bucketFullness, bitPerPic));
        DBG(0, (DBGOUTPUT, "HRD overflow, frame discard\n"));
        rc->frameCoded = ENCHW_NO;
        return H264RC_OVERFLOW;
    } else {
        vb->bucketFullness += bitCnt;
        vb->realBitCnt += bitCnt;
    }

    DBG(1, (DBGOUTPUT, "plot\t%4i\t%4i\t%8i\t%8i\t%8i\t%8i\t%8i\n",
            rc->frameCnt, rc->qpHdr, rc->targetPicSize, bitCnt,
            bitPerPic, rc->gopAvgBitCnt, vb->realBitCnt-vb->virtualBitCnt));

    if (rc->hrd == ENCHW_NO) {
        return 0;
    }

    tmp = 0;

#if RC_CBR_HRD
    /* Bits needed to prevent bucket underflow */
    tmp = bitPerPic - vb->bucketFullness;

    if (tmp > 0) {
        tmp = (tmp + 7) / 8;
        vb->bucketFullness += tmp * 8;
        vb->realBitCnt += tmp * 8;
    } else {
        tmp = 0;
    }
#endif

    /* Update Buffering Info */
    stat = vb->bufferSize - vb->bucketFullness;

    rc->gInitialDelay = H264Calculate(90000, stat, vb->bitRate);
    rc->gInitialDoffs = rc->gDelaySum - rc->gInitialDelay;

    rc->sei.icrd  = (u32)rc->gInitialDelay;
    rc->sei.icrdo = (u32)rc->gInitialDoffs;

    DBG(1, (DBGOUTPUT, "initialDelay: %5i  ", rc->gInitialDelay));
    DBG(1, (DBGOUTPUT, "initialDoffs: %5i\n", rc->gInitialDoffs));
    DBG(1, (DBGOUTPUT, "Be: %7i  ", vb->bucketFullness));
    DBG(1, (DBGOUTPUT, "fillerBits %5i\n", tmp * 8));

#ifdef TRACE_RC
    if (vb->bucketFullness < rc->gBufferMin) {
        rc->gBufferMin = vb->bucketFullness;
    }
    if (vb->bucketFullness > rc->gBufferMax) {
        rc->gBufferMax = vb->bucketFullness;
    }
    DBG(1, (DBGOUTPUT, "\nLeaky Bucket Min: %i (%d%%)  Max: %i (%d%%)\n", 
            rc->gBufferMin, rc->gBufferMin*100/vb->bufferSize,
            rc->gBufferMax, rc->gBufferMax*100/vb->bufferSize));
#endif
    return tmp;
}

/*------------------------------------------------------------------------------
  H264BeforePicRc()  Update virtual buffer, and calculate picInitQp for current
  picture , and coded status.
------------------------------------------------------------------------------*/
void H264BeforePicRc_new(h264RateControl_s * rc, u32 timeInc, u32 sliceType)
{
    h264VirtualBuffer_s *vb = &rc->virtualBuffer;
    i32 i, rcWindow, intraBits = 0, tmp = 0;

    rc->frameCoded = ENCHW_YES;
    rc->sliceTypeCur = sliceType;

    DBG(0, (DBGOUTPUT, "\nBEFORE PIC RC:\n"));
    DBG(0, (DBGOUTPUT, "Frame type current\t%2i\n", sliceType));

    tmp = VirtualBuffer_new(&rc->virtualBuffer, (i32) timeInc, rc->hrd);
    //printf("tmp0=%d\n", tmp);
    for(i = 0; i < CHECK_POINTS_MAX; i++) {
        rc->qpCtrl.wordCntTarget[i] = 0;
    }

    if (vb->windowRem == 0) {
        vb->windowRem = rc->windowLen-1;
        /* New bitrate window, reset error counters */
        update_rc_error_new(&rc->rError, 0x7fffffff);
        /* Don't reset intra error in case of intra-only, it would cause step. */
        if (rc->sliceTypeCur != rc->sliceTypePrev)
            update_rc_error_new(&rc->intraError, 0x7fffffff);
    } else {
        vb->windowRem--;
    }

    /* Calculate target size for this picture. Adjust the target bitPerPic
     * with the cumulated error between target and actual bitrates (tmp).
     * Also take into account the bits used by intra frame starting the GOP. */
    if (rc->sliceTypeCur != ISLICE && rc->sliceTypeCur != ISLICES &&
        rc->intraInterval > 1) {
        /* GOP bits that are used by intra frame. Amount of bits
         * "stolen" by intra from each inter frame in the GOP. */
        intraBits = vb->bitPerPic*rc->intraInterval*get_avg_bits_new(&rc->gop, 10)/100;
        intraBits -= vb->bitPerPic;
        intraBits /= (rc->intraInterval-1);
        intraBits = MAX(0, intraBits);
    }

    /* Compensate for intra "stealing" bits from inters. */
    tmp += intraBits*(rc->intraInterval-rc->intraIntervalCtr);
	//printf("tmp1=%d\n", tmp);
#ifdef RC_WINDOW_STRICT
    /* In the end of window don't be too strict with matching the error
     * otherwise the end of window tends to twist QP. */
    rcWindow = MAX(MAX(3, rc->windowLen/8), vb->windowRem);
#else
    /* Actually we can be fairly easy with this one, let's make it
     * a moving window to smoothen the changes. */
    rcWindow = MAX(1, rc->windowLen);
	//printf("rc->windowLen=%d\n", rc->windowLen);
#endif
	//printf("vb->bitPerPic=%d,intraBits=%d,tmp=%d,rcWindow=%d\n", vb->bitPerPic, intraBits, tmp, rcWindow);
    rc->targetPicSize = vb->bitPerPic - intraBits + DIV(tmp, rcWindow);
    /* Don't let the target go negative because it won't make any difference
     * and it will confuse RC because it can never be reached. */
    rc->targetPicSize = MAX(0, rc->targetPicSize);
    
    DBG(1, (DBGOUTPUT, "intraBits: %7i\tintraRatio: %3i%%\n",
                intraBits, get_avg_bits_new(&rc->gop, 10)));
    DBG(1, (DBGOUTPUT, "WndRem: %4i  ", vb->windowRem));
    if (rc->sliceTypeCur == ISLICE || rc->sliceTypeCur == ISLICES) {
        DBG(1, (DBGOUTPUT, "Rd: %6d  ", avg_rc_error_new(&rc->intraError)));
    } else {
        DBG(1, (DBGOUTPUT, "Rd: %6d  ", avg_rc_error_new(&rc->rError)));
    }
    DBG(1, (DBGOUTPUT, "Tr: %7d\n", rc->targetPicSize));

    if(rc->picSkip)
        PicSkip_new(rc);

    /* determine initial quantization parameter for current picture */
    PicQuant_new(rc);
    
    /* quantization parameter user defined limitations */
    PicQuantLimit_new(rc);
    /* Store the start QP, before ROI adjustment */
    rc->qpHdrPrev = rc->qpHdr;

    if(rc->sliceTypeCur == ISLICE || rc->sliceTypeCur == ISLICES)
    {
        if(rc->fixedIntraQp)
            rc->qpHdr = rc->fixedIntraQp;
        else if (rc->sliceTypePrev != ISLICE && rc->sliceTypePrev != ISLICES)
            rc->qpHdr += rc->intraQpDelta;
        
        /* quantization parameter user defined limitations still apply */
        PicQuantLimit_new(rc);
        if (rc->intraIntervalCtr > 1)
            rc->intraInterval = rc->intraIntervalCtr;
        rc->intraIntervalCtr = 1;
    }
    else
    {
        /* trace the QP over GOP, excluding Intra QP */
        rc->gopQpSum += rc->qpHdr;
        rc->gopQpDiv++;
        rc->intraIntervalCtr++;

        /* Check that interval is repeating */
        if (rc->intraIntervalCtr > rc->intraInterval)
            rc->intraInterval = rc->intraIntervalCtr;
    }

    /* mb rate control (check point rate control) */
#ifdef ROI_SUPPORT
    if (rc->roiRc)
    {
        H264EncRoiModel(rc);
        PicQuantLimit_new(rc);
    }
    else 
#endif
    if (rc->mbRc)
    {
        MbQuant_new(rc);
    }

    /* reset counters */
    rc->qpSum = 0;
    rc->qpLastCoded = rc->qpHdr;
    rc->qpTarget = rc->qpHdr;
    rc->nonZeroCnt = 0;
    rc->sliceTypePrev = rc->sliceTypeCur;

    DBG(0, (DBGOUTPUT, "Frame type current\t%i\n",rc->sliceTypeCur));
    DBG(0, (DBGOUTPUT, "Frame coded\t\t%2i\n", rc->frameCoded));
    DBG(0, (DBGOUTPUT, "Frame qpHdr\t\t%2i\n", rc->qpHdr));

    for(i = 0; i < CHECK_POINTS_MAX; i++) {
        DBG(1, (DBGOUTPUT, "CP %i  mbNum %4i  wTarg %5i\n", i,
                (rc->qpCtrl.checkPointDistance * (i + 1)),
                rc->qpCtrl.wordCntTarget[i]*32));
    }

    rc->sei.crd += timeInc;

    rc->sei.dod = 0;
}

/*------------------------------------------------------------------------------

  MbQuant_new()

------------------------------------------------------------------------------*/
void MbQuant_new(h264RateControl_s * rc)
{
    i32 nonZeroTarget;

    /* Disable Mb Rc for Intra Slices, because coeffTarget will be wrong */
    if(rc->sliceTypeCur == ISLICE || rc->sliceTypeCur == ISLICES ||
       rc->srcPrm == 0)
    {
        return;
    }

    /* Required zero cnt */
    nonZeroTarget = H264Calculate(rc->targetPicSize, 256, rc->srcPrm);
    nonZeroTarget = MIN(rc->coeffCntMax, MAX(0, nonZeroTarget));

    nonZeroTarget = MIN(0x7FFFFFFFU / 1024U, (u32)nonZeroTarget);

    rc->virtualBuffer.nonZeroTarget = nonZeroTarget;

    /* Use linear model when previous frame can't be used for prediction */
    if ((rc->sliceTypeCur != rc->sliceTypePrev) || (rc->nonZeroCnt == 0)) 
    {
        LinearModel_new(rc);
    }
    else
    {
        AdaptiveModel_new(rc);
    }
}

/*------------------------------------------------------------------------------

  LinearModel_new()

------------------------------------------------------------------------------*/
void LinearModel_new(h264RateControl_s * rc)
{
    const i32 sscale = 256;
    h264QpCtrl_s *qc = &rc->qpCtrl;
    i32 scaler;
    i32 i;
    i32 tmp, nonZeroTarget = rc->virtualBuffer.nonZeroTarget;

    ASSERT(nonZeroTarget < (0x7FFFFFFF / sscale));

    if(nonZeroTarget > 0)
    {
        scaler = H264Calculate(nonZeroTarget, sscale, (i32) rc->mbPerPic);
    }
    else
    {
        return;
    }

    DBG(1, (DBGOUTPUT, " Linear Target: %8d prevCnt:\t %6d Scaler:\t %6d\n", 
            nonZeroTarget, rc->nonZeroCnt, scaler / sscale));

    for(i = 0; i < rc->qpCtrl.checkPoints; i++)
    {
        tmp = (scaler * (qc->checkPointDistance * (i + 1) + 1)) / sscale;
        tmp = MIN(WORD_CNT_MAX, tmp / 32 + 1);
        if (tmp < 0) tmp = WORD_CNT_MAX;    /* Detect overflow */
        qc->wordCntTarget[i] = tmp; /* div32 for regs */
    }

    /* calculate nz count for avg. bits per frame */
    tmp = H264Calculate(rc->virtualBuffer.bitPerPic, 256, rc->srcPrm);

    DBG(1, (DBGOUTPUT, "Error Limit:\t %8d SrcPrm:\t %6d\n", 
                tmp, rc->srcPrm / 256));

    qc->wordError[0] = -tmp * 3;
    qc->qpChange[0] = -3;
    qc->wordError[1] = -tmp * 2;
    qc->qpChange[1] = -2;
    qc->wordError[2] = -tmp * 1;
    qc->qpChange[2] = -1;
    qc->wordError[3] = tmp * 1;
    qc->qpChange[3] = 0;
    qc->wordError[4] = tmp * 2;
    qc->qpChange[4] = 1;
    qc->wordError[5] = tmp * 3;
    qc->qpChange[5] = 2;
    qc->wordError[6] = tmp * 4;
    qc->qpChange[6] = 3;

    for(i = 0; i < CTRL_LEVELS; i++)
    {
        tmp = qc->wordError[i];
        tmp = CLIP3(tmp/4, -32768, 32767);
        qc->wordError[i] = tmp;
    }
}


/*------------------------------------------------------------------------------

  AdaptiveModel_new()

------------------------------------------------------------------------------*/
void AdaptiveModel_new(h264RateControl_s * rc)
{
    const i32 sscale = 256;
    h264QpCtrl_s *qc = &rc->qpCtrl;
    i32 i;
    i32 tmp, nonZeroTarget = rc->virtualBuffer.nonZeroTarget;
    i32 scaler;

    ASSERT(nonZeroTarget < (0x7FFFFFFF / sscale));

    if((nonZeroTarget > 0) && (rc->nonZeroCnt > 0))
    {
        scaler = H264Calculate(nonZeroTarget, sscale, rc->nonZeroCnt);
    }
    else
    {
        return;
    }
    DBG(1, (DBGOUTPUT, "Adaptive Target: %8d prevCnt:\t %6d Scaler:\t %6d\n", 
            nonZeroTarget, rc->nonZeroCnt, scaler / sscale));

    for(i = 0; i < rc->qpCtrl.checkPoints; i++)
    {
        tmp = (i32) (qc->wordCntPrev[i] * scaler) / sscale;
        tmp = MIN(WORD_CNT_MAX, tmp / 32 + 1);
        if (tmp < 0) tmp = WORD_CNT_MAX;    /* Detect overflow */
        qc->wordCntTarget[i] = tmp; /* div32 for regs */
        DBG(2, (DBGOUTPUT, " CP %i  wordCntPrev %6i  wordCntTarget_div32 %6i\n",
                    i, qc->wordCntPrev[i], qc->wordCntTarget[i]));
    }

    /* Qp change table */

    /* calculate nz count for avg. bits per frame */
    tmp = H264Calculate(rc->virtualBuffer.bitPerPic, 256, (rc->srcPrm * 3));

    DBG(1, (DBGOUTPUT, "Error Limit:\t %8d SrcPrm:\t %6d\n", 
            tmp, rc->srcPrm / 256));

    qc->wordError[0] = -tmp * 3;
    qc->qpChange[0] = -3;
    qc->wordError[1] = -tmp * 2;
    qc->qpChange[1] = -2;
    qc->wordError[2] = -tmp * 1;
    qc->qpChange[2] = -1;
    qc->wordError[3] = tmp * 1;
    qc->qpChange[3] = 0;
    qc->wordError[4] = tmp * 2;
    qc->qpChange[4] = 1;
    qc->wordError[5] = tmp * 3;
    qc->qpChange[5] = 2;
    qc->wordError[6] = tmp * 4;
    qc->qpChange[6] = 3;

    for(i = 0; i < CTRL_LEVELS; i++)
    {
        tmp = qc->wordError[i];
        tmp = CLIP3(tmp/4, -32768, 32767);
        qc->wordError[i] = tmp;
    }
}

/*------------------------------------------------------------------------------

  SourceParameter_new()  Source parameter of last coded frame. Parameters
  has been scaled up by factor 256.

------------------------------------------------------------------------------*/
void SourceParameter_new(h264RateControl_s * rc, i32 nonZeroCnt)
{
    ASSERT(rc->qpSum <= 51 * rc->mbPerPic);
    ASSERT(nonZeroCnt <= rc->coeffCntMax);
    ASSERT(nonZeroCnt >= 0 && rc->coeffCntMax >= 0);

    /* AVOID division by zero */
    if(nonZeroCnt == 0)
    {
        nonZeroCnt = 1;
    }

    rc->srcPrm = H264Calculate(rc->frameBitCnt, 256, nonZeroCnt);

    DBG(1, (DBGOUTPUT, "nonZeroCnt %6i, srcPrm %i\n", 
                nonZeroCnt, rc->srcPrm/256));

}

/*------------------------------------------------------------------------------
  PicSkip_new()  Decrease framerate if not enough bits available.
------------------------------------------------------------------------------*/
void PicSkip_new(h264RateControl_s * rc)
{
    h264VirtualBuffer_s *vb = &rc->virtualBuffer;
    i32 bitAvailable = vb->virtualBitCnt - vb->realBitCnt;
    i32 skipIncLimit = -vb->bitPerPic / 3;
    i32 skipDecLimit = vb->bitPerPic / 3;

    /* When frameRc is enabled, skipFrameTarget is not allowed to be > 1
     * This makes sure that not too many frames is skipped and lets
     * the frameRc adjust QP instead of skipping many frames */
    if(((rc->picRc == ENCHW_NO) || (vb->skipFrameTarget == 0)) &&
       (bitAvailable < skipIncLimit))
    {
        vb->skipFrameTarget++;
    }

    if((bitAvailable > skipDecLimit) && vb->skipFrameTarget > 0)
    {
        vb->skipFrameTarget--;
    }

    if(vb->skippedFrames < vb->skipFrameTarget)
    {
        vb->skippedFrames++;
        rc->frameCoded = ENCHW_NO;
    }
    else
    {
        vb->skippedFrames = 0;
    }
}

/*------------------------------------------------------------------------------
  PicQuant_new()  Calculate quantization parameter for next frame. In the beginning
                of window use previous GOP average QP and otherwise find new QP
                using the target size and previous frames QPs and bit counts.
------------------------------------------------------------------------------*/
void PicQuant_new(h264RateControl_s * rc)
{
    i32 normBits, targetBits;
    true_e useQpDeltaLimit = ENCHW_YES;
 
    if(rc->picRc != ENCHW_YES)
    {
        rc->qpHdr = rc->fixedQp;
        DBG(1, (DBGOUTPUT, "R/cx:  xxxx  QP: xx xx D:  xxxx newQP: xx\n"));
        return;
    }

    /* If HRD is enabled we must make sure this frame fits in buffer */
    if (rc->hrd == ENCHW_YES) 
    {
        i32 bitsAvailable = 
            (rc->virtualBuffer.bufferSize - rc->virtualBuffer.bucketFullness); 

        /* If the previous frame didn't fit the buffer we don't limit QP change */
        if (rc->frameBitCnt > bitsAvailable) {
            useQpDeltaLimit = ENCHW_NO;
        }
    }

    /* determine initial quantization parameter for current picture */
    if (rc->sliceTypeCur == ISLICE || rc->sliceTypeCur == ISLICES) {
        /* Default intra QP == prev GOP average */
        rc->qpHdr = gop_avg_qp_new(rc);
        /* If all frames are intra we calculate new QP
         * for intra the same way as for inter */
        if (rc->sliceTypePrev == ISLICE || rc->sliceTypePrev == ISLICES) {
            targetBits = rc->targetPicSize - avg_rc_error_new(&rc->intraError);
            normBits = H264Calculate(targetBits, 256, rc->mbPerPic);
            rc->qpHdr = new_pic_quant_new(&rc->intra, normBits, useQpDeltaLimit);
        } else {
            DBG(1, (DBGOUTPUT, "R/cx:  xxxx  QP: xx xx D:  xxxx newQP: xx\n"));
        }
    } else {
        /* Calculate new QP by matching to previous inter frames R-Q curve */
		
        targetBits = rc->targetPicSize - avg_rc_error_new(&rc->rError);
        normBits = H264Calculate(targetBits, 256, rc->mbPerPic);
        rc->qpHdr = new_pic_quant_new(&rc->linReg, normBits, useQpDeltaLimit);
		//printf("rc->targetBits=%d,useQpDeltaLimit=%d\n", rc->targetPicSize, useQpDeltaLimit);
		//printf("targetBits=%d,normBits=%d,rc->qpHdr=%d\n", targetBits, normBits, rc->qpHdr);
    }
}

/*------------------------------------------------------------------------------

  PicQuantLimit_new()

------------------------------------------------------------------------------*/
void PicQuantLimit_new(h264RateControl_s * rc)
{
    rc->qpHdr = MIN(rc->qpMax, MAX(rc->qpMin, rc->qpHdr));
}

/*------------------------------------------------------------------------------
  avg_rc_error_new()  PI(D)-control for rate prediction error.
------------------------------------------------------------------------------*/
static i32 avg_rc_error_new(linReg_s *p)
{
    return DIV(p->bits[2] * 4 + p->bits[1] * 6 + p->bits[0] * 0, 100);
}

/*------------------------------------------------------------------------------
  update_overhead()  Update PI(D)-control values
------------------------------------------------------------------------------*/
static void update_rc_error_new(linReg_s *p, i32 bits)
{
    p->len = 3;

    if (bits == (i32)0x7fffffff) {
        /* RESET */
        p->bits[0] = 0;
        p->bits[1] = 0;
        p->bits[2] = 0;
        return;
    }
    p->bits[0] = bits - p->bits[2]; /* Derivative */
    if ((bits > 0) && (bits + p->bits[1] > p->bits[1]))
        p->bits[1] = bits + p->bits[1]; /* Integral */
    if ((bits < 0) && (bits + p->bits[1] < p->bits[1]))
        p->bits[1] = bits + p->bits[1]; /* Integral */
    p->bits[2] = bits;              /* Proportional */
    DBG(1, (DBGOUTPUT, "P %6d I %7d D %7d\n", p->bits[2],  p->bits[1], p->bits[0]));
}

/*------------------------------------------------------------------------------
  gop_avg_qp_new()  Average quantization parameter of P frames since previous I.
------------------------------------------------------------------------------*/
i32 gop_avg_qp_new(h264RateControl_s *rc)
{
    i32 tmp = rc->qpHdrPrev;

    if (rc->gopQpSum && rc->gopQpDiv) {
        tmp = DIV(rc->gopQpSum, rc->gopQpDiv);
    }
    /* Average bit count per frame for previous GOP (intra + inter) */
    rc->gopAvgBitCnt = DIV(rc->gopBitCnt, (rc->gopQpDiv+1));
    /* Ratio of intra_frame_bits/all_gop_bits % for previous GOP */
    if (rc->gopBitCnt) {
        i32 gopIntraBitRatio =
            H264Calculate(get_avg_bits_new(&rc->intra,1), rc->mbPerPic, 256) * 100;
        gopIntraBitRatio = DIV(gopIntraBitRatio, rc->gopBitCnt);
        /* GOP bit count must be > intra bit count, so ratio must be < 100 */
        gopIntraBitRatio = MIN(99, gopIntraBitRatio);
        update_tables_new(&rc->gop, tmp, gopIntraBitRatio);
    }
    rc->gopQpSum = 0;
    rc->gopQpDiv = 0;
    rc->gopBitCnt = 0;

    return tmp;
}

/*------------------------------------------------------------------------------
  new_pic_quant_new()  Calculate new quantization parameter from the 2nd degree R-Q
  equation. Further adjust Qp for "smoother" visual quality.
------------------------------------------------------------------------------*/
static i32 new_pic_quant_new(linReg_s *p, i32 bits, true_e useQpDeltaLimit)
{
    i32 tmp, qp_best = p->qp_prev, qp = p->qp_prev, diff;
    i32 diff_prev = 0, qp_prev = 0, diff_best = 0x7FFFFFFF;

    DBG(1, (DBGOUTPUT, "R/cx:%6d ",bits));

    if (p->a1 == 0 && p->a2 == 0) {
        DBG(1, (DBGOUTPUT, " QP: xx xx D:  ==== newQP: %2d\n", qp));
        return qp;
    }
    if (bits <= 0) {
        if (useQpDeltaLimit)
            qp = MIN(51, MAX(0, qp + QP_DELTA));
        else
            qp = MIN(51, MAX(0, qp + 10));

        DBG(1, (DBGOUTPUT, " QP: xx xx D:  ---- newQP: %2d\n", qp));
        return qp;
    }

    do {
        tmp  = DIV(p->a1, q_step[qp]);
        tmp += DIV(p->a2, q_step[qp] * q_step[qp]);
        diff = ABS(tmp - bits);

        if (diff < diff_best) {
            if (diff_best == 0x7FFFFFFF) {
                diff_prev = diff;
                qp_prev   = qp;
            } else {
                diff_prev = diff_best;
                qp_prev   = qp_best;
            }
            diff_best = diff;
            qp_best   = qp;
            if ((tmp - bits) <= 0) {
                if (qp < 1) {
                    break;
                }
                qp--;
            } else {
                if (qp > 50) {
                    break;
                }
                qp++;
            }
        } else {
            break;
        }
    } while ((qp >= 0) && (qp <= 51));
    qp = qp_best;

    DBG(1,(DBGOUTPUT, " QP: %2d %2d D: %5d", qp, qp_prev, diff_prev - diff_best));

    /* One unit change in Qp changes rate about 12% ca. 1/8. If the
     * difference is less than half use the one closer to previous value */
    if (ABS(diff_prev - diff_best) <= ABS(bits) / 16) {
        qp = qp_prev;
    }
    DBG(1, (DBGOUTPUT, " newQP: %2d\n", qp));

    /* Limit Qp change for smoother visual quality */
    if (useQpDeltaLimit) {
        tmp = qp - p->qp_prev;
        if (tmp > QP_DELTA) {
            qp = p->qp_prev + QP_DELTA;
            /* When QP is totally wrong, allow faster QP increase */
            if (tmp > QP_DELTA_LIMIT)
                qp = p->qp_prev + QP_DELTA*2;
        } else if (tmp < -QP_DELTA) {
            qp = p->qp_prev - QP_DELTA;
        }
    }

    return qp;
}

/*------------------------------------------------------------------------------
  get_avg_bits_new()
------------------------------------------------------------------------------*/
static i32 get_avg_bits_new(linReg_s *p, i32 n)
{
    i32 i;
    i32 sum = 0;
    i32 pos = p->pos;

    if (!p->len) return 0;

    if (n == -1 || n > p->len)
        n = p->len;

    i = n;
    while (i--) {
        if (pos) pos--;
        else pos = p->len-1;
        sum += p->bits[pos];
        if (sum < 0) {
            return I32_MAX / (n-i);
        }
    }
    return DIV(sum, n);
}

/*------------------------------------------------------------------------------
  update_tables_new()  only statistics of PSLICE, please.
------------------------------------------------------------------------------*/
static void update_tables_new(linReg_s *p, i32 qp, i32 bits)
{
    const i32 clen = RC_TABLE_LENGTH;
    i32 tmp = p->pos;

    p->qp_prev   = qp;
    p->qs[tmp]   = q_step[qp];
    p->bits[tmp] = bits;

    if (++p->pos >= clen) {
        p->pos = 0;
    }
    if (p->len < clen) {
        p->len++;
    }
}

/*------------------------------------------------------------------------------
            update_model_new()  Update model parameter by Linear Regression.
------------------------------------------------------------------------------*/
static void update_model_new(linReg_s *p)
{
    i32 *qs = p->qs, *r = p->bits, n = p->len;
    i32 i, a1, a2, sx = lin_sx_new(qs, n), sy = lin_sy_new(qs, r, n);

    for (i = 0; i < n; i++) {
        DBG(2, (DBGOUTPUT, "model: qs %i  r %i\n",qs[i], r[i]));
    }

    a1 = lin_sxy_new(qs, r, n);
    a1 = a1 < I32_MAX / n ? a1 * n : I32_MAX;

    if (sy == 0) {
        a1 = 0;
    } else {
        a1 -= sx < I32_MAX / sy ? sx * sy : I32_MAX;
    }

    a2 = (lin_nsxx_new(qs, n) - (sx * sx));
    if (a2 == 0) {
        if (p->a1 == 0) {
            /* If encountered in the beginning */
            a1 = 0;
        } else {
            a1 = (p->a1 * 2) / 3;
        }
    } else {
        a1 = H264Calculate(a1, DSCY, a2);
    }

    /* Value of a1 shouldn't be excessive (small) */
    a1 = MAX(a1, -262144);
    a1 = MIN(a1,  262143);
    a1 = MAX(a1, -I32_MAX/q_step[51]/RC_TABLE_LENGTH);
    a1 = MIN(a1,  I32_MAX/q_step[51]/RC_TABLE_LENGTH);

    ASSERT(ABS(a1) * sx >= 0);
    ASSERT(sx * DSCY >= 0);
    a2 = DIV(sy * DSCY, n) - DIV(a1 * sx, n);

    DBG(2, (DBGOUTPUT, "model: a2:%9d  a1:%8d\n", a2, a1));

    if (p->len > 0) {
        p->a1 = a1;
        p->a2 = a2;
    }
}

/*------------------------------------------------------------------------------
  lin_sy_new()  calculate value of Sy for n points.
------------------------------------------------------------------------------*/
static i32 lin_sy_new(i32 *qp, i32 *r, i32 n)
{
    i32 sum = 0;

    while (n--) {
        sum += qp[n] * qp[n] * r[n];
        if (sum < 0) {
            return I32_MAX / DSCY;
        }
    }
    return DIV(sum, DSCY);
}

/*------------------------------------------------------------------------------
  lin_sx_new()  calculate value of Sx for n points.
------------------------------------------------------------------------------*/
static i32 lin_sx_new(i32 *qp, i32 n)
{
    i32 tmp = 0;

    while (n--) {
        ASSERT(qp[n]);
        tmp += qp[n];
    }
    return tmp;
}

/*------------------------------------------------------------------------------
  lin_sxy_new()  calculate value of Sxy for n points.
------------------------------------------------------------------------------*/
static i32 lin_sxy_new(i32 *qp, i32 *r, i32 n)
{
    i32 tmp, sum = 0;

    while (n--) {
        tmp = qp[n] * qp[n] * qp[n];
        if (tmp > r[n]) {
            sum += DIV(tmp, DSCY) * r[n];
        } else {
            sum += tmp * DIV(r[n], DSCY);
        }
        if (sum < 0) {
            return I32_MAX;
        }
    }
    return sum;
}

/*------------------------------------------------------------------------------
  lin_nsxx_new()  calculate value of n * Sxy for n points.
------------------------------------------------------------------------------*/
static i32 lin_nsxx_new(i32 *qp, i32 n)
{
    i32 tmp = 0, sum = 0, d = n ;

    while (n--) {
        tmp = qp[n];
        tmp *= tmp;
        sum += d * tmp;
    }
    return sum;
}
