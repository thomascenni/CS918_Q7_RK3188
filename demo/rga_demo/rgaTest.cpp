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
#include <stdlib.h>

#include "rgaApi.h"
#include "ionalloc.h"

#define ALIGN_4K(x)     ((x+(4096))&(~4095))

extern "C" int ion_open(unsigned long align, enum ion_module_id id, ion_device_t **ion);
extern "C" int ion_close(ion_device_t *ion);

int main()
{
    int size = 1920*1088*4;

    ion_device_t *dev;
    ion_buffer_t *src_buffer = NULL;
    ion_buffer_t *dst_buffer = NULL;
	const char *yuvFilePath = "./in_1920x1088_nv12.yuv";
    FILE *file = NULL;

    int err = ion_open(4096, ION_MODULE_VPU, &dev);
    if (err) {
        fprintf(stderr, "open ion failed ret %d\n", err);
        return -1;
    }

    err = dev->alloc(dev, ALIGN_4K(size), _ION_HEAP_RESERVE, &src_buffer);
    if (err) {
        fprintf(stderr,"IONMem: ION_IOC_ALLOC size 0x%x failed ret %d\n", size, err);
        goto err1;
    } else {
        fprintf(stderr,"SRC: Ion alloc phy: 0x%lx vaddr: %p, size: %d\n", src_buffer->phys, src_buffer->virt, size);
    }

    err = dev->alloc(dev, ALIGN_4K(size), _ION_HEAP_RESERVE, &dst_buffer);
    if (err) {
        fprintf(stderr,"IONMem: ION_IOC_ALLOC size 0x%x failed ret %d\n", size, err);
        goto err2;
    } else {
        fprintf(stderr,"DST: Ion alloc phy: 0x%lx vaddr: %p, size: %d\n", dst_buffer->phys, dst_buffer->virt, size);
    }

	file = fopen(yuvFilePath, "rb");
	if (!file) {
		fprintf(stderr, "Could not open %s\n", yuvFilePath);
		goto err3;
	} else
		fprintf(stderr, "open %s sucess\n", yuvFilePath);
	fread(src_buffer->virt, 2 * 1920 * 1088, 1, file);
	fclose(file);
	rgaScaleAndRotate(0, (void*)src_buffer->phys, (void*)dst_buffer->phys,
            /*src*/0, 0, 1920, 1088, 1920, 10,/*dst*/0, 0, 1920, 1080, 1920, 1);

	{
		const char *outFilePath = "./out_1920x1080_ABGR.yuv";
		FILE *file = fopen(outFilePath, "wb+");
		if (!file) {
			fprintf(stderr, "Could not open %s\n", outFilePath);
			goto err3;
		}
		if (dst_buffer->virt) {
			fwrite(dst_buffer->virt, 1920*1080*4, 1, file);
			fprintf(stderr, "open %s write\n", outFilePath);
		}
		fclose(file);
	}

	fprintf(stderr, "test ok\n");
	return 0;

err3:
    dev->free(dev, dst_buffer);
err2:
    dev->free(dev, src_buffer);
err1:
    ion_close(dev);
    return -1;
}
