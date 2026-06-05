#!/usr/bin/env python3
"""Generate devices/poryaaaa.amxd — Max for Live INSTRUMENT device that hosts
the [poryaaaa~] m4a synth and provides UI for project root, voicegroup
(bank), volume, reverb, polyphony, reload, and panic.

Built with py2max. Run via the project venv:

    scripts/.venv/bin/python scripts/gen_poryaaaa_amxd.py

Architecture:

  - One [poryaaaa~] instance per Live set. All ccomidi devices on other
    tracks route their MIDI to this device's track. Each ccomidi sends
    Program Change on its assigned channel; poryaaaa~ tracks per-channel
    program internally, so this device intentionally has NO preset menu.
  - Project root + chosen bank → wrapper message `voicegroup <root> <bank>`
    (A_SYM A_SYM). The wrapper takes the same args the original
    poryaaaa CLAP plugin's "voicegroup" parameter consumed.
  - Volume (songvol 0-127), Reverb (reverb 0-127), Polyphony (maxpcm 1-12)
    are Live parameters — saved/restored automatically by Live across set
    save/reload, and live.dial sends matching `<attr> $1` messages on edit.
  - The Node for Max voicegroup server owns project-root and selected-bank strings and persists
    them to a JSON file at ~/Library/Application Support/poryaaaa/projects.json
    (or %APPDATA%\\poryaaaa\\projects.json on Windows). Live's parameter system can't reliably
    persist symbol values for our hidden pattrs, so we own the persistence
    side ourselves.
  - Bank scanner is [node.script poryaaaa_voicegroup_server.js]. It emits
    route-tagged bank/path/voicegroup messages; restore happens in Node from
    the last saved voicegroup state.
"""

import json
import shutil
import struct
from pathlib import Path

from py2max import Patcher
from py2max.core import Box
from py2max.m4l import add_to_presentation, ensure_amxd_project_block
from poryaaaa_voicegroup_amxd import (
    NODE_FOR_MAX_BIN_ATTRS,
    add_node_boot_chain,
    add_voicegroup_controls,
)


PROJECT_DIR = Path(__file__).resolve().parent.parent
OUT_PATH = PROJECT_DIR / "devices" / "poryaaaa.amxd"

# Live's User Library "Imported" folder. Devices placed here appear in the Live
# Browser under User Library → Presets → Instruments → Max Instrument → Imported,
# so reloading from the browser picks up the rebuilt .amxd without a manual copy.
# Skipped silently on machines that don't have this path (CI, other dev hosts).
LIVE_IMPORTED_DIR = (
    Path.home() / "Music" / "Ableton" / "User Library" / "Presets"
    / "Instruments" / "Max Instrument" / "Imported"
)


def install_into_live_library(amxd_path: Path) -> None:
    if not LIVE_IMPORTED_DIR.exists():
        print(f"skip Live install: {LIVE_IMPORTED_DIR} not found")
        return
    dest = LIVE_IMPORTED_DIR / amxd_path.name
    shutil.copy2(amxd_path, dest)
    print(f"installed → {dest}")


# py2max's pack_amxd uses an "mx@c" sub-block envelope which Max happily
# accepts for midi_effect / audio_effect devices but apparently rejects for
# *instrument* devices — Live throws "parsing object, possible missing
# initial '{' character: line=1, char=2, text='...m'", which is Max reading
# the file as raw JSON and choking on the 'm' of the binary 'ampf' magic
# at byte 1 (1-indexed). Factory Max-exported instrument .amxd files use a
# different, simpler envelope: ampf + version + kind + meta(=1) + ptch +
# raw JSON + NUL. This packer mirrors that.
DEVICE_TYPE_TAG = {
    "audio_effect": b"aaaa",
    "instrument":   b"iiii",
    "midi_effect":  b"mmmm",
}


def pack_amxd_factory(json_text, device_type):
    """Pack a patcher JSON into a factory-format .amxd binary."""
    tag = DEVICE_TYPE_TAG[device_type]
    body = json_text.encode("utf-8") + b"\x00"
    return (
        b"ampf"
        + struct.pack("<I", 4)             # version
        + tag                              # kind: aaaa/iiii/mmmm
        + b"meta"
        + struct.pack("<I", 4)             # meta content size
        + struct.pack("<I", 1)             # meta value (1 = raw JSON)
        + b"ptch"
        + struct.pack("<I", len(body))     # ptch payload = JSON + NUL
        + body
    )


def write_amxd_factory(patcher, path, device_type):
    """Render a py2max Patcher and write it using the factory binary format."""
    patcher.render()
    patcher_dict = patcher.to_dict()
    ensure_amxd_project_block(patcher_dict, device_type=device_type)
    payload = json.dumps(patcher_dict, indent=4)
    Path(path).parent.mkdir(parents=True, exist_ok=True)
    Path(path).write_bytes(pack_amxd_factory(payload, device_type))


def add_raw(p, *, maxclass, numinlets, numoutlets, outlettype, patching_rect,
            **kwds):
    """Low-level add for maxclass-style objects (live.text, live.dial, etc.)."""
    return p.add_box(Box(
        id=p.get_id(maxclass),
        maxclass=maxclass,
        numinlets=numinlets,
        numoutlets=numoutlets,
        outlettype=outlettype,
        patching_rect=patching_rect,
        **kwds,
    ))


def live_text_button(p, label, patching_rect, *, varname=None, pres_rect=None):
    """Momentary live.text button; not a Live parameter."""
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


def live_toggle(p, *, longname, shortname, default, varname,
                patching_rect, pres_rect):
    """live.toggle wired as a Live parameter (persists with the Live set)."""
    box = add_raw(
        p,
        maxclass="live.toggle",
        numinlets=1, numoutlets=1, outlettype=[""],
        patching_rect=patching_rect,
        parameter_enable=1,
        varname=varname,
        saved_attribute_attributes={
            "valueof": {
                "parameter_longname": longname,
                "parameter_shortname": shortname,
                "parameter_type": 1,
                "parameter_mmin": 0,
                "parameter_mmax": 1,
                "parameter_initial_enable": 1,
                "parameter_initial": [default],
            },
        },
    )
    add_to_presentation(box, pres_rect)
    return box


def live_dial(p, *, longname, shortname, ptype, lo, hi, default,
              patching_rect, pres_rect, varname):
    """live.dial wired as a Live parameter (saved with the Live set).

    ptype: 0=int, 1=float (we use 0 for all three of ours).
    """
    box = add_raw(
        p,
        maxclass="live.dial",
        numinlets=1, numoutlets=1, outlettype=[""],
        patching_rect=patching_rect,
        parameter_enable=1,
        varname=varname,
        saved_attribute_attributes={
            "valueof": {
                "parameter_longname": longname,
                "parameter_shortname": shortname,
                "parameter_type": ptype,
                "parameter_mmin": lo,
                "parameter_mmax": hi,
                "parameter_initial_enable": 1,
                "parameter_initial": [default],
            },
        },
    )
    add_to_presentation(box, pres_rect)
    return box


def add_recorder_section(p, *, poryaaaa, status_outlet_targets,
                         thisdevice, boot_msg, devicewidth):
    """Recorder UI + wiring for the v8-owned save flow.

    The C++ external (poryaaaa~) holds only a thin byte buffer. JS owns
    ticks, tempo, loop, SMF format, and disk IO. The patcher's job here is:

      1. Feed plugsync~'s "Ticks (1 PPQ)" outlet into poryaaaa~ as a
         `beats <float>` message so every captured MIDI event gets a
         timeline-relative beat stamp.
      2. Wire the Record toggle to `record $1` on poryaaaa~.
      3. Route the Save button through the v8 (which orchestrates the
         dump → read → SMF-build → disk-write flow).
      4. Route `dumped <path> <count>` replies from poryaaaa~'s status
         outlet back into the v8 so the dump-promise can resolve.
      5. Hold the filename / range / loop-marker textedits and forward edits
         into the v8.

    What was removed (compared to the previous embedded-recorder build):
      - the is_playing transport observer + sendall trick on arm
      - loop_on/loop_start/loop_length observers + loop_pak
      - the Clear button (clear is implicit on next Rec rising edge)
      - the status metro/status comment (no `status` handler in the new
        external, so there's no event-count poll to display)

    `poryaaaa` — the [poryaaaa~] textbox.
    `status_outlet_targets` — [prepend dumped] / [prepend dumpfailed] objects
                 downstream of poryaaaa~'s status outlet. Their outlet 0
                 invokes the recorder v8's dump reply handlers.
    """
    REC_X = 950.0
    REC_Y = 30.0

    p.add_comment(
        "MIDI recorder (v8-owned)",
        patching_rect=[REC_X, REC_Y, 200.0, 18.0],
    )

    rec_v8 = p.add_textbox(
        "v8 ccomidi_recorder.js",
        patching_rect=[REC_X, REC_Y + 70.0, 220.0, 22.0],
        numoutlets=7,
        outlettype=["", "", "", "", "", "", ""],
    )
    # Boot: reuse the existing `ready` message from live.thisdevice's left outlet.
    p.add_line(boot_msg, rec_v8)

    # v8 outlet 0 carries two message kinds:
    #   - `dump <path>`   → [poryaaaa~] (asks the external to dump its buffer)
    #   - `unlink <path>` → [node.script cleanup.js] (deletes a temp file)
    # [route unlink] splits unlink off; everything else (dump) falls through.
    # See docs/max-gotchas.md "v8 Folder is not the right tool…" for why a
    # Node-for-Max sidecar handles the actual fs.unlinkSync call.
    rec_route = p.add_textbox(
        "route unlink",
        patching_rect=[REC_X, REC_Y + 105.0, 100.0, 22.0],
    )
    p.add_line(rec_v8, rec_route, outlet=0)
    # Unmatched (dump and any future passthrough) → [poryaaaa~].
    p.add_line(rec_route, poryaaaa, outlet=1)

    # [node.script cleanup.js @autostart 1] — sidecar that owns fs.unlinkSync
    # for temp files. The Node process is explicitly started from loadbang;
    # Live-dependent recorder restore remains gated by live.thisdevice.
    cleanup_node = p.add_textbox(
        f"node.script cleanup.js @autostart 1 {NODE_FOR_MAX_BIN_ATTRS}",
        patching_rect=[REC_X + 120.0, REC_Y + 135.0, 660.0, 22.0],
    )
    cleanup_loadbang = p.add_textbox(
        "loadbang",
        patching_rect=[REC_X + 40.0, REC_Y + 105.0, 70.0, 22.0],
    )
    cleanup_start = p.add_message(
        "script start",
        patching_rect=[REC_X + 120.0, REC_Y + 105.0, 90.0, 22.0],
    )
    p.add_line(cleanup_loadbang, cleanup_start)
    p.add_line(cleanup_start, cleanup_node)
    # [route unlink] strips the matched selector, so its outlet 0 emits just
    # the bare path. Re-prepend `unlink` so node.script's max-api dispatches
    # to the addHandler("unlink", …) handler registered in cleanup.js.
    cleanup_prep = p.add_textbox(
        "prepend unlink",
        patching_rect=[REC_X, REC_Y + 135.0, 100.0, 22.0],
    )
    p.add_line(rec_route, cleanup_prep, outlet=0)
    p.add_line(cleanup_prep, cleanup_node)

    # Forward dump replies from poryaaaa~'s status outlet into the recorder v8.
    for target in status_outlet_targets:
        p.add_line(target, rec_v8)

    # ---- Layout constants (used by Record toggle + UI section below) ---
    PRES_REC_LBL_X = 228
    PRES_REC_LBL_W = 24
    PRES_REC_TOG_X = PRES_REC_LBL_X + PRES_REC_LBL_W      # 252
    PRES_REC_TOG_W = 16
    PRES_REC_TOG_H = 16
    PRES_SAVE_X    = PRES_REC_TOG_X + PRES_REC_TOG_W + 10  # 278
    PRES_SAVE_Y    = 6
    PRES_BTN_W     = 56
    PRES_BTN_H     = 18
    PRES_GAP       = 6
    # Single-row layout:
    # Rec | Save | Start | Beats | LoopIn | LoopOut | Filename | Status.
    # Start is the absolute song beat; Beats is the export duration in beats.
    # LoopIn/LoopOut are Ableton bar.beat.sixteenth positions for SMF markers.
    PRES_START_X   = PRES_SAVE_X + PRES_BTN_W + PRES_GAP   # 340
    PRES_START_Y   = PRES_SAVE_Y
    PRES_START_W   = 60
    PRES_LEN_X     = PRES_START_X + PRES_START_W + PRES_GAP   # 406
    PRES_LEN_Y     = PRES_SAVE_Y
    PRES_LEN_W     = 60
    PRES_LOOP_START_X = PRES_LEN_X + PRES_LEN_W + PRES_GAP  # 472
    PRES_LOOP_W       = 60
    PRES_LOOP_END_X   = PRES_LOOP_START_X + PRES_LOOP_W + PRES_GAP  # 538
    PRES_FN_X      = PRES_LOOP_END_X + PRES_LOOP_W + PRES_GAP   # 604
    PRES_FN_Y      = PRES_SAVE_Y
    # Status indicator sits to the right of the filename textedit on the same
    # row. ~104px is wide enough for "Saved: <name>.mid" — longer event-count
    # suffixes truncate, which is acceptable visual feedback.
    PRES_STATUS_W  = 104
    PRES_FN_W      = devicewidth - PRES_FN_X - 8 - PRES_STATUS_W - PRES_GAP
    PRES_FN_H      = PRES_BTN_H
    PRES_STATUS_X  = PRES_FN_X + PRES_FN_W + PRES_GAP
    PRES_STATUS_Y  = PRES_FN_Y
    PRES_STATUS_H  = PRES_BTN_H

    # ---- Record toggle -------------------------------------------------------
    # Direct cord to poryaaaa~. Every toggle transition explicitly clears the
    # capture buffer before applying `record $1`; this guarantees a fresh take
    # even if Live restores/touches the parameter without a clean edge.
    record_toggle = live_toggle(
        p,
        longname="Record", shortname="Rec", default=0,
        varname="RecordArm",
        patching_rect=[REC_X + 250.0, REC_Y + 70.0, 24, 24],
        pres_rect=[PRES_REC_TOG_X, PRES_SAVE_Y + 1,
                   PRES_REC_TOG_W, PRES_REC_TOG_H],
    )
    rec_label = add_raw(
        p,
        maxclass="comment", text="Rec",
        numinlets=1, numoutlets=0, outlettype=[],
        patching_rect=[REC_X + 200.0, REC_Y + 70.0, 40, 18],
    )
    add_to_presentation(
        rec_label,
        [PRES_REC_LBL_X, PRES_SAVE_Y + 2, PRES_REC_LBL_W, 14],
    )
    record_msg = p.add_message(
        "record $1",
        patching_rect=[REC_X + 250.0, REC_Y + 105.0, 100.0, 22.0],
    )
    record_trig = p.add_textbox(
        "t i b",
        patching_rect=[REC_X + 360.0, REC_Y + 105.0, 60.0, 22.0],
        numoutlets=2,
        outlettype=["int", "bang"],
    )
    record_clear_msg = p.add_message(
        "clear",
        patching_rect=[REC_X + 430.0, REC_Y + 105.0, 60.0, 22.0],
    )
    p.add_line(record_toggle, record_trig)
    p.add_line(record_trig, record_msg, outlet=0)
    p.add_line(record_trig, record_clear_msg, outlet=1)
    p.add_line(record_clear_msg, poryaaaa)
    p.add_line(record_msg, poryaaaa)
    p.add_line(record_msg, rec_v8)

    # ---- plugsync~ → beats handler -----------------------------------------
    # [plugsync~] outlet id=6 (1-based outlet 7, the "Ticks (1 PPQ)" float)
    # is the only host-sync source that advances at the correct rate during
    # Live's offline Export Audio. Each MIDI event coming into porya_int gets
    # stamped with the most-recent latched beat value. JS quantizes that beat
    # value to the SMF PPQ grid at Save time.
    #
    # Note: plugsync~'s outlets are scheduler-domain (message) outputs, not
    # signals — the object name has a `~` but its outputs are typed
    # int/double/list per the maxref. So we just connect it via a normal
    # cord through [prepend beats].
    plugsync = p.add_textbox(
        "plugsync~",
        patching_rect=[REC_X + 100.0, REC_Y + 130.0, 90.0, 22.0],
    )
    beats_prep = p.add_textbox(
        "prepend beats",
        patching_rect=[REC_X + 100.0, REC_Y + 165.0, 110.0, 22.0],
    )
    p.add_line(plugsync, beats_prep, outlet=6)
    p.add_line(beats_prep, poryaaaa)

    # ---- Tempo observer (engine, not recorder) ------------------------------
    # poryaaaa~'s `tempo` handler still drives the m4a engine's tempo so
    # playback timing is right. The recorder side now reads tempo from
    # LiveAPI at Save time inside the v8.
    tempo_path_msg = p.add_message(
        "path live_set",
        patching_rect=[REC_X + 230.0, REC_Y + 130.0, 110.0, 22.0],
    )
    tempo_path = p.add_textbox(
        "live.path",
        patching_rect=[REC_X + 230.0, REC_Y + 160.0, 90.0, 22.0],
    )
    tempo_obs = p.add_textbox(
        "live.observer tempo",
        patching_rect=[REC_X + 230.0, REC_Y + 190.0, 160.0, 22.0],
    )
    tempo_prep = p.add_textbox(
        "prepend tempo",
        patching_rect=[REC_X + 230.0, REC_Y + 220.0, 110.0, 22.0],
    )
    p.add_line(thisdevice, tempo_path_msg, outlet=0)
    p.add_line(tempo_path_msg, tempo_path)
    p.add_line(tempo_path, tempo_obs)
    p.add_line(tempo_obs, tempo_prep)
    p.add_line(tempo_prep, poryaaaa)

    # ---- UI ------------------------------------------------------------------
    FN_PATCH_X = REC_X
    FN_PATCH_Y = REC_Y + 300.0

    # Save button → first force the current filename / range / marker textedit
    # values out, then send `save` to v8. This covers Live/Max restoring
    # visible textedit values without v8 having seen fresh messages;
    # importantly, blank fields must actively clear persisted values.
    save_btn = live_text_button(
        p, "Save",
        patching_rect=[FN_PATCH_X, FN_PATCH_Y + 40.0, PRES_BTN_W, 22.0],
        pres_rect=[PRES_SAVE_X, PRES_SAVE_Y, PRES_BTN_W, PRES_BTN_H],
    )
    save_trig = p.add_textbox(
        "trigger bang bang bang bang bang bang",
        patching_rect=[FN_PATCH_X, FN_PATCH_Y + 70.0, 180.0, 22.0],
        numoutlets=6,
        outlettype=["bang", "bang", "bang", "bang", "bang", "bang"],
    )
    save_msg = p.add_message(
        "save",
        patching_rect=[FN_PATCH_X + 130.0, FN_PATCH_Y + 70.0, 60.0, 22.0],
    )
    p.add_line(save_btn, save_trig)
    p.add_line(save_trig, save_msg, outlet=0)
    p.add_line(save_msg, rec_v8)

    # Filename textedit → v8 (via `filename <text>`).
    fn_edit = add_raw(
        p,
        maxclass="textedit",
        numinlets=1, numoutlets=4, outlettype=["", "", "", ""],
        patching_rect=[FN_PATCH_X, FN_PATCH_Y, 220.0, 22.0],
    )
    add_to_presentation(
        fn_edit,
        [PRES_FN_X, PRES_FN_Y, PRES_FN_W, PRES_FN_H],
    )

    route_text = p.add_textbox(
        "route text",
        patching_rect=[FN_PATCH_X, FN_PATCH_Y + 30.0, 80.0, 22.0],
    )
    tosymbol = p.add_textbox(
        "tosymbol",
        patching_rect=[FN_PATCH_X + 90.0, FN_PATCH_Y + 30.0, 80.0, 22.0],
    )
    fn_prep = p.add_textbox(
        "prepend filename",
        patching_rect=[FN_PATCH_X + 180.0, FN_PATCH_Y + 30.0, 120.0, 22.0],
    )
    p.add_line(fn_edit, route_text)
    p.add_line(route_text, tosymbol)
    p.add_line(tosymbol, fn_prep)
    p.add_line(fn_prep, rec_v8)
    p.add_line(save_trig, fn_edit, outlet=1)

    # v8 outlet 1 → textedit (restore filename on load).
    p.add_line(rec_v8, fn_edit, outlet=1)

    # ---- Start / Length textedits (beat counts) ----------------------------
    # When either field is empty, no explicit export range is used: the SMF
    # spans the whole captured buffer at song-absolute positions. When BOTH
    # are set: events before Start are dropped, tick 0 of the SMF = Start, and
    # Beats limits the export duration. Parse failures are warned in the Max
    # console and treated as empty.
    def _make_range_field(label_suffix, patch_dx, prep_selector, restore_outlet):
        edit = add_raw(
            p,
            maxclass="textedit",
            numinlets=1, numoutlets=4, outlettype=["", "", "", ""],
            patching_rect=[FN_PATCH_X + patch_dx, FN_PATCH_Y + 80.0, 80.0, 22.0],
        )
        rt = p.add_textbox(
            "route text",
            patching_rect=[FN_PATCH_X + patch_dx, FN_PATCH_Y + 110.0, 80.0, 22.0],
        )
        ts = p.add_textbox(
            "tosymbol",
            patching_rect=[FN_PATCH_X + patch_dx, FN_PATCH_Y + 140.0, 80.0, 22.0],
        )
        pr = p.add_textbox(
            f"prepend {prep_selector}",
            patching_rect=[FN_PATCH_X + patch_dx, FN_PATCH_Y + 170.0, 120.0, 22.0],
        )
        p.add_line(edit, rt)
        p.add_line(rt, ts)
        p.add_line(ts, pr)
        p.add_line(pr, rec_v8)
        # v8 → textedit (restore persisted value on load).
        p.add_line(rec_v8, edit, outlet=restore_outlet)
        return edit

    start_edit = _make_range_field("Start", 340.0, "startbeat",   3)
    len_edit   = _make_range_field("Beats", 430.0, "lengthbeats", 4)
    loop_start_edit = _make_range_field("LoopIn", 520.0, "loopstart", 5)
    loop_end_edit   = _make_range_field("LoopOut", 610.0, "loopend", 6)
    p.add_line(save_trig, start_edit, outlet=2)
    p.add_line(save_trig, len_edit, outlet=3)
    p.add_line(save_trig, loop_start_edit, outlet=4)
    p.add_line(save_trig, loop_end_edit, outlet=5)
    add_to_presentation(
        start_edit,
        [PRES_START_X, PRES_START_Y, PRES_START_W, PRES_FN_H],
    )
    add_to_presentation(
        len_edit,
        [PRES_LEN_X, PRES_LEN_Y, PRES_LEN_W, PRES_FN_H],
    )
    add_to_presentation(
        loop_start_edit,
        [PRES_LOOP_START_X, PRES_START_Y, PRES_LOOP_W, PRES_FN_H],
    )
    add_to_presentation(
        loop_end_edit,
        [PRES_LOOP_END_X, PRES_START_Y, PRES_LOOP_W, PRES_FN_H],
    )

    # v8 outlet 2 → save-status indicator. The v8 emits `set <text>` directly
    # at every save-flow transition (Saving... / Saved: ... / FAILED: ...).
    # Persistent — stays on screen until the next save attempt overwrites it.
    # `comment` accepts a `set` message verbatim (see docs/max-ref/max/comment.md).
    status_comment = add_raw(
        p,
        maxclass="comment",
        text="(no saves yet)",
        numinlets=1, numoutlets=0, outlettype=[],
        patching_rect=[FN_PATCH_X + 320.0, FN_PATCH_Y, PRES_STATUS_W, 22.0],
    )
    add_to_presentation(
        status_comment,
        [PRES_STATUS_X, PRES_STATUS_Y, PRES_STATUS_W, PRES_STATUS_H],
    )
    p.add_line(rec_v8, status_comment, outlet=2)


def build():
    p = Patcher(
        path=str(OUT_PATH),
        device_type="instrument",          # was midi_effect — now hosts poryaaaa~
        openinpresentation=1,
    )
    # Device strip is wider to fit the recorder UI (Save, Clear, filename
    # textedit, status comment) on existing rows without growing vertically —
    # Live's device window doesn't scroll, so we never stack new presentation
    # rows below the existing widgets.
    DEVICE_WIDTH = 860
    p.enable_presentation(devicewidth=DEVICE_WIDTH)

    # ------------------------------------------------------------------
    # Patching-mode layout follows the actual data flow:
    #   left:   boot, persistence, cross-device requests
    #   middle: project chooser, Node voicegroup server, bank routing
    #   right:  synth wrapper, audio/MIDI I/O, controls
    #   bottom: MIDI channel activity monitor
    # ------------------------------------------------------------------

    BOOT_X = 30.0
    MAIN_X = 300.0
    ENGINE_X = 620.0
    MON_X = 300.0
    MON_Y = 470.0

    boot_label = p.add_comment(
        "Boot / saved state",
        patching_rect=[BOOT_X, 20.0, 170.0, 18.0],
    )

    thisdevice = p.add_textbox(
        "live.thisdevice",
        patching_rect=[BOOT_X, 50.0, 120.0, 22.0],
    )

    # The left outlet fires when Live has finished restoring the device. The
    # recorder v8 still consumes `ready`; the voicegroup Node sidecar starts
    # from loadbang and receives a delayed Live-ready restore.
    boot_msg = p.add_message(
        "ready",
        patching_rect=[BOOT_X, 85.0, 100.0, 22.0],
    )
    p.add_line(thisdevice, boot_msg, outlet=0)

    node_boot_chain = add_node_boot_chain(
        p,
        thisdevice=thisdevice,
        boot_x=BOOT_X,
    )
    voicegroup_controls = add_voicegroup_controls(
        p,
        boot_x=BOOT_X,
        main_x=MAIN_X,
        thisdevice=thisdevice,
        node_start_msg=node_boot_chain.node_start_msg,
        restore_msg=node_boot_chain.restore_msg,
        add_raw=add_raw,
        live_text_button=live_text_button,
    )

    # ============ Engine + controls ====================================
    engine_label = p.add_comment(
        "Engine / audio / controls",
        patching_rect=[ENGINE_X, 20.0, 190.0, 18.0],
    )

    # MIDI in (raw bytes, dispatched via wrapper's `int` method)
    midiin = p.add_textbox(
        "midiin",
        patching_rect=[ENGINE_X, 50.0, 60.0, 22.0],
    )

    # numoutlets=3:
    #   outlet 0 = signal L  → plugout~ 1
    #   outlet 1 = signal R  → plugout~ 2
    #   outlet 2 = recorder replies (`dumped` / `dumpfailed`) → recorder v8
    poryaaaa = p.add_textbox(
        "poryaaaa~",
        patching_rect=[ENGINE_X, 325.0, 120.0, 22.0],
        numinlets=1, numoutlets=3,
        outlettype=["signal", "signal", ""],
    )

    # voicegroup output from [route] has had its selector stripped; restore it
    # before poryaaaa~ so the external receives `voicegroup <root> <bank>`.
    p.add_line(voicegroup_controls.voicegroup_prepend, poryaaaa)

    # poryaaaa~ outlet 2 emits recorder messages:
    #   - `dumped <path> <count>` after MidiBuffer::dump_to_file completes
    #     → recorder v8 (wired downstream in add_recorder_section).
    #   - `dumpfailed <path> <reason>` if a dump request cannot complete.
    # [route dumped dumpfailed] strips selectors; re-prepend them for the
    # recorder v8 global handlers.
    status_route = p.add_textbox(
        "route dumped dumpfailed",
        patching_rect=[ENGINE_X, 295.0, 210.0, 22.0],
        numoutlets=3,
        outlettype=["", "", ""],
    )
    p.add_line(poryaaaa, status_route, outlet=2)
    # Re-prepend `dumped` for the recorder v8 so its global `dumped` handler
    # receives the selector + args. The recorder section wires this prepend's
    # output into rec_v8.
    dumped_reprepend = p.add_textbox(
        "prepend dumped",
        patching_rect=[ENGINE_X + 150.0, 265.0, 110.0, 22.0],
    )
    p.add_line(status_route, dumped_reprepend, outlet=0)
    dumpfailed_reprepend = p.add_textbox(
        "prepend dumpfailed",
        patching_rect=[ENGINE_X + 280.0, 265.0, 130.0, 22.0],
    )
    p.add_line(status_route, dumpfailed_reprepend, outlet=1)

    p.add_line(midiin, poryaaaa)

    # Audio out: outlet 0 = L, outlet 1 = R (per wrapper source comment).
    # Syntax is `[plugout~ N]` for M4L instrument outputs — the older
    # `[plugout~ Out~ N]` form is `vst~`-host plugin syntax (where "Out~"
    # is a named outlet of the hosted plugin) and does NOT apply here.
    # Using the wrong form silently breaks the right channel: Max either
    # routes both plugouts to channel 1 (you hear a mono mix on left,
    # silence on right) or fails to bind the right output entirely.
    plugout_l = p.add_textbox(
        "plugout~ 1",
        patching_rect=[ENGINE_X, 370.0, 110.0, 22.0],
    )
    plugout_r = p.add_textbox(
        "plugout~ 2",
        patching_rect=[ENGINE_X + 120.0, 370.0, 110.0, 22.0],
    )
    p.add_line(poryaaaa, plugout_l, outlet=0)
    p.add_line(poryaaaa, plugout_r, outlet=1)

    # ---- live.dial: Volume (songvol 0..127, default 127) ----
    vol_dial = live_dial(
        p,
        longname="Volume", shortname="Vol",
        ptype=0, lo=0, hi=127, default=127,
        patching_rect=[ENGINE_X, 105.0, 40.0, 48.0],
        pres_rect=[8, 84, 50, 50],
        varname="VolDial",
    )
    vol_msg = p.add_message(
        "songvol $1",
        patching_rect=[ENGINE_X, 170.0, 100.0, 22.0],
    )
    p.add_line(vol_dial, vol_msg)
    p.add_line(vol_msg, poryaaaa)

    # ---- live.dial: Reverb (reverb 0..127, default 0) ----
    rev_dial = live_dial(
        p,
        longname="Reverb", shortname="Rev",
        ptype=0, lo=0, hi=127, default=0,
        patching_rect=[ENGINE_X + 65.0, 105.0, 40.0, 48.0],
        pres_rect=[68, 84, 50, 50],
        varname="RevDial",
    )
    rev_msg = p.add_message(
        "reverb $1",
        patching_rect=[ENGINE_X + 65.0, 170.0, 100.0, 22.0],
    )
    p.add_line(rev_dial, rev_msg)
    p.add_line(rev_msg, poryaaaa)

    # ---- live.dial: Polyphony (maxpcm 1..12, default 12) ----
    poly_dial = live_dial(
        p,
        longname="Polyphony", shortname="Poly",
        ptype=0, lo=1, hi=12, default=12,
        patching_rect=[ENGINE_X + 130.0, 105.0, 40.0, 48.0],
        pres_rect=[128, 84, 50, 50],
        varname="PolyDial",
    )
    poly_msg = p.add_message(
        "maxpcm $1",
        patching_rect=[ENGINE_X + 130.0, 170.0, 100.0, 22.0],
    )
    p.add_line(poly_dial, poly_msg)
    p.add_line(poly_msg, poryaaaa)

    # ---- Reload selected voicegroup from disk ----
    reload_btn = live_text_button(
        p, "Reload Voicegroup",
        patching_rect=[ENGINE_X + 220.0, 105.0, 110.0, 22.0],
        varname="ReloadVoicegroupBtn",
        pres_rect=[184, 78, 98, 18],
    )
    reload_msg = p.add_message(
        "reload",
        patching_rect=[ENGINE_X + 245.0, 145.0, 70.0, 22.0],
    )
    p.add_line(reload_btn, reload_msg)
    p.add_line(reload_msg, voicegroup_controls.scanner)

    # ---- Panic button ----
    panic_btn = live_text_button(
        p, "Panic",
        patching_rect=[ENGINE_X + 220.0, 185.0, 50.0, 22.0],
        varname="PanicBtn",
        pres_rect=[200, 100, 60, 18],
    )
    panic_msg = p.add_message(
        "panic",
        patching_rect=[ENGINE_X + 220.0, 225.0, 60.0, 22.0],
    )
    p.add_line(panic_btn, panic_msg)
    p.add_line(panic_msg, poryaaaa)

    # ============ MIDI activity indicators =============================
    # Parallel branch off [midiin] purely for indicator triggering. The
    # synth path stays untouched. [midiparse] drops XCMD bytes (those ride
    # on CCs in the m4a engine), but note-on is what we care about for an
    # activity LED, so this is fine — XCMD is the wrapper's concern.
    #
    # midiparse fires its outlets right-to-left, so for a single MIDI event
    # outlet 6 (channel) lands in the [int]'s sticky-store slot BEFORE
    # outlet 0 (note pitch+vel) hits the vel-filter chain. By the time the
    # bang reaches the [int]'s left inlet, the right channel is in place.
    monitor_label = p.add_comment(
        "MIDI channel activity monitor",
        patching_rect=[MON_X, MON_Y, 220.0, 18.0],
    )

    mparse = p.add_textbox(
        "midiparse",
        patching_rect=[MON_X, MON_Y + 35.0, 90.0, 22.0],
    )
    p.add_line(midiin, mparse)

    # ---- vel-filter chain: bang only on note-on (vel > 0) ----
    # midiparse normalises note-off (status 0x8n) to vel=0, and a true
    # note-on with vel=0 is also note-off, so [> 0] is the right filter.
    unpack_note = p.add_textbox(
        "unpack 0 0",
        patching_rect=[MON_X, MON_Y + 75.0, 80.0, 22.0],
    )
    p.add_line(mparse, unpack_note, outlet=0)

    gt_zero = p.add_textbox(
        "> 0",
        patching_rect=[MON_X + 50.0, MON_Y + 115.0, 50.0, 22.0],
    )
    # unpack outlet 1 (right) = velocity, fires before pitch
    p.add_line(unpack_note, gt_zero, outlet=1)

    sel_on = p.add_textbox(
        "sel 1",
        patching_rect=[MON_X + 50.0, MON_Y + 155.0, 50.0, 22.0],
    )
    p.add_line(gt_zero, sel_on)

    # ---- sticky channel int: stored on right inlet, banged out left ----
    chan_int = p.add_textbox(
        "int",
        patching_rect=[MON_X + 130.0, MON_Y + 75.0, 40.0, 22.0],
    )
    p.add_line(mparse, chan_int, outlet=6, inlet=1)   # store (no output)
    p.add_line(sel_on, chan_int, inlet=0)             # bang → output stored

    # ---- channel router: 16 outlets, one per channel ----
    sel_chans = add_raw(
        p,
        maxclass="newobj",
        text="sel 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16",
        numinlets=2,
        numoutlets=17,
        outlettype=["bang"] * 16 + [""],
        patching_rect=[MON_X, MON_Y + 205.0, 320.0, 22.0],
    )
    p.add_line(chan_int, sel_chans)

    # ---- 16 LED widgets ----
    # `live.indicator` doesn't exist in Max — there's no live.* equivalent
    # for a flashing indicator. The right pick is the classic Max [button],
    # which has a built-in flash animation on bang and works in M4L
    # presentation mode. Each [button] also re-emits the bang from its
    # outlet; we leave those unwired.
    #
    # Patching mode: 8x2 grid for legibility. Presentation mode: horizontal
    # strip just below the dial row, with a label above.
    LED_PRES_X     = 8
    LED_PRES_Y_LBL = 138
    LED_PRES_Y_LED = 154
    LED_PRES_W     = 14
    LED_PRES_H     = 14
    LED_PRES_GAP   = 2

    activity_label = p.add_comment(
        "MIDI ch activity",
        patching_rect=[MON_X, MON_Y + 350.0, 200.0, 18.0],
    )
    add_to_presentation(activity_label, [LED_PRES_X, LED_PRES_Y_LBL - 2, 120, 14])

    for ch in range(16):
        led = add_raw(
            p,
            maxclass="button",
            numinlets=1, numoutlets=1, outlettype=["bang"],
            patching_rect=[
                MON_X + (ch % 8) * 28.0,
                MON_Y + 245.0 + (ch // 8) * 28.0,
                22.0, 22.0,
            ],
            parameter_enable=0,
        )
        add_to_presentation(led, [
            LED_PRES_X + ch * (LED_PRES_W + LED_PRES_GAP),
            LED_PRES_Y_LED, LED_PRES_W, LED_PRES_H,
        ])
        p.add_line(sel_chans, led, outlet=ch)

    # ============ Recorder ==============================================
    # v8 owns the entire save flow (filename persistence, dump request, SMF
    # build, disk write). The C++ external just holds a byte buffer and
    # responds to `dump <path>` by writing a PRBY-v1 blob and emitting
    # `dumped <path> <count>` or `dumpfailed <path> <reason>` on its status
    # outlet. Both are forwarded into the recorder v8.
    add_recorder_section(
        p,
        poryaaaa=poryaaaa,
        status_outlet_targets=[dumped_reprepend, dumpfailed_reprepend],
        thisdevice=thisdevice,
        boot_msg=boot_msg,
        devicewidth=DEVICE_WIDTH,
    )

    write_amxd_factory(p, OUT_PATH, device_type="instrument")

    boxes = len(p.boxes) if hasattr(p, "boxes") else "?"
    lines = len(p.lines) if hasattr(p, "lines") else "?"
    print(f"wrote {OUT_PATH} ({boxes} boxes, {lines} lines)")

    install_into_live_library(OUT_PATH)


if __name__ == "__main__":
    build()
