TOP_DIR = $(shell pwd)

# export CROSS_COMPILER := ../../tools/cross-compiler/gcc-linaro-arm-linux-gnueabihf-4.8/bin/arm-linux-gnueabihf-
CC     := $(CROSS_COMPILER)gcc
CXX    := $(CROSS_COMPILER)g++
AR     := $(CROSS_COMPILER)ar
AS     := $(CROSS_COMPILER)as
LD     := $(CROSS_COMPILER)ld
RANLIB := $(CROSS_COMPILER)ranlib
STRIP  := $(CROSS_COMPILER)strip

# define the source code
#SRC_CODE_C = $(wildcard *.c)
#SRC_CODE_CPP = $(wildcard *.cpp)

#SRC_CODE_C = \
#	main.c

SRC_CODE_CPP = \
	rgaProcess.cpp \
	rgaTest.cpp
		
INCLUDE = -I. -I$(TOP_DIR) -I$(TOP_DIR)/include

# define generation

#OBJ_C = $(SRC_CODE_C:.c=.o)
#OBJ_CPP = $(SRC_CODE_CPP:.cpp=.o)

#LIB_NAME = libvpu_avcdec.so

EXC_NAME = testrga

# define the build flag
CFLAGS += $(INCLUDE)
CFLAGS += -fpermissive
#CFLAGS += -fPIC
#LDFLAGS

SHARED_LIB += -L. -L../../output/rootfs/usr/local/lib -L../../output/rootfs/lib -L../../output/rootfs/usr/lib
SHARED_LIB += -lion_vpu
#SHARED_LIB +=  -lvpu -lrk_on2 -lpost_deinterlace -lvpu_avcdec -lion

# build steps

.PHONY : all all_src install clean

all : $(OBJ_C) $(OBJ_CPP)
#	$(CC) $(CFLAGS) $^ -shared -fPIC -o $(LIB_NAME)
	$(CXX) $(CFLAGS) $(SRC_CODE_CPP) $(SHARED_LIB)  -o  $(EXC_NAME)
#	$(CC) $(CFLAGS) $(SRC_CODE_C) $(SHARED_LIB)  -o  $(EXC_NAME)

$(OBJ_C) : %.o : %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_CPP) : %.o : %.cpp
	$(CXX) $(CFLAGS) -c $< -o $@

#all_src :
#$(OBJ_CPP) : %.o : %.cpp
#	$(CXX) $(CFLAGS) -c $< -o $@

clean :
	@rm -rf $(OBJ_C) $(OBJ_CPP) $(EXC_NAME)

