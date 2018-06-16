/*******************************************************************************************************
	File:
		mpeg4_rk_decoder.cpp
	Description:
		Mpeg4 Rk Decoder Routine
	Author:
		Jian Huan
	Date:
		2010-11-17 13:38:14
 ******************************************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#ifdef WIN32
#include <memory.h>
#else
#include <string.h>
#endif
#include "bitstream_rk.h"
#include "mpeg4_rk_macro.h"
#include "mpeg4_rk_decoder.h"
#include "vpu.h"
#define READ_MARKER()	Cbits->skipbits( 1)
#define LOG_TAG "MPEG4_DEC"
#include <utils/Log.h>
#ifdef AVS40
#undef ALOGV
#define ALOGV LOGV

#undef ALOGD
#define ALOGD LOGD

#undef ALOGI
#define ALOGI LOGI

#undef ALOGW
#define ALOGW LOGW

#undef ALOGE
#define ALOGE LOGE

#endif

////////////////////////////////////////////////////////////////////////////////////////////////////////
RK_U8 default_intra_matrix[64] =
{
    8, 17, 18, 19, 21, 23, 25, 27,
    17, 18, 19, 21, 23, 25, 27, 28,
    20, 21, 22, 23, 24, 26, 28, 30,
    21, 22, 23, 24, 26, 28, 30, 32,
    22, 23, 24, 26, 28, 30, 32, 35,
    23, 24, 26, 28, 30, 32, 35, 38,
    25, 26, 28, 30, 32, 35, 38, 41,
    27, 28, 30, 32, 35, 38, 41, 45
};

RK_U8 default_inter_matrix[64] =
{
    16, 17, 18, 19, 20, 21, 22, 23,
    17, 18, 19, 20, 21, 22, 23, 24,
    18, 19, 20, 21, 22, 23, 24, 25,
    19, 20, 21, 22, 23, 24, 26, 27,
    20, 21, 22, 23, 25, 26, 27, 28,
    21, 22, 23, 24, 26, 27, 28, 30,
    22, 23, 24, 26, 27, 28, 30, 31,
    23, 24, 25, 27, 28, 30, 31, 33
};

RK_U8 intra_dc_threshold_table[] =
{
    32,       /* never use */
    13,
    15,
    17,
    19,
    21,
    23,
    1,
};

#if (defined(WIN32) && defined(PRINT_OUTPUT_ENABLE))
char int2char(int type)
{
	if (type == 0)
		return 'I';
	else if (type == 1)
		return 'P';
	else if (type == 2)
		return 'B';
	else if (type == 4)
		return 'N';
	else
		return 'X';
}
#endif
////////////////////////////////////////////////////////////////////////////////////////////////////////

Mpeg4_Decoder::Mpeg4_Decoder()
{
	Cregs = new rkregister;
	Cbits = new bitstream;
	Cfrm  = new framemanager;

	memset(&mp4Header,0,3*sizeof(MPEG4_HEADER));

    memset(&vpu_bits_mem,0,sizeof(VPUMemLinear_t));
	memset(&vpu_q_mem,0,sizeof(VPUMemLinear_t));
	memset(&vpu_mv_mem,0,sizeof(VPUMemLinear_t));
	mp4Header[0].flipflop_rounding = 1;
	mp4Header[1].flipflop_rounding = 1;
	mp4Header[2].flipflop_rounding = 1;

	mp4Header[0].ver_id	= 1;
	mp4Header[1].ver_id	= 1;
	mp4Header[2].ver_id	= 1;

	mp4Header[0].coding_type = MPEG4_INVALID_VOP;
	mp4Header[1].coding_type = MPEG4_INVALID_VOP;
	mp4Header[2].coding_type = MPEG4_INVALID_VOP;

	mp4Hdr_Ref0 = &mp4Header[0];
	mp4Hdr_Ref1 = &mp4Header[1];
	mp4Hdr_Cur	= &mp4Header[2];

	mp4Hdr_Cur->fcode_forward = 1;

	isMpeg4 = 0;
	ImgWidth = ImgHeight = 0;
	last_timestamp = time_inc = 0;

    mFlushDpbAtEos =0;
	frameX = 0;

	cusomVersion = 0;
	mFlags =0;
    qTable_VirAddr = NULL;
	vpumem_ctx = NULL;
}

Mpeg4_Decoder::~Mpeg4_Decoder()
{
    reset_keyframestate();
	delete Cregs;
	delete Cbits;
	delete Cfrm;
}

/***************************************************************************************************
	Func:
		getVideoFrame
	Description:
		get a video frame from bitstream buffer
	Author:
		Jian Huan
	Date:
		2010-12-2 17:04:56
 **************************************************************************************************/
RK_S32 Mpeg4_Decoder::getVideoFrame(RK_U8 *bitbuf_ptr, RK_U32 chunksize, VPU_POSTPROCESSING *pp)
{
	RK_S32 status;
    VPUMemLinear_t *p;

    if(!qTable_VirAddr){
        int err = VPUMallocLinear(&vpu_q_mem, 64*2*sizeof(RK_U8));

    	if(err)
    	{
    		return MPEG4_INIT_ERR;
    	}
    	qTable_VirAddr = (RK_U8 *)vpu_q_mem.vir_addr;
    }
    if (chunksize > vpu_bits_mem.size)
    {
        if(vpu_bits_mem.phy_addr){
        	VPUFreeLinear(&vpu_bits_mem);
        }
        int err = VPUMallocLinear(&vpu_bits_mem, chunksize);

        if (err)
        {
            VPU_TRACE("phy address is null in <getVideoFrame>\n");
            return MPEG4_DEC_ERR;
        }
    }

    memcpy(vpu_bits_mem.vir_addr, bitbuf_ptr, chunksize);
    if(VPUMemClean(&vpu_bits_mem))
    {
        ALOGE("mpeg4 VPUMemClean error");
        return MPEG4_DEC_ERR;
    }
    for(int i =0 ; i < 8; i++)
    {
        VPU_TRACE("0x%08x\n", vpu_bits_mem.vir_addr[i]);
    }
    p = &vpu_bits_mem;

	Cbits->init((RK_U8 *)p->vir_addr, p->phy_addr, chunksize);
	status = Cbits->getslice();

	return status;
}


RK_S32  Mpeg4_Decoder::mpeg4_get_stride(MPEG4_HEADER *mp4Hdr)
{
	if ((ImgWidth == 0) || (ImgHeight == 0))
	{
		#ifdef TRACE_ENABLE
		fprintf(stderr, "ImgWidth or ImgHeight is not valid!\n");
		#endif
		return MPEG4_DEC_ERR;
	}

	mp4Hdr->image_width		= ImgWidth;
	mp4Hdr->image_height	= ImgHeight;

	mp4Hdr->mb_width  = (mp4Hdr->image_width + 15) >> 4;
	mp4Hdr->mb_height = (mp4Hdr->image_height + 15)>> 4;

	mp4Hdr->width_offset = mp4Hdr->image_width & 0xF;
	mp4Hdr->height_offset= mp4Hdr->image_height& 0xF;

	return MPEG4_DEC_OK;
}

int Mpeg4_Decoder::log2bin(RK_U32 value)
{
    int n = 0;

    while (value)
    {
        value >>= 1;
        n++;
    }

    return n;
}

void Mpeg4_Decoder::bits_parse_matrix(RK_U8 * matrix)
{
    int i = 0;
    int last, value = 0;

	do
    {
        last = value;
        value = Cbits->getbits(8);
        matrix[i++] = value;
    }
    while (value != 0 && i < 64);

    if (value != 0)
		return;

    i--;

    while (i < 64)
    {
        matrix[i++ ] = last;
    }
}

void Mpeg4_Decoder::set_intra_matrix(RK_U8 * mpeg_quant_matrices, RK_U8 * matrix)
{
    int i;

    RK_U8 *intra_matrix = mpeg_quant_matrices + 0 * 64;

    for (i = 0; i < 64; i++)
    {
        intra_matrix[i] = (!i) ? (RK_U8)8 : (RK_U8)matrix[i];
    }
}

void Mpeg4_Decoder::set_inter_matrix(RK_U8 * mpeg_quant_matrices, RK_U8 * matrix)
{
    RK_S32  i;
    RK_U8   *inter_matrix = mpeg_quant_matrices + 1 * 64;

     for (i = 0; i < 64; i++)
    {
        inter_matrix[i] = (RK_U8) (matrix[i]);
    }
}

/***************************************************************************************************
    Func:
        decode012
    Author:
        Jianhuan
    Date:
        2010-11-22 15:30:28
***************************************************************************************************/
RK_S32 Mpeg4_Decoder::decode012()
{
    int n;
    n = Cbits->getbits(1);

    if (n == 0)
        return 0;
    else
        return Cbits->getbits(1) + 1;
}

void Mpeg4_Decoder::mpeg4_read_vol_complexity_estimation_header(MPEG4_HEADER *mp4Hdr)
{
    ESTIMATION * e = &estimation;

    e->method = Cbits->getbits( 2); /* estimation_method */

    if (e->method == 0 || e->method == 1)
    {
        if (!Cbits->getbits(1))  /* shape_complexity_estimation_disable */
        {
            e->opaque = Cbits->getbits(1);  /* opaque */
            e->transparent = Cbits->getbits(1);  /* transparent */
            e->intra_cae = Cbits->getbits(1);  /* intra_cae */
            e->inter_cae = Cbits->getbits(1);  /* inter_cae */
            e->no_update = Cbits->getbits(1);  /* no_update */
            e->upsampling = Cbits->getbits(1);  /* upsampling */
        }

        if (!Cbits->getbits(1)) /* texture_complexity_estimation_set_1_disable */
        {
            e->intra_blocks = Cbits->getbits(1);  /* intra_blocks */
            e->inter_blocks = Cbits->getbits(1);  /* inter_blocks */
            e->inter4v_blocks = Cbits->getbits(1);  /* inter4v_blocks */
            e->not_coded_blocks = Cbits->getbits(1);  /* not_coded_blocks */
        }
    }

    READ_MARKER();

    if (!Cbits->getbits(1))  /* texture_complexity_estimation_set_2_disable */
    {
        e->dct_coefs = Cbits->getbits(1);  /* dct_coefs */
        e->dct_lines = Cbits->getbits(1);  /* dct_lines */
        e->vlc_symbols = Cbits->getbits(1);  /* vlc_symbols */
        e->vlc_bits = Cbits->getbits(1);  /* vlc_bits */
    }

    if (!Cbits->getbits(1))  /* motion_compensation_complexity_disable */
    {
        e->apm = Cbits->getbits(1);  /* apm */
        e->npm = Cbits->getbits(1);  /* npm */
        e->interpolate_mc_q = Cbits->getbits(1);  /* interpolate_mc_q */
        e->forw_back_mc_q = Cbits->getbits(1);  /* forw_back_mc_q */
        e->halfpel2 = Cbits->getbits(1);  /* halfpel2 */
        e->halfpel4 = Cbits->getbits(1);  /* halfpel4 */
    }

    READ_MARKER();

    if (e->method == 1)
    {
        if (!Cbits->getbits(1)) /* versirk_complexity_estimation_disable */
        {
            e->sadct = Cbits->getbits(1);  /* sadct */
            e->quarterpel = Cbits->getbits(1);  /* quarterpel */
        }
    }
}


/* vop estimation header */

void Mpeg4_Decoder::mpeg4_read_vop_complexity_estimation_header(MPEG4_HEADER *mp4Hdr, int coding_type)
{
    ESTIMATION * e = &estimation;

    if (e->method == 0 || e->method == 1)
    {
        if (coding_type == MPEG4_I_VOP)
        {
            if (e->opaque)  Cbits->skipbits( 8); /* dcecs_opaque */

            if (e->transparent) Cbits->skipbits( 8); /* */

            if (e->intra_cae) Cbits->skipbits( 8); /* */

            if (e->inter_cae) Cbits->skipbits( 8); /* */

            if (e->no_update) Cbits->skipbits( 8); /* */

            if (e->upsampling) Cbits->skipbits( 8); /* */

            if (e->intra_blocks) Cbits->skipbits( 8); /* */

            if (e->not_coded_blocks) Cbits->skipbits( 8); /* */

            if (e->dct_coefs) Cbits->skipbits( 8); /* */

            if (e->dct_lines) Cbits->skipbits( 8); /* */

            if (e->vlc_symbols) Cbits->skipbits( 8); /* */

            if (e->vlc_bits) Cbits->skipbits( 8); /* */

            if (e->sadct)  Cbits->skipbits( 8); /* */
        }

        if (coding_type == MPEG4_P_VOP)
        {
            if (e->opaque) Cbits->skipbits( 8);  /* */

            if (e->transparent) Cbits->skipbits( 8); /* */

            if (e->intra_cae) Cbits->skipbits( 8); /* */

            if (e->inter_cae) Cbits->skipbits( 8); /* */

            if (e->no_update) Cbits->skipbits( 8); /* */

            if (e->upsampling) Cbits->skipbits( 8); /* */

            if (e->intra_blocks) Cbits->skipbits( 8); /* */

            if (e->not_coded_blocks) Cbits->skipbits( 8); /* */

            if (e->dct_coefs) Cbits->skipbits( 8); /* */

            if (e->dct_lines) Cbits->skipbits( 8); /* */

            if (e->vlc_symbols) Cbits->skipbits( 8); /* */

            if (e->vlc_bits) Cbits->skipbits( 8); /* */

            if (e->inter_blocks) Cbits->skipbits( 8); /* */

            if (e->inter4v_blocks) Cbits->skipbits( 8); /* */

            if (e->apm)   Cbits->skipbits( 8); /* */

            if (e->npm)   Cbits->skipbits( 8); /* */

            if (e->forw_back_mc_q) Cbits->skipbits( 8); /* */

            if (e->halfpel2) Cbits->skipbits( 8); /* */

            if (e->halfpel4) Cbits->skipbits( 8); /* */

            if (e->sadct)  Cbits->skipbits( 8); /* */

            if (e->quarterpel) Cbits->skipbits( 8); /* */
        }

        if (coding_type == MPEG4_B_VOP)
        {
            if (e->opaque)  Cbits->skipbits( 8); /* */

            if (e->transparent) Cbits->skipbits( 8); /* */

            if (e->intra_cae) Cbits->skipbits( 8); /* */

            if (e->inter_cae) Cbits->skipbits( 8); /* */

            if (e->no_update) Cbits->skipbits( 8); /* */

            if (e->upsampling) Cbits->skipbits( 8); /* */

            if (e->intra_blocks) Cbits->skipbits( 8); /* */

            if (e->not_coded_blocks) Cbits->skipbits( 8); /* */

            if (e->dct_coefs) Cbits->skipbits( 8); /* */

            if (e->dct_lines) Cbits->skipbits( 8); /* */

            if (e->vlc_symbols) Cbits->skipbits( 8); /* */

            if (e->vlc_bits) Cbits->skipbits( 8); /* */

            if (e->inter_blocks) Cbits->skipbits( 8); /* */

            if (e->inter4v_blocks) Cbits->skipbits( 8); /* */

            if (e->apm)   Cbits->skipbits( 8); /* */

            if (e->npm)   Cbits->skipbits( 8); /* */

            if (e->forw_back_mc_q) Cbits->skipbits( 8); /* */

            if (e->halfpel2) Cbits->skipbits( 8); /* */

            if (e->halfpel4) Cbits->skipbits( 8); /* */

            if (e->interpolate_mc_q) Cbits->skipbits( 8); /* */

            if (e->sadct)  Cbits->skipbits( 8); /* */

            if (e->quarterpel) Cbits->skipbits( 8); /* */
        }

#ifdef GMC_SUPPORT
        if (coding_type == MPEG4_S_VOP && mp4Hdr->sprite_enable == MPEG4_SPRITE_STATIC)
        {
            if (e->intra_blocks) Cbits->skipbits( 8); /* */

            if (e->not_coded_blocks) Cbits->skipbits( 8); /* */

            if (e->dct_coefs) Cbits->skipbits( 8); /* */

            if (e->dct_lines) Cbits->skipbits( 8); /* */

            if (e->vlc_symbols) Cbits->skipbits( 8); /* */

            if (e->vlc_bits) Cbits->skipbits( 8); /* */

            if (e->inter_blocks) Cbits->skipbits( 8); /* */

            if (e->inter4v_blocks) Cbits->skipbits( 8); /* */

            if (e->apm)   Cbits->skipbits( 8); /* */

            if (e->npm)   Cbits->skipbits( 8); /* */

            if (e->forw_back_mc_q) Cbits->skipbits( 8); /* */

            if (e->halfpel2) Cbits->skipbits( 8); /* */

            if (e->halfpel4) Cbits->skipbits( 8); /* */

            if (e->interpolate_mc_q) Cbits->skipbits( 8); /* */
        }
#endif
    }
}

/******************************************************************************************************
	Func:
		mpeg4_parse_header
	Description:
		decoding mpeg4 frame header
	Author:
		Jian Huan
	Date:
		2010-11-19 10:28:42
 ******************************************************************************************************/
RK_S32 Mpeg4_Decoder::mpeg4_parse_header(MPEG4_HEADER *mp4Hdr)
{
    RK_U32  vol_ver_id;
    RK_U32  start_code;
    RK_U32  time_incr = 0;
    RK_S32  time_increment = 0;
    RK_S32  resize = 0;
    RK_S32  scalability = 0;

	VPU_TRACE("---> <mpeg4_parse_header>\n");
	while (Cbits->leftbit() >= 32)
    {
        Cbits->bytealign();
        start_code = Cbits->showbits(32);
		VPU_TRACE("start_code = 0x%08x\n", start_code);

        if (start_code == MPEG4_VISOBJSEQ_START_CODE)
        {

			int profile;

            Cbits->skipbits(32); 													/* visual_object_sequence_start_code */
            profile = Cbits->getbits(8); 											/* profile_and_level_indication */

            VPU_TRACE("<mpeg4_parse_header> profile = 0x%08x\n", profile);

		}
        else if (start_code == MPEG4_VISOBJSEQ_STOP_CODE)
        {

			Cbits->skipbits(32); 													/* visual_object_sequence_stop_code */

		}
        else if (start_code == MPEG4_VISOBJ_START_CODE)
        {

            VPU_TRACE("<mpeg4_parse_header> MPEG4_VISOBJ_START_CODE\n");

            Cbits->skipbits(32); 													/* visual_object_start_code */

            if (Cbits->getbits(1)) 													/* is_visual_object_identified */
            {
                mp4Hdr->ver_id = Cbits->getbits(4); 									/* visual_object_ver_id */
                Cbits->skipbits( 3); 												/* visual_object_priority */
            }
            else
            {
                mp4Hdr->ver_id = 1;
            }

            if (Cbits->showbits(4) != MPEG4_VISOBJ_TYPE_VIDEO) 						/* visual_object_type */
            {
            	VPU_TRACE("<--- <mpeg4_parse_header>\n");
                return MPEG4_INVALID_VOP;
            }

            Cbits->skipbits( 4);

            /* video_signal_type */

            if (Cbits->getbits(1)) 													/* video_signal_type */
            {
                Cbits->skipbits( 3); 												/* video_format */
                Cbits->skipbits( 1); 												/* video_range */

                if (Cbits->getbits(1)) 												/* color_description */
                {
                    Cbits->skipbits( 8); 											/* color_primaries */
                    Cbits->skipbits( 8); 											/* transfer_characteristics */
                    Cbits->skipbits( 8); 											/* matrix_coefficients */
                }
            }

		}
        else if ((start_code & ~MPEG4_VIDOBJ_START_CODE_MASK) == MPEG4_VIDOBJ_START_CODE)
        {
            VPU_TRACE("<mpeg4_parse_header> MPEG4_VIDOBJ_START_CODE\n");
            Cbits->skipbits( 32); 													/* video_object_start_code */

		}
        else if ((start_code & ~MPEG4_VIDOBJLAY_START_CODE_MASK) == MPEG4_VIDOBJLAY_START_CODE)
        {
            int aspect_ratio;

            VPU_TRACE("<mpeg4_parse_header> MPEG4_VIDOBJLAY_START_CODE\n");

            Cbits->skipbits( 32); 													/* video_object_layer_start_code */
            Cbits->skipbits( 1); 													/* random_accessible_vol */

            Cbits->skipbits( 8);   													/* video_object_type_indication */

            if (Cbits->getbits(1)) 													/* is_object_layer_identifier */
            {
                vol_ver_id = Cbits->getbits( 4); 									/* video_object_layer_verid */
                Cbits->skipbits( 3); 												/* video_object_layer_priority */
            }
            else
            {
                vol_ver_id = mp4Hdr->ver_id;
            }

            aspect_ratio = Cbits->getbits( 4);

            if (aspect_ratio == MPEG4_VIDOBJLAY_AR_EXTPAR) 						/* aspect_ratio_info */
            {
                int par_width, par_height;

                par_width = Cbits->getbits( 8); 									/* par_width */
                par_height = Cbits->getbits( 8); 									/* par_height */
            }

            if (Cbits->getbits(1)) 													/* vol_control_parameters */
            {
                Cbits->skipbits( 2); 												/* chroma_format */
                mp4Hdr->low_delay = Cbits->getbits(1); 								/* low_delay flage (1 means no B_VOP) */

                if (Cbits->getbits(1)) 												/* vbv_parameters */
                {
                    unsigned int bitrate;
                    unsigned int buffer_size;
                    unsigned int occupancy;


                    bitrate = Cbits->getbits( 15) << 15; 							/* first_half_bit_rate */
                    READ_MARKER();
                    bitrate |= Cbits->getbits( 15); 									/* latter_half_bit_rate */
                    READ_MARKER();

                    buffer_size = Cbits->getbits( 15) << 3; 						/* first_half_vbv_buffer_size */
                    READ_MARKER();
                    buffer_size |= Cbits->getbits( 3);  							/* latter_half_vbv_buffer_size */

                    occupancy = Cbits->getbits( 11) << 15; 							/* first_half_vbv_occupancy */
                    READ_MARKER();
                    occupancy |= Cbits->getbits( 15); 								/* latter_half_vbv_occupancy */
                    READ_MARKER();

                }
            }
            else
            {
                mp4Hdr->low_delay = 0;
            }

            mp4Hdr->shape = Cbits->getbits( 2); 										/* video_object_layer_shape */


            if (mp4Hdr->shape != MPEG4_VIDOBJLAY_SHAPE_RECTANGULAR)
            {
            	VPU_TRACE("<--- <mpeg4_parse_header>\n");
                return MPEG4_INVALID_VOP;
            }

            if (mp4Hdr->shape == MPEG4_VIDOBJLAY_SHAPE_GRAYSCALE && vol_ver_id != 1)
            {
                Cbits->skipbits(4); 													/* video_object_layer_shape_extension */
            }

            READ_MARKER();

            /********************** for decode B-frame time ***********************/
            mp4Hdr->time_inc_resolution = Cbits->getbits( 16); 						/* vop_time_increment_resolution */

            if (mp4Hdr->time_inc_resolution > 0)
            {
                mp4Hdr->time_inc_bits = MAX(log2bin(mp4Hdr->time_inc_resolution - 1), 1);
            }
            else
            {
                /* for "old" compatibility, set time_inc_bits = 1 */
                mp4Hdr->time_inc_bits = 1;
            }

            READ_MARKER();

            if (Cbits->getbits(1)) /* fixed_vop_rate */
            {
                Cbits->skipbits( mp4Hdr->time_inc_bits); /* fixed_vop_time_increment */
            }

            if (mp4Hdr->shape != MPEG4_VIDOBJLAY_SHAPE_BINARY_ONLY)
            {

                if (mp4Hdr->shape == MPEG4_VIDOBJLAY_SHAPE_RECTANGULAR)
                {
                    RK_U32 width, height;

                    READ_MARKER();
                    width  = Cbits->getbits( 13); /* video_object_layer_width */
                    READ_MARKER();
                    height = Cbits->getbits( 13); /* video_object_layer_height */
                    READ_MARKER();

                    VPU_TRACE("<mpeg4_parse_header> width = 0x%08x\n, height = 0x%08x\n", width, height);

                    if (mp4Hdr->width != width || mp4Hdr->height != height)
                    {
                        mp4Hdr->width  = width;
                        mp4Hdr->height = height;

                        resize = 1;

						mp4Hdr->mb_width  = (mp4Hdr->width  + 15) >> 4;
    					mp4Hdr->mb_height = (mp4Hdr->height + 15) >> 4;
						mp4Hdr->totalMbInVop = mp4Hdr->mb_width * mp4Hdr->mb_height;

					    /*Image width & height*/
					    mp4Hdr->image_width  = 16 * mp4Hdr->mb_width;
					    mp4Hdr->image_height = 16 * mp4Hdr->mb_height;

						mp4Hdr->width_offset = mp4Hdr->width & 0xF;
						mp4Hdr->height_offset= mp4Hdr->height& 0xF;
                    }
                }

                mp4Hdr->interlacing = Cbits->getbits(1);
                mp4Hdr->sprite_enable = Cbits->getbits( (vol_ver_id == 1 ? 1 : 2)); 		/* sprite_enable */

#ifdef GMC_SUPPORT
				;
#else
             	if(mp4Hdr->sprite_enable != MPEG4_SPRITE_NONE)
             	{
                    ALOGE("GMC_NO_SUPPORT \n");
             		return MPEG4_FORMAT_ERR;
             	}
#endif
#ifdef GMC_SUPPORT
                if (mp4Hdr->sprite_enable == MPEG4_SPRITE_STATIC || mp4Hdr->sprite_enable == MPEG4_SPRITE_GMC)
                {
                    int low_latency_sprite_enable;
                    int sprite_brightness_change;

                    if (mp4Hdr->sprite_enable != MPEG4_SPRITE_GMC)
                    {
                        int sprite_width;
                        int sprite_height;
                        int sprite_left_coord;
                        int sprite_top_coord;
                        sprite_width = Cbits->getbits( 13);  								/* sprite_width */
                        READ_MARKER();
                        sprite_height = Cbits->getbits( 13); 								/* sprite_height */
                        READ_MARKER();
                        sprite_left_coord = Cbits->getbits( 13); 							/* sprite_left_coordinate */
                        READ_MARKER();
                        sprite_top_coord = Cbits->getbits( 13); 							/* sprite_top_coordinate */
                        READ_MARKER();
                    }

                    mp4Hdr->sprite_warping_points = Cbits->getbits( 6);  					/* no_of_sprite_warping_points */

                    mp4Hdr->sprite_warping_accuracy = Cbits->getbits( 2);  					/* sprite_warping_accuracy */
                    sprite_brightness_change = Cbits->getbits( 1);  						/* brightness_change */

                    if (mp4Hdr->sprite_enable != MPEG4_SPRITE_GMC)
                    {
                        low_latency_sprite_enable = Cbits->getbits( 1);  					/* low_latency_sprite_enable */
                    }
                }
#endif
                if (vol_ver_id != 1 &&
                        mp4Hdr->shape != MPEG4_VIDOBJLAY_SHAPE_RECTANGULAR)
                {
                    Cbits->skipbits( 1); 													/* sadct_disable */
                }

                if (Cbits->getbits(1)) /* not_8_bit */
                {
                    mp4Hdr->quant_bits = Cbits->getbits( 4); 								/* quant_precision */
                    Cbits->skipbits( 4); 													/* bits_per_pixel */
                }
                else
                {
                    mp4Hdr->quant_bits = 5;
                }

                if (mp4Hdr->shape == MPEG4_VIDOBJLAY_SHAPE_GRAYSCALE)
                {
                    Cbits->skipbits( 1); 													/* no_gray_quant_update */
                    Cbits->skipbits( 1); 													/* composition_method */
                    Cbits->skipbits( 1); 													/* linear_composition */
                }

                mp4Hdr->quant_type = Cbits->getbits(1); 										/* quant_type */

                VPU_TRACE("<mpeg4_parse_header> quant_type = 0x%08x\n",mp4Hdr->quant_type);

                if (mp4Hdr->quant_type)
                {
					mp4Hdr->mpeg_quant_matrices = (RK_U8 *)qTable_VirAddr;

                    if (Cbits->getbits(1)) 													/* load_intra_quant_mat */
                    {
                        RK_U8 matrix[64];

                        bits_parse_matrix(matrix);
                        set_intra_matrix(mp4Hdr->mpeg_quant_matrices, matrix);
                    }
                    else
                        set_intra_matrix(mp4Hdr->mpeg_quant_matrices, default_intra_matrix);

                    if (Cbits->getbits(1)) /* load_inter_quant_mat */
                    {
                        RK_U8 matrix[64];


                        bits_parse_matrix(matrix);
                        set_inter_matrix(mp4Hdr->mpeg_quant_matrices, matrix);
                    }
                    else
                        set_inter_matrix(mp4Hdr->mpeg_quant_matrices, default_inter_matrix);

                    if (mp4Hdr->shape == MPEG4_VIDOBJLAY_SHAPE_GRAYSCALE)
                    {
                    	ALOGE("<--- <SHAPE_GRAYSCALE mpeg4_parse_header>\n");
                        return MPEG4_INVALID_VOP;
                    }

                }

                if (vol_ver_id != 1)
                {
                    mp4Hdr->quarterpel = Cbits->getbits(1); /* quarter_sample */
                }
                else
                    mp4Hdr->quarterpel = 0;
#ifndef QPEL_ENABLE
                //if(mp4Hdr->quarterpel)
                    //return MPEG4_FORMAT_ERR;
#endif
                mp4Hdr->complexity_estimation_disable = Cbits->getbits(1); /* complexity estimation disable */

                if (!mp4Hdr->complexity_estimation_disable)
                {
                    VPU_TRACE("<mpeg4_parse_header> mpeg4_read_vol_complexity_estimation_header\n");
                    mpeg4_read_vol_complexity_estimation_header(mp4Hdr);
                }

                mp4Hdr->resync_marker_disable = Cbits->getbits( 1); /* resync_marker_disable */

                if (Cbits->getbits(1)) /* data_partitioned */
                {
                    Cbits->skipbits( 1); /* reversible_vlc */
                }

                if (vol_ver_id != 1)
                {
                    mp4Hdr->newpred_enable = Cbits->getbits(1);

                    if (mp4Hdr->newpred_enable) /* newpred_enable */
                    {
                        Cbits->skipbits( 2); /* requested_upstream_message_type */
                        Cbits->skipbits( 1); /* newpred_segment_type */
                    }

                    mp4Hdr->reduced_resolution_enable = Cbits->getbits(1); /* reduced_resolution_vop_enable */

                }
                else
                {
                    mp4Hdr->newpred_enable = 0;
                    mp4Hdr->reduced_resolution_enable = 0;
                }

                scalability = Cbits->getbits(1); /* scalability */

                if (scalability)
                {
                    Cbits->skipbits( 1); /* hierarchy_type */
                    Cbits->skipbits( 4); /* ref_layer_id */
                    Cbits->skipbits( 1); /* ref_layer_sampling_direc */
                    Cbits->skipbits( 5); /* hor_sampling_factor_n */
                    Cbits->skipbits( 5); /* hor_sampling_factor_m */
                    Cbits->skipbits( 5); /* vert_sampling_factor_n */
                    Cbits->skipbits( 5); /* vert_sampling_factor_m */
                    Cbits->skipbits( 1); /* enhancement_type */

                    if (mp4Hdr->shape == MPEG4_VIDOBJLAY_SHAPE_BINARY /* && hierarchy_type==0 */)
                    {
                        Cbits->skipbits( 1 + 1 + 1 + 5 + 5 + 5 + 5); /* use_ref_shape and so on*/
                    }

					ALOGE("<--- <scalability mpeg4_parse_header>\n");
                    return MPEG4_INVALID_VOP;
                }
            }
            else    /* mp4Hdr->shape == BINARY_ONLY */
            {
            	VPU_TRACE("<--- <mpeg4_parse_header>\n");
                return MPEG4_INVALID_VOP;

            }

            //return (resize ? -3 : -2 ); /* VOL */
            if (resize)
            {
            	VPU_TRACE("<--- <mpeg4_parse_header>\n");
                return MPEG4_MEM_INIT;
            }

        }
        else if (start_code == MPEG4_GRPOFVOP_START_CODE)
        {

            VPU_TRACE("<mpeg4_parse_header> MPEG4_GRPOFVOP_START_CODE\n");

            Cbits->skipbits(32);
            {
                int hours, minutes, seconds;

                hours = Cbits->getbits(5);
                minutes = Cbits->getbits(6);
                READ_MARKER();
                seconds = Cbits->getbits(6);

            }

            Cbits->skipbits( 1); /* closed_gov */
            Cbits->skipbits( 1); /* broken_link */

        }
        else if (start_code == MPEG4_VOP_START_CODE)
        {
            VPU_TRACE("<mpeg4_parse_header> MPEG4_VOP_START_CODE\n");

            Cbits->skipbits( 32); /* vop_start_code */

            mp4Hdr->coding_type = Cbits->getbits(2); /* vop_coding_type */

            /*********************** for decode B-frame time ***********************/

            while (Cbits->getbits(1) != 0) /* time_base */
                time_incr++;

            READ_MARKER();

            if (mp4Hdr->time_inc_bits)
            {
                time_increment = (Cbits->getbits( mp4Hdr->time_inc_bits)); /* vop_time_increment */
            }

            if (mp4Hdr->coding_type != MPEG4_B_VOP)
            {
                mp4Hdr->last_time_base = mp4Hdr->time_base;
                mp4Hdr->time_base += time_incr;
                mp4Hdr->time = mp4Hdr->time_base * mp4Hdr->time_inc_resolution + time_increment;
                mp4Hdr->time_pp = (RK_S32)(mp4Hdr->time - mp4Hdr->last_non_b_time);
                mp4Hdr->last_non_b_time = mp4Hdr->time;
            }
            else
            {
                mp4Hdr->time = (mp4Hdr->last_time_base + time_incr) * mp4Hdr->time_inc_resolution + time_increment;
                mp4Hdr->time_bp = mp4Hdr->time_pp - (RK_S32)(mp4Hdr->last_non_b_time - mp4Hdr->time);
            }

            READ_MARKER();

            if (!Cbits->getbits(1)) /* vop_coded */
            {
            	mp4Hdr->coding_type = MPEG4_N_VOP;
				VPU_TRACE("<--- <mpeg4_parse_header>\n");
                return MPEG4_N_VOP;
            }

            if (mp4Hdr->newpred_enable)
            {
                int vop_id;
                int vop_id_for_prediction;

                vop_id = Cbits->getbits( MIN(mp4Hdr->time_inc_bits + 3, 15));

                if (Cbits->getbits(1)) /* vop_id_for_prediction_indication */
                {
                    vop_id_for_prediction = Cbits->getbits( MIN(mp4Hdr->time_inc_bits + 3, 15));
                }

                READ_MARKER();
            }

            /* fix a little bug by MinChen <chenm002@163.com> */
            if ((mp4Hdr->shape != MPEG4_VIDOBJLAY_SHAPE_BINARY_ONLY) &&
                    ( (mp4Hdr->coding_type == MPEG4_P_VOP) || (mp4Hdr->coding_type == MPEG4_S_VOP && mp4Hdr->sprite_enable == MPEG4_SPRITE_GMC) ) )
            {
                mp4Hdr->rounding = Cbits->getbits(1); /* rounding_type */
            }
            if (mp4Hdr->shape != MPEG4_VIDOBJLAY_SHAPE_BINARY_ONLY)
            {

                if (!mp4Hdr->complexity_estimation_disable)
                {
                    mpeg4_read_vop_complexity_estimation_header(mp4Hdr, mp4Hdr->coding_type);
                }

                /* intra_dc_vlc_threshold */
                mp4Hdr->intra_dc_threshold = (RK_S32)intra_dc_threshold_table[Cbits->getbits(3)];

                mp4Hdr->top_field_first = 0;

                mp4Hdr->alternate_vertical_scan = 0;

				if (mp4Hdr->interlacing) {
					mp4Hdr->top_field_first = Cbits->getbits(1);
					mp4Hdr->alternate_vertical_scan = Cbits->getbits(1);
				}
            }

            if ((mp4Hdr->sprite_enable == MPEG4_SPRITE_STATIC || mp4Hdr->sprite_enable == MPEG4_SPRITE_GMC) && mp4Hdr->coding_type == MPEG4_S_VOP)
            {
#ifdef GMC_SUPPORT
                int i;

                for (i = 0 ; i < mp4Hdr->sprite_warping_points; i++)
                {
                    int length;
                    int x = 0, y = 0;

                    /* sprite code borowed from ffmpeg; thx Michael Niedermayer <michaelni@gmx.at> */
                    length = bits_parse_spritetrajectory();

                    if (length)
                    {
                        x = Cbits->getbits( length);

                        if ((x >> (length - 1)) == 0) /* if MSB not set it is negative*/
                            x = - (x ^ ((1 << length) - 1));
                    }

                    READ_MARKER();

                    length = bits_parse_spritetrajectory();

                    if (length)
                    {
                        y = Cbits->getbits( length);

                        if ((y >> (length - 1)) == 0) /* if MSB not set it is negative*/
                            y = - (y ^ ((1 << length) - 1));
                    }

                    READ_MARKER();

                    gmc_warp->duv[i].x = x;
                    gmc_warp->duv[i].y = y;

                }
#else
				VPU_TRACE("<--- <mpeg4_parse_header>\n");
				return MPEG4_INVALID_VOP;
#endif
            }

            if ((mp4Hdr->quant = Cbits->getbits(mp4Hdr->quant_bits)) < 1) /* vop_quant */
                mp4Hdr->quant = 1;


            if (mp4Hdr->coding_type != MPEG4_I_VOP)
            {
                mp4Hdr->fcode_forward = Cbits->getbits(3); /* fcode_forward */
            }

            if (mp4Hdr->coding_type == MPEG4_B_VOP)
            {
                mp4Hdr->fcode_backward = Cbits->getbits(3); /* fcode_backward */
            }

            if (!scalability)
            {
                if ((mp4Hdr->shape != MPEG4_VIDOBJLAY_SHAPE_RECTANGULAR) &&
                        (mp4Hdr->coding_type != MPEG4_I_VOP))
                {
                    Cbits->skipbits( 1); /* vop_shape_coding_type */
                }
            }
			VPU_TRACE("<--- <mpeg4_parse_header>\n");

            return mp4Hdr->coding_type;

        }
        else if (start_code == MPEG4_USERDATA_START_CODE)
        {
            char tmp[256];
            int i, j = 0; //version, build;
            char packed = 0;

            Cbits->skipbits( 32); /* user_data_start_code */

            memset(tmp, 0, 256);
            tmp[0] = (char)(Cbits->showbits(8));

            for (i = 1; i < 256; i++)
            {
                tmp[i] = (char)(Cbits->showbits(16) & 0xFF);

                if (tmp[i] == 0)
                    break;

                Cbits->skipbits( 8);
            }

            if (!mp4Hdr->packed_mode)
            {
                if ((tmp[0] == 'D') && (tmp[1] == 'i') && (tmp[2] == 'v') && (tmp[3] == 'X'))
                {
                    j = 4;

					if (tmp[j] <= '4')
					{
						cusomVersion = 4;
					}
					else
					{
						cusomVersion = 5;
					}

                    while ((tmp[j] >= '0') && (tmp[j] <= '9'))
                    {
                        j++;
                    }

                    if (tmp[j] == 'b')
                    {
                        j++;

                        while ((tmp[j] >= '0') && (tmp[j] <= '9'))
                        {
                            j++;
                        }

                        packed = tmp[j];
                    }
                    else if ((tmp[j] == 'B') && (tmp[j+1] == 'u') && (tmp[j+2] == 'i') && (tmp[j+3] == 'l') && (tmp[j+4] == 'd'))
                    {
                        j += 5;

                        while ((tmp[j] >= '0') && (tmp[j] <= '9'))
                        {
                            j++;
                        }

                        packed = tmp[j];
                    }
                    else
                    {
                        ;
                    }

                    if (packed == 'p')
                        mp4Hdr->packed_mode = 1;
                    else
                        mp4Hdr->packed_mode = 0;
                }
                else
                {
                    mp4Hdr->packed_mode = 0;
                }
            }

        }
        else     /* start_code == ? */
        {
            Cbits->skipbits(8);
        }
    }

	VPU_T("No any valid header found in <mpeg4_parse_header>\n");
    return MPEG4_INVALID_VOP;     /* ignore it */
}


/******************************************************************************************************
	Func:
		mpeg4_memory_init
	Description:
		init memory in each frame decoding
	Author:
		Jian Huan
	Date:
		2010-11-19 10:31:15
 ******************************************************************************************************/
RK_S32 Mpeg4_Decoder::mpeg4_memory_init(MPEG4_HEADER *mp4Hdr)
{
	VPU_TRACE("---> <mpeg4_memory_init>\n");

    if (vpu_mv_mem.phy_addr) {
        VPUFreeLinear(&vpu_mv_mem);
        vpu_mv_mem.phy_addr =0;
    }
	int err = VPUMallocLinear(&vpu_mv_mem, mp4Hdr->totalMbInVop*4*sizeof(RK_U32));

	if (err)
	{
		return MPEG4_MEM_ERR;
	}

	directMV_Addr = vpu_mv_mem.phy_addr;

	VPU_TRACE("directMV_Addr = 0x%08X\n", directMV_Addr);

	VPU_TRACE("<--- <mpeg4_memory_init>\n");

	return MPEG4_MEM_OK;
}

/******************************************************************************************************
	Func:
		mpeg4_frame_init
	Description:
		init frame in each frame decoding
	Author:
		Jian Huan
	Date:
		2010-11-19 10:31:15
 ******************************************************************************************************/
RK_S32 Mpeg4_Decoder::mpeg4_frame_init(MPEG4_HEADER *mp4Hdr)
{
	int ret;

	VPU_TRACE("---> <mpeg4_frame_init>\n");

	ret = Cfrm->init(8);

	if (ret != VPU_OK)
	{
		Cfrm->deinit();
		VPU_TRACE("frame management init error!\n");

		return MPEG4_MEM_ERR;
	}

	VPU_TRACE("<--- <mpeg4_frame_init>\n");

	return MPEG4_MEM_OK;
}

/******************************************************************************************************
	Func:
		mpeg4_frame_manage
	Description:
		manage frame in each frame decoding
	Author:
		Jian Huan
	Date:
		2010-11-19 10:30:49
 ******************************************************************************************************/
RK_S32 Mpeg4_Decoder::mpeg4_frame_manage(MPEG4_HEADER *mp4Hdr)
{
	RK_U32 		frameSize;
	VPU_FRAME 	*frame;

	VPU_TRACE("---><mpeg4_frame_manage>\n");

	frameSize = mp4Hdr->image_height * mp4Hdr->image_width * 3 / 2;
	frame = Cfrm->get_frame(frameSize,vpumem_ctx);
	if (frame == NULL)
	{
        ALOGE("get NULL frame in <mpeg4_frame_manage>\n");
		return VPU_ERR;
	}

    mp4Hdr_Cur->frameNumber++;
	mp4Hdr->frame_space = frame;

	mp4Hdr->frame_space->DisplayHeight	= mp4Hdr->height;
	mp4Hdr->frame_space->DisplayWidth	= mp4Hdr->width;
	mp4Hdr->frame_space->FrameHeight	= mp4Hdr->image_height;
	mp4Hdr->frame_space->FrameWidth		= mp4Hdr->image_width;

	mp4Hdr->frame_space->FrameBusAddr[0] = mp4Hdr->frame_space->vpumem.phy_addr;
	mp4Hdr->frame_space->FrameBusAddr[1] = mp4Hdr->frame_space->vpumem.phy_addr + mp4Hdr->image_height * mp4Hdr->image_width;

	VPU_TRACE("frame Y address(physical): 0x%08x, UV address(physical): 0x%08x\n", mp4Hdr->frame_space->FrameBusAddr[0], mp4Hdr->frame_space->FrameBusAddr[1]);

	VPU_TRACE("<---<mpeg4_frame_manage>\n");

	return VPU_OK;
}
/***************************************************************************************************
	Func:
		mpeg4_decode_init
	Description:
		Mpeg4 decoder init.
	Author:
		Jian Huan
	Date:
		2010-12-3 10:54:15
 **************************************************************************************************/
RK_S32 Mpeg4_Decoder::mpeg4_decode_init(VPU_GENERIC *vpug)
{
	if (vpug->CodecType == VPU_CODEC_DEC_MPEG4)
	{
		isMpeg4 = 1;
	}
	else
	{
		#ifdef TRACE_ENABLE
		fprintf(stderr, "Codec Type is not valid!\n");
		#endif
		return MPEG4_INIT_ERR;
	}
	ImgWidth = vpug->ImgWidth;
	ImgHeight= vpug->ImgHeight;

	return MPEG4_INIT_OK;
}

/***************************************************************************************************
	Func:
		mpeg4_decode_deinit
	Description:
		Mpeg4 decoder deinit.
	Notice:

	Author:
		Jian Huan
	Date:
		2010-12-3 10:54:15
 **************************************************************************************************/
RK_S32 Mpeg4_Decoder::mpeg4_decode_deinit(VPU_GENERIC *vpug)
{
    if(vpu_bits_mem.phy_addr)
    VPUFreeLinear(&vpu_bits_mem);
    if(vpu_q_mem.phy_addr)
	VPUFreeLinear(&vpu_q_mem);
    if(vpu_mv_mem.phy_addr)
	VPUFreeLinear(&vpu_mv_mem);

	return MPEG4_DEINIT_OK;
}
/***************************************************************************************************
	Func:
		mpeg4_decode_frame
	Description:
		total flow of mpeg4 decoding
	Author:
		Jian Huan
	Date:
		2010-11-19 10:29:15
 **************************************************************************************************/
RK_S32 Mpeg4_Decoder::mpeg4_decode_frame()
{
	RK_S32	coding_type, ret;

	VPU_TRACE("--> <mpeg4_decode_frame>\n");

	vopType = MPEG4_INVALID_VOP;

	VPU_TRACE("No. %3d frame\n", frameX++);
mpeg4_header:
	/*@jh: Parse Mpeg4 Header*/
	if (isMpeg4)
	{
		coding_type = mpeg4_parse_header(mp4Hdr_Cur);
	}
	else
	{
		#ifdef TRACE_ENABLE
		fprintf(stderr, "codec type error!\n");
		#endif
		return MPEG4_DEC_ERR;
	}

	VPU_TRACE("coding_type = %d\n", coding_type);

	if (coding_type == MPEG4_INVALID_VOP)
	{
		VPU_T("coding type is not valid!\n");
		return MPEG4_HDR_ERR;
	}
	else if (coding_type == MPEG4_MEM_INIT)
	{
		/*@jh: display_width, display_height如何处理?*/
		mp4Hdr_Cur->display_width = mp4Hdr_Cur->image_width;
		mp4Hdr_Cur->display_height= mp4Hdr_Cur->image_height;

		ret = mpeg4_memory_init(mp4Hdr_Cur);

		ret = mpeg4_frame_init(mp4Hdr_Cur);

		goto mpeg4_header;
	}
    else if(coding_type == MPEG4_FORMAT_ERR)
    {
		return MPEG4_DEC_ERR;
    }
	else if(coding_type == MPEG4_RESYNC_VOP)
	{
		return MPEG4_DEC_ERR;
	}
    else if ((coding_type != MPEG4_I_VOP) && (isMpeg4) && (mp4Hdr_Cur->frameNumber == 0))
    {
        ALOGE("The 1st frame is not Intra Frame!");
        return MPEG4_HDR_ERR;
    }

	vopType = coding_type;

	if (coding_type == MPEG4_N_VOP)
	{
		if (mp4Hdr_Cur->packed_mode)
		{
			#ifdef PRINT_OUTPUT_ENABLE
			fprintf(stdout, "packed_mode: special-N_VOP treament!\n");
			fprintf(stdout, "CodingType:%c / ",int2char(coding_type));
			#endif
			mp4Hdr_Ref0->frame_space->DecodeFrmNum++;

			last_timestamp = Cbits->getslicetime();
			mp4Hdr_Ref0->frame_space->ShowTime.TimeLow = last_timestamp;
		}
	}
	else
	{
		/*@jh: according parse result, init frame.*/
		if(mpeg4_frame_manage(mp4Hdr_Cur))
		{
            return MPEG4_DEC_ERR;
        }

		mp4Hdr_Cur->frame_space->ShowTime.TimeLow = Cbits->getslicetime();

		if (mp4Hdr_Cur->frame_space->ShowTime.TimeLow != (RK_U32)last_timestamp)
		{
			time_inc = mp4Hdr_Cur->frame_space->ShowTime.TimeLow - last_timestamp;
		}

        if(mp4Hdr_Cur->interlacing) {
            if(mp4Hdr_Cur->top_field_first) {
                mp4Hdr_Cur->frame_space->FrameType = 1;
            } else {
                mp4Hdr_Cur->frame_space->FrameType = 2;
            }
        }else{
             mp4Hdr_Cur->frame_space->FrameType = VPU_OUTPUT_FRAME_TYPE;
        }
		last_timestamp = mp4Hdr_Cur->frame_space->ShowTime.TimeLow;

		mp4Hdr_Cur->frame_space->DecodeFrmNum = Cbits->getframeID();

		#ifdef PRINT_OUTPUT_ENABLE
		fprintf(stdout, "CodingType:%c / ",int2char(coding_type));
		#endif

		/*@jh: config register*/
		mpeg4_hwregister_config(mp4Hdr_Cur);

		/*@jh:start hw*/
		 {
            int     i;
            VPU_CMD_TYPE cmd;

            VPUClientSendReg(socket, (RK_U32*)mp4Regs, VPU_REG_NUM_DEC);
            {
                RK_S32 len;
                if(VPUClientWaitResult(socket, (RK_U32*)&mp4RetRegs, VPU_REG_NUM_DEC,
                                    &cmd, &len))
                {
                     ALOGV("VPUClientWaitResult error \n");
                }
            }
        }
	}

	VPU_TRACE("<--- <mpeg4_decode_frame>\n");

	return MPEG4_DEC_OK;
}

/******************************************************************************************************
	Func:
		mpeg4_decoder_endframe
	Description:
		manage MPEG4_HEADER structer contents between current, reference0 and reference 1
	Author:
		Jian Huan
	Date:
		2010-11-19 10:26:48
 ******************************************************************************************************/
RK_S32 Mpeg4_Decoder::mpeg4_decoder_endframe()
{
	VPU_FRAME	*frame = NULL;
	RK_S32		timestamp;

	if (mp4Hdr_Cur->coding_type == MPEG4_N_VOP)
	{
		if (mp4Hdr_Cur->packed_mode)
		{
			mp4Hdr_Cur->frame_space = NULL;

			return VPU_ERR;
		}
	}
	else if (mp4Hdr_Cur->coding_type == MPEG4_B_VOP)
	{
		if ((!(mFlags & MPEG4_DEC_USE_PRESENTATION_TIME)) &&
                (mp4Hdr_Ref0->frame_space != MPEG4_RK_FRAME_NULL)) {
			timestamp = mp4Hdr_Ref0->frame_space->ShowTime.TimeLow;

			if (mp4Hdr_Ref0->frame_space->ShowTime.TimeLow != mp4Hdr_Cur->frame_space->ShowTime.TimeLow)
			{
				mp4Hdr_Ref0->frame_space->ShowTime.TimeLow = mp4Hdr_Cur->frame_space->ShowTime.TimeLow;
			}
			else if (time_inc > 0)
			{
				mp4Hdr_Ref0->frame_space->ShowTime.TimeLow += time_inc;
			}
			mp4Hdr_Cur->frame_space->ShowTime.TimeLow = timestamp;
		}

		mp4Hdr_Cur->frame_space->ColorType = VPU_OUTPUT_FORMAT_YUV420_SEMIPLANAR;


		Cfrm->push_display(mp4Hdr_Cur->frame_space);
		Cfrm->free_frame(mp4Hdr_Cur->frame_space);
		mp4Hdr_Cur->frame_space = NULL;
	}
	else if (mp4Hdr_Cur->coding_type != MPEG4_INVALID_VOP)
	{
		MPEG4_HEADER	*tmpHdr;

		if (mp4Hdr_Ref0->frame_space)
		{
			mp4Hdr_Ref0->frame_space->ColorType = VPU_OUTPUT_FORMAT_YUV420_SEMIPLANAR;


			Cfrm->push_display(mp4Hdr_Ref0->frame_space);
		}

		if (mp4Hdr_Ref1->frame_space)
		{

			Cfrm->free_frame(mp4Hdr_Ref1->frame_space);
			mp4Hdr_Ref1->frame_space = NULL;
		}

#ifdef WIN32
		if (frame != NULL)
			frame->CodingType = mp4Hdr_Ref0->coding_type;
#endif
		tmpHdr		= mp4Hdr_Ref1;
		mp4Hdr_Ref1 = mp4Hdr_Ref0;
		mp4Hdr_Ref0 = mp4Hdr_Cur;
		mp4Hdr_Cur 	= tmpHdr;

		memcpy(mp4Hdr_Cur, mp4Hdr_Ref0, sizeof(MPEG4_HEADER));
		mp4Hdr_Cur->frame_space = NULL;
	}

	return VPU_OK;
}

VPU_FRAME*  Mpeg4_Decoder::mpeg4_decoder_displayframe(VPU_FRAME *frame)
{
	VPU_FRAME	*fr;

	fr = Cfrm->get_display();
	if (fr)
	{
		*frame = *fr;
		VPUMemDuplicate(&frame->vpumem, &fr->vpumem);
        VPU_TRACE("frame addr %08x, %08x\n", frame->FrameBusAddr[0], frame->FrameBusAddr[1]);
		Cfrm->free_frame(fr);
	}
	else
		memset(frame,0,sizeof(VPU_FRAME));

    return NULL;
}

RK_S32 Mpeg4_Decoder::mpeg4_flush_oneframe_dpb(VPU_FRAME *frame)
{
    if ((frame ==NULL) || (mFlushDpbAtEos)) {
        return -1;
    }

    if (mp4Hdr_Ref0->frame_space ==MPEG4_RK_FRAME_NULL) {
        return -1;
    }

    VPUMemDuplicate(&frame->vpumem, &mp4Hdr_Ref0->frame_space->vpumem);
    frame->ShowTime.TimeLow = last_timestamp * 1000;
    frame->DisplayWidth = ImgWidth;
    frame->DisplayHeight = ImgHeight;
    frame->ColorType = VPU_OUTPUT_FORMAT_YUV420_SEMIPLANAR;
    mFlushDpbAtEos = 1;

    return 0;
}

void Mpeg4_Decoder::reset_keyframestate()
{
	VPU_FRAME	*fr;
	while(fr = Cfrm->get_display())
	{
		Cfrm->free_frame(fr);
	}
	if (mp4Hdr_Ref0->frame_space)
	    Cfrm->free_frame(mp4Hdr_Ref0->frame_space);
	if (mp4Hdr_Ref1->frame_space)
		Cfrm->free_frame(mp4Hdr_Ref1->frame_space);
	mp4Hdr_Ref0->frame_space = NULL;
	mp4Hdr_Ref1->frame_space = NULL;
    mp4Hdr_Cur->frameNumber = 0;
}

RK_S32 Mpeg4_Decoder::mpeg4_control(RK_U32 cmd_type, void* data)
{
    int32_t ret = 0;
    switch (cmd_type){
        case VPU_API_USE_PRESENT_TIME_ORDER: {
            ALOGD("VPU_API_USE_PRESENT_TIME_ORDER");
            mFlags |=MPEG4_DEC_USE_PRESENTATION_TIME;
        } break;
        default : {
            ALOGI("invalid command %d", cmd_type);
            ret = -1;
        } break;
    }
    return ret;
}

/***************************************************************************************************
	File :
		mpeg4_test_new_slice
	Description:
		test new slice in the same chunk buffer
	Return:
		VPU_OK: 	if no new slice existed, in other words, only 1 frame in one chunk;
		VPU_ERR: 	a new slice existed, in other words, 2 frames in one chunk;
	Author:
		Jian Huan
	Date:
		2010-11-24 11:42:08
 **************************************************************************************************/
RK_S32 Mpeg4_Decoder::mpeg4_test_new_slice()
{
	RK_U32	bits_phyaddr;
	RK_S32  status = VPU_OK, i;

	if (isMpeg4)
	    bits_phyaddr = mp4RetRegs[12];
		status = Cbits->testslice(bits_phyaddr, ((vopType!=MPEG4_B_VOP)&&(vopType!=MPEG4_N_VOP)));
	}

	return status;
}

/******************************************************************************************************
	Func:
		mpeg4_hwregister_init
	Description:
		Hardware Register Initialization before decoding start
	Author:
		Jian Huan
	Date:
		2010-11-17 15:52:23
 ******************************************************************************************************/
RK_S32 Mpeg4_Decoder::mpeg4_hwregister_init()
{
	memset(mp4Regs, 0, DEC_X170_REGISTERS*sizeof(RK_U32));

	Cregs->SetRegisterMapAddr(mp4Regs);

    Cregs->SetRegisterFile(HWIF_DEC_OUT_ENDIAN,		DEC_X170_LITTLE_ENDIAN);			//little endian
    Cregs->SetRegisterFile(HWIF_DEC_IN_ENDIAN,  	DEC_X170_LITTLE_ENDIAN);
    Cregs->SetRegisterFile(HWIF_DEC_STRENDIAN_E,	DEC_X170_LITTLE_ENDIAN);
    Cregs->SetRegisterFile(HWIF_DEC_OUT_TILED_E,   	0);
    Cregs->SetRegisterFile(HWIF_DEC_MAX_BURST,		DEC_X170_BUS_BURST_LENGTH_16);
	Cregs->SetRegisterFile(HWIF_DEC_SCMD_DIS, 		0);									//enable
	Cregs->SetRegisterFile(HWIF_DEC_ADV_PRE_DIS, 	0);
	Cregs->SetRegisterFile(HWIF_APF_THRESHOLD, 		1);
    Cregs->SetRegisterFile(HWIF_DEC_LATENCY,		0);
    Cregs->SetRegisterFile(HWIF_DEC_DATA_DISC_E,	0);
    Cregs->SetRegisterFile(HWIF_DEC_OUTSWAP32_E,	1);									//VERIFICATION
    Cregs->SetRegisterFile(HWIF_DEC_INSWAP32_E,		1);									//VERIFICATION
    Cregs->SetRegisterFile(HWIF_DEC_STRSWAP32_E,	1);									//VERIFICATION
	Cregs->SetRegisterFile(HWIF_DEC_TIMEOUT_E, 		1);
    Cregs->SetRegisterFile(HWIF_DEC_CLK_GATE_E, 	1);
	Cregs->SetRegisterFile(HWIF_DEC_IRQ_DIS, 		0);
    Cregs->SetRegisterFile(HWIF_DEC_E, 		        1);

    /* Set prediction filter taps */
    Cregs->SetRegisterFile(HWIF_PRED_BC_TAP_0_0,  	-1);
    Cregs->SetRegisterFile(HWIF_PRED_BC_TAP_0_1,   	3);
    Cregs->SetRegisterFile(HWIF_PRED_BC_TAP_0_2,  	-6);
    Cregs->SetRegisterFile(HWIF_PRED_BC_TAP_0_3,	20);

    /* set AXI RW IDs */
    Cregs->SetRegisterFile(HWIF_DEC_AXI_RD_ID, 		0);									//UNKNOWN
    Cregs->SetRegisterFile(HWIF_DEC_AXI_WR_ID, 		0);									//???

	return MPEG4_DEC_OK;
}

/******************************************************************************************************
	Func:
		mpeg4_hwregister_config
	Description:
		Hardware Register Config after mpeg4 header parsing finished
	Author:
		Jian Huan
	Date:
		2010-11-17 15:53:18
 ******************************************************************************************************/
RK_S32 Mpeg4_Decoder::mpeg4_hwregister_config(MPEG4_HEADER *mp4Hdr)
{
	VPU_TRACE("---> <mpeg4_hwregister_config>\n");

    if(VPUMemClean(&vpu_q_mem))
    {
        ALOGE("mpeg4 vpu_q_mem VPUMemClean error");
        return MPEG4_DEC_ERR;
    }

	Cregs->SetRegisterFile(HWIF_PIC_MB_WIDTH,		mp4Hdr->mb_width);
	Cregs->SetRegisterFile(HWIF_PIC_MB_HEIGHT_P,	mp4Hdr->mb_height);
	if (cusomVersion == 4)
	{
		Cregs->SetRegisterFile(HWIF_MB_WIDTH_OFF, 	mp4Hdr->width_offset);
		Cregs->SetRegisterFile(HWIF_MB_HEIGHT_OFF, 	mp4Hdr->height_offset);
	}
	else
	{
		Cregs->SetRegisterFile(HWIF_MB_WIDTH_OFF, 	0);
		Cregs->SetRegisterFile(HWIF_MB_HEIGHT_OFF, 	0);
	}
	Cregs->SetRegisterFile(HWIF_DEC_MODE, 			1);
	Cregs->SetRegisterFile(HWIF_ALT_SCAN_E, 		mp4Hdr->alternate_vertical_scan);

	Cregs->SetRegisterFile(HWIF_STARTMB_X, 			0);
	Cregs->SetRegisterFile(HWIF_STARTMB_Y, 			0);
	Cregs->SetRegisterFile(HWIF_BLACKWHITE_E, 		0);			// 4:2:0 sampling format
	Cregs->SetRegisterFile(HWIF_DEC_OUT_BASE,		mp4Hdr_Cur->frame_space->FrameBusAddr[0]);
	Cregs->SetRegisterFile(HWIF_DEC_OUT_DIS,		0);
	Cregs->SetRegisterFile(HWIF_FILTERING_DIS,		1);

	Cregs->SetRegisterFile(HWIF_MPEG4_RC, 		    mp4Hdr->rounding);
	Cregs->SetRegisterFile(HWIF_INTRADC_VLC_THR, 	mp4Hdr->intra_dc_threshold);
	Cregs->SetRegisterFile(HWIF_INIT_QP, 			mp4Hdr->quant);
	Cregs->SetRegisterFile(HWIF_SYNC_MARKER_E, 		1);

	VPU_TRACE("rounding = %d\n", mp4Hdr->rounding);
	VPU_TRACE("intra_dc_threshold = %d\n", mp4Hdr->intra_dc_threshold);
	VPU_TRACE("quant = %d\n", mp4Hdr->quant);
	if(mp4Hdr->rlcmode == 1)
	{
		Cregs->SetRegisterFile(HWIF_RLC_MODE_E, 		1);
		Cregs->SetRegisterFile(HWIF_RLC_VLC_BASE,		0);
		Cregs->SetRegisterFile(HWIF_MB_CTRL_BASE,		0);		//RLC mode MB Control Register
		Cregs->SetRegisterFile(HWIF_MPEG4_DC_BASE,		0);
		Cregs->SetRegisterFile(HWIF_DIFF_MV_BASE,		0);
		Cregs->SetRegisterFile(HWIF_STREAM_LEN,			0);
		Cregs->SetRegisterFile(HWIF_STRM_START_BIT,		0);
	}
	else
	{
		RK_U32 	consumed_bits, bit_position, byte_len, chunk_size;
		RK_U32	base_phyaddr, start_byteaddr;

		consumed_bits = Cbits->getreadbits();

		base_phyaddr	= Cbits->getbasephyaddr();
        if(!VPUMemJudgeIommu()){
			start_byteaddr	= base_phyaddr + (((consumed_bits >> 3))&(~0X7));
        }else{
		    start_byteaddr	= base_phyaddr | ((((consumed_bits >> 3))&(~0X7))<<10);
        }

		bit_position = (consumed_bits & 0x3F);

		chunk_size = Cbits->getchunksize();
		byte_len = chunk_size - (((consumed_bits >> 3))&(~0X7));

		Cregs->SetRegisterFile(HWIF_RLC_MODE_E, 		0);
		Cregs->SetRegisterFile(HWIF_RLC_VLC_BASE, 		start_byteaddr);
		Cregs->SetRegisterFile(HWIF_STRM_START_BIT, 	bit_position);
		Cregs->SetRegisterFile(HWIF_STREAM_LEN, 		byte_len);
	}

	Cregs->SetRegisterFile(HWIF_VOP_TIME_INCR, 	mp4Hdr->time_inc_resolution);
	VPU_TRACE("time_inc_resolution = %d\n", 		mp4Hdr->time_inc_resolution);

	if(mp4Hdr->coding_type == MPEG4_B_VOP)
	{
		RK_U32 trb_per_trd_d0, trb_per_trd_d1, trb_per_trd_dm1;

		trb_per_trd_d0 = ((((RK_S64)(1*mp4Hdr->time_bp + 0)) << 27) + 1*(mp4Hdr->time_pp - 1))/ mp4Hdr->time_pp;
		trb_per_trd_d1 = ((((RK_S64)(2*mp4Hdr->time_bp + 1)) << 27) + 2*(mp4Hdr->time_pp - 0))/ (2*mp4Hdr->time_pp + 1);
		trb_per_trd_dm1= ((((RK_S64)(2*mp4Hdr->time_bp - 1)) << 27) + 2*(mp4Hdr->time_pp - 1))/ (2*mp4Hdr->time_pp - 1);

		mp4Hdr->rounding = 0;

		Cregs->SetRegisterFile(HWIF_PIC_B_E, 			1);
		Cregs->SetRegisterFile(HWIF_PIC_INTER_E,		1);
		Cregs->SetRegisterFile(HWIF_MPEG4_RC, 		    mp4Hdr->rounding);
		Cregs->SetRegisterFile(HWIF_REFER0_BASE,		mp4Hdr_Ref1->frame_space->FrameBusAddr[0]);
		Cregs->SetRegisterFile(HWIF_REFER1_BASE,		mp4Hdr_Ref1->frame_space->FrameBusAddr[0]);
		Cregs->SetRegisterFile(HWIF_REFER2_BASE,		mp4Hdr_Ref0->frame_space->FrameBusAddr[0]);
		Cregs->SetRegisterFile(HWIF_REFER3_BASE,		mp4Hdr_Ref0->frame_space->FrameBusAddr[0]);
		Cregs->SetRegisterFile(HWIF_FCODE_FWD_HOR,		mp4Hdr->fcode_forward);
		Cregs->SetRegisterFile(HWIF_FCODE_FWD_VER,		mp4Hdr->fcode_forward);
		Cregs->SetRegisterFile(HWIF_FCODE_BWD_HOR,		mp4Hdr->fcode_backward);
		Cregs->SetRegisterFile(HWIF_FCODE_BWD_VER,		mp4Hdr->fcode_backward);
		Cregs->SetRegisterFile(HWIF_WRITE_MVS_E, 		0);
		Cregs->SetRegisterFile(HWIF_DIR_MV_BASE, 		directMV_Addr);
		Cregs->SetRegisterFile(HWIF_TRB_PER_TRD_D0,		trb_per_trd_d0);
		Cregs->SetRegisterFile(HWIF_TRB_PER_TRD_D1,		trb_per_trd_d1);
		Cregs->SetRegisterFile(HWIF_TRB_PER_TRD_DM1,	trb_per_trd_dm1);
		mpeg4_hw_refbuf_config(mp4Hdr);
	}
	else
	{
		Cregs->SetRegisterFile(HWIF_PIC_B_E, 			0);
		if(mp4Hdr->coding_type == MPEG4_P_VOP)
		{
			Cregs->SetRegisterFile(HWIF_PIC_INTER_E,	1);
			Cregs->SetRegisterFile(HWIF_REFER0_BASE,	mp4Hdr_Ref0->frame_space->FrameBusAddr[0]);
			Cregs->SetRegisterFile(HWIF_REFER1_BASE,	mp4Hdr_Ref0->frame_space->FrameBusAddr[0]);
			Cregs->SetRegisterFile(HWIF_FCODE_FWD_HOR,	mp4Hdr->fcode_forward);
			Cregs->SetRegisterFile(HWIF_FCODE_FWD_VER,	mp4Hdr->fcode_forward);
			Cregs->SetRegisterFile(HWIF_WRITE_MVS_E, 	1);
			Cregs->SetRegisterFile(HWIF_DIR_MV_BASE, 	directMV_Addr);
			mpeg4_hw_refbuf_config(mp4Hdr);
		}
		else if(mp4Hdr->coding_type == MPEG4_I_VOP)
		{
			Cregs->SetRegisterFile(HWIF_PIC_INTER_E,	0);
			Cregs->SetRegisterFile(HWIF_WRITE_MVS_E, 	0);
			Cregs->SetRegisterFile(HWIF_DIR_MV_BASE, 	directMV_Addr);
			Cregs->SetRegisterFile(HWIF_FCODE_FWD_HOR,	1);
			Cregs->SetRegisterFile(HWIF_FCODE_FWD_VER,	1);
		}
		else /*N_VOP*/
		{
			/*How to deal with N vop???*/;
		}
	}
	VPU_TRACE("fcode_forward = %d\n", 		mp4Hdr->fcode_forward);
	if(mp4Hdr->interlacing == 1)
	{
		Cregs->SetRegisterFile(HWIF_PIC_INTERLACE_E, 	1);
		Cregs->SetRegisterFile(HWIF_PIC_FIELDMODE_E, 	0);
		Cregs->SetRegisterFile(HWIF_TOPFIELDFIRST_E, 	mp4Hdr->top_field_first);
	}

	Cregs->SetRegisterFile(HWIF_PREV_ANC_TYPE, 	mp4Hdr_Ref0->coding_type == MPEG4_P_VOP);

	Cregs->SetRegisterFile(HWIF_TYPE1_QUANT_E, 	mp4Hdr->quant_type);
	VPU_TRACE("quant_type = %d\n", 		mp4Hdr->quant_type);

	Cregs->SetRegisterFile(HWIF_QTABLE_BASE, 	vpu_q_mem.phy_addr);

	Cregs->SetRegisterFile(HWIF_MV_ACCURACY_FWD, mp4Hdr->quarterpel);

	if(mp4Hdr->version == MPEG4_STANDARD_VERSION)
	{
		Cregs->SetRegisterFile(HWIF_SKIP_MODE,		0);
	}
	VPU_TRACE("<--- <mpeg4_hwregister_config>\n");

	return MPEG4_DEC_OK;
}
/**************************************************************************************
	Func:
		mpeg4_hw_refbuf_init
	Description:
		RK Reference Buffer Init
	Author:
		Jian Huan
	Date:
		2010-11-18 9:20:38
 **************************************************************************************/
RK_S32 Mpeg4_Decoder::mpeg4_hw_refbuf_init()
{
	return MPEG4_DEC_OK;
}

/**************************************************************************************
	Func:
		mpeg4_hw_refbuf_init
	Description:
		RK Reference Buffer Init
	Author:
		Jian Huan
	Date:
		2010-11-18 9:26:56
 **************************************************************************************/
RK_S32 Mpeg4_Decoder::mpeg4_hw_refbuf_config(MPEG4_HEADER *mp4Hdr)
{
	return MPEG4_DEC_OK;
}

/***************************************************************************************************
	Func:
		mpeg4_hwregister_start
	Description:
		hw setup
	Author:
		Jian Huan
	Date:
		2010-12-9 11:16:27
 **************************************************************************************************/
RK_S32 Mpeg4_Decoder::mpeg4_hwregister_start()
{
	Cregs->HwStart();

	return MPEG4_DEC_OK;
}

/***************************************************************************************************
	Func:
		mpeg4_hwregister_waitfinish
	Description:
		hw finish
	Author:
		Jian Huan
	Date:
		2010-12-9 11:16:27
 **************************************************************************************************/
RK_S32 Mpeg4_Decoder::mpeg4_hwregister_waitfinish()
{
	Cregs->HwFinish();

	return MPEG4_DEC_OK;
}

/***************************************************************************************************
	Func:
		mpeg4_dumpyuv
	Desciption:
		Dump YUV into file
	Author:
		Jian Huan
	Date:
		2010-12-7 17:17:58
 **************************************************************************************************/
RK_S32 Mpeg4_Decoder::mpeg4_dumpyuv(FILE *fp, VPU_FRAME *frame)
{
#ifndef WIN32
	RK_U32 yuv_addr;
	RK_U32 width;
	RK_U32 height;

	if (frame == NULL)
	{
		#ifdef TRACE_ENABLE
		fprintf(stdout, "DO NOT output this frame, because 1st frame or N-vop!\n");
		#endif
		return MPEG4_DEC_OK;
	}

	yuv_addr = frame->FrameBusAddr[0];
	width	  = frame->FrameWidth;
	height	  = frame->FrameHeight;

	fwrite((RK_U8*)yuv_addr, 1, width*height*3/2, fp);
	fflush(fp);
#endif

#ifdef PRINT_OUTPUT_ENABLE
	if (frame != MPEG4_RK_FRAME_NULL)
		fprintf(stdout, "Output: No.%2d frame(%c) / time: %5d\n",
				frame->DecodeFrmNum - 1, int2char(frame->CodingType), frame->ShowTime.TimeLow/1000);
	else
		fprintf(stdout, "Output: -NULL-!\n");
#endif

	return MPEG4_DEC_OK;
}
