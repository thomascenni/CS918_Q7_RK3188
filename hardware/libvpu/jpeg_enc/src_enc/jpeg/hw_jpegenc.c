#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <linux/fb.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>

#include "jpegencapi.h"
#include "ewl.h"
#include "hw_jpegenc.h"
#include <utils/Log.h>
#include "EncJpegInstance.h"

#include <poll.h>
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

#ifndef AVS40
#undef LOGD
#define LOGD ALOGD
#undef LOGE
#define LOGE ALOGE
#endif

#ifdef USE_PIXMAN
#include "pixman-private.h"
#else
//#include "swscale.h"//if need crop, may be change to use pixman lib
#endif

//#define HW_JPEG_DEBUG

#define LOG_TAG "hw_jpeg_encode"

#ifdef HW_JPEG_DEBUG
#define WHENCLOG LOGE
#else
#define WHENCLOG(...)
#endif

#define SECOND_USE_SOFT

//#define USE_IPP//must use ipp, encoder does not support scale
//#define DO_APP1_THUMB

#define BACKUP_LEN 20//FFD8 + FFE0 0010 4A46 4946 00 0102 00 0064 0064 00 00

#define SETTWOBYTE(a,b,value) a = (unsigned short*)b;\
							*a = value;\
							b += 2;

#define SETFOURBYTE(a,b,value) a = (uint32_t*)b;\
							*a = value;\
							b += 4;

#define SETFOURBYTESTRING(b,str,len,increment) increment = 4;\
							while(increment-- > 0){\
								if(len > 0){\
									*b++ = *str++;\
									len--;\
								}else{\
									*b++ = '\0';\
								}\
							 }

#define BEFORE_EXIF_LEN 12
int customInsertApp1Header(unsigned char * startpos, JpegEncInInfo *inInfo, uint32_t**jpeglenPos){
	RkExifInfo *exifinfo = inInfo->exifInfo;//can't be null
	RkGPSInfo *gpsinfo = inInfo->gpsInfo;
	unsigned short* shortpos = NULL;
	uint32_t *intpos = NULL;
	int stringlen = 0;
	int increment = 0;
	unsigned char* initpos = NULL;
	int offset = 0;
	*startpos++ = 0xff;//SOI
	*startpos++ = 0xd8;

	*startpos++ = 0xff;//app1
	*startpos++ = 0xe1;

	//length
	*startpos++ = 0x00;
	*startpos++ = 0x00;

	*startpos++ = 0x45;//EXIF
	*startpos++ = 0x78;
	*startpos++ = 0x69;
	*startpos++ = 0x66;

	*startpos++ = 0x00;
	*startpos++ = 0x00;

	initpos = startpos;
	SETTWOBYTE(shortpos,startpos,0x4949)//intel align
	SETTWOBYTE(shortpos,startpos,0x2a)
	SETFOURBYTE(intpos,startpos,0x08)//offset to ifd0
	if(gpsinfo != NULL){
		SETTWOBYTE(shortpos,startpos,0x0a)//num of directory entry
		offset = 10 + 0x0a*12 + 4;
	}else{
		SETTWOBYTE(shortpos,startpos,0x09)//num of directory entry
		offset = 10 + 0x09*12 + 4;
	}
//1
	SETTWOBYTE(shortpos,startpos,0x010e)//imagedescription
	SETTWOBYTE(shortpos,startpos,0x02)
	SETFOURBYTE(intpos,startpos,04)
	SETFOURBYTE(intpos,startpos,0x726b2020)
//2
	SETTWOBYTE(shortpos,startpos,0x010f)//Make
	SETTWOBYTE(shortpos,startpos,0x02)
	stringlen = exifinfo->makerchars;
	SETFOURBYTE(intpos,startpos,stringlen)
	if(stringlen <= 4){
		SETFOURBYTESTRING(startpos,exifinfo->maker,stringlen, increment)
	}else{
		SETFOURBYTE(intpos,startpos,offset)
		memcpy(initpos + offset,exifinfo->maker,stringlen);
		offset += stringlen;
	}
//3
	SETTWOBYTE(shortpos,startpos,0x0110)//Model
	SETTWOBYTE(shortpos,startpos,0x02)
	stringlen = exifinfo->modelchars;
	SETFOURBYTE(intpos,startpos,stringlen)
	if(stringlen <= 4){
		SETFOURBYTESTRING(startpos,exifinfo->maker,stringlen, increment)
	}else{
		SETFOURBYTE(intpos,startpos,offset)
		memcpy(initpos + offset,exifinfo->modelstr,stringlen);
		offset += stringlen;
	}
//4
	SETTWOBYTE(shortpos,startpos,0x0112)//Orientation
	SETTWOBYTE(shortpos,startpos,0x03)
	SETFOURBYTE(intpos,startpos,0x01)
	SETFOURBYTE(intpos,startpos,exifinfo->Orientation)
//5
	SETTWOBYTE(shortpos,startpos,0x011a)//XResolution
	SETTWOBYTE(shortpos,startpos,0x05)
	SETFOURBYTE(intpos,startpos,0x01)
	SETFOURBYTE(intpos,startpos,offset)
	intpos = (uint32_t*)(initpos + offset);
	*intpos++ = 0x48;//72inch
	*intpos = 0x01;
	offset += 8;
//6
	SETTWOBYTE(shortpos,startpos,0x011b)//YResolution
	SETTWOBYTE(shortpos,startpos,0x05)
	SETFOURBYTE(intpos,startpos,0x01)
	SETFOURBYTE(intpos,startpos,offset)
	intpos = (uint32_t*)(initpos + offset);
	*intpos++ = 0x48;//72inch
	*intpos = 0x01;
	offset += 8;
//7
	SETTWOBYTE(shortpos,startpos,0x0128)//ResolutionUnit
	SETTWOBYTE(shortpos,startpos,0x03)
	SETFOURBYTE(intpos,startpos,0x01)
	SETFOURBYTE(intpos,startpos,0x02)//inch
//8
	SETTWOBYTE(shortpos,startpos,0x0132)//DateTime
	SETTWOBYTE(shortpos,startpos,0x02)
	SETFOURBYTE(intpos,startpos,0x14)
	SETFOURBYTE(intpos,startpos,offset)
	memcpy(initpos + offset, exifinfo->DateTime, 0x14);
	offset += 0x14;
//9
	SETTWOBYTE(shortpos,startpos,0x8769)//exif sub ifd
	SETTWOBYTE(shortpos,startpos,0x04)
	SETFOURBYTE(intpos,startpos,0x01)
	SETFOURBYTE(intpos,startpos,offset)
	{
		unsigned char* subifdPos = initpos+offset;
		SETTWOBYTE(shortpos,subifdPos,0x1c)//num of ifd
		offset += 2 + 0x1c*12 + 4;
		SETTWOBYTE(shortpos,subifdPos,0x829a)//ExposureTime
		SETTWOBYTE(shortpos,subifdPos,0x05)
		SETFOURBYTE(intpos,subifdPos,0x01)
		SETFOURBYTE(intpos,subifdPos,offset)
		intpos = (uint32_t*)(initpos + offset);
		*intpos++ = exifinfo->ExposureTime.num;
		*intpos = exifinfo->ExposureTime.denom;
		offset += 8;

		SETTWOBYTE(shortpos,subifdPos,0x829d)//FNumber
		SETTWOBYTE(shortpos,subifdPos,0x05)
		SETFOURBYTE(intpos,subifdPos,0x01)
		SETFOURBYTE(intpos,subifdPos,offset)
		intpos = (uint32_t*)(initpos + offset);
		*intpos++ = exifinfo->ApertureFNumber.num;
		*intpos = exifinfo->ApertureFNumber.denom;
		offset += 8;

		SETTWOBYTE(shortpos,subifdPos,0x8827)//ISO Speed Ratings
		SETTWOBYTE(shortpos,subifdPos,0x03)
		SETFOURBYTE(intpos,subifdPos,0x01)
		SETFOURBYTE(intpos,subifdPos,exifinfo->ISOSpeedRatings)

		SETTWOBYTE(shortpos,subifdPos,0x9000)//ExifVersion
		SETTWOBYTE(shortpos,subifdPos,0x07)
		SETFOURBYTE(intpos,subifdPos,0x04)
		SETFOURBYTE(intpos,subifdPos,0x31323230)

		SETTWOBYTE(shortpos,subifdPos,0x9003)//DateTimeOriginal
		SETTWOBYTE(shortpos,subifdPos,0x02)
		SETFOURBYTE(intpos,subifdPos,0x14)
		SETFOURBYTE(intpos,subifdPos,offset)
		memcpy(initpos + offset, exifinfo->DateTime, 0x14);
		offset += 0x14;

		SETTWOBYTE(shortpos,subifdPos,0x9004)//DateTimeDigitized
		SETTWOBYTE(shortpos,subifdPos,0x02)
		SETFOURBYTE(intpos,subifdPos,0x14)
		SETFOURBYTE(intpos,subifdPos,offset)
		memcpy(initpos + offset, exifinfo->DateTime, 0x14);
		offset += 0x14;

		SETTWOBYTE(shortpos,subifdPos,0x9101)//ComponentsConfiguration
		SETTWOBYTE(shortpos,subifdPos,0x07)
		SETFOURBYTE(intpos,subifdPos,0x04)
		SETFOURBYTE(intpos,subifdPos,0x00030201)//YCbCr-format

		SETTWOBYTE(shortpos,subifdPos,0x9102)//CompressedBitsPerPixel
		SETTWOBYTE(shortpos,subifdPos,0x05)
		SETFOURBYTE(intpos,subifdPos,0x01)
		SETFOURBYTE(intpos,subifdPos,offset)
		intpos = (uint32_t*)(initpos + offset);
		*intpos++ = exifinfo->CompressedBitsPerPixel.num;
		*intpos = exifinfo->CompressedBitsPerPixel.denom;
		offset += 8;

		SETTWOBYTE(shortpos,subifdPos,0x9201)//ShutterSpeedValue
		SETTWOBYTE(shortpos,subifdPos,0x0a)
		SETFOURBYTE(intpos,subifdPos,0x01)
		SETFOURBYTE(intpos,subifdPos,offset)
		intpos = (uint32_t*)(initpos + offset);
		*intpos++ = exifinfo->ShutterSpeedValue.num;
		*intpos = exifinfo->ShutterSpeedValue.denom;
		offset += 8;

		SETTWOBYTE(shortpos,subifdPos,0x9202)//ApertureValue
		SETTWOBYTE(shortpos,subifdPos,0x05)
		SETFOURBYTE(intpos,subifdPos,0x01)
		SETFOURBYTE(intpos,subifdPos,offset)
		intpos = (uint32_t*)(initpos + offset);
		*intpos++ = exifinfo->ApertureValue.num;
		*intpos = exifinfo->ApertureValue.denom;
		offset += 8;

		SETTWOBYTE(shortpos,subifdPos,0x9204)//ExposureBiasValue
		SETTWOBYTE(shortpos,subifdPos,0x0a)
		SETFOURBYTE(intpos,subifdPos,0x01)
		SETFOURBYTE(intpos,subifdPos,offset)
		intpos = (uint32_t*)(initpos + offset);
		*intpos++ = exifinfo->ExposureBiasValue.num;
		*intpos = exifinfo->ExposureBiasValue.denom;
		offset += 8;

		SETTWOBYTE(shortpos,subifdPos,0x9205)//MaxApertureValue
		SETTWOBYTE(shortpos,subifdPos,0x05)
		SETFOURBYTE(intpos,subifdPos,0x01)
		SETFOURBYTE(intpos,subifdPos,offset)
		intpos = (uint32_t*)(initpos + offset);
		*intpos++ = exifinfo->MaxApertureValue.num;
		*intpos = exifinfo->MaxApertureValue.denom;
		offset += 8;

		SETTWOBYTE(shortpos,subifdPos,0x9207)//MeteringMode
		SETTWOBYTE(shortpos,subifdPos,0x03)
		SETFOURBYTE(intpos,subifdPos,0x01)
		SETFOURBYTE(intpos,subifdPos,exifinfo->MeteringMode)

		SETTWOBYTE(shortpos,subifdPos,0x9209)//Flash
		SETTWOBYTE(shortpos,subifdPos,0x03)
		SETFOURBYTE(intpos,subifdPos,0x01)
		SETFOURBYTE(intpos,subifdPos,exifinfo->Flash)

		SETTWOBYTE(shortpos,subifdPos,0x920a)//FocalLength
		SETTWOBYTE(shortpos,subifdPos,0x05)
		SETFOURBYTE(intpos,subifdPos,0x01)
		SETFOURBYTE(intpos,subifdPos,offset)
		intpos = (uint32_t*)(initpos + offset);
		*intpos++ = exifinfo->FocalLength.num;
		*intpos = exifinfo->FocalLength.denom;
		offset += 8;

		SETTWOBYTE(shortpos,subifdPos,0xa001)//ColorSpace
		SETTWOBYTE(shortpos,subifdPos,0x03)
		SETFOURBYTE(intpos,subifdPos,0x01)
		SETFOURBYTE(intpos,subifdPos,0x01)//sRGB

		SETTWOBYTE(shortpos,subifdPos,0xa002)//exifImageWidth
		SETTWOBYTE(shortpos,subifdPos,0x03)
		SETFOURBYTE(intpos,subifdPos,0x01)
		SETFOURBYTE(intpos,subifdPos,inInfo->inputW)

		SETTWOBYTE(shortpos,subifdPos,0xa003)//exifImageHeight
		SETTWOBYTE(shortpos,subifdPos,0x03)
		SETFOURBYTE(intpos,subifdPos,0x01)
		SETFOURBYTE(intpos,subifdPos,inInfo->inputH)

		SETTWOBYTE(shortpos,subifdPos,0xa20e)//FocalPlaneXResolution
		SETTWOBYTE(shortpos,subifdPos,0x05)
		SETFOURBYTE(intpos,subifdPos,0x01)
		SETFOURBYTE(intpos,subifdPos,offset)
		intpos = (uint32_t*)(initpos + offset);
		*intpos++ = exifinfo->FocalPlaneXResolution.num;
		*intpos = exifinfo->FocalPlaneXResolution.denom;
		offset += 8;

		SETTWOBYTE(shortpos,subifdPos,0xa20f)//FocalPlaneYResolution
		SETTWOBYTE(shortpos,subifdPos,0x05)
		SETFOURBYTE(intpos,subifdPos,0x01)
		SETFOURBYTE(intpos,subifdPos,offset)
		intpos = (uint32_t*)(initpos + offset);
		*intpos++ = exifinfo->FocalPlaneYResolution.num;
		*intpos = exifinfo->FocalPlaneYResolution.denom;
		offset += 8;

		SETTWOBYTE(shortpos,subifdPos,0xa210)//FocalPlaneResolutionUnit
		SETTWOBYTE(shortpos,subifdPos,0x03)
		SETFOURBYTE(intpos,subifdPos,0x01)
		SETFOURBYTE(intpos,subifdPos,0x02)//inch

		SETTWOBYTE(shortpos,subifdPos,0xa217)//SensingMethod
		SETTWOBYTE(shortpos,subifdPos,0x03)
		SETFOURBYTE(intpos,subifdPos,0x01)
		SETFOURBYTE(intpos,subifdPos,exifinfo->SensingMethod)

		SETTWOBYTE(shortpos,subifdPos,0xa300)//FileSource
		SETTWOBYTE(shortpos,subifdPos,0x07)
		SETFOURBYTE(intpos,subifdPos,0x01)
		SETFOURBYTE(intpos,subifdPos,exifinfo->FileSource)

		SETTWOBYTE(shortpos,subifdPos,0xa401)//CustomRendered
		SETTWOBYTE(shortpos,subifdPos,0x03)
		SETFOURBYTE(intpos,subifdPos,0x01)
		SETFOURBYTE(intpos,subifdPos,exifinfo->CustomRendered)

		SETTWOBYTE(shortpos,subifdPos,0xa402)//ExposureMode
		SETTWOBYTE(shortpos,subifdPos,0x03)
		SETFOURBYTE(intpos,subifdPos,0x01)
		SETFOURBYTE(intpos,subifdPos,exifinfo->ExposureMode)

		SETTWOBYTE(shortpos,subifdPos,0xa403)//WhiteBalance
		SETTWOBYTE(shortpos,subifdPos,0x03)
		SETFOURBYTE(intpos,subifdPos,0x01)
		SETFOURBYTE(intpos,subifdPos,exifinfo->WhiteBalance)

		SETTWOBYTE(shortpos,subifdPos,0xa404)//DigitalZoomRatio
		SETTWOBYTE(shortpos,subifdPos,0x05)
		SETFOURBYTE(intpos,subifdPos,0x01)
		SETFOURBYTE(intpos,subifdPos,offset)
		intpos = (uint32_t*)(initpos + offset);
		*intpos++ = exifinfo->DigitalZoomRatio.num;
		*intpos = exifinfo->DigitalZoomRatio.denom;
		offset += 8;
/*
		SETTWOBYTE(shortpos,subifdPos,0xa405)//FocalLengthIn35mmFilm
		SETTWOBYTE(shortpos,subifdPos,0x03)
		SETFOURBYTE(intpos,subifdPos,0x01)
		SETFOURBYTE(intpos,subifdPos,exifinfo->FocalLengthIn35mmFilm)
*/
		SETTWOBYTE(shortpos,subifdPos,0xa406)//SceneCaptureType
		SETTWOBYTE(shortpos,subifdPos,0x03)
		SETFOURBYTE(intpos,subifdPos,0x01)
		SETFOURBYTE(intpos,subifdPos,exifinfo->SceneCaptureType)

		SETFOURBYTE(intpos,subifdPos,0x00)//link to next ifd
	}
	if(gpsinfo != NULL){
		//10
		SETTWOBYTE(shortpos,startpos,0x8825)//gps ifd
		SETTWOBYTE(shortpos,startpos,0x04)
		SETFOURBYTE(intpos,startpos,0x01)
		SETFOURBYTE(intpos,startpos,offset)
			unsigned char* gpsPos = initpos+offset;
			SETTWOBYTE(shortpos,gpsPos,0x0a)//num of ifd
			offset += 2 + 0x0a*12 + 4;

			SETTWOBYTE(shortpos,gpsPos,0x00)//GPSVersionID
			SETTWOBYTE(shortpos,gpsPos,0x01)
			SETFOURBYTE(intpos,gpsPos,0x04)
			SETFOURBYTE(intpos,gpsPos,0x0202)

			SETTWOBYTE(shortpos,gpsPos,0x01)//GPSLatitudeRef
			SETTWOBYTE(shortpos,gpsPos,0x02)
			SETFOURBYTE(intpos,gpsPos,0x02)
			*gpsPos++ = gpsinfo->GPSLatitudeRef[0];
			*gpsPos++ = gpsinfo->GPSLatitudeRef[1];
			*gpsPos++ = 0;*gpsPos++ = 0;

			SETTWOBYTE(shortpos,gpsPos,0x02)//GPSLatitude
			SETTWOBYTE(shortpos,gpsPos,0x05)
			SETFOURBYTE(intpos,gpsPos,0x03)
			SETFOURBYTE(intpos,gpsPos,offset)
			intpos = (uint32_t*)(initpos + offset);
			*intpos++ = gpsinfo->GPSLatitude[0].num;
			*intpos++ = gpsinfo->GPSLatitude[0].denom;
			*intpos++ = gpsinfo->GPSLatitude[1].num;
			*intpos++ = gpsinfo->GPSLatitude[1].denom;
			*intpos++ = gpsinfo->GPSLatitude[2].num;
			*intpos = gpsinfo->GPSLatitude[2].denom;
			offset += 8*3;

			SETTWOBYTE(shortpos,gpsPos,0x03)//GPSLongitudeRef
			SETTWOBYTE(shortpos,gpsPos,0x02)
			SETFOURBYTE(intpos,gpsPos,0x02)
			*gpsPos++ = gpsinfo->GPSLongitudeRef[0];
			*gpsPos++ = gpsinfo->GPSLongitudeRef[1];
			*gpsPos++ = 0;*gpsPos++ = 0;

			SETTWOBYTE(shortpos,gpsPos,0x04)//GPSLongitude
			SETTWOBYTE(shortpos,gpsPos,0x05)
			SETFOURBYTE(intpos,gpsPos,0x03)
			SETFOURBYTE(intpos,gpsPos,offset)
			intpos = (uint32_t*)(initpos + offset);
			*intpos++ = gpsinfo->GPSLongitude[0].num;
			*intpos++ = gpsinfo->GPSLongitude[0].denom;
			*intpos++ = gpsinfo->GPSLongitude[1].num;
			*intpos++ = gpsinfo->GPSLongitude[1].denom;
			*intpos++ = gpsinfo->GPSLongitude[2].num;
			*intpos = gpsinfo->GPSLongitude[2].denom;
			offset += 8*3;

			SETTWOBYTE(shortpos,gpsPos,0x05)//GPSAltitudeRef
			SETTWOBYTE(shortpos,gpsPos,0x02)
			SETFOURBYTE(intpos,gpsPos,0x01)
			*gpsPos++ = gpsinfo->GPSAltitudeRef;
			*gpsPos++ = 0;*gpsPos++ = 0;*gpsPos++ = 0;

			SETTWOBYTE(shortpos,gpsPos,0x06)//GPSAltitude
			SETTWOBYTE(shortpos,gpsPos,0x05)
			SETFOURBYTE(intpos,gpsPos,0x01)
			SETFOURBYTE(intpos,gpsPos,offset)
			intpos = (uint32_t*)(initpos + offset);
			*intpos++ = gpsinfo->GPSAltitude.num;
			*intpos = gpsinfo->GPSAltitude.denom;
			offset += 8;

			SETTWOBYTE(shortpos,gpsPos,0x07)//GpsTimeStamp
			SETTWOBYTE(shortpos,gpsPos,0x05)
			SETFOURBYTE(intpos,gpsPos,0x03)
			SETFOURBYTE(intpos,gpsPos,offset)
			intpos = (uint32_t*)(initpos + offset);
			*intpos++ = gpsinfo->GPSLongitude[0].num;
			*intpos++ = gpsinfo->GPSLongitude[0].denom;
			*intpos++ = gpsinfo->GPSLongitude[1].num;
			*intpos++ = gpsinfo->GPSLongitude[1].denom;
			*intpos++ = gpsinfo->GPSLongitude[2].num;
			*intpos = gpsinfo->GPSLongitude[2].denom;
			offset += 8*3;

			SETTWOBYTE(shortpos,gpsPos,0x1b)//GPSProcessingMethod
			SETTWOBYTE(shortpos,gpsPos,0x07)
			stringlen = gpsinfo->GpsProcessingMethodchars;
			SETFOURBYTE(intpos,gpsPos,stringlen)
			if(stringlen <= 4){
				SETFOURBYTESTRING(gpsPos,gpsinfo->GPSProcessingMethod,stringlen, increment)
               #if      0
                int i =0;
                increment = 4;
                while(increment > 0)
                {
                  if(stringlen > 0)
                  {
                    *gpsPos++ = gpsinfo->GPSProcessingMethod[i];
                     stringlen--;
                     i++;
                  }else{
                    *gpsPos++ ='\0';
                  }
                  increment--;
                }
              #endif
			}else{
				SETFOURBYTE(intpos,gpsPos,offset)
				memcpy(initpos + offset,gpsinfo->GPSProcessingMethod,stringlen);
				offset += stringlen;
			}

			SETTWOBYTE(shortpos,gpsPos,0x1d)//GPSDateStamp
			SETTWOBYTE(shortpos,gpsPos,0x02)
			SETFOURBYTE(intpos,gpsPos,0x11)
			SETFOURBYTE(intpos,gpsPos,offset)
			memcpy(initpos + offset, gpsinfo->GpsDateStamp, 11);
			offset += 11;

			SETFOURBYTE(intpos,gpsPos,0x00)//link to next ifd
	}

	SETFOURBYTE(intpos,startpos,offset)//next ifd offset
	startpos = initpos + offset;
	SETTWOBYTE(shortpos,startpos,0x03)
	offset += 2 + 0x03*12 + 4;
	if(((offset+BEFORE_EXIF_LEN)&63) != 0){//do not align 64, adjust
		stringlen = 64 - ((offset+BEFORE_EXIF_LEN)&63);
		offset += stringlen;
	}
	SETTWOBYTE(shortpos,startpos,0x0103)//Compression
	SETTWOBYTE(shortpos,startpos,0x03)
	SETFOURBYTE(intpos,startpos,0x01)
	SETFOURBYTE(intpos,startpos,0x06)//JPEG

	SETTWOBYTE(shortpos,startpos,0x0201)//JPEG data offset
	SETTWOBYTE(shortpos,startpos,0x04)
	SETFOURBYTE(intpos,startpos,0x01)
	SETFOURBYTE(intpos,startpos,offset)

	SETTWOBYTE(shortpos,startpos,0x0202)//JPEG data count
	SETTWOBYTE(shortpos,startpos,0x04)
	SETFOURBYTE(intpos,startpos,0x01)
	SETFOURBYTE(intpos,startpos,0x0)
	*jpeglenPos = intpos;//save the been set pos
	SETFOURBYTE(intpos,startpos,0x00)//link to next ifd
	if(stringlen != 0){
		memset(startpos,0,stringlen);
	}
	return offset + BEFORE_EXIF_LEN;
}

//flag: scale algorithm(see at swscale.h if use swscale, else see at pixman-private.h)
int doSoftScale(uint8_t *srcy, uint8_t *srcuv, int srcw, int srch, uint8_t *dsty, uint8_t *dstuv, int dstw, int dsth, int flag){
	if(srcw <= 0 || srch <= 0 || dstw <= 0 || dsth <= 0){
		return -1;
	}
#if 0
#ifdef USE_PIXMAN
	if(srcuv != srcy+srcw*srch || dstuv != dsty+dstw*dsth){
		LOGE("uv data must follow y data.");
		return -1;
	}
	pixman_format_code_t format = PIXMAN_yv12;
	pixman_filter_t f = (pixman_filter_t)flag;
	pixman_image_t *src_img = NULL, *dst_img = NULL;
	pixman_transform_t transform, pform;
	pixman_box16_t box;
	box.x1 = 384;
	box.y1 = 256;
	box.x2 = 768;
	box.y2 = 512;
	pixman_transform_init_identity(&transform);
	pixman_transform_init_identity(&pform);
	double xscale = (double)dstw/(double)srcw;
	double yscale = (double)dsth/(double)srch;
	//pixman_transform_init_scale(&transform, pixman_int_to_fixed(srcw), pixman_int_to_fixed(srch));
	if(!pixman_transform_scale(&pform, &transform, pixman_double_to_fixed(xscale), pixman_double_to_fixed(yscale))){
		LOGE("pixman_transform_scale fail.");
		return -1;
	}
	pixman_transform_bounds(&pform, &box);
	LOGE("foward box: %d, %d, %d, %d", box.x1, box.y1, box.x2, box.y2);
	box.x1 = 384;
	box.y1 = 256;
	box.x2 = 768;
	box.y2 = 512;
	pixman_transform_bounds(&transform, &box);
	LOGE("reverse box: %d, %d, %d, %d", box.x1, box.y1, box.x2, box.y2);

	src_img = pixman_image_create_bits(format, srcw, srch, srcy, srcw);
	if(src_img == NULL){
		LOGE("pixman create src imge fail.");
		return -1;
	}
	dst_img = pixman_image_create_bits(format, dstw, dsth, dsty, dstw);
	if(dst_img == NULL){
		LOGE("pixman create dst imge fail.");
		return -1;
	}
	if(!pixman_image_set_transform(src_img, &transform) ||
		!pixman_image_set_filter(src_img, f, NULL, 0)){
		LOGE("pixman_image_set_transform or pixman_image_set_filter fail.");
		return -1;
	}

	//yuv unsupport out yuv
	pixman_image_composite(PIXMAN_OP_SRC, src_img,
						NULL, dst_img,
						0, 0, 0, 0,
						0, 0, dstw, dsth);
	pixman_image_unref(src_img);
	pixman_image_unref(dst_img);
	return -1;
#else
	uint8_t * yuvsrc[4] = {srcy, srcuv, NULL, NULL};
	int yuvsrcstride[4] = {srcw, srcw, 0, 0};
	uint8_t * yuvdst[4] = {dsty, dstuv, NULL, NULL};
	int yuvdststride[4] = {dstw, dstw, 0, 0};
	struct SwsContext *sws = sws_getContext(srcw,srch, PIX_FMT_NV12, dstw, dsth, PIX_FMT_NV12,
		flag, NULL, NULL, NULL);//just for yuv420sp
	if(sws == NULL){
		return -1;
	}
	WHENCLOG("before sws_scale.");
	sws_scale(sws, yuvsrc, yuvsrcstride, 0, srch, yuvdst, yuvdststride);
	WHENCLOG("after sws_scale.");
	sws_freeContext(sws);
#endif
#endif
	return 0;
}

#define IPP_TIMES_PER 8
#define ALIGNVAL 31
int encodeThumb(JpegEncInInfo *inInfo, JpegEncOutInfo *outInfo){
	JpegEncInInfo JpegInInfo;
	JpegEncOutInfo JpegOutInfo;
#ifdef USE_IPP
	VPUMemLinear_t inMem;
	int tmpw = 0;
	int tmph = 0;
	int tmpwhIsValid = 0;
#endif
	int outw = inInfo->thumbW;
	int outh = inInfo->thumbH;
	if(inInfo->rotateDegree == DEGREE_90 || inInfo->rotateDegree == DEGREE_270){
		outw = inInfo->thumbH;
		outh = inInfo->thumbW;
	}
#ifdef DO_APP1_THUMB
	unsigned char *startpos = (unsigned char*)outInfo->outBufVirAddr;
	uint32_t *thumbPos = NULL;
	int headerlen = 0;
	int addzerolen = 0;
#endif
#ifdef USE_IPP
	//ipp
	struct rk29_ipp_req ipp_req;
	int ipp_fd;
	//static struct pollfd fd;
	int ippret = -1;
#endif

#ifdef HW_JPEG_DEBUG
FILE *file = NULL;
FILE *ippoutyuv = NULL;
#endif

#ifdef USE_IPP
	if(inInfo->type != JPEGENC_YUV420_SP){
		LOGE("source type is not yuv420sp, can not do thumbnail.");
		return 0;
	}
	WHENCLOG("open ipp.");
	ipp_fd = open("/dev/rk29-ipp",O_RDWR,0);
	if(ipp_fd < 0)
	{
		LOGE("encodethumb, open /dev/rk29-ipp fail!");
		return 0;
	}
	if(inInfo->inputW <= outw*IPP_TIMES_PER){
		tmpw = outw;
	}else{
		tmpw = (inInfo->inputW/IPP_TIMES_PER + ALIGNVAL) & (~ALIGNVAL);//set to align
	}
	if(inInfo->inputH <= outh*IPP_TIMES_PER){
		tmph = outh;
	}else{
		tmph = (inInfo->inputH/IPP_TIMES_PER + ALIGNVAL) & (~ALIGNVAL);//set to align
	}
	if(tmpw != outw || tmph != outh){
		tmpwhIsValid = 1;
	}
	if(VPUMallocLinear(&inMem, (outw*outh + tmpw*tmph*tmpwhIsValid)*3/2) != 0){
		LOGE("encodethumb malloc pmem fail.");
		close(ipp_fd);
		return 0;
	}
	if(inInfo->rotateDegree != DEGREE_180){
		ipp_req.src0.YrgbMst = inInfo->y_rgb_addr;
		ipp_req.src0.CbrMst = inInfo->uv_addr;
	}else{
		ipp_req.src0.YrgbMst = inInfo->yuvaddrfor180;
		ipp_req.src0.CbrMst = inInfo->yuvaddrfor180 + inInfo->inputW*inInfo->inputH;
	}
	ipp_req.src0.w = inInfo->inputW;
	ipp_req.src0.h = inInfo->inputH;
	ipp_req.src0.fmt = IPP_Y_CBCR_H2V2;//420SP

	ipp_req.dst0.YrgbMst = inMem.phy_addr;
	ipp_req.dst0.CbrMst = inMem.phy_addr + tmpw*tmph;
	ipp_req.dst0.w = tmpw;
	ipp_req.dst0.h = tmph;

	ipp_req.src_vir_w = inInfo->inputW;
	ipp_req.dst_vir_w = tmpw;
	ipp_req.timeout = 500;
	ipp_req.flag = IPP_ROT_0;
	ipp_req.store_clip_mode = 0;
	ipp_req.complete = NULL;


	//fd.fd = ipp_fd;
	//fd.events = POLLIN;

	ippret = ioctl(ipp_fd, IPP_BLIT_SYNC, &ipp_req);//sync
	if(ippret != 0){
		LOGE("encodethumb, ioctl: IPP_BLIT_SYNC faild!");
		VPUFreeLinear(&inMem);
		close(ipp_fd);
		return 0;
	}
	if(tmpwhIsValid){
		WHENCLOG("scale need to do second time, midtmpWH:%d,%d,outWH:%d,%d", tmpw, tmph, outw, outh);
#ifdef SECOND_USE_SOFT
		VPUMemInvalidate(&inMem);
		{
			uint8_t * src = (uint8_t*)inMem.vir_addr;
			uint8_t * dst = (uint8_t*)inMem.vir_addr + tmpw*tmph*3/2;
#if 0
#ifdef USE_PIXMAN
			if(doSoftScale(src, src+tmpw*tmph, tmpw, tmph,
				dst, dst+outw*outh, outw, outh, (int)PIXMAN_FILTER_FAST) < 0){
#else
			if(doSoftScale(src, src+tmpw*tmph, tmpw, tmph,
				dst, dst+outw*outh, outw, outh, SWS_FAST_BILINEAR) < 0){
#endif
#endif
				LOGE("encodethumb, doSoftScale faild!");
				VPUFreeLinear(&inMem);
				close(ipp_fd);
				return 0;
			}
		}
		VPUMemClean(&inMem);
		ipp_req.dst0.YrgbMst = inMem.phy_addr + tmpw*tmph*3/2;
		ipp_req.dst0.CbrMst = ipp_req.dst0.YrgbMst + outw*outh;
		ipp_req.dst0.w = outw;
		ipp_req.dst0.h = outh;
#else
		ipp_req.src0.YrgbMst = inMem.phy_addr;
		ipp_req.src0.CbrMst = inMem.phy_addr + tmpw*tmph;
		ipp_req.src0.w = tmpw;
		ipp_req.src0.h = tmph;
		ipp_req.src0.fmt = IPP_Y_CBCR_H2V2;//420SP

		ipp_req.dst0.YrgbMst = inMem.phy_addr + tmpw*tmph*3/2;
		ipp_req.dst0.CbrMst = ipp_req.dst0.YrgbMst + outw*outh;
		ipp_req.dst0.w = outw;
		ipp_req.dst0.h = outh;

		ipp_req.src_vir_w = tmpw;
		ipp_req.dst_vir_w = outw;
		ipp_req.timeout = 500;
		ipp_req.flag = IPP_ROT_0;
		ipp_req.store_clip_mode = 0;
		ipp_req.complete = NULL;

		ippret = ioctl(ipp_fd, IPP_BLIT_SYNC, &ipp_req);//sync
		if(ippret != 0){
			LOGE("encodethumb, ioctl2: IPP_BLIT_SYNC faild!");
			VPUFreeLinear(&inMem);
			close(ipp_fd);
			return 0;
		}
#endif
	}
	if(close(ipp_fd) < 0){
		LOGE("close ipp fd fail. Check here.");
	}
	WHENCLOG("close ipp.");
#endif
	JpegInInfo.frameHeader = 1;
	if(inInfo->rotateDegree != DEGREE_180){
		JpegInInfo.rotateDegree = inInfo->rotateDegree;
	}else{
		JpegInInfo.rotateDegree = DEGREE_0;
	}
#ifdef USE_IPP
	JpegInInfo.y_rgb_addr = ipp_req.dst0.YrgbMst;
	JpegInInfo.uv_addr = ipp_req.dst0.CbrMst;
	JpegInInfo.inputW = ipp_req.dst0.w;
	JpegInInfo.inputH = ipp_req.dst0.h;
#else
	JpegInInfo.y_rgb_addr = inInfo->y_rgb_addr;
	JpegInInfo.uv_addr = inInfo->uv_addr;
	JpegInInfo.inputW = inInfo->inputW;
	JpegInInfo.inputH = inInfo->inputH;
#endif
	JpegInInfo.type = inInfo->type;
	JpegInInfo.qLvl = inInfo->thumbqLvl;
	JpegInInfo.doThumbNail = 0;
	JpegInInfo.thumbData = NULL;
	JpegInInfo.thumbDataLen = -1;
	JpegInInfo.thumbW = -1;
	JpegInInfo.thumbH = -1;
	JpegInInfo.thumbqLvl = 0;
	JpegInInfo.exifInfo = NULL;
	JpegInInfo.gpsInfo = NULL;
#ifdef DO_APP1_THUMB
    WHENCLOG("exifInfo is : 0x%x", inInfo->exifInfo);
	if(inInfo->frameHeader && inInfo->exifInfo != NULL){
        LOGD("do customInsertApp1Header.");
		headerlen =   customInsertApp1Header(startpos, inInfo, &thumbPos);
	}else{
		headerlen = 0;
	}
#endif
#ifdef DO_APP1_THUMB
	JpegOutInfo.outBufPhyAddr = outInfo->outBufPhyAddr+headerlen;//ffd8 + app0 header - ffd9
	JpegOutInfo.outBufVirAddr = outInfo->outBufVirAddr+headerlen;
	JpegOutInfo.outBuflen = outw * outh + headerlen;
#else
	JpegOutInfo.outBufPhyAddr = outInfo->outBufPhyAddr;
	JpegOutInfo.outBufVirAddr = outInfo->outBufVirAddr;
	JpegOutInfo.outBuflen = outw * outh;
#endif
	if(JpegOutInfo.outBuflen < 1024){
		JpegOutInfo.outBuflen = 1024;
	}
	JpegOutInfo.jpegFileLen = 0;
	JpegOutInfo.cacheflush = outInfo->cacheflush;

	WHENCLOG("encodethumb, outbuflen: %d", JpegOutInfo.outBuflen);
	if(hw_jpeg_encode(&JpegInInfo, &JpegOutInfo) < 0 || JpegOutInfo.jpegFileLen <= 0){
		LOGE("encodethumb, hw jpeg encode fail.");
#ifdef USE_IPP
		VPUFreeLinear(&inMem);
#endif
		return 0;
	}

#ifdef DO_APP1_THUMB
    //LOGD("headerlen: %d",headerlen);
	if(headerlen != 0){
		*thumbPos = JpegOutInfo.jpegFileLen;
		int retlen = JpegOutInfo.jpegFileLen + headerlen - BACKUP_LEN;
		if((retlen & 63) != 0){
			addzerolen = (64 - (retlen & 63));
			memset(startpos + JpegOutInfo.jpegFileLen + headerlen, 0, addzerolen);
		}
		WHENCLOG("thumb filelen: %d, headerlen: %d, addzerolen: %d, before ffd8 len: %d",
			 JpegOutInfo.jpegFileLen, headerlen, addzerolen, retlen+addzerolen);
		retlen = JpegOutInfo.jpegFileLen + headerlen - 4/*LEN_BEFORE_APP1LEN*/ + addzerolen;
		*((unsigned short*)(startpos+4)) = ((retlen&0xff00)>>8) | ((retlen&0xff)<<8);
	}
#endif

#ifdef USE_IPP
	VPUFreeLinear(&inMem);
#endif

#ifdef DO_APP1_THUMB
	if(headerlen != 0){
		//flush out buf
		if(JpegOutInfo.cacheflush){
			JpegOutInfo.cacheflush(1, 0, 0x7fffffff/*JpegOutInfo.jpegFileLen + headerlen*/);
		}
		return JpegOutInfo.jpegFileLen + headerlen - BACKUP_LEN + addzerolen;
	}else {
		return JpegOutInfo.jpegFileLen;
	}
#else
	return JpegOutInfo.jpegFileLen;
#endif
}

int hw_jpeg_rotate(JpegEncInInfo *inInfo){
#ifdef USE_IPP
	struct rk29_ipp_req ipp_req;
	int ipp_fd;
	int ippret = -1;
	int tmpw = inInfo->inputW;
	int tmph = inInfo->inputH;
	ipp_fd = open("/dev/rk29-ipp",O_RDWR,0);
	if(ipp_fd < 0)
	{
		LOGE("hw_jpeg_rotate, open /dev/rk29-ipp fail!");
		return -1;
	}
	ipp_req.src0.YrgbMst = inInfo->y_rgb_addr;
	ipp_req.src0.CbrMst = inInfo->uv_addr;
	ipp_req.src0.w = tmpw;
	ipp_req.src0.h = tmph;
	ipp_req.src0.fmt = IPP_Y_CBCR_H2V2;//420SP

	ipp_req.dst0.YrgbMst = inInfo->yuvaddrfor180;
	ipp_req.dst0.CbrMst = inInfo->yuvaddrfor180 + tmpw*tmph;
	ipp_req.dst0.w = tmpw;
	ipp_req.dst0.h = tmph;

	ipp_req.src_vir_w = inInfo->inputW;
	ipp_req.dst_vir_w = tmpw;
	ipp_req.timeout = 500;
	if(inInfo->rotateDegree == DEGREE_180){
		ipp_req.flag = IPP_ROT_180;
	}else{
		//ipp_req.flag = IPP_ROT_0;
	}
	ipp_req.complete = NULL;
	ippret = ioctl(ipp_fd, IPP_BLIT_SYNC, &ipp_req);//sync
	if(ippret != 0){
		LOGE("hw_jpeg_rotate, ioctl: IPP_BLIT_SYNC faild!");
		close(ipp_fd);
		return -1;
	}
	if(close(ipp_fd) < 0){
		LOGE("close ipp fd fail. Check here.");
	}
	return 0;
#else
	return -1;
#endif
}

int hw_jpeg_encode(JpegEncInInfo *inInfo, JpegEncOutInfo *outInfo)
{
	JpegEncRet ret = JPEGENC_OK;
	JpegEncInst encoder = NULL;
	JpegEncCfg cfg;
	JpegEncIn encIn;
	JpegEncOut encOut;
	JpegEncThumb thumbCfg;

	/* Output buffer size: For most images 1 byte/pixel is enough
	for high quality JPEG */
	RK_U32 outbuf_size = outInfo->outBuflen;
	RK_U32 pict_bus_address = 0;
#ifdef DO_APP1_THUMB
	unsigned char *startpos = NULL;
	unsigned char endch[BACKUP_LEN] = {0};
#endif
	/* Step 1: Initialize an encoder instance */
	memset(&cfg, 0, sizeof(JpegEncCfg));
	cfg.qLevel = inInfo->qLvl;
	cfg.frameType = inInfo->type;
	cfg.markerType = JPEGENC_SINGLE_MARKER;
	cfg.unitsType = JPEGENC_DOTS_PER_INCH;
	cfg.xDensity = 72;
	cfg.yDensity = 72;
	/*  no cropping */
	cfg.inputWidth = inInfo->inputW;
	cfg.codingWidth = inInfo->inputW;
	cfg.inputHeight = inInfo->inputH;
	cfg.codingHeight = inInfo->inputH;
	cfg.xOffset = 0;
	cfg.yOffset = 0;
	if(inInfo->rotateDegree == DEGREE_90 || inInfo->rotateDegree == DEGREE_270){
		cfg.codingWidth = inInfo->inputH;
		cfg.codingHeight = inInfo->inputW;
		cfg.rotation = inInfo->rotateDegree;
	}else{
		cfg.rotation = JPEGENC_ROTATE_0;
		if(inInfo->rotateDegree == DEGREE_180){
			if(hw_jpeg_rotate(inInfo) < 0){
				LOGD("hw_jpeg_rotate fail.");
				goto end;
			}
		}
	}
	cfg.codingType = JPEGENC_WHOLE_FRAME;
	/* disable */
	cfg.restartInterval = 0;

	outInfo->finalOffset = 0;

	thumbCfg.dataLength = 0;
	if(inInfo->doThumbNail)
	{
		thumbCfg.data = NULL;
		thumbCfg.format = JPEGENC_THUMB_JPEG;//just use jpeg enc
		thumbCfg.width = inInfo->thumbW;
		thumbCfg.height = inInfo->thumbH;
		if(NULL != inInfo->thumbData){
			thumbCfg.dataLength = inInfo->thumbDataLen;
			thumbCfg.data = inInfo->thumbData;
		}else{
			if((thumbCfg.dataLength = encodeThumb(inInfo, outInfo)) <= 0){
				goto end;
			}
//goto end;
			thumbCfg.data = (void*)outInfo->outBufVirAddr;
			//thumbCfg.dataLength -= 2;//ffd8 ffd9or0000
#ifdef DO_APP1_THUMB
			if(inInfo->frameHeader && NULL != inInfo->exifInfo){
				startpos = (((unsigned char*)outInfo->outBufVirAddr) + thumbCfg.dataLength);
				memcpy(endch, startpos, BACKUP_LEN);
				WHENCLOG("thumbnail datalen: %d", thumbCfg.dataLength);
			}
#endif
		}

	}

	if((ret = JpegEncInit(&cfg, &encoder)) != JPEGENC_OK)
	{
		/* Handle here the error situation */
		//LOGD( "JPEGENCDOER: JpegEncInit fail.\n");
		goto end;
	}

	jpegInstance_s *pEncInst = (jpegInstance_s*)encoder;
	pEncInst->cacheflush = outInfo->cacheflush;

#ifdef DO_APP1_THUMB
	if(startpos != NULL){
		//only app1 thumbnail, startpos is not null
		outInfo->finalOffset = 0;
		encIn.pOutBuf = (RK_U8*)outInfo->outBufVirAddr + thumbCfg.dataLength;// == startpos
		encIn.busOutBuf = outInfo->outBufPhyAddr + thumbCfg.dataLength;
		encIn.outBufSize = outbuf_size - thumbCfg.dataLength;
	}else{
		outInfo->finalOffset = ((thumbCfg.dataLength+63)&(~63));
		encIn.pOutBuf = (RK_U8*)outInfo->outBufVirAddr + outInfo->finalOffset;
		encIn.busOutBuf = outInfo->outBufPhyAddr + outInfo->finalOffset;
		encIn.outBufSize = outbuf_size - outInfo->finalOffset;
		/* Step 2: Configuration of thumbnail */
		if(inInfo->doThumbNail)
		{
			if((ret = JpegEncSetThumbnail(encoder, &thumbCfg)) != JPEGENC_OK)
			{
				/* Handle here the error situation */
				LOGD( "JPEGENCDOER: JpegEncSetPictureSize fail.\n");
				goto close;
			}
		}
	}
#else
	encIn.pOutBuf = (RK_U8*)outInfo->outBufVirAddr;
	encIn.busOutBuf = outInfo->outBufPhyAddr;
	encIn.outBufSize = outbuf_size;
#endif

	/* Step 3: Configuration of picture size */
	if((ret = JpegEncSetPictureSize(encoder, &cfg)) != JPEGENC_OK)
	{
		/* Handle here the error situation */
		LOGD( "JPEGENCDOER: JpegEncSetPictureSize fail.\n");
		goto close;
	}


	/* frame headers */
	encIn.frameHeader = inInfo->frameHeader;

	/* not 420_p */
	if(inInfo->rotateDegree != DEGREE_180){
		encIn.busLum = inInfo->y_rgb_addr;
		encIn.busCb = inInfo->uv_addr;
	}else{
		encIn.busLum = inInfo->yuvaddrfor180;
		encIn.busCb = inInfo->yuvaddrfor180 + inInfo->inputW*inInfo->inputH;
	}

    encIn.busCr = encIn.busCb + inInfo->inputW*inInfo->inputH/4;

	/* Step 5: Encode the picture */
	/* Loop until the frame is ready */

	//LOGD( "\t-JPEG: JPEG output bus: 0x%08x\n",
    //               		encIn.busOutBuf);

	//LOGD( "\t-JPEG: File input bus: 0x%08x\n",
    //                	encIn.busLum);
	do
	{
		ret = JpegEncEncode(encoder, &encIn, &encOut);
		switch (ret){
			case JPEGENC_RESTART_INTERVAL:
				LOGD("JPEGENC_RESTART_INTERVAL");
			case JPEGENC_FRAME_READY:
				//fLOGD(stdout, "OUTPUT JPG length is %d\n",encOut.jfifSize);
				//fout = fopen(outPath, "wb");
				//fwrite(encIn.pOutBuf, encOut.jfifSize, 1, fout);
				//fclose(fout);
				//memcpy(outInfo->outBufVirAddr, outMem.vir_addr,encOut.jfifSize);
				outInfo->jpegFileLen = encOut.jfifSize;
			break;
			default:
			/* All the others are error codes */
			break;
		}
	}
	while (ret == JPEGENC_RESTART_INTERVAL);

close:
end:
	/* Last Step: Release the encoder instance */
	if((ret = JpegEncRelease(encoder)) != JPEGENC_OK){
		LOGD( "JPEGENCDOER: JpegEncRelease fail with : %d\n", ret);
	}else{
#ifdef DO_APP1_THUMB
WHENCLOG("00000000000000000000 ret : %d, startpos: %x, FileLen+datalength: %d", ret, startpos,
outInfo->jpegFileLen + thumbCfg.dataLength);
		if(ret >= 0 && startpos != NULL){
			//only app1 thumbnail, startpos is not null
			memcpy(startpos, endch, BACKUP_LEN);
			//flush out buf
			if(outInfo->cacheflush){
				outInfo->cacheflush(1, 0, 0x7fffffff/*outInfo->jpegFileLen + headerlen*/);
			}
			outInfo->jpegFileLen += thumbCfg.dataLength;
		}
#endif
	}
	/* release output buffer and picture buffer, if needed */
 	/* nothing else to do */
	//LOGV( "RUN OUT JPEGENCDOER.\n");
return 0;
}

