"""Shared helpers for the .amxd generators.

Used by gen_poryaaaa_amxd.py and gen_ccomidi_amxd.py. Centralises:

  * factory-format .amxd packing (py2max's own pack_amxd uses an mxac/dlst
    envelope that an older release got wrong for instruments; the factory
    format used here is the simpler ampf+meta+ptch+JSON+NUL layout that
    matches what Max itself emits for hand-saved devices)
  * py2max Box construction with arbitrary kwargs (live.* widgets)
  * the parameterised live.* widget pattern shared between both gens
"""

import json
import shutil
import struct
from pathlib import Path
from typing import List, Optional, Sequence, Union

from py2max import Patcher
from py2max.core import Box
from py2max.m4l import add_to_presentation, ensure_amxd_project_block


DEVICE_TYPE_TAG = {
    "audio_effect": b"aaaa",
    "instrument":   b"iiii",
    "midi_effect":  b"mmmm",
}

LIVE_IMPORTED_ROOT = (
    Path.home() / "Music" / "Ableton" / "User Library" / "Presets"
)

LIVE_IMPORTED_DIRS = {
    "audio_effect": LIVE_IMPORTED_ROOT / "Audio Effects" / "Max Audio Effect" / "Imported",
    "instrument": LIVE_IMPORTED_ROOT / "Instruments" / "Max Instrument" / "Imported",
    "midi_effect": LIVE_IMPORTED_ROOT / "MIDI Effects" / "Max MIDI Effect" / "Imported",
}


def pack_amxd_factory(json_text: str, device_type: str) -> bytes:
    """Pack a patcher JSON string into a factory-format .amxd binary."""
    tag = DEVICE_TYPE_TAG[device_type]
    body = json_text.encode("utf-8") + b"\x00"
    return (
        b"ampf"
        + struct.pack("<I", 4)
        + tag
        + b"meta"
        + struct.pack("<I", 4)
        + struct.pack("<I", 1)
        + b"ptch"
        + struct.pack("<I", len(body))
        + body
    )


def write_amxd_factory(patcher: Patcher, path: Union[str, Path],
                       device_type: str) -> None:
    """Render a py2max Patcher and write it as a factory-format .amxd."""
    patcher.render()
    patcher_dict = patcher.to_dict()
    ensure_amxd_project_block(patcher_dict, device_type=device_type)
    payload = json.dumps(patcher_dict, indent=4)
    out = Path(path)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(pack_amxd_factory(payload, device_type))


def install_amxd_into_live_library(amxd_path: Union[str, Path],
                                   device_type: str) -> None:
    """Copy a generated device into Live's User Library Imported folder."""
    dest_dir = LIVE_IMPORTED_DIRS[device_type]
    if not dest_dir.exists():
        print(f"skip Live install: {dest_dir} not found")
        return
    src = Path(amxd_path)
    dest = dest_dir / src.name
    shutil.copy2(src, dest)
    print(f"installed -> {dest}")


def add_raw(p: Patcher, *, maxclass: str, numinlets: int, numoutlets: int,
            outlettype: List[str], patching_rect, **kwds) -> Box:
    """Low-level Box add for native maxclass widgets (live.dial, live.text…)."""
    return p.add_box(Box(
        id=p.get_id(maxclass),
        maxclass=maxclass,
        numinlets=numinlets,
        numoutlets=numoutlets,
        outlettype=outlettype,
        patching_rect=patching_rect,
        **kwds,
    ))


def live_text_button(p: Patcher, label: str, patching_rect, *,
                     varname: Optional[str] = None,
                     pres_rect=None) -> Box:
    """Momentary live.text button (mode=0). Not a Live parameter."""
    extra = {"varname": varname} if varname else {}
    box = add_raw(
        p,
        maxclass="live.text",
        numinlets=1, numoutlets=1, outlettype=[""],
        patching_rect=patching_rect,
        parameter_enable=0,
        mode=0,
        text=label,
        **extra,
    )
    if pres_rect is not None:
        add_to_presentation(box, pres_rect)
    return box


def live_param(p: Patcher, *,
               maxclass: str,
               longname: str, shortname: str,
               ptype: int,
               prange: Sequence,
               initial,
               patching_rect,
               pres_rect=None,
               varname: Optional[str] = None,
               active: bool = True) -> Box:
    """Live-parameter widget (live.dial / live.menu / live.toggle / live.numbox).

    `ptype`: 0=int, 1=float, 2=enum.
    `prange`: for enum, a list of label strings; otherwise [lo, hi].
    `pres_rect`: if None, the widget is parameter-only (saved with the Live
        set, not visible in presentation mode).
    """
    valueof = {
        "parameter_initial":        [initial],
        "parameter_initial_enable": 1,
        "parameter_longname":       longname,
        "parameter_shortname":      shortname,
        "parameter_type":           ptype,
    }
    if ptype == 2:
        valueof["parameter_range"] = list(prange)
        valueof["parameter_enum"]  = list(prange)
    else:
        valueof["parameter_range"] = list(prange)
        valueof["parameter_mmin"]  = prange[0]
        valueof["parameter_mmax"]  = prange[1]

    extra = {}
    if not active:
        extra["active"] = 0

    box = add_raw(
        p,
        maxclass=maxclass,
        numinlets=1,
        numoutlets=2 if maxclass == "live.menu" else 1,
        outlettype=["", "float"] if maxclass == "live.menu" else [""],
        patching_rect=patching_rect,
        parameter_enable=1,
        varname=varname or longname,
        saved_attribute_attributes={"valueof": valueof},
        **extra,
    )
    if pres_rect is not None:
        add_to_presentation(box, pres_rect)
    return box
