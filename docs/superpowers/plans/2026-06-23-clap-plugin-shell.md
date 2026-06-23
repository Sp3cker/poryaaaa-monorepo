# CLAP Plugin Shell Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Decide whether `ccomidi.clap` and `ccomidi_legatobend.clap` should share a CLAP shell, then extract only the lifecycle code that clearly duplicates.

**Architecture:** A shared shell is earned only if it hides CLAP lifecycle complexity behind a smaller interface. Core MIDI behavior, param schema, and editor drawing stay plugin-specific adapters.

**Tech Stack:** C++20, CLAP, ccomidi core tests, Release-only ccomidi build.

---

## My Take

This is tempting, but it is not first. Duplication is annoying; a bad shared shell would be worse. Start with a diff audit and extract only if the deletion test says lifecycle bugs would otherwise be fixed twice.

## Files

- Inspect: `packages/ccomidi/src/plugin/ccomidi_plugin.cpp`
- Inspect: `packages/ccomidi/src/plugin/legatobend_plugin.cpp`
- Inspect: `packages/ccomidi/src/plugin/ccomidi_plugin_shared.h`
- Inspect: `packages/ccomidi/src/plugin/legatobend_plugin_shared.h`
- Possibly create: `packages/ccomidi/src/plugin/clap_plugin_shell.h`
- Possibly create: `packages/ccomidi/src/plugin/clap_plugin_shell.cpp`
- Modify: `packages/ccomidi/CMakeLists.txt`

## Tasks

### Task 1: Do The Deletion Test In A Diff

- [ ] Compare lifecycle functions in both plugin files: init, destroy, activate, deactivate, start, stop, reset, process, state, params, GUI.
- [ ] Write down which duplicated sections are byte-similar and which differ semantically.
- [ ] Stop if fewer than three lifecycle sections are truly duplicated.

### Task 2: Extract Only Lifecycle Plumbing

- [ ] If Task 1 passes, create a shell module that owns descriptor wiring and lifecycle callback dispatch.
- [ ] Keep plugin-specific state structs in their current shared headers.
- [ ] Keep plugin-specific `process()` bodies out of the shell unless they become identical after adapter calls.
- [ ] Add shell code to `packages/ccomidi/CMakeLists.txt`.

### Task 3: Preserve Both Plugin Behaviors

- [ ] Run `ccomidi_core_tests`.
- [ ] Run `ccomidi_legatobend_core_tests`.
- [ ] Build `ccomidi` Release.
- [ ] Build `ccomidi_legatobend` Release.

### Task 4: Stop Before Param Router Work

- [ ] Do not add a shared param router in this plan.
- [ ] If param duplication remains painful after shell extraction, write a separate plan with exact duplicated param sections.

## Verification

- `cmake --build packages/ccomidi/build --config Release --target ccomidi_core_tests`
- `packages/ccomidi/build/ccomidi_core_tests`
- `cmake --build packages/ccomidi/build --config Release --target ccomidi_legatobend_core_tests`
- `packages/ccomidi/build/ccomidi_legatobend_core_tests`
- `just build ccomidi`

