# Audio Engine Rewrite: Layered DSP Architecture

**Status:** plan, not started.  Branch: `loader-refactor`.

The current `plugin/m4a_channel.c` and `plugin/m4a_engine.c` mix four
concerns into the same per-sample functions:

1. Clean voice synthesis (m4a-style ideal sound)
2. GBA hardware artifacts (mGBA-style emulation: noise averaging, S&H, NR32 quantization)
3. Output gain / scaling (per-channel `>>=1`, `(sample * 3) >> 2`, divide-by-256)
4. Resampling (CGB internal → host, per-channel S&H upsamplers)

This has produced a chain of one-off fixes — `(sample * 3) >> 2`, CGB internal
rate ping-ponging between 32768/65536/49152, PCM `pcmStepHz` accumulator, noise
`_coalesceNoiseChannel` clone — each correct for its scenario but increasingly
hard to reason about together.  The user wants a layered split:

```
Voice synthesis (clean m4a-equivalent) →
   per-channel hardware DSP (mGBA-equivalent filtering) →
      mix bus (master volume, pan, bias) →
         single high-quality resampler to host rate
```

## Current state — what's where (commits to delete or move)

Working tree is clean, all relevant fixes committed on `loader-refactor`:

- `41c25ba` Match mGBA audio path: PCM S&H, native-rate CGB, noise coalescing
- `9aad002` Match mGBA per-channel CGB amplitude: ×0.5 → ×0.75
- `3dffdeb` CGB internal rate 32768 → 49152 Hz to match mGBA at common DAW rates
- `171dec8` Fix CGB rate comment: 65536 → 49152

These fixes solve real problems but bury hardware emulation inside synthesis.
Each becomes a single discrete filter stage in the new architecture.

### `plugin/m4a_channel.c::m4a_pcm_channel_render` — 60 lines
Currently:
- Linear-interp at host rate (clean PCM synthesis)
- `ampR = sample * envelopeVolumeRight; *mixR += ampR >> 8` (channel scaling + mix)

After rewrite:
- Layer 1: linear-interp at PCM internal rate (~13379 Hz), output float
- Layer 2: S&H from 13379 to internal mix rate; bias-clip simulation
- Layer 3: master volume × pan in mix bus

### `plugin/m4a_channel.c::m4a_cgb_channel_render` — 130 lines
Currently:
- Branches on cgbType (square/wave/noise)
- Square: duty pattern lookup, ±64 bipolar, envelope, `>>=1`/`*3>>2` scale
- Wave: nibble lookup, NR32 quantization, `(shifted - meanShifted) * 8` mean-subtract, declick
- Noise: LFSR clocking, sub-sample averaging (mGBA `_coalesceNoiseChannel` clone)

After rewrite:
- Layer 1 square: clean unipolar `pattern[bit] * env / 15`, no scale
- Layer 1 wave: clean unipolar nibble × NR32 shift, no mean-subtract
- Layer 1 noise: single LFSR bit × env, no averaging
- Layer 2 noise: `_coalesceNoiseChannel`-equivalent filter
- Layer 2 wave: AC-couple (DC block) + declick
- Layer 2 PSG bias: simulate the SOUNDBIAS bias add/clip if needed for parity
- Layer 3: NR50 master volume × NR51 pan in mix bus

### `plugin/m4a_engine.c::m4a_engine_process` — ~120 lines
Currently:
- Per-sample: tick, render PCM channels (with S&H accumulator), reverb, render CGB channels (with linear-interp resampler from CGB_INTERNAL_RATE), output divide-by-256, optional analog filter

After rewrite:
- Per internal-rate sample: tick (at vblank), call layer 1 synthesis, layer 2 filtering per channel, layer 3 mix, layer 4 resample to host
- Output buffer fill loop pulls from the resampler

## Target architecture

### Layer 1 — Voice Synthesis (mGBA-agnostic)

Per-channel, produces `float` at the internal mix rate.  Synthesis is "ideal" —
no hardware artifacts, just what m4a *intended* the channel to sound like.

| Channel | Output range | Synthesis math |
|---|---|---|
| Square 1, 2 | [0, 1] unipolar (or bipolar; pick one and stay) | `dutyPattern[(phase >> 29) & 7] * env / 15` |
| Wave (ch3) | [0, 1] unipolar | `nibble[pos] >> NR32_shift` (NR32 quantization is a SYNTHESIS choice — m4a writes the shift, that's what the song wants) |
| Noise (ch4) | [0, 1] unipolar | `(lfsr & 1) * env / 15`; LFSR clocks at noise rate |
| PCM ChA, ChB | [-1, +1] signed | `linear_interp(source, fw)`; runs at PCM internal rate (13379 Hz) |

Decision: **unipolar for CGB, bipolar for PCM** — matches mGBA's actual signal
shapes.  DC offsets are real; the mix-bus DC blocker (Layer 3) handles them.

Wave's mean-subtraction (`(shifted - meanShifted) * 8`) **disappears** — it was
a workaround for not handling DC in the mix bus.

### Layer 2 — Per-Channel Hardware DSP

Each channel passes through a small filter chain that emulates the GBA hardware
between the CPU and the DAC.  Stateful, per-channel.

| Channel | Filter chain |
|---|---|
| Square 1, 2 | (none — GBA squares are clean step functions; hardware passes through) |
| Wave (ch3) | declick fade on note-off (currently in synth code, move here) |
| Noise (ch4) | `_coalesceNoiseChannel`-equivalent: accumulate LFSR clocks, return mean when ≥2 since last reset |
| PCM | sample-and-hold from PCM rate (13379) to internal mix rate; SOUNDBIAS bias-clip simulation `[0, 0x3FF]` if pursuing strict parity |

The noise averaging logic (`noiseAccum`, `noiseSamples` fields) **moves to a
dedicated filter state struct**, out of `M4ACGBChannel`.  The PCM S&H state
(`pcmStepHz`, `pcmStepAccum`, `pcmHeldL/R` engine fields) **moves to a
per-PCM-channel filter state**, out of `M4AEngine`.

### Layer 3 — Mix Bus

Sums all 6 channels.  Runs at internal mix rate.

```
sumL = pcm_chA_filtered_L + pcm_chB_filtered_L
     + (sq1 + sq2 + wave + noise) * NR51_left_mask * (1 + NR50_left_vol)
sumR = ... (same for right)
```

Apply:
- Master volume scale (`masterVolume * 3 / 16` à la mGBA's `_applyBias`)
- Optional DC blocker (1-pole HPF, ~10-20 Hz cutoff) — **make this a flag**;
  matches what real DAC would strip but DAW level meters care
- Reverb (currently here; stays here)

The per-channel `>>=1`, `(sample * 3) >> 2`, and engine `/256` **collapse into
a single master volume coefficient** computed from NR50 + master vol with the
right ratio.

### Layer 4 — Output Resampler

One high-quality resampler from internal mix rate to host rate.

Internal rate proposal: **131072 Hz** (4× mGBA's 32768 native PSG rate).

- Clean integer relationship to PSG synthesis rate (×4)
- Clean integer relationship to noise rates (always sub-multiples of 524288)
- Adequate Nyquist (65536 Hz) for any audible content
- ~3× downsample to 48 kHz host, ~3× to 44.1 kHz — manageable polyphase

Alternative: **196608 Hz** (= 4 × 48000 = 6 × 32768) — perfect 4:1 ratio to
48 kHz host, exact integer downsample for the most common DAW rate.  Slightly
higher CPU.

Implementation: polyphase FIR or windowed-sinc with ~16 taps.  Replaces:

- The per-CGB linear-interp upsampler (currently 49152 → host)
- The per-PCM S&H upsampler (currently 13379 → host)

Both become moot — synthesis runs at internal rate, single resampler at the end.

## What stays / moves / disappears

### Stays (no changes)
- `m4a_engine.c::m4a_engine_tick` (vblank tick, envelope/MIDI dispatch)
- `m4a_engine.c::m4a_track_vol_pit_set` (TrkVolPitSet equivalent)
- `m4a_engine.c::m4a_midi_key_to_freq` / `m4a_midi_key_to_cgb_freq`
- `m4a_engine.c::m4a_engine_note_on/off/cc/pitch_bend` (MIDI dispatch)
- Voicegroup loading (`voicegroup/`)
- Tempo system (`tempoD/U/I/C`)
- `m4a_reverb.{c,h}` (moves into Layer 3 unchanged)

### Moves (refactored into new files)
- PCM linear-interp synthesis → `m4a_synth.c::synth_pcm_render` (Layer 1)
- PCM S&H → `m4a_hwdsp.c::hwdsp_pcm_filter` (Layer 2)
- Square duty pattern + envelope → `m4a_synth.c::synth_square_render` (Layer 1)
- Wave nibble + NR32 quantization → `m4a_synth.c::synth_wave_render` (Layer 1)
- Wave declick → `m4a_hwdsp.c::hwdsp_wave_filter` (Layer 2)
- LFSR clocking → `m4a_synth.c::synth_noise_render` (Layer 1)
- LFSR sub-sample averaging → `m4a_hwdsp.c::hwdsp_noise_filter` (Layer 2)
- Reverb → `m4a_mix.c::mix_apply_reverb` (Layer 3)
- CGB resampling + PCM S&H upsampler → `m4a_resample.c::resample_to_host` (Layer 4)

### Disappears
- `sample >>= 1` and `(sample * 3) >> 2` per-channel CGB scaling (subsumed by Layer 3 master volume)
- `(shifted - meanShifted) * 8` wave mean-subtraction (replaced by Layer 3 DC blocker if desired)
- Per-channel resampler state in `M4APCMChannel` and `M4ACGBChannel`
- `M4AEngine::pcmStepHz`, `pcmStepAccum`, `pcmHeldL/R`
- `M4AEngine::cgbResampleAccum`, `cgbPrevL/R`, `cgbCurrL/R`
- `M4AEngine::analogFilter`, `lpfBypass`, `lpf_*` (unless we want a real DAW-mode HPF; revisit at end)

## Internal sample rate decision

Recommendation: start with **131072 Hz**.

| Rate | × mGBA PSG | host=48k ratio | host=44.1k ratio | CPU |
|---|---|---|---|---|
| 65536 | 2× | 1.46:1 (upsample) | 1.49:1 (upsample) | low |
| 131072 | 4× | 2.73:1 (down) | 2.97:1 (down) | medium |
| 196608 | 6× | **4:1 exact** | 4.46:1 (down) | medium-high |
| 262144 | 8× | 5.46:1 | 5.94:1 | high |

131072 gives clean 4× headroom over mGBA's PSG rate (so all aliasing
matches mGBA), good Nyquist for the resampler input, manageable CPU.  If 48 kHz
DAW workflow ends up dominant we can switch to 196608 for the exact 4:1
downsample, but 131072 is safer for the first cut.

PCM internal at **13379 Hz** (the m4a freq-index-4 rate).  S&H from 13379 to
131072 is a clean ratio.

## Validation plan

We've validated against:
- `MUS_EVER_GRANDE` (full mix, mGBA-headless 32768 Hz reference)
- `friendventure` (wave + noise only, mGBA Qt 65536 Hz reference, 10.041s skip)

Need before merging:
- **Per-channel isolation tests**: capture mGBA Qt with each channel solo
  (channel mute UI lets you isolate ch1/ch2/ch3/ch4/ChA/ChB).  Render same
  song with poryaaaa per-channel solo (would need a `--solo-channel` flag).
  Compare per-band RMS within ±0.5 dB.
- **Multiple songs**: at least 5 across PCM-heavy and CGB-heavy material.
- **Multiple host rates**: 32768, 44100, 48000, 88200, 96000.  Wave/noise
  ratio should match mGBA within ±0.5 dB at every rate.
- **A/B blind listening**: user listens to poryaaaa and mGBA back-to-back at
  matched LUFS; should be indistinguishable.

Reference capture tool: `tools/captures/mgba-headless-audio-out/mgba-headless`
with `--audio-out` produces 32768 Hz PCM s16le.  See README in that dir.

## Migration plan

Two-track parallel build, gated by a CMake/runtime flag:

1. **Skeleton**: create `m4a_synth.c/h`, `m4a_hwdsp.c/h`, `m4a_mix.c/h`,
   `m4a_resample.c/h`.  Hollow stubs.

2. **Layer 1 first**: implement clean synthesis per channel.  Internal mix
   rate 131072 Hz.  Bypass Layers 2, 3, 4 (mix to stereo, output at 131072
   downsampled with simple linear-interp temporarily).  Verify the engine
   produces audible output that "kind of sounds right."

3. **Layer 4**: add proper resampler (windowed-sinc 16-tap or polyphase).
   Validate output quality at 44.1, 48, 88.2 kHz host.

4. **Layer 3**: master volume / pan / bias / reverb.  Verify level sanity.

5. **Layer 2 noise**: port `_coalesceNoiseChannel` into the noise filter.
   Validate friendventure noise at all host rates.

6. **Layer 2 PCM**: port S&H.  Validate PCM-heavy song.

7. **Layer 2 wave**: port declick.  Optional DC handling.

8. **A/B against current path**: gate behind `M4A_ENGINE_V2` flag.  Run both
   on same MIDI, compare diff.wav.  Should be quieter than current
   diff RMS.

9. **Migrate validation suite**: re-run all comparisons (MUS_EVER_GRANDE,
   friendventure, etc.) with v2.

10. **Delete old code**: once v2 ships, remove the old `m4a_channel.c` PCM
    and CGB renders, the engine's per-sample resampler accumulators, and
    the dead analog filter scaffolding.

Estimated effort: ~2-3 days of focused work.  Largest risk: getting the
resampler right (Layer 4); easiest to validate in isolation by feeding a
known sine sweep.

## Build/install reminder for next session

`rm -rf ~/Library/Audio/Plug-Ins/CLAP/poryaaaa.clap` then `cmake --build
build --target poryaaaa_render poryaaaa` then `cp -R build/poryaaaa.clap
~/Library/Audio/Plug-Ins/CLAP/` then `ls -la
~/Library/Audio/Plug-Ins/CLAP/poryaaaa.clap/Contents/MacOS/poryaaaa` to
verify mtime.  See memory file `feedback_delete_clap_before_build.md`.

Renderer:
```
build/poryaaaa_render <project_root> <voicegroup> --midi <file.mid> \
  --output <out.wav> --sample-rate 32768 --total-duration-seconds 30 \
  --fadeout 0 --tail 0 --reverb 50
```

Compare:
```
cd tools && ./compare.sh <basename>   # expects <basename>_poryaaaa.wav
                                       # and <basename>_mgba.wav in captures/
```

## Reference docs

- `/tmp/mgba-ref/gba_audio.c` — mGBA's PCM mixer, FIFO, bias (`_applyBias`)
- `/tmp/mgba-ref/gb_audio.c` — mGBA's PSG synthesis, `_coalesceNoiseChannel`,
  NR32 wave volume table, square duty patterns
- `tools/captures/mgba-headless-audio-out/README.md` — patched headless mGBA
  with `--audio-out` for capturing reference WAVs at 32768 Hz native
