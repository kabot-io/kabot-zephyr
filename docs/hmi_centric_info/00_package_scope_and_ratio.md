# HMI-Centric Knowledge Package Scope

## Purpose

This package is a detailed technical onboarding set for:

- HMI developers joining the Kabot team
- UX and UI designers working with the current and future HMI
- New technical team members who need system-level context

The package describes the current system first and the target stack second.

## Coverage Rules Used In This Package

### Domain ratio across the full package

The global content weighting follows this ratio:

- HMI: 5
- Firmware: 2
- General system data flow: 2
- Project knowledge, what Kabot is: 1

### Current stack to target stack ratio

Across HMI-focused documents, stack description follows:

- Current stack: 4
- New stack: 1

That means the package is migration-aware but still primarily documents the running system in production development today.

## Current Stack Summary

- Host runtime: Python
- UI toolkit: Tkinter
- Plotting: Matplotlib
- Networking: UDP sockets for control and state
- Management channel: SMP over UDP via MCUmgr-compatible client libraries
- Serialization: Protobuf schema shared with firmware, Python code generated at runtime

## Target Stack Summary

- Backend remains Python for transport, protocol handling, and orchestration
- Frontend transitions to web technology
- Backend-frontend bridge uses Socket.IO
- Data model remains protobuf-first with controlled JSON metadata where needed

## Documents In This Package

1. 01_hmi_core_architecture_current_stack.md
2. 02_hmi_runtime_behavior_and_ux_contract.md
3. 03_hmi_target_stack_and_migration_guardrails.md
4. 04_firmware_interface_for_hmi_developers.md
5. 05_system_data_flow_reference.md
6. 06_kabot_project_definition.md
