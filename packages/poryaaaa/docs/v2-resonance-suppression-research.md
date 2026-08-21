# v2 Resonance Suppression Research

> **Non-shipping research.** This document records hypotheses and measurement
> criteria. It does not establish an audible defect, authorize a filter, or
> provide parity evidence. Only the pinned native harness can make a bounded
> parity claim.

## Present facts

- The current poryaaaa path is PSG/PCM rendering, `hw_mix`, then the frontend
  resampler. `analog_filter` is stored by the M4A driver but is not consumed;
  that fact alone does not identify a cause.
- Current mGBA at `afd6f14eaf8bd35214ed3fb9dc69a92bfc3877a9` mixes GBA audio and
  applies bias without a GBA low-pass/EQ stage. Its current frontend resampler
  uses a normalized 16-tap Nuttall-windowed sinc interpolator.
- mGBA removed its old blip path in 2024. Therefore a listening comparison
  between an old blip frontend and current mGBA is not native source/mix
  evidence.
- Candidate observation points are post-`hw_psg_render`/`hw_pcm_render`,
  post-`hw_mix_render` before resampling, and final post-resampler stereo
  output. They distinguish source/mix, timing, and frontend behavior.

No conclusion here is derived from generated local captures, temporary branch
names, or untracked scratch evidence.

## Hypotheses

1. **Frontend:** an old blip response differs audibly from mGBA's current sinc
   frontend.
2. **Source/mix:** PSG/noise generation, PCM FIFO hold, gain, normalization,
   SOUNDBIAS, clipping, or event segmentation differs before resampling.
3. **Stable coloration:** after hypotheses 1 and 2 are excluded, a broad stable
   residual might support evaluating a fixed low-pass response; a narrow stable
   residual might support measured fixed peaking/notch cuts.
4. **Dynamic behavior:** attenuation varies with level or time. This is
   unproven and is rejected initially because detector/control behavior can add
   modulation, program dependence, and latency.
5. **Analog flag:** the inert `analog_filter` setting is related. Its lack of a
   consumer is not causal evidence.

## Measurement gate

Before changing a filter or resampler, make matched captures at the native DAC
cadence (normally 32,768 Hz): poryaaaa after mix/bias and mGBA's raw
`mAudioBuffer`, before Qt/SDL output resampling. Capture solo channels and the
mixed stream at all three observation points.

For each matched native stream, align equivalent observations, remove DC for
this *diagnostic analysis*, fit delay and one scalar gain, then report null
residual, PSD/CSD, and coherence. Repeat on held-out material. The diagnostic
must retain the raw inputs and report its precise selected window; it must not
be called an exact native-hardware pass.

**Decision rule:** no resonance filter is admissible until matched captures
show a stable residual after source/mix and frontend attribution. Do not claim
that the listening issue is established before this rule passes.

## Decision tree

- Native poryaaaa core does not match mGBA core: correct source, mix,
  SOUNDBIAS/clipping, FIFO, or timing; do not EQ.
- Native cores match but old blip differs from mGBA sinc: migrate/port the
  current sinc frontend; do not add resonance suppression.
- Native core and frontend match with a broad stable residual: evaluate a
  one-pole or low-pass biquad.
- A narrow, stable transfer-ratio residual remains: evaluate one or two fixed
  measured peaking/notch cuts.
- The ratio varies by note or channel: reject a fixed filter.
- Held-out captures demonstrate level/time-dependent attenuation: only then
  investigate dynamic control; otherwise reject it.

## Implementation constraints

Keep source/mix and frontend changes separate. Instrument the listed seams and
make any resampler A/B selection capture-only; do not insert EQ into the shared
source path. A permanent change requires a production contract and test in
addition to the matched-capture research result.

If copying mGBA code or tables, review MPL-2.0 obligations and retain required
notices. A clean-room implementation based on the cited algorithm can avoid
copying code text but does not remove attribution/license review.

## Primary sources

- [mGBA GBA audio mix/bias](https://github.com/mgba-emu/mgba/blob/afd6f14eaf8bd35214ed3fb9dc69a92bfc3877a9/src/gba/audio.c#L341-L428)
- [mGBA audio resampler](https://github.com/mgba-emu/mgba/blob/afd6f14eaf8bd35214ed3fb9dc69a92bfc3877a9/src/util/audio-resampler.c)
- [mGBA normalized interpolator](https://github.com/mgba-emu/mgba/blob/afd6f14eaf8bd35214ed3fb9dc69a92bfc3877a9/src/util/interpolator.c#L18-L86)
- [mGBA blip-removal commit](https://github.com/mgba-emu/mgba/commit/fa2fe8eed4fdf47766e8107b3a75ec5af762be9d)
- [W3C Audio EQ Cookbook](https://www.w3.org/TR/2021/NOTE-audio-eq-cookbook-20210608/)
- [CCRMA filter notes](https://ccrma.stanford.edu/~jos/filters/)
