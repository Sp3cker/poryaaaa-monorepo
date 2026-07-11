# PSG mGBA parity handoff

Date: 2026-07-10

Repository: `/Users/spencer/dev/cProjects/poryaaaa-monorepo`

Package: `packages/poryaaaa`

Branch / base HEAD: `main` at `ac61c6044a5cc8688c5e68b6ab06d8bf22deb33c`

## Objective

Continue from the completed isolated PSG work by making the automated
comparison robust for repeating musical material and by extending the proof to
combined PSG mixes, especially the programmable-wave channel.

Do not retune SQ2 gain or begin with MP2K channel-allocation tracing. The three
previously reported song-level SQ2 outliers were comparator indexing artifacts,
not evidence of different hardware-channel ownership or envelope state.

## Current status

The square oscillator, noise generator, PSG mix scale, hardware envelope
engine, SOUNDBIAS DAC cadence, and host frontend have been brought over to mGBA
behavior. Headless mGBA reference capture and waveform-comparison tooling are
present in the worktree.

### Controlled SQ2 proof

A one-voice pair using `voicegroup_weather_groudon` voice 81, note 99,
velocity 112, volume 39, and right pan produced:

| Correlation | Gain-fitted residual | poryaaaa vs mGBA RMS |
| ---: | ---: | ---: |
| `0.9999990187` | `0.1401%` | `+0.00114 dB` |

This proves the Weather SQ2 voice synthesis, envelope, and gain in isolation.

### Cross-song proof

Ten representative `mus_` songs were captured for eight seconds with SQ1 and
SQ2 isolated. Nineteen audible channel/song pairs were comparable; all 19
found strict, same-polarity local waveform matches. `mus_littleroot` SQ1 was
silent and therefore not comparable.

Neighborhood-aligned representative SQ2 level deltas are:

| Song | poryaaaa vs mGBA RMS |
| --- | ---: |
| `mus_weather_groudon` | `-0.025 dB` |
| `mus_encounter_girl` | `-0.023 dB` |
| `mus_vs_regi` | `-0.081 dB` |

The old values of `-6.085 dB`, `-0.368 dB`, and `-10.137 dB` came from
matching different repeated musical events or envelope stages. They must not
be cited as real song-state discrepancies.

### Isolated stereo proof

Independent left/right comparisons established the expected mGBA routing:

| Capture | Expected routing | Result |
| --- | --- | --- |
| Weather SQ1 | left only | match |
| Weather SQ2 | right only | match |
| Birch Lab SQ2 | left only | match |
| Door noise | centered | match |

All eight implementation/routing signatures passed. Deliberately swapping the
Weather SQ1 channels failed, and the expected silent sides contained zero PCM.
This rules out stereo collapse, channel reversal, and incorrect NR51 nibble
decoding for isolated SQ1, SQ2, and noise.

## What was fixed

### MP2K CGB register behavior

Relevant files:

- `packages/poryaaaa/plugin/m4a/m4a_cgb.c`
- `packages/poryaaaa/plugin/m4a/m4a_internal.h`

The implementation now emits the observable `CgbSound` register order,
programs the hardware envelope and length control, matches pitch-before-volume
ordering, preserves trigger-on-volume-write behavior, and uses the real square
and noise oscillator-off writes.

### PSG hardware model

Relevant files:

- `packages/poryaaaa/plugin/hw_audio/hw_psg.c`
- `packages/poryaaaa/plugin/hw_audio/hw_psg.h`

SQ1/SQ2 use exact GBA CPU-cycle periods and retain elapsed timer time across
silence and register writes. Hardware envelopes, frame sequencing, NR52 phase,
noise LFSR trigger/shift behavior, and GBA-mode unipolar output match mGBA.

### mGBA DAC/frontend behavior

Relevant files:

- `packages/poryaaaa/plugin/hw_audio/hw_audio.c`
- `packages/poryaaaa/plugin/hw_audio/hw_resample.c`
- `packages/poryaaaa/plugin/hw_audio/hw_resample.h`
- `packages/poryaaaa/plugin/hw_audio/LICENSE.blip_buf`

The complete GBA mix is sampled at the SOUNDBIAS cadence
(`32768 << sampling_cycle`). The former Hann/polyphase frontend was replaced by
the impulse table, clock mapping, integrator, clipping, and 511/512 high-pass
behavior from the `blip_buf` bundled with mGBA 0.10.5.

### Other parity fixes

- Removed the artificial CGB gain multiplier.
- Set the M4A SOUNDBIAS default to sampling cycle 1 / 65536 Hz.
- Default MIDI track volume is 127, matching MP2K.
- Renderer events with the same rounded sample retain chronological order.
- Non-loop `--total-duration-seconds` renders exactly the requested duration.
- The capture helper defaults missing midi.cfg `-V` values to 127.

## Important interpretation of comparison results

`coverage_report.py` detects each WAV's onset independently, scans equal
relative offsets, and selects the earliest strict local waveform match. This is
useful for isolated material, but repeating square-wave events at different
envelope stages can both pass the shape gate and produce a false raw-RMS delta.

For song-level work:

1. Build an RMS-energy envelope in small blocks.
2. Match normalized 1.25-to-2-second musical neighborhoods with continuity-
   constrained search.
3. Run the strict sample-level comparator locally around the tracked
   neighborhood.
4. Require signed correlation at least `0.999`, residual at most `5%`, and
   audible activity on both sides before reporting raw RMS.
5. Treat isolated repeated-wave ambiguities as comparison failures, not engine
   defects or level measurements.

Do not fold to mono when proving routing. Compare left and right independently,
including the expected silent channel.

## Remaining boundary and next action

The next implementation task is the comparison harness, not the audio engine:

- add a regression fixture containing repeated same-frequency square windows at
  different amplitudes;
- make the old earliest-window strategy fail that fixture;
- add the smallest neighborhood/sequence-aligned comparison mode that rejects
  the false match;
- preserve the strict polarity, activity, correlation, and residual gates;
- report left/right results independently for stereo captures.

After that, use the corrected comparator to prove a combined PSG capture that
includes the programmable-wave channel. Isolated SQ1, SQ2, and noise are
already proven; combined four-channel PSG/program-wave parity is not.

Do not reopen channel-allocation, retrigger, or NRxx tracing without new evidence
from a correctly aligned comparison.

## Reproduction commands

```bash
cd /Users/spencer/dev/cProjects/poryaaaa-monorepo/packages/poryaaaa

cmake -B build \
  -DPORYAAAA_BUILD_MGBA_REFERENCE=ON \
  -DPORYAAAA_MGBA_ROOT=/opt/homebrew

cmake --build build --target \
  poryaaaa_unit_tests poryaaaa_render mgba_mp2k_reference
```

Capture a representative isolated pair:

```bash
tools/mgba-reference/capture_song_pair.sh \
  --decomp /Users/spencer/dev/hearth-test \
  --song mus_weather_groudon \
  --solo sq2 \
  --duration-seconds 8
```

Run focused validation:

```bash
./build/poryaaaa_unit_tests
python3 tools/mgba-reference/test_waveform_compare.py
python3 tools/mgba-reference/test_coverage_report.py
tools/mgba-reference/test_capture_song_pair.sh
tools/mgba-reference/smoke_test.sh /Users/spencer/dev/hearth-test
```

## Latest verification

- Direct C/C++ suite: `775/777`; the two failures are unrelated
  voicegroup-core parity failures.
- Waveform comparator tests: `6/6`.
- Coverage reporter tests: `6/6`.
- Capture helper test: pass.
- Headless mGBA smoke test: pass.
- Ten-song isolated square matrix: `19/19` audible pairs with strict local
  same-polarity waveform matches.
- Stereo routing signatures: `8/8`.

## Worktree warning

All parity work is currently uncommitted. The worktree also contains unrelated
changes in other packages and scratch material. Do not reset or revert them.
Read the root and `packages/poryaaaa/AGENTS.md` files before editing package
code.

## Definition of done for the next follow-up

1. A red regression demonstrates the repeated-event indexing failure.
2. The automated comparator tracks a consistent musical neighborhood and no
   longer reports the three disproven SQ2 outliers.
3. Stereo mode reports independent left/right shape, level, and silence gates.
4. A combined PSG/program-wave capture passes the corrected comparison, or its
   first genuine divergence is reduced to a minimal reproducer.
5. Existing unit, comparator, capture-helper, smoke, isolated-channel, and
   stereo-routing evidence does not regress.
