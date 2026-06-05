# Domain Map

Use this map before editing or spawning subagents.

## Domains

| Domain | Specialist | File ownership | Primary triggers |
| --- | --- | --- | --- |
| TypeScript V8 | `specialists/typescript-v8.md` | `code-src/*.ts` except Node-owned paths | V8, GUI, controller, recorder UI, LiveAPI, scanner, voices |
| Node transport | `specialists/node-transport.md` | `code-src/poryaaaa-node/*`, `code-src/poryaaaa_voicegroup_server.ts`, `code-src/ccomidi_voicegroup_client.ts` | node.script, WebSocket, voicegroup state, project store, filesystem |
| C++ externals | `specialists/cpp-externals.md` | `source/audio/poryaaaa~/`, `source/midi/ccomidi/`, CMake | external, mxo, poryaaaa~, ccomidi, MIDI parser, recorder buffer |
| AMXD generator | `specialists/amxd-generator.md` | `scripts/gen_*_amxd.py`, `scripts/_amxd_helpers.py`, `devices/*.amxd` | py2max, .amxd, patchcord, presentation, live widgets |
| Max/M4L reference | `specialists/max-m4l-reference.md` | `docs/max-ref/`, `docs/max-gotchas.md`, Max/LOM docs | Max object, Live API, LOM, object syntax, inlets/outlets |
| Verification | `specialists/verification.md` | tests and command selection | build, test, check, validate, smoke |

## Lead Selection

- Generated-device structural problems: AMXD generator lead, Max/M4L reference
  secondary, Verification final.
- Voicegroup transport behavior: Node transport lead, TypeScript V8 secondary
  if UI/controller code receives the state, AMXD generator secondary if patcher
  routing changes.
- Recorder save flow: TypeScript V8 lead, C++ external secondary if capture
  bytes or wrapper messages change.
- MIDI parsing/playback bugs: C++ externals lead, TypeScript V8 secondary only
  for UI or recorder serialization.
- Live API object usage or patcher syntax: Max/M4L reference lead, AMXD
  generator secondary if generated patch changes.
- Cross-domain feature work: router lead first, then spawn one subagent per
  independent domain.

## Spawn Examples

Spawn subagents:

- Review Node WebSocket state behavior while another agent audits generated
  patch routing.
- Investigate C++ MIDI parser behavior while another agent checks TypeScript
  recorder tests.
- Run a Max reference lookup pass while the main agent edits generator code.

Do not spawn parallel writers:

- Two agents editing the same generator.
- One agent editing `scripts/_amxd_helpers.py` while another regenerates
  devices from it.
- One agent changing the Node/V8 message contract while another changes the
  receiving side without a shared contract written first.
