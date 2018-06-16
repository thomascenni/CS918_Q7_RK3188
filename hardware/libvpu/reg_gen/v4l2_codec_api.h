/*
 * All structures are passed together with a queued stream buffer
 * in separate planes as needed.
 * Only v4l2_h264_slice_param and v4l2_h264_decode_param have
 * to be queued with every slice.
 */
struct v4l2_h264_sps {
	u8 profile_idc;
	u8 constraint_set0_flag :1;
	u8 constraint_set1_flag :1;
	u8 constraint_set2_flag :1;
	u8 constraint_set3_flag :1;
	u8 constraint_set4_flag :1;
	u8 constraint_set5_flag :1;
	u8 level_idc;
	u8 seq_parameter_set_id;
	u8 chroma_format_idc;
	u8 separate_colour_plane_flag :1;
	u8 bit_depth_luma_minus8;
	u8 bit_depth_chroma_minus8;
	u8 qpprime_y_zero_transform_bypass_flag :1;
	u16 max_frame_num;
	u8 pic_order_cnt_type;
	u16 max_pic_order_cnt;
	u8 delta_pic_order_always_zero_flag :1;
	s32 offset_for_non_ref_pic;
	s32 offset_for_top_to_bottom_field;
	u8 num_ref_frames_in_pic_order_cnt_cycle;
	s32 offset_for_ref_frame[255];
	u8 max_num_ref_frames;
	u8 gaps_in_frame_num_value_allowed_flag :1;
	u16 pic_width_in_mbs;
	u16 pic_height_in_map_units;
	u8 frame_mbs_only_flag :1;
	u8 mb_adaptive_frame_field_flag :1;
	u8 direct_8x8_inference_flag :1;
};

struct v4l2_h264_pps {
	u8 pic_parameter_set_id;
	u8 seq_parameter_set_id;
	u8 entropy_coding_mode_flag :1;
	u8 bottom_field_pic_order_in_frame_present_flag :1;
	u8 num_slice_groups_minus_1;
	u8 num_ref_idx_l0_default_active_minus1;
	u8 num_ref_idx_l1_default_active_minus1;
	u8 weighted_pred_flag :1;
	u8 weighted_bipred_idc;
	s8 pic_init_qp_minus26;
	s8 pic_init_qs_minus26;
	s8 chroma_qp_index_offset;
	u8 deblocking_filter_control_present_flag :1;
	u8 constrained_intra_pred_flag :1;
	u8 redundant_pic_cnt_present_flag :1;
	u8 transform_8x8_mode_flag :1;
	u8 pic_scaling_matrix_present_flag :1;
	s8 second_chroma_qp_index_offset;
};

struct v4l2_h264_scaling_matrix {
	u8 scaling_list_4x4[6][16];
	u8 scaling_list_8x8[6][64];
};

struct v4l2_h264_weight_factors {
	u8 luma_weight_flag :1;
	u8 chroma_weight_flag :1;
	s8 luma_weight[32];
	s8 luma_offset[32];
	s8 chroma_weight[32][2];
	s8 chroma_offset[32][2];
};

struct v4l2_h264_pred_weight_table {
	u8 luma_log2_weight_denom;
	u8 chroma_log2_weight_denom;
	struct v4l2_h264_weight_factors weight_factors[2];
};

struct v4l2_h264_slice_param {
	u16 first_mb_in_slice;
	u8 slice_type;
	u8 pic_parameter_set_id;
	u8 colour_plane_id;
	u16 frame_num;
	u8 field_pic_flag :1;
	u8 bottom_field_flag :1;
	u16 idr_pic_id;
	u16 pic_order_cnt_lsb;
	s32 delta_pic_order_cnt_bottom;
	s32 delta_pic_order_cnt0;
	s32 delta_pic_order_cnt1;
	u8 redundant_pic_cnt;
	u8 direct_spatial_mv_pred_flag :1;

	struct v4l2_h264_pred_weight_table pred_weight_table;

	u8 cabac_init_idc;
	s8 slice_qp_delta;
	u8 sp_for_switch_flag :1;
	s8 slice_qs_delta;
	u8 disable_deblocking_filter_idc;
	s8 slice_alpha_c0_offset_div2;
	s8 slice_beta_offset_div2;
	u32 slice_group_change_cycle;

	u8 num_ref_idx_l0_active_minus1;
	u8 num_ref_idx_l1_active_minus1;
	u32 ref_pic_list0[32];
	u32 ref_pic_list1[32];
};

/* If not set, this entry is unused for reference. */
#define V4L2_H264_DPB_ENTRY_FLAG_ACTIVE		0x0001
#define V4L2_H264_DPB_ENTRY_FLAG_LONG_TERM	0x0002

/* Note that field is indicated by v4l2_buffer.field */
struct v4l2_h264_dpb_entry {
	u32 buf_index; /* v4l2_buffer index */
	u16 frame_num;
	u16 pic_num;
	u16 top_field_order_cnt;
	u16 bottom_field_order_cnt;
	u32 flags; /* V4L2_H264_DPB_ENTRY_FLAG_* */
};

struct v4l2_h264_decode_param {
	u8 idr_pic_flag;
	u8 nal_ref_idc;
	u32 buf_index;  /* Target v4l2_buffer index for decode */
	struct v4l2_h264_dpb_entry dpb[16];
};
