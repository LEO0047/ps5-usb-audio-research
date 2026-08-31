# M16 Pro evidence manifest

Device: EDIFIER M16 Pro (`2D99:A020`)  
Capture date: 2026-08-31  
Host: macOS 27.0 / build 26A5421a

## Descriptor evidence

- `m16-usb-descriptor.txt`: Device/Configuration/String descriptor dump and decoded UAC1 topology.
- `m16-hid-report.bin`: exact 317-byte HID Report Descriptor.
- `m16-hid-report-hex.txt`: readable hex rendering of the same bytes.
- `m16-hid-report-parsed.txt`: complete item-by-item parse and report layouts.
- `m16-hid-report-fetch.log`: request type, safety boundary and received length.

## macOS cross-checks

- `m16-ioreg-host-devices.txt` and `m16-ioreg-host-interfaces.txt`: scoped IORegistry evidence for this USB device and its interfaces.
- `m16-hidutil-*.txt`: primary HID usage and keyboard-match observations.
- `m16-system-profiler-audio.txt`, `m16-ioreg-apple-usb-audio-control.txt`, `m16-usbaudiod-process.txt`: CoreAudio/driver binding evidence.
- `m16-host-*.txt`: capture host version.

The acquisition tools used standard read-only descriptor requests. They did not claim interfaces, detach drivers, send vendor commands or perform USB OUT transfers.

The raw evidence retains the device serial number and ephemeral IOKit registry identifiers. Review/redact those fields before changing a fork or copy of this repository to public visibility.
