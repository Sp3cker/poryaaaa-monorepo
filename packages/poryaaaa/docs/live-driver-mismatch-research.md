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
| P0 | LFOS/LFODL reset running LFO phase | CC 0x15 / 0x1A mid-note | high |
| P0 | Note-on does not re-arm LFODL / `clear_modM` | next note on a MOD track | high |
| P0 | Same-vblank CGB STOP still emits start | note on+off before VBlank | high |
| — | CGB A=D=S=0 same-tick echo/off | fixed 2026-08-13 | high |
| P0 | Note-off stops every same-key voice | overlapping same-key ties only | high |
| P1 | PCM IEC length 0 stops instead of wrapping | `xIECL=0` with nonzero volume | high |
| P1 | Default PCM pool is not hearth's 12 | raw driver 15 / CLAP 5 | high |
| P1 | No `PRIO` ingress; no player priority | contended steal | high |
| P2 | No song walker / waits / FINE / patterns | ROM blob playback | high |
| P2 | No `gateTime` on `m4a_note_on` | ROM note duration byte | high |
| P2 | No `KEYSH` ingress | mid-song transpose opcode | high |

## P0 — wrong live semantics

### 1. LFOS and LFODL restart in-flight modulation

- ROM: `ply_lfos` stores `lfoSpeed` and calls `clear_modM` only when the new
  speed is 0 (`m4a_1.s:2392-2402`). `ply_lfodl` stores only `lfoDelay`
  (`m4a_1.s:1524-1530`).
- Poryaaaa: CC `0x15` always zeroes `lfoSpeedC` and `modM`
  (`m4a_track.c:857-863`). CC `0x1A` writes `lfoDelayC`, zeroes `lfoSpeedC`,
  and if `modM != 0` clears it and refreshes both CGB/PCM axes
  (`m4a_track.c:938-954`).
- Effect: a live LFOS/LFODL edit restarts and recentres vibrato/tremolo.
  ROM leaves the running triangle and current `modM` alone.
- Not the already-fixed `modT` axis gate.

### 2. Note-on does not re-arm LFO delay

- ROM: after allocation, `ply_note` copies `lfoDelay` → `lfoDelayC`; if the
  delay is nonzero it calls `clear_modM` (`m4a_1.s:2227-2242`; `r1` is 0
  from the preceding `ClearChain` setup).
- Poryaaaa: `m4a_note_on` never writes `lfoDelayC`, `lfoSpeedC`, or `modM`
  (`m4a_track.c:419-475`). LFO state only changes in CC handlers and
  `m4a_internal_lfo_tick`.
- Effect: later notes on a MOD track start at the leftover `modM` instead of
  dry, then waiting `lfoDelay` ticks.

### 3. STOP before the first CgbSound tick still starts the oscillator

- ROM: `START && STOP` branches to `oscillator_off` and never takes the
  start/write path (`m4a.c:986-1044`, `CgbOscOff` at `m4a.c:855-875`).
- Poryaaaa: `m4a_drv_cgb_start` always sets `freshStart`
  (`m4a_cgb.c:78-83`). `tick_one` emits start writes before it honors STOP
  (`m4a_cgb.c:407-432`). There is no `freshStart && STOP` off path.
- Effect: a note released before the next VBlank can load wave RAM, enable
  DAC, and trigger, then release. ROM only writes oscillator-off.

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

### 5. MIDI Note Off is not `ply_endtie` for duplicated same-key voices

- ROM: `ply_endtie` marks STOP on the first eligible matching `midiKey` and
  exits (`m4a_1.s:2342-2354`).
- Poryaaaa: `m4a_note_off` marks STOP on every matching CGB and PCM channel
  (`m4a_track.c:693-708`).
- Effect: overlapping tied same-key voices all enter release. Ordinary
  non-overlapping MIDI is fine; MP2K multiplicity is not.

## P1 — wrong under specific live config

### 6. PCM pseudo-echo length 0 is suppressed

- ROM: after release, nonzero `pseudoEchoVolume` sets IEC regardless of
  length (`m4a_1.s:296-304`). IEC does `length--` then `bhi` continue
  (`m4a_1.s:270-273`), so start length 0 wraps through 255.
- Poryaaaa: `pcm_can_pseudo_echo` requires both volume and length nonzero
  (`m4a_pcm.c:88-90`). IEC with length 0 stops immediately
  (`m4a_pcm.c:119-130`). CGB IEC already uses the signed wrap
  (`m4a_cgb.c:412-413` vs `m4a.c:1048-1049`).
- Effect: `xIECV != 0` and `xIECL == 0` loses the ROM wraparound tail.

### 7. Default DirectSound pool is not this ROM's 12

- ROM: `SoundInit` writes `maxChans = 8` (`m4a.c:381`), then
  `m4aSoundInit` immediately applies `SOUND_MODE_MAXCHN = 12`
  (`m4a.c:76-79`, `m4a.c:445-451`). Hearth runtime is 12.
- Poryaaaa: raw `m4a_driver_create` uses `M4A_MAX_PCM_CHANNELS` (15)
  (`m4a_driver.h:29`, `m4a_driver.c:169-172`). The CLAP plugin and several
  product paths then force 5 (`m4a_plugin.c:301`).
- Effect: dense PCM either keeps extra voices (raw 15) or steals early
  (CLAP 5) versus hearth's 12. This is a default/host-config mismatch, not
  a mixer-arithmetic bug. Hosts that call `m4a_set_max_pcm_channels(12)`
  match this ROM.

### 8. Track `PRIO` cannot be expressed; player priority is omitted

- ROM: `ply_prio` stores track priority (`m4a_1.s:1417-1423`). `ply_note`
  adds `MusicPlayerInfo.priority + track.priority`, saturating at 255
  (`m4a_1.s:2132-2140`), and allocation uses that sum.
- Poryaaaa: `t->priority` exists and is used (`m4a_track.c:440-454`), but
  no CC/API writes it (`m4a_driver.h:111-128`, `m4a_track.c:819-960`).
  `M4ADriver` has no player-priority field (`m4a_internal.h:223-246`).
- Effect: song-authored `PRIO` and multi-player steal order cannot reach
  live allocation. Single-player DAW use with default 0 is unaffected.

## P2 — missing sequencer surface (not a live CgbSound bug)

These are real losses of ROM song meaning. They are expected of a DAW
player unless the host reconstructs them.

### 9. No song walker

`MPlayMain` consumes waits, `GOTO`/`PATT`/`PEND`/`REPT`, and `FINE`
(`m4a_1.s:1255-1282`, `1336-1423`, `1839-1937`). Poryaaaa's main only advances `tempoC`
and the LFO (`m4a_main.c:16-31`). An MP2K song blob cannot play without an
external scheduler.

### 10. No bytecode gate duration

`ply_note` derives `gateTime` from `gClockTable` plus an optional duration
byte (`m4a_1.s:2043-2078`) and copies it onto the channel (`2247-2248`).
The channel loop stops on expiry (`m4a_1.s:1701-1709`). `m4a_note_on` has
no duration argument (`m4a_driver.h:112-113`) and writes `gateTime = 0`
(`m4a_track.c:475`, `625`; fields at `m4a_internal.h:156`, `200`). A host
Note Off at the gate boundary is equivalent; otherwise the note sustains.

### 11. No `KEYSH` ingress

`ply_keysh` writes signed key shift and ORs `MPT_FLG_PITCHG`
(`m4a_1.s:1438-1448`). Poryaaaa applies `keyShift` in `m4a_trk_vol_pit_set`
(`m4a_track.c:52-60`) but exposes no API/CC that sets it. Mid-song
transposition must be done by transposing MIDI notes.

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

1. LFOS/LFODL field-only updates, plus `ply_note` LFO re-arm on note-on.
2. CgbSound start: `START+STOP` → osc-off; A=D=S=0 → same-tick
   pseudo-echo/off.
3. `m4a_note_off` stops one eligible voice, matching `ply_endtie`.
4. PCM IEC length-0 wrap; default `maxChans` 12 on product paths.
5. Optional: `PRIO` / `KEYSH` CC if song conversion needs them.

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
