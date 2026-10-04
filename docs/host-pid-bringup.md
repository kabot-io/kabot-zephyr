# Host PID Bring-up And Open-Loop Fallback

Snapshot: 2026-10-04. **Current operating mode is open-loop**, at the user's
request, because the second encoder is suspected to have a hardware fault.
That hardware diagnosis is not confirmed yet. This document preserves the PID
implementation, test results and remaining work; it is not an instruction to
resume closed-loop driving before the encoder problem is resolved.

For router, robot, ROS startup, claim/release and bounded `cmd_vel` examples,
use [the hardware runbook](ros2-zenbedded-hardware-stack.md).

## Current Configuration

The current default has moved to [calibrated motion and model odometry](calibrated-motion.md).
The FF-only profile below is preserved as `native calibrated:=false` for
historical trials. It is no longer the default native controller chain.

The [controller configuration](../kabot_ros2/kabot_robot/bringup/config/kabot_native_sim_controllers.yaml)
keeps the standard controller chain, but disables feedback correction:

```text
/cmd_vel (TwistStamped)
  -> kabot_base_controller (diff_drive_controller)
  -> kabot_wheel_pid/{left,right}_wheel_joint/velocity references
  -> kabot_wheel_pid (PidController, P=I=D=0, feedforward=1)
  -> {left,right}_wheel_joint/effort
  -> Zenbedded -> ESP32 claim gate / control validator / watchdog -> motors
```

| Setting | Current value | Reason |
| --- | --- | --- |
| Diff-drive `open_loop` | `true` | Integrate odometry from commands, not encoders. |
| Diff-drive `position_feedback` | `false` | Retained for a future velocity-feedback mode; unused for open-loop odometry. |
| Each wheel's P, I, D | `0.0` | No correction from velocity error. |
| Each wheel's `feedforward_gain` | `1.0` | Reference-to-effort numeric mapping is 1:1. |
| `set_current_state_as_first_setpoint` | `false` | Do not seed a command from a possibly faulty measured speed. |
| Effort limits | `[-1, 1]` | PID configuration and hardware command interface limits. |
| Linear / angular input limits | Disabled | Removed for manual calibration; effort still saturates at +/-1. |
| Controller / encoder rates | nominally 10 Hz | No rate or firmware resource changes. |

The node still has `pid` in its name and reads feedback for diagnostics, but
its error correction is disabled. Hardware command names remain `effort`, not
`velocity`. The five state interfaces remain published for inspection.
Do not delete the encoder telemetry merely to hide the faulty channel.

With radius 0.016 m and wheel separation 0.102 m:

```text
left_reference  = (linear.x - angular.z * 0.102 / 2) / 0.016
right_reference = (linear.x + angular.z * 0.102 / 2) / 0.016
left_effort = clamp(left_reference, -1, 1)
right_effort = clamp(right_reference, -1, 1)
```

The former +/-0.01 m/s and +/-0.1 rad/s input limits were subsequently removed
at the user's request for [manual calibration](open-loop-calibration.md).
A straight `linear.x=0.005` command gives 0.3125 effort; `0.01` gives 0.625,
and `0.016` gives 1.0. Turning saturates at about +/-0.3137255 rad/s. This is not
calibrated speed control or torque in Nm. Above saturation, a larger command
does not produce more power. Do not reuse previous PID tuning assumptions.

Odometry and `odom -> base_link` are command-derived again. They may indicate
motion while the robot is unclaimed, disconnected, stalled or stationary. They
must not be treated as physical motion feedback. The command timeout still
zeros references after 300 ms; with zero feedback gains this also zeros effort,
rather than actively braking against the reported encoder speed.

Switching from the deployed effort/PID firmware to this fallback requires only
a ROS controller restart. Stop publishers, send zero, release the robot, stop
the old launch and restart it using the runbook. No firmware flash, schema
change, sensor driver change or permanent gain tuning is needed.

## Preserved PID Implementation

The closed-loop experiment used the same standard
`pid_controller/PidController` with two independent wheel channels. It claimed
hardware `effort` commands and read hardware `velocity` states in rad/s.
Diff-drive consumed both the reference and measured-state interfaces exported
by PID, using `open_loop: false` and `position_feedback: false`.

Initial settings were P=0.5, I=D=0, feedforward=0, output/integral clamps +/-1,
`conditional_integration` anti-windup and `save_i_term: false`.
`set_current_state_as_first_setpoint` was effectively `true` by default.
These were trial settings, not a tuned speed controller. With I=D=0 and no
feedforward, effort was `clamp(0.5 * (reference - measured_velocity), -1, 1)`.

The feedback test needs the previous initialization setting: using `false`
with feedback-based diff-drive caused activation to fail on initially invalid
exported wheel states in the installed controller version. The retained PID
test explicitly enables it. Do not re-enable closed-loop mode by changing only
`open_loop`, or only P, and leave an inconsistent combination of settings.

The ESP32 schema is unchanged by this fallback: state payload is 20 bytes
(heartbeat, left position/velocity, right position/velocity); command payload is
8 bytes (left/right effort). The RCL adapter forwards commands to the existing
zbus validator, claim gate and watchdog. PID and odometry live on the host.
ESP32 DRAM was 382616/384256 bytes; no further resource reductions were made.

## Physical Evidence

Firmware with effort commands was flashed with hash verification. All three
ROS controllers activated and the five-state hardware smoke test passed.
The following pulses were explicitly authorized with raised, free wheels:

| Trial | Command / duration | Peak effort | Left-labelled velocity / position span | Right-labelled feedback |
| --- | --- | --- | --- | --- |
| Initial P=0.5 | 0.01 m/s / 1 s | 0.3125 | Zero / zero | Zero |
| P=0.96, effort capped at 0.6 | 0.01 m/s / 0.5 s | 0.6 | 0.4800 rad/s / 0.085844 rad | Zero |
| Repeat of the same pulse | 0.01 m/s / 0.5 s | 0.6 | 0.4505 rad/s / 0.114459 rad | Zero |
| Temporary input limit 0.012 and effort cap 0.7 | 0.012 m/s / 0.5 s | 0.7 | 1.1851 rad/s / 0.295183 rad | Zero |

The user saw the physical right wheel move, and the left move slightly during
the repeated 0.6 trial. Only the state labelled left changed. Possibilities
include a faulty second encoder path and/or swapped physical channel labels;
the tests do not distinguish them. Do not silently swap labels based on these
observations alone. The user requested open-loop operation pending hardware work.

Each pulse ended with explicit zero commands, confirmed `claimed=False` and zero
PID outputs. Temporary gains/clamps and the diff-drive input limit were restored
after every trial. The subsequent open-loop configuration change is deliberate
and permanent until explicitly revisited. No further motion test is part of
this fallback change.

## Tests And Tools

[test_native_pid.py](../kabot_ros2/kabot_robot/test/test_native_pid.py) runs the
real controller chain with isolated mock hardware, fixed positions and nonzero
velocity feedback (left +1, right -2 rad/s), not physical motors:

- `kabot_native_open_loop`: current production parameters; effort follows only
   the clamped reference, odometry follows commands, full effort +/-1 is reachable,
   and timeout stops output/odometry despite nonzero feedback. It also exercises
   the timed calibration script on mock hardware for forward and both turns.
- `kabot_native_pid`: explicit historical test overrides; verifies velocity
  error, proportional output, saturation and feedback-based odometry twist/pose.

From `kabot_ros2`, using the existing router:

```bash
export ZENOH_CONFIG_OVERRIDE='connect/endpoints=["tcp/192.168.0.105:7447"]'
pixi run build-native
pixi run colcon test --packages-select kabot_robot --ctest-args -R '^kabot_native_(pid|open_loop)$' --output-on-failure
pixi run colcon test-result --test-result-base build/kabot_robot/test_results --verbose
```

`build-native` pins CMake's Python to the Pixi interpreter. An earlier CMake
cache selected the firmware Python 3.12 environment against ROS/Pixi Python
3.14 packages, causing a NumPy ABI failure before tests even started. Old CTest
`Testing/<date>/Test.xml` files can retain historical failures; inspect the
current test-result files rather than confusing old failures with the latest run.

The encoder estimator host unit test and the local native motor regression are
described in [the simulator guide](../kabot_ros2/docs/native_sim.md). The older
experimental physical motor test script remains unfinished; the bounded bench
trials above did not use it.

## Before Returning To PID

1. With motors disabled, inspect power, ground, signal wiring and the second
   encoder's A/B outputs. Verify pulses electrically before changing software.
2. Turn one physical wheel at a time and identify its raw PCNT channel, position
   and velocity topic labels. Verify both positive and negative rotation. No
   diagnosis or label reassignment has been performed in this change.
3. Confirm counts/revolution, direction and radians conversion. The current PCNT
   angle wraps at one revolution; the velocity estimator assumes less than half
   a turn between consumed samples (about 5 rev/s at 100 ms). Missed samples or
   faster motion can alias speed and direction.
4. Address stale feedback and enable/claim transitions before integral tuning.
   Last valid encoder values are retained; advancing heartbeat is not encoder
   freshness. Host PID does not know whether the robot accepted a claim or
   effort command. A command watchdog does not detect bad sensor data while
   fresh host command packets continue arriving.
5. On a secured bench, deliberately restore a coherent closed-loop profile:
   diff-drive `open_loop: false`, `position_feedback: false`, PID feedforward=0,
   initialization=true and small provisional P with I=D=0. Rerun the PID test,
   then use bounded physical trials. Do not restore temporary P=0.96 or higher
   command limits as presumed calibrated values.

Router-loss recovery, calibrated speed/odometry, physical watchdog fault
injection and runtime stack high-water marks remain unverified.