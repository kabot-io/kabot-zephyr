#!/usr/bin/zsh
# Build script for ESP32-S3 DevKit C
#
# Usage:
#   ./build.zsh                 # Build, flash, and monitor
#   ./build.zsh --no-flash      # Build only, skip flashing
#   ./build.zsh --no-monitor    # Build and flash, skip monitoring
#   ./build.zsh --no-build      # Skip building the image and just flash the existing build
#   ./build.zsh --sim           # Build for native_sim instead of ESP32
#   ./build.zsh --pristine      # Pristine build before flashing
#   ./build.zsh --nuke          # Erase flash, pristine build, and full-chain flash
#
# Options:
#   --no-flash    Skip flashing the device after build
#   --no-monitor  Skip launching serial monitor after flashing
#   --no-build    Skip building the image and just flash the existing build
#   --sim         Build for native_sim instead of esp32s3
#   --pristine    Run a pristine build
#   --nuke        Erase flash, pristine build, and flash mcuboot + app

if [[ "$*" == *"--help"* || "$*" == *"-h"* ]]; then
    cat << 'EOF'
Build script for ESP32-S3 DevKit C

Usage:
  ./build.zsh                 # Build, flash, and monitor
  ./build.zsh --no-flash      # Build only, skip flashing
  ./build.zsh --no-monitor    # Build and flash, skip monitoring
  ./build.zsh --no-build      # Skip building the image and just flash the existing build
  ./build.zsh --sim           # Build for native_sim instead of ESP32
  ./build.zsh --pristine      # Pristine build before flashing
  ./build.zsh --nuke          # Erase flash, pristine build, and full-chain flash
  ./build.zsh --help          # Show this help message

Options:
  --no-flash    Skip flashing the device after build
  --no-monitor  Skip launching serial monitor after flashing
  --no-build    Skip building the image and just flash the existing build
  --sim         Build for native_sim instead of esp32s3
  --pristine    Run a pristine build
  --nuke        Erase flash, pristine build, and flash mcuboot + app
  --help, -h    Show this help message
EOF
    exit 0
fi

CURRENT_DIR=$(pwd)
cd "$(git rev-parse --show-toplevel)" || exit

# shellcheck source=/dev/null
source .venv/bin/activate

FLASH_PORT="${ESPTOOL_PORT:-/dev/ttyACM1}"
APP_VERSION_FILE="$PWD/app/VERSION"

PRISTINE_ARGS=()
if [[ "$*" == *"--pristine"* ]]; then
    PRISTINE_ARGS=(--pristine=always)
fi

if [[ "$*" == *"--nuke"* ]]; then
    PRISTINE_ARGS=(--pristine=always)
fi

if [[ "$*" == *"--sim"* ]]; then
    if [[ "$*" == *"--nuke"* ]]; then
        echo "Error: --nuke is only supported for ESP32-S3 builds (not --sim)."
        exit 1
    fi
    python3 scripts/firmware_version.py --version-file "$APP_VERSION_FILE" || exit 1
    west build "${PRISTINE_ARGS[@]}" app --build-dir build/native_sim -b native_sim -- -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
else
    if [[ "$*" == *"--nuke"* ]]; then
        python3 scripts/firmware_version.py --version-file "$APP_VERSION_FILE" || exit 1
        west build --sysbuild "${PRISTINE_ARGS[@]}" app --build-dir build/esp32s3_devkitc -b esp32s3_devkitc/esp32s3/procpu -- -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
        # shellcheck disable=SC2181
        [[ $? -eq 0 ]] || exit 1

        MCUBOOT_BIN="build/esp32s3_devkitc/mcuboot/zephyr/zephyr.bin"
        APP_SIGNED_BIN="build/esp32s3_devkitc/app/zephyr/zephyr.signed.bin"

        [[ -f "$MCUBOOT_BIN" ]] || {
            echo "Error: MCUboot image not found at $MCUBOOT_BIN"
            exit 1
        }
        [[ -f "$APP_SIGNED_BIN" ]] || {
            echo "Error: Signed app image not found at $APP_SIGNED_BIN"
            exit 1
        }

        python -m esptool \
          --port "$FLASH_PORT" \
          --before default-reset \
          --after hard-reset \
          erase-flash || exit 1

        python -m esptool \
          --port "$FLASH_PORT" \
          --baud 921600 \
          --before default-reset \
          --after hard-reset \
          write-flash -u \
          --flash-mode dio \
          --flash-freq 80m \
          --flash-size 16MB \
          0x0 "$MCUBOOT_BIN" \
          0x20000 "$APP_SIGNED_BIN" || exit 1

        if [[ "$*" != *"--no-monitor"* ]]; then
            tio "$FLASH_PORT" -b 115200
        fi
    else
        if [[ "$*" != *"--no-build"* ]]; then
            python3 scripts/firmware_version.py --version-file "$APP_VERSION_FILE" || exit 1
            west build --sysbuild "${PRISTINE_ARGS[@]}" app --build-dir build/esp32s3_devkitc -b esp32s3_devkitc/esp32s3/procpu -- -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
            # shellcheck disable=SC2181
            [[ $? -eq 0 ]] || exit 1
        fi
        if [[ "$*" != *"--no-flash"* ]]; then
            west flash --build-dir build/esp32s3_devkitc || exit 1
            if [[ "$*" != *"--no-monitor"* ]]; then
                tio "$FLASH_PORT" -b 115200
            fi
        fi
    fi
fi

cd "$CURRENT_DIR" || exit
