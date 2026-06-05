# Critic Protocol

Use after generation, multi-domain edits, or before claiming a Max/M4L change is
complete.

## Severity

| Severity | Meaning | Action |
| --- | --- | --- |
| blocker | Likely broken behavior, broken build, invalid patcher structure, or wrong domain boundary | Fix before completion |
| warning | Risky or partially verified behavior | Report and fix when cheap |
| note | Context or improvement | Report only when useful |

## Review Passes

1. **Domain compliance:** changed files match the responsible specialist.
2. **Contract compliance:** route tags, status payloads, I/O counts, and Live
   parameters agree across domains.
3. **Generated structure:** inspect `.amxd` for dangling endpoints and
   out-of-range inlet/outlet use.
4. **Max object accuracy:** object syntax and Live API claims are backed by
   `docs/max-ref/`, Max docs, or `docs/max-gotchas.md`.
5. **Verification:** run the smallest relevant command set from
   `build-test-matrix.md`.

## Loop

- If blockers are found, fix and rerun the relevant review pass.
- If the same blocker survives three attempts, stop and explain the blocker.
- Warnings and notes do not block final output, but final response must disclose
  them if they affect user testing.

## Findings Format

Lead with findings:

```text
blocker: path:line - concise issue and consequence
warning: path:line - concise risk
```

Then list verification commands and residual risk.
