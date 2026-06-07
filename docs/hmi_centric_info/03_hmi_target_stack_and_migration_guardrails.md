# HMI Target Stack and Migration Guardrails

## Purpose

Define migration path from current Python Tkinter HMI to web frontend while preserving stable system behavior.

## Target Topology

- Backend: Python
- Frontend: web UI
- Bridge: Socket.IO
- Protocol model: protobuf-first for telemetry and structured control payloads

## Why This Direction

- Keep existing Python transport expertise and runtime behavior
- Reduce UI maintenance burden from custom widget behavior
- Use mature terminal and chart libraries in web ecosystem
- Improve design-system and UX iteration speed

## Socket.IO Use Model

### Event classes

- telemetry state updates
- control command requests
- SMP shell and stats requests and responses
- runtime status events

### Delivery patterns

- telemetry events without ack
- command events with ack and timeout
- reconnect and buffering strategy explicitly configured
- optional websocket-only mode for desktop distribution

## Recommended Data Strategy

- Use protobuf payloads for telemetry channels where compactness matters
- Use compact JSON for UI metadata and control-plane status
- Preserve schema ownership in app/protos/state_control_msg.proto

## Performance Guardrails for 60 FPS UI

- UI render loop driven by requestAnimationFrame
- Do not re-render component tree per packet
- Coalesce incoming state changes and apply latest frame on next render tick
- Use ring buffers for charts
- Keep transport frequency independent from render frequency

## Migration Stages

### Stage 1

- Keep existing Python model logic
- Add Socket.IO server process in Python
- Keep Tkinter app operational in parallel

### Stage 2

- Build web frontend with parity screens
- Validate control, state, shell, and stats parity against Tkinter baseline

### Stage 3

- Freeze legacy Tkinter for maintenance only
- Make web frontend the default operator UI

## Non-Negotiable Compatibility Rules

- No firmware API break in migration stage 1
- Keep port contracts unchanged for existing firmware
- Keep protobuf schema compatibility and explicit version handling
- Preserve existing shell operational semantics from operator perspective

## Current to Target ratio statement

This document intentionally describes target architecture in detail, but implementation planning should still prioritize preserving current behavior in a 4:1 current-to-target reference balance across the package.
