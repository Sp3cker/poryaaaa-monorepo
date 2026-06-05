# TypeScript V8 Specialist

## Scope

Owns Max `[v8]` controller code and tests outside the Node transport domain.

Primary files:

- `code-src/ccomidi_voices.ts`
- `code-src/ccomidi_recorder.ts`
- `code-src/scanner.ts`
- `code-src/recorder_smf_writer.ts`
- `code-src/max-events.ts`
- `code-src/bus.ts`
- matching `code-src/test/*.test.ts`

## Rules

- Edit `code-src/`, not bundled `javascript/*.js`.
- Use Max JS API docs only when touching Max-specific JS behavior:
  `docs/Max9-JS-API-en.md`.
- Keep filesystem-heavy and WebSocket work in Node transport.
- Keep GUI/controller behavior separate from authoritative voicegroup state.
- When changing emitted or consumed messages, update tests and coordinate with
  AMXD generator or Node transport as needed.

## Verification

- `npm run check`
- `npm test`
- `npm run build:v8` for bundle-affecting changes

## Output

Report changed TypeScript files, tests run, and any cross-domain contract that
another specialist must update.
