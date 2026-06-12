# Envelope for voice

Carry per-slot ADSR from `poryaaaa~` to `ccomidi` so the envelope dials reflect
the selected voicegroup voice, and so the persisted **Send Env** toggle gates
whether dial moves emit MIDI XCMD rows. Built on top of the v8-bus state
plumbing (no `state.json`).

## Goals

1. `poryaaaa~` includes ADSR (or `null` for keysplits) in each slot of the
   `status` payload.
2. ccomidi's voice-select path auto-fills the four ADSR `live.dial`s when a
   voice is picked.
3. Selecting a keysplit voice forces Send Env = 0 and greys it.
4. Turning Send Env from ON → OFF re-emits the stock envelope so `poryaaaa~`
   reverts to the voicegroup-defined ADSR before row enables drop.

## Current state

Historically the `.amxd` devices (including ccomidi) were produced by Python
generators. They are now hand-maintained: edit `devices/ccomidi.amxd` directly
in Max and Save, then validate with `python3 scripts/amxd_inspect.py
devices/ccomidi.amxd validate`.

State transport uses the Node WebSocket voicegroup path (`poryaaaa_voicegroup_server.ts`
+ `ccomidi_voicegroup_client.ts`). The v8 side lives in `code-src/ccomidi_voices.ts`
(which contains the slot parsing and selection logic that used to be split across
separate service modules).

Patcher prep work for the envelope UI is in place in the hand-maintained
`devices/ccomidi.amxd`:
- Four `live.dial`s (Attack/Decay/Sustain/Release) wired to rows.
- `Send Env` `live.toggle` drives enable bits, with `#0-sync` fanout.

The route after the ccomidi v8 and the back-path from the toggle into v8 still
need the envelope-specific messages (`senvchanged` etc.) if not already wired.

## Wire-format change

Per-slot in the `status` JSON poryaaaa~ emits, and therefore in every
`StateBroadcast`/`Dict` payload that ccomidi reads:

```json
{ "program": 12, "name": "Square 1",
  "envelope": { "attack": 3, "decay": 2, "sustain": 8, "release": 1 } }
{ "program": 40, "name": "Piano Split",
  "envelope": null }
```

Rules:

- Keysplit voice types (`VOICE_KEYSPLIT`, `VOICE_KEYSPLIT_ALL` — names from
  the engine's `voicegroup_types.h`; verify the codes during implementation)
  → `"envelope": null`.
- All other types → object with four integer fields, copied raw from the
  slot's `ToneData` (engine's `0..255` byte range, no scaling at the writer).
- No `type`, `typeCode`, `supportsEnvelope`, or `schemaVersion` fields.
  Reader derives `supportsEnvelope := envelope !== null`.
- A slot whose `envelope` is present but malformed (missing a field,
  non-numeric) is treated as `null` plus a `post()` diagnostic. The slot
  stays in the menu; no crash, no skip.

## C external — `source/audio/poryaaaa~/poryaaaa~.c`

`porya_emit_status` (currently lines 479-522) walks `vg->voiceSampleNames[i]`
and emits `{program, name}` per non-empty slot. Extend to also read
`vg->voices[i]` (the `ToneData` array) and write `envelope`:

- Add a static helper near the top of the file:
  `static bool voice_type_supports_envelope(uint8_t type)` → `false` for
  keysplit codes, `true` otherwise. Confirm the codes by reading the engine's
  `voicegroup_types.h` once located (the header is *not* in this repo's
  tree — it's pulled in via `voicegroup/voicegroup_loader.h`).
- For each emitted slot, append `,"envelope":{...}` or `,"envelope":null`
  after the existing `name` field.
- Keep the existing slot-inclusion filter
  (`voiceSampleNames[i][0] != '\0'`) for now. **Verify** during
  implementation that the engine populates names for square / noise /
  programmable-wave / cry / keysplit slots — if any voice family parses to
  an empty `voiceSampleNames[i]`, no envelope will ever be emitted for it
  regardless of carrier-side work, and that's an upstream bug to fix
  separately.
- Recompute the JSON buffer math. Today: `char json[24576]`, comment claims
  "128 slots × ~140 chars + headers." Each envelope adds ~50 chars, so worst
  case ≈ 128 × 190 ≈ 24 KB. Bump to `char json[32768]` (and the
  `encoded[]` staging buffer to match) to keep margin.
- The "drop broken `#include "voicegroup/voicegroup_state.h"`" cleanup is
  already done. No further removal needed.

## Implementation notes (see current code)

The slot/envelope types, parsing, and fan-out now live in `code-src/ccomidi_voices.ts`
(and supporting voice-slot-contract). The v8 emits the additional `senv*` messages
on voice selection / toggle changes so the patcher can drive the ADSR dials and
enable bits. LOM / state details are in the Node side (`poryaaaa-node/`). 

Consult the source for the exact current shapes rather than the historical plan
text below.
  - `senvactive 1` — un-grey.
  - `dialset attack <a>`, `dialset decay <d>`, `dialset sustain <s>`,
    `dialset release <r>`. Values are emitted **as the JSON bytes** — the
    range-scaling lives in the patcher's dial → XCMD chain (see Outstanding
    questions). Do not preset the toggle to `1`; let the user opt in.

Add a `senvchanged(value)` service method so the toggle's int can route
through v8 and trigger the OFF→restore behavior:

- On `senvchanged 0`, if the currently selected slot has a non-null
  envelope, re-emit the stock `dialset attack/decay/sustain/release` for
  that slot, **then** `senvtoggle 0`. Ordering matters: stock dial values
  must reach `[ccomidi]` while r5..r8 enables are still high, so
  `poryaaaa~` actually receives the reset. After the dial emissions, the
  toggle's `0` propagates the row-disable.
- On `senvchanged 1`, no-op (the toggle's transition into ON already lets
  current dial values flow through).
- For keysplit slots, no-op — Send Env is already forced off and disabled.

State the service needs to keep:

- Selected slot index (already implicit via `lastRestoredName`; promote to
  an explicit `selectedIdx | null`).

`VoicesService` interface gains `senvchanged: (value: unknown) => void`.

## Tests — `code-src/test/ccomidi_voices.test.ts`

Extend the existing test file. New cases:

- Bus payload with `slots[].envelope` objects → service stores them; select
  on an envelope-capable slot emits, in order: `program <p>`,
  `senvactive 1`, `dialset attack <a>`, `dialset decay <d>`,
  `dialset sustain <s>`, `dialset release <r>`. **No** `senvtoggle`
  emission.
- Bus payload with `slots[].envelope === null` → select emits
  `program <p>`, `senvactive 0`, `senvtoggle 0`. **No** `dialset`
  emissions.
- Bus payload with malformed envelope (e.g. missing `decay`) on one slot →
  service treats it as `null` (select behaves like keysplit), `post` was
  called once. Other slots in the same payload are unaffected.
- `senvchanged 0` after selecting an envelope-capable slot → re-emits the
  four `dialset` messages followed by `senvtoggle 0`, in that order.
- `senvchanged 0` after selecting a keysplit slot → no emissions
  (already off + disabled).
- `senvchanged 1` → no emissions.

`code-src/test/_mocks.ts` already exposes `MockBus` with `seedSnapshot()`
and `deliver()` — sufficient for all of the above.

## Patcher — `devices/ccomidi.amxd` (hand-maintained)

Build on the existing envelope section (hand-edit the post-v8 route and
envelope wiring directly in the .amxd — we used to generate these devices
but now edit and save them manually in Max):

1. **Expand the `[route]` after the v8 outlet** from `slots program` to
   `slots program senvtoggle senvactive dialset`. Wire each new outlet:
   - `senvtoggle <0|1>` → `send_toggle`'s int input. Setting the value
     re-fires the existing `r{N}_en $1` chain that's already wired (lines
     506-510).
   - `senvactive <0|1>` → message `active $1` → `send_toggle`'s input.
     Sets `@active`; Max greys when `0`, opaque when `1`.
   - `dialset` → `[route attack decay sustain release]` → respective
     `live.dial`s' (`R06_V0`, `R07_V0`, `R08_V0`, `R09_V0`) inputs. Each
     dial's existing `attr_msg` chain emits `r{N}_v0 $1` into `[ccomidi]`
     on receipt.

2. **Wire the Send Env toggle's int back to v8.** A new connection from
   `send_toggle` (or the same `r #0-sync` aware tap point used by
   `attr_msg`) → `prepend senvchanged` → `[v8 ccomidi_voices.js]`'s left
   inlet. This is the path that fires the OFF→restore behavior.

3. **Verify the existing dial value-scaling.** The current dials are
   `prange=[0, 127]` (line 481). The plan emits raw engine bytes
   (`0..255`, or `0..254` for DirectSound, or smaller caps for
   square/noise — see Outstanding). Either:
   - Add per-family scaling on the v8 side before emitting `dialset`
     (preferred — keeps the patcher dumb and reflects ccomidi's own
     control range), or
   - Add a `[scale]` between the `dialset` route outlet and each dial.

   Pick one, document it where the choice lives. Don't split the logic.

4. The old `Send Env` push-button (removed earlier) is **not** re-added.
   The toggle is structurally distinct.

## Outstanding implementation questions

To resolve while coding, not blocking:

- **Engine ADSR byte ranges per voice family.**
  - DirectSound: `0..254` (full-scale = `254`, not `255`).
  - Square / noise (incl. `_ALT` and Square 2 / Noise 2): smaller caps.
    Attack appears to top out at `7`; decay/sustain/release likely cap
    around `12` or `16`. Verify against the engine's voice parser and the
    XCMD encoding in `core/sender_core.cpp` before settling on a scale.
  - Programmable-wave / cry: assume DirectSound-like until verified.
  - The writer stays raw. The reader (v8 `dialset` emitter, or the
    patcher's `[scale]`) owns the per-family normalization onto the
    `0..127` `live.dial` range.
- **`set_slot_name` coverage in the engine's `vg_parser.c`.** If
  square/noise/keysplit slots get empty `voiceSampleNames[i]`, they're
  filtered out at `poryaaaa~.c:508` and never reach the bus. Fix is
  upstream and out of scope here, but flag if observed.
- **Per-voice envelope dial-fill direction vs. user-edit direction.**
  Confirm whether `SenderCore`'s XCMD encoding for ATTA/DECA/SUST/RELE
  already operates in `0..127`. If yes, the dial → XCMD path needs no
  additional scaling; only the read-side fill is scaled.

## Verification (current process)

1. Rebuild the external (via `npm run build:externals` or the package CMake
   Release target) so `poryaaaa~` emits the `envelope` field per slot.
2. `npm test && npm run check && npm run build` clean.
3. Hand-edit + Save `devices/ccomidi.amxd` in Max for any additional route /
   dialset wiring needed for the new messages. Run
   `python3 scripts/amxd_inspect.py devices/ccomidi.amxd validate` (and
   `cords --from-text v8` etc.).
4. In Live: test the selection / toggle / dial behavior exactly as described
   in the Live verification steps below. (Devices are hand-maintained; no
   regeneration step.)

## Live verification steps

In Live, on a track running `ccomidi` with `poryaaaa` on a sibling track:
- Click Reload. Pick an envelope-capable voice → ADSR dials snap to the
  scaled stock values. Send Env toggle is enabled but stays off. Flip
  it ON → ADSR XCMD rows emit on dial moves. Flip OFF → emission stops
  and the dials hold their last positions.
- Pick a keysplit voice → toggle forces off and greys. Dials retain
  their last values; moving them does not emit.
- Pick another envelope-capable voice → toggle un-greys, dials
  repopulate. If the toggle was already ON, the new envelope auto-emits
  via the dial set-fires-emit chain.
- With Send Env ON, drag ADSR dials to override a voice. Then turn
  Send Env OFF → `poryaaaa~` receives the slot's stock envelope values,
  and further dial moves are inert.

## Critical files (current)

- `source/audio/poryaaaa~/poryaaaa~.cpp` and the recorder/ support (status emission)
- `code-src/ccomidi_voices.ts` (slot parsing, selection, envelope messages to patcher)
- `code-src/poryaaaa-node/voicegroup-service.ts` + client (transport)
- `devices/ccomidi.amxd` (hand-edit the post-v8 route and envelope wiring)
- Tests under `code-src/test/` that cover voices / slots

(Old plan text for bus/service modules has been removed; the implementation
moved to the files above during voicegroup/Node work.)
