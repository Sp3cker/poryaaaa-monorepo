#!/usr/bin/env python3
"""Generate devices/ccomidi.amxd - Max for Live MIDI Effect device.

The device exposes fixed named controls. Legacy row names such as `r0_v0`,
`R01_V0`, and `R10_Type` are intentionally not generated.

Wiring inside the patch:

    [midiin] -> [ccomidi] -> [midiout]

Live parameter restore is pushed into the external under `restore 1/0`, so
loading a set prepares ccomidi for playback without emitting MIDI. Transport
start is sent as `transport <0|1>` and the external decides what state should
be replayed.
"""

from pathlib import Path

from py2max import Patcher
from py2max.core.common import Rect
from py2max.m4l import add_to_presentation

from _amxd_helpers import (
    add_raw,
    install_amxd_into_live_library,
    live_param,
    live_text_button,
    write_amxd_factory,
)


PROJECT_DIR = Path(__file__).resolve().parent.parent
OUT_PATH = PROJECT_DIR / "devices" / "ccomidi.amxd"
NODE_BIN_PATH = "/Users/spencer/.nvm/versions/node/v26.1.0/bin/node"
NPM_BIN_PATH = "/Users/spencer/.nvm/versions/node/v26.1.0/bin/npm"
NODE_FOR_MAX_BIN_ATTRS = f"@node_bin_path {NODE_BIN_PATH} @npm_bin_path {NPM_BIN_PATH}"


class HelperCursor:
    """Tracks helper message placement in patching mode."""

    def __init__(self, helper_x: float, rsync_x: float, y0: float = 30.0):
        self.helper_x = helper_x
        self.rsync_x = rsync_x
        self.y = y0

    def step(self, dy: float = 28.0):
        y = self.y
        self.y += dy
        return y


def _msg_width(text: str) -> int:
    return max(40, 8 * len(text) + 12)


def attr_msg(p: Patcher, ccomidi, widget, msg_text: str,
             cur: HelperCursor, sync_receive: str,
             widget_outlet: int = 0):
    """widget -> [message] -> ccomidi, plus optional restore-sync bang."""
    y = cur.step()
    msg = p.add_message(
        msg_text,
        patching_rect=[cur.helper_x, y, _msg_width(msg_text), 22],
    )
    p.add_line(widget, msg, outlet=widget_outlet)
    p.add_line(msg, ccomidi)
    if sync_receive:
        rsync = p.add_textbox(
            sync_receive,
            patching_rect=[cur.rsync_x, y, 80, 22],
            numinlets=0,
            numoutlets=1,
            outlettype=["bang"],
        )
        p.add_line(rsync, widget)
    return msg


def sub_attr_emit(child: Patcher, outlet_box, widget, msg_text: str,
                  cur: HelperCursor, sync_receive: str,
                  widget_outlet: int = 0,
                  sync_outputvalue: bool = False):
    """Subpatcher version of attr_msg."""
    y = cur.step()
    msg = child.add_message(
        msg_text,
        patching_rect=[cur.helper_x, y, _msg_width(msg_text), 22],
    )
    child.add_line(widget, msg, outlet=widget_outlet)
    child.add_line(msg, outlet_box)
    if sync_receive:
        rsync = child.add_textbox(
            sync_receive,
            patching_rect=[cur.rsync_x, y, 80, 22],
            numinlets=0,
            numoutlets=1,
            outlettype=["bang"],
        )
        if sync_outputvalue:
            outputvalue = child.add_message(
                "outputvalue",
                patching_rect=[cur.rsync_x + 90, y, 80, 22],
            )
            child.add_line(rsync, outputvalue)
            child.add_line(outputvalue, widget)
        else:
            child.add_line(rsync, widget)
    return msg


def sub_attr_emit_expr(child: Patcher, outlet_box, widget, expr_text: str,
                       msg_text: str, cur: HelperCursor, sync_receive: str):
    """Emit widget values after a simple Max expr conversion."""
    y = cur.step()
    expr = add_raw(
        child,
        maxclass="newobj",
        text=expr_text,
        numinlets=1,
        numoutlets=1,
        outlettype=["int"],
        patching_rect=[cur.helper_x, y, 130, 22],
    )
    msg = child.add_message(
        msg_text,
        patching_rect=[cur.helper_x + 140, y, _msg_width(msg_text), 22],
    )
    child.add_line(widget, expr)
    child.add_line(expr, msg)
    child.add_line(msg, outlet_box)
    rsync = child.add_textbox(
        sync_receive,
        patching_rect=[cur.rsync_x, y, 80, 22],
        numinlets=0,
        numoutlets=1,
        outlettype=["bang"],
    )
    child.add_line(rsync, widget)
    return msg


def embedded_bpatcher(parent: Patcher, *, varname: str, patching_rect,
                      pres_rect=None, args=None):
    """Create an embedded bpatcher whose inner patcher is generated inline."""
    sub = Patcher(parent=parent)
    sub.openinpresentation = 1
    sub.default_fontsize = 12.0
    sub.default_fontface = 0
    sub.default_fontname = "Ableton Sans Medium"
    sub.gridonopen = 2
    sub.gridsize = [15.0, 15.0]
    sub.gridsnaponopen = 2
    sub.objectsnaponopen = 1
    box = add_raw(
        parent,
        maxclass="bpatcher",
        numinlets=0,
        numoutlets=1,
        outlettype=[""],
        border=0,
        offset=[0.0, 0.0],
        patching_rect=patching_rect,
        patcher=sub,
        args=args or [],
        embed=1,
        varname=varname,
    )
    if pres_rect is not None:
        add_to_presentation(box, pres_rect)
    return box, sub


def build_voice_picker_section(p: Patcher, ccomidi, thisdevice):
    """Top-row voice picker.

    VoiceIdx is both the UI source of truth and the MIDI Program Change value.
    The slot list from poryaaaa is used only for labels.
    """

    PRES_Y = 4
    PRES_H = 22
    PRES_X0 = 4
    PRES_NUMBOX_W = 42
    PRES_GAP = 4
    PRES_RELOAD_W = 58
    PRES_ROUTE_W = 50
    PRES_SENDALL_W = 64
    PRES_CONN_W = 78
    PRES_DEV_W = 860

    PRES_NUMBOX_X = PRES_X0
    PRES_MENU_X = PRES_NUMBOX_X + PRES_NUMBOX_W + PRES_GAP
    PRES_SENDALL_X = PRES_DEV_W - PRES_SENDALL_W - PRES_X0
    PRES_ROUTE_X = PRES_SENDALL_X - PRES_ROUTE_W - PRES_GAP
    PRES_RELOAD_X = PRES_ROUTE_X - PRES_RELOAD_W - PRES_GAP
    PRES_CONN_X = PRES_RELOAD_X - PRES_CONN_W - PRES_GAP
    PRES_MENU_W = PRES_CONN_X - PRES_MENU_X - PRES_GAP

    PX = 30.0
    PY = 760.0

    voice_numbox = live_param(
        p,
        maxclass="live.numbox",
        longname="VoiceIdx",
        shortname="VIdx",
        ptype=0,
        prange=[0, 127],
        initial=0,
        patching_rect=[PX, PY, 60, 22],
        pres_rect=[PRES_NUMBOX_X, PRES_Y, PRES_NUMBOX_W, PRES_H],
        varname="VoiceIdx",
    )

    umenu = p.add_umenu(
        items=["(waiting for poryaaaa)"],
        patching_rect=[PX + 80, PY, 360, 22],
    )
    add_to_presentation(umenu, [PRES_MENU_X, PRES_Y, PRES_MENU_W, PRES_H])

    conn_status = add_raw(
        p,
        maxclass="comment",
        text="trying",
        numinlets=1,
        numoutlets=0,
        outlettype=[],
        patching_rect=[PX + 605, PY + 165, 90, 18],
    )
    add_to_presentation(conn_status, [PRES_CONN_X, PRES_Y + 3, PRES_CONN_W, 16])

    reload_btn = live_text_button(
        p,
        "Reload",
        patching_rect=[PX + 460, PY, 60, 22],
        pres_rect=[PRES_RELOAD_X, PRES_Y, PRES_RELOAD_W, PRES_H],
    )

    route_btn = live_text_button(
        p,
        "Route",
        patching_rect=[PX + 530, PY, 60, 22],
        pres_rect=[PRES_ROUTE_X, PRES_Y, PRES_ROUTE_W, PRES_H],
    )

    sendall_btn = live_text_button(
        p,
        "Send All",
        patching_rect=[PX + 600, PY, 70, 22],
        pres_rect=[PRES_SENDALL_X, PRES_Y, PRES_SENDALL_W, PRES_H],
    )
    sendall_msg = p.add_message(
        "sendall",
        patching_rect=[PX + 600, PY + 30, 70, 22],
    )
    p.add_line(sendall_btn, sendall_msg)
    p.add_line(sendall_msg, ccomidi)

    v8 = p.add_textbox(
        "v8 ccomidi_voices.js",
        patching_rect=[PX + 210, PY + 95, 220, 22],
        numoutlets=2,
        outlettype=["", ""],
    )

    voicegroup_client = p.add_textbox(
        f"node.script ccomidi_voicegroup_client.js @autostart 1 {NODE_FOR_MAX_BIN_ATTRS}",
        patching_rect=[PX + 440, PY + 125, 760, 22],
        numoutlets=1,
        outlettype=[""],
    )
    client_script_loadbang = p.add_textbox(
        "loadbang",
        patching_rect=[PX + 440, PY + 80, 70, 22],
    )
    client_script_start = p.add_message(
        "script start",
        patching_rect=[PX + 525, PY + 80, 90, 22],
    )
    p.add_line(client_script_loadbang, client_script_start)
    p.add_line(client_script_start, voicegroup_client)

    client_connect_start = p.add_message(
        "start",
        patching_rect=[PX + 440, PY + 200, 60, 22],
    )
    p.add_line(client_connect_start, voicegroup_client)

    client_route = p.add_textbox(
        "route ready connstatus",
        patching_rect=[PX + 440, PY + 165, 150, 22],
        numoutlets=3,
        outlettype=["", "", ""],
    )
    p.add_line(voicegroup_client, client_route)
    p.add_line(client_route, client_connect_start, outlet=0)
    p.add_line(client_route, conn_status, outlet=1)
    p.add_line(client_route, v8, outlet=2)

    p.add_line(umenu, voice_numbox, outlet=0)

    voice_trig = p.add_textbox(
        "t i i",
        patching_rect=[PX, PY + 35, 50, 22],
        numoutlets=2,
        outlettype=["int", "int"],
    )
    p.add_line(voice_numbox, voice_trig)

    program_prep = p.add_textbox(
        "prepend program",
        patching_rect=[PX + 70, PY + 35, 120, 22],
    )
    p.add_line(voice_trig, program_prep, outlet=1)
    p.add_line(program_prep, ccomidi)

    pick_msg = p.add_message(
        "pick $1",
        patching_rect=[PX + 210, PY + 35, 80, 22],
    )
    p.add_line(voice_trig, pick_msg, outlet=0)
    p.add_line(pick_msg, v8)

    sync_recv = p.add_textbox(
        "r #0-sync",
        patching_rect=[PX + 310, PY + 35, 80, 22],
        numinlets=0,
        numoutlets=1,
        outlettype=["bang"],
    )
    p.add_line(sync_recv, voice_numbox)

    autoroute_done = live_param(
        p,
        maxclass="live.toggle",
        longname="AutoRouteDone",
        shortname="AutoR",
        ptype=0,
        prange=[0, 1],
        initial=0,
        patching_rect=[PX + 680, PY, 40, 22],
        varname="AutoRouteDone",
    )
    autoroute_outputvalue = p.add_message(
        "outputvalue",
        patching_rect=[PX + 680, PY + 60, 90, 22],
    )
    p.add_line(sync_recv, autoroute_outputvalue)
    p.add_line(autoroute_outputvalue, autoroute_done)
    autoroute_msg = p.add_message(
        "autorouteifnew $1",
        patching_rect=[PX + 680, PY + 30, 130, 22],
    )
    p.add_line(autoroute_done, autoroute_msg)
    p.add_line(autoroute_msg, v8)

    route_box = p.add_textbox(
        "route slots autorouted",
        patching_rect=[PX + 210, PY + 130, 190, 22],
        numoutlets=3,
        outlettype=[""] * 3,
    )
    p.add_line(v8, route_box)
    p.add_line(route_box, umenu, outlet=0)
    p.add_line(route_box, autoroute_done, outlet=1)

    reload_msg = p.add_message(
        "reload",
        patching_rect=[PX + 460, PY + 30, 60, 22],
    )
    p.add_line(reload_btn, reload_msg)
    p.add_line(reload_msg, v8)

    route_msg = p.add_message(
        "route",
        patching_rect=[PX + 530, PY + 30, 60, 22],
    )
    p.add_line(route_btn, route_msg)
    p.add_line(route_msg, v8)

    reroute_recv = p.add_textbox(
        "r ccomidi.reroute",
        patching_rect=[PX + 210, PY + 65, 200, 22],
    )
    p.add_line(reroute_recv, v8)

    return v8


def build_main_controls_bpatcher(parent: Patcher):
    """Main performance dials."""
    box, sub = embedded_bpatcher(
        parent,
        varname="MainControls",
        patching_rect=[30.0, 500.0, 360.0, 90.0],
        pres_rect=[2.0, 32.0, 356.0, 66.0],
        args=["#0"],
    )
    out_id = add_raw(
        sub,
        maxclass="outlet",
        numinlets=1,
        numoutlets=0,
        outlettype=[],
        patching_rect=[650.0, 30.0, 30.0, 30.0],
        comment="to ccomidi",
    )
    cur = HelperCursor(helper_x=440.0, rsync_x=550.0, y0=30.0)
    sync_recv = "r #1-sync"

    dial_layout = [
        ("VOL", 64, [0, 127], "Volume", "Vol", "Volume", "vol $1"),
        ("PAN", 64, [0, 127], "Pan", "Pan", "Pan", "pan $1"),
        ("MOD", 0, [0, 127], "Mod", "Mod", "Mod", "mod $1"),
        ("LFO", 0, [0, 127], "LFO Speed", "LFOSpd", "LFOSpeed", "lfo_spd $1"),
        ("BEND", 0, [-8192, 8191], "Pitch Bend", "Bend", "PitchBend", "bend $1"),
        ("DELAY", 0, [0, 127], "LFO Delay", "LFODly", "LFODelay", "lfo_dly $1"),
    ]

    for col, (label, default, prange, longname, shortname, varname, msg) in enumerate(dial_layout):
        px = col * 60
        patch_x = 30.0 + col * 64.0

        lbl = sub.add_comment(
            label,
            patching_rect=[patch_x, 30.0, 80, 18],
            justify="center",
        )
        add_to_presentation(lbl, [px, 0, 56, 14])

        dial = live_param(
            sub,
            maxclass="live.dial",
            longname=longname,
            shortname=shortname,
            ptype=0,
            prange=prange,
            initial=default,
            patching_rect=[patch_x, 60.0, 40, 40],
            pres_rect=[px + 9, 16, 38, 50],
            varname=varname,
        )
        if varname == "PitchBend":
            sub_attr_emit_expr(sub, out_id, dial, "expr $i1 + 8192", msg, cur, sync_recv)
        else:
            sub_attr_emit(sub, out_id, dial, msg, cur, sync_recv)

    sub.rect = Rect(200.0, 200.0, 360.0, 90.0)
    return box


def build_expression_controls_bpatcher(parent: Patcher):
    """Compact fixed CC/XCMD controls rendered as live.numbox widgets."""
    box, sub = embedded_bpatcher(
        parent,
        varname="ExpressionControls",
        patching_rect=[400.0, 500.0, 250.0, 90.0],
        pres_rect=[362.0, 32.0, 250.0, 66.0],
        args=["#0"],
    )
    out_id = add_raw(
        sub,
        maxclass="outlet",
        numinlets=1,
        numoutlets=0,
        outlettype=[],
        patching_rect=[650.0, 30.0, 30.0, 30.0],
        comment="to ccomidi",
    )
    cur = HelperCursor(helper_x=430.0, rsync_x=560.0, y0=30.0)
    sync_recv = "r #1-sync"

    controls = [
        ("Echo Len", "Echo Length", "EchoLen", [0, 32], 0, "EchoLength", "iecl $1", None),
        ("Echo Vol", "Echo Volume", "EchoVol", [0, 32], 0, "EchoVolume", "iecv $1", None),
        ("Bend Rng", "Bend Range", "BndRng", [0, 32], 0, "BendRange", "bend_range $1", None),
        ("Mod Type", "Mod Type", "ModTyp", [0, 2], 0, "ModType", "mod_type $1", None),
        ("Tune", "Tune", "Tune", [-64, 63], 0, "Tune", "tune $1", "expr $i1 + 64"),
    ]

    for col, (label, longname, shortname, prange, initial, varname, msg, expr_text) in enumerate(controls):
        px = col * 50
        patch_x = 30.0 + col * 74.0
        lbl = sub.add_comment(
            label,
            patching_rect=[patch_x, 30.0, 70, 18],
            justify="center",
        )
        add_to_presentation(lbl, [px, 2, 48, 14])
        num = live_param(
            sub,
            maxclass="live.numbox",
            longname=longname,
            shortname=shortname,
            ptype=0,
            prange=prange,
            initial=initial,
            patching_rect=[patch_x, 60.0, 55, 22],
            pres_rect=[px + 3, 22, 44, 22],
            varname=varname,
        )
        if expr_text:
            y = cur.step()
            expr = add_raw(
                sub,
                maxclass="newobj",
                text=expr_text,
                numinlets=1,
                numoutlets=1,
                outlettype=["int"],
                patching_rect=[cur.helper_x, y, 90, 22],
            )
            msg_box = sub.add_message(
                msg,
                patching_rect=[cur.helper_x + 100, y, _msg_width(msg), 22],
            )
            sub.add_line(num, expr)
            sub.add_line(expr, msg_box)
            sub.add_line(msg_box, out_id)
            rsync = sub.add_textbox(
                sync_recv,
                patching_rect=[cur.rsync_x, y, 80, 22],
                numinlets=0,
                numoutlets=1,
                outlettype=["bang"],
            )
            sub.add_line(rsync, num)
        else:
            sub_attr_emit(sub, out_id, num, msg, cur, sync_recv)

    sub.rect = Rect(200.0, 200.0, 250.0, 90.0)
    return box


def build():
    p = Patcher(
        path=str(OUT_PATH),
        device_type="midi_effect",
        openinpresentation=1,
    )
    p.enable_presentation(devicewidth=860)
    p.rect = Rect(100.0, 100.0, 1160.0, 900.0)
    p.description = "ccomidi - Max for Live MIDI effect (m4a CC sender)."

    BOOT_X = 30.0
    OPTIONS_X = 720.0

    boot_label = p.add_comment(
        "Boot / transport / MIDI I/O",
        patching_rect=[BOOT_X, 20.0, 220.0, 18.0],
    )

    thisdevice = p.add_textbox(
        "live.thisdevice",
        patching_rect=[BOOT_X, 50.0, 120.0, 22.0],
    )

    sync_delay = p.add_textbox(
        "delay 3000",
        patching_rect=[BOOT_X, 90.0, 80, 22],
    )
    restore_trig = p.add_textbox(
        "t b b b",
        patching_rect=[BOOT_X + 95.0, 90.0, 70, 22],
        numoutlets=3,
        outlettype=["bang", "bang", "bang"],
    )
    sync_send = add_raw(
        p,
        maxclass="newobj",
        text="s #0-sync",
        numinlets=1,
        numoutlets=0,
        outlettype=[],
        patching_rect=[BOOT_X + 220.0, 90.0, 80, 22],
    )
    restore_on = p.add_message(
        "restore 1",
        patching_rect=[BOOT_X + 95.0, 120.0, 80, 22],
    )
    restore_off = p.add_message(
        "restore 0",
        patching_rect=[BOOT_X + 185.0, 120.0, 80, 22],
    )

    midiin = p.add_textbox(
        "midiin",
        patching_rect=[BOOT_X + 360.0, 50.0, 50, 22],
    )
    ccomidi = add_raw(
        p,
        maxclass="newobj",
        text="ccomidi",
        numinlets=1,
        numoutlets=1,
        outlettype=["int"],
        patching_rect=[BOOT_X + 360.0, 90.0, 80, 22],
    )
    midiout = p.add_textbox(
        "midiout",
        patching_rect=[BOOT_X + 360.0, 130.0, 50, 22],
    )
    p.add_line(midiin, ccomidi)
    p.add_line(ccomidi, midiout)

    p.add_line(thisdevice, sync_delay, outlet=0)
    p.add_line(sync_delay, restore_trig)
    p.add_line(restore_trig, restore_on, outlet=2)
    p.add_line(restore_on, ccomidi)
    p.add_line(restore_trig, sync_send, outlet=1)
    p.add_line(restore_trig, restore_off, outlet=0)
    p.add_line(restore_off, ccomidi)

    tp_path_msg = p.add_message(
        "path live_set",
        patching_rect=[BOOT_X, 185.0, 110, 22],
    )
    tp_path = p.add_textbox(
        "live.path",
        patching_rect=[BOOT_X, 220.0, 90, 22],
    )
    tp_obs = p.add_textbox(
        "live.observer is_playing",
        patching_rect=[BOOT_X, 255.0, 160, 22],
    )
    tp_chg = p.add_textbox(
        "change",
        patching_rect=[BOOT_X, 290.0, 60, 22],
    )
    tp_prep = p.add_textbox(
        "prepend transport",
        patching_rect=[BOOT_X + 75.0, 290.0, 130, 22],
    )
    p.add_line(thisdevice, tp_path_msg, outlet=0)
    p.add_line(tp_path_msg, tp_path)
    p.add_line(tp_path, tp_obs)
    p.add_line(tp_obs, tp_chg)
    p.add_line(tp_chg, tp_prep)
    p.add_line(tp_prep, ccomidi)

    main_controls = build_main_controls_bpatcher(p)
    p.add_line(main_controls, ccomidi)

    expression_controls = build_expression_controls_bpatcher(p)
    p.add_line(expression_controls, ccomidi)

    voice_v8 = build_voice_picker_section(p, ccomidi, thisdevice)

    voice_start_msg = p.add_message(
        "start",
        patching_rect=[OPTIONS_X, 150.0, 60, 22],
    )
    p.add_line(thisdevice, voice_start_msg, outlet=0)
    p.add_line(voice_start_msg, voice_v8)

    voice_retry_1 = p.add_textbox(
        "delay 1000",
        patching_rect=[OPTIONS_X + 70.0, 150.0, 80, 22],
    )
    voice_retry_2 = p.add_textbox(
        "delay 3000",
        patching_rect=[OPTIONS_X + 160.0, 150.0, 80, 22],
    )
    voice_retry_msg = p.add_message(
        "reload",
        patching_rect=[OPTIONS_X + 250.0, 150.0, 60, 22],
    )
    p.add_line(thisdevice, voice_retry_1, outlet=0)
    p.add_line(thisdevice, voice_retry_2, outlet=0)
    p.add_line(voice_retry_1, voice_retry_msg)
    p.add_line(voice_retry_2, voice_retry_msg)
    p.add_line(voice_retry_msg, voice_v8)

    freebang = p.add_textbox(
        "freebang",
        patching_rect=[OPTIONS_X, 215.0, 70, 22],
    )
    free_msg = p.add_message(
        "free",
        patching_rect=[OPTIONS_X + 80.0, 215.0, 50, 22],
    )
    p.add_line(freebang, free_msg)
    p.add_line(free_msg, voice_v8)

    sendall_recv = p.add_textbox(
        "r ccomidi.sendall",
        patching_rect=[OPTIONS_X, 305.0, 200, 22],
    )
    sendall_trigger_msg = p.add_message(
        "sendall",
        patching_rect=[OPTIONS_X, 335.0, 80, 22],
    )
    p.add_line(sendall_recv, sendall_trigger_msg)
    p.add_line(sendall_trigger_msg, ccomidi)

    write_amxd_factory(p, OUT_PATH, device_type="midi_effect")
    install_amxd_into_live_library(OUT_PATH, device_type="midi_effect")

    boxes = len(p.boxes) if hasattr(p, "boxes") else "?"
    lines = len(p.lines) if hasattr(p, "lines") else "?"
    print(f"wrote {OUT_PATH} ({boxes} boxes, {lines} lines)")


if __name__ == "__main__":
    build()
