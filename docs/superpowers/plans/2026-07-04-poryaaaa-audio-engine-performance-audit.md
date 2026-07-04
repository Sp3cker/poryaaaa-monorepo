# Poryaaaa Audio Engine Performance Audit Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Measure where CPU time is spent in poryaaaa's C audio engine and produce an evidence-backed optimization report without changing audio behavior.

**Architecture:** Add one local benchmark executable that exercises the existing `m4a_driver` -> `hw_audio_render_events` -> `m4a_consume_writes` path and several chip-only scenarios. Use Release builds, repeatable command-line measurements, and profiler traces before proposing code changes. Keep the audit separate from optimization implementation.

**Tech Stack:** C23/C11-compatible project C, CMake `build/`, macOS `/usr/bin/time`, optional `xcrun xctrace`, existing `poryaaaa_unit_tests`, existing `poryaaaa_render`.

---

## Scope

- Audit only the C audio engine:
  - `packages/poryaaaa/plugin/m4a/`
  - `packages/poryaaaa/plugin/hw_audio/`
  - `packages/poryaaaa/plugin/m4a_engine.c`
  - `packages/poryaaaa/cmd/poryaaaa_render.c` only as an end-to-end host.
- Do not audit M4L device patching, TypeScript, Node, UI drawing, voicegroup parser performance, or DAW scheduling unless the engine measurements show they are needed.
- Do not optimize during the audit. The output is a report and a ranked list of optimization candidates.
- Preserve hardware-fidelity behavior. Any later optimization must prove output equivalence or explain a deliberate tolerance.
- Use the existing `build/` directory. Do not create `build-release`, `build-perf`, or other alternate build directories.

## Files

- Create: `packages/poryaaaa/bench/audio_engine_bench.c`
  - Small standalone benchmark harness.
  - Runs synthetic chip-only and driver-plus-chip scenarios.
  - Prints machine-readable per-scenario timing.
- Modify: `packages/poryaaaa/CMakeLists.txt`
  - Add `poryaaaa_audio_bench`.
  - Link it to `m4a_driver`, `hw_audio`, and `m` on non-MSVC, matching the existing target pattern.
- Create: `docs/performance/poryaaaa-audio-engine-audit.md`
  - Final audit report with environment, commands, raw results, profiler findings, and ranked candidates.
- Do not modify engine source files during the audit except to add temporary profiler markers if a later task explicitly proves they are required. If temporary markers are added, remove them before the final report.

---

### Task 1: Record Baseline Environment

**Files:**
- Create: `docs/performance/poryaaaa-audio-engine-audit.md`

- [ ] **Step 1: Capture repository state**

Run:

```bash
git status --short
git rev-parse --short HEAD
```

Record both outputs in `docs/performance/poryaaaa-audio-engine-audit.md` under `## Environment`.

- [ ] **Step 2: Capture toolchain and host**

Run:

```bash
uname -a
cmake --version
cc --version
sysctl -n machdep.cpu.brand_string 2>/dev/null || true
sysctl -n hw.ncpu 2>/dev/null || true
```

Record the output under `## Environment`.

- [ ] **Step 3: Configure the existing build directory for Release**

Run from `packages/poryaaaa`:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
```

Expected: configure completes and reuses `packages/poryaaaa/build`.

- [ ] **Step 4: Build current validation targets**

Run:

```bash
cmake --build build --target poryaaaa_unit_tests poryaaaa_render
```

Expected: both targets build. If `poryaaaa_unit_tests` later fails due unrelated voicegroup-core parity state, record the exact failure in the report and continue with benchmark build only.

---

### Task 2: Add the Benchmark Harness

**Files:**
- Create: `packages/poryaaaa/bench/audio_engine_bench.c`
- Modify: `packages/poryaaaa/CMakeLists.txt`

- [ ] **Step 1: Create the benchmark directory**

Run:

```bash
mkdir -p packages/poryaaaa/bench docs/performance
```

- [ ] **Step 2: Implement `audio_engine_bench.c`**

Create `packages/poryaaaa/bench/audio_engine_bench.c` with these responsibilities:

- Parse:
  - `--scenario all|chip-sq2|chip-pcm|driver-psg`
  - `--sample-rate HZ`, default `44100`
  - `--seconds SECONDS`, default `20`
  - `--warmup-seconds SECONDS`, default `2`
  - `--block FRAMES`, default `512`
  - `--repeat COUNT`, default `5`
- Allocate `float* left` and `float* right` for one block.
- Use `clock_gettime(CLOCK_MONOTONIC, &ts)` on POSIX and `timespec_get(&ts, TIME_UTC)` fallback where needed.
- For every measured run, print one CSV row:

```text
scenario,sample_rate,block,seconds,repeat,rendered_frames,elapsed_seconds,ns_per_frame,realtime_factor
```

- Implement scenarios:
  - `chip-sq2`: create `HwAudio`, send a canned SQ2 register batch once, then render empty batches for the requested frame count.
  - `chip-pcm`: create `HwAudio`, prefill `M4APcmRing` with a deterministic sawtooth, route DMA A/B through `SOUNDCNT_H`, then render for the requested frame count.
  - `driver-psg`: create `M4ADriver` and `HwAudio`, create a small `ToneData voices[128]` array with one `VOICE_SQUARE_2` program, start a sustained note, then loop `m4a_advance`, `hw_audio_render_events`, and `m4a_consume_writes`.

Keep the harness deterministic. Do not load project assets or MIDI files.

- [ ] **Step 3: Add the CMake target**

In `packages/poryaaaa/CMakeLists.txt`, add after `poryaaaa_render`:

```cmake
# ---- Audio Engine Benchmark ----
add_executable(poryaaaa_audio_bench
    bench/audio_engine_bench.c
    ${ENGINE_SOURCES}
)
poryaaaa_enable_msvc_c_atomics(poryaaaa_audio_bench)
poryaaaa_enable_project_warnings(poryaaaa_audio_bench)
target_include_directories(poryaaaa_audio_bench PRIVATE plugin)
target_link_libraries(poryaaaa_audio_bench PRIVATE voicegroup_loader m4a_driver hw_audio)
if(NOT MSVC)
    target_link_libraries(poryaaaa_audio_bench PRIVATE m)
endif()
```

Do not add this target to `_PORYAAAA_V2_TARGETS`, because it links `m4a_driver` and `hw_audio` directly in its own block.

- [ ] **Step 4: Build the benchmark**

Run from `packages/poryaaaa`:

```bash
cmake --build build --target poryaaaa_audio_bench
```

Expected: target builds with project warnings enabled.

---

### Task 3: Collect Repeatable Benchmark Numbers

**Files:**
- Modify: `docs/performance/poryaaaa-audio-engine-audit.md`

- [ ] **Step 1: Run short smoke benchmark**

Run:

```bash
./build/poryaaaa_audio_bench --scenario all --seconds 2 --warmup-seconds 1 --repeat 1
```

Expected: one CSV header plus one row per scenario. Confirm `realtime_factor` is greater than `1.0` for every scenario.

- [ ] **Step 2: Run the primary benchmark set**

Run:

```bash
./build/poryaaaa_audio_bench --scenario all --sample-rate 44100 --block 512 --seconds 20 --warmup-seconds 2 --repeat 10 > /tmp/poryaaaa-audio-44100.csv
./build/poryaaaa_audio_bench --scenario all --sample-rate 48000 --block 512 --seconds 20 --warmup-seconds 2 --repeat 10 > /tmp/poryaaaa-audio-48000.csv
./build/poryaaaa_audio_bench --scenario all --sample-rate 96000 --block 512 --seconds 20 --warmup-seconds 2 --repeat 10 > /tmp/poryaaaa-audio-96000.csv
```

Append the three CSV files to the report under `## Benchmark Results`.

- [ ] **Step 3: Run block-size sensitivity measurements**

Run:

```bash
./build/poryaaaa_audio_bench --scenario all --sample-rate 44100 --block 64 --seconds 20 --warmup-seconds 2 --repeat 10 > /tmp/poryaaaa-audio-block64.csv
./build/poryaaaa_audio_bench --scenario all --sample-rate 44100 --block 256 --seconds 20 --warmup-seconds 2 --repeat 10 > /tmp/poryaaaa-audio-block256.csv
./build/poryaaaa_audio_bench --scenario all --sample-rate 44100 --block 2048 --seconds 20 --warmup-seconds 2 --repeat 10 > /tmp/poryaaaa-audio-block2048.csv
```

Append the results under `## Block Size Sensitivity`.

- [ ] **Step 4: Capture process-level timing**

Run:

```bash
/usr/bin/time -lp ./build/poryaaaa_audio_bench --scenario all --sample-rate 44100 --block 512 --seconds 60 --warmup-seconds 2 --repeat 1 > /tmp/poryaaaa-audio-time.csv 2> /tmp/poryaaaa-audio-time.txt
```

Append `/tmp/poryaaaa-audio-time.txt` under `## Process Timing`.

---

### Task 4: Capture Profiler Evidence

**Files:**
- Modify: `docs/performance/poryaaaa-audio-engine-audit.md`

- [ ] **Step 1: Record a Time Profiler trace if `xctrace` is available**

Run:

```bash
command -v xcrun
xcrun xctrace list templates | rg "Time Profiler"
```

If both commands succeed, run:

```bash
xcrun xctrace record --template "Time Profiler" --output /tmp/poryaaaa-audio-engine.trace --launch -- ./build/poryaaaa_audio_bench --scenario all --sample-rate 44100 --block 512 --seconds 60 --warmup-seconds 2 --repeat 1
```

If `xctrace` is unavailable or fails due local permissions, record the exact failure in the report and continue with `/usr/bin/time` data.

- [ ] **Step 2: Extract top functions**

Using Instruments UI or `xctrace export` if available, record the top self-time and total-time functions under `## Profiler Findings`.

Classify functions into these buckets:

- `m4a_driver`: `m4a_advance`, `m4a_sound_main_ram`, `m4a_drv_pcm_*`, `m4a_trk_vol_pit_set`, CGB emit paths.
- `hw_audio`: `hw_audio_render_events`, `render_segment`, `render_internal_chunk`.
- `hw_resample`: `hw_resample_process`.
- `hw_psg`: `hw_psg_render`, `hw_psg_apply_event`.
- `hw_pcm`: `hw_pcm_render`, `hw_pcm_apply_event`.
- `hw_mix`: `hw_mix_render`, `hw_mix_apply_event`.
- `host-only`: WAV writing, MIDI parsing, voicegroup loading, file IO.

Do not rank host-only costs as audio-engine optimization candidates unless they dominate end-to-end export and the report labels them separately.

---

### Task 5: Run End-to-End Renderer Timing Separately

**Files:**
- Modify: `docs/performance/poryaaaa-audio-engine-audit.md`

- [ ] **Step 1: Find or create a stable input command**

Prefer an existing small MIDI and project root already used by local tests. If no stable asset is available, do not fabricate a project-root dependency. Instead, mark this section as blocked by missing local render fixture and keep the benchmark harness as the audit source of truth.

- [ ] **Step 2: Time `poryaaaa_render` when assets exist**

After selecting the fixture, write the exact `poryaaaa_render` command into the report before running it. Use real paths from the selected fixture.

Record:

- input MIDI path
- voicegroup
- rendered duration
- wall time
- user CPU time
- system CPU time
- peak memory

Keep this separate from synthetic benchmark rows because it includes MIDI parsing, voicegroup loading, WAV writing, and allocation behavior.

---

### Task 6: Produce the Audit Report

**Files:**
- Modify: `docs/performance/poryaaaa-audio-engine-audit.md`

- [ ] **Step 1: Summarize measured hot spots**

Write `## Summary` with:

- fastest and slowest scenario by `ns_per_frame`
- sample-rate scaling behavior
- block-size sensitivity
- top 5 profiler functions by self time
- top 5 profiler functions by total time

- [ ] **Step 2: Rank optimization candidates**

Create `## Optimization Candidates` with this table:

```markdown
| Rank | Area | Evidence | Expected upside | Risk | Required correctness check |
| ---- | ---- | -------- | --------------- | ---- | -------------------------- |
```

Use only evidence from the benchmark CSVs and profiler trace. If a suspected area is not measured, put it under `## Not Proven`.

- [ ] **Step 3: Apply the optimization gate**

For each candidate, require all of:

- It accounts for at least 5% self time in a measured scenario, or it causes clear block-size scaling overhead.
- The proposed change can be verified by `poryaaaa_unit_tests`.
- The proposed change has an audio-output equivalence check or a stated tolerance.
- The proposed change does not weaken GBA hardware-fidelity behavior.

If a candidate fails any gate, do not recommend implementation yet.

---

### Task 7: Final Verification

**Files:**
- Modify: `docs/performance/poryaaaa-audio-engine-audit.md`

- [ ] **Step 1: Build validation targets**

Run from `packages/poryaaaa`:

```bash
cmake --build build --target poryaaaa_audio_bench poryaaaa_unit_tests poryaaaa_render
```

Expected: all requested targets build.

- [ ] **Step 2: Run unit tests**

Run:

```bash
./build/poryaaaa_unit_tests
```

Expected: pass, or record exact unrelated failures in `## Verification`.

- [ ] **Step 3: Check diff hygiene**

Run from the monorepo root:

```bash
git diff --check
```

Expected: no whitespace errors.

- [ ] **Step 4: Commit only audit artifacts if requested**

Do not commit automatically. If the user asks for a commit, stage only:

```bash
git add packages/poryaaaa/bench/audio_engine_bench.c packages/poryaaaa/CMakeLists.txt docs/performance/poryaaaa-audio-engine-audit.md
git commit -m "Add poryaaaa audio engine performance audit harness"
```

---

## Done Criteria

- `poryaaaa_audio_bench` builds and runs repeatably.
- The report contains raw benchmark data, environment details, and profiler findings or an exact reason profiler capture was unavailable.
- The report clearly separates audio-engine CPU from renderer/MIDI/WAV/asset IO costs.
- No engine behavior changes are made by the audit.
- Any optimization recommendation is backed by measured evidence.
