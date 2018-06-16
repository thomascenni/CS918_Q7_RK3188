#ifndef _PV_VP8ENC_API_H_
#define _PV_VP8ENC_API_H_


#include "vpu_mem.h"
#include "vp8encapi.h"

typedef struct
{
   int width;
   int height;
   int rc_mode;
   int bitRate;
   int framerate;
   int	qp;
   int	reserved[10];
}EncParams1;


class Rk_Vp8Encoder
{
    public:
         Rk_Vp8Encoder();
        ~Rk_Vp8Encoder() { };
        int pv_rkvp8encoder_init(EncParams1 *aEncOption, uint8_t* aOutBuffer, uint32_t* aOutputLength);
        int pv_rkvp8encoder_oneframe(uint8_t* aOutBuffer, uint32_t*   aOutputLength,
        uint8_t *aInBuffer, uint32_t aInBuffPhy,uint32_t*   aInBufSize,uint32_t* aOutTimeStamp, bool*  aSyncFlag );
		void pv_rkvp8encoder_getconfig(EncParams1 *vpug);
		void pv_rkvp8encoder_setconfig(EncParams1* vpug);
		int VP8encSetInputFormat(VP8EncPictureType inputFormat);
		void VP8encSetintraPeriodCnt();
        int pv_rkvp8encoder_deinit();
    private:
		int AllocRes(testbench_s * cml, VP8EncInst enc);
        void FreeRes(testbench_s * cml, VP8EncInst enc);
    private:

		testbench_s cmdl;
		VP8EncInst encoder;
		VP8EncIn encIn;
		int intraPeriodCnt;
		int codedFrameCnt;
		int	src_img_size;
		int next;
};
#endif

