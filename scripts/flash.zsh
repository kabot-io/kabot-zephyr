#!/usr/bin/zsh

set -e

if [ ! -d /dev/serial/by-id ] || [ -z "$(ls -A /dev/serial/by-id 2>/dev/null)" ]; then
    echo "Error: no serial devices detected in /dev/serial/by-id"

    if [ -n "$WSL_DISTRO_NAME" ]; then
        echo "To attach a device to WSL:"
        echo "In Powershell (admin), run:"
        echo "winget install usbipd"
        echo "usbipd list"
        echo "usbipd bind --busid <id>"
        echo "usbipd attach --wsl --busid <id>"
        exit 1
    fi
fi

if ! groups | grep -qw dialout; then
    echo "Error: user '$USER' is not in the 'dialout' group"
    exit 1
fi

CURRENT_DIR=$(pwd)
cd $(git rev-parse --show-toplevel)

source .venv/bin/activate

west build app --build-dir build -b esp32s3_devkitc/esp32s3/procpu --pristine -- -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
west flash --build-dir build -r esp32 --skip-rebuild

cd $CURRENT_DIR
