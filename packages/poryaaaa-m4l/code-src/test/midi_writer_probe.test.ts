// §7.2 probe: confirm midi-writer-js preserves insertion order for events
// added back-to-back with the same delta on the same channel. This is the
// invariant our same-tick GBA 0x1D/0x1E CC pairs depend on.
//
// Strategy: build a track with two CCs at tick 0 (delta=0 on both), write
// the SMF, parse it back, assert the controller numbers come out in the
// order they were added.

import test from "node:test";
import assert from "node:assert/strict";

import MidiWriter from "midi-writer-js";

test("midi-writer-js: same-tick CCs preserve insertion order", () => {
    const track = new MidiWriter.Track();

    // Two CCs at the same tick, GBA-style pair: 0x1E (prefix) then 0x1D (value).
    track.addEvent(new MidiWriter.ControllerChangeEvent({
        controllerNumber: 0x1E,
        controllerValue: 0x42,
        channel: 1,
    }));
    track.addEvent(new MidiWriter.ControllerChangeEvent({
        controllerNumber: 0x1D,
        controllerValue: 0x55,
        channel: 1,
    }));

    const writer = new MidiWriter.Writer([track]);
    const bytes = writer.buildFile();

    // Walk the SMF byte stream and pull CC controller numbers in order.
    // Format: MThd ... MTrk <length> <delta varint><status><cn><cv> ...
    // We don't need a full parser — just scan for 0xB0 status bytes.
    const ccOrder: number[] = [];
    for (let i = 0; i < bytes.length - 2; i++) {
        // CC status nibble is 0xB; channel is the low nibble.
        if ((bytes[i] & 0xF0) === 0xB0) {
            // Sanity-check that bytes[i+1] is a valid 7-bit controller number.
            if (bytes[i + 1] < 0x80) {
                ccOrder.push(bytes[i + 1]);
            }
        }
    }

    assert.deepEqual(
        ccOrder,
        [0x1E, 0x1D],
        "CCs must appear in the order they were added (0x1E before 0x1D)",
    );
});

test("midi-writer-js: same-tick CC+PC preserves insertion order", () => {
    const track = new MidiWriter.Track();

    // PC first, then CC (the order recorder_smf_writer will use for tick-0
    // initial state).
    track.addEvent(new MidiWriter.ProgramChangeEvent({
        instrument: 5,
        channel: 1,
    }));
    track.addEvent(new MidiWriter.ControllerChangeEvent({
        controllerNumber: 7,        // Volume
        controllerValue: 100,
        channel: 1,
    }));

    const writer = new MidiWriter.Writer([track]);
    const bytes = writer.buildFile();

    // Scan for status bytes in event-data order. PC = 0xC0, CC = 0xB0.
    const order: string[] = [];
    for (let i = 0; i < bytes.length - 1; i++) {
        if ((bytes[i] & 0xF0) === 0xC0 && bytes[i + 1] < 0x80) {
            order.push("PC");
        } else if ((bytes[i] & 0xF0) === 0xB0 && bytes[i + 1] < 0x80) {
            order.push("CC");
        }
    }

    assert.deepEqual(order, ["PC", "CC"], "PC must precede CC at tick 0");
});
