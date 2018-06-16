#!/bin/bash

if [ "$1"x == ""x  ]; then
	echo "Please specify walle root path."
	exit
fi

WALLE_ROOT=$1
IMAGE=$WALLE_ROOT/output/image

echo "copying.."

mkdir -p $IMAGE
cp -rf $WALLE_ROOT/tools/common/RK3188Loader*.bin $IMAGE
cp -rf $WALLE_ROOT/tools/common/parameter* $IMAGE
cp -rf $WALLE_ROOT/tools/common/boot.img $IMAGE

cp -rf $WALLE_ROOT/kernel/kernel.img $IMAGE
echo "done."
