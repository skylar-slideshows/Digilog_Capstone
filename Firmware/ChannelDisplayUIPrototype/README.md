# Channel Display UI Prototype

Standalone browser prototype for the four 240 × 240 Digilog channel displays.

## Scope

- Implements the approved Layout C geometry and interaction model.
- Shows the fixed channel chassis and the SENDS, EQ and COMP stages.
- Simulates gain reduction so meter motion can be reviewed before ADC4 integration.
- Provides controls for page selection, flags, input, pan and every page parameter.
- Models sends 1–3 pan, logarithmic EQ frequency, held-knob HMF/LMF Q, and the
  stepped compressor ratio/attack/release values from the handoff.
- Uses rasterized 1-bit stand-in glyphs with the firmware font cell sizes.
- Can reset the selected screen to the current firmware defaults.

## Safety boundary

This directory is not referenced by `BucketController_FINAL/CMakeLists.txt`, included by firmware
headers, or called from MCU startup. Opening `index.html` changes browser-only state and cannot
change console firmware behavior.

Firmware wiring will be handled as separate reviewed changes:

1. Generic drawing primitives in `displays.c/.h`.
2. A new read-only channel display task.
3. Minimal CMake and startup registration.
4. Optional channel-name, remote-switch and gain-reduction integrations.

## Run

Open `index.html` directly in a browser, or serve the repository root:

```sh
python3 -m http.server 4173
```

Then visit:

`http://localhost:4173/Firmware/ChannelDisplayUIPrototype/`

## Design references

The prototype follows `digilog_channel_ui.html`, `DISPLAY_UI_HANDOFF.md`, and
`bucket_digital_HANDOFF.md`. The checked-out firmware remains authoritative when those handoffs
describe implementation details that have since changed.
