LOCAL_PATH := $(call my-dir)
BUILD_AVC_TEST := false
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	src/deinter.cpp 

LOCAL_MODULE := libpost_deinterlace

LOCAL_ARM_MODE := arm

LOCAL_STATIC_LIBRARIES :=

LOCAL_SHARED_LIBRARIES :=

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/../../common \
 	$(LOCAL_PATH)/../../common/include \
 	$(TOP)/frameworks/av/media/libstagefright/include \
 	$(LOCAL_PATH)/include \
 	$(LOCAL_PATH)/include/android \
	$(TOP)/hardware/rk29/libon2 

include $(BUILD_STATIC_LIBRARY)
