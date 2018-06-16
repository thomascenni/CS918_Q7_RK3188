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
--  Abstract : Hantro H1 VP8 Encoder API
--
------------------------------------------------------------------------------*/

#ifndef __VP8ENCAPI_H__
#define __VP8ENCAPI_H__

#include "basetype.h"
#include "ewl.h"

#ifdef __cplusplus
extern "C"
{
#endif

/* The maximum amount of frames for bitrate moving average calculation */
#define MOVING_AVERAGE_FRAMES    30

/*------------------------------------------------------------------------------
    1. Type definition for encoder instance
------------------------------------------------------------------------------*/

    typedef const void *VP8EncInst;

/*------------------------------------------------------------------------------
    2. Enumerations for API parameters
------------------------------------------------------------------------------*/

/* Function return values */
    typedef enum
    {
        VP8ENC_OK = 0,
        VP8ENC_FRAME_READY = 1,

        VP8ENC_ERROR = -1,
        VP8ENC_NULL_ARGUMENT = -2,
        VP8ENC_INVALID_ARGUMENT = -3,
        VP8ENC_MEMORY_ERROR = -4,
        VP8ENC_EWL_ERROR = -5,
        VP8ENC_EWL_MEMORY_ERROR = -6,
        VP8ENC_INVALID_STATUS = -7,
        VP8ENC_OUTPUT_BUFFER_OVERFLOW = -8,
        VP8ENC_HW_BUS_ERROR = -9,
        VP8ENC_HW_DATA_ERROR = -10,
        VP8ENC_HW_TIMEOUT = -11,
        VP8ENC_HW_RESERVED = -12,
        VP8ENC_SYSTEM_ERROR = -13,
        VP8ENC_INSTANCE_ERROR = -14,
        VP8ENC_HRD_ERROR = -15,
        VP8ENC_HW_RESET = -16
    } VP8EncRet;

/* Picture YUV type for pre-processing */
    typedef enum
    {
        VP8ENC_YUV420_PLANAR = 0,               /* YYYY... UUUU... VVVV */
        VP8ENC_YUV420_SEMIPLANAR = 1,           /* YYYY... UVUVUV...    */
        VP8ENC_YUV422_INTERLEAVED_YUYV = 2,     /* YUYVYUYV...          */
        VP8ENC_YUV422_INTERLEAVED_UYVY = 3,     /* UYVYUYVY...          */
        VP8ENC_RGB565 = 4,                      /* 16-bit RGB           */
        VP8ENC_BGR565 = 5,                      /* 16-bit RGB           */
        VP8ENC_RGB555 = 6,                      /* 15-bit RGB           */
        VP8ENC_BGR555 = 7,                      /* 15-bit RGB           */
        VP8ENC_RGB444 = 8,                      /* 12-bit RGB           */
        VP8ENC_BGR444 = 9,                      /* 12-bit RGB           */
        VP8ENC_RGB888 = 10,                     /* 24-bit RGB           */
        VP8ENC_BGR888 = 11,                     /* 24-bit RGB           */
        VP8ENC_RGB101010 = 12,                  /* 30-bit RGB           */
        VP8ENC_BGR101010 = 13                   /* 30-bit RGB           */
    } VP8EncPictureType;

/* Picture rotation for pre-processing */
    typedef enum
    {
        VP8ENC_ROTATE_0 = 0,
        VP8ENC_ROTATE_90R = 1, /* Rotate 90 degrees clockwise           */
        VP8ENC_ROTATE_90L = 2  /* Rotate 90 degrees counter-clockwise   */
    } VP8EncPictureRotation;

/* Picture color space conversion (RGB input) for pre-processing */
    typedef enum
    {
        VP8ENC_RGBTOYUV_BT601 = 0, /* Color conversion according to BT.601  */
        VP8ENC_RGBTOYUV_BT709 = 1, /* Color conversion according to BT.709  */
        VP8ENC_RGBTOYUV_USER_DEFINED = 2   /* User defined color conversion */
    } VP8EncColorConversionType;

/* Picture type for encoding */
    typedef enum
    {
        VP8ENC_INTRA_FRAME = 0,
        VP8ENC_PREDICTED_FRAME = 1,
        VP8ENC_NOTCODED_FRAME               /* Used just as a return value  */
    } VP8EncPictureCodingType;

/* Reference picture mode for reading and writing */
    typedef enum
    {
        VP8ENC_NO_REFERENCE_NO_REFRESH = 0,
        VP8ENC_REFERENCE = 1,
        VP8ENC_REFRESH = 2,
        VP8ENC_REFERENCE_AND_REFRESH = 3
    } VP8EncRefPictureMode;

/* Enumerations to enable encoder internal control of filter parameters */
    enum {
        VP8ENC_FILTER_LEVEL_AUTO = 64,
        VP8ENC_FILTER_SHARPNESS_AUTO = 8
    };

/*------------------------------------------------------------------------------
    3. Structures for API function parameters
------------------------------------------------------------------------------*/

/* Configuration info for initialization
 * Width and height are picture dimensions after rotation
 */
    typedef struct
    {
        u32 refFrameAmount; /* Amount of reference frame buffers, [1..3]
                             * 1 = only last frame buffered,
                             *     always predict from and refresh ipf,
                             *     stream buffer overflow causes new key frame
                             * 2 = last and golden frames buffered
                             * 3 = last and golden and altref frame buffered  */
        u32 width;          /* Encoded picture width in pixels, multiple of 4 */
        u32 height;         /* Encoded picture height in pixels, multiple of 2*/
        u32 frameRateNum;   /* The stream time scale, [1..1048575]            */
        u32 frameRateDenom; /* Maximum frame rate is frameRateNum/frameRateDenom
                             * in frames/second. [1..frameRateNum]            */
    } VP8EncConfig;

/* Defining rectangular macroblock area in encoded picture */
    typedef struct
    {
        u32 enable;         /* [0,1] Enables this area */
        u32 top;            /* Top mb row inside area [0..heightMbs-1]      */
        u32 left;           /* Left mb row inside area [0..widthMbs-1]      */
        u32 bottom;         /* Bottom mb row inside area [top..heightMbs-1] */
        u32 right;          /* Right mb row inside area [left..widthMbs-1]  */
    } VP8EncPictureArea;

/* Coding control parameters */
    typedef struct
    {
        /* The following parameters can only be adjusted before stream is
         * started. They affect the stream profile and decoding complexity. */
        u32 interpolationFilter;/* Defines the type of interpolation filter
                                 * for reconstruction. [0..2]
                                 * 0 = Bicubic (6-tap interpolation filter),
                                 * 1 = Bilinear (2-tap interpolation filter),
                                 * 2 = None (Full pixel motion vectors)     */
        u32 filterType;         /* Deblocking loop filter type,
                                 * 0 = Normal deblocking filter,
                                 * 1 = Simple deblocking filter             */

        /* The following parameters can be adjusted between frames.         */
        u32 filterLevel;        /* Deblocking filter level [0..64]
                                 * 0 = No filtering,
                                 * higher level => more filtering,
                                 * VP8ENC_FILTER_LEVEL_AUTO = calculate
                                 *      filter level based on quantization  */
        u32 filterSharpness;    /* Deblocking filter sharpness [0..8],
                                 * VP8ENC_FILTER_SHARPNESS_AUTO = calculate
                                 *      filter sharpness automatically      */
        u32 dctPartitions;      /* Amount of DCT coefficient partitions to
                                 * create for each frame, subject to HW
                                 * limitations on maximum partition amount.
                                 * 0 = 1 DCT residual partition,
                                 * 1 = 2 DCT residual partitions,
                                 * 2 = 4 DCT residual partitions,
                                 * 3 = 8 DCT residual partitions            */
        u32 errorResilient;     /* Enable error resilient stream mode. [0,1]
                                 * This prevents cumulative probability
                                 * updates.                                 */
        u32 splitMv;            /* Split MV mode ie. using more than 1 MV/MB
                                 * 0=disabled, 1=adaptive, 2=enabled        */
        u32 quarterPixelMv;     /* 1/4 pixel motion estimation
                                 * 0=disabled, 1=adaptive, 2=enabled        */
        u32 cirStart;           /* [0..mbTotal] First macroblock for
                                 * Cyclic Intra Refresh                     */
        u32 cirInterval;        /* [0..mbTotal] Macroblock interval for
                                 * Cyclic Intra Refresh, 0=disabled         */
        VP8EncPictureArea intraArea;  /* Area for forcing intra macroblocks */
        VP8EncPictureArea roi1Area;   /* Area for 1st Region-Of-Interest */
        VP8EncPictureArea roi2Area;   /* Area for 2nd Region-Of-Interest */
        i32 roi1DeltaQp;              /* [-50..0] QP delta value for 1st ROI */
        i32 roi2DeltaQp;              /* [-50..0] QP delta value for 2nd ROI */
    } VP8EncCodingCtrl;

/* Rate control parameters */
    typedef struct
    {
        u32 pictureRc;       /* Adjust QP between pictures, [0,1]           */
        u32 pictureSkip;     /* Allow rate control to skip pictures, [0,1]  */
        i32 qpHdr;           /* QP for next encoded picture, [-1..127]
                              * -1 = Let rate control calculate initial QP.
                              * qpHdr is used for all pictures if
                              * pictureRc is disabled.                      */
        u32 qpMin;           /* Minimum QP for any picture, [0..127]        */
        u32 qpMax;           /* Maximum QP for any picture, [0..127]        */
        u32 bitPerSecond;    /* Target bitrate in bits/second
                              * [10000..60000000]                           */
        u32 bitrateWindow;   /* Number of pictures over which the target
                              * bitrate should be achieved. Smaller window
                              * maintains constant bitrate but forces rapid
                              * quality changes whereas larger window
                              * allows smoother quality changes. [1..150]   */
        i32 intraQpDelta;    /* Intra QP delta. intraQP = QP + intraQpDelta
                              * This can be used to change the relative
                              * quality of the Intra pictures or to decrease
                              * the size of Intra pictures. [-12..12]       */
        u32 fixedIntraQp;    /* Fixed QP value for all Intra pictures, [0..127]
                              * 0 = Rate control calculates intra QP.       */
        u32 intraPictureRate;/* The distance of two intra pictures, [0..300]
                              * This will force periodical intra pictures.
                              * 0=disabled.                                 */
        u32 goldenPictureRate;/* The distance of two golden pictures, [0..300]
                              * This will force periodical golden pictures.
                              * 0=disabled.                                 */
        u32 altrefPictureRate;/* The distance of two altref pictures, [0..300]
                              * This will force periodical altref pictures.
                              * 0=disabled.                                 */
		u32	outRateNum;	/*framerate*/
		u32	outRateDenom;	/*=1*/                              
    } VP8EncRateCtrl;

/* Encoder input structure */
    typedef struct
    {
        u32 busLuma;         /* Bus address for input picture
                              * planar format: luminance component
                              * semiplanar format: luminance component
                              * interleaved format: whole picture
                              */
        u32 busChromaU;      /* Bus address for input chrominance
                              * planar format: cb component
                              * semiplanar format: both chrominance
                              * interleaved format: not used
                              */
        u32 busChromaV;      /* Bus address for input chrominance
                              * planar format: cr component
                              * semiplanar format: not used
                              * interleaved format: not used
                              */
        u32 *pOutBuf;        /* Pointer to output stream buffer             */
        u32 busOutBuf;       /* Bus address of output stream buffer         */
        u32 outBufSize;      /* Size of output stream buffer in bytes       */
        VP8EncPictureCodingType codingType; /* Proposed picture coding type,
                                             * INTRA/PREDICTED              */
        u32 timeIncrement;   /* The previous picture duration in units
                              * of 1/frameRateNum. 0 for the very first
                              * picture and typically equal to frameRateDenom
                              * for the rest.                               */
        u32 busLumaStab;     /* Bus address of next picture to stabilize,
                              * optional (luminance)                        */

                             /* The following three parameters apply when
                              * codingType == PREDICTED. They define for each
                              * of the reference pictures if it should be used
                              * for prediction and if it should be refreshed
                              * with the encoded frame. There must always be
                              * atleast one (ipf/grf/arf) that is referenced
                              * and atleast one that is refreshed.
                              * Note that refFrameAmount may limit the
                              * availability of golden and altref frames.   */
        VP8EncRefPictureMode ipf; /* Immediately previous == last frame */
        VP8EncRefPictureMode grf; /* Golden reference frame */
        VP8EncRefPictureMode arf; /* Alternative reference frame */
    } VP8EncIn;

/* Encoder output structure */
    typedef struct
    {
        VP8EncPictureCodingType codingType;     /* Realized picture coding type,
                                                 * INTRA/PREDICTED/NOTCODED   */
        u32 *pOutBuf[9];     /* Pointers to start of each partition in
                              * output stream buffer,
                              * pOutBuf[0] = Frame header + mb mode partition,
                              * pOutBuf[1] = First DCT partition,
                              * pOutBuf[2] = Second DCT partition (if exists)
                              * etc.                                          */
        u32 streamSize[9];   /* Size of each partition of output stream
                              * in bytes.                                     */
        u32 frameSize;       /* Size of output frame in bytes
                              * (==sum of partition sizes)                    */
        i8 *motionVectors;   /* Pointer to buffer storing encoded frame MVs.
                              * One pixel motion vector x and y and
                              * corresponding SAD value for every macroblock.
                              * Format: mb0x mb0y mb0sadMsb mb0sadLsb mb1x .. */

                             /* The following three parameters apply when
                              * codingType == PREDICTED. They define for each
                              * of the reference pictures if it was used for
                              * prediction and it it was refreshed. */
        VP8EncRefPictureMode ipf; /* Immediately previous == last frame */
        VP8EncRefPictureMode grf; /* Golden reference frame */
        VP8EncRefPictureMode arf; /* Alternative reference frame */
    } VP8EncOut;

/* Input pre-processing */
    typedef struct
    {
        VP8EncColorConversionType type;
        u16 coeffA;          /* User defined color conversion coefficient */
        u16 coeffB;          /* User defined color conversion coefficient */
        u16 coeffC;          /* User defined color conversion coefficient */
        u16 coeffE;          /* User defined color conversion coefficient */
        u16 coeffF;          /* User defined color conversion coefficient */
    } VP8EncColorConversion;

    typedef struct
    {
        u32 origWidth;
        u32 origHeight;
        u32 xOffset;
        u32 yOffset;
        VP8EncPictureType inputType;
        VP8EncPictureRotation rotation;
        u32 videoStabilization;
        VP8EncColorConversion colorConversion;
    } VP8EncPreProcessingCfg;

/* Version information */
    typedef struct
    {
        u32 major;           /* Encoder API major version */
        u32 minor;           /* Encoder API minor version */
    } VP8EncApiVersion;

    typedef struct
    {
        u32 swBuild;         /* Software build ID */
        u32 hwBuild;         /* Hardware build ID */
    } VP8EncBuild;


typedef struct {
    i32 frame[MOVING_AVERAGE_FRAMES];
    i32 length;
    i32 count;
    i32 pos;
    i32 frameRateNumer;
    i32 frameRateDenom;
} ma_s;

/* Structure for command line options and testbench variables */
typedef struct
{
    char *input;
    char *output;
    i32 firstPic;
    i32 lastPic;
    i32 width;
    i32 height;
    i32 lumWidthSrc;
    i32 lumHeightSrc;
    i32 horOffsetSrc;
    i32 verOffsetSrc;
    i32 outputRateNumer;
    i32 outputRateDenom;
    i32 inputRateNumer;
    i32 inputRateDenom;
    i32 refFrameAmount;

    i32 inputFormat;
    i32 rotation;
    i32 colorConversion;
    i32 videoStab;

    i32 bitPerSecond;
    i32 picRc;
    i32 picSkip;
    i32 gopLength;
    i32 qpHdr;
    i32 qpMin;
    i32 qpMax;
    i32 intraQpDelta;
    i32 fixedIntraQp;

    i32 intraPicRate;
    i32 dctPartitions;  /* Dct data partitions 0=1, 1=2, 2=4, 3=8 */
    i32 errorResilient;
    i32 ipolFilter;
    i32 quarterPixelMv;
    i32 splitMv;
    i32 filterType;
    i32 filterLevel;
    i32 filterSharpness;
    i32 cirStart;
    i32 cirInterval;
    i32 intraAreaEnable;
    i32 intraAreaLeft;
    i32 intraAreaTop;
    i32 intraAreaRight;
    i32 intraAreaBottom;
    i32 roi1AreaEnable;
    i32 roi2AreaEnable;
    i32 roi1AreaTop;
    i32 roi1AreaLeft;
    i32 roi1AreaBottom;
    i32 roi1AreaRight;
    i32 roi2AreaTop;
    i32 roi2AreaLeft;
    i32 roi2AreaBottom;
    i32 roi2AreaRight;
    i32 roi1DeltaQp;
    i32 roi2DeltaQp;

    i32 printPsnr;
    i32 mvOutput;
    i32 testId;
    i32 droppable;

    i32 intra16Favor;
    i32 intraPenalty;
    i32 traceRecon;

    /* SW/HW shared memories for input/output buffers */
    VPUMemLinear_t pictureMem;
    VPUMemLinear_t pictureStabMem;
    VPUMemLinear_t outbufMem;

    FILE *yuvFile;
    char *yuvFileName;
    off_t file_size;

    VP8EncRateCtrl rc;
    VP8EncOut encOut;

    u32 streamSize;     /* Size of output stream in bytes */
    u32 bitrate;        /* Calculate average bitrate of encoded frames */
    ma_s ma;            /* Calculate moving average of bitrate */
    u32 psnrSum;        /* Calculate average PSNR over encoded frames */
    u32 psnrCnt;

    i32 frameCnt;       /* Frame counter of input file */
    u64 frameCntTotal;  /* Frame counter of all frames */
} testbench_s;


/*------------------------------------------------------------------------------
    4. Encoder API function prototypes
------------------------------------------------------------------------------*/

/* Version information */
    VP8EncApiVersion VP8EncGetApiVersion(void);
    VP8EncBuild VP8EncGetBuild(void);

/* Initialization & release */
    VP8EncRet VP8EncInit(const VP8EncConfig * pEncConfig,
                           VP8EncInst * instAddr);
    VP8EncRet VP8EncRelease(VP8EncInst inst);

/* Encoder configuration before stream generation */
    VP8EncRet VP8EncSetCodingCtrl(VP8EncInst inst, const VP8EncCodingCtrl *
                                    pCodingParams);
    VP8EncRet VP8EncGetCodingCtrl(VP8EncInst inst, VP8EncCodingCtrl *
                                    pCodingParams);

/* Encoder configuration before and during stream generation */
    VP8EncRet VP8EncSetRateCtrl(VP8EncInst inst,
                                  const VP8EncRateCtrl * pRateCtrl);
    VP8EncRet VP8EncGetRateCtrl(VP8EncInst inst,
                                  VP8EncRateCtrl * pRateCtrl);

    VP8EncRet VP8EncSetPreProcessing(VP8EncInst inst,
                                       const VP8EncPreProcessingCfg *
                                       pPreProcCfg);
    VP8EncRet VP8EncGetPreProcessing(VP8EncInst inst,
                                       VP8EncPreProcessingCfg * pPreProcCfg);

/* Stream generation */

/* VP8EncStrmEncode encodes one video frame.
 */
    VP8EncRet VP8EncStrmEncode(VP8EncInst inst, const VP8EncIn * pEncIn,
                                 VP8EncOut * pEncOut, testbench_s *cml);

/* Hantro internal encoder testing */
    VP8EncRet VP8EncSetTestId(VP8EncInst inst, u32 testId);

/*------------------------------------------------------------------------------
    5. Encoder API tracing callback function
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
	4. Local function prototypes
------------------------------------------------------------------------------*/
//static int AllocRes(testbench_s * tb, VP8EncInst enc);
//static void FreeRes(testbench_s * tb, VP8EncInst enc);
    void VP8EncTrace(const char *msg);

	int OpenEncoder(testbench_s * tb, VP8EncInst * encoder);
	i32 Encode(VP8EncInst inst, testbench_s *tb);
	void CloseEncoder(VP8EncInst encoder);

	int Parameter(testbench_s * tb);
	i32 NextPic(i32 inputRateNumer, i32 inputRateDenom, i32 outputRateNumer,
	                i32 outputRateDenom, i32 frameCnt, i32 firstPic);
	int ReadPic(testbench_s * cml, u8 * image, i32 size, i32 nro);
	// int ChangeInput(i32 argc, char **argv, char **name, option_s * option);
	void IvfHeader(testbench_s *tb, u8 *out);
	void IvfFrame(testbench_s *tb, u8 *out);
	void WriteStrm(FILE * fout, u32 * outbuf, u32 size, u32 endian);
	void WriteMotionVectors(FILE *file, i8 *mvs, i32 frame, i32 width, i32 height);

	u32 GetResolution(char *filename, i32 *pWidth, i32 *pHeight);
	void PrintTitle(testbench_s *cml);
	void PrintFrame(testbench_s *cml, VP8EncInst encoder, u32 frameNumber,
	    VP8EncRet ret);
	void PrintErrorValue(const char *errorDesc, u32 retVal);
	u32 PrintPSNR(u8 *a, u8 *b, i32 scanline, i32 wdh, i32 hgt, i32 rotation);

	void MaAddFrame(ma_s *ma, i32 frameSizeBits);
	i32 Ma(ma_s *ma);

#ifdef __cplusplus
}
#endif

#endif /*__VP8ENCAPI_H__*/
