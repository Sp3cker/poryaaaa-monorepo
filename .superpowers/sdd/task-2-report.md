# Task 2 Report: M4L parser adapter voicegroup-core-node cutover

## Status
- Complete.
- `scanVoicegroupBanks` now delegates to `@poryaaaa/voicegroup-core-node` and returns project-indexed banks from `sound/voice_groups.inc`.
- `parseVoicegroup` now delegates to `@poryaaaa/voicegroup-core-node`, strips a trailing `.inc` suffix before delegation, maps successful core slots into the unchanged M4L `{ name, typeCode }` slot contract, and maps core diagnostics into diagnostic strings.
- No fallback to `poryaaaa_voicegroup.node` remains in `voicegroup-parser.ts`.

## Changed files
- `packages/poryaaaa-m4l/code-src/test/poryaaaa_node_voicegroup_parser.test.ts`
- `packages/poryaaaa-m4l/code-src/poryaaaa-node/voicegroup-parser.ts`

## Commands and results

### RED: focused parser test after replacing tests
Command, run from `packages/poryaaaa-m4l`:

```bash
npm test -- code-src/test/poryaaaa_node_voicegroup_parser.test.ts
```

Result: failed as expected against the old adapter.

Key failures:
- `scanVoicegroupBanks` returned `['alpha', 'unlisted']` instead of only project-indexed `['alpha']`.
- `parseVoicegroup` success assertions failed because the old native adapter path did not satisfy the new core-backed semantics.
- Exit code: 1.

### GREEN: focused parser test after replacing adapter
Command, run from `packages/poryaaaa-m4l`:

```bash
npm test -- code-src/test/poryaaaa_node_voicegroup_parser.test.ts
```

Result:

```text
ℹ tests 250
ℹ suites 0
ℹ pass 250
ℹ fail 0
ℹ cancelled 0
ℹ skipped 0
ℹ todo 0
ℹ duration_ms 438.944417
```

Exit code: 0.

### Fallback check
Searched `packages/poryaaaa-m4l/code-src/poryaaaa-node/voicegroup-parser.ts` for old native fallback markers:

```text
poryaaaa_voicegroup.node|native\(|existsSync|readdirSync
```

Result: no matches.

## Concerns
- The requested focused npm command runs through the package's existing `test` script, which includes `code-src/test/*.test.ts`; the observed green run therefore executed the parser test plus the rest of the package test glob. It passed with 250/250 tests.
