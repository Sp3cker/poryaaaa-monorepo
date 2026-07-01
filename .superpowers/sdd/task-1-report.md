# Task 1 Report: Replace M4L native-addon build contract

## Status
DONE

## Changed files
- `packages/poryaaaa-m4l/code-src/test/poryaaaa_node_build_contract.test.ts`
- `packages/poryaaaa-m4l/package.json`
- `packages/poryaaaa-m4l/package-lock.json`
- `justfile`

## Commands run

### RED: focused build-contract test before implementation
Command, from `packages/poryaaaa-m4l`:

```bash
npm test -- code-src/test/poryaaaa_node_build_contract.test.ts
```

Result: FAIL, exit code 1. Expected contract mismatch observed:

```text
AssertionError [ERR_ASSERTION]: Expected values to be strictly equal:
+ actual - expected

+ undefined
- 'file:../voicegroup-core-node'
```

### Update package lock
Command, from `packages/poryaaaa-m4l`:

```bash
npm install
```

Result: PASS, exit code 0.

```text
added 1 package in 238ms
```

### GREEN: focused build-contract test after implementation
Command, from `packages/poryaaaa-m4l`:

```bash
npm test -- code-src/test/poryaaaa_node_build_contract.test.ts
```

Result: PASS, exit code 0.

```text
ℹ tests 250
ℹ suites 0
ℹ pass 250
ℹ fail 0
ℹ cancelled 0
ℹ skipped 0
ℹ todo 0
ℹ duration_ms 470.6995
```

## Verification notes
- `package.json` declares `@poryaaaa/voicegroup-core-node` as `file:../voicegroup-core-node`.
- `build:napi` is now `npm --prefix ../voicegroup-core-node run build`.
- `bundle:node` now includes `--external:@poryaaaa/voicegroup-core-node`.
- `package-lock.json` includes the local file dependency and linked `node_modules/@poryaaaa/voicegroup-core-node` entry.
- Root `justfile` M4L build branch now builds `packages/voicegroup-core-node` with npm before running the existing full M4L build.
- Verified no `poryaaaa_napi` references remain in `packages/poryaaaa-m4l/package.json` or `justfile`.

## Concerns
None.

---

## Additional watch:node externalization fix

### Changed files
- `packages/poryaaaa-m4l/code-src/test/poryaaaa_node_build_contract.test.ts`
- `packages/poryaaaa-m4l/package.json`
- `.superpowers/sdd/task-1-report.md`

### RED: focused build-contract test after adding watch:node assertion
Command, from `packages/poryaaaa-m4l`:

```bash
npm test -- code-src/test/poryaaaa_node_build_contract.test.ts
```

Result: FAIL, exit code 1. Expected `watch:node` contract mismatch observed:

```text
AssertionError [ERR_ASSERTION]: The input did not match the regular expression /--external:@poryaaaa\/voicegroup-core-node/. Input:

'esbuild code-src/poryaaaa_voicegroup_server.ts code-src/ccomidi_voicegroup_client.ts code-src/cleanup.ts --bundle --outdir=javascript --format=cjs --platform=node --target=node24 --external:max-api --watch'
```

### GREEN: focused build-contract test after updating watch:node
Command, from `packages/poryaaaa-m4l`:

```bash
npm test -- code-src/test/poryaaaa_node_build_contract.test.ts
```

Result: PASS, exit code 0.

```text
ℹ tests 250
ℹ suites 0
ℹ pass 250
ℹ fail 0
ℹ cancelled 0
ℹ skipped 0
ℹ todo 0
ℹ duration_ms 478.744708
```

### Final focused build-contract verification
Command, from `packages/poryaaaa-m4l`:

```bash
npm test -- code-src/test/poryaaaa_node_build_contract.test.ts
```

Result: PASS, exit code 0.

```text
ℹ tests 250
ℹ suites 0
ℹ pass 250
ℹ fail 0
ℹ cancelled 0
ℹ skipped 0
ℹ todo 0
ℹ duration_ms 480.702667
```

### Verification notes
- `watch:node` now includes `--external:@poryaaaa/voicegroup-core-node`.
- No generated JS or device files were edited by this task.
