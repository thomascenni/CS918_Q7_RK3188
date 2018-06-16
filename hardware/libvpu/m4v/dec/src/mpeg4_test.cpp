/***************************************************************************************************
    File:
        Mpeg4_Test.c
    Description:
        Test routine for RK MPEG4 Decoder,including MPEG-4
    Author:
        Jian Huan
    Date:
        2010-11-17 11:56:26
 **************************************************************************************************/
#include "stdio.h"
#include "stdlib.h"
#ifdef WIN32
#include <memory.h>
#else
#include <string.h>
#endif
#include "mpeg4_rk_decoder.h"
#include "vpu_mem.h"


  
#define MPEG4_BITSTREAM_BUFFER_SIZE     102400

FILE	*fp_mpeg4;
FILE	*fp_yuv;

#define VIDEO_CHUNK_GET_OK			 	0
#define VIDEO_CHUNK_GET_FAIL			-1
#define VIDEO_CHUNK_GET_FINISH		 	1

#define INDEX_NUM	300
RK_U32	indexChunkOffset[INDEX_NUM];		//temp suport INDEX_NUM frames

RK_U8 Mpeg4BitsTable[204800*2];

RK_S32 buildIndex(FILE *fp)
{
	indexChunkOffset[  0] = 0x00000000;
	indexChunkOffset[  1] = 0x00000e08;
	indexChunkOffset[  2] = 0x00001e10;
	indexChunkOffset[  3] = 0x000027f0;
	indexChunkOffset[  4] = 0x0000322c;
	indexChunkOffset[  5] = 0x000040b8;
	indexChunkOffset[  6] = 0x000040e0;
	indexChunkOffset[  7] = 0x00004ecc;
	indexChunkOffset[  8] = 0x00004ef4;
	indexChunkOffset[  9] = 0x00006050;
	indexChunkOffset[ 10] = 0x0000748c;
	indexChunkOffset[ 11] = 0x000089b8;
	indexChunkOffset[ 12] = 0x00009dd8;
	indexChunkOffset[ 13] = 0x0000af00;
	indexChunkOffset[ 14] = 0x0000c2cc;
	indexChunkOffset[ 15] = 0x0000e480;
	indexChunkOffset[ 16] = 0x0000e4a8;
	indexChunkOffset[ 17] = 0x000106dc;
	indexChunkOffset[ 18] = 0x00010704;
	indexChunkOffset[ 19] = 0x0001283c;
	indexChunkOffset[ 20] = 0x00012864;
	indexChunkOffset[ 21] = 0x00015650;
	indexChunkOffset[ 22] = 0x00015678;
	indexChunkOffset[ 23] = 0x00019660;
	indexChunkOffset[ 24] = 0x00019688;
	indexChunkOffset[ 25] = 0x0001e18c;
	indexChunkOffset[ 26] = 0x0001e1b4;
	indexChunkOffset[ 27] = 0x000207c8;
	indexChunkOffset[ 28] = 0x000229fc;
	indexChunkOffset[ 29] = 0x00025308;
	indexChunkOffset[ 30] = 0x00029b20;
	indexChunkOffset[ 31] = 0x00029b48;
	indexChunkOffset[ 32] = 0x0002c48c;
	indexChunkOffset[ 33] = 0x0002c4b4;
	indexChunkOffset[ 34] = 0x0002efb4;
	indexChunkOffset[ 35] = 0x0002efdc;
	indexChunkOffset[ 36] = 0x00031318;
	indexChunkOffset[ 37] = 0x00031340;
	indexChunkOffset[ 38] = 0x00033ab4;
	indexChunkOffset[ 39] = 0x00033adc;
	indexChunkOffset[ 40] = 0x00036014;
	indexChunkOffset[ 41] = 0x0003603c;
	indexChunkOffset[ 42] = 0x00037ba0;
	indexChunkOffset[ 43] = 0x00037bc8;
	indexChunkOffset[ 44] = 0x00039990;
	indexChunkOffset[ 45] = 0x000399b8;
	indexChunkOffset[ 46] = 0x0003b5ac;
	indexChunkOffset[ 47] = 0x0003b5d4;
	indexChunkOffset[ 48] = 0x0003cbc4;
	indexChunkOffset[ 49] = 0x0003cbec;
	indexChunkOffset[ 50] = 0x0003e1e0;
	indexChunkOffset[ 51] = 0x0003e208;
	indexChunkOffset[ 52] = 0x0003f5e4;
	indexChunkOffset[ 53] = 0x0003f60c;
	indexChunkOffset[ 54] = 0x00040638;
	indexChunkOffset[ 55] = 0x00040660;
	indexChunkOffset[ 56] = 0x000412c0;
	indexChunkOffset[ 57] = 0x000412e8;
	indexChunkOffset[ 58] = 0x00041b5c;
	indexChunkOffset[ 59] = 0x00041b84;
	indexChunkOffset[ 60] = 0x000424a0;
	indexChunkOffset[ 61] = 0x000424c8;
	indexChunkOffset[ 62] = 0x00042b64;
	indexChunkOffset[ 63] = 0x00042b8c;
	indexChunkOffset[ 64] = 0x00043194;
	indexChunkOffset[ 65] = 0x000431bc;
	indexChunkOffset[ 66] = 0x000437b8;
	indexChunkOffset[ 67] = 0x000437e0;
	indexChunkOffset[ 68] = 0x00043ea0;
	indexChunkOffset[ 69] = 0x00043ec8;
	indexChunkOffset[ 70] = 0x00044654;
	indexChunkOffset[ 71] = 0x0004467c;
	indexChunkOffset[ 72] = 0x00044e54;
	indexChunkOffset[ 73] = 0x00044e7c;
	indexChunkOffset[ 74] = 0x000453e4;
	indexChunkOffset[ 75] = 0x0004540c;
	indexChunkOffset[ 76] = 0x000458f4;
	indexChunkOffset[ 77] = 0x0004591c;
	indexChunkOffset[ 78] = 0x00046064;
	indexChunkOffset[ 79] = 0x0004608c;
	indexChunkOffset[ 80] = 0x00046c2c;
	indexChunkOffset[ 81] = 0x00047168;
	indexChunkOffset[ 82] = 0x00047cc8;
	indexChunkOffset[ 83] = 0x00049194;
	indexChunkOffset[ 84] = 0x0004ab64;
	indexChunkOffset[ 85] = 0x0004c338;
	indexChunkOffset[ 86] = 0x0004d434;
	indexChunkOffset[ 87] = 0x0004eb80;
	indexChunkOffset[ 88] = 0x0004eba8;
	indexChunkOffset[ 89] = 0x0004ff5c;
	indexChunkOffset[ 90] = 0x0004ff84;
	indexChunkOffset[ 91] = 0x00050b78;
	indexChunkOffset[ 92] = 0x000522fc;
	indexChunkOffset[ 93] = 0x00052324;
	indexChunkOffset[ 94] = 0x000538d0;
	indexChunkOffset[ 95] = 0x000538f8;
	indexChunkOffset[ 96] = 0x000542b4;
	indexChunkOffset[ 97] = 0x00056000;
	indexChunkOffset[ 98] = 0x00059214;
	indexChunkOffset[ 99] = 0x0005b794;

	return 0;

}
RK_S32 getNextVideoChunk(VPUMemLinear_t *p)
{
	static int index = 0;
	RK_U32	offset;
	RK_U32	bufsize, size;
	RK_U8	*ptr;
	RK_U8 	*bitBuf;

	bitBuf 	= (RK_U8 *)p->vir_addr;
	bufsize	= p->size;
	
	//flush memory
	memset(bitBuf, 0x0, bufsize*sizeof(RK_S8));

	buildIndex(fp_mpeg4);

	offset = indexChunkOffset[index];
	
	if(offset != 0xFFFFFFFF)
	{
		size = (RK_S32)indexChunkOffset[index + 1] - (RK_S32)offset;
		
		index++;
		
		if(size < 0)
			return VIDEO_CHUNK_GET_FAIL;
		
		ptr = Mpeg4BitsTable + offset;

		for (RK_S32 k = 0; k < size; k++)
		{
			bitBuf[k] = (RK_U8)ptr[k];
		}
		p->size = size;
		
		return VIDEO_CHUNK_GET_OK;
	}
	else
	{
		return VIDEO_CHUNK_GET_FAIL;
	}
}


/*****************************************************************************
    Function:
        main
    Description:
        test entrance
    Author:
        Jian Huan
    Date:
        2010-11-17 11:58:29
 *****************************************************************************/
int main()
{
	RK_U8			*bitbuf_ptr;
	RK_S32			needNewChunk = VPU_OK, status, ret;
	RK_U32			frame=0, chunkSize = 0;
	Mpeg4_Decoder	mpeg4_dec;
	VPU_GENERIC		vpu_generic_info;
	VPU_FRAME*		output_frame;
	VPUMemLinear_t 	vpu_bitsbuf;

    VPU_TRACE("*******Begin MPEG4 Decoding Now*******\n");


	fp_mpeg4 = fopen("/sdcard/sec1.mpeg4.vid", "rb");
  	
	if (fp_mpeg4 == NULL)
	{
		VPU_TRACE("cannot open vid file in sdcard!\n");
		return -1;
	} 
	
	fp_yuv = fopen("/sdcard/sec1.mpeg4.vid.yuv", "wb");

	if (fp_yuv == NULL) {
		VPU_TRACE("cannot open yuv file in sdcard!\n");
		return -1;
	}

	fread(Mpeg4BitsTable, 1, 204800*2, fp_mpeg4);
	ret = VPUMallocLinear(&vpu_bitsbuf, MPEG4_BITSTREAM_BUFFER_SIZE);
	if (ret)
	{
		VPU_TRACE("bitstream buffer malloc error!");
		goto MAIN_EXCEPTIONAL;
	}

	vpu_generic_info.CodecType	= VPU_CODEC_DEC_MPEG4;
	vpu_generic_info.ImgWidth		= 0;
	vpu_generic_info.ImgHeight	= 0;

	/*@jh: decoder init*/
	ret = mpeg4_dec.mpeg4_decode_init(&vpu_generic_info);

	if (ret != MPEG4_INIT_OK)
	{
		VPU_TRACE("decoder init error!\n");

		goto MAIN_EXCEPTIONAL;
	}
	
	mpeg4_dec.mpeg4_hwregister_init();
	do
	{
		if (needNewChunk == VPU_OK)
		{
			ret = getNextVideoChunk(&vpu_bitsbuf);
			if (ret != VIDEO_CHUNK_GET_OK)
				break;

			bitbuf_ptr = (RK_U8 *)vpu_bitsbuf.vir_addr;
			chunkSize  = vpu_bitsbuf.size;

			VPU_TRACE("chunkSize = %d\n", chunkSize);
			VPU_TRACE("the first 4 binary number is: 0x%02x, 0x%02x, 0x%02x, 0x%02x\n", bitbuf_ptr[0], bitbuf_ptr[1], bitbuf_ptr[2], bitbuf_ptr[3]);

			ret = mpeg4_dec.getVideoFrame(&vpu_bitsbuf);
			if (ret != VPU_OK)
				break;
		}

		VPU_TRACE("~~~~~~~~~Bitstream Buffer~~~~~~~~~\n");
		VPU_TRACE("virtual address: 0x%08X\n", (RK_U32)vpu_bitsbuf.vir_addr);
		VPU_TRACE("Physical address: 0x%08X\n", vpu_bitsbuf.phy_addr);
		VPU_TRACE("Size: 0x%08X\n\n", vpu_bitsbuf.size);

		status = mpeg4_dec.mpeg4_decode_frame();
		
		mpeg4_dec.mpeg4_decoder_endframe();

		output_frame = mpeg4_dec.mpeg4_decoder_displayframe();

		mpeg4_dec.mpeg4_dumpyuv(fp_yuv, output_frame);

		/*@jh: estimate another frame in the same chunk*/
		needNewChunk = mpeg4_dec.mpeg4_test_new_slice();
	}
	while(status == MPEG4_DEC_OK);

MAIN_EXCEPTIONAL:
	if(fp_mpeg4)
	    fclose(fp_mpeg4);

	if(fp_yuv)
		fclose(fp_yuv);

	#ifdef TRACE_ENABLE
	fprintf(stdout, "RK Decoding Finished!\n");
	#endif

	return 0;
}
