# CCoMIDI Drumset Pads Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a `Drumset` tab to `ccomidi.clap` that renders 6-column drum pads for the selected `VOICE_KEYSPLIT_ALL` voice and emits the pad MIDI notes.

**Architecture:** `poryaaaa` remains the voicegroup source of truth and writes drumset metadata into the existing shared `projects.json`. `ccomidi.clap` reads that metadata, renders pads only when the selected slot has `drumset`, and queues Note On/Off events from the UI into the normal CLAP output path.

**Tech Stack:** C/C++17, CLAP, ImGui, existing `projects.json` state bridge.

---

## Agreed Decisions

- Use the existing shared `projects.json`; `poryaaaa.json` means the same file in conversation, not a new file.
- `poryaaaa` writes `typeCode` for every slot.
- `poryaaaa` writes `drumset` only on `VOICE_KEYSPLIT_ALL` slots.
- `drumset` items contain only `{ "note": number, "name": string }`.
- `ccomidi.clap` never guesses drumsets from names and does not parse voicegroup source.
- The `Drumset` tab label is always visible; its body renders nothing unless the selected slot has `drumset`.
- Pads are text-only for now, 6 per row.
- Pad display strips only the `DirectSoundWaveData_` prefix.
- Press sends Note On, release sends Note Off, using the current ccomidi output channel.
- Multiple pads may be held at once.
- Changing selected program flushes held notes.
- No automation, no parameter changes, no velocity UI, no pad layout persistence, no icons/colors/decorations yet.

## Domain Gate

Before starting each domain below, stop and ask the user for approval. Do not start the next domain because the previous one was approved.

## Files

- Modify: `packages/poryaaaa/plugin/voicegroup/voicegroup_state.c`
- Modify if needed: `packages/poryaaaa/plugin/voicegroup/voicegroup_loader.h`
- Test: `packages/poryaaaa/test/test_voicegroup_loader.c` or nearest existing poryaaaa voicegroup/state test
- Modify: `packages/ccomidi/src/plugin/voicegroup_bridge.h`
- Modify: `packages/ccomidi/src/plugin/voicegroup_bridge.cpp`
- Test: add or extend nearest `packages/ccomidi/src/tests/*` bridge/core test
- Modify: `packages/ccomidi/src/plugin/ccomidi_plugin_shared.h`
- Modify: `packages/ccomidi/src/plugin/ccomidi_plugin.cpp`
- Modify: `packages/ccomidi/src/gui/ccomidi_editor.cpp`
- Test: `packages/ccomidi/src/tests/test_sender_core.cpp` or focused plugin/core test if exposed

---

### Task 1: poryaaaa `projects.json` Writer

**Approval required before starting:** Ask: `Approve starting the poryaaaa projects.json writer domain?`

- [x] Write a failing test that loads/writes a voicegroup containing a `VOICE_KEYSPLIT_ALL` slot and asserts JSON contains:
  - `"typeCode": 128` on that slot
  - `"drumset"` only on that slot
  - drum pads sorted by ascending note
  - pad entries with only `note` and `name`

- [x] Run the focused poryaaaa test and confirm it fails for missing `typeCode`/`drumset`.

- [x] Update `voicegroup_state_write_default(...)` to write `typeCode` for each emitted slot.

- [x] For slots where `vg->voices[i].type & VOICE_KEYSPLIT_ALL`, walk `((ToneData*)vg->voices[i].subGroup)[0..127]` and emit non-empty sample names as `drumset` entries.

- [x] Keep the writer atomic: temp file, close, rename. Do not append raw JSON.

- [x] Run focused poryaaaa test, then the package-local poryaaaa voicegroup/unit test target in Release.

### Task 2: ccomidi Bridge Reads Drumset Metadata

**Approval required before starting:** Ask: `Approve starting the ccomidi projects.json bridge domain?`

- [ ] Write a failing bridge test with a minimal `projects.json` body containing a normal slot and a drumset slot.

- [ ] Extend `VoiceSlot` in `voicegroup_bridge.h`:

```cpp
struct DrumPad
{
    int note = 0;
    std::string name;
};

struct VoiceSlot
{
    int program = 0;
    int typeCode = 0;
    std::string name;
    std::vector<DrumPad> drumset;
};
```

- [ ] Parse optional `typeCode`; default to `0` for older `projects.json`.

- [ ] Parse optional `drumset`; ignore malformed pad entries rather than failing the whole voice list.

- [ ] Run the focused ccomidi bridge/core test.

### Task 3: ccomidi Drumset Tab and MIDI Queue

**Approval required before starting:** Ask: `Approve starting the ccomidi Drumset UI/output domain?`

- [ ] Write or extend a focused core/plugin test for queued UI note events:
  - queue Note On for note 36
  - process emits `0x90 | outputChannel, 36, 100`
  - queue Note Off for note 36
  - process emits `0x80 | outputChannel, 36, 0`

- [ ] Add minimal pending UI note storage to `Plugin`, using `std::array<bool, 128>` for held notes and a small pending MIDI vector.

- [ ] In `plugin_process`, drain queued UI note events before normal input processing and emit them through `push_midi_event(...)`.

- [ ] In `ccomidi_editor.cpp`, add `BeginTabBar` / `BeginTabItem("Drumset")`.

- [ ] In the `Drumset` tab, find the selected `VoiceSlot` by current program. If `slot.drumset.empty()`, render nothing.

- [ ] Render pads in 6 columns. Label each pad with note number and sample name after removing only `DirectSoundWaveData_`.

- [ ] On pad press, queue Note On if not already held. On release, queue Note Off if held.

- [ ] When selected program changes, flush all held notes.

- [ ] Run focused ccomidi tests and Release build only.

## Verification

- `packages/poryaaaa`: run the focused voicegroup/state test and the nearest Release unit target.
- `packages/ccomidi`: run focused tests and Release build only.
- Do not run broad monorepo builds unless a domain change crosses package boundaries unexpectedly.
