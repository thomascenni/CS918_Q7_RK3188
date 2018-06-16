LOCAL_PATH = $(shell pwd)

TARGET = $(LOCAL_PATH)/libion.so

SRCS = \
	$(LOCAL_PATH)/ion.c

OBJS = $(patsubst %.cpp, %.o, $(filter-out %.c, $(SRCS)))
OBJS += $(patsubst %.c, %.o, $(filter-out %.cpp, $(SRCS)))

CFLAGS = \
	-fPIC \
	-shared \
	-fpermissive

LDFLAGS = \
	-L$(LOCAL_PATH)/../libs

LD_SHARE_FLAGS =

LD_STATIC_FLAGS =

LDFLAGS += $(LD_SHARE_FLAGS) $(LD_STATIC_FLAGS)

all:$(TARGET)

%.o:%.c
	$(CC) $(CFLAGS) -c $^ -o $@

%.o:%.cpp
	$(CPP) $(CFLAGS) -c $^ -o $@

$(TARGET):$(OBJS)
	$(CPP) $^ -o $@ $(CFLAGS) $(LDFLAGS)
	@mv $@ $(LOCAL_PATH)/../libs

clean:
	@rm $(LOCAL_PATH)/*.o

.PHONY:all clean
