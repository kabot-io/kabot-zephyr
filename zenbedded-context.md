# Zenbedded External Zephyr Integration Handoff

Date: 2026-10-03
Workspace: `/workspaces/kabot-zephyr`
Zenbedded checkout: `/workspaces/kabot-zephyr/deps/modules/zenbedded`

## User Intent and Work Status

- The user added Zenbedded to Kabot's west manifest and wants to use it in the robot firmware.
- They asked us to study the sine-wave and inverted-pendulum demos, then asked what should change upstream to make Zenbedded easier to consume as a Zephyr module.
- The user has write permissions to Zenbedded. Kabot is its first integration into an external project.
- We recommended fixing upstream packaging before adding consumer-side workarounds to Kabot.
- The latest request was to create this context handoff, not to implement the recommendations yet.
- Prior work was read-only. No firmware, manifest, library, or build configuration changes were made by this agent. This handoff is the only file added.
- No builds, tests, dependency updates, flashing, or runtime verification were performed.
- No commits or branches were created. Existing worktree changes were not inventoried; inspect both repositories before editing and preserve user changes.

## Current Manifest and Checkout

Kabot's [west.yml](west.yml) contains:

```yaml
- name: zenbedded
  remote: ros-controls
  revision: zenbedded-as-zephyr-module
  path: deps/modules/zenbedded
```

Zenbedded has its own `.git/` directory. Treat upstream library work separately from the parent Kabot repository.

The nested [dependency manifest](deps/modules/zenbedded/zenbedded_transport/west.yml) declares:

```yaml
- name: zenoh-pico
  remote: ros-controls
  revision: zenbedded
  path: modules/lib/zenoh-pico
  clone-depth: 1
```

Kabot's Zenbedded project entry does not import that manifest. No Zenoh-pico entry was found in the inspected Zephyr manifest, and the inspected `deps/modules/lib` directory contained nanopb, picolibc, and zcbor, not Zenoh-pico. This was not an exhaustive filesystem search.

West dependency imports and Zephyr module discovery are separate mechanisms. Adding module metadata does not fetch Zenoh-pico. Preserve the consuming project's ability to choose dependency paths and its own Zephyr version.

## Findings: Module Packaging

### Discovery

- The repository root has no Zephyr module entry point in the inspected checkout.
- Each nested component has its own metadata:
  - [Transport metadata](deps/modules/zenbedded/zenbedded_transport/zephyr/module.yml#L1): name `zenbedded_transport`, CMake `.`, Kconfig `Kconfig`.
  - [RCL metadata](deps/modules/zenbedded/zenbedded_rcl/zephyr/module.yml#L1): name `zenbedded_rcl`, CMake `.`, Kconfig `Kconfig`.
- Zephyr's [module discovery implementation](deps/zephyr/scripts/zephyr_module.py#L200) checks a project's root for `zephyr/module.yml` or the conventional Zephyr CMake/Kconfig pair. It does not recursively discover both nested modules.
- The demos compensate by appending both internal component directories to `ZEPHYR_EXTRA_MODULES` before `find_package(Zephyr)`.
- The sine-wave build additionally supplies a manual RCL include directory, although the RCL build already exports its public include directory.

### Enablement and Configuration

- [Transport CMake](deps/modules/zenbedded/zenbedded_transport/CMakeLists.txt#L3) unconditionally creates and builds the transport library when registered.
- [Transport Kconfig](deps/modules/zenbedded/zenbedded_transport/Kconfig#L11) has an unguarded integration-tier choice, defaulting to Tier 1 and selecting networking/Zenoh dependencies.
- It also introduces application-wide defaults for packet buffers, a 65536-byte heap, a 16384-byte main stack, and an 8192-byte system workqueue stack. Do not assume these necessarily win Kconfig precedence; the issue is exposing library-wide application policy without an enable guard.
- [RCL Kconfig](deps/modules/zenbedded/zenbedded_rcl/Kconfig#L3) defaults `ZENBEDDED_RCL` to `y` without expressing its C++ requirements.
- [RCL CMake](deps/modules/zenbedded/zenbedded_rcl/CMakeLists.txt#L1) is conditional on `CONFIG_ZENBEDDED_RCL` and exports its include directory. Its hardware-component source is effectively empty; the client source holds the implementation.
- C transport and C++ RCL are already separate components. Preserve the ability to use the C transport without enabling C++.

### Schema Generation

- Tier 2 generates `zenbedded_state_t` and `zenbedded_command_t` from YAML into a build-tree header, included as `<zenbedded_transport/generated/interface_data.h>`.
- [Transport CMake](deps/modules/zenbedded/zenbedded_transport/CMakeLists.txt#L20) reads `CONFIG_ZENBEDDED_TIER2_SCHEMA_PATH`, joining it to `APPLICATION_SOURCE_DIR`.
- The Kconfig default is `config/interface_schema.yaml`.
- CMake's conventional-path fallbacks are only reached when the configured path is absent/empty, not when a nonempty configured path points to a missing file.
- The sine-wave CMake file sets `ZENBEDDED_SCHEMA_FILE`, but the inspected generator wiring does not consume that variable. Its working setting is the Kconfig schema path.
- The generator uses PyYAML, preserves YAML iteration order, and emits packed numeric structures and byte-size macros.
- Its custom command already depends on the schema and generator script. Preserve and test regeneration behavior.
- RawCodec copies struct bytes without CDR framing or byte-order conversion. Both ends must agree on field order, types, layout, endianness, and units. Do not describe this as protobuf or automatic schema negotiation.

## Recommended First Upstream PR

These are recommendations, not implemented or approved design decisions.

### 1. One Root Zephyr Entry Point

Proposed new layout:

```text
zephyr/
  module.yml
  CMakeLists.txt
  Kconfig
```

Have that root entry point conditionally include the existing transport and RCL components. Avoid moving their source files or restructuring the ROS packages just to add discovery.

The desired external application experience is: declare dependencies through west, enable Kconfig options, include public headers. No internal path references, manual include paths, or nested `ZEPHYR_EXTRA_MODULES` entries should be required.

Migrate the demos to exercise the same root entry point. Decide how to retain or retire nested metadata without registering the same sources or Kconfig definitions twice, including in the existing Docker workspace.

### 2. Passive Discovery and Explicit Enablement

- Introduce an explicit top-level enable option, for example `CONFIG_ZENBEDDED`, defaulting to `n`.
- Gate compilation, tier choices, and related options appropriately.
- Express RCL's C++17 requirements using the supported Zephyr Kconfig conventions; do not indiscriminately select dependency-bearing symbols.
- Keep C-only transport supported.
- Move sample-specific memory/stack sizing to sample configurations where appropriate; scope any genuine library defaults to enablement.
- Merely fetching Zenbedded must not enable networking, C++, schema generation, or library code in unrelated applications or MCUboot images.

### 3. Documented Dependency Contract

- Provide a dependency-only root west manifest, potentially importing the existing nested manifest.
- Document that the consumer must explicitly import it and how to relocate dependencies using west import prefixes.
- Document the required Zenoh-pico fork, tested revision, supported Zephyr versions, and any remaining patches.
- Prefer a tested immutable dependency revision for releases.
- Do not pull in or replace the consumer's Zephyr project as a library dependency.

### 4. One Schema Configuration Mechanism

- Keep `CONFIG_ZENBEDDED_TIER2_SCHEMA_PATH` as the initial canonical mechanism unless there is a clear reason to change it.
- Support application-relative and absolute paths explicitly.
- Remove or reconcile unused sample settings such as `ZENBEDDED_SCHEMA_FILE`.
- Report the resolved path on failure and document PyYAML requirements.
- Preserve generated-header build ordering and incremental regeneration.

### 5. External-Consumer Smoke Test

- Create a clean west workspace with a minimal application outside the library checkout.
- Use root discovery, not hard-coded paths into nested components.
- Test module-disabled builds, C-only transport, Tier 2 C++ usage, and schema regeneration.
- Include an ESP32-S3 build and suitable native_sim coverage where supported. Distinguish build-only tests from network/runtime tests.
- Do not rely on the development container's preconfigured module symlinks, a ROS install, or prebuilt host artifacts to build the firmware consumer.
- Consider a disabled child-image check because Kabot uses MCUboot/sysbuild.
- Preserve existing demo coverage and add root packaging files to CI path triggers.

Existing [Twister CI](deps/modules/zenbedded/.github/workflows/zephyr-twister.yml#L1) runs inside the project Docker environment, builds ROS packages first, and tests demos on native_sim/native/64 and esp32s3_devkitc/esp32s3/procpu. It does not currently prove clean external-consumer packaging.

## Demo Behavior Studied

### Sine Wave

- [Firmware](deps/modules/zenbedded/demos/sine_wave/sine_wave_zephyr/src/main.cpp#L1) uses `ZenbeddedClient<RawCodec<zenbedded_state_t>, RawCodec<zenbedded_command_t>>`.
- State is `sine_wave.position: float64`; command is `sine_wave.amplitude: float64`.
- Firmware initializes the client with 100 Hz, but updates state in a loop sleeping 20 ms, so publication can repeat a sample.
- The generated signal has default amplitude 5 and frequency 1.4 Hz. Commands adjust its amplitude using absolute value after the first nontrivial command.
- Startup retries stored Wi-Fi connection requests, then waits for preferred IPv4 without a timeout, disables ESP32 Wi-Fi power saving, and initializes the client.
- The ROS side uses `zenbedded_hardware_interface/ZenbeddedHardware`, the same schema, and a 100 Hz controller manager. Its sine-wave controller actually sends amplitude commands.
- The README's state-only description and network configuration instructions lag behind the implementation.
- Demo firmware and host endpoint addresses differ in the inspected configuration. They need deployment-specific configuration, not blind copying.

### Inverted Pendulum

- `inverted_pendulum_zephyr` contains configuration only in this checkout. Executable firmware lives in Tier 1 and Tier 2 directories.
- [Tier 1 firmware](deps/modules/zenbedded/demos/inverted_pendulum/inverted_pendulum_tier1/src/main.cpp#L1) uses `JointStateCodec` and `JointCommandCodec`, publishes two joint positions at 50 Hz, and applies stepper velocity commands in a nominal 10 ms loop.
- Tier 1 obtains NTP time for ROS timestamps, but continues if synchronization fails.
- [Tier 2 firmware](deps/modules/zenbedded/demos/inverted_pendulum/inverted_pendulum_tier2/src/main.cpp#L1) publishes motor and pendulum position/velocity, and receives motor acceleration.
- Tier 2 samples the encoder at 400 Hz with velocity filtering, runs a nominal 100 Hz application loop, integrates acceleration into velocity, limits velocity to 7 rad/s, and constrains stepper travel to +/-135 degrees from startup zero.
- The Tier 2 ROS controller uses full-state feedback and sends bounded acceleration while the pendulum is within 20 degrees of the balance target.
- Both firmware tiers use `accepted_command_count()` and cycle counting for an approximately 200 ms command timeout.
- Tier 2 waits on its encoder queue with `K_FOREVER` inside the same loop that checks command timeout. Sensor failure can delay that safety check indefinitely. Do not copy this as a production watchdog design.

## Client and Transport Semantics

- [Client API](deps/modules/zenbedded/zenbedded_rcl/include/zenbedded_rcl/zenbedded_client.hpp#L1): `init(frequency)` initializes transport and starts a background state-publication thread.
- `write_state()` updates a double buffer; it does not perform the network publish itself.
- `read_command()` reads the latest cached payload. Success does not mean a new command arrived, and initial buffers are zero-filled.
- `accepted_command_count()` increments for transport payloads of the expected size; this is not semantic validation or proof that a host controller is healthy.
- [Client implementation](deps/modules/zenbedded/zenbedded_rcl/src/zenbedded_client.cpp#L1) spins until a consistent buffer read succeeds. Do not promise a hard bounded read time.
- The RCL currently takes endpoint, topics, domain, and node name from Kconfig; the lower-level C transport already accepts several runtime configuration arguments.
- Partial initialization can fail after transport/publisher creation but before `initialized_` is set. `destroy()` returns immediately when that flag is false, so cleanup/retry behavior needs attention before implementing robust reconnects.
- `init(0)` is not currently a supported way to disable background publishing: thread startup rejects zero frequency.

Suggested later API work, separate from packaging:

1. Runtime endpoint/topic configuration with Kconfig defaults.
2. A command read API returning payload and freshness metadata from a consistent snapshot.
3. Complete partial-initialization cleanup and tested retry behavior.
4. Clear threading, lifecycle, and sample-freshness documentation.

Keep Wi-Fi provisioning, robot ownership, and motor safety policy in the application.

## Kabot Integration Implications

Tier 2 was recommended as the closest fit, using a small C++ adapter while leaving the existing application and drivers in C:

```text
Sensor publishers -> zbus state -> Zenbedded adapter -> ROS hardware plugin
ROS controller -> Zenbedded adapter -> control_channel -> motor subscriber
```

- [Application build](app/CMakeLists.txt#L1) currently registers local motor/LED modules, generates nanopb messages, and builds C services.
- [Startup](app/src/main.c#L1) initializes settings, sensor subscription, UDP control, and discovery services via `SYS_INIT`; the main thread sleeps.
- [State UDP sender](app/src/zbus/state/state_udp_sender.c#L1) reads aggregated egress state, checks claim/network readiness, encodes protobuf, and sends to the selected HMI target.
- State egress is configured at 100 ms in [app/prj.conf](app/prj.conf#L1). A 100 Hz Zenbedded publisher alone will not make sensors or this egress stream update at 100 Hz.
- [Control ingress](app/src/control/control_service.c#L1) decodes UDP protobuf and publishes to `control_channel`.
- [Control channel](app/src/zbus/channels/control_channel.c#L1) validates left/right normalized effort and notifies motor/effort-state subscribers.
- [Motor subscriber](app/src/zbus/control/effort_subscriber.c#L1) checks `robot_settings_is_claimed()` and independently stops motors if commands stop arriving within `CONFIG_KABOT_CONTROL_WATCHDOG_MS`.
- Forward only newly accepted, semantically valid Zenbedded commands to zbus. Re-publishing a cached command periodically would defeat that watchdog.
- Ownership remains unresolved: replace UDP control, or coexist with explicit source arbitration. Zenbedded does not automatically participate in the current HMI claim protocol. Do not silently bypass it.
- [Actual motor API](modules/motor_driver/include/motor/motor_driver.h#L25) accepts float normalized effort in `[-1, 1]`, not Q31, torque in Nm, or wheel velocity. The inspected motor API documentation is stale about Q31.
- Define ROS interface units explicitly. A velocity command requires a controller/conversion; do not directly treat it as normalized effort.
- [Current protobuf schema](app/protos/state_control_msg.proto#L1) contains effort and various IMU, magnetometer, distance, light, and electrical state fields. It does not expose wheel joint position/velocity fields. Do not assume those are ready for ROS wheel feedback merely because encoder sources exist.

## Next-Agent Starting Point

1. Confirm whether the next user request authorizes implementing the upstream packaging PR, Kabot integration, or further design discussion.
2. Read applicable repository instructions and inspect parent/nested worktree status. This handoff is historical evidence, not a guarantee files are unchanged.
3. For packaging, begin with root module discovery and opt-in enablement, using a minimal external-consumer configuration/build as the discriminating check.
4. Keep upstream and consumer edits scoped separately. Do not commit, push, change branches, flash hardware, or redesign ownership without authorization.
5. Report exactly which configurations were built or tested and which networking/hardware behaviors remain unverified.
