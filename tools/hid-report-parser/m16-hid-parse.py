#!/usr/bin/env python3
"""Strict, read-only USB HID report-descriptor parser for the M16 evidence dump.

Implements the HID 1.11 short-item state machine (Main, Global, Local, Push/Pop),
reports every item, resolves usages, tracks collections, and calculates per-report
Input/Output/Feature layouts. It never opens or writes to a USB device.
"""

from __future__ import annotations

import hashlib
import math
import sys
from dataclasses import dataclass, field
from pathlib import Path


PAGE_NAMES = {
    0x01: "Generic Desktop",
    0x07: "Keyboard/Keypad",
    0x08: "LED",
    0x09: "Button",
    0x0B: "Telephony Device",
    0x0C: "Consumer",
}

USAGE_NAMES = {
    0x01: {
        0x01: "Pointer",
        0x02: "Mouse",
        0x06: "Keyboard",
        0x30: "X",
        0x31: "Y",
        0x38: "Wheel",
        0x80: "System Control",
    },
    0x07: {
        0x00: "Reserved/Undefined",
        0xE0: "Keyboard LeftControl",
        0xE1: "Keyboard LeftShift",
        0xE2: "Keyboard LeftAlt",
        0xE3: "Keyboard Left GUI",
        0xE4: "Keyboard RightControl",
        0xE5: "Keyboard RightShift",
        0xE6: "Keyboard RightAlt",
        0xE7: "Keyboard Right GUI",
    },
    0x08: {
        0x01: "Num Lock",
        0x02: "Caps Lock",
        0x03: "Scroll Lock",
        0x08: "Do Not Disturb",
        0x09: "Mute",
        0x17: "Off-Hook",
        0x18: "Ring",
        0x19: "Message Waiting",
        0x1A: "Data Mode",
        0x1E: "Speaker",
        0x1F: "Headset",
        0x20: "Hold",
        0x21: "Microphone",
        0x22: "Coverage",
        0x23: "Night Mode",
        0x24: "Send Calls",
        0x25: "Call Pickup",
        0x26: "Conference",
    },
    0x0B: {
        0x01: "Phone",
        0x20: "Hook Switch",
        0x21: "Flash",
        0x2F: "Phone Mute",
    },
    0x0C: {
        0x01: "Consumer Control",
        0xB3: "Fast Forward",
        0xB5: "Scan Next Track",
        0xB6: "Scan Previous Track",
        0xB7: "Stop",
        0xCD: "Play/Pause",
        0xE2: "Mute",
        0xE9: "Volume Increment",
        0xEA: "Volume Decrement",
    },
}

COLLECTION_TYPES = {
    0x00: "Physical",
    0x01: "Application",
    0x02: "Logical",
    0x03: "Report",
    0x04: "Named Array",
    0x05: "Usage Switch",
    0x06: "Usage Modifier",
}


def unsigned(data: bytes) -> int:
    return int.from_bytes(data, "little", signed=False) if data else 0


def signed(data: bytes) -> int:
    return int.from_bytes(data, "little", signed=True) if data else 0


def page_name(page: int) -> str:
    if page >= 0xFF00:
        return f"Vendor Defined 0x{page:04X}"
    return PAGE_NAMES.get(page, f"Usage Page 0x{page:04X}")


def usage_name(page: int, usage: int) -> str:
    return USAGE_NAMES.get(page, {}).get(usage, f"Usage 0x{usage:04X}")


def usage_text(value: tuple[int, int] | None) -> str:
    if value is None:
        return "<no usage>"
    page, usage = value
    return f"0x{page:04X}:{usage:04X} {page_name(page)} / {usage_name(page, usage)}"


def flags_text(value: int, main_name: str) -> str:
    parts = [
        "Constant" if value & 0x01 else "Data",
        "Variable" if value & 0x02 else "Array",
        "Relative" if value & 0x04 else "Absolute",
        "Wrap" if value & 0x08 else "No Wrap",
        "Non Linear" if value & 0x10 else "Linear",
        "No Preferred" if value & 0x20 else "Preferred State",
        "Null State" if value & 0x40 else "No Null",
    ]
    if main_name in ("Output", "Feature"):
        parts.append("Volatile" if value & 0x80 else "Non Volatile")
    parts.append("Buffered Bytes" if value & 0x100 else "Bit Field")
    return ", ".join(parts)


@dataclass
class GlobalState:
    usage_page: int = 0
    logical_min: int = 0
    logical_max: int = 0
    physical_min: int = 0
    physical_max: int = 0
    unit_exponent: int = 0
    unit: int = 0
    report_size: int = 0
    report_id: int = 0
    report_count: int = 0

    def copy(self) -> "GlobalState":
        return GlobalState(**self.__dict__)


@dataclass
class LocalState:
    usages: list[tuple[int, int]] = field(default_factory=list)
    usage_min: tuple[int, int] | None = None
    usage_max: tuple[int, int] | None = None
    other: list[str] = field(default_factory=list)

    def clear(self) -> None:
        self.usages.clear()
        self.usage_min = None
        self.usage_max = None
        self.other.clear()

    def primary_usage(self) -> tuple[int, int] | None:
        if self.usages:
            return self.usages[0]
        return self.usage_min

    def describe(self) -> str:
        pieces: list[str] = []
        if self.usages:
            pieces.append("usages=[" + "; ".join(usage_text(u) for u in self.usages) + "]")
        if self.usage_min is not None or self.usage_max is not None:
            pieces.append(f"usage_range=[{usage_text(self.usage_min)} .. {usage_text(self.usage_max)}]")
        if self.other:
            pieces.append("other=[" + "; ".join(self.other) + "]")
        return ", ".join(pieces) if pieces else "no local usages"


def resolve_usage(raw: int, data_size: int, current_page: int) -> tuple[int, int]:
    if data_size == 4:
        embedded_page = (raw >> 16) & 0xFFFF
        if embedded_page:
            return embedded_page, raw & 0xFFFF
    return current_page, raw


def main_tag_name(tag: int) -> str:
    return {8: "Input", 9: "Output", 10: "Collection", 11: "Feature", 12: "End Collection"}.get(tag, f"Main tag {tag}")


def global_tag_name(tag: int) -> str:
    return {
        0: "Usage Page", 1: "Logical Minimum", 2: "Logical Maximum",
        3: "Physical Minimum", 4: "Physical Maximum", 5: "Unit Exponent",
        6: "Unit", 7: "Report Size", 8: "Report ID", 9: "Report Count",
        10: "Push", 11: "Pop",
    }.get(tag, f"Global tag {tag}")


def local_tag_name(tag: int) -> str:
    return {
        0: "Usage", 1: "Usage Minimum", 2: "Usage Maximum",
        3: "Designator Index", 4: "Designator Minimum", 5: "Designator Maximum",
        7: "String Index", 8: "String Minimum", 9: "String Maximum", 10: "Delimiter",
    }.get(tag, f"Local tag {tag}")


def parse(path: Path) -> str:
    blob = path.read_bytes()
    if len(blob) != 317:
        raise ValueError(f"expected 317 bytes, got {len(blob)}")

    out: list[str] = []
    out.append(f"HID report descriptor: {path}")
    out.append(f"Length: {len(blob)} bytes")
    out.append(f"SHA-256: {hashlib.sha256(blob).hexdigest()}")
    out.append("")
    out.append("== COMPLETE ITEM-BY-ITEM DECODE ==")

    gs = GlobalState()
    gstack: list[GlobalState] = []
    ls = LocalState()
    collections: list[dict[str, object]] = []
    top_level: list[dict[str, object]] = []
    report_bits: dict[tuple[int, str], int] = {}
    report_fields: list[dict[str, object]] = []
    report_ids_seen: set[int] = set()
    pos = 0

    while pos < len(blob):
        start = pos
        prefix = blob[pos]
        pos += 1
        if prefix == 0xFE:
            if pos + 2 > len(blob):
                raise ValueError(f"truncated long item header at {start}")
            size = blob[pos]
            long_tag = blob[pos + 1]
            pos += 2
            if pos + size > len(blob):
                raise ValueError(f"truncated long item data at {start}")
            data = blob[pos:pos + size]
            pos += size
            raw = blob[start:pos]
            out.append(f"@0x{start:04X}  {raw.hex(' '):<30}  Long item tag=0x{long_tag:02X}, size={size}, data={data.hex(' ')}")
            continue

        size_code = prefix & 0x03
        size = 4 if size_code == 3 else size_code
        item_type = (prefix >> 2) & 0x03
        tag = (prefix >> 4) & 0x0F
        if pos + size > len(blob):
            raise ValueError(f"truncated short item at {start}")
        data = blob[pos:pos + size]
        pos += size
        raw = blob[start:pos]
        uval = unsigned(data)
        sval = signed(data)
        prefix_text = f"@0x{start:04X}  {raw.hex(' '):<30}"

        if item_type == 1:  # Global
            name = global_tag_name(tag)
            detail = ""
            if tag == 0:
                gs.usage_page = uval
                detail = f"0x{uval:04X} ({page_name(uval)})"
            elif tag == 1:
                gs.logical_min = sval
                detail = str(sval)
            elif tag == 2:
                gs.logical_max = sval if gs.logical_min < 0 else uval
                detail = str(gs.logical_max)
            elif tag == 3:
                gs.physical_min = sval
                detail = str(sval)
            elif tag == 4:
                gs.physical_max = sval if gs.physical_min < 0 else uval
                detail = str(gs.physical_max)
            elif tag == 5:
                nibble = uval & 0x0F
                gs.unit_exponent = nibble - 16 if nibble & 0x08 else nibble
                detail = str(gs.unit_exponent)
            elif tag == 6:
                gs.unit = uval
                detail = f"0x{uval:X}"
            elif tag == 7:
                gs.report_size = uval
                detail = f"{uval} bits"
            elif tag == 8:
                if uval == 0:
                    raise ValueError(f"invalid Report ID 0 at {start}")
                gs.report_id = uval
                report_ids_seen.add(uval)
                detail = str(uval)
            elif tag == 9:
                gs.report_count = uval
                detail = str(uval)
            elif tag == 10:
                gstack.append(gs.copy())
                detail = f"depth={len(gstack)}"
            elif tag == 11:
                if not gstack:
                    raise ValueError(f"global Pop with empty stack at {start}")
                gs = gstack.pop()
                detail = f"restored, depth={len(gstack)}"
            else:
                detail = f"raw=0x{uval:X}"
            out.append(f"{prefix_text}  GLOBAL {name}: {detail}")
            continue

        if item_type == 2:  # Local
            name = local_tag_name(tag)
            detail = ""
            if tag in (0, 1, 2):
                resolved = resolve_usage(uval, size, gs.usage_page)
                detail = usage_text(resolved)
                if tag == 0:
                    ls.usages.append(resolved)
                elif tag == 1:
                    ls.usage_min = resolved
                else:
                    ls.usage_max = resolved
            else:
                detail = f"0x{uval:X}"
                ls.other.append(f"{name}=0x{uval:X}")
            out.append(f"{prefix_text}  LOCAL  {name}: {detail}")
            continue

        if item_type == 0:  # Main
            name = main_tag_name(tag)
            if tag == 10:  # Collection
                usage = ls.primary_usage()
                ctype = uval
                ctype_name = COLLECTION_TYPES.get(ctype, "Vendor-defined" if ctype >= 0x80 else "Reserved")
                entry = {"offset": start, "type": ctype, "type_name": ctype_name, "usage": usage, "depth": len(collections)}
                if not collections:
                    top_level.append(entry)
                collections.append(entry)
                out.append(f"{prefix_text}  MAIN   Collection: {ctype_name} (0x{ctype:02X}), usage={usage_text(usage)}, depth={len(collections)-1}")
                ls.clear()
                continue
            if tag == 12:  # End Collection
                if not collections:
                    raise ValueError(f"End Collection with empty stack at {start}")
                closed = collections.pop()
                out.append(f"{prefix_text}  MAIN   End Collection: closes {closed['type_name']} {usage_text(closed['usage'])}, new_depth={len(collections)}")
                ls.clear()
                continue
            if tag in (8, 9, 11):
                if gs.report_size == 0 or gs.report_count == 0:
                    raise ValueError(f"{name} at {start} has zero Report Size/Count")
                key = (gs.report_id, name)
                bit_offset = report_bits.get(key, 0)
                bit_length = gs.report_size * gs.report_count
                report_bits[key] = bit_offset + bit_length
                local_desc = ls.describe()
                flag_desc = flags_text(uval, name)
                current_collection = collections[-1] if collections else None
                out.append(
                    f"{prefix_text}  MAIN   {name}: flags=0x{uval:X} ({flag_desc}); "
                    f"Report ID={gs.report_id}, bits={bit_offset}..{bit_offset + bit_length - 1}, "
                    f"size={gs.report_size} x count={gs.report_count}, logical={gs.logical_min}..{gs.logical_max}; {local_desc}"
                )
                report_fields.append({
                    "offset": start, "kind": name, "report_id": gs.report_id,
                    "bit_offset": bit_offset, "bit_length": bit_length,
                    "report_size": gs.report_size, "report_count": gs.report_count,
                    "flags": uval, "flags_text": flag_desc, "local": local_desc,
                    "collection": current_collection,
                })
                ls.clear()
                continue
            out.append(f"{prefix_text}  MAIN   {name}: raw=0x{uval:X}")
            ls.clear()
            continue

        out.append(f"{prefix_text}  RESERVED type item: tag=0x{tag:X}, value=0x{uval:X}")

    if collections:
        raise ValueError(f"{len(collections)} unclosed collection(s)")
    if gstack:
        raise ValueError(f"{len(gstack)} unpopped global state(s)")

    out.append("")
    out.append("== TOP-LEVEL COLLECTIONS ==")
    for index, entry in enumerate(top_level, 1):
        out.append(
            f"{index}. offset=0x{entry['offset']:04X}, type={entry['type_name']} (0x{entry['type']:02X}), "
            f"usage={usage_text(entry['usage'])}"
        )

    out.append("")
    out.append("== REPORT LAYOUTS ==")
    for report_id in sorted(report_ids_seen):
        for kind in ("Input", "Output", "Feature"):
            bits = report_bits.get((report_id, kind), 0)
            if bits:
                payload_bytes = math.ceil(bits / 8)
                out.append(
                    f"Report ID {report_id} {kind}: payload={bits} bits ({payload_bytes} bytes), "
                    f"wire size including Report ID={payload_bytes + 1} bytes"
                )

    out.append("")
    out.append("== MAIN DATA ITEMS ==")
    for field in report_fields:
        coll = field["collection"]
        coll_text = usage_text(coll["usage"]) if coll else "<none>"
        out.append(
            f"offset=0x{field['offset']:04X}; Report ID={field['report_id']}; {field['kind']}; "
            f"bits={field['bit_offset']}..{field['bit_offset'] + field['bit_length'] - 1}; "
            f"size={field['report_size']}x{field['report_count']}; {field['flags_text']}; "
            f"collection={coll_text}; {field['local']}"
        )

    out.append("")
    out.append("Parser validation: PASS (317 bytes consumed, balanced collections, valid report layouts)")
    return "\n".join(out) + "\n"


def main() -> int:
    if len(sys.argv) != 2:
        print(f"usage: {Path(sys.argv[0]).name} HID_REPORT.bin", file=sys.stderr)
        return 2
    try:
        sys.stdout.write(parse(Path(sys.argv[1])))
        return 0
    except (OSError, ValueError) as exc:
        print(f"parse error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
