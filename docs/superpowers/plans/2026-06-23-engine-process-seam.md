# Engine Process Seam Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make every poryaaaa caller drive audio through `m4a_engine_process()` instead of hand-rolling the advance/render/consume order.

**Architecture:** Keep `m4a_engine_process()` as the deep module. The renderer should own setup and output-file concerns, but the engine module should own the order of `m4a_advance`, `hw_audio_render_events`, and `m4a_consume_writes`.

**Tech Stack:** C, poryaaaa renderer, m4a engine tests, Release CMake build.

---

## My Take

This is a good small cleanup. It is not a new abstraction; it deletes a duplicate ritual in `poryaaaa_render.c`.

## Files

- Modify: `packages/poryaaaa/cmd/poryaaaa_render.c`
- Possibly modify: `packages/poryaaaa/plugin/m4a_engine.h`
- Possibly modify: `packages/poryaaaa/plugin/m4a_engine.c`
- Test: `packages/poryaaaa/test/test_engine.c`

## Current Evidence

- `packages/poryaaaa/plugin/m4a_plugin.c` already calls `m4a_engine_process()`.
- `packages/poryaaaa/cmd/poryaaaa_render.c` still calls `m4a_advance`, `hw_audio_render_events`, and `m4a_consume_writes` directly.

## Tasks

### Task 1: Add A Renderer Regression Check

- [ ] Find the smallest existing renderer or engine test that can render a short fixture.
- [ ] Capture current sample count and non-silent output behavior before changing the renderer.
- [ ] If no renderer test exists, add one small command-level smoke test rather than a broad golden-audio suite.

### Task 2: Route Renderer Through The Engine Interface

- [ ] Replace the manual render loop in `poryaaaa_render.c` with `m4a_engine_process()`.
- [ ] Keep renderer-owned chunking, WAV writing, and CLI parsing in `poryaaaa_render.c`.
- [ ] Do not expose `pending_writes` or `pcm_ring` from the renderer after this change.
- [ ] Build the renderer target.

### Task 3: Delete The Leaked Ordering Comments

- [ ] Remove comments that teach external callers the three-step ritual.
- [ ] Keep one short comment on `m4a_engine_process()` that it advances the driver, renders hardware audio, and consumes writes.
- [ ] Run `xcrun clang-format -i` on touched C files.

## Verification

- Focused engine or renderer test.
- `just build poryaaaa`

