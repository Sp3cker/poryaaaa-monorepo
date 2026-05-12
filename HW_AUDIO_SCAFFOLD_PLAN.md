# Scaffold the m4a-driver / hw_audio split

**Status:** §12 steps 1–9 + 10a closed.  Driver core (LFO advancement
in `m4a_internal_lfo_tick`, PCM mixer + reverb audit with int8-clamp
parity fix, CGB retrigger semantics fix — `freshStart` flag in
`m4a_cgb.c` so NRx4 trigger fires only on note start, never on
envelope-update MO_VOL, and 2026-04-30 PCM event/ring timing fix —
new `M4A_REG_PCM_PUBLISH` event stamps each vblank's ring writes
with the firing offset, chip clamps reads to the published range,
`pcm_pos` pauses on FIFO underrun) and chip-side stack (PSG/PCM/mix
at variable internal rate driven by SOUNDBIAS sampling_cycle; two-
stage HwDmaToFifo + HwFifoDrain; windowed-sinc polyphase resampler to
host) are all in.  §12.10a self-consistency tests landed (per-cadence
audibility sweep, block-size invariance, DC streaming, anti-alias,
direct internal_rate switching assertion, two-stage drain regression,
fresh-trigger-only event-stream regression, PCM chunk-size
invariance, PCM publish-timing).  Audible + band-limited end-to-end
behind v2 flags.  **Current plan direction:** v1 should be deprecated
and removed from normal build products; it remains only as a comparison
reference until the v2 path has measured parity evidence.  The immediate
next step is not more tuning or v1 deletion: it is §12.11 comparison
test infrastructure that renders stable v2 and reference captures,
computes RMS / peak / DC / spectral metrics, and records tolerances.
Those comparison tests gate the remaining parity work.

Three blocking gates remain before any parity claim is valid (see
"Blocking gates before parity claims" at the end of §12): §12.10b mGBA
capture parity, the wave trigger / phase contract resolved against the
intended reference (mGBA / hardware, per memory
`project_audio_reference_target.md`), and PSG/DC/absolute-level parity.
PSG-unipolar **structural rework landed 2026-04-30** (hw_psg
unipolar synth + hw_mix `_applyBias`-mirror); the synth-domain shape
is now unipolar like mGBA's GBA-mode `GBAudioSamplePSG`.  2026-05-01
update with mgba-ss2 captures + analyzer single-trim alignment: full-
mix DC delta improved further to **+0.00364 (0.36 % full-scale)** and
the **DirectSound int8 DC follow-up has CLOSED** — pory fifo-a/-b DC
matches mGBA within 0.2 % normalized.  The wave-silent finding from
earlier was an analyzer artifact from per-channel silence-trim, not a
bug.  The gate is still **OPEN** on absolute level tuning, but the
data needed to drive that tune is blocked on a cleaner mGBA reference
(savestate captures include gameplay SFX that overlap the song; mGBA
headless `--audio-out` collapses to mono regardless of stereo
panning).  Work that changes parity-sensitive DSP now belongs after the
comparison harness so every tune is measured against the same reference
pipeline.  Final cleanup is v2 cutover / v1 removal, not a blind delete.
Branch: `loader-refactor`.
**Supersedes the structural part of `NEXT_SESSION.md`** (Layer 1–4
content there is still valid as future work; this doc replaces the
"two-track parallel build" section with a more faithful module split).

## 0. Progress snapshot — 2026-04-29

The v2 path is **audible end-to-end** under all four flag combos.
Production v2 (`M4A_DRIVER_V2=ON`, `HW_AUDIO_V2=ON`) drives the
event-stream API: `m4a_advance()` queues `M4ARegWrite` events with
sample offsets, `hw_audio_render_events()` segments the render span at
each event and produces audible PSG (square + wave + noise) plus
DirectSound output.  The snapshot (`M4ARegisterFile`) is no longer the
chip's timing input — it survives only as a non-timing read accessor
for UI / params / debug.

⚠ **Audibility ≠ parity.**  PSG / PCM / mix-bus rendering runs at the
chip-internal rate, which now derives from SOUNDBIAS sampling_cycle
per plan §7b: `internal_rate = max(131072, 32768 << sampling_cycle)`
(131072 Hz for sampling_cycle 0/1/2; 262144 Hz for sampling_cycle 3).
A windowed-sinc polyphase resampler bridges internal → host with
anti-alias band-limiting at host_rate/2 and is rebuilt when
sampling_cycle changes.  Sample-clock accounting is cumulative (block-
size invariant — same audio split across N small render calls
produces the same output as one big call past startup warmup).  Step
8 landed the mix-bus structure — full SOUNDCNT_L (NR50/NR51) +
SOUNDCNT_H (PSG vol code, DMA A/B vol codes, DMA L/R routing) +
SOUNDBIAS bias-add/clip pipeline (`hw_mix.{c,h}`).  Step 9 added the
polyphase resampler with cumulative tracking (`hw_resample.{c,h}` +
`hw_audio.c`).  Step 5 split landed: HwDmaToFifo + HwFifoDrain are
now distinct stages — `M4APcmRing` is read at pcm_rate (≈13379 Hz
for Pokemon Emerald), the FIFO head byte snapshots at SOUNDBIAS-
derived quirk_rate (32k/65k/131k/262k Hz), then upsamples to
internal_rate via render-rate hold.  Step 10's **self-consistency**
portion added per-cadence chip-only tests + a direct
`hw_audio_internal_rate()` assertion that proves cadence switching
actually wires up + a two-stage drain regression test; the
**mGBA capture-comparison parity** part of step 10 remains an open
parity blocking gate.  Three blocking gates remain (full detail in
"Blocking gates before parity claims" at the end of §12):
- **§12.10b mGBA capture parity**: chip-only canned outputs compared
  to mGBA captures at matching SOUNDBIAS rates.  Self-consistency
  tests (10a) landed but DON'T prove parity vs a reference.
- **Wave trigger / phase contract**: the chip resets `wave_phase=0`
  on a real NR34 trigger (matches GBATEK / mGBA), v1 preserves
  phase on its own retrigger path.  Today's `freshStart` fix means
  retriggers only fire on note start, but the chip-side reset on a
  legitimate trigger remains a divergence vs v1.  Block until the
  reference (mGBA / hardware, per `project_audio_reference_target.md`)
  is reflected by `hw_psg.{h,c}`, `m4a_cgb.c`, and tests agree.
- **PSG unipolar synth rework**: structural ports landed 2026-04-30,
  gate still OPEN.  hw_psg now outputs unipolar `bit * env_vol/15`
  (or 0) per mGBA `GBAudioSamplePSG` (`dcOffset=0`); hw_mix mirrors
  `_applyBias` (add bias → clip [0, 0x3FF] → subtract bias).  The
  PSG synth-domain shape is now correct.  Empirical state, 2026-05-01
  with ss2 captures + analyzer single-trim alignment:
  full-mix DC delta is now **+0.00364 (0.36 % full-scale, mGBA
  -0.01866 vs pory -0.01502)**.  **DirectSound DC follow-up CLOSED**
  — pory fifo-a R DC = -0.01550 vs mGBA -0.01656 (Δ +0.00106);
  fifo-b L DC = -0.01519 vs mGBA -0.01690 (Δ +0.00171).  Wave-
  silent finding was an analyzer artifact (per-channel silence-trim
  picked different musical regions per channel); both sides agree
  wave is silent in the first 30 s of mus_shop.  **Single open
  follow-up**: absolute level tuning, blocked on a cleaner mGBA
  reference (gameplay SFX overlap the song; headless `--audio-out`
  collapses to mono regardless of NR51 panning).  See
  `project_psg_unipolar_parity.md` for the full change log.

Closed since prior plan revision: DirectSound PCM event/ring timing.
2026-04-30 fix landed `M4A_REG_PCM_PUBLISH` events (one per vblank
stamped with the firing sample_offset) plus a chip-side
`pcm_published_through` clamp that gates ring reads, with `pcm_pos`
pausing on FIFO underrun.  Two regressions guard against drift:
`test_v2_pcm_chunk_size_invariance` (one big call vs 2048-frame vs
64-frame chunks of the same PCM render → bit-identical output) and
`test_v2_pcm_publish_timing` (pre-vblank host frames don't see
post-vblank PCM data).  Driver-side `vblank_accum` was promoted from
`float` to `double` to bound cumulative-add error across long runs.
Driver timing note: the v2 m4a driver timing constants are now fixed
for this rewrite.  Tests that assert exact vblank counts or 44.1 kHz
sample offsets are intentional canonical-rate regressions, not generic
timing-property tests.  If they fail, treat that as a driver timing
contract break unless the reference plan is explicitly revised.

**Boot-time-only SOUNDBIAS target restriction** (mid-call
sampling_cycle changes are deferred to next render boundary; this
matches Pokemon Emerald and ROMhacks that set SOUNDBIAS at boot)
is documented as a chosen scope reduction, not a closed parity gate.

Verification most recently observed (2026-04-30):

| `M4A_DRIVER_V2` / `HW_AUDIO_V2` | `poryaaaa_unit_tests` |
|---|---|
| OFF / OFF (deprecated v1 reference) | 86 / 86 |
| ON / OFF (v2 driver, v1 chip — incl. LFO + reverb + retrigger + PCM publish driver-side regressions) | 190 / 190 |
| OFF / ON (chip-only canned-event tests + solo-mask) | 148 / 148 |
| ON / ON (full v2 — driver + LFO + reverb + retrigger + PCM publish + chip incl. PCM two-stage / mix / polyphase / cadence sweep + PCM chunk-size invariance + PCM publish-timing + solo-mask) | 267 / 267 |

`poryaaaa` and `poryaaaa_render` built in v1 and full-v2 configs.

Closed during the rewrite (highlights):

- CGB pan routing no longer gets cleared by `panMask`; tests assert
  centered CGB output populates both NR51 pan masks.
- DirectSound frequency now applies the same PCM tick-rate divider as v1.
- Active PCM channels now get volume and pitch refreshes for CC7, pan,
  bend range, MODT, TUNE, pitch bend, song-volume changes, and future
  LFO reset cases.
- `m4a_consume_writes()` clears snapshot trigger latches after the event
  batch is consumed, so the event path and snapshot cache do not diverge.
- Wave-channel note start emits byte-granular wave-RAM events in the
  intended order: `NR30=0`, 16 `WAVE_RAM_BYTE` writes, `NR30=DAC-state`,
  then length/volume/frequency/trigger writes.
- MODT now recomputes derived track state before refreshing active
  channels.
- §12.6 noise verification gate closed: drop v1's coalesce; mirror
  mGBA `gb_audio.c:631-637` polynomial; LFSR resets to 0 (NOT 0x7FFF
  — that's a fixed point) on NR44 trigger.  See `noise_verification_gate.md`.
- §12.8 mix-bus structure landed (`hw_mix.{c,h}`): PSG/PCM produce
  per-channel mono buffers; HwMixBus consumes those, applies full
  SOUNDCNT_L/H routing+scaling, then runs the GBA's unsigned 10-bit
  DAC bias-add/clip pipeline (default bias 0x200 → symmetric ±1; non-
  default offsets DC + clips asymmetrically).  Four chip-only tests
  cover bias DC offset, bias asymmetric clip, PSG vol code 25/50/100%,
  DMA vol code 50/100% scaling.
- §12.9 polyphase resampler landed (`hw_resample.{c,h}`): windowed-sinc
  (TAPS=32, PHASES=64, Hann window) with kernel cut at min(internal,
  host)/2.  Synth + mix moved off host rate to a chip-internal rate
  driven by SOUNDBIAS sampling_cycle: `max(131072, 32768 << sc)`.
  Resampler downsamples to host with anti-alias band-limiting at
  host_rate/2.  Cumulative sample-clock accounting (`hw_audio.c`'s
  `inputs_for_total_outputs` formula) gives block-size invariance:
  identical output regardless of how the caller chunks render windows.
  New chip-only tests confirm attenuation above host Nyquist + block-
  size invariance + DC preservation across many small calls.
- §12.10a per-SOUNDBIAS-cadence tests landed: chip-only canned-event
  tests sweep all four sampling_cycle values via the setup-then-play
  pattern (one short render call applies SOUNDBIAS, the next runs
  audio at the new rate).  Verifies audibility and comparable peak
  amplitudes across cadences.  Mid-call SOUNDBIAS sampling_cycle
  changes deferred to next render boundary by design (snapshot at
  start-of-call).
- §12.1 LFO advancement landed: `m4a_internal_lfo_tick` (m4a_track.c)
  fires from m4a_main.c's tempoC overflow loop; vibrato/tremolo/
  autopan now move `modM` per-tick, refresh active CGB + PCM volume
  (and pitch when MODT=0).  Four behavioural tests pin the
  non-degenerate cases.
- §12.2 PCM mixer + reverb audit landed: one parity bug fixed —
  reverb delay line stores int8-clamped wet samples (matches v1 /
  real m4a where the delay buffer IS the int8 FIFO).  Envelope
  state machine, render loop wraparound, and DMA A/B routing
  verified equivalent to v1.
- 2026-04-30: CGB retrigger contract fix landed.  `M4ADriverCgbChan`
  gained a `freshStart` flag, set in `m4a_drv_cgb_start` and consumed
  by `emit_vol_write`, so NRx4 trigger=1 fires on note start only.
  Pre-fix: every envelope-update MO_VOL re-emitted NR34/NR44 with
  trigger=1, resetting the chip's wave RAM position and noise LFSR
  every vblank during sustained notes.  Driver-side regression test
  `test_v2_cgb_trigger_only_on_note_start` counts trigger events
  across attack→sustain→release→re-attack.
- 2026-04-30: DirectSound PCM event/ring timing fix landed.  Driver
  emits one `M4A_REG_PCM_PUBLISH` event per vblank inside
  `m4a_sound_main_ram` with `sample_offset = event_vblank_offset`;
  HwPcm advances `pcm_published_through` by
  `M4A_PCM_SAMPLES_PER_VBLANK` per event and clamps ring reads to
  the published range.  `pcm_pos` pauses on FIFO underrun, matching
  real-hardware DAC-on-stalled-DMA semantics.  `vblank_accum` was
  promoted from `float` to `double` to bound cumulative-add error
  across long runs.  Two regressions cover the contract:
  `test_v2_pcm_chunk_size_invariance` (one big call vs 2048-frame
  vs 64-frame chunks → bit-identical) and
  `test_v2_pcm_publish_timing` (pre-vblank frames don't see post-
  vblank ring data).  Closed the prior plan revision's "DirectSound
  PCM event/ring timing" blocking gate.
- 2026-04-30: HwAudio per-channel solo-mask landed for §12.10b
  capture-parity infra.  `hw_audio_set_solo_mask()` + bit enum
  `HW_AUDIO_SOLO_*` gate which pre-mix buffers feed `hw_mix_render`;
  PSG and PCM still synthesise unconditionally so internal state
  stays in sync.  Channel-name parity with the patched mGBA tool:
  `poryaaaa_render --solo <name>` accepts full / psg / directsound /
  ch1\|sq1 / ch2\|sq2 / wave / noise / fifo-a\|dma-a / fifo-b\|dma-b.
  Two chip-only regressions guard:
  `test_chip_canned_solo_mask_isolates_channels` (PSG-only mutes
  PCM, DSound-only mutes PSG, individual channels isolate cleanly,
  zero-data channels read silent) and
  `test_chip_canned_solo_mask_empty_falls_back_to_full` (defensive:
  empty mask doesn't accidentally silence everything).

---

## 1. Context

The current audio engine in `plugin/m4a_engine.c` and `plugin/m4a_channel.c`
fuses four concerns into the same per-sample functions: voice synthesis,
GBA hardware artifacts, output gain/scaling, and resampling.  A chain of
one-off fixes — `(sample * 3) >> 2` per-CGB scaling, `_coalesceNoiseChannel`
clone embedded in noise synth, PCM `pcmStepHz` accumulator on the engine
struct, CGB internal rate ping-pong (32768 → 65536 → 32768 → 49152) —
each correct in isolation but the layering is wrong; new fixes break
old ones.

The decision (memory `project_audio_engine_rewrite.md`, plan
`NEXT_SESSION.md`) was a layered DSP rewrite.  Conversation 2026-04-28
sharpened the split: the boundary should follow the line that exists in
the real GBA, between **software** (m4a, the GBA's audio driver, running
on the ARM7) and **hardware** (the GBA audio chip, emulated by mGBA).

So the rewrite is two top-level modules:

- **`plugin/m4a/`** — software driver.  Mirrors the real m4a routines from
  the pokeemerald disassembly (Music 4 Advance / Sappy / MPlayDef).
  Responsibilities: MIDI/song walking, per-track state, envelope/pitch/pan
  computation, **PCM software mixing** (real m4a does this in software —
  `SoundMainRAM`), **reverb** (real m4a, `SoundMainRAM_Reverb`), and
  writing CGB hardware registers (`ChnVolSetCgb`, `CgbSound`).
- **`plugin/hw_audio/`** — chip emulation, mirroring mGBA's
  `gb_audio.c` + `gba_audio.c`.  Responsibilities: PSG synthesis (square 1/2,
  wave, noise) from register state, hardware quirks (NR32 quantization,
  verified noise sampling behavior, PCM FIFO sample-and-hold, declick, SOUNDBIAS
  clip), final mix via SOUNDCNT_H/L, output resampler to host rate.

**Crucial fidelity guideline**: code in `plugin/m4a/` must track the
behaviour of real m4a.  Function names, control flow, and data shapes
should be recognisable to anyone reading the pokeemerald disassembly.
That's how we keep the layering honest — if a piece of behaviour doesn't
appear in real m4a, it doesn't belong in `plugin/m4a/`.

**Dialect:** we mirror **vanilla Sappy m4a** (as in vanilla pokeemerald),
where `SoundMainRAM_Reverb` is a separate routine running after
`SoundMainRAM`.  We do *not* mirror pokeemerald-expansion's ipatix
HQ-Mixer rev 4.0, where reverb is integrated into the mixer loop.  The
existing v1 `plugin/m4a_reverb.c` is already a separate-pass design;
v2 keeps that shape.  When future contributors grep pokeemerald-expansion
sources, they'll see different routine names — that's expected; cross-check
against vanilla pokeemerald.

This plan started as **scaffolding only**.  The scaffold has since grown
into the first driver-port pass; §12 now tracks both completed scaffold
work and the remaining parity gates.

---

## 2. Rationale (per-decision)

### 2a. Two modules, not four layers

`NEXT_SESSION.md` originally proposed four numbered layers all under one
`m4a_*` namespace.  Today's refinement: that conflates the *software*
(m4a) with the *hardware* (chip).  Splitting at the software/hardware
boundary is more faithful to the GBA itself, makes the contract between
modules explicit (it's a register file — the same one the real ARM7
writes to), and gives us a clean place to put each concern without having
to invent a new ontology.  Internal staging within `hw_audio/` (synth →
fifo S&H → mix bus → resample) is still useful — but as private detail,
not a public layering scheme.

### 2b. Function-name fidelity to real m4a

Driver-side code names should mirror pokeemerald disassembly:
`m4a_sound_main`, `m4a_mplay_main`, `m4a_trk_vol_pit_set`, `m4a_cgb_sound`,
`m4a_chn_vol_set_cgb`, `m4a_chn_vol_set_direct`, `m4a_sound_main_ram`,
`m4a_sound_main_ram_reverb`, `m4a_midi_key_to_freq`,
`m4a_midi_key_to_cgb_freq`.  Reasons:
- A new contributor (or future-me) can compare directly against
  pokeemerald source to sanity-check behaviour.
- Drift-prevention: if a function does something real m4a's namesake
  doesn't do, that's an early warning to question it.
- It keeps documentation/comments referenceable across projects.

Chip-side names mirror mGBA's structure (`hw_psg_*`, `hw_fifo_*`,
`hw_mix_*`, `hw_resample_*`) for the same reason.

### 2c. PCM mixing belongs to the driver, not the chip

Real m4a's `SoundMainRAM` walks every active PCM voice, fetches samples
from the voicegroup, multiplies by the m4a-computed envelope/pan, and
**sums into a stereo mix slice** (224 frames per VBlank in Pokemon
Emerald's default 13379 Hz mode).  After reverb, the mix is clamped to
int8 and written into the fixed PCM DMA ring.  DMA then feeds FIFO_A/B,
with final left/right routing controlled by SOUNDCNT_H.  The audio chip
never sees individual PCM voices — only the mixed FIFO stream.

Putting per-voice PCM synthesis in `hw_audio/` would invent a code path
that doesn't exist on real hardware.  The PCM mixer therefore lives in
`plugin/m4a/`, writing into the shared `M4APcmRing` (a ring buffer
matching real m4a's `gPcmDmaBuffer`).  The chip's PCM job is purely the
FIFO/S&H part.

### 2d. Reverb is software

`SoundMainRAM_Reverb` is m4a software, running on the same mix buffer
between the mixer and the DMA write.  It's not a hardware effect — the
chip never sees reverb.  So reverb lives in `plugin/m4a/`, applied
in-place on the mix buffer.

This corrects the earlier sketch (in `NEXT_SESSION.md`) which had reverb
inside the mix bus.  The mix bus is hw, reverb is sw; they don't belong
together.

### 2e. Decoded fields, not raw NRxx bytes

The scaffold snapshot (`M4ARegisterFile`) exposes envelope volume, freq
word, duty cycle etc. as already-decoded fields rather than packed NRxx
bit layouts.  Reasons:
- m4a always computes envelope volume in software anyway.  Raw NR12/NR22
  bit packing would be busywork on the driver side and bit-decoding
  busywork on the chip side, with zero behavioural change.
- Decoded fields are easier to read/diff/test — a parity test comparing
  freq=`0x4A0` vs duty=`2` is easier than comparing two `uint8_t` blobs.
- We retain GBA *semantics* exactly; only the *encoding* differs.
- The Layer 1.5 event stream still uses register-shaped write IDs for
  timing-sensitive behavior.  The decoded snapshot is a cache for
  scaffold/debug/UI paths, not the authoritative chip timing interface.

### 2f. Two CMake flags, not one — driver/chip independent

CMake exposes `M4A_DRIVER_V2` and `HW_AUDIO_V2` independently, each
defaulting OFF in the original scaffold.  That was correct while v1
was the safety path.  The project is now moving out of scaffold mode:
v1 should be deprecated and no longer built for normal products.  The
flags may remain temporarily as comparison / bisect controls, but
full-v2 should become the normal build configuration once the cutover
plumbing is in place.  Reasons the flags still matter during this
transition:
- Each Layer 1–7 pass can be merged independently and validated against
  v1 by toggling either flag.
- v1 can remain a reference path for comparison tests without being the
  shipped or default product path.
- A/B diff testing (render same MIDI through v1 and v2, take diff WAV)
  becomes trivial with multiple builds.
- **Independent validation**: with driver-only ON we can A/B the m4a
  port against v1's chip path.  With chip-only ON we can feed canned
  snapshots during the scaffold, and canned `M4ARegWriteBatch` events
  after Layer 1.5, into `hw_audio` tests without booting a driver.

The flags also document intent: `plugin/m4a/` is *only* the v2 driver,
`plugin/hw_audio/` is *only* the v2 chip.  v1 stays under
`plugin/m4a_engine.c` and friends only as a deprecated comparison
reference until v2 cutover removes it from normal targets.

### 2g. Wave declick is a hardware artifact (in `hw_audio/`)

Earlier drafts kept declick in voice-level synthesis, sometimes in driver
code.  But real m4a's `CgbSound` doesn't have a declick — the audible
declick on note transitions comes from the GBA hardware (the wave-RAM
write cycle vs. NR30 retrigger sequencing creates a brief discontinuity).
Strict m4a fidelity puts declick in `hw_audio/`, applied as part of the
wave-channel emulation.

### 2h. Two internal rates: SOUNDBIAS-derived quirks, output upsample floor

This is a `hw_audio/` Layer 1 concern, not a scaffolding concern, but
the rate strategy needs to be locked now to keep the contract honest.

mGBA's `audio->sampleInterval = 0x200 >> resolution` makes SOUNDBIAS
resolution the GBA output sample cadence (32768/65536/131072/262144 Hz).
The `hw_audio` quirk-application rate must follow SOUNDBIAS: PCM S&H,
NR32 quantization, wave declick, and the final mix/bias stage happen at
the rate the GBA chip actually produces samples.  Noise behavior is
verified before the noise port; do not assume v1's coalescing is correct.

Plan splits the chip into two rates:
- **Quirk rate** = SOUNDBIAS-derived.  PSG synth, FIFO drain, mix bus.
  For Pokemon Emerald (SOUNDBIAS=0): 32768 Hz.
- **Output mix rate** = `max(131072, quirk_rate)`.  Per-channel signals
  upsample (or pass through) to this rate purely so the polyphase
  resampler to host has clean integer relationships and never
  downsamples chip output before the resampler.  Detail in §7b.

131072 Hz remains a target floor for output but is no longer the
artifact-producing rate, and not a hard cap when SOUNDBIAS=3 is in use.
v1's "CGB native at 49152 Hz then resample" path is
behaviourally close to the new quirk-rate model for SOUNDBIAS=0 (where
v1 used 49152 mostly for clean ratios; v2 uses 32768 because that's what
SOUNDBIAS actually says).

### 2i. Scaffold-only first, ports follow

We could try porting Layer 1 in the same PR as the directory creation,
but every prior attempt to fold structural and behavioural changes into
one PR has produced regressions that take days to bisect.  Scaffolding
in isolation gave us a stable initial target — "the call graph compiles
and runs" — verifiable in minutes.  Behaviour ports have landed on top
in ordered passes: §12 steps 1 (driver core incl. LFO advancement +
2026-04-30 freshStart retrigger fix), 2 (PCM mixer + reverb audit,
one parity bug fixed: int8-clamped reverb delay line), 3 (event-
stream contract), 4 (PSG square + wave), 5 (PCM two-stage drain),
6 (noise gate), 7 (noise synth), 8 (mix bus + SOUNDBIAS bias/clip),
9 (polyphase resampler), and 10a (chip-only self-consistency tests)
are all closed.  Three blocking gates remain before any parity claim
is valid (full detail in "Blocking gates before parity claims" at
the end of §12): §12.10b mGBA capture parity, the wave trigger /
phase contract resolved against the intended reference (mGBA /
hardware), and PSG/DC/absolute-level parity.  PSG-unipolar structural
work has landed; the remaining question is measured level / DC parity
against a cleaner reference capture set.  DirectSound PCM event/ring
timing + chunk-size-invariance closed 2026-04-30 (PCM_PUBLISH event
+ chip-side publish gate + chunk-size invariance regression).  The
next phase is comparison testing first, then parity-dependent tuning /
cutover work based on those measurements.

---

## 3. What real m4a actually does (reference table)

Mapping the canonical m4a routines to module placement:

| Routine | What it does | Module |
|---|---|---|
| `SoundMain` | Top-level vblank entry; calls each player + FIFO buffer flip | `m4a/` |
| `MPlayMain` | Walk one music player's track events, dispatch ply_* | `m4a/` |
| `ply_note` / `ply_endtie` / etc. | Per-event handlers | `m4a/` |
| `TrkVolPitSet` | Recompute track vol/pan/pitch from CC + LFO + bend | `m4a/` |
| `ChnVolSetCgb` | Compute CGB env volume + pan; write NRx2 / NR51 | `m4a/` |
| `ChnVolSetDirect` | Compute envelope volumes for PCM voices | `m4a/` |
| `CgbSound` | Software CGB envelope/length/register updates; writes NRxx | `m4a/` |
| `SoundMainRAM` | PCM software mixer | `m4a/` |
| `SoundMainRAM_Reverb` | Reverb on mix buffer | `m4a/` |
| `MidiKeyToFreq` / `MidiKeyToCgbFreq` | MIDI key → freq word | `m4a/` |
| `gFreqTable` / `gCgbFreqTable` / `gNoiseTable` / `gCgb3Vol` | static tables | `m4a/` |
| GBA PSG synthesis | Reads NRxx, generates samples | `hw_audio/` |
| FIFO read with sample-and-hold | DMA FIFO drained at PCM rate | `hw_audio/` |
| SOUNDCNT_H DMA volume codes (50/100%) | DMA-channel post-attenuation | `hw_audio/` |
| SOUNDCNT_H PSG volume code (25/50/100%) | PSG bus attenuation | `hw_audio/` |
| SOUNDCNT_L (NR50/NR51) | PSG mixing | `hw_audio/` |
| SOUNDBIAS bias add + clip | DAC stage | `hw_audio/` |
| Output resampling to host | (mGBA's role) | `hw_audio/` |
| Wave declick | hardware artifact | `hw_audio/` |

---

## 4. Scope — structural scaffold + driver port + audible chip placeholder

Completed scaffold + driver-port work (§12 steps 1-9 + 10a closed
2026-04-29; step 10b mGBA capture-comparison parity remains the
open parity gate):

- `plugin/m4a/` and `plugin/hw_audio/` directories with full module
  layout (see §5).
- `M4ARegisterFile` (driver→chip register snapshot, non-timing) and
  `M4APcmRing` (driver-mixed PCM ring) types.
- `M4ARegWrite` event stream (§6c) implemented end-to-end on both sides:
  driver emits per-CgbSound writes with sample offsets; chip's
  `hw_audio_render_events()` segments at each offset, applies events to
  PSG + PCM subsystems, and produces audible output.
- CMake `M4A_DRIVER_V2` and `HW_AUDIO_V2` independent flags currently
  drive a 4-way build matrix (v1 reference / driver-only / chip-only /
  full v2).  V1 is deprecated and should stop being built for normal
  product targets; keep it only where comparison tests explicitly need
  the reference path.
- All four entry points (CLAP `process`, headless export, CLI render,
  unit tests) chunk at `M4A_RECOMMENDED_MAX_ADVANCE_FRAMES` and feed
  `m4a_advance` / `hw_audio_render_events` / `m4a_consume_writes`.
- `hw_audio_render()` (the legacy snapshot-driven API) is preserved as
  a trigger-consumption-only no-render path for any call site that
  hasn't migrated; production v2 does NOT use it.
- v1 path (both flags OFF) is **deprecated reference code**.  It should
  no longer be the normal product build, but it remains useful until
  comparison tests have captured the v1-vs-v2 and mGBA-vs-v2 deltas.

Driver-side modules:

- `m4a_main.c`: vblank accumulator and `SoundMain` shell with event
  `sample_offset` stamping.
- `m4a_track.c`: program/note/CC/pitch/all-notes/all-sound ingress,
  CGB/PCM channel allocation, CGB + PCM volume/pitch refresh.
- `m4a_cgb.c`: CGB envelope advancement + Layer 1.5 NRxx event emission
  in pokeemerald CgbSound order (with byte-granular wave-RAM events).
- `m4a_pcm.c`: `SoundMainRAM` PCM mixer + `SoundMainRAM_Reverb`,
  ring writer.
- `m4a_freq.c`: v2-local frequency/noise tables and MIDI key conversion.

Chip-side modules:

- `hw_audio.c`: `hw_audio_render_events()` walks events, renders
  PSG/PCM/mix at the chip-internal rate (131072 Hz) in chunks, feeds
  to the polyphase resampler, drains host samples to outL/outR.
  `hw_audio_render()` legacy snapshot path retained as no-render
  trigger-consumption only.
- `hw_psg.c` / `hw_psg.h`: square 1, square 2, wave, noise synth at
  the chip-internal render rate (`max(131072, 32768 << sampling_cycle)`,
  driven by SOUNDBIAS).  Produces 4 mono per-channel buffers; mix is
  HwMixBus's job.
- `hw_pcm.c` / `hw_pcm.h`: two-stage drain — HwDmaToFifo reads
  `M4APcmRing` at `pcm_rate_hz`, HwFifoDrain snapshots the FIFO head
  at SOUNDBIAS-derived quirk_rate.  Produces 2 mono per-FIFO buffers
  at internal_rate; routing/scaling is HwMixBus's job.
- `hw_mix.c` / `hw_mix.h`: SOUNDCNT_L (NR50/NR51) + SOUNDCNT_H +
  SOUNDBIAS pipeline.  Combines 4 PSG mono + 2 DMA mono buffers,
  applies master vol + pan + per-channel bus volume codes + DMA L/R
  routing, then runs the GBA's unsigned 10-bit DAC bias-add/clip.
  Operates at the chip-internal rate.
- `hw_resample.c` / `hw_resample.h`: windowed-sinc polyphase
  resampler (TAPS=32, PHASES=64, Hann window).  Bridges
  chip-internal rate → host rate with anti-alias band-limit at
  host_rate/2.  DC-gain-normalized per-phase kernel.  Kernel rebuilds
  + state resets when sampling_cycle changes (i.e. internal rate
  changes between calls).

Open work — see "Blocking gates before parity claims" at the end of §12:

- §12 step 11 — comparison test infrastructure is the next step.  It
  should render stable v2/reference captures and report RMS / peak / DC /
  spectral metrics before further parity tuning.
- §12 step 10b — mGBA capture-comparison parity (self-consistency
  landed at §12.10a but doesn't prove match against a reference).
- Wave trigger / phase contract resolved against the intended
  reference (mGBA / hardware), with `hw_psg.{h,c}`, `m4a_cgb.c`,
  and tests agreeing.
- PSG/DC/absolute-level parity: structural unipolar PSG work and
  DirectSound int8 DC follow-up are closed, but absolute level tuning
  still needs clean comparison data.  See `project_psg_unipolar_parity.md`.
- After comparison tests exist: parity tuning, whole-song A/B, then v2
  cutover and v1 removal.

---

## 5. File layout

```
plugin/
  m4a/                       NEW — software driver (mirrors real m4a)
    CMakeLists.txt
    m4a_driver.h             public API to the rest of the program
    m4a_driver.c             lifecycle, config, register defaults, accessors
    m4a_internal.h           private driver state
    m4a_register_file.h      driver→chip register snapshot (shared)
    m4a_pcm_ring.h           PCM ring buffer type (shared)
    m4a_main.c               SoundMain shell + vblank accumulator
    m4a_track.c              TrkVolPitSet, ingress, channel allocation
    m4a_cgb.c                CgbSound: envelope ticks + Layer 1.5 NRxx event emission
    m4a_pcm.c                SoundMainRAM PCM mixer + reverb ring writer
    m4a_freq.c/.h            MidiKeyToFreq, MidiKeyToCgbFreq, tables
    /* still future: m4a_mplay.c — MPlayMain / ply_* event handlers */
  hw_audio/                  NEW — chip emulation
    CMakeLists.txt
    hw_audio.h               public API
    hw_audio.c               event-walk + segment render (Layer 1.5);
                             legacy hw_audio_render() retained as
                             trigger-consume-only no-render path
    hw_audio_internal.h      internal stage markers (psg_synth, fifo_sh, mix, resample)
    hw_psg.c/.h              square + wave + noise synth at chip-internal
                             rate (max(131072, 32768<<sampling_cycle)).
                             4 mono per-channel buffers.
    hw_pcm.c/.h              two-stage drain (HwDmaToFifo at pcm_rate +
                             HwFifoDrain at SOUNDBIAS quirk_rate).
                             2 mono per-FIFO buffers at internal rate.
    hw_mix.c/.h              SOUNDCNT_L/H + SOUNDBIAS bias-add/clip mix bus
                             (step 8 closed; runs at internal rate)
    hw_resample.c/.h         windowed-sinc polyphase resampler internal
                             → host (step 9 closed; rebuilds when
                             sampling_cycle changes between render calls)
  m4a_engine.{c,h}           UNCHANGED (v1)
  m4a_channel.{c,h}          UNCHANGED (v1)
  m4a_reverb.{c,h}           UNCHANGED (v1)
  m4a_tables.{c,h}           UNCHANGED (v1)
  m4a_plugin.c               +#ifdef branch at the engine-process call site
  m4a_params.c               UNCHANGED
```

---

## 6. Driver → chip contract

Two shared types in `plugin/m4a/`, included by both modules.

### 6a. `M4ARegisterFile` — GBA audio register snapshot

**Provisional contract.** The snapshot is the v2-scaffold contract and a
non-authoritative read-accessor going forward.  It is *not* the long-term
authoritative interface for chip timing — see §6c for the ordered-event
stream that replaces it at Layer 1.5.  PSG parity claims are gated on
events landing.  The snapshot remains in the public API for non-timing
consumers (UI, params, debug, some tests) as a "current state" cache,
but the chip's internal logic at Layer 1.5+ is driven by events.

**Ownership rule:** every numeric field is the *current* hardware register
value as the GBA chip would see it.  The driver owns *writes* to those
fields (driven by m4a's `CgbSound`, `ChnVolSetCgb`, etc.).  The chip owns
*advancement* of hardware state: sweep counter (NR10), length counters
(NRx1 low bits, decremented by the GB APU frame sequencer when NRx4
length-enable is set), hardware envelope pace counter (NRx2 — although
m4a typically writes pace=0 to disable it, see the trigger paragraph),
LFSR (noise channel), and phase accumulators.  The GB APU frame sequencer
is independent of SOUNDBIAS; SOUNDBIAS controls the GBA output sample
cadence and FIFO sample arrays.

Envelope *volume* is the exception: m4a software ticks it (the
NRx2-high-nibble value the chip reads) and writes the result back via
register events.  The chip's hardware envelope pace counter never gets
to count down because m4a writes pace=0 + a fresh volume each tick.
So envelope volume is driver-ticked; envelope pace counter is
chip-owned-but-disabled.

Phase counters, LFSR state, sweep-modified frequency, length-counter
remaining count, DAC click state, and similar *hardware* runtime state
lives inside the chip module and is not visible in the register file.
See the trigger-latch semantics paragraph below.

```c
typedef struct {
    /* Master enable (NR52 bit 7).  Real m4a writes via REG_SOUNDCNT_X;
     * when 0 all four PSG channels are silent regardless of per-channel
     * enable. */
    bool     psg_master_enabled;

    /* PSG square 1 (NR10–NR14) */
    uint8_t  sq1_sweep_pace;        /* 0..7 — *hardware* sweep pace */
    int8_t   sq1_sweep_dir;         /* -1 / +1 */
    uint8_t  sq1_sweep_step;
    uint8_t  sq1_duty;              /* 0..3 → 12.5/25/50/75% */
    uint8_t  sq1_env_volume;        /* 0..15, m4a-computed; chip never advances */
    uint16_t sq1_freq;              /* 11-bit GB freq word */
    uint8_t  sq1_length;            /* NR11 low 6 bits, set on note-on/start */
    bool     sq1_length_enable;     /* NR14 bit 6 */
    bool     sq1_enabled;
    /* …sq2 same minus sweep, with sq2_length / sq2_length_enable… */

    /* Wave (NR30–NR34 + wave RAM) */
    bool     wave_dac_on;           /* NR30 bit 7 — drives chip-side declick */
    uint8_t  wave_length;           /* NR31, set on note-on */
    bool     wave_length_enable;    /* NR34 bit 6 */
    uint8_t  wave_volume_shift;     /* 0=mute, 1=100%, 2=50%, 3=25% */
    uint16_t wave_freq;             /* already rounded by driver per SOUNDBIAS res */
    bool     wave_enabled;
    uint8_t  wave_ram[16];          /* 32 nibbles; written only when wave_dac_on=false */

    /* Noise (NR41–NR44) */
    uint8_t  noise_env_volume;
    uint8_t  noise_length;
    bool     noise_length_enable;
    uint8_t  noise_clock_shift;     /* s */
    uint8_t  noise_divisor_code;    /* r */
    bool     noise_width_7bit;
    bool     noise_enabled;

    /* SOUNDCNT_L (NR50/NR51) — *PSG-only* master volume + per-PSG-channel pan
     * masks.  PCM channels do NOT route through these. */
    uint8_t  master_vol_left;       /* 0..7 */
    uint8_t  master_vol_right;
    uint8_t  pan_mask_left;         /* bit per PSG channel: bit0=sq1, …, bit3=noise */
    uint8_t  pan_mask_right;

    /* SOUNDCNT_H — PSG/DMA bus volumes + DMA L/R routing */
    uint8_t  psg_volume_code;       /* 0..2 = 25/50/100% */
    uint8_t  dma_a_volume_code;     /* 0..1 = 50/100% */
    uint8_t  dma_b_volume_code;
    bool     dma_a_enable_left, dma_a_enable_right;
    bool     dma_b_enable_left, dma_b_enable_right;

    /* SOUNDBIAS */
    uint16_t bias_level;            /* default 0x200 */
    uint8_t  bias_sampling_cycle;   /* 0..3 — 32k/65k/131k/262k DAC PWM */

    /* Edge-trigger latches.  Driver sets when m4a writes NRx4 with bit 7,
     * which corresponds to MO_VOL events in CgbSound: START / IEC / sustain
     * step / release-rate change.  NOT every tick.  Chip clears after
     * consuming via m4a_consume_writes.
     *
     * Production v2 timing now flows through the §6c sample-offset
     * `M4ARegWrite` event stream — these latches are a non-timing
     * mirror for legacy / debug consumers.  The legacy snapshot API
     * `hw_audio_render()` clears them as a no-render trigger-consume
     * step for any caller that hasn't migrated; production v2 routes
     * exclusively through `hw_audio_render_events()`. */
    bool     trigger_sq1, trigger_sq2, trigger_wave, trigger_noise;
} M4ARegisterFile;
```

**Phase preservation rules** (chip-internal, documented for the Layer 1 port):
- `sq*_freq` change without trigger: phase accumulator preserved (real GB).
- `wave_freq` change without trigger: phase preserved.
- `noise_clock_shift` / `noise_divisor_code` change without trigger: LFSR
  preserved; clock-divider state preserved (matches mGBA).
- `wave_ram[]` rewrite while `wave_dac_on=true`: undefined per real GB
  (1-cycle bus glitch).  Driver writes wave RAM only with `wave_dac_on=false`,
  matching m4a.c:986–994 — chip-side reads must be muted while DAC is off.

**Sweep ownership** (NR10): the chip owns sweep state.  The driver emits
NR10 writes when m4a writes sweep, and emits NR13/NR14 frequency writes
only when m4a actually changes frequency (note start, pitch bend, LFO
vibrato, etc.).  Absence of a frequency write means the chip-side sweep
ticker continues from its current sweep-modified frequency.

### 6b. `M4APcmRing` — PCM software mix output (ring buffer)

The contract is a **ring buffer**, not a per-call buffer.  Real m4a writes
slices of `pcmSamplesPerVBlank` samples per vblank into a fixed
`PCM_DMA_BUF_SIZE` ring (1584 bytes per side for Pokemon Emerald;
`m4a_internal.h:169`).  The DMA hardware (which the chip emulates) advances
its read cursor at `pcm_rate_hz`.  This struct mirrors that arrangement:

```c
/* PCM_DMA_BUF_SIZE for the fixed v2 m4a driver contract; see
 * pokeemerald-expansion include/gba/m4a_internal.h:169. */
#define M4A_PCM_DMA_BUF_SIZE 1584

typedef struct {
    int8_t   ring_a[M4A_PCM_DMA_BUF_SIZE];  /* DMA-channel-A side */
    int8_t   ring_b[M4A_PCM_DMA_BUF_SIZE];  /* DMA-channel-B side */
    /* Driver-owned write cursor: bytes written since init, used as
     * `write_cursor % M4A_PCM_DMA_BUF_SIZE` for the next write offset.
     * Driver advances by `pcmSamplesPerVBlank` per vblank tick. */
    uint64_t write_cursor;
    /* Sample rate at which the chip's read cursor advances.  Fixed by
     * the v2 m4a driver contract (Pokemon Emerald: 13379). */
    uint32_t pcm_rate_hz;
} M4APcmRing;
```

Naming: `ring_a` / `ring_b` mirror the *register* names (FIFO_A / FIFO_B).
Each is mono.  L/R routing comes from `dma_a_enable_left/right` and
`dma_b_enable_left/right` in the register file — the chip applies the
configured routing on every render.  Pokemon Emerald's default is A→R, B→L
(`SOUND_A_RIGHT_OUTPUT | SOUND_B_LEFT_OUTPUT`, m4a.c:352–354), but other
games configure differently and the chip honours whatever the register file
says.

**S&H state ownership:** the chip owns the cross-render-call accumulator
that bridges `pcm_rate_hz` to the internal mix rate.  This state is private
to `HwFifoDrain` (see `hw_audio_internal.h`) and survives `hw_audio_render`
boundaries — otherwise upsampled output glitches every host buffer.

### 6c. Contract evolution: ordered register-write events (Layer 1.5)

The snapshot in §6a is *provisional*.  mGBA's chip side is write-timing
sensitive: `GBAAudioSample()` runs before each register write, and writes
apply at their actual sample timestamps.  A snapshot collapses ordered
writes into a final state and loses information that matters for parity:

- Multiple triggers per render span collapse to one bool.
- Trigger position relative to a same-span freq/duty/envelope write is
  lost (matters for phase-reset semantics).
- Sweep + driver freq-write disambiguation is impossible from snapshot
  alone (chip can't tell "driver wrote new freq" from "driver echoed
  unchanged value while chip's sweep was running").
- NR30 DAC off→on transitions vs wave-RAM rewrites lose ordering.
- `CgbSound`'s actual register-write order (NR10 → NR11 → NRx2 → NR13 →
  NR14-trigger) collapses to "all written simultaneously."

**Long-term contract** (lands as Layer 1.5, between Layer 1 driver core
and Layer 1 PSG synth):

```c
typedef enum {
    M4A_REG_NR10, M4A_REG_NR11, M4A_REG_NR12, M4A_REG_NR13, M4A_REG_NR14,
    M4A_REG_NR21, M4A_REG_NR22, M4A_REG_NR23, M4A_REG_NR24,
    M4A_REG_NR30, M4A_REG_NR31, M4A_REG_NR32, M4A_REG_NR33, M4A_REG_NR34,
    M4A_REG_NR41, M4A_REG_NR42, M4A_REG_NR43, M4A_REG_NR44,
    M4A_REG_NR50, M4A_REG_NR51, M4A_REG_NR52,
    M4A_REG_SOUNDCNT_H, M4A_REG_SOUNDBIAS,
    M4A_REG_WAVE_RAM_BYTE,   /* one byte at (value >> 8) & 0xF, byte = value & 0xFF */
} M4ARegId;

typedef struct {
    uint32_t sample_offset;  /* 0..frames-1 within the current render span */
    M4ARegId reg;
    uint32_t value;          /* register payload.  For most registers this
                              * is the 8-bit NRxx value zero-extended.  For
                              * M4A_REG_WAVE_RAM_BYTE it's
                              *   (addr_in_wave_ram << 8) | byte_value
                              * — m4a rewrites wave RAM byte-by-byte (16
                              * events per full rewrite), matching the STMIA
                              * write order on real hardware.  Per-byte
                              * granularity lets the chip reproduce the
                              * wave-RAM-during-DAC-on bus glitch
                              * (irrelevant when m4a writes with NR30=0,
                              * relevant for ROMhacks that don't). */
} M4ARegWrite;

typedef struct {
    const M4ARegWrite *events;
    size_t             count;
} M4ARegWriteBatch;

const M4ARegWriteBatch *m4a_get_pending_writes(const M4ADriver*);
void                    m4a_consume_writes(M4ADriver*);  /* chip calls after render */
```

Driver emits writes in `CgbSound`'s actual order, with `sample_offset` set
to the sample-at-which the write would have hit on real hardware (within
a vblank, m4a writes hit at deterministic offsets we can compute from
CgbSound's instruction count).  Chip consumes the batch in order, segmenting
its render span at each `sample_offset` and applying the write at that
boundary, exactly as mGBA does with `GBAAudioSample()` + write.

**Snapshot relationship after Layer 1.5:** the snapshot is computed as a
side-effect of the event stream (final state after applying all events
emitted so far).  Read accessors from §7a (`m4a_get_register_file`) keep
working.  The chip's *internal* logic uses events; non-timing consumers
(UI, params) use the snapshot.

**Implementation status (2026-04-29):** §6c **landed at §12 step 3** —
the `M4ARegWrite` event stream is the authoritative driver→chip
contract.  Production v2 always routes through
`hw_audio_render_events()`.  The snapshot remains queryable via
`m4a_get_register_file()` for non-timing consumers, but chip-internal
logic is event-driven; using the snapshot for timing-sensitive
decisions defeats the whole point.

---

## 7. Module APIs

### 7a. `plugin/m4a/m4a_driver.h`

```c
typedef struct M4ADriver M4ADriver;        /* opaque */

M4ADriver *m4a_driver_create(float host_sample_rate);
void       m4a_driver_destroy(M4ADriver*);
void       m4a_driver_set_voicegroup(M4ADriver*, ToneData *vg);
void       m4a_driver_set_host_rate(M4ADriver*, float hz);

/* MIDI ingress (DAW-side). */
void m4a_note_on(M4ADriver*, int track, int key, int vel);
void m4a_note_off(M4ADriver*, int track, int key);
void m4a_cc(M4ADriver*, int track, int cc, int value);
void m4a_pitch_bend(M4ADriver*, int track, int value14);

/* Advance the driver's internal vblank clock by `host_frames` at the
 * configured host rate.  Internally fires SoundMain N times where
 * N = floor(elapsed / vblank_period); each firing runs:
 *   MPlayMain → TrkVolPitSet → ChnVolSetDirect/Cgb → CgbSound
 *   (updates internal NRxx mirror) → SoundMainRAM → SoundMainRAM_Reverb
 *   (writes pcmSamplesPerVBlank bytes into the ring at write_cursor).
 *
 * After return, the latest register snapshot and ring state are observable
 * via the accessors below.  Multiple host_frames calls between vblanks
 * are no-ops on register state but advance the host-side clock. */
void m4a_advance(M4ADriver*, int host_frames);

const M4ARegisterFile *m4a_get_register_file(const M4ADriver*);
const M4APcmRing      *m4a_get_pcm_ring(const M4ADriver*);
```

This signature shape resolves the vblank-vs-host-block cadence question:
the driver owns the clock, vblanks fire internally, the chip just polls.
Where real m4a uses `SOUND_INFO_PTR` globals because it runs in ROM with a
single shared instance, our driver passes state explicitly to support
multiple plugin instances.  Internal control flow (the `m4a_sound_main`
→ `m4a_mplay_main` → … chain that runs inside `m4a_advance`) mirrors real
m4a function-by-function.

### 7b. `plugin/hw_audio/hw_audio.h`

```c
typedef struct HwAudio HwAudio;            /* opaque */

HwAudio *hw_audio_create(float host_sample_rate);
void     hw_audio_destroy(HwAudio*);
void     hw_audio_set_host_rate(HwAudio*, float hz);

/* SCAFFOLD API — snapshot-driven.  Provisional; replaced at Layer 1.5.
 *
 * Separate L/R buffers match the existing m4a_engine_process() convention
 * across all four call sites; lets the v1/v2 #ifdef fork stay a one-line
 * swap with no interleave glue. */
void hw_audio_render(HwAudio*,
                     M4ARegisterFile *regs,
                     const M4APcmRing *pcm,
                     float *outL, float *outR, int frames);

/* LAYER 1.5 API — event-driven.  Authoritative interface from Layer 1.5
 * onwards.  Chip segments its `frames` render span at each event's
 * sample_offset, applies the write at that boundary, and renders each
 * segment with the resulting register state — exactly as mGBA does
 * with GBAAudioSample() + write.
 *
 *   `events->events` is in non-decreasing sample_offset order.
 *   `pcm`           is the driver's PCM ring as before.
 *   `outL` / `outR` separate stereo float buffers at host rate.
 *
 * The snapshot is computed as a side effect (final state after applying
 * all events ≤ now) and remains accessible via M4ARegisterFile read
 * accessors for non-timing consumers.  hw_audio MUST NOT use the
 * snapshot for timing-sensitive logic from this API onward. */
void hw_audio_render_events(HwAudio*,
                            const M4ARegWriteBatch *events,
                            const M4APcmRing *pcm,
                            float *outL, float *outR, int frames);
```

`regs` is mutable in the scaffold API because the chip consumes
edge-trigger latches by clearing them after render.  Non-chip consumers
must continue to use the const `m4a_get_register_file()` accessor.

`hw_audio_internal.h` declares opaque stage state.  PCM and PSG paths
each have multiple stages so the real GBA ring → DMA → FIFO → DAC
pipeline stays explicit:

```c
/* PSG path */
typedef struct HwPsgSynth   HwPsgSynth;    /* square/wave/noise generators
                                              with hardware quirks (NR32 quant,
                                              verified noise sampling behavior,
                                              wave declick on NR30 edges).
                                              Operates at SOUNDBIAS rate. */

/* PCM path */
typedef struct HwDmaToFifo  HwDmaToFifo;   /* drains M4APcmRing at PCM rate;
                                              writes 32-bit words into FIFO_A/B */
typedef struct HwFifoDrain  HwFifoDrain;   /* drains FIFO bytes at SOUNDBIAS
                                              cadence; sign-extends to s8;
                                              owns the cross-render-call S&H
                                              accumulator */

/* Mix + output */
typedef struct HwMixBus     HwMixBus;      /* SOUNDCNT_H/L routing + scaling +
                                              SOUNDBIAS bias add/clip.
                                              Operates at SOUNDBIAS rate. */
typedef struct HwResample   HwResample;    /* SOUNDBIAS rate →
                                              max(131072, quirk_rate) →
                                              polyphase to host */
```

**Two rates inside `hw_audio/`:**
- **Quirk rate** = SOUNDBIAS-derived (`0x200 >> bias_sampling_cycle`).
  All hardware-artifact stages (PSG synth, FIFO drain, mix bus) operate
  here.  Possible values: 32768 / 65536 / 131072 / 262144 Hz.
  For Pokemon Emerald (SOUNDBIAS=0): 32768 Hz.
- **Output mix rate** = `max(131072, quirk_rate)`.  Per-channel signals
  upsample to this rate purely so the polyphase resampler to host has
  clean integer relationships and never downsamples chip output before
  the resampler.  In practice: 131072 Hz for SOUNDBIAS modes 0/1/2;
  262144 Hz for SOUNDBIAS mode 3.  No artifacts produced at this rate;
  it's an upsample (or pass-through when quirk = output rate).

The `max()` rule matters because if quirk rate ever exceeds a fixed
131072 Hz output rate, the chip would have to downsample its quirk
output before the polyphase resampler — losing information and
introducing pre-output aliasing.  Pokemon Emerald never sets SOUNDBIAS=3,
but ROMhacks for cleaner PCM sometimes do, so the rule keeps general
parity intact.

**Two APIs, two roles** (as of 2026-04-30):

- `hw_audio_render_events()` — Layer 1.5 event-driven API; the
  authoritative interface used by all production v2 call sites.
  Audible end-to-end: PSG (square + wave + noise) and the PCM
  two-stage drain (HwDmaToFifo + HwFifoDrain) run at the chip-
  internal render rate (`max(131072, 32768 << sampling_cycle)`),
  then `hw_resample.{c,h}` polyphase-resamples to host with anti-
  alias band-limiting at host_rate/2.
- `hw_audio_render()` — legacy snapshot-driven API; retained only as
  a no-render trigger-consumption step (writes zeros to outL/outR;
  clears `trigger_sq1/sq2/wave/noise` when a mutable register file is
  provided).  No production caller routes through it.  Will be
  removed when the scaffold-era integration tests migrate.

---

### 7c. CLAP lifecycle and realtime integration

The scaffold may use simple globals for non-plugin test targets, but the
CLAP plugin path must not keep them.  When v2 enters `plugin/m4a_plugin.c`,
`M4ADriver` and `HwAudio` are owned per plugin instance in `M4APluginData`,
matching today's `M4AEngine` ownership model.

`plugin_process()` must preserve the existing sample-accurate event slicing:
process CLAP/MIDI/param events at their frame offsets, then call
`m4a_advance()` and `hw_audio_render()` / `hw_audio_render_events()` for
the segment before the next event.  Do not collapse a CLAP block into one
driver advance once events have occurred inside it.

All current plugin behavior must bridge into v2 before v2 becomes the
default: config/state loading, voicegroup assignment, program params,
sample swaps, master/song volume, reverb amount, max PCM channels,
transport tempo, all-notes-off/all-sound-off, GUI snapshots, and reload /
restart behavior.  The audio thread remains realtime-safe: no file IO,
allocation, locks, or unbounded event queue growth inside `plugin_process()`.

---

## 8. CMake wiring

Two independent flags so we can validate the driver and the chip in
isolation during the rewrite.  Original scaffold default:

```cmake
option(M4A_DRIVER_V2 "Use v2 m4a software driver" OFF)
option(HW_AUDIO_V2   "Use v2 hw_audio chip emulation" OFF)

if(M4A_DRIVER_V2)
    add_subdirectory(plugin/m4a)
endif()
if(HW_AUDIO_V2)
    add_subdirectory(plugin/hw_audio)
endif()
```

Next cutover direction: v1 is deprecated and should no longer be built
for normal product targets.  Full-v2 should become the normal build
path; v1 should survive only behind explicit comparison / bisect targets
until parity evidence is captured and reviewed.

Combinations:
- `ON / ON`: full v2 path through both modules; target normal product
  configuration after cutover.
- `OFF / OFF`: pure v1 — deprecated reference configuration only.
- `ON / OFF`: v2 driver feeds its register/ring output into a v1
  chip-side adapter (Layer 1 has a small shim that drives v1's
  `m4a_pcm_channel_render` / `m4a_cgb_channel_render` from the v2
  register snapshot).  Lets us A/B the driver port in isolation.
- `OFF / ON`: scripted unit tests feed canned `M4ARegisterFile` +
  `M4APcmRing` into `hw_audio_render` during the scaffold, and canned
  `M4ARegWriteBatch` events into `hw_audio_render_events` after Layer
  1.5.  Lets us PSG-test the chip in isolation.

```cmake
if(M4A_DRIVER_V2)
    foreach(tgt poryaaaa poryaaaa_test poryaaaa_render
                poryaaaa_unit_tests poryaaaa-standalone)
        target_link_libraries(${tgt} PRIVATE m4a_driver)
        target_compile_definitions(${tgt} PRIVATE M4A_DRIVER_V2=1)
    endforeach()
endif()
if(HW_AUDIO_V2)
    foreach(tgt poryaaaa poryaaaa_test poryaaaa_render
                poryaaaa_unit_tests poryaaaa-standalone)
        target_link_libraries(${tgt} PRIVATE hw_audio)
        target_compile_definitions(${tgt} PRIVATE HW_AUDIO_V2=1)
    endforeach()
endif()
```

`plugin/m4a/CMakeLists.txt`:

```cmake
add_library(m4a_driver STATIC
    m4a_driver.c
    m4a_freq.c
    m4a_track.c
    m4a_cgb.c
    m4a_pcm.c
    m4a_main.c
)
target_include_directories(m4a_driver PUBLIC . ..)
# Driver depends on the voicegroup *types* only (header).  It must NOT
# call into the loader's IO/parsing code; voicegroups arrive as
# already-populated ToneData* via m4a_driver_set_voicegroup.
target_link_libraries(m4a_driver PRIVATE voicegroup_loader)
```

`plugin/hw_audio/CMakeLists.txt`:

```cmake
add_library(hw_audio STATIC hw_audio.c)
target_include_directories(hw_audio PUBLIC . ../m4a ..)
```

---

## 9. Integration at call sites

Four entry points call `m4a_engine_process()`:

- `plugin/m4a_plugin.c:667` — CLAP `process`
- `test/test_wav_export.c:118–210` — headless export
- `cmd/poryaaaa_render.c:732` — CLI renderer
- `test/test_engine.c:380` — unit tests

Each gains a `#if defined(M4A_DRIVER_V2) && defined(HW_AUDIO_V2)` fork
**at the render call**.  Both modules' lifecycle is gated on its
respective flag alone (`M4A_DRIVER_V2` for `M4ADriver`,
`HW_AUDIO_V2` for `HwAudio`) so the four combinations from §8 each
produce a coherent build.

```c
#if defined(M4A_DRIVER_V2) && defined(HW_AUDIO_V2)
    m4a_advance(g_driver, nframes);
    hw_audio_render(g_hw,
                    m4a_get_register_file_mut(g_driver),
                    m4a_get_pcm_ring(g_driver),
                    outL, outR, nframes);
#else
    m4a_engine_process(engine, outL, outR, nframes);
#endif
```

**Ingress mirroring** (added 2026-04-28 per pushback v3 follow-up).
Every direct v1 engine ingress call site (`m4a_engine_set_voicegroup`, `note_on`,
`note_off`, `cc`, `pitch_bend`, `program_change`, `all_sound_off`,
`refresh_voices`, `set_song_volume`, `set_tempo_bpm`,
`set_xcmd_callback`) gains a sibling `#if defined(M4A_DRIVER_V2)`
mirror that forwards the same arguments into the v2 driver:

```c
m4a_engine_note_on(&data->engine, channel, key, vel);
#if defined(M4A_DRIVER_V2)
m4a_note_on(data->m4a_v2, channel, key, vel);
#endif
```

This way the v2 driver is the recipient of the direct engine ingress
surface regardless of which flag combination is set.  The remaining
non-engine ingress paths are tracked in §12a so Layer 1 work can land
into the v2 driver and immediately become testable through the existing
entry points without further plumbing.  During early scaffold builds the
driver absorbed these calls as no-ops; it now has partial behavior, so
new ingress mirrors must be backed by parity tests before being treated
as complete.  `OFF / OFF` behaviour is preserved only as a reference for
comparison tests; it should not remain the default product path.

For the scaffold, `g_driver` and `g_hw` are file-scope statics in the
non-plugin targets, lazily initialised by `v2_lazy_init()` (test_wav_export)
or eagerly at the engine_init sibling (poryaaaa_render).  In the CLAP
plugin, `M4ADriver*` and `HwAudio*` live in `M4APluginData` per
instance, created in `plugin_activate` and destroyed in
`plugin_deactivate` / `plugin_destroy`, matching the existing
`M4AEngine` ownership model.  Per-instance threading lands fully when
Layer 1 ports the real MIDI/state machinery.

---

## 10. Verification

Latest observed verification (2026-04-28):

```
cmake --build build --target poryaaaa_unit_tests
./build/poryaaaa_unit_tests                      # 86/86

cmake --build build-v2 --target poryaaaa_unit_tests
./build-v2/poryaaaa_unit_tests                   # 160/160

cmake --build build-driver-only --target poryaaaa_unit_tests
./build-driver-only/poryaaaa_unit_tests          # 160/160

cmake --build build-chip-only --target poryaaaa_unit_tests
./build-chip-only/poryaaaa_unit_tests            # 86/86

cmake --build build --target poryaaaa poryaaaa_render
cmake --build build-v2 --target poryaaaa poryaaaa_render
```

Historical scaffold acceptance commands:

1. **Default build unaffected**:
   ```
   rm -rf build && cmake -B build && cmake --build build --target poryaaaa poryaaaa_render poryaaaa_unit_tests
   ```
   succeeds; v1 path identical.

2. **v2 build compiles**:
   ```
   rm -rf build-v2 && cmake -B build-v2 -DM4A_DRIVER_V2=ON -DHW_AUDIO_V2=ON
   cmake --build build-v2 --target poryaaaa poryaaaa_render poryaaaa_unit_tests
   ```
   succeeds.

3. **v2 produced a valid silent WAV during the silence-only scaffold**:
   ```
   build-v2/poryaaaa_render <project> <vg> --midi <song.mid> --output /tmp/v2_silent.wav \
       --sample-rate 32768 --total-duration-seconds 5 --fadeout 0 --tail 0 --reverb 0
   ```
   WAV opened, had correct duration/channels, and all samples were 0.
   ⚠ **Outdated** — this stopped being meaningful at §12 step 2
   (driver writes non-zero PCM) and §12 step 4 (chip emits PSG audio).
   Real-song renders now produce non-zero output end-to-end; see
   §12 step 4/5/7 status entries for the current audibility evidence.

4. **Unit tests both paths**: v1 passes unchanged (86/86); v2 passes
   the full suite at all four flag combos (86 / 161 / 99 / 185 as of
   2026-04-29).  v2 tests assert trigger semantics, event-stream
   ordering / consumption / drop-counter, wave-RAM byte event order,
   song-volume rescale for CGB, PCM ring fill, PCM frequency scaling,
   CGB pan-mask routing, PCM CC7 refresh, MODT recomputation smoke,
   immediate all-sound-off, plus chip-only canned-event tests for
   square / wave / noise audibility, master-disable silencing, NR51
   pan routing, SOUNDCNT_H DMA routing, and noise-DAC-off silencing.

5. **Plug-in install smoke**: per
   `feedback_delete_clap_before_build.md`; default v1 build still
   audible in DAW.

---

## 11. Critical files

- **Modified**:
  - `CMakeLists.txt` (root) — option, conditional subdirs, conditional
    target_link_libraries / compile_definitions.
  - `plugin/m4a_plugin.c:667` — `#ifdef` fork.
  - `test/test_wav_export.c:118–210` — `#ifdef` fork.
  - `cmd/poryaaaa_render.c:732` — `#ifdef` fork.
  - `test/test_engine.c:380` — `#ifdef` fork (+ `#ifndef`-skip on
    audio-asserting tests during scaffold).
- **New**:
  - `plugin/m4a/CMakeLists.txt`
  - `plugin/m4a/m4a_register_file.h`
  - `plugin/m4a/m4a_pcm_ring.h`
  - `plugin/m4a/m4a_driver.h`
  - `plugin/m4a/m4a_driver.c`
  - `plugin/m4a/m4a_internal.h`
  - `plugin/m4a/m4a_main.c`
  - `plugin/m4a/m4a_track.c`
  - `plugin/m4a/m4a_cgb.c`
  - `plugin/m4a/m4a_pcm.c`
  - `plugin/m4a/m4a_freq.h`
  - `plugin/m4a/m4a_freq.c`
  - `plugin/hw_audio/CMakeLists.txt`
  - `plugin/hw_audio/hw_audio.h`
  - `plugin/hw_audio/hw_audio_internal.h`
  - `plugin/hw_audio/hw_audio.c`
- **Reused / not touched**:
  - `plugin/voicegroup/*` — driver depends via existing
    `voicegroup_loader` target.
  - `plugin/m4a_engine.{c,h}`, `plugin/m4a_channel.{c,h}`,
    `plugin/m4a_reverb.{c,h}`, `plugin/m4a_tables.{c,h}` — v1, untouched.

---

## 12. What comes next (after this scaffold)

Each pass keeps v1 working and ports only into v2.  Reverb and PCM mixer
mirror **vanilla Sappy m4a** (matching the existing v1
`plugin/m4a_reverb.c` separate-pass design), not pokeemerald-expansion's
HQ-Mixer where reverb is integrated into the mixer loop.

### 12a. Scaffold completion follow-ups and current open fixes

Completed or substantially progressed:

- `M4ADriver` and `HwAudio` are owned per `M4APluginData` in the CLAP
  path, rather than by scaffold globals.
- Driver/chip lifecycle is wired across default, full-v2, driver-only,
  and chip-only build combinations.
- CLAP/plugin state now pushes the main engine-level controls into v2:
  song volume, master volume, reverb amount, analog filter, max PCM
  channels, tempo, voicegroup, refresh voices, and all-sound-off.
- Production v2 routes through `hw_audio_render_events()` (Layer 1.5
  event-driven API; PSG + PCM render at the chip-internal rate
  driven by SOUNDBIAS sampling_cycle, then polyphase-resample to
  host).  The legacy `hw_audio_render()` snapshot-driven API is
  retained only as a no-render trigger-consumption path for non-
  migrated callers and is still exercised by trigger-consumption
  regression tests.
- `m4a_all_notes_off()` and `m4a_all_sound_off()` now touch PCM channels
  as well as CGB channels.
- `m4a_sound_main()` now calls `m4a_sound_main_ram()`, so DirectSound
  notes can populate the PCM ring in driver-v2 builds.
- CGB pan routing, DirectSound frequency scaling, active PCM refresh,
  MODT recomputation, event/snapshot trigger consumption, and wave-RAM
  byte-event ordering have focused tests and currently pass.
- Stale comments that described `M4ADriver` as a no-op/zero scaffold
  have been updated in the public headers.

Layer 1 status (2026-04-30):

- `MPlayMain` / `ply_*` song-walk is **deliberately out of scope**:
  the DAW is the song player, so direct MIDI/CC ingress is the
  authoritative driver entry; the real m4a player loop is not
  needed for poryaaaa's use case.
- LFO advancement landed (§12.1, see closed-during-rewrite list);
  four behavioural tests exercise vibrato / tremolo / autopan /
  LFODL reset paths.
- PCM mixer + reverb were audited against v1 (§12.2, see closed-
  during-rewrite list); one int8-clamp parity bug fixed.

Audible parity claims still depend on the three blocking gates
listed at the end of §12 ("Blocking gates before parity claims") —
§12.10b mGBA capture parity, the wave trigger / phase contract, and
PSG/DC/absolute-level parity.  PSG unipolar structural work and the
DirectSound PCM event/ring timing gate (a prior entry on this list)
are closed; level tuning still needs comparison-test evidence.

Historical scaffold items to continue watching:

- Keep CLAP parameter/state replay mirrored into `M4ADriver` as new
  state surfaces are added.  The current main controls are wired, but
  future GUI/state fields should not quietly remain v1-only.
- Treat the v1 `M4AEngine` activity in full-v2 builds as temporary
  shadow plumbing.  It is acceptable while GUI/state/params still depend
  on v1 data, but the long-term owner is `M4ADriver` + `HwAudio`.
- Clean up v2 lifecycle in partial-flag validation builds: destroy
  `M4ADriver` when only `M4A_DRIVER_V2` is enabled, destroy `HwAudio`
  when only `HW_AUDIO_V2` is enabled, and avoid leaving lazy test globals
  allocated at process exit.
- Keep the Layer 1 acceptance gate explicit: real entry points
  (`plugin_process`, `poryaaaa_render`, and tests) must feed v2 state,
  not just compile a ported internal routine.

1. **Complete `m4a/` driver core** — ✅ **Closed 2026-04-29.**
   `SoundMain`, `TrkVolPitSet`, `ChnVolSetCgb`, `CgbSound`, frequency
   tables, direct ingress, event-stream emission, trigger consumption,
   CGB pan routing, active PCM refresh, and the LFO advancement
   (`m4a_internal_lfo_tick` in m4a_track.c, fired from m4a_main.c's
   tempoC overflow loop with one tick per ~150 tempoI units) are all
   present.  MPlayMain's song-walk stays a deliberate no-op: the DAW
   is the song player, not poryaaaa, so MIDI ingress goes through
   `m4a_note_on` / `m4a_cc` / `m4a_pitch_bend` rather than ply_*
   bytecode.  `trigger_*` flags in `M4ARegisterFile` are set exactly
   when m4a's MO_VOL bit triggers an NRx4 rewrite (per
   m4a.c:1188–1208), not on every snapshot, and cleared when the
   event batch is consumed.  LFO test coverage:
   `test_v2_lfo_disabled_no_freq_drift`,
   `test_v2_lfo_vibrato_modulates_freq`,
   `test_v2_lfo_delay_holds_off`,
   `test_v2_lfo_lfodl_resets_running_modulation`.
2. **Harden `m4a/` PCM mixer + reverb** — ✅ **Closed 2026-04-29.**
   Initial `SoundMainRAM` → ring writer and `SoundMainRAM_Reverb` are
   present; DirectSound frequency scaling, active PCM refresh, and
   loop handling all match v1 (`render_channel` math is byte-for-byte
   equivalent at the per-sample level).  **Audit fixed one parity
   bug**: `reverbBufL/R` was `int16_t` and stored int16-saturated
   wet samples, but real m4a's reverb buffer IS the int8 FIFO buffer
   (`gPcmDmaBuffer`) — the wet writeback should be int8-clamped so
   future tap reads see the same values that would have been DMA'd.
   Heavy mixes pushing pcmMix outside [-128, 127] would feed back
   out-of-range values into subsequent reverb sums, drifting from
   v1 / real-hardware behaviour.  Fix: changed the type to `int8_t`
   and added an explicit int8 clamp on writeback inside
   `sound_main_ram_reverb`.  DMA A/B routing (driver writes
   `ring_a = right mix, ring_b = left mix` matching Pokemon Emerald
   m4a.c:352-354 boot setup) is documented + correct.  New
   regression test `test_v2_pcm_reverb_pipeline` exercises the full
   pipeline with reverb on vs off across ~22 vblanks (long enough
   for the +M4A_PCM_SAMPLES_PER_VBLANK "other" tap to wrap into
   previously-written ring positions and actually fire).
3. **Layer 1.5 — Event stream contract**: §6c `M4ARegWrite` queue
   with sample offsets implemented end-to-end.  Driver side: CGB write
   order, NR51 events, trigger consumption, byte-granular wave-RAM
   events.  Chip side: `hw_audio_render_events()` segments its render
   span at each event's `sample_offset` and applies the write to the
   PSG + PCM subsystems before rendering the next segment — exactly as
   mGBA does with `GBAAudioSample()` + write.  The driver's snapshot
   (`M4ARegisterFile`) is computed as a side effect for non-timing
   consumers; chip-internal logic is event-driven from this point
   onward.  ✅ **Closed** — production v2 routes through this API.
4. **`hw_audio/` PSG synth — square + wave** driven by the event stream
   from §6c.  Phase preservation rules from §6a.  Noise was deferred
   to step 7 pending the §12.6 verification gate (closed as a separate
   pass).
   ✅ **Closed (post-§12.9)**: synth + wave operate at the chip-internal
   render rate (`max(131072, 32768 << sampling_cycle)` per §7b),
   resampled to host via the polyphase filter from §12.9.
5. **`hw_audio/` PCM path split** — ✅ **Closed 2026-04-29.**
   `hw_pcm.c` now implements two distinct stages:
   - **HwDmaToFifo**: pulls bytes from `M4APcmRing` at `pcm_rate_hz`
     (≈13379 Hz for Pokemon Emerald).  The FIFO head byte
     (`held_pcm_a` / `held_pcm_b`) updates whenever the pcm-rate
     clock crosses an integer.
   - **HwFifoDrain**: snapshots the FIFO head byte at the SOUNDBIAS-
     derived quirk_rate (32k/65k/131k/262k Hz) into `held_quirk_a` /
     `held_quirk_b`.  Quirk_rate is pushed by HwAudio when SOUNDBIAS
     sampling_cycle changes between render calls.
   - Output at internal_rate is `held_quirk` sign-extended to ±~1.0,
     held across render samples within the same quirk-rate interval.
   For Pokemon Emerald defaults (pcm=13379, quirk=32768) the two
   stages collapse behaviourally to a single S&H — quirk cadence is
   far above pcm cadence.  For ROMhacks pushing pcm above quirk
   Nyquist, the quirk-rate S&H now correctly low-pass-filters the
   PCM at quirk/2, matching the real chip's DAC cadence.
   `test_chip_canned_pcm_two_stage_drain` exercises the new pipeline
   on the Pokemon Emerald default rates.
6. **Noise coalescing verification gate** (precondition for step 7):
   ✅ **Closed.**  mGBA's `_coalesceNoiseChannel` is only called in
   DMG/CGB modes (gb_audio.c:782); GBA mode uses the "current LFSR
   sample shifted" path with no averaging.  V1's coalesce match was
   against mGBA Qt's resampler-output, not native-rate noise.
   Decision documented in `noise_verification_gate.md`: drop coalesce,
   use mGBA's GBA-mode `(audio->ch4.sample << 3)` model in v2.
   Empirical mgba-headless bit-comparison deferred to a regression
   test post-step-7 (not blocking).
7. **`hw_audio/` PSG synth — noise** implementing the canonical
   "current LFSR sample shifted" model from step 6's gate.  Same
   event-stream / quirk-rate / upsample model as step 4.
   ✅ **Closed 2026-04-29.**  Mirrors mGBA `gb_audio.c:631-637`
   verbatim: `lsb = (lfsr ^ (lfsr>>1) ^ 1) & 1`, shift right, OR/AND
   coeff (0x4000 / 0x40); LFSR resets to 0 on NR44 trigger (NOT 0x7FFF
   — that's a fixed point of this polynomial; `noise_verification_gate.md`
   updated with the corrected formula).  No sub-sample averaging.
   Output is dipolar `lsb` × env_vol/15 × kChanScale.  Two new
   chip-only canned tests (audible + DAC-off-silences) validate.
   All 4 flag combos green at step-7-close: 86 / 161 / 99 / 185
   (host-rate placeholder synth at this snapshot).  Step 8/9 then
   moved synth and mix to the chip-internal rate + polyphase
   resampler; the noise channel rides that pipeline in current
   builds.
8. **`hw_audio/` mix bus + bias** — ✅ **Closed 2026-04-29 (math) /
   continued at step 9 (rate).**
   SOUNDCNT_H/L routing + scaling and SOUNDBIAS bias add/clip pipeline
   landed 2026-04-29 in `hw_mix.{h,c}`.  Mix bus subscribes to NR50/
   NR51, SOUNDCNT_H (PSG vol code, DMA A/B vol codes, DMA L/R routing),
   and SOUNDBIAS (bias_level + sampling_cycle).  hw_psg / hw_pcm now
   produce per-channel mono buffers; HwMixBus consumes those, applies
   the full SOUNDCNT_L/H pipeline, then runs the GBA's unsigned 10-bit
   DAC bias-add/clip (default bias 0x200 → symmetric ±1 clip; non-
   default offsets DC + clips asymmetrically).  Four chip-only canned-
   event tests cover DC offset, asymmetric clip, PSG vol code 25/50/
   100%, and DMA vol code 50/100% scaling.  Step 9 then moved synth +
   mix from host rate to the chip-internal render rate (131072 Hz);
   that part counts as step 9's contribution.  All four flag combos
   green: 86 / 161 / 113 / 199.
9. **`hw_audio/` resampler** — ✅ **Closed 2026-04-29.**
   Windowed-sinc polyphase resampler in `hw_resample.{h,c}` (TAPS=32,
   PHASES=64, Hann window, kernel cut at `min(internal,host)/2`,
   DC-gain-normalized).  Synth (`hw_psg.c`, `hw_pcm.c`) and mix bus
   (`hw_mix.c`) run at a chip-internal rate driven by SOUNDBIAS
   sampling_cycle: `max(131072, 32768 << sc)` (131072 Hz for
   sampling_cycle 0/1/2; 262144 Hz for sampling_cycle 3).  The
   resampler downsamples to host with anti-alias band-limiting at
   host_rate/2 and rebuilds its kernel + state when sampling_cycle
   changes.  Sample-clock accounting (`hw_audio.c`'s
   `inputs_for_total_outputs` formula) is cumulative — identical
   output regardless of how the caller chunks render windows
   (block-size invariant past startup warmup).
   Chip-only tests cover: anti-aliasing (a 26 kHz square attenuated
   to < 50% of a 1.3 kHz reference), block-size invariance (same
   audio rendered as one 4096-frame call vs 64/127/512/2048-frame
   chunks within 1e-4), DC streaming (constant DC across 200 ×
   73-frame chunks within 5e-5), and per-cadence sweep (audibility
   + comparable peaks across all four sampling_cycle values).
   All four flag combos green: 86 / 161 / 126 / 212.
   ⚠ **Known limitation**: mid-call SOUNDBIAS sampling_cycle changes
   are deferred to the next render boundary (snapshot at start-of-
   call).  In practice SOUNDBIAS is set at boot and not changed
   mid-stream, so this rarely matters; tests follow the setup-then-
   play pattern.
10. **Edge-case parity tests** (chip-only, before song A/B) —
    split into two sub-gates:
    
    **10a. Self-consistency tests** — ✅ **Closed 2026-04-29.**
    - PSG audibility + comparable peak amplitude across all four
      SOUNDBIAS sampling_cycle values (0/1/2/3 → quirk 32k/65k/131k/
      262k Hz; internal rate pins to 131072 for 0/1/2 and bumps to
      262144 for 3).  Setup-then-play render-call pattern documented.
    - Direct `hw_audio_internal_rate()` switching assertion: explicit
      proof that the variable-internal-rate machinery actually fires
      when SOUNDBIAS sampling_cycle changes (a fixed-rate
      implementation that ignored sampling_cycle would fail this; the
      level-comparison test alone WOULDN'T catch that, since at low
      audio frequencies host-rate output is rate-invariant).
    - Block-size invariance: same audio rendered as one 4096-frame
      call vs 64/127/512/2048-frame chunks produces identical output
      past warmup (within 1e-4).
    - DC streaming preservation: constant bias across 200 small
      chunks holds steady within 5e-5.
    - Anti-alias quality: a 26 kHz square (above host Nyquist) is
      attenuated by the polyphase filter to < 50% of a low-frequency
      reference.
    - Pan routing, master-disable, DAC-off, SOUNDBIAS asymmetric clip,
      PSG vol code 25/50/100%, DMA vol code 50/100% — all covered by
      the `test_chip_canned_*` suite under HW_AUDIO_V2.
    
    **10b. mGBA capture-comparison parity** — ⚠ **OPEN.**
    The original §12.10 spec called for chip-only canned outputs
    compared to **mGBA reference captures** at matching SOUNDBIAS
    rates — that's the actual parity gate.  Self-consistency tests
    (10a) prove the chip's pipeline is internally coherent and
    plumbed correctly, but do NOT prove it matches mGBA / real
    hardware.  Closing 10b requires:
    - mGBA captures of canned register sequences at SOUNDBIAS=0/1/2/3,
      cross-rendered through the v2 chip with identical event timing.
    - Tolerance bounds on RMS / peak / spectral envelope vs the
      mGBA reference.
    - A regression harness that runs the comparison as part of CI.
    
    **DirectSound PCM event/ring timing + chunk-size invariance —
    ✅ Closed 2026-04-30.**  `m4a_sound_main_ram` now emits one
    `M4A_REG_PCM_PUBLISH` event per vblank, stamped with the firing
    sample_offset.  HwPcm advances `pcm_published_through` by
    `M4A_PCM_SAMPLES_PER_VBLANK` per event; reads from
    `M4APcmRing` are clamped to the published range.  When `pcm_pos`
    overruns the publish gate (FIFO underrun — the next vblank's
    PUBLISH hasn't fired yet), `pcm_pos` pauses and the held byte
    persists, mirroring real-hardware DAC-on-stalled-DMA behaviour.
    Two regressions cover the contract:
    `test_v2_pcm_chunk_size_invariance` (same audio rendered as one
    big call vs 2048-frame chunks vs 64-frame chunks → bit-identical
    PCM output, max diff < 1e-4) and `test_v2_pcm_publish_timing`
    (pre-vblank frames don't see post-vblank ring data).
    Driver timing constants are fixed for this rewrite, so canonical
    44.1 kHz offset/count assertions are deliberate contract tests, not
    loose property tests.
    Side-fix: `vblank_accum` promoted from `float` to `double` so
    cumulative-add error stays bounded across long runs with many
    small `m4a_advance` calls.  A canned-mode fallback in
    `hw_audio_render_events` snaps `pcm_published_through` to
    `ring->write_cursor` when the chip has never seen a PUBLISH
    event AND the current batch has none — keeps chip-only canned
    tests working without forcing them through the driver.
    
    Out-of-scope for the v2 chip-side stack as currently delivered:
    - Mid-call SOUNDBIAS sampling_cycle change handling — see
      "boot-time-only target restriction" below.
    
    **Boot-time-only SOUNDBIAS target restriction.**  The chip syncs
    sampling_cycle at the START of each render call (snapshot).
    Mid-call sampling_cycle changes are deferred to the next render
    boundary by design — Pokemon Emerald and ROMhacks both configure
    SOUNDBIAS at boot and never change it during playback, so this
    restriction has no observable effect for the target catalogue.
    A behavioural test (`test_chip_canned_soundbias_internal_rate_
    switches`) explicitly asserts this: an in-call SOUNDBIAS event
    does NOT change `internal_rate` until the next call.  This is a
    chosen scope reduction, not a closed parity gate — if future ROM
    hacks demand mid-call SOUNDBIAS, the resampler + cumulative
    trackers will need a flush-and-rebuild path mid-call.
11. **Comparison test infrastructure** — ⚠ **NEXT.**
    Build the harness before further parity tuning.  It should:
    - Render stable v2 captures through `poryaaaa_render` / full-v2
      builds, including per-channel solo output.
    - Render or load reference captures from patched mGBA and, while it
      still exists, deprecated v1 reference builds.
    - Align captures deterministically and report raw + AC metrics:
      RMS, peak, DC, min/max, spectral envelope, and channel balance.
    - Store tolerance policy per comparison type instead of baking one
      global number into every test.
    - Skip gracefully when external ROM / decomp / mGBA assets are not
      available.
12. **Parity work driven by comparison results.**
    Use the §12.11 harness to close §12.10b, wave trigger / phase, and
    PSG/DC/absolute-level gates.  Re-enable any `#ifndef`-skipped tests
    only when the comparison data says the expected tolerance is real.
    Keep subjective whole-song A/B as the final smoke check, not the
    first source of truth.
13. **V2 cutover and v1 removal.**
    V1 is deprecated and should no longer be built for normal products.
    Once comparison tests exist and the remaining parity gates are
    closed or explicitly waived:
    - Make full-v2 the normal target configuration.
    - Remove unconditional `M4AEngine` ownership from plugin / renderer /
      test plumbing.
    - Stop compiling v1 `ENGINE_SOURCES` into normal targets.
    - Remove ingress mirroring that exists only to keep v1 and v2 in
      parallel.
    - Delete or quarantine v1 files after the build no longer depends on
      them.

---

### Blocking gates before parity claims

The current v2 build is **audible end-to-end** under all four flag
combos but is **not** a parity-faithful chip model.  The following
gates must close before any spectral, level, or per-channel parity
claim against v1 / mGBA / real hardware is valid.  Anything claiming
"v2 reaches parity" before these close is wrong and the project
documentation should explicitly say so.

| Gate | What it unblocks | Why it's blocking | Closing artefacts |
|---|---|---|---|
| **§12.10b mGBA capture parity** — chip-only canned outputs compared to mGBA reference captures at SOUNDBIAS=0/1/2/3 | Empirical confirmation that the v2 chip matches mGBA on identical canned event sequences | §12.10a self-consistency tests landed but they only prove the v2 chip's pipeline is internally coherent — they do NOT compare to a reference. | Reference captures via `tools/captures/mgba-headless-channel-mute/` (patched mGBA headless binary; `--audio-out FILE` for native WAV, `--solo LIST` / `--mute LIST` for discrete channel isolation, `--hold-a-frames` / `--tap-a-frames` for menu state setup).  v2-side captures via `poryaaaa_render --solo <name>` (HW_AUDIO_V2 builds; channel-name parity with the mGBA tool: full / psg / directsound / ch1\|sq1 / ch2\|sq2 / wave / noise / fifo-a\|dma-a / fifo-b\|dma-b).  Plus a cross-render comparator harness, tolerance bounds on RMS / peak / spectral envelope, CI regression. |
| **Wave trigger / phase contract** | A documented, tested agreement between `hw_psg.{h,c}`, `m4a_cgb.c`, and the audio reference on whether a real fresh-note NR34 trigger should reset chip `wave_phase` (and likewise the noise LFSR on NR44) | Today's `freshStart` fix means MO_VOL events no longer re-trigger, so the contract only fires on note-start.  But on a legitimate note-start trigger the chip resets `wave_phase=0` (matches GBATEK / mGBA gb_audio.c) while v1 preserves phase — that divergence is unresolved against v1.  Reference target is mGBA / hardware (per `project_audio_reference_target.md`), so v2's current chip behaviour is correct; the work is verifying it against captures and adding a regression. | Use the §12.10b mGBA capture infrastructure to capture wave output through the v2 chip on a sustained-wave-with-MO_VOL sequence; assert phase coherence + match vs mGBA; add a chip-side regression that pins the contract. |
| **PSG/DC/absolute-level parity** — structural unipolar work landed; level tuning remains open | Sample-domain raw / DC / RMS parity (not just AC-normalized) between poryaaaa per-channel + full-mix WAVs and mGBA captures. | **Closed:** `hw_psg.c` now synthesises unipolar at the synth boundary per mGBA `GBAudioSamplePSG`, `hw_mix.c` mirrors mGBA `_applyBias`, and DirectSound int8 DC follow-up is closed in the 2026-05-01 capture pass.  **Open:** absolute level tuning remains blocked on cleaner reference captures because the current savestate captures include gameplay SFX and mGBA headless `--audio-out` collapses to mono regardless of stereo panning. | First land §12.11 comparison tests so every level/DC change is measured through the same pipeline.  Then tune remaining PSG / mix scale issues against clean captures and pin final raw + AC metrics within tolerance. |

When you hear "v2 is audible" — that's true today.  When you hear "v2
matches mGBA / v1" — that's not true until the gates listed above close.

Reviewers / auditors: if a code comment, doc, or memory entry implies
parity status before the gates close, treat it as a stale comment and
flag it for correction.

---
