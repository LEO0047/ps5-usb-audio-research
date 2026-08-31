# PS5 USB Audio comparison matrix

Evidence labels:

- **CONFIRMED**: raw descriptor, host binding or direct device observation.
- **STRONG INFERENCE**: multiple observations support it, but no PS5 host trace confirms causality.
- **UNKNOWN**: no direct evidence yet.

| Field | EDIFIER M16 Pro | EDIFIER Melo Bar | HECATE G1500 BAR |
|---|---|---|---|
| VID:PID | `2D99:A020` | UNKNOWN | UNKNOWN |
| USB Audio | UAC1, 2ch/16-bit/48 kHz PCM | UNKNOWN | UNKNOWN |
| Audio endpoint | Adaptive isochronous OUT | UNKNOWN | UNKNOWN |
| Interface order | HID 0 / AC 1 / AS 2 | UNKNOWN | UNKNOWN |
| HID keyboard collection | CONFIRMED (`0x01:0x06`) | UNKNOWN | UNKNOWN |
| PS5 keyboard classification | CONFIRMED by direct observation | UNKNOWN | UNKNOWN |
| PS5 Audio | FAIL | UNKNOWN | PASS by direct use case; descriptor pending |

## Current interpretation

- **CONFIRMED:** M16's Interface 0 contains a real Keyboard Application Collection with modifiers, a 6-key array and keyboard LED Output. macOS still binds its Audio interfaces successfully.
- **STRONG INFERENCE:** this keyboard-like, HID-first presentation may influence PS5 peripheral classification.
- **UNKNOWN:** whether that classification causes PS5 to skip or reject AudioControl/AudioStreaming.

The highest-value next evidence is a full descriptor dump from the known-pass G1500 BAR, followed by Melo Bar. A USB gadget ordering A/B test is useful only after those lower-cost comparisons.
