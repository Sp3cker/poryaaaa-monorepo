# mGBA reference tooling

This directory supplies opt-in reproducibility tooling for the poryaaaa audio
parity harness. It does not change the shipped audio path. The canonical
external gate is `validate_driver_matrix.py`; all waveform, capture-pair,
coverage, and long-duration procedures are diagnostics unless explicitly
promoted.

## Scope boxes

| Scope | A passing result proves | Explicit bound |
| --- | --- | --- |
| Oracle | Pinned mGBA trace replay reproduces one full-mGBA native capture exactly. | Every frame/cycle between that capture's `BEGIN` and `END`; not other ROM runs or frontends. |
| Hardware | Pinned mGBA and poryaaaa replay one identical trace exactly. | Every observed stereo integer and cycle in that trace; not driver parity. |
| Driver | Candidate and reference agree through the retained lifecycle contract. | 30 fixed cells, each run twice; not arbitrary MP2K state or ARM instruction-cycle parity. |
| Renderer | The shipped renderer executes the documented fixed profile. | 60,000 stereo frames (1.25 s at 48 kHz); not full-ROM, hardware, or driver parity. |
| Frontend diagnostics | A selected local waveform window has the reported relationship. | 512 samples after diagnostic alignment; not stereo/timing/hardware/driver proof. |

Never promote a result from one scope to another. A bounded hardware replay
pass is not end-to-end driver parity.

## Prerequisites

- A matching, already-built external decomp checkout such as
  `/path/to/hearth-test`, containing its ROM and ELF. The checkout is an input;
  none of these tools modifies it.
- GNU Arm Embedded `nm`, assembler, linker, `readelf`, and `objcopy` for
  ROM-building helpers.
- A dedicated external mGBA worktree at
  `afd6f14eaf8bd35214ed3fb9dc69a92bfc3877a9` for native oracle/hardware work.
  Apply [`mgba-audio-observation.patch`](mgba-audio-observation.patch) to its
  clean checkout. The observation patch must be the only modification.
- The existing package `build/` directory. `packages/poryaaaa/.gitignore`
  already ignores it and all documented build-output descendants. Keep traces,
  PCM/cycles, WAV files, manifests, temporary ROMs, and failed diagnostics
  there or outside the repository; do not add child build-directory ignore
  rules.

An installed mGBA development library is permitted only for non-authoritative
frontend recording. Native oracle and hardware claims require the pinned source
worktree, not an installed library.

## Build

From `packages/poryaaaa`, configure the opt-in tools against the pinned,
patched source worktree. The normal build and renderer do not depend on trace
tooling.

```bash
cmake -B build \
  -DPORYAAAA_BUILD_TRACE_TOOLS=ON \
  -DPORYAAAA_BUILD_MGBA_REFERENCE=ON \
  -DPORYAAAA_MGBA_SOURCE=/path/to/mgba-audio-reference
cmake --build build --target \
  mgba_mp2k_reference mgba_audio_trace_replay \
  poryaaaa_audio_trace poryaaaa_driver_trace
```

For a frontend-only recorder, configure
`-DPORYAAAA_BUILD_MGBA_REFERENCE=ON -DPORYAAAA_MGBA_ROOT=/path/to/mgba-prefix`
and build `mgba_mp2k_reference`; do not use that mode for the canonical gates.

## Native trace (canonical)

`record_voice.sh` records a full-ROM native trace using the ROM's MP2K driver;
it does not reuse poryaaaa's driver or `HwAudio`. The recorder writes a versioned
trace plus sibling native artifacts: interleaved signed PCM16 `.pcm`, a
little-endian `uint64` GBA-cycle `.cycles` stream, and `.json` provenance.

```bash
tools/mgba-reference/record_voice.sh \
  --decomp /path/to/hearth-test \
  --voicegroup vs_wild --voice 1 \
  --capture-stage native \
  --trace-output /tmp/sq1.trace \
  --native-output-prefix /tmp/mgba-native \
  --duration-seconds 2

./build/mgba_audio_trace_replay \
  --input /tmp/sq1.trace --output-prefix /tmp/mgba-replay
./build/poryaaaa_audio_trace \
  --input /tmp/sq1.trace --output-prefix /tmp/poryaaaa-replay
python3 tools/mgba-reference/native_compare.py \
  /tmp/mgba-native.json /tmp/mgba-replay.json
python3 tools/mgba-reference/native_compare.py \
  /tmp/mgba-native.json /tmp/poryaaaa-replay.json
```

The trace begins with `PORYAAAA_AUDIO_TRACE 1` and `CLOCK 16777216`. Events
are ordered by absolute GBA cycle and same-cycle order. `WRITE` covers audio
registers, Wave RAM, and 32-bit FIFO A/B words; `TIMER` records DirectSound
byte consumption; `SAMPLE` records the native DAC frame; and `BEGIN`/`END`
bound measured output. FIFO writes caused by a timer underflow precede that
`TIMER` at the same cycle. Extended order values record delayed timer callback
observation without retiming the logical underflow.

`native_compare.py` is exact: frame counts, cycles, left samples, and right
samples must match. Mono folding, lag search, DC removal, gain fitting,
polarity correction, and normalization are prohibited in this gate. Record the
pinned mGBA revision, observation-patch SHA-256, compiler identity, ROM/ELF
hashes, trace hash, and artifact hashes in accepted manifests.

## Driver lifecycle (`validate_driver_matrix.py`, canonical)

This is the only top-level external parity gate. It drives public M4A controls
through five source-verified fixtures: PSW normal (`0x03`), PSW alternate
(`0x0B`), DirectSound (`0x00`), Square 1 (`0x01`), and Square 2 (`0x02`). Each
runs `start`, `envelope`, `pitch`, `volume-pan`, `retrigger`, and `release`, for
30 cells; the complete matrix runs twice and must be deterministic.

```bash
python3 tools/mgba-reference/validate_driver_matrix.py \
  --decomp /path/to/hearth-test \
  --output-dir build/driver-validation/lifecycle
```

Each cell requires exact transaction order/payload/logical state plus reference
native, reference-hardware, and candidate-hardware replay results. Transaction
comparison retains source ordinal and observed cycle but does not claim raw ARM
instruction-cycle parity. The fixed DirectSound fixture exercises timer 0;
timer-1 routing is outside this matrix. Accepted results publish atomically
only after both runs agree; failed diagnostics stay in the sibling
`build/driver-validation/lifecycle.failed/` directory.

Run one cell with `validate_driver.py` only to investigate a matrix failure:

```bash
python3 tools/mgba-reference/validate_driver.py \
  --decomp /path/to/hearth-test \
  --voicegroup voicegroup_rg_poke_center --voice 4 \
  --scenario start \
  --output-dir build/driver-validation/directsound-start
```

## Bounded renderer evidence

This exercises the shipped `poryaaaa_render` path, not native hardware,
driver, or frontend parity. Keep output stereo and use no normalization, gain
fitting, DC removal, or lag diagnostics.

```bash
./build/poryaaaa_render /path/to/hearth-test rs_sfx_1 \
  --midi /path/to/hearth-test/sound/songs/midi/se_pc_on.mid \
  --sample-rate 48000 \
  --song-volume 100 --reverb 50 --polyphony 5 \
  --solo full --total-duration-seconds 1.25 --fadeout 0 \
  --output /tmp/poryaaaa-se-pc-on-48000.wav
```

The profile is analog filter off, full hardware mask, and a 1.25-second
60,000-frame capture. Record hashes of the rendered binary, selected ROM,
voicegroup source, MIDI input, and output beside any comparison. This remains
bounded renderer evidence even when those hashes agree.

## Appendix: legacy diagnostics

The following procedures aid investigation and never pass a canonical gate.

### Frontend waveform diagnostics

`record_voice.sh` can record the frontend path (normally 65,536 Hz PCM drained
from mGBA's frontend buffer). `capture_song_pair.sh`, `waveform_compare.py`,
and `coverage_report.py` may fold stereo to mono, search lag, remove DC, and
fit gain to study local waveform shape. Such a result covers only the selected
512-sample window. Compare left and right independently before making any
stereo observation.

```bash
tools/mgba-reference/capture_song_pair.sh \
  --decomp /path/to/hearth-test --song se_pc_on --solo sq1
python3 tools/mgba-reference/waveform_compare.py \
  build/mgba-reference-pairs/se_pc_on-solo-sq1/mgba.wav \
  build/mgba-reference-pairs/se_pc_on-solo-sq1/poryaaaa.wav \
  --reference-start 6780 --candidate-start 1121 \
  --reference-length 512 --max-lag 64
```

Waveform mono/lag/gain checks, capture-pair output, and coverage reports are
diagnostics, not native or renderer gates. The smoke test similarly checks
recorder health only:

```bash
tools/mgba-reference/smoke_test.sh /path/to/hearth-test
```

### Isolated DirectSound diagnostic

`capture_directsound_fixture.sh` creates private fixture inputs below `build/`
and reports aligned/difference WAVs. Its alignment, correlation, and
 gain-fitted-residual result is diagnostic; it does not establish native
hardware parity.

```bash
tools/mgba-reference/capture_directsound_fixture.sh
```

### Long-duration hardware stress diagnostic

`validate_hardware_stress.py` is opt-in until explicitly promoted. Its four
fixed cases (`sq1`, `sq2`, `directsound`, and `full-mix`) run for 3.0 seconds:
196,608 frames, 50,331,648 GBA cycles, and a 256-cycle sample cadence. It
checks trace integrity plus exact mGBA replay and poryaaaa replay for those
observed spans. This strengthens sampled hardware coverage only; it is not the
external gate and does not prove arbitrary register sequences, driver states,
or game songs.

```bash
python3 tools/mgba-reference/validate_hardware_stress.py \
  --decomp /path/to/hearth-test \
  --output-dir build/hardware-stress
```
