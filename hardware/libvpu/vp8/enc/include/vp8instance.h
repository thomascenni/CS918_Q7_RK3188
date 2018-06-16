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
--  Abstract  :   Encoder instance
--
------------------------------------------------------------------------------*/

#ifndef __VP8_INSTANCE_H__
#define __VP8_INSTANCE_H__

/*------------------------------------------------------------------------------
    1. Include headers
------------------------------------------------------------------------------*/
#include "enccommon.h"
#include "encpreprocess.h"
#include "encasiccontroller.h"

#include "vp8seqparameterset.h"
#include "vp8picparameterset.h"
#include "vp8picturebuffer.h"
#include "vp8putbits.h"
#include "vp8ratecontrol.h"
#include "vp8quanttable.h"

#ifdef VIDEOSTAB_ENABLED
#include "vidstabcommon.h"
#endif

/*------------------------------------------------------------------------------
    2. External compiler flags
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
    3. Module defines
------------------------------------------------------------------------------*/

enum VP8EncStatus
{
    VP8ENCSTAT_INIT = 0xA1,
    VP8ENCSTAT_KEYFRAME,
    VP8ENCSTAT_START_FRAME,
    VP8ENCSTAT_ERROR
};

typedef struct {
    i32 quant[2];
    i32 zbin[2];
    i32 round[2];
    i32 dequant[2];
} qp;

typedef struct {
    /* Approximate bit cost of mode. IOW bits used when selected mode is
     * boolean encoded using appropriate tree and probabilities. Note that
     * this value is scale up with SCALE (=256) */
    i32 intra16ModeBitCost[4 + 1];
    i32 intra4ModeBitCost[14 + 1];
} mbs;

typedef struct
{
    u32 encStatus;
    u32 mbPerFrame;
    u32 mbPerRow;
    u32 mbPerCol;
    u32 frameCnt;
    u32 testId;
    u32 numRefBuffsLum;
    u32 numRefBuffsChr;
    u32 prevFrameLost;
    preProcess_s preProcess;
    vp8RateControl_s rateControl;
    picBuffer picBuffer;         /* Reference picture container */
    sps sps;                     /* Sequence parameter set */
    ppss ppss;                   /* Picture parameter set */
    vp8buffer buffer[4];         /* Stream buffer per partition */
    qp qpY1[QINDEX_RANGE];  /* Quant table for 1'st order luminance */
    qp qpY2[QINDEX_RANGE];  /* Quant table for 2'nd order luminance */
    qp qpCh[QINDEX_RANGE];  /* Quant table for chrominance */
    mbs mbs;
    asicData_s asic;
    u32 *pOutBuf;                   /* User given stream output buffer */
    const void *inst;               /* Pointer to this instance for checking */
#ifdef VIDEOSTAB_ENABLED
    HWStabData vsHwData;
    SwStbData vsSwData;
#endif
    entropy_st entropy[1];
} vp8Instance_s;

#endif
