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

  
#define MPEG4_BITSTREAM_BUFFER_SIZE     102400

FILE	*fp_mpeg4;
FILE	*fp_yuv;

#define VIDEO_CHUNK_GET_OK			 	0
#define VIDEO_CHUNK_GET_FAIL			-1
#define VIDEO_CHUNK_GET_FINISH		 	1

#define INDEX_NUM	300
RK_U32	indexChunkOffset[INDEX_NUM];		//temp suport INDEX_NUM frames

RK_U8 Mpeg4BitsTable[204800*2/*4459750*/];

RK_S32 buildIndex(FILE *fp)
{
	indexChunkOffset[  0] = 0x00000000;
	indexChunkOffset[  1] = 0x000007b4;
	indexChunkOffset[  2] = 0x0000083c;
	indexChunkOffset[  3] = 0x00000864;
	indexChunkOffset[  4] = 0x000008ec;
	indexChunkOffset[  5] = 0x00000914;
	indexChunkOffset[  6] = 0x0000099c;
	indexChunkOffset[  7] = 0x000009c4;
	indexChunkOffset[  8] = 0x00000a4c;
	indexChunkOffset[  9] = 0x00000a74;
	indexChunkOffset[ 10] = 0x00000afc;
	indexChunkOffset[ 11] = 0x00000b24;
	indexChunkOffset[ 12] = 0x00000bac;
	indexChunkOffset[ 13] = 0x00000bd4;
	indexChunkOffset[ 14] = 0x00000c5c;
	indexChunkOffset[ 15] = 0x00000c84;
	indexChunkOffset[ 16] = 0x00000d0c;
	indexChunkOffset[ 17] = 0x00000d34;
	indexChunkOffset[ 18] = 0x00000dbc;
	indexChunkOffset[ 19] = 0x00000de4;
	indexChunkOffset[ 20] = 0x00000e6c;
	indexChunkOffset[ 21] = 0x00000e94;
	indexChunkOffset[ 22] = 0x00000f1c;
	indexChunkOffset[ 23] = 0x00000f44;
	indexChunkOffset[ 24] = 0x00000f6c;
	indexChunkOffset[ 25] = 0x00001720;
	indexChunkOffset[ 26] = 0x000017a8;
	indexChunkOffset[ 27] = 0x000017d0;
	indexChunkOffset[ 28] = 0x00001858;
	indexChunkOffset[ 29] = 0x00001880;
	indexChunkOffset[ 30] = 0x00001908;
	indexChunkOffset[ 31] = 0x00001930;
	indexChunkOffset[ 32] = 0x000019b8;
	indexChunkOffset[ 33] = 0x000019e0;
	indexChunkOffset[ 34] = 0x00001a68;
	indexChunkOffset[ 35] = 0x00001a90;
	indexChunkOffset[ 36] = 0x00001ab8;
	indexChunkOffset[ 37] = 0x0000226c;
	indexChunkOffset[ 38] = 0x000022f4;
	indexChunkOffset[ 39] = 0x0000231c;
	indexChunkOffset[ 40] = 0x000023a4;
	indexChunkOffset[ 41] = 0x000023cc;
	indexChunkOffset[ 42] = 0x00002454;
	indexChunkOffset[ 43] = 0x0000247c;
	indexChunkOffset[ 44] = 0x00002504;
	indexChunkOffset[ 45] = 0x0000252c;
	indexChunkOffset[ 46] = 0x000025b4;
	indexChunkOffset[ 47] = 0x000025dc;
	indexChunkOffset[ 48] = 0x00002664;
	indexChunkOffset[ 49] = 0x0000268c;
	indexChunkOffset[ 50] = 0x000027d4;
	indexChunkOffset[ 51] = 0x000027fc;
	indexChunkOffset[ 52] = 0x00002884;
	indexChunkOffset[ 53] = 0x000028ac;
	indexChunkOffset[ 54] = 0x00002d64;
	indexChunkOffset[ 55] = 0x00002d8c;
	indexChunkOffset[ 56] = 0x00002e14;
	indexChunkOffset[ 57] = 0x00002e3c;
	indexChunkOffset[ 58] = 0x00002ee4;
	indexChunkOffset[ 59] = 0x00002f0c;
	indexChunkOffset[ 60] = 0x000030d4;
	indexChunkOffset[ 61] = 0x000030fc;
	indexChunkOffset[ 62] = 0x00003790;
	indexChunkOffset[ 63] = 0x000037b8;
	indexChunkOffset[ 64] = 0x00003e58;
	indexChunkOffset[ 65] = 0x000044c4;
	indexChunkOffset[ 66] = 0x00005008;
	indexChunkOffset[ 67] = 0x00005030;
	indexChunkOffset[ 68] = 0x00005e38;
	indexChunkOffset[ 69] = 0x00006e40;
	indexChunkOffset[ 70] = 0x00007820;
	indexChunkOffset[ 71] = 0x0000825c;
	indexChunkOffset[ 72] = 0x000090e8;
	indexChunkOffset[ 73] = 0x00009110;
	indexChunkOffset[ 74] = 0x00009efc;
	indexChunkOffset[ 75] = 0x00009f24;
	indexChunkOffset[ 76] = 0x0000b080;
	indexChunkOffset[ 77] = 0x0000c4bc;
	indexChunkOffset[ 78] = 0x0000d9e8;
	indexChunkOffset[ 79] = 0x0000ee08;
	indexChunkOffset[ 80] = 0x0000ff30;
	indexChunkOffset[ 81] = 0x000112fc;
	indexChunkOffset[ 82] = 0x000134b0;
	indexChunkOffset[ 83] = 0x000134d8;
	indexChunkOffset[ 84] = 0x0001570c;
	indexChunkOffset[ 85] = 0x00015734;
	indexChunkOffset[ 86] = 0x0001786c;
	indexChunkOffset[ 87] = 0x00017894;
	indexChunkOffset[ 88] = 0x0001a680;
	indexChunkOffset[ 89] = 0x0001a6a8;
	indexChunkOffset[ 90] = 0x0001e690;
	indexChunkOffset[ 91] = 0x0001e6b8;
	indexChunkOffset[ 92] = 0x000231bc;
	indexChunkOffset[ 93] = 0x000231e4;
	indexChunkOffset[ 94] = 0x000257f8;
	indexChunkOffset[ 95] = 0x00027a2c;
	indexChunkOffset[ 96] = 0x0002a338;
	indexChunkOffset[ 97] = 0x0002eb50;
	indexChunkOffset[ 98] = 0x0002eb78;
	indexChunkOffset[ 99] = 0x000314bc;
	indexChunkOffset[100] = 0x000314e4;
	indexChunkOffset[101] = 0x00034000;
	indexChunkOffset[102] = 0x00034028;
	indexChunkOffset[103] = 0x00036364;
	indexChunkOffset[104] = 0x0003638c;
	indexChunkOffset[105] = 0x00038b00;
	indexChunkOffset[106] = 0x00038b28;
	indexChunkOffset[107] = 0x0003b060;
	indexChunkOffset[108] = 0x0003b088;
	indexChunkOffset[109] = 0x0003cbec;
	indexChunkOffset[110] = 0x0003cc14;
	indexChunkOffset[111] = 0x0003e9dc;
	indexChunkOffset[112] = 0x0003ea04;
	indexChunkOffset[113] = 0x000405f8;
	indexChunkOffset[114] = 0x00040620;
	indexChunkOffset[115] = 0x00041c10;
	indexChunkOffset[116] = 0x00041c38;
	indexChunkOffset[117] = 0x0004322c;
	indexChunkOffset[118] = 0x00043254;
	indexChunkOffset[119] = 0x00044630;
	indexChunkOffset[120] = 0x00044658;
	indexChunkOffset[121] = 0x00045684;
	indexChunkOffset[122] = 0x000456ac;
	indexChunkOffset[123] = 0x0004630c;
	indexChunkOffset[124] = 0x00046334;
	indexChunkOffset[125] = 0x00046ba8;
	indexChunkOffset[126] = 0x00046bd0;
	indexChunkOffset[127] = 0x000474ec;
	indexChunkOffset[128] = 0x00047514;
	indexChunkOffset[129] = 0x00047bb0;
	indexChunkOffset[130] = 0x00047bd8;
	indexChunkOffset[131] = 0x000481e0;
	indexChunkOffset[132] = 0x00048208;
	indexChunkOffset[133] = 0x00048804;
	indexChunkOffset[134] = 0x0004882c;
	indexChunkOffset[135] = 0x00048eec;
	indexChunkOffset[136] = 0x00048f14;
	indexChunkOffset[137] = 0x000496a0;
	indexChunkOffset[138] = 0x000496c8;
	indexChunkOffset[139] = 0x00049ea0;
	indexChunkOffset[140] = 0x00049ec8;
	indexChunkOffset[141] = 0x0004a430;
	indexChunkOffset[142] = 0x0004a458;
	indexChunkOffset[143] = 0x0004a940;
	indexChunkOffset[144] = 0x0004a968;
	indexChunkOffset[145] = 0x0004b0b0;
	indexChunkOffset[146] = 0x0004b0d8;
	indexChunkOffset[147] = 0x0004bc78;
	indexChunkOffset[148] = 0x0004c1b4;
	indexChunkOffset[149] = 0x0004cd14;
	indexChunkOffset[150] = 0x0004e1e0;
	indexChunkOffset[151] = 0x0004fbb0;
	indexChunkOffset[152] = 0x00051384;
	indexChunkOffset[153] = 0x00052480;
	indexChunkOffset[154] = 0x00053bcc;
	indexChunkOffset[155] = 0x00053bf4;
	indexChunkOffset[156] = 0x00054fa8;
	indexChunkOffset[157] = 0x00054fd0;
	indexChunkOffset[158] = 0x00055bc4;
	indexChunkOffset[159] = 0x00057348;
	indexChunkOffset[160] = 0x00057370;
	indexChunkOffset[161] = 0x0005891c;
	indexChunkOffset[162] = 0x00058944;
	indexChunkOffset[163] = 0x00059300;
	indexChunkOffset[164] = 0x0005b044;
	indexChunkOffset[165] = 0x0005e258;
	indexChunkOffset[166] = 0x000607d8;
	indexChunkOffset[167] = 0x00064ee4;
	indexChunkOffset[168] = 0x00064f0c;
	indexChunkOffset[169] = 0x00069450;
	indexChunkOffset[170] = 0x00069478;
	indexChunkOffset[171] = 0x0006cac4;
	indexChunkOffset[172] = 0x0006caec;
	indexChunkOffset[173] = 0x00070b10;
	indexChunkOffset[174] = 0x00070b38;
	indexChunkOffset[175] = 0x00073f1c;
	indexChunkOffset[176] = 0x00073f44;
	indexChunkOffset[177] = 0x000771e8;
	indexChunkOffset[178] = 0x00078b08;
	indexChunkOffset[179] = 0x0007aef8;
	indexChunkOffset[180] = 0x0007d130;
	indexChunkOffset[181] = 0x0007ef40;
	indexChunkOffset[182] = 0x00081384;
	indexChunkOffset[183] = 0x00083cc4;
	indexChunkOffset[184] = 0x000867ac;
	indexChunkOffset[185] = 0x00089b4c;
	indexChunkOffset[186] = 0x0008c84c;
	indexChunkOffset[187] = 0x0008e85c;
	indexChunkOffset[188] = 0x00090c14;
	indexChunkOffset[189] = 0x00092b50;
	indexChunkOffset[190] = 0x000947d4;
	indexChunkOffset[191] = 0x00096064;
	indexChunkOffset[192] = 0x000977cc;
	indexChunkOffset[193] = 0x00098c54;
	indexChunkOffset[194] = 0x0009a830;
	indexChunkOffset[195] = 0x0009c020;
	indexChunkOffset[196] = 0x0009d78c;
	indexChunkOffset[197] = 0x0009ec18;
	indexChunkOffset[198] = 0x000a0260;
	indexChunkOffset[199] = 0x000a1830;
	indexChunkOffset[200] = 0x000a2c90;
	indexChunkOffset[201] = 0x000a40ac;
	indexChunkOffset[202] = 0x000a547c;
	indexChunkOffset[203] = 0x000a6768;
	indexChunkOffset[204] = 0x000a7a80;
	indexChunkOffset[205] = 0x000a8f90;
	indexChunkOffset[206] = 0x000aa304;
	indexChunkOffset[207] = 0x000ab468;
	indexChunkOffset[208] = 0x000ac5bc;
	indexChunkOffset[209] = 0x000ad7a0;
	indexChunkOffset[210] = 0x000ae9b0;
	indexChunkOffset[211] = 0x000afd04;
	indexChunkOffset[212] = 0x000b125c;
	indexChunkOffset[213] = 0x000b2cc4;
	indexChunkOffset[214] = 0x000b40c8;
	indexChunkOffset[215] = 0x000b5a90;
	indexChunkOffset[216] = 0x000b787c;
	indexChunkOffset[217] = 0x000b9b2c;
	indexChunkOffset[218] = 0x000bb2d8;
	indexChunkOffset[219] = 0x000bd4a4;
	indexChunkOffset[220] = 0x000bf144;
	indexChunkOffset[221] = 0x000c1000;
	indexChunkOffset[222] = 0x000c2a54;
	indexChunkOffset[223] = 0x000c48a8;
	indexChunkOffset[224] = 0x000c67dc;
	indexChunkOffset[225] = 0x000c8154;
	indexChunkOffset[226] = 0x000cb150;
	indexChunkOffset[227] = 0x000cb178;
	indexChunkOffset[228] = 0x000ccc38;
	indexChunkOffset[229] = 0x000cf3f8;
	indexChunkOffset[230] = 0x000d196c;
	indexChunkOffset[231] = 0x000d3498;
	indexChunkOffset[232] = 0x000d5784;
	indexChunkOffset[233] = 0x000d7b54;
	indexChunkOffset[234] = 0x000d9784;
	indexChunkOffset[235] = 0x000dbccc;
	indexChunkOffset[236] = 0x000dde0c;
	indexChunkOffset[237] = 0x000df5d4;
	indexChunkOffset[238] = 0x000e169c;
	indexChunkOffset[239] = 0x000e3824;
	indexChunkOffset[240] = 0x000e5768;
	indexChunkOffset[241] = 0x000e8474;
	indexChunkOffset[242] = 0x000e849c;
	indexChunkOffset[243] = 0x000ea8e8;
	indexChunkOffset[244] = 0x000eccf0;
	indexChunkOffset[245] = 0x000eec50;
	indexChunkOffset[246] = 0x000f192c;
	indexChunkOffset[247] = 0x000f4f8c;
	indexChunkOffset[248] = 0x000f7c54;
	indexChunkOffset[249] = 0x000fba90;
	indexChunkOffset[250] = 0x000feea0;
	indexChunkOffset[251] = 0x00102e28;
	indexChunkOffset[252] = 0x001066c8;
	indexChunkOffset[253] = 0x0010b378;
	indexChunkOffset[254] = 0x001103cc;
	indexChunkOffset[255] = 0x00114a9c;
	indexChunkOffset[256] = 0x0011a604;
	indexChunkOffset[257] = 0x0011ee1c;
	indexChunkOffset[258] = 0x00124590;
	indexChunkOffset[259] = 0x00128ab4;
	indexChunkOffset[260] = 0x0012e354;
	indexChunkOffset[261] = 0x00132e54;
	indexChunkOffset[262] = 0x00138780;
	indexChunkOffset[263] = 0x0013e6e4;
	indexChunkOffset[264] = 0x00142e90;
	indexChunkOffset[265] = 0x00148654;
	indexChunkOffset[266] = 0x0014c928;
	indexChunkOffset[267] = 0x001526c8;
	indexChunkOffset[268] = 0x00155dec;
	indexChunkOffset[269] = 0x00159428;
	indexChunkOffset[270] = 0x0015e19c;
	indexChunkOffset[271] = 0x00163a68;
	indexChunkOffset[272] = 0x0016b2c4;
	indexChunkOffset[273] = 0x00171a0c;
	indexChunkOffset[274] = 0x00176f60;
	indexChunkOffset[275] = 0x0017ccdc;
	indexChunkOffset[276] = 0x0018148c;
	indexChunkOffset[277] = 0x00186334;
	indexChunkOffset[278] = 0x0018cda8;
	indexChunkOffset[279] = 0x0018cdd0;
	indexChunkOffset[280] = 0x00193280;
	indexChunkOffset[281] = 0x001932a8;
	indexChunkOffset[282] = 0x00198484;
	indexChunkOffset[283] = 0x001984ac;
	indexChunkOffset[284] = 0x0019dde0;
	indexChunkOffset[285] = 0x0019de08;
	indexChunkOffset[286] = 0x001a1ddc;
	indexChunkOffset[287] = 0x001a1e04;
	indexChunkOffset[288] = 0x001a66bc;
	indexChunkOffset[289] = 0x001a66e4;
	indexChunkOffset[290] = 0x001a9ad0;
	indexChunkOffset[291] = 0x001a9af8;
	indexChunkOffset[292] = 0x001adc08;
	indexChunkOffset[293] = 0x001adc30;
	indexChunkOffset[294] = 0x001b0958;
	indexChunkOffset[295] = 0x001b0980;
	indexChunkOffset[296] = 0x001b3efc;
	indexChunkOffset[297] = 0x001b3f24;
	indexChunkOffset[298] = 0x001b6bb4;
	indexChunkOffset[299] = 0x001b6bdc;

	return 0;

}

RK_S32 getNextVideoChunk(RK_U8 *bitBuf,  RK_U32 *chunkSize, RK_U32 bufSize)
{
	static int index = 0;
	RK_U32	offset;
	RK_S32	size;
	RK_U8	*ptr;
	
	//flush memory
	memset(bitBuf, 0x0, bufSize*sizeof(RK_S8));

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
		*chunkSize = size;
		
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

	fp_mpeg4 = fopen("D:\\TEST\\Src\\RK\\avi\\sec0.vid", "rb");
	
	if (fp_mpeg4 == NULL)
	{
		#ifdef TRACE_ENABLE
		fprintf(stderr, "can't open the input file!\n");
		#endif
		return -1;
	}
	
	fp_yuv = fopen("D:\\TEST\\Src\\RK\\avi\\sec0.vid.yuv", "wb");

	if (fp_yuv == NULL) {
		#ifdef TRACE_ENABLE
		fprintf(stderr, "can't open the output file!\n");
		#endif
		return -1;
	}

	fread(Mpeg4BitsTable, 1, 204800*2, fp_mpeg4);
	
	bitbuf_ptr = (RK_U8*)malloc(MPEG4_BITSTREAM_BUFFER_SIZE);

	if(bitbuf_ptr == NULL)
	{
		#ifdef TRACE_ENABLE
	    fprintf(stderr, "BITSTREAM Buffer Allocation Failed!");
		#endif
	    goto MAIN_EXCEPTIONAL;
	}

	vpu_generic_info.CodecType	= VPU_CODEC_DEC_MPEG4;
	vpu_generic_info.ImgWidth		= 0;
	vpu_generic_info.ImgHeight	= 0;

	/*@jh: decoder init*/
	ret = mpeg4_dec.mpeg4_decode_init(&vpu_generic_info);

	if (ret != MPEG4_INIT_OK)
	{
		#ifdef TRACE_ENABLE
		fprintf(stderr, "decoder init error!\n");
		#endif
		goto MAIN_EXCEPTIONAL;
	}
	
	mpeg4_dec.mpeg4_hwregister_init();

	do
	{
		if (needNewChunk == VPU_OK)
		{
			ret = getNextVideoChunk(bitbuf_ptr, &chunkSize, MPEG4_BITSTREAM_BUFFER_SIZE);
			if (ret != VIDEO_CHUNK_GET_OK)
				break;

			ret = mpeg4_dec.getVideoFrame(bitbuf_ptr, chunkSize);
			if (ret != VPU_OK)
				break;
		}

		status = mpeg4_dec.mpeg4_decode_frame();
		
		output_frame = mpeg4_dec.mpeg4_decoder_endframe();

		mpeg4_dec.mpeg4_dumpyuv(fp_yuv, output_frame);

		/*@jh: estimate another frame in the same chunk*/
		needNewChunk = mpeg4_dec.mpeg4_test_new_slice();
	}
	while(status == MPEG4_DEC_OK);

MAIN_EXCEPTIONAL:
	if(bitbuf_ptr)
	    free(bitbuf_ptr);

	if(fp_mpeg4)
	    fclose(fp_mpeg4);

	if(fp_yuv)
		fclose(fp_yuv);

	#ifdef TRACE_ENABLE
	fprintf(stdout, "RK Decoding Finished!\n");
	#endif

	return 0;
}
