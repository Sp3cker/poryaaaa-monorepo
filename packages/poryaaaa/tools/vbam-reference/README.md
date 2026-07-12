# Headless VBA-M MP2K reference recorder

This recorder runs the existing audio-only song ROM through VBA-M's libretro
core and captures the PCM16 stereo frames delivered by `SoundRetro::write()`.
VBA-M fixes this frontend stream at 32,768 Hz.

## Build

Build the VBA-M libretro core in a separate VBA-M checkout, then build the
small headless frontend here:

```bash
make -C /Users/spencer/dev/cProjects/visualboyadvance-m/src/libretro

cd packages/poryaaaa
cmake -B build \
  -DPORYAAAA_BUILD_VBAM_REFERENCE=ON \
  -DPORYAAAA_VBAM_SOURCE=/Users/spencer/dev/cProjects/visualboyadvance-m
cmake --build build --target vbam_mp2k_reference
```

## Record an audio-only song ROM

```bash
tools/vbam-reference/record_song.sh \
  --decomp /Users/spencer/dev/hearth-test \
  --song se_door \
  --core /Users/spencer/dev/cProjects/visualboyadvance-m/src/libretro/vbam_libretro.dylib \
  --output /tmp/vbam-se-door.wav
```

The adjacent manifest records the VBA-M version, sample format, fixed sample
rate, interpolation/filtering settings, and all six channel switches. Channel
names are `sq1`, `sq2`, `wave`, `noise`, `fifo-a`, and `fifo-b`; use `--solo`
or `--mute` with a comma-separated list.

Defaults match the VBA-M libretro core: interpolation enabled, filtering 5,
and all channels enabled. Captures fail if the core reports a rate other than
32,768 Hz or produces only silence.

## Verify

The smoke test builds `se_pc_on` as an audio-only ROM, isolates SQ1, and checks
the non-silent WAV header, exact frame count, and recorded core settings:

```bash
tools/vbam-reference/smoke_test.sh \
  /Users/spencer/dev/hearth-test \
  /Users/spencer/dev/cProjects/visualboyadvance-m/src/libretro/vbam_libretro.dylib
```
