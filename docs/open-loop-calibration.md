# Open-Loop Motion Calibration

This page preserves the **historical raw calibration commands**. They require
`native calibrated:=false`. The default launch now uses an empirical physical
speed mapping; see [calibrated motion, RViz and the return loop](calibrated-motion.md).
Do not replay the raw speeds/trim against the calibrated profile.

Use [calibrate-motion.py](../kabot_ros2/scripts/calibrate-motion.py) through
`pixi run calibrate` for one timed forward run or one timed turn. Measure actual
travel independently; the encoder fault is unresolved and open-loop odometry is
command-derived. The script does not stop at a measured distance or angle.

## Prepare

The former +/-0.01 m/s and +/-0.1 rad/s diff-drive limits have been removed.
Each motor still saturates at effort +/-1. P=I=D=0 and feedforward=1 are unchanged.
No firmware update is required. Restart the existing native launch to load this
configuration: close teleop/other publishers, release the robot, stop the old
launch with Ctrl+C, then start it again. Do not run two stacks or a simulator
on the shared Zenbedded keys. Keep your existing router running.

In `kabot_ros2`, with the robot address and router endpoint adjusted as needed
(latest trial used robot 192.168.0.103; DHCP can change it):

```bash
export ZENOH_CONFIG_OVERRIDE='connect/endpoints=["tcp/192.168.0.105:7447"]'
pixi run native-claim --host 192.168.0.103 --release
# Stop the previous native launch before this command.
pixi run native -- zenoh_endpoint:=tcp/192.168.0.105:7447 calibrated:=false
```

Use another terminal in `kabot_ros2`, with the same endpoint export, for trials.
Leave the robot unclaimed: the script claims and releases it for each trial.
Close teleop, even when it is idle; do not send UDP HMI commands during trials.
Clear the travel area and keep a physical power disconnect accessible. Start
with a short duration before the full 1.265 m course or five full rotations.

## Preview And Run

Without `--execute`, the script checks live configuration/telemetry and prints
predicted wheel references and effort. It does not publish commands or claim.
It refuses already claimed robots, competing ROS publishers, missing/stale
telemetry, feedback PID, inactive controllers or the old velocity limits.

```bash
pixi run calibrate forward --host 192.168.0.103 --speed 0.016 --duration 2
```

Add `--execute` to move after a three-second countdown. Each line below is a
separate trial, not a sequence to paste and run unattended:

```bash
pixi run calibrate forward --host 192.168.0.103 --speed 0.016 --duration 2 --execute
```

```bash
pixi run calibrate left --host 192.168.0.103 --speed 0.314 --duration 2 --execute
```

```bash
pixi run calibrate right --host 192.168.0.103 --speed 0.314 --duration 2 --execute
```

`--speed` is positive: m/s for `forward`, rad/s for `left`/`right`. Positive
angular velocity is left/counterclockwise viewed from above; verify physical
wiring agrees. `--duration` is seconds of command publication, not a distance
or rotation target. Values must be finite and greater than zero.

### Forward Rotation Trim

`forward` accepts `--angular` in rad/s, default 0. Negative values steer right;
positive values steer left. Zero preserves the original forward command. The
option is rejected for `left`/`right`, whose angular speed is set by `--speed`.
The selected trim is included in the report's existing `angular_rad_s` field
and in the predicted efforts. Stop and cleanup always command both components
to zero, regardless of the trim. This script-only option needs no controller
restart or firmware flash.

### Accepted Forward Settings

Accepted by the user on 2026-10-04 after the course trials below:

| Parameter | Saved value |
| --- | --- |
| Course length | 1265 mm / 1.265 m |
| `--speed` | 0.016 m/s command |
| `--angular` | -0.0122 rad/s command |
| `--duration` | 18.59 s |

Preview the saved settings:

```bash
pixi run calibrate forward --host 192.168.0.103 --speed 0.016 --angular -0.0122 --duration 18.59
```

After checking clearance, add `--execute` for a single run. Verify the robot's
current DHCP address. The final result was reported as very good; no numerical
residual error or repeatability measurements were supplied. Preserve these
settings as the forward baseline while calibrating rotation separately.

At these settings, left effort is clamped to 1 and right effort is 0.9611125.
The saturation notice is expected.
Steering works by reducing one wheel's effort, not exceeding the other wheel's
maximum. These are saved raw trial settings, not physical command values for
the new calibrated profile. That profile embeds this effort pair as an anchor.

### Accepted Lower-Speed Forward Settings

Accepted by the user on 2026-10-04 for the shorter 575 mm course:

| Parameter | Saved value |
| --- | --- |
| Course length | 575 mm / 0.575 m |
| `--speed` | 0.012 m/s command |
| `--angular` | -0.0015 rad/s command |
| `--duration` | 17.05 s |

Preview the saved settings:

```bash
pixi run calibrate forward --host 192.168.0.103 --speed 0.012 --angular -0.0015 --duration 17.05
```

The final result was accepted as OK; residual errors and repeatability were
not quantified. Predicted efforts are left 0.75478125 and right 0.74521875,
without saturation. The inferred average physical speed is about 0.03372 m/s,
roughly half the accepted high-effort forward rate, despite using 75% of its
linear command. The trim is a separately tuned value, not the high-speed trim
scaled proportionally. Both forward effort pairs are now anchors in the
calibrated model; raw trial settings and nominal geometry remain unchanged.

### Accepted Left Rotation Settings

On 2026-10-04 the user reported exactly five counterclockwise revolutions
(viewed from above) with these settings:

| Parameter | Saved value |
| --- | --- |
| Mode | `left` |
| `--speed` | 0.314 rad/s command |
| `--duration` | 22.9 s |
| Observed rotation | 5 complete counterclockwise revolutions |

Preview the saved left-turn settings:

```bash
pixi run calibrate left --host 192.168.0.103 --speed 0.314 --duration 22.9
```

### Accepted Right Rotation Settings

On 2026-10-04 the user reported exactly five clockwise revolutions (viewed from
above) at `--speed 0.314` and `--duration 22.93`. Preview the saved settings:

```bash
pixi run calibrate right --host 192.168.0.103 --speed 0.314 --duration 22.93
```

Add `--execute` only for an attended trial with clear surroundings. Do not add
the forward `--angular` trim. Keep both directions' results separate;
repeatability has not been quantified.

### Calibration Status And Next Steps

Three high-effort settings and one lower-effort forward setting are accepted. Average
physical rates inferred from the reported travel and command durations are:

| Motion | Travel | Command duration | Approximate average physical rate |
| --- | --- | --- | --- |
| Forward with trim -0.0122 | 1.265 m | 18.59 s | 0.06805 m/s |
| Lower-speed forward with trim -0.0015 | 0.575 m | 17.05 s | 0.03372 m/s |
| Left / CCW | 5 turns | 22.9 s | +1.3719 rad/s |
| Right / CW | 5 turns | 22.93 s | -1.3701 rad/s |

These rates include startup and stopping effects; they are not steady-state
encoder measurements or a calibration of the whole speed range. The 0.03 s
turn-duration difference is smaller than the nominal 0.1 s controller period;
do not infer a repeatable directional asymmetry without repeated trials.

These observations now anchor a provisional host-side open-loop
[speed-to-effort model](calibrated-motion.md), leaving real wheel geometry
unchanged. It preserves exact zero, effort limits +/-1, claim/release and
watchdog behavior. Straightness compensation varies with command speed;
there is no fixed angular offset at rest or during every turn.

Before treating that mapping as calibrated, repeat the accepted runs. Forward
effort near 0.75 now has an accepted point; the earlier roughly half-effort trial
did not move. Additional effort levels can refine the usable range. Measure
the starting threshold and reverse travel separately.
This distinguishes starting friction, speed-dependent imbalance and reverse
behavior from a simple scale error. Battery and surface conditions must be
comparable. Combined forward/turn commands also need validation after mapping.

The default launch and teleop now use physical calibrated units. Raw mode
preserves the former FF-only gains and command semantics. Model odometry
remains an estimate, not confirmation of physical travel.

### Reported Measurements

User observations on 2026-10-04, not automated odometry measurements:

| Observation | Reported value |
| --- | --- |
| Current calibration course (replaces the earlier 5 m target) | 1265 mm / 1.265 m |
| Initial forward input / approximate traversal time | speed 0.016 / about 20 s |
| Separately reported lateral drift | 245 mm left over 1400 mm |

Subsequent forward trials on the 1265 mm course, all at `--speed 0.016`:

| Angular trim (rad/s) | Duration (s) | Distance error | Lateral error | Outcome |
| --- | --- | --- | --- | --- |
| -0.01 | 20 | 85 mm too far | 65 mm left | Adjusted |
| -0.014 | 18.74 | 10 mm too far | 55 mm right (interpreted from the user's "55m") | Adjusted |
| -0.0122 | 18.59 | Not quantified | Not quantified | User accepted: very good |

The initial course result implies approximately 0.06325 m/s average physical
speed, about 3.95 times the 0.016 m/s command. It is one full-effort operating
point, not a linear calibration across all speeds. The drift observation uses
a different distance; do not combine it with the 20 s time to infer an exact
angular correction. Raw feedforward, geometry and default trim remain unchanged.
Record precise elapsed time and lateral offset on
the 1265 mm course for each subsequent trim setting.

Lower-speed trials targeting 575 mm:

| Speed command (m/s) | Angular trim (rad/s) | Duration (s) | Reported travel | Drift / result |
| --- | --- | --- | --- | --- |
| 0.008 | -0.0061 | 16.9 | No movement | User attributed this to stiction |
| 0.012 | -0.00915 | 11.27 | 375 mm | Strong right turn; user wrote "45%", interpreted as about 45 degrees, unconfirmed |
| 0.012 | 0 | 11.27 | 440 mm | 40 mm left |
| 0.012 | -0.0015 | 11.27 | 380 mm | Straight |
| 0.012 | -0.0015 | 17.05 | 575 mm target accepted | User accepted: OK; residual error not quantified |

At speed 0.008 with trim -0.0061, predicted efforts were left 0.51944375 and
right 0.48055625. No motion in that trial is a useful no-start observation, not
a precise measurement of either motor's breakaway or running-friction threshold.
Curved-trial distances should not be used as straight-line speed calibration.

Current nominal geometry (radius 0.016 m, separation 0.102 m) and feedforward=1:

| Trial command | Left effort | Right effort |
| --- | --- | --- |
| Forward 0.008 m/s | 0.5 | 0.5 |
| Forward 0.012 m/s | 0.75 | 0.75 |
| Forward 0.016 m/s | 1.0 | 1.0 |
| Left 0.314 rad/s | -1.0 (saturated) | 1.0 (saturated) |
| Right 0.314 rad/s | 1.0 (saturated) | -1.0 (saturated) |

These are commanded speeds, not calibrated physical speeds. Beyond 0.016 m/s
straight or about 0.3137255 rad/s turning, effort is saturated. Larger numbers
cannot increase motor power. At saturation, command odometry is particularly
misleading. The script reports saturation but does not impose another speed cap.

## Stop And Record

The script publishes at 20 Hz through `Twist -> twist_stamper -> TwistStamped`.
After the requested duration, Ctrl+C/SIGTERM or a detected fault, it attempts
zero, release (up to three attempts) and repeated zero messages. It monitors
advancing firmware heartbeat, controller state and command publishers during
countdown and motion. A report marks whether release was confirmed.

The duration uses a host monotonic clock. Physical start/stop includes 10 Hz
controller scheduling, network delay and coast-down; it is not hard real-time.
The 300 ms controller timeout and firmware watchdog remain enabled. If release
cannot be confirmed or the robot does not stop, use the physical disconnect.
SIGKILL, host failure and network failure cannot guarantee cleanup. Claim is an
enable gate, not ownership arbitration; ROS graph checks cannot detect a UDP
HMI sender or prove that shared telemetry belongs to the claimed robot.

Each invocation writes a new JSON report under `kabot_ros2/log/calibration/`,
including command, requested/elapsed time, live configuration, predicted effort,
controller effort samples and completion/release status. `--output PATH` chooses
a new file; existing files are never overwritten. Preview/failure reports are
marked accordingly. `measured_distance_m` and `measured_turns` start as null;
provide your measurements alongside the report. There is no automatic tuning.

For each completed trial record:

| Motion | Speed input | Angular trim (rad/s) | Duration | Measured distance (m) / turns | Surface, battery, drift |
| --- | --- | --- | --- | --- | --- |
| forward | | | | | |
| left | | N/A | | | |
| right | | N/A | | | |

Measure distance from the same point on the chassis, not a wheel. For turns,
count complete revolutions plus the remaining fraction, starting and finishing
after the robot settles. Record veering separately. Repeat each direction at
least three times on the same surface and battery condition; several effort
levels help distinguish starting friction from proportional scaling.

After a trial lasting `T` seconds that travels `d` metres, a first estimate for
a 1.265 m run at the same speed and trim is `T_next = T * 1.265 / d`. After `n` turns use
`T_next = T * 5 / n`. These estimates require nonzero measurements and similar
conditions; acceleration/coast-down prevent exact scaling. Do not set duration
to `1.265 / commanded_speed` and assume that means 1265 physical millimetres.

The measurements give average physical `v = distance / time` and
`omega = 2*pi*turns / time`. Send both turn directions and forward results with
their reports. We can then tune feedforward (and assess starting deadband or
left/right asymmetry) without disguising motor mapping errors as wheel geometry.

## Verification

No physical motion is part of automated validation. Run from `kabot_ros2`:

```bash
pixi run build-native
pixi run colcon test --packages-select kabot_robot --ctest-args -R '^kabot_(calibrate_motion|native_pid|native_open_loop)$' --output-on-failure
pixi run colcon test-result --test-result-base build/kabot_robot/test_results --verbose
```

Unit tests cover arguments, signed forward trim, direction, saturation and stop/release on success,
interruption, claim/release failure and motion failure. The isolated launch test
uses real stamper/diff-drive/PID controllers with mock hardware, mocked UDP and
mocked advancing heartbeat to exercise all three timed modes, both signs of
forward trim and final zero.