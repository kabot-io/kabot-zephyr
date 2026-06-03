# Quality Gates Checklist

This checklist is used for sensor-integration changes that span firmware, devicetree, protobuf schema, and host tooling.

## 1. Build Gate

- [x] Run CI-equivalent firmware build:
  - `source .venv/bin/activate && ./scripts/build.zsh --no-flash`
- [x] Build completes without compile or link errors.

## 2. Devicetree Gate

- [x] Generated devicetree confirms expected nodes and aliases.
- [x] Distinct bus lineage for duplicated-address sensors behind mux.
- [x] No mux init-order assertion failures.

Evidence command:

- `source .venv/bin/activate && rg "ltr329_(left|right)|mux_i2c0_0_S_ltr329_29_BUS|mux_i2c1_1_S_ltr329_29_BUS" build/esp32s3_devkitc/zephyr/include/generated/zephyr/devicetree_generated.h`

## 3. Firmware Static Gate

- [x] Editor/compile diagnostics checked for modified C files.
- [x] No unresolved errors in new publisher, aggregator, or patched driver files.

## 4. Host Static Gate

- [x] Python syntax check for modified host files:
  - `source .venv/bin/activate && python -m py_compile scripts/kabot_io/model.py scripts/kabot_io/state_fields.py scripts/kabot_io/view.py`

## 5. State Contract Gate

- [x] Protobuf schema updated.
- [x] Firmware aggregation updated for new fields.
- [x] Host decode/mapping updated for new fields.

## 6. Runtime Hardware Gate (Required Before Final Release)

- [ ] Verify both devices initialize on real hardware.
- [ ] Verify left/right lux values diverge under asymmetric lighting.
- [ ] Verify both light fields appear in live state stream over time at expected cadence.
- [ ] Verify invalid conversion cycles are skipped and do not inject spike values.

Suggested commands:

1. `i2c scan i2c0`
2. `sensor get <left-light-device-name> light`
3. `sensor get <right-light-device-name> light`

## 7. Documentation Gate

- [x] Architecture docs updated.
- [x] Tutorial docs updated.
- [x] Implementation report added.
- [x] Blog post added.

## 8. Diff Hygiene Gate

- [ ] Review `git diff --stat` and `git diff` against main before commit.
- [ ] Ensure no unrelated changes are included.
- [ ] Ensure generated artifacts are not accidentally committed.

## Improvements Added After This Implementation

1. Added explicit venv precondition to all commands using west or Python tooling.
2. Added devicetree lineage evidence command as a hard requirement for muxed duplicate-address sensors.
3. Split runtime gate from build gate so hardware-only checks remain visible and not implied by successful compile.
4. Added schema parity checks across firmware and host as a single contract gate.
