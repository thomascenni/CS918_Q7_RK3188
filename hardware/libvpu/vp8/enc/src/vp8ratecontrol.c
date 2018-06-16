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
#include "vp8ratecontrol.h"
#include "vp8quanttable.h"

/*------------------------------------------------------------------------------
  Module defines
------------------------------------------------------------------------------*/

#ifdef TRACE_RC
#include <stdio.h>
FILE *fpRcTrc = NULL;
/* Select debug output: fpRcTrc (==file rc.trc) or stdout */
#define DBGOUTPUT fpRcTrc
/* Select debug level: 0 = minimum, 2 = maximum */
#define DBG_LEVEL 2
#define DBG(l, str) if (l <= DBG_LEVEL) fprintf str
#else
#define DBG(l, str)
#endif

#define INITIAL_BUFFER_FULLNESS   60    /* Decoder Buffer in procents */
#define MIN_PIC_SIZE              50    /* Procents from picPerPic */

#define DIV(a, b)               (((a) + (SIGN(a) * (b)) / 2) / (b))
#define DSCY                    64 /* n * 64 */
#define I32_MAX                 2147483647 /* 2 ^ 31 - 1 */
#define QP_DELTA                4
#define RC_ERROR_RESET          0x7fffffff

/*------------------------------------------------------------------------------
  Local structures
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
  Local function prototypes
------------------------------------------------------------------------------*/

static i32 InitialQp(i32 bits, i32 pels);
static void PicSkip(vp8RateControl_s * rc);
static void PicQuantLimit(vp8RateControl_s * rc);
static i32 VirtualBuffer(vp8VirtualBuffer_s *vb, i32 timeInc);
static void PicQuant(vp8RateControl_s * rc);
static i32 avg_rc_error(linReg_s *p);
static void update_rc_error(linReg_s *p, i32 bits);
static i32 gop_avg_qp(vp8RateControl_s *rc);
static i32 new_pic_quant(linReg_s *p, i32 bits, true_e useQpDeltaLimit);
static void update_tables(linReg_s *p, i32 qp, i32 bits);
static void update_model(linReg_s *p);
static i32 lin_sy(i32 *qp, i32 *r, i32 n);
static i32 lin_sx(i32 *qp, i32 n);
static i32 lin_sxy(i32 *qp, i32 *r, i32 n);
static i32 lin_nsxx(i32 *qp, i32 n);

/*------------------------------------------------------------------------------
  VP8InitRc() Initialize rate control.
------------------------------------------------------------------------------*/
void VP8InitRc(vp8RateControl_s * rc, u32 newStream)
{
    vp8VirtualBuffer_s *vb = &rc->virtualBuffer;
    i32 maxBps;

#if defined(TRACE_RC) && (DBGOUTPUT == fpRcTrc)
    if (!fpRcTrc) fpRcTrc = fopen("rc.trc", "w");
#endif

    if (rc->qpMax >= QINDEX_RANGE)
        rc->qpMax = QINDEX_RANGE-1;

    if (rc->qpMin < 0)
        rc->qpMin = 0;

    /* Limit bitrate settings that are way over head.
     * Maximum limit is half of the uncompressed YUV bitrate (12bpp). */
    maxBps = rc->mbPerPic*16*16*6;   /* Max bits per frame */
    maxBps = VP8Calculate(maxBps, rc->outRateNum, rc->outRateDenom);
    if (maxBps < 0)
        maxBps = I32_MAX;
    vb->bitRate = MIN(vb->bitRate, maxBps);

    vb->bitPerPic = VP8Calculate(vb->bitRate, rc->outRateDenom, rc->outRateNum);

    /* QP -1: Initial QP estimation done by RC */
    if (rc->qpHdr == -1)
        rc->qpHdr = InitialQp(vb->bitPerPic, rc->mbPerPic*16*16);

    PicQuantLimit(rc);

    DBG(0, (DBGOUTPUT, "\nInitRc:\n  picRc\t\t%i\n  picSkip\t%i\n",
                     rc->picRc, rc->picSkip));
    DBG(0, (DBGOUTPUT, "  qpHdr\t\t%i\n  qpMin,Max\t%i,%i\n",
                     rc->qpHdr, rc->qpMin, rc->qpMax));

    DBG(0, (DBGOUTPUT, "  BitRate\t%i\n  BitPerPic\t%i\n",
            vb->bitRate, vb->bitPerPic));

    /* If changing QP/bitrate between frames don't reset GOP RC */
    if (!newStream)
        return;

    rc->qpHdrPrev       = rc->qpHdr;
    rc->fixedQp         = rc->qpHdr;
    rc->frameCoded      = ENCHW_YES;
    rc->currFrameIntra  = 1;
    rc->prevFrameIntra  = 0;
    rc->frameCnt        = 0;
    rc->gopQpSum        = 0;
    rc->gopQpDiv        = 0;
    rc->targetPicSize   = 0;
    rc->frameBitCnt     = 0;

    memset(&rc->linReg, 0, sizeof(linReg_s));
    rc->linReg.qs[0]    = AcQLookup[QINDEX_RANGE-1];
    rc->linReg.qp_prev  = rc->qpHdr;

    vb->gopRem          = rc->gopLen;
    vb->timeScale       = rc->outRateNum;

    update_rc_error(&rc->rError, RC_ERROR_RESET);

}

/*------------------------------------------------------------------------------
  InitialQp()   Returns sequence initial quantization parameter based on the
                configured resolution and bitrate.
------------------------------------------------------------------------------*/
static i32 InitialQp(i32 bits, i32 pels)
{
    /* Table with resulting average bits/pixel as a function of QP.
     * The table is calculated by encoding a set of 4CIF resolution video
     * clips with fixed QP. */
    const i32 qp_tbl[2][12] = {
            {47, 57, 73, 93, 122, 155, 214, 294, 373, 506, 781, 0x7FFFFFFF},
            {120, 110, 100, 90, 80, 70, 60, 50, 40, 30, 20, 10}};
    const i32 upscale = 8000;
    i32 i = -1;

    /* prevents overflow, QP would anyway be 10 with this high bitrate
       for all resolutions under and including 1920x1088 */
    if (bits > 1000000)
        return 10;

    /* Make room for multiplication */
    pels >>= 8;
    bits >>= 5;

    /* Adjust the bits value for the current resolution */
    bits *= pels + 250;
    ASSERT(pels > 0);
    ASSERT(bits > 0);
    bits /= 350 + (3 * pels) / 4;
    bits = VP8Calculate(bits, upscale, pels << 6);

    while (qp_tbl[0][++i] < bits);

    DBG(0, (DBGOUTPUT, "BPP\t\t%d\n", bits));

    return qp_tbl[1][i];
}

/*------------------------------------------------------------------------------
  VirtualBuffer()  Return difference of target and real buffer fullness.
  Virtual buffer and real bit count grow until one second.  After one second
  output bit rate per second is removed from virtualBitCnt and realBitCnt. Bit
  drifting has been taken care.

  If the leaky bucket in VBR mode becomes empty (e.g. underflow), those R * T_e
  bits are lost and must be decremented from virtualBitCnt. (NOTE: Drift
  calculation will mess virtualBitCnt up, so the loss is added to realBitCnt)
------------------------------------------------------------------------------*/
static i32 VirtualBuffer(vp8VirtualBuffer_s *vb, i32 timeInc)
{
    i32 drift, target;

    /* Saturate realBitCnt, this is to prevent overflows caused by much greater
       bitrate setting than is really possible to reach */
    if (vb->realBitCnt > 0x1FFFFFFF)
        vb->realBitCnt = 0x1FFFFFFF;
    if (vb->realBitCnt < -0x1FFFFFFF)
        vb->realBitCnt = -0x1FFFFFFF;

    vb->picTimeInc    += timeInc;
    vb->virtualBitCnt += VP8Calculate(vb->bitRate, timeInc, vb->timeScale);
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
    drift = VP8Calculate(vb->bitRate, vb->picTimeInc, vb->timeScale);
    drift -= vb->virtualBitCnt;
    vb->virtualBitCnt += drift;

    DBG(1, (DBGOUTPUT, "virtualBitCnt:\t%7i\nrealBitCnt:\t%7i",
                vb->virtualBitCnt, vb->realBitCnt));
    DBG(1, (DBGOUTPUT, "  diff bits:\t%7i\n", target));

    return target;
}

/*------------------------------------------------------------------------------
  VP8BeforePicRc()  Update virtual buffer and calculate picInitQp for current
  picture.
------------------------------------------------------------------------------*/
void VP8BeforePicRc(vp8RateControl_s * rc, u32 timeInc, u32 frameTypeIntra)
{
    vp8VirtualBuffer_s *vb = &rc->virtualBuffer;
    i32 tmp = 0;

    rc->frameCoded = ENCHW_YES;
    rc->currFrameIntra = frameTypeIntra;

    DBG(1, (DBGOUTPUT, "\nBEFORE PIC RC: pic=%d\n", rc->frameCnt));
    DBG(1, (DBGOUTPUT, "Frame type:\t%7i  timeInc:\t%7i\n",
                frameTypeIntra, timeInc));

    if (rc->currFrameIntra || vb->gopRem == 1) {
        vb->gopRem = rc->gopLen;
    } else {
        vb->gopRem--;
    }

    /* Use virtual buffer to calculate the difference of target bitrate
     * and actual bitrate */
    tmp = VirtualBuffer(&rc->virtualBuffer, (i32) timeInc);

    /* Calculate target size for this picture */
    rc->targetPicSize = vb->bitPerPic + DIV(tmp, MAX(rc->virtualBuffer.gopRem, 3));
    rc->targetPicSize = MAX(0, rc->targetPicSize);

    if(rc->picSkip)
        PicSkip(rc);

    /* determine initial quantization parameter for current picture */
    PicQuant(rc);
    /* quantization parameter user defined limitations */
    PicQuantLimit(rc);
    /* Store the start QP, before any adjustment */
    rc->qpHdrPrev = rc->qpHdr;

    if(rc->currFrameIntra) {
        if(rc->fixedIntraQp)
            rc->qpHdr = rc->fixedIntraQp;
        else if (!rc->prevFrameIntra)
            rc->qpHdr += rc->intraQpDelta;

        /* quantization parameter user defined limitations still apply */
        PicQuantLimit(rc);
    } else {
        /* trace the QP over GOP, excluding Intra QP */
        rc->gopQpSum += rc->qpHdr;
        rc->gopQpDiv++;
    }

    DBG(0, (DBGOUTPUT, "Frame coded\t%7d  ", rc->frameCoded));
    DBG(0, (DBGOUTPUT, "Frame qpHdr\t%7d  ", rc->qpHdr));
    DBG(0, (DBGOUTPUT, "GopRem:\t%7d  ", vb->gopRem));
    DBG(0, (DBGOUTPUT, "Target bits:\t%7d  ", rc->targetPicSize));
    DBG(1, (DBGOUTPUT, "\nRd:\t\t%7d\n", avg_rc_error(&rc->rError)));

}

/*------------------------------------------------------------------------------
  VP8AfterPicRc()  Update RC statistics after encoding frame.
------------------------------------------------------------------------------*/
void VP8AfterPicRc(vp8RateControl_s * rc, u32 byteCnt)
{
    vp8VirtualBuffer_s *vb = &rc->virtualBuffer;
    i32 tmp, bitCnt = (i32)byteCnt * 8;

    rc->frameCnt++;
    rc->frameBitCnt     = bitCnt;
    rc->prevFrameIntra  = rc->currFrameIntra;
    vb->realBitCnt      += bitCnt;

    if (rc->targetPicSize) {
        tmp = ((bitCnt - rc->targetPicSize) * 100) /
                rc->targetPicSize;
    } else {
        tmp = -1;
    }

    DBG(1, (DBGOUTPUT, "\nAFTER PIC RC:\n"));
    DBG(0, (DBGOUTPUT, "BitCnt\t\t%7d\n", bitCnt));
    DBG(1, (DBGOUTPUT, "BitErr/avg\t%6d%%  ",
                ((bitCnt - vb->bitPerPic) * 100) / (vb->bitPerPic+1)));
    DBG(1, (DBGOUTPUT, "BitErr/target\t%6d%%\n", tmp));

    /* Needs number of bits used for residual */
    if ((!rc->currFrameIntra) || (rc->gopLen == 1)) {

        update_tables(&rc->linReg, rc->qpHdrPrev,
                   VP8Calculate(bitCnt, 256, rc->mbPerPic));

        if (vb->gopRem == rc->gopLen-1) {
            /* First INTER frame of GOP */
            update_rc_error(&rc->rError, RC_ERROR_RESET);
            DBG(2, (DBGOUTPUT, "P\t\t    ---  I\t\t    ---  D\t    ---\n"));
        } else {
            /* Store the error between target and actual frame size
             * Saturate the error to avoid inter frames with
             * mostly intra MBs to affect too much */
            update_rc_error(&rc->rError,
                    MIN(bitCnt - rc->targetPicSize, 2*rc->targetPicSize));
        }
        update_model(&rc->linReg);
    } else {
        DBG(2, (DBGOUTPUT, "P\t\t    xxx  I\t\t    xxx  D\t    xxx\n"));
    }

}

/*------------------------------------------------------------------------------
  PicSkip()  Decrease framerate if not enough bits available.
------------------------------------------------------------------------------*/
void PicSkip(vp8RateControl_s * rc)
{
    vp8VirtualBuffer_s *vb = &rc->virtualBuffer;
    i32 bitAvailable = vb->virtualBitCnt - vb->realBitCnt;
    i32 skipIncLimit = -vb->bitPerPic / 3;
    i32 skipDecLimit = vb->bitPerPic / 3;

    /* When frameRc is enabled, skipFrameTarget is not allowed to be > 1
     * This makes sure that not too many frames is skipped and lets
     * the frameRc adjust QP instead of skipping many frames */
    if (((rc->picRc == ENCHW_NO) || (vb->skipFrameTarget == 0)) &&
       (bitAvailable < skipIncLimit))
        vb->skipFrameTarget++;

    if ((bitAvailable > skipDecLimit) && vb->skipFrameTarget > 0)
        vb->skipFrameTarget--;

    if (vb->skippedFrames < vb->skipFrameTarget) {
        vb->skippedFrames++;
        rc->frameCoded = ENCHW_NO;
    } else {
        vb->skippedFrames = 0;
    }
}

/*------------------------------------------------------------------------------
  PicQuant()  Calculate quantization parameter for next frame. In the beginning
                of GOP use previous GOP average QP and otherwise find new QP
                using the target size and previous frames QPs and bit counts.
------------------------------------------------------------------------------*/
void PicQuant(vp8RateControl_s * rc)
{
    i32 tmp = 0;
    i32 tmpValue, avgRcError;
    true_e useQpDeltaLimit = ENCHW_YES;

    if(rc->picRc != ENCHW_YES) {
        rc->qpHdr = rc->fixedQp;
        DBG(2, (DBGOUTPUT,
                "R/cx:\t\t   xxxx  QP:\t\t  xx xx  D:\t  xxxx  newQP: xx\n"));
        return;
    }

    /* Determine initial quantization parameter for current picture */
    if (rc->currFrameIntra) {
        /* If all frames or every other frame is intra we calculate new QP
         * for intra the same way as for inter */
        if (rc->gopLen == 1 || rc->gopLen == 2) {
            tmp = new_pic_quant(&rc->linReg,
                    VP8Calculate(rc->targetPicSize, 256, rc->mbPerPic),
                    useQpDeltaLimit);
        } else {
            DBG(2, (DBGOUTPUT,
                "R/cx:\t\t   xxxx  QP:\t\t  xx xx  D:\t  xxxx  newQP: xx\n"));
            tmp = gop_avg_qp(rc);
        }
        if (tmp) {
            rc->qpHdr = tmp;
        }
    } else if (rc->prevFrameIntra) {
        /* Previous frame was intra, use the same QP */
        DBG(2, (DBGOUTPUT,
                "R/cx:\t\t   xxxx  QP:\t\t  == ==  D:\t  ====  newQP: ==\n"));
        rc->qpHdr = rc->qpHdrPrev;
    } else {
        /* Calculate new QP by matching to previous frames R-Q curve */
        avgRcError = avg_rc_error(&rc->rError);
        tmpValue = VP8Calculate(rc->targetPicSize  - avgRcError,
                256, rc->mbPerPic);
        rc->qpHdr = new_pic_quant(&rc->linReg, tmpValue, useQpDeltaLimit);
    }
}

/*------------------------------------------------------------------------------
  PicQuantLimit()
------------------------------------------------------------------------------*/
void PicQuantLimit(vp8RateControl_s * rc)
{
    rc->qpHdr = MIN(rc->qpMax, MAX(rc->qpMin, rc->qpHdr));
}

/*------------------------------------------------------------------------------
  Calculate()  I try to avoid overflow and calculate good enough result of a*b/c
------------------------------------------------------------------------------*/
i32 VP8Calculate(i32 a, i32 b, i32 c)
{
    u32 left = 32;
    u32 right = 0;
    u32 shift;
    i32 sign = 1;
    i32 tmp;

    if(a == 0 || b == 0)
    {
        return 0;
    }
    else if((a * b / b) == a && c != 0)
    {
        return (a * b / c);
    }
    if(a < 0)
    {
        sign = -1;
        a = -a;
    }
    if(b < 0)
    {
        sign *= -1;
        b = -b;
    }
    if(c < 0)
    {
        sign *= -1;
        c = -c;
    }

    if(c == 0 )
    {
        return 0x7FFFFFFF * sign;
    }

    if(b > a)
    {
        tmp = b;
        b = a;
        a = tmp;
    }

    for(--left; (((u32)a << left) >> left) != (u32)a; --left);
    left--; /* unsigned values have one more bit on left,
               we want signed accuracy. shifting signed values gives
               lint warnings */

    while(((u32)b >> right) > (u32)c)
    {
        right++;
    }

    if(right > left)
    {
        return 0x7FFFFFFF * sign;
    }
    else
    {
        shift = left - right;
        return (i32)((((u32)a << shift) / (u32)c * (u32)b) >> shift) * sign;
    }
}

/*------------------------------------------------------------------------------
  avg_rc_error()  PI(D)-control for rate prediction error.
------------------------------------------------------------------------------*/
static i32 avg_rc_error(linReg_s *p)
{
    return DIV(p->bits[2] * 4 + p->bits[1] * 6 + p->bits[0] * 0, 100);
}

/*------------------------------------------------------------------------------
  update_rc_error()  Update PI(D)-control values
------------------------------------------------------------------------------*/
static void update_rc_error(linReg_s *p, i32 bits)
{
    p->len = 3;

    if (bits == (i32)RC_ERROR_RESET) {
        /* RESET */
        p->bits[0] = 0;
        p->bits[1] = 0;
        p->bits[2] = 0;
        return;
    }
    p->bits[0] = bits - p->bits[2]; /* Derivative */
    p->bits[1] = bits + p->bits[1]; /* Integral */
    p->bits[2] = bits;              /* Proportional */

    DBG(2, (DBGOUTPUT, "P\t\t%7d  I\t\t%7d  D\t\t%7d\n",
                p->bits[2],  p->bits[1], p->bits[0]));
}

/*------------------------------------------------------------------------------
  gop_avg_qp()  Average quantization parameter of P frames of the previous GOP.
------------------------------------------------------------------------------*/
i32 gop_avg_qp(vp8RateControl_s *rc)
{
    i32 tmp = 0;

    if (rc->gopQpSum) {
        tmp = DIV(rc->gopQpSum, rc->gopQpDiv);
    }
    rc->gopQpSum = 0;
    rc->gopQpDiv = 0;

    return tmp;
}

/*------------------------------------------------------------------------------
  new_pic_quant()  Calculate new quantization parameter from the 2nd degree R-Q
  equation. Further adjust Qp for "smoother" visual quality.
------------------------------------------------------------------------------*/
static i32 new_pic_quant(linReg_s *p, i32 bits, true_e useQpDeltaLimit)
{
    i32 tmp, qp_best = p->qp_prev, qp = p->qp_prev, diff;
    i32 diff_prev = 0, qp_prev = 0, diff_best = 0x7FFFFFFF;

    DBG(2, (DBGOUTPUT, "R/cx:\t\t%7d ",bits));

    if (p->a1 == 0 && p->a2 == 0) {
        DBG(2, (DBGOUTPUT, "  QP:\t\t  xx xx  D:\t\t   ====  newQP: %2d\n", qp));
        return qp;
    }

    /* Target bits is negative => increase QP by maximum allowed */
    if (bits <= 0) {
        if (useQpDeltaLimit)
            qp = MIN(QINDEX_RANGE-1, MAX(0, qp + QP_DELTA));
        else
            qp = MIN(QINDEX_RANGE-1, MAX(0, qp + 10));

        DBG(2, (DBGOUTPUT, "  QP:\t\t  xx xx  D:\t\t   ----  newQP: %2d\n", qp));
        return qp;
    }

    /* Find the qp that has the best match on fitted curve */
    do {
        tmp  = DIV(p->a1, AcQLookup[qp]);
        tmp += DIV(p->a2, AcQLookup[qp] * AcQLookup[qp]);
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
                if (qp >= QINDEX_RANGE-1) {
                    break;
                }
                qp++;
            }
        } else {
            break;
        }
    } while ((qp >= 0) && (qp < QINDEX_RANGE));
    qp = qp_best;

    DBG(2,(DBGOUTPUT, "  QP:\t\t  %2d %2d  D:\t\t%7d", qp, qp_prev, diff_prev - diff_best));

    /* One unit change in Qp changes rate about 12% ca. 1/8. If the
     * difference is less than half use the one closer to previous value
    if (ABS(diff_prev - diff_best) <= ABS(bits) / 16) {
        qp = qp_prev;
    }*/
    DBG(2, (DBGOUTPUT, "  newQP: %2d\n", qp));

    /* Limit Qp change for smoother visual quality */
    if (useQpDeltaLimit) {
        tmp = qp - p->qp_prev;
        if (tmp > QP_DELTA) {
            qp = p->qp_prev + QP_DELTA;
        } else if (tmp < -QP_DELTA) {
            qp = p->qp_prev - QP_DELTA;
        }
    }

    return qp;
}

/*------------------------------------------------------------------------------
  update_tables()  only statistics of PSLICE, please.
------------------------------------------------------------------------------*/
static void update_tables(linReg_s *p, i32 qp, i32 bits)
{
    const i32 clen = 10;
    i32 tmp = p->pos;

    p->qp_prev   = qp;
    p->qs[tmp]   = AcQLookup[qp];
    p->bits[tmp] = bits;

    if (++p->pos >= clen) {
        p->pos = 0;
    }
    if (p->len < clen) {
        p->len++;
    }
}

/*------------------------------------------------------------------------------
            update_model()  Update model parameter by Linear Regression.
------------------------------------------------------------------------------*/
static void update_model(linReg_s *p)
{
    i32 *qs = p->qs, *r = p->bits, n = p->len;
    i32 i, a1, a2, sx = lin_sx(qs, n), sy = lin_sy(qs, r, n);

    for (i = 0; i < n; i++) {
        DBG(2, (DBGOUTPUT, "model: qs: %i  r: %i\n",qs[i], r[i]));
    }

    a1 = lin_sxy(qs, r, n);
    a1 = a1 < I32_MAX / n ? a1 * n : I32_MAX;

    DBG(2, (DBGOUTPUT, "model: sy: %i  sx: %i\n", sy, sx));
    if (sy == 0) {
        a1 = 0;
    } else {
        a1 -= (sx < I32_MAX/sy) ? sx * sy : I32_MAX;
    }

    a2 = (lin_nsxx(qs, n) - (sx * sx));
    if (a2 == 0) {
        if (p->a1 == 0) {
            /* If encountered in the beginning */
            a1 = 0;
        } else {
            a1 = (p->a1 * 2) / 3;
        }
    } else {
        a1 = VP8Calculate(a1, DSCY, a2);
    }

    /* Value of a1 shouldn't be excessive (small) */
    a1 = MAX(a1, -4096*DSCY);
    a1 = MIN(a1,  4096*DSCY-1);

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
  lin_sy()  calculate value of Sy for n points.
------------------------------------------------------------------------------*/
static i32 lin_sy(i32 *qp, i32 *r, i32 n)
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
  lin_sx()  calculate value of Sx for n points.
------------------------------------------------------------------------------*/
static i32 lin_sx(i32 *qp, i32 n)
{
    i32 tmp = 0;

    while (n--) {
        ASSERT(qp[n]);
        tmp += qp[n];
    }
    return tmp;
}

/*------------------------------------------------------------------------------
  lin_sxy()  calculate value of Sxy for n points.
------------------------------------------------------------------------------*/
static i32 lin_sxy(i32 *qp, i32 *r, i32 n)
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
  lin_nsxx()  calculate value of n * Sxy for n points.
------------------------------------------------------------------------------*/
static i32 lin_nsxx(i32 *qp, i32 n)
{
    i32 tmp = 0, sum = 0, d = n ;

    while (n--) {
        tmp = qp[n];
        tmp *= tmp;
        sum += d * tmp;
    }
    return sum;
}
