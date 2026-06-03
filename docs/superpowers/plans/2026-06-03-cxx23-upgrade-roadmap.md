# C++23 Upgrade Roadmap Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Continue the C++23 migration in small, verified steps that reduce pointer/count risk and ownership ambiguity without disturbing GBA audio behavior.

**Architecture:** Keep the audio engine, renderer, and voicegroup loader in C11 until a stronger reason exists to convert them. Apply C++23 only to project-owned C++ targets first, then use C++23 features where they remove code or make ownership and bounds clearer. Add native macOS memory/lifecycle validation before deeper GUI or ownership refactors.

**Tech Stack:** CMake, AppleClang, C11 engine code, C++23 GUI/recorder code, CLAP, Pugl, Dear ImGui, macOS `leaks`/`heap`/`vmmap`, optional ASan/UBSan.

---

## Current State Snapshot

The following work is already in progress or complete in the current working tree:

- `poryaaaa`, `recorder`, and `poryaaaa_unit_tests` use target-scoped `cxx_std_23`.
- C audio modules still compile as C11.
- `plugin/m4a_gui.cpp` uses `std::span` internally for GUI voice and project-asset views.

Do not remove unrelated untracked build directories unless explicitly requested.

### Task 1: Commit Current C++23 Build And GUI Span Cleanup

**Files:**
- Modify: `CMakeLists.txt`
- Modify: `plugin/recorder/CMakeLists.txt`
- Modify: `plugin/m4a_gui.cpp`

- [ ] **Step 1: Review the current diff**

Run:

```bash
git diff -- CMakeLists.txt plugin/recorder/CMakeLists.txt plugin/m4a_gui.cpp
```

Expected: the diff only contains target-scoped C++23 settings and the GUI span-state cleanup. No audio-engine files are modified.

- [ ] **Step 2: Build the CLAP target**

Run:

```bash
cmake --build build-valgrind --target poryaaaa
```

Expected: `poryaaaa` builds successfully. On macOS, the post-build copy installs `poryaaaa.clap` to the configured CLAP plugin directory.

- [ ] **Step 3: Build and run unit tests**

Run:

```bash
cmake --build build-valgrind --target poryaaaa_unit_tests
./build-valgrind/poryaaaa_unit_tests
```

Expected: the unit-test binary reports `429/429 tests passed`.

- [ ] **Step 4: Commit the C++23 build/span cleanup**

Run:

```bash
git add CMakeLists.txt plugin/recorder/CMakeLists.txt plugin/m4a_gui.cpp
git commit -m "Use C++23 for non-audio targets and GUI spans"
```

Expected: only the three listed source/build files are committed.

### Task 2: Add A Native macOS Memory-Validation Build Path

**Files:**
- Create: `docs/memory-validation.md`

- [ ] **Step 1: Add documented sanitizer configuration commands**

Create `docs/memory-validation.md` with this content:

````markdown
# Memory Validation

## macOS Native Tools

Valgrind is not the native path on Apple Silicon macOS. Use AddressSanitizer and UndefinedBehaviorSanitizer for invalid memory access, and use `leaks`, `heap`, `vmmap`, and Instruments for allocation/leak analysis.

## ASan/UBSan Debug Build

```bash
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_FLAGS="-fsanitize=address,undefined" \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined" \
  -DCMAKE_OBJCXX_FLAGS="-fsanitize=address,undefined"

cmake --build build-asan --target poryaaaa_unit_tests
./build-asan/poryaaaa_unit_tests
```

## Malloc Stack Logging

Run a long-lived test process with malloc stack logging enabled:

```bash
MallocStackLogging=1 MallocStackLoggingNoCompact=1 \
./build-debug/poryaaaa_lifecycle_tests --pause-before-exit
```

Inspect it from another terminal:

```bash
leaks <pid>
heap <pid>
vmmap <pid>
```

## Instruments

Use Instruments Allocations and Leaks for GUI or CLAP lifecycle analysis. Prefer Allocations when memory grows across repeated open/close cycles, and Leaks when looking for definitely unreachable allocations.
````

- [ ] **Step 2: Verify the ASan command configures**

Run:

```bash
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_FLAGS="-fsanitize=address,undefined" \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined" \
  -DCMAKE_OBJCXX_FLAGS="-fsanitize=address,undefined"
```

Expected: CMake configures successfully. If AppleClang rejects a sanitizer flag, update `docs/memory-validation.md` with the working command observed locally.

- [ ] **Step 3: Build and run unit tests under sanitizers**

Run:

```bash
cmake --build build-asan --target poryaaaa_unit_tests
./build-asan/poryaaaa_unit_tests
```

Expected: tests pass with no sanitizer reports. If the dynamic sanitizer runtime is blocked by macOS security settings, document the exact failure in `docs/memory-validation.md`.

- [ ] **Step 4: Commit the memory-validation docs**

Run:

```bash
git add docs/memory-validation.md
git commit -m "Document native memory validation workflow"
```

Expected: only memory-validation documentation is committed unless CMake needs a small project option for sanitizer support.

### Task 3: Add A Headless Lifecycle Stress Executable

**Files:**
- Modify: `CMakeLists.txt`
- Create: `test/test_lifecycle.c`

- [ ] **Step 1: Add the test executable skeleton**

Create `test/test_lifecycle.c`:

```c
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

#include "m4a_engine.h"
#include "m4a_engine_recorder.h"

static int run_one_iteration(void)
{
	M4AEngine engine;
	if (!m4a_engine_init(&engine, 44100.0f)) {
		fprintf(stderr, "m4a_engine_init failed\n");
		return 1;
	}

	M4ARecorder *recorder = m4a_recorder_create();
	if (!recorder) {
		fprintf(stderr, "m4a_recorder_create failed\n");
		m4a_engine_destroy(&engine);
		return 1;
	}

	m4a_recorder_set_sample_rate(recorder, 44100.0);
	m4a_recorder_push(recorder, 0, 0x90, 60, 100);
	m4a_recorder_advance(recorder, 512);
	m4a_recorder_destroy(recorder);
	m4a_engine_destroy(&engine);
	return 0;
}

int main(int argc, char **argv)
{
	int iterations = 10000;
	bool pause_before_exit = false;

	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--iterations") == 0 && i + 1 < argc) {
			iterations = atoi(argv[++i]);
		} else if (strcmp(argv[i], "--pause-before-exit") == 0) {
			pause_before_exit = true;
		}
	}

	for (int i = 0; i < iterations; i++) {
		if (run_one_iteration() != 0)
			return 1;
	}

	printf("lifecycle iterations passed: %d\n", iterations);
	if (pause_before_exit) {
		printf("pid %d paused for memory inspection\n", getpid());
		fflush(stdout);
		sleep(60);
	}

	return 0;
}
```

- [ ] **Step 2: Wire the executable in CMake**

Add this target in `CMakeLists.txt` near the other tests:

```cmake
add_executable(poryaaaa_lifecycle_tests
    test/test_lifecycle.c
    plugin/m4a_engine_recorder.cpp
    ${ENGINE_SOURCES}
)

target_compile_features(poryaaaa_lifecycle_tests PRIVATE cxx_std_23)
target_include_directories(poryaaaa_lifecycle_tests PRIVATE plugin)
target_link_libraries(poryaaaa_lifecycle_tests PRIVATE m voicegroup_loader recorder)

if(M4A_DRIVER_V2)
    target_link_libraries(poryaaaa_lifecycle_tests PRIVATE m4a_driver)
    target_compile_definitions(poryaaaa_lifecycle_tests PRIVATE M4A_DRIVER_V2=1)
endif()

if(HW_AUDIO_V2)
    target_link_libraries(poryaaaa_lifecycle_tests PRIVATE hw_audio)
    target_compile_definitions(poryaaaa_lifecycle_tests PRIVATE HW_AUDIO_V2=1)
endif()
```

- [ ] **Step 3: Build and run the lifecycle test**

Run:

```bash
cmake --build build-valgrind --target poryaaaa_lifecycle_tests
./build-valgrind/poryaaaa_lifecycle_tests --iterations 10000
```

Expected: output includes `lifecycle iterations passed: 10000`.

- [ ] **Step 4: Run a native leak inspection pass**

Run the process:

```bash
MallocStackLogging=1 MallocStackLoggingNoCompact=1 \
./build-valgrind/poryaaaa_lifecycle_tests --iterations 10000 --pause-before-exit
```

In another terminal, use the printed pid:

```bash
leaks <pid>
heap <pid>
vmmap <pid>
```

Expected: record whether `leaks` reports definite leaks. If it reports leaks from system frameworks or sanitizer/runtime setup, document them separately from project-owned allocations.

- [ ] **Step 5: Commit the lifecycle harness**

Run:

```bash
git add CMakeLists.txt test/test_lifecycle.c docs/memory-validation.md
git commit -m "Add lifecycle memory validation harness"
```

Expected: the commit adds the lifecycle executable and updates memory-validation docs with observed local usage.

### Task 4: Convert Recorder Fixed Arrays To `std::array`

**Files:**
- Modify: `plugin/recorder/smf_writer.cpp`

- [ ] **Step 1: Replace fixed MIDI-channel arrays**

In `plugin/recorder/smf_writer.cpp`, include `<array>` and replace:

```cpp
int channelTrack[16];
int lastTickPerChannel[16] = {0};
int lastCcValue[16][128];
int lastPcValue[16];
```

with:

```cpp
std::array<int, 16> channelTrack;
std::array<int, 16> lastTickPerChannel {};
std::array<std::array<int, 128>, 16> lastCcValue;
std::array<int, 16> lastPcValue;
```

Initialize with `.fill(-1)` for arrays that use `-1` as the sentinel.

- [ ] **Step 2: Build and run unit tests**

Run:

```bash
cmake --build build-valgrind --target poryaaaa_unit_tests
./build-valgrind/poryaaaa_unit_tests
```

Expected: `429/429 tests passed`.

- [ ] **Step 3: Commit the recorder array cleanup**

Run:

```bash
git add plugin/recorder/smf_writer.cpp
git commit -m "Use std::array in SMF writer state"
```

Expected: only `plugin/recorder/smf_writer.cpp` is committed.

### Task 5: Evaluate Replacing `third_party/midifile`

**Files:**
- Create: `docs/smf-writer-replacement-notes.md`

- [ ] **Step 1: Document the current dependency surface**

Create `docs/smf-writer-replacement-notes.md` with sections for:

```markdown
# SMF Writer Replacement Notes

## Current Calls Into MidiFile

- `addTracks`
- `setTicksPerQuarterNote`
- `addTrackName`
- `addTimeSignature`
- `addMarker`
- `addTempo`
- `addEvent`
- `sortTrackNoteOnsBeforeOffs`
- `write`

## Project-Specific Ordering Constraint

Music tracks must preserve same-tick controller ordering for GBA XCMD prefix/value pairs. The conductor track still needs sorting to avoid negative delta times.

## Replacement Requirements

- write SMF format 1
- write variable-length quantities
- write tempo meta events
- write marker meta events
- write track names and time signature
- preserve event order within music tracks
- sort only conductor events as needed

## C++23 Facilities That Help

- `std::array` for fixed channel state
- `std::vector<std::byte>` for binary chunks
- `std::endian` and `std::byteswap` for explicit byte order when useful
```

- [ ] **Step 2: Commit the evaluation note**

Run:

```bash
git add docs/smf-writer-replacement-notes.md
git commit -m "Document SMF writer replacement requirements"
```

Expected: documentation only. Do not replace `third_party/midifile` in this task.

### Task 6: Defer GUI RAII Until Lifecycle Coverage Exists

**Files:**
- Modify: `docs/memory-validation.md`

- [ ] **Step 1: Add the RAII gate**

Append this section to `docs/memory-validation.md`:

```markdown
## GUI RAII Refactor Gate

Do not refactor Pugl, ImGui, Metal, or GUI teardown ownership until `poryaaaa_lifecycle_tests` exists and passes under the native memory-validation workflow. GUI teardown order is host/platform-sensitive, so ownership cleanup needs lifecycle evidence before code churn.
```

- [ ] **Step 2: Commit the gate**

Run:

```bash
git add docs/memory-validation.md
git commit -m "Gate GUI RAII work on lifecycle validation"
```

Expected: documentation only.

### Task 7: Keep Audio Engine Conversion Deferred

**Files:**
- Modify: `docs/superpowers/plans/2026-06-03-cxx23-upgrade-roadmap.md`

- [ ] **Step 1: Re-check before any audio-engine conversion**

Before converting any file under `plugin/m4a_engine.c`, `plugin/m4a/`, or `plugin/hw_audio/`, update this roadmap with:

```markdown
## Audio Engine Conversion Pre-Conditions

- lifecycle memory validation exists and passes
- C++ conversion target is a small isolated module
- tests cover the exact timing/audio behavior touched by the conversion
- public C API compatibility is preserved or explicitly redesigned
- generated flags prove C engine conversion is intentional, not target leakage
```

- [ ] **Step 2: Commit the pre-condition update**

Run:

```bash
git add docs/superpowers/plans/2026-06-03-cxx23-upgrade-roadmap.md
git commit -m "Document audio engine conversion preconditions"
```

Expected: documentation only.
