LOCAL_PATH := $(call my-dir)
BUILD_AVC_TEST := false
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	src/h264decapi.cpp \
	src/h264hwd_asic.cpp \
	src/h264hwd_decoder.cpp \
	src/h264hwd_dpb.cpp \
	src/h264hwd_storage.cpp \
	src/h264hwd_byte_stream.cpp \
	src/h264hwd_nal_unit.cpp \
	src/h264hwd_pic_order_cnt.cpp \
	src/h264hwd_pic_param_set.cpp \
	src/h264hwd_seq_param_set.cpp \
	src/h264hwd_slice_group_map.cpp \
	src/h264hwd_slice_header.cpp \
	src/h264hwd_stream.cpp \
	src/h264hwd_util.cpp \
	src/h264hwd_vlc.cpp \
	src/h264hwd_vui.cpp \
	src/pv_avcdec_api.cpp \
	src/refbuffer.cpp \
	src/regdrv.cpp 

LOCAL_MODULE := libvpu_avcdec

LOCAL_ARM_MODE := arm

LOCAL_STATIC_LIBRARIES :=

LOCAL_SHARED_LIBRARIES :=

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/src \
	$(LOCAL_PATH)/include \
	$(LOCAL_PATH)/../../common \
 	$(LOCAL_PATH)/../../common/include

include $(BUILD_STATIC_LIBRARY)

ifeq ($(BUILD_AVC_TEST),true)
include $(CLEAR_VARS)
LOCAL_MODULE := avc_test
LOCAL_CFLAGS += -Wno-multichar -DAVC_TEST
LOCAL_ARM_MODE := arm
LOCAL_PRELINK_MODULE := false
LOCAL_STATIC_LIBRARIES := 
LOCAL_SHARED_LIBRARIES := libvpu libstagefright libcutils libutils
LOCAL_SRC_FILES := \
	src/h264decapi.cpp \
	src/h264hwd_asic.cpp \
	src/h264hwd_decoder.cpp \
	src/h264hwd_dpb.cpp \
	src/h264hwd_storage.cpp \
	src/h264hwd_byte_stream.cpp \
	src/h264hwd_nal_unit.cpp \
	src/h264hwd_pic_order_cnt.cpp \
	src/h264hwd_pic_param_set.cpp \
	src/h264hwd_seq_param_set.cpp \
	src/h264hwd_slice_group_map.cpp \
	src/h264hwd_slice_header.cpp \
	src/h264hwd_stream.cpp \
	src/h264hwd_util.cpp \
	src/h264hwd_vlc.cpp \
	src/h264hwd_vui.cpp \
	src/pv_avcdec_api.cpp \
	src/refbuffer.cpp \
	src/regdrv.cpp 
LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/src \
	$(LOCAL_PATH)/include \
	$(LOCAL_PATH)/../../common \
 	$(LOCAL_PATH)/../../common/include
LOCAL_MODULE_TAGS := optional
include $(BUILD_EXECUTABLE)
endif
