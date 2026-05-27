# AGENTS.md - poryaaaa

Instructions for coding agents working in this repository.

## Purpose

`poryaaaa` is a Game Boy Advance m4a audio synthesizer with two shipped artifacts:

- `poryaaaa.clap`: CLAP instrument plugin
- `poryaaaa_render`: CLI MIDI-to-WAV renderer

Core audio logic is C11. GUI and platform entry points are C++.

## Read This First

- Prefer small, direct changes, but evaluate the long-term value of refactoring and adding abstractions for architectural purposes and do so if necessary.
- Preserve hardware-accurate behavior. This project cares about matching the GBA engine, not approximating it.
- Avoid vendor and generated trees unless the task explicitly requires them.
- Do not add dependencies that require reworking the submodule layout without explicitly calling that out.
- Do not edit vendor code to work around local project bugs unless the task explicitly requires vendor changes.

## Where To Work

Start in these paths unless the task clearly points elsewhere:

- Engine API and MIDI routing: `plugin/m4a_engine.c`, `plugin/m4a_engine.h`
- M4A driver behavior: `plugin/m4a/`
- Hardware audio mixing and reverb: `plugin/hw_audio/`
- Frequency tables and pitch math: `plugin/m4a_tables.c`, `plugin/m4a_tables.h`
- Voicegroup and sample discovery: `plugin/voicegroup_loader.c`, `plugin/voicegroup_loader.h`
- CLAP plugin behavior and state: `plugin/m4a_plugin.c`, `plugin/m4a_plugin.h`
- Parameters: `plugin/m4a_params.c`, `plugin/m4a_params.h`
- GUI: `plugin/m4a_gui.cpp`, `plugin/m4a_gui.h`, `plugin/imgui_impl_pugl.cpp`, `plugin/imgui_impl_pugl.h`
- CLI renderer: `cmd/poryaaaa_render.c`
- Tests: `test/test_engine.c`, `test/test_wav_export.c`
- Build changes only when necessary: `CMakeLists.txt`

Vendor trees and build outputs are large. Search narrowly — scope to `plugin/`, `cmd/`, and `test/` first before going broader.

## Where Not To Work

Do not edit these unless the task explicitly requires it:

- `clap-sdk/`
- `imgui/`
- `third_party/`
- `build*/`

If you must touch one of these, ask for consent first, explaining why.

## Build And Test

Configure and build:

```bash
cmake -B build
cmake --build build --target poryaaaa
```

On macOS, the `poryaaaa` target installs the freshly built CLAP bundle after every successful build. By default it copies to the user CLAP directory, `~/Library/Audio/Plug-Ins/CLAP/poryaaaa.clap`, so normal builds do not require elevated permissions.

```bash
cmake --build build --target poryaaaa
```

To install system-wide instead, configure with `-DPORYAAAA_CLAP_INSTALL_DIR=/Library/Audio/Plug-Ins/CLAP` and run the build with permissions that can write there. If install fails, say that the built plugin was not installed and include the stale/installed path status.

Primary validation command after code changes:

```bash
cmake --build build --target poryaaaa_unit_tests
./build/poryaaaa_unit_tests
```

Relevant targets:

- `poryaaaa`: CLAP plugin bundle
- `poryaaaa_render`: CLI renderer
- `poryaaaa_unit_tests`: engine/unit test binary
- `poryaaaa_test`: WAV export integration test (not a general unit test binary)

## Validation Expectations

A task is not complete until all applicable items are done:

1. Build the touched target or targets.
2. Run `poryaaaa_unit_tests`.
3. If the CLAP plugin target was built or plugin/audio behavior changed, verify the post-build copy installed the freshly built `poryaaaa.clap` to the configured CLAP plugin directory before DAW/manual validation.
4. Run any additional affected test or executable if the change is integration-sensitive.
5. If validation or install cannot run, say exactly what was skipped and why.
6. Report any platform caveats or follow-up work.

Testing rules:

- Always add or update tests for bug fixes and new behavior.
- Engine or algorithm work usually belongs in `test/test_engine.c`.
- Loader, renderer, or asset-dependent behavior may use a dedicated test under `test/`.
- Tests that require an external decomp project must skip gracefully when assets are unavailable.

## Coding Rules

- Match the style of the file you edit. Use tabs and K&R braces.
- Use `snake_case` for functions and variables, `ALL_CAPS` for macros and constants.
- Do not reformat unrelated code. An easy-to-explain git diff is important.

Example style:

```c
static void process_channel(M4aChannel *ch, int16_t *buf, uint32_t count) {
	if (!ch->active) {
		return;
	}
	for (uint32_t i = 0; i < count; i++) {
		buf[i] += render_sample(ch);
	}
}
```

## Architecture Invariants

Do not break these:

### Timing

- Engine logic ticks at the GBA VBlank rate, about 59.7 Hz.
- Audio rendering runs at the host sample rate, typically 44100 Hz.
- Do not conflate engine tick timing with audio sample timing.

### Audio Channels

- Up to 12 PCM DirectSound channels
- 4 CGB channels: 2 square, 1 programmable wave, 1 noise
- Channel state lives in `M4aEngine`

### Frequency Math

- Frequency scaling must match GBA `MidiKeyToFreq()` behavior.
- Use the lookup tables in `plugin/m4a_tables.c` plus the fixed-point multiply path already in the codebase.
- Do not replace this with floating-point approximations.

### Voicegroup Loader

- `plugin/voicegroup_loader.c` must continue supporting both:
  - pokeemerald-style per-file layouts
  - pokefirered-style monolithic `voice_groups.inc`
- Asset-loading work needs both code-path preservation and graceful behavior when expected files are absent.

### Plugin State And Config

- Runtime defaults come from `poryaaaa.cfg`.
- DAW-saved plugin state goes through `CLAP_EXT_STATE` in `plugin/m4a_plugin.c`.
- When adding plugin behavior, check existing CLAP extensions before inventing custom mechanisms.

Currently implemented CLAP extensions: `AUDIO_PORTS`, `NOTE_PORTS`, `PARAMS`, `STATE`, `GUI`, `TIMER_SUPPORT`

## Platform Notes

- macOS is the default correctness target.
- If a change touches platform-specific code, note Windows and Linux follow-up work if needed, but do not block macOS completion on them.
- Use existing platform guards such as `__APPLE__` and `_WIN32`.
- The GUI uses Dear ImGui + Pugl with Metal on macOS and OpenGL elsewhere. Do not assume GLFW.

## Known Gaps

Already known — do not treat as surprise regressions unless the task is specifically about them:

- High-DPI or scale-aware window sizing
- File browser dialog for Project Root selection
- Full MIDI-to-WAV regression coverage
- Performance profiling and optimization
