# Max Live Adapter Seam Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Remove hidden Max/Live side effects from parser and routing logic, while avoiding a broad adapter framework.

**Architecture:** Keep M4L domains separate: V8 controllers stay in `code-src/*.ts`, Node transport stays in `code-src/poryaaaa-node/*`, and native parsing stays behind NAPI. Introduce a seam only where there are two adapters: real Max/Live runtime and tests.

**Tech Stack:** TypeScript, Max for Live `max-api`, Node for Max, existing poryaaaa-m4l tests.

---

## My Take

The report is right about side effects, but "one adapter seam" can become busywork. The useful first move is smaller: parser modules do no logging or file parsing, and routing logic gets tested without Max.

## Files

- Modify: `packages/poryaaaa-m4l/code-src/poryaaaa-node/voicegroup-parser.ts`
- Modify: `packages/poryaaaa-m4l/code-src/ccomidi_voices.ts`
- Modify: `packages/poryaaaa-m4l/code-src/test/ccomidi_voices.test.ts`
- Modify: `packages/poryaaaa-m4l/code-src/test/poryaaaa_node_voicegroup_parser.test.ts`
- Possibly modify: `packages/poryaaaa-m4l/code-src/recorder/ccomidi_recorder.ts`

## Tasks

### Task 1: Remove Parser Side Effects

- [ ] Delete `maxApi.post(res)` from `voicegroup-parser.ts`.
- [ ] Keep `parseVoicegroup()` as a thin call into `poryaaaa_voicegroup.node`.
- [ ] Add or update a parser test proving parse failure returns diagnostics without requiring `max-api`.
- [ ] Run the focused poryaaaa-m4l parser tests.

### Task 2: Keep File IO Out Of The Parser

- [ ] Confirm `voicegroup-parser.ts` does not read voicegroup file contents.
- [ ] Leave `scanVoicegroupBanks()` as directory discovery only if current callers still need bank names.
- [ ] If bank discovery moves native later, remove `scanVoicegroupBanks()` in that same commit.
- [ ] Run `rg -n "readFile|readFileSync|writeFile|writeFileSync" packages/poryaaaa-m4l/code-src/poryaaaa-node/voicegroup-parser.ts`.

### Task 3: Extract The Smallest Testable Route Function

- [ ] In `ccomidi_voices.ts`, keep Max handler registration where it is.
- [ ] Extract only the pure decision logic from `routeTrack()` if tests cannot already cover it.
- [ ] Do not create a generic Max adapter unless both `ccomidi_voices.ts` and `ccomidi_recorder.ts` use it in the same patch.
- [ ] Extend `ccomidi_voices.test.ts` for the selected-track routing case that motivated the extraction.

### Task 4: Gate Recorder Adapter Work

- [ ] Inspect `ccomidi_recorder.ts` after Tasks 1-3.
- [ ] If recorder tests already cover save orchestration, stop.
- [ ] If recorder logic still requires Max to test, extract one narrow adapter for LOM calls used by the save flow.
- [ ] Run focused recorder tests only.

## Verification

- Focused poryaaaa-m4l parser tests.
- Focused `ccomidi_voices` tests.
- Recorder tests only if recorder code changes.

