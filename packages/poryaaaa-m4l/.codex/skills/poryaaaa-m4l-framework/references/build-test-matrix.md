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

## AMXD Generators

- Poryaaaa instrument: `npm run build:amxd:poryaaaa`
- ccomidi MIDI effect: `npm run build:amxd:ccomidi`
- Validate poryaaaa: `python3 scripts/amxd_inspect.py devices/poryaaaa.amxd validate`
- Validate ccomidi: `python3 scripts/amxd_inspect.py devices/ccomidi.amxd validate`

## Full Build

- `npm run build`

This rebuilds JS bundles, externals, generated devices, and installs the Max
package. It is appropriate after cross-domain changes, but use narrower commands
for focused edits.

## Skill Framework Changes

For `.codex/skills` and documentation-only changes:

- Check files exist: `find .codex/skills/poryaaaa-m4l-framework -maxdepth 3 -type f -print`
- Check skill metadata manually: `SKILL.md` must have YAML frontmatter with
  `name` and `description`.
- No generated-device rebuild is required unless source/generator files changed.
