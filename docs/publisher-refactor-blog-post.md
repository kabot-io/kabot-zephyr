# Publisher Refactor Blog Post

Date: June 3, 2026

## Why This Refactor Happened

Our real and simulated state publishers had drifted into multiple styles over time:

- mixed devicetree lookup patterns (`DT_COMPAT_GET_ANY_STATUS_OKAY` vs aliases)
- inconsistent startup readiness behavior
- different publish timeout conventions (`20 ms` caps/constants vs per-publisher config)
- duplicated sensor value conversion helpers
- host plot scales mismatched with the latest real sensor behavior

The goal of this refactor was to converge publishers onto one consistent pattern while preserving firmware data-flow architecture and avoiding API breaks.

Architecture references:

- `docs/firmware-data-flow.md`
- `docs/real-sensor-publisher-tutorial.md`

## The Target Pattern

For each real sensor publisher:

1. Resolve the sensor via a named devicetree alias.
2. Assert the alias exists and is enabled at build time.
3. Retry startup readiness using shared retry cadence config.
4. Fetch sample and channel values using Zephyr sensor API.
5. Publish a partial `State` fragment using publisher period config as timeout.
6. Sleep by the same configured period.

### Code Samples For The 6 Steps

#### 1) Resolve the sensor via a named devicetree alias

```c
const struct device *imu = DEVICE_DT_GET(DT_ALIAS(kabot_imu));
```

Distance and magnetometer follow the same pattern:

```c
const struct device *tof = DEVICE_DT_GET(DT_ALIAS(kabot_distance));
const struct device *mag = DEVICE_DT_GET(DT_ALIAS(kabot_mag));
```

#### 2) Assert the alias exists and is enabled at build time

```c
BUILD_ASSERT(DT_HAS_ALIAS(kabot_imu),
       "No devicetree alias 'kabot-imu' found for IMU publisher");
BUILD_ASSERT(DT_NODE_HAS_STATUS_OKAY(DT_ALIAS(kabot_imu)),
       "IMU devicetree node is not enabled");
```

#### 3) Retry startup readiness using shared retry cadence config

```c
while (!device_is_ready(imu)) {
  LOG_ERR("ICM42X70 device not ready: %s. Retrying...", imu->name);
  k_sleep(K_MSEC(CONFIG_KABOT_SENSOR_RETRY_PERIOD_MS));
}
```

#### 4) Fetch sample and channel values using Zephyr sensor API

```c
struct sensor_value accel_xyz[3] = {0};
int rc = sensor_sample_fetch(imu);
if (rc != 0) {
  LOG_WRN("sensor_sample_fetch failed: %d", rc);
  k_sleep(K_MSEC(CONFIG_KABOT_STATE_IMU_PERIOD_MS));
  continue;
}

rc = sensor_channel_get(imu, SENSOR_CHAN_ACCEL_XYZ, accel_xyz);
if (rc != 0) {
  LOG_WRN("sensor_channel_get(SENSOR_CHAN_ACCEL_XYZ) failed: %d", rc);
  k_sleep(K_MSEC(CONFIG_KABOT_STATE_IMU_PERIOD_MS));
  continue;
}
```

#### 5) Publish a partial `State` fragment using publisher period config as timeout

```c
State state = State_init_zero;
const uint64_t stamp = state_now_stamp_ms();

state.has_linear_acceleration = true;
state.linear_acceleration.has_header = true;
state.linear_acceleration.header.stamp = stamp;
set_header_frame_id(&state.linear_acceleration.header, CONFIG_KABOT_STATE_IMU_FRAME_ID);
state.linear_acceleration.has_state = true;
state.linear_acceleration.state.x = sensor_value_to_float(&accel_xyz[0]);
state.linear_acceleration.state.y = sensor_value_to_float(&accel_xyz[1]);
state.linear_acceleration.state.z = sensor_value_to_float(&accel_xyz[2]);

rc = publish_state_msg(&state, K_MSEC(CONFIG_KABOT_STATE_IMU_PERIOD_MS));
if (rc != 0) {
  LOG_WRN("Failed to publish IMU state: %d", rc);
}
```

#### 6) Sleep by the same configured period

```c
k_sleep(K_MSEC(CONFIG_KABOT_STATE_IMU_PERIOD_MS));
```

### Kconfig Mapping And Conventions

Startup retry cadence (shared across real publishers):

- `CONFIG_KABOT_SENSOR_RETRY_PERIOD_MS`

Publisher cadence and publish timeout source (per publisher):

- IMU publisher: `CONFIG_KABOT_STATE_IMU_PERIOD_MS`
- Magnetometer publisher: `CONFIG_KABOT_STATE_MAG_PERIOD_MS`
- Distance publisher: `CONFIG_KABOT_STATE_DISTANCE_PERIOD_MS`

Thread stack conventions:

- Distance publisher: `CONFIG_KABOT_DISTANCE_PUBLISHER_STACK_SIZE`
- IMU publisher: `CONFIG_KABOT_IMU_PUBLISHER_STACK_SIZE`
- Magnetometer publisher: `CONFIG_KABOT_MAGNETOMETER_PUBLISHER_STACK_SIZE`

Timeout convention used in this refactor:

- Use each publisher's configured period directly for `publish_state_msg(..., K_MSEC(...))`.
- Do not introduce hardcoded publish timeout constants.
- Do not cap publish timeout with local ad-hoc `min` logic.
- Keep timeout and `k_sleep` period aligned to the same Kconfig period symbol.

This keeps behavior predictable and removes hidden timing differences between publishers.

For simulated publishers:

- Use each publisher's configured period directly for publish timeout (no hardcoded constants/caps).
- Keep produced value ranges aligned with current real-sensor semantics.

## Subscriber and Publisher Commonality

Another outcome from this refactor is a stronger convention: implementation patterns for zbus workers should stay as common as possible across modules.

Practical reasons:

- lower review overhead (same control flow in each worker)
- easier debugging and bring-up (known-good structure)
- less copy-paste drift in timeout/retry/error handling

Near-term direction:

- keep publishers and subscribers on the same structural template
- centralize repeated helper logic in utility headers where possible

Future direction (recommended):

- evaluate macro helpers for repetitive boilerplate similar to `K_THREAD_DEFINE`
- possible targets include alias asserts, readiness retry loops, and publish-loop scaffolding

The goal is not to hide behavior, but to make the common behavior explicit and mechanically consistent.

## What Changed

### 1) Shared Config Knobs

Added common sensor startup retry config:

- `app/Kconfig`
  - `CONFIG_KABOT_SENSOR_RETRY_PERIOD_MS`

Added dedicated distance publisher stack config:

- `app/Kconfig`
  - `CONFIG_KABOT_DISTANCE_PUBLISHER_STACK_SIZE`

### 2) Devicetree Alias Wiring (Board Overlay)

Added explicit aliases for real sensors:

- `app/boards/esp32s3_devkitc_esp32s3_procpu.overlay`
  - `kabot-imu = &icm42670;`
  - `kabot-mag = &mmc5603;`
  - `kabot-distance = &vl53l0x;`
  - `kabot-temp = &coretemp;`

This makes publisher binding explicit and removes compatibility-based ambiguity.

### 3) Real Publisher Refactor

#### Distance publisher

- file: `app/src/zbus/state/distance_publisher.c`
- replaced compat lookup with alias-based lookup (`DT_ALIAS(kabot_distance)`)
- added build asserts for alias presence and status
- changed startup behavior to retry-until-ready loop
- removed local conversion helper; switched to `sensor_value_to_float`
- removed publish-timeout cap logic and now uses `CONFIG_KABOT_STATE_DISTANCE_PERIOD_MS`
- switched thread stack to `CONFIG_KABOT_DISTANCE_PUBLISHER_STACK_SIZE`

#### IMU publisher

- file: `app/src/zbus/state/imu_publisher.c`
- replaced compat fallback logic with alias-based lookup (`DT_ALIAS(kabot_imu)`)
- added build asserts for alias presence and status
- replaced fixed retry constant with `CONFIG_KABOT_SENSOR_RETRY_PERIOD_MS`
- removed publish-timeout cap logic
- switched numeric conversion to `sensor_value_to_float`

#### Magnetometer publisher

- file: `app/src/zbus/state/magnetometer_publisher.c`
- replaced compat lookup with alias-based lookup (`DT_ALIAS(kabot_mag)`)
- added build asserts for alias presence and status
- replaced fixed retry constant with `CONFIG_KABOT_SENSOR_RETRY_PERIOD_MS`
- removed publish-timeout cap logic
- removed local conversion helper and uses `sensor_value_to_float`
- kept physical sensor behavior intact (publishes raw Zephyr magnetometer channel values)

## Sim Publisher Alignment

### Timeout style normalization

Updated all simulated state publishers to use each publisher's period config directly as publish timeout:

- `app/src/zbus/state/sim_distance_publisher.c`
- `app/src/zbus/state/sim_imu_publisher.c`
- `app/src/zbus/state/sim_magnetometer_publisher.c`

Removed:

- hardcoded `PUBLISH_TIMEOUT_MS = 20`
- distance-only timeout cap pattern

### Sim magnetometer range alignment

To match current real sensor behavior, simulated magnetic field range was changed from:

- `[-60.0, 60.0]`

to:

- `[-0.6, 0.6]`

File:

- `app/src/zbus/state/sim_magnetometer_publisher.c`

## HMI Plot Scale Alignment

Because magnetic field values are now displayed in the same scale as real sensor output, HMI plot limits were updated.

File:

- `scripts/kabot_io/view.py`

Change:

- `Magnetic Field` plot y-axis from `(-200.0, 50.0)` to `(-0.8, 0.8)`

## Behavior and Contract Notes

- No protobuf schema changes were introduced.
- No zbus channel contract changes were introduced.
- State merge semantics remain unchanged.
- Refactor focuses on implementation consistency and operational robustness.

## Validation

Validation command:

- `./scripts/build.zsh --no-flash`

Result:

- build completed successfully for `esp32s3_devkitc`

## Lessons Learned

1. Alias-based sensor binding is clearer and safer than ad-hoc compatibility scans.
2. Startup retry behavior should be standardized across all real publishers.
3. Timeout style consistency makes publisher behavior easier to reason about.
4. Any sensor semantic choice should be reflected in both simulation and HMI scale.

## Follow-Up Ideas

- Add a short "publisher compliance checklist" to `docs/real-sensor-publisher-tutorial.md`.
- Add a unit/semantic note for `magnetic_field` in host docs to reduce ambiguity.
- Consider per-publisher stack-size symbols for all real publishers for symmetry.
