# Repo Rules

## Environment

- Target runtime: Ableton Live 12.4, Max 9.
- Node engine: `>=24.6.0`.
- The Max package install path is created by `scripts/install_max_package.sh`,
  which symlinks the repo into `~/Documents/Max 9/Packages/poryaaaa-m4l`.

## Source Ownership

- `code-src/*.ts`: TypeScript source.
- `javascript/*.js`: generated bundles. Do not edit directly.
- `code-src/ccomidi_voices.ts` and `code-src/ccomidi_recorder.ts`: V8 bundles.
- `code-src/poryaaaa_voicegroup_server.ts`,
  `code-src/ccomidi_voicegroup_client.ts`, and `code-src/poryaaaa-node/*`:
  Node-for-Max bundles and transport support.
- `source/audio/poryaaaa~/`: `poryaaaa~` audio external and recorder support.
- `source/midi/ccomidi/`: `ccomidi` MIDI external and parser tests.
- `devices/*.amxd`: hand-maintained device binaries (opened/edited/saved directly
  in Max; validated with amxd_inspect after changes).

## Voicegroup Boundary

- `poryaaaa~` emits `status <uri-encoded-json>` after successful voicegroup
  load. That confirmed payload is authoritative for slot state.
- V8 owns GUI/controller logic and Live API integration that belongs inside the
  device controller.
- Node owns filesystem traversal, project-store persistence, WebSocket
  transport, and voicegroup state broadcast.
- Do not create a second independent state source to compete with confirmed
  `poryaaaa~` state.

## Max/M4L Rules

- Historically, the `.amxd` devices were produced by Python generators (py2max +
  custom packers). They are now hand-maintained: open `devices/*.amxd` in Max,
  make edits in the patcher (patching view and presentation), Save in place, then
  run `python3 scripts/amxd_inspect.py devices/<name>.amxd validate` and commit
  the binary. This is the current and only supported workflow.
- Use `[v8]` for in-device controller logic.
- Use `[node.script]` for arbitrary filesystem access, long-running async work,
  WebSocket transport, and npm ecosystem code.
- `node.script` should use `@autostart 1`, plus an explicit `script start`
  from `[live.thisdevice]` boot wiring.
- Route Node output with tags, for example `route bank path voicegroup state`.
- Live device output uses bare `[plugout~ 1]` and `[plugout~ 2]`, not
  `[plugout~ Out~ N]`.
- Non-UI Live API objects such as `live.thisdevice`, `live.path`,
  `live.object`, and `live.observer` must serialize as `maxclass: "newobj"`
  with the object name in `text`; native UI widgets remain native `live.*`
  maxclasses.
- `[midiin]` emits raw MIDI bytes. Do not insert `[midiparse]` in the main byte
  path when sysex/XCMD-style bytes must survive.
- When inlet/outlet counts matter for patchlines (e.g. `[route ...]`, `[sel ...]`),
  set them explicitly with `add_raw(...)` (or verify after hand-edits).

## Device Maintenance Rules

- Devices are hand-maintained (we used to generate them with Python scripts; we
  no longer do). Open the .amxd from the repo in Max, edit the patcher (patching
  rects, cords, bpatchers, live.* widgets, etc.), and Save in place over the file
  in `devices/`.
- After any structural change (especially cords or object counts), run
  `python3 scripts/amxd_inspect.py devices/<device>.amxd validate` (plus boxes/
  cords queries as needed) and commit the updated binary .amxd.
- The `.amxd` files use the factory-style `ampf + meta + ptch + JSON` layout
  produced by Max on save.
- Non-UI Live API objects serialize as `maxclass: "newobj"` with the object name
  in `text`; UI widgets (live.dial etc.) remain native maxclasses.
- Presentation layout (`presentation` + `presentation_rect`) is part of the
  deliverable for anything shown in Live's device strip.

## Feature Removal

Before destructive cleanup, preserve a clean baseline commit if the user asks
for it. Remove source, tests, generators, generated artifacts, and build
references together; do not leave orphaned device code.
