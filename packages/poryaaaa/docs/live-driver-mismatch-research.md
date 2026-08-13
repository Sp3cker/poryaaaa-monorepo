# Live-driver mismatches vs ROM MP2K

> **Non-shipping research.** Source-backed comparison of poryaaaa's live
> `M4ADriver` to hearth ROM `MPlayMain` / `CgbSound` / `SoundMainRAM` /
> `ply_*`. Isolated hardware replay of ROM traces is already bit-exact and is
> out of scope here. ARM cycle offsets and Phase 5 `WRITE` vs `TIMER` are also
> out of scope.
>
> Date: 2026-08-13. ROM oracle: `/Users/sallegrezza/dev/pokemon-hearth`
> (`src/m4a.c`, `src/m4a_1.s`). Poryaaaa: `packages/poryaaaa/plugin/m4a/`.
>
> Supporting slices: `local://slice-sequence.md`, `local://slice-cgb.md`,
> `local://slice-pcm.md`, `local://slice-effects.md`,
> `local://slice-midi-xcmd.md`.

## Present facts

- Hardware (`hw_psg` / `hw_mix` / frontend) is not the remaining seam.
- Poryaaaa is a DAW player. `m4a_sound_main` does not walk song bytecode
  (`m4a_main.c:10-31`). MIDI/CC/XCMD is the ingress.
- The LFO axis gate is already fixed: `modT == 0` refreshes pitch only;
  tremolo/autopan refresh volume only (`m4a_track.c:1069-1080`).
- Wave start/load/DAC/trigger order, length-enable, and normal `CgbOscOff`
  writes match ROM `CgbSound` on the inspected paths.

## Ranking

Priority is live audible impact under ordinary MIDI/CC use, then
voice-config-specific bugs, then missing sequencer surface. Architectural
absences are real, but they are not “wrong CgbSound.”

| Pri | Finding | Live trigger | Confidence |
|---|---|---|---|
| — | LFOS/LFODL field-only | fixed 2026-08-13 | high |
| — | Note-on re-arms LFODL / `clear_modM` | fixed 2026-08-13 | high |
| — | Same-vblank CGB STOP is osc-off | fixed 2026-08-13 | high |
| — | CGB A=D=S=0 same-tick echo/off | fixed 2026-08-13 | high |
| — | Note-off is `ply_endtie` first match | fixed 2026-08-13 | high |
| — | PCM IEC length 0 wraps | fixed 2026-08-13 | high |
| — | Default PCM pool is hearth 12 | fixed 2026-08-13 | high |
| — | `PRIO` CC + player priority | fixed 2026-08-13 | high |
| P2 | No song walker / waits / FINE / patterns | ROM blob playback — not implemented | high |
| — | `m4a_note_on_timed` gate duration | fixed 2026-08-13 | high |
| — | `KEYSH` CC `0x1B` | fixed 2026-08-13 | high |

## P0 — wrong live semantics

### 1. LFOS and LFODL restart in-flight modulation — **fixed**

- ROM: `ply_lfos` stores `lfoSpeed` and calls `clear_modM` only when the new
  speed is 0 (`m4a_1.s:2392-2402`). `ply_lfodl` stores only `lfoDelay`
  (`m4a_1.s:1524-1530`).
- Now: CC `0x15` stores `lfoSpeed` and `clear_mod_m` only at 0. CC `0x1A`
  stores `lfoDelay` only. Tests: `test_v2_lfo_lfodl_resets_running_modulation`,
  `test_v2_lfos_nonzero_preserves_running_mod`.

### 2. Note-on does not re-arm LFO delay — **fixed**

- ROM: after allocation, `ply_note` copies `lfoDelay` → `lfoDelayC`; if the
  delay is nonzero it calls `clear_modM` (`m4a_1.s:2237-2242`).
- Now: successful CGB/PCM allocate copies `lfoDelayC` and `clear_mod_m` when
  delay != 0. Test: `test_v2_note_on_rearms_lfo_delay`.

### 3. STOP before the first CgbSound tick still starts the oscillator — **fixed**

- ROM: `START && STOP` branches to `oscillator_off` (`m4a.c:986-1044`).
- Now: `tick_one` disables before `emit_start_write` when `freshStart && STOP`.
  Test: `test_v2_cgb_start_stop_same_vblank_osc_off`.

### 4. CGB attack=0, decay=0, sustain=0 is deferred through RELEASE — **fixed**

- ROM: attack 0 falls to `envelope_decay_start`; decay 0 falls to
  `envelope_sustain_start`; sustain 0 goes to `envelope_pseudoecho_start` in
  the same `CgbSound` call (`m4a.c:1035-1038` → `1148-1160` → `1123-1127` →
  `1088-1101`).
- Was: `m4a_drv_cgb_start` set `M4A_CHN_ENV_RELEASE` (`m4a_cgb.c` old 90-95).
- Now: same-tick echo volume `(goal * iecv + 0xFF) >> 8`; nonzero → IEC +
  `MO_VOL` at that volume; zero → start writes then `CgbOscOff`. First IEC
  tick does not decrement `pseudoEchoLength`. Tests:
  `test_v2_cgb_triple_zero_echo_zero_disables`,
  `test_v2_cgb_triple_zero_echo_starts_iec`.

### 5. MIDI Note Off is not `ply_endtie` for duplicated same-key voices — **fixed**

- ROM: `ply_endtie` marks STOP on the first eligible matching `midiKey` and
  exits (`m4a_1.s:2342-2354`).
- Now: `m4a_note_off` stops the first matching CGB then PCM voice. Test:
  `test_v2_note_off_stops_first_same_key_only`.

## P1 — wrong under specific live config

### 6. PCM pseudo-echo length 0 is suppressed — **fixed**

- ROM: IEC if volume != 0; `length--` then `bhi` (`m4a_1.s:296-304`, `270-273`).
- Now: `pcm_can_pseudo_echo` is volume-only; length 0 wraps to 255. Test:
  `test_v2_pcm_pseudo_echo_zero_length_stops`.

### 7. Default DirectSound pool is not this ROM's 12 — **fixed**

- ROM hearth runtime is 12 (`m4a.c:76-79`, `445-451`).
- Now: `m4a_driver_create`, CLAP, engine, and Rust runtime default to 12.
  `M4A_MAX_PCM_CHANNELS` remains the 15-slot pool cap. Test:
  `test_v2_default_pcm_pool_is_hearth_12`.

### 8. Track `PRIO` cannot be expressed; player priority is omitted — **fixed**

- ROM: `ply_prio` + player priority, saturate at 255 (`m4a_1.s:1417-1423`,
  `2132-2140`).
- Now: CC `0x1C` writes track `priority`; `m4a_set_player_priority` stores
  `player_priority`; allocate uses the saturated sum. Test:
  `test_v2_prio_cc_and_player_priority_steal`.

## P2 — missing sequencer surface (not a live CgbSound bug)

These are real losses of ROM song meaning. They are expected of a DAW
player unless the host reconstructs them.

### 9. No song walker — **intentionally not implemented**

`MPlayMain` consumes waits, `GOTO`/`PATT`/`PEND`/`REPT`, and `FINE`
(`m4a_1.s:1255-1282`, `1336-1423`, `1839-1937`). Poryaaaa's main only advances `tempoC`
and the LFO (`m4a_main.c:16-31`). A DAW/MIDI host owns sequencing. Do not
build a ROM song walker unless the product goal changes.

### 10. No bytecode gate duration — **fixed**

`m4a_note_on` still has no duration (MIDI Note Off remains the DAW path).
`m4a_note_on_timed` copies `gateTime` onto the allocated CGB/PCM channel.
CGB and PCM decrement it each SoundMain and OR STOP at 0. Test:
`test_v2_note_on_timed_cgb_gate_expires`.

### 11. No `KEYSH` ingress — **fixed**

CC `0x1B` writes `keyShift = value - 0x40` and refreshes CGB/PCM pitches.
Test: `test_v2_keysh_cc_transposes_square`.

`xWAIT` (0x0C) is an intentional no-op (`m4a_track.c:723-726`; README and
`xcmd.md` already say so). A MIDI path has no script PC to stall.

## ROM-correct on the inspected live paths

- Voice / keysplit / drum resolution (`m4a_track.c:63-94`, `386-426` vs
  `m4a_1.s:2079-2110`).
- CC7 / CC10 / BEND / BENDR / TUNE / MOD / MODT scaling
  (`m4a_track.c:829-878`, `34-60` vs `m4a.c:757-805`, `m4a_1.s:1474-1546`).
- MODT switch refreshing both axes (`m4a_1.s:1532-1546`;
  `m4a_track.c:864-877`).
- LFO tick axis split (already fixed).
- Wave start/load/DAC/trigger order and same-pointer retrigger
  (`m4a.c:1000-1022`; `m4a_cgb.c:212-236`, `258-340`).
- Length-enable bit from nonzero voice length (`m4a.c:1013-1026`;
  `m4a_cgb.c:259-311`).
- Normal `CgbOscOff` payloads (`m4a.c:855-875`; `m4a_cgb.c:369-403`).
- CGB MO_VOL phase rules for square/noise vs wave sustain
  (`m4a.c:1091-1140`; `m4a_cgb.c:469-563`).
- PCM ADSR / derived L/R, FIX rate, loop/reverse, reverb int8 clamp,
  Golden Sun zero-size synth mode select (`m4a_pcm.c` vs `m4a_1.s`
  SoundMainRAM).
- Tempo accumulator formula `tempoI = (tempoD * tempoU) >> 8`, tick every
  150 (`m4a_driver.c:194-199`, `327-345`; `m4a_main.c:23-30` vs
  `m4a_1.s:1425-1435`, `1858-1869`).
- `xWAVE` notify-only (correct under MIDI; ROM address is not a host
  pointer). Implemented XCMD 0x02 and 0x04–0x0B store. `xWAIT` 0x0C is an
  intentional MIDI no-op, not an undocumented drop.

## Unknown / needs a fixture

- SMF BPM / PPQN versus ROM `TEMPO` + wait grid. Accumulator math matches;
  musical equivalence does not follow automatically.
- XCMD 0x0D: ROM writes `unk_3C` (`m4a.c:1637-1651`); poryaaaa stores the
  payload (`m4a_track.c:806-808`) and uses it as a PCM start offset
  (`m4a_track.c:657-660`). No named ROM reader was found. Host mapping, not
  a ROM-correct store.
- Golden Sun triangle/saw / modulated-pulse long traces.
- Portamento and PWM: present in poryaaaa, plugin-enabled, absent from the
  supplied ROM `m4a.c` / `m4a_1.s`. Non-ROM extensions, not source-provable
  mismatches.

## What not to chase next

- `hw_psg` / mix / SOUNDBIAS / frontend.
- ARM-cycle write spacing and Phase 5 driver-gate `WRITE` vs `TIMER`.
- Re-implementing `MPlayMain` inside the driver unless the product goal
  becomes a ROM song player.
- Re-opening the fixed vibrato `MO_VOL` zipper.

## Suggested next implementation order

Live-driver findings 1–8, 10, and 11 are implemented. Finding 9 stays a
documented DAW absence. Remaining research is the Unknown list and the
hardware/cycle items under “What not to chase next.”

## Sources

- `/Users/sallegrezza/dev/pokemon-hearth/src/m4a.c`
- `/Users/sallegrezza/dev/pokemon-hearth/src/m4a_1.s`
- `packages/poryaaaa/plugin/m4a/m4a_track.c`
- `packages/poryaaaa/plugin/m4a/m4a_cgb.c`
- `packages/poryaaaa/plugin/m4a/m4a_pcm.c`
- `packages/poryaaaa/plugin/m4a/m4a_main.c`
- `packages/poryaaaa/plugin/m4a/m4a_driver.c`
- `packages/poryaaaa/plugin/m4a/m4a_driver.h`
- `packages/poryaaaa/plugin/m4a/m4a_internal.h`
- `packages/poryaaaa/plugin/m4a_plugin.c`
