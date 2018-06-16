/*
 * Copyright (C) 2016 Rockchip Electronics Co.Ltd
 * Authors:
 *      Zhiqin Wei <wzq@rock-chips.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "rgaProcess.h"

#define ALOGE	printf
#define ALOGW	printf

int rgaScaleAndRotate(int rot, void* src, void* dst,
                            int sx, int sy, int sw, int sh, int ss, int sf,
                            int dx, int dy, int dw, int dh, int ds, int df)
{
	struct rga_req rga_request;
	int orientation = rot;
	long ret = 0;
	int rga_fd = -1;

	UNUSED(sx);
	UNUSED(sy);
	UNUSED(dx);
	UNUSED(dy);
	if((rga_fd = open("/dev/rga",O_RDWR)) < 0) {
		ALOGE("%s(%d):open rga device failed!!",__FUNCTION__,__LINE__);
		ret = -1;
		return ret;
	}

	memset(&rga_request, 0, sizeof(rga_request));

	switch (orientation) {
	case 2: 
		rga_request.rotate_mode = 2;
                rga_request.dst.act_w = dw;
                rga_request.dst.act_h = dh;
                rga_request.dst.x_offset = 0;
                rga_request.dst.y_offset = 0;
		break;
	case 3:
		rga_request.rotate_mode = 3;
                rga_request.dst.act_w = dw;
                rga_request.dst.act_h = dh;
                rga_request.dst.x_offset = 0;
                rga_request.dst.y_offset = 0;
                break;
	case 90:
		rga_request.rotate_mode = 1;
		rga_request.sina = 65536;
		rga_request.cosa = 0;
		rga_request.dst.act_w = dh;
		rga_request.dst.act_h = dw;
		rga_request.dst.x_offset = dw - 1;
		rga_request.dst.y_offset = 0;
		break;
	case 180:
		rga_request.rotate_mode = 1;
		rga_request.sina = 0;
		rga_request.cosa = -65536;
		rga_request.dst.act_w = dw;
		rga_request.dst.act_h = dh;
		rga_request.dst.x_offset = dw - 1;
		rga_request.dst.y_offset = dh - 1;
		break;
	case 270:
		rga_request.rotate_mode = 1;
		rga_request.sina = -65536;
		rga_request.cosa = 0;
		rga_request.dst.act_w = dh;
		rga_request.dst.act_h = dw;
		rga_request.dst.x_offset = 0;
		rga_request.dst.y_offset = dh - 1;
		break;
	default:
		rga_request.rotate_mode = 0;
		rga_request.dst.act_w = dw;
		rga_request.dst.act_h = dh;
		rga_request.dst.x_offset = 0;
		rga_request.dst.y_offset = 0;
		break;
	}

	rga_request.src.yrgb_addr = (unsigned int)src;
	rga_request.src.uv_addr = (unsigned int)src + ss * sh;
	rga_request.src.v_addr = 0;

	rga_request.dst.yrgb_addr = (unsigned int)dst;
	rga_request.dst.uv_addr = (unsigned int)dst + ds * dh;
	rga_request.dst.v_addr = 0;

	rga_request.src.vir_w = ss;
	rga_request.src.vir_h = sh;
	rga_request.src.format = sf;
	rga_request.src.act_w = sw;
	rga_request.src.act_h = sh;
	rga_request.src.x_offset = 0;
	rga_request.src.y_offset = 0;

	rga_request.dst.vir_w = ds;
	rga_request.dst.vir_h = dh;
	rga_request.dst.format = df;

	rga_request.clip.xmin = 0;
	rga_request.clip.xmax = dw - 1;
	rga_request.clip.ymin = 0;
	rga_request.clip.ymax = dh - 1;
	rga_request.scale_mode = 0;

//	rga_request.mmu_info.mmu_en = 1;
//	rga_request.mmu_info.mmu_flag = ((2 & 0x3) << 4) | 1 | (1 <<31 | 1 << 8 | 1<<10);

	if(ioctl(rga_fd, RGA_BLIT_SYNC, &rga_request)) {
		//LOGE(" %s(%d) RGA_BLIT fail",__FUNCTION__, __LINE__);
		fprintf(stderr, " %s(%d) RGA_BLIT fail",__FUNCTION__, __LINE__);
	}

	close(rga_fd);

	return 0;
}

int rgaLargeScale(void* src, void* dst,
                            int sx, int sy, int sw, int sh, int ss, int sf,
                            int dx, int dy, int dw, int dh, int ds, int df)
{
	bool check = true;
	int sbytes, dbytes;

	UNUSED(sx);
	UNUSED(sy);
	UNUSED(ss);
	UNUSED(sf);
	UNUSED(dx);
	UNUSED(dy);
	UNUSED(ds);
	UNUSED(df);
	UNUSED(src);
	UNUSED(dst);

	check = check && (sw / dw < 2 && sh / dh < 2);

	if (check)
		ALOGW("Not large data,do not call this api");

	sbytes = rgaGetBytesPerPixel(sf);
	dbytes = rgaGetBytesPerPixel(df);

	return 0;
}

int rgaGetBytesPerPixel(int format)
{
	int bytes = 0;
	switch (format) {
		case RK_FORMAT_RGBA_8888:
		case RK_FORMAT_RGBX_8888:
		case RK_FORMAT_BGRA_8888:
			bytes = 4;
			break;

		case RK_FORMAT_RGB_565:
		case RK_FORMAT_RGBA_5551:
		case RK_FORMAT_RGBA_4444:
			bytes = 2;
			break;

		case RK_FORMAT_RGB_888:
		case RK_FORMAT_BGR_888:
			bytes = 3;
			break;

		case RK_FORMAT_YCbCr_422_SP:
		case RK_FORMAT_YCbCr_422_P:
		case RK_FORMAT_YCbCr_420_SP:
		case RK_FORMAT_YCbCr_420_P:
		case RK_FORMAT_YCrCb_422_SP:
		case RK_FORMAT_YCrCb_422_P:
		case RK_FORMAT_YCrCb_420_SP:
		case RK_FORMAT_YCrCb_420_P:
			bytes = 1;
			break;
		default:
			ALOGE("Un suport format!!!");
			break;
	}

	return bytes;
}
