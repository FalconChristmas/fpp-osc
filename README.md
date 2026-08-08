# fpp-osc

Open Sound Control (OSC) support for [Falcon Player (FPP)](https://github.com/FalconChristmas/fpp) —
listen for OSC messages to trigger FPP Commands, and send OSC messages out from a playlist or event.

Useful for driving FPP from lighting desks, TouchOSC / Open Stage Control layouts, QLab, Ableton,
or anything else that speaks OSC.

## Features

- Listens for incoming OSC messages on a configurable port and runs an FPP Command in response.
- Per-event conditions to filter on the message parameters, so one OSC path can drive different
  actions depending on the values sent.
- Expression support, so a command argument can be computed from the OSC parameters — for example
  turning a 0.0–1.0 fader into a 0–100 volume percentage.
- Sends OSC messages out via the **OSC Event** command, with up to four typed arguments.
- A "Last Messages" panel showing the most recent 10 messages FPPD received, to help work out what
  a controller is actually sending.

## Installation

Install from **Content Setup → Plugins** in the FPP web UI, then restart FPPD.

## Configuration

**Content Setup → Open Sound Control** in the FPP web UI.

**Listen Port** must match the port your OSC controller sends to.

Each event you add has:

- **Description** — a label for your own use; FPP ignores it.
- **Path** — the OSC path the controller sends to.
- **Conditions** — filters on the message parameters.
- **Command** — the FPP Command to run, with its arguments.

### Expressions in command arguments

An argument beginning with a single `=` is evaluated as a formula, where `p1`, `p2`, … are the OSC
parameters. An argument without a leading `=` is treated as a string, but `%%p1%%` placeholders are
substituted — for example `Matrix-%%p1%%`.

So if parameter 1 is a float from a 0.0–1.0 slider and the command needs 0–100:

```
=p1*100
```

Three helper functions are provided in addition to the standard operators:

- `rgb(r, g, b)` — r/g/b values 0–255 packed into a single colour integer.
- `hsv(h, s, v)` — hue/saturation/value 0–1 packed into a single colour integer.
- `if(cond, tExp, fExp)` — returns `tExp` when `cond` is non-zero, otherwise `fExp`.

Expression evaluation uses the [TinyExpr](https://github.com/codeplea/tinyexpr) library.

## Commands

- **OSC Event** — send an OSC message. Arguments are the path, target IP address, port, and up to
  four parameters, each typed as None, Integer, Float, or String.

## License

GPLv2 — see [LICENSE](LICENSE).
