LOCAL_PATH = $(shell pwd)

TARGET = $(LOCAL_PATH)/libion_vpu.so

SRCS = \
	$(LOCAL_PATH)/ionalloc.c

OBJS = $(patsubst %.cpp, %.o, $(filter-out %.c, $(SRCS)))
OBJS += $(patsubst %.c, %.o, $(filter-out %.cpp, $(SRCS)))

CFLAGS = \
	-I$(LOCAL_PATH)/../../vpu_api/include \
	-fPIC \
	-shared \
	-fpermissive

LDFLAGS = \
	-L$(LOCAL_PATH)/../../libs

LD_SHARE_FLAGS = \
	-lpthread

LD_STATIC_FLAGS =

LDFLAGS += $(LD_SHARE_FLAGS) $(LD_STATIC_FLAGS)

all:$(TARGET)

%.o:%.c
	$(CC) $(CFLAGS) -c $^ -o $@

%.o:%.cpp
	$(CPP) $(CFLAGS) -c $^ -o $@

$(TARGET):$(OBJS)
	$(CC) $^ -o $@ $(CFLAGS) $(LDFLAGS)
	@mv $@ $(LOCAL_PATH)/../../libs

clean:
	@rm $(LOCAL_PATH)/*.o

.PHONY:all clean
