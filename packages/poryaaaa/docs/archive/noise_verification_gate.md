# §12.6 Noise Coalescing Verification Gate

**Status: closed.**  Decision: **drop v1's `_coalesceNoiseChannel` clone.
Use mGBA's GBA-mode "current LFSR sample shifted" path for v2.**

This is the gate plan §12 step 6 calls for; it must close in writing
before step 7 (noise channel implementation in `hw_audio/`) begins.

---

## The question

V1's `plugin/m4a_channel.c:534` ports mGBA's `_coalesceNoiseChannel`:
when the noise LFSR is clocked more than once per output sample, v1
averages the clocks instead of emitting only the latest bit.  Memory
`project_cgb_channel_mismatch.md` records that v1's coalesce matches
mGBA captures within 1.5 dB at 4–16 kHz.

But mGBA's noise has two paths — `_coalesceNoiseChannel` (averaging)
for DMG/CGB modes, and a separate "current sample shifted" path for
GBA mode.  Question: which path does mGBA actually take in GBA mode,
and was v1's 1.5 dB match against that path or against mGBA's
*resampler-output* averaging?

If v1's match was against the resampler's averaging, the coalesce is
wrong for native-rate parity — the chip-side synth should produce raw
shifted bits and rely on the output resampler for band-limiting.

---

## Methodology (verification plan)

How to confirm empirically:

1. Capture mGBA noise at **native 32768 Hz** (bypassing the sinc
   resampler).  Tool: `mgba-headless --audio-out` — see memory
   `reference_mgba_headless_audio_out.md`.  Run a noise-only test
   ROM with deterministic NRxx state.
2. Render the same NRxx state through v1's `_coalesceNoiseChannel`
   path at 32768 Hz output rate.
3. Align by sample (LFSR-trigger zero-cross) and compare bit-for-bit.
4. Also run mGBA Qt at 44100 Hz (with its sinc resampler engaged),
   capture, and compare to v1's 44100-Hz output — that's what v1's
   1.5 dB match was originally measured against.

Expected results under each hypothesis:

| Hypothesis | mGBA native vs v1 coalesce | mGBA Qt vs v1 coalesce |
|---|---|---|
| H1: coalesce IS what GBA mode does | matches (±LSB) | matches (±LSB) |
| H2: coalesce matches *resampler* effect | **diverges** | matches |

---

## Code-level analysis (settles the question without empirics)

The mGBA source is unambiguous.  In `gb_audio.c`:

```c
// gb_audio.c:782
if (!audio->forceDisableCh[3]) {
    int16_t sample = audio->style == GB_AUDIO_GBA
        ? (audio->ch4.sample << 3)              // <-- GBA mode
        : _coalesceNoiseChannel(&audio->ch4);   // DMG / CGB mode only
    if (audio->ch4Left)  sampleLeft  += sample;
    if (audio->ch4Right) sampleRight += sample;
}
```

`_coalesceNoiseChannel` (gb_audio.c:920) is **only called in DMG/CGB
mode**.  In GBA mode, mGBA reads `audio->ch4.sample` — the *latest*
LFSR-bit-shift × envelope volume — and shifts it left by 3 (the same
3-bit shift applied to channels 1/2/3; gb_audio.c:778-779).  No
averaging.

`audio->ch4.sample` is updated in the noise-cycle loop (gb_audio.c:641):
```c
audio->ch4.sample = lsb * audio->ch4.envelope.currentVolume;
```
where `lsb` is the *most recent* LFSR bit emitted in the loop.
`nSamples` / `samples` accumulators are tracked but never read in the
GBA path — they exist only for `_coalesceNoiseChannel`'s use in DMG/CGB.

This is **hypothesis H2 confirmed structurally**: mGBA GBA mode does
not coalesce.  V1's coalesce produces a result that approximates the
mGBA Qt **resampler output**, not mGBA's native-rate noise.

---

## Why v1 sounds "close enough"

The 1.5 dB match recorded in `project_cgb_channel_mismatch.md` came
from comparing v1 vs mGBA Qt — both at 44100 Hz host rate, both with
some form of LF-band-limiting applied.  V1 explicitly averages
sub-sample LFSR clocks to band-limit; mGBA Qt produces raw shifted
bits at 32768 Hz internal, then sinc-resamples to 44100 Hz, which
*also* acts as a low-pass filter.  Two different mechanisms, similar
LF spectrum.

Above 16 kHz the two diverge — mGBA's resampler has a sharper
cutoff, while v1's per-sample averaging produces a different rolloff
shape.  But the test's frequency band stopped at 16 kHz.

Net result: v1's coalesce was matching the **resampler's averaging
effect, not mGBA's hardware-level noise**.  Per plan §6c the chip
operates at SOUNDBIAS quirk rate (32k/65k/131k/262k Hz) and the
proper polyphase resampler at step 9 produces the host-rate output.
Adding coalesce inside the noise synth would be a *second* low-pass
on top of step 9's resampler — over-band-limited.

---

## Decision

**Drop v1's coalesce.  Step 7 implements the canonical
"current LFSR sample shifted" model**:

```c
// Step 7 noise synth, mirrors mGBA gb_audio.c:782 (GBA mode):
//   1. Per chip-rate sample (SOUNDBIAS quirk rate), advance LFSR by
//      however many noise-timer periods elapsed since last sample.
//   2. ch4.sample = (latest_lfsr_bit ? +1 : 0) × envelope_volume
//   3. Output contribution = ch4.sample << 3
//   4. NO sub-sample averaging.  The output resampler (step 9) produces
//      the host-rate band-limited signal.
```

The 7-bit-vs-15-bit width selection (NR43 bit 3) and the timer period
math (clock_shift << 4 | divisor_code) are hardware concerns
unchanged by this gate.

### Step 7 implementation notes

**As-implemented (2026-04-29).**  Mirrors mGBA `gb_audio.c:631-637`
verbatim — the canonical reference is the source, not summary text.

- LFSR step (per clock):
  ```c
  uint16_t lsb = (uint16_t)((lfsr ^ (lfsr >> 1) ^ 1u) & 1u);
  lfsr >>= 1;
  uint16_t coeff = width_7bit ? 0x0040u : 0x4000u;
  if (lsb) lfsr |= coeff;
  else     lfsr &= ~coeff;
  ```
  The feedback `lsb` IS the channel sample (`audio->ch4.sample = lsb *
  envelope.currentVolume`, gb_audio.c:641).  The post-shift `lfsr & 1`
  bit is *not* the output — that's a different value than the feedback
  bit.  Match mGBA: output the feedback bit.
- LFSR initial state on NR44 trigger: **0** (mirrors mGBA
  gb_audio.c:374 `audio->ch4.lfsr = 0;`).  **Not** 0x7FFF — under this
  polynomial the all-ones state is a fixed point (verified by trace and
  by failing unit test before the fix).  v1 uses 0x7FFF because v1's
  polynomial is the un-inverted variant `((lfsr >> 1) ^ lfsr) & 1`,
  which has 0x0000 as the lockup state instead.  Two valid polynomials,
  different lockup states, statistically equivalent noise.
- Per mGBA, sample in GBA mode is shifted left by 3 in the mix.  Our
  v2 PSG output uses normalized float [-1,1]; the equivalent is to
  treat `lsb` as dipolar (lsb=1 → +1, lsb=0 → -1) and apply
  `kChanScale = 0.25` × `env_vol/15`.  The dipolar mapping (vs mGBA's
  unipolar 0..env_vol) trades a constant DC offset for symmetric
  headroom; SOUNDBIAS at step 8 will normalize the DC anyway.
- `noise_last_sample` field holds `lsb` across output frames that don't
  step the LFSR (when `clocks_per_sample < 1.0`), so the channel keeps
  emitting its current bit between rare clocks.

### Empirical follow-up (deferred, not blocking step 7)

The verification plan above (mgba-headless capture + bit-compare)
remains useful as a regression test once step 7 lands.  Defer to a
post-step-7 task: `noise_native_parity_test.md`.  The structural
finding above is sufficient to unblock step 7.

---

## Cross-references

- v1 coalesce: `plugin/m4a_channel.c:534-577`
- mGBA noise update: `gb_audio.c:585-647`
- mGBA noise output: `gb_audio.c:743-794` (GBAudioSamplePSG)
- mGBA coalesce (DMG/CGB only): `gb_audio.c:920-929`
- Plan: `HW_AUDIO_SCAFFOLD_PLAN.md` §12.6
- Memory: `reference_mgba_headless_audio_out.md`,
  `project_cgb_channel_mismatch.md`
