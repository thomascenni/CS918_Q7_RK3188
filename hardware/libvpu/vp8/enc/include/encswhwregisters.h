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
--  Description :  Encoder SW/HW interface register definitions
--
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------

    Table of contents

    1. Include headers
    2. External compiler flags
    3. Module defines

------------------------------------------------------------------------------*/
#ifndef ENC_SWHWREGISTERS_H
#define ENC_SWHWREGISTERS_H

/*------------------------------------------------------------------------------
    1. Include headers
------------------------------------------------------------------------------*/

#include "basetype.h"

/*------------------------------------------------------------------------------
    2. External compiler flags
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
    3. Module defines
------------------------------------------------------------------------------*/

#define ASIC_SWREG_AMOUNT               (0x28C/4+1)

#define ASIC_INPUT_YUV420PLANAR         0x00
#define ASIC_INPUT_YUV420SEMIPLANAR     0x01
#define ASIC_INPUT_YUYV422INTERLEAVED   0x02
#define ASIC_INPUT_UYVY422INTERLEAVED   0x03
#define ASIC_INPUT_RGB565               0x04
#define ASIC_INPUT_RGB555               0x05
#define ASIC_INPUT_RGB444               0x06
#define ASIC_INPUT_RGB888               0x07
#define ASIC_INPUT_RGB101010            0x08

/* Bytes of external memory for VP8 counters for probability updates,
 * 252 counters for dct coeff probs, 1 for skipped, 1 for intra type and
 * 2 * 11 for mv probs, each counter 2 bytes */
#define ASIC_VP8_PROB_COUNT_SIZE            244*2
#define ASIC_VP8_PROB_COUNT_MODE_OFFSET     220
#define ASIC_VP8_PROB_COUNT_MV_OFFSET       222

/* HW Register field names */
typedef enum {
    HEncProductID,
    HEncProductMajor,
    HEncProductMinor,
    HEncProductBuild,

    HEncIRQSliceReady,
    HEncIRQTimeout,
    HEncIRQBuffer,
    HEncIRQReset,
    HEncIRQBusError,
    HEncIRQFrameReady,
    HEncIRQDisable,
    HEncIRQ,

    HEncAXIWriteID,
    HEncAXIReadID,
    HEncOutputSwap16,
    HEncInputSwap16,
    HEncBurstLength,
    HEncBurstDisable,
    HEncBurstIncr,
    HEncDataDiscard,
    HEncClockGating,
    HEncOutputSwap32,
    HEncInputSwap32,
    HEncOutputSwap8,
    HEncInputSwap8,

    HEncTestCounter,
    HEncTestLength,
    HEncTestMem,
    HEncTestReg,
    HEncTestIrq,

    HEncBaseStream,
    HEncBaseControl,
    HEncBaseRefLum,
    HEncBaseRefChr,
    HEncBaseRecLum,
    HEncBaseRecChr,
    HEncBaseInLum,
    HEncBaseInCb,
    HEncBaseInCr,

    HEncIntTimeout,
    HEncMvWrite,
    HEncNalSizeWrite,
    HEncIntSliceReady,
    HEncWidth,
    HEncHeight,
    HEncRecWriteDisable,
    HEncPictureType,
    HEncEncodingMode,
    HEncEnable,

    HEncChrOffset,
    HEncLumOffset,
    HEncRowLength,
    HEncXFill,
    HEncYFill,
    HEncInputFormat,
    HEncInputRot,

    HEncBaseRefLum2,
    HEncBaseRefChr2,
    HEncPicInitQp,
    HEncSliceAlpha,
    HEncSliceBeta,
    HEncChromaQp,
    HEncIdrPicId,
    HEncConstrIP,
    HEncPPSID,
    HEncIPPrevModeFavor,
    HEncFrameNum,

    HEncDeblocking,
    HEncSliceSize,
    HEncDisableQPMV,
    HEncTransform8x8,
    HEncCabacInitIdc,
    HEncCabacEnable,
    HEncInter4Restrict,
    HEncStreamMode,
    HEncIPIntra16Favor,

    HEncSplitMv,
    HEncDMVPenalty1p,
    HEncDMVPenalty4p,
    HEncDMVPenaltyQp,

    HEncJpegMode,
    HEncJpegSlice,
    HEncJpegRSTInt,
    HEncJpegRST,
    HEncSplitPenalty16x8,
    HEncSplitPenalty8x8,
    HEncSplitPenalty8x4,

    HEncSkipPenalty,
    HEncNumSlicesReady,
    HEncInterFavor,

    HEncStrmHdrRem1,
    HEncStrmHdrRem2,

    HEncStrmBufLimit,

    HEncMadQpDelta,
    HEncMadThreshold,
    HEncQpSum,

    HEncQp,
    HEncMaxQp,
    HEncMinQp,
    HEncCPDist,

    HEncCP1WordTarget,
    HEncCP2WordTarget,
    HEncCP3WordTarget,
    HEncCP4WordTarget,
    HEncCP5WordTarget,
    HEncCP6WordTarget,
    HEncCP7WordTarget,
    HEncCP8WordTarget,
    HEncCP9WordTarget,
    HEncCP10WordTarget,

    HEncCPWordError1,
    HEncCPWordError2,
    HEncCPWordError3,
    HEncCPWordError4,
    HEncCPWordError5,
    HEncCPWordError6,

    HEncCPDeltaQp1,
    HEncCPDeltaQp2,
    HEncCPDeltaQp3,
    HEncCPDeltaQp4,
    HEncCPDeltaQp5,
    HEncCPDeltaQp6,
    HEncCPDeltaQp7,

    HEncVp8Y1QuantDc,
    HEncVp8Y1ZbinDc,
    HEncVp8Y1RoundDc,
    HEncVp8Y1QuantAc,
    HEncVp8Y1ZbinAc,
    HEncVp8Y1RoundAc,
    HEncVp8Y2QuantDc,
    HEncVp8Y2ZbinDc,
    HEncVp8Y2RoundDc,
    HEncVp8Y2QuantAc,
    HEncVp8Y2ZbinAc,
    HEncVp8Y2RoundAc,
    HEncVp8ChQuantDc,
    HEncVp8ChZbinDc,
    HEncVp8ChRoundDc,
    HEncVp8ChQuantAc,
    HEncVp8ChZbinAc,
    HEncVp8ChRoundAc,

    HEncVp8Y1DequantDc,
    HEncVp8Y1DequantAc,
    HEncVp8Y2DequantDc,
    HEncVp8MvRefIdx,
    HEncVp8Y2DequantAc,
    HEncVp8ChDequantDc,
    HEncVp8ChDequantAc,
    HEncVp8MvRefIdx2,
    HEncVp8Ref2Enable,
    HEncVp8SegmentEnable,
    HEncVp8SegmentMapUpdate,

    HEncVp8BoolEncValue,
    HEncVp8GoldenPenalty,
    HEncVp8FilterSharpness,
    HEncVp8FilterLevel,
    HEncVp8DctPartitionCount,
    HEncVp8BoolEncValueBits,
    HEncVp8BoolEncRange,

    HEncStartOffset,
    HEncRlcSum,
    HEncMadCount,
    HEncMbCount,

    HEncBaseNextLum,

    HEncStabMode,
    HEncStabMinimum,
    HEncStabMotionSum,
    HEncStabGmvX,
    HEncStabMatrix1,
    HEncStabGmvY,
    HEncStabMatrix2,
    HEncStabMatrix3,
    HEncStabMatrix4,
    HEncStabMatrix5,
    HEncStabMatrix6,
    HEncStabMatrix7,
    HEncStabMatrix8,
    HEncStabMatrix9,

    HEncBaseCabacCtx,
    HEncBaseMvWrite,

    HEncRGBCoeffA,
    HEncRGBCoeffB,
    HEncRGBCoeffC,
    HEncRGBCoeffE,
    HEncRGBCoeffF,

    HEncRMaskMSB,
    HEncGMaskMSB,
    HEncBMaskMSB,

    HEncIntraAreaLeft,
    HEncIntraAreaRight,
    HEncIntraAreaTop,
    HEncIntraAreaBottom,

    HEncCirStart,
    HEncCirInterval,

    HEncIntraSliceMap1,
    HEncIntraSliceMap2,
    HEncIntraSliceMap3,
    HEncBasePartition1,
    HEncBasePartitirk,
    HEncBaseVp8ProbCount,

    HEncRoi1Left,
    HEncRoi1Right,
    HEncRoi1Top,
    HEncRoi1Bottom,

    HEncRoi2Left,
    HEncRoi2Right,
    HEncRoi2Top,
    HEncRoi2Bottom,

    HEncRoi1DeltaQp,
    HEncRoi2DeltaQp,
    HEncZeroMvFavor,
    HEncSplitPenalty4x4,

    HEncMvcPriorityId,
    HEncMvcViewId,
    HEncMvcTemporalId,
    HEncMvcAnchorPicFlag,
    HEncMvcInterViewFlag,

    HEncHWTiledSupport,
    HEncHWSearchArea,
    HEncHWRgbSupport,
    HEncHWH264Support,
    HEncHWVp8Support,
    HEncHWJpegSupport,
    HEncHWStabSupport,
    HEncHWBus,
    HEncHWSynthesisLan,
    HEncHWBusWidth,
    HEncHWMaxVideoWidth,

    HEncJpegQuantLuma1,
    HEncJpegQuantLuma2,
    HEncJpegQuantLuma3,
    HEncJpegQuantLuma4,
    HEncJpegQuantLuma5,
    HEncJpegQuantLuma6,
    HEncJpegQuantLuma7,
    HEncJpegQuantLuma8,
    HEncJpegQuantLuma9,
    HEncJpegQuantLuma10,
    HEncJpegQuantLuma11,
    HEncJpegQuantLuma12,
    HEncJpegQuantLuma13,
    HEncJpegQuantLuma14,
    HEncJpegQuantLuma15,
    HEncJpegQuantLuma16,

    HEncJpegQuantChroma1,
    HEncJpegQuantChroma2,
    HEncJpegQuantChroma3,
    HEncJpegQuantChroma4,
    HEncJpegQuantChroma5,
    HEncJpegQuantChroma6,
    HEncJpegQuantChroma7,
    HEncJpegQuantChroma8,
    HEncJpegQuantChroma9,
    HEncJpegQuantChroma10,
    HEncJpegQuantChroma11,
    HEncJpegQuantChroma12,
    HEncJpegQuantChroma13,
    HEncJpegQuantChroma14,
    HEncJpegQuantChroma15,
    HEncJpegQuantChroma16,

    /* VP8 penalty registers */
    HEncVp8Mode0Penalty,
    HEncVp8Mode1Penalty,
    HEncVp8Mode2Penalty,
    HEncVp8Mode3Penalty,
    HEncVp8Bmode0Penalty,
    HEncVp8Bmode1Penalty,
    HEncVp8Bmode2Penalty,
    HEncVp8Bmode3Penalty,
    HEncVp8Bmode4Penalty,
    HEncVp8Bmode5Penalty,
    HEncVp8Bmode6Penalty,
    HEncVp8Bmode7Penalty,
    HEncVp8Bmode8Penalty,
    HEncVp8Bmode9Penalty,
    HEncBaseVp8SegmentMap,
    HEncVp8Seg1Y1QuantDc,
    HEncVp8Seg1Y1ZbinDc,
    HEncVp8Seg1Y1RoundDc,
    HEncVp8Seg1Y1QuantAc,
    HEncVp8Seg1Y1ZbinAc,
    HEncVp8Seg1Y1RoundAc,
    HEncVp8Seg1Y2QuantDc,
    HEncVp8Seg1Y2ZbinDc,
    HEncVp8Seg1Y2RoundDc,
    HEncVp8Seg1Y2QuantAc,
    HEncVp8Seg1Y2ZbinAc,
    HEncVp8Seg1Y2RoundAc,
    HEncVp8Seg1ChQuantDc,
    HEncVp8Seg1ChZbinDc,
    HEncVp8Seg1ChRoundDc,
    HEncVp8Seg1ChQuantAc,
    HEncVp8Seg1ChZbinAc,
    HEncVp8Seg1ChRoundAc,
    HEncVp8Seg1Y1DequantDc,
    HEncVp8Seg1Y1DequantAc,
    HEncVp8Seg1Y2DequantDc,
    HEncVp8Seg1Y2DequantAc,
    HEncVp8Seg1ChDequantDc,
    HEncVp8Seg1ChDequantAc,
    HEncVp8Seg1FilterLevel,
    HEncVp8Seg2Y1QuantDc,
    HEncVp8Seg2Y1ZbinDc,
    HEncVp8Seg2Y1RoundDc,
    HEncVp8Seg2Y1QuantAc,
    HEncVp8Seg2Y1ZbinAc,
    HEncVp8Seg2Y1RoundAc,
    HEncVp8Seg2Y2QuantDc,
    HEncVp8Seg2Y2ZbinDc,
    HEncVp8Seg2Y2RoundDc,
    HEncVp8Seg2Y2QuantAc,
    HEncVp8Seg2Y2ZbinAc,
    HEncVp8Seg2Y2RoundAc,
    HEncVp8Seg2ChQuantDc,
    HEncVp8Seg2ChZbinDc,
    HEncVp8Seg2ChRoundDc,
    HEncVp8Seg2ChQuantAc,
    HEncVp8Seg2ChZbinAc,
    HEncVp8Seg2ChRoundAc,
    HEncVp8Seg2Y1DequantDc,
    HEncVp8Seg2Y1DequantAc,
    HEncVp8Seg2Y2DequantDc,
    HEncVp8Seg2Y2DequantAc,
    HEncVp8Seg2ChDequantDc,
    HEncVp8Seg2ChDequantAc,
    HEncVp8Seg2FilterLevel,
    HEncVp8Seg3Y1QuantDc,
    HEncVp8Seg3Y1ZbinDc,
    HEncVp8Seg3Y1RoundDc,
    HEncVp8Seg3Y1QuantAc,
    HEncVp8Seg3Y1ZbinAc,
    HEncVp8Seg3Y1RoundAc,
    HEncVp8Seg3Y2QuantDc,
    HEncVp8Seg3Y2ZbinDc,
    HEncVp8Seg3Y2RoundDc,
    HEncVp8Seg3Y2QuantAc,
    HEncVp8Seg3Y2ZbinAc,
    HEncVp8Seg3Y2RoundAc,
    HEncVp8Seg3ChQuantDc,
    HEncVp8Seg3ChZbinDc,
    HEncVp8Seg3ChRoundDc,
    HEncVp8Seg3ChQuantAc,
    HEncVp8Seg3ChZbinAc,
    HEncVp8Seg3ChRoundAc,
    HEncVp8Seg3Y1DequantDc,
    HEncVp8Seg3Y1DequantAc,
    HEncVp8Seg3Y2DequantDc,
    HEncVp8Seg3Y2DequantAc,
    HEncVp8Seg3ChDequantDc,
    HEncVp8Seg3ChDequantAc,
    HEncVp8Seg3FilterLevel,

    HEncDmvPenalty1,
    HEncDmvPenalty2,
    HEncDmvPenalty3,
    HEncDmvPenalty4,
    HEncDmvPenalty5,
    HEncDmvPenalty6,
    HEncDmvPenalty7,
    HEncDmvPenalty8,
    HEncDmvPenalty9,
    HEncDmvPenalty10,
    HEncDmvPenalty11,
    HEncDmvPenalty12,
    HEncDmvPenalty13,
    HEncDmvPenalty14,
    HEncDmvPenalty15,
    HEncDmvPenalty16,
    HEncDmvPenalty17,
    HEncDmvPenalty18,
    HEncDmvPenalty19,
    HEncDmvPenalty20,
    HEncDmvPenalty21,
    HEncDmvPenalty22,
    HEncDmvPenalty23,
    HEncDmvPenalty24,
    HEncDmvPenalty25,
    HEncDmvPenalty26,
    HEncDmvPenalty27,
    HEncDmvPenalty28,
    HEncDmvPenalty29,
    HEncDmvPenalty30,
    HEncDmvPenalty31,
    HEncDmvPenalty32,

    HEncDmvQpelPenalty1,
    HEncDmvQpelPenalty2,
    HEncDmvQpelPenalty3,
    HEncDmvQpelPenalty4,
    HEncDmvQpelPenalty5,
    HEncDmvQpelPenalty6,
    HEncDmvQpelPenalty7,
    HEncDmvQpelPenalty8,
    HEncDmvQpelPenalty9,
    HEncDmvQpelPenalty10,
    HEncDmvQpelPenalty11,
    HEncDmvQpelPenalty12,
    HEncDmvQpelPenalty13,
    HEncDmvQpelPenalty14,
    HEncDmvQpelPenalty15,
    HEncDmvQpelPenalty16,
    HEncDmvQpelPenalty17,
    HEncDmvQpelPenalty18,
    HEncDmvQpelPenalty19,
    HEncDmvQpelPenalty20,
    HEncDmvQpelPenalty21,
    HEncDmvQpelPenalty22,
    HEncDmvQpelPenalty23,
    HEncDmvQpelPenalty24,
    HEncDmvQpelPenalty25,
    HEncDmvQpelPenalty26,
    HEncDmvQpelPenalty27,
    HEncDmvQpelPenalty28,
    HEncDmvQpelPenalty29,
    HEncDmvQpelPenalty30,
    HEncDmvQpelPenalty31,
    HEncDmvQpelPenalty32,

    HEncVp8CostInter,
    HEncVp8DmvCostConst,
    HEncVp8CostGoldenRef,

    /* VP8 loop filter deltas */
    HEncVp8LfRefDelta0,
    HEncVp8LfRefDelta1,
    HEncVp8LfRefDelta2,
    HEncVp8LfRefDelta3,
    HEncVp8LfModeDelta0,
    HEncVp8LfModeDelta1,
    HEncVp8LfModeDelta2,
    HEncVp8LfModeDelta3,

    HEncRegisterAmount

} regName;

/* HW Register field descriptions */
typedef struct {
    i32 name;               /* Register name and index  */
    i32 base;               /* Register base address  */
    u32 mask;               /* Bitmask for this field */
    i32 lsb;                /* LSB for this field [31..0] */
    i32 trace;              /* Enable/disable writing in swreg_params.trc */
    char *description;      /* Field description */
} regField_s;

#endif
