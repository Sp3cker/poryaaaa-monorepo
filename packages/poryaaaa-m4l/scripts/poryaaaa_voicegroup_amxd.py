"""Voicegroup/project patcher wiring for gen_poryaaaa_amxd.py."""

from typing import NamedTuple

from _amxd_helpers import node_for_max_bin_attrs
from py2max.m4l import add_to_presentation


NODE_FOR_MAX_BIN_ATTRS = node_for_max_bin_attrs()


class NodeBootChain(NamedTuple):
    node_start_msg: object
    restore_msg: object


class VoicegroupControls(NamedTuple):
    voicegroup_prepend: object
    scanner: object


def add_node_boot_chain(p, *, thisdevice, boot_x) -> NodeBootChain:
    node_start_loadbang = p.add_textbox(
        "loadbang",
        patching_rect=[boot_x, 125.0, 80.0, 22.0],
    )
    node_start_msg = p.add_message(
        "script start",
        patching_rect=[boot_x + 90.0, 125.0, 110.0, 22.0],
    )
    p.add_line(node_start_loadbang, node_start_msg)

    restore_delay = p.add_textbox(
        "delay 500",
        patching_rect=[boot_x, 165.0, 90.0, 22.0],
    )
    restore_msg = p.add_message(
        "restore",
        patching_rect=[boot_x, 205.0, 130.0, 22.0],
    )
    p.add_line(thisdevice, restore_delay, outlet=0)
    p.add_line(restore_delay, restore_msg)
    return NodeBootChain(node_start_msg, restore_msg)


def add_voicegroup_controls(
    p,
    *,
    boot_x,
    main_x,
    thisdevice,
    node_start_msg,
    restore_msg,
    add_raw,
    live_text_button,
) -> VoicegroupControls:
    # Console echo of the Live-ready bang.
    boot_print = p.add_textbox(
        "print porya_boot",
        patching_rect=[boot_x + 200.0, 50.0, 130.0, 22.0],
    )
    p.add_line(thisdevice, boot_print, outlet=0)

    # "Dump State" posts Node's current in-memory/project JSON state.
    dump_btn = live_text_button(
        p, "Dump State",
        patching_rect=[boot_x + 130.0, 85.0, 90.0, 22.0],
        varname="DumpStateBtn",
        pres_rect=[140, 6, 80, 18],
    )
    dump_msg = p.add_message(
        "dumpstate",
        patching_rect=[boot_x + 130.0, 125.0, 90.0, 22.0],
    )
    p.add_line(dump_btn, dump_msg)

    ws_label = p.add_comment(
        "WS",
        patching_rect=[boot_x + 240.0, 85.0, 30.0, 18.0],
    )
    add_to_presentation(ws_label, [232, 8, 20, 14])

    ws_status = add_raw(
        p,
        maxclass="comment",
        text="off",
        numinlets=1,
        numoutlets=0,
        outlettype=[],
        patching_rect=[boot_x + 270.0, 85.0, 45.0, 18.0],
    )
    add_to_presentation(ws_status, [254, 8, 34, 14])

    ws_poll_delay = p.add_textbox(
        "delay 700",
        patching_rect=[boot_x + 240.0, 125.0, 80.0, 22.0],
    )
    ws_poll_start = p.add_message(
        "1",
        patching_rect=[boot_x + 240.0, 165.0, 35.0, 22.0],
    )
    ws_poll_metro = p.add_textbox(
        "metro 5000",
        patching_rect=[boot_x + 240.0, 205.0, 90.0, 22.0],
    )
    ws_poll_trigger = p.add_textbox(
        "t b b",
        patching_rect=[boot_x + 240.0, 245.0, 60.0, 22.0],
        numoutlets=2,
        outlettype=["bang", "bang"],
    )
    ws_off_msg = p.add_message(
        "set off",
        patching_rect=[boot_x + 310.0, 285.0, 60.0, 22.0],
    )
    ws_status_msg = p.add_message(
        "wsstatus",
        patching_rect=[boot_x + 240.0, 285.0, 80.0, 22.0],
    )
    p.add_line(thisdevice, ws_poll_delay, outlet=0)
    p.add_line(ws_poll_delay, ws_poll_start)
    p.add_line(ws_poll_start, ws_poll_metro)
    p.add_line(ws_poll_delay, ws_poll_trigger)
    p.add_line(ws_poll_metro, ws_poll_trigger)
    p.add_line(ws_poll_trigger, ws_status_msg, outlet=0)
    p.add_line(ws_poll_trigger, ws_off_msg, outlet=1)
    p.add_line(ws_off_msg, ws_status)

    # ============ Project chooser + Node routing =======================
    p.add_comment(
        "Project / voicegroup",
        patching_rect=[main_x, 20.0, 180.0, 18.0],
    )

    choose_btn = live_text_button(
        p, "Choose project…",
        patching_rect=[main_x, 50.0, 120.0, 22.0],
        varname="ChooseBtn",
        pres_rect=[8, 6, 120, 18],
    )

    opendlg = p.add_textbox(
        "opendialog folder",
        patching_rect=[main_x, 85.0, 140.0, 22.0],
    )
    p.add_line(choose_btn, opendlg)

    # opendialog emits HFS-style paths on macOS ("Macintosh HD:/Users/...").
    # Node normalizes/scans/persists the root and emits the final voicegroup.
    prepend_rawroot = p.add_textbox(
        "prepend rawroot",
        patching_rect=[main_x, 125.0, 110.0, 22.0],
    )
    p.add_line(opendlg, prepend_rawroot)

    path_label = add_raw(
        p,
        maxclass="comment",
        text="(no project)",
        numinlets=1,
        numoutlets=0,
        outlettype=[],
        patching_rect=[main_x, 165.0, 320.0, 18.0],
    )
    add_to_presentation(path_label, [8, 30, 260, 18])

    # Node owns voicegroup persistence, scanning, parser validation, and the
    # ccomidi WebSocket snapshot server.
    scanner = p.add_textbox(
        f"node.script poryaaaa_voicegroup_server.js @autostart 1 {NODE_FOR_MAX_BIN_ATTRS}",
        patching_rect=[main_x, 205.0, 720.0, 22.0],
        numoutlets=1,
        outlettype=[""],
    )
    p.add_line(prepend_rawroot, scanner)
    p.add_line(node_start_msg, scanner)
    p.add_line(restore_msg, scanner)
    p.add_line(dump_msg, scanner)
    p.add_line(ws_status_msg, scanner)

    # node.script has one outlet. Route-tagged messages fan out here:
    #   ready      -> delayed restore now that Node has registered handlers
    #   bank       -> bank umenu / setsymbol
    #   path       -> path label (comment "set ...")
    #   voicegroup -> [prepend voicegroup] -> poryaaaa~
    #   wsstatus   -> websocket status comment (comment "set on/off")
    node_route = p.add_textbox(
        "route ready bank path voicegroup wsstatus",
        patching_rect=[main_x, 245.0, 190.0, 22.0],
        numoutlets=6,
        outlettype=["", "", "", "", "", ""],
    )
    p.add_line(scanner, node_route)
    p.add_line(node_route, restore_msg, outlet=0)

    bank_menu = p.add_umenu(
        items=["(no project loaded)"],
        patching_rect=[main_x, 285.0, 240.0, 22.0],
    )
    add_to_presentation(bank_menu, [8, 56, 260, 18])
    p.add_line(node_route, bank_menu, outlet=1)
    p.add_line(node_route, path_label, outlet=2)
    p.add_line(node_route, ws_status, outlet=4)

    voicegroup_prepend = p.add_textbox(
        "prepend voicegroup",
        patching_rect=[main_x + 210.0, 245.0, 140.0, 22.0],
    )
    p.add_line(node_route, voicegroup_prepend, outlet=3)

    bank_tosym = p.add_textbox(
        "tosymbol",
        patching_rect=[main_x, 325.0, 90.0, 22.0],
    )
    p.add_line(bank_menu, bank_tosym, outlet=1)

    bank_select = p.add_textbox(
        "prepend bankselect",
        patching_rect=[main_x, 365.0, 140.0, 22.0],
    )
    p.add_line(bank_tosym, bank_select)
    p.add_line(bank_select, scanner)

    return VoicegroupControls(voicegroup_prepend, scanner)
