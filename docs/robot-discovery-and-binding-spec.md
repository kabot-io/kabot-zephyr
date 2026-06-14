<!--
SPDX-License-Identifier: Apache-2.0
SPDX-FileCopyrightText: Copyright The Kabot Project Contributors
-->

# Robot Discovery and Egress Binding Specification

## Purpose

Define a complete, implementation-ready contract for robot discovery and state egress target binding.

This specification is normative for external HMI implementations and is intentionally independent from the debug/reference HMI in this repository.

## Scope

- Discovery transport and payload contract.
- Host-side discovery sweep algorithm.
- Robot-side binding and persistence behavior.
- Logging contract for takeover visibility.

Out of scope in this phase:

- Multicast discovery.
- Broadcast discovery.
- `human_name` write/update flow (managed by another path).

## Terms

- Bonjour request: discovery packet sent by HMI to robot discovery UDP endpoint.
- Bonjour response: discovery reply sent by robot.
- Last Bonjour wins: the most recently accepted Bonjour request defines active state egress destination.

## Protocol Messages

Messages are protobuf-encoded.

```proto
message Bonjour {
    uint32 hmi_port = 1;
    bool claim = 2;
    bool release = 3;
}

message BonjourResponse {
    string serial = 1;
    string human_name = 2;
    uint32 control_port = 3;
    string firmware_version = 4;
    bool is_claimed = 5;
    string claimed_by_ip = 6;
}
```

## Discovery Transport Contract

- Transport: UDP/IPv4.
- Robot discovery bind port: firmware configuration (`KABOT_DISCOVERY_PORT`).
- Request payload: protobuf `Bonjour`.
- Response payload: protobuf `BonjourResponse`.

`BonjourResponse` claim-state fields:

- `is_claimed`: runtime claim gate state in robot.
- `claimed_by_ip`: claimant IP when claimed, empty string when not claimed.

- Response destination: source IP and source port from received UDP datagram.

Firmware defaults implemented in this repository:

- Discovery bind port default: `30012` (`CONFIG_KABOT_DISCOVERY_PORT`).
- Control ingress port default: `30010` (`CONFIG_KABOT_CONTROL_INGRESS_PORT`).
- State fallback egress port default: `30011` (`CONFIG_KABOT_STATE_EGRESS_PORT`).

`Bonjour.hmi_port` is the HMI UDP port where robot state snapshots should be sent.

`Bonjour.claim` controls side effects:

- `claim=false`: discovery-only request; robot responds without changing active egress target.
- `claim=true`: claim request; robot performs takeover and persistence update before responding.

`Bonjour.release` disables active stream:

- `release=true`: robot clears runtime claim state and stops state egress until a new
  `claim=true` request is accepted.

## Host Discovery Sweep Contract

HMI must perform unicast sweep based on host interface data.

Required behavior:

1. Enumerate host IPv4 interfaces from the operating system.
2. For each interface, obtain IPv4 address and netmask/prefix.
3. Sort interface subnets by host cardinality (smallest first, largest last).
4. Iterate unicast destination hosts in that interface subnet and send `Bonjour` to each.
5. Keep large subnets (for example Docker or ZeroTier) in the sweep set; do not skip them.
6. During scan, emit progress logs (scan start, periodic progress for large subnets, scan finish).
7. Collect responses for a short receive window (< 1 second).
   - HMI should apply a minimum effective receive window (0.8s recommended).
   - HMI should apply a short localhost grace receive loop immediately after
     sending the localhost probe (about 100-150 ms recommended).
8. Aggregate valid responses and present a robot list for operator selection.
9. Send `claim=true` Bonjour only to the selected robot endpoint.
10. Before switching from robot A to robot B, send `release=true` to robot A.
    - Release is best-effort in this phase.
    - If release does not succeed (for example timeout/disconnect), HMI may proceed
      with claim for robot B to avoid operator lock.
11. Provide an explicit operator action to send `release=true` for current robot
    without selecting a new robot (for example an `Unclaim` button).

### Addressing Edge Cases

- `/32` networks:
  - Contain one address only.
  - Treat that single address as candidate destination.
- `/31` networks:
  - Point-to-point style addressing.
  - Treat both addresses in range as candidates.
- `127.0.0.1`:
  - Must be considered valid for simulation/localhost testing.
  - HMI should include localhost candidate path when local simulation mode is active.

## Robot Binding Contract

For each accepted Bonjour request:

1. Robot extracts source IPv4 from UDP datagram sender address.
2. Robot reads `hmi_port` and `claim` from protobuf payload.
3. If `claim=false`:
   - robot does not modify active egress target,
   - robot does not persist target settings,
   - robot replies with `BonjourResponse`.
5. If `release=true`:
   - robot clears runtime claim state,
   - robot stops state egress,
   - robot replies with `BonjourResponse`.
6. If `claim=true`:
   - robot sets active egress destination to `<sender_ipv4, hmi_port>`,
   - robot persists destination in settings backend,
   - robot replies with `BonjourResponse`.

Takeover policy remains last-claim-wins.

No application-level deduplication is required before settings write in this phase.

The settings backend is expected to avoid duplicate physical writes where supported.

## Persistence Contract

Settings keys:

- `kabot/id/serial`
- `kabot/id/human_name`
- `kabot/net/hmi_ip`
- `kabot/net/hmi_port`

Identity behavior:

- `serial` is read from settings.
- If missing, firmware uses Kconfig default serial.
- `human_name` is read from settings.
- If missing, firmware uses Kconfig default human-readable name.

Egress behavior:

- State egress is gated and does not transmit until a `claim=true` Bonjour request is accepted
  in the current runtime.
- On boot, robot keeps endpoint defaults/settings values loaded but still waits for runtime claim
  before sending.
- A new `claim=true` Bonjour request overwrites active destination and persisted destination.

## Logging Contract

Firmware logs must include:

1. Discovery receive event:
   - source IP
   - source port
   - requested `hmi_port`
2. Takeover/bind event:
   - old target `<ip:port>`
   - new target `<ip:port>`
3. Settings persistence result code.
4. Discovery response transmission metadata:
   - destination `<ip:port>`
   - serial
   - human_name
  - is_claimed
  - claimed_by_ip
   - control_port
   - firmware_version

## External HMI Implementation Notes

- This repository's Tkinter HMI is not normative for discovery behavior.
- External HMI should treat this document plus HMI-centric contracts as source of truth.
- Discovery timeout should balance scan completeness and responsiveness.
  - Reference HMI default in this repository: `3.0s`.
  - Effective receive window must be at least `0.8s`.

## Firmware Implementation References

- Discovery UDP service: `app/src/control/discovery_service.c`
- Runtime/persistent identity and endpoint storage: `app/src/system/robot_settings.c`
- Runtime destination use during state send: `app/src/zbus/state/state_udp_sender.c`
- Message schema: `app/protos/state_control_msg.proto`
- Config definitions: `app/Kconfig`