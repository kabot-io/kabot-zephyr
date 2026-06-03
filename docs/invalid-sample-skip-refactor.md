# Refactor: Invalid Sample Skip Pattern Across Real State Publishers

Date: June 4, 2026

## Motivation

During dual-light integration, we observed conversion edge cases where a valid transport/read cycle could still produce an invalid interpreted measurement (`-EINVAL`) at `sensor_channel_get(...)` time.

When invalid interpreted samples are treated like normal values (for example by fallback conversion to `0`), downstream plots can show artificial spikes or sharp transients that do not represent physical sensor behavior.

The agreed policy is:

- if a measurement is flagged invalid at conversion stage, skip that sample
- do not publish replacement spike values
- preserve last good state in aggregator until a new valid sample arrives

## Scope of Refactor

This refactor applies the skip policy consistently to all real state publishers that consume Zephyr sensor channels:

- [app/src/zbus/state/imu_publisher.c](app/src/zbus/state/imu_publisher.c)
- [app/src/zbus/state/magnetometer_publisher.c](app/src/zbus/state/magnetometer_publisher.c)
- [app/src/zbus/state/distance_publisher.c](app/src/zbus/state/distance_publisher.c)
- [app/src/zbus/state/light_publisher.c](app/src/zbus/state/light_publisher.c)

Simulated publishers are intentionally unchanged because they do not perform hardware conversion reads.

## Design

### Shared helper

A single helper centralizes the policy decision:

- [app/include/zbus/state_publish_utils.h](app/include/zbus/state_publish_utils.h)

```c
static inline bool should_skip_invalid_sensor_sample(int rc)
{
    return rc == -EINVAL;
}
```

This keeps logic uniform and prevents per-publisher drift.

### Real publisher behavior

For `sensor_channel_get(...)` calls:

1. if `rc == -EINVAL`: skip current loop iteration as invalid sample
2. if `rc != 0` and not skippable: warn and continue with normal retry cadence
3. only publish when decoded values are valid for that publisher contract

### Light publisher behavior (dual-source)

Light publisher has two independent channels in one thread. Behavior:

- left invalid + right valid -> publish right only
- right invalid + left valid -> publish left only
- both invalid -> skip publish for this cycle

This is intentional to maximize useful throughput while avoiding invalid-value injection.

## Why this avoids spikes

The state aggregator is timestamp/field replacement based.

If a field is not published in a cycle, aggregator keeps previous valid value for that field.
Therefore, invalid conversion cycles become "no update" events instead of value transients.

## Minimality of code changes

Changes are deliberately small:

- one helper function added
- narrow branch insertion before existing non-zero error handling in each real publisher
- no changes to threading model
- no changes to zbus channels
- no protobuf contract changes
- no simulated publisher behavior changes

## Interaction with driver-level edge cases

For LTR55X high-ratio edge cases, driver returns `-EINVAL` for conversion step.
With this refactor, that naturally maps to skip behavior in publisher and avoids plotting artifacts.

## Observability and logging policy

- invalid-sample skip paths use debug-level logs where needed
- warning-level logs remain for transport/hardware failures that are actionable

This reduces log spam while preserving fault visibility.

## Validation checklist for this refactor

1. Build with venv + west:
   - `source .venv/bin/activate && ./scripts/build.zsh --no-flash`
2. Confirm no compile diagnostics in updated publishers and helper header.
3. On hardware, observe that invalid conversion cycles do not produce value spikes.
4. Confirm regular telemetry resumes immediately once valid samples return.

## Risks and tradeoffs

- Skipping invalid samples can create brief "flat" segments in plots (by design, last-good hold).
- This is preferred over publishing synthetic fallback values that misrepresent measurement.

## Follow-up recommendations

1. Keep `should_skip_invalid_sensor_sample()` as the only gate for this policy.
2. If additional skippable codes are proven safe across devices, extend helper centrally.
3. Add unit/integration tests around publisher skip behavior where practical.
