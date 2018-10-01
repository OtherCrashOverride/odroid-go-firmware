#!/bin/bash
TITLE="odroid-go-firmware-$(date +%Y%m%d).img"
./mkimg "$TITLE" 0x1000 ../../build/bootloader/bootloader.bin 0x10000 ../../build/odroid-go-firmware.bin 0x8000 ../../build/partitions.bin 0xf000 ../../build/phy_init_data.bin
