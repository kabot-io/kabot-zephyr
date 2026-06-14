<!--
SPDX-License-Identifier: Apache-2.0
SPDX-FileCopyrightText: Copyright (c) 2026 Krzysztof Pochwała
-->

# Firmware Interface for HMI Developers

## Purpose

Define the firmware-side contracts HMI must respect.

## Source Of Truth Files

- docs/firmware-data-flow.md
- app/prj.conf
- app/protos/state_control_msg.proto
- app/src/control/control_service.c
- app/src/zbus/state/state_udp_sender.c

## Ingress Contract, Control

- Transport: UDP
- Port: `CONFIG_KABOT_CONTROL_INGRESS_PORT` (default `30010`)
- Message type: Control protobuf
- Required fields used by actuation path:
  - control.effort.state.x
  - control.effort.state.y

## Egress Contract, State

- Transport: UDP
- Active destination is runtime-bound by last accepted Bonjour request
- Fallback destination comes from:
  - `CONFIG_KABOT_STATE_EGRESS_HOST`
  - `CONFIG_KABOT_STATE_EGRESS_PORT` (default `30011`)
- Message type: State protobuf snapshot
- Snapshot built from merged partial updates on firmware side

## Merge Semantics That Affect HMI

Firmware merge accepts incoming field when:

- field missing in cache
- incoming stamp is newer
- incoming stamp equals cached stamp

HMI should treat identical stamp updates as valid latest value updates.

## SMP Management Contract

- Transport: MCUmgr SMP over UDP
- Port: 1337
- Groups in use:
  - OS
  - STAT
  - TASKSTAT
  - SHELL

## SMP Output Size Constraints

Current firmware settings constrain shell response size to fit safe UDP payload limits.

Operational implication:

- Long shell outputs may be truncated or fail with size-related errors
- HMI must show result robustly and avoid assuming infinite shell output stream

## Firmware Timing Expectations For HMI

Nominal producer rates documented in firmware data flow:

- IMU: 10 Hz
- Magnetometer: 5 Hz
- Distance: about 60 Hz
- Light: 2 Hz

HMI should not hardcode these values as strict guarantees.

## Discovery and Binding Contract

Discovery and dynamic state egress binding are specified in:

- docs/robot-discovery-and-binding-spec.md

Normative constraints in this phase:

- Discovery transport is UDP unicast sweep from host-side HMI.
- A Bonjour request contains:
  - `hmi_port`
  - `claim`
  - `release`
- A Bonjour response contains:
  - `serial`
  - `human_name`
  - `control_port`
  - `firmware_version`
  - `is_claimed`
  - `claimed_by_ip`
- `claim=false` performs discovery-only response with no target mutation.
- `claim=true` applies ownership takeover (last claim wins).
- `release=true` clears runtime claim state and disables state egress until next claim.
- During robot switch, HMI may treat release as best-effort and continue with new
  claim if release response is not received.
- Human-readable name updates are out of scope for Bonjour and use another path.
- Firmware writes destination settings on accepted `claim=true`.

Robot settings keys used by this contract:

- `kabot/id/serial`
- `kabot/id/human_name`
- `kabot/net/hmi_ip`
- `kabot/net/hmi_port`

## Ports Relevant To External HMI

- Control ingress UDP port: firmware-configured control port.
- State egress destination UDP port: HMI-provided `Bonjour.hmi_port` after bind.
- Discovery UDP bind port: `CONFIG_KABOT_DISCOVERY_PORT` (default `30012`).
