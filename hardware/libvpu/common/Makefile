LOCAL_PATH = $(shell pwd)

TARGET = $(LOCAL_PATH)/libvpu.so

SRCS = \
	$(LOCAL_PATH)/vpu_mem.c \
	$(LOCAL_PATH)/vpu.c \
	$(LOCAL_PATH)/vpu_mem_pool/tsemaphore.c

OBJS = $(patsubst %.cpp, %.o, $(filter-out %.c, $(SRCS)))
OBJS += $(patsubst %.c, %.o, $(filter-out %.cpp, $(SRCS)))

CFLAGS = \
	-I$(LOCAL_PATH) \
	-I$(LOCAL_PATH)/../vpu_api/include \
	-I$(LOCAL_PATH)/libion \
	-I$(LOCAL_PATH)/include \
	-I$(LOCAL_PATH)/libvpu_mem_pool \
	-I$(LOCAL_PATH)/../post_process/deinterlace/include \
	-fPIC \
	-shared

LDFLAGS = \
	-L$(LOCAL_PATH)/../libs

LD_SHARE_FLAGS = \
	-lion_vpu -lpthread

LD_STATIC_FLAGS =

LDFLAGS += $(LD_SHARE_FLAGS) $(LD_STATIC_FLAGS)

SUBDIRS = $(LOCAL_PATH)/libion
EXCLUDEDIRS = $(LOCAL_PATH)/vpu_mem_pool $(LOCAL_PATH)/include $(LOCAL_PATH)/libion_vpu $(LOCAL_PATH)
SUBDIRS_1 = $(shell find $(LOCAL_PATH) -maxdepth 1 -type d)
#SUBDIRS_2 = $(basename $(patsubst ./%,%,$(SUBDIRS_1)))
SUBDIRS += $(filter-out $(EXCLUDEDIRS),$(SUBDIRS_1))

all:$(TARGET)

%.o:%.c
	$(CC) $(CFLAGS) -c $^ -o $@

%.o:%.cpp
	$(CPP) $(CFLAGS) -c $^ -o $@
	
#$(OBJS):$(SRCS)
#	$(CC) $(CFLAGS) -c $^ -o $@

$(TARGET):$(OBJS)
	for dir in $(SUBDIRS);do $(MAKE) -C $$dir all && echo "finish" || exit 1;done
	$(CC) $^ -o $@ $(CFLAGS) $(LDFLAGS)
	mv $@ $(LOCAL_PATH)/../libs

clean:
	@for dir in $(SUBDIRS);do $(MAKE) -C $$dir clean || exit 1;done
	@rm *.o $(LOCAL_PATH)/vpu_mem_pool/*.o

.PHONY:all clean
