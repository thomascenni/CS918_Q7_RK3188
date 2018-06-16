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
--  Abstract : VP8 Encoder API
--
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
       Version Information
------------------------------------------------------------------------------*/

#define VP8ENC_MAJOR_VERSION 0
#define VP8ENC_MINOR_VERSION 4

#define VP8ENC_BUILD_MAJOR 0
#define VP8ENC_BUILD_MINOR 13
#define VP8ENC_BUILD_REVISION 0
#define VP8ENC_SW_BUILD ((VP8ENC_BUILD_MAJOR * 1000000) + \
(VP8ENC_BUILD_MINOR * 1000) + VP8ENC_BUILD_REVISION)

/*------------------------------------------------------------------------------
    1. Include headers
------------------------------------------------------------------------------*/
#include "vp8encapi.h"
#include "enccommon.h"
#include "vp8instance.h"
#include "vp8init.h"
#include "vp8codeframe.h"
#include "vp8ratecontrol.h"
#include "vp8putbits.h"

#ifdef INTERNAL_TEST
#include "vp8testid.h"
#endif

#ifdef VIDEOSTAB_ENABLED
#include "vidstabcommon.h"
#endif

/*------------------------------------------------------------------------------
    2. External compiler flags
--------------------------------------------------------------------------------

#define EVALUATION_LIMIT 300    max number of frames to encode

--------------------------------------------------------------------------------
    3. Module defines
------------------------------------------------------------------------------*/

/* Parameter limits */
#define VP8ENCSTRMENCODE_MIN_BUF       4096
#define VP8ENC_MAX_PP_INPUT_WIDTH      8176
#define VP8ENC_MAX_PP_INPUT_HEIGHT     8176
#define VP8ENC_MAX_BITRATE             (50000*1200)

#define VP8_BUS_ADDRESS_VALID(bus_address)  (((bus_address) != 0) && \
                                              ((bus_address & 0x07) == 0))

/* HW ID check. VP8EncInit() will fail if HW doesn't match. */
#define HW_ID_MASK  0xFFFF0000
#define HW_ID       0x48310000

/* Tracing macro */
#ifdef VP8ENC_TRACE
#define APITRACE(str) VP8EncTrace(str)
#else
#define APITRACE(str)
#endif

/*------------------------------------------------------------------------------
    4. Local function prototypes
------------------------------------------------------------------------------*/

static i32 VSCheckSize(u32 inputWidth, u32 inputHeight, u32 stabilizedWidth,
                       u32 stabilizedHeight);

/*------------------------------------------------------------------------------

    Function name : VP8EncGetApiVersion
    Description   : Return the API version info

    Return type   : VP8EncApiVersion
    Argument      : void
------------------------------------------------------------------------------*/
VP8EncApiVersion VP8EncGetApiVersion(void)
{
    VP8EncApiVersion ver;

    ver.major = VP8ENC_MAJOR_VERSION;
    ver.minor = VP8ENC_MINOR_VERSION;

    APITRACE("VP8EncGetApiVersion# OK");
    return ver;
}

/*------------------------------------------------------------------------------
    Function name : VP8EncGetBuild
    Description   : Return the SW and HW build information

    Return type   : VP8EncBuild
    Argument      : void
------------------------------------------------------------------------------*/
VP8EncBuild VP8EncGetBuild(void)
{
    VP8EncBuild ver;

    ver.swBuild = VP8ENC_SW_BUILD;
    ver.hwBuild = 0;//EWLReadAsicID();

    APITRACE("VP8EncGetBuild# OK");

    return (ver);
}

/*------------------------------------------------------------------------------
    Function name : VP8EncInit
    Description   : Initialize an encoder instance and returns it to application

    Return type   : VP8EncRet
    Argument      : pEncCfg - initialization parameters
                    instAddr - where to save the created instance
------------------------------------------------------------------------------*/
VP8EncRet VP8EncInit(const VP8EncConfig * pEncCfg, VP8EncInst * instAddr)
{
    VP8EncRet ret;
    vp8Instance_s *pEncInst = NULL;

    APITRACE("VP8EncInit#");

    /* check that right shift on negative numbers is performed signed */
    /*lint -save -e* following check causes multiple lint messages */
#if (((-1) >> 1) != (-1))
#error Right bit-shifting (>>) does not preserve the sign
#endif
    /*lint -restore */

    /* Check for illegal inputs */
    if(pEncCfg == NULL || instAddr == NULL)
    {
        APITRACE("VP8EncInit: ERROR Null argument");
        return VP8ENC_NULL_ARGUMENT;
    }

    /* Check for correct HW */
    /*if ((EWLReadAsicID() & HW_ID_MASK) != HW_ID)
    {
        APITRACE("VP8EncInit: ERROR Invalid HW ID");
        return VP8ENC_ERROR;
    }*/

    /* Check that configuration is valid */
    if(VP8CheckCfg(pEncCfg) == ENCHW_NOK)
    {
        APITRACE("VP8EncInit: ERROR Invalid configuration");
        return VP8ENC_INVALID_ARGUMENT;
    }

    /* Initialize encoder instance and allocate memories */
    ret = VP8Init(pEncCfg, &pEncInst);

    if(ret != VP8ENC_OK)
    {
        APITRACE("VP8EncInit: ERROR Initialization failed");
        return ret;
    }

    /* Status == INIT   Initialization succesful */
    pEncInst->encStatus = VP8ENCSTAT_INIT;

    pEncInst->inst = pEncInst;  /* used as checksum */

    *instAddr = (VP8EncInst) pEncInst;

	//get socket
	pEncInst->asic.regs.socket = VPUClientInit(VPU_ENC);

    APITRACE("VP8EncInit: OK");
    return VP8ENC_OK;
}

/*------------------------------------------------------------------------------

    Function name : VP8EncRelease
    Description   : Releases encoder instance and all associated resource

    Return type   : VP8EncRet
    Argument      : inst - the instance to be released
------------------------------------------------------------------------------*/
VP8EncRet VP8EncRelease(VP8EncInst inst)
{
    vp8Instance_s *pEncInst = (vp8Instance_s *) inst;

    APITRACE("VP8EncRelease#");

    /* Check for illegal inputs */
    if(pEncInst == NULL)
    {
        APITRACE("VP8EncRelease: ERROR Null argument");
        return VP8ENC_NULL_ARGUMENT;
    }

    /* Check for existing instance */
    if(pEncInst->inst != pEncInst)
    {
        APITRACE("VP8EncRelease: ERROR Invalid instance");
        return VP8ENC_INSTANCE_ERROR;
    }

#ifdef TRACE_STREAM
    EncCloseStreamTrace();
#endif

    VPUClientRelease(pEncInst->asic.regs.socket);

    VP8Shutdown(pEncInst);

    APITRACE("VP8EncRelease: OK");
    return VP8ENC_OK;
}

/*------------------------------------------------------------------------------

    Function name : VP8EncSetCodingCtrl
    Description   : Sets encoding parameters

    Return type   : VP8EncRet
    Argument      : inst - the instance in use
                    pCodeParams - user provided parameters
------------------------------------------------------------------------------*/
VP8EncRet VP8EncSetCodingCtrl(VP8EncInst inst,
                                const VP8EncCodingCtrl * pCodeParams)
{
    vp8Instance_s *pEncInst = (vp8Instance_s *) inst;
    regValues_s *regs;
    sps *sps;
    u32 area1 = 0, area2 = 0;

    APITRACE("VP8EncSetCodingCtrl#");

    /* Check for illegal inputs */
    if((pEncInst == NULL) || (pCodeParams == NULL))
    {
        APITRACE("VP8EncSetCodingCtrl: ERROR Null argument");
        return VP8ENC_NULL_ARGUMENT;
    }

    /* Check for existing instance */
    if(pEncInst->inst != pEncInst)
    {
        APITRACE("VP8EncSetCodingCtrl: ERROR Invalid instance");
        return VP8ENC_INSTANCE_ERROR;
    }

    /* check limits */
    if (pCodeParams->filterLevel > VP8ENC_FILTER_LEVEL_AUTO ||
        pCodeParams->filterSharpness > VP8ENC_FILTER_SHARPNESS_AUTO)
    {
        APITRACE("VP8EncSetCodingCtrl: ERROR Invalid parameter");
        return VP8ENC_INVALID_ARGUMENT;
    }

    if(pCodeParams->cirStart > pEncInst->mbPerFrame ||
       pCodeParams->cirInterval > pEncInst->mbPerFrame)
    {
        APITRACE("VP8EncSetCodingCtrl: ERROR Invalid CIR value");
        return VP8ENC_INVALID_ARGUMENT;
    }

    if(pCodeParams->intraArea.enable) {
        if(!(pCodeParams->intraArea.top <= pCodeParams->intraArea.bottom &&
           pCodeParams->intraArea.bottom < pEncInst->mbPerCol &&
           pCodeParams->intraArea.left <= pCodeParams->intraArea.right &&
           pCodeParams->intraArea.right < pEncInst->mbPerRow))
        {
            APITRACE("VP8EncSetCodingCtrl: ERROR Invalid intraArea");
            return VP8ENC_INVALID_ARGUMENT;
        }
    }

    if(pCodeParams->roi1Area.enable) {
        if(!(pCodeParams->roi1Area.top <= pCodeParams->roi1Area.bottom &&
           pCodeParams->roi1Area.bottom < pEncInst->mbPerCol &&
           pCodeParams->roi1Area.left <= pCodeParams->roi1Area.right &&
           pCodeParams->roi1Area.right < pEncInst->mbPerRow))
        {
            APITRACE("VP8EncSetCodingCtrl: ERROR Invalid roi1Area");
            return VP8ENC_INVALID_ARGUMENT;
        }
        area1 = (pCodeParams->roi1Area.right + 1 - pCodeParams->roi1Area.left)*
                (pCodeParams->roi1Area.bottom + 1 - pCodeParams->roi1Area.top);
    }

    if(pCodeParams->roi2Area.enable) {
        if (!pCodeParams->roi1Area.enable) {
            APITRACE("VP8EncSetCodingCtrl: ERROR Roi2 enabled but not Roi1");
            return VP8ENC_INVALID_ARGUMENT;
        }
        if(!(pCodeParams->roi2Area.top <= pCodeParams->roi2Area.bottom &&
           pCodeParams->roi2Area.bottom < pEncInst->mbPerCol &&
           pCodeParams->roi2Area.left <= pCodeParams->roi2Area.right &&
           pCodeParams->roi2Area.right < pEncInst->mbPerRow))
        {
            APITRACE("VP8EncSetCodingCtrl: ERROR Invalid roi2Area");
            return VP8ENC_INVALID_ARGUMENT;
        }
        area2 = (pCodeParams->roi2Area.right + 1 - pCodeParams->roi2Area.left)*
                (pCodeParams->roi2Area.bottom + 1 - pCodeParams->roi2Area.top);
    }

    if (area1 + area2 >= pEncInst->mbPerFrame) {
        APITRACE("VP8EncSetCodingCtrl: ERROR Invalid roi (whole frame)");
        return VP8ENC_INVALID_ARGUMENT;
    }

    if(pCodeParams->roi1DeltaQp < -50 ||
       pCodeParams->roi1DeltaQp > 0 ||
       pCodeParams->roi2DeltaQp < -50 ||
       pCodeParams->roi2DeltaQp > 0)
    {
        APITRACE("VP8EncSetCodingCtrl: ERROR Invalid ROI delta QP");
        return VP8ENC_INVALID_ARGUMENT;
    }

    sps = &pEncInst->sps;

    /* TODO check limits */
    sps->filterType        = pCodeParams->filterType;
    if (pCodeParams->filterLevel == VP8ENC_FILTER_LEVEL_AUTO) {
        sps->autoFilterLevel = 1;
        sps->filterLevel     = 0;
    } else {
        sps->autoFilterLevel = 0;
        sps->filterLevel     = pCodeParams->filterLevel;
    }

    if (pCodeParams->filterSharpness == VP8ENC_FILTER_SHARPNESS_AUTO) {
        sps->autoFilterSharpness = 1;
        sps->filterSharpness     = 0;
    } else {
        sps->autoFilterSharpness = 0;
        sps->filterSharpness     = pCodeParams->filterSharpness;
    }

    sps->dctPartitions     = pCodeParams->dctPartitions;
    sps->partitionCnt      = 2 + (1 << sps->dctPartitions);
    sps->refreshEntropy    = pCodeParams->errorResilient ? 0 : 1;
    sps->quarterPixelMv    = pCodeParams->quarterPixelMv;
    sps->splitMv           = pCodeParams->splitMv;

    regs = &pEncInst->asic.regs;
    regs->cirStart = pCodeParams->cirStart;
    regs->cirInterval = pCodeParams->cirInterval;
    if (pCodeParams->intraArea.enable) {
        regs->intraAreaTop = pCodeParams->intraArea.top;
        regs->intraAreaLeft = pCodeParams->intraArea.left;
        regs->intraAreaBottom = pCodeParams->intraArea.bottom;
        regs->intraAreaRight = pCodeParams->intraArea.right;
    }
    if (pCodeParams->roi1Area.enable) {
        regs->roi1Top = pCodeParams->roi1Area.top;
        regs->roi1Left = pCodeParams->roi1Area.left;
        regs->roi1Bottom = pCodeParams->roi1Area.bottom;
        regs->roi1Right = pCodeParams->roi1Area.right;
    }
    if (pCodeParams->roi2Area.enable) {
        regs->roi2Top = pCodeParams->roi2Area.top;
        regs->roi2Left = pCodeParams->roi2Area.left;
        regs->roi2Bottom = pCodeParams->roi2Area.bottom;
        regs->roi2Right = pCodeParams->roi2Area.right;
    }

    /* ROI setting updates the segmentation map usage */
    if (pCodeParams->roi1Area.enable || pCodeParams->roi2Area.enable)
        pEncInst->ppss.pps->segmentEnabled = 1;
    else {
        pEncInst->ppss.pps->segmentEnabled = 0;
        /* Disabling ROI will clear the segment ID map */
        memset(pEncInst->asic.segmentMap.vir_addr, 0,
                  pEncInst->asic.segmentMap.size);
    }
    pEncInst->ppss.pps->sgm.mapModified = true;

    regs->roi1DeltaQp = -pCodeParams->roi1DeltaQp;
    regs->roi2DeltaQp = -pCodeParams->roi2DeltaQp;

    APITRACE("VP8EncSetCodingCtrl: OK");
    return VP8ENC_OK;
}

/*------------------------------------------------------------------------------

    Function name : VP8EncGetCodingCtrl
    Description   : Returns current encoding parameters

    Return type   : VP8EncRet
    Argument      : inst - the instance in use
                    pCodeParams - palce where parameters are returned
------------------------------------------------------------------------------*/
VP8EncRet VP8EncGetCodingCtrl(VP8EncInst inst,
                                VP8EncCodingCtrl * pCodeParams)
{
    vp8Instance_s *pEncInst = (vp8Instance_s *) inst;
    regValues_s *regs;
    sps *sps;

    APITRACE("VP8EncGetCodingCtrl#");

    /* Check for illegal inputs */
    if((pEncInst == NULL) || (pCodeParams == NULL))
    {
        APITRACE("VP8EncGetCodingCtrl: ERROR Null argument");
        return VP8ENC_NULL_ARGUMENT;
    }

    /* Check for existing instance */
    if(pEncInst->inst != pEncInst)
    {
        APITRACE("VP8EncGetCodingCtrl: ERROR Invalid instance");
        return VP8ENC_INSTANCE_ERROR;
    }

    sps = &pEncInst->sps;

    pCodeParams->interpolationFilter    = 1;    /* Only bicubic supported */
    pCodeParams->dctPartitions          = sps->dctPartitions;
    pCodeParams->quarterPixelMv         = sps->quarterPixelMv;
    pCodeParams->splitMv                = sps->splitMv;
    pCodeParams->filterType             = sps->filterType;
    pCodeParams->errorResilient         = !sps->refreshEntropy;

    if (sps->autoFilterLevel)
        pCodeParams->filterLevel            = VP8ENC_FILTER_LEVEL_AUTO;
    else
        pCodeParams->filterLevel            = sps->filterLevel;

    if (sps->autoFilterSharpness)
        pCodeParams->filterSharpness        = VP8ENC_FILTER_SHARPNESS_AUTO;
    else
        pCodeParams->filterSharpness        = sps->filterSharpness;

    regs = &pEncInst->asic.regs;
    pCodeParams->cirStart = regs->cirStart;
    pCodeParams->cirInterval = regs->cirInterval;
    pCodeParams->intraArea.enable = regs->intraAreaTop < pEncInst->mbPerCol ? 1 : 0;
    pCodeParams->intraArea.top    = regs->intraAreaTop;
    pCodeParams->intraArea.left   = regs->intraAreaLeft;
    pCodeParams->intraArea.bottom = regs->intraAreaBottom;
    pCodeParams->intraArea.right  = regs->intraAreaRight;

    APITRACE("VP8EncGetCodingCtrl: OK");
    return VP8ENC_OK;
}

/*------------------------------------------------------------------------------

    Function name : VP8EncSetRateCtrl
    Description   : Sets rate control parameters

    Return type   : VP8EncRet
    Argument      : inst - the instance in use
                    pRateCtrl - user provided parameters
------------------------------------------------------------------------------*/
VP8EncRet VP8EncSetRateCtrl(VP8EncInst inst,
                              const VP8EncRateCtrl * pRateCtrl)
{
    vp8Instance_s *pEncInst = (vp8Instance_s *) inst;
    vp8RateControl_s rc_tmp;

    APITRACE("VP8EncSetRateCtrl#");

    /* Check for illegal inputs */
    if((pEncInst == NULL) || (pRateCtrl == NULL))
    {
        APITRACE("VP8EncSetRateCtrl: ERROR Null argument");
        return VP8ENC_NULL_ARGUMENT;
    }

    /* Check for existing instance */
    if(pEncInst->inst != pEncInst)
    {
        APITRACE("VP8EncSetRateCtrl: ERROR Invalid instance");
        return VP8ENC_INSTANCE_ERROR;
    }

    /* TODO Check for invalid values */

    rc_tmp = pEncInst->rateControl;
    rc_tmp.qpHdr                    = pRateCtrl->qpHdr;
    rc_tmp.picRc                    = pRateCtrl->pictureRc;
    rc_tmp.picSkip                  = pRateCtrl->pictureSkip;
    rc_tmp.qpMin                    = pRateCtrl->qpMin;
    rc_tmp.qpMax                    = pRateCtrl->qpMax;
    rc_tmp.virtualBuffer.bitRate    = pRateCtrl->bitPerSecond;
    rc_tmp.gopLen                   = pRateCtrl->bitrateWindow;
    rc_tmp.intraQpDelta             = pRateCtrl->intraQpDelta;
    rc_tmp.fixedIntraQp             = pRateCtrl->fixedIntraQp;
    rc_tmp.intraPictureRate         = pRateCtrl->intraPictureRate;
    rc_tmp.goldenPictureRate        = pRateCtrl->goldenPictureRate;
    rc_tmp.altrefPictureRate        = pRateCtrl->altrefPictureRate;
    rc_tmp.outRateDenom				= pRateCtrl->outRateDenom;
    rc_tmp.outRateNum				= pRateCtrl->outRateNum;

    VP8InitRc(&rc_tmp, pEncInst->encStatus == VP8ENCSTAT_INIT);

    /* Set final values into instance */
    pEncInst->rateControl = rc_tmp;

    APITRACE("VP8EncSetRateCtrl: OK");
    return VP8ENC_OK;
}

/*------------------------------------------------------------------------------

    Function name : VP8EncGetRateCtrl
    Description   : Return current rate control parameters

    Return type   : VP8EncRet
    Argument      : inst - the instance in use
                    pRateCtrl - place where parameters are returned
------------------------------------------------------------------------------*/
VP8EncRet VP8EncGetRateCtrl(VP8EncInst inst, VP8EncRateCtrl * pRateCtrl)
{
    vp8Instance_s *pEncInst = (vp8Instance_s *) inst;

    APITRACE("VP8EncGetRateCtrl#");

    /* Check for illegal inputs */
    if((pEncInst == NULL) || (pRateCtrl == NULL))
    {
        APITRACE("VP8EncGetRateCtrl: ERROR Null argument");
        return VP8ENC_NULL_ARGUMENT;
    }

    /* Check for existing instance */
    if(pEncInst->inst != pEncInst)
    {
        APITRACE("VP8EncGetRateCtrl: ERROR Invalid instance");
        return VP8ENC_INSTANCE_ERROR;
    }

    pRateCtrl->qpHdr            = pEncInst->rateControl.qpHdr;
    pRateCtrl->pictureRc        = pEncInst->rateControl.picRc;
    pRateCtrl->pictureSkip      = pEncInst->rateControl.picSkip;
    pRateCtrl->qpMin            = pEncInst->rateControl.qpMin;
    pRateCtrl->qpMax            = pEncInst->rateControl.qpMax;
    pRateCtrl->bitPerSecond     = pEncInst->rateControl.virtualBuffer.bitRate;
    pRateCtrl->bitrateWindow    = pEncInst->rateControl.gopLen;
    pRateCtrl->intraQpDelta     = pEncInst->rateControl.intraQpDelta;
    pRateCtrl->fixedIntraQp     = pEncInst->rateControl.fixedIntraQp;
    pRateCtrl->intraPictureRate = pEncInst->rateControl.intraPictureRate;
    pRateCtrl->goldenPictureRate= pEncInst->rateControl.goldenPictureRate;
    pRateCtrl->altrefPictureRate= pEncInst->rateControl.altrefPictureRate;
    pRateCtrl->outRateNum		= pEncInst->rateControl.outRateNum;
    pRateCtrl->outRateDenom		= pEncInst->rateControl.outRateDenom;


    APITRACE("VP8EncGetRateCtrl: OK");
    return VP8ENC_OK;
}

/*------------------------------------------------------------------------------
    Function name   : VSCheckSize
    Description     :
    Return type     : i32
    Argument        : u32 inputWidth
    Argument        : u32 inputHeight
    Argument        : u32 stabilizedWidth
    Argument        : u32 stabilizedHeight
------------------------------------------------------------------------------*/
i32 VSCheckSize(u32 inputWidth, u32 inputHeight, u32 stabilizedWidth,
                u32 stabilizedHeight)
{
    /* Input picture minimum dimensions */
    if((inputWidth < 104) || (inputHeight < 104))
        return 1;

    /* Stabilized picture minimum  values */
    if((stabilizedWidth < 96) || (stabilizedHeight < 96))
        return 1;

    /* Stabilized dimensions multiple of 4 */
    if(((stabilizedWidth & 3) != 0) || ((stabilizedHeight & 3) != 0))
        return 1;

    /* Edge >= 4 pixels, not checked because stabilization can be
     * used without cropping for scene detection
    if((inputWidth < (stabilizedWidth + 8)) ||
       (inputHeight < (stabilizedHeight + 8)))
        return 1; */

    return 0;
}

/*------------------------------------------------------------------------------
    Function name   : VP8EncSetPreProcessing
    Description     : Sets the preprocessing parameters
    Return type     : VP8EncRet
    Argument        : inst - encoder instance in use
    Argument        : pPreProcCfg - user provided parameters
------------------------------------------------------------------------------*/
VP8EncRet VP8EncSetPreProcessing(VP8EncInst inst,
                                   const VP8EncPreProcessingCfg * pPreProcCfg)
{
    vp8Instance_s *pEncInst = (vp8Instance_s *) inst;
    preProcess_s pp_tmp;

    APITRACE("VP8EncSetPreProcessing#");

    /* Check for illegal inputs */
    if((inst == NULL) || (pPreProcCfg == NULL))
    {
        APITRACE("VP8EncSetPreProcessing: ERROR Null argument");
        return VP8ENC_NULL_ARGUMENT;
    }

    /* Check for existing instance */
    if(pEncInst->inst != pEncInst)
    {
        APITRACE("VP8EncSetPreProcessing: ERROR Invalid instance");
        return VP8ENC_INSTANCE_ERROR;
    }

    /* check HW limitations */
    /*{
        EWLHwConfig_t cfg = EWLReadAsicConfig();

        // is video stabilization supported?
        if(cfg.vsEnabled == EWL_HW_CONFIG_NOT_SUPPORTED &&
           pPreProcCfg->videoStabilization != 0)
        {
            APITRACE("VP8EncSetPreProcessing: ERROR Stabilization not supported");
            return VP8ENC_INVALID_ARGUMENT;
        }
        if(cfg.rgbEnabled == EWL_HW_CONFIG_NOT_SUPPORTED &&
           pPreProcCfg->inputType >= VP8ENC_RGB565 &&
           pPreProcCfg->inputType <= VP8ENC_BGR101010)
        {
            APITRACE("VP8EncSetPreProcessing: ERROR RGB input not supported");
            return VP8ENC_INVALID_ARGUMENT;
        }
    }*/

    if(pPreProcCfg->origWidth > VP8ENC_MAX_PP_INPUT_WIDTH ||
       pPreProcCfg->origHeight > VP8ENC_MAX_PP_INPUT_HEIGHT)
    {
        APITRACE("VP8EncSetPreProcessing: ERROR Too big input image");
        return VP8ENC_INVALID_ARGUMENT;
    }

    if(pPreProcCfg->inputType > VP8ENC_BGR101010)
    {
        APITRACE("VP8EncSetPreProcessing: ERROR Invalid YUV type");
        return VP8ENC_INVALID_ARGUMENT;
    }

    if(pPreProcCfg->rotation > VP8ENC_ROTATE_90L)
    {
        APITRACE("VP8EncSetPreProcessing: ERROR Invalid rotation");
        return VP8ENC_INVALID_ARGUMENT;
    }

    /* Encoded frame resolution as set in Init() */
    pp_tmp.lumWidth     = pEncInst->preProcess.lumWidth;
    pp_tmp.lumHeight    = pEncInst->preProcess.lumHeight;

    pp_tmp.lumHeightSrc = pPreProcCfg->origHeight;
    pp_tmp.lumWidthSrc  = pPreProcCfg->origWidth;
    pp_tmp.rotation     = pPreProcCfg->rotation;
    pp_tmp.inputFormat  = pPreProcCfg->inputType;
    pp_tmp.videoStab    = (pPreProcCfg->videoStabilization != 0) ? 1 : 0;

    if(pPreProcCfg->videoStabilization == 0) {
        pp_tmp.horOffsetSrc = pPreProcCfg->xOffset;
        pp_tmp.verOffsetSrc = pPreProcCfg->yOffset;
    } else {
        pp_tmp.horOffsetSrc = pp_tmp.verOffsetSrc = 0;
    }

    /* Check for invalid values */
    if(VP8_EncPreProcessCheck(&pp_tmp) != ENCHW_OK) {
        APITRACE("VP8EncSetPreProcessing: ERROR Invalid cropping values");
        return VP8ENC_INVALID_ARGUMENT;
    }

    if(pp_tmp.videoStab != 0) {
        u32 width = pp_tmp.lumWidth;
        u32 height = pp_tmp.lumHeight;

        if(pp_tmp.rotation) {
            u32 tmp = width;
            width = height;
            height = tmp;
        }

        if(VSCheckSize(pp_tmp.lumWidthSrc, pp_tmp.lumHeightSrc, width, height) != 0) {
            APITRACE("H264EncSetPreProcessing: ERROR Invalid size for stabilization");
            return VP8ENC_INVALID_ARGUMENT;
        }

#ifdef VIDEOSTAB_ENABLED
        VSAlgInit(&pEncInst->vsSwData, pp_tmp.lumWidthSrc, pp_tmp.lumHeightSrc,
                  width, height);

        VSAlgGetResult(&pEncInst->vsSwData, &pp_tmp.horOffsetSrc,
                       &pp_tmp.verOffsetSrc);
#endif
    }

    pp_tmp.colorConversionType = pPreProcCfg->colorConversion.type;
    pp_tmp.colorConversionCoeffA = pPreProcCfg->colorConversion.coeffA;
    pp_tmp.colorConversionCoeffB = pPreProcCfg->colorConversion.coeffB;
    pp_tmp.colorConversionCoeffC = pPreProcCfg->colorConversion.coeffC;
    pp_tmp.colorConversionCoeffE = pPreProcCfg->colorConversion.coeffE;
    pp_tmp.colorConversionCoeffF = pPreProcCfg->colorConversion.coeffF;
    VP8_EncSetColorConversion(&pp_tmp, &pEncInst->asic);

    /* Set final values into instance */
    pEncInst->preProcess = pp_tmp;

    APITRACE("VP8EncSetPreProcessing: OK");

    return VP8ENC_OK;
}

/*------------------------------------------------------------------------------
    Function name   : VP8EncGetPreProcessing
    Description     : Returns current preprocessing parameters
    Return type     : VP8EncRet
    Argument        : inst - encoder instance
    Argument        : pPreProcCfg - place where the parameters are returned
------------------------------------------------------------------------------*/
VP8EncRet VP8EncGetPreProcessing(VP8EncInst inst,
                                   VP8EncPreProcessingCfg * pPreProcCfg)
{
    vp8Instance_s *pEncInst = (vp8Instance_s *) inst;
    preProcess_s *pPP;

    APITRACE("VP8EncGetPreProcessing#");

    /* Check for illegal inputs */
    if((inst == NULL) || (pPreProcCfg == NULL))
    {
        APITRACE("VP8EncGetPreProcessing: ERROR Null argument");
        return VP8ENC_NULL_ARGUMENT;
    }

    /* Check for existing instance */
    if(pEncInst->inst != pEncInst)
    {
        APITRACE("VP8EncGetPreProcessing: ERROR Invalid instance");
        return VP8ENC_INSTANCE_ERROR;
    }

    pPP = &pEncInst->preProcess;

    pPreProcCfg->origWidth              = pPP->lumWidthSrc;
    pPreProcCfg->origHeight             = pPP->lumHeightSrc;
    pPreProcCfg->xOffset                = pPP->horOffsetSrc;
    pPreProcCfg->yOffset                = pPP->verOffsetSrc;
    pPreProcCfg->inputType              = pPP->inputFormat;
    pPreProcCfg->rotation               = pPP->rotation;
    pPreProcCfg->videoStabilization     = pPP->videoStab;
    pPreProcCfg->colorConversion.type   = pPP->colorConversionType;

    APITRACE("VP8EncGetPreProcessing: OK");
    return VP8ENC_OK;
}

/*------------------------------------------------------------------------------

    Function name : VP8EncStrmEncode
    Description   : Encodes a new picture
    Return type   : VP8EncRet
    Argument      : inst - encoder instance
    Argument      : pEncIn - user provided input parameters
                    pEncOut - place where output info is returned
------------------------------------------------------------------------------*/
VP8EncRet VP8EncStrmEncode(VP8EncInst inst, const VP8EncIn * pEncIn,
                             VP8EncOut * pEncOut, testbench_s *cml)
{
    vp8Instance_s *pEncInst = (vp8Instance_s *) inst;
    regValues_s *regs;
    vp8EncodeFrame_e ret;
    picBuffer *picBuffer;
    i32 i;
    VP8EncPictureCodingType ct;

    APITRACE("VP8EncStrmEncode#");

    /* Check for illegal inputs */
    if((pEncInst == NULL) || (pEncIn == NULL) || (pEncOut == NULL))
    {
        APITRACE("VP8EncStrmEncode: ERROR Null argument");
        return VP8ENC_NULL_ARGUMENT;
    }

    /* Check for existing instance */
    if(pEncInst->inst != pEncInst)
    {
        APITRACE("VP8EncStrmEncode: ERROR Invalid instance");
        return VP8ENC_INSTANCE_ERROR;
    }

    /* Clear the output structure */
    pEncOut->codingType = VP8ENC_NOTCODED_FRAME;
    pEncOut->frameSize = 0;
    for (i = 0; i < 9; i++)
    {
        pEncOut->pOutBuf[i] = NULL;
        pEncOut->streamSize[i] = 0;
    }

#ifdef EVALUATION_LIMIT
    /* Check for evaluation limit */
    if(pEncInst->frameCnt >= EVALUATION_LIMIT)
    {
        APITRACE("VP8EncStrmEncode: OK Evaluation limit exceeded");
        return VP8ENC_OK;
    }
#endif

    /* Check status, ERROR not allowed */
    if((pEncInst->encStatus != VP8ENCSTAT_INIT) &&
       (pEncInst->encStatus != VP8ENCSTAT_KEYFRAME) &&
       (pEncInst->encStatus != VP8ENCSTAT_START_FRAME))
    {
        APITRACE("VP8EncStrmEncode: ERROR Invalid status");
        return VP8ENC_INVALID_STATUS;
    }

    /* Check for invalid input values */
    if((!VP8_BUS_ADDRESS_VALID(pEncIn->busOutBuf)) ||
       (pEncIn->pOutBuf == NULL) ||
       (pEncIn->outBufSize < VP8ENCSTRMENCODE_MIN_BUF) ||
       (pEncIn->codingType > VP8ENC_PREDICTED_FRAME))
    {
        APITRACE("VP8EncStrmEncode: ERROR Invalid input. Output buffer");
        return VP8ENC_INVALID_ARGUMENT;
    }

    switch (pEncInst->preProcess.inputFormat)
    {
    case VP8ENC_YUV420_PLANAR:
        if(!VP8_BUS_ADDRESS_VALID(pEncIn->busChromaV))
        {
            APITRACE("VP8EncStrmEncode: ERROR Invalid input busChromaU");
            return VP8ENC_INVALID_ARGUMENT;
        }
        /* fall through */
    case VP8ENC_YUV420_SEMIPLANAR:
        if(!VP8_BUS_ADDRESS_VALID(pEncIn->busChromaU))
        {
            APITRACE("VP8EncStrmEncode: ERROR Invalid input busChromaU");
            return VP8ENC_INVALID_ARGUMENT;
        }
        /* fall through */
    case VP8ENC_YUV422_INTERLEAVED_YUYV:
    case VP8ENC_YUV422_INTERLEAVED_UYVY:
    case VP8ENC_RGB565:
    case VP8ENC_BGR565:
    case VP8ENC_RGB555:
    case VP8ENC_BGR555:
    case VP8ENC_RGB444:
    case VP8ENC_BGR444:
    case VP8ENC_RGB888:
    case VP8ENC_BGR888:
    case VP8ENC_RGB101010:
    case VP8ENC_BGR101010:
        if(!VP8_BUS_ADDRESS_VALID(pEncIn->busLuma))
        {
            APITRACE("VP8EncStrmEncode: ERROR Invalid input busLuma");
            return VP8ENC_INVALID_ARGUMENT;
        }
        break;
    default:
        APITRACE("VP8EncStrmEncode: ERROR Invalid input format");
        return VP8ENC_INVALID_ARGUMENT;
    }

    if(pEncInst->preProcess.videoStab)
    {
        if(!VP8_BUS_ADDRESS_VALID(pEncIn->busLumaStab))
        {
            APITRACE("VP8EncStrmEncode: ERROR Invalid input busLumaStab");
            return VP8ENC_INVALID_ARGUMENT;
        }
    }

    /* some shortcuts */
    regs = &pEncInst->asic.regs;

    /* update in/out buffers */
    regs->inputLumBase = pEncIn->busLuma;
    regs->inputCbBase = pEncIn->busChromaU;
    regs->inputCrBase = pEncIn->busChromaV;

    /* Try to reserve the HW resource */
    /*if(EWLReserveHw(pEncInst->asic.ewl) == EWL_ERROR)
    {
        APITRACE("VP8EncStrmEncode: ERROR HW unavailable");
        return VP8ENC_HW_RESERVED;
    }*/

    /* setup stabilization */
    if(pEncInst->preProcess.videoStab)
        regs->vsNextLumaBase = pEncIn->busLumaStab;

    /* Choose frame coding type */
    ct = pEncIn->codingType;

    /* Status may affect the frame coding type */
    if ((pEncInst->encStatus == VP8ENCSTAT_INIT) ||
        (pEncInst->encStatus == VP8ENCSTAT_KEYFRAME))
        ct = VP8ENC_INTRA_FRAME;

#ifdef VIDEOSTAB_ENABLED
    if(pEncInst->vsSwData.sceneChange)
    {
        pEncInst->vsSwData.sceneChange = 0;
        ct = VP8ENC_INTRA_FRAME;
    }
#endif

    /* Divide stream buffer for every partition */
    {
        u8 *pStart = (u8*)pEncIn->pOutBuf;
        u8 *pEnd;
        u32 busAddr = pEncIn->busOutBuf;
        i32 bufSize = pEncIn->outBufSize;
        i32 status = ENCHW_OK;

        /* Frame tag 10 bytes (I-frame) or 3 bytes (P-frame),
         * written by SW at end of frame */
        pEnd = pStart + 3;
        if (ct == VP8ENC_INTRA_FRAME) pEnd += 7;
        if(VP8SetBuffer(&pEncInst->buffer[0], pStart, pEnd-pStart) == ENCHW_NOK)
            status = ENCHW_NOK;

        /* StrmBase is not 64-bit aligned now but that will be taken care of
         * in CodeFrame() */
        busAddr += pEnd-pStart;
        regs->outputStrmBase = busAddr;

        /* 10% for frame header and mb control partition. */
        pStart = pEnd;
        pEnd = pStart + bufSize/10;
        pEnd = (u8*)((size_t)pEnd & ~0x7);   /* 64-bit aligned address */
        if(VP8SetBuffer(&pEncInst->buffer[1], pStart, pEnd-pStart) == ENCHW_NOK)
            status = ENCHW_NOK;

        busAddr += pEnd-pStart;
        regs->partitionBase[0] = busAddr;

        /* The rest is divided for one or two DCT partitions */
        pStart = pEnd;
        if (pEncInst->sps.dctPartitions)
            pEnd = pStart + 9*bufSize/20;
        else
            pEnd = ((u8*)pEncIn->pOutBuf) + bufSize;
        pEnd = (u8*)((size_t)pEnd & ~0x7);   /* 64-bit aligned address */
        if(VP8SetBuffer(&pEncInst->buffer[2], pStart, pEnd-pStart) == ENCHW_NOK)
            status = ENCHW_NOK;

        busAddr += pEnd-pStart;
        regs->partitionBase[1] = busAddr;
        /* ASIC stream buffer limit, the same limit is used for all partitions */
        regs->outputStrmSize = pEnd-pStart;

        pStart = pEnd;
        if (pEncInst->sps.dctPartitions) {
            pEnd = ((u8*)pEncIn->pOutBuf) + bufSize;
            if(VP8SetBuffer(&pEncInst->buffer[3], pStart, pEnd-pStart) == ENCHW_NOK)
                status = ENCHW_NOK;
        }

        if (status == ENCHW_NOK)
        {
            APITRACE("VP8EncStrmEncode: ERROR Invalid output buffer");
            return VP8ENC_INVALID_ARGUMENT;
        }
    }

    /* Initialize picture buffer and ref pic list according to frame type */
    picBuffer = &pEncInst->picBuffer;
    picBuffer->cur_pic->show    = 1;
    picBuffer->cur_pic->poc     = pEncInst->frameCnt;
    picBuffer->cur_pic->i_frame = (ct == VP8ENC_INTRA_FRAME);
    InitializePictureBuffer(picBuffer);

    /* Set picture buffer according to frame coding type */
    if (ct == VP8ENC_PREDICTED_FRAME) {
        picBuffer->cur_pic->p_frame = 1;
        picBuffer->cur_pic->arf = (pEncIn->arf&VP8ENC_REFRESH) ? 1 : 0;
        picBuffer->cur_pic->grf = (pEncIn->grf&VP8ENC_REFRESH) ? 1 : 0;
        picBuffer->cur_pic->ipf = (pEncIn->ipf&VP8ENC_REFRESH) ? 1 : 0;
        picBuffer->refPicList[0].search = (pEncIn->ipf&VP8ENC_REFERENCE) ? 1 : 0;
        picBuffer->refPicList[1].search = (pEncIn->grf&VP8ENC_REFERENCE) ? 1 : 0;
        picBuffer->refPicList[2].search = (pEncIn->arf&VP8ENC_REFERENCE) ? 1 : 0;
    }

    /* Rate control */
    VP8BeforePicRc(&pEncInst->rateControl, pEncIn->timeIncrement,
                    picBuffer->cur_pic->i_frame);

    /* Rate control may choose to skip the frame */
    if(pEncInst->rateControl.frameCoded == ENCHW_NO)
    {
        APITRACE("VP8EncStrmEncode: OK, frame skipped");
        //EWLReleaseHw(pEncInst->asic.ewl);

        return VP8ENC_FRAME_READY;
    }

    /* TODO: RC can set frame to grf and copy grf to arf */
    if (pEncInst->rateControl.goldenPictureRate) {
        picBuffer->cur_pic->grf = 1;
        if (!picBuffer->cur_pic->arf)
            picBuffer->refPicList[1].arf = 1;
    }

    /* Set some frame coding parameters before internal test configure */
    VP8SetFrameParams(pEncInst);

#ifdef TRACE_STREAM
    traceStream.frameNum = pEncInst->frameCnt;
    traceStream.id = 0; /* Stream generated by SW */
    traceStream.bitCnt = 0;  /* New frame */
#endif

#ifdef INTERNAL_TEST
    /* Configure the encoder instance according to the test vector */
    Vp8ConfigureTestBeforeFrame(pEncInst);
#endif

    /* update any cropping/rotation/filling */
    VP8_EncPreProcess(&pEncInst->asic, &pEncInst->preProcess);

    /* Get the reference frame buffers from picture buffer */
    PictureBufferSetRef(picBuffer, &pEncInst->asic);

    /* Code one frame */
    ret = VP8CodeFrame(pEncInst, cml);

#ifdef TRACE_RECON
    EncDumpRecon(&pEncInst->asic);
#endif

    if(ret != VP8ENCODE_OK)
    {
        /* Error has occured and the frame is invalid */
        VP8EncRet to_user;

        switch (ret)
        {
        case VP8ENCODE_TIMEOUT:
            APITRACE("VP8EncStrmEncode: ERROR HW/IRQ timeout");
            to_user = VP8ENC_HW_TIMEOUT;
            break;
        case VP8ENCODE_HW_RESET:
            APITRACE("VP8EncStrmEncode: ERROR HW reset detected");
            to_user = VP8ENC_HW_RESET;
            break;
        case VP8ENCODE_HW_ERROR:
            APITRACE("VP8EncStrmEncode: ERROR HW bus access error");
            to_user = VP8ENC_HW_BUS_ERROR;
            break;
        case VP8ENCODE_SYSTEM_ERROR:
        default:
            /* System error has occured, encoding can't continue */
            pEncInst->encStatus = VP8ENCSTAT_ERROR;
            APITRACE("VP8EncStrmEncode: ERROR Fatal system error");
            to_user = VP8ENC_SYSTEM_ERROR;
        }

        return to_user;
    }

#ifdef VIDEOSTAB_ENABLED
    /* Finalize video stabilization */
    if(pEncInst->preProcess.videoStab) {
        u32 no_motion;

        VSReadStabData(pEncInst->asic.regs.regMirror, &pEncInst->vsHwData);

        no_motion = VSAlgStabilize(&pEncInst->vsSwData, &pEncInst->vsHwData);
        if(no_motion) {
            VSAlgReset(&pEncInst->vsSwData);
        }

        /* update offset after stabilization */
        VSAlgGetResult(&pEncInst->vsSwData, &pEncInst->preProcess.horOffsetSrc,
                       &pEncInst->preProcess.verOffsetSrc);
    }
#endif

    pEncOut->motionVectors = (i8*)pEncInst->asic.mvOutput.vir_addr;

    /* After stream buffer overflow discard the coded frame */
    if(VP8BufferOverflow(&pEncInst->buffer[1]) == ENCHW_NOK)
    {
        /* Frame discarded, reference frame corrupted, next must be intra. */
        pEncInst->encStatus = VP8ENCSTAT_KEYFRAME;
        pEncInst->prevFrameLost = 1;
        APITRACE("VP8EncStrmEncode: ERROR Output buffer too small");
        return VP8ENC_OUTPUT_BUFFER_OVERFLOW;
    }

    /* Store the stream size and frame coding type in output structure */
    pEncOut->pOutBuf[0] = (u32*)pEncInst->buffer[0].pData;
    pEncOut->pOutBuf[1] = (u32*)pEncInst->buffer[2].pData;
    if (pEncInst->sps.dctPartitions)
        pEncOut->pOutBuf[2] = (u32*)pEncInst->buffer[3].pData;
    pEncOut->streamSize[0] = pEncInst->buffer[0].byteCnt +
                             pEncInst->buffer[1].byteCnt;
    pEncOut->streamSize[1] = pEncInst->buffer[2].byteCnt;
    if (pEncInst->sps.dctPartitions)
        pEncOut->streamSize[2] = pEncInst->buffer[3].byteCnt;

    /* Total frame size */
    pEncOut->frameSize =
        pEncOut->streamSize[0] + pEncOut->streamSize[1] + pEncOut->streamSize[2];

    /* Rate control action after frame */
    VP8AfterPicRc(&pEncInst->rateControl, pEncOut->frameSize);

    if(picBuffer->cur_pic->i_frame) {
        pEncOut->codingType = VP8ENC_INTRA_FRAME;
        pEncOut->arf = pEncOut->grf = pEncOut->ipf = 0;
    } else {
        pEncOut->codingType = VP8ENC_PREDICTED_FRAME;
        pEncOut->ipf = picBuffer->refPicList[0].search ? VP8ENC_REFERENCE : 0;
        pEncOut->grf = picBuffer->refPicList[1].search ? VP8ENC_REFERENCE : 0;
        pEncOut->arf = picBuffer->refPicList[2].search ? VP8ENC_REFERENCE : 0;
    }

    /* Mark which reference frame was refreshed */
    pEncOut->arf |= picBuffer->cur_pic->arf ? VP8ENC_REFRESH : 0;
    pEncOut->grf |= picBuffer->cur_pic->grf ? VP8ENC_REFRESH : 0;
    pEncOut->ipf |= picBuffer->cur_pic->ipf ? VP8ENC_REFRESH : 0;

    UpdatePictureBuffer(picBuffer);

    /* Frame was encoded so increment frame number */
    pEncInst->frameCnt++;
    pEncInst->encStatus = VP8ENCSTAT_START_FRAME;
    pEncInst->prevFrameLost = 0;

    APITRACE("VP8EncStrmEncode: OK");
    return VP8ENC_FRAME_READY;
}


/*------------------------------------------------------------------------------
    Function name : VP8EncSetTestId
    Description   : Sets the encoder configuration according to a test vector
    Return type   : VP8EncRet
    Argument      : inst - encoder instance
    Argument      : testId - test vector ID
------------------------------------------------------------------------------*/
VP8EncRet VP8EncSetTestId(VP8EncInst inst, u32 testId)
{
    vp8Instance_s *pEncInst = (vp8Instance_s *) inst;
    (void) pEncInst;
    (void) testId;

    APITRACE("VP8EncSetTestId#");

#ifdef INTERNAL_TEST
    pEncInst->testId = testId;

    APITRACE("VP8EncSetTestId# OK");
    return VP8ENC_OK;
#else
    /* Software compiled without testing support, return error always */
    APITRACE("VP8EncSetTestId# ERROR, testing disabled at compile time");
    return VP8ENC_ERROR;
#endif
}

