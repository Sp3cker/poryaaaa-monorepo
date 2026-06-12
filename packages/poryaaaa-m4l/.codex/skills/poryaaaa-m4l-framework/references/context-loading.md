# Context Loading

Load only what the task needs.

## Always

- `package.json` for current command names.
- `references/repo-rules.md`.
- `references/domain-map.md`.

## By Domain

- V8 TypeScript: relevant `code-src/*.ts`, matching tests in `code-src/test/`,
  and `docs/Max9-JS-API-en.md` only when using Max JS APIs.
- Node transport: `code-src/poryaaaa-node/*`,
  `code-src/poryaaaa_voicegroup_server.ts`,
  `code-src/ccomidi_voicegroup_client.ts`, matching tests, and
  `docs/Max9-NodeForMax-API-en.md` only when using Node-for-Max APIs.
- C++ externals: relevant files under `source/audio/poryaaaa~/` or
  `source/midi/ccomidi/`, local CMake files, and focused tests.
- AMXD devices: `devices/*.amxd` and `scripts/amxd_inspect.py` (usage docs
  from `specialists/amxd-generator.md` — the hand-maintained devices specialist).
- Max/M4L reference: exact object reference under `docs/max-ref/<domain>/`,
  `docs/max-gotchas.md`, `docs/Max9-LOM-en.md` for Live Object Model work.

## Avoid

- Do not load all Max reference docs.
- Do not load generated `javascript/*.js` (the bundles) unless debugging bundle output.
- Do not load binary `.amxd` raw content; use `scripts/amxd_inspect.py`.
- Do not use `CLAUDE.md` as project context. It is replaced.
