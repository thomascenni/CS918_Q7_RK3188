# Copyright 2006 The Android Open Source Project
LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_DIR := $(LOCAL_PATH)/../../src_enc
LOCAL_INC_DIR := $(LOCAL_PATH)/../inc
LOCAL_VPU_DIR := $(LOCAL_PATH)/../../../common

LOCAL_MODULE := libvpu_mjpegenc
LOCAL_MODULE_TAGS := optional

ifeq ($(PLATFORM_VERSION),4.0.4)
	LOCAL_CFLAGS := -DAVS40
endif

LOCAL_ARM_MODE := arm

LOCAL_STATIC_LIBRARIES :=
	
LOCAL_C_INCLUDES := $(LOCAL_PATH) \
                    $(LOCAL_INC_DIR)\
                    $(LOCAL_VPU_DIR)\
										$(LOCAL_SRC_DIR)/jpeg \
										$(LOCAL_SRC_DIR)/common \
										$(LOCAL_VPU_DIR)/include
					
LOCAL_SRC_FILES :=  JpegEncApi.c \
										EncJpegPutBits.c \
										EncJpegInit.c \
										EncJpegCodeFrame.c \
										EncJpeg.c \
										hw_jpegenc.c \
                   	pv_jpegenc_api.cpp \
                   	../common/encasiccontroller.c \
                   	../common/encasiccontroller_v2.c \
                   	../common/encpreprocess.c
 

include $(BUILD_STATIC_LIBRARY)
