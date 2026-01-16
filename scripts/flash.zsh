#!/usr/bin/zsh

set -e

if [ ! -d ".venv" ]; then
    python3 -m venv .venv
fi
source .venv/bin/activate

west flash --build-dir build/kabot_esp32s3_procpu

tio $ESPTOOL_PORT
