#include "framemanager.h"
#include "bitstream_rk.h"
#include "reg.h"
#include "mpeg4_rk_macro.h"
#include "vpu_mem.h"

typedef struct
{
    int method;

    int opaque;
    int transparent;
    int intra_cae;
    int inter_cae;
    int no_update;
    int upsampling;

    int intra_blocks;
    int inter_blocks;
    int inter4v_blocks;
    int gmc_blocks;
    int not_coded_blocks;

    int dct_coefs;
    int dct_lines;
    int vlc_symbols;
    int vlc_bits;

    int apm;
    int npm;
    int interpolate_mc_q;
    int forw_back_mc_q;
    int halfpel2;
    int halfpel4;

    int sadct;
    int quarterpel;
} ESTIMATION;

typedef struct MPEG4_HEADER
{
    /*General...*/
    RK_U32  mb_x;
    RK_U32  mb_y;
    RK_U32  mb_width;
    RK_U32  mb_height;
    RK_U32  totalMbInVop;

    RK_U32  width;    /*image width in name*/
    RK_U32  height;    /*image height in name*/
    RK_U32  image_width;   /*image width in memory*/
    RK_U32  image_height;  /*image height in memory*/
    RK_U32  width_offset;
    RK_U32  height_offset;
    RK_U32  display_width;
    RK_U32  display_height;

    RK_U32  quant;                      //OFFSET_OF_QUANT_IN_DEC
    RK_U32  intra_dc_threshold;
    RK_U32  fcode_forward;
    RK_U32  fcode_backward;
    RK_U32  rounding;
    RK_U32  vop_type;
    RK_U32  low_delay;
    RK_U32  vopcoded;

    RK_U32  quant_bits;
    RK_U32  quant_type;
    RK_U8   *mpeg_quant_matrices;       //be careful of  big-endian or little-endian
    RK_S32  quarterpel;
    RK_S32  packed_mode;  /* bframes packed bits? (1 = yes) */
    RK_U32  time_pp;
    RK_U32  time_bp;

    RK_S32  sprite_enable;
    RK_S32  sprite_warping_points;
    RK_S32  sprite_warping_accuracy;

    RK_U32  shape;
    RK_S32  ver_id;
    RK_U32  resync_marker_disable;

    RK_S32  time_inc_resolution;
    RK_U32  time_inc_bits;

    RK_S32  newpred_enable;
    RK_S32  reduced_resolution_enable;


    RK_U32  interlacing;
    RK_U32  top_field_first;
    RK_U32  is_short_video;
    RK_U32  alternate_vertical_scan;
    RK_U32  rlcmode;
    RK_S32  coding_type;
    RK_U32  version;

    RK_U32  last_time_base;
    RK_U32  time_base;
    RK_U32  time;
    RK_U32  last_non_b_time;
    RK_U32  frameNumber;

    VPU_FRAME   *frame_space;

    RK_S32  complexity_estimation_disable;

}
MPEG4_HEADER;

typedef enum MPEG4_DEC_CFG_MAP
{
    MPEG4_DEC_USE_PRESENTATION_TIME     =0x01,
}MPEG4_DEC_CFG_MAP;

class Mpeg4_Decoder
{

public:
    Mpeg4_Decoder();
    ~Mpeg4_Decoder();

    RK_S32      mpeg4_get_stride(MPEG4_HEADER *mp4Hdr);
    RK_S32      getVideoFrame(RK_U8*, RK_U32, VPU_POSTPROCESSING*);

    RK_S32      mpeg4_memory_init(MPEG4_HEADER *mp4Hdr);
    RK_S32      mpeg4_frame_init(MPEG4_HEADER *mp4Hdr);

    RK_S32      mpeg4_hwregister_init(void);
    RK_S32      mpeg4_hwregister_config(MPEG4_HEADER *mp4Hdr);
    RK_S32      mpeg4_hwregister_start();
    RK_S32      mpeg4_hwregister_waitfinish();

    RK_S32      mpeg4_hw_refbuf_init(void);
    RK_S32      mpeg4_hw_refbuf_config(MPEG4_HEADER *mp4Hdr);

    RK_S32      mpeg4_parse_header(MPEG4_HEADER *mp4Hdr);

    RK_S32      mpeg4_frame_manage(MPEG4_HEADER *mp4Hdr);

    RK_S32      mpeg4_decode_init(VPU_GENERIC *vpug);
    RK_S32      mpeg4_decode_deinit(VPU_GENERIC *vpug);
    RK_S32      mpeg4_decode_frame(void);
    RK_S32      mpeg4_decoder_endframe();
    VPU_FRAME*  mpeg4_decoder_displayframe(VPU_FRAME *frame);
    RK_S32      mpeg4_flush_oneframe_dpb(VPU_FRAME *frame);
    RK_S32      mpeg4_test_new_slice(void);
    RK_S32      mpeg4_dumpyuv(FILE *fp, VPU_FRAME *frame);
    void        reset_keyframestate();
	RK_S32      mpeg4_control(RK_U32 cmd_type, void* data);
    MPEG4_HEADER* getCurrentFrameHeader() {return mp4Hdr_Cur;}

public:
    int         socket;
    void        *vpumem_ctx;
private:
    RK_S32      decode012();
    void        set_intra_matrix(RK_U8 * mpeg_quant_matrices, RK_U8 * matrix);
    void        set_inter_matrix(RK_U8 * mpeg_quant_matrices, RK_U8 * matrix);
    void        mpeg4_read_vol_complexity_estimation_header(MPEG4_HEADER *mp4Hdr);
    void        mpeg4_read_vop_complexity_estimation_header(MPEG4_HEADER *mp4Hdr, int coding_type);

    RK_U32      GetPicsize(RK_U32 *width, RK_U32 *height);
    RK_S32      log2bin(RK_U32 value);
    void        bits_parse_matrix(RK_U8 * matrix);

private:

    bitstream       *Cbits;

    framemanager    *Cfrm;   //fr_manager;

    rkregister     *Cregs;

    VPUMemLinear_t  vpu_bits_mem;
    VPUMemLinear_t  vpu_q_mem;
    VPUMemLinear_t  vpu_mv_mem;

    RK_U32  mp4Regs[DEC_X170_REGISTERS];
    RK_U32  mp4RetRegs[DEC_X170_REGISTERS];

    RK_U32  isMpeg4;

    RK_U32  ImgWidth;                       //Image Width from Opencore
    RK_U32  ImgHeight;                      //Image Height from Opencore

    RK_U32  display_width;
    RK_U32  display_height;
    RK_U32  decode_width;
    RK_U32  decode_height;
    RK_U32  directMV_Addr;              //need alloc linear MV memory
    RK_U8   *qTable_VirAddr;                //need alloc linear Q memory
    RK_S32  time_inc;
    RK_S32  last_timestamp;
    ESTIMATION      estimation;

    RK_S32  vopType;
    RK_S32  cusomVersion;

    MPEG4_HEADER mp4Header[3];
    MPEG4_HEADER *mp4Hdr_Ref0;
    MPEG4_HEADER *mp4Hdr_Ref1;
    MPEG4_HEADER *mp4Hdr_Cur;

    RK_S32  mFlushDpbAtEos;
    int     frameX;
    RK_U32  mFlags;
};
