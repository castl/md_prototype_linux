#!/bin/sh

export CROSS_COMPILE=arm-linux-gnueabihf-
make ARCH=arm UIMAGE_LOADADDR=0x8000 uImage -j 25
