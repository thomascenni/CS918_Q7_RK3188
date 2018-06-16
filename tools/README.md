Rockchip Walle SDK
======

It's an ubuntu base SDK, now it support RK3188 chip.

### Download source code
```
$ repo init -u https://github.com/rockchip-linux/rk3188-manifests
$ repo sync -j3
```

### Source tree
```
walle
├── demo
│   ├── vpu_api_demo
│   ├── camera_demo
│   └── rga_demo
├── firmware
│   ├── lubuntu-core
│   └── ubuntu-base
├── hardware
│   ├── gpu
│   ├── libvpu
│   ├── touchscreen
│   └── wifi
├── kernel
├── output
│   ├── image
│   └── rootfs
└── tools
    ├── common
    ├── cross-compiler
    └── DFU
```

### Environment configuration
You need install some packages before building.  
Ubuntu 14.04:
```
$ sudo apt-get install git-core gnupg flex bison gperf build-essential \
				zip curl zlib1g-dev gcc-multilib g++-multilib libc6-dev-i386 \
				lib32ncurses5-dev x11proto-core-dev libx11-dev lib32z-dev ccache \
				libgl1-mesa-dev libxml2-utils xsltproc unzip qemu-user-static \
				qemu qemu-system
```

### Build
* $ make all  
    use for first build
* $ make base  
    build kernel/demo/hardware
* $ make kernel  
    just build kernel
* $ make ubuntu-base  
    just build ubuntu-base image
* $ make lubuntu-core  
    just build lubuntu-core image

### Flash Image
```
$ sudo ./tools/DFU/linux/upgrade_tool ul output/image/RK3188Loader(L)_V2.19.bin
$ sudo ./tools/DFU/linux/upgrade_tool di -p output/image/parameter
$ sudo ./tools/DFU/linux/upgrade_tool di -k output/image/kernel.img
$ sudo ./tools/DFU/linux/upgrade_tool di -b output/image/boot.img
$ sudo ./tools/DFU/linux/upgrade_tool di -s output/image/lubuntu-core.img
$ sudo ./tools/DFU/linux/upgrade_tool rd
```
If you use eMMC storage, you need flash parameter.emmc:
```
$ sudo ./tools/DFU/linux/upgrade_tool di -p output/image/parameter.emmc
```

### Demo
* #### GPU
  glmark is a benchmark for GPU， you can use it in lubuntu-core firmware, open an terminal and input:
  ```
  sudo glmark2-es2
  ```

* #### Video Decode&Encode
  We provide testvpu tools for H264 encode & decode, it's code in demo/vpu_api_demo/。
	* Video Decode  
		Input H264 data, output yuv(nv12) data：
		```
    sudo /root/testvpu -i ./h264_320x240_coding_7_for_dec.bin -o ./test.yuv -w 320 -h 240 -coding 7 -vframes 60
    ```
    Note: at the beginning of each frame, 4 bytes are used to indicate the length of the frame.
	* Video Encode  
		Input yuv(nv12) data, output H264 data:
    ```
    sudo /root/testvpu -t 2 -i ./vpu_enc_input_yuv420sp_320x240.yuv -o ./test.bin -w 320 -h 240 -coding 7
    ```

* #### Camera
  * UVC Camera
    We use luvcview tools for camera preview:
      1. Plug-in  USB Camera， we will find a video device at /dev/，like /dev/video1.
      2. run command:
      ```
      luvcview -d /dev/video1 -f yuv -s 320x240
      ```
  * CIF Camera
    We provide testcamera for cif camera:
    ```
    sudo /root/testcamera
    ```
    It will open cif camera and output data to camera_640x480_nv12.yuv

* #### RGA
  We provide testrga for image scale and codec translate：
  ```
  sudo /root/testrga
  ```
  Input NV12 file: in_1920x1088_nv12.yuv, output ABGR file: out_1920x1080_ABGR.yuv。

* #### Audio
  We use alsa tools for audio control.
  playback test:
  ```
  aplay /usr/share/sounds/alsa/Front_Center.wav
  ```
  record test:
  ```
  arecord -f cd -d 10 -t wav /tmp/test.wav
  aplay /tmp/test.wav
  ```
  play mp3 file：
  ```
  play sample.mp3
  ```

* #### Ethernet
  We use ifconfig to turn on/off ethernet.
  ```
  ifconfig eth0 down
  ifconfig eth0 up
  dhclient eth0
  ```

* #### WIFI
  Use wpa_cli application to control wifi
  AP Scanning:
  ```
  wpa_cli scan
  sleep 3
  wpa_cli scan_result
  ```
  Add network:
  ```
  wpa_cli add_network
  wpa_cli set_network 0 ssid \"Rockchip-XXX\"
  wpa_cli set_network 0 psk \"XXXXXXXX\"
  ```
  Delete network:
  ```
  wpa_cli remove_network 0
  ```
  Select network:
  ```
  wpa_cli select_network 0
  dhclient wlan0
  ```
  Enable network
  ```
  enable_network 0
  ```
  Disable network
  ```
  disable_network 0
  ```
  You can write the network info to /etc/wpa_supplicant.conf for auto connect. like:
  ```
  network={
    ssid="AP name"
    psk="Password"
  }
  ```

* #### backlight contorl
  Manually set backlight
  ```
  sudo echo 60 > /sys/class/backlight/rk28_bl/brightness
  ```
  Auto set backlight： add below command line to /etc/rc.local file
  ```
  echo 60 > /sys/class/backlight/rk28_bl/brightness
  ```

* #### SD Card and USB disk.
  Plug-in SD card and mount it:
  ```
  mount /dev/mmcblk1p1 /mnt
  ```
  Plug-in USB disk and mount it:
  ```
  mount /dev/sda1 /mnt
  ```
  umount SD card or USB disk:
  ```
  umount /mnt
  ```
