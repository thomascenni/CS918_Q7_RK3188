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
#define LOG_TAG "enc_control"
#include "encpreprocess.h"
#include "encasiccontroller.h"
#include "enccommon.h"
#include "ewl.h"
#include <utils/Log.h>
/*------------------------------------------------------------------------------
    External compiler flags
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    Module defines
------------------------------------------------------------------------------*/

/* Mask fields */
#define mask_2b         (u32)0x00000003
#define mask_3b         (u32)0x00000007
#define mask_4b         (u32)0x0000000F
#define mask_5b         (u32)0x0000001F
#define mask_6b         (u32)0x0000003F
#define mask_11b        (u32)0x000007FF
#define mask_14b        (u32)0x00003FFF
#define mask_16b        (u32)0x0000FFFF

#define HSWREG(n)       ((n)*4)

/*------------------------------------------------------------------------------
    Local function prototypes
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------

    EncAsicMemAlloc_V2

    Allocate HW/SW shared memory

    Input:
        asic        asicData structure
        width       width of encoded image, multiple of four
        height      height of encoded image
        type        ASIC_MPEG4  / ASIC_JPEG
        numRefBuffs amount of reference luma frame buffers to be allocated

    Output:
        asic        base addresses point to allocated memory areas

    Return:
        ENCHW_OK        Success.
        ENCHW_NOK       Error: memory allocation failed, no memories allocated
                        and EWL instance released

------------------------------------------------------------------------------*/
i32 VP8_EncAsicMemAlloc_V2(asicData_s * asic, u32 width, u32 height,
                       u32 encodingType, u32 numRefBuffsLum, u32 numRefBuffsChr)
{
    u32 mbTotal, i;
    regValues_s *regs;
    VPUMemLinear_t *buff = NULL;

    ASSERT(asic != NULL);
    ASSERT(width != 0);
    ASSERT(height != 0);
    ASSERT((height % 2) == 0);
    ASSERT((width % 4) == 0);

    regs = &asic->regs;

    regs->codingType = encodingType;

    width = (width + 15) / 16;
    height = (height + 15) / 16;

    mbTotal = width * height;

    /* Allocate H.264 internal memories */
    if(regs->codingType != ASIC_JPEG)
    {
        /* The sizes of the memories */
        u32 internalImageLumaSize = mbTotal * (16 * 16);
        u32 internalImageChromaSize = mbTotal * (2 * 8 * 8);

        ASSERT((numRefBuffsLum >= 1) && (numRefBuffsLum <= 3));
        ASSERT((numRefBuffsChr >= 2) && (numRefBuffsChr <= 4));

        for (i = 0; i < numRefBuffsLum; i++)
        {
            if(VPUMallocLinear(&asic->internalImageLuma[i], internalImageLumaSize) != EWL_OK)
            {
                VP8_EncAsicMemFree_V2(asic);
                return ENCHW_NOK;
            }
        }

        for (i = 0; i < numRefBuffsChr; i++)
        {

            if(VPUMallocLinear(&asic->internalImageChroma[i], internalImageChromaSize) != EWL_OK)
            {
                VP8_EncAsicMemFree_V2(asic);
                return ENCHW_NOK;
            }
        }

        /* Set base addresses to the registers */
        regs->internalImageLumBaseW = asic->internalImageLuma[0].phy_addr;
        regs->internalImageChrBaseW = asic->internalImageChroma[0].phy_addr;
        if (numRefBuffsLum > 1)
            regs->internalImageLumBaseR[0] = asic->internalImageLuma[1].phy_addr;
        else
            regs->internalImageLumBaseR[0] = asic->internalImageLuma[0].phy_addr;
        regs->internalImageChrBaseR[0] = asic->internalImageChroma[1].phy_addr;

        /* NAL size table, table size must be 64-bit multiple,
         * space for SEI, MVC prefix, filler and zero at the end of table.
         * Atleast 1 macroblock row in every slice.
         * Also used for VP8 partitions. */
        buff = &asic->sizeTbl;
        asic->sizeTblSize = (sizeof(u32) * (height+4) + 7) & (~7);

        if(VPUMallocLinear(buff, asic->sizeTblSize) != EWL_OK)
        {
            VP8_EncAsicMemFree_V2(asic);
            return ENCHW_NOK;
        }

        /* H264: CABAC context tables: all qps, intra+inter, 464 bytes/table.
         * VP8: The same table is used for probability tables, 1208 bytes. */
        if (regs->codingType == ASIC_VP8) i = 8*55+8*96;
        else i = 52*2*464;

        if(VPUMallocLinear(&asic->cabacCtx, i) != EWL_OK)
        {
            VP8_EncAsicMemFree_V2(asic);
            return ENCHW_NOK;
        }
        regs->cabacCtxBase = asic->cabacCtx.phy_addr;

        /* MV output table: 4 bytes per MB  */

        if(VPUMallocLinear(&asic->mvOutput, mbTotal*4) != EWL_OK)
        {
            VP8_EncAsicMemFree_V2(asic);
            return ENCHW_NOK;
        }
        regs->mvOutputBase = asic->mvOutput.phy_addr;

        /* Clear mv output memory*/
        for (i = 0; i < mbTotal; i++)
            asic->mvOutput.vir_addr[i] = 0;

        if(regs->codingType == ASIC_VP8) {
            /* VP8: Table of counter for probability updates. */

         
            if(VPUMallocLinear(&asic->probCount, ASIC_VP8_PROB_COUNT_SIZE) != EWL_OK)
            {
                VP8_EncAsicMemFree_V2(asic);
                return ENCHW_NOK;
            }
            regs->probCountBase = asic->probCount.phy_addr;

            /* VP8: Segmentation map, 4 bits/mb, 64-bit multiple. */

          
            if(VPUMallocLinear(&asic->segmentMap, (mbTotal*4 + 63)/64*8) != EWL_OK)
            {
                VP8_EncAsicMemFree_V2(asic);
                return ENCHW_NOK;
            }
            regs->segmentMapBase = asic->segmentMap.phy_addr;

            memset(asic->segmentMap.vir_addr, 0,
                      asic->segmentMap.size);

        }
    }

    return ENCHW_OK;
}

/*------------------------------------------------------------------------------

    EncAsicMemFree_V2

    Free HW/SW shared memory

------------------------------------------------------------------------------*/
void VP8_EncAsicMemFree_V2(asicData_s * asic)
{
    ASSERT(asic != NULL);

    if(asic->internalImageLuma[0].vir_addr != NULL)
        VPUFreeLinear(&asic->internalImageLuma[0]);

    if(asic->internalImageLuma[1].vir_addr != NULL)
        VPUFreeLinear(&asic->internalImageLuma[1]);

    if(asic->internalImageChroma[0].vir_addr != NULL)
        VPUFreeLinear(&asic->internalImageChroma[0]);

    if(asic->internalImageChroma[1].vir_addr != NULL)
        VPUFreeLinear(&asic->internalImageChroma[1]);

    if(asic->internalImageChroma[2].vir_addr != NULL)
        VPUFreeLinear(&asic->internalImageChroma[2]);

    if(asic->sizeTbl.vir_addr != NULL)
        VPUFreeLinear(&asic->sizeTbl);

    if(asic->cabacCtx.vir_addr != NULL)
        VPUFreeLinear(&asic->cabacCtx);

    if(asic->mvOutput.vir_addr != NULL)
        VPUFreeLinear(&asic->mvOutput);

    if(asic->probCount.vir_addr != NULL)
        VPUFreeLinear(&asic->probCount);

    asic->internalImageLuma[0].vir_addr = NULL;
    asic->internalImageLuma[1].vir_addr = NULL;
    asic->internalImageChroma[0].vir_addr = NULL;
    asic->internalImageChroma[1].vir_addr = NULL;
    asic->internalImageChroma[2].vir_addr = NULL;
    asic->sizeTbl.vir_addr = NULL;
    asic->cabacCtx.vir_addr = NULL;
    asic->mvOutput.vir_addr = NULL;
    asic->probCount.vir_addr = NULL;
}

/*------------------------------------------------------------------------------
------------------------------------------------------------------------------*/
i32 VP8_EncAsicCheckStatus_V2(asicData_s * asic)
{
    i32 ret;
    u32 status;

    status = VP8_EncAsicGetStatus(&asic->regs);

    if(status & ASIC_STATUS_ERROR)
    {
        /* Get registers for debugging */
        VP8_EncAsicGetRegisters(asic->ewl, &asic->regs);

        ret = ASIC_STATUS_ERROR;

        //EWLReleaseHw(asic->ewl);
    }
    else if(status & ASIC_STATUS_HW_TIMEOUT)
    {
        /* Get registers for debugging */
        VP8_EncAsicGetRegisters(asic->ewl, &asic->regs);

        ret = ASIC_STATUS_HW_TIMEOUT;

        //EWLReleaseHw(asic->ewl);
    }
    else if(status & ASIC_STATUS_FRAME_READY)
    {
        VP8_EncAsicGetRegisters(asic->ewl, &asic->regs);

        ret = ASIC_STATUS_FRAME_READY;

        //EWLReleaseHw(asic->ewl);
    }
    else if(status & ASIC_STATUS_BUFF_FULL)
    {
        ret = ASIC_STATUS_BUFF_FULL;
        /* ASIC doesn't support recovery from buffer full situation,
         * at the same time with buff full ASIC also resets itself. */
        //EWLReleaseHw(asic->ewl);
    }
    else if(status & ASIC_STATUS_HW_RESET)
    {
        ret = ASIC_STATUS_HW_RESET;

        //EWLReleaseHw(asic->ewl);
    }
    else /* Don't check SLICE_READY status bit, it is reseted by IRQ handler */
    {
        /* IRQ received but none of the status bits is high, so it must be
         * slice ready. */
        ret = ASIC_STATUS_SLICE_READY;
    }

    return ret;
}
