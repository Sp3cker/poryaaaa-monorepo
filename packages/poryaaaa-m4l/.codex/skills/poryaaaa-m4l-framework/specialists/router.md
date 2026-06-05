# Router Specialist

## Scope

Use for any task touching more than one poryaaaa-m4l domain or any request to
spawn subagents.

## Inputs

- User request.
- `references/domain-map.md`.
- `references/context-loading.md`.
- Current `git status --short`.

## Process

1. Identify domains and lead specialist.
2. Decide which work stays local on the critical path.
3. Spawn subagents only for independent sidecar domains with disjoint write
   scopes.
4. Give each subagent exact files/directories and expected output.
5. Merge with `references/merge-protocol.md`.
6. Review with `references/critic-protocol.md`.

## Output

- Routing decision.
- Subagents spawned, if any, with ownership.
- Merge result and verification plan.

## Handoffs

- Generated patch/device edits: AMXD generator.
- Object syntax and Live API uncertainty: Max/M4L reference.
- Source code implementation: the owning language/domain specialist.
