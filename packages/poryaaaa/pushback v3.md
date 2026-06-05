# Pushback v3: Closing the Loop

In response to your second-round pushback (after `pushback v2.md`).
All seven follow-up items accepted; plan file (`HW_AUDIO_SCAFFOLD_PLAN.md`)
updated.  Specifics below — short because there's no architectural
disagreement left, just cleanup.

---

## 1. Driver "owns length decrement, sweep" was leftover wording — fixed

You're right.  The §6a ownership rule was pre-event-stream language I
forgot to rewrite when I made events authoritative.  Real model:

- **Driver owns register *writes***: m4a's `CgbSound`, `ChnVolSetCgb`
  etc. produce write events to NRxx fields.
- **Chip owns *advancement* of all hardware-ticked counters**: sweep
  (NR10, 128 Hz pulse), length (NRx1 low bits, 256 Hz pulse when
  length-enable set), hardware envelope pace counter (NRx2; m4a writes
  pace=0 to disable so it never counts down, but the *counter* still
  belongs to the chip's frame sequencer), LFSR (noise), phase
  accumulators.
- **Envelope volume is the exception**: m4a software ticks it (not the
  hardware envelope counter), so volume is driver-ticked and emitted as
  NRx2 register events; the chip's hardware envelope counter is
  chip-owned-but-disabled.

Updated text in §6a "Ownership rule" paragraph.  Sweep-ownership
paragraph at the end of §6a stays — it's now consistent with the
broader rule rather than a "special case."

## 2. Soft "if measured" wording on triggers — struck

§6a `trigger_*` comment block had "If Layer 1 PSG parity tests show
audible phase noise from this, extend the contract with a per-trigger
sample offset; defer until measured."  After §14 made events a hard
precondition for PSG parity, the hedge was incoherent.

New wording: "Snapshot-boundary semantics for v2-scaffold ONLY … Replaced
unconditionally by the sample-offset-bearing M4ARegWrite events in §6c
at Layer 1.5, which is a hard precondition for any PSG parity claim."
No "if measured" left.

## 3. Future chip-side API was missing — added

§7b now sketches both:

```c
/* SCAFFOLD API — snapshot-driven.  Provisional; replaced at Layer 1.5. */
void hw_audio_render(HwAudio*,
                     const M4ARegisterFile *regs,
                     const M4APcmRing *pcm,
                     float *out_lr, int frames);

/* LAYER 1.5 API — event-driven.  Authoritative interface from Layer
 * 1.5 onwards.  Chip segments its `frames` render span at each
 * event's sample_offset, applies the write at that boundary, and
 * renders each segment with the resulting register state — exactly
 * as mGBA does with GBAAudioSample() + write. */
void hw_audio_render_events(HwAudio*,
                            const M4ARegWriteBatch *events,
                            const M4APcmRing *pcm,
                            float *out_lr, int frames);
```

Note added that hw_audio MUST NOT use the snapshot for timing-sensitive
logic from this API onwards — it's strictly a non-authoritative read
accessor for non-timing consumers (UI, params, debug).

## 4. Wave RAM payload doesn't fit `uint32_t` — switched to byte-granular events

You're right that `value` as `uint32_t` can't hold 16 wave-RAM bytes.
Two valid shapes; I picked byte-granular because it matches m4a's STMIA
write order and gives the chip per-byte visibility for the
wave-RAM-during-DAC-on glitch (irrelevant when m4a writes with NR30=0,
relevant for ROMhacks that don't).  Putting all 16 bytes in one event
would defeat the point of the event model.

New shape:

```c
M4A_REG_WAVE_RAM_BYTE,   /* one byte at (value >> 8) & 0xF, byte = value & 0xFF */
```

A full wave-RAM rewrite emits 16 events with consecutive sample offsets,
matching the real STMIA write sequence.

## 5. "Happen to match" was overreach — acknowledged

Fair.  Pokemon Emerald's snapshot semantics are *close enough for
audibility* — not strictly equivalent to event semantics.  Within-vblank
ordering still differs; Emerald just happens not to produce audible
artefacts because its register-write patterns don't depend on
sub-vblank ordering.

This wording lives in `pushback v2.md`, which I'm leaving as historical
record (we're treating v1 → v2 → v3 as a correspondence trail).  Future
documentation should say "snapshot is sufficient for Emerald audibility,
not for general parity."  I won't claim equivalence again.

## 6. Verification gate ordering — fixed by splitting square+wave from noise

§12 step 4 was "PSG synth (square/wave/noise)" — putting noise *before*
the verification gate at step 6.  Reordered:

- Step 4: PSG synth — **square + wave** only.  Noise deferred to step 7.
- Step 6: Noise coalescing verification gate (precondition for step 7).
  Decision documented in writing before step 7 begins.
- Step 7: PSG synth — **noise** implementing the model chosen by step 6.

Steps 8 onwards renumbered accordingly.

## 7. 262144 Hz mode breaks fixed 131072 output — switched to `max(131072, quirk_rate)`

You're right that fixing output mix at 131072 Hz inverts the rate
hierarchy when SOUNDBIAS=3 (262144 Hz quirk rate).  Chip would have to
downsample from 262144 to 131072 before the polyphase resampler — losing
information and pre-output aliasing.

Fix: `output_mix_rate = max(131072, quirk_rate)`.  In practice:

| SOUNDBIAS | Quirk rate | Output mix rate |
|---|---|---|
| 0 | 32768 | 131072 (4× upsample) |
| 1 | 65536 | 131072 (2× upsample) |
| 2 | 131072 | 131072 (pass-through) |
| 3 | 262144 | 262144 (pass-through) |

Polyphase resampler adapts to the actual output mix rate.  Pokemon
Emerald never sets SOUNDBIAS=3, but the long-term general-parity claim
demands it works.

Updated in both §2h and §7b.

---

## Bottom line

| Item | Status |
|---|---|
| 1 — driver ownership leftover wording | Fixed (§6a ownership rule rewritten) |
| 2 — soft trigger language | Struck (§6a trigger comment) |
| 3 — chip-side post-1.5 API | Sketched (§7b, both APIs side-by-side) |
| 4 — wave RAM payload | Byte-granular events (§6c sketch updated) |
| 5 — "happen to match" overreach | Acknowledged here; no future claims of equivalence |
| 6 — verification gate ordering | Fixed (§12.4 split, noise → §12.7) |
| 7 — 262144 Hz output rate | `max(131072, quirk_rate)` (§2h, §7b) |

Plan should be ready for the next pass.  Holler if I missed a corner.
