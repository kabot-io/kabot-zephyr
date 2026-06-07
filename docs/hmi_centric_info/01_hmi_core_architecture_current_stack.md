# HMI Core Architecture, Current Stack Reference

## Audience

Primary audience:

- HMI developer onboarding

Secondary audience:

- UX and UI designer

## Scope

This document is a reference for the current HMI implementation and module boundaries.

## Source Of Truth Files

- scripts/kabot_io/main.py
- scripts/kabot_io/config.py
- scripts/kabot_io/model.py
- scripts/kabot_io/controller.py
- scripts/kabot_io/view.py
- scripts/kabot_io/proto_codec.py
- scripts/kabot_io/state_fields.py
- app/protos/state_control_msg.proto
- docs/hmi-architecture.md

## Runtime Entry and Configuration

### Entry point

- main.py parses CLI arguments
- Initializes protobuf runtime
- Builds model, view, controller
- Starts main UI loop

### Core runtime parameters

- Control target host and port
- State bind host and port
- SMP host and port
- SMP timeout
- Default periodic control interval

### Effective defaults

- control port: 30010
- state listen port: 30011
- SMP port: 1337
- SMP host defaults to control host when not explicitly passed

## Current Architecture Model

### Model responsibilities

- Own UDP socket lifecycle
- Encode Control protobuf frames
- Decode State protobuf frames
- Execute SMP requests and parse typed responses and errors

### View responsibilities

- Own all widgets and bindings
- Render control panel, state panel, plots, and SMP terminal
- Render VT100-like shell output behavior in the SMP terminal area

### Controller responsibilities

- Attach UI callbacks to model operations
- Manage periodic control send loop
- Poll state updates
- Compute and clear per-field Hz values
- Coordinate SMP shell and stats actions

## Data Schema Contract

### Control message used by HMI

- Control.effort.state.x
- Control.effort.state.y

### State message consumed by HMI

- top-level header
- effort
- linear_acceleration
- angular_velocity
- magnetic_field
- distance
- light_left
- light_right

## Hz Policy In HMI

- Computed only on stamp change with positive delta
- Uses moving average window of 5 samples
- Clears stale Hz after timeout
- Clears immediately for invalid or non-monotonic stamp

## Current Stack Strengths

- Very fast ad hoc feature iteration
- Direct code-level control of behavior
- Easy protocol-level debugging with Python

## Current Stack Risks

- Growing custom UI behavior maintenance
- Limited sensible defaults compared to mature web UI ecosystem
- Terminal and advanced interaction semantics require manual implementation and testing

## Target Stack Delta, short section

Future architecture keeps Python backend responsibilities but replaces Tkinter view with web frontend connected over Socket.IO. This reduces custom UI maintenance while preserving protocol and transport logic in Python.
