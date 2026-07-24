# kabot-zephyr

This repository contains firmware of the Kabot project robot. There are two builds avaialble in the [releases](https://github.com/kabot-io/kabot-zephyr/releases):

- ESP32-S3 OTA update image: `kabot-<version>-esp32s3-zephyr.signed.bin` which is used to update robot firmware using [mcuboot](https://docs.mcuboot.com) (possible to upload using [SMP protocol](https://docs.zephyrproject.org/latest/services/device_mgmt/smp_protocol.html) over WiFi)

- Linux x86-64 native-sim build: `kabot-<version>-native-sim-zephyr.exe` which is a firmware build as a single linux executable, used for software-in-the-loop (SITL) testing, and emulating the robot.

Generally, it is advisable to update the firmware and test the robot using Graphical User Interface available at [kabot-hmi](https://github.com/kabot-io/kabot-hmi) repository. The hardware files (KiCad and FreeCAD source files) are available at the [kabot-hardware](https://github.com/kabot-io/kabot-hardware) repository.

## Architecture

Firmware architecture is designed to mimick ROS2 pub-sub system. Sensor reading ([State protobuf message](https://github.com/kabot-io/kabot-zephyr/blob/main/app/protos/state_control_msg.proto#L39)) and actuator controls ([Control protobuf message](https://github.com/kabot-io/kabot-zephyr/blob/main/app/protos/state_control_msg.proto#L64)) are passed around using [Zephyr bus (zbus) messaging subsystem](https://docs.zephyrproject.org/latest/services/zbus/index.html).


![alt text](docs/img/kabot-architecture.drawio.png)

Note: the architecture diagram contains draw.io sources, so it is possible to import it into the editor, or possibly parse it.
## Development

### AI Disclaimer

To be fair, a lot of the code is developed using AI, however the architecture is strictly followed, and the firmware is composed of reusable building blocks. The code is kept human-readable (and -developable). To get a grip about the firmware, it might be a good idea to point your agent of choice into `/docs` directory (where a lot of markdown files are left as a context/knowledge artifacts for the future agents) and ask around.

### Environment

This repository is configured to use a `.devcontainer` for development - to spin up the environment, just open this repository using Visual Studio Code with the [Dev Containers extension](https://marketplace.visualstudio.com/items?itemName=ms-vscode-remote.remote-containers).


Once the development container is ready, use the project build script:

```zsh
./scripts/build.zsh
```

Default behavior builds the ESP32-S3 firmware with [sysbuild](https://docs.zephyrproject.org/latest/build/sysbuild/index.html) (application + MCUboot),
flashes it, and opens monitor output.

Sysbuild means a single multi-image Zephyr build that
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

`--nuke` is intended for ESP32-S3 initial flashing and it builds the sysbuild pristine image, wipes all flash and writes bootloader and firmware image.

### Conventions

All of the code must be commited using [conventional commits](https://www.conventionalcommits.org/en/v1.0.0/). They are later used to construct changelog during tagged release.

Build scripts generate [`VERSION`](https://docs.zephyrproject.org/latest/build/version/index.html) file from the latest SemVer tag. `VERSION_TWEAK` is the commit distance from that tag and `EXTRAVERSION` is the sanitized branch name (omitted on the default branch).

### Continous Integration

CI uploads signed image as a workflow artifact. Pushing a SemVer tag creates a GitHub release with changelog, and both signed image and native-sim build as release assets.

Zephyr automatically uses `APP_VERSION_TWEAK_STRING` for MCUboot signing
version.
