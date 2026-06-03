# C++23 Non-Audio Build Probe Design

## Purpose

Move the existing non-audio C++ surfaces toward C++23 without changing audio-engine behavior. The first implementation phase is a build-policy probe only: compile the relevant C++ code as C++23, discover and fix required compatibility issues, and verify that the C audio engine remains C11.

After the C++23 build is understood and stable, a second implementation phase may introduce small internal C++ cleanup, starting with `std::span` in the GUI.

## Scope

The C++23 build probe applies to project-owned C++ and Objective-C++ surfaces:

- `poryaaaa`, for plugin/GUI C++ sources such as `plugin/m4a_gui.cpp`, `plugin/imgui_impl_pugl.cpp`, and `plugin/m4a_engine_recorder.cpp`
- `recorder`, for `plugin/recorder/recorder_core.cpp` and `plugin/recorder/smf_writer.cpp`
- `poryaaaa_unit_tests`, only for the C++ recorder bridge it compiles

The first phase does not intentionally modernize source code. Source edits in phase 1 are limited to fixes needed for a clean C++23 build.

## Out Of Scope

These modules remain C11 in the first phase:

- `plugin/m4a_engine.c`
- `plugin/m4a/`
- `plugin/hw_audio/`
- `plugin/voicegroup/`
- `cmd/poryaaaa_render.c`
- C test sources

These larger changes are also out of scope for the first phase:

- converting the audio engine to C++
- changing public C ABI headers
- replacing `third_party/midifile`
- refactoring GUI teardown to RAII
- changing audio rendering, frequency math, timing, or hardware behavior

## CMake Design

Keep `CMAKE_C_STANDARD` at C11. Stop relying on a broad global C++ standard as the meaningful policy for this migration.

Use target-scoped C++23 for the affected C++ targets, preferably through `target_compile_features(... PRIVATE cxx_std_23)` or equivalent target properties. The implementation should keep the migration focused on project-owned C++ surfaces and avoid forcing C++23 onto unrelated C-only targets.

If vendored C++ sources compiled into a target inherit C++23, the first phase should treat any resulting build issue as compatibility evidence. The fix should be the smallest local build or source adjustment that preserves existing behavior.

## Phase 1 Validation

Phase 1 succeeds only when the build policy is verified with fresh output:

1. Configure the build.
2. Build `poryaaaa`.
3. Build and run `poryaaaa_unit_tests`.
4. Inspect generated flags to confirm affected C++ sources use C++23.
5. Inspect generated flags to confirm engine C sources still use C11.
6. On macOS, confirm the `poryaaaa.clap` post-build copy installs the freshly built bundle to the configured CLAP plugin directory.

Any compiler warnings or compatibility failures introduced by C++23 should be recorded before fixes are made, so later cleanup is not confused with standard-migration work.

## Phase 2 Cleanup Direction

After the C++23 build is known and stable, the first cleanup target is internal `std::span` use in `plugin/m4a_gui.cpp`.

The public C ABI remains raw pointer/count based. Internal GUI helpers may convert stored pointer/count pairs into non-owning bounded views for:

- live/original voice data used while rendering voice controls
- DirectSound project assets
- programmable-wave project assets

This cleanup should be behavior-preserving. It should not reach into audio rendering or loader ownership.

## Risks

The main build risk is target-scope leakage: C++23 may affect vendored C++ sources already compiled into `poryaaaa` or `recorder`. This is acceptable as discovery during phase 1, but fixes should stay minimal.

The main design risk is mixing the build probe with code modernization. The implementation should keep phase 1 and phase 2 separate so failures can be attributed clearly.

The audio-risk budget for phase 1 is zero. If validation shows audio-engine behavior changed, the implementation has exceeded the approved scope.

## Success Criteria

- Non-audio C++ targets compile as C++23.
- Audio engine and other scoped-out C modules continue compiling as C11.
- `poryaaaa` builds successfully.
- `poryaaaa_unit_tests` builds and passes.
- No public C ABI changes are made in phase 1.
- Phase 2 `std::span` cleanup is not started until the C++23 build state is verified.
