# mgba-headless channel-mute capture build

This directory contains a local patched mGBA clone for reference-audio
capture work.

## What changed

- `src/platform/headless-main.c`
  - Adds `--audio-out FILE` for direct stereo s16 WAV capture.
  - Adds `--sample-rate HZ` and `--duration-seconds S` /
    `--total-duration-seconds S` so reference captures can match
    `poryaaaa_render --sample-rate` and stop cleanly without an external
    interrupt.
  - Adds `--solo LIST` and `--mute LIST` for discrete channel isolation.
  - Adds `--hold-a-frames N` to hold the A button for N emulated frames
    after loading the ROM/savestate, useful for advancing a menu state before
    capture.
  - Adds `--tap-a-frames START,DURATION` to issue an A-button edge after the
    state has run for `START` frames.
  - Loads `-t` savestates with Qt-like extra state flags for savedata, cheats,
    and RTC instead of the upstream headless/perf-test `flags=0` path.
  - Uses mGBA's existing `core->enableAudioChannel()` path, which maps GBA
    channels to PSG 1-4 and DirectSound FIFO A/B.
- `src/gba/cart/ereader.c`
  - Adds the missing VFS include needed by this local build configuration.

## Channels

Supported channel names:

- `ch1`, `sq1`, `square1`
- `ch2`, `sq2`, `square2`
- `ch3`, `wave`
- `ch4`, `noise`
- `fifo-a`, `fifoa`, `a`
- `fifo-b`, `fifob`, `b`
- groups: `psg`, `directsound`, `fifo`, `dma`, `all`

Examples:

```bash
tools/captures/mgba-headless-channel-mute/build/mgba-headless \
  -t /path/to/state.ss2 \
  --audio-out tools/captures/reference_wave.wav \
  --sample-rate 44100 \
  --duration-seconds 30 \
  --solo wave \
  /path/to/rom.gba
```

```bash
tools/captures/mgba-headless-channel-mute/build/mgba-headless \
  -t /path/to/state.ss2 \
  --audio-out tools/captures/reference_no_directsound.wav \
  --sample-rate 44100 \
  --duration-seconds 30 \
  --mute directsound \
  /path/to/rom.gba
```

```bash
tools/captures/mgba-headless-channel-mute/build/mgba-headless \
  -t /path/to/menu_state.ss1 \
  --hold-a-frames 30 \
  --audio-out tools/captures/reference_after_a.wav \
  --sample-rate 44100 \
  --duration-seconds 30 \
  /path/to/rom.gba
```

```bash
tools/captures/mgba-headless-channel-mute/build/mgba-headless \
  -t /path/to/menu_state.ss1 \
  --tap-a-frames 60,10 \
  --audio-out tools/captures/reference_after_delayed_a.wav \
  --sample-rate 44100 \
  --duration-seconds 30 \
  /path/to/rom.gba
```

## Build command

```bash
cmake -S tools/captures/mgba-headless-channel-mute/mgba \
  -B tools/captures/mgba-headless-channel-mute/build \
  -DBUILD_HEADLESS=ON \
  -DBUILD_QT=OFF \
  -DBUILD_SDL=OFF \
  -DBUILD_GL=OFF \
  -DBUILD_GLES2=OFF \
  -DBUILD_GLES3=OFF \
  -DUSE_FFMPEG=ON \
  -DUSE_LIBZIP=OFF \
  -DUSE_PNG=ON \
  -DUSE_LUA=OFF \
  -DUSE_EDITLINE=OFF \
  -DUSE_ELF=OFF \
  -DUSE_MINIZIP=OFF \
  -DUSE_SQLITE3=OFF

cmake --build tools/captures/mgba-headless-channel-mute/build \
  --target mgba-headless -j 8
```

The local binary currently links against Homebrew FFmpeg 7 libraries
(`libavcodec.61`), unlike the older `mgba-headless-audio-out` bundle that
expects FFmpeg 8 (`libavcodec.62`).

PNG support must stay enabled: mGBA GUI slot savestates are PNG files with
the core state stored in custom chunks. Without `USE_PNG=ON`, `-t` will not
load those states.
