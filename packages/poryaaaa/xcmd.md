# v2 XCMD CC Port — Draft for Review

**Status:** implemented pending expert review. **Audience:** GBA audio expert.

The plan below was implemented on branch `loader-refactor` after a first
internal review. Three deviations from a verbatim v1 port were directed:

1. **xWAVE (0x01) is notify-only.** The 32-bit payload is a ROM address
   from the original game, not a host pointer; casting it would dangle.
   `currentVoice.wav` is left untouched and the callback fires with the
   assembled u32 so a future address→loaded-sample resolver can map it.
2. **xCmd 0x0D stores into `extendedValue` and notifies** (matches v1).
   Even without a known consumer in v2, preserving the value is cheap
   parity insurance.
3. **xCmd 0x0C (xWAIT) is intentionally not implemented.** v2 has no
   song-script interpreter in the MIDI ingress path that could honour
   the wait/loop semantic, so 0x0C stays as an unknown selector
   (`xcmd_data_length` returns 0): payload bytes are dropped silently
   and the callback never fires.  No `extendedLoopCount` /
   `extendedWait` state on `M4ADriverTrack`.

The two-CC protocol is order-dependent and the selector is sticky after
apply (only the byte count resets) — verified by tests below.

Files actually touched:
- `plugin/m4a/m4a_internal.h` — 6 new fields on `M4ADriverTrack`.
- `plugin/m4a/m4a_track.c` — 4 new file-local statics
  (`xcmd_data_length`, `xcmd_read_le`, `xcmd_notify`, `xcmd_apply`) plus
  CC `0x1D`, `0x1E`, `0x1F` cases inside `m4a_cc`.
- `test/test_engine.c` — 4 new v2 tests
  (`test_v2_xcmd_mutates_track_state`,
  `test_v2_xcmd_propagates_to_new_notes`,
  `test_v2_xcmd_protocol_safety`,
  `test_v2_xcmd_render_changes_audio`) covering field mutation,
  propagation into new PCM + CGB notes, multi-byte LE assembly, partial
  / unknown-selector safety, sticky-selector behaviour, and a
  render-level proof that `xSUST` changes ring-output amplitude.
- All 324/324 unit tests pass on `cmake --build build --target poryaaaa_unit_tests`
  with `M4A_DRIVER_V2=ON HW_AUDIO_V2=ON`.

## Problem

`poryaaaa` is in the middle of a v1 → v2 driver refactor (branch `loader-refactor`).
The v1 engine (`plugin/m4a_engine.c`) implements the canonical MP2K xCmd surface.
The v2 driver (`plugin/m4a/`) does **not** apply any xCmd. The CC ingress
(`m4a_cc` at `plugin/m4a/m4a_track.c:413-499`) handles only the standard MIDI
CCs (mod, vol, pan, BENDR, LFOS, MODT, TUNE, LFODL, all-notes/sound-off) and
silently drops `0x1D` / `0x1E` / `0x1F`.

A hearth-format song that uses `XATTA`/`XRELE`/etc. plays correctly on v1
and on real hardware (per pokemon-hearth `src/m4a.c:1526` dispatch table)
but is silently identical to "stock voice" on v2. This is a parity gap.

## Reference: what v1 and pokemon-hearth do

### Selector table (verified against pokemon-hearth `src/m4a_tables.c:291` + `src/m4a.c:1526`)

| Sel  | Name  | Bytes | hearth target          | v1 target (`plugin/m4a_engine.c:359-424`) |
|------|-------|-------|------------------------|-------------------------------------------|
| 0x01 | xWAVE | 4 LE  | `tone.wav`             | `currentVoice.wav` (ptr cast from u32)    |
| 0x02 | xTYPE | 1     | `tone.type`            | `currentVoice.type`                        |
| 0x04 | xATTA | 1     | `tone.attack`          | `currentVoice.attack`                      |
| 0x05 | xDECA | 1     | `tone.decay`           | `currentVoice.decay`                       |
| 0x06 | xSUST | 1     | `tone.sustain`         | `currentVoice.sustain`                     |
| 0x07 | xRELE | 1     | `tone.release`         | `currentVoice.release`                     |
| 0x08 | xIECV | 1     | `track->pseudoEchoVolume` | `track->pseudoEchoVolume`               |
| 0x09 | xIECL | 1     | `track->pseudoEchoLength` | `track->pseudoEchoLength`               |
| 0x0A | xLENG | 1     | `tone.length`          | `currentVoice.length`                      |
| 0x0B | xSWEE | 1     | `tone.pan_sweep`       | `currentVoice.panSweep`                    |
| 0x0C | xWAIT | 2 LE  | timing (loop/wait)     | tracks `extendedLoopCount`/`extendedWait` (no playback effect — v1 has no song interpreter) |
| 0x0D | —     | 4 LE  | `track->unk_3C`        | `track->extendedValue`                     |

### CC-side state machine (v1, `plugin/m4a_engine.c:779-796`)

Two-CC protocol, matches `mid2agb`:

```
CC 0x1E  → track->extendedCommand = value;
           track->extendedCommandCount = 0;
           memset(track->extendedCommandBytes, 0, 4);

CC 0x1D  → if (xcmd_data_length(track->extendedCommand) == 0) break;
or 0x1F    track->extendedCommandBytes[count++] = value;
           if (count >= dataLength) xcmd_apply(...);
```

`xcmd_apply` then dispatches on `extendedCommand`, mutates the per-track
target, fires `engine_notify_xcmd(engine, trackIndex, selector, value)`,
and resets `extendedCommandCount = 0`.

`xcmd_data_length` (`plugin/m4a_engine.c:326-347`): returns 4 for 0x01/0x0D,
2 for 0x0C, 1 for 0x02 + 0x04..0x0B, 0 otherwise.

`xcmd_read_le` (`plugin/m4a_engine.c:349-357`): plain little-endian assembly
across 1..4 bytes.

## Proposed v2 port

### Track state additions (`plugin/m4a/m4a_internal.h` `M4ADriverTrack`)

`pseudoEchoVolume` / `pseudoEchoLength` already exist (lines 87-88) and are
already read by `m4a_pcm.c:75-92` and `m4a_cgb.c:313-345` — wiring xIECV/xIECL
is just "actually write to fields the envelope path is already reading".

New fields:

```c
uint8_t  extendedCommand;      /* last 0x1E selector, 0 = idle */
uint8_t  extendedCommandCount; /* bytes accumulated so far */
uint8_t  extendedCommandBytes[4];
uint32_t extendedValue;        /* xCmd 0x0D payload (notify-only) */
uint16_t extendedLoopCount;    /* xCmd 0x0C state, no playback effect */
uint8_t  extendedWait;         /* xCmd 0x0C state, no playback effect */
```

These mirror v1's `M4ATrack` field set 1:1, so disasm-comparison stays clean.

### Static helpers (in `plugin/m4a/m4a_track.c`, file-local)

Direct copies of v1's `xcmd_data_length` and `xcmd_read_le` — no behavior
change. `xcmd_apply` is rewritten to use v2's `M4ADriverTrack` field names
(`currentVoice.panSweep` not `tone.pan_sweep` — v2 uses the existing
`ToneData` struct from `voicegroup/voicegroup_types.h`).

### `m4a_cc` additions (`plugin/m4a/m4a_track.c:413-499`)

Insert before the `default:` label:

```c
case 0x1D:
case 0x1F: {
    uint8_t dataLength = xcmd_data_length(t->extendedCommand);
    if (dataLength == 0) break;
    t->extendedCommandBytes[t->extendedCommandCount++] = value;
    if (t->extendedCommandCount >= dataLength)
        xcmd_apply(drv, track, t);
    break;
}
case 0x1E:
    t->extendedCommand = value;
    t->extendedCommandCount = 0;
    memset(t->extendedCommandBytes, 0, sizeof(t->extendedCommandBytes));
    break;
```

### Outbound notification

`drv->xcmd_fn` is set via `m4a_driver_set_xcmd_callback` (`m4a_driver.c:82-87`)
but never called. `xcmd_apply` calls it on every successful dispatch, exact
same signature as v1: `(ctx, trackIndex, selector, value)`.

## Open questions for the expert

Items 1, 2, 3, 4, and 6 are now decided (resolution noted under each).
Item 5 is the only open one and is more of an FYI than a blocker.

### 1. xWAVE (0x01) pointer semantics — RESOLVED (notify-only)

v1 stores the 4-byte LE payload into `currentVoice.wav` cast to `WaveData *`
(`m4a_engine.c:363`). On real GBA this is a ROM address; in poryaaaa we're a
plugin running off a host-side voicegroup table, not ROM. Three options:

- **A.** Notify-only — leave `currentVoice.wav` alone, just fire the callback.
  Plugin host can map the address to its own WaveData if it cares.
- **B.** Mirror v1 verbatim — store the raw u32 as a pointer. Will dangle
  unless the host reinterprets it. (Current v1 behavior.)
- **C.** Look up the u32 as an index into the loaded voicegroup. Probably wrong
  — mid2agb emits real ROM addresses, not voicegroup indices.

**Resolution:** option **A** (notify-only). `currentVoice.wav` is not
modified; the callback receives the assembled u32 LE payload so plugin
code can resolve it later if/when a host-side sample table arrives.

### 2. xCmd 0x0C (xWAIT) — RESOLVED (drop)

**Resolution:** option **B**. v2 has no song-script interpreter on the
MIDI ingress path that could honour the wait/loop semantic, so 0x0C is
not in `xcmd_data_length`'s table and any payload bytes are silently
dropped. No `extendedLoopCount` / `extendedWait` fields on
`M4ADriverTrack`. Verified by `test_v2_xcmd_mutates_track_state`: a
`CC 0x1E = 0x0C` followed by two payload bytes does not fire the
callback and does not mutate any track state.

### 3. xCmd 0x0D (unnamed) — RESOLVED (store + notify)

**Resolution:** matches v1. The 4-byte LE payload is stored into
`track->extendedValue` and forwarded via the callback. Even without a
known consumer in v2, preserving the value is cheap parity insurance and
keeps the door open for whichever hearth-side use eventually surfaces.
Open meta-question for the expert: **what does 0x0D actually do in the
original engine? `unk_3C` is undocumented in pokemon-hearth.**

### 4. Active-channel refresh on voice-mutating xCmds — RESOLVED (no refresh)

**Resolution:** xATTA / xDECA / xSUST / xRELE / xLENG / xSWEE / xTYPE
mutate `currentVoice.*` only. They affect *future* note-ons; already-
sounding channels keep their per-note ADSR snapshot taken at note-on
time (`m4a_note_on()` copies `voice->{attack,decay,sustain,release}`
into `ch->*` once and the channel owns its envelope thereafter). This
matches v1 and matches hearth/MP2K (which mutates `track->tone.attack`,
not active channel ADSR). Refreshing active channels would be a new
semantic — mid-note envelope-parameter mutation — and would be ill-
defined for a channel already in DECAY/SUSTAIN/RELEASE.

The exceptions are *not* the voice-definition xCmds. The track-runtime
controls that already refresh active channels in v2 are normal MIDI CCs:
volume (CC7), pan (CC10), pitch bend, BENDR, LFOS, MODT, TUNE, LFODL —
those are runtime track state and belong on every active channel
immediately, which `m4a_cc` already handles via
`refresh_cgb_volumes/pitches` + `refresh_pcm_volumes/pitches`.

### 5. `m4a_driver_refresh_voices` clobber risk

`m4a_driver.c:94-102` re-copies each track's voicegroup entry over
`currentVoice` whenever the voicegroup is edited. Any xCmd mutation to
`currentVoice.{attack,decay,...}` will be wiped. v1 has the same risk in
`m4a_engine_refresh_voices`. Acceptable, or do we need an "xCmd overrides"
shadow that survives refresh?

### 6. Selector reset / persistence across notes — RESOLVED (sticky)

**Resolution:** sticky, matching v1 + hearth. After a successful
`xcmd_apply`, only `extendedCommandCount` resets; `extendedCommand`
stays latched until the next `CC 0x1E` arrives. Verified by
`test_v2_xcmd_protocol_safety`: a payload byte sent after a complete
xATTA dispatch (no fresh `0x1E`) drives a second xATTA apply, and the
callback's `selector` field still reads `0x04`.

## Test plan (pre-merge)

1. **Unit:** new test that drives `m4a_cc` with `0x1E,0x04` → `0x1D,0x7A` and
   asserts `track->currentVoice.attack == 0x7A`. One per selector.
2. **Notify:** verify `drv->xcmd_fn` fires with `(track, 0x04, 0x7A)`.
3. **Multi-byte:** `0x1E,0x0D` → four `0x1D` bytes → assert
   `track->extendedValue == 0x04030201`.
4. **Partial / malformed:** selector with `dataLength==0` (e.g. 0x03) followed
   by 0x1D should be ignored, not crash.
5. **Sticky selector:** after a complete dispatch, another `0x1D` should start
   a new buffer for the same selector (matches v1).
6. **Audio parity:** capture-compare a hearth song that uses xATTA/xDECA/xRELE
   against v1 with the same song; spectral diff should drop into the
   already-defined parity tolerances after the port. (Re: `feedback_audio_compare_skip.md`,
   skip the first 7s; for `pokeemerald-hearth.ss2` use `--test-skip 0.3`.)

## Files touched (estimate)

- `plugin/m4a/m4a_internal.h` — add 6 fields to `M4ADriverTrack`.
- `plugin/m4a/m4a_track.c` — three new file-local statics + 2 new cases in
  `m4a_cc`. ~80 lines.
- `tests/` — new xcmd test file. ~150 lines.

No changes to `m4a_driver.h` (public surface), `m4a_engine.h` (v1), or any
non-m4a code. The xcmd callback fn pointer + setter already exist on v2.
