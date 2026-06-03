# C++23 Non-Audio Build Probe Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Compile the project-owned non-audio C++ surfaces as C++23, leave scoped-out C modules as C11, and record whether the current code builds.

**Architecture:** This is a build-policy probe, not a source modernization pass. C++23 should be applied target-by-target to `poryaaaa`, `recorder`, and the unit-test target that compiles the C++ recorder bridge. The audio engine, voicegroup loader, renderer, and C tests remain C11.

**Tech Stack:** CMake 3.24+, AppleClang, C11, C++23, CLAP plugin bundle, existing `poryaaaa_unit_tests`.

---

### Task 1: Apply Target-Scoped C++23 Build Policy And Probe Build

**Files:**
- Modify: `CMakeLists.txt`
- Modify: `plugin/recorder/CMakeLists.txt`

- [ ] **Step 1: Inspect current standard settings**

Run:

```bash
rg -n "CMAKE_CXX_STANDARD|CMAKE_C_STANDARD|target_compile_features|CXX_STANDARD|CXX_EXTENSIONS" CMakeLists.txt plugin/recorder/CMakeLists.txt
```

Expected: the root CMake file sets `CMAKE_C_STANDARD 11` and `CMAKE_CXX_STANDARD 17`, with no target-scoped C++23 policy yet.

- [ ] **Step 2: Make the minimal CMake policy change**

Edit `CMakeLists.txt`:

```cmake
set(CMAKE_C_STANDARD 11)
```

Remove the global C++17 line:

```cmake
set(CMAKE_CXX_STANDARD 17)
```

After `add_library(poryaaaa MODULE ...)`, add:

```cmake
target_compile_features(poryaaaa PRIVATE cxx_std_23)
```

After `add_executable(poryaaaa_unit_tests ...)`, add:

```cmake
target_compile_features(poryaaaa_unit_tests PRIVATE cxx_std_23)
```

Edit `plugin/recorder/CMakeLists.txt` after `add_library(recorder STATIC ...)` and add:

```cmake
target_compile_features(recorder PRIVATE cxx_std_23)
```

Do not add C++23 settings to `poryaaaa_test`, `poryaaaa_render`, `voicegroup_loader`, `m4a_driver`, or `hw_audio`.

- [ ] **Step 3: Configure a fresh probe build**

Run:

```bash
cmake -S . -B build-cxx23-probe -DCMAKE_BUILD_TYPE=Release
```

Expected: configure succeeds. If configure fails because CMake cannot satisfy `cxx_std_23`, record the exact failure and stop.

- [ ] **Step 4: Build the CLAP target**

Run:

```bash
cmake --build build-cxx23-probe --target poryaaaa
```

Expected: either the build succeeds, or the first C++23 compatibility issue is exposed. Record any warnings or errors before making any source fixes.

- [ ] **Step 5: Build and run unit tests**

Run:

```bash
cmake --build build-cxx23-probe --target poryaaaa_unit_tests
./build-cxx23-probe/poryaaaa_unit_tests
```

Expected: the unit-test target builds and the tests pass. If the target does not exist because configure or build failed earlier, report the earlier blocker instead.

- [ ] **Step 6: Inspect generated compile flags**

Run:

```bash
rg -n -- "-std=.*\\+\\+|-std=gnu11|-std=c11" build-cxx23-probe
```

Expected:
- `poryaaaa` C++ or Objective-C++ flags use a C++23 mode such as `-std=gnu++23`.
- `recorder` C++ flags use a C++23 mode such as `-std=gnu++23`.
- `poryaaaa_unit_tests` C++ flags use a C++23 mode such as `-std=gnu++23`.
- Engine and scoped-out C sources still use C11 mode such as `-std=gnu11`.

- [ ] **Step 7: Report changed files and build result**

Run:

```bash
git diff -- CMakeLists.txt plugin/recorder/CMakeLists.txt
git status --short
```

Expected: only the two CMake files are modified by this task. Existing untracked build directories may remain untracked and should not be removed unless explicitly requested.
