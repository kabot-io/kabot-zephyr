# Documentation Index

This page is the entry point for project documentation under `docs/`.

For project setup and day-1 usage, start from the root README:

- [../README.md](../README.md)

## Core Terms

- Machine mirror: the merged latest robot `State` snapshot assembled from partial state fragments.
- zbus: Zephyr's channel-based publish/subscribe messaging used to decouple producers and consumers.
- Q31 effort: signed 32-bit effort representation where `INT32_MIN` is full reverse, `0` neutral, and `INT32_MAX` full forward.

## Start Here

- [firmware-data-flow.md](firmware-data-flow.md) - firmware ingress/egress architecture and state merge policy.
- [hmi-architecture.md](hmi-architecture.md) - reference host HMI architecture and runtime behavior.
- [robot-discovery-and-binding-spec.md](robot-discovery-and-binding-spec.md) - normative discovery and state binding contract.
- [real-sensor-publisher-tutorial.md](real-sensor-publisher-tutorial.md) - end-to-end sensor publisher implementation guide.
- [quality-gates-checklist.md](quality-gates-checklist.md) - integration quality checklist before release.
- [esp32s3-mcuboot-flash-recovery.md](esp32s3-mcuboot-flash-recovery.md) - ESP32-S3 recovery and full-chain flashing workflow.

## API and App Usage

- [motor-api.md](motor-api.md)
- [motor-app-usage.md](motor-app-usage.md)
- [motor-driver-development.md](motor-driver-development.md)
- [led-strip-api.md](led-strip-api.md)
- [led-strip-app-usage.md](led-strip-app-usage.md)

## Integration Tutorials and Refactors

- [real-sensor-publisher-tutorial.md](real-sensor-publisher-tutorial.md)
- [publisher-refactor-blog-post.md](publisher-refactor-blog-post.md)
- [invalid-sample-skip-refactor.md](invalid-sample-skip-refactor.md)

## Sensor Implementation Reports

- [magnetometer-implementation-report.md](magnetometer-implementation-report.md)
- [icm42670l-implementation-report.md](icm42670l-implementation-report.md)
- [light-sensor-implementation-report.md](light-sensor-implementation-report.md)
- [ina219-implementation-report.md](ina219-implementation-report.md)
- [light-sensor-integration-blog-post.md](light-sensor-integration-blog-post.md)
- [ina219-integration-blog-post.md](ina219-integration-blog-post.md)

## HMI-Centric Documentation Package

- [hmi_centric_info/00_package_scope_and_ratio.md](hmi_centric_info/00_package_scope_and_ratio.md)
- [hmi_centric_info/01_hmi_core_architecture_current_stack.md](hmi_centric_info/01_hmi_core_architecture_current_stack.md)
- [hmi_centric_info/02_hmi_runtime_behavior_and_ux_contract.md](hmi_centric_info/02_hmi_runtime_behavior_and_ux_contract.md)
- [hmi_centric_info/03_hmi_target_stack_and_migration_guardrails.md](hmi_centric_info/03_hmi_target_stack_and_migration_guardrails.md)
- [hmi_centric_info/04_firmware_interface_for_hmi_developers.md](hmi_centric_info/04_firmware_interface_for_hmi_developers.md)
- [hmi_centric_info/05_system_data_flow_reference.md](hmi_centric_info/05_system_data_flow_reference.md)
- [hmi_centric_info/06_kabot_project_definition.md](hmi_centric_info/06_kabot_project_definition.md)
