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
--  Abstract : Hantro Encoder Wrapper Layer for OS services
--
------------------------------------------------------------------------------*/

#ifndef __EWL_H__
#define __EWL_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include "basetype.h"
#include "vpu_mem.h"
#include "vpu.h"
#include "vpu_drv.h"

/* Return values */
#define EWL_OK                      0
#define EWL_ERROR                  -1

#define EWL_HW_WAIT_OK              EWL_OK
#define EWL_HW_WAIT_ERROR           EWL_ERROR
#define EWL_HW_WAIT_TIMEOUT         1

/* HW configuration values */
#define EWL_HW_BUS_TYPE_UNKNOWN     0
#define EWL_HW_BUS_TYPE_AHB         1
#define EWL_HW_BUS_TYPE_OCP         2
#define EWL_HW_BUS_TYPE_AXI         3
#define EWL_HW_BUS_TYPE_PCI         4

#define EWL_HW_BUS_WIDTH_UNKNOWN     0
#define EWL_HW_BUS_WIDTH_32BITS      1
#define EWL_HW_BUS_WIDTH_64BITS      2
#define EWL_HW_BUS_WIDTH_128BITS     3

#define EWL_HW_SYNTHESIS_LANGUAGE_UNKNOWN     0
#define EWL_HW_SYNTHESIS_LANGUAGE_VHDL        1
#define EWL_HW_SYNTHESIS_LANGUAGE_VERILOG     2

#define EWL_HW_CONFIG_NOT_SUPPORTED    0
#define EWL_HW_CONFIG_ENABLED          1

/* Hardware configuration description */
    typedef struct EWLHwConfig
    {
        u32 maxEncodedWidth; /* Maximum supported width for video encoding (not JPEG) */
        u32 h264Enabled;     /* HW supports H.264 */
        u32 jpegEnabled;     /* HW supports JPEG */
        u32 vp8Enabled;      /* HW supports VP8 */
        u32 vsEnabled;       /* HW supports video stabilization */
        u32 rgbEnabled;      /* HW supports RGB input */
        u32 searchAreaSmall; /* HW search area reduced */
        u32 inputYuvTiledEnabled; /* HW supports tiled YUV input format */
        u32 busType;         /* HW bus type in use */
        u32 busWidth;
        u32 synthesisLanguage;
    } EWLHwConfig_t;

/* EWLInitParam is used to pass parameters when initializing the EWL */
    typedef struct EWLInitParam
    {
        u32 clientType;
    } EWLInitParam_t;

#define EWL_CLIENT_TYPE_H264_ENC         1U
#define EWL_CLIENT_TYPE_VP8_ENC          2U
#define EWL_CLIENT_TYPE_JPEG_ENC         3U
#define EWL_CLIENT_TYPE_VIDEOSTAB        4U


#ifdef __cplusplus
}
#endif
#endif /*__EWL_H__*/
