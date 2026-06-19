#!/usr/bin/zsh
# Build script for ESP32-S3 DevKit C
#
# Usage:
#   ./build.zsh                 # Build, flash, and monitor
#   ./build.zsh --no-flash      # Build only, skip flashing
#   ./build.zsh --no-monitor    # Build and flash, skip monitoring
#   ./build.zsh --sim           # Build for native_sim instead of ESP32
#
# Options:
#   --no-flash    Skip flashing the device after build
#   --no-monitor  Skip launching serial monitor after flashing
#   --sim         Build for native_sim instead of esp32s3

if [[ "$*" == *"--help"* || "$*" == *"-h"* ]]; then
    cat << 'EOF'
Build script for ESP32-S3 DevKit C

Usage:
  ./build.zsh                 # Build, flash, and monitor
  ./build.zsh --no-flash      # Build only, skip flashing
  ./build.zsh --no-monitor    # Build and flash, skip monitoring
  ./build.zsh --sim           # Build for native_sim instead of ESP32
  ./build.zsh --help          # Show this help message

Options:
  --no-flash    Skip flashing the device after build
  --no-monitor  Skip launching serial monitor after flashing
  --sim         Build for native_sim instead of esp32s3
  --help, -h    Show this help message
EOF
    exit 0
fi

CURRENT_DIR=$(pwd)
cd "$(git rev-parse --show-toplevel)" || exit

# shellcheck source=/dev/null
source .venv/bin/activate

if [[ "$*" == *"--sim"* ]]; then
    west build app --build-dir build/native_sim -b native_sim -- -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
else
    west build app --build-dir build/esp32s3_devkitc -b esp32s3_devkitc/esp32s3/procpu -- -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    # shellcheck disable=SC2181
    [[ $? -eq 0 ]] || exit 1
    if [[ "$*" != *"--no-flash"* ]]; then
        west flash --build-dir build/esp32s3_devkitc || exit 1
        if [[ "$*" != *"--no-monitor"* ]]; then
            tio "${ESPTOOL_PORT}" -b 115200
        fi
    fi
fi

cd "$CURRENT_DIR" || exit
