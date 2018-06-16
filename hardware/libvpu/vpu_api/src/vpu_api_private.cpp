#include "vpu_api_private.h"
#include "vpu_api.h"
#include <stdint.h>
#include <memory.h>
#include <unistd.h>
#include <sys/cdefs.h>
#include <stddef.h>

#include "ctype.h"

#define  LOG_TAG "vpu_api_private"
#include "log.h"

//#define _VPU_API_PRIVATE_DEBUG
#ifdef  _VPU_API_PRIVATE_DEBUG
#ifdef AVS40
#define API_PRIVATE_DEBUG           LOGD
#else
#define API_PRIVATE_DEBUG           ALOGD
#endif
#else
#define API_PRIVATE_DEBUG
#endif

#ifdef AVS40
#define API_PRIVATE_ERROR           LOGE
#else
#define API_PRIVATE_ERROR           ALOGE
#endif

static const char* MEDIA_RK_CFG_DEC_MPEG4   = "media.rk.cfg.dec.mpeg4";
static const char* MEDIA_RK_CFG_DEC_AVC     = "media.rk.cfg.dec.h264";
static const char* MEDIA_RK_CFG_DEC_MJPEG   = "media.rk.cfg.dec.mjpeg";
static const char* MEDIA_RK_CFG_DEC_VP8     = "media.rk.cfg.dec.vp8";

static RkCodecConfigure_t rk_codec_cfg[] = {
    {CODEC_DECODER,     OMX_RK_VIDEO_CodingMPEG4,      "M4v",        MEDIA_RK_CFG_DEC_MPEG4, CFG_VALUE_UNKNOW},
    {CODEC_DECODER,     OMX_RK_VIDEO_CodingAVC,        "AVC",          MEDIA_RK_CFG_DEC_AVC, CFG_VALUE_UNKNOW},
    {CODEC_DECODER,     OMX_RK_VIDEO_CodingMJPEG,      "NULL",         MEDIA_RK_CFG_DEC_MJPEG, CFG_VALUE_UNKNOW},
    {CODEC_DECODER,     OMX_RK_VIDEO_CodingVP8,        "VPX",          MEDIA_RK_CFG_DEC_VP8,   CFG_VALUE_UNKNOW},
};

uint8_t VpuApiPrivate::mHaveParseCfg = 0;

VpuApiPrivate::VpuApiPrivate()
{
    mDebugFile = NULL;
    memset(&mCodecCfg, 0, sizeof(VpuApiCodecCfg_t));
    mCoding = OMX_RK_VIDEO_CodingUnused;
    memset(&mTimeTrace, 0, sizeof(VpuApiTimeStampTrace_t));
 }

VpuApiPrivate::~VpuApiPrivate()
{
    if (mDebugFile) {
        fclose(mDebugFile);
        mDebugFile = NULL;
    }
}

int32_t VpuApiPrivate::vpu_api_init(VPU_API *vpu_api, uint32_t video_coding_type)
{
    if (vpu_api == NULL) {
        API_PRIVATE_ERROR("vpu_api_init fail, input parameter invalid\n");
        return -1;
    }

    memset(vpu_api, 0, sizeof(VPU_API));

    switch (video_coding_type) {

    case OMX_RK_VIDEO_CodingAVC:
        vpu_api->get_class_RkDecoder                = get_class_RkAvcDecoder;
        vpu_api->destroy_class_RkDecoder            = destroy_class_RkAvcDecoder;
        vpu_api->init_class_RkDecoder_AVC           = init_class_RkAvcDecoder;
        vpu_api->deinit_class_RkDecoder             = deinit_class_RkAvcDecoder;
        vpu_api->dec_oneframe_class_RkDecoder_WithTimeStamp       = dec_oneframe_class_RkAvcDecoder;
        vpu_api->reset_class_RkDecoder              = reset_class_RkAvcDecoder;
        vpu_api->flush_oneframe_in_dpb_class_RkDecoder = flush_oneframe_in_dpb_class_RkAvcDecoder;
        vpu_api->perform_seting_class_RkDecoder     = perform_seting_class_RkAvcDecoder;

        vpu_api->get_class_RkEncoder                = get_class_RkAvcEncoder;
        vpu_api->destroy_class_RkEncoder            = destroy_class_RkAvcEncoder;
        vpu_api->init_class_RkEncoder               = init_class_RkAvcEncoder;
        vpu_api->deinit_class_RkEncoder             = deinit_class_RkAvcEncoder;
        vpu_api->enc_oneframe_class_RkEncoder       = enc_oneframe_class_RkAvcEncoder;
        vpu_api->enc_setconfig_class_RkEncoder      = set_config_class_RkAvcEncoder;
        vpu_api->enc_getconfig_class_RkEncoder      = get_config_class_RkAvcEncoder;
        vpu_api->enc_setInputFormat_class_RkEncoder = set_inputformat_class_RkAvcEncoder;
        vpu_api->enc_setIdrframe_class_RkEncoder    = set_idrframe_class_RkAvcEncoder;
        break;
#if 0
    case OMX_RK_VIDEO_CodingMPEG4:
        vpu_api->get_class_RkDecoder                = get_class_RkM4vDecoder;
        vpu_api->destroy_class_RkDecoder            = destroy_class_RkM4vDecoder;
        vpu_api->reset_class_RkDecoder              = reset_class_RkM4vDecoder;
        vpu_api->init_class_RkDecoder_M4V           = init_class_RkM4vDecoder;
        vpu_api->deinit_class_RkDecoder             = deinit_class_RkM4vDecoder;
        vpu_api->dec_oneframe_class_RkDecoder       = dec_oneframe_class_RkM4vDecoder;
        vpu_api->flush_oneframe_in_dpb_class_RkDecoder = flush_oneframe_in_dpb_class_RkM4vDecoder;
        vpu_api->perform_seting_class_RkDecoder     = perform_seting_class_RkM4vDecoder;
        break;
    case OMX_RK_VIDEO_CodingVP8:
        vpu_api->get_class_RkDecoder                = get_class_RkVp8Decoder;
        vpu_api->destroy_class_RkDecoder            = destroy_class_RkVp8Decoder;
        vpu_api->reset_class_RkDecoder              = reset_class_RkVp8Decoder;
        vpu_api->init_class_RkDecoder               = init_class_RkVp8Decoder;
        vpu_api->deinit_class_RkDecoder             = deinit_class_RkVp8Decoder;
        vpu_api->dec_oneframe_class_RkDecoder       = dec_oneframe_class_RkVp8Decoder;
        vpu_api->perform_seting_class_RkDecoder     = perform_seting_class_RkVp8Decoder;

        vpu_api->get_class_RkEncoder                = get_class_RkVp8Encoder;
        vpu_api->destroy_class_RkEncoder            = destroy_class_RkVp8Encoder;
        vpu_api->init_class_RkEncoder               = init_class_RkVp8Encoder;
        vpu_api->deinit_class_RkEncoder             = deinit_class_RkVp8Encoder;
        vpu_api->enc_oneframe_class_RkEncoder       = enc_oneframe_class_RkVp8Encoder;
        break;
     case OMX_RK_VIDEO_CodingMJPEG:
        vpu_api->get_class_RkDecoder                = get_class_RkJpegDecoder;
        vpu_api->destroy_class_RkDecoder            = destroy_class_RkJpegDecoder;
        vpu_api->init_class_RkDecoder               = init_class_RkJpegDecoder;
        vpu_api->deinit_class_RkDecoder             = deinit_class_RkJpegDecoder;
        vpu_api->dec_oneframe_class_RkDecoder       = dec_oneframe_class_RkJpegDecoder;
        vpu_api->reset_class_RkDecoder              = reset_class_RkJpegDecoder;
        vpu_api->perform_seting_class_RkDecoder     = perform_seting_class_RkJpegDecoder;
        vpu_api->get_class_RkEncoder                = get_class_RkMjpegEncoder;
    	vpu_api->destroy_class_RkEncoder            = destroy_class_RkMjpegEncoder;
    	vpu_api->init_class_RkEncoder               = init_class_RkMjpegEncoder;
    	vpu_api->deinit_class_RkEncoder             = deinit_class_RkMjpegEncoder;
    	vpu_api->enc_oneframe_class_RkEncoder       = enc_oneframe_class_RkMjpegEncoder;
		break;
#endif
    default:
        API_PRIVATE_ERROR("vpu_api_init fail, unsupport coding type\n");
        return -1;
    }

    return 0;
}

int32_t VpuApiPrivate::video_fill_hdr(uint8_t *dst,uint8_t *src, uint32_t size,
            int64_t time, uint32_t type, uint32_t num)
{
    if ((NULL ==dst) || (NULL ==src)) {
        API_PRIVATE_ERROR("video_fill_hdr fail, input parameter invalid\n");
        return -1;
    }

    VPU_BITSTREAM h;
    uint32_t TimeLow = 0;

    TimeLow = (uint32_t)(time/1000);
    h.StartCode = BSWAP(0x42564b52);
    h.SliceLength= BSWAP(size);
    h.SliceTime.TimeLow = BSWAP(TimeLow);
    h.SliceTime.TimeHigh= 0;
    h.SliceType= BSWAP(type);
    h.SliceNum= BSWAP(num);
    h.Res[0] = 0;
    h.Res[1] = 0;
    memcpy(dst, &h, sizeof(VPU_BITSTREAM));
    memcpy((uint8_t*)(dst + sizeof(VPU_BITSTREAM)),src, size);
    return 0;
}

int32_t VpuApiPrivate::get_line(VpuApiCodecCfg_t *s, uint8_t *buf, uint32_t maxlen)
{
    if ((s == NULL) || (buf == NULL)) {
        return 0;
    }

    int32_t i = 0;
    uint8_t c;

    do {
        c = *(s->buf + s->cfg_file.read_pos++);
        if (c && i < maxlen-1)
            buf[i++] = c;
    } while (c != '\n' && c);

    buf[i] = 0;
    return i;
}

int32_t VpuApiPrivate::read_chomp_line(VpuApiCodecCfg_t* s, uint8_t *buf, uint32_t maxlen)
{
    int len = get_line(s, buf, maxlen);
    while (len > 0 && isspace(buf[len - 1]))
        buf[--len] = '\0';
    return len;
}

int32_t VpuApiPrivate::parseStagefrightCfgFile(VpuApiCodecCfg_t* cfg)
{
    if (cfg == NULL) {
        API_PRIVATE_ERROR("parseStagefrightCfgFile fail, input parameter invalid\n");
        return -1;
    }

    uint8_t line[512], tmp[100];
    uint32_t size =0, ret =0, n =0, k =0;
    uint32_t tag_read =0;
    VpuApiCodecCfg_t* p = cfg;
    FILE* file = p->cfg_file.file;
    RkCodecConfigure_t* rk_cfg = NULL;
    char *c = NULL, *s = NULL;
    char quote = '"';

    fseek(file, 0, SEEK_END);
    p->cfg_file.file_size = size = ftell(file);
    fseek(file, 0, SEEK_SET);
    API_PRIVATE_DEBUG("stagefright cfg file size: %d",
        p->cfg_file.file_size);
    if (size >=1024*1024) {
        API_PRIVATE_ERROR("parseStagefrightCfgFile fail, config file "
                "size: %d too large, max support 1M\n", size);
        return -1;
    }
    if (p->buf) {
        free(p->buf);
        p->buf = NULL;
    }
    p->buf = (uint8_t*)malloc(size);
    if (p->buf == NULL) {
        API_PRIVATE_ERROR("alloc memory fail while parse cfg file\n");
        ret = -1;
        goto PARSE_CFG_OUT;
    }
    memset(p->buf, 0, size);

    if ((n = fread(p->buf, 1, size, file)) !=size) {
        API_PRIVATE_ERROR("read bytes from parse cfg file fail\n");
        ret = -1;
        goto PARSE_CFG_OUT;
    }

    size =0;
    while (size <p->cfg_file.file_size) {
        if ((n = read_chomp_line(p, line, sizeof(line))) <0) {
            break;
        }

        API_PRIVATE_DEBUG("read one line, size: %d, : %s\n", n, line);
        if (n ==0) {
            if (p->cfg_file.read_pos >=p->cfg_file.file_size) {
                break;
            }

            continue;
        }
        if (!tag_read) {
            if (strstr((char*)line, "rockchip_stagefright_config")) {
                continue;
            }
            if (!strstr((char*)line, "<Decoders>")) {
                API_PRIVATE_ERROR("parseStagefrightCfgFile fail, invalid tag\n");
                ret = -1;
                break;
            }

            tag_read = 1;
            continue;
        }
        if (!(s =strstr((char*)line, "media.rk.cfg.dec."))) {
            if (tag_read && (strstr((char*)line, "</Decoders>") ||
                                strstr((char*)line, "</rockchip_stagefright_config>"))) {
                break;
            }
            API_PRIVATE_ERROR("parseStagefrightCfgFile fail, invalid line: %s\n", line);
            ret = -1;
            break;
        } else  {
            for (k =0; k <=strlen(s); k++) {
                if (s[k] ==quote) {
                    break;
                }
            }
            if (k <sizeof(tmp)) {
                memset(tmp, 0, sizeof(tmp));
                strncpy((char*)tmp, s, k);
            }

            if (rk_cfg = getStagefrightCfgByName(CODEC_DECODER, tmp)) {
                if (strstr((char*)line, "support=\"yes\"")) {
                    rk_cfg->value = CFG_VALUE_SUPPORT;
                } else {
                    rk_cfg->value = CFG_VALUE_NOT_SUPPORT;
                }
            }
        }

        size +=n;
    }

PARSE_CFG_OUT:
    if (p->buf) {
        free(p->buf);
        p->buf = NULL;
    }

    return ret;
}

RkCodecConfigure_t* VpuApiPrivate::getStagefrightCfgByCoding(
            CODEC_TYPE codec_type,
            uint32_t video_coding_type)
{
    uint32_t i =0, cfgCnt = 0;
    uint32_t coding = video_coding_type;

    cfgCnt = sizeof(rk_codec_cfg) / sizeof(RkCodecConfigure_t);
    for (; i <=(cfgCnt); i++) {
        if ((coding == rk_codec_cfg[i].videoCoding) &&
                (codec_type == rk_codec_cfg[i].codecType)) {
            return &rk_codec_cfg[i];
        }
    }

    return NULL;
}

RkCodecConfigure_t* VpuApiPrivate::getStagefrightCfgByName(
        CODEC_TYPE codec_type, uint8_t* name)
{
    uint32_t i =0, cfgCnt = 0;
    cfgCnt = sizeof(rk_codec_cfg) / sizeof(RkCodecConfigure_t);
    for (; i <=(cfgCnt); i++) {
        if ((!strncasecmp(rk_codec_cfg[i].cfg,
                (char*)name, strlen(rk_codec_cfg[i].cfg))) &&
                (codec_type == rk_codec_cfg[i].codecType)) {
            return &rk_codec_cfg[i];
        }
    }

    return NULL;
}

int32_t VpuApiPrivate::isSupportUnderCfg(uint32_t video_coding_type)
{
    int32_t support = 1;

    API_PRIVATE_DEBUG("isSupportUnderCfg out, support: %d", support);
    return support;
}

int32_t VpuApiPrivate::read_user_config()
{
    VpuApiCodecCfg_t* codecCfg = &mCodecCfg;
    if (mHaveParseCfg ==1) {
        return 0;
    }

    FILE *file = fopen("/system/etc/media_stagefright.cfg", "r");
    if (file == NULL) {
        API_PRIVATE_DEBUG("unable to open media codecs configuration file.");
        return -1;
    }

    int32_t ret =0;
    codecCfg->cfg_file.file = file;
    if ((ret = parseStagefrightCfgFile(codecCfg)) <0) {
        API_PRIVATE_ERROR("parse media_stagefright.cfg fail");
    }

    if (file) {
        fclose(file);
        file = NULL;
    }

    mHaveParseCfg = 1;
    return ret;
}

int32_t VpuApiPrivate::openDebugFile(const char *path)
{
    if (mDebugFile) {
        fclose(mDebugFile);
        mDebugFile = NULL;
    }

    mDebugFile = fopen(path, "wb");
    return mDebugFile == NULL ? -1 : 0;
}

int32_t VpuApiPrivate::writeToDebugFile(uint8_t* data, uint32_t size)
{
    if ((data == NULL) || (size ==0)) {
        return -1;
    }

    if (mDebugFile) {
        fwrite(data, 1, size, mDebugFile);
        fflush(mDebugFile);
    }

    return 0;
}

void VpuApiPrivate::setVideoCoding(uint32_t coding)
{
    mCoding = coding;
}

int32_t VpuApiPrivate::postProcessTimeStamp(int64_t *inTimeUs, int64_t *outTimeUs)
{
    if ((inTimeUs ==NULL) || (outTimeUs ==NULL)) {
        return -1;
    }

    VpuApiTimeStampTrace_t* p = &mTimeTrace;
    if (!p->init) {
        p->pre_timeUs = *inTimeUs;
        *outTimeUs = *inTimeUs;
        p->same_cnt =0;
        p->init = 1;
        return 0;
    }

    if (p->pre_timeUs != *inTimeUs) {
        p->pre_timeUs = *inTimeUs;
        p->same_cnt =0;
        *outTimeUs = *inTimeUs;
        return 0;
    } else {
        p->same_cnt++;
    }

    uint32_t factor = 1;
    switch(mCoding){
        case OMX_RK_VIDEO_CodingAVC:
            factor = 66;
            break;
        default:
            break;
    }

    *outTimeUs = (p->same_cnt*factor*1000) + p->pre_timeUs;

    return 0;
}

void VpuApiPrivate::resetIfNecessary()
{
    VpuApiTimeStampTrace_t* p = &mTimeTrace;
    p->init = 0;
}


/* for rkffplayer */
extern "C"
void vpu_api_init(VPU_API *vpu_api, OMX_RK_VIDEO_CODINGTYPE video_coding_type)
{
    int32_t ret = VpuApiPrivate::vpu_api_init(vpu_api, video_coding_type);
}


