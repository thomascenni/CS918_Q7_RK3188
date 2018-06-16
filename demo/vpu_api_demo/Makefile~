
CROSS_COMPILER=../../tools/cross-compiler/gcc-linaro-arm-linux-gnueabihf-4.8/bin/arm-linux-gnueabihf-
CC=$(CROSS_COMPILER)gcc
CFLAGS=-I./include -I../../output/rootfs/usr/local/include -I../../output/rootfs/include -I../../output/rootfs/usr/include
LIBS=-lstdc++ -lvpu -lvpu_avcdec -lion_vpu -lrk_codec -lvpu_avcenc
LDFLAGS=-L../../output/rootfs/usr/local/lib -L../../output/rootfs/lib -L../../output/rootfs/usr/lib
C_SRC = $(wildcard *.c)
C_OBJ = $(patsubst %c, %o, $(C_SRC))
EXEC_NAME = testvpu

.PHONY:all clean
all: $(EXEC_NAME)
%o: %c
	@$(CC) $(CFLAGS) -c $<
$(EXEC_NAME):$(C_OBJ)
	@$(CC) -o $@ $^ $(LIBS) $(LDFLAGS)
clean:
	@rm $(EXEC_NAME) *.o -f
