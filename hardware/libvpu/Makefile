SYSROOT = /usr/arm-linux-gnueabihf/lib
CC = $(CROSS_COMPILE)gcc
CPP = $(CROSS_COMPILE)g++
AR = $(CROSS_COMPILE)ar
MAKE = make

LOCAL_PATH = $(shell pwd)

TARGET = $(LOCAL_PATH)/librk_codec.so

SRCS = \
	$(LOCAL_PATH)/vpu_api/src/vpu_api.cpp \
	$(LOCAL_PATH)/vpu_api/src/vpu_api_private.cpp \
	$(LOCAL_PATH)/common/ppOp.cpp \
	$(LOCAL_PATH)/common/rk_list.cpp \

OBJS = $(patsubst %.cpp, %.o, $(filter-out %.c, $(SRCS)))
OBJS += $(patsubst %.c, %.o, $(filter-out %.cpp, $(SRCS)))

CFLAGS = \
	-I$(LOCAL_PATH)/common \
	-I$(LOCAL_PATH)/common/include \
	-I$(LOCAL_PATH)/common/vpu_mem_pool \
	-I$(LOCAL_PATH)/vpu_api/include \
	-I$(LOCAL_PATH)/post_process/deinterlace/include \
	-I$(LOCAL_PATH)/jpeg_enc/src_enc/inc \
	-fPIC \
	-shared

LDFLAGS = \
	-L$(LOCAL_PATH)/libs

LD_SHARE_FLAGS = \
	-lvpu -lpthread -lion_vpu -lvpu_avcdec -lvpu_avcenc

LD_STATIC_FLAGS = \
	#$(LOCAL_PATH)/libs/libvpu_avcdec.a

LDFLAGS += $(LD_SHARE_FLAGS) $(LD_STATIC_FLAGS)

SUBDIRS =
EXCLUDEDIRS = \
	$(LOCAL_PATH)/jpeg_dec \
	$(LOCAL_PATH)/jpeg_enc \
	$(LOCAL_PATH)/libion \
	$(LOCAL_PATH)/m4v \
	$(LOCAL_PATH)/reg_gen \
	$(LOCAL_PATH)/vp8 \
	$(LOCAL_PATH)/vpu_api \
	$(LOCAL_PATH)/libs \
	$(LOCAL_PATH)/libion \
	$(LOCAL_PATH)/post_process \
	$(LOCAL_PATH)/vpu_api_demo \
	$(LOCAL_PATH)/si \
	$(LOCAL_PATH)/.git \
	$(LOCAL_PATH)
SUBDIRS_1 = $(shell find $(LOCAL_PATH) -maxdepth 1 -type d)
#SUBDIRS_2 = $(basename $(patsubst ./%,%,$(SUBDIRS_1)))
SUBDIRS += $(filter-out $(EXCLUDEDIRS),$(SUBDIRS_1))

export CC
export CPP
export AR
export SYSROOT
export MAKE

all:$(TARGET)

%.o:%.c
	$(CC) $(CFLAGS) -c $^ -o $@

%.o:%.cpp
	$(CPP) $(CFLAGS) -c $^ -o $@

$(TARGET):$(OBJS)
	@rm -rf libs
	@mkdir libs
	for dir in $(SUBDIRS);do $(MAKE) -C $$dir all || exit 1;done
	$(CPP) $^ -o $@ $(CFLAGS) $(LDFLAGS)
	@mv $@ $(LOCAL_PATH)/libs

install:
	@cp $(LOCAL_PATH)/libs/* /usr/lib

clean:
	@rm $(LOCAL_PATH)/libs/*
	@find . -name "*.o" | xargs rm

.PHONY:all clean
