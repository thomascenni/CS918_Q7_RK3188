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
--  Description : ASIC low level controller
--
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    Include headers
------------------------------------------------------------------------------*/

#include "enccommon.h"
#include "encasiccontroller.h"
#include "encpreprocess.h"
#include "ewl.h"
#include "encswhwregisters.h"

/*------------------------------------------------------------------------------
    External compiler flags
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    Module defines
------------------------------------------------------------------------------*/

#ifdef ASIC_WAVE_TRACE_TRIGGER
extern i32 trigger_point;    /* picture which will be traced */
#endif

/* Mask fields */
#define mask_2b         (u32)0x00000003
#define mask_3b         (u32)0x00000007
#define mask_4b         (u32)0x0000000F
#define mask_5b         (u32)0x0000001F
#define mask_6b         (u32)0x0000003F
#define mask_7b         (u32)0x0000007F
#define mask_11b        (u32)0x000007FF
#define mask_14b        (u32)0x00003FFF
#define mask_16b        (u32)0x0000FFFF

#define HSWREG(n)       ((n)*4)

/* JPEG QUANT table order */
static const u32 qpReorderTable[64] =
    { 0,  8, 16, 24,  1,  9, 17, 25, 32, 40, 48, 56, 33, 41, 49, 57,
      2, 10, 18, 26,  3, 11, 19, 27, 34, 42, 50, 58, 35, 43, 51, 59,
      4, 12, 20, 28,  5, 13, 21, 29, 36, 44, 52, 60, 37, 45, 53, 61,
      6, 14, 22, 30,  7, 15, 23, 31, 38, 46, 54, 62, 39, 47, 55, 63
};

/* NOTE: Don't use ',' in descriptions, because it is used as separator in csv
 * parsing. */
static const regField_s asicRegisterDesc[] = {
/* HW ID register, read-only */
    {HEncProductID        , 0x000, 0xffff0000, 16, 0, "Product ID"},
    {HEncProductMajor     , 0x000, 0x0000f000, 12, 0, "Major number"},
    {HEncProductMinor     , 0x000, 0x00000ff0,  4, 0, "Minor number"},
    {HEncProductBuild     , 0x000, 0x0000000f,  0, 0, "Build number defined in synthesis."},
/* Encoder interrupt register */
    {HEncIRQSliceReady    , 0x004, 0x00000100,  8, 0, "IRQ slice ready status bit."},
    {HEncIRQTimeout       , 0x004, 0x00000040,  6, 0, "IRQ HW timeout status bit."},
    {HEncIRQBuffer        , 0x004, 0x00000020,  5, 1, "IRQ buffer full status bit. bufferFullInterrupt"},
    {HEncIRQReset         , 0x004, 0x00000010,  4, 0, "IRQ SW reset status bit."},
    {HEncIRQBusError      , 0x004, 0x00000008,  3, 0, "IRQ bus error status bit."},
    {HEncIRQFrameReady    , 0x004, 0x00000004,  2, 0, "IRQ frame ready status bit. Encoder has finished a frame."},
    {HEncIRQDisable       , 0x004, 0x00000002,  1, 0, "IRQ disable. No interrupts from HW. SW must use polling."},
    {HEncIRQ              , 0x004, 0x00000001,  0, 0, "HINTenc Interrupt from HW. SW resets at IRQ handler."},
/* Encoder configuration register */
    {HEncAXIWriteID       , 0x008, 0xff000000, 24, 0, "AXI Write ID"},
    {HEncAXIReadID        , 0x008, 0x00ff0000, 16, 0, "AXI Read ID"},
    {HEncOutputSwap16     , 0x008, 0x00008000, 15, 0, "Enable output swap 16-bits"},
    {HEncInputSwap16      , 0x008, 0x00004000, 14, 0, "Enable input swap 16-bits"},
    {HEncBurstLength      , 0x008, 0x00003f00,  8, 0, "Burst length. 0=incremental. 4=max BURST4.8=max BURST8. 16=max BURST16"},
    {HEncBurstDisable     , 0x008, 0x00000080,  7, 0, "Disable burst mode for AXI"},
    {HEncBurstIncr        , 0x008, 0x00000040,  6, 0, "Burst incremental. 1=INCR burst allowed. 0=use SINGLE burst"},
    {HEncDataDiscard      , 0x008, 0x00000020,  5, 0, "Enable burst data discard. 2 or 3 long reads are using BURST4"},
    {HEncClockGating      , 0x008, 0x00000010,  4, 0, "Enable clock gating"},
    {HEncOutputSwap32     , 0x008, 0x00000008,  3, 0, "Enable output swap 32-bits"},
    {HEncInputSwap32      , 0x008, 0x00000004,  2, 0, "Enable input swap 32-bits"},
    {HEncOutputSwap8      , 0x008, 0x00000002,  1, 0, "Enable output swap 8-bits"},
    {HEncInputSwap8       , 0x008, 0x00000001,  0, 0, "Enable input swap 8-bits"},
    {HEncTestCounter      , 0x00c, 0xf0000000, 28, 0, "Test counter"},
    {HEncTestLength       , 0x00c, 0x001ffff8,  3, 0, "Test data length for memory test"},
    {HEncTestMem          , 0x00c, 0x00000004,  2, 0, "Enable memory coherency test. Reads BaseStream. Writes BaseControl"},
    {HEncTestReg          , 0x00c, 0x00000002,  1, 0, "Enable register coherency test. Increments test counter"},
    {HEncTestIrq          , 0x00c, 0x00000001,  0, 0, "Enable IRQ test. HW gives interrupt"},
/* External memory base addresses for encoder input/output */
    {HEncBaseStream       , 0x014, 0xffffffff,  0, 0, "Base address for output stream data"},
    {HEncBaseControl      , 0x018, 0xffffffff,  0, 0, "Base address for output control data"},
    {HEncBaseRefLum       , 0x01c, 0xffffffff,  0, 0, "Base address for reference luma"},
    {HEncBaseRefChr       , 0x020, 0xffffffff,  0, 0, "Base address for reference chroma"},
    {HEncBaseRecLum       , 0x024, 0xffffffff,  0, 0, "Base address for reconstructed luma"},
    {HEncBaseRecChr       , 0x028, 0xffffffff,  0, 0, "Base address for reconstructed chroma"},
    {HEncBaseInLum        , 0x02c, 0xffffffff,  0, 0, "Base address for input picture luma"},
    {HEncBaseInCb         , 0x030, 0xffffffff,  0, 0, "Base address for input picture cb"},
    {HEncBaseInCr         , 0x034, 0xffffffff,  0, 0, "Base address for input picture cr"},
    {HEncIntTimeout       , 0x038, 0x80000000, 31, 0, "Enable interrupt for timeout"},
    {HEncMvWrite          , 0x038, 0x40000000, 30, 1, "Enable writing MV and SAD of each MB to BaseMvWrite"},
    {HEncNalSizeWrite     , 0x038, 0x20000000, 29, 1, "Enable writing size of each NAL unit to BaseControl, nalSizeWriteOut"},
    {HEncIntSliceReady    , 0x038, 0x10000000, 28, 0, "Enable interrupt for slice ready"},
    {HEncWidth            , 0x038, 0x0ff80000, 19, 1, "Encoded width. lumWidth (macroblocks) H264:[9..255] JPEG:[6..511]"},
    {HEncHeight           , 0x038, 0x0007fc00, 10, 1, "Encoded height. lumHeight (macroblocks) H264:[6..255] JPEG:[2..511]"},
    {HEncRecWriteDisable  , 0x038, 0x00000040,  6, 1, "Disable writing of reconstructed image. recWriteDisable"},
    {HEncPictureType      , 0x038, 0x00000018,  3, 1, "Encoded picture type. frameType. 0=INTER. 1=INTRA(IDR). 2=MVC-INTER. 3=MVC-INTER(ref mod)."},
    {HEncEncodingMode     , 0x038, 0x00000006,  1, 1, "Encoding mode. streamType. 1=VP8. 2=JPEG. 3=H264"},
    {HEncEnable           , 0x038, 0x00000001,  0, 0, "Encoder enable"},
    {HEncChrOffset        , 0x03c, 0xe0000000, 29, 0, "Input chrominance offset (bytes) [0..7]"},
    {HEncLumOffset        , 0x03c, 0x1c000000, 26, 0, "Input luminance offset (bytes) [0..7]"},
    {HEncRowLength        , 0x03c, 0x03fff000, 12, 1, "Input luminance row length. lumWidthSrc (bytes) [96..8192]"},
    {HEncXFill            , 0x03c, 0x00000c00, 10, 0, "Overfill pixels on right edge of image div4 [0..3]"},
    {HEncYFill            , 0x03c, 0x000003c0,  6, 1, "Overfill pixels on bottom edge of image. YFill. [0..15]"},
    {HEncInputFormat      , 0x03c, 0x0000003c,  2, 1, "Input image format. inputFormat. YUV420P/YUV420SP/YUYV422/UYVY422/RGB565/RGB555/RGB444/RGB888/RGB101010"},
    {HEncInputRot         , 0x03c, 0x00000003,  0, 1, "Input image rotation. 0=disabled. 1=90 degrees right. 2=90 degrees left"},

/* VP8 / H.264 mixed definitions */
    {HEncBaseRefLum2      , 0x040, 0xffffffff,  0, 0, "Base address for second reference luma"},
    {HEncBaseRefChr2      , 0x044, 0xffffffff,  0, 0, "Base address for second reference chroma"},
    {HEncPicInitQp        , 0x040, 0xfc000000, 26, 0, "H.264 Pic init qp in PPS [0..51]"},
    {HEncSliceAlpha       , 0x040, 0x03c00000, 22, 0, "H.264 Slice filter alpha c0 offset div2 [-6..6]"},
    {HEncSliceBeta        , 0x040, 0x003c0000, 18, 0, "H.264 Slice filter beta offset div2 [-6..6]"},
    {HEncChromaQp         , 0x040, 0x0003e000, 13, 0, "H.264 Chroma qp index offset [-12..12]"},
    {HEncIdrPicId         , 0x040, 0x0000001e,  1, 0, "H.264 IDR picture ID"},
    {HEncConstrIP         , 0x040, 0x00000001,  0, 1, "H.264 Constrained intra prediction enable. constIntraPred"},
    {HEncPPSID            , 0x044, 0xff000000, 24, 0, "H.264 pic_parameter_set_id"},
    {HEncIPPrevModeFavor  , 0x044, 0x00ff0000, 16, 0, "H.264 Intra prediction previous 4x4 mode favor"},
    {HEncFrameNum         , 0x044, 0x0000ffff,  0, 0, "H.264 Frame num"},

    {HEncDeblocking       , 0x048, 0xc0000000, 30, 0, "Deblocking filter mode. 0=enabled. 1=disabled (vp8=simple). 2=disabled on slice borders"},
    {HEncSliceSize        , 0x048, 0x3f800000, 23, 1, "H.264 Slice size. mbRowPerSlice (mb rows) [0..127] 0=one slice per picture"},
    {HEncDisableQPMV      , 0x048, 0x00400000, 22, 1, "H.264 Disable quarter pixel MVs. disableQuarterPixelMv"},
    {HEncTransform8x8     , 0x048, 0x00200000, 21, 1, "H.264 Transform 8x8 enable. High Profile H.264. transform8x8Mode"},
    {HEncCabacInitIdc     , 0x048, 0x00180000, 19, 0, "H.264 CABAC initial IDC. [0..2]"},
    {HEncCabacEnable      , 0x048, 0x00040000, 18, 1, "H.264 CABAC / VP8 boolenc enable. entropyCodingMode. 0=CAVLC (Baseline Profile H.264). 1=CABAC (Main Profile H.264)"},
    {HEncInter4Restrict   , 0x048, 0x00020000, 17, 1, "H.264 Inter 4x4 mode restriction. restricted4x4Mode"},
    {HEncStreamMode       , 0x048, 0x00010000, 16, 1, "H.264 Stream mode. byteStream. 0=NAL unit stream. 1=Byte stream"},
    {HEncIPIntra16Favor   , 0x048, 0x0000ffff,  0, 0, "Intra prediction intra 16x16 mode favor"},
    {HEncSplitMv          , 0x04c, 0x40000000, 30, 1, "Enable using more than 1 MV per macroblock."},
    {HEncDMVPenalty1p     , 0x04c, 0x000003ff,  0, 1, "Differential MV penalty for 1p ME. DMVPenalty1p"},
    {HEncDMVPenalty4p     , 0x04c, 0x000ffc00, 10, 1, "Differential MV penalty for 4p ME. DMVPenalty4p"},
    {HEncDMVPenaltyQp     , 0x04c, 0x3ff00000, 20, 1, "Differential MV penalty for 1/4p ME. DMVPenaltyQp"},

/* Mixed definitions JPEG / video */
    {HEncJpegMode         , 0x050, 0x02000000, 25, 0, "JPEG mode. 0=4:2:0 (4lum+2chr blocks/MCU). 1=4:2:2 (2lum+2chr blocks/MCU)"},
    {HEncJpegSlice        , 0x050, 0x01000000, 24, 0, "JPEG slice enable. 0=picture ends with EOI. 1=slice ends with RST"},
    {HEncJpegRSTInt       , 0x050, 0x00ff0000, 16, 0, "JPEG restart marker interval when slices are disabled (mb rows) [0..255]"},
    {HEncJpegRST          , 0x050, 0x0000ffff,  0, 0, "JPEG restart marker for first RST. incremented by HW for next RST"},
    {HEncSplitPenalty16x8 , 0x050, 0x3ff00000, 20, 0, "Penalty for using 16x8 or 8x16 MV."},
    {HEncSplitPenalty8x8  , 0x050, 0x000ffc00, 10, 0, "Penalty for using 8x8 MV."},
    {HEncSplitPenalty8x4  , 0x050, 0x000003ff,  0, 0, "Penalty for using 8x4 or 4x8 MV."},

    {HEncSkipPenalty      , 0x054, 0xff000000, 24, 0, "H.264 SKIP macroblock mode / VP8 zero/nearest/near mode penalty"},
    {HEncNumSlicesReady   , 0x054, 0x00ff0000, 16, 0, "H.264 amount of completed slices."},
    {HEncInterFavor       , 0x054, 0x0000ffff,  0, 0, "Inter MB mode favor in intra/inter selection"},
    {HEncStrmHdrRem1      , 0x058, 0xffffffff,  0, 0, "Stream header remainder bits MSB (MSB aligned)"},
    {HEncStrmHdrRem2      , 0x05c, 0xffffffff,  0, 0, "Stream header remainder bits LSB (MSB aligned)"},
    {HEncStrmBufLimit     , 0x060, 0xffffffff,  0, 1, "Stream buffer limit (64bit addresses) / output stream size (bits). HWStreamDataCount. If limit is reached buffer full IRQ is given."},
    {HEncMadQpDelta       , 0x064, 0xf0000000, 28, 1, "MAD based QP adjustment. madQpChange [-8..7]"},
    {HEncMadThreshold     , 0x064, 0x0fc00000, 22, 0, "MAD threshold div256"},
    {HEncQpSum            , 0x064, 0x001fffff,  0, 0, "QP Sum div2 output"},

/* H.264 Rate control registers */
    {HEncQp               , 0x06c, 0xfc000000, 26, 1, "H.264 Initial QP. qpLum [0..51]"},
    {HEncMaxQp            , 0x06c, 0x03f00000, 20, 1, "H.264 Maximum QP. qpMax [0..51]"},
    {HEncMinQp            , 0x06c, 0x000fc000, 14, 1, "H.264 Minimum QP. qpMin [0..51]"},
    {HEncCPDist           , 0x06c, 0x00001fff,  0, 0, "H.264 Checkpoint distance (mb) 0=disabled [0..8191]"},
    {HEncCP1WordTarget    , 0x070, 0xffff0000, 16, 0, "H.264 Checkpoint 1 word target/usage div32 [0..65535]"},
    {HEncCP2WordTarget    , 0x070, 0x0000ffff,  0, 0, "H.264 Checkpoint 2 word target/usage div32 [0..65535]"},
    {HEncCP3WordTarget    , 0x074, 0xffff0000, 16, 0, "H.264 Checkpoint 3 word target/usage div32 [0..65535]"},
    {HEncCP4WordTarget    , 0x074, 0x0000ffff,  0, 0, "H.264 Checkpoint 4 word target/usage div32 [0..65535]"},
    {HEncCP5WordTarget    , 0x078, 0xffff0000, 16, 0, "H.264 Checkpoint 5 word target/usage div32 [0..65535]"},
    {HEncCP6WordTarget    , 0x078, 0x0000ffff,  0, 0, "H.264 Checkpoint 6 word target/usage div32 [0..65535]"},
    {HEncCP7WordTarget    , 0x07c, 0xffff0000, 16, 0, "H.264 Checkpoint 7 word target/usage div32 [0..65535]"},
    {HEncCP8WordTarget    , 0x07c, 0x0000ffff,  0, 0, "H.264 Checkpoint 8 word target/usage div32 [0..65535]"},
    {HEncCP9WordTarget    , 0x080, 0xffff0000, 16, 0, "H.264 Checkpoint 9 word target/usage div32 [0..65535]"},
    {HEncCP10WordTarget   , 0x080, 0x0000ffff,  0, 0, "H.264 Checkpoint 10 word target/usage div32 [0..65535]"},
    {HEncCPWordError1     , 0x084, 0xffff0000, 16, 0, "H.264 Checkpoint word error 1 div4 [-32768..32767]"},
    {HEncCPWordError2     , 0x084, 0x0000ffff,  0, 0, "H.264 Checkpoint word error 2 div4 [-32768..32767]"},
    {HEncCPWordError3     , 0x088, 0xffff0000, 16, 0, "H.264 Checkpoint word error 3 div4 [-32768..32767]"},
    {HEncCPWordError4     , 0x088, 0x0000ffff,  0, 0, "H.264 Checkpoint word error 4 div4 [-32768..32767]"},
    {HEncCPWordError5     , 0x08c, 0xffff0000, 16, 0, "H.264 Checkpoint word error 5 div4 [-32768..32767]"},
    {HEncCPWordError6     , 0x08c, 0x0000ffff,  0, 0, "H.264 Checkpoint word error 6 div4 [-32768..32767]"},
    {HEncCPDeltaQp1       , 0x090, 0x0f000000, 24, 0, "H.264 Checkpoint delta QP 1 [-8..7]"},
    {HEncCPDeltaQp2       , 0x090, 0x00f00000, 20, 0, "H.264 Checkpoint delta QP 2 [-8..7]"},
    {HEncCPDeltaQp3       , 0x090, 0x000f0000, 16, 0, "H.264 Checkpoint delta QP 3 [-8..7]"},
    {HEncCPDeltaQp4       , 0x090, 0x0000f000, 12, 0, "H.264 Checkpoint delta QP 4 [-8..7]"},
    {HEncCPDeltaQp5       , 0x090, 0x00000f00,  8, 0, "H.264 Checkpoint delta QP 5 [-8..7]"},
    {HEncCPDeltaQp6       , 0x090, 0x000000f0,  4, 0, "H.264 Checkpoint delta QP 6 [-8..7]"},
    {HEncCPDeltaQp7       , 0x090, 0x0000000f,  0, 0, "H.264 Checkpoint delta QP 7 [-8..7]"},
/* VP8 Rate control registers, regs 0x6C - 0x90 redefined for VP8 */
    {HEncVp8Y1QuantDc        , 0x06C, 0x00003fff,  0, 1, "VP8 qpY1QuantDc 14b"},
    {HEncVp8Y1ZbinDc         , 0x06C, 0x007fc000, 14, 1, "VP8 qpY1ZbinDc 9b"},
    {HEncVp8Y1RoundDc        , 0x06C, 0x7f800000, 23, 1, "VP8 qpY1RoundDc 8b"},
    {HEncVp8Y1QuantAc        , 0x070, 0x00003fff,  0, 1, "VP8 qpY1QuantAc 14b"},
    {HEncVp8Y1ZbinAc         , 0x070, 0x007fc000, 14, 1, "VP8 qpY1ZbinAc 9b"},
    {HEncVp8Y1RoundAc        , 0x070, 0x7f800000, 23, 1, "VP8 qpY1RoundAc 8b"},
    {HEncVp8Y2QuantDc        , 0x074, 0x00003fff,  0, 1, "VP8 qpY2QuantDc 14b"},
    {HEncVp8Y2ZbinDc         , 0x074, 0x007fc000, 14, 1, "VP8 qpY2ZbinDc 9b"},
    {HEncVp8Y2RoundDc        , 0x074, 0x7f800000, 23, 1, "VP8 qpY2RoundDc 8b"},
    {HEncVp8Y2QuantAc        , 0x078, 0x00003fff,  0, 1, "VP8 qpY2QuantAc 14b"},
    {HEncVp8Y2ZbinAc         , 0x078, 0x007fc000, 14, 1, "VP8 qpY2ZbinAc 9b"},
    {HEncVp8Y2RoundAc        , 0x078, 0x7f800000, 23, 1, "VP8 qpY2RoundAc 8b"},
    {HEncVp8ChQuantDc        , 0x07C, 0x00003fff,  0, 1, "VP8 qpChQuantDc 14b"},
    {HEncVp8ChZbinDc         , 0x07C, 0x007fc000, 14, 1, "VP8 qpChZbinDc 9b"},
    {HEncVp8ChRoundDc        , 0x07C, 0x7f800000, 23, 1, "VP8 qpChRoundDc 8b"},
    {HEncVp8ChQuantAc        , 0x080, 0x00003fff,  0, 1, "VP8 qpChQuantAc 14b"},
    {HEncVp8ChZbinAc         , 0x080, 0x007fc000, 14, 1, "VP8 qpChZbinAc 9b"},
    {HEncVp8ChRoundAc        , 0x080, 0x7f800000, 23, 1, "VP8 qpChRoundAc 8b"},
    {HEncVp8Y1DequantDc      , 0x084, 0x000000ff,  0, 1, "VP8 qpY1DequantDc 8b"},
    {HEncVp8Y1DequantAc      , 0x084, 0x0001ff00,  8, 1, "VP8 qpY1DequantAc 9b"},
    {HEncVp8Y2DequantDc      , 0x084, 0x03fe0000, 17, 1, "VP8 qpY2DequantDc 9b"},
    {HEncVp8MvRefIdx         , 0x084, 0x0c000000, 26, 0, "VP8 mvRefIdx for first reference frame. 0=ipf. 1=grf. 2=arf."},
    {HEncVp8Y2DequantAc      , 0x088, 0x000001ff,  0, 1, "VP8 qpY2DequantAc 9b"},
    {HEncVp8ChDequantDc      , 0x088, 0x0001fe00,  9, 1, "VP8 qpChDequantDc 8b"},
    {HEncVp8ChDequantAc      , 0x088, 0x03fe0000, 17, 1, "VP8 qpChDequantAc 9b"},
    {HEncVp8MvRefIdx2        , 0x088, 0x0c000000, 26, 0, "VP8 mvRefIdx for second reference frame. 0=ipf. 1=grf. 2=arf."},
    {HEncVp8Ref2Enable       , 0x088, 0x10000000, 28, 0, "VP8 enable for second reference frame."},
    {HEncVp8SegmentEnable    , 0x088, 0x20000000, 29, 1, "VP8 enable for segmentation. Segmentation map is stored in BaseVp8SegmentMap."},
    {HEncVp8SegmentMapUpdate , 0x088, 0x40000000, 30, 0, "VP8 enable for segmentation map update. Map is different from previous frame and is written in stream. "},
    {HEncVp8BoolEncValue     , 0x08C, 0xffffffff,  0, 1, "VP8 boolEncValue"},
    {HEncVp8GoldenPenalty    , 0x090, 0xff000000, 24, 0, "VP8 Penalty value for second reference frame zero-mv [0..255]"},
    {HEncVp8FilterSharpness  , 0x090, 0x00e00000, 21, 0, "VP8 Deblocking filter sharpness [0..7]"},
    {HEncVp8FilterLevel      , 0x090, 0x001f8000, 15, 0, "VP8 Deblocking filter level [0..63]"},
    {HEncVp8DctPartitionCount, 0x090, 0x00006000, 13, 0, "VP8 DCT partition count. 0=1. 1=2 [0..1]"},
    {HEncVp8BoolEncValueBits , 0x090, 0x00001f00,  8, 1, "VP8 boolEncValueBitsMinus8 [0..23]"},
    {HEncVp8BoolEncRange     , 0x090, 0x000000ff,  0, 1, "VP8 boolEncRange [0..255]"},

    {HEncStartOffset      , 0x094, 0x1f800000, 23, 0, "Stream start offset = amount of StrmHdrRem (bits) [0..63]"},
    {HEncRlcSum           , 0x094, 0x007fffff,  0, 0, "RLC codeword count div4 output. max 255*255*384/4"},
    {HEncMadCount         , 0x098, 0xffff0000, 16, 0, "Macroblock count with MAD value under threshold output"},
    {HEncMbCount          , 0x098, 0x0000ffff,  0, 0, "MB count output. max 255*255"},
/* Stabilization parameters and outputs */
    {HEncBaseNextLum      , 0x09c, 0xffffffff,  0, 0, "Base address for next pic luminance"},
    {HEncStabMode         , 0x0a0, 0xc0000000, 30, 1, "Stabilization mode. 0=disabled. 1=stab only. 2=stab+encode"},
    {HEncStabMinimum      , 0x0a0, 0x00ffffff,  0, 0, "Stabilization minimum value output. max 253*253*255"},
    {HEncStabMotionSum    , 0x0a4, 0xffffffff,  0, 0, "Stabilization motion sum div8 output. max 253*253*255*1089/8"},
    {HEncStabGmvX         , 0x0a8, 0xfc000000, 26, 0, "Stabilization GMV horizontal output [-16..16]"},
    {HEncStabMatrix1      , 0x0a8, 0x00ffffff,  0, 0, "Stabilization matrix 1 (up-left position) output"},
    {HEncStabGmvY         , 0x0ac, 0xfc000000, 26, 0, "Stabilization GMV vertical output [-16..16]"},
    {HEncStabMatrix2      , 0x0ac, 0x00ffffff,  0, 0, "Stabilization matrix 2 (up position) output"},
    {HEncStabMatrix3      , 0x0b0, 0x00ffffff,  0, 0, "Stabilization matrix 3 (up-right position) output"},
    {HEncStabMatrix4      , 0x0b4, 0x00ffffff,  0, 0, "Stabilization matrix 4 (left position) output"},
    {HEncStabMatrix5      , 0x0b8, 0x00ffffff,  0, 0, "Stabilization matrix 5 (GMV position) output"},
    {HEncStabMatrix6      , 0x0bc, 0x00ffffff,  0, 0, "Stabilization matrix 6 (right position) output"},
    {HEncStabMatrix7      , 0x0c0, 0x00ffffff,  0, 0, "Stabilization matrix 7 (down-left position) output"},
    {HEncStabMatrix8      , 0x0c4, 0x00ffffff,  0, 0, "Stabilization matrix 8 (down position) output"},
    {HEncStabMatrix9      , 0x0c8, 0x00ffffff,  0, 0, "Stabilization matrix 9 (down-right position) output"},
    {HEncBaseCabacCtx     , 0x0cc, 0xffffffff,  0, 0, "Base address for cabac context tables (H264) or probability tables (VP8)"},
    {HEncBaseMvWrite      , 0x0d0, 0xffffffff,  0, 0, "Base address for MV output writing"},
/* Pre-processor color conversion parameters */
    {HEncRGBCoeffA        , 0x0d4, 0x0000ffff,  0, 0, "RGB to YUV conversion coefficient A"},
    {HEncRGBCoeffB        , 0x0d4, 0xffff0000, 16, 0, "RGB to YUV conversion coefficient B"},
    {HEncRGBCoeffC        , 0x0d8, 0x0000ffff,  0, 0, "RGB to YUV conversion coefficient C"},
    {HEncRGBCoeffE        , 0x0d8, 0xffff0000, 16, 0, "RGB to YUV conversion coefficient E"},
    {HEncRGBCoeffF        , 0x0dc, 0x0000ffff,  0, 0, "RGB to YUV conversion coefficient F"},
    {HEncRMaskMSB         , 0x0dc, 0x001f0000, 16, 0, "RGB R-component mask MSB bit position [0..31]"},
    {HEncGMaskMSB         , 0x0dc, 0x03e00000, 21, 0, "RGB G-component mask MSB bit position [0..31]"},
    {HEncBMaskMSB         , 0x0dc, 0x7c000000, 26, 0, "RGB B-component mask MSB bit position [0..31]"},
    {HEncIntraAreaLeft    , 0x0e0, 0xff000000, 24, 0, "Intra area left mb column (inside area) [0..255]"},
    {HEncIntraAreaRight   , 0x0e0, 0x00ff0000, 16, 0, "Intra area right mb column (outside area) [0..255]"},
    {HEncIntraAreaTop     , 0x0e0, 0x0000ff00,  8, 0, "Intra area top mb row (inside area) [0..255]"},
    {HEncIntraAreaBottom  , 0x0e0, 0x000000ff,  0, 0, "Intra area bottom mb row (outside area) [0..255]"},
    {HEncCirStart         , 0x0e4, 0xffff0000, 16, 0, "CIR first intra mb. 0=disabled [0..65535]"},
    {HEncCirInterval      , 0x0e4, 0x0000ffff,  0, 0, "CIR intra mb interval. 0=disabled [0..65535]"},

/* H264 / VP8 mixed definitions */
    {HEncIntraSliceMap1   , 0x0e8, 0xffffffff,  0, 0, "Intra slice bitmap for slices 0..31. LSB=slice0. MSB=slice31. 1=intra."},
    {HEncIntraSliceMap2   , 0x0ec, 0xffffffff,  0, 0, "Intra slice bitmap for slices 32..63. LSB=slice32. MSB=slice63. 1=intra."},
    {HEncIntraSliceMap3   , 0x068, 0xffffffff,  0, 0, "Intra slice bitmap for slices 64..95. LSB=slice64. MSB=slice95. 1=intra."},
    {HEncBasePartition1   , 0x0e8, 0xffffffff,  0, 0, "Base address for VP8 1st DCT partition"},
    {HEncBasePartitirk   , 0x0ec, 0xffffffff,  0, 0, "Base address for VP8 2nd DCT partition"},
    {HEncBaseVp8ProbCount , 0x068, 0xffffffff,  0, 0, "Base address for VP8 counters for probability updates"},

    {HEncRoi1Left         , 0x0f0, 0xff000000, 24, 0, "1st ROI area left mb column (inside area) qp+=Roi1DeltaQp"},
    {HEncRoi1Right        , 0x0f0, 0x00ff0000, 16, 0, "1st ROI area right mb column (outside area) qp-=Roi1DeltaQp"},
    {HEncRoi1Top          , 0x0f0, 0x0000ff00,  8, 0, "1st ROI area top mb row (inside area)"},
    {HEncRoi1Bottom       , 0x0f0, 0x000000ff,  0, 0, "1st ROI area bottom mb row (outside area)"},
    {HEncRoi2Left         , 0x0f4, 0xff000000, 24, 0, "2nd ROI area left mb column (inside area) qp+=Roi2DeltaQp"},
    {HEncRoi2Right        , 0x0f4, 0x00ff0000, 16, 0, "2nd ROI area right mb column (outside area) qp-=Roi2DeltaQp"},
    {HEncRoi2Top          , 0x0f4, 0x0000ff00,  8, 0, "2nd ROI area top mb row (inside area)"},
    {HEncRoi2Bottom       , 0x0f4, 0x000000ff,  0, 0, "2nd ROI area bottom mb row (outside area)"},
    {HEncRoi1DeltaQp      , 0x0f8, 0x000000f0,  4, 0, "1st ROI area delta QP. qp = Qp - Roi1DeltaQp [0..15]"},
    {HEncRoi2DeltaQp      , 0x0f8, 0x0000000f,  0, 0, "2nd ROI area delta QP. qp = Qp - Roi2DeltaQp [0..15]"},
    {HEncZeroMvFavor      , 0x0f8, 0xf0000000, 28, 0, "Zero 16x16 MV favor div2."},
    {HEncSplitPenalty4x4  , 0x0f8, 0x0ff80000, 19, 0, "Penalty for using 4x4 MV."},
    {HEncMvcPriorityId    , 0x0f8, 0x00070000, 16, 0, "MVC priority_id [0..7]"},
    {HEncMvcViewId        , 0x0f8, 0x0000e000, 13, 0, "MVC view_id [0..7]"},
    {HEncMvcTemporalId    , 0x0f8, 0x00001c00, 10, 0, "MVC temporal_id [0..7]"},
    {HEncMvcAnchorPicFlag , 0x0f8, 0x00000200,  9, 0, "MVC anchor_pic_flag. Specifies that the picture is part of an anchor access unit."},
    {HEncMvcInterViewFlag , 0x0f8, 0x00000100,  8, 0, "MVC inter_view_flag. Specifies that the picture is used for inter-view prediction."},

/* HW synthesis config register, read-only */
    {HEncHWTiledSupport   , 0x0fc, 0x40000000, 30, 0, "Tiled 4x4 input mode supported by HW. 0=not supported. 1=supported"},
    {HEncHWSearchArea     , 0x0fc, 0x20000000, 29, 0, "HW search area height. 0=5 MB rows. 1=3 MB rows"},
    {HEncHWRgbSupport     , 0x0fc, 0x10000000, 28, 0, "RGB to YUV conversion supported by HW. 0=not supported. 1=supported"},
    {HEncHWH264Support    , 0x0fc, 0x08000000, 27, 0, "H.264 encoding supported by HW. 0=not supported. 1=supported"},
    {HEncHWVp8Support     , 0x0fc, 0x04000000, 26, 0, "VP8 encoding supported by HW. 0=not supported. 1=supported"},
    {HEncHWJpegSupport    , 0x0fc, 0x02000000, 25, 0, "JPEG encoding supported by HW. 0=not supported. 1=supported"},
    {HEncHWStabSupport    , 0x0fc, 0x01000000, 24, 0, "Stabilization supported by HW. 0=not supported. 1=supported"},
    {HEncHWBus            , 0x0fc, 0x00f00000, 20, 0, "Bus connection of HW. 1=AHB. 2=OCP. 3=AXI. 4=PCI. 5=AXIAHB. 6=AXIAPB."},
    {HEncHWSynthesisLan   , 0x0fc, 0x000f0000, 16, 0, "Synthesis language. 1=vhdl. 2=verilog"},
    {HEncHWBusWidth       , 0x0fc, 0x0000f000, 12, 0, "Bus width of HW. 0=32b. 1=64b. 2=128b"},
    {HEncHWMaxVideoWidth  , 0x0fc, 0x00000fff,  0, 0, "Maximum video width supported by HW (pixels)"},

/* JPEG / VP8 mixed definitions regs 0x100-0x17C */
    {HEncJpegQuantLuma1   , 0x100, 0xffffffff,  0, 0, "JPEG luma quantization 1"},
    {HEncJpegQuantLuma2   , 0x104, 0xffffffff,  0, 0, "JPEG luma quantization 2"},
    {HEncJpegQuantLuma3   , 0x108, 0xffffffff,  0, 0, "JPEG luma quantization 3"},
    {HEncJpegQuantLuma4   , 0x10c, 0xffffffff,  0, 0, "JPEG luma quantization 4"},
    {HEncJpegQuantLuma5   , 0x110, 0xffffffff,  0, 0, "JPEG luma quantization 5"},
    {HEncJpegQuantLuma6   , 0x114, 0xffffffff,  0, 0, "JPEG luma quantization 6"},
    {HEncJpegQuantLuma7   , 0x118, 0xffffffff,  0, 0, "JPEG luma quantization 7"},
    {HEncJpegQuantLuma8   , 0x11c, 0xffffffff,  0, 0, "JPEG luma quantization 8"},
    {HEncJpegQuantLuma9   , 0x120, 0xffffffff,  0, 0, "JPEG luma quantization 9"},
    {HEncJpegQuantLuma10  , 0x124, 0xffffffff,  0, 0, "JPEG luma quantization 10"},
    {HEncJpegQuantLuma11  , 0x128, 0xffffffff,  0, 0, "JPEG luma quantization 11"},
    {HEncJpegQuantLuma12  , 0x12c, 0xffffffff,  0, 0, "JPEG luma quantization 12"},
    {HEncJpegQuantLuma13  , 0x130, 0xffffffff,  0, 0, "JPEG luma quantization 13"},
    {HEncJpegQuantLuma14  , 0x134, 0xffffffff,  0, 0, "JPEG luma quantization 14"},
    {HEncJpegQuantLuma15  , 0x138, 0xffffffff,  0, 0, "JPEG luma quantization 15"},
    {HEncJpegQuantLuma16  , 0x13c, 0xffffffff,  0, 0, "JPEG luma quantization 16"},
    {HEncJpegQuantChroma1 , 0x140, 0xffffffff,  0, 0, "JPEG chroma quantization 1"},
    {HEncJpegQuantChroma2 , 0x144, 0xffffffff,  0, 0, "JPEG chroma quantization 2"},
    {HEncJpegQuantChroma3 , 0x148, 0xffffffff,  0, 0, "JPEG chroma quantization 3"},
    {HEncJpegQuantChroma4 , 0x14c, 0xffffffff,  0, 0, "JPEG chroma quantization 4"},
    {HEncJpegQuantChroma5 , 0x150, 0xffffffff,  0, 0, "JPEG chroma quantization 5"},
    {HEncJpegQuantChroma6 , 0x154, 0xffffffff,  0, 0, "JPEG chroma quantization 6"},
    {HEncJpegQuantChroma7 , 0x158, 0xffffffff,  0, 0, "JPEG chroma quantization 7"},
    {HEncJpegQuantChroma8 , 0x15c, 0xffffffff,  0, 0, "JPEG chroma quantization 8"},
    {HEncJpegQuantChroma9 , 0x160, 0xffffffff,  0, 0, "JPEG chroma quantization 9"},
    {HEncJpegQuantChroma10, 0x164, 0xffffffff,  0, 0, "JPEG chroma quantization 10"},
    {HEncJpegQuantChroma11, 0x168, 0xffffffff,  0, 0, "JPEG chroma quantization 11"},
    {HEncJpegQuantChroma12, 0x16c, 0xffffffff,  0, 0, "JPEG chroma quantization 12"},
    {HEncJpegQuantChroma13, 0x170, 0xffffffff,  0, 0, "JPEG chroma quantization 13"},
    {HEncJpegQuantChroma14, 0x174, 0xffffffff,  0, 0, "JPEG chroma quantization 14"},
    {HEncJpegQuantChroma15, 0x178, 0xffffffff,  0, 0, "JPEG chroma quantization 15"},
    {HEncJpegQuantChroma16, 0x17C, 0xffffffff,  0, 0, "JPEG chroma quantization 16"},
    {HEncVp8Mode0Penalty     , 0x100, 0x00000fff,  0, 0, "VP8 intra 16x16 mode 0 penalty"},
    {HEncVp8Mode1Penalty     , 0x100, 0x00fff000, 12, 0, "VP8 intra 16x16 mode 1 penalty"},
    {HEncVp8Mode2Penalty     , 0x104, 0x00000fff,  0, 0, "VP8 intra 16x16 mode 2 penalty"},
    {HEncVp8Mode3Penalty     , 0x104, 0x00fff000, 12, 0, "VP8 intra 16x16 mode 3 penalty"},
    {HEncVp8Bmode0Penalty    , 0x108, 0x00000fff,  0, 0, "VP8 intra 4x4 mode 0 penalty"},
    {HEncVp8Bmode1Penalty    , 0x108, 0x00fff000, 12, 0, "VP8 intra 4x4 mode 1 penalty"},
    {HEncVp8Bmode2Penalty    , 0x10C, 0x00000fff,  0, 0, "VP8 intra 4x4 mode 2 penalty"},
    {HEncVp8Bmode3Penalty    , 0x10C, 0x00fff000, 12, 0, "VP8 intra 4x4 mode 3 penalty"},
    {HEncVp8Bmode4Penalty    , 0x110, 0x00000fff,  0, 0, "VP8 intra 4x4 mode 4 penalty"},
    {HEncVp8Bmode5Penalty    , 0x110, 0x00fff000, 12, 0, "VP8 intra 4x4 mode 5 penalty"},
    {HEncVp8Bmode6Penalty    , 0x114, 0x00000fff,  0, 0, "VP8 intra 4x4 mode 6 penalty"},
    {HEncVp8Bmode7Penalty    , 0x114, 0x00fff000, 12, 0, "VP8 intra 4x4 mode 7 penalty"},
    {HEncVp8Bmode8Penalty    , 0x118, 0x00000fff,  0, 0, "VP8 intra 4x4 mode 8 penalty"},
    {HEncVp8Bmode9Penalty    , 0x118, 0x00fff000, 12, 0, "VP8 intra 4x4 mode 9 penalty"},
    {HEncBaseVp8SegmentMap   , 0x11C, 0xffffffff,  0, 0, "Base address for VP8 segmentation map, segmentId 2-bits/macroblock"},
    {HEncVp8Seg1Y1QuantDc    , 0x120, 0x00003fff,  0, 1, "VP8 segment1 qpY1QuantDc 14b"},
    {HEncVp8Seg1Y1ZbinDc     , 0x120, 0x007fc000, 14, 1, "VP8 segment1 qpY1ZbinDc 9b"},
    {HEncVp8Seg1Y1RoundDc    , 0x120, 0x7f800000, 23, 1, "VP8 segment1 qpY1RoundDc 8b"},
    {HEncVp8Seg1Y1QuantAc    , 0x124, 0x00003fff,  0, 1, "VP8 segment1 qpY1QuantAc 14b"},
    {HEncVp8Seg1Y1ZbinAc     , 0x124, 0x007fc000, 14, 1, "VP8 segment1 qpY1ZbinAc 9b"},
    {HEncVp8Seg1Y1RoundAc    , 0x124, 0x7f800000, 23, 1, "VP8 segment1 qpY1RoundAc 8b"},
    {HEncVp8Seg1Y2QuantDc    , 0x128, 0x00003fff,  0, 1, "VP8 segment1 qpY2QuantDc 14b"},
    {HEncVp8Seg1Y2ZbinDc     , 0x128, 0x007fc000, 14, 1, "VP8 segment1 qpY2ZbinDc 9b"},
    {HEncVp8Seg1Y2RoundDc    , 0x128, 0x7f800000, 23, 1, "VP8 segment1 qpY2RoundDc 8b"},
    {HEncVp8Seg1Y2QuantAc    , 0x12C, 0x00003fff,  0, 1, "VP8 segment1 qpY2QuantAc 14b"},
    {HEncVp8Seg1Y2ZbinAc     , 0x12C, 0x007fc000, 14, 1, "VP8 segment1 qpY2ZbinAc 9b"},
    {HEncVp8Seg1Y2RoundAc    , 0x12C, 0x7f800000, 23, 1, "VP8 segment1 qpY2RoundAc 8b"},
    {HEncVp8Seg1ChQuantDc    , 0x130, 0x00003fff,  0, 1, "VP8 segment1 qpChQuantDc 14b"},
    {HEncVp8Seg1ChZbinDc     , 0x130, 0x007fc000, 14, 1, "VP8 segment1 qpChZbinDc 9b"},
    {HEncVp8Seg1ChRoundDc    , 0x130, 0x7f800000, 23, 1, "VP8 segment1 qpChRoundDc 8b"},
    {HEncVp8Seg1ChQuantAc    , 0x134, 0x00003fff,  0, 1, "VP8 segment1 qpChQuantAc 14b"},
    {HEncVp8Seg1ChZbinAc     , 0x134, 0x007fc000, 14, 1, "VP8 segment1 qpChZbinAc 9b"},
    {HEncVp8Seg1ChRoundAc    , 0x134, 0x7f800000, 23, 1, "VP8 segment1 qpChRoundAc 8b"},
    {HEncVp8Seg1Y1DequantDc  , 0x138, 0x000000ff,  0, 1, "VP8 segment1 qpY1DequantDc 8b"},
    {HEncVp8Seg1Y1DequantAc  , 0x138, 0x0001ff00,  8, 1, "VP8 segment1 qpY1DequantAc 9b"},
    {HEncVp8Seg1Y2DequantDc  , 0x138, 0x03fe0000, 17, 1, "VP8 segment1 qpY2DequantDc 9b"},
    {HEncVp8Seg1Y2DequantAc  , 0x13C, 0x000001ff,  0, 1, "VP8 segment1 qpY2DequantAc 9b"},
    {HEncVp8Seg1ChDequantDc  , 0x13C, 0x0001fe00,  9, 1, "VP8 segment1 qpChDequantDc 8b"},
    {HEncVp8Seg1ChDequantAc  , 0x13C, 0x03fe0000, 17, 1, "VP8 segment1 qpChDequantAc 9b"},
    {HEncVp8Seg1FilterLevel  , 0x13C, 0xfc000000, 26, 1, "VP8 segment1 filter level 6b"},
    {HEncVp8Seg2Y1QuantDc    , 0x140, 0x00003fff,  0, 1, "VP8 segment2 qpY1QuantDc 14b"},
    {HEncVp8Seg2Y1ZbinDc     , 0x140, 0x007fc000, 14, 1, "VP8 segment2 qpY1ZbinDc 9b"},
    {HEncVp8Seg2Y1RoundDc    , 0x140, 0x7f800000, 23, 1, "VP8 segment2 qpY1RoundDc 8b"},
    {HEncVp8Seg2Y1QuantAc    , 0x144, 0x00003fff,  0, 1, "VP8 segment2 qpY1QuantAc 14b"},
    {HEncVp8Seg2Y1ZbinAc     , 0x144, 0x007fc000, 14, 1, "VP8 segment2 qpY1ZbinAc 9b"},
    {HEncVp8Seg2Y1RoundAc    , 0x144, 0x7f800000, 23, 1, "VP8 segment2 qpY1RoundAc 8b"},
    {HEncVp8Seg2Y2QuantDc    , 0x148, 0x00003fff,  0, 1, "VP8 segment2 qpY2QuantDc 14b"},
    {HEncVp8Seg2Y2ZbinDc     , 0x148, 0x007fc000, 14, 1, "VP8 segment2 qpY2ZbinDc 9b"},
    {HEncVp8Seg2Y2RoundDc    , 0x148, 0x7f800000, 23, 1, "VP8 segment2 qpY2RoundDc 8b"},
    {HEncVp8Seg2Y2QuantAc    , 0x14C, 0x00003fff,  0, 1, "VP8 segment2 qpY2QuantAc 14b"},
    {HEncVp8Seg2Y2ZbinAc     , 0x14C, 0x007fc000, 14, 1, "VP8 segment2 qpY2ZbinAc 9b"},
    {HEncVp8Seg2Y2RoundAc    , 0x14C, 0x7f800000, 23, 1, "VP8 segment2 qpY2RoundAc 8b"},
    {HEncVp8Seg2ChQuantDc    , 0x150, 0x00003fff,  0, 1, "VP8 segment2 qpChQuantDc 14b"},
    {HEncVp8Seg2ChZbinDc     , 0x150, 0x007fc000, 14, 1, "VP8 segment2 qpChZbinDc 9b"},
    {HEncVp8Seg2ChRoundDc    , 0x150, 0x7f800000, 23, 1, "VP8 segment2 qpChRoundDc 8b"},
    {HEncVp8Seg2ChQuantAc    , 0x154, 0x00003fff,  0, 1, "VP8 segment2 qpChQuantAc 14b"},
    {HEncVp8Seg2ChZbinAc     , 0x154, 0x007fc000, 14, 1, "VP8 segment2 qpChZbinAc 9b"},
    {HEncVp8Seg2ChRoundAc    , 0x154, 0x7f800000, 23, 1, "VP8 segment2 qpChRoundAc 8b"},
    {HEncVp8Seg2Y1DequantDc  , 0x158, 0x000000ff,  0, 1, "VP8 segment2 qpY1DequantDc 8b"},
    {HEncVp8Seg2Y1DequantAc  , 0x158, 0x0001ff00,  8, 1, "VP8 segment2 qpY1DequantAc 9b"},
    {HEncVp8Seg2Y2DequantDc  , 0x158, 0x03fe0000, 17, 1, "VP8 segment2 qpY2DequantDc 9b"},
    {HEncVp8Seg2Y2DequantAc  , 0x15C, 0x000001ff,  0, 1, "VP8 segment2 qpY2DequantAc 9b"},
    {HEncVp8Seg2ChDequantDc  , 0x15C, 0x0001fe00,  9, 1, "VP8 segment2 qpChDequantDc 8b"},
    {HEncVp8Seg2ChDequantAc  , 0x15C, 0x03fe0000, 17, 1, "VP8 segment2 qpChDequantAc 9b"},
    {HEncVp8Seg2FilterLevel  , 0x15C, 0xfc000000, 26, 1, "VP8 segment2 filter level 6b"},
    {HEncVp8Seg3Y1QuantDc    , 0x160, 0x00003fff,  0, 1, "VP8 segment3 qpY1QuantDc 14b"},
    {HEncVp8Seg3Y1ZbinDc     , 0x160, 0x007fc000, 14, 1, "VP8 segment3 qpY1ZbinDc 9b"},
    {HEncVp8Seg3Y1RoundDc    , 0x160, 0x7f800000, 23, 1, "VP8 segment3 qpY1RoundDc 8b"},
    {HEncVp8Seg3Y1QuantAc    , 0x164, 0x00003fff,  0, 1, "VP8 segment3 qpY1QuantAc 14b"},
    {HEncVp8Seg3Y1ZbinAc     , 0x164, 0x007fc000, 14, 1, "VP8 segment3 qpY1ZbinAc 9b"},
    {HEncVp8Seg3Y1RoundAc    , 0x164, 0x7f800000, 23, 1, "VP8 segment3 qpY1RoundAc 8b"},
    {HEncVp8Seg3Y2QuantDc    , 0x168, 0x00003fff,  0, 1, "VP8 segment3 qpY2QuantDc 14b"},
    {HEncVp8Seg3Y2ZbinDc     , 0x168, 0x007fc000, 14, 1, "VP8 segment3 qpY2ZbinDc 9b"},
    {HEncVp8Seg3Y2RoundDc    , 0x168, 0x7f800000, 23, 1, "VP8 segment3 qpY2RoundDc 8b"},
    {HEncVp8Seg3Y2QuantAc    , 0x16C, 0x00003fff,  0, 1, "VP8 segment3 qpY2QuantAc 14b"},
    {HEncVp8Seg3Y2ZbinAc     , 0x16C, 0x007fc000, 14, 1, "VP8 segment3 qpY2ZbinAc 9b"},
    {HEncVp8Seg3Y2RoundAc    , 0x16C, 0x7f800000, 23, 1, "VP8 segment3 qpY2RoundAc 8b"},
    {HEncVp8Seg3ChQuantDc    , 0x170, 0x00003fff,  0, 1, "VP8 segment3 qpChQuantDc 14b"},
    {HEncVp8Seg3ChZbinDc     , 0x170, 0x007fc000, 14, 1, "VP8 segment3 qpChZbinDc 9b"},
    {HEncVp8Seg3ChRoundDc    , 0x170, 0x7f800000, 23, 1, "VP8 segment3 qpChRoundDc 8b"},
    {HEncVp8Seg3ChQuantAc    , 0x174, 0x00003fff,  0, 1, "VP8 segment3 qpChQuantAc 14b"},
    {HEncVp8Seg3ChZbinAc     , 0x174, 0x007fc000, 14, 1, "VP8 segment3 qpChZbinAc 9b"},
    {HEncVp8Seg3ChRoundAc    , 0x174, 0x7f800000, 23, 1, "VP8 segment3 qpChRoundAc 8b"},
    {HEncVp8Seg3Y1DequantDc  , 0x178, 0x000000ff,  0, 1, "VP8 segment3 qpY1DequantDc 8b"},
    {HEncVp8Seg3Y1DequantAc  , 0x178, 0x0001ff00,  8, 1, "VP8 segment3 qpY1DequantAc 9b"},
    {HEncVp8Seg3Y2DequantDc  , 0x178, 0x03fe0000, 17, 1, "VP8 segment3 qpY2DequantDc 9b"},
    {HEncVp8Seg3Y2DequantAc  , 0x17C, 0x000001ff,  0, 1, "VP8 segment3 qpY2DequantAc 9b"},
    {HEncVp8Seg3ChDequantDc  , 0x17C, 0x0001fe00,  9, 1, "VP8 segment3 qpChDequantDc 8b"},
    {HEncVp8Seg3ChDequantAc  , 0x17C, 0x03fe0000, 17, 1, "VP8 segment3 qpChDequantAc 9b"},
    {HEncVp8Seg3FilterLevel  , 0x17C, 0xfc000000, 26, 1, "VP8 segment3 filter level 6b"},

    {HEncDmvPenalty1         , 0x180, 0xffffffff,  0, 0, "DMV 4p/1p penalty values 0-3"},
    {HEncDmvPenalty2         , 0x184, 0xffffffff,  0, 0, "DMV 4p/1p penalty values 4-7"},
    {HEncDmvPenalty3         , 0x188, 0xffffffff,  0, 0, "DMV 4p/1p penalty values"},
    {HEncDmvPenalty4         , 0x18C, 0xffffffff,  0, 0, "DMV 4p/1p penalty values"},
    {HEncDmvPenalty5         , 0x190, 0xffffffff,  0, 0, "DMV 4p/1p penalty values"},
    {HEncDmvPenalty6         , 0x194, 0xffffffff,  0, 0, "DMV 4p/1p penalty values"},
    {HEncDmvPenalty7         , 0x198, 0xffffffff,  0, 0, "DMV 4p/1p penalty values"},
    {HEncDmvPenalty8         , 0x19C, 0xffffffff,  0, 0, "DMV 4p/1p penalty values"},
    {HEncDmvPenalty9         , 0x1A0, 0xffffffff,  0, 0, "DMV 4p/1p penalty values"},
    {HEncDmvPenalty10        , 0x1A4, 0xffffffff,  0, 0, "DMV 4p/1p penalty values"},
    {HEncDmvPenalty11        , 0x1A8, 0xffffffff,  0, 0, "DMV 4p/1p penalty values"},
    {HEncDmvPenalty12        , 0x1AC, 0xffffffff,  0, 0, "DMV 4p/1p penalty values"},
    {HEncDmvPenalty13        , 0x1B0, 0xffffffff,  0, 0, "DMV 4p/1p penalty values"},
    {HEncDmvPenalty14        , 0x1B4, 0xffffffff,  0, 0, "DMV 4p/1p penalty values"},
    {HEncDmvPenalty15        , 0x1B8, 0xffffffff,  0, 0, "DMV 4p/1p penalty values"},
    {HEncDmvPenalty16        , 0x1BC, 0xffffffff,  0, 0, "DMV 4p/1p penalty values"},
    {HEncDmvPenalty17        , 0x1C0, 0xffffffff,  0, 0, "DMV 4p/1p penalty values"},
    {HEncDmvPenalty18        , 0x1C4, 0xffffffff,  0, 0, "DMV 4p/1p penalty values"},
    {HEncDmvPenalty19        , 0x1C8, 0xffffffff,  0, 0, "DMV 4p/1p penalty values"},
    {HEncDmvPenalty20        , 0x1CC, 0xffffffff,  0, 0, "DMV 4p/1p penalty values"},
    {HEncDmvPenalty21        , 0x1D0, 0xffffffff,  0, 0, "DMV 4p/1p penalty values"},
    {HEncDmvPenalty22        , 0x1D4, 0xffffffff,  0, 0, "DMV 4p/1p penalty values"},
    {HEncDmvPenalty23        , 0x1D8, 0xffffffff,  0, 0, "DMV 4p/1p penalty values"},
    {HEncDmvPenalty24        , 0x1DC, 0xffffffff,  0, 0, "DMV 4p/1p penalty values"},
    {HEncDmvPenalty25        , 0x1E0, 0xffffffff,  0, 0, "DMV 4p/1p penalty values"},
    {HEncDmvPenalty26        , 0x1E4, 0xffffffff,  0, 0, "DMV 4p/1p penalty values"},
    {HEncDmvPenalty27        , 0x1E8, 0xffffffff,  0, 0, "DMV 4p/1p penalty values"},
    {HEncDmvPenalty28        , 0x1EC, 0xffffffff,  0, 0, "DMV 4p/1p penalty values"},
    {HEncDmvPenalty29        , 0x1F0, 0xffffffff,  0, 0, "DMV 4p/1p penalty values"},
    {HEncDmvPenalty30        , 0x1F4, 0xffffffff,  0, 0, "DMV 4p/1p penalty values"},
    {HEncDmvPenalty31        , 0x1F8, 0xffffffff,  0, 0, "DMV 4p/1p penalty values"},
    {HEncDmvPenalty32        , 0x1FC, 0xffffffff,  0, 0, "DMV 4p/1p penalty values 124-127"},

    {HEncDmvQpelPenalty1     , 0x200, 0xffffffff,  0, 0, "DMV qpel penalty values 0-3"},
    {HEncDmvQpelPenalty2     , 0x204, 0xffffffff,  0, 0, "DMV qpel penalty values 4-7"},
    {HEncDmvQpelPenalty3     , 0x208, 0xffffffff,  0, 0, "DMV qpel penalty values"},
    {HEncDmvQpelPenalty4     , 0x20C, 0xffffffff,  0, 0, "DMV qpel penalty values"},
    {HEncDmvQpelPenalty5     , 0x210, 0xffffffff,  0, 0, "DMV qpel penalty values"},
    {HEncDmvQpelPenalty6     , 0x214, 0xffffffff,  0, 0, "DMV qpel penalty values"},
    {HEncDmvQpelPenalty7     , 0x218, 0xffffffff,  0, 0, "DMV qpel penalty values"},
    {HEncDmvQpelPenalty8     , 0x21C, 0xffffffff,  0, 0, "DMV qpel penalty values"},
    {HEncDmvQpelPenalty9     , 0x220, 0xffffffff,  0, 0, "DMV qpel penalty values"},
    {HEncDmvQpelPenalty10    , 0x224, 0xffffffff,  0, 0, "DMV qpel penalty values"},
    {HEncDmvQpelPenalty11    , 0x228, 0xffffffff,  0, 0, "DMV qpel penalty values"},
    {HEncDmvQpelPenalty12    , 0x22C, 0xffffffff,  0, 0, "DMV qpel penalty values"},
    {HEncDmvQpelPenalty13    , 0x230, 0xffffffff,  0, 0, "DMV qpel penalty values"},
    {HEncDmvQpelPenalty14    , 0x234, 0xffffffff,  0, 0, "DMV qpel penalty values"},
    {HEncDmvQpelPenalty15    , 0x238, 0xffffffff,  0, 0, "DMV qpel penalty values"},
    {HEncDmvQpelPenalty16    , 0x23C, 0xffffffff,  0, 0, "DMV qpel penalty values"},
    {HEncDmvQpelPenalty17    , 0x240, 0xffffffff,  0, 0, "DMV qpel penalty values"},
    {HEncDmvQpelPenalty18    , 0x244, 0xffffffff,  0, 0, "DMV qpel penalty values"},
    {HEncDmvQpelPenalty19    , 0x248, 0xffffffff,  0, 0, "DMV qpel penalty values"},
    {HEncDmvQpelPenalty20    , 0x24C, 0xffffffff,  0, 0, "DMV qpel penalty values"},
    {HEncDmvQpelPenalty21    , 0x250, 0xffffffff,  0, 0, "DMV qpel penalty values"},
    {HEncDmvQpelPenalty22    , 0x254, 0xffffffff,  0, 0, "DMV qpel penalty values"},
    {HEncDmvQpelPenalty23    , 0x258, 0xffffffff,  0, 0, "DMV qpel penalty values"},
    {HEncDmvQpelPenalty24    , 0x25C, 0xffffffff,  0, 0, "DMV qpel penalty values"},
    {HEncDmvQpelPenalty25    , 0x260, 0xffffffff,  0, 0, "DMV qpel penalty values"},
    {HEncDmvQpelPenalty26    , 0x264, 0xffffffff,  0, 0, "DMV qpel penalty values"},
    {HEncDmvQpelPenalty27    , 0x268, 0xffffffff,  0, 0, "DMV qpel penalty values"},
    {HEncDmvQpelPenalty28    , 0x26C, 0xffffffff,  0, 0, "DMV qpel penalty values"},
    {HEncDmvQpelPenalty29    , 0x270, 0xffffffff,  0, 0, "DMV qpel penalty values"},
    {HEncDmvQpelPenalty30    , 0x274, 0xffffffff,  0, 0, "DMV qpel penalty values"},
    {HEncDmvQpelPenalty31    , 0x278, 0xffffffff,  0, 0, "DMV qpel penalty values"},
    {HEncDmvQpelPenalty32    , 0x27C, 0xffffffff,  0, 0, "DMV qpel penalty values 124-127"},

    {HEncVp8CostInter        , 0x280, 0x00000fff,  0, 0, "VP8 bit cost of inter type"},
    {HEncVp8DmvCostConst     , 0x280, 0x00fff000, 12, 0, "VP8 coeff for dmv penalty for intra/inter selection"},
    {HEncVp8CostGoldenRef    , 0x284, 0x00000fff,  0, 0, "VP8 bit cost of golden ref frame"},
    {HEncVp8LfRefDelta0      , 0x288, 0x0000007f,  0, 0, "VP8 loop filter delta for intra mb"},
    {HEncVp8LfRefDelta1      , 0x288, 0x00003f80,  7, 0, "VP8 loop filter delta for last ref"},
    {HEncVp8LfRefDelta2      , 0x288, 0x001fc000, 14, 0, "VP8 loop filter delta for golden ref"},
    {HEncVp8LfRefDelta3      , 0x288, 0x0fe00000, 21, 0, "VP8 loop filter delta for alt ref"},
    {HEncVp8LfModeDelta0     , 0x28C, 0x0000007f,  0, 0, "VP8 loop filter delta for BPRED"},
    {HEncVp8LfModeDelta1     , 0x28C, 0x00003f80,  7, 0, "VP8 loop filter delta for ZEROMV"},
    {HEncVp8LfModeDelta2     , 0x28C, 0x001fc000, 14, 0, "VP8 loop filter delta for NEWMV"},
    {HEncVp8LfModeDelta3     , 0x28C, 0x0fe00000, 21, 0, "VP8 loop filter delta for SPLITMV"}

};


/*------------------------------------------------------------------------------
    Local function prototypes
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    Initialize empty structure with default values.
------------------------------------------------------------------------------*/
i32 VP8_EncAsicControllerInit(asicData_s * asic)
{
    ASSERT(asic != NULL);

    /* Initialize default values from defined configuration */
    asic->regs.irqDisable = ENCH1_IRQ_DISABLE;

    asic->regs.asicCfgReg =
        ((ENCH1_AXI_WRITE_ID & (255)) << 24) |
        ((ENCH1_AXI_READ_ID & (255)) << 16) |
        ((ENCH1_OUTPUT_SWAP_16 & (1)) << 15) |
        ((ENCH1_BURST_LENGTH & (63)) << 8) |
        ((ENCH1_BURST_SCMD_DISABLE & (1)) << 7) |
        ((ENCH1_BURST_INCR_TYPE_ENABLED & (1)) << 6) |
        ((ENCH1_BURST_DATA_DISCARD_ENABLED & (1)) << 5) |
        ((ENCH1_ASIC_CLOCK_GATING_ENABLED & (1)) << 4) |
        ((ENCH1_OUTPUT_SWAP_32 & (1)) << 3) |
        ((ENCH1_OUTPUT_SWAP_8 & (1)) << 1);

#ifdef INTERNAL_TEST
    /* With internal testing use random values for burst mode. */
    asic->regs.asicCfgReg |= (EWLReadReg(asic->ewl, 0x60) & 0xE0);
#endif

    /* Initialize default values */
    asic->regs.roundingCtrl = 0;
    asic->regs.cpDistanceMbs = 0;
    asic->regs.reconImageId = 0;

    /* User must set these */
    asic->regs.inputLumBase = 0;
    asic->regs.inputCbBase = 0;
    asic->regs.inputCrBase = 0;

    asic->internalImageLuma[0].vir_addr = NULL;
    asic->internalImageChroma[0].vir_addr = NULL;
    asic->internalImageLuma[1].vir_addr = NULL;
    asic->internalImageChroma[1].vir_addr = NULL;
    asic->internalImageLuma[2].vir_addr = NULL;
    asic->internalImageChroma[2].vir_addr = NULL;
    asic->sizeTbl.vir_addr = NULL;
    asic->cabacCtx.vir_addr = NULL;
    asic->mvOutput.vir_addr = NULL;
    asic->probCount.vir_addr = NULL;

#ifdef ASIC_WAVE_TRACE_TRIGGER
    asic->regs.vop_count = 0;
#endif

    /* get ASIC ID value */
    asic->regs.asicHwId = VP8_EncAsicGetId(asic->ewl);

/* we do NOT reset hardware at this point because */
/* of the multi-instance support                  */

    return ENCHW_OK;
}


/*------------------------------------------------------------------------------

    EncAsicSetQuantTable

    Set new jpeg quantization table to be used by ASIC

------------------------------------------------------------------------------*/
void VP8_EncAsicSetQuantTable(asicData_s * asic,
                          const u8 * lumTable, const u8 * chTable)
{
    i32 i;

    ASSERT(lumTable);
    ASSERT(chTable);

    for(i = 0; i < 64; i++)
    {
        asic->regs.quantTable[i] = lumTable[qpReorderTable[i]];
    }
    for(i = 0; i < 64; i++)
    {
        asic->regs.quantTable[64 + i] = chTable[qpReorderTable[i]];
    }
}

/*------------------------------------------------------------------------------

    EncAsicSetRegisterValue

    Set a value into a defined register field

------------------------------------------------------------------------------*/
void VP8_EncAsicSetRegisterValue(u32 *regMirror, regName name, u32 value)
{
    const regField_s *field;
    u32 regVal;

    field = &asicRegisterDesc[name];

    /* Check that value fits in field */
    ASSERT(field->name == name);
    ASSERT((field->mask >> field->lsb) >= value);
    ASSERT(field->base < ASIC_SWREG_AMOUNT*4);

    /* Clear previous value of field in register */
    regVal = regMirror[field->base/4] & ~(field->mask);

    /* Put new value of field in register */
    regMirror[field->base/4] = regVal | ((value << field->lsb) & field->mask);
}

/*------------------------------------------------------------------------------

    EncAsicGetRegisterValue

    Get an unsigned value from the ASIC registers

------------------------------------------------------------------------------*/
u32 VP8_EncAsicGetRegisterValue(const void *ewl, u32 *regMirror, regName name)
{
    const regField_s *field;
    u32 value;

    field = &asicRegisterDesc[name];

    ASSERT(field->base < ASIC_SWREG_AMOUNT*4);

    value = regMirror[field->base/4];// = EWLReadReg(ewl, field->base);
    value = (value & field->mask) >> field->lsb;

    return value;
}

/*------------------------------------------------------------------------------
------------------------------------------------------------------------------*/
u32 VP8_EncAsicGetId(const void *ewl)
{
    return 0x8190;//EWLReadReg(ewl, 0x0);
}

/*------------------------------------------------------------------------------
    When the frame is successfully encoded the internal image is recycled
    so that during the next frame the current internal image is read and
    the new reconstructed image is written. Note that this is done for
    the NEXT frame.
------------------------------------------------------------------------------*/
#define NEXTID(currentId, maxId) ((currentId == maxId) ? 0 : currentId+1)
#define PREVID(currentId, maxId) ((currentId == 0) ? maxId : currentId-1)

void VP8_EncAsicRecycleInternalImage(asicData_s *asic, u32 numViews, u32 viewId,
        u32 anchor, u32 numRefBuffsLum, u32 numRefBuffsChr)
{
    u32 tmp, id;

    if (numViews == 1)
    {
        /* H.264 base view stream, just swap buffers */
        tmp = asic->regs.internalImageLumBaseW;
        asic->regs.internalImageLumBaseW = asic->regs.internalImageLumBaseR[0];
        asic->regs.internalImageLumBaseR[0] = tmp;

        tmp = asic->regs.internalImageChrBaseW;
        asic->regs.internalImageChrBaseW = asic->regs.internalImageChrBaseR[0];
        asic->regs.internalImageChrBaseR[0] = tmp;
    }
    else
    {
        /* MVC stereo stream, the buffer amount tells if the second view
         * is inter-view or inter coded. This affects how the buffers
         * should be swapped. */

        if ((viewId == 0) && (anchor || (numRefBuffsLum == 1)))
        {
            /* Next frame is viewId=1 with inter-view prediction */
            asic->regs.internalImageLumBaseR[0] = asic->internalImageLuma[0].phy_addr;
            asic->regs.internalImageLumBaseW = asic->internalImageLuma[1].phy_addr;
        }
        else if (viewId == 0)
        {
            /* Next frame is viewId=1 with inter prediction */
            asic->regs.internalImageLumBaseR[0] = asic->internalImageLuma[1].phy_addr;
            asic->regs.internalImageLumBaseW = asic->internalImageLuma[1].phy_addr;
        }
        else
        {
            /* Next frame is viewId=0 with inter prediction */
            asic->regs.internalImageLumBaseR[0] = asic->internalImageLuma[0].phy_addr;
            asic->regs.internalImageLumBaseW = asic->internalImageLuma[0].phy_addr;
        }

        if (numRefBuffsChr == 2)
        {
            if (viewId == 0)
            {
                /* For inter-view prediction chroma is swapped after base view only */
                tmp = asic->regs.internalImageChrBaseW;
                asic->regs.internalImageChrBaseW = asic->regs.internalImageChrBaseR[0];
                asic->regs.internalImageChrBaseR[0] = tmp;
            }
        }
        else
        {
            /* For viewId==1 the anchor frame uses inter-view prediction,
             * all other frames use inter-prediction. */
            if ((viewId == 0) && anchor)
                id = asic->regs.reconImageId;
            else
                id = PREVID(asic->regs.reconImageId, 2);

            asic->regs.internalImageChrBaseR[0] = asic->internalImageChroma[id].phy_addr;
            asic->regs.reconImageId = id = NEXTID(asic->regs.reconImageId, 2);
            asic->regs.internalImageChrBaseW = asic->internalImageChroma[id].phy_addr;
        }
    }
}

/*------------------------------------------------------------------------------
------------------------------------------------------------------------------*/
void VP8_CheckRegisterValues(regValues_s * val)
{
    u32 i;

    ASSERT(val->irqDisable <= 1);
    ASSERT(val->rlcLimitSpace / 2 < (1 << 20));
    ASSERT(val->mbsInCol <= 511);
    ASSERT(val->mbsInRow <= 511);
    ASSERT(val->filterDisable <= 2);
    ASSERT(val->skipPenalty <= 255);
    ASSERT(val->goldenPenalty <= 255);
    ASSERT(val->recWriteDisable <= 1);
    ASSERT(val->madThreshold <= 63);
    ASSERT(val->madQpDelta >= -8 && val->madQpDelta <= 7);
    ASSERT(val->qp <= 63);
    ASSERT(val->constrainedIntraPrediction <= 1);
    ASSERT(val->roundingCtrl <= 1);
    ASSERT(val->frameCodingType <= 3);
    ASSERT(val->codingType <= 3);
    ASSERT(val->outputStrmSize <= 0x1FFFFFF);
    ASSERT(val->pixelsOnRow >= 16 && val->pixelsOnRow <= 8192); /* max input for cropping */
    ASSERT(val->xFill <= 3);
    ASSERT(val->yFill <= 14 && ((val->yFill & 0x01) == 0));
    ASSERT(val->inputLumaBaseOffset <= 7);
    ASSERT(val->inputChromaBaseOffset <= 7);
    ASSERT(val->sliceAlphaOffset >= -6 && val->sliceAlphaOffset <= 6);
    ASSERT(val->sliceBetaOffset >= -6 && val->sliceBetaOffset <= 6);
    ASSERT(val->chromaQpIndexOffset >= -12 && val->chromaQpIndexOffset <= 12);
    ASSERT(val->sliceSizeMbRows <= 127);
    ASSERT(val->inputImageFormat <= ASIC_INPUT_YUYV422TILED);
    ASSERT(val->inputImageRotation <= 2);
    ASSERT(val->cpDistanceMbs <= 8191);
    if(val->codingType == ASIC_H264) {
        ASSERT(val->roi1DeltaQp >= 0 && val->roi1DeltaQp <= 15);
        ASSERT(val->roi2DeltaQp >= 0 && val->roi2DeltaQp <= 15);
    }
    ASSERT(val->cirStart <= 65535);
    ASSERT(val->cirInterval <= 65535);
    ASSERT(val->intraAreaTop <= 255);
    ASSERT(val->intraAreaLeft <= 255);
    ASSERT(val->intraAreaBottom <= 255);
    ASSERT(val->intraAreaRight <= 255);
    ASSERT(val->roi1Top <= 255);
    ASSERT(val->roi1Left <= 255);
    ASSERT(val->roi1Bottom <= 255);
    ASSERT(val->roi1Right <= 255);
    ASSERT(val->roi2Top <= 255);
    ASSERT(val->roi2Left <= 255);
    ASSERT(val->roi2Bottom <= 255);
    ASSERT(val->roi2Right <= 255);

    if(val->codingType != ASIC_JPEG && val->cpTarget != NULL)
    {
        ASSERT(val->cpTargetResults != NULL);

        for(i = 0; i < 10; i++)
        {
            ASSERT(*val->cpTarget < (1 << 16));
        }

        ASSERT(val->targetError != NULL);

        for(i = 0; i < 7; i++)
        {
            ASSERT((*val->targetError) >= -32768 &&
                   (*val->targetError) < 32768);
        }

        ASSERT(val->deltaQp != NULL);

        for(i = 0; i < 7; i++)
        {
            ASSERT((*val->deltaQp) >= -8 && (*val->deltaQp) < 8);
        }
    }

    (void) val;
}

/*------------------------------------------------------------------------------
    Function name   : EncAsicFrameStart
    Description     : 
    Return type     : void 
    Argument        : const void *ewl
    Argument        : regValues_s * val
------------------------------------------------------------------------------*/
void VP8_EncAsicFrameStart(const void *ewl, regValues_s * val)
{
    i32 i;

	
    VP8_CheckRegisterValues(val);

    memset(val->regMirror, 0, sizeof(val->regMirror));

    /* encoder interrupt */
    VP8_EncAsicSetRegisterValue(val->regMirror, HEncIRQDisable, val->irqDisable);

    /* system configuration */
    if (val->inputImageFormat < ASIC_INPUT_RGB565)      /* YUV input */
        val->regMirror[2] = val->asicCfgReg |
            ((ENCH1_INPUT_SWAP_16_YUV & (1)) << 14) |
            ((ENCH1_INPUT_SWAP_32_YUV & (1)) << 2) |
            (ENCH1_INPUT_SWAP_8_YUV & (1));
    else if (val->inputImageFormat < ASIC_INPUT_RGB888) /* 16-bit RGB input */
        val->regMirror[2] = val->asicCfgReg |
            ((ENCH1_INPUT_SWAP_16_RGB16 & (1)) << 14) |
            ((ENCH1_INPUT_SWAP_32_RGB16 & (1)) << 2) |
            (ENCH1_INPUT_SWAP_8_RGB16 & (1));
    else    /* 32-bit RGB input */
        val->regMirror[2] = val->asicCfgReg |
            ((ENCH1_INPUT_SWAP_16_RGB32 & (1)) << 14) |
            ((ENCH1_INPUT_SWAP_32_RGB32 & (1)) << 2) |
            (ENCH1_INPUT_SWAP_8_RGB32 & (1));

    /* output stream buffer */
    VP8_EncAsicSetRegisterValue(val->regMirror, HEncBaseStream, val->outputStrmBase);

    /* Video encoding output buffers and reference picture buffers */
    if(val->codingType != ASIC_JPEG)
    {
        VP8_EncAsicSetRegisterValue(val->regMirror, HEncBaseControl, val->sizeTblBase);
        VP8_EncAsicSetRegisterValue(val->regMirror, HEncNalSizeWrite, val->sizeTblBase != 0);
        VP8_EncAsicSetRegisterValue(val->regMirror, HEncMvWrite, val->mvOutputBase != 0);

        VP8_EncAsicSetRegisterValue(val->regMirror, HEncBaseRefLum, val->internalImageLumBaseR[0]);
        VP8_EncAsicSetRegisterValue(val->regMirror, HEncBaseRefChr, val->internalImageChrBaseR[0]);
        VP8_EncAsicSetRegisterValue(val->regMirror, HEncBaseRecLum, val->internalImageLumBaseW);
        VP8_EncAsicSetRegisterValue(val->regMirror, HEncBaseRecChr, val->internalImageChrBaseW);
    }

    /* Input picture buffers */
    VP8_EncAsicSetRegisterValue(val->regMirror, HEncBaseInLum, val->inputLumBase);
    VP8_EncAsicSetRegisterValue(val->regMirror, HEncBaseInCb, val->inputCbBase);
    VP8_EncAsicSetRegisterValue(val->regMirror, HEncBaseInCr, val->inputCrBase);

    /* Common control register */
    VP8_EncAsicSetRegisterValue(val->regMirror, HEncIntTimeout, ENCH1_TIMEOUT_INTERRUPT&1);
    VP8_EncAsicSetRegisterValue(val->regMirror, HEncIntSliceReady, val->sliceReadyInterrupt);
    VP8_EncAsicSetRegisterValue(val->regMirror, HEncRecWriteDisable, val->recWriteDisable);
    VP8_EncAsicSetRegisterValue(val->regMirror, HEncWidth, val->mbsInRow);
    VP8_EncAsicSetRegisterValue(val->regMirror, HEncHeight, val->mbsInCol);
    VP8_EncAsicSetRegisterValue(val->regMirror, HEncPictureType, val->frameCodingType);
    VP8_EncAsicSetRegisterValue(val->regMirror, HEncEncodingMode, val->codingType);

    /* PreP control */
    VP8_EncAsicSetRegisterValue(val->regMirror, HEncChrOffset, val->inputChromaBaseOffset);
    VP8_EncAsicSetRegisterValue(val->regMirror, HEncLumOffset, val->inputLumaBaseOffset);
    VP8_EncAsicSetRegisterValue(val->regMirror, HEncRowLength, val->pixelsOnRow);
    VP8_EncAsicSetRegisterValue(val->regMirror, HEncXFill, val->xFill);
    VP8_EncAsicSetRegisterValue(val->regMirror, HEncYFill, val->yFill);
    VP8_EncAsicSetRegisterValue(val->regMirror, HEncInputFormat, val->inputImageFormat);
    VP8_EncAsicSetRegisterValue(val->regMirror, HEncInputRot, val->inputImageRotation);

    /* Common controls */
    VP8_EncAsicSetRegisterValue(val->regMirror, HEncCabacEnable, val->enableCabac);
    VP8_EncAsicSetRegisterValue(val->regMirror, HEncIPIntra16Favor, val->intra16Favor);
    VP8_EncAsicSetRegisterValue(val->regMirror, HEncInterFavor, val->interFavor);
    VP8_EncAsicSetRegisterValue(val->regMirror, HEncDisableQPMV, val->disableQuarterPixelMv);
    VP8_EncAsicSetRegisterValue(val->regMirror, HEncDeblocking, val->filterDisable);
    VP8_EncAsicSetRegisterValue(val->regMirror, HEncSkipPenalty, val->skipPenalty);
    VP8_EncAsicSetRegisterValue(val->regMirror, HEncSplitMv, val->splitMvMode);
    VP8_EncAsicSetRegisterValue(val->regMirror, HEncSplitPenalty16x8, val->splitPenalty[0]);
    VP8_EncAsicSetRegisterValue(val->regMirror, HEncSplitPenalty8x8, val->splitPenalty[1]);
    VP8_EncAsicSetRegisterValue(val->regMirror, HEncSplitPenalty8x4, val->splitPenalty[2]);
    VP8_EncAsicSetRegisterValue(val->regMirror, HEncSplitPenalty4x4, val->splitPenalty[3]);
    VP8_EncAsicSetRegisterValue(val->regMirror, HEncZeroMvFavor, val->zeroMvFavorDiv2);

    /* H.264 specific control */
    if (val->codingType == ASIC_H264)
    {
        VP8_EncAsicSetRegisterValue(val->regMirror, HEncPicInitQp, val->picInitQp);
        VP8_EncAsicSetRegisterValue(val->regMirror, HEncSliceAlpha, val->sliceAlphaOffset & mask_4b);
        VP8_EncAsicSetRegisterValue(val->regMirror, HEncSliceBeta, val->sliceBetaOffset & mask_4b);
        VP8_EncAsicSetRegisterValue(val->regMirror, HEncChromaQp, val->chromaQpIndexOffset & mask_5b);
        VP8_EncAsicSetRegisterValue(val->regMirror, HEncIdrPicId, val->idrPicId);
        VP8_EncAsicSetRegisterValue(val->regMirror, HEncConstrIP, val->constrainedIntraPrediction);

        VP8_EncAsicSetRegisterValue(val->regMirror, HEncPPSID, val->ppsId);
        VP8_EncAsicSetRegisterValue(val->regMirror, HEncIPPrevModeFavor, val->prevModeFavor);

        VP8_EncAsicSetRegisterValue(val->regMirror, HEncSliceSize, val->sliceSizeMbRows);
        VP8_EncAsicSetRegisterValue(val->regMirror, HEncTransform8x8, val->transform8x8Mode);
        VP8_EncAsicSetRegisterValue(val->regMirror, HEncCabacInitIdc, val->cabacInitIdc);
        VP8_EncAsicSetRegisterValue(val->regMirror, HEncInter4Restrict, val->h264Inter4x4Disabled);
        VP8_EncAsicSetRegisterValue(val->regMirror, HEncStreamMode, val->h264StrmMode);
        VP8_EncAsicSetRegisterValue(val->regMirror, HEncFrameNum, val->frameNum);

        VP8_EncAsicSetRegisterValue(val->regMirror, HEncMadQpDelta, val->madQpDelta & mask_4b);
        VP8_EncAsicSetRegisterValue(val->regMirror, HEncMadThreshold, val->madThreshold);
    }

    /* JPEG specific control */
    if (val->codingType == ASIC_JPEG)
    {
        VP8_EncAsicSetRegisterValue(val->regMirror, HEncJpegMode, val->jpegMode);
        VP8_EncAsicSetRegisterValue(val->regMirror, HEncJpegSlice, val->jpegSliceEnable);
        VP8_EncAsicSetRegisterValue(val->regMirror, HEncJpegRSTInt, val->jpegRestartInterval);
        VP8_EncAsicSetRegisterValue(val->regMirror, HEncJpegRST, val->jpegRestartMarker);
    }

    /* stream buffer limits */
    VP8_EncAsicSetRegisterValue(val->regMirror, HEncStrmHdrRem1, val->strmStartMSB);
    VP8_EncAsicSetRegisterValue(val->regMirror, HEncStrmHdrRem2, val->strmStartLSB);
    VP8_EncAsicSetRegisterValue(val->regMirror, HEncStrmBufLimit, val->outputStrmSize);

    /* video encoding rate control regs 0x6C - 0x90,
     * different register definitions for VP8 and H.264 */
    if(val->codingType != ASIC_JPEG)
    {
        if (val->codingType == ASIC_VP8)
        {
            VP8_EncAsicSetRegisterValue(val->regMirror, HEncBaseRefLum2, val->internalImageLumBaseR[1]);
            VP8_EncAsicSetRegisterValue(val->regMirror, HEncBaseRefChr2, val->internalImageChrBaseR[1]);

            VP8_EncAsicSetRegisterValue(val->regMirror, HEncVp8Y1QuantDc, val->qpY1QuantDc[0]);
            VP8_EncAsicSetRegisterValue(val->regMirror, HEncVp8Y1QuantAc, val->qpY1QuantAc[0]);
            VP8_EncAsicSetRegisterValue(val->regMirror, HEncVp8Y2QuantDc, val->qpY2QuantDc[0]);
            VP8_EncAsicSetRegisterValue(val->regMirror, HEncVp8Y2QuantAc, val->qpY2QuantAc[0]);
            VP8_EncAsicSetRegisterValue(val->regMirror, HEncVp8ChQuantDc, val->qpChQuantDc[0]);
            VP8_EncAsicSetRegisterValue(val->regMirror, HEncVp8ChQuantAc, val->qpChQuantAc[0]);

            VP8_EncAsicSetRegisterValue(val->regMirror, HEncVp8Y1ZbinDc, val->qpY1ZbinDc[0]);
            VP8_EncAsicSetRegisterValue(val->regMirror, HEncVp8Y1ZbinAc, val->qpY1ZbinAc[0]);
            VP8_EncAsicSetRegisterValue(val->regMirror, HEncVp8Y2ZbinDc, val->qpY2ZbinDc[0]);
            VP8_EncAsicSetRegisterValue(val->regMirror, HEncVp8Y2ZbinAc, val->qpY2ZbinAc[0]);
            VP8_EncAsicSetRegisterValue(val->regMirror, HEncVp8ChZbinDc, val->qpChZbinDc[0]);
            VP8_EncAsicSetRegisterValue(val->regMirror, HEncVp8ChZbinAc, val->qpChZbinAc[0]);

            VP8_EncAsicSetRegisterValue(val->regMirror, HEncVp8Y1RoundDc, val->qpY1RoundDc[0]);
            VP8_EncAsicSetRegisterValue(val->regMirror, HEncVp8Y1RoundAc, val->qpY1RoundAc[0]);
            VP8_EncAsicSetRegisterValue(val->regMirror, HEncVp8Y2RoundDc, val->qpY2RoundDc[0]);
            VP8_EncAsicSetRegisterValue(val->regMirror, HEncVp8Y2RoundAc, val->qpY2RoundAc[0]);
            VP8_EncAsicSetRegisterValue(val->regMirror, HEncVp8ChRoundDc, val->qpChRoundDc[0]);
            VP8_EncAsicSetRegisterValue(val->regMirror, HEncVp8ChRoundAc, val->qpChRoundAc[0]);

            VP8_EncAsicSetRegisterValue(val->regMirror, HEncVp8Y1DequantDc, val->qpY1DequantDc[0]);
            VP8_EncAsicSetRegisterValue(val->regMirror, HEncVp8Y1DequantAc, val->qpY1DequantAc[0]);
            VP8_EncAsicSetRegisterValue(val->regMirror, HEncVp8Y2DequantDc, val->qpY2DequantDc[0]);
            VP8_EncAsicSetRegisterValue(val->regMirror, HEncVp8Y2DequantAc, val->qpY2DequantAc[0]);
            VP8_EncAsicSetRegisterValue(val->regMirror, HEncVp8ChDequantDc, val->qpChDequantDc[0]);
            VP8_EncAsicSetRegisterValue(val->regMirror, HEncVp8ChDequantAc, val->qpChDequantAc[0]);

            VP8_EncAsicSetRegisterValue(val->regMirror, HEncVp8MvRefIdx, val->mvRefIdx[0]);
            VP8_EncAsicSetRegisterValue(val->regMirror, HEncVp8MvRefIdx2, val->mvRefIdx[1]);
            VP8_EncAsicSetRegisterValue(val->regMirror, HEncVp8Ref2Enable, val->ref2Enable);

            VP8_EncAsicSetRegisterValue(val->regMirror, HEncVp8BoolEncValue, val->boolEncValue);
            VP8_EncAsicSetRegisterValue(val->regMirror, HEncVp8BoolEncValueBits, val->boolEncValueBits);
            VP8_EncAsicSetRegisterValue(val->regMirror, HEncVp8BoolEncRange, val->boolEncRange);

            VP8_EncAsicSetRegisterValue(val->regMirror, HEncVp8FilterLevel, val->filterLevel[0]);
            VP8_EncAsicSetRegisterValue(val->regMirror, HEncVp8GoldenPenalty, val->goldenPenalty);
            VP8_EncAsicSetRegisterValue(val->regMirror, HEncVp8FilterSharpness, val->filterSharpness);
            VP8_EncAsicSetRegisterValue(val->regMirror, HEncVp8DctPartitionCount, val->dctPartitions);
        } else {    /* H.264 */
            VP8_EncAsicSetRegisterValue(val->regMirror, HEncQp, val->qp);
            VP8_EncAsicSetRegisterValue(val->regMirror, HEncMaxQp, val->qpMax);
            VP8_EncAsicSetRegisterValue(val->regMirror, HEncMinQp, val->qpMin);
            VP8_EncAsicSetRegisterValue(val->regMirror, HEncCPDist, val->cpDistanceMbs);

            if(val->cpTarget != NULL)
            {
                VP8_EncAsicSetRegisterValue(val->regMirror, HEncCP1WordTarget, val->cpTarget[0]);
                VP8_EncAsicSetRegisterValue(val->regMirror, HEncCP2WordTarget, val->cpTarget[1]);
                VP8_EncAsicSetRegisterValue(val->regMirror, HEncCP3WordTarget, val->cpTarget[2]);
                VP8_EncAsicSetRegisterValue(val->regMirror, HEncCP4WordTarget, val->cpTarget[3]);
                VP8_EncAsicSetRegisterValue(val->regMirror, HEncCP5WordTarget, val->cpTarget[4]);
                VP8_EncAsicSetRegisterValue(val->regMirror, HEncCP6WordTarget, val->cpTarget[5]);
                VP8_EncAsicSetRegisterValue(val->regMirror, HEncCP7WordTarget, val->cpTarget[6]);
                VP8_EncAsicSetRegisterValue(val->regMirror, HEncCP8WordTarget, val->cpTarget[7]);
                VP8_EncAsicSetRegisterValue(val->regMirror, HEncCP9WordTarget, val->cpTarget[8]);
                VP8_EncAsicSetRegisterValue(val->regMirror, HEncCP10WordTarget, val->cpTarget[9]);

                VP8_EncAsicSetRegisterValue(val->regMirror, HEncCPWordError1,
                                        val->targetError[0] & mask_16b);
                VP8_EncAsicSetRegisterValue(val->regMirror, HEncCPWordError2,
                                        val->targetError[1] & mask_16b);
                VP8_EncAsicSetRegisterValue(val->regMirror, HEncCPWordError3,
                                        val->targetError[2] & mask_16b);
                VP8_EncAsicSetRegisterValue(val->regMirror, HEncCPWordError4,
                                        val->targetError[3] & mask_16b);
                VP8_EncAsicSetRegisterValue(val->regMirror, HEncCPWordError5,
                                        val->targetError[4] & mask_16b);
                VP8_EncAsicSetRegisterValue(val->regMirror, HEncCPWordError6,
                                        val->targetError[5] & mask_16b);

                VP8_EncAsicSetRegisterValue(val->regMirror, HEncCPDeltaQp1, val->deltaQp[0] & mask_4b);
                VP8_EncAsicSetRegisterValue(val->regMirror, HEncCPDeltaQp2, val->deltaQp[1] & mask_4b);
                VP8_EncAsicSetRegisterValue(val->regMirror, HEncCPDeltaQp3, val->deltaQp[2] & mask_4b);
                VP8_EncAsicSetRegisterValue(val->regMirror, HEncCPDeltaQp4, val->deltaQp[3] & mask_4b);
                VP8_EncAsicSetRegisterValue(val->regMirror, HEncCPDeltaQp5, val->deltaQp[4] & mask_4b);
                VP8_EncAsicSetRegisterValue(val->regMirror, HEncCPDeltaQp6, val->deltaQp[5] & mask_4b);
                VP8_EncAsicSetRegisterValue(val->regMirror, HEncCPDeltaQp7, val->deltaQp[6] & mask_4b);
            }
        }
    }

    /* Stream start offset */
    VP8_EncAsicSetRegisterValue(val->regMirror, HEncStartOffset, val->firstFreeBit);

    /* Stabilization */
    if(val->codingType != ASIC_JPEG)
    {
        VP8_EncAsicSetRegisterValue(val->regMirror, HEncBaseNextLum, val->vsNextLumaBase);
        VP8_EncAsicSetRegisterValue(val->regMirror, HEncStabMode, val->vsMode);
    }

    VP8_EncAsicSetRegisterValue(val->regMirror, HEncDMVPenalty4p, val->diffMvPenalty[0]);
    VP8_EncAsicSetRegisterValue(val->regMirror, HEncDMVPenalty1p, val->diffMvPenalty[1]);
    VP8_EncAsicSetRegisterValue(val->regMirror, HEncDMVPenaltyQp, val->diffMvPenalty[2]);

    VP8_EncAsicSetRegisterValue(val->regMirror, HEncBaseCabacCtx, val->cabacCtxBase);
    VP8_EncAsicSetRegisterValue(val->regMirror, HEncBaseMvWrite, val->mvOutputBase);

    VP8_EncAsicSetRegisterValue(val->regMirror, HEncRGBCoeffA,
                            val->colorConversionCoeffA & mask_16b);
    VP8_EncAsicSetRegisterValue(val->regMirror, HEncRGBCoeffB,
                            val->colorConversionCoeffB & mask_16b);
    VP8_EncAsicSetRegisterValue(val->regMirror, HEncRGBCoeffC,
                            val->colorConversionCoeffC & mask_16b);
    VP8_EncAsicSetRegisterValue(val->regMirror, HEncRGBCoeffE,
                            val->colorConversionCoeffE & mask_16b);
    VP8_EncAsicSetRegisterValue(val->regMirror, HEncRGBCoeffF,
                            val->colorConversionCoeffF & mask_16b);

    VP8_EncAsicSetRegisterValue(val->regMirror, HEncRMaskMSB, val->rMaskMsb & mask_5b);
    VP8_EncAsicSetRegisterValue(val->regMirror, HEncGMaskMSB, val->gMaskMsb & mask_5b);
    VP8_EncAsicSetRegisterValue(val->regMirror, HEncBMaskMSB, val->bMaskMsb & mask_5b);

    VP8_EncAsicSetRegisterValue(val->regMirror, HEncCirStart, val->cirStart);
    VP8_EncAsicSetRegisterValue(val->regMirror, HEncCirInterval, val->cirInterval);

    VP8_EncAsicSetRegisterValue(val->regMirror, HEncIntraAreaLeft, val->intraAreaLeft);
    VP8_EncAsicSetRegisterValue(val->regMirror, HEncIntraAreaRight, val->intraAreaRight);
    VP8_EncAsicSetRegisterValue(val->regMirror, HEncIntraAreaTop, val->intraAreaTop);
    VP8_EncAsicSetRegisterValue(val->regMirror, HEncIntraAreaBottom, val->intraAreaBottom);
    VP8_EncAsicSetRegisterValue(val->regMirror, HEncRoi1Left, val->roi1Left);
    VP8_EncAsicSetRegisterValue(val->regMirror, HEncRoi1Right, val->roi1Right);
    VP8_EncAsicSetRegisterValue(val->regMirror, HEncRoi1Top, val->roi1Top);
    VP8_EncAsicSetRegisterValue(val->regMirror, HEncRoi1Bottom, val->roi1Bottom);

    VP8_EncAsicSetRegisterValue(val->regMirror, HEncRoi2Left, val->roi2Left);
    VP8_EncAsicSetRegisterValue(val->regMirror, HEncRoi2Right, val->roi2Right);
    VP8_EncAsicSetRegisterValue(val->regMirror, HEncRoi2Top, val->roi2Top);
    VP8_EncAsicSetRegisterValue(val->regMirror, HEncRoi2Bottom, val->roi2Bottom);

    /* H.264 specific */
    if (val->codingType == ASIC_H264)
    {
        /* Limit ROI delta QPs so that ROI area QP is always >= 0.
         * ASIC doesn't check this. */
        VP8_EncAsicSetRegisterValue(val->regMirror, HEncRoi1DeltaQp, MIN(val->qp, val->roi1DeltaQp));
        VP8_EncAsicSetRegisterValue(val->regMirror, HEncRoi2DeltaQp, MIN(val->qp, val->roi2DeltaQp));

        VP8_EncAsicSetRegisterValue(val->regMirror, HEncIntraSliceMap1, val->intraSliceMap1);
        VP8_EncAsicSetRegisterValue(val->regMirror, HEncIntraSliceMap2, val->intraSliceMap2);
        VP8_EncAsicSetRegisterValue(val->regMirror, HEncIntraSliceMap3, val->intraSliceMap3);

        /* MVC */
        VP8_EncAsicSetRegisterValue(val->regMirror, HEncMvcAnchorPicFlag, val->mvcAnchorPicFlag);
        VP8_EncAsicSetRegisterValue(val->regMirror, HEncMvcPriorityId, val->mvcPriorityId);
        VP8_EncAsicSetRegisterValue(val->regMirror, HEncMvcViewId, val->mvcViewId);
        VP8_EncAsicSetRegisterValue(val->regMirror, HEncMvcTemporalId, val->mvcTemporalId);
        VP8_EncAsicSetRegisterValue(val->regMirror, HEncMvcInterViewFlag, val->mvcInterViewFlag);
    }

    /* VP8 specific */
    if (val->codingType == ASIC_VP8)
    {
        VP8_EncAsicSetRegisterValue(val->regMirror, HEncBasePartition1, val->partitionBase[0]);
        VP8_EncAsicSetRegisterValue(val->regMirror, HEncBasePartitirk, val->partitionBase[1]);
        VP8_EncAsicSetRegisterValue(val->regMirror, HEncBaseVp8ProbCount, val->probCountBase);

        VP8_EncAsicSetRegisterValue(val->regMirror, HEncVp8Mode0Penalty, val->intraModePenalty[0]);
        VP8_EncAsicSetRegisterValue(val->regMirror, HEncVp8Mode1Penalty, val->intraModePenalty[1]);
        VP8_EncAsicSetRegisterValue(val->regMirror, HEncVp8Mode2Penalty, val->intraModePenalty[2]);
        VP8_EncAsicSetRegisterValue(val->regMirror, HEncVp8Mode3Penalty, val->intraModePenalty[3]);

        VP8_EncAsicSetRegisterValue(val->regMirror, HEncVp8Bmode0Penalty, val->intraBmodePenalty[0]);
        VP8_EncAsicSetRegisterValue(val->regMirror, HEncVp8Bmode1Penalty, val->intraBmodePenalty[1]);
        VP8_EncAsicSetRegisterValue(val->regMirror, HEncVp8Bmode2Penalty, val->intraBmodePenalty[2]);
        VP8_EncAsicSetRegisterValue(val->regMirror, HEncVp8Bmode3Penalty, val->intraBmodePenalty[3]);
        VP8_EncAsicSetRegisterValue(val->regMirror, HEncVp8Bmode4Penalty, val->intraBmodePenalty[4]);
        VP8_EncAsicSetRegisterValue(val->regMirror, HEncVp8Bmode5Penalty, val->intraBmodePenalty[5]);
        VP8_EncAsicSetRegisterValue(val->regMirror, HEncVp8Bmode6Penalty, val->intraBmodePenalty[6]);
        VP8_EncAsicSetRegisterValue(val->regMirror, HEncVp8Bmode7Penalty, val->intraBmodePenalty[7]);
        VP8_EncAsicSetRegisterValue(val->regMirror, HEncVp8Bmode8Penalty, val->intraBmodePenalty[8]);
        VP8_EncAsicSetRegisterValue(val->regMirror, HEncVp8Bmode9Penalty, val->intraBmodePenalty[9]);

        VP8_EncAsicSetRegisterValue(val->regMirror, HEncVp8SegmentEnable, val->segmentEnable);
        VP8_EncAsicSetRegisterValue(val->regMirror, HEncVp8SegmentMapUpdate, val->segmentMapUpdate);
        VP8_EncAsicSetRegisterValue(val->regMirror, HEncBaseVp8SegmentMap, val->segmentMapBase);

        for (i = 0; i < 3; i++) {
            i32 off = (HEncVp8Seg2Y1QuantDc - HEncVp8Seg1Y1QuantDc) * i;
            VP8_EncAsicSetRegisterValue(val->regMirror, HEncVp8Seg1Y1QuantDc+off, val->qpY1QuantDc[i+1]);
            VP8_EncAsicSetRegisterValue(val->regMirror, HEncVp8Seg1Y1QuantAc+off, val->qpY1QuantAc[i+1]);
            VP8_EncAsicSetRegisterValue(val->regMirror, HEncVp8Seg1Y2QuantDc+off, val->qpY2QuantDc[i+1]);
            VP8_EncAsicSetRegisterValue(val->regMirror, HEncVp8Seg1Y2QuantAc+off, val->qpY2QuantAc[i+1]);
            VP8_EncAsicSetRegisterValue(val->regMirror, HEncVp8Seg1ChQuantDc+off, val->qpChQuantDc[i+1]);
            VP8_EncAsicSetRegisterValue(val->regMirror, HEncVp8Seg1ChQuantAc+off, val->qpChQuantAc[i+1]);

            VP8_EncAsicSetRegisterValue(val->regMirror, HEncVp8Seg1Y1ZbinDc+off, val->qpY1ZbinDc[i+1]);
            VP8_EncAsicSetRegisterValue(val->regMirror, HEncVp8Seg1Y1ZbinAc+off, val->qpY1ZbinAc[i+1]);
            VP8_EncAsicSetRegisterValue(val->regMirror, HEncVp8Seg1Y2ZbinDc+off, val->qpY2ZbinDc[i+1]);
            VP8_EncAsicSetRegisterValue(val->regMirror, HEncVp8Seg1Y2ZbinAc+off, val->qpY2ZbinAc[i+1]);
            VP8_EncAsicSetRegisterValue(val->regMirror, HEncVp8Seg1ChZbinDc+off, val->qpChZbinDc[i+1]);
            VP8_EncAsicSetRegisterValue(val->regMirror, HEncVp8Seg1ChZbinAc+off, val->qpChZbinAc[i+1]);

            VP8_EncAsicSetRegisterValue(val->regMirror, HEncVp8Seg1Y1RoundDc+off, val->qpY1RoundDc[i+1]);
            VP8_EncAsicSetRegisterValue(val->regMirror, HEncVp8Seg1Y1RoundAc+off, val->qpY1RoundAc[i+1]);
            VP8_EncAsicSetRegisterValue(val->regMirror, HEncVp8Seg1Y2RoundDc+off, val->qpY2RoundDc[i+1]);
            VP8_EncAsicSetRegisterValue(val->regMirror, HEncVp8Seg1Y2RoundAc+off, val->qpY2RoundAc[i+1]);
            VP8_EncAsicSetRegisterValue(val->regMirror, HEncVp8Seg1ChRoundDc+off, val->qpChRoundDc[i+1]);
            VP8_EncAsicSetRegisterValue(val->regMirror, HEncVp8Seg1ChRoundAc+off, val->qpChRoundAc[i+1]);

            VP8_EncAsicSetRegisterValue(val->regMirror, HEncVp8Seg1Y1DequantDc+off, val->qpY1DequantDc[i+1]);
            VP8_EncAsicSetRegisterValue(val->regMirror, HEncVp8Seg1Y1DequantAc+off, val->qpY1DequantAc[i+1]);
            VP8_EncAsicSetRegisterValue(val->regMirror, HEncVp8Seg1Y2DequantDc+off, val->qpY2DequantDc[i+1]);
            VP8_EncAsicSetRegisterValue(val->regMirror, HEncVp8Seg1Y2DequantAc+off, val->qpY2DequantAc[i+1]);
            VP8_EncAsicSetRegisterValue(val->regMirror, HEncVp8Seg1ChDequantDc+off, val->qpChDequantDc[i+1]);
            VP8_EncAsicSetRegisterValue(val->regMirror, HEncVp8Seg1ChDequantAc+off, val->qpChDequantAc[i+1]);

            VP8_EncAsicSetRegisterValue(val->regMirror, HEncVp8Seg1FilterLevel+off, val->filterLevel[i+1]);
        }

        VP8_EncAsicSetRegisterValue(val->regMirror, HEncVp8LfRefDelta0, val->lfRefDelta[0] & mask_7b);
        VP8_EncAsicSetRegisterValue(val->regMirror, HEncVp8LfRefDelta1, val->lfRefDelta[1] & mask_7b);
        VP8_EncAsicSetRegisterValue(val->regMirror, HEncVp8LfRefDelta2, val->lfRefDelta[2] & mask_7b);
        VP8_EncAsicSetRegisterValue(val->regMirror, HEncVp8LfRefDelta3, val->lfRefDelta[3] & mask_7b);
        VP8_EncAsicSetRegisterValue(val->regMirror, HEncVp8LfModeDelta0, val->lfModeDelta[0] & mask_7b);
        VP8_EncAsicSetRegisterValue(val->regMirror, HEncVp8LfModeDelta1, val->lfModeDelta[1] & mask_7b);
        VP8_EncAsicSetRegisterValue(val->regMirror, HEncVp8LfModeDelta2, val->lfModeDelta[2] & mask_7b);
        VP8_EncAsicSetRegisterValue(val->regMirror, HEncVp8LfModeDelta3, val->lfModeDelta[3] & mask_7b);
    }

#ifdef ASIC_WAVE_TRACE_TRIGGER
    if(val->vop_count++ == trigger_point)
    {
        /* logic analyzer triggered by writing to the ID reg */
        EWLWriteReg(ewl, 0x00, ~0);
    }
#endif

    /* Write regMirror to registers, 0x4 to 0xF8 */
    /*{
    	i32 i;
		
        for(i = 1; i <= 62; i++)
            EWLWriteReg(ewl, HSWREG(i), val->regMirror[i]);

		// swreg[64]=0x100 to swreg[95]=0x17C, VP8 regs 
		for(i = 64; i <= 95; i++)
			EWLWriteReg(ewl, HSWREG(i), val->regMirror[i]);
	}*/


	{
		i32 i = 0;
		
		/* Write DMV penalty tables to regs */
		for(i = 0; i < 128; i += 4)
		{
			/* swreg[96]=0x180 to swreg[127]=0x1FC */
			val->regMirror[96 + (i/4)] = 
						((val->dmvPenalty[i	  ] << 24) |
						(val->dmvPenalty[i + 1] << 16) |
						(val->dmvPenalty[i + 2] << 8) |
						(val->dmvPenalty[i + 3]));
		}
		for(i = 0; i < 128; i += 4)
		{
			/* swreg[128]=0x200 to swreg[159]=0x27C */
			val->regMirror[128 + (i/4)] = 
						((val->dmvQpelPenalty[i	  ] << 24) |
						(val->dmvQpelPenalty[i + 1] << 16) |
						(val->dmvQpelPenalty[i + 2] << 8) |
						(val->dmvQpelPenalty[i + 3]));
		}
	}

#ifdef TRACE_REGS
    EncTraceRegs(ewl, 0, 0);
#endif

    /* Register with enable bit is written last */
    val->regMirror[14] |= ASIC_STATUS_ENABLE;

    //EWLEnableHW(ewl, HSWREG(14), val->regMirror[14]);
    Vp8encFlushRegs(val);
	
}

void Vp8encFlushRegs(regValues_s * val)
{
    VPU_DEBUG("Vp8encFlushRegs\n");

	#if 0
	int i;
	for(i=0;i<ASIC_SWREG_AMOUNT;i++)
		ALOGE("val->regMirror[%d]=0x%x", i, val->regMirror[i]);
	#endif
	
    if (VPUClientSendReg(val->socket, val->regMirror, ASIC_SWREG_AMOUNT))
        VPU_DEBUG("Vp8encFlushRegs fail\n");
    else
        VPU_DEBUG("Vp8encFlushRegs success\n");
}

/*------------------------------------------------------------------------------
------------------------------------------------------------------------------*/
void VP8_EncAsicFrameContinue(const void *ewl, regValues_s * val)
{
    /* clear status bits, clear IRQ => HW restart */
    u32 status = val->regMirror[1];

    status &= (~ASIC_STATUS_ALL);
    status &= ~ASIC_IRQ_LINE;

    val->regMirror[1] = status;

    /*CheckRegisterValues(val); */

    /* Write only registers which may be updated mid frame */
    //EWLWriteReg(ewl, HSWREG(24), (val->rlcLimitSpace / 2));
    val->regMirror[24] = val->rlcLimitSpace / 2;
	
    val->regMirror[5] = val->rlcBase;
    //EWLWriteReg(ewl, HSWREG(5), val->regMirror[5]);

#ifdef TRACE_REGS
    EncTraceRegs(ewl, 0, EncAsicGetRegisterValue(ewl, val->regMirror, HEncMbCount));
#endif

    /* Register with status bits is written last */
    //EWLEnableHW(ewl, HSWREG(1), val->regMirror[1]);

}

/*------------------------------------------------------------------------------
------------------------------------------------------------------------------*/
void VP8_EncAsicGetRegisters(const void *ewl, regValues_s * val)
{

    /* HW output stream size, bits to bytes */
    val->outputStrmSize =
            VP8_EncAsicGetRegisterValue(ewl, val->regMirror, HEncStrmBufLimit) / 8;

    if(val->codingType != ASIC_JPEG && val->cpTarget != NULL)
    {
        /* video coding with MB rate control ON */
        u32 i;
        u32 cpt_prev = 0;
        u32 overflow = 0;

        for(i = 0; i < 10; i++)
        {
            u32 cpt;

            /* Checkpoint result div32 */
            cpt = VP8_EncAsicGetRegisterValue(ewl, val->regMirror,
                                    (regName)(HEncCP1WordTarget+i)) * 32;

            /* detect any overflow, overflow div32 is 65536 => overflow at 21 bits */
            if(cpt < cpt_prev)
                overflow += (1 << 21);
            cpt_prev = cpt;

            val->cpTargetResults[i] = cpt + overflow;
        }
    }

    /* QP sum div2 */
    val->qpSum = VP8_EncAsicGetRegisterValue(ewl, val->regMirror, HEncQpSum) * 2;

    /* MAD MB count*/
    val->madCount = VP8_EncAsicGetRegisterValue(ewl, val->regMirror, HEncMadCount);

    /* Non-zero coefficient count*/
    val->rlcCount = VP8_EncAsicGetRegisterValue(ewl, val->regMirror, HEncRlcSum) * 4;

    /* get stabilization results if needed */
    if(val->vsMode != 0)
    {
        i32 i;

        /*for(i = 40; i <= 50; i++)
        {
            val->regMirror[i] = EWLReadReg(ewl, HSWREG(i));
        }*/
    }

#ifdef TRACE_REGS
    EncTraceRegs(ewl, 1, EncAsicGetRegisterValue(ewl, val->regMirror, HEncMbCount));
#endif

}

/*------------------------------------------------------------------------------
------------------------------------------------------------------------------*/
void VP8_EncAsicStop(const void *ewl)
{
    //EWLDisableHW(ewl, HSWREG(14), 0);
}

/*------------------------------------------------------------------------------
------------------------------------------------------------------------------*/
u32 VP8_EncAsicGetStatus(regValues_s * val)
{
    return val->regMirror[1];//EWLReadReg(ewl, HSWREG(1));
}


