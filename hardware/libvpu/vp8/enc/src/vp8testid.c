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
--  Abstract : Encoder setup according to a test vector
--
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    1. Include headers
------------------------------------------------------------------------------*/
#include "vp8testid.h"

#include <stdio.h>
#include <stdlib.h>

/*------------------------------------------------------------------------------
    2. External compiler flags
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
    3. Module defines
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    4. Local function prototypes
------------------------------------------------------------------------------*/
static void Vp8FrameQuantizationTest(vp8Instance_s *inst);
static void Vp8StreamBufferLimitTest(vp8Instance_s *inst);
static void Vp8FilterTest(vp8Instance_s *inst);
static void Vp8FilterDeltaTest(vp8Instance_s *inst);
static void Vp8FilterDeltaDisable(vp8Instance_s *inst);
static void Vp8SegmentTest(vp8Instance_s *inst);
static void Vp8SegmentMapTest(vp8Instance_s *inst);
static void Vp8RefFrameTest(vp8Instance_s *inst);
static void Vp8Intra16FavorTest(vp8Instance_s *inst);
static void Vp8InterFavorTest(vp8Instance_s *inst);
static void Vp8CroppingTest(vp8Instance_s *inst);
static void Vp8RgbInputMaskTest(vp8Instance_s *inst);
static void Vp8MvTest(vp8Instance_s *inst);
static void Vp8DMVPenaltyTest(vp8Instance_s *inst);
static void Vp8MaxOverfillMv(vp8Instance_s *inst);
static void Vp8RoiTest(vp8Instance_s *inst);
static void Vp8IntraAreaTest(vp8Instance_s *inst);
static void Vp8CirTest(vp8Instance_s *inst);

/*------------------------------------------------------------------------------

    TestID defines a test configuration for the encoder. If the encoder control
    software is compiled with INTERNAL_TEST flag the test ID will force the 
    encoder operation according to the test vector. 

    TestID  Description
    0       No action, normal encoder operation
    1       Frame quantization test, adjust qp for every frame, qp = 0..127
    4       Stream buffer limit test, test limit on all partitions
    5       Filter delta disable
    6       Filter delta test, set filter delta values
    7       Filter test, set disableDeblocking and filterOffsets A and B
    8       Segment test, set segment map and update, segment qps and filters
    9       Reference frame test, all combinations of reference and refresh.
    10      Segment map test
    15      Intra16Favor test, set to maximum value
    16      Cropping test, set cropping values for every frame
    19      RGB input mask test, set all values
    21      InterFavor test, set to maximum value
    22      MV test, set cropping offsets so that max MVs are tested
    23      DMV penalty test, set to minimum/maximum values
    24      Max overfill MV
    26      ROI test
    27      Intra area test
    28      CIR test

------------------------------------------------------------------------------*/


/*------------------------------------------------------------------------------

    Vp8ConfigureTestBeforeFrame

    Function configures the encoder instance before starting frame encoding

------------------------------------------------------------------------------*/
void Vp8ConfigureTestBeforeFrame(vp8Instance_s * inst)
{
    ASSERT(inst);

    switch(inst->testId)
    {
        case 1:
            Vp8FrameQuantizationTest(inst);
            break;
        case 4:
            Vp8StreamBufferLimitTest(inst);
            break;
        case 5:
            Vp8FilterDeltaDisable(inst);
            break;
        case 6:
            Vp8FilterDeltaTest(inst);
            break;
        case 7:
            Vp8FilterTest(inst);
            break;
        case 8:
            Vp8SegmentTest(inst);
            break;
        case 9:
            Vp8RefFrameTest(inst);
            break;
        case 10:
            Vp8SegmentMapTest(inst);
            break;
        case 15:
            Vp8Intra16FavorTest(inst);
            break;
        case 16:
            Vp8CroppingTest(inst);
            break;
        case 19:
            Vp8RgbInputMaskTest(inst);
            break;
        case 21:
            Vp8InterFavorTest(inst);
            break;
        case 22:
            Vp8MvTest(inst);
            break;
        case 23:
            Vp8DMVPenaltyTest(inst);
            break;
        case 24:
            Vp8MaxOverfillMv(inst);
            break;
        case 26:
            Vp8RoiTest(inst);
            break;
        case 27:
            Vp8IntraAreaTest(inst);
            break;
        case 28:
            Vp8CirTest(inst);
            break;
        default:
            break;
    }

}

/*------------------------------------------------------------------------------
  Vp8QuantizationTest
------------------------------------------------------------------------------*/
void Vp8FrameQuantizationTest(vp8Instance_s *inst)
{
    i32 vopNum = inst->frameCnt;
    pps *pps = inst->ppss.pps;

    /* Inter frame qp start zero */
    pps->qpSgm[0] = inst->rateControl.qpHdr = MIN(127, MAX(0, (vopNum-1)%128));

    ALOGV("Vp8FrameQuantTest# qpHdr %d\n", inst->rateControl.qpHdr);
}

/*------------------------------------------------------------------------------
  Vp8StreamBufferLimitTest
------------------------------------------------------------------------------*/
void Vp8StreamBufferLimitTest(vp8Instance_s *inst)
{
    /* inst->frameCnt doesn't increase for discarded frames. */
    static u32 frameCnt = 0;

    /* Limits customized for case 3801 so that first frame will overflow
     * only first partition, second frame only second partition etc. */
    inst->asic.regs.outputStrmSize = 80;
    if (frameCnt >= 1) inst->asic.regs.outputStrmSize += 1500;
    if (frameCnt >= 3) inst->asic.regs.outputStrmSize = frameCnt * 1000;
    inst->preProcess.verOffsetSrc = (frameCnt / 2)*16;
    if (VP8_EncPreProcessCheck(&inst->preProcess) == ENCHW_NOK)
        inst->preProcess.verOffsetSrc = 0;
    frameCnt++;

    ALOGV("Vp8StreamBufferLimitTest# streamBufferLimit %d bytes, Y-offset %d\n",
            inst->asic.regs.outputStrmSize, inst->preProcess.verOffsetSrc);
}

/*------------------------------------------------------------------------------
  Vp8FilterTest
------------------------------------------------------------------------------*/
void Vp8FilterTest(vp8Instance_s *inst)
{
    i32 vopNum = inst->frameCnt;
    sps *sps = &inst->sps;

    sps->filterType = (vopNum/2)%2;
    sps->filterLevel = (vopNum/2)%64;
    sps->filterSharpness = vopNum%8;

    ALOGV("Vp8FilterTest# filterType = %d filterLevel = %i filterSharpnesss = %i\n",
             sps->filterType, sps->filterLevel, sps->filterSharpness);

}

/*------------------------------------------------------------------------------
  Vp8FilterDeltaTest
------------------------------------------------------------------------------*/
void Vp8FilterDeltaTest(vp8Instance_s *inst)
{
    i32 frame = inst->frameCnt;
    sps *sps = &inst->sps;
    i32 i;

    sps->filterDeltaEnable = 2; /* Special meaning, test ID set deltas */

    if (frame&1)
        frame = -frame;

    for (i = 0; i < 4; i++) {
        sps->refDelta[i] = CLIP3((frame+i)%64, -0x3f, 0x3f);
        sps->modeDelta[i] = CLIP3((2*frame+i)%64, -0x3f, 0x3f);
    }

    ALOGV("Vp8FilterDeltaTest# refDelta = %d,%d,%d,%d  modeDelta = %d,%d,%d,%d\n",
        sps->refDelta[0], sps->refDelta[1],
        sps->refDelta[2], sps->refDelta[3],
        sps->modeDelta[0], sps->modeDelta[1],
        sps->modeDelta[2], sps->modeDelta[3]);

}

/*------------------------------------------------------------------------------
  Vp8FilterDeltaDisable
------------------------------------------------------------------------------*/
void Vp8FilterDeltaDisable(vp8Instance_s *inst)
{
    sps *sps = &inst->sps;

    sps->filterDeltaEnable = 0;

    ALOGV("Vp8FilterDeltaDisable#\n");
}

/*------------------------------------------------------------------------------
  Vp8SegmentTest
------------------------------------------------------------------------------*/
void Vp8SegmentTest(vp8Instance_s *inst)
{
    u32 frame = (u32)inst->frameCnt;
    u32 *map = inst->asic.segmentMap.vir_addr;
    sps *sps = &inst->sps;
    pps *pps = inst->ppss.pps;
    u32 mbPerFrame = (inst->mbPerFrame+7)/8*8;  /* Rounded upwards to 8 */
    i32 i, j;
    u32 mask;

    pps->segmentEnabled = 1;
    pps->sgm.mapModified = ((frame%2) == 0);
    if (pps->sgm.mapModified) {
        if (frame < 2) {
            for (i = 0; i < mbPerFrame/8/4; i++) {
                map[i*4+0] = 0x00000000;
                map[i*4+1] = 0x11111111;
                map[i*4+2] = 0x22222222;
                map[i*4+3] = 0x33333333;
            }
        } else {
            for (i = 0; i < mbPerFrame/8; i++) {
                mask = 0;
                for (j = 0; j < 8; j++)
                    mask |= ((j + (frame-2)/2 + j*frame/3)%4) << (28-j*4);
                map[i] = mask;
            }
        }
		VPUMemClean(&inst->asic.segmentMap);
    }

    /* Disable auto setting so that it won't overwrite this. */
    sps->autoFilterLevel = 0;
    for (i = 0; i < 4; i++) {
        pps->qpSgm[i] = (64 + i + frame/2 + frame*i)%128;
        pps->levelSgm[i] = (32 + i + frame/2 + frame*i)%64;
    }
    inst->rateControl.qpHdr = pps->qpSgm[0];
    sps->filterLevel = pps->levelSgm[0];

    ALOGV("Vp8SegmentTest# enable=%d update=%d map=0x%08x%08x%08x%08x\n",
             pps->segmentEnabled, pps->sgm.mapModified,
             map[0], map[1], map[2], map[3]);
    ALOGV("Vp8SegmentTest# qp=%d,%d,%d,%d level=%d,%d,%d,%d\n",
            pps->qpSgm[0], pps->qpSgm[1], pps->qpSgm[2], pps->qpSgm[3],
            pps->levelSgm[0], pps->levelSgm[1],
            pps->levelSgm[2], pps->levelSgm[3]);
}

/*------------------------------------------------------------------------------
  Vp8SegmentMapTest
------------------------------------------------------------------------------*/
void Vp8SegmentMapTest(vp8Instance_s *inst)
{
    u32 frame = (u32)inst->frameCnt;
    u32 *map = inst->asic.segmentMap.vir_addr;
    sps *sps = &inst->sps;
    pps *pps = inst->ppss.pps;
    u32 mbPerFrame = (inst->mbPerFrame+7)/8*8;  /* Rounded upwards to 8 */
    i32 i, j;
    u32 mask;

    pps->segmentEnabled = 1;
    pps->sgm.mapModified = 1;
    for (i = 0; i < mbPerFrame/8; i++) {
        mask = 0;
        for (j = 0; j < 8; j++)
            mask |= ((j + frame)%4) << (28-j*4);
        map[i] = mask;
    }

    sps->autoFilterLevel = 1;
    for (i = 0; i < 4; i++) {
        pps->qpSgm[i] = (64 + i*frame)%128;
    }
    inst->rateControl.qpHdr = pps->qpSgm[0];

    ALOGV("Vp8SegmentMapTest# enable=%d update=%d map=0x%08x%08x%08x%08x\n",
             pps->segmentEnabled, pps->sgm.mapModified,
             map[0], map[1], map[2], map[3]);
    ALOGV("Vp8SegmentMapTest# qp=%d,%d,%d,%d level=%d,%d,%d,%d\n",
            pps->qpSgm[0], pps->qpSgm[1], pps->qpSgm[2], pps->qpSgm[3],
            pps->levelSgm[0], pps->levelSgm[1],
            pps->levelSgm[2], pps->levelSgm[3]);
}

/*------------------------------------------------------------------------------
  Vp8RefFrameTest
------------------------------------------------------------------------------*/
void Vp8RefFrameTest(vp8Instance_s *inst)
{
    i32 pic = inst->frameCnt;
    picBuffer *picBuffer = &inst->picBuffer;

    /* Only adjust for p-frames */
    if (picBuffer->cur_pic->p_frame) {
        picBuffer->cur_pic->ipf = pic & 0x1 ? 1 : 0;
        picBuffer->cur_pic->grf = pic & 0x2 ? 1 : 0;
        picBuffer->cur_pic->arf = pic & 0x4 ? 1 : 0;
        picBuffer->refPicList[0].search = pic & 0x8 ? 1 : 0;
        picBuffer->refPicList[1].search = pic & 0x10 ? 1 : 0;
        picBuffer->refPicList[2].search = pic & 0x20 ? 1 : 0;
    }

    ALOGV("Vp8RefFrameTest#\n");
}

/*------------------------------------------------------------------------------
  Vp8Intra16FavorTest
------------------------------------------------------------------------------*/
void Vp8Intra16FavorTest(vp8Instance_s *inst)
{
    /* Force intra16 favor to maximum value */
    inst->asic.regs.intra16Favor = 65535;

    ALOGV("Vp8Intra16FavorTest# intra16Favor %d\n",
            inst->asic.regs.intra16Favor);
}

/*------------------------------------------------------------------------------
  Vp8InterFavorTest
------------------------------------------------------------------------------*/
void Vp8InterFavorTest(vp8Instance_s *inst)
{
    /* Force combinations of inter favor and skip penalty values */

    if ((inst->frameCnt % 3) == 0) {
        inst->asic.regs.skipPenalty = ASIC_PENALTY_UNDEFINED;   /* default */
        inst->asic.regs.interFavor = 0x7FFF;
    } else if ((inst->frameCnt % 3) == 1) {
        inst->asic.regs.skipPenalty = 0;
        inst->asic.regs.interFavor = 0x7FFF;
    } else {
        inst->asic.regs.skipPenalty = 0;
        inst->asic.regs.interFavor = ASIC_PENALTY_UNDEFINED;    /* default */
    }

    ALOGV("Vp8InterFavorTest# interFavor %d skipPenalty %d\n",
            inst->asic.regs.interFavor, inst->asic.regs.skipPenalty);
}

/*------------------------------------------------------------------------------
  Vp8CroppingTest
------------------------------------------------------------------------------*/
void Vp8CroppingTest(vp8Instance_s *inst)
{
    inst->preProcess.horOffsetSrc = inst->frameCnt % 8;
    if (VP8_EncPreProcessCheck(&inst->preProcess) == ENCHW_NOK)
        inst->preProcess.horOffsetSrc = 0;
    inst->preProcess.verOffsetSrc = inst->frameCnt / 2;
    if (VP8_EncPreProcessCheck(&inst->preProcess) == ENCHW_NOK)
        inst->preProcess.verOffsetSrc = 0;

    ALOGV("Vp8CroppingTest# horOffsetSrc %d  verOffsetSrc %d\n",
            inst->preProcess.horOffsetSrc, inst->preProcess.verOffsetSrc);
}

/*------------------------------------------------------------------------------
  Vp8RgbInputMaskTest
------------------------------------------------------------------------------*/
void Vp8RgbInputMaskTest(vp8Instance_s *inst)
{
    u32 frameNum = (u32)inst->frameCnt;
    static u32 rMsb = 0;
    static u32 gMsb = 0;
    static u32 bMsb = 0;
    static u32 lsMask = 0;  /* Lowest possible mask position */
    static u32 msMask = 0;  /* Highest possible mask position */

    /* First frame normal
     * 1..29 step rMaskMsb values
     * 30..58 step gMaskMsb values
     * 59..87 step bMaskMsb values */
    if (frameNum == 0) {
        rMsb = inst->asic.regs.rMaskMsb;
        gMsb = inst->asic.regs.gMaskMsb;
        bMsb = inst->asic.regs.bMaskMsb;
        lsMask = MIN(rMsb, gMsb);
        lsMask = MIN(bMsb, lsMask);
        msMask = MAX(rMsb, gMsb);
        msMask = MAX(bMsb, msMask);
        if (msMask < 16)
            msMask = 15-2;    /* 16bit RGB, 13 mask positions: 3..15  */
        else
            msMask = 31-2;    /* 32bit RGB, 29 mask positions: 3..31 */
    } else if (frameNum <= msMask) {
        inst->asic.regs.rMaskMsb = MAX(frameNum+2, lsMask);
        inst->asic.regs.gMaskMsb = gMsb;
        inst->asic.regs.bMaskMsb = bMsb;
    } else if (frameNum <= msMask*2) {
        inst->asic.regs.rMaskMsb = rMsb;
        inst->asic.regs.gMaskMsb = MAX(frameNum-msMask+2, lsMask);
        if (inst->asic.regs.inputImageFormat == 4)  /* RGB 565 special case */
            inst->asic.regs.gMaskMsb = MAX(frameNum-msMask+2, lsMask+1);
        inst->asic.regs.bMaskMsb = bMsb;
    } else if (frameNum <= msMask*3) {
        inst->asic.regs.rMaskMsb = rMsb;
        inst->asic.regs.gMaskMsb = gMsb;
        inst->asic.regs.bMaskMsb = MAX(frameNum-msMask*2+2, lsMask);
    } else {
        inst->asic.regs.rMaskMsb = rMsb;
        inst->asic.regs.gMaskMsb = gMsb;
        inst->asic.regs.bMaskMsb = bMsb;
    }

    ALOGV("Vp8RgbInputMaskTest#  %d %d %d\n", inst->asic.regs.rMaskMsb,
            inst->asic.regs.gMaskMsb, inst->asic.regs.bMaskMsb);
}

/*------------------------------------------------------------------------------
  Vp8MvTest
------------------------------------------------------------------------------*/
void Vp8MvTest(vp8Instance_s *inst)
{
    u32 frame = (u32)inst->frameCnt;

    /* Set cropping offsets according to max MV length, decrement by frame
     * x = 32, 160, 32, 159, 32, 158, ..
     * y = 48, 80, 48, 79, 48, 78, .. */
    inst->preProcess.horOffsetSrc = 32 + (frame%2)*128 - (frame%2)*(frame/2);
    if (VP8_EncPreProcessCheck(&inst->preProcess) == ENCHW_NOK)
        inst->preProcess.horOffsetSrc = 0;
    inst->preProcess.verOffsetSrc = 48 + (frame%2)*32 - (frame%2)*(frame/2);
    if (VP8_EncPreProcessCheck(&inst->preProcess) == ENCHW_NOK)
        inst->preProcess.verOffsetSrc = 0;

    ALOGV("Vp8MvTest# horOffsetSrc %d  verOffsetSrc %d\n",
            inst->preProcess.horOffsetSrc, inst->preProcess.verOffsetSrc);
}

/*------------------------------------------------------------------------------
  Vp8DMVPenaltyTest
------------------------------------------------------------------------------*/
void Vp8DMVPenaltyTest(vp8Instance_s *inst)
{
    u32 frame = (u32)inst->frameCnt;

    /* Set DMV penalty values to maximum and minimum */
    inst->asic.regs.diffMvPenalty[0] = frame%2 ? 0x3FF-frame/2 : frame/2;
    inst->asic.regs.diffMvPenalty[1] = frame%2 ? 0x3FF-frame/2 : frame/2;
    inst->asic.regs.diffMvPenalty[2] = frame%2 ? 0x3FF-frame/2 : frame/2;

    ALOGV("Vp8DMVPenaltyTest# penalty4p %d  penalty1p %d  penaltyQp %d\n",
            inst->asic.regs.diffMvPenalty[0],
            inst->asic.regs.diffMvPenalty[1],
            inst->asic.regs.diffMvPenalty[2]);
}

/*------------------------------------------------------------------------------
  Vp8MaxOverfillMv
------------------------------------------------------------------------------*/
void Vp8MaxOverfillMv(vp8Instance_s *inst)
{
    u32 frame = (u32)inst->frameCnt;

    /* Set cropping offsets according to max MV length.
     * In test cases the picture is selected so that this will
     * cause maximum horizontal MV to point into overfilled area. */
    inst->preProcess.horOffsetSrc = 32 + (frame%2)*128;
    if (VP8_EncPreProcessCheck(&inst->preProcess) == ENCHW_NOK)
        inst->preProcess.horOffsetSrc = 0;

    inst->preProcess.verOffsetSrc = 176;
    if (VP8_EncPreProcessCheck(&inst->preProcess) == ENCHW_NOK)
        inst->preProcess.verOffsetSrc = 0;

    ALOGV("Vp8MaxOverfillMv# horOffsetSrc %d  verOffsetSrc %d\n",
            inst->preProcess.horOffsetSrc, inst->preProcess.verOffsetSrc);
}

/*------------------------------------------------------------------------------
  Vp8RoiTest
------------------------------------------------------------------------------*/
void Vp8RoiTest(vp8Instance_s *inst)
{
    regValues_s *regs = &inst->asic.regs;
    u32 frame = (u32)inst->frameCnt;
    u32 mbPerRow = inst->mbPerRow;
    u32 mbPerCol = inst->mbPerCol;
    u32 frames = MIN(mbPerRow-1, mbPerCol-1);
    u32 loop = frames*3;

    /* Loop after this many encoded frames */
    frame = frame % loop;

    regs->roi1DeltaQp = (frame % 50) + 1;
    regs->roi2DeltaQp = 50 - (frame % 50);

    /* Set two ROI areas according to frame dimensions. */
    if (frame < frames)
    {
        /* ROI1 in top-left corner, ROI2 in bottom-right corner */
        regs->roi1Left = regs->roi1Top = 0;
        regs->roi1Right = regs->roi1Bottom = frame;
        regs->roi2Left = mbPerRow - 1 - frame;
        regs->roi2Top = mbPerCol - 1 - frame;
        regs->roi2Right = mbPerRow - 1;
        regs->roi2Bottom = mbPerCol - 1;
    }
    else if (frame < frames*2)
    {
        /* ROI1 gets smaller towards top-right corner,
         * ROI2 towards bottom-left corner */
        frame -= frames;
        regs->roi1Left = frame+1;
        regs->roi1Top = 0;
        regs->roi1Right = mbPerRow - 1;
        regs->roi1Bottom = mbPerCol - 2 - frame;
        regs->roi2Left = 0;
        regs->roi2Top = frame+1;
        regs->roi2Right = mbPerRow - 2 - frame;
        regs->roi2Bottom = mbPerCol - 1;
    }
    else if (frame < frames*3)
    {
        /* 1x1/2x2 ROIs moving diagonal across frame */
        frame -= frames*2;
        regs->roi1Left = frame - frame%2;
        regs->roi1Right = frame;
        regs->roi1Top = frame - frame%2;
        regs->roi1Bottom = frame;
        regs->roi2Left = frame - frame%2;
        regs->roi2Right = frame;
        regs->roi2Top = mbPerCol - 1 - frame;
        regs->roi2Bottom = mbPerCol - 1 - frame + frame%2;
    }

    /* When ROI is updated we must inform that segment map has been modified. */
    inst->ppss.pps->sgm.mapModified = 1;

    ALOGV("Vp8RoiTest# ROI1:%d x%dy%d-x%dy%d  ROI2:%d x%dy%d-x%dy%d\n",
            regs->roi1DeltaQp, regs->roi1Left, regs->roi1Top,
            regs->roi1Right, regs->roi1Bottom,
            regs->roi2DeltaQp, regs->roi2Left, regs->roi2Top,
            regs->roi2Right, regs->roi2Bottom);
}

/*------------------------------------------------------------------------------
  Vp8IntraAreaTest
------------------------------------------------------------------------------*/
void Vp8IntraAreaTest(vp8Instance_s *inst)
{
    regValues_s *regs = &inst->asic.regs;
    u32 frame = (u32)inst->frameCnt;
    u32 mbPerRow = inst->mbPerRow;
    u32 mbPerCol = inst->mbPerCol;
    u32 frames = MIN(mbPerRow, mbPerCol);
    u32 loop = frames*3;

    /* Loop after this many encoded frames */
    frame = frame % loop;

    if (frame < frames)
    {
        /* Intra area in top-left corner, gets bigger every frame */
        regs->intraAreaLeft = regs->intraAreaTop = 0;
        regs->intraAreaRight = regs->intraAreaBottom = frame;
    }
    else if (frame < frames*2)
    {
        /* Intra area gets smaller towards top-right corner */
        frame -= frames;
        regs->intraAreaLeft = frame;
        regs->intraAreaTop = 0;
        regs->intraAreaRight = mbPerRow - 1;
        regs->intraAreaBottom = mbPerCol - 1 - frame;
    }
    else if (frame < frames*3)
    {
        /* 1x1/2x2 Intra area moving diagonal across frame */
        frame -= frames*2;
        regs->intraAreaLeft = frame - frame%2;
        regs->intraAreaRight = frame;
        regs->intraAreaTop = frame - frame%2;
        regs->intraAreaBottom = frame;
    }

    ALOGV("Vp8IntraAreaTest# x%dy%d-x%dy%d\n",
            regs->intraAreaLeft, regs->intraAreaTop,
            regs->intraAreaRight, regs->intraAreaBottom);
}

/*------------------------------------------------------------------------------
  Vp8CirTest
------------------------------------------------------------------------------*/
void Vp8CirTest(vp8Instance_s *inst)
{
    regValues_s *regs = &inst->asic.regs;
    u32 frame = (u32)inst->frameCnt;
    u32 mbPerRow = inst->mbPerRow;
    u32 mbPerFrame = inst->mbPerFrame;
    u32 loop = inst->mbPerFrame+6;

    /* Loop after this many encoded frames */
    frame = frame % loop;

    switch (frame)
    {
        case 0:
        case 1:
            regs->cirStart = 0;
            regs->cirInterval = 1;
            break;
        case 2:
            regs->cirStart = 0;
            regs->cirInterval = 2;
            break;
        case 3:
            regs->cirStart = 0;
            regs->cirInterval = 3;
            break;
        case 4:
            regs->cirStart = 0;
            regs->cirInterval = mbPerRow;
            break;
        case 5:
            regs->cirStart = 0;
            regs->cirInterval = mbPerRow+1;
            break;
        case 6:
            regs->cirStart = 0;
            regs->cirInterval = mbPerFrame-1;
            break;
        case 7:
            regs->cirStart = mbPerFrame-1;
            regs->cirInterval = 1;
            break;
        default:
            regs->cirStart = frame-7;
            regs->cirInterval = (mbPerFrame-frame)%(mbPerRow*2);
            break;
    }

    ALOGV("Vp8CirTest# start:%d interval:%d\n",
            regs->cirStart, regs->cirInterval);
}


