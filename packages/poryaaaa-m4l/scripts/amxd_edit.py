#!/usr/bin/env python3
"""Small, explicit edits for hand-maintained .amxd patcher JSON.

This complements amxd_inspect.py: it preserves the Max container prefix, updates
its patcher payload length, and applies narrowly named patcher-level edits.
"""
from __future__ import annotations

import argparse
import json
import struct
import sys
from pathlib import Path
from typing import Any


def load_container(path: Path) -> tuple[bytearray, dict[str, Any]]:
    raw = path.read_bytes()
    start = raw.find(b"{")
    if start < 0:
        sys.exit(f"{path}: no JSON payload found")
    payload = raw[start:].decode("utf-8", "ignore").rstrip("\x00")
    return bytearray(raw[:start]), json.loads(payload)


def write_container(path: Path, header: bytearray, document: dict[str, Any]) -> None:
    payload = (json.dumps(document, indent=4) + "\n").encode("utf-8")
    if len(header) >= 32 and bytes(header[24:28]) == b"ptch":
        header[28:32] = struct.pack("<I", len(payload))
    path.write_bytes(bytes(header) + payload)


def patcher_lines(document: dict[str, Any]) -> list[dict[str, Any]]:
    return document["patcher"].setdefault("lines", [])


def line_matches(entry: dict[str, Any], src: str, outlet: int, dst: str, inlet: int) -> bool:
    patchline = entry.get("patchline", {})
    return patchline.get("source") == [src, outlet] and patchline.get("destination") == [dst, inlet]


def has_line(lines: list[dict[str, Any]], src: str, outlet: int, dst: str, inlet: int) -> bool:
    return any(line_matches(entry, src, outlet, dst, inlet) for entry in lines)


def remove_line(lines: list[dict[str, Any]], src: str, outlet: int, dst: str, inlet: int) -> None:
    lines[:] = [entry for entry in lines if not line_matches(entry, src, outlet, dst, inlet)]


def add_line(lines: list[dict[str, Any]], src: str, outlet: int, dst: str, inlet: int, order: int) -> None:
    if has_line(lines, src, outlet, dst, inlet):
        return
    lines.append({
        "patchline": {
            "destination": [dst, inlet],
            "order": order,
            "source": [src, outlet],
        },
    })


def set_line_order(lines: list[dict[str, Any]], src: str, outlet: int, dst: str, inlet: int, order: int) -> None:
    for entry in lines:
        if line_matches(entry, src, outlet, dst, inlet):
            entry["patchline"]["order"] = order
            return
    sys.exit(f"missing patchline {src}[{outlet}] -> {dst}[{inlet}]")


def defer_poryaaaa_wsstatus(document: dict[str, Any]) -> None:
    """Route wsstatus polling from node.script ready instead of startup delay.

    Object IDs are intentionally explicit because poryaaaa.amxd is hand-maintained:
    - obj-13: [delay 700] fired by [live.thisdevice]
    - obj-25 outlet 0: [route ready ...] ready outlet from node.script
    - obj-14: [1] starts [metro 5000]
    - obj-16: [t b b] sends [set off] and [wsstatus]
    """
    lines = patcher_lines(document)
    remove_line(lines, "obj-13", 0, "obj-14", 0)
    remove_line(lines, "obj-13", 0, "obj-16", 0)
    add_line(lines, "obj-25", 0, "obj-14", 0, 0)
    add_line(lines, "obj-25", 0, "obj-16", 0, 1)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("file", type=Path)
    parser.add_argument("command", choices=["defer-poryaaaa-wsstatus"])
    args = parser.parse_args()

    header, document = load_container(args.file)
    if args.command == "defer-poryaaaa-wsstatus":
        defer_poryaaaa_wsstatus(document)
    write_container(args.file, header, document)


if __name__ == "__main__":
    main()
