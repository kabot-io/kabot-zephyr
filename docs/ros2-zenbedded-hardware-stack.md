# ROS 2 + Zenbedded: Physical Robot Runbook

Current integration snapshot: 2026-10-04. This guide runs the ROS controller on
the Linux host and firmware on the ESP32-S3. The launch is named `native`, but
its hardware plugin also connects to the physical robot; it does not start a
simulator. For a software-only setup, see
[the native simulator guide](../kabot_ros2/docs/native_sim.md).

**Current mode: empirically calibrated open-loop control and model odometry.**
The second encoder is suspected to have a hardware fault. The default chain
uses `kabot_motion_model`, not feedback PID. See
[physical Twist, RViz and the timed return loop](calibrated-motion.md).
Raw telemetry and hardware `effort` interfaces remain available. See the
[PID knowledge dump](host-pid-bringup.md) for implementation details, trial
history and requirements before returning to closed-loop operation.

## Topology And Addresses

```text
Host: /cmd_vel (TwistStamped)
  -> controller_manager / kabot_base_controller (diff_drive_controller)
  -> velocity references -> kabot_motion_model (empirical calibration)
  -> effort -> ZenbeddedHardware -> Zenoh router -> ESP32 Zenbedded RCL client
  -> existing claim gate / zbus control validator / motor watchdog -> motors

ESP32: PCNT encoders -> encoder_publisher -> sensor_channel -> RCL state
  -> Zenoh router -> ZenbeddedHardware -> kabot_state_broadcaster -> ROS topics
```

| Component | Current endpoint |
| --- | --- |
| Host Zenoh router | `tcp/192.168.0.105:7447` |
| ESP32 Wi-Fi address | `192.168.0.103` (latest trial; DHCP can change it) |
| Claim/discovery service on ESP32 | UDP `30012` |
| Existing HMI motor control / state | UDP `30010` / `30011` |
| Flash port in dev container | `/dev/ttyACM0` |
| Application console in dev container | `/dev/ttyACM1`, 115200 baud |

Motor commands in this guide use Zenoh, not UDP `30010`. UDP `30012` is still
needed for the existing application enable gate. `hmi_port=30011` in a claim
request advertises the legacy state receiver port; ROS feedback uses Zenoh.

The robot must reach the host's TCP port 7447 over Wi-Fi. Allow that traffic
through the host firewall and allow claim requests/replies over UDP 30012.
`127.0.0.1` inside a container is not the host or robot. Run the router on the
host, not behind unpublished container ports. Use an isolated/trusted LAN:
the current claim mechanism is not authentication or exclusive ROS ownership.

## Prerequisites

- A complete `kabot-zephyr` checkout, including `kabot_ros2` and the West
  dependencies under `deps/`. Preserve the current local Zenbedded and Zephyr
  dependency changes; an upstream checkout alone may not reproduce this build.
- Firmware tools in the project dev container, as described in the
  [root README](../README.md). Build and flash there.
- Pixi on the Linux host, with access to the same checkout and the adjacent
  `deps/modules/zenbedded` source. ROS Lyrical and dependencies come from
  `kabot_ros2/pixi.toml`. Use `pixi`, or `$HOME/.pixi/bin/pixi` if it is not on PATH.
- Saved Wi-Fi credentials on the robot, an existing valid MCUboot layout and
  the robot connected to the same reachable LAN. Do not erase flash for an
  ordinary update. For an unprovisioned board or layout migration, consult the
  [flash recovery guide](esp32s3-mcuboot-flash-recovery.md).

Paths below start at `/path/to/kabot-zephyr`; replace this with the checkout path
in that terminal. The dev container checkout is `/workspaces/kabot-zephyr`.
Host and container paths can differ. Do not share a container-built Pixi/colcon
environment with an incompatible host; build the ROS packages in the environment
where they will run.

## 1. Start The Router On The Host

Host terminal A, from `kabot_ros2`:

```bash
cd /path/to/kabot-zephyr/kabot_ros2
pixi install
pixi run ros2 run rmw_zenoh_cpp rmw_zenohd
```

Keep it running. If the router is already running, use it instead of starting a
second instance. Check that its TCP listener is reachable through the host LAN
address, not only loopback. Deprecated routing-option warnings in the vendor
configuration have not prevented startup in the tested setup.

## 2. Build And Connect The Robot

When upgrading an existing running stack, stop command publishers, send zero,
release the robot and stop the old ROS launch first. The current calibration
change is host-only: if effort firmware is already deployed, skip flashing.
For a legacy velocity-command firmware version, deploy compatible effort
firmware and ROS together without leaving the previous controller live.

Dev container terminal, from the firmware root:

```bash
cd /workspaces/kabot-zephyr
./scripts/build.zsh --no-flash
./scripts/build.zsh --no-build --no-monitor
```

Ordinary flashing writes the signed application at `0x20000`, preserving
Wi-Fi/settings and the installed bootloader. Do not use `--nuke` for this flow.
The artifact is `build/esp32s3_devkitc/app/zephyr/zephyr.signed.bin`.
`ESPTOOL_PORT` and `KABOT_MONITOR_PORT` override the two serial port defaults.

The current ESP32 board configuration uses `192.168.0.105:7447`. If the router
address changes, update
[the board configuration](../app/boards/esp32s3_devkitc_esp32s3_procpu.conf)
and rebuild/flash. The shared filename `zenbedded_native_sim` does not imply
that this adapter is simulator-only.

Firmware waits for Wi-Fi and a preferred IPv4 address before initializing the
client, then retries failed initialization every second. To diagnose network
state, open the application console at 115200 baud and use these Zephyr shell
commands:

```text
wifi status
net iface
```

Expect station connectivity and an IPv4 address such as `192.168.0.101`.
The console can disconnect/re-enumerate during flashing. Use one serial reader
at a time, reopen it after reset, and do not interpret missing/dropped startup
logs as proof that initialization never ran.

## 3. Start The Controller On The Host

Host terminal B:

```bash
cd /path/to/kabot-zephyr/kabot_ros2
export RMW_IMPLEMENTATION=rmw_zenoh_cpp
export ZENOH_CONFIG_OVERRIDE='connect/endpoints=["tcp/192.168.0.105:7447"]'
pixi run build-native
pixi run native -- zenoh_endpoint:=tcp/192.168.0.105:7447
```

Keep this terminal running. `native` also invokes `build-native` before launch.
It starts the hardware plugin, `kabot_state_broadcaster`, `kabot_motion_model` and
`kabot_base_controller`, activated as a group. It does not start the router or
native firmware.

Both endpoint settings are required: `ZENOH_CONFIG_OVERRIDE` configures the ROS
RMW session; `zenoh_endpoint:=...` configures the hardware plugin's raw Tier 2
connection. Setting only one can leave ROS graph traffic and robot traffic on
different routers. Pixi activation already exports `RMW_IMPLEMENTATION`.

Run only one controller stack in this ROS domain: do not also launch `mock`,
`sim`, another `native`, or native firmware using the same Zenoh keys. This
configuration uses shared keys, not per-robot namespacing.

## 4. Verify Feedback Before Claiming

Host terminal C, also used for `/cmd_vel` below:

```bash
cd /path/to/kabot-zephyr/kabot_ros2
export RMW_IMPLEMENTATION=rmw_zenoh_cpp
export ZENOH_CONFIG_OVERRIDE='connect/endpoints=["tcp/192.168.0.105:7447"]'
pixi run ros2 control list_controllers
pixi run ros2 control list_hardware_interfaces
pixi run test-native
```

All three controllers should be active. The hardware list should contain the
five original state interfaces and both wheel `effort` command interfaces.
The model claims `left_wheel_joint/effort` and `right_wheel_joint/effort` and exports
`kabot_motion_model/{left,right}_wheel_joint/velocity` references and modeled
states for diff-drive. `[claimed]` indicates ros2_control resource ownership,
not the robot's UDP claim gate, and does not itself enable motors.

`test-native` checks 12 finite state samples and a positive advancing heartbeat;
it does not move the robot or claim it. Despite its name it also tests physical
feedback. Merely receiving repeated topic values is not enough: the plugin
retains cached values after disconnect. An advancing heartbeat proves firmware
traffic, but does not prove that the encoders are producing fresh samples.

To inspect values, use terminal C after the test:

```bash
pixi run ros2 topic echo /kabot_state_broadcaster/names control_msgs/msg/Keys --once --qos-durability transient_local
pixi run ros2 topic echo /kabot_state_broadcaster/values control_msgs/msg/Float64Values
```

Match each value to the key at the same index. Stop the echo with Ctrl+C before
using this terminal to publish commands.

## 5. Claim The Physical Robot

Secure the robot with its wheels clear of the surface, keep a physical power
disconnect accessible, and stop other command publishers before claiming.
Do not drive from ROS and the UDP HMI simultaneously; there is no input arbiter.

`pixi run native-claim` defaults to localhost. For the ESP32, use
`pixi run native-claim --host 192.168.0.101`; add `--release` to disable motors.
It uses the parent repository's existing protobuf codec. You can also use the
HMI, or the following alternative helper in a dev container Bash terminal with
`protobuf` and `grpcio-tools` in the firmware `.venv`.

Define this function once and keep the terminal open for release:

```bash
cd /workspaces/kabot-zephyr
export KABOT_ROOT="$PWD"
export ROBOT_IP=192.168.0.101

robot_claim() {
  "$KABOT_ROOT/.venv/bin/python" - "$1" <<'PY'
import os
import socket
import sys
from pathlib import Path

sys.path.insert(0, str(Path(os.environ["KABOT_ROOT"]) / "scripts/kabot_io"))
from proto_codec import decode_bonjour_response, encode_bonjour

action = sys.argv[1]
if action not in {"status", "claim", "release"}:
    raise SystemExit("Expected status, claim or release")
address = os.environ["ROBOT_IP"]
with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as connection:
    connection.settimeout(2)
    connection.sendto(
        encode_bonjour(30011, claim=action == "claim", release=action == "release"),
        (address, 30012),
    )
    payload, sender = connection.recvfrom(1024)
if sender[0] != address:
    raise SystemExit(f"Unexpected responder: {sender}")
response = decode_bonjour_response(payload)
if action != "status" and response.is_claimed != (action == "claim"):
    raise SystemExit("Robot did not accept the requested claim state")
print(f"robot={address} claimed={response.is_claimed}")
PY
}

robot_claim status
```

Expect `claimed=False` before a new test. If it is already claimed, coordinate
with the current operator; do not assume ownership from a successful response.
After verifying feedback, stopping other publishers and securing the robot:

```bash
robot_claim claim
```

Expect `claimed=True`. Claim is only an application enable gate, not an exclusive
lock, and does not start motion by itself. A pending nonzero ROS command can
start motion as soon as the gate is enabled.

## 6. Publish A Bounded Command And Stop

In host terminal C, with the endpoint exports from step 4 still set, publish
20 fresh-stamped commands at 20 Hz, then explicit zero commands:

```bash
pixi run ros2 topic pub --rate 20 --times 20 --max-wait-time-secs 5 /cmd_vel geometry_msgs/msg/TwistStamped '{header: auto, twist: {linear: {x: 0.0337243402}, angular: {z: 0.0}}}'
pixi run ros2 topic pub --rate 20 --times 5 --max-wait-time-secs 5 /cmd_vel geometry_msgs/msg/TwistStamped '{header: auto, twist: {linear: {x: 0.0}, angular: {z: 0.0}}}'
```

The first command requests approximately one second of updates once publication
starts. CLI startup/discovery can take longer; waiting for a matching subscriber
is capped at five seconds. `header: auto` refreshes the stamp
on every message. A plain `Twist` or a stale `TwistStamped` is not the right input.
Use one publisher at a time. For turning, replace the nonzero command's
`linear.x` with `0.0` and `angular.z` with `0.5`; keep the same bounded count
and the zero command afterwards. Negative values request reverse motion/turning.

Immediately after the test, in the claim terminal:

```bash
robot_claim release
robot_claim status
```

Expect `claimed=False`. If command publication is interrupted, still send zero
and release. When `/cmd_vel` updates stop, diff-drive zeros velocity references
after its 300 ms timeout plus scheduling/transport latency. Zero maps to zero
effort, without encoder-based braking. The firmware has a
300 ms command watchdog. These are not a certified emergency stop; use the
physical disconnect if the robot does not stop or communication is unavailable.
Do not stop the controller/router while deliberately leaving a nonzero command
active. After zero and release, the controller can be stopped with Ctrl+C.

### What These Numbers Mean

Wheel radius `0.016 m` and separation `0.102 m` remain unchanged. Physical
forward commands 0.03372434 and 0.06804734 m/s reproduce the accepted effort
pairs near 0.75 and 1. Full-rate in-place turns are approximately +/-1.37 rad/s.
The model scales saturated commands together to preserve curvature and reports
the resulting modeled motion to diff-drive. Reverse, arcs and very slow motion
remain unverified assumptions. See the [model details](calibrated-motion.md).

The old numeric FF-only mapping is available only with `calibrated:=false`
for the [historical timed calibration procedure](open-loop-calibration.md).
Do not use those raw speeds/trim in the default calibrated profile.

Inspect requested/model wheel velocities and output efforts while testing:

```bash
pixi run ros2 topic echo /kabot_motion_model/controller_state control_msgs/msg/MultiDOFStateStamped
```

## 7. Drive From The Keyboard

The native launch now includes the source submodule `twist_stamper`, built by
`pixi run build-native`. Restart an older native launch once to load it. Do not
start a second native stack or stamper alongside the first. This leaves the
current open-loop configuration and firmware unchanged.

In a separate interactive host terminal, after the controller and router are
running and you have verified that the robot can move safely:

```bash
cd /path/to/kabot-zephyr/kabot_ros2
export ZENOH_CONFIG_OVERRIDE='connect/endpoints=["tcp/192.168.0.105:7447"]'
pixi run native-claim --host 192.168.0.101
pixi run teleop
```

Keep keyboard focus in that terminal. `i` drives forward, `,` backward, `j`/`l`
turn left/right, and `k` stops. `w`/`x` increase/decrease requested linear speed;
`e`/`c` do the same for angular speed. Start values are 0.03372434 m/s and 0.5 rad/s;
there is no longer a low diff-drive speed cap, but effort still saturates at
[-1, 1]. Small effort may not overcome the observed motor starting threshold.

Press Ctrl+C to leave teleop, then release the robot in the same terminal:

```bash
pixi run native-claim --host 192.168.0.101 --release
```

Data path: `teleop_twist_keyboard` publishes `Twist` on `/cmd_vel_unstamped`;
`twist_stamper` stamps each received message with current ROS time and
`frame_id=base_link`, then publishes `TwistStamped` on `/cmd_vel`. The keyboard
node requires a real terminal; do not redirect its stdin or run it as a hidden
background process. Do not run a direct `/cmd_vel` publisher at the same time.

This installed keyboard version publishes on key events, not on a timer. Hold
the movement key for keyboard auto-repeat; if no key event arrives for 300 ms,
diff-drive times out and commands zero effort. A long desktop key-repeat delay
can therefore cause a brief stop after the first press. Stamper does not replay
old messages, so it does not defeat this timeout. Use `k`, Ctrl+C and release
for an intentional stop rather than relying only on auto-repeat behavior.

## 8. Timed Calibration

Use [open-loop-calibration.md](open-loop-calibration.md) for `pixi run calibrate`
forward/left/right trials in `calibrated:=false`, with a selected raw speed and duration, automatic zero and
release, JSON reports and manually measured distance/turns. Without `--execute`
it only checks readiness and displays predicted effort. It does not estimate
travel from faulty encoders or open-loop odometry and does not tune gains itself.

## State Contract And Implementation Map

Shared schema: [zenbedded_native_sim.yaml](../app/config/zenbedded_native_sim.yaml).
Tier 2 uses packed native numeric values, not ROS message serialization:

| State offset | Interface | Type / units |
| --- | --- | --- |
| 0 | `kabot/heartbeat` | float32, firmware uptime in seconds |
| 4 | `left_wheel_joint/position` | float32, radians in `[0, 2*pi)` |
| 8 | `left_wheel_joint/velocity` | float32, rad/s |
| 12 | `right_wheel_joint/position` | float32, radians in `[0, 2*pi)` |
| 16 | `right_wheel_joint/velocity` | float32, rad/s |

State key: `zenbedded/state`, 20 bytes, nominally 10 Hz. Command key:
`zenbedded/cmd`, 8 bytes: left then right wheel `effort` float32. Old heartbeat
or position-only payloads are incompatible; rebuild firmware and restart ROS
together after schema changes. Keep URDF and broadcaster interfaces in sync.
ROS exposes decoded states as float64 via `StateInterfacesBroadcaster`.

- [RCL adapter](../app/src/system/zenbedded_native_sim.cpp): reads paired valid
  encoder samples without blocking, publishes state, and forwards only newly
  accepted commands into the existing control path.
- [Encoder publisher](../app/src/zbus/state/encoder_publisher.c): PCNT/RTIO
  sampling and publication to `sensor_channel`, nominally every 100 ms.
- [Velocity estimator](../app/include/system/wheel_encoder_state.h): converts
  the shortest wrapped angular difference and sensor timestamp difference into
  rad/s. First sample or a regressed timestamp gives zero velocity; duplicate
  timestamps retain the estimate. A new stationary sample gives zero.
- [Firmware startup](../app/src/main.c): network readiness and client retry.
- [ROS launch](../kabot_ros2/kabot_robot/bringup/launch/native_sim.launch.py),
  [URDF](../kabot_ros2/kabot_robot/description/urdf/kabot_native_sim.urdf.xacro),
  [controller configuration](../kabot_ros2/kabot_robot/bringup/config/kabot_calibrated_controllers.yaml).

PCNT angles are wrapped, not accumulated positions. Velocity estimation assumes
less than half a revolution between consumed samples (about 5 rev/s at 100 ms).
Faster rotation or missed samples can alias both speed and direction. Missing
samples retain the last position and velocity; there is no encoder freshness
flag. Native simulation does not synthesize encoder motion, so its measured
states remain zero unless samples are supplied.

The calibrated profile uses diff-drive `open_loop: false` and
`position_feedback: false` to consume model velocity states, not raw encoders.
`/kabot_base_controller/odom`, `odom -> base_link` and `/joint_states` are
model-derived. Physical motor control remains open-loop.
Odometry can indicate movement while motors are unclaimed, disconnected or
stalled; it is not evidence of real motion. Telemetry remains available, but
the suspect encoder is not used to correct effort or integrate odometry.

No encoder repair or freshness gate is included. Heartbeat does not prove
encoder freshness. Before restoring PID, verify both channels, signs, resolution
and the estimator's aliasing limits, as detailed in the knowledge dump.

## Firmware Profile And Verified Results

The ESP32 profile retains both encoders, sensor subscriber, RTIO and PCNT. To
fit Zenbedded it disables non-encoder sensor publishers/drivers/mux, dummy shell
and MCUmgr/SMP. MCUboot and the application UART console remain enabled.
`CONFIG_MAIN_STACK_SIZE=5632`. The latest link used 382616 of 384256 bytes of
DRAM, leaving only 1640 bytes; this is a link result, not a runtime stack margin
measurement. Do not enable more services without checking the memory budget.

Verified before the host PID change:

- ESP32 and native_sim firmware builds, ROS package build, and agreement of all
  five state interfaces across schema/URDF/broadcaster.
- Ordinary signed-app flashing with hash verification and saved Wi-Fi retained.
- Hardware state smoke test: 12 samples, heartbeat `14.28 -> 15.38 s`, positions
  and velocities zero on stationary wheels; ROS lists all five state interfaces.
- Estimator tests for positive/negative motion, wraparound, duplicate/reset
  timestamps, stationary samples and irregular intervals. Run from the root:

```bash
c++ -std=c++17 -Wall -Wextra -Werror -I app/include tests/wheel_encoder_state_test.cpp -o /tmp/kabot-wheel-encoder-state-test
/tmp/kabot-wheel-encoder-state-test
```

The earlier PID/effort revision passed ESP32, native_sim and ROS builds and the
isolated `kabot_native_pid` launch test with mock hardware: distinct measured
wheel velocities, forward/reverse/turn references, proportional output, saturation,
command timeout, and feedback-based odometry twist/pose. This revision was then
flashed to ESP32 with hash verification. All three host controllers activated;
the hardware state check passed with advancing heartbeat and stationary wheels.
From `kabot_ros2`, with the ROS endpoint export from step 3:

```bash
pixi run colcon test --packages-select kabot_robot --ctest-args -R '^kabot_native_(pid|open_loop)$' --output-on-failure
pixi run colcon test-result --test-result-base build/kabot_robot/test_results --verbose
```

Historical bounded PID tests on 2026-10-04, before the open-loop fallback, with
the user confirming raised wheels:

| Trial | Forward command / duration | Peak effort | Feedback labelled left | Feedback labelled right |
| --- | --- | --- | --- | --- |
| Initial P=0.5 | 0.01 m/s / 1 s | 0.3125 | No movement | No movement |
| Temporary P=0.96, clamp +/-0.6 | 0.01 m/s / 0.5 s | 0.6 | Peak 0.4800 rad/s, position span 0.085844 rad | Zero |
| Same pulse repeated | 0.01 m/s / 0.5 s | 0.6 | Peak 0.4505 rad/s, position span 0.114459 rad | Zero |
| Temporary clamp +/-0.7 and higher command limit | 0.012 m/s / 0.5 s | 0.7 | Peak 1.1851 rad/s, position span 0.295183 rad | Zero |

The user saw the right wheel move and the left move slightly in the repeated
0.6 trial, while only the state labelled left changed. The physical assignment
of encoder channels and the missing right feedback need investigation; these
results do not establish correct independent wheel feedback. Do not increase
power further or tune PID as though both feedback channels were validated.
Each trial sent zero, confirmed `claimed=False` and ended with zero PID outputs.
Temporary runtime gains/clamps and the diff-drive limit were restored to P=0.5,
effort +/-1 and `linear.x.max_velocity=0.01`; configuration files were unchanged.

Earlier bounded hardware pulses confirmed both motors move through direct UDP
and ROS/Zenoh. The position/velocity changes were subsequently tested without
moving motors, before the PID trials above. Correct assignment/signs of both
moving encoders, calibrated speeds, physical watchdog fault injection, runtime
stack high-water marks and recovery after router loss remain unverified. Initial
connection retry is not proof of reconnection after an established session fails.

`pixi run test-native-motors` is a local simulator regression; see the simulator
guide for isolation/startup requirements. The experimental physical motor test
script is unfinished and must not be treated as a passing regression or run
automatically as part of this procedure.

## Troubleshooting

| Symptom | Check |
| --- | --- |
| TCP connection refused | Router running on host, LAN listener on 7447, firewall and correct router IP. |
| Firmware init fails before Wi-Fi | Check `wifi status` / `net iface`; current firmware waits for preferred IPv4 and retries. |
| ROS topics exist but heartbeat stays zero/frozen | Cached plugin state is not delivery proof; check both endpoint settings, robot connection and the 20-byte schema. |
| Old/missing ROS graph after changing RMW | In the configured ROS shell run `pixi run ros2 daemon stop`, then retry the query. |
| Claim request times out | Correct DHCP address, UDP 30012 reachable, firmware running; do not proceed to motion. |
| ros2_control says claimed but motors stay disabled | ROS resource ownership is separate from the robot's UDP claim gate. |
| Fresh heartbeat but no wheel movement | Claim gate, fresh `TwistStamped`, active diff-drive, limits and motor starting threshold; check one publisher only. |
| Velocity stays nonzero after encoder failure | Last valid sample is retained; heartbeat is not encoder freshness. |
| Zero cmd_vel but nonzero effort | Check that the old PID launch was stopped and current P/I/D=0, feedforward=1 are loaded; release the robot. |
| PID correction is unexpectedly active | Restart with the open-loop configuration; do not tune against the suspect second encoder. |
| Wrong speed/direction near high rotation rates | Wrapped-angle estimator aliasing or uncalibrated encoder sign/resolution. |
| Flash fails on ACM1 | ACM0 is the tested ROM flash port; ACM1 is the application console. Recheck enumeration after reset. |
| Editor reports missing Zephyr headers | Check the actual firmware build; editor include-path diagnostics have differed from successful builds. |

For a standalone ROS checkout, `ZENBEDDED_ROOT` selects the dependency for
`build-native`, while `KABOT_NATIVE_SIM_SCHEMA` or `schema_path:=...` selects the
shared schema for launch. Keep that schema byte-for-byte compatible with the
firmware; there is no negotiated schema migration.