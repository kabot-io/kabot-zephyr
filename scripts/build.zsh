#!/usr/bin/zsh

CURRENT_DIR=$(pwd)
cd $(git rev-parse --show-toplevel)

west build app --build-dir build/native_sim -b native_sim -- -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

west build app --build-dir build/esp32s3_devkitc -b esp32s3_devkitc/esp32s3/appcpu -- -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

cd $CURRENT_DIR