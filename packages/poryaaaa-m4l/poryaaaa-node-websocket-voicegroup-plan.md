# poryaaaa Node WebSocket Voicegroup Plan

## Summary

Move poryaaaa voicegroup state out of `[v8 poryaaaa.js]` and into a Node for Max process. poryaaaa becomes the voicegroup state owner, persists project root/bank in Node, scans voicegroup banks in Node, validates and parses selected voicegroup `.inc` files for ccomidi's UI contract, emits poryaaaa UI and `voicegroup` commands to the patcher, and pushes ccomidi voice snapshots over WebSocket.

ccomidi keeps `[v8 ccomidi_voices.js]` as the GUI, LiveAPI, envelope, and routing controller. Only the voicegroup transport changes: a small Node sidecar connects to poryaaaa over WebSocket, receives typed snapshot frames, strips them down to `{ slots }`, URI-encodes that JSON for Max message safety, and forwards `state <encoded-json>` into `ccomidi_voices`.

## Decisions

- poryaaaa uses fixed localhost port `127.0.0.1:17777`.
- The fixed port bind is the singleton enforcement. Do not add lockfiles, identity handshakes, or arbitration.
- If poryaaaa cannot bind, fail loudly in the Max console and do not serve voicegroup state.
- Use native Node `http.Server` plus `ws`; no Express.
- WebSocket is the only voicegroup state API.
- WebSocket path is `/` only.
- No `/health` endpoint.
- Use `ws` and `@types/ws`; do not hand-roll WebSocket framing.
- WebSocket frames are plain JSON text.
- Server outbound snapshot frame shape is:
  ```ts
  {
    type: "snapshot";
    slots: VoiceSlot[];
  }
  ```
- The ccomidi Node sidecar forwards only `{ slots }` into V8, not the `type` envelope.
- poryaaaa server ignores all inbound client WebSocket messages for v1.
- ccomidi client silently ignores all non-`snapshot` WebSocket messages for v1.
- Delete the old voicegroup state flow outright:
  - no `messnamed("poryaaaa.state")`
  - no `Dict("poryaaaa.state")`
  - no `[r poryaaaa.state]`
  - no Max global `get_voices`
  - no poryaaaa `[v8]` object for voicegroup state
- Keep `ccomidi.reroute` on its existing Max global bus. It is separate from voicegroup state.
- Keep ccomidi V8 logic for GUI and LiveAPI work. Do not migrate ccomidi routing or voice UI to Node.
- Keep ccomidi's existing VoiceIdx flow:
  - `umenu` index -> `VoiceIdx` live.numbox
  - `VoiceIdx` sends `program <idx>` to `ccomidi`
  - `VoiceIdx` sends `pick <idx>` to `ccomidi_voices`
- VoiceIdx is user-facing line index in the loaded voicegroup, not a poryaaaa loader implementation detail.
- The snapshot payload sent to ccomidi contains only slots:
  ```ts
  {
    slots: VoiceSlot[];
  }
  ```
- Do not include `seq`, `projectRoot`, or `bank` in ccomidi snapshots.
- ccomidi applies every valid snapshot it receives. Do not reject snapshots by `seq`.
- poryaaaa Node is the authoritative validator for outbound snapshots. ccomidi keeps only minimal bad-frame safety.
- Remove ccomidi defensive defaults for missing `typeCode` or malformed `envelope`; malformed slots are producer bugs and should not be broadcast.
- Keep last-used poryaaaa root/bank persistence in Node. Do not key poryaaaa voicegroup state by Ableton Live Set path.
- Preserve the existing macOS path and unrelated project-keyed recorder entries:
  - `~/Library/Application Support/poryaaaa/projects.json`
  - top-level `root` and `bank` for poryaaaa voicegroup state
  - preserve unrelated fields such as `recorderFilename`, recorder range, and recorder loop markers
- Preserve malformed `projects.json` behavior as-is.
- Update platform paths when implementing Node persistence:
  - macOS: `~/Library/Application Support/poryaaaa/projects.json`
  - Windows: `%APPDATA%\poryaaaa\projects.json`
  - Linux: `${XDG_CONFIG_HOME:-~/.config}/poryaaaa/projects.json`
- Node creates the parent state directory automatically.
- poryaaaa does not need the Ableton Live Set `file_path`. Do not add patcher-side Live objects just to identify the Set.
- `[node.script]` has one practical outlet. Use route-tagged output and patcher fanout.
- Debug/status goes to `maxApi.post()` for now, not visible UI.
- Node scripts are authored in TypeScript and built to JavaScript in `javascript/`.
- Use separate JS build scripts for V8 and Node targets:
  - `build:v8`: IIFE/neutral bundles for `[v8]`
  - `build:node`: CommonJS/Node bundles for `[node.script]`
  - `build:js`: runs both
- Bundle `ws` into Node outputs if esbuild handles it cleanly.
- Keep `max-api` external because Node for Max provides it.
- Native TypeScript loading in Node for Max is acceptable for experiments only. Shipped devices load built JavaScript for reproducibility.

## Voicegroup Discovery And Parsing

Bank scanning:

- Scan only `<root>/sound/voicegroups/*.inc`.
- Ignore `.s` files completely for ccomidi-facing voicegroup state.
- Silently hide `.inc` banks whose basename starts with `se_`.
- Preserve filename case in displayed bank names.
- Sort bank names case-insensitively.
- Do not syntax-validate every bank during root scan.

Syntax validation and parsing:

- Validate only when a bank is selected, reloaded, or restored from saved state.
- Validation is syntax/structure validation for ccomidi's slot contract.
- Do not require referenced sample assets, wave data, or voice runtime assets to exist.
- If syntax validation fails:
  - post a diagnostic
  - do not send `voicegroup <root> <bank>` to `poryaaaa~`
  - do not broadcast a WebSocket snapshot
  - keep the previous latest snapshot, if any
- If syntax validation succeeds:
  - build a ccomidi snapshot
  - emit `voicegroup <root> <bank>` to `poryaaaa~`
  - update latest snapshot
  - broadcast the snapshot to connected ccomidi clients
- Do not wait for a poryaaaa external response before broadcasting.
- Runtime-invalid voices are discovered during playback or through existing external diagnostics. Do not build a second confirmation protocol for v1.

`VoiceSlot` contract:

```ts
interface VoiceSlot {
  program: number; // compact user-facing line index
  name: string;
  typeCode: number;
  envelope: {
    attack: number;
    decay: number;
    sustain: number;
    release: number;
  } | null;
}
```

Parser scope:

- Satisfy ccomidi's requirements, not full engine parity.
- Produce a compact, ordered slot list matching the user-visible line order.
- `program` means the compact ccomidi/VoiceIdx line index.
- No sparse padding or placeholder rows.
- No trailing empty slots.
- Parse enough voice macro syntax to derive `name`, `typeCode`, and ADSR/null envelope.
- Preserve hardware voice names and keysplit/comment names enough for useful menu labels.
- Reject and diagnose `voice_group ..., <startingNote>` offsets that would make compact ccomidi line index diverge from the engine Program Change value. V1 keeps ccomidi VoiceIdx unchanged rather than adding an engine-slot remap layer.

## poryaaaa Runtime Model

Node owns poryaaaa voicegroup behavior that `[v8 poryaaaa.js]` currently owns, plus the new ccomidi snapshot producer role:

- project id state
- root/bank persistence
- bank scanning and sorting
- current root and bank
- selected-bank syntax validation
- selected-bank slot parsing
- ccomidi snapshot validation
- bank menu command generation
- path label command generation
- `voicegroup <root> <bank>` command generation
- WebSocket server and connected ccomidi clients
- latest-on-connect and broadcast-on-successful-parse
- dump/debug state

The poryaaaa patcher owns wiring:

- UI controls into Node
- delayed `restore` into Node after `node.script` startup
- Node output through route fanout
- `bank` output into `umenu`
- `path` output into path label
- `voicegroup` output through `[prepend voicegroup]` into `poryaaaa~`, because `[route voicegroup]` strips the selector.

`poryaaaa~` owns engine loading and playback only:

- It receives `voicegroup <root> <bank>`.
- It applies the voicegroup to the engine.
- It does not marshal voicegroup JSON.
- Its old voicegroup `status <encoded-json>` path should be removed when no longer needed.
- It must not auto-load saved `vgroot` / `vgname` attrs on `loadbang`; Node must be the only voicegroup load path.
- Recorder `dumped` / `dumpfailed` status messages stay unchanged unless directly conflicted.

## Proposed Modules

Add focused Node modules rather than growing the old V8 entrypoint:

- `code-src/poryaaaa-node/state.ts`
  - Pure poryaaaa state machine.
  - Methods: `restore`, `rawroot`, `bankselect`, `reload`, `dumpstate`.
  - Emits typed commands such as bank clear/append/setsymbol, path set, voicegroup, and snapshot broadcast requests.

- `code-src/poryaaaa-node/project-store.ts`
  - Node `fs` persistence.
  - Creates parent directory.
  - Reads/writes compatible `projects.json`.
  - Preserves unrelated fields such as recorder settings.

- `code-src/poryaaaa-node/voicegroup-parser.ts`
  - Finds and parses `.inc` voicegroup files for ccomidi's slot contract.
  - Filters `se_` only during bank scan, not during selected-bank parser validation.
  - Does not load sample assets.
  - Validates snapshots before they can be broadcast.

- `code-src/poryaaaa-node/websocket-state.ts`
  - Native `http.Server` plus `ws`.
  - Fixed port `17777` in production, ephemeral port in tests.
  - Accepts path `/` only.
  - Starts immediately on Node startup.
  - On client connect, sends latest snapshot frame to that socket only if one exists.
  - On valid selected-bank parse/reload, broadcasts to all connected ccomidi sockets.
  - Ignores inbound client messages for v1.

- `code-src/poryaaaa_voicegroup_server.ts`
  - Node for Max entrypoint.
  - Registers `max-api` handlers.
  - Wires pure state commands to `maxApi.outlet(...)` and `maxApi.post(...)`.

Add ccomidi transport sidecar:

- `code-src/ccomidi_voicegroup_client.ts`
  - `ws` client.
  - Connects to `ws://127.0.0.1:17777/`.
  - Reconnects forever with a fixed delay.
  - Keeps the last applied voice list visible after disconnect.
  - Validates basic WebSocket frame shape.
  - Accepts only `{ type: "snapshot", slots }`.
  - Forwards `state <encodeURIComponent(JSON.stringify({ slots }))>` to ccomidi V8.
  - Silently ignores non-snapshot messages.

## Patcher Changes

### poryaaaa

- Remove `[v8 poryaaaa.js]`.
- Remove `[r get_voices]` and all poryaaaa wiring for `get_voices`.
- Add `[node.script poryaaaa_voicegroup_server.js @autostart 1]`.
- Keep explicit `script start` boot wiring for Node for Max.
- Gate initial restore work so it cannot arrive before `node.script` is ready:
  - `[live.thisdevice] -> [script start] -> node.script`
  - separate delayed branch, e.g. `[live.thisdevice] -> [delay 500] -> [restore] -> node.script`
  - use `[trigger]` where deterministic ordering matters
- Do not use Max/Live objects to fetch Live Set `file_path` for poryaaaa voicegroup persistence.
- Route Node output:
  - `bank ...` to bank `umenu`
  - `path ...` to path label
  - `voicegroup ...` through `[prepend voicegroup]` to `poryaaaa~`
- Do not route `poryaaaa~ status` voicegroup JSON back into Node.
- Keep `poryaaaa~` status outlet routing for recorder replies:
  - keep `dumped` and `dumpfailed` wired to `ccomidi_recorder.js`
  - remove only the voicegroup JSON `status -> scanner/Node` flow
- Keep recorder cleanup sidecar unchanged unless the new wiring directly conflicts.

### ccomidi

- Keep `[v8 ccomidi_voices.js]`.
- Keep `VoiceIdx` / `umenu` / `program <idx>` / `pick <idx>` behavior as-is.
- Keep `ccomidi.reroute` wiring as-is.
- Remove `[r poryaaaa.state]`.
- Remove any `get_voices` request path.
- Add `[node.script ccomidi_voicegroup_client.js @autostart 1]`.
- Keep explicit `script start` boot wiring for Node for Max.
- Route client `state <encoded-json>` output into `ccomidi_voices`.

## Snapshot Behavior

- poryaaaa stores the latest valid ccomidi snapshot internally.
- ccomidi connect:
  - if latest exists, send `{ type: "snapshot", slots }` only to the newly connected socket
  - if latest does not exist, keep socket open and wait
- successful selected-bank parse:
  - emit `voicegroup <root> <bank>` to poryaaaa external
  - update latest snapshot
  - broadcast `{ type: "snapshot", slots }`
- parse failure:
  - post diagnostic
  - do not emit `voicegroup`
  - do not overwrite latest snapshot
  - do not broadcast
- ccomidi:
  - shows waiting until first valid snapshot
  - applies every valid snapshot received
  - keeps current menu on disconnect/reconnect

## Inbound poryaaaa Node API

- `restore`
- `rawroot <folder path>`
- `bankselect <bank name>`
- `reload`
- `dumpstate`

Do not keep `ready`, `root`, `status`, or `get_voices` unless implementation discovers a concrete remaining caller that cannot be removed safely.

## Build Plan

- Add `ws` to dependencies.
- Add `@types/ws` to devDependencies.
- Add `build:v8` for existing `[v8]` scripts.
- Add `build:node` for Node for Max scripts:
  - CommonJS output
  - Node platform
  - `max-api` external
  - `ws` bundled if practical
- Make `build:js` run both `build:v8` and `build:node`.
- Keep top-level `npm run build` behavior intact.
- Add a short docs note explaining why Node scripts are built JS even though modern Node can type-strip TypeScript.

## Test Plan

- Unit test `project-store`:
  - missing file returns no state
  - writes top-level root/bank
  - preserves project-keyed recorder entries
  - creates parent directory
  - malformed JSON behavior remains compatible
  - platform path resolution covers macOS, Windows, and Linux

- Unit test bank scanning:
  - lists only `.inc` files from `sound/voicegroups`
  - ignores `.s`
  - silently hides `se_*.inc`
  - preserves display case
  - sorts case-insensitively

- Unit test voicegroup parser:
  - parses the voice macro forms ccomidi needs
  - emits compact line-indexed slots
  - sets `program` to compact line index
  - emits `typeCode`
  - emits ADSR envelope for envelope-capable voices
  - emits `envelope: null` for keysplits
  - uses useful display names for hardware voices and keysplits/comments
  - rejects malformed syntax
  - rejects `voice_group` starting offsets that would break compact VoiceIdx-to-Program-Change behavior
  - does not require sample assets to exist

- Unit test poryaaaa state service:
  - `restore` restores saved root/bank
  - restore emits bank/path UI commands
  - valid saved bank emits `voicegroup` and broadcasts snapshot
  - invalid saved bank posts diagnostic and does not emit/broadcast
  - stale saved bank does not emit `voicegroup`
  - `rawroot` scans and persists root without validating every bank
  - `bankselect` validates, persists, emits `voicegroup`, and broadcasts
  - `reload` reparses and broadcasts even when root/bank are unchanged
  - parse failure does not overwrite latest snapshot
  - numeric/string boundary validation matches current behavior where still relevant

- Unit test WebSocket transport:
  - server binds fixed port in production path and ephemeral port in tests
  - only `/` is accepted
  - client connecting after latest exists receives latest only on that socket
  - new snapshot broadcasts to all connected clients
  - inbound client messages no-op
  - client reconnects after close with fixed delay
  - invalid/non-snapshot payloads are ignored by ccomidi client

- Unit test ccomidi V8 simplification:
  - applies `{ slots }`
  - no `seq` de-dupe
  - no defaults for missing `typeCode`
  - no defaults for malformed `envelope`
  - ignores empty slot arrays
  - keeps VoiceIdx behavior unchanged

- Generator tests:
  - poryaaaa generated patch contains no `v8 poryaaaa.js`
  - poryaaaa generated patch contains `node.script poryaaaa_voicegroup_server.js @autostart 1`
  - poryaaaa generated patch contains route fanout for `bank path voicegroup`
  - poryaaaa generated patch re-prepends `voicegroup` after route before `poryaaaa~`
  - poryaaaa generated patch has delayed/gated initial `restore` delivery to `node.script`
  - poryaaaa generated patch keeps `dumped` / `dumpfailed` recorder reply wiring intact
  - poryaaaa generated patch contains no `r get_voices`
  - ccomidi generated patch contains no `r poryaaaa.state`
  - ccomidi generated patch contains no `get_voices` state request wiring
  - ccomidi generated patch contains `node.script ccomidi_voicegroup_client.js @autostart 1`
  - generated AMXDs pass `scripts/amxd_inspect.py <device> validate`
  - generated patchlines have valid serialized inlet/outlet metadata for `node.script`, `route`, and `prepend`

- Existing checks:
  - `npm run check`
  - `npm test`
  - `npm run build:js`
  - regenerate poryaaaa and ccomidi AMXDs
  - copy both AMXDs to Ableton User Library and verify copies match

## Acceptance Criteria

- poryaaaa device works without `[v8 poryaaaa.js]`.
- poryaaaa restores the last saved root/bank from `projects.json` after device load.
- poryaaaa persists in-memory root/bank when root/bank changes.
- poryaaaa bank menu lists only non-`se_` `.inc` voicegroup banks.
- poryaaaa bank menu and path label update through Node output.
- poryaaaa validates selected/restored/reloaded bank syntax before loading.
- poryaaaa sends `voicegroup <root> <bank>` to `poryaaaa~` only after selected bank syntax validates.
- poryaaaa does not auto-load voicegroups from saved external attrs outside the Node validation path.
- poryaaaa patcher re-prepends `voicegroup` after route so `poryaaaa~` receives the required selector.
- recorder `dumped` / `dumpfailed` replies still reach the recorder V8.
- poryaaaa broadcasts only validated ccomidi slot snapshots.
- poryaaaa WebSocket listens on `ws://127.0.0.1:17777/`.
- ccomidi populates voices from WebSocket snapshots.
- ccomidi receives latest state on WebSocket connect when poryaaaa already has one.
- connected ccomidi instances receive broadcasts when poryaaaa parses a selected/reloaded voicegroup.
- ccomidi keeps the last valid menu visible during WebSocket reconnect.
- ccomidi VoiceIdx behavior remains line-index based and unchanged.
- No voicegroup state path uses `messnamed("poryaaaa.state")`, `Dict("poryaaaa.state")`, `[r poryaaaa.state]`, or `get_voices`.
- `ccomidi.reroute` still works.
