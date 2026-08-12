# Full mGBA MP2K reference recorder

This tool records MP2K through the complete reference path:

```text
real GBA ROM -> ROM's m4a driver -> full mGBA core -> stereo WAV
```

It does not reuse poryaaaa's MP2K driver or `hw_audio`. Voice mode injects a
one-track song after the game initializes MP2K. Song mode builds a derived ROM
whose `AgbMain` only calls `m4aSoundInit`, `m4aSongNumStart`, `m4aSoundVSync`,
and `m4aSoundMain`. Timers, DMA, PSG, and audio sampling remain in mGBA.

The ROM and ELF must be matching outputs from the same decomp build. The ELF is
used only to resolve the exact ROM and RAM addresses needed by the selected
capture mode. The decomp checkout is never modified.

## Build

mGBA must be installed with its development headers and library. The target is
opt-in so normal poryaaaa builds do not gain an emulator dependency.
Building a configured song ROM also requires the GNU Arm Embedded assembler,
linker, `nm`, `readelf`, and `objcopy` from the decomp toolchain.

```bash
cd packages/poryaaaa
cmake -B build \
  -DPORYAAAA_BUILD_MGBA_REFERENCE=ON \
  -DPORYAAAA_MGBA_ROOT=/opt/homebrew
cmake --build build --target mgba_mp2k_reference
```

## Build an audio-only song ROM

The builder copies the matching game ROM and replaces `AgbMain` with a
124-byte audio-only bootstrap. The normal CRT, exact compiled m4a code, song
table, voicegroups, and samples remain unchanged. The source ROM is never
modified. The selected song starts immediately after `m4aSoundInit` in a GBA
emulator.

```bash
tools/mgba-reference/build_song_rom.sh \
  --decomp /Users/spencer/dev/hearth-test \
  --song se_door \
  --output /tmp/se-door-audio-only.gba
```

## Record a voice

Voice indices are zero-based entries in the named voicegroup.

```bash
tools/mgba-reference/record_voice.sh \
  --decomp /Users/spencer/dev/hearth-test \
  --voicegroup vs_wild \
  --voice 1 \
  --note 60 \
  --duration-seconds 2 \
  --output square-1.wav
```

The wrapper accepts either `vs_wild` or `voicegroup_vs_wild`. Use `--pan 64`,
`--volume 127`, and `--velocity 127` for centered full-level comparisons.
The WAV is 65536 Hz PCM drained from mGBA's `postAudioBuffer` blip buffers,
the same band-limited, DC-blocked signal consumed by mGBA frontends. It is not
the raw `postAudioFrame` DAC stream.

## Cycle-domain native comparison

Hardware comparisons use a cycle-stamped trace rather than separately timed
WAV recordings. `poryaaaa_audio_trace` consumes the same trace that the pinned
mGBA replay adapter will consume and bypasses the sinc frontend:

```bash
cmake --build build --target poryaaaa_audio_trace
./build/poryaaaa_audio_trace \
  --input /tmp/song-audio.trace \
  --output-prefix /tmp/poryaaaa-native \
  --solo sq1
```

The text trace is deliberately narrow and versioned:

```text
PORYAAAA_AUDIO_TRACE 1
CLOCK 16777216
WRITE 0 0 2 04000088 00000200
BEGIN 0 1
SAMPLE 0 2
END 1 0
```

Each event is ordered by `(absolute_gba_cycle, same_cycle_order)`. `WRITE`
accepts mGBA's halfword audio-register calls, visible Wave RAM writes, and
32-bit FIFO A/B writes. `TIMER cycle order 0|1` drains the DirectSound channels
selecting that timer. `SAMPLE` records one native DAC frame at the exact point
where mGBA emitted it. Events before `BEGIN` establish chip state without being
included in the measured output.

The recorder writes three sibling artifacts:

- `.pcm`: interleaved little-endian signed PCM16, left then right.
- `.cycles`: one little-endian `uint64` GBA cycle per stereo frame.
- `.json`: versioned format, clock, frame count, cycle range, and channel mask.

Compare a validated mGBA-clone capture against poryaaaa without altering either
signal:

```bash
python3 tools/mgba-reference/native_compare.py \
  /tmp/mgba-native.json \
  /tmp/poryaaaa-native.json
```

The native gate requires identical frame counts, cycles, left samples, and
right samples. It reports the first mismatch, mismatch counts, maximum integer
errors, and artifact hashes. It never folds to mono, searches lag, removes DC,
or fits gain. `waveform_compare.py` remains a diagnostic frontend-WAV tool and
cannot pass the native hardware gate.

Hardware-channel isolation uses mGBA's channel controls after reset. `--solo`
keeps a comma-separated channel list; `--mute` removes it. Supported names are
`sq1`, `sq2`, `wave`, `noise`, `fifo-a`, `fifo-b`, `psg`, `directsound`, and
`all`.

```bash
tools/mgba-reference/record_voice.sh \
  --decomp /Users/spencer/dev/hearth-test \
  --song mus_slateport \
  --mute directsound \
  --duration-seconds 8 \
  --output /tmp/mus-slateport-psg-only.wav
```

## Shipped renderer evidence profile

Use this one fixed profile for the `poryaaaa_render` shipped-path evidence.
It is a renderer exercise, not a whole-song parity claim: native hardware,
driver-trace, and frontend gates still apply independently.

- Analog filter: off. The renderer has no `--analog-filter` option because it
  did not control a renderer filter.
- Fadeout: `0` seconds; output normalization, gain fitting, and DC removal:
  none.
- Host rate: `48000` Hz. Channel mask: `full` (`HW_AUDIO_SOLO_FULL`, `0x3F`).
- Polyphony: `5`; song volume: `100`; reverb: `50`; duration: `1.25` seconds.
- Fixture: `se_pc_on`, with `rs_sfx_1`.

Run the renderer from the package directory with a supplied decomp checkout:

```bash
./build/poryaaaa_render \
  "$PROJECT_ROOT" rs_sfx_1 \
  --midi "$PROJECT_ROOT/sound/songs/midi/se_pc_on.mid" \
  --sample-rate 48000 \
  --song-volume 100 \
  --reverb 50 \
  --polyphony 5 \
  --solo full \
  --total-duration-seconds 1.25 \
  --fadeout 0 \
  --output /tmp/poryaaaa-se-pc-on-48000.wav
```

The evidence manifest must record these SHA-256 values before comparing
artifacts:

| Input | SHA-256 |
| --- | --- |
| ROM image | `f24f420542315fc699fad541915e9a437e55fbc3e9cdf2dfdaf3a5a0e55466f3` |
| `sound/voicegroups/rs_sfx_1.inc` | `46dc8966a9760bfc74a59308ae766fb6742994b97b4c37456a0b11f12974b1bd` |
| `sound/songs/midi/se_pc_on.mid` | `672490fca6e381fd4f0d9d1fd0707b77714c52905f15321cb7206c6fb6864def` |

Record the SHA-256 of `./build/poryaaaa_render` and every capture alongside
those inputs. If a copied renderer is exercised, require both `cmp -s` and
equal SHA-256 values against `./build/poryaaaa_render`; equality proves the
executed binary is the built binary.

Keep the WAV's left and right channels independent in every evidence artifact
and comparison. Do not fold to mono, normalize, fit gain, remove DC, or use
lag/correlation diagnostics as a parity pass. The 65536 Hz commands below are
legacy frontend diagnostics, not this fixed shipped-renderer profile.

## Compare a real game sound

`se_pc_on` is a useful square-1 regression because song mode runs its compiled
ROM SongHeader through `m4aSongNumStart` and `gMPlayInfo_SE1`, including the
real tempo, bend, XCMD echo, volume, notes, and reverb. `record_voice.sh --song`
builds a temporary audio-only ROM before recording, so no game or other music
player runs during the capture.

```bash
tools/mgba-reference/record_voice.sh \
  --decomp /Users/spencer/dev/hearth-test \
  --song se_pc_on \
  --duration-seconds 1.25 \
  --output /tmp/mgba-se-pc-on.wav

./build/poryaaaa_render \
  /Users/spencer/dev/hearth-test rs_sfx_1 \
  --midi /Users/spencer/dev/hearth-test/sound/songs/midi/se_pc_on.mid \
  --sample-rate 65536 \
  --song-volume 100 \
  --reverb 50 \
  --tail 0.25 \
  --output /tmp/poryaaaa-se-pc-on.wav
```

The ROM boot and the renderer's vblank/resampler startup have different fixed
latencies, so align the first non-silent samples before comparing the WAVs.
Local waveform comparisons are mono: fold both captures down with the
arithmetic mean of their channels. This proves waveform shape and level only;
it does not prove stereo routing. Verify stereo separately by comparing the
left sides to each other and the right sides to each other without folding.

```bash
ffmpeg -i /tmp/mgba-se-pc-on.wav \
  -af 'pan=mono|c0=0.5*c0+0.5*c1' /tmp/mgba-se-pc-on-mono.wav
ffmpeg -i /tmp/poryaaaa-se-pc-on.wav \
  -af 'pan=mono|c0=0.5*c0+0.5*c1' /tmp/poryaaaa-se-pc-on-mono.wav
```

Before comparing volume, prove that the mono window contains the same waveform:
remove DC, search for the best sample lag, fit one gain scalar, and require both
positive normalized correlation and a low gain-fitted null residual. A polarity
inversion is a failure. Only compare raw peak or RMS inside a window that passes
that waveform check. Do not normalize either capture.

The pair helper reads `midi.cfg`, runs both capture paths at 65536 Hz, applies
the same hardware solo mask, and writes a manifest with commands and hashes:

```bash
tools/mgba-reference/capture_song_pair.sh \
  --decomp /Users/spencer/dev/hearth-test \
  --song se_pc_on \
  --solo sq1
```

Use the checked-in comparator for the mono fold, lag search, correlation,
gain-fitted residual, and raw RMS ratio. Window positions are sample indices:

```bash
tools/mgba-reference/waveform_compare.py \
  build/mgba-reference-pairs/se_pc_on-solo-sq1/mgba.wav \
  build/mgba-reference-pairs/se_pc_on-solo-sq1/poryaaaa.wav \
  --reference-start 6780 \
  --candidate-start 1121 \
  --reference-length 512 \
  --max-lag 64
```

For a song matrix, the coverage reporter scans onset-relative mono windows,
requires audible activity on both sides, and emits one JSON object per pair.
It selects the earliest passing window and reports raw RMS level only when
correlation and gain-fitted residual prove that the selected windows contain
the same wave shape:

```bash
python3 tools/mgba-reference/coverage_report.py \
  build/mgba-reference-pairs/mus_weather_groudon-solo-sq1 \
  build/mgba-reference-pairs/mus_weather_groudon-solo-sq2
```

Use a larger explicit `--scan-span` for songs whose first note contains a
sweep or attack transient; the selected onset-relative offset is included in
the result so the matching evidence remains visible.

The onset-relative earliest-window scan is only a local waveform check. Square
waves repeat, so it can select the same wave shape from different musical
events or envelope stages and produce a false whole-song RMS difference. Do
not use that result alone for a whole-song level or timing conclusion. Longer
song comparisons must first align corresponding musical neighborhoods or
track their sequence over time, then run the strict waveform check within
those neighborhoods. A coverage pass proves only the reported local wave
window, not the entire song timeline or every envelope transition; the JSON
labels this scope explicitly.

Stereo parity also requires independent left- and right-channel checks. A mono
fold can hide reversed routing or collapse. Capture isolated hardware channels
with `--solo`, compare each output side to the corresponding mGBA side, and
confirm that sides expected to be silent contain zero PCM.

## Verify

The smoke test records the known square-1 and square-2 entries at indices 1 and
4 in `voicegroup_vs_wild`, rejects identical output, records `se_pc_on` and
`se_door` through separate audio-only ROMs, and checks distinct PSG-only and
DirectSound-only `mus_slateport` captures. It checks recorder health, not
waveform parity, and skips cleanly if the external ROM/ELF is absent.

```bash
tools/mgba-reference/smoke_test.sh /Users/spencer/dev/hearth-test
```

The recorder treats a silent capture, an MP2K initialization timeout, or a
failed `MPlayStart` as errors. On success it prints the mGBA version, frontend
rate, peak, and RMS.

## Reproduce the isolated DirectSound mismatch

The dedicated fixture keeps `se_pc_on`'s compiled notes and timing but replaces
its voicegroup with one `voice_directsound` entry at program 0. It builds and
captures private ROM/MIDI/project copies under `build/`; the source decomp is
not modified. The comparison searches integer lag in the time domain, requires
onset/sustain/release lag agreement, and writes aligned and difference WAVs.

```bash
tools/mgba-reference/capture_directsound_fixture.sh
```

The command is intentionally red until DirectSound parity meets the declared
level, correlation, and gain-fitted-residual thresholds. A red comparison with
`alignment_passed: true` means the captures match in time but not waveform.

## Driver lifecycle validation

The opt-in driver lifecycle matrix differentially validates retained transaction
emission and hardware interpretation for Programmable Sound Wave (PSW),
DirectSound, Square 1, and Square 2 against the pinned full-ROM mGBA reference.
Each fixed scenario drives the real MP2K driver on the reference side and only
public `M4ADriver` controls on the candidate side.

### Build inputs and targets

The reference recorder requires a patched checkout of mGBA revision
`afd6f14eaf8bd35214ed3fb9dc69a92bfc3877a9`; installed system mGBA libraries
are not an acceptable substitute. Configure the package with that checkout:

```bash
cd packages/poryaaaa
cmake -B build \
  -DPORYAAAA_BUILD_MGBA_REFERENCE=ON \
  -DPORYAAAA_MGBA_SOURCE=/path/to/mgba-audio-reference
cmake --build build --target mgba_mp2k_reference
cmake --build build --target mgba_audio_trace_replay
cmake --build build --target poryaaaa_driver_trace
cmake --build build --target poryaaaa_audio_trace
```

The external decomp is also required. It is deliberately not built or changed
by this harness; the expected local checkout is
`/Users/spencer/dev/hearth-test`, with its `pokeemerald-hearth.gba` and
`pokeemerald-hearth.elf` already built.

### Commands and fixed matrix

The matrix is opt-in: it is not part of the default package build or test run.
It runs every fixed fixture/scenario cell twice independently:

```bash
python3 tools/mgba-reference/validate_driver_matrix.py \
  --decomp /Users/spencer/dev/hearth-test \
  --output-dir build/driver-validation/lifecycle
```

`validate_driver.py` runs one cell. It derives the family fail-closed from the
resolved ToneData; callers do not select a family:

```bash
python3 tools/mgba-reference/validate_driver.py \
  --decomp /Users/spencer/dev/hearth-test \
  --voicegroup voicegroup_rg_poke_center \
  --voice 4 \
  --scenario start \
  --output-dir build/driver-validation/directsound-start
```

Both commands accept `--note`, `--velocity`, `--volume`, and `--pan` controls
(each `0` through `127`). The fixed scenario enum is `start`, `envelope`,
`pitch`, `volume-pan`, `retrigger`, and `release`. The candidate command used
by the validator is `poryaaaa_driver_trace PROJECT_ROOT VOICEGROUP VOICE_INDEX
--scenario SCENARIO --trace-output FILE` with the same optional controls.
`driver_compare.py REFERENCE.trace CANDIDATE.trace --family FAMILY --output
RESULT.json` is the standalone transaction comparator; normal validation
derives its profile from manifests rather than accepting a family flag.

The baseline has 30 cells (five fixtures times six scenarios):

| Family | Canonical fixture |
| --- | --- |
| PSW normal | `voicegroup_aa_girl:4` |
| PSW alternate | `voicegroup_poke_center:1` |
| DirectSound | `voicegroup_rg_poke_center:4` |
| Square 1 | `voicegroup_vs_wild:1` |
| Square 2 | `voicegroup_vs_wild:4` |

### Gates and diagnostics

- `transaction_exact` requires exact retained event order, kind, width, address,
  value, and same-cycle order. Its projections are PSW NR30--NR34, NR50/NR51,
  and Wave RAM; Sq1 `0x04000060`--`0x04000065` plus routing; Sq2
  `0x04000068`--`0x0400006D` plus routing; and DirectSound FIFO A/B words,
  TIMER records, and routing/setup.
- `payload_exact` independently checks the effective family payload: PSW wave
  bytes, Sq1 sweep/duty/envelope, Sq2 duty/envelope/frequency, or DirectSound
  FIFO streams and setup. Equal final bytes never excuse a different
  transaction shape.
- `logical_state_exact` finds the first divergent post-ordinal family state.
  Sq1 retains NR10 sweep separately, Sq2 has no fabricated sweep state, and
  DirectSound retains FIFO queue/head/held-sample and timer state.
- `reference_hardware_exact` and `candidate_hardware_exact` require the
  respective trace to replay bit/cycle-identically in pinned mGBA and
  poryaaaa. `reference_native_exact` additionally requires reference trace
  replay to match the full-ROM native capture.

Comparator and replay reports retain the first transaction divergence or first
native-sample divergence, respectively, with their source ordinal and observed
cycle. Cycle deltas and SAMPLE-boundary crossings are diagnostics only:
transaction equality never claims absolute ARM instruction-cycle parity.
These gates also do not claim reference-versus-candidate whole-engine parity or
use that comparison as a pass gate.

### Published artifacts

Successful matrix output is published atomically only after both runs agree on
canonical manifests and artifact hashes. A gate or infrastructure failure keeps
the complete staged diagnostics in the sibling
`build/driver-validation/lifecycle.failed/` directory.

```text
build/driver-validation/lifecycle/
├── matrix_report.json
└── cases/
    └── directsound-start/
        ├── first/
        │   ├── manifest.json
        │   ├── reference.trace
        │   ├── candidate.trace
        │   ├── candidate.trace.manifest.json
        │   ├── reference-native.{pcm,cycles,json}
        │   ├── reference-mgba.{pcm,cycles,json}
        │   ├── reference-pory.{pcm,cycles,json}
        │   ├── candidate-mgba.{pcm,cycles,json}
        │   ├── candidate-pory.{pcm,cycles,json}
        │   ├── driver-compare.json
        │   ├── reference-hardware-compare.json
        │   ├── candidate-hardware-compare.json
        │   └── reference-native-compare.json
        └── repeat/
            └── ...
```