/***************************************************************************************************
    File:
        mpeg4_rk_macro.h
    Descrption:
        macro definition
    Author:
        Jian Huan
    Date:
        2010-12-3 11:05:29
***************************************************************************************************/
#ifndef _MPEG4_RK_MACRO_H
#define _MPEG4_RK_MACRO_H

#include "vpu_macro.h"

#define MPEG4_RK_MAX_BIT_BUF_SIZE      (512*1024)

#define MPEG4_RK_MAX_FRAME_WIDTH       VPU_MAX_FRAME_WIDTH
#define MPEG4_RK_MAX_FRAME_HEIGHT      VPU_MAX_FRAME_HEIGHT

#define MPEG4_RK_FRAME_NULL            VPU_FRAME_NULL

#define MPEG4_STANDARD_VERSION          0
/***************************************************************************************************
                                            VOP MACRO
 **************************************************************************************************/
/*video object specific*/
#define MPEG4_VIDOBJ_START_CODE_MASK    0x0000001f
#define MPEG4_VIDOBJLAY_START_CODE_MASK 0x0000000f

#define MPEG4_VIDOBJ_START_CODE         0x00000100 /* ..0x0000011f  */
#define MPEG4_VIDOBJLAY_START_CODE      0x00000120 /* ..0x0000012f */
#define MPEG4_VISOBJSEQ_START_CODE      0x000001b0
#define MPEG4_VISOBJSEQ_STOP_CODE       0x000001b1 /* ??? */
#define MPEG4_USERDATA_START_CODE       0x000001b2
#define MPEG4_GRPOFVOP_START_CODE       0x000001b3
#define MPEG4_VISOBJ_START_CODE         0x000001b5
#define MPEG4_VOP_START_CODE            0x000001b6

#define MPEG4_VISOBJ_TYPE_VIDEO             1

#define MPEG4_VIDOBJLAY_AR_SQUARE           1
#define MPEG4_VIDOBJLAY_AR_625TYPE_43       2
#define MPEG4_VIDOBJLAY_AR_525TYPE_43       3
#define MPEG4_VIDOBJLAY_AR_625TYPE_169      8
#define MPEG4_VIDOBJLAY_AR_525TYPE_169      9
#define MPEG4_VIDOBJLAY_AR_EXTPAR           15

#define MPEG4_VIDOBJLAY_SHAPE_RECTANGULAR   0
#define MPEG4_VIDOBJLAY_SHAPE_BINARY        1
#define MPEG4_VIDOBJLAY_SHAPE_BINARY_ONLY   2
#define MPEG4_VIDOBJLAY_SHAPE_GRAYSCALE     3

/*video sprite specific*/
#define MPEG4_SPRITE_NONE                   0
#define MPEG4_SPRITE_STATIC                 1
#define MPEG4_SPRITE_GMC                    2

/*video vop specific*/
#define MPEG4_RESYNC_VOP                    -2
#define MPEG4_INVALID_VOP                   -1
#define MPEG4_I_VOP                         0
#define MPEG4_P_VOP                         1
#define MPEG4_B_VOP                         2
#define MPEG4_S_VOP                         3
#define MPEG4_N_VOP                         4
#define MPEG4_D_VOP                         5       /*Drop Frame*/

/*video mode specific*/
#define MPEG4_MODE_INTER                    0
#define MPEG4_MODE_INTER_Q                  1
#define MPEG4_MODE_INTER4V                  2
#define MPEG4_MODE_INTRA                    3
#define MPEG4_MODE_INTRA_Q                  4
#define MPEG4_MODE_NOT_CODED                16
#define MPEG4_MODE_NOT_CODED_GMC            17

/*video bframe specific*/
#define MODE_DIRECT                         0
#define MODE_INTERPOLATE                    1
#define MODE_BACKWARD                       2
#define MODE_FORWARD                        3
#define MODE_DIRECT_NONE_MV                 4
#define MODE_DIRECT_NO4V                    5

/***************************************************************************************************
                                            RESYNC MACRO
 **************************************************************************************************/
#define NUMBITS_VP_RESYNC_MARKER    17
#define RESYNC_MARKER               1

#define MPEG4_MEM_INIT  8
#define MPEG4_INIT_OK   VPU_OK
#define MPEG4_DEINIT_OK VPU_OK
#define MPEG4_HDR_OK    VPU_OK
#define MPEG4_MEM_OK    VPU_OK
#define MPEG4_DEC_OK    VPU_OK
#define MPEG4_VLD_OK    VPU_OK

#define MPEG4_IVOP_OK   MPEG4_DEC_OK
#define MPEG4_PVOP_OK   MPEG4_DEC_OK
#define MPEG4_BVOP_OK   MPEG4_DEC_OK
#define MPEG4_SVOP_OK   MPEG4_DEC_OK


#define MPEG4_INIT_ERR      -1
#define MPEG4_HDR_ERR       -2
#define MPEG4_MEM_ERR       -3
#define MPEG4_DEC_ERR       -4
#define MPEG4_VLD_ERR       -5
#define MPEG4_FORMAT_ERR    -6

#define MPEG4_IVOP_ERR  MPEG4_DEC_ERR
#define MPEG4_PVOP_ERR  MPEG4_DEC_ERR
#define MPEG4_BVOP_ERR  MPEG4_DEC_ERR
#define MPEG4_SVOP_ERR  MPEG4_DEC_ERR



#endif /*_MPEG4_RK_MACRO_H*/
