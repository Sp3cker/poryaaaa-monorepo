# Build And Test Matrix

Run commands from repo root.

## TypeScript V8

- Typecheck: `npm run check`
- Tests: `npm test`
- Bundle: `npm run build:v8`

## Node Transport

- Typecheck: `npm run check`
- Tests: `npm test`
- Bundle: `npm run build:node`

## C++ Externals

- Build: `npm run build:externals`
- Parser tests, if already wired in the build tree: prefer the repo's CMake test
  target when present. If no target is present, report that only build
  verification was available.

## AMXD Devices (hand-maintained)

- Inspect/validate after hand edits in Max:
  `python3 scripts/amxd_inspect.py devices/poryaaaa.amxd validate`
- Same for ccomidi: `python3 scripts/amxd_inspect.py devices/ccomidi.amxd validate`
- No npm scripts rebuild devices (they are saved directly from Max).

## Full Build

- `npm run build`

This rebuilds JS bundles, externals, and installs the Max package. It is
appropriate after cross-domain changes, but use narrower commands for focused
edits. Devices are not part of the automated build.

## Skill Framework Changes

For `.codex/skills` and documentation-only changes:

- Check files exist: `find .codex/skills/poryaaaa-m4l-framework -maxdepth 3 -type f -print`
- Check skill metadata manually: `SKILL.md` must have YAML frontmatter with
  `name` and `description`.
- No device rebuild is required for framework/doc-only changes.
