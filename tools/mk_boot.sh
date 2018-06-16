#!/bin/bash

echo "unsupport."
exit

# extra boot.img
dd if=boot.img of=boot.cpio.gz iflag=skip_bytes skip=8 && \
gzip -d boot.cpio.gz && \
mkdir boot && cd boot && \
cpio --no-absolute-filenames -idmv < ../boot.cpio && \
cd -

# arch boot.img
mkdir boot && cd boot && \
find . | cpio -o | gzip > ../boot.cpio.gz && \
./mkkrnlimg boot.cpio.gz boot.img

