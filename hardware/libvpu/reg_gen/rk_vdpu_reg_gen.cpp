#define ALOG_TAG "reg_gen"
#include <utils/Errors.h>
#include "rk_vdpu_reg_gen.h"
#include "deccfg.h"
#include "rk_vdpu_regs.h"
#include <stdlib.h>

#define MODE_H264	        0
#define SHOW_SPS_INFO       0
#define SHOW_PPS_INFO       0
#define SHOW_DPB_INFO       0
#define SHOW_LIST_INFO      0
#define SHOW_SORT_PROC      0
#define USE_STD_LIST_INIT   1

namespace android {

static DPB_PIC_STATUS convert_status(dpbPictureStatus_e status)
{
    DPB_PIC_STATUS ret = PIC_EMPTY;
    switch (status) {
    case UNUSED : {
        ret = PIC_UNUSED;
    } break;
    case NON_EXISTING : {
        ret = PIC_NON_EXISTING;
    } break;
    case SHORT_TERM : {
        ret = PIC_SHORT_TERM;
    } break;
    case LONG_TERM : {
        ret = PIC_LONG_TERM;
    } break;
    case EMPTY : {
        ret = PIC_EMPTY;
    } break;
    default : {
        ret = PIC_EMPTY;
    } break;
    }
    return ret;
}

// check for start code is 000 or 001
static u32 check_start_code(u8 *str)
{
    if ((str[0] + str[1]) == 0 && str[2] < 2)
        return 1;
    else
        return 0;
}

static u32 _check_status_eq(DPB_PIC *p, DPB_PIC_TYPE type, DPB_PIC_STATUS status)
{
    u32 ret = 0;
    switch (type) {
    case PIC_TOP : {
        ret = (p->status[0] == status);
    } break;
    case PIC_BOT : {
        ret = (p->status[1] == status);
    } break;
    case PIC_FRAME : {
        ret = ((p->status[0] == status) &&
               (p->status[1] == status));
    } break;
    default : {
    } break;
    }
    return ret;
}

static u32 _check_status_ne(DPB_PIC *p, DPB_PIC_TYPE type, DPB_PIC_STATUS status)
{
    u32 ret = 0;
    switch (type) {
    case PIC_TOP : {
        ret = (p->status[0] != status);
    } break;
    case PIC_BOT : {
        ret = (p->status[1] != status);
    } break;
    case PIC_FRAME : {
        ret = ((p->status[0] != status) &&
               (p->status[1] != status));
    } break;
    default : {
    } break;
    }
    return ret;
}

// macro check: return check result
#define CHK_IS_VALID(p, type)       _check_status_ne((p), DPB_PIC_TYPE(type), PIC_EMPTY)

#define CHK_IS_EXIST(p, type)       (_check_status_ne((p), DPB_PIC_TYPE(type), PIC_EMPTY) && \
                                     _check_status_ne((p), DPB_PIC_TYPE(type), PIC_NON_EXISTING))
#define CHK_IS_REF(p, type)         (_check_status_eq((p), DPB_PIC_TYPE(type), PIC_SHORT_TERM) || \
                                     _check_status_eq((p), DPB_PIC_TYPE(type), PIC_LONG_TERM))
#define CHK_IS_ST_TERM(p, type)      _check_status_eq((p), DPB_PIC_TYPE(type), PIC_SHORT_TERM)
#define CHK_IS_LT_TERM(p, type)      _check_status_eq((p), DPB_PIC_TYPE(type), PIC_LONG_TERM)

// frame mode compare macro, used in list init
#define FRM_IS_VALID(p)              _check_status_ne((p), PIC_FRAME, PIC_EMPTY)
#define FRM_IS_ST_TERM(p)            _check_status_eq((p), PIC_FRAME, PIC_SHORT_TERM)
#define FRM_IS_LT_TERM(p)            _check_status_eq((p), PIC_FRAME, PIC_LONG_TERM)
#define FRM_IS_REF(p)               (FRM_IS_ST_TERM((p)) || FRM_IS_LT_TERM((p)))
#define FRM_IS_EXIST(p)             (_check_status_ne((p), PIC_FRAME, PIC_EMPTY) && \
                                     _check_status_ne((p), PIC_FRAME, PIC_NON_EXISTING))

// field mode compare macro, used in list init
#define FLD_IS_VALID(p)             (_check_status_ne((p), PIC_TOP, PIC_EMPTY)      || _check_status_ne((p), PIC_BOT, PIC_EMPTY))
#define FLD_IS_ST_TERM(p)           (_check_status_eq((p), PIC_TOP, PIC_SHORT_TERM) || _check_status_eq((p), PIC_BOT, PIC_SHORT_TERM))
#define FLD_IS_LT_TERM(p)           (_check_status_eq((p), PIC_TOP, PIC_LONG_TERM ) || _check_status_eq((p), PIC_BOT, PIC_LONG_TERM ))
#define FLD_IS_REF(p)               (FLD_IS_ST_TERM((p)) || FLD_IS_LT_TERM((p)))
#define FLD_IS_EXIST(p)          (!((_check_status_eq((p), PIC_TOP, PIC_EMPTY) || _check_status_eq((p), PIC_TOP, PIC_NON_EXISTING)) && \
                                    (_check_status_eq((p), PIC_BOT, PIC_EMPTY) || _check_status_eq((p), PIC_BOT, PIC_NON_EXISTING))))

static i32 pic_get_poc(DPB_PIC *p, DPB_PIC_TYPE type)
{
    i32 poc = 0;
    switch (type) {
    case PIC_TOP : {
        poc = p->poc[0];
    } break;
    case PIC_BOT : {
        poc = p->poc[1];
    } break;
    case PIC_FRAME : {
        poc = MIN(p->poc[0], p->poc[1]);
    } break;
    default : {
    } break;
    }
    return poc;
}

#define FRM_POC(p)                  (FRM_IS_ST_TERM((p))?(pic_get_poc((p), PIC_FRAME):0x7fffffff))
#define FLD_POC(p)                  (CHK_IS_ST_TERM((p), PIC_TOP)?(pic_get_poc((p), PIC_TOP)): \
                                     CHK_IS_ST_TERM((p), PIC_BOT)?(pic_get_poc((p), PIC_BOT)):0x7fffffff)

static RK_VPU_INFO_DPB *dpb_on_sort = NULL;
static i32 dpb_curr_poc = 0;
static u32 dpb_size     = 0;

static int pic_sort_frm_by_desc(const void *arg1, const void *arg2)
{
    u32 idx1 = *((u32*)arg1);
    u32 idx2 = *((u32*)arg2);
    DPB_PIC *pic1 = &dpb_on_sort->pics[idx1];
    DPB_PIC *pic2 = &dpb_on_sort->pics[idx2];
    i32 poc1 = pic_get_poc(pic1, PIC_FRAME);
    i32 poc2 = pic_get_poc(pic2, PIC_FRAME);
    if (poc1 < poc2)
        return 1;
    if (poc1 > poc2)
        return -1;
    else
        return 0;
}

static int pic_sort_frm_by_asc(const void *arg1, const void *arg2)
{
    u32 idx1 = *((u32*)arg1);
    u32 idx2 = *((u32*)arg2);
    DPB_PIC *pic1 = &dpb_on_sort->pics[idx1];
    DPB_PIC *pic2 = &dpb_on_sort->pics[idx2];
    i32 poc1 = pic_get_poc(pic1, PIC_FRAME);
    i32 poc2 = pic_get_poc(pic2, PIC_FRAME);
    if (poc1 < poc2)
        return -1;
    if (poc1 > poc2)
        return 1;
    else
        return 0;
}

static int pic_sort_p_slice_frm(u32 idx1, u32 idx2)
{
    DPB_PIC *pic1 = &dpb_on_sort->pics[idx1];
    DPB_PIC *pic2 = &dpb_on_sort->pics[idx2];
    u32 is_vld1 = FRM_IS_VALID(pic1);
    u32 is_vld2 = FRM_IS_VALID(pic2);
    u32 is_ref1 = FRM_IS_REF(pic1);
    u32 is_ref2 = FRM_IS_REF(pic2);
    u32 is_st1  = FRM_IS_ST_TERM(pic1);
    u32 is_st2  = FRM_IS_ST_TERM(pic2);
    u32 is_lt1  = FRM_IS_LT_TERM(pic1);
    u32 is_lt2  = FRM_IS_LT_TERM(pic2);
    i32 poc1    = pic_get_poc(pic1, PIC_FRAME);
    i32 poc2    = pic_get_poc(pic2, PIC_FRAME);

#if SHOW_SORT_PROC
    ALOGE("FRM P: idx %2d %2d  vld %d %d  ref %d %d  st %d %d  lt %d %d  poc %3d %3d",
        idx1,    idx2,
        is_vld1, is_vld2, is_ref1, is_ref2,
        is_st1,  is_st2,  is_lt1,  is_lt2,
        poc1, poc2);
#endif

    if (!is_vld1 && !is_vld2)
        return 0;
    else if (is_vld1 && !is_vld2)
        return (-1);
    else if (!is_vld1 && is_vld2)
        return (1);

    /* both are non-reference pictures, check if needed for display */
    if (!is_ref1 && !is_ref2) {
#if 0
        if (pic1->pic_num > pic2->pic_num)
            return (-1);
        else if (pic1->pic_num < pic2->pic_num)
            return (1);
        else
            return (0);
#else
        if      (!pic1->is_displayed &&  pic2->is_displayed)
            return -1;
        else if ( pic1->is_displayed && !pic2->is_displayed)
            return 1;
        else
            return 0;
#endif
    }
    /* only pic 1 needed for reference -> greater */
    else if (!is_ref2) {
        return (-1);
    }
    /* only pic 2 needed for reference -> greater */
    else if (!is_ref1) {
        return (1);
    }
    /* both are short term reference pictures -> check picNum */
    else if (is_st1 && is_st2) {
        if (pic1->pic_num > pic2->pic_num)
            return (-1);
        else if (pic1->pic_num < pic2->pic_num)
            return (1);
        else
            return (0);
    }
    /* only pic 1 is short term -> greater */
    else if (is_st1) {
        return (-1);
    }
    /* only pic 2 is short term -> greater */
    else if (is_st2) {
        return (1);
    }
    /* both are long term reference pictures -> check picNum (contains the
     * longTermPicNum */
    else {
        if (pic1->pic_num > pic2->pic_num)
            return (1);
        else if (pic1->pic_num < pic2->pic_num)
            return (-1);
        else
            return (0);
    }
}

static int pic_sort_b_slice_frm(u32 idx1, u32 idx2)
{
    DPB_PIC *pic1 = &dpb_on_sort->pics[idx1];
    DPB_PIC *pic2 = &dpb_on_sort->pics[idx2];
    u32 is_vld1 = FRM_IS_VALID(pic1);
    u32 is_vld2 = FRM_IS_VALID(pic2);
    u32 is_ref1 = FRM_IS_REF(pic1);
    u32 is_ref2 = FRM_IS_REF(pic2);
    u32 is_st1  = FRM_IS_ST_TERM(pic1);
    u32 is_st2  = FRM_IS_ST_TERM(pic2);
    u32 is_lt1  = FRM_IS_LT_TERM(pic1);
    u32 is_lt2  = FRM_IS_LT_TERM(pic2);
    i32 poc1    = pic_get_poc(pic1, PIC_FRAME);
    i32 poc2    = pic_get_poc(pic2, PIC_FRAME);

#if SHOW_SORT_PROC
    ALOGE("FRM B: idx %2d %2d  vld %d %d  ref %d %d  st %d %d  lt %d %d  poc %3d %3d",
        idx1,    idx2,
        is_vld1, is_vld2, is_ref1, is_ref2,
        is_st1,  is_st2,  is_lt1,  is_lt2,
        poc1, poc2);
#endif

    if (!is_vld1 && !is_vld2)
        return 0;
    else if (is_vld1 && !is_vld2)
        return (-1);
    else if (!is_vld1 && is_vld2)
        return (1);

    /* both are non-reference pictures, check if needed for display */
    if (!is_ref1 && !is_ref2) {
#if 0
        if (pic1->pic_num > pic2->pic_num)
            return (-1);
        else if (pic1->pic_num < pic2->pic_num)
            return (1);
        else
            return (0);
#else
        return 0;
#endif
    }
    /* only pic 1 needed for reference -> greater */
    else if (!is_ref2) {
        return (-1);
    }
    /* only pic 2 needed for reference -> greater */
    else if (!is_ref1) {
        return (1);
    }
    /* both are short term reference pictures -> check picNum */
    else if (is_st1 && is_st2) {
        if (poc1 < dpb_curr_poc && poc2 < dpb_curr_poc)
            return (poc1 < poc2 ? 1 : -1);
        else
            return (poc1 < poc2 ? -1 : 1);
    }
    /* only pic 1 is short term -> greater */
    else if (is_st1) {
        return (-1);
    }
    /* only pic 2 is short term -> greater */
    else if (is_st2) {
        return (1);
    }
    /* both are long term reference pictures -> check picNum (contains the
     * longTermPicNum */
    else {
        if (pic1->pic_num > pic2->pic_num)
            return (1);
        else if (pic1->pic_num < pic2->pic_num)
            return (-1);
        else
            return (0);
    }
}

static int pic_sort_p_slice_fld(u32 idx1, u32 idx2)
{
    DPB_PIC *pic1 = &dpb_on_sort->pics[idx1];
    DPB_PIC *pic2 = &dpb_on_sort->pics[idx2];
    u32 is_vld1 = FLD_IS_VALID(pic1);
    u32 is_vld2 = FLD_IS_VALID(pic2);
    u32 is_ref1 = FLD_IS_REF(pic1);
    u32 is_ref2 = FLD_IS_REF(pic2);
    u32 is_st1  = FLD_IS_ST_TERM(pic1);
    u32 is_st2  = FLD_IS_ST_TERM(pic2);
    u32 is_lt1  = FLD_IS_LT_TERM(pic1);
    u32 is_lt2  = FLD_IS_LT_TERM(pic2);
    i32 poc1    = pic_get_poc(pic1, PIC_FRAME);
    i32 poc2    = pic_get_poc(pic2, PIC_FRAME);

#if SHOW_SORT_PROC
    ALOGE("FLD P: idx %2d %2d  vld %d %d  ref %d %d  st %d %d  lt %d %d  poc %3d %3d",
        idx1,    idx2,
        is_vld1, is_vld2, is_ref1, is_ref2,
        is_st1,  is_st2,  is_lt1,  is_lt2,
        poc1, poc2);
#endif

    if (!is_vld1 && !is_vld2)
        return 0;
    else if (is_vld1 && !is_vld2)
        return (-1);
    else if (!is_vld1 && is_vld2)
        return (1);

    /* both are non-reference pictures, check if needed for display */
    if (!is_ref1 && !is_ref2) {
        return 0;
    }
    /* only pic 1 needed for reference -> greater */
    else if (!is_ref2) {
        return (-1);
    }
    /* only pic 2 needed for reference -> greater */
    else if (!is_ref1) {
        return (1);
    }
    /* both are short term reference pictures -> check picNum */
    else if (is_st1 && is_st2) {
        if (pic1->pic_num > pic2->pic_num)
            return (-1);
        else if (pic1->pic_num < pic2->pic_num)
            return (1);
        else
            return (0);
    }
    /* only pic 1 is short term -> greater */
    else if (is_st1) {
        return (-1);
    }
    /* only pic 2 is short term -> greater */
    else if (is_st2) {
        return (1);
    }
    /* both are long term reference pictures -> check picNum (contains the
     * longTermPicNum */
    else {
        if (pic1->pic_num > pic2->pic_num)
            return (1);
        else if (pic1->pic_num < pic2->pic_num)
            return (-1);
        else
            return (0);
    }
}

static int pic_sort_b_slice_fld(u32 idx1, u32 idx2)
{
    DPB_PIC *pic1 = &dpb_on_sort->pics[idx1];
    DPB_PIC *pic2 = &dpb_on_sort->pics[idx2];
    u32 is_vld1 = FLD_IS_VALID(pic1);
    u32 is_vld2 = FLD_IS_VALID(pic2);
    u32 is_ref1 = FLD_IS_REF(pic1);
    u32 is_ref2 = FLD_IS_REF(pic2);
    u32 is_st1  = FLD_IS_ST_TERM(pic1);
    u32 is_st2  = FLD_IS_ST_TERM(pic2);
    u32 is_lt1  = FLD_IS_LT_TERM(pic1);
    u32 is_lt2  = FLD_IS_LT_TERM(pic2);
    i32 poc1    = FRM_IS_ST_TERM(pic1) ?
                  pic_get_poc(pic1, PIC_FRAME) :
                  CHK_IS_ST_TERM(pic1, PIC_TOP) ? pic_get_poc(pic1, PIC_TOP) :
                  pic_get_poc(pic1, PIC_BOT);
    i32 poc2    = FRM_IS_ST_TERM(pic2) ?
                  pic_get_poc(pic2, PIC_FRAME) :
                  CHK_IS_ST_TERM(pic2, PIC_TOP) ? pic_get_poc(pic2, PIC_TOP) :
                  pic_get_poc(pic2, PIC_BOT);

#if SHOW_SORT_PROC
    ALOGE("FLD B: idx %2d %2d  vld %d %d  ref %d %d  st %d %d  lt %d %d  poc %3d %3d",
        idx1,    idx2,
        is_vld1, is_vld2, is_ref1, is_ref2,
        is_st1,  is_st2,  is_lt1,  is_lt2,
        poc1, poc2);
#endif

    if (!is_vld1 && !is_vld2)
        return 0;
    else if (is_vld1 && !is_vld2)
        return (-1);
    else if (!is_vld1 && is_vld2)
        return (1);

    /* both are non-reference pictures, check if needed for display */
    if (!is_ref1 && !is_ref2) {
        return 0;
    }
    /* only pic 1 needed for reference -> greater */
    else if (!is_ref2) {
        return (-1);
    }
    /* only pic 2 needed for reference -> greater */
    else if (!is_ref1) {
        return (1);
    }
    /* both are short term reference pictures -> check picNum */
    else if (is_st1 && is_st2) {
        if (poc1 <= dpb_curr_poc && poc2 <= dpb_curr_poc)
            return (poc1 < poc2 ? 1 : -1);
        else
            return (poc1 < poc2 ? -1 : 1);
    }
    /* only pic 1 is short term -> greater */
    else if (is_st1) {
        return (-1);
    }
    /* only pic 2 is short term -> greater */
    else if (is_st2) {
        return (1);
    }
    /* both are long term reference pictures -> check picNum (contains the
     * longTermPicNum */
    else {
        if (pic1->pic_num > pic2->pic_num)
            return (1);
        else if (pic1->pic_num < pic2->pic_num)
            return (-1);
        else
            return (0);
    }
}

void pic_sort(u32 * list, int (*func)(u32, u32))
{
    u32 i, j;
    u32 step;
    u32 tmp;
    u32 num = dpb_size + 1;
    //ALOGE("pic_sort_frm dpb size %d", num);
#if USE_STD_LIST_INIT
    step = 7;

    while(step) {
        for(i = step; i < num; i++) {
            tmp = list[i];
            j = i;
            while (j >= step && (func(list[j - step], tmp) > 0)) {
                //ALOGE("pic_sort_frm swap %2d %2d", j, j - step);
                list[j] = list[j - step];
                j -= step;
            }
            list[j] = tmp;
        }
        step >>= 1;
    }
#else
    u32 changed = 0;
    do {
        changed = 0;
        for (i = 1; i < num; i++) {
            int cmp = func(list[i-1], list[i]);
            if (cmp > 0) {
                tmp = list[i-1];
                list[i-1] = list[i];
                list[i] = tmp;
                changed = 1;
            }
        }
    } while (changed);
#endif
}


const HWIF_VDPU_E regs_list0[16] = {
    HWIF_BINIT_RLIST_F0, HWIF_BINIT_RLIST_F1, HWIF_BINIT_RLIST_F2,
    HWIF_BINIT_RLIST_F3, HWIF_BINIT_RLIST_F4, HWIF_BINIT_RLIST_F5,
    HWIF_BINIT_RLIST_F6, HWIF_BINIT_RLIST_F7, HWIF_BINIT_RLIST_F8,
    HWIF_BINIT_RLIST_F9, HWIF_BINIT_RLIST_F10, HWIF_BINIT_RLIST_F11,
    HWIF_BINIT_RLIST_F12, HWIF_BINIT_RLIST_F13, HWIF_BINIT_RLIST_F14,
    HWIF_BINIT_RLIST_F15
};

const HWIF_VDPU_E regs_list1[16] = {
    HWIF_BINIT_RLIST_B0, HWIF_BINIT_RLIST_B1, HWIF_BINIT_RLIST_B2,
    HWIF_BINIT_RLIST_B3, HWIF_BINIT_RLIST_B4, HWIF_BINIT_RLIST_B5,
    HWIF_BINIT_RLIST_B6, HWIF_BINIT_RLIST_B7, HWIF_BINIT_RLIST_B8,
    HWIF_BINIT_RLIST_B9, HWIF_BINIT_RLIST_B10, HWIF_BINIT_RLIST_B11,
    HWIF_BINIT_RLIST_B12, HWIF_BINIT_RLIST_B13, HWIF_BINIT_RLIST_B14,
    HWIF_BINIT_RLIST_B15
};

const HWIF_VDPU_E regs_listP[16] = {
    HWIF_PINIT_RLIST_F0, HWIF_PINIT_RLIST_F1, HWIF_PINIT_RLIST_F2,
    HWIF_PINIT_RLIST_F3, HWIF_PINIT_RLIST_F4, HWIF_PINIT_RLIST_F5,
    HWIF_PINIT_RLIST_F6, HWIF_PINIT_RLIST_F7, HWIF_PINIT_RLIST_F8,
    HWIF_PINIT_RLIST_F9, HWIF_PINIT_RLIST_F10, HWIF_PINIT_RLIST_F11,
    HWIF_PINIT_RLIST_F12, HWIF_PINIT_RLIST_F13, HWIF_PINIT_RLIST_F14,
    HWIF_PINIT_RLIST_F15
};

static void pic_frm_list_init(RkVdpuRegs *reg, RK_VPU_INFO_DPB *dpb, i32 curr_poc)
{
    // list index to dpb index map
    u32 dpb_idx = 0, i, pic_vld_num = 0;
    u32 pic_st[16], pic_st_num = 0, st_neg_num = 0, st_pos_num = 0;
    u32 pic_lt[16], pic_lt_num = 0;
    u32 list0[16], list1[16], listP[16];
    DPB_PIC *pic = &dpb->pics[0];

    // init status
    dpb_on_sort     = dpb;
    dpb_curr_poc    = curr_poc;
    for (i = 0; i < 16; i++) {
        pic_st[i] = pic_lt[i] = i;
        list0[i]  = list1[i]  = listP[i] = i;
    }

    // get total valid frames
    for (dpb_idx = 0; dpb_idx < 16; dpb_idx++) {
        if (FRM_IS_VALID(&pic[dpb_idx])) {
            pic_vld_num++;
        }
    }

    // get all short term reference frames
    for (dpb_idx = 0; dpb_idx < 16; dpb_idx++) {
        if (FRM_IS_ST_TERM(&pic[dpb_idx])) {
            ALOGE("st list idx %2d: %2d", pic_st_num, dpb_idx);
            pic_st[pic_st_num]  = dpb_idx;
            pic_st_num++;

            i32 ref_poc = pic_get_poc(&pic[dpb_idx], PIC_FRAME);
            if (ref_poc < curr_poc) {
                st_neg_num++;
            } else {
                st_pos_num++;
            }
        }
    }

    // get all long term reference frame
    for (dpb_idx = 0; dpb_idx < 16; dpb_idx++) {
        if (FRM_IS_LT_TERM(&pic[dpb_idx])) {
            ALOGE("lt list idx %2d: %2d", pic_st_num, dpb_idx);
            pic_lt[pic_lt_num] = dpb_idx;
            pic_lt_num++;
        }
    }

#if USE_STD_LIST_INIT
    // P slice
    pic_sort(listP, pic_sort_p_slice_frm);
    //qsort((void *)listP, 16, sizeof(u32), pic_sort_p_slice_frame);

    // B slice
    pic_sort(list0, pic_sort_b_slice_frm);
    //qsort((void *)list0, 16, sizeof(u32), pic_sort_b_slice_frame);
    // get list1
    // revert st order and copy list0 to list1
    for (i = 0; i < st_pos_num; i++) {
        list1[i] = list0[st_neg_num + i];
    }
    for (i = 0; i < st_neg_num; i++) {
        list1[st_pos_num + i] = list0[i];
    }
    for (i = pic_st_num; i < 16; i++) {
        list1[i] = list0[i];
    }

#else
    // sort short term frames by poc desending order
    qsort((void *)pic_st, pic_st_num, sizeof(u32), pic_sort_frm_by_desc);

    // sort long term frames by long term index desending order
    qsort((void *)pic_lt, pic_lt_num, sizeof(u32), pic_sort_frm_by_asc);

    // P slice
    // list0 = st_des + lt_asc
    // add short term first
    for (i = 0; i < pic_st_num; i++) {
        listP[i] = pic_st[i];
    }
    // then add long term
    for (i = 0; i < pic_lt_num; i++) {
        listP[i+pic_st_num] = pic_lt[i];
    }

    // B slice
    // list0 = st_neg_des + st_pos_asc + lt_asc
    // list1 = st_pos_asc + st_neg_des + lt_asc
    // copy all short term frame to list1
    // add negtive poc in list0
    for (i = 0; i < pic_st_num; i++) {
        list0[i] = pic_st[i];
    }
    // then add long term
    for (i = 0; i < pic_lt_num; i++) {
        list0[pic_st_num + i] = pic_lt[i];
    }
    // reorder the st_pos pics
    qsort(&list0[st_neg_num], st_pos_num, sizeof(u32), pic_sort_frm_by_asc);

    // revert st order and copy list0 to list1
    for (i = 0; i < st_pos_num; i++) {
        list1[i] = list0[st_neg_num + i];
    }
    for (i = 0; i < st_neg_num; i++) {
        list1[st_pos_num + i] = list0[i];
    }
    for (i = 0; i < pic_lt_num; i++) {
        list1[pic_st_num + i] = pic_lt[i];
    }
#endif

    // check list0 and list1 identical.
    // NOTE: in frame case list0 size equal to list1 size but in field case it is not.
    if (pic_st_num + pic_lt_num > 1) {
        u32 diff = 0;
        for (i = 0; i < pic_st_num + pic_lt_num; i++) {
            if (list0[i] != list1[i]) {
                diff = 1;
                break;
            }
        }

        if (!diff) {
            // if list0 and list1 are the same swap list1[0] and list1[1]
            u32 tmp = list1[0];
            list1[0] = list1[1];
            list1[1] = tmp;
        }
    }

    // write to hw regs
    for (i = 0; i < 16; i++) {
        reg->write(regs_listP[i], listP[i]);
        reg->write(regs_list0[i], list0[i]);
        reg->write(regs_list1[i], list1[i]);
    }

#if SHOW_LIST_INFO
    ALOGE("regs list info:");
    ALOGE("list0: %.2d %.2d %.2d %.2d %.2d %.2d %.2d %.2d %.2d %.2d %.2d %.2d %.2d %.2d %.2d %.2d",
        list0[0], list0[1], list0[ 2], list0[ 3], list0[ 4], list0[ 5], list0[ 6], list0[ 7],
        list0[8], list0[9], list0[10], list0[11], list0[12], list0[13], list0[14], list0[15]);
    ALOGE("list1: %.2d %.2d %.2d %.2d %.2d %.2d %.2d %.2d %.2d %.2d %.2d %.2d %.2d %.2d %.2d %.2d",
        list1[0], list1[1], list1[ 2], list1[ 3], list1[ 4], list1[ 5], list1[ 6], list1[ 7],
        list1[8], list1[9], list1[10], list1[11], list1[12], list1[13], list1[14], list1[15]);
    ALOGE("listP: %.2d %.2d %.2d %.2d %.2d %.2d %.2d %.2d %.2d %.2d %.2d %.2d %.2d %.2d %.2d %.2d",
        listP[0], listP[1], listP[ 2], listP[ 3], listP[ 4], listP[ 5], listP[ 6], listP[ 7],
        listP[8], listP[9], listP[10], listP[11], listP[12], listP[13], listP[14], listP[15]);
#endif
}

static void pic_fld_list_init(RkVdpuRegs *reg, RK_VPU_INFO_DPB *dpb, i32 curr_poc)
{
    // list index to dpb index map
    u32 dpb_idx = 0, i, pic_vld_num = 0;
    u32 pic_st[16], pic_st_num = 0, st_neg_num = 0, st_pos_num = 0;
    u32 pic_lt[16], pic_lt_num = 0;
    u32 list0[16], list1[16], listP[16];
    DPB_PIC *pic = &dpb->pics[0];

    // init status
    dpb_on_sort     = dpb;
    dpb_curr_poc    = curr_poc;
    for (i = 0; i < 16; i++) {
        pic_st[i] = pic_lt[i] = i;
        list0[i]  = list1[i]  = listP[i] = i;
    }

    // get total valid frames
    for (dpb_idx = 0; dpb_idx < 16; dpb_idx++) {
        if (FLD_IS_VALID(&pic[dpb_idx])) {
            pic_vld_num++;
        }
    }

    // get all short term reference frames
    for (dpb_idx = 0; dpb_idx < 16; dpb_idx++) {
        if (FLD_IS_ST_TERM(&pic[dpb_idx])) {
            ALOGE("st list idx %2d: %2d", pic_st_num, dpb_idx);
            pic_st[pic_st_num]  = dpb_idx;
            pic_st_num++;

            i32 ref_poc = FLD_POC(&pic[dpb_idx]);
            if (ref_poc <= curr_poc) {
                st_neg_num++;
            } else {
                st_pos_num++;
            }
        }
    }

    // get all long term reference frame
    for (dpb_idx = 0; dpb_idx < 16; dpb_idx++) {
        if (FLD_IS_LT_TERM(&pic[dpb_idx])) {
            ALOGE("lt list idx %2d: %2d", pic_st_num, dpb_idx);
            pic_lt[pic_lt_num] = dpb_idx;
            pic_lt_num++;
        }
    }

#if USE_STD_LIST_INIT
    // P slice
    pic_sort(listP, pic_sort_p_slice_fld);
    //qsort((void *)listP, 16, sizeof(u32), pic_sort_p_slice_frame);

    // B slice
    pic_sort(list0, pic_sort_b_slice_fld);
    //qsort((void *)list0, 16, sizeof(u32), pic_sort_b_slice_frame);
    // get list1
    // revert st order and copy list0 to list1
    for (i = 0; i < st_pos_num; i++) {
        list1[i] = list0[st_neg_num + i];
    }
    for (i = 0; i < st_neg_num; i++) {
        list1[st_pos_num + i] = list0[i];
    }
    for (i = pic_st_num; i < 16; i++) {
        list1[i] = list0[i];
    }
#else
    // sort short term frames by poc desending order
    qsort((void *)pic_st, pic_st_num, sizeof(u32), pic_sort_frm_by_desc);

    // sort long term frames by long term index desending order
    qsort((void *)pic_lt, pic_lt_num, sizeof(u32), pic_sort_frm_by_asc);

    // P slice
    // list0 = st_des + lt_asc
    // add short term first
    for (i = 0; i < pic_st_num; i++) {
        listP[i] = pic_st[i];
    }
    // then add long term
    for (i = 0; i < pic_lt_num; i++) {
        listP[i+pic_st_num] = pic_lt[i];
    }

    // B slice
    // list0 = st_neg_des + st_pos_asc + lt_asc
    // list1 = st_pos_asc + st_neg_des + lt_asc
    // copy all short term frame to list1
    // add negtive poc in list0
    for (i = 0; i < pic_st_num; i++) {
        list0[i] = pic_st[i];
    }
    // then add long term
    for (i = 0; i < pic_lt_num; i++) {
        list0[pic_st_num + i] = pic_lt[i];
    }
    // reorder the st_pos pics
    qsort(&list0[st_neg_num], st_pos_num, sizeof(u32), pic_sort_frm_by_asc);

    // revert st order and copy list0 to list1
    for (i = 0; i < st_pos_num; i++) {
        list1[i] = list0[st_neg_num + i];
    }
    for (i = 0; i < st_neg_num; i++) {
        list1[st_pos_num + i] = list0[i];
    }
    for (i = 0; i < pic_lt_num; i++) {
        list1[pic_st_num + i] = pic_lt[i];
    }
#endif

    // NOTE: field picture do not check list0 and list1 identical.

    // write to hw regs
    for (i = 0; i < 16; i++) {
        reg->write(regs_listP[i], listP[i]);
        reg->write(regs_list0[i], list0[i]);
        reg->write(regs_list1[i], list1[i]);
    }

#if SHOW_LIST_INFO
    ALOGE("regs list info:");
    ALOGE("list0: %.2d %.2d %.2d %.2d %.2d %.2d %.2d %.2d %.2d %.2d %.2d %.2d %.2d %.2d %.2d %.2d",
        list0[0], list0[1], list0[ 2], list0[ 3], list0[ 4], list0[ 5], list0[ 6], list0[ 7],
        list0[8], list0[9], list0[10], list0[11], list0[12], list0[13], list0[14], list0[15]);
    ALOGE("list1: %.2d %.2d %.2d %.2d %.2d %.2d %.2d %.2d %.2d %.2d %.2d %.2d %.2d %.2d %.2d %.2d",
        list1[0], list1[1], list1[ 2], list1[ 3], list1[ 4], list1[ 5], list1[ 6], list1[ 7],
        list1[8], list1[9], list1[10], list1[11], list1[12], list1[13], list1[14], list1[15]);
    ALOGE("listP: %.2d %.2d %.2d %.2d %.2d %.2d %.2d %.2d %.2d %.2d %.2d %.2d %.2d %.2d %.2d %.2d",
        listP[0], listP[1], listP[ 2], listP[ 3], listP[ 4], listP[ 5], listP[ 6], listP[ 7],
        listP[8], listP[9], listP[10], listP[11], listP[12], listP[13], listP[14], listP[15]);
#endif
}

const HWIF_VDPU_E regs_ref_base[16] = {
    HWIF_REFER0_BASE, HWIF_REFER1_BASE, HWIF_REFER2_BASE,
    HWIF_REFER3_BASE, HWIF_REFER4_BASE, HWIF_REFER5_BASE,
    HWIF_REFER6_BASE, HWIF_REFER7_BASE, HWIF_REFER8_BASE,
    HWIF_REFER9_BASE, HWIF_REFER10_BASE, HWIF_REFER11_BASE,
    HWIF_REFER12_BASE, HWIF_REFER13_BASE, HWIF_REFER14_BASE,
    HWIF_REFER15_BASE
};

const HWIF_VDPU_E regs_pic_num[16] = {
    HWIF_REFER0_NBR, HWIF_REFER1_NBR, HWIF_REFER2_NBR,
    HWIF_REFER3_NBR, HWIF_REFER4_NBR, HWIF_REFER5_NBR,
    HWIF_REFER6_NBR, HWIF_REFER7_NBR, HWIF_REFER8_NBR,
    HWIF_REFER9_NBR, HWIF_REFER10_NBR, HWIF_REFER11_NBR,
    HWIF_REFER12_NBR, HWIF_REFER13_NBR, HWIF_REFER14_NBR,
    HWIF_REFER15_NBR
};

u32 rk_vpu_set_sps(RK_VPU_INFO_SPS *info, decContainer_t* ctx)
{
    storage_t *storage = &ctx->storage;
    seqParamSet_t *sps = storage->activeSps;
    if (NULL == sps || NULL == info) {
        return BAD_VALUE;
    }

    info->profile_idc                   = sps->profileIdc;
    info->chroma_format_idc             = sps->chromaFormatIdc;
    info->max_frame_num                 = sps->maxFrameNum;
    u32 max_frame_num = info->max_frame_num;
    u32 frame_num_len = 0;
    while (max_frame_num >> frame_num_len)
        frame_num_len++;
    frame_num_len--;
    info->frame_num_len                 = frame_num_len;
    info->frame_mb_only_flag            = sps->frameMbsOnlyFlag;
    info->mb_adaptive_frame_field_flag  = sps->mbAdaptiveFrameFieldFlag;
    info->max_num_ref_frames            = sps->numRefFrames;
    info->pic_width_in_mbs              = sps->picWidthInMbs;
    info->pic_height_in_mbs             = sps->picHeightInMbs;
    info->direct_8x8_inference_flag     = sps->direct8x8InferenceFlag;

#if SHOW_SPS_INFO
    ALOGE("sps->profileIdc %d",                 sps->profileIdc);
    ALOGE("sps->chromaFormatIdc %d",            sps->chromaFormatIdc);
    ALOGE("sps->maxFrameNum %d",                sps->maxFrameNum);
    ALOGE("sps->frame_num_len %d",              frame_num_len);
    ALOGE("sps->frameMbsOnlyFlag %d",           sps->frameMbsOnlyFlag);
    ALOGE("sps->mbAdaptiveFrameFieldFlag %d",   sps->mbAdaptiveFrameFieldFlag);
    ALOGE("sps->numRefFrames %d",               sps->numRefFrames);
    ALOGE("sps->picWidthInMbs %d",              sps->picWidthInMbs);
    ALOGE("sps->picHeightInMbs %d",             sps->picHeightInMbs);
    ALOGE("sps->direct8x8InferenceFlag %d",     sps->direct8x8InferenceFlag);
#endif
    return OK;
}

u32 rk_vpu_set_pps(RK_VPU_INFO_PPS *info, decContainer_t* ctx)
{
    storage_t *storage = &ctx->storage;
    picParamSet_t *pps = storage->activePps;
    if (NULL == pps || NULL == info) {
        return BAD_VALUE;
    }
    info->entropy_coding_mode_flag                  = pps->entropyCodingModeFlag;
    info->num_ref_idx_l0_active                     = pps->numRefIdxL0Active;
    info->num_ref_idx_l1_active                     = pps->numRefIdxL1Active;
    info->weighted_pred_flag                        = pps->weightedPredFlag;
    info->weighted_bipred_flag                      = pps->weightedBiPredIdc;
    info->pic_init_qp                               = pps->picInitQp;
    info->chroma_qp_index_offset                    = pps->chromaQpIndexOffset;
    info->chroma_qp_index_offset2                   = pps->chromaQpIndexOffset2;
    info->deblocking_filter_control_present_flag    = pps->deblockingFilterControlPresentFlag;
    info->constrained_intra_pred_flag               = pps->constrainedIntraPredFlag;
    info->redundant_pic_cnt_present_flag            = pps->redundantPicCntPresentFlag;
    info->transform_8x8_mode_flag                   = pps->transform8x8Flag;
    info->scaling_matrix_present_flag               = pps->scalingMatrixPresentFlag;
    info->scaling_list                              = &pps->scalingList[0][0];

#if SHOW_PPS_INFO
    ALOGE("pps->entropyCodingModeFlag %d",              pps->entropyCodingModeFlag);
    ALOGE("pps->numRefIdxL0Active %d",                  pps->numRefIdxL0Active);
    ALOGE("pps->numRefIdxL1Active %d",                  pps->numRefIdxL1Active);
    ALOGE("pps->weightedPredFlag %d",                   pps->weightedPredFlag);
    ALOGE("pps->weightedBiPredIdc %d",                  pps->weightedBiPredIdc);
    ALOGE("pps->picInitQp %d",                          pps->picInitQp);
    ALOGE("pps->chromaQpIndexOffset %d",                pps->chromaQpIndexOffset);
    ALOGE("pps->chromaQpIndexOffset2 %d",               pps->chromaQpIndexOffset2);
    ALOGE("pps->deblockingFilterControlPresentFlag %d", pps->deblockingFilterControlPresentFlag);
    ALOGE("pps->constrainedIntraPredFlag %d",           pps->constrainedIntraPredFlag);
    ALOGE("pps->redundantPicCntPresentFlag %d",         pps->redundantPicCntPresentFlag);
    ALOGE("pps->transform8x8Flag %d",                   pps->transform8x8Flag);
    ALOGE("pps->scalingMatrixPresentFlag %d",           pps->scalingMatrixPresentFlag);
    ALOGE("pps->scaling_list %p",                       &pps->scalingList[0][0]);
#endif
    return OK;
}

u32 rk_vpu_set_dpb(RK_VPU_INFO_DPB *info, decContainer_t* ctx)
{
    if (NULL == ctx || NULL == info) {
        return BAD_VALUE;
    }
    memset(info, 0, sizeof(RK_VPU_INFO_DPB));

    storage_t *storage      = &ctx->storage;
    dpbStorage_t *dpb       = storage->dpb;
    u32 idx = 0, i = 0;

    for (i = 0; i < 16; i++) {
        dpbPicture_t *dpb_pic = dpb->buffer[i];
        if (dpb_pic) {
#if USE_STD_LIST_INIT
            DPB_PIC *pic = &info->pics[i];
#else
            DPB_PIC *pic = &info->pics[idx++];
#endif
            pic->is_field       = dpb_pic->isFieldPic;
            pic->is_displayed   = !dpb_pic->toBeDisplayed;
            pic->status[0]      = convert_status(dpb_pic->status[0]);
            pic->status[1]      = convert_status(dpb_pic->status[1]);
            pic->poc[0]         = dpb_pic->picOrderCnt[0];
            pic->poc[1]         = dpb_pic->picOrderCnt[1];
            pic->frame_num      = dpb_pic->frameNum;
            pic->pic_num        = dpb_pic->picNum;
        }
    }

#if SHOW_DPB_INFO
    for (idx = 0; idx < 16; idx++) {
        DPB_PIC *pic = &info->pics[idx];
        ALOGE("dpb info: index %.2d fld %d show %d status %d %d poc %3d %3d frm %3d pic %3d",
            idx,
            pic->is_field,
            pic->is_displayed,
            pic->status[0], pic->status[1],
            pic->poc[0], pic->poc[1],
            pic->frame_num,
            pic->pic_num);
    }
#endif
    return OK;
}

u32 rk_vpu_set_mem(RK_VPU_INFO_MEM *info, decContainer_t* ctx)
{
    if (NULL == ctx || NULL == info) {
        return BAD_VALUE;
    }

    storage_t *storage      = &ctx->storage;
    dpbStorage_t *dpb       = storage->dpb;
    info->stream_vir_addr   = (u8*)ctx->pHwStreamStart;
    info->stream_phy_addr   = ctx->hwStreamStartBus + ctx->hwStreamStartOffset;
    info->stream_len        = ctx->hwLength;
    info->start_offset      = (ctx->hwStreamStartOffset & 0x7) * 8;
    info->recon_addr        = dpb->currentOut->data->phy_addr;
    u32 dir_mv_offset       = dpb->dirMvOffset +
                             (storage->sliceHeader->bottomFieldFlag?(dpb->picSizeInMbs*32):0);
    info->dir_mv_addr       = info->recon_addr + dir_mv_offset;
    info->cabac_tab_addr    = (u32 *)ctx->asicBuff->cabacInit.phy_addr;
    memset(info->dpb_pic_addr, 0, sizeof(info->dpb_pic_addr));
    u32 idx = 0, i = 0;
    for (i = 0; i < 16; i++) {
        dpbPicture_t *dpb_pic = dpb->buffer[i];
        if (dpb_pic) {
#if USE_STD_LIST_INIT
            info->dpb_pic_addr[i] = dpb_pic->data->phy_addr;
#else
            info->dpb_pic_addr[idx++] = dpb_pic->data->phy_addr;
#endif
        }
    }
    return OK;
}

u32 rk_vpu_set_slice(RK_VPU_INFO_SLICE *info, decContainer_t* ctx)
{
    if (NULL == ctx || NULL == info) {
        return BAD_VALUE;
    }

    storage_t *storage      = &ctx->storage;
    sliceHeader_t *slice    = storage->sliceHeader;
    info->nal_ref_idc       = storage->prevNalUnit->nalRefIdc;
    info->pps_id            = slice->picParameterSetId;
    info->frame_num         = slice->frameNum;
    info->field_pic_flag    = slice->fieldPicFlag;
    info->bottom_field_flag = slice->bottomFieldFlag;
    info->idr_pic_id        = slice->idrPicId;
    info->dec_ref_pic_marking_stream_len    = slice->decRefPicMarking.strmLen;
    info->is_idr            = IS_IDR_NAL_UNIT(storage->prevNalUnit);
    i32 poc = (slice->fieldPicFlag && slice->bottomFieldFlag) ?
              (storage->poc[0].picOrderCnt[1]) : (storage->poc[0].picOrderCnt[0]);
    info->poc               = poc;
    info->poc_len           = slice->pocLengthHw;
    dpb_size                = storage->dpb->dpbSize;
    return OK;
}

u32 rk_vpu_send_info(RK_VPU_INFO_SPS *sps, RK_VPU_INFO_PPS *pps,
                     RK_VPU_INFO_DPB *dpb, RK_VPU_INFO_SLICE *slice,
                     RK_VPU_INFO_MEM *mem, decContainer_t* ctx)
{
    RkVdpuRegs regs;
    u32 i;
    u32 need_mvs    = 0;
    u32 mono_chrome = 0;

    // hardware basic info init
    regs.write(HWIF_DEC_OUT_ENDIAN,		DEC_X170_LITTLE_ENDIAN);
    regs.write(HWIF_DEC_IN_ENDIAN,  	DEC_X170_BIG_ENDIAN);
    regs.write(HWIF_DEC_STRENDIAN_E,	DEC_X170_LITTLE_ENDIAN);
    regs.write(HWIF_DEC_OUT_TILED_E,   	DEC_X170_OUTPUT_FORMAT_RASTER_SCAN);
    regs.write(HWIF_DEC_MAX_BURST,		DEC_X170_BUS_BURST_LENGTH_16);
	regs.write(HWIF_DEC_SCMD_DIS, 		DEC_X170_SCMD_DISABLE);
	regs.write(HWIF_DEC_ADV_PRE_DIS, 	0);
	regs.write(HWIF_APF_THRESHOLD, 		8);

    regs.write(HWIF_DEC_LATENCY,		0);
    regs.write(HWIF_DEC_DATA_DISC_E,	0);
    regs.write(HWIF_DEC_OUTSWAP32_E,	1);
    regs.write(HWIF_DEC_INSWAP32_E,		1);
    regs.write(HWIF_DEC_STRSWAP32_E,	1);
	regs.write(HWIF_DEC_TIMEOUT_E, 		1);
    regs.write(HWIF_DEC_CLK_GATE_E, 	1);
	regs.write(HWIF_DEC_IRQ_DIS, 		0);
    regs.write(HWIF_DEC_E, 		        1);

    regs.write(HWIF_DEC_OUT_DIS,        0);
    regs.write(HWIF_CH_8PIX_ILEAV_E,    0);
	regs.write(HWIF_DEC_IRQ_STAT,       0);
	regs.write(HWIF_DEC_IRQ,            0);


    //DEC_SET_APF_THRESHOLD(regs.base());

    /* set AXI RW IDs */
    regs.write(HWIF_DEC_AXI_RD_ID,      0xff);
    regs.write(HWIF_DEC_AXI_WR_ID,      0xff);

    /* global define */
	regs.write(HWIF_DEC_MODE,           MODE_H264);
    regs.write(HWIF_PRED_BC_TAP_0_0,    1);
    regs.write(HWIF_PRED_BC_TAP_0_1,    (u32) (-5));
    regs.write(HWIF_PRED_BC_TAP_0_2,    20);

    regs.write(HWIF_RLC_MODE_E,         0);
    regs.write(HWIF_CH_8PIX_ILEAV_E,    0);

    // sps info
    need_mvs    = (sps->profile_idc > 66)?(1):(0);
    mono_chrome = (sps->chroma_format_idc == 0);
    regs.write(HWIF_BLACKWHITE_E,       sps->profile_idc >= 100 && sps->chroma_format_idc == 0);
    regs.write(HWIF_FIELDPIC_FLAG_E,   !sps->frame_mb_only_flag);
    regs.write(HWIF_SEQ_MBAFF_E,        sps->mb_adaptive_frame_field_flag);
    regs.write(HWIF_REF_FRAMES,         sps->max_num_ref_frames);
    regs.write(HWIF_FRAMENUM_LEN,       sps->frame_num_len);
	regs.write(HWIF_PIC_MB_WIDTH,       sps->pic_width_in_mbs);
	regs.write(HWIF_PIC_MB_HEIGHT_P,    sps->pic_height_in_mbs);
    regs.write(HWIF_DIR_8X8_INFER_E,    sps->direct_8x8_inference_flag);
    regs.write(HWIF_PICORD_COUNT_E,     need_mvs);

    // pps info
    regs.write(HWIF_CABAC_E,            pps->entropy_coding_mode_flag);
    regs.write(HWIF_REFIDX0_ACTIVE,     pps->num_ref_idx_l0_active);
    regs.write(HWIF_REFIDX1_ACTIVE,     pps->num_ref_idx_l1_active);
    regs.write(HWIF_WEIGHT_PRED_E,      pps->weighted_pred_flag);
    regs.write(HWIF_WEIGHT_BIPR_IDC,    pps->weighted_bipred_flag);
    regs.write(HWIF_INIT_QP,            pps->pic_init_qp);
    regs.write(HWIF_CH_QP_OFFSET,       pps->chroma_qp_index_offset);
    regs.write(HWIF_CH_QP_OFFSET2,      pps->chroma_qp_index_offset2);
    regs.write(HWIF_FILT_CTRL_PRES,     pps->deblocking_filter_control_present_flag);
    regs.write(HWIF_CONST_INTRA_E,      pps->constrained_intra_pred_flag);
    regs.write(HWIF_RDPIC_CNT_PRES,     pps->redundant_pic_cnt_present_flag);
    regs.write(HWIF_8X8TRANS_FLAG_E,    pps->transform_8x8_mode_flag);
    regs.write(HWIF_TYPE1_QUANT_E,      pps->scaling_matrix_present_flag);

    if (pps->scaling_matrix_present_flag) {
        // copy scaling matrix table to the end of cabac table
#if 0
        u32 i, j, tmp;
        u32 *p;

        u8*scalingList = pps->scaling_list;

        #define CABAC_INIT_BUFFER_SIZE 3680/* bytes */
        #define SCALING_LIST_SIZE      6*16+2*64
        #define POC_BUFFER_SIZE        34*4

        p = (u32 *) ((u8 *)mem->cabac_tab_addr + CABAC_INIT_BUFFER_SIZE + POC_BUFFER_SIZE);
        for(i = 0; i < 6; i++)
        {
            for(j = 0; j < 4; j++)
            {
                tmp = (scalingList[i * 64 + 4 * j + 0] << 24) |
                      (scalingList[i * 64 + 4 * j + 1] << 16) |
                      (scalingList[i * 64 + 4 * j + 2] <<  8) |
                      (scalingList[i * 64 + 4 * j + 3]);
                *p++ = tmp;
            }
        }
        for(i = 6; i < 8; i++)
        {
            for(j = 0; j < 16; j++)
            {
                tmp = (scalingList[i * 64 + 4 * j + 0] << 24) |
                      (scalingList[i * 64 + 4 * j + 1] << 16) |
                      (scalingList[i * 64 + 4 * j + 2] <<  8) |
                      (scalingList[i * 64 + 4 * j + 3]);
                *p++ = tmp;
            }
        }
#endif
    }

    // init list for both P slice and B slice
    if (!slice->field_pic_flag) {
        pic_frm_list_init(&regs, dpb, slice->poc);
    } else {
        pic_fld_list_init(&regs, dpb, slice->poc);
    }

    // valid flag and long term flag generation
    u32 lt_flags    = 0;
    u32 vld_flags   = 0;
    if (slice->field_pic_flag) {
        for(i = 0; i < 32; i++) {
            if (CHK_IS_LT_TERM(&dpb->pics[i/2], i&1)) {
                lt_flags    = (lt_flags << 1) | 1;
            } else {
                lt_flags  <<= 1;
            }
            if (CHK_IS_EXIST(&dpb->pics[i/2], i&1)) {
                vld_flags   = (vld_flags << 1) | 1;
            } else {
                vld_flags <<= 1;
            }
        }
    } else {
        for(i = 0; i < 16; i++) {
            if (CHK_IS_LT_TERM(&dpb->pics[i], PIC_FRAME)) {
                lt_flags    = (lt_flags << 1) | 1;
            } else {
                lt_flags  <<= 1;
            }
            if (CHK_IS_REF(&dpb->pics[i], PIC_FRAME)) {
                vld_flags   = (vld_flags << 1) | 1;
            } else {
                vld_flags <<= 1;
            }
        }
        lt_flags    <<= 16;
        vld_flags   <<= 16;
    }
    regs.write(HWIF_REFER_LTERM_E, lt_flags);
    regs.write(HWIF_REFER_VALID_E, vld_flags);

    // setup pic_num for st and lt_idx for lt
    for(i = 0; i < 16; i++) {
        if (FLD_IS_EXIST(&dpb->pics[i])) {
            regs.write(regs_pic_num[i], dpb->pics[i].frame_num);
        }
    }

    // slice header info
    regs.write(HWIF_WRITE_MVS_E,        need_mvs && (slice->nal_ref_idc != 0));
    regs.write(HWIF_PPS_ID,             slice->pps_id);
    regs.write(HWIF_FRAMENUM,           slice->frame_num);
    regs.write(HWIF_PIC_INTERLACE_E,    !sps->frame_mb_only_flag &&
                                        (sps->mb_adaptive_frame_field_flag || slice->field_pic_flag));
    regs.write(HWIF_PIC_FIELDMODE_E,    slice->field_pic_flag);
    regs.write(HWIF_PIC_TOPFIELD_E,     !slice->bottom_field_flag);
    regs.write(HWIF_IDR_PIC_E,          slice->is_idr);
    regs.write(HWIF_IDR_PIC_ID,         slice->idr_pic_id);
    regs.write(HWIF_REFPIC_MK_LEN,      slice->dec_ref_pic_marking_stream_len);
    regs.write(HWIF_POC_LENGTH,         slice->poc_len);


    // memory info
    RK_U32 have_start_code = check_start_code((u8*)mem->stream_vir_addr);
    regs.write(HWIF_START_CODE_E,       have_start_code);
    regs.write(HWIF_STRM_START_BIT,     mem->start_offset);
    regs.write(HWIF_RLC_VLC_BASE,       mem->stream_phy_addr);
    u32 recon_addr = mem->recon_addr;
    if(slice->field_pic_flag && slice->bottom_field_flag) {
        recon_addr += sps->pic_width_in_mbs * 16;
    }
    regs.write(HWIF_DEC_OUT_BASE,       recon_addr);
    if (need_mvs) {
        regs.write(HWIF_DIR_MV_BASE,    mem->dir_mv_addr);
    }

    // fill dpb buffer address
    for (i = 0; i < 16; i++) {
        DPB_PIC *pic = &dpb->pics[i];
        u32 phy_addr = 0;
        if (FLD_IS_EXIST(pic)) {
            phy_addr = mem->dpb_pic_addr[i];
        } else {
            phy_addr = mem->recon_addr;
        }
        if (pic->is_field)
            phy_addr |= 2;
        i32 curr_poc = slice->poc;
        i32 diff_top = ABS(pic->poc[0] - curr_poc);
        i32 diff_bot = ABS(pic->poc[1] - curr_poc);
        if (diff_top < diff_bot)
            phy_addr |= 1;
        regs.write(regs_ref_base[i], phy_addr);
    }

    ALOGE("HWIF_STREAM_LEN %x", mem->stream_len);
    regs.write(HWIF_STREAM_LEN,         mem->stream_len);
    regs.write(HWIF_QTABLE_BASE,        (RK_U32)mem->cabac_tab_addr);

    // debug interface
    //regs.setLog(1);
    //regs.setLog(0);

    // check regs
    rk_vpu_check_info(ctx, regs.base());
    return OK;
}

u32 rk_vpu_check_info(decContainer_t* ctx, u32 *test)
{
    u32 *golden = ctx->h264Regs;
    ALOGE("checking frame %u", ctx->storage.sliceHeader->picOrderCntLsb);
    u32 ret = OK;

    // from 56 to 60 regs are read only
    for (u32 i = 1; i < 56; i++) {
        if (i == 50 || i == 52 || i == 53 || i == 54) {
            // 50 is read only
            continue;
        }

        if (golden[i] != test[i]) {
            ret = -1;
            ALOGE("error on %3d golden %.8x test %.8x", i, golden[i], test[i]);
        }
    }

    if (ret) {
        ALOGE("error found");
        usleep(500*1000);
        exit(0);
    } else {
        ALOGE("\n");
        ALOGE("done without error");
        ALOGE("\n");
    }
    return ret;
}

u32 rk_vpu_regs_gen_test(decContainer_t* ctx)
{
    RK_VPU_INFO_SPS sps;
    RK_VPU_INFO_PPS pps;
    RK_VPU_INFO_DPB dpb;
    RK_VPU_INFO_SLICE slice;
    RK_VPU_INFO_MEM mem;

    rk_vpu_set_sps(&sps, ctx);
    rk_vpu_set_pps(&pps, ctx);
    rk_vpu_set_dpb(&dpb, ctx);
    rk_vpu_set_slice(&slice, ctx);
    rk_vpu_set_mem(&mem, ctx);

    rk_vpu_send_info(&sps, &pps, &dpb, &slice, &mem, ctx);

    return OK;
}

}

