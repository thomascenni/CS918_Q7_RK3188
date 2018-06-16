#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "pv_vp8dec_api.h"
#include "vpu.h"
#define LOG_TAG "vp8dec_api"
#include <utils/Log.h>
#undef ALOGV
#define ALOGV ALOGV

int Rk_Vp8Decoder::pv_rkvp8decoder_init()
{
	int i,err;
	vp8_dec = new vp8decoder;
	if(vp8_dec->decoder_int())
	{
        return -1;
    }
#ifdef FRAME_COPY
    FrameCount = 0;
    MemAllocFlag = 0;
#endif

	return 0;
}

int Rk_Vp8Decoder::pv_rkvp8decoder_deinit()
{
    VPUClientRelease(vp8_dec->socket);
    int i;
#ifdef FRAME_COPY
    if(MemAllocFlag)
    {
        for(i = 0; i< OUT_TEM_NUM;i++)
        {
           VPUFreeLinear(&TESTMem[i]);
        }
        MemAllocFlag = 0;
    }
#endif
    delete vp8_dec;
	return 0;
}

int Rk_Vp8Decoder::pv_rkvp8decoder_oneframe(uint8_t* aOutBuffer, uint32_t* aOutputLength,uint8_t* aInputBuf, uint32_t* aInBufSize)
{
    int		i;
	unsigned char		*point;
	VPU_FRAME *frame = NULL;
    VPU_FRAME getFrame;
	*aOutputLength = 0;
    if(vp8_dec->decoder_frame(aInputBuf,*aInBufSize, 0))
    {
        return -1;
    }
#ifdef FRAME_COPY
    vp8_dec->get_displayframe(&getFrame);
    frame = (VPU_FRAME*)aOutBuffer;
    if(getFrame.FrameBusAddr[0])
    {
        int i;
        if(!MemAllocFlag)
        {
            for(int ii = 0; ii< OUT_TEM_NUM; ii++)
            {
                VPUMallocLinear(&TESTMem[ii], getFrame.FrameHeight*getFrame.FrameWidth*3/2);
            }
            MemAllocFlag = 1;
        }
        i = FrameCount++%OUT_TEM_NUM;
        {
            VPUMemInvalidate(&getFrame.vpumem);
            memcpy((uint8_t*)TESTMem[i].vir_addr, getFrame.vpumem.vir_addr,getFrame.FrameWidth*getFrame.FrameHeight*3/2);
            if(FrameCount < 5)
            {
                 memcpy(aOutBuffer, getFrame.vpumem.vir_addr,getFrame.FrameWidth*getFrame.FrameHeight*3/2);
            }
            VPUMemClean(&TESTMem[i]);
            frame->FrameBusAddr[0] = TESTMem[i].phy_addr;
            frame->FrameBusAddr[1] = (uint32_t)(TESTMem[i].phy_addr +((getFrame.FrameWidth+ 15) & 0xfffffff0) * ((getFrame.FrameHeight+ 15) & 0xfffffff0));
        }

        frame->DisplayWidth=  getFrame.FrameWidth;
        frame->DisplayHeight= getFrame.FrameHeight;
        frame->ShowTime.TimeLow = getFrame.ShowTime.TimeLow;
        frame->ShowTime.TimeHigh = getFrame.ShowTime.TimeHigh;
        frame->Res[0] = getFrame.Res[0];
        frame->vpumem.phy_addr = 0;
        frame->vpumem.vir_addr =  0;
        *aOutputLength = sizeof(VPU_FRAME);
        VPUMemLink(&getFrame.vpumem);
        VPUFreeLinear(&getFrame.vpumem);
    }
#else
	vp8_dec->get_displayframe((VPU_FRAME*)aOutBuffer);
	frame = (VPU_FRAME*)aOutBuffer;
	if(frame->FrameBusAddr[0])
	{
	    *aOutputLength = sizeof(VPU_FRAME);
	}
#endif
	return 0;
}
int Rk_Vp8Decoder::pv_rkvp8decoder_reset()
{
    vp8_dec->reset_keyframestate();
    return 0;
}
int32_t Rk_Vp8Decoder::pv_rkvp8_perform_seting(int cmd,void *param){
    int32_t ret = 0;
    switch (cmd) {
        case VPU_API_SET_VPUMEM_CONTEXT:{
            if(vp8_dec != NULL){
                vp8_dec->vpumem_ctx = param;
            }else{
                ALOGE("vp8 set contxt fail");
                ret = -1;
            }
        }break;

        default : {
            ret = -EINVAL;
        } break;
    }
    return ret;
}
extern "C"
void *  get_class_RkVp8Decoder(void)
{
    return (void*)new Rk_Vp8Decoder();
}
extern "C"
void  destroy_class_RkVp8Decoder(void * Vp8Decoder)
{
    delete (Rk_Vp8Decoder *)Vp8Decoder;
}
extern "C"
int init_class_RkVp8Decoder(void * Vp8Decoder)
{
	Rk_Vp8Decoder * Vp8dec = (Rk_Vp8Decoder *)Vp8Decoder;
	return Vp8dec->pv_rkvp8decoder_init();
}
extern "C"
int deinit_class_RkVp8Decoder(void * Vp8Decoder)
{
	Rk_Vp8Decoder * Vp8dec = (Rk_Vp8Decoder *)Vp8Decoder;
	return Vp8dec->pv_rkvp8decoder_deinit();
}

extern "C"
int dec_oneframe_class_RkVp8Decoder(void * Vp8Decoder,uint8_t* aOutBuffer, uint32_t *aOutputLength,
        uint8_t* aInputBuf, uint32_t* aInBufSize)
{
	int ret;
	Rk_Vp8Decoder * Vp8dec = (Rk_Vp8Decoder *)Vp8Decoder;
	ret = Vp8dec->pv_rkvp8decoder_oneframe(aOutBuffer, aOutputLength,
					aInputBuf,aInBufSize);
	return ret;
}
extern "C"
int reset_class_RkVp8Decoder(void * Vp8Decoder)
{
	Rk_Vp8Decoder * Vp8dec = (Rk_Vp8Decoder *)Vp8Decoder;
	return Vp8dec->pv_rkvp8decoder_reset();
}
extern "C"
int perform_seting_class_RkVp8Decoder(void * Vp8Decoder,VPU_API_CMD cmd,void *param){
	Rk_Vp8Decoder * Vp8dec = (Rk_Vp8Decoder *)Vp8Decoder;
    int ret = 0;
    if(param != NULL){
        ret = Vp8dec->pv_rkvp8_perform_seting(cmd,param);
    }else{
        ret = -1;
    }
    return 0;
}
