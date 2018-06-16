LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := ionalloc.c IonAlloc.cpp
LOCAL_SHARED_LIBRARIES := liblog libutils 
LOCAL_MODULE := libion
LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS:= -DLOG_TAG=\"Ionalloc\"
include $(BUILD_SHARED_LIBRARY)

