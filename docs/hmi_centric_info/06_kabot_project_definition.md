# Kabot Project Technical Definition

## What Kabot Is

Kabot is a mobile robotics learning platform focused on practical robotics software and system integration.

In this repository, Kabot includes:

- Zephyr-based robot firmware
- Host-side HMI for control, telemetry, and diagnostics
- Shared protobuf contract between firmware and host

## Technical Pillars

- Command ingress to robot motion control
- Telemetry egress from robot state producers
- Diagnostics and management through SMP
- Modular firmware architecture via channels and publishers

## Current Technical Scope

### Firmware scope

- UDP control ingress
- UDP state egress
- zbus channel-based internal data flow
- Real and simulated sensor publisher ecosystem

### HMI scope

- Manual and periodic control commands
- Live state inspection and plotting
- SMP shell and statistics operations

## Development Environment Statement

This project is designed to build in the provided development container setup and has project documentation under docs for firmware flow, HMI architecture, and sensor integration practices.

## Why This Definition Matters For New Team Members

- Clarifies that Kabot is both robot firmware and host operations software
- Clarifies that protocol and data contracts are central to cross-team work
- Clarifies that UI changes must preserve transport and schema guarantees

## Relation To Planned Stack Evolution

Kabot scope remains unchanged while the HMI presentation layer evolves from Tkinter toward a web frontend model with Python backend continuity.
