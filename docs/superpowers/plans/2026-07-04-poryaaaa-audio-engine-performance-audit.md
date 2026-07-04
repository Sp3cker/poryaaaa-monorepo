# Poryaaaa Audio Engine Performance Audit Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Use macOS Instruments Time Profiler to identify which functions take the most time in poryaaaa's audio engine and export path, then produce an evidence-backed optimization report without changing audio behavior.

**Architecture:** Add one local benchmark executable that exercises the existing `m4a_driver` -> `hw_audio_render_events` -> `m4a_consume_writes` path and several chip-only scenarios. Use Instruments Time Profiler as the primary tool for hot-function attribution, with command-line timing only as supporting context. Keep engine-only profiling separate from end-to-end renderer profiling so host, file IO, MIDI parsing, WAV writing, and asset loading do not get mistaken for audio-engine cost.

**Tech Stack:** C23/C11-compatible project C, CMake `build/`, macOS Instruments Time Profiler through `xcrun xctrace`, macOS `/usr/bin/time`, existing `poryaaaa_unit_tests`, existing `poryaaaa_render`.

---

## Scope

- Audit only the C audio engine:
  - `packages/poryaaaa/plugin/m4a/`
  - `packages/poryaaaa/plugin/hw_audio/`
  - `packages/poryaaaa/plugin/m4a_engine.c`
  - `packages/poryaaaa/cmd/poryaaaa_render.c` only as an end-to-end host.
- Do not audit M4L device patching, TypeScript, Node, UI drawing, voicegroup parser performance, DAW scheduling, or Ableton/Max process overhead unless engine-only and renderer profiles fail to explain the observed cost.
- Do not optimize during the audit. The output is a report and a ranked list of optimization candidates.
- Preserve hardware-fidelity behavior. Any later optimization must prove output equivalence or explain a deliberate tolerance.
- Use the existing `build/` directory. Do not create `build-release`, `build-perf`, or other alternate build directories.
- Prefer profiling standalone executables launched by `xctrace`; attach Instruments to Ableton or Max only if a later task explicitly scopes the audit to DAW-host behavior.

## Files

- Create: `packages/poryaaaa/bench/audio_engine_bench.c`
  - Small standalone benchmark harness.
  - Runs synthetic chip-only and driver-plus-chip scenarios.
  - Prints machine-readable per-scenario timing.
- Modify: `packages/poryaaaa/CMakeLists.txt`
  - Add `poryaaaa_audio_bench`.
  - Link it to `m4a_driver`, `hw_audio`, and `m` on non-MSVC, matching the existing target pattern.
- Create: `docs/performance/poryaaaa-audio-engine-audit.md`
  - Final audit report with environment, commands, Instruments findings, supporting raw timing results, and ranked candidates.
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

- [ ] **Step 3: Configure the existing build directory for profiling symbols**

Run from `packages/poryaaaa`:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
```

Expected: configure completes and reuses `packages/poryaaaa/build`. `RelWithDebInfo` keeps optimized code while preserving useful function symbols for Instruments.

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

### Task 3: Capture Engine Hot Spots With Instruments

**Files:**
- Modify: `docs/performance/poryaaaa-audio-engine-audit.md`

- [ ] **Step 1: Confirm Instruments Time Profiler is available**

Run:

```bash
command -v xcrun
xcrun xctrace list templates | rg "Time Profiler"
```

Expected: both commands succeed. If either command fails, record the exact failure under `## Profiler Findings` and continue with Task 4 timing data. Do not substitute a different profiler silently.

- [ ] **Step 2: Run short benchmark smoke test before profiling**

Run:

```bash
./build/poryaaaa_audio_bench --scenario all --seconds 2 --warmup-seconds 1 --repeat 1
```

Expected: one CSV header plus one row per scenario. Confirm `realtime_factor` is greater than `1.0` for every scenario.

- [ ] **Step 3: Record the engine-only Time Profiler trace**

Run from `packages/poryaaaa`:

```bash
rm -rf /tmp/poryaaaa-audio-engine.trace
xcrun xctrace record \
  --template "Time Profiler" \
  --output /tmp/poryaaaa-audio-engine.trace \
  --launch -- ./build/poryaaaa_audio_bench \
    --scenario all \
    --sample-rate 44100 \
    --block 512 \
    --seconds 60 \
    --warmup-seconds 2 \
    --repeat 1
```

Expected: `/tmp/poryaaaa-audio-engine.trace` exists. If macOS prompts for profiling permissions or the command fails, record the exact message under `## Profiler Findings`.

- [ ] **Step 4: Extract top engine functions from Instruments**

Open `/tmp/poryaaaa-audio-engine.trace` in Instruments UI and record the top functions under `## Profiler Findings`.

Record two lists:

- top 10 functions by self time
- top 10 functions by total time

For every listed function, record:

- function name
- self time or percentage
- total time or percentage
- owning bucket from this list:
  - `m4a_driver`: `m4a_advance`, `m4a_sound_main_ram`, `m4a_drv_pcm_*`, `m4a_trk_vol_pit_set`, CGB emit paths.
  - `hw_audio`: `hw_audio_render_events`, `render_segment`, `render_internal_chunk`.
  - `hw_resample`: `hw_resample_process`.
  - `hw_psg`: `hw_psg_render`, `hw_psg_apply_event`.
  - `hw_pcm`: `hw_pcm_render`, `hw_pcm_apply_event`.
  - `hw_mix`: `hw_mix_render`, `hw_mix_apply_event`.
  - `other`: startup, C runtime, allocation, or any function not owned by poryaaaa audio engine code.

Do not rank `other` costs as audio-engine optimization candidates unless the report labels them separately.

---

### Task 4: Collect Supporting Command-Line Timing Numbers

**Files:**
- Modify: `docs/performance/poryaaaa-audio-engine-audit.md`

- [ ] **Step 1: Run the primary benchmark set**

Run:

```bash
./build/poryaaaa_audio_bench --scenario all --sample-rate 44100 --block 512 --seconds 20 --warmup-seconds 2 --repeat 10 > /tmp/poryaaaa-audio-44100.csv
./build/poryaaaa_audio_bench --scenario all --sample-rate 48000 --block 512 --seconds 20 --warmup-seconds 2 --repeat 10 > /tmp/poryaaaa-audio-48000.csv
./build/poryaaaa_audio_bench --scenario all --sample-rate 96000 --block 512 --seconds 20 --warmup-seconds 2 --repeat 10 > /tmp/poryaaaa-audio-96000.csv
```

Append the three CSV files to the report under `## Benchmark Results`.

- [ ] **Step 2: Run block-size sensitivity measurements**

Run:

```bash
./build/poryaaaa_audio_bench --scenario all --sample-rate 44100 --block 64 --seconds 20 --warmup-seconds 2 --repeat 10 > /tmp/poryaaaa-audio-block64.csv
./build/poryaaaa_audio_bench --scenario all --sample-rate 44100 --block 256 --seconds 20 --warmup-seconds 2 --repeat 10 > /tmp/poryaaaa-audio-block256.csv
./build/poryaaaa_audio_bench --scenario all --sample-rate 44100 --block 2048 --seconds 20 --warmup-seconds 2 --repeat 10 > /tmp/poryaaaa-audio-block2048.csv
```

Append the results under `## Block Size Sensitivity`.

- [ ] **Step 3: Capture process-level timing**

Run:

```bash
/usr/bin/time -lp ./build/poryaaaa_audio_bench --scenario all --sample-rate 44100 --block 512 --seconds 60 --warmup-seconds 2 --repeat 1 > /tmp/poryaaaa-audio-time.csv 2> /tmp/poryaaaa-audio-time.txt
```

Append `/tmp/poryaaaa-audio-time.txt` under `## Process Timing`.

---

### Task 5: Capture End-to-End Renderer Hot Spots Separately

**Files:**
- Modify: `docs/performance/poryaaaa-audio-engine-audit.md`

- [ ] **Step 1: Find a stable input command**

Prefer an existing small MIDI and project root already used by local tests. Check `packages/poryaaaa/README.md` for `poryaaaa_render` argument shape, then search for local render fixtures with:

```bash
rg -n "poryaaaa_render|--midi|\\.mid" packages/poryaaaa/test packages/poryaaaa docs -g '!build/**'
```

If no stable asset is available, do not fabricate a project-root dependency. Mark `## End-to-End Renderer Findings` as blocked by missing local render fixture and keep the benchmark harness as the audit source of truth.

- [ ] **Step 2: Record the renderer Time Profiler trace when assets exist**

After selecting the fixture, write the exact `poryaaaa_render` command into the report before running it. Use real paths from the selected fixture.

Run the selected command through `xcrun xctrace record` by placing the exact selected command after `--launch --`. Before running, paste the final command into the report under `## End-to-End Renderer Findings`; it must contain real absolute paths. If the command cannot be made concrete from local files, stop this task and record the missing fixture instead.

Expected: `/tmp/poryaaaa-render.trace` exists. Open it in Instruments UI and record the top 10 self-time and top 10 total-time functions under `## End-to-End Renderer Findings`.

Classify functions into:

- `audio-engine`: driver, hardware audio, resampler, PSG, PCM, or mix functions.
- `host-render`: MIDI parsing, voicegroup loading, WAV writing, allocation, file IO, or CLI setup.

- [ ] **Step 3: Capture renderer process-level timing when assets exist**

Run the same selected command under `/usr/bin/time -lp`.

Record:

- input MIDI path
- voicegroup
- rendered duration
- wall time
- user CPU time
- system CPU time
- peak memory

Keep renderer findings separate from synthetic benchmark rows because renderer profiling includes MIDI parsing, voicegroup loading, WAV writing, and allocation behavior.

---

### Task 6: Produce the Audit Report

**Files:**
- Modify: `docs/performance/poryaaaa-audio-engine-audit.md`

- [ ] **Step 1: Summarize measured hot spots**

Write `## Summary` with:

- fastest and slowest scenario by `ns_per_frame`
- sample-rate scaling behavior
- block-size sensitivity
- top 10 engine-profile functions by self time
- top 10 engine-profile functions by total time
- top renderer-profile functions only if Task 5 had a local fixture

- [ ] **Step 2: Rank optimization candidates**

Create `## Optimization Candidates` with this table:

```markdown
| Rank | Area | Evidence | Expected upside | Risk | Required correctness check |
| ---- | ---- | -------- | --------------- | ---- | -------------------------- |
```

Use Instruments self-time and total-time evidence as the primary ranking source. Use benchmark CSVs and `/usr/bin/time` data only to support or sanity-check the Instruments findings. If a suspected area is not measured, put it under `## Not Proven`.

- [ ] **Step 3: Apply the optimization gate**

For each candidate, require all of:

- It accounts for at least 5% self time in an Instruments profile, or it causes clear block-size scaling overhead in the supporting benchmark data.
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
- The report contains environment details, Instruments Time Profiler findings, supporting raw benchmark data, or an exact reason profiler capture was unavailable.
- The report clearly separates audio-engine CPU from renderer/MIDI/WAV/asset IO costs.
- No engine behavior changes are made by the audit.
- Any optimization recommendation is backed by measured evidence.
