#!/usr/bin/zsh

if [ ! -d ".venv" ]; then
    python3 -m venv .venv
fi
source .venv/bin/activate
pip install -U pip
pip install west
west packages pip --install
west sdk install --toolchain x86_64-zephyr-elf xtensa-espressif_esp32s3_zephyr-elf
west blobs fetch hal_espressif
west update
west zephyr-export
