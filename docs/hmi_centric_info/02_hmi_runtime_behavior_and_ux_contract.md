# HMI Runtime Behavior and UX Contract

## Purpose

This document defines expected HMI behavior as an implementation contract for developers and designers.

## Control Interaction Contract

- Arrow keys map to directional effort presets
- Send once transmits one control frame
- Periodic mode transmits at configured interval
- Invalid numeric input is surfaced via status line

## State Panel Contract

- State fields are read-only
- Each field displays last decoded value
- Header-based Hz cells reflect update cadence

## Plot Contract

- Rolling window visualization
- Series reset if producer time goes backward to avoid visual discontinuities
- Zero line shown only where axis range crosses zero

## SMP Terminal Contract

### Terminal layout and flow

- Prompt on the last line
- Previous command output shown above prompt
- Output scroll behavior matches terminal expectations

### Input behavior

- Enter submits command
- Up and down cycle command history
- Home and Ctrl+A move cursor to prompt input start
- Backspace and Left do not cross prompt boundary
- Selection should allow partial output selection

### SMP operation behavior

- Shell run returns output and ret code
- Quick actions map to predefined commands
- Stats and taskstat actions report status and output in same terminal area

## VT100 Behavior Contract

The shell output renderer supports major VT100/ANSI classes used by shell output:

- cursor movement
- erase commands
- color and style SGR sequences
- carriage return and line feed semantics

## Error and Status Contract

- Running long SMP action sets running status
- Failed SMP action sets failed status
- Successful action sets completion status
- Transport parse or network errors are visible in output and status

## UX Constraints Designers Must Know

- SMP shell is request-response, not a true remote interactive TTY session
- Extremely large shell outputs may be truncated by firmware-side constraints
- High-frequency state rendering should avoid expensive per-update full layout recalculation

## Target Stack Delta, short section

Under web frontend migration, the same behavior contract should remain stable. Only rendering technology and component implementation change.
