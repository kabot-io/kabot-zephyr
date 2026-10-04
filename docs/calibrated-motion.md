# Calibrated Motion And RViz

See [control architecture](control-architecture.md) for editable D2 diagrams of
the controller stack, model/telemetry paths and firmware safety gates.

The default `pixi run native` now uses empirical physical `Twist` units through
`kabot_motion_model`, while retaining the standard diff-drive controller and
real wheel geometry. Control is still physically open-loop: no encoder feedback
corrects effort. The second encoder fault remains unresolved.

## Start One Stack

Stop teleop/other publishers, send zero, release the robot, then stop the old
native launch with Ctrl+C. Keep the existing router running. From `kabot_ros2`:

```bash
export ZENOH_CONFIG_OVERRIDE='connect/endpoints=["tcp/192.168.0.105:7447"]'
pixi run native-claim --host 192.168.0.103 --release
# Stop the previous native launch before starting this one.
pixi run native -- zenoh_endpoint:=tcp/192.168.0.105:7447 calibrated:=true rviz:=true
```

`native` builds the host packages before launch. No firmware flash is needed
when the existing effort-command firmware is deployed. Verify the DHCP address.
Never run a second native, mock or sim stack on the same shared topics and
Zenbedded keys. Omit `rviz:=true` on a headless host; GUI rendering requires a
working display.

To open the view separately while the controller is already running, use a
second terminal in `kabot_ros2` with the same router settings:

```bash
export ZENOH_CONFIG_OVERRIDE='connect/endpoints=["tcp/192.168.0.105:7447"]'
pixi run view
```

`view` now starts only RViz. It does not rebuild, start controllers, generate
joint states or open a joint slider GUI. Close any old view launch first to
remove its former synthetic publishers. The existing stack supplies
`/robot_description`, `/joint_states` and TF; the viewer uses `odom` as both
Fixed Frame and camera target, so the robot moves over the grid as odometry
changes. Without the stack it waits for data. No controller restart is needed
to attach the new viewer; do not also open the embedded `rviz:=true` window.

The three active controllers are `kabot_state_broadcaster`,
`kabot_motion_model` and `kabot_base_controller`. `pixi run test-native` remains
a read-only check of firmware telemetry. Keyboard teleop starts at
0.03372434 m/s and 0.5 rad/s; the latter is an unmeasured interpolation.
Claim before keyboard motion and release after stopping. Only one command
source may run at a time.

## Physical Commands

Use `Twist` on `/cmd_vel_unstamped` through the existing stamper, or fresh
`TwistStamped` on `/cmd_vel`. Positive angular velocity is counterclockwise
viewed from above. Do not add the old forward angular trim: the model already
includes the two accepted straightness corrections.

| Motion | Physical linear.x (m/s) | Physical angular.z (rad/s) | Effort left / right |
| --- | --- | --- | --- |
| Lower forward | 0.575 / 17.05 = 0.0337243402 | 0 | 0.75478125 / 0.74521875 |
| Higher forward | 1.265 / 18.59 = 0.0680473373 | 0 | 1 / 0.9611125 |
| Left in place | 0 | 10*pi / 22.9 = 1.3718745212 | -1 / 1 |
| Right in place | 0 | -10*pi / 22.93 = -1.3700796570 | 1 / -1 |
| Stop | 0 | 0 | 0 / 0 |

These are average rates from the [accepted manual observations](open-loop-calibration.md),
not newly measured results. Repeat those courses with the new physical command
values before relying on accuracy. The small CW/CCW difference is retained but
does not establish repeatable asymmetry.

## Return Loop

The agreed geometry uses **20 cm straight segments and a 40 cm arc diameter**.
After the two straight segments, a 20 cm diameter quarter-circle cannot reach
the start. The script uses the approved 20 cm radius, with no reversing:

| Segment | Command | Nominal duration |
| --- | --- | --- |
| 1 | Forward 20 cm, 0.03372434 m/s | 5.9304 s |
| 2 | Right in place 90 degrees, -1.370079657 rad/s | 1.1465 s |
| 3 | Forward 20 cm, 0.03372434 m/s | 5.9304 s |
| 4 | Right in place 90 degrees | 1.1465 s |
| 5 | Right quarter-circle, radius 20 cm, v=0.03372434 m/s, w=-0.16862170 rad/s | 9.3155 s |
| 6 | Right in place 90 degrees to the initial heading | 1.1465 s |

In another terminal, with the same router export, preview offline first:

```bash
pixi run drive-loop --host 192.168.0.103
```

Preview does not open a ROS node, send network traffic or claim the robot.
Close teleop/HMI and leave the robot unclaimed. Clear the path, keep a physical
power disconnect accessible, then explicitly execute:

```bash
pixi run drive-loop --host 192.168.0.103 --execute
```

There is a 3 s countdown, one claim for the sequence, 0.5 s zero-command pauses
between segments, then zero/release cleanup. Ctrl+C, SIGTERM, stale heartbeat,
stale controller output or competing ROS publishers abort through the same
cleanup path. ROS timeout and the firmware watchdog remain 300 ms. These are
not a certified emergency stop. The claim gate is not an exclusive lock; do not
send UDP HMI motor commands concurrently. Shared telemetry cannot identify
which robot produced it; run only one robot on these keys.

Execution writes a new JSON file under `log/calibration`, or the path supplied
with `--output`. It records the plan, **nominal** relative poses, live model
parameters, per-segment elapsed publication times/completion, effort samples
and release status. Existing files are not overwritten. It does not measure
actual distance or terminate on measured position. Total moving time is about
24.62 s, plus pauses, countdown and communication/setup time.

The ideal relative poses are (0.2,0), (0.2,-0.2), then (0,0), with final
heading equivalent to the starting heading. This is timed open-loop closure,
not a guarantee of returning to the physical start. The nominal 10 Hz cycle
alone can quantize a full-rate turn by about 8 degrees per cycle. Short turns,
startup/coasting, battery, traction and the uncalibrated arc can add error.

## Model And Odometry

The [model](../kabot_ros2/kabot_robot/include/kabot_robot/motion_calibration.hpp)
fits a separate affine effort-versus-forward-speed line through the two
accepted points for each wheel. For each wheel, slope `a`, offset `b` and virtual
full-effort speed `smax` are:

```text
a = (high_effort - low_effort) / (high_speed - low_speed)
b = low_effort - a * low_speed
smax = (1 - b) / a
demand_left  = linear - angular * smax_left  / directional_turn_rate
demand_right = linear + angular * smax_right / directional_turn_rate
scale = 1 / max(1, abs(demand_left)/smax_left, abs(demand_right)/smax_right)
effort = sign(demand) * (b + a * abs(demand * scale))
```

Zero wheel demand gives exactly zero effort, and each output is bounded by
[-1,1]. Shared scaling preserves requested curvature when effort saturates;
the model then reports `linear*scale`, `angular*scale`, not impossible input
speeds. Very large/nonfinite inputs are handled without nonfinite motor output.
The positive affine intercept implies a step from zero to nonzero effort.
It is an inferred friction term, **not a measured breakaway threshold**.
Slow motion below the accepted lower-speed point may still stall. Reverse
uses assumed sign symmetry; mixed arcs and lower-rate turns use unverified
superposition/interpolation. No constant trim is added at rest or to all turns.

The [controller profile](../kabot_ros2/kabot_robot/bringup/config/kabot_calibrated_controllers.yaml)
uses nominal radius 0.016 m and separation 0.102 m in both controllers.
Keep these two geometry settings equal. Configure calibration before launch;
the model reads it during configuration, not continuously during operation.

```text
Twist -> stamper -> standard diff_drive
      -> kabot_motion_model/{left,right}_wheel_joint/velocity references
      -> empirical model -> hardware effort -> Zenbedded -> firmware

model velocities -> diff_drive odom + odom->base_link TF
model positions  -> /joint_states -> robot_state_publisher -> wheel TF
raw encoders     -> /kabot_state_broadcaster/{names,values} (diagnostics only)
```

Diff-drive uses `open_loop: false`, `position_feedback: false` solely to consume
the **model-exported velocities**, including saturation. It does not read the
suspect encoder. Wheel positions/velocities in `/joint_states` are virtual
kinematic values for visualization, not measured shaft states. Its `effort`
field is normalized duty, not Nm. `/kabot_motion_model/controller_state` shows
requested wheel velocity, modeled velocity, their difference and output effort.

RViz uses `odom` as Fixed Frame and the existing robot/grid view. Both the
displayed pose and wheel motion can advance while the real robot is unclaimed,
stalled or disconnected: the model does not observe claim/traction. Do not use
this digital twin as independent localization or evidence of physical motion.

## Historical Raw Mode And Tests

For the old `pixi run calibrate` commands only, stop the current launch and use
`pixi run native -- zenoh_endpoint:=tcp/192.168.0.105:7447 calibrated:=false`.
That preserves FF-only PID mapping and raw command odometry. The two scripts
reject the wrong active profile; never silently reinterpret historical speeds.
Default teleop values are now for calibrated mode, not raw calibration.

Host-only tests use isolated mock hardware, never the physical robot:

```bash
pixi run build-native
pixi run colcon test --packages-select kabot_robot --ctest-args -R '^kabot_(motion_calibration|calibrated_motion_chain|drive_loop|calibrate_motion|native_pid|native_open_loop)$' --output-on-failure
pixi run colcon test-result --test-result-base build/kabot_robot/test_results --verbose
```

Coverage includes all four anchors, assumed reverse, bounded outputs, shared
saturation, timeout/stale stamps, model odometry with nonzero fake encoders,
wheel TF, the six-segment sequence and cleanup. Mock route closure tolerances
are 5 cm / 0.3 rad for scheduling; they are not physical accuracy specifications.
No physical run was performed for this implementation. The separate `view`
was subsequently checked under Xvfb with mock hardware: visible model motion,
RViz status OK, no extra state publishers and clean process shutdown. The
calibrated-chain test also checks that TF pose equals same-timestamp odometry.