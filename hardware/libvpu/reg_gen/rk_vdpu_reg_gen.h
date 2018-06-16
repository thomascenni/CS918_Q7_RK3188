#ifndef _REG_GEN_H_
#define _REG_GEN_H_

#include "basetype.h"
#include "h264decapi.h"
#include "v4l2_codec_api.h"

namespace android {

typedef struct {
    u32 profile_idc;
    u32 chroma_format_idc;
    u32 max_frame_num;
    u32 frame_num_len;
    u32 frame_mb_only_flag;
    u32 mb_adaptive_frame_field_flag;
    u32 max_num_ref_frames;
    u32 pic_width_in_mbs;
    u32 pic_height_in_mbs;
    u32 direct_8x8_inference_flag;
} RK_VPU_INFO_SPS;

typedef struct {
    u32 entropy_coding_mode_flag;
    u32 num_ref_idx_l0_active;
    u32 num_ref_idx_l1_active;
    u32 weighted_pred_flag;
    u32 weighted_bipred_flag;
    u32 pic_init_qp;
    u32 chroma_qp_index_offset;
    u32 chroma_qp_index_offset2;
    u32 deblocking_filter_control_present_flag;
    u32 constrained_intra_pred_flag;
    u32 redundant_pic_cnt_present_flag;
    u32 transform_8x8_mode_flag;
    u32 scaling_matrix_present_flag;
    u8  *scaling_list;      // 8 * 64
} RK_VPU_INFO_PPS;

// field type
typedef enum
{
    PIC_TOP             = 0,
    PIC_BOT             = 1,
    PIC_FRAME           = 2,
} DPB_PIC_TYPE;

// field status
typedef enum {
    PIC_EMPTY           = 0x0,          // invalid or undecoded status
    PIC_UNUSED          = 0x1,          // valid but unreferenced status
    PIC_SHORT_TERM      = 0x2,          // valid and used as short term referenced
    PIC_LONG_TERM       = 0x3,          // valid and used as long  term referenced
    PIC_NON_EXISTING    = 0x4,          // valid but in frame gap, can not display
} DPB_PIC_STATUS;

typedef struct {
    u32 is_field;
    u32 is_displayed;
    DPB_PIC_STATUS status[2];
    i32 poc[2];                         // if empty, set to 0x7FFFFFFF
    i32 pic_num;                        // short -> no wrap pic_num, long -> long_pic_num
    u32 frame_num;                      // raw frame_num from slice header
} DPB_PIC;

typedef struct {
    DPB_PIC pics[16];                   // must be 16 elements
} RK_VPU_INFO_DPB;

typedef struct {
    u32 nal_ref_idc;
    u32 pps_id;
    u32 frame_num;
    u32 field_pic_flag;
    u32 bottom_field_flag;
    i32 poc;
    u32 poc_len;
    u32 is_idr;
    u32 idr_pic_id;
    u32 dec_ref_pic_marking_stream_len;
} RK_VPU_INFO_SLICE;

typedef struct {
    u8* stream_vir_addr;
    u32 stream_phy_addr;
    u32 stream_len;
    u32 stream_fd;
    u32 start_offset;
    u32 recon_addr;                     // reconstruct picture address
    u32 dir_mv_addr;                    // direct mv output address
    u32 *cabac_tab_addr;
    u32 dpb_pic_addr[16];
} RK_VPU_INFO_MEM;

u32 rk_vpu_set_sps(RK_VPU_INFO_SPS *info, decContainer_t* ctx);
u32 rk_vpu_set_pps(RK_VPU_INFO_PPS *info, decContainer_t* ctx);
u32 rk_vpu_set_dpb(RK_VPU_INFO_DPB *info, decContainer_t* ctx);
u32 rk_vpu_set_mem(RK_VPU_INFO_MEM *info, decContainer_t* ctx);
u32 rk_vpu_set_slice(RK_VPU_INFO_SLICE *info, decContainer_t* ctx);

u32 rk_vpu_send_info(RK_VPU_INFO_SPS *sps, RK_VPU_INFO_PPS *pps,
                     RK_VPU_INFO_DPB *dpb, RK_VPU_INFO_SLICE *slice,
                     RK_VPU_INFO_MEM *mem, decContainer_t* ctx);
u32 rk_vpu_check_info(decContainer_t* ctx, u32 *test);
u32 rk_vpu_regs_gen_test(decContainer_t* ctx);

}

#endif /* _REG_GEN_H_ */
