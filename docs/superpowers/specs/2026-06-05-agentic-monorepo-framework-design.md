# poryaaaa-monorepo Agentic Framework Design

## Purpose

Create a root Codex framework for `poryaaaa-monorepo` so cross-project work is
routed through domain agents instead of handled as one large undifferentiated
context. The framework must preserve package-local `AGENTS.md` guidance while
adding monorepo-level routing, merge, and verification rules.

## Assumptions

- The framework is repo-local documentation under `.codex/skills/`, not a
  Conan package, build artifact, or runtime dependency.
- First-party projects stay as normal directories under `packages/`.
- Package-local `AGENTS.md` files remain authoritative for package code.
- Cross-project work should use subagents by default when domains are
  independent and write scopes do not overlap.
- `/Users/sallegrezza/dev/pokemon-hearth` is the primary Pokemon decomp truth
  source for engine behavior and `mid2agb` relevance checks.
- `/Users/sallegrezza/dev/pokeemerald-expansion` may be used as secondary
  reference when the primary checkout is insufficient, but not as the default
  source of truth.

## Architecture

Add a root Codex skill:

```text
.codex/skills/poryaaaa-monorepo-framework/
```

The root `AGENTS.md` points cross-project tasks to this framework. The framework
then loads only the package and specialist context needed for the task.

The initial framework files are:

- `SKILL.md`: entrypoint, non-negotiable rules, and routing summary.
- `agents/openai.yaml`: display metadata for the framework.
- `references/domain-map.md`: domain ownership and trigger rules.
- `references/context-loading.md`: minimum context by task type.
- `references/subagent-prompts.md`: reusable explorer, worker, reviewer, and
  Pokemon-reference prompts.
- `references/merge-protocol.md`: how the main agent reconciles subagent work.
- `references/build-test-matrix.md`: root and package validation commands.
- `specialists/poryaaaa-engine.md`
- `specialists/ccomidi-midi.md`
- `specialists/poryaaaa-m4l-bridge.md`
- `specialists/pokemon-reference.md`
- `specialists/verification.md`

## Domain Routing

`poryaaaa-engine` owns engine, CLAP, renderer, voicegroup loader, recorder, and
GBA m4a behavior inside `packages/poryaaaa`.

`ccomidi-midi` owns MIDI sender behavior, CC mapping, command export assumptions,
and `mid2agb`-facing questions inside `packages/ccomidi`.

`poryaaaa-m4l-bridge` owns cross-package Max for Live integration, then delegates
deep package work to `packages/poryaaaa-m4l/.codex/skills/poryaaaa-m4l-framework/`.

`pokemon-reference` is read-only against `/Users/sallegrezza/dev/pokemon-hearth`.
It answers engine-behavior questions by citing concrete source paths and line
evidence.

`verification` owns command selection and final validation reporting.

## Pokemon Reference Rule

When validating whether a CC or MIDI event matters after parsing/export through
`mid2agb`, agents must not infer behavior from MIDI names or `ccomidi` parser
code alone.

The expected flow is:

1. `ccomidi-midi` determines what `ccomidi` or `mid2agb` emits.
2. `pokemon-reference` determines what the emitted command actually does in the
   Pokemon engine.
3. The main agent merges those findings into a behavior claim and names any
   uncertainty.

For CC behavior specifically, `pokemon-reference` must inspect both:

- C engine source and headers, primarily `src/m4a.c`, `include/m4a.h`, and
  related `gba/m4a_internal.h` definitions.
- Assembly, macro, or song evidence, especially `asm/macros/m4a.inc`,
  `data/sound_data.s`, generated song command syntax, and `tools/mid2agb/*`.

If either side of that evidence is missing, the agent must say so and avoid
making a stronger claim than the evidence supports.

## Subagent Policy

The main agent owns routing, final edits that cross domains, conflict
resolution, and final verification.

Spawn subagents when:

- A task crosses two or more independent domains.
- A read-only Pokemon source investigation can run beside package work.
- Review or verification can run without editing the same files as a worker.

Do not spawn parallel writers for:

- The same package files.
- Shared generated artifacts.
- Both sides of a message or MIDI contract before the contract is written down.

Each subagent must report:

- Domain and allowed write scope.
- Files changed or inspected.
- Commands run and results.
- Evidence for behavior claims.
- Remaining risks or unknowns.

## Success Criteria

- Root `AGENTS.md` directs cross-project work to the monorepo framework.
- The framework exists under `.codex/skills/poryaaaa-monorepo-framework/`.
- Specialist files describe domain ownership without duplicating whole package
  `AGENTS.md` files.
- Pokemon reference prompts require both engine C and assembly/macro/song
  evidence for CC behavior claims.
- Build/test guidance names package-local verification commands and root wrapper
  scripts.
- The framework can be validated with file presence checks and a grep check for
  the Pokemon evidence rule.

