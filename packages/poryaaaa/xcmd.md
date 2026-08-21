# XCMD CC Specification and Driver Implementation

**Status:** Implemented and active in `M4ADriver`. **Audience:** GBA audio maintainers & engine consumers.

This document specifies the MP2K extended command (XCMD) MIDI CC protocol as supported by `M4ADriver`.

---

## Overview and Key Decisions

The driver handles extended command CCs (`0x1D`, `0x1E`, `0x1F`) emitted by `mid2agb` and hearth-format songs, mapping them to per-track voice parameters and triggering notification callbacks.

Three key design decisions define the driver implementation:

1. **xWAVE (0x01) is notify-only.** The 32-bit LE payload is forwarded via
   `drv->xcmd_fn`. `currentVoice.wav` is untouched. The payload is **not**
   stored in `extendedValue`.
2. **xWAIT (0x0C) is not accepted.** `xcmd_data_length(0x0C)` returns 0, so
   payload bytes never accumulate or apply. There is no song-script PC to stall.
3. **x0D stores into `extendedValue` and notifies.** The next PCM `m4a_note_on`
   passes that value to `m4a_drv_pcm_start` as a sample start offset. ROM
   `ply_xcmd_0D` writes `unk_3C`; `ply_note` copies it to `SoundChannel.count`.

The two-CC protocol is order-dependent and the selector is sticky after apply (only the byte count resets).

---

## Reference: MP2K and Hearth Protocol

### Selector Table (verified against pokemon-hearth `src/m4a_tables.c` & `src/m4a.c`)

| Sel  | Name  | Bytes | hearth target          | M4ADriver Track Target (`plugin/m4a/m4a_track.c`) |
|------|-------|-------|------------------------|--------------------------------------------------|
| 0x00 | —     | 0     | `ply_xxx` nop          | not accepted (`dataLength == 0`)                 |
| 0x01 | xWAVE | 4 LE  | `tone.wav`             | notify-only (`currentVoice.wav` unchanged)       |
| 0x02 | xTYPE | 1     | `tone.type`            | `track->currentVoice.type`                       |
| 0x03 | —     | 0     | `ply_xxx` nop          | not accepted (`dataLength == 0`)                 |
| 0x04 | xATTA | 1     | `tone.attack`          | `track->currentVoice.attack`                     |
| 0x05 | xDECA | 1     | `tone.decay`           | `track->currentVoice.decay`                      |
| 0x06 | xSUST | 1     | `tone.sustain`         | `track->currentVoice.sustain`                    |
| 0x07 | xRELE | 1     | `tone.release`         | `track->currentVoice.release`                    |
| 0x08 | xIECV | 1     | `track->pseudoEchoVolume` | `track->pseudoEchoVolume`                     |
| 0x09 | xIECL | 1     | `track->pseudoEchoLength` | `track->pseudoEchoLength`                     |
| 0x0A | xLENG | 1     | `tone.length`          | `track->currentVoice.length`                     |
| 0x0B | xSWEE | 1     | `tone.pan_sweep`       | `track->currentVoice.panSweep`                   |
| 0x0C | xWAIT | 2 LE  | song `timer` / `wait`  | not accepted (`dataLength == 0`)                 |
| 0x0D | —     | 4 LE  | `track->unk_3C`        | `track->extendedValue` + next PCM start offset   |

### CC-side State Machine (`plugin/m4a/m4a_track.c`)

Two-CC protocol matching `mid2agb`:

```
CC 0x1E (value = selector)  --> track->extendedCommand = selector
                                 track->extendedCommandCount = 0
                                 memset(track->extendedCommandBytes, 0, 4)

CC 0x1D or 0x1F (value)     --> if (extendedCommand != 0):
                                     bytes[count++] = value
                                     if (count >= data_length(extendedCommand)):
                                         xcmd_apply(drv, track)
```

`xcmd_apply` dispatches on `extendedCommand`, mutates the per-track target, fires `drv->xcmd_fn(drv->xcmd_ctx, trackIndex, selector, value)`, and resets `extendedCommandCount = 0`.

- `xcmd_data_length`: returns 4 for 0x01/0x0D, 1 for 0x02 and 0x04..0x0B, 0 otherwise (including 0x00, 0x03, and 0x0C).
- `xcmd_read_le`: little-endian assembly across 1..4 bytes.

---

## Driver Implementation

### Track State Fields (`plugin/m4a/m4a_internal.h`)

`M4ADriverTrack` includes:

```c
uint8_t extendedCommand;        /* 1E selector, 0 = idle */
uint8_t extendedCommandCount;   /* Bytes accumulated so far */
uint8_t extendedCommandBytes[4];
uint32_t extendedValue;         /* xCmd 0x0D payload; PCM start offset */
uint8_t pseudoEchoVolume;       /* xIECV 0x08 */
uint8_t pseudoEchoLength;       /* xIECL 0x09 */
```

### Ingress (`plugin/m4a/m4a_track.c`)

`m4a_cc()` handles `0x1D`, `0x1E`, and `0x1F`:

```c
case 0x1D:
case 0x1F:
    if (t->extendedCommand != 0) {
        t->extendedCommandBytes[t->extendedCommandCount++] = value;
        if (t->extendedCommandCount >= xcmd_data_length(t->extendedCommand)) {
            xcmd_apply(drv, t);
        }
    }
    break;
case 0x1E:
    t->extendedCommand = value;
    t->extendedCommandCount = 0;
    memset(t->extendedCommandBytes, 0, sizeof(t->extendedCommandBytes));
    break;
```

### Callback Surface

Outbound notifications are registered via `m4a_driver_set_xcmd_callback(drv, fn, ctx)` declared in `plugin/m4a/m4a_driver.h`:

```c
typedef void (*M4ADriverXcmdFn)(void *ctx, int trackIndex, uint8_t selector, uint32_t value);
```

When an XCMD completes apply, `xcmd_apply` calls `drv->xcmd_fn(drv->xcmd_ctx, trackIndex, selector, value)`.

---

## Settled Design Rules

1. **xWAVE (0x01) pointer semantics:** Notify-only. `currentVoice.wav` is left unmodified. The callback receives the u32 LE payload so host code can resolve sample pointers if desired. The payload is not written to `extendedValue`.
2. **xCmd 0x0C (xWAIT):** Not accepted. `xcmd_data_length` returns 0; payload CCs are ignored. A MIDI path has no script PC to stall.
3. **xCmd 0x0D:** Stored into `track->extendedValue` and forwarded via callback. The next DirectSound note-on uses it as the sample start offset (`m4a_track.c` → `m4a_drv_pcm_start`). ROM copies `unk_3C` onto `SoundChannel.count`.
4. **Active-channel ADSR snapshot rules:** xATTA / xDECA / xSUST / xRELE / xLENG / xSWEE / xTYPE mutate `currentVoice.*` only. They affect future note-ons; already-sounding channels retain their per-note ADSR snapshot taken at note-on time (`m4a_driver_note_on()` copies voice params to channel params).
5. **Voicegroup refresh:** Re-copying a voicegroup entry over `currentVoice` will reset xCmd voice mutations on future notes unless re-applied, matching standard GBA m4a behavior.
6. **Selector persistence:** Sticky selector protocol. After a successful `xcmd_apply`, only `extendedCommandCount` resets; `extendedCommand` remains latched for subsequent single-byte sends until a new `0x1E` selector arrives.

---

## Test Verification Contracts

1. `test_v2_xcmd_mutates_track_state`: validates that driving `0x1E, 0x04` then `0x1D, 0x7A` mutates `track->currentVoice.attack` to `0x7A`.
2. `test_v2_xcmd_propagates_to_new_notes`: validates that subsequent note-ons inherit the modified voice attack rate.
3. `test_v2_xcmd_protocol_safety`: validates sticky selector behavior and payload byte framing.
4. `test_v2_xcmd_render_changes_audio`: validates audible output difference after xSUST/xRELE mutation.

---

## Files touched

- `plugin/m4a/m4a_internal.h` — `M4ADriverTrack` extended command state fields.
- `plugin/m4a/m4a_track.c` — `m4a_cc` ingress, helper functions, and `xcmd_apply`.
- `plugin/m4a/m4a_driver.h` — `M4ADriverXcmdFn` callback setter and typedef.
