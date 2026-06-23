# Timed MIDI Event Wire Format Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Put recorded timed-MIDI events behind one versioned PRBY format module so C++ writers and TypeScript readers cannot silently drift.

**Architecture:** Keep the current PRBY idea, but make the binary format explicit and tested at the seam. The C++ recorder writes one format, TypeScript reads one format, and both reject unknown versions instead of guessing.

**Tech Stack:** C++ recorder code, TypeScript recorder writer, PRBY-v1 fixtures, existing recorder tests.

---

## My Take

This is higher risk than the grammar item because drift corrupts recordings. Do this before broad CLAP-shell cleanup if recorder exports matter right now.

## Files

- Modify: `packages/poryaaaa/plugin/recorder/recorder_core.h`
- Modify: `packages/poryaaaa/plugin/recorder/recorder_core.cpp`
- Modify: `packages/poryaaaa-m4l/source/audio/poryaaaa~/recorder/midi_buffer.h`
- Modify: `packages/poryaaaa-m4l/source/audio/poryaaaa~/recorder/midi_buffer.cpp`
- Modify: `packages/poryaaaa-m4l/code-src/recorder/ccomidi_recorder.ts`
- Modify: `packages/poryaaaa-m4l/code-src/recorder/recorder_smf_writer.ts`
- Test: `packages/poryaaaa-m4l/code-src/test/recorder_prby_format.test.ts`
- Test: `packages/poryaaaa-m4l/source/audio/poryaaaa~/recorder/export_capture_tests.cpp`

## Tasks

### Task 1: Document The Current Format In Code

- [ ] Add named constants for PRBY magic, version, header size, and record size in the C++ writer module.
- [ ] Mirror those constants in the TypeScript reader module for now.
- [ ] Add comments beside both constant sets that name the other file and say both must change in the same commit until Task 3 removes the duplication.

### Task 2: Add Cross-Language Fixture Tests

- [ ] Add a small binary fixture with two events: one note-on at beat `0`, one program-change at beat `1.5`.
- [ ] Add a TypeScript test that reads the fixture and asserts exact `beats`, `status`, `d1`, and `d2`.
- [ ] Add a C++ test that writes the same two events and byte-compares the output against the fixture.
- [ ] Run the focused TypeScript and C++ recorder tests.

### Task 3: Centralize Version Checks

- [ ] Move PRBY read validation into one TypeScript function used by `ccomidi_recorder.ts` and `recorder_smf_writer.ts`.
- [ ] Reject unknown magic or version with a clear error string that includes the observed version.
- [ ] Add a test for unknown version `2`.
- [ ] Add a test for truncated record data.

### Task 4: Resolve The Mutex Difference

- [ ] Decide whether `packages/poryaaaa-m4l/source/audio/poryaaaa~/recorder/midi_buffer.*` is still live code.
- [ ] If live, add the same locking rule as `recorder_core`.
- [ ] If not live, remove it in a separate deletion commit after proving no build target references it.
- [ ] Verify with `rg -n "MidiBuffer|midi_buffer" packages/poryaaaa-m4l packages/poryaaaa`.

## Verification

- Focused recorder C++ tests.
- `npm test` from `packages/poryaaaa-m4l` or focused recorder tests.
- `just build poryaaaa`

