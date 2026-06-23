# PCBus Sidechannel Decision Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Resolve the reintroduced `pc_bus` sidechannel by deleting it unless a real reader is added in the same feature.

**Architecture:** One adapter is a hypothetical seam. `ccomidi` currently publishes Program Change data to shared memory, but no reader consumes it; the MIDI stream remains the real interface. Delete the sidechannel until there are two adapters.

**Tech Stack:** C++20, CLAP, ccomidi Release build.

---

## My Take

Delete it. `origin/clay` brought this back after we had already identified it as unused. If Program Change fidelity needs a sidechannel later, add writer and reader together.

## Files

- Delete: `packages/ccomidi/src/ipc/pc_bus.h`
- Delete: `packages/ccomidi/src/ipc/pc_bus.cpp`
- Modify: `packages/ccomidi/CMakeLists.txt`
- Modify: `packages/ccomidi/src/plugin/ccomidi_plugin.cpp`
- Modify: `packages/ccomidi/src/plugin/ccomidi_plugin_shared.h`

## Tasks

### Task 1: Prove There Is Still No Reader

- [ ] Run `rg -n "read_slot|PCBus|pc_bus|ccomidi_pc_bus" packages --glob '!**/build/**'`.
- [ ] Confirm all hits are writer ownership, build wiring, or the `pc_bus` module itself.
- [ ] If a real reader appears, stop and replace this plan with a shared writer-reader contract plan.

### Task 2: Remove The Writer

- [ ] Remove `#include "ipc/pc_bus.h"` from plugin files.
- [ ] Remove `ipc::PCBus pcBus` from `Plugin`.
- [ ] Remove `self->pcBus.open()` from activation.
- [ ] Remove `self->pcBus.close()` from deactivation.
- [ ] Delete `publish_program_changes(...)`.
- [ ] Remove all calls to `publish_program_changes(...)`.

### Task 3: Remove The Build Target

- [ ] Delete `add_library(ccomidi_ipc STATIC ...)` and its include/property wiring from `packages/ccomidi/CMakeLists.txt`.
- [ ] Remove `ccomidi_ipc` from `target_link_libraries(ccomidi PRIVATE ...)`.
- [ ] Delete `packages/ccomidi/src/ipc/pc_bus.h`.
- [ ] Delete `packages/ccomidi/src/ipc/pc_bus.cpp`.

### Task 4: Verify The Deletion

- [ ] Run `rg -n "pc_bus|PCBus|publish_program_changes|ccomidi_ipc" packages/ccomidi --glob '!packages/ccomidi/build/**'` and expect no output.
- [ ] Run `xcrun clang-format -i packages/ccomidi/src/plugin/ccomidi_plugin.cpp packages/ccomidi/src/plugin/ccomidi_plugin_shared.h`.
- [ ] Run `just build ccomidi`.

## Verification

- `rg -n "pc_bus|PCBus|publish_program_changes|ccomidi_ipc" packages/ccomidi --glob '!packages/ccomidi/build/**'` returns no matches.
- `just build ccomidi`

