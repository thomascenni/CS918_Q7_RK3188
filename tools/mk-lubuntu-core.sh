#!/bin/bash

if [ "$WALLE_ROOT"x == ""x  ]; then
         WALLE_ROOT="`pwd`/.."
fi

echo "WALLE_ROOT=$WALLE_ROOT"

if [ ! -d $WALLE_ROOT/firmware ] && [ ! -d $WALLE_ROOT/kernel ]
then
	echo "Invalid walle root: $WALLE_ROOT"
	exit
fi

OUT=$WALLE_ROOT/output
SYSTEM_IMG=$OUT/image/lubuntu-core.img
UBUNTU_BASE_IMG=$OUT/image/ubuntu-base.img
MOUNT_DIR=$OUT/img_mount
UBUNTU_BASE_DIR=$WALLE_ROOT/firmware/lubuntu-core

echo "Building lubuntu-core"

if [ ! -f $UBUNTU_BASE_IMG ]; then
	echo "ubuntu-base image isn't exist, build it first."
	$WALLE_ROOT/tools/mk-ubuntu-base.sh
fi
rm -rf $SYSTEM_IMG
cp $UBUNTU_BASE_IMG $SYSTEM_IMG

mkdir -p $OUT/img_mount
mkdir -p $OUT/image

# block size
block_size=`tune2fs -l $SYSTEM_IMG | grep "Block size:" | awk '{print $3;}'`
echo "Block size: $block_size"

#let delta=128*1024*1024/$block_size
delta=$((1024*1024*1024/$block_size))
echo "delta is $delta"

# minimal system block
block_num=`resize2fs -P $SYSTEM_IMG 2>&1 | tail -n1 | awk '{print $7;}'`
echo "Block min num: $block_num"
block_num=$(($block_num + $delta))

echo "New system blocks: $block_num"
e2fsck -fy $SYSTEM_IMG
resize2fs $SYSTEM_IMG $block_num

sudo mount -o loop $SYSTEM_IMG $OUT/img_mount

sudo cp $UBUNTU_BASE_DIR/deb/*.deb $MOUNT_DIR/var/cache/apt/archives/

sudo cp $UBUNTU_BASE_DIR/lubuntu-core-helper.sh $MOUNT_DIR/tmp/
sudo chmod 0755 $MOUNT_DIR/tmp/lubuntu-core-helper.sh
sudo chroot $MOUNT_DIR /bin/bash -c "/tmp/lubuntu-core-helper.sh"
sudo rm -rf $MOUNT_DIR/tmp/lubuntu-core-helper.sh

sudo umount -f -l $OUT/img_mount && \
rm -rf $OUT/img_mount

echo "Optimization system..."
e2fsck -fy $SYSTEM_IMG && \
resize2fs -p -M $SYSTEM_IMG && \
tune2fs -O dir_index,filetype,sparse_super -L system -c -1 -i 0 $SYSTEM_IMG && \
tune2fs -j $SYSTEM_IMG && \
e2fsck -fyD $SYSTEM_IMG
sync
echo "Done."

