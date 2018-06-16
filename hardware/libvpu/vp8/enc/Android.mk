LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
		src/vp8codeframe.c \
		src/vp8encapi.c \
		src/vp8entropy.c \
		src/vp8header.c \
		src/vp8init.c \
		src/vp8macroblocktools.c \
		src/vp8picparameterset.c \
		src/vp8picturebuffer.c \
		src/vp8putbits.c \
		src/vp8ratecontrol.c \
		src/vp8testid.c     \
		src/encasiccontroller.c     \
		src/encasiccontroller_v2.c     \
		src/encpreprocess.c     \
		src/pv_vp8enc_api.cpp


LOCAL_MODULE := libvpu_vp8enc

ifeq ($(PLATFORM_VERSION),4.0.4)
	LOCAL_CFLAGS := -DAVS40
endif

LOCAL_ARM_MODE := arm

LOCAL_STATIC_LIBRARIES := 

LOCAL_SHARED_LIBRARIES := libvpu

LOCAL_C_INCLUDES := \
		$(LOCAL_PATH)/src \
 		$(LOCAL_PATH)/include \
 		$(LOCAL_PATH)/../../common/include \
 		$(LOCAL_PATH)/../../common \

include $(BUILD_STATIC_LIBRARY)
#include $(BUILD_EXECUTABLE)
