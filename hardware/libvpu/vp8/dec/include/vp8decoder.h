#ifndef _VP8DECODER_H_
#define _VP8DECODER_H_

#include "vpu_global.h"
#include "framemanager.h"
#include "reg.h"
#include "postprocess.h"

#define VP8HWD_VP7             1
#define VP8HWD_VP8             2
#define VP8HWD_WEBP		3

#define DEC_MODE_VP7  9
#define DEC_MODE_VP8 10

#define	VP8_Dec_OK			0
#define	VP8_Dec_Unsurport	1

#define MAX_NBR_OF_SEGMENTS             (4)
#define MAX_NBR_OF_MB_REF_LF_DELTAS     (4)
#define MAX_NBR_OF_MB_MODE_LF_DELTAS    (4)

#define MAX_NBR_OF_DCT_PARTITIONS       (8)

#define MAX_NBR_OF_VP7_MB_FEATURES      (4)


#define VP7_MV_PROBS_PER_COMPONENT      (17)
#define VP8_MV_PROBS_PER_COMPONENT      (19)

#define PROB_TABLE_SIZE  (1<<16) /* TODO */

typedef enum
{
    VP8_YCbCr_BT601,
    VP8_CUSTOM
} vpColorSpace_e;

#define DEC_8190_ALIGN_MASK         0x07U

typedef struct
{
    RK_U32 lowvalue;
    RK_U32 range;
    RK_U32 value;
    RK_S32 count;
    RK_U32 pos;
    RK_U8 *buffer;
    RK_U32 BitCounter;
    RK_U32 streamEndPos;
    RK_U32 strmError;
} vpBoolCoder_t;

typedef struct
{
    RK_U8              probLuma16x16PredMode[4];
    RK_U8              probChromaPredMode[3];
    RK_U8              probMvContext[2][VP8_MV_PROBS_PER_COMPONENT];
    RK_U8              probCoeffs[4][8][3][11];
} vp8EntropyProbs_t;


typedef struct
{
	RK_U8		*stream;
	RK_U32		len;
	RK_U32		time;
}input_stream;

class vp8decoder
{
	public:
		vp8decoder();
		~vp8decoder();
		int	decoder_int();
		int	decoder_frame(RK_U8 *pbuff, RK_U32 size, VPU_POSTPROCESSING *ppcfg);
		void	get_displayframe(VPU_FRAME *frame);
        void  reset_keyframestate();
        int socket;
        void *vpumem_ctx;
	private:
		void vp8hwdBoolStart(RK_U8 *buffer, RK_U32 len);
		RK_U32 vp8hwdDecodeBool(RK_S32 probability);
		RK_U32 vp8hwdDecodeBool128();
		RK_U32 vp8hwdReadBits(RK_S32 bits);
		RK_U32 ScaleDimension( RK_U32 orig, RK_U32 scale );
		RK_S32 DecodeQuantizerDelta();
		void vp8hwdResetProbs();
		void vp8hwdDecodeCoeffUpdate();
		int	decoder_frame_header(RK_U8 *pbase, RK_U32 size);
		int	vp8hwdSetPartitionOffsets(RK_U8 *stream, RK_U32 len);
		void VP8HwdAsicProbUpdate(void);
		int	decinit_hwcfg();
		int	frame_hwcfg();
	private:
		RK_U32					regBase[100];
		VPUMemLinear_t			bitstream;
		VPUMemLinear_t			probTbl;
		VPUMemLinear_t			segmentMap;

		vpBoolCoder_t		bitstr;



		RK_U32             decMode;

		/* Current frame dimensions */
		RK_U32             width;
		RK_U32             height;
		RK_U32             scaledWidth;
		RK_U32             scaledHeight;

		RK_U32             vpVersion;
		RK_U32             vpProfile;

		RK_U32             keyFrame;

		RK_U32             coeffSkipMode;

		/* DCT coefficient partitions */
		RK_U32             offsetToDctParts;
		RK_U32             nbrDctPartitions;
		RK_U32             dctPartitionOffsets[MAX_NBR_OF_DCT_PARTITIONS];

		vpColorSpace_e  colorSpace;
		RK_U32             clamping;
		RK_U32             showFrame;


		RK_U32             refreshGolden;
		RK_U32             refreshAlternate;
		RK_U32             refreshLast;
		RK_U32             refreshEntropyProbs;
		RK_U32             copyBufferToGolden;
		RK_U32             copyBufferToAlternate;

		RK_U32             refFrameSignBias[2];
		RK_U32             useAsReference;
		RK_U32             loopFilterType;
		RK_U32             loopFilterLevel;
		RK_U32             loopFilterSharpness;

		/* Quantization parameters */
		RK_S32             qpYAc, qpYDc, qpY2Ac, qpY2Dc, qpChAc, qpChDc;

		/* From here down, frame-to-frame persisting stuff */

		RK_U32             vp7ScanOrder[16];
		RK_U32             vp7PrevScanOrder[16];

		/* Probabilities */
		RK_U32             probIntra;
		RK_U32             probRefLast;
		RK_U32             probRefGolden;
		RK_U32             probMbSkipFalse;
		RK_U32             probSegment[3];
		vp8EntropyProbs_t entropy, entropyLast;

		/* Segment and macroblock specific values */
		RK_U32             segmentationEnabled;
		RK_U32             segmentationMapUpdate;
		RK_U32             segmentFeatureMode; /* delta/abs */
		RK_S32             segmentQp[MAX_NBR_OF_SEGMENTS];
		RK_S32             segmentLoopfilter[MAX_NBR_OF_SEGMENTS];
		RK_U32             modeRefLfEnabled;
		RK_S32             mbRefLfDelta[MAX_NBR_OF_MB_REF_LF_DELTAS];
		RK_S32             mbModeLfDelta[MAX_NBR_OF_MB_MODE_LF_DELTAS];

		RK_U32             frameTagSize;

		/* Value to remember last frames prediction for hits into most
		* probable reference frame */
		RK_U32             refbuPredHits;


		RK_S32			dcPred[2];
		RK_S32			dcMatch[2];



		RK_U32			Init_memed;
		RK_U32			mb_width;
		RK_U32			mb_height;

		RK_U32			frame_cnt;
		VPU_FRAME		*frame_out;
		VPU_FRAME		*frame_preout;
		VPU_FRAME		*frame_ref;
		VPU_FRAME		*frame_golden;
		VPU_FRAME		*frame_alternate;

		RK_U32			PostProcess_enable;
		RK_U32			out_width;
		RK_U32			out_height;
		RK_U32			out_color_type;
		RK_U32			out_rotate;

		VPU_FRAME		*pp_outframe;
		postprocess		*vpu_pp;
		rkregister		*regproc;
		framemanager		*frmanager;
};
#endif
