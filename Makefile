export WALLE_ROOT:=$(shell pwd)
export CROSS_COMPILE:=$(WALLE_ROOT)/tools/cross-compiler/gcc-linaro-arm-linux-gnueabihf-4.9/bin/arm-linux-gnueabihf-

$(warning WALLE_ROOT is: $(WALLE_ROOT))

.PHONY: all ubuntu-base lubuntu-core demo base hardware kernel clean distclean

base: kernel demo
	@tools/copy_files.sh $(WALLE_ROOT)

demo: hardware
	@mkdir -p output/rootfs/root/
	# VPU
	@make -C demo/vpu_api_demo
	@cp -rf demo/vpu_api_demo/testvpu output/rootfs/root/
	@cp -rf demo/vpu_api_demo/h264_320x240_coding_7_for_dec.bin output/rootfs/root/
	@cp -rf demo/vpu_api_demo/vpu_enc_input_yuv420sp_320x240.yuv output/rootfs/root/
	# RGA
	@make -C demo/rga_demo
	@cp -rf demo/rga_demo/testrga output/rootfs/root/
	@cp -rf demo/rga_demo/in_1920x1088_nv12.yuv output/rootfs/root/
	# CIF Camera
	@make -C demo/camera_demo
	@cp -rf demo/camera_demo/testcamera output/rootfs/root/

hardware:
	@mkdir -p output/rootfs/usr/lib/
	@make -C hardware/wifi
	@cp -rf hardware/gpu/* output/rootfs/
	@make -C hardware/libvpu
	@cp -rf hardware/libvpu/libs/* output/rootfs/usr/lib/

kernel:
	@tools/mk_kernel.sh $(WALLE_ROOT)

ubuntu-base:
	@tools/mk_system.sh ubuntu-base.img

lubuntu-core:
	@tools/mk_system.sh lubuntu-core.img

all: base ubuntu-base lubuntu-core

clean:
	@echo "cleaning..."
	@rm -rf output

distclean: clean
	@make -C demo/vpu_api_demo clean
	@make -C kernel distclean
