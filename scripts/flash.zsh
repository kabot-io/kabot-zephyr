#!/usr/bin/zsh

set -e

ESP32_DEV_NAME="ttyACM0"

if [ ! -e "/dev/$ESP32_DEV_NAME" ]; then
    echo "Error: no USB devices attached to WSL"
    echo ""
    echo "on Powershell (admin), run:"
    echo "winget install usbipd"
    echo "usbipd list"
    echo "usbipd bind --busid <id>"
    echo "usbipd attach --wsl --busid <id>"
    exit 1
fi

if ! groups "$USER" | grep -qw dialout; then
    echo "Error: user '$USER' is not in the 'dialout' group"
    echo ""
    echo "in the devcontainer, run:"
    echo "sudo usermod -aG dialout vscode"
    echo "then, in powershell, run:"
    echo "wsl --shutdown"
    exit 1
fi

CURRENT_DIR=$(pwd)
cd $(git rev-parse --show-toplevel)

source .venv/bin/activate

west build app --build-dir build -b esp32s3_devkitc/esp32s3/procpu --pristine -- -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
west flash --build-dir build -r esp32 --skip-rebuild

cd $CURRENT_DIR
