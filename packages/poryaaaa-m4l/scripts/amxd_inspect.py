#!/usr/bin/env python3
"""Inspect a .amxd device patcher.

`.amxd` files are an 8-byte binary header followed by the patcher JSON. This
strips the header, parses the JSON, and exposes the queries used for
post-hand-edit validation (which boxes exist, which cords source/target a
given box, dangling patchlines, including embedded patchers).

Devices in this repo are hand-maintained (edited and saved directly in Max);
the tool is run after changes with `python3 scripts/amxd_inspect.py
devices/<name>.amxd validate`.

Usage:
    amxd_inspect.py <file> boxes [--match PATTERN]
    amxd_inspect.py <file> box <id>
    amxd_inspect.py <file> cords [--from ID|--to ID|--from-text PAT|--to-text PAT]
    amxd_inspect.py <file> validate
    amxd_inspect.py <file> raw

PATTERN is a case-insensitive substring matched against `maxclass + text`.
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path
from typing import Iterable


def load(path: Path) -> dict:
    raw = path.read_bytes()
    start = raw.find(b"{")
    if start < 0:
        sys.exit(f"{path}: no JSON payload found")
    text = raw[start:].decode("utf-8", "ignore").rstrip("\x00")
    return json.loads(text)


def boxes(d: dict) -> list[dict]:
    return [entry["box"] for entry in d["patcher"].get("boxes", [])]


def lines(d: dict) -> list[dict]:
    return [entry["patchline"] for entry in d["patcher"].get("lines", [])]


def patcher_boxes(patcher: dict) -> list[dict]:
    return [entry["box"] for entry in patcher.get("boxes", [])]


def patcher_lines(patcher: dict) -> list[dict]:
    return [entry["patchline"] for entry in patcher.get("lines", [])]


def describe(box: dict) -> str:
    klass = box.get("maxclass", "?")
    text = box.get("text", "")
    return f"{klass}" + (f" / {text}" if text else "")


def by_id(d: dict) -> dict[str, dict]:
    return {bx["id"]: bx for bx in boxes(d)}


def patcher_by_id(patcher: dict) -> dict[str, dict]:
    return {bx["id"]: bx for bx in patcher_boxes(patcher)}


def nested_path(parent_path: str, box: dict) -> str:
    return f"{parent_path} > {box.get('id', '<missing-id>')} ({describe(box)})"


def iter_patchers(d: dict) -> Iterable[tuple[str, dict]]:
    root = d["patcher"]
    yield "root", root
    yield from iter_nested_patchers(root, "root")


def iter_nested_patchers(patcher: dict, parent_path: str) -> Iterable[tuple[str, dict]]:
    for bx in patcher_boxes(patcher):
        nested = bx.get("patcher")
        if not isinstance(nested, dict):
            continue
        path = nested_path(parent_path, bx)
        yield path, nested
        yield from iter_nested_patchers(nested, path)


def diagnostic(path: str, message: str) -> str:
    if path == "root":
        return message
    return f"{path}: {message}"


def matches(box: dict, pattern: str) -> bool:
    needle = pattern.lower()
    hay = (box.get("maxclass", "") + " " + box.get("text", "")).lower()
    return needle in hay


def cmd_boxes(d: dict, match: str | None) -> None:
    for bx in boxes(d):
        if match and not matches(bx, match):
            continue
        print(f"{bx['id']:>10}  {describe(bx)}")


def cmd_box(d: dict, target_id: str) -> None:
    bx = by_id(d).get(target_id)
    if not bx:
        sys.exit(f"no box with id {target_id}")
    print(json.dumps(bx, indent=2))


def cmd_cords(d: dict, *, src: str | None, dst: str | None,
              src_text: str | None, dst_text: str | None) -> None:
    ids = by_id(d)

    def text_match(bid: str, pat: str) -> bool:
        bx = ids.get(bid)
        return bx is not None and matches(bx, pat)

    for ln in lines(d):
        s_id, s_out = ln["source"][0], ln["source"][1]
        d_id, d_in = ln["destination"][0], ln["destination"][1]
        if src and s_id != src:
            continue
        if dst and d_id != dst:
            continue
        if src_text and not text_match(s_id, src_text):
            continue
        if dst_text and not text_match(d_id, dst_text):
            continue
        s_desc = describe(ids[s_id]) if s_id in ids else "<missing>"
        d_desc = describe(ids[d_id]) if d_id in ids else "<missing>"
        print(f"{s_id}[{s_out}] -> {d_id}[{d_in}]   {s_desc}  =>  {d_desc}")


def cmd_validate(d: dict) -> None:
    bad = 0
    for path, patcher in iter_patchers(d):
        ids = patcher_by_id(patcher)
        for ln in patcher_lines(patcher):
            s_id = ln["source"][0]
            d_id = ln["destination"][0]
            if s_id not in ids:
                print(diagnostic(path, f"DANGLING source: {s_id} -> {d_id}"))
                bad += 1
            if d_id not in ids:
                print(diagnostic(path, f"DANGLING dest:   {s_id} -> {d_id}"))
                bad += 1
        # outlet/inlet bounds
        for ln in patcher_lines(patcher):
            s_id, s_out = ln["source"]
            d_id, d_in = ln["destination"]
            s_box = ids.get(s_id)
            d_box = ids.get(d_id)
            if s_box and s_out >= s_box.get("numoutlets", 0):
                print(diagnostic(
                    path,
                    f"OUT-OF-RANGE outlet {s_out} on {s_id} ({describe(s_box)})",
                ))
                bad += 1
            if d_box and d_in >= d_box.get("numinlets", 0):
                print(diagnostic(
                    path,
                    f"OUT-OF-RANGE inlet {d_in} on {d_id} ({describe(d_box)})",
                ))
                bad += 1
    if bad == 0:
        print("ok: all patchlines resolve")


def main(argv: Iterable[str] | None = None) -> None:
    ap = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    ap.add_argument("file", type=Path)
    sub = ap.add_subparsers(dest="cmd", required=True)

    p_boxes = sub.add_parser("boxes", help="list boxes (id, maxclass, text)")
    p_boxes.add_argument("--match", help="substring filter on maxclass+text")

    p_box = sub.add_parser("box", help="show one box's full JSON")
    p_box.add_argument("id")

    p_cords = sub.add_parser("cords", help="list patchlines, optionally filtered")
    p_cords.add_argument("--from", dest="src", help="source object id")
    p_cords.add_argument("--to", dest="dst", help="destination object id")
    p_cords.add_argument("--from-text", help="source maxclass/text substring")
    p_cords.add_argument("--to-text", help="destination maxclass/text substring")

    sub.add_parser("validate", help="report dangling/out-of-range patchlines")
    sub.add_parser("raw", help="dump the parsed patcher JSON")

    args = ap.parse_args(list(argv) if argv is not None else None)
    d = load(args.file)

    if args.cmd == "boxes":
        cmd_boxes(d, args.match)
    elif args.cmd == "box":
        cmd_box(d, args.id)
    elif args.cmd == "cords":
        cmd_cords(d, src=args.src, dst=args.dst,
                  src_text=args.from_text, dst_text=args.to_text)
    elif args.cmd == "validate":
        cmd_validate(d)
    elif args.cmd == "raw":
        print(json.dumps(d, indent=2))


if __name__ == "__main__":
    main()
