#ifndef __LIBVPU_LOG_H__
#define __LIBVPU_LOG_H__

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#define ALOGW(fmt, ...) printf(fmt "\n", ##__VA_ARGS__)
#define ALOGV(fmt, ...) printf(fmt "\n", ##__VA_ARGS__)
#define ALOGI(fmt, ...) printf(fmt "\n", ##__VA_ARGS__)
#define ALOGD(fmt, ...) printf(fmt "\n", ##__VA_ARGS__)
#define ALOGE(fmt, ...) printf(fmt "\n", ##__VA_ARGS__)

#define PROPERTY_VALUE_MAX 92

#endif
