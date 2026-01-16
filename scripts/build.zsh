#!/usr/bin/zsh

set -e

CURRENT_DIR=$(pwd)
cd $(git rev-parse --show-toplevel)

source $(find /opt/ros/*/setup.zsh)
source .venv/bin/activate

# west build app --build-dir build/native_sim -b native_sim -- -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
west build app --build-dir build/native_sim --board native_sim
west build app --build-dir build/kabot_esp32s3_procpu --board kabot/esp32s3/procpu --sysbuild

cd $CURRENT_DIR
