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
OUTPUT_IMG=$OUT/image/qt-ubuntu-base.img
MOUNT_DIR=$OUT/img_mount
UBUNTU_BASE_DIR=$WALLE_ROOT/firmware/ubuntu-base
UBUNTU_ARCHIVES=$UBUNTU_BASE_DIR/ubuntu-trusty-armhf-base.tgz
QT_BASE_DIR=$WALLE_ROOT/firmware/qt
QT_ARCHIVES=$QT_BASE_DIR/Qt-5.5.1.tgz

echo "Building qt-ubuntu-base"

mkdir -p $MOUNT_DIR
mkdir -p $OUT/image

echo "make empty image"
dd if=/dev/zero of=$OUTPUT_IMG bs=1M count=500
mkfs.ext4 -F $OUTPUT_IMG
sudo mount -o loop $OUTPUT_IMG $MOUNT_DIR
cd $MOUNT_DIR
sudo tar xvf $UBUNTU_ARCHIVES > /dev/null
sudo tar xvf $QT_ARCHIVES > /dev/null
sudo cp -rf $WALLE_ROOT/hardware/gpu/* $MOUNT_DIR/
cd -

# enable ttyS2 for debug console
sudo cp $UBUNTU_BASE_DIR/ttyS2.conf $MOUNT_DIR/etc/init/
sudo chmod 0644 $MOUNT_DIR/etc/init/ttyS2.conf
# detemine first boot and expand system partition.
sudo touch $MOUNT_DIR/firstboot
sudo cp $UBUNTU_BASE_DIR/mtd-by-name.sh $MOUNT_DIR/usr/local/bin/
sudo chmod 0755 $MOUNT_DIR/usr/local/bin/mtd-by-name.sh
sudo cp $UBUNTU_BASE_DIR/first-boot-recovery.sh $MOUNT_DIR/usr/local/bin/
sudo chmod 0755 $MOUNT_DIR/usr/local/bin/first-boot-recovery.sh
# for alsa
sudo cp $UBUNTU_BASE_DIR/tinymix $MOUNT_DIR/usr/local/bin/
sudo chmod 0755 $MOUNT_DIR/usr/local/bin/tinymix
sudo cp $UBUNTU_BASE_DIR/alsa-rt5640-enable.sh $MOUNT_DIR/usr/local/bin/
sudo chmod 0755 $MOUNT_DIR/usr/local/bin/alsa-rt5640-enable.sh

# install ubuntu-base deb
sudo cp $UBUNTU_BASE_DIR/deb/*.deb $MOUNT_DIR/var/cache/apt/archives/
sudo cp $UBUNTU_BASE_DIR/ubuntu-base-helper.sh $MOUNT_DIR/tmp/
sudo chmod 0755 $MOUNT_DIR/tmp/ubuntu-base-helper.sh
sudo chroot $MOUNT_DIR /bin/bash -c "/tmp/ubuntu-base-helper.sh"
sudo rm -rf $MOUNT_DIR/tmp/ubuntu-base-helper.sh

sudo umount -f -l $MOUNT_DIR && \
rm -rf $MOUNT_DIR && \

echo "Optimization image..."
e2fsck -fy $OUTPUT_IMG && \
resize2fs -p -M $OUTPUT_IMG && \
tune2fs -O dir_index,filetype,sparse_super -L system -c -1 -i 0 $OUTPUT_IMG && \
tune2fs -j $OUTPUT_IMG && \
e2fsck -fyD $OUTPUT_IMG
sync
echo "Done."
