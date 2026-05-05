# Thoughts and Concerns

Red-team review of `HW_AUDIO_SCAFFOLD_PLAN.md` by GBA-audio expert subagent.

## Sources grounding the review
- `HW_AUDIO_SCAFFOLD_PLAN.md` (the plan)
- `NEXT_SESSION.md` (prior context)
- `plugin/m4a_engine.c`, `plugin/m4a_channel.c` (poryaaaa v1)
- `pokeemerald-expansion/src/m4a.c` (esp. `CgbSound` 907–1213, `SoundInit` 335–380, `SampleFreqSet` 382–412)
- `pokeemerald-expansion/src/m4a_1.s` (`SoundMain` 20–86, `SoundMainRAM` 160–1342)
- `pokeemerald-expansion/include/gba/m4a_internal.h` (struct layouts, `PCM_DMA_BUF_SIZE = 1584`)
- `pokeemerald-expansion/src/m4a_tables.c:102-116` (`gPcmSamplesPerVBlankTable`)
- `/tmp/mgba-ref/gba_audio.c`, `gb_audio.c` (chip side)

**Caveat:** pokeemerald-expansion uses ipatix HQ-Mixer rev 4.0, which differs
from vanilla Sappy m4a in some details (notably reverb integration). Called
out in finding 12; otherwise the review relies on parts that match across
both dialects (CgbSound, register write patterns, buffer sizes).

---

## Critical (would break parity if not addressed before Layer 1)

1. **PCM mix buffer size is wrong, and the layout is the wrong abstraction.**
   The plan says `int8_t fifo_a[1568]` + `int8_t fifo_b[1568]`. Real m4a uses
   `PCM_DMA_BUF_SIZE = 1584` (not 1568) and the layout is a single
   `s8 pcmBuffer[PCM_DMA_BUF_SIZE * 2]` where DMA1 reads the first 1584 bytes
   and DMA2 reads the next 1584 — see `m4a_internal.h:169` and `m4a.c:357–360`.
   The buffer is *not* per-vblank-resized; the driver writes a different *slice*
   of length `pcmSamplesPerVBlank` each tick and `pcmDmaCounter` cycles through
   `pcmDmaPeriod = PCM_DMA_BUF_SIZE / pcmSamplesPerVBlank` slices before
   wrapping. So 1584 is the ring-buffer span, not "one block" — `frames` in
   `M4APcmMixBuffer` is the slice length, and the chip needs to know which
   slice (offset) is fresh. The proposed struct collapses ring + slice into
   one shape and loses the `pcmDmaCounter` / `pcmDmaPeriod` state that real
   m4a uses to coordinate driver-write vs. chip-read positions.

2. **`m4a_sound_main` has no concept of vblank cadence vs host block cadence.**
   Real m4a runs `SoundMain` exactly once per vblank (≈59.7275 Hz), producing
   `pcmSamplesPerVBlank` PCM frames per call (224 at freq index 4 = 13379 Hz).
   The plan's signature calls `m4a_sound_main` once per host-audio block. At
   256 frames @ 48 kHz that block is 5.33 ms ≈ 0.319 vblanks; at 1024 frames
   it's 1.27 vblanks. There is no specification of "tick the driver once per
   integer number of vblanks elapsed" or "sub-tick if block straddles a vblank."
   Without it, every behaviour port that follows will pick a different cadence
   and have to be re-validated. The scaffold should either make the driver
   internally accumulate a vblank counter from a host-rate advance, or accept
   frames-since-last-call and emit (regs, pcm) only on vblank boundaries while
   caching the previous outputs in between. **Single biggest fidelity risk in
   the scaffold.**

3. **Decoded `_env_volume` field hides whether driver or chip ticks the
   envelope, and real m4a's behaviour is non-obvious.** Real m4a's `CgbSound`
   does software envelope arithmetic and writes the result into NRx2's high
   nibble (`*nrx2ptr = (envelopeStepTimeAndDir & envMask) + (channels->envelopeVolume << 4)`
   at m4a.c:1204). It also re-writes `*nrx4ptr = channels->n4 | 0x80` whenever
   MO_VOL is set (lines 1205, 1207), which **re-triggers** the channel — this
   is how the driver suppresses the hardware envelope: by re-arming with a
   fresh volume each tick. The hardware envelope's pace counter therefore
   never gets to count down because m4a both writes the volume and re-triggers.
   Implication: the chip MUST honour the trigger latch as "load NRx2 volume
   into internal envelope register, do nothing else" — if the chip implements
   a real hardware envelope ticker reading NRx2's pace nibble, even with pace
   nibble = 0, it must also model "writing NRx2 with pace=0 forces a DAC off/on
   cycle that resets phase" (Pan Docs / GB hardware spec). Plan doesn't pin
   this down. Add a comment to `M4ARegisterFile`: "envelope volume is the
   *current* hardware register volume and is fully driver-controlled; chip
   never advances it" and decide whether `trigger_*` bits also reset phase
   (real GB: yes for square; wave channel only resets phase if NR30 toggled
   off→on).

4. **NRx4 length-counter and NR52 master enable not represented; NR30 DAC
   enable missing.** Real m4a writes length explicitly via
   `*nrx1ptr = channels->length` and uses `n4` to hold the length-enable bit
   (`channels->n4 = 0x40` vs `0x00` at lines 1007–1010). Wave-channel DAC
   enable lives in NR30 bit 7 — `*nrx0ptr = 0x40` (DAC on) /
   `*nrx0ptr = 0` (DAC off) at lines 988/995. The plan's `wave_dac_on` is
   correct in spirit, but there is no length-counter field anywhere, and
   crucially no length-enable. m4a *does* use length on most note-offs because
   it's how the hardware actually mutes NR1/2/4 between declick-sensitive
   transitions. NR52 (master enable, written via
   `REG_SOUNDCNT_X = SOUND_MASTER_ENABLE | …` at m4a.c:264, 347) is also
   absent. If the chip doesn't honour NR52 the four PSG channels are always
   "on" from boot, wrong for the silence period at startup and at
   `m4aSoundClear`. Add `psg_master_enabled`, `sq1_length`, `sq2_length`,
   `wave_length`, `noise_length`, plus per-channel `length_enable`.

5. **Trigger-latch semantics are under-specified; "chip clears after
   consuming" is wrong for our cadence.** On real GBA the trigger is a *write
   event* — the chip-side latch happens at the bus cycle of the write, not in
   a frame loop. In our split, where the driver may emit a register snapshot
   once per vblank (or once per host block, see #2) and the chip may render
   N samples between snapshots, the chip needs to resolve "trigger occurred
   at *which* sample-instant within this snapshot's validity window" to pick
   the correct phase / lfsr-reset / declick-onset point. Plan as written
   gives the chip only "trigger happened sometime in the last block." Either
   (a) accept that triggers always land on snapshot boundaries (loses
   intra-vblank sub-sample timing m4a actually has, since `SoundMain` writes
   NRx4 inside the vblank window), or (b) extend the contract with a
   per-trigger sample-offset. Document the choice; otherwise Layer 1 PSG
   parity tests against mGBA will fail with phase noise that's hard to
   bisect.

6. **PSG sweep (NR10) is in the register file but its tick is unassigned.**
   Real GB hardware ticks the sweep counter at 128 Hz and adjusts NR13/NR14
   freq accordingly. m4a does write the sweep register (`*nrx0ptr = channels->sweep`
   at m4a.c:980, only on START), but does not subsequently rewrite freq —
   the GB hardware sweep is what advances the freq word after the trigger.
   So sweep is **hardware-ticked**, but the plan's contract has the driver
   writing `sq1_freq` every snapshot, which would clobber any chip-side
   sweep-driven freq advance. Either (a) the chip owns sweep state and ignores
   driver freq writes when sweep is active (risky; needs trigger detection),
   or (b) the chip exposes the sweep-modified freq back to the driver each
   snapshot (round-trip register file). Pokemon games rarely use sweep, but
   the m4a fidelity guideline ("if real m4a writes it, we honour it") demands
   the contract handle this.

## Important (must fix during the rewrite, not blockers for scaffolding)

7. **Wave declick is HW-only is debatable.** The wave-channel transitions in
   m4a *do* emit a specific NRx0 sequence — `*nrx0ptr = 0x40` (DAC on), wait,
   then `*nrx0ptr = 0` (DAC off). The DAC on/off sequence is what produces
   the "declick"; the *driver* is choosing the sequence, but the *audible
   artifact* is hardware (DAC level shift). If `hw_audio/` only sees
   `wave_dac_on` as a bool, it can replicate the click. The existing v1
   `m4a_channel.c:7-8,231-235,449-457` implements declick as an explicit
   sample fade in software, which is *not* what real hardware does. The plan
   correctly removes that. Verify chip-side declick is parameterised on the
   actual NR30 toggle event, not on note-off semantics — those are different
   things in m4a (note-off can happen 60+ frames before NR30 actually goes 0).
   m4a only writes NR30=0 on `oscillator_off` path at line 1037 via
   `CgbOscOff` (line 852).

8. **CGB env volume re-trigger every tick will cause phase reset on real
   hardware (and must in our chip).** Per #3, `*nrx4ptr = n4 | 0x80` gets
   written by m4a every tick that has MO_VOL set. m4a only sets MO_VOL (via
   `channels->modify |= 0x01`) on specific events: START, IEC, a release-rate
   change, sustain-step. On a quiet tick where envelope didn't change, MO_VOL
   is not set and NRx4 is not rewritten. The plan's `trigger_*` bool model
   captures "did m4a re-trigger this tick" only if the driver sets it exactly
   when m4a sets MO_VOL — make explicit in the driver-port. If the driver
   flips trigger every snapshot for convenience, square-wave phase will reset
   every vblank and duty-pattern timing will diverge from mGBA.

9. **`pcm_rate_hz` in `M4APcmMixBuffer` is fixed-per-snapshot but real m4a's
   PCM rate can change at runtime.** `m4aSoundMode(SOUND_MODE_FREQ)` calls
   `SampleFreqSet` (m4a.c:382) which rewrites `pcmFreq`,
   `pcmSamplesPerVBlank`, and the Timer 0 reload (line 401). Some
   pokeemerald romhacks toggle this for pokemon cries. Pokemon Emerald
   itself uses freq index 4 (13379 Hz) and never changes it, but make
   explicit in the contract how `pcm_rate_hz` changes propagate to the chip's
   S&H accumulator — a freq change mid-stream needs a phase-aligned reset
   to avoid a click.

10. **Stateless-register-file vs chip-internal-state boundary risks freq
    reload semantics.** Concrete cases the plan should commit on now:
    (a) on `sq1_freq` change without trigger: phase accumulator preserved or
    zeroed? Real GB: preserved. (b) on `wave_freq` change without trigger:
    preserved. (c) on `noise_clock_shift / divisor_code` change without
    trigger: LFSR state preserved, divider state unspecified (mGBA preserves;
    other cores reset). (d) on `wave_ram[]` rewrite while wave channel is
    on: the famous 1-cycle bus glitch; mGBA partially models it. m4a writes
    wave RAM only on START (m4a.c:986-994), with NR30=0 first to avoid the
    glitch — chip-side DAC-off must mute reads during wave-RAM-rewrite. Make
    explicit in `hw_audio_internal.h`.

11. **`int8_t fifo_a[N]`/`int8_t fifo_b[N]` is the right shape, but the plan
    claims it's "post-clamp post-reverb stereo."** Two mono channels, each
    routed to one DAC (FIFO_A → DMA1 → channel A; FIFO_B → DMA2 → channel B).
    In Pokemon Emerald config (`SOUND_A_RIGHT_OUTPUT | SOUND_B_LEFT_OUTPUT`,
    m4a.c:352-354), A is right and B is left. Real m4a's `SoundMainRAM`
    mixer writes left-mixed samples to `pcmBuffer[0..1583]` (DMA-init wires
    that to FIFO_B going to L) and right-mixed to `pcmBuffer[1584..3167]`
    (DMA to FIFO_A going to R). So the *name* `fifo_a` should hold the
    *right* side and `fifo_b` the *left* if matching real m4a's pcmBuffer
    layout. Or rename to `fifo_left` / `fifo_right` and let the chip handle
    the SOUNDCNT_H routing. Pick one, document it.

12. **Reverb is software, but `SoundMainRAM_Reverb` doesn't exist in
    pokeemerald-expansion's HQ-mixer.** The plan section 2d says
    "`SoundMainRAM_Reverb`" lives in m4a/. In *vanilla* pokeemerald (Sappy
    variants vary) the routine name and structure are different.
    pokeemerald-expansion uses HQ-Mixer rev 4.0 which has reverb integrated
    into the mixer loop (see `ENABLE_REVERB` at m4a_1.s:91). Vanilla
    pokeemerald uses Sappy's m4a where reverb is a separate routine that
    runs after `SoundMainRAM`. The plan's name is fine for vanilla, but the
    v1 code at `plugin/m4a_reverb.{c,h}` is already a separate-pass design
    matching vanilla Sappy. Decide which dialect the driver mirrors and pin
    in section 2b. Probably vanilla Sappy — but state explicitly so future
    contributors don't grep pokeemerald-expansion and get confused.

13. **CMake flag granularity: `M4A_ENGINE_V2` toggles both libraries together;
    you can't validate driver-only or chip-only.** Two known cases want finer
    control: (a) Layer 1 driver port lands first; want a build that uses v2
    driver but feeds v1 chip-side rendering for A/B (impossible without
    separate flags). (b) hw_audio/ unit tests ideally feed canned
    `M4ARegisterFile` snapshots without going through a driver — a
    `hw_audio_only` test target wants `hw_audio` linked but not the driver
    lib. Splitting into `M4A_DRIVER_V2` + `HW_AUDIO_V2` flags adds 5 lines
    of CMake and doubles the validation surface. Worth doing now while the
    scaffolding is empty; painful to retrofit once Layer 1 is mid-port.

14. **Test coverage: the plan disables audio-asserting unit tests under v2
    "during scaffold" with no recovery commitment.** Section 11 `#ifndef`-skips
    on audio-asserting tests is fine for the scaffold PR (v2 produces silence),
    but section 12 ("What comes next") doesn't list "re-enable skipped tests"
    as a numbered step. Risk: the tests stay disabled through Layers 1–7 and
    get forgotten. Add explicit bullet to section 12: "after Layer 2 (driver
    core + PCM mixer port), re-enable audio-asserting tests under both v1 and
    v2 builds; both must pass with matching output within ±X dB." Committing
    the threshold now (e.g. ±0.5 dB at 32768 Hz host) gives a bisect target.

## Minor / Stylistic

15. **`m4a_sound_main` taking out-args is fine, but lose the "real m4a doesn't
    take args" argument from the plan.** Real `SoundMain` writes globals
    because it's running in ROM with one shared `SOUND_INFO_PTR`. Our
    multiple-instances story (one driver per CLAP plugin instance) makes
    globals wrong. The plan's parameterisation is correct even though
    section 2b ("function-name fidelity") implies otherwise. Add a one-liner
    under 2b: "where real m4a uses `SOUND_INFO_PTR` globals, our driver
    passes the equivalent state explicitly to support multiple plugin
    instances; the public API shape may differ but the *internal* control
    flow mirrors real m4a."

16. **Voicegroup loader integration: real m4a treats voicegroups as ROM
    pointer arrays.** The v1 engine uses `ToneData *currentVoice` and
    `voiceGroup[program]` — a flat array indexed by program — which mirrors
    real m4a's `track->voicegroup[track->program]` access pattern. The
    loader's job is purely IO: parsing pokeemerald source to populate
    ToneData arrays in memory. Driver should depend on `voicegroup_types.h`
    for the struct shape and on the *result* of `voicegroup_loader` (a
    `ToneData*`), not on parsing/IO. Recommend explicit comment that driver
    must not call into the loader's parsing functions — only
    `voicegroup_types.h` is in its include path.

17. **131072 Hz internal mix rate "hidden inside hw_audio_render" is correct,
    but the plan never says how `pcm_rate_hz` (13379) bridges to that.**
    Layer 1 will need an S&H from 13379 → 131072 (factor of ~9.79). Not an
    integer ratio. The chip needs an accumulator; not visible in the API.
    Call out explicitly that the chip's S&H stage owns this state across
    `hw_audio_render` calls (otherwise across a host buffer boundary you get
    a phase glitch in the upsampled FIFO output).

18. **SOUNDBIAS sampling cycle: included in the register file, fine, but its
    semantics affect PCM not PSG.** `bias_sampling_cycle` controls the DAC
    PWM rate — 32768 / 65536 / 131072 / 262144 Hz (mGBA:
    `audio->sampleInterval = 0x200 >> resolution`, gba_audio.c:232). Real
    m4a *reads* this back in CgbSound type 3 fix-frequency adjustment
    (m4a.c:1171–1176) to round freq to the PWM rate's sub-multiple. So the
    chip-side must apply the same rounding the driver applies — or the
    driver does it once and the chip just consumes the rounded `wave_freq`.
    Plan as written: driver writes `wave_freq` (already rounded), chip
    consumes. That's fine — but make sure the driver port preserves m4a's
    read-modify-write of SOUNDBIAS_H (line 1171).

19. **NR50/NR51 are PSG-only; PCM has separate routing in SOUNDCNT_H.** Plan
    section 6a is correct but the comment "SOUNDCNT_L (NR50/NR51) — PSG-only
    mixer" is worth bolding in the actual header so future readers don't
    accidentally try to gate PCM through it. PCM left/right enable is
    `dma_a_enable_left/right` and `dma_b_enable_left/right` (SOUNDCNT_H bits
    9/10 and 13/14), which the plan has.

20. **`int8_t fifo_a[1568]` is statically sized — but PCM_DMA_BUF_SIZE is a
    per-game constant.** Vanilla pokeemerald uses 1584. Pokemon Emerald
    specifically uses 1584 with `pcmSamplesPerVBlank = 224`. The plan's 1568
    looks like a typo (224 × 7 = 1568 — perhaps maxLines × pcmSamplesPerVBlank
    came from some HQ-Mixer config). Pin the source: write the actual number
    with provenance (`/* PCM_DMA_BUF_SIZE for Pokemon Emerald m4a config; see
    m4a_internal.h:169 */`). If we intend to support other games, make it a
    compile-time parameter.
