#!/usr/bin/zsh

set -e
setopt null_glob

if [ ! -d /dev/serial/by-id ] || [ -z "$(ls -A /dev/serial/by-id 2>/dev/null)" ]; then
    has_serial=false
    for dev in /dev/ttyACM* /dev/ttyUSB*; do
        if [ -e "$dev" ]; then
            has_serial=true
            break
        fi
    done
    if [ "$has_serial" = false ]; then
        echo "Error: no serial devices detected in /dev/serial/by-id or /dev/ttyACM* /dev/ttyUSB*"

    if [ -n "$WSL_DISTRO_NAME" ]; then
        echo "WSL2 devcontainer: attach the USB device to the WSL distro, then retry."
        echo "To attach a device to WSL, follow Microsoft documentation:"
        echo "https://learn.microsoft.com/en-us/windows/wsl/connect-usb"
        echo
        echo "In Powershell (admin), run:"
        echo "winget install usbipd"
        echo "usbipd list"
        echo "usbipd bind --busid <id> # id of the board from usbipd list"
        echo "usbipd attach --wsl --busid <id>"
    fi
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

west flash --build-dir build/esp32s3_procpu -r esp32 --no-rebuild

cd $CURRENT_DIR
