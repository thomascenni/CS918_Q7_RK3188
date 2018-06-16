#ifndef __LIBVPU_LOG_H__
#define __LIBVPU_LOG_H__

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#define PAGE_SHIFT      12
#define PAGE_SIZE       1UL << PAGE_SHIFT

#define VPU_DEBUG(fmt, ...) \
    if (getenv("VPU_DEBUG_ENABLE") != NULL && atoi(getenv("VPU_DEBUG_ENABLE")) == 1) { \
        printf("DEBUG:%s:%d " fmt "\n", __func__, __LINE__, ##__VA_ARGS__); \
    }

#define VPU_ERROR(fmt, ...) \
    printf("ERROR:%s:%d " fmt "\n", __func__, __LINE__, ##__VA_ARGS__);

#define ALOGW(fmt, ...) VPU_DEBUG(fmt, ##__VA_ARGS__)
#define ALOGV(fmt, ...) VPU_DEBUG(fmt, ##__VA_ARGS__)
#define ALOGI(fmt, ...) VPU_DEBUG(fmt, ##__VA_ARGS__)
#define ALOGD(fmt, ...) VPU_DEBUG(fmt, ##__VA_ARGS__)
#define ALOGE(fmt, ...) VPU_ERROR(fmt, ##__VA_ARGS__)

#define PROPERTY_VALUE_MAX 92

#endif
