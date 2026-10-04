# kabot-zephyr

This repository is configured to use a `.devcontainer` for development. To ensure a consistent and reliable environment, you must open this repository using Visual Studio Code with the Dev Containers extension.

## Getting Started

1. **Open the Repository in VS Code**:
    Make sure you have the [Dev Containers extension](https://marketplace.visualstudio.com/items?itemName=ms-vscode-remote.remote-containers) installed. Open this repository in VS Code, and it will automatically set up the development container.

2. **Build Firmware**:
    Once the development container is ready, use the project build script:

    ```zsh
    ./scripts/build.zsh
    ```

    Default behavior builds the ESP32-S3 firmware with sysbuild (application + MCUboot),
    flashes it, and opens monitor output.

    In this repository, sysbuild means a single multi-image Zephyr build that
    produces both the MCUboot bootloader image and the application image.

    Useful options:

    ```zsh
    ./scripts/build.zsh --no-flash
    ./scripts/build.zsh --no-monitor
    ./scripts/build.zsh --no-build
    ./scripts/build.zsh --pristine
    ./scripts/build.zsh --nuke --no-monitor
    ./scripts/build.zsh --sim
    ```

    `--nuke` is intended for ESP32-S3 recovery/migration flows and performs:
    erase flash -> pristine sysbuild build -> full-chain flash (MCUboot + signed app).

3. **SMP / OTA (ESP32-S3 target)**:
    The current ESP32-S3 Zenbedded profile disables MCUmgr SMP/UDP OTA to fit
    memory. Update it through the serial build/flash script; MCUboot and signed
    application images remain enabled.

    Signed application image artifact:

    ```text
    build/esp32s3_devkitc/app/zephyr/zephyr.signed.bin
    ```

    CI uploads this signed image as a workflow artifact. Pushing a SemVer tag
    creates a GitHub release with conventional-changelog notes and attaches the
    signed image as a release asset. Build scripts generate `app/VERSION` from the
    latest SemVer tag. `VERSION_TWEAK` is the commit distance from that tag and
    `EXTRAVERSION` is the sanitized branch name (omitted on the default branch).
    Zephyr automatically uses `APP_VERSION_TWEAK_STRING` for MCUboot signing
    version.

## Requirements

- Visual Studio Code
- Dev Containers extension

## Zenbedded Dependency

Kabot's West manifest fetches Zenbedded and its Zenoh-pico dependency. The
library's public RCL header is `<zenbedded/rcl/zenbedded_client.hpp>`. Both
`native_sim` and ESP32-S3 enable the Tier 2 TCP client: the current router
endpoints are `127.0.0.1:7447` and `192.168.0.105:7447`, respectively.
Firmware publishes heartbeat and both wheels' encoder positions and estimated
velocities at 10 Hz. The `kabot_ros2` submodule exposes these five state
interfaces through a standard ROS broadcaster using `rmw_zenoh_cpp`.

See the [physical robot + host controller runbook](docs/ros2-zenbedded-hardware-stack.md)
for startup, feedback checks, claim/release and bounded `/cmd_vel` commands, or
the [native simulator guide](kabot_ros2/docs/native_sim.md) for software-only use.
ROS currently uses [empirically calibrated open-loop motion](docs/calibrated-motion.md)
and model-derived odometry while the second encoder is investigated. A host
model maps standard diff-drive references to normalized hardware `effort` and
supplies virtual wheel states for RViz. `native calibrated:=false` retains the
historical FF-only PID profile. Explicit robot claim is still required.
See the [PID knowledge dump](docs/host-pid-bringup.md)
for the preserved implementation, bench results and return-to-feedback criteria.

## Documentation

- Documentation hub: [docs/README.md](docs/README.md)
- HMI architecture: [docs/hmi-architecture.md](docs/hmi-architecture.md)
- Firmware data flow: [docs/firmware-data-flow.md](docs/firmware-data-flow.md)
- Discovery and binding spec: [docs/robot-discovery-and-binding-spec.md](docs/robot-discovery-and-binding-spec.md)
- Real sensor tutorial: [docs/real-sensor-publisher-tutorial.md](docs/real-sensor-publisher-tutorial.md)
- ESP32-S3 MCUboot recovery: [docs/esp32s3-mcuboot-flash-recovery.md](docs/esp32s3-mcuboot-flash-recovery.md)
- MMC5603 bring-up report: [docs/magnetometer-implementation-report.md](docs/magnetometer-implementation-report.md)
- ICM42670L IMU bring-up report: [docs/icm42670l-implementation-report.md](docs/icm42670l-implementation-report.md)

By using the provided `.devcontainer`, you ensure that all dependencies and tools are correctly configured for this project.
