#!/usr/bin/zsh

west build app --build-dir build/native_sim -b native_sim
west build app --build-dir build/esp32s3_devkitc -b esp32s3_devkitc/esp32s3/appcpu
