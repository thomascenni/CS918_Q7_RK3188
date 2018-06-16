#!/bin/bash

WALLE_ROOT=".."
if [ "$1"x != ""x  ]; then
         WALLE_ROOT=$1
fi

if [ ! -d $WALLE_ROOT/kernel ]
then
	echo "Invalid walle root: $WALLE_ROOT"
	exit
fi

echo "WALLE_ROOT=$WALLE_ROOT"
OUT=$WALLE_ROOT/output
KERNEL=$WALLE_ROOT/kernel

echo "Building kernel"
cd $KERNEL
make rk3188_ubuntu_defconfig && make kernel.img -j3
echo "Done."
cd -
