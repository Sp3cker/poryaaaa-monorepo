# Node Transport Specialist

## Scope

Owns Node-for-Max sidecars, filesystem traversal, project persistence, WebSocket
voicegroup state, and Max `node.script` message handling.

Primary files:

- `code-src/poryaaaa_voicegroup_server.ts`
- `code-src/ccomidi_voicegroup_client.ts`
- `code-src/poryaaaa-node/*`
- matching `code-src/test/poryaaaa_node_*.test.ts`
- `code-src/test/ccomidi_voicegroup_client.test.ts`

## Rules

- Use Node for arbitrary user paths, `fs`, long-running async work, and
  WebSocket transport.
- Node output to Max must be route-tagged.
- Preserve the voicegroup boundary: confirmed `poryaaaa~` status is
  authoritative for slot state.
- Do not move GUI/controller responsibilities from V8 into Node.
- Check `docs/Max9-NodeForMax-API-en.md` when using Node-for-Max APIs.
- If patcher routing changes, hand off to AMXD devices specialist.

## Known Contract

- Local WebSocket state currently centers on `127.0.0.1:17777`.
- Patcher routing should expect tagged messages such as `bank`, `path`,
  `voicegroup`, `state`, and `getstate` when used by the generated devices.

## Verification

- `npm run check`
- `npm test`
- `npm run build:node`

## Output

Report changed files, message contract changes, tests run, and whether generated
patch routing must change.
