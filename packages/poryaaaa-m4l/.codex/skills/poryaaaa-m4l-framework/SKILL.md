---
name: poryaaaa-m4l-framework
description: Use when working in the poryaaaa-m4l repo on Max for Live devices, hand-maintained .amxd files, Max object references, TypeScript for V8 or Node for Max, WebSocket voicegroup transport, C++ Max externals, build/test workflows, or domain-routed subagent work.
---

# poryaaaa-m4l Codex Framework

This is the authoritative agent guidance for this repo. Do not load `CLAUDE.md`
for project rules; it has been replaced by this Codex skill.

## Quick Start

1. Confirm the repo root is the nearest ancestor containing `package.json`,
   `code-src/`, `scripts/`, `source/`, and `devices/`.
2. Read `references/repo-rules.md` before editing.
3. Read `references/domain-map.md` to choose a lead specialist and any
   secondary specialists.
4. Read `references/context-loading.md` for the minimum task context.
5. Load only the relevant specialist file(s) under `specialists/`.
6. If work crosses independent domains, spawn focused subagents using
   `references/subagent-prompts.md`.
7. For multi-domain results, apply `references/merge-protocol.md` and then
   `references/critic-protocol.md`.
8. Verify with the smallest command set from `references/build-test-matrix.md`.

## Non-Negotiable Rules

- Edit TypeScript sources in `code-src/`; do not hand-edit generated
  `javascript/*.js`.
- Treat V8-controller TypeScript and Node-for-Max transport TypeScript as
  separate domains unless the task explicitly crosses their message contract.
- Devices in `devices/*.amxd` are hand-maintained. After editing a device in Max,
  run `python3 scripts/amxd_inspect.py devices/<name>.amxd validate` and commit the
  resulting binary.
- Before recommending or wiring a Max/M4L object, check `docs/max-ref/` or
  explicitly state when no local reference exists.
- Serialize non-UI Live API objects as `newobj` with `text`, not native
  `maxclass`; keep UI widgets like `live.dial` and `live.text` native.
- Keep voicegroup truth-source boundaries intact: `poryaaaa~` confirms loaded
  voicegroup state; V8 is GUI/controller; Node is transport, project-store, and
  state broadcast.
- Preserve user changes. Never revert unrelated files.

## Specialist Routing

Load `references/domain-map.md` first for routing detail.

- TypeScript V8: `specialists/typescript-v8.md`
- Node transport: `specialists/node-transport.md`
- C++ externals: `specialists/cpp-externals.md`
- AMXD generator: `specialists/amxd-generator.md`
- Max/M4L reference: `specialists/max-m4l-reference.md`
- Verification: `specialists/verification.md`
- Router/merge protocol: `specialists/router.md`

Common Codex workflow requests are mapped in `references/command-map.md`.

If three or more domains are relevant, load the lead specialist fully and load
only "Scope", "Inputs", and "Output" sections from secondary specialists unless
the task requires deeper implementation detail.

## Subagent Policy

The default execution model for multi-domain work is:

1. Main agent owns routing, merge decisions, and final verification.
2. Spawn one subagent per independent domain when scopes do not overlap.
3. Give each subagent an explicit file/domain ownership boundary.
4. Require each subagent to report findings, changed files, commands run, and
   unresolved risks.
5. Do not spawn parallel writers over shared device artifacts. Coordinate
   device writes (hand-edited .amxd binaries) in the main agent or run those tasks
   sequentially.

Use `references/subagent-prompts.md` for prompt templates.
