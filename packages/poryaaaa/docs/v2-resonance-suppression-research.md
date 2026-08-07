# v2 Resonance Suppression Research

## Conclusion

**Fact.** The highest-priority explanation to test is the frontend mismatch: v2 uses the mGBA 0.10.5 blip frontend (`hw_resample.c:24-76`), while current mGBA uses a normalized 16-tap Nuttall-windowed sinc resampler. This is not evidence that a resonance exists or that the listening issue is proven.

Do not add a resonance filter before matched native-rate captures. First determine whether v2's source/mix matches mGBA. Migrate v2 to the current mGBA sinc only if the native-rate output matches and the remaining difference is the frontend. Reject dynamic resonance suppression as the initial implementation.

## Current v2 path

**Fact.** `plugin/hw_audio/hw_audio.c:64-82` selects `32768 << sampling_cycle` as the DAC cadence; `plugin/hw_audio/hw_audio.c:145-181` runs PSG → PCM → `hw_mix` → `hw_resample`. `plugin/hw_audio/hw_mix.c:59-70,91-118,120-158` applies gains, SOUNDBIAS bias/clipping, and normalization. `plugin/hw_audio/hw_resample.c:24-76` contains the old blip table; `plugin/hw_audio/hw_resample.c:97-138` performs int16 clamping, integration, and blip_buf's 511/512 DC-blocking pole; `plugin/hw_audio/hw_resample.c:142-225` handles rates and processing.

**Fact.** `analog_filter` is only stored in `plugin/m4a/m4a_driver.c:135-140` and `plugin/m4a/m4a_internal.h:211-215`; it is never consumed. It is inert, but that is not proof that it is causal (or that its absence explains the issue).

**Capture seams.** Capture solo channels after `hw_psg_render`/`hw_pcm_render`, pre-resampler after `hw_mix_render`, and final post-resampler output. Candidate nonlinear behavior is clipping in `hw_mix`; source candidates are PSG/noise and PCM FIFO hold; timing candidate is event segmentation; frontend candidate is the obsolete resampler.

## Current mGBA path

**Fact.** At current mGBA HEAD `afd6f14eaf8bd35214ed3fb9dc69a92bfc3877a9`, GBA `audio.c` mixes and applies bias but has no low-pass/EQ; the GB analog-cap filter is not in the GBA path. See `src/gba/audio.c:341-428` and `src/gb/audio.c:743-818`.

**Fact.** mGBA switched away from blip in 2024 (`f51cb153d109f842fdc5325715be97a71fcacd28`) and removed it (`fa2fe8eed4fdf47766e8107b3a75ec5af762be9d`). Current Qt/SDL frontends use `mAudioResampler` SINC from core rate to actual output. The interpolator is a normalized 16-tap (`i=-7..8`), width-8 sinc with a three-term Nuttall window at resolution 8192 (`src/util/audio-resampler.c`, `src/util/interpolator.c:18-86`). The local `tools/mgba-reference/mgba_mp2k_reference.c:382-417,761-785` reads old core blip channels and therefore is an old/core reference, not current Qt/SDL coloration.

**Fact.** Libretro's optional one-pole post-filter is disabled by default and is not the mGBA default filter; see its implementation and options sources.

## Hypotheses

Label and test these, rather than treating them as conclusions:

1. **H1 — frontend:** v2's old blip response differs audibly from current mGBA's normalized 16-tap sinc. This is the leading hypothesis.
2. **H2 — source/mix:** PSG/noise generation, PCM FIFO hold, gain, normalization, bias, clipping, or event segmentation differs before resampling.
3. **H3 — stable coloration:** after H1/H2 are excluded, a broad stable residual may be explained by a fixed one-pole/low-pass response; a narrow stable residual by one or two measured peaking cuts/notches.
4. **H4 — dynamic behavior:** attenuation is level/time dependent. Treat dynamic suppression as unproven and reject it initially because detector/control behavior can add modulation sidebands, program dependence, and latency.
5. **H5 — analog flag:** the inert `analog_filter` flag is related to the issue. It is not consumed, but no causal claim follows without captures.

## Measurement gate

Before any filter or resampler migration, capture v2 immediately after mix/bias at the native cadence and capture current mGBA's raw `mAudioBuffer` at the same rate (normally 32,768 Hz), before Qt/SDL resampling. Align the captures, remove DC, fit delay and scalar gain, then compare null residual, PSD/CSD, and coherence. Solo every channel. Only after the native streams match should the same source be passed through current mGBA sinc and v2 blip. The existing checks (`test/test_engine.c:4488-4559` for frontend high-pass and exact 0.10.5 blip step; `test/test_engine.c:5219-5315` for gross anti-aliasing) do not provide matched full-spectrum/capture parity and cannot close this gate.

**Gate:** no resonance filter is admissible until matched captures identify a stable residual after source/mix and frontend attribution. Do not claim the listening issue is proven before this gate passes.

## Decision tree

- Native-rate v2 core does **not** match mGBA core → fix source, mix, bias/clipping, FIFO, or timing; do not EQ.
- Native-rate core matches, and v2 blip differs from current mGBA sinc → migrate/port the current sinc frontend; do not add resonance suppression.
- Native-rate core and frontend match, with broad stable residual → evaluate one-pole or low-pass biquad.
- Narrow stable transfer-ratio residual → evaluate one or two fixed measured peaking cuts/notches.
- Ratio varies across notes/channels → reject a fixed filter.
- Held-out captures show demonstrable level/time-dependent attenuation → only then investigate dynamic control; otherwise reject dynamic suppression.

## Implementation options

1. **Preferred:** add matched-capture tooling/gates at the existing seams, then port the current mGBA `mAudioResampler` sinc algorithm if H1 is confirmed. Preserve native-rate and post-resampler comparisons.
2. **Conditional:** implement a fixed one-pole/low-pass biquad only for a broad, stable residual after parity. Use normalized coefficients and transfer equations from the W3C Audio EQ Cookbook; account for CCRMA's one-pole/time-varying-filter caveats.
3. **Conditional:** implement fixed peaking/notch cuts only for a narrow, stable measured residual.
4. **Rejected initially:** dynamic resonance suppression. It is not fidelity-first and has no measured level/time-dependent requirement yet.

If copying mGBA code or tables, perform an MPL-2.0 license review, retain copyright/license notices, and satisfy the applicable source/disclosure terms. A clean-room reimplementation from the cited algorithm is preferable where it avoids copying implementation text, but it does not remove the need to review the license and attribution obligations.

## Integration seam

Keep source/mix and frontend changes separate. Instrument the per-channel post-`hw_psg_render`/`hw_pcm_render` seam, the pre-resampler post-`hw_mix_render` seam, and final post-resampler output. Make the resampler selectable for A/B capture (old blip versus current sinc) without inserting an EQ into the shared source path. Add the matched parity gate alongside the existing frontend/anti-alias tests; do not weaken those tests.

## Acceptance criteria

- Matched captures exist for solo channels and mixed output at per-channel, pre-resampler, and final seams.
- Captures are aligned with DC removed and delay/scalar gain fitted; null residual, PSD/CSD, and coherence are recorded.
- Native-rate v2-versus-mGBA parity is explicitly reported before frontend migration or filtering.
- The result distinguishes source/mix mismatch from old-blip-versus-current-sinc mismatch.
- No resonance filter ships unless a residual remains stable under the stated gate; no dynamic filter ships without held-out evidence of level/time-dependent attenuation.
- If H1 is confirmed, v2's current mGBA sinc migration is preferred over EQ, with MPL-2.0 obligations documented and met.

## Primary sources

- Current mGBA GBA audio mix/bias (no GBA low-pass/EQ): <https://github.com/mgba-emu/mgba/blob/afd6f14eaf8bd35214ed3fb9dc69a92bfc3877a9/src/gba/audio.c#L341-L428>
- mGBA GB audio path: <https://github.com/mgba-emu/mgba/blob/afd6f14eaf8bd35214ed3fb9dc69a92bfc3877a9/src/gb/audio.c#L743-L818>
- mGBA switch away from blip: <https://github.com/mgba-emu/mgba/commit/f51cb153d109f842fdc5325715be97a71fcacd28>
- mGBA blip removal: <https://github.com/mgba-emu/mgba/commit/fa2fe8eed4fdf47766e8107b3a75ec5af762be9d>
- Current mGBA resampler: <https://github.com/mgba-emu/mgba/blob/afd6f14eaf8bd35214ed3fb9dc69a92bfc3877a9/src/util/audio-resampler.c>
- Current mGBA normalized interpolator: <https://github.com/mgba-emu/mgba/blob/afd6f14eaf8bd35214ed3fb9dc69a92bfc3877a9/src/util/interpolator.c#L18-L86>
- Qt frontend reference: <https://github.com/mgba-emu/mgba/blob/afd6f14eaf8bd35214ed3fb9dc69a92bfc3877a9/src/platform/qt/AudioDevice.cpp>
- SDL frontend reference: <https://github.com/mgba-emu/mgba/blob/afd6f14eaf8bd35214ed3fb9dc69a92bfc3877a9/src/platform/sdl/sdl-audio.c>
- Libretro optional post-filter and formula: <https://github.com/mgba-emu/mgba/blob/afd6f14eaf8bd35214ed3fb9dc69a92bfc3877a9/src/platform/libretro/libretro.c#L121-L173>
- Libretro filter options/default: <https://github.com/mgba-emu/mgba/blob/afd6f14eaf8bd35214ed3fb9dc69a92bfc3877a9/src/platform/libretro/libretro_core_options.h#L172-L207>
- W3C Audio EQ Cookbook: <https://www.w3.org/TR/2021/NOTE-audio-eq-cookbook-20210608/>
- CCRMA filters: <https://ccrma.stanford.edu/~jos/filters/>
