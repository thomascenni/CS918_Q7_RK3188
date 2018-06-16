LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
		../../common/bitstream.cpp \
 		../../common/framemanager.cpp \
 		../../common/reg.cpp \
		../../common/postprocess.cpp \
		src/vp8decoder.cpp \
		src/pv_vp8dec_api.cpp \

LOCAL_MODULE := libvpu_vp8dec

ifeq ($(PLATFORM_VERSION),4.0.4)
	LOCAL_CFLAGS := -DAVS40
endif

LOCAL_ARM_MODE := arm

LOCAL_STATIC_LIBRARIES := 

LOCAL_SHARED_LIBRARIES := 

LOCAL_C_INCLUDES := \
		$(LOCAL_PATH)/src \
 		$(LOCAL_PATH)/include \
 		$(LOCAL_PATH)/../../common/include \
 		$(LOCAL_PATH)/../../common \

include $(BUILD_STATIC_LIBRARY)
