# Domain Map

Use this map before editing or spawning subagents.

## Domains

| Domain | Specialist | File ownership | Primary triggers |
| --- | --- | --- | --- |
| TypeScript V8 | `specialists/typescript-v8.md` | `code-src/*.ts` except Node-owned paths | V8, GUI, controller, recorder UI, LiveAPI, scanner, voices |
| Node transport | `specialists/node-transport.md` | `code-src/poryaaaa-node/*`, `code-src/poryaaaa_voicegroup_server.ts`, `code-src/ccomidi_voicegroup_client.ts` | node.script, WebSocket, voicegroup state, project store, filesystem |
| C++ externals | `specialists/cpp-externals.md` | `source/audio/poryaaaa~/`, `source/midi/ccomidi/`, CMake | external, mxo, poryaaaa~, ccomidi, MIDI parser, recorder buffer |
| AMXD devices | `specialists/amxd-generator.md` | `devices/*.amxd`, `scripts/amxd_inspect.py` | hand-edited devices, patcher structure, amxd_inspect validation, presentation/patching layout |
| Max/M4L reference | `specialists/max-m4l-reference.md` | `docs/max-ref/`, `docs/max-gotchas.md`, Max/LOM docs | Max object, Live API, LOM, object syntax, inlets/outlets |
| Verification | `specialists/verification.md` | tests and command selection | build, test, check, validate, smoke |

## Lead Selection

- Device structural / patcher problems: AMXD devices lead, Max/M4L reference
  secondary, Verification final.
- Voicegroup transport behavior: Node transport lead, TypeScript V8 secondary
  if UI/controller code receives the state, AMXD devices secondary if patcher
  routing changes.
- Recorder save flow: TypeScript V8 lead, C++ external secondary if capture
  bytes or wrapper messages change.
- MIDI parsing/playback bugs: C++ externals lead, TypeScript V8 secondary only
  for UI or recorder serialization.
- Live API object usage or patcher syntax: Max/M4L reference lead, AMXD
  devices secondary if patcher changes.
- Cross-domain feature work: router lead first, then spawn one subagent per
  independent domain.

## Spawn Examples

Spawn subagents:

- Review Node WebSocket state behavior while another agent audits patcher
  routing in a device.
- Investigate C++ MIDI parser behavior while another agent checks TypeScript
  recorder tests.
- Run a Max reference lookup pass while the main agent edits device layout.

Do not spawn parallel writers:

- Two agents editing the same device .amxd.
- One agent editing device patch cords while another is validating the same
  device with amxd_inspect.
- One agent changing the Node/V8 message contract while another changes the
  receiving side without a shared contract written first.
