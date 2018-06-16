LOCAL_PATH = $(shell pwd)

TARGET = $(LOCAL_PATH)/libpost_deinterlace.a

SRCS = \
	$(LOCAL_PATH)/src/deinter.cpp

OBJS = $(patsubst %.cpp, %.o, $(filter-out %.c, $(SRCS)))
OBJS += $(patsubst %.c, %.o, $(filter-out %.cpp, $(SRCS)))

CFLAGS = \
	-I$(LOCAL_PATH)/../../common \
	-I$(LOCAL_PATH)/../../common/include \
	-I$(LOCAL_PATH)/../../common/vpu_mem_pool \
	-I$(LOCAL_PATH)/../../vpu_api/include \
	-I$(LOCAL_PATH)/../../jpeg_enc/src_enc/inc \
	-I$(LOCAL_PATH)/include

LDFLAGS = \
	-L$(LOCAL_PATH)/../../libs

LD_SHARE_FLAGS =

LD_STATIC_FLAGS =

LDFLAGS += $(LD_SHARE_FLAGS) $(LD_STATIC_FLAGS)

SUBDIRS =
EXCLUDEDIRS = $(LOCAL_PATH)
SUBDIRS_1 = $(shell find $(LOCAL_PATH) -maxdepth 1 -type d)
#SUBDIRS_2 = $(basename $(patsubst ./%,%,$(SUBDIRS_1)))
SUBDIRS += $(filter-out $(EXCLUDEDIRS),$(SUBDIRS_1))

all:$(TARGET)

%.o:%.c
	$(CC) $(CFLAGS) -c $^ -o $@

%.o:%.cpp
	$(CPP) $(CFLAGS) -c $^ -o $@

$(TARGET):$(OBJS)
	for dir in $(SUBDIRS);do $(MAKE) -C $$dir all || exit 1;done
	$(CPP) $^ -o $@ $(LDFLAGS)
	@mv $@ $(LOCAL_PATH)/../../libs

clean:
	@for dir in $(SUBDIRS);do $(MAKE) -C $$dir clean || exit 1;done
	@rm *.o

.PHONY:all clean
