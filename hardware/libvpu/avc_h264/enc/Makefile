TARGET = $(LOCAL_PATH)/libvpu_avcenc.so

SRCS = \
	$(LOCAL_PATH)/src/H264Cabac.c \
	$(LOCAL_PATH)/src/H264CodeFrame.c \
	$(LOCAL_PATH)/src/H264EncApi.c \
	$(LOCAL_PATH)/src/h264encapi_ext.c \
	$(LOCAL_PATH)/src/H264Init.c \
	$(LOCAL_PATH)/src/H264Mad.c \
	$(LOCAL_PATH)/src/H264NalUnit.c \
	$(LOCAL_PATH)/src/H264PictureParameterSet.c \
	$(LOCAL_PATH)/src/H264PutBits.c \
	$(LOCAL_PATH)/src/H264RateControl.c \
	$(LOCAL_PATH)/src/H264RateControl_new.c \
	$(LOCAL_PATH)/src/H264Sei.c \
	$(LOCAL_PATH)/src/H264SequenceParameterSet.c \
	$(LOCAL_PATH)/src/H264Slice.c \
	$(LOCAL_PATH)/src/H264TestId.c \
	$(LOCAL_PATH)/src/vidstabalg.c \
	$(LOCAL_PATH)/src/vidstabapi.c \
	$(LOCAL_PATH)/src/vidstabcommon.c \
	$(LOCAL_PATH)/src/vidstabinternal.c \
	$(LOCAL_PATH)/src/encasiccontroller.c \
	$(LOCAL_PATH)/src/encasiccontroller_v2.c \
	$(LOCAL_PATH)/src/encpreprocess.c \
	$(LOCAL_PATH)/src/pv_avcenc_api.cpp \

OBJS = $(patsubst %.cpp, %.o, $(filter-out %.c, $(SRCS)))
OBJS += $(patsubst %.c, %.o, $(filter-out %.cpp, $(SRCS)))

LOCAL_PATH = $(shell pwd)

CFLAGS = \
	-I$(LOCAL_PATH)/src \
	-I$(LOCAL_PATH)/include \
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
