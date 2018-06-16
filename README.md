# Introduction
An Ubuntu firmware for CS918 Smart TV Box (based on RockChip RK3188).

This work is a modification of the [Rockchip Walle SDK](https://github.com/rockchip-linux/rk3188-manifests) with these fixes:

1. Correction of CROSS_COMPILER and CROSS_COMPILE path inside different Makefile.
2. Disable of the touchscreen driver (not available on CS918).

