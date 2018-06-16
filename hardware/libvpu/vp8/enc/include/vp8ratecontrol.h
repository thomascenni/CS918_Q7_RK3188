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
--  Description : Rate control structures and function prototypes
--
------------------------------------------------------------------------------*/

#ifndef VP8_RATE_CONTROL_H
#define VP8_RATE_CONTROL_H

#include "enccommon.h"

enum
{ VP8RC_OVERFLOW = -1 };

typedef struct {
    i32  a1;               /* model parameter */
    i32  a2;               /* model parameter */
    i32  qp_prev;          /* previous QP */
    i32  qs[15];           /* quantization step size */
    i32  bits[15];         /* Number of bits needed to code residual */
    i32  pos;              /* current position */
    i32  len;              /* current lenght */
    i32  zero_div;         /* a1 divisor is 0 */
} linReg_s;

/* Virtual buffer */
typedef struct
{
    i32 bufferSize;          /* size of the virtual buffer */
    i32 bitRate;             /* input bit rate per second */
    i32 bitPerPic;           /* average number of bits per picture */
    i32 picTimeInc;          /* timeInc since last coded picture */
    i32 timeScale;           /* input frame rate numerator */
    i32 unitsInTic;          /* input frame rate denominator */
    i32 virtualBitCnt;       /* virtual (channel) bit count */
    i32 realBitCnt;          /* real bit count */
    i32 bufferOccupancy;     /* number of bits in the buffer */
    i32 skipFrameTarget;     /* how many frames should be skipped in a row */
    i32 skippedFrames;       /* how many frames have been skipped in a row */
    i32 bucketFullness;      /* Leaky Bucket fullness */
    i32 gopRem;              /* Number of frames remaining in this GOP */
} vp8VirtualBuffer_s;

typedef struct
{
    true_e picRc;
    true_e picSkip;          /* Frame Skip enable */
    true_e frameCoded;       /* Frame coded or not */
    i32 mbPerPic;            /* Number of macroblock per picture */
    i32 mbRows;              /* MB rows in picture */
    i32 currFrameIntra;      /* Is current frame intra frame? */
    i32 prevFrameIntra;      /* Was previous frame intra frame? */
    i32 fixedQp;             /* Pic header qp when fixed */
    i32 qpHdr;               /* Pic header qp of current voded picture */
    i32 qpMin;               /* Pic header minimum qp, user set */
    i32 qpMax;               /* Pic header maximum qp, user set */
    i32 qpHdrPrev;           /* Pic header qp of previous coded picture */
    i32 outRateNum;
    i32 outRateDenom;
    vp8VirtualBuffer_s virtualBuffer;
   /* for frame QP rate control */
    linReg_s linReg;       /* Data for R-Q model */
    linReg_s rError;       /* Rate prediction error (bits) */
    i32 targetPicSize;
    i32 frameBitCnt;
   /* for GOP rate control */
    i32 gopQpSum;
    i32 gopQpDiv;
    i32 frameCnt;
    i32 gopLen;
    i32 intraQpDelta;
    i32 fixedIntraQp;
    i32 mbQpAdjustment;     /* QP delta for MAD macroblock QP adjustment */
    i32 intraPictureRate;
    i32 goldenPictureRate;
    i32 altrefPictureRate;
} vp8RateControl_s;

/*------------------------------------------------------------------------------
    Function prototypes
------------------------------------------------------------------------------*/
void VP8InitRc(vp8RateControl_s * rc, u32 newStream);
void VP8BeforePicRc(vp8RateControl_s * rc, u32 timeInc, u32 frameTypeIntra);
void VP8AfterPicRc(vp8RateControl_s * rc, u32 byteCnt);
i32 VP8Calculate(i32 a, i32 b, i32 c);
#endif /* VP8_RATE_CONTROL_H */

