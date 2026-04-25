#!/usr/bin/zsh

CURRENT_DIR=$(pwd)
cd $(git rev-parse --show-toplevel)

source $(find /opt/ros/*/setup.zsh)
source .venv/bin/activate

# west build app --build-dir build/native_sim -b native_sim -- -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
west build app --build-dir build/esp32s3_devkitc -b esp32s3_devkitc/esp32s3/procpu -- -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
[[ $? -eq 0 ]] || exit 1
if [[ "$*" != *"--no-flash"* ]]; then
    west flash --build-dir build/esp32s3_devkitc || exit 1
fi

cd $CURRENT_DIR
