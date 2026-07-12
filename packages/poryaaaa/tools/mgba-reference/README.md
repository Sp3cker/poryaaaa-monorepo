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
