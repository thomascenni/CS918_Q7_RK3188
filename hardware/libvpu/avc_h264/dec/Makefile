TARGET = $(LOCAL_PATH)/libvpu_avcdec.so

SRCS = \
	$(LOCAL_PATH)/src/h264decapi.cpp \
	$(LOCAL_PATH)/src/h264hwd_asic.cpp \
	$(LOCAL_PATH)/src/h264hwd_decoder.cpp \
	$(LOCAL_PATH)/src/h264hwd_dpb.cpp \
	$(LOCAL_PATH)/src/h264hwd_storage.cpp \
	$(LOCAL_PATH)/src/h264hwd_byte_stream.cpp \
	$(LOCAL_PATH)/src/h264hwd_nal_unit.cpp \
	$(LOCAL_PATH)/src/h264hwd_pic_order_cnt.cpp \
	$(LOCAL_PATH)/src/h264hwd_pic_param_set.cpp \
	$(LOCAL_PATH)/src/h264hwd_seq_param_set.cpp \
	$(LOCAL_PATH)/src/h264hwd_slice_group_map.cpp \
	$(LOCAL_PATH)/src/h264hwd_slice_header.cpp \
	$(LOCAL_PATH)/src/h264hwd_stream.cpp \
	$(LOCAL_PATH)/src/h264hwd_util.cpp \
	$(LOCAL_PATH)/src/h264hwd_vlc.cpp \
	$(LOCAL_PATH)/src/h264hwd_vui.cpp \
	$(LOCAL_PATH)/src/pv_avcdec_api.cpp \
	$(LOCAL_PATH)/src/refbuffer.cpp \
	$(LOCAL_PATH)/src/regdrv.cpp 

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
