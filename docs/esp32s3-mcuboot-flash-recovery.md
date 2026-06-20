# ESP32-S3 MCUboot Flash Recovery

See also:

- [README.md](../README.md)
- [README.md](README.md)
- [firmware-data-flow.md](firmware-data-flow.md)
- [scripts/build.zsh](../scripts/build.zsh)

## Symptom

After flashing an ESP32-S3 build, the board may reset without starting the expected firmware. The low-level ROM UART, usually on /dev/ttyACM0 can show output similar to:

```text
ESP-ROM:esp32s3-20210327
rst:0x1 (POWERON),boot:0xb (SPI_FAST_FLASH_BOOT)
SHA-256 comparison failed:
Attempting to boot anyway...
```

The application console, usually on `/dev/ttyACM1` will stay silent.

## Cause

This branch changed the ESP32-S3 boot layout when [app/sysbuild.conf](../app/sysbuild.conf) enabled MCUboot with sysbuild (Zephyr multi-image build: bootloader + app). Before that change, boards could have been flashed with an application image that did not include MCUboot. After the change, the expected layout is:

```text
0x000000  MCUboot ESP boot image
0x010000  sys/settings area
0x020000  image-0 / primary signed application slot
0x5f0000  image-1 / secondary signed application slot
0xfe0000  scratch slot
```

The layout change was introduced on this branch by commit `dbec512` (`feat(firmware): move esp32s3 flash layout to config files`), which added `SB_CONFIG_BOOTLOADER_MCUBOOT=y`, app-side `CONFIG_BOOTLOADER_MCUBOOT=y`, and the 16 MB MCUboot partition layout.

If a board still contains an older non-MCUboot image or stale data at low flash offsets, flashing only the application slot at `0x20000` is not enough. The ESP ROM starts from the boot image at the beginning of flash, so stale content at `0x0` can remain in control even if the new signed application was written successfully.

This is a flash layout migration issue, not a RAM pressure issue.

## Recovery

Do a full flash erase once, then flash the full boot chain: MCUboot at `0x0` and the signed application at `0x20000`.

Fast path with the build script:

```bash
./scripts/build.zsh --nuke --no-monitor
```

Equivalent manual steps:

```bash
cd /workspaces/kabot-zephyr
. .venv/bin/activate

python -m esptool \
  --port /dev/ttyACM1 \
  --before default-reset \
  --after hard-reset \
  erase-flash
```

Rebuild the sysbuild artifacts:

```bash
./scripts/build.zsh --no-flash
```

Flash the full image set:

```bash
python -m esptool \
  --port /dev/ttyACM1 \
  --baud 921600 \
  --before default-reset \
  --after hard-reset \
  write-flash -u \
  --flash-mode dio \
  --flash-freq 80m \
  --flash-size 16MB \
  0x0 build/esp32s3_devkitc/mcuboot/zephyr/zephyr.bin \
  0x20000 build/esp32s3_devkitc/app/zephyr/zephyr.signed.bin
```

In this repository layout there is no separate generated ESP partition-table binary to flash; the fixed partition layout is compiled from devicetree into the MCUboot and application images.

## Verification

Use `/dev/ttyACM0` for low-level ROM logs and `/dev/ttyACM1` for the Zephyr shell/application console. Avoid keeping two tools attached to the same port.

```bash
tio -b 115200 /dev/ttyACM1
```

The recovered firmware should print Zephyr logs and eventually show the shell prompt:

```text
uart:~$
```

At that point, `help` should list shell commands.

## Prevention

When migrating a board from a non-MCUboot image to this MCUboot/sysbuild layout, do a full erase and full image-set flash once. After the board is running MCUboot, normal OTA updates can use [build/esp32s3_devkitc/app/zephyr/zephyr.signed.bin](../build/esp32s3_devkitc/app/zephyr/zephyr.signed.bin) through MCUmgr.
