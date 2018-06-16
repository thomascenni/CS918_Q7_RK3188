/*****************************************************************************
	File:
		rk_vdpu_regs.cpp
	Decription:
		register operation
	Author:
		herman.chen@rock-chips.com
		2014-8-13 10:58:47
 *****************************************************************************/

#define LOG_TAG "regs"
#include <utils/Log.h>
#include <stdio.h>
#include <stdlib.h>
#include "rk_vdpu_regs.h"

typedef enum {
    RK_VPU_29xx,
    RK_VPU_30xx,
    RK_VPU_31xx,
    RK_VPU_32xx,
} RK_VPU_TYPE;

typedef RK_U32  REG_SPEC[3];

static  RK_S32      platform_type   = -1;
static  REG_SPEC*   regs_spec       = NULL;

/*HW _config as follows */
static const RK_U32 kRegMask[33] = {    0x00000000,
	0x00000001, 0x00000003, 0x00000007, 0x0000000F,
	0x0000001F, 0x0000003F, 0x0000007F, 0x000000FF,
	0x000001FF, 0x000003FF, 0x000007FF, 0x00000FFF,
	0x00001FFF, 0x00003FFF, 0x00007FFF, 0x0000FFFF,
	0x0001FFFF, 0x0003FFFF, 0x0007FFFF, 0x000FFFFF,
	0x001FFFFF, 0x003FFFFF, 0x007FFFFF, 0x00FFFFFF,
	0x01FFFFFF, 0x03FFFFFF, 0x07FFFFFF, 0x0FFFFFFF,
	0x1FFFFFFF, 0x3FFFFFFF, 0x7FFFFFFF, 0xFFFFFFFF
};

/* { SWREG, BITS, POSITION } */
static REG_SPEC vdpu_6731_reg_spec[HWIF_LAST_REG + 1] = {
	/* include script-generated part */
#include "rk_6731table.h"
	/* HWIF_DEC_IRQ_STAT */ {1, 7, 12},
	/* HWIF_PP_IRQ_STAT */ {60, 2, 12},
	/* dummy entry */ {0, 0, 0}
};

static void updatePlatType()
{
    if (platform_type < 0) {
        platform_type = RK_VPU_32xx;
        regs_spec     = &vdpu_6731_reg_spec[0];
    }
}

RkVdpuRegs::RkVdpuRegs()
  : regs_base(NULL),
    regs_size(0),
    regs_log(0) {

	updatePlatType();

    switch (platform_type) {
    case RK_VPU_29xx :
    case RK_VPU_30xx :
    case RK_VPU_31xx :
    case RK_VPU_32xx : {
        regs_size   = 60*sizeof(RK_U32);
        regs_base   = (RK_U32*)malloc(regs_size);
        if (NULL != regs_base)
            memset(regs_base, 0, regs_size);
    } break;
    default : {
    } break;
    }
}

RkVdpuRegs::~RkVdpuRegs()
{
    if (regs_size) {
        free(regs_base);
        regs_base = NULL;
        regs_size = 0;
    }
}

RK_U32* RkVdpuRegs::base()
{
    return regs_base;
}

RK_U32  RkVdpuRegs::size()
{
    return regs_size;
}

RK_S32  RkVdpuRegs::write(HWIF_VDPU_E index, RK_U32 val)
{
    if (!regs_size || index >= HWIF_LAST_REG) {
        return -1;
    }

	RK_U32 tmp;

    if (regs_log) {
        ALOGE("write: index %.3d spec %.2d %.2d %.2d value 0x%x",
            index, regs_spec[index][0], regs_spec[index][1], regs_spec[index][2], val);
    }
	tmp = regs_base[regs_spec[index][0]];
	tmp &= ~(kRegMask[regs_spec[index][1]] << regs_spec[index][2]);
	tmp |= (val & kRegMask[regs_spec[index][1]]) << regs_spec[index][2];
	regs_base[regs_spec[index][0]] = tmp;

    return 0;
}


RK_U32  RkVdpuRegs::read(HWIF_VDPU_E index)
{
    if (!regs_size || index >= HWIF_LAST_REG) {
        return 0;
    }

	RK_U32 tmp;

	tmp = regs_base[regs_spec[index][0]];
	tmp = tmp >> regs_spec[index][2];
	tmp &= kRegMask[regs_spec[index][1]];

    if (regs_log) {
        ALOGE("read : index %.3d spec %.2d %.2d %.2d value 0x%x",
            index, regs_spec[index][0], regs_spec[index][1], regs_spec[index][2], tmp);
    }

	return tmp;
}

void    RkVdpuRegs::dump(RK_U32 *addr, RK_S32 len)
{
    if (!regs_size)
        return;

    if (len > regs_size)
        len = regs_size;

    if (NULL != addr) {
	    memcpy(addr, regs_base, len);
    } else {
        for (RK_U32 i = 0; i < len/sizeof(RK_U32); i++) {
            ALOGE("dump: %2d 0x%.8x\n", i, regs_base[i]);
        }
    }
}

void    RkVdpuRegs::setLog(RK_U32 on)
{
    regs_log = on;
}

