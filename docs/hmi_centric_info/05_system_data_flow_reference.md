# System Data Flow Reference

## Purpose

Provide a cross-domain flow model that aligns HMI, firmware, and UX understanding.

## End-to-End Control Flow

1. Operator updates effort in HMI
2. HMI serializes Control protobuf
3. HMI sends UDP datagram to firmware control port
4. Firmware decodes and validates Control
5. Firmware publishes control to internal channel
6. Actuation subscriber applies motor effort

## End-to-End State Flow

1. Firmware producers publish partial State fragments
2. Aggregator merges fragments into cached full state
3. Periodic publisher emits snapshot
4. UDP sender transmits snapshot
5. HMI receives and decodes State
6. HMI updates read-only fields and chart buffers

## SMP Flow

1. Operator sends shell or stats action
2. HMI issues SMP request over UDP
3. Firmware MCUmgr handler executes command
4. Response payload returns to HMI
5. HMI renders output in terminal section

## End-to-End Discovery Flow (No Side Effects)

1. HMI enumerates host IPv4 interfaces and netmasks from OS.
2. HMI performs UDP unicast Bonjour sweep over host candidates for each interface subnet.
3. Robot receives Bonjour datagram on discovery UDP port.
4. Robot decodes `Bonjour.hmi_port` with `claim=false` and extracts sender IPv4 from UDP source address.
5. Robot replies with `BonjourResponse` to request source endpoint.
6. HMI records robot identity and control metadata in discovered-robots list.

## End-to-End Claim Flow (Last Claim Wins)

1. Operator selects a robot row in HMI discovered-robots list.
2. HMI sends valid Bonjour with `claim=true` to selected robot endpoint.
3. Robot accepts claim and overwrites active egress destination with claim sender endpoint.
4. Robot persists claimed endpoint in settings backend.
5. Robot enables state egress and streams subsequent packets to claimed HMI endpoint.
6. Firmware logs takeover event old `A -> B`.

Switching-robot behavior in current reference HMI:

1. If robot A is currently claimed and operator selects robot B, HMI first sends
	`release=true` to robot A.
2. If release response is not received (for example robot A disconnected), HMI still
	proceeds with `claim=true` to robot B.
3. This avoids UI/operator lock while keeping release intent explicit.

## Discovery Logging Expectations

For each valid Bonjour request, firmware logs should provide enough context to audit takeover:

1. Discovery receive tuple:
	- source IP
	- source port
	- requested `hmi_port`
2. Target transition:
	- previous egress target
	- new egress target
3. Settings persistence result code.
4. Discovery response metadata:
	- response destination endpoint
	- serial
	- human name
	- control port
	- firmware version

## Clock and Sampling Semantics

- Producer stamps are firmware-owned
- HMI-derived Hz is inferred from stamp deltas
- A field can become stale while others continue updating

## Design Implications

- UI should tolerate asynchronous per-field freshness
- UI should separate transport rate from render rate
- UI should support degraded mode when one subsystem stalls

## Migration Note

This data flow must remain stable when moving from Tkinter frontend to web frontend. Backend transport and protocol ownership remains unchanged.
