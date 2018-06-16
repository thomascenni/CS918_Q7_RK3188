#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "pv_mpeg4dec_api.h"
#include "vpu.h"
#define LOG_TAG "Mpeg4_api"
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
int Rk_Mpeg4Decoder::pv_rkmpeg4decoder_init(VPU_GENERIC *vpug)
{
    int err;
    mpeg4_dec = NULL;

    if (vpug && (vpug->ImgWidth >=3280) || (vpug->ImgHeight >=2160)) {
        ALOGE("mpeg4 not support resolution:  w(%d) x h(%d)", vpug->ImgWidth, vpug->ImgHeight);
        return -1;
    }

    mpeg4_dec = new Mpeg4_Decoder;
	mpeg4_dec->socket = VPUClientInit(VPU_DEC);
	if (mpeg4_dec->socket >= 0)
        ALOGV("mpeg4: VPUSendMsg VPU_CMD_REGISTER success\n");
    else
    {
        ALOGE("mpeg4: VPUSendMsg VPU_CMD_REGISTER failed\n");
		return -1;
    }
	ALOGV("start VPUMallocLinear \n");
    mpeg4_dec->mpeg4_hwregister_init();
    if(mpeg4_dec->mpeg4_decode_init(vpug))
    {
        return -1;
    }
#ifdef FRAME_COPY
    FrameCount = 0;
    MemAllocFlag = 0;
#endif

	return 0;
}
int Rk_Mpeg4Decoder::pv_rkmpeg4decoder_deinit()
{
    VPU_GENERIC vpug;
    int i;
    if (mpeg4_dec) {
        vpug.CodecType = VPU_CODEC_DEC_MPEG4;
        VPUClientRelease(mpeg4_dec->socket);
        mpeg4_dec->mpeg4_decode_deinit(&vpug);
        delete mpeg4_dec;
        mpeg4_dec = NULL;
    }

#ifdef FRAME_COPY
    if(MemAllocFlag)
    {
        for(i = 0; i< 3;i++)
        {
           VPUFreeLinear(&TESTMem[i]);
        }
        MemAllocFlag = 0;
    }
#endif
	return 0;
}

int Rk_Mpeg4Decoder::pv_rkmpeg4decoder_oneframe(uint8_t* aOutBuffer, uint32_t* aOutputLength,
        uint8_t* aInputBuf, uint32_t* aInBufSize)
{
    if (mpeg4_dec ==NULL) {
        return -1;
    }

	int		i, status, dec_result;
	VPU_FRAME *frame = NULL;
    VPU_FRAME getFrame;
	*aOutputLength = 0;
    RK_S32 needNewChunk = VPU_OK;
    do
    {
        if (needNewChunk == VPU_OK)
        {
            RK_S32 ret;
            ret = mpeg4_dec->getVideoFrame(aInputBuf, *aInBufSize, 0);
            if (ret != VPU_OK)
                break;
        }
		dec_result = mpeg4_dec->mpeg4_decode_frame();

		if(MPEG4_HDR_ERR == dec_result)
		{
            ALOGE("MPEG4 format error, gooooon! \n");
            MPEG4_HEADER* curHdr = mpeg4_dec->getCurrentFrameHeader();
            if (curHdr && (!curHdr->resync_marker_disable)) {
                return -1;
            }
			if (needNewChunk != VPU_OK)
			{
				break;
			}
			else
				return 0;
		}
		if(MPEG4_DEC_ERR == dec_result)
        {
            ALOGE("MPEG4 decoding error \n");
            return -1;

        }
        status = mpeg4_dec->mpeg4_decoder_endframe();
		/*@jh: estimate another frame in the same chunk*/
        needNewChunk = mpeg4_dec->mpeg4_test_new_slice();
		if (needNewChunk != VPU_OK)
		{
			VPU_TRACE("needNewChunk = %d\n", needNewChunk);
		}
    }while(needNewChunk != VPU_OK);
#ifdef FRAME_COPY
        mpeg4_dec->mpeg4_decoder_displayframe(&getFrame);
        frame = (VPU_FRAME*)aOutBuffer;
        if(getFrame.FrameBusAddr[0])
        {
            int i;
            if(!MemAllocFlag)
            {
                for(int ii = 0; ii< 3; ii++)
                {
                    VPUMallocLinear(&TESTMem[ii], getFrame.FrameHeight*getFrame.FrameWidth*3/2);
                }
                MemAllocFlag = 1;
            }
            i = FrameCount++%3;
            {
                VPUMemInvalidate(&getFrame.vpumem);
                memcpy((uint8_t*)TESTMem[i].vir_addr, getFrame.vpumem.vir_addr,getFrame.FrameWidth*getFrame.FrameHeight*3/2);
                VPUMemClean(&TESTMem[i]);
                if(FrameCount < 5)
                {
                    memcpy(aOutBuffer, getFrame.vpumem.vir_addr,getFrame.FrameWidth*getFrame.FrameHeight*3/2);
                }
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
     mpeg4_dec->mpeg4_decoder_displayframe((VPU_FRAME*)aOutBuffer);
     frame = (VPU_FRAME*)aOutBuffer;
     if(frame->FrameBusAddr[0])
	 {
        *aOutputLength = sizeof(VPU_FRAME);
     }
#endif
	 return 0;
}

int Rk_Mpeg4Decoder::pv_rkmpeg4_flushOneFrameInDpb(uint8_t* aOutBuffer, uint32_t *aOutputLength)
{
    if ((aOutBuffer == NULL) || (aOutputLength ==NULL)) {
        ALOGE("input parameter is InValid");
        return -1;
    }

    int ret =0;
    if (mpeg4_dec) {
        if (mpeg4_dec->mpeg4_flush_oneframe_dpb((VPU_FRAME*)aOutBuffer)) {
            *aOutputLength =0;
            return -1;
        } else {
            *aOutputLength =sizeof(VPU_FRAME);
            return 0;
        }
    }

    return -1;
}

int Rk_Mpeg4Decoder::pv_rkmpeg4decoder_rest()
{
    mpeg4_dec->reset_keyframestate();
    mpeg4_dec->mpeg4_hwregister_init();
    return 0;
}
int32_t Rk_Mpeg4Decoder::pv_rkmpeg4_perform_seting(int cmd,void *param){
    int32_t ret = 0;
    switch (cmd) {
        case VPU_API_SET_VPUMEM_CONTEXT:{
            if(mpeg4_dec != NULL){
                mpeg4_dec->vpumem_ctx = param;
            }else{
                ALOGE("mpeg4 set contxt fail");
                ret = -1;
            }
        }break;
		case VPU_API_USE_PRESENT_TIME_ORDER: {
            if (mpeg4_dec) {
                mpeg4_dec->mpeg4_control(cmd, NULL);
            }
        } break;
        default : {
            ALOGI("invalid command %d", cmd);
            ret = -EINVAL;
        } break;
    }
    return ret;
}
extern "C"
void *  get_class_RkM4vDecoder(void)
{
    return (void*)new Rk_Mpeg4Decoder();
}
extern "C"
void  destroy_class_RkM4vDecoder(void * M4vDecoder)
{
    delete (Rk_Mpeg4Decoder *)M4vDecoder;
}
extern "C"
int init_class_RkM4vDecoder(void * M4vDecoder, VPU_GENERIC *vpug)
{
	Rk_Mpeg4Decoder * M4vdec = (Rk_Mpeg4Decoder *)M4vDecoder;
	return M4vdec->pv_rkmpeg4decoder_init(vpug);
}
extern "C"
int deinit_class_RkM4vDecoder(void * M4vDecoder)
{
	Rk_Mpeg4Decoder * M4vdec = (Rk_Mpeg4Decoder *)M4vDecoder;
	return M4vdec->pv_rkmpeg4decoder_deinit();
}
extern "C"
int reset_class_RkM4vDecoder(void * M4vDecoder)
{
	Rk_Mpeg4Decoder * M4vdec = (Rk_Mpeg4Decoder *)M4vDecoder;
	return M4vdec->pv_rkmpeg4decoder_rest();
}
extern "C"
int dec_oneframe_class_RkM4vDecoder(void * M4vDecoder,uint8_t* aOutBuffer, uint32_t *aOutputLength,
        uint8_t* aInputBuf, uint32_t* aInBufSize)
{
	int ret;
	Rk_Mpeg4Decoder * M4vdec = (Rk_Mpeg4Decoder *)M4vDecoder;
	ret = M4vdec->pv_rkmpeg4decoder_oneframe(aOutBuffer,aOutputLength,
					aInputBuf,aInBufSize);
	return ret;
}
extern "C"
int flush_oneframe_in_dpb_class_RkM4vDecoder(void * M4vDecoder,uint8_t* aOutBuffer, uint32_t *aOutputLength)
{
    int ret;
    Rk_Mpeg4Decoder * M4vdec = (Rk_Mpeg4Decoder *)M4vDecoder;
    ret = M4vdec->pv_rkmpeg4_flushOneFrameInDpb(aOutBuffer,aOutputLength);
    return ret;
}

extern "C"
int perform_seting_class_RkM4vDecoder(void * M4vDecoder,VPU_API_CMD cmd,void *param){
    Rk_Mpeg4Decoder * M4vdec = (Rk_Mpeg4Decoder *)M4vDecoder;
    int ret = 0;
    if(M4vdec != NULL){
        ret = M4vdec->pv_rkmpeg4_perform_seting(cmd,param);
    }else{
        ret = -1;
    }
    return 0;
}
