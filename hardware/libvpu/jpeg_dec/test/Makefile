LOCAL_PATH = $(shell pwd)

TARGET = $(LOCAL_PATH)/libvpu_mjpegdec.so

LOCAL_TOP_DIR := $(LOCAL_PATH)/../
LOCAL_SRC_DIR := $(LOCAL_PATH)/../dec
LOCAL_TST_DIR := $(LOCAL_PATH)/../test

SRCS = \
	$(LOCAL_PATH)/../dec/jpeg/jpegdecapi.c \
	$(LOCAL_PATH)/../dec/jpeg/jpegdechdrs.c \
	$(LOCAL_PATH)/../dec/jpeg/jpegdecinternal.c \
	$(LOCAL_PATH)/../dec/jpeg/jpegdecscan.c \
	$(LOCAL_PATH)/../dec/jpeg/jpegdecutils.c \
	$(LOCAL_PATH)/../dec/jpeg/dwl_test.c      \
	$(LOCAL_PATH)/hw_jpegdecapi.c \
	$(LOCAL_PATH)/pv_jpegdec_api.cpp

OBJS = $(patsubst %.cpp, %.o, $(filter-out %.c, $(SRCS)))
OBJS += $(patsubst %.c, %.o, $(filter-out %.cpp, $(SRCS)))

CFLAGS = \
	-I$(LOCAL_PATH) \
	-I$(LOCAL_VPU_DIR) \
	-I$(LOCAL_INC_DIR) \
	-I$(LOCAL_SRC_DIR) \
	-I$(LOCAL_SRC_DIR)/config \
	-I$(LOCAL_SRC_DIR)/inc \
	-I$(LOCAL_SRC_DIR)/jpeg \
	-I$(LOCAL_TST_DIR) \
	-I$(LOCAL_PATH)/../../common \
	-I$(LOCAL_PATH)/../../vpu_api/include \
	-I$(LOCAL_PATH)/../../common/include \
	-I$(LOCAL_PATH)/../../post_process/deinterlace/include \
	-fPIC \
	-shared \

LDFLAGS = \

all:$(TARGET)

%.o:%.c
	$(CC) $(CFLAGS) -c $^ -o $@

%.o:%.cpp
	$(CPP) $(CFLAGS) -c $^ -o $@

$(TARGET):$(OBJS)
	$(CPP) $^ -o $@ $(CFLAGS) $(LDFLAGS)
	#$(AR) -r $@ $^ $(LDFLAGS)
	mv $@ $(LOCAL_PATH)/../../libs

clean:
	@rm $(LOCAL_PATH)/src/*.o

.PHONY:all clean
