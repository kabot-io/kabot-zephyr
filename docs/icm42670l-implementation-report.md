# ICM42670L IMU Bring-Up Report

## Why This Report Exists

This is a practical implementation report for adding ICM42670L support to kabot firmware.

It follows the state egress architecture and records:

- what was implemented in app and devicetree
- what was patched in Zephyr driver
- why WHO_AM_I became the key blocker
- how to keep that patch under version control with west
- what to validate next on hardware

## Architecture Baseline

The implementation follows the current state egress architecture:

- sensor publisher thread reads hardware through Zephyr sensor API
- publisher emits partial State fragments to state_channel
- aggregator merges by per-field timestamps
- periodic publisher emits full machine-mirror state snapshot

Reference:

- docs/real-sensor-publisher-tutorial.md
- docs/firmware-data-flow.md

## Implementation Summary

### 1. Real IMU Publisher Added

A new real publisher thread was added:

- app/src/zbus/state/imu_publisher.c

Behavior:

- resolves IMU from devicetree using compatible lookup for invensense,icm42670s or invensense,icm42670p
- retries until device_is_ready
- reads with sensor_sample_fetch + sensor_channel_get for accel and gyro
- fills State.linear_acceleration and State.angular_velocity with a shared stamp/frame id
- publishes to state_channel at CONFIG_KABOT_STATE_IMU_PERIOD_MS

Snippet:

```c
rc = sensor_channel_get(imu, SENSOR_CHAN_ACCEL_XYZ, accel_xyz);
...
rc = sensor_channel_get(imu, SENSOR_CHAN_GYRO_XYZ, gyro_xyz);
...
state.has_linear_acceleration = true;
state.linear_acceleration.header.stamp = stamp;
state.has_angular_velocity = true;
state.angular_velocity.header.stamp = stamp;
```

### 2. Build and Kconfig Wiring

Added Kconfig gates:

- app/Kconfig
  - KABOT_ENABLE_IMU_PUBLISHER
  - KABOT_IMU_PUBLISHER_STACK_SIZE

Added CMake wiring:

- app/CMakeLists.txt includes src/zbus/state/imu_publisher.c when CONFIG_KABOT_ENABLE_IMU_PUBLISHER is enabled.

Board config updates:

- app/boards/esp32s3_devkitc_esp32s3_procpu.conf
  - disables simulated IMU publisher
  - enables real IMU publisher
  - enables ICM42X70 driver

Snippet:

```ini
CONFIG_KABOT_ENABLE_SIMULATED_IMU_PUBLISHER=n
CONFIG_KABOT_ENABLE_IMU_PUBLISHER=y
CONFIG_ICM42X70=y
```

### 3. Devicetree Integration

IMU node was added on I2C0:

- app/boards/esp32s3_devkitc_esp32s3_procpu.overlay
- node: icm42670@68
- compatible: invensense,icm42670s
- address: 0x68
- no int-gpios property (I2C-only wiring)

Snippet:

```dts
&i2c0 {
    icm42670: icm42670@68 {
        compatible = "invensense,icm42670s";
        reg = <0x68>;
        accel-hz = <100>;
        gyro-hz = <100>;
        accel-fs = <16>;
        gyro-fs = <2000>;
    };
};
```

## Interesting Catch: WHO_AM_I Mismatch on ICM42670L

Observed failure mode:

- hardware reports WHO_AM_I = 0x63
- selected ICM42X70 path expected 0x69 for ICM42670S
- strict equality in Zephyr init rejected the sensor with -ENOTSUP

This is why the IMU could still fail after bus/devicetree/publisher wiring looked correct.

## Zephyr Driver Fix Implemented

Patch location:

- deps/zephyr/drivers/sensor/tdk/icm42x70/icm42x70.c

Patch behavior:

- keep strict match for normal case
- additionally accept WHO_AM_I 0x63 only when expected family is ICM42670P/S
- keep -ENOTSUP for unrelated devices
- emit warning log when fallback path is used

Key snippet:

```c
whoami_ok = (data->chip_id == data->imu_whoami) ||
            ((data->chip_id == 0x63) &&
             ((data->imu_whoami == INV_ICM42670P_WHOAMI) ||
              (data->imu_whoami == INV_ICM42670S_WHOAMI)));

if (!whoami_ok) {
    return -ENOTSUP;
}
```

## Can West Keep This Patch With The Project?

Short answer: yes, but west does not natively manage a patch queue for imported modules.

Recommended approach:

1. Keep Zephyr patch in a branch/fork.
2. Point west.yml zephyr project revision to that commit/branch.
3. Commit west.yml change in the app repository.

Example manifest strategy:

```yaml
- name: zephyr
  remote: your-zephyr-remote
  revision: feature/icm42670l-whoami-compat
  path: deps/zephyr
```

Why this is best:

- patch is reproducible from clean checkout
- no post-sync mutation step needed
- CI and teammates resolve the same zephyr commit

Alternative (works but less robust):

- store a patch file in this repo and apply it after west update via script.
- this can fail when upstream context shifts, so it requires maintenance.

## Suggested Version Control Workflow

1. Commit app-side IMU wiring in this repository.
2. Commit Zephyr driver fix in your Zephyr fork/branch.
3. Pin west.yml zephyr revision to that commit.
4. In CI, run west update, then build.

This gives deterministic source + deterministic third-party dependencies.

## Current Outcome

- Real IMU publisher path is integrated.
- IMU node is present in devicetree.
- WHO_AM_I mismatch path has a compatibility fix in Zephyr driver.
- Build succeeds.

## Remaining Assumptions And Risks

- I2C address is currently set to 0x68. If your board straps AD0 high, it may be 0x69.
- This fix intentionally broadens acceptance only for ICM42670 family expectation paths.
- Long-term upstreaming of ICM42670L identity handling would reduce local patch burden.

## Appendix A: Full Zephyr Driver Patch (Markdown Diff)

```diff
diff --git a/drivers/sensor/tdk/icm42x70/icm42x70.c b/drivers/sensor/tdk/icm42x70/icm42x70.c
index a6341ce15..12ed33b34 100644
--- a/drivers/sensor/tdk/icm42x70/icm42x70.c
+++ b/drivers/sensor/tdk/icm42x70/icm42x70.c
@@ -29,6 +29,7 @@ LOG_MODULE_REGISTER(ICM42X70, CONFIG_SENSOR_LOG_LEVEL);
 /* Maximum bytes to read/write on ICM42X70 serial interface */
 #define ICM42X70_SERIAL_INTERFACE_MAX_READ  (1024 * 32)
 #define ICM42X70_SERIAL_INTERFACE_MAX_WRITE (1024 * 32)
+#define ICM42670L_WHOAMI_COMPAT_ID         0x63
 
 static inline int icm42x70_reg_read(const struct device *dev, uint8_t reg, uint8_t *buf,
                                     uint32_t size)
@@ -346,6 +347,7 @@ static int icm42x70_sensor_init(const struct device *dev)
 {
        struct icm42x70_data *data = dev->data;
        const struct icm42x70_config *config = dev->config;
+       bool whoami_ok;
        int err = 0;
 
        /* Initialize serial interface and device */
@@ -367,12 +369,24 @@ static int icm42x70_sensor_init(const struct device *dev)
                return err;
        }
 
-       if (data->chip_id != data->imu_whoami) {
+       whoami_ok = (data->chip_id == data->imu_whoami) ||
+                   ((data->chip_id == ICM42670L_WHOAMI_COMPAT_ID) &&
+                    ((data->imu_whoami == INV_ICM42670P_WHOAMI) ||
+                     (data->imu_whoami == INV_ICM42670S_WHOAMI)));
+
+       if (!whoami_ok) {
                LOG_ERR("invalid WHO_AM_I value, was 0x%x but expected 0x%x for %s", data->chip_id,
                        data->imu_whoami, data->imu_name);
                return -ENOTSUP;
        }
 
+       if ((data->chip_id == ICM42670L_WHOAMI_COMPAT_ID) &&
+           ((data->imu_whoami == INV_ICM42670P_WHOAMI) ||
+            (data->imu_whoami == INV_ICM42670S_WHOAMI))) {
+               LOG_WRN("WHO_AM_I fallback accepted: got 0x%x for %s", data->chip_id,
+                       data->imu_name);
+       }
+
        LOG_DBG("\"%s\" %s OK", dev->name, data->imu_name);
        return 0;
 }
```
