# DirectSound parity handoff

Date: 2026-07-11

## Objective

Make poryaaaa DirectSound sample playback match mGBA. PSG families are already close enough. Any future source-file inspection must be delegated to the Luna subagent, per user instruction.

## Confirmed engine fixes now implemented in poryaaaa

1. MP2K master volume defaults to 12, matching the tested Pokémon Emerald ROM, instead of 15.
2. Fresh PCM envelopes start at zero. The first `SoundMainRAM` tick applies attack once; poryaaaa no longer precomputes attack and then applies it a second time.
3. When both derived integer L/R mixer volumes are zero, PCM source/cursor processing is skipped, matching ROM `SoundMainRAM` behavior.

Focused regressions were added for all three. Before the fixes they reproduced:

- Master default 15 instead of 12.
- Fresh/first-tick envelope 64/128 instead of 0/64.
- A silent channel advancing its cursor by 224 samples.

## Valid source-domain result

The corrected MP2K source mixer is close enough to use as a valid same-sound comparison:

- Onset correlation: 0.996619
- Gain-fitted residual: 8.2166%
- Level difference: +0.409 dB
- 73 active blocks: minimum correlation 0.991042
- Maximum block residual: 13.3554%
- Level range: -0.422 to +0.409 dB

Artifacts:

- `packages/poryaaaa/build/directsound-fifo-investigation/mgba-source.wav`
- `packages/poryaaaa/build/directsound-fifo-investigation/poryaaaa-source.wav`
- `packages/poryaaaa/build/directsound-fifo-investigation/source-gate.json`
- `packages/poryaaaa/build/directsound-fifo-investigation/REPORT.md`

## Critical harness correction: 12 PCM voices

The previous poryaaaa capture silently used the renderer default of five PCM voices. The apparent chirp/premature cutoff around aligned sample 58173 (0.887650 seconds) was voice stealing:

- Old polyphony 5: incoming note 72 overwrote active note 96.
- Polyphony 12: note 96 remains on channel 7 and note 72 starts on free channel 8, matching mGBA.

Both harnesses are now explicitly configured and validated at 12 PCM voices:

- mGBA: runtime `gSoundInfo.maxChans == 12`
- poryaaaa: explicit renderer `--polyphony 12`

Persistent harness work:

- `capture_directsound_fixture.sh` requires mGBA max channels 12 and passes poryaaaa polyphony 12.
- `mgba_mp2k_reference` has `--require-max-chans` and fails on mismatch after MP2K becomes live.
- `record_voice.sh` forwards the requirement.
- `test_capture_directsound_fixture_contract.sh` locks down the contract.
- Capture manifest records expected/observed/requested voice limits.

Canonical regenerated listening files:

- `packages/poryaaaa/build/directsound-fixture-corrected/aligned-mgba.wav`
- `packages/poryaaaa/build/directsound-fixture-corrected/aligned-poryaaaa.wav`

The premature cutoff is absent in this regenerated pair.

## Current end-to-end hardware result

The strict same-sound waveform gate still fails, so do not infer FIFO or sample-and-hold behavior from the full pair yet:

- Global lag: -2407 samples
- Onset correlation: 0.967104
- Onset residual: 25.438%
- Onset level difference: -0.183 dB
- Later multi-note windows diverge substantially

Resetting boot players, MP2K channels/staging, reverb, and hardware FIFOs did not make the isolated note-60 hardware capture pass the gate. FIFO publication, drain, and resampling have no valid cross-engine conclusion yet.

## Invalid/discarded experiments

- `build/directsound-cursor-test`: invalid. Normal-ROM boot audio contaminated the mGBA DirectSound hardware capture, and poryaaaa actual runtime cursor was not initially traced.
- Old five-voice `directsound-fixture-corrected` capture: superseded. Its cutoff was a harness polyphony error.
- Isolated note-60 hardware comparison: invalid for cross-engine conclusions because the onset gate failed.
- Onset “click” diagnosis: superseded by the user's timestamp. The reported chirp was the five-voice cutoff, not a unique FIFO/resampler transient.

## Remaining concrete state mismatch

With 12 voices, allocation and source cursor state match before the old cutoff. A remaining event/envelope timing difference was observed:

- Poryaaaa explicit MIDI note-off enters release immediately: envelope 255 -> 243.
- ROM note N4 receives one decay tick 255 -> 187 before STOP, then release reaches 178.

This does not kill the channel prematurely, but it is the cleanest next hypothesis. Build a source-domain fixture that reproduces the exact note duration/event ordering in both engines and passes the same-sound gate before drawing a conclusion or changing code.

## Next-session plan

1. Delegate all relevant source reading to Luna.
2. Preserve explicit 12-voice configuration in every capture.
3. Create an exact single-note N4/event-order fixture for both runtimes.
4. Gate on matching sample, key, cursor, frequency, and pre-note-off source audio.
5. Trace envelope/status on the VBlank containing note end and the following release ticks.
6. If event ordering is confirmed, add a red regression and implement only that parity fix.
7. Regenerate source-domain and canonical 12-voice hardware artifacts.

## Validation status

- New focused tests pass.
- Full poryaaaa unit suite: 787/789; only the two existing voicegroup-core parity failures remain.
- Alignment test passes.
- Capture-pair test passes.
- New 12-voice capture-contract test passes.
- Renderer and mGBA reference targets build.
- CLAP build/install passed; installed hash matched the build.
- `just build m4l` passed on 2026-07-11 and rebuilt/linked `poryaaaa~.mxo` with the parity changes.
- `git diff --check` passed.

## Worktree caution

Preserve unrelated user changes/noise, including root/package `.DS_Store`, the pre-existing `packages/poryaaaa/CMakeLists.txt` modification where unrelated, and untracked `packages/poryaaaa/tools/vbam-reference/`. The DirectSound tooling under `tools/mgba-reference/` is current task work.
