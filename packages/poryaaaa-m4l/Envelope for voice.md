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

- State transport is bus-based: `poryaaaa~` emits `status <uri-encoded-json>`
  on its rightmost outlet → `code-src/service.ts` decodes → publishes on
  `messnamed("---poryaaaa.state", "state", encoded)` and writes a `Dict`
  snapshot named `poryaaaa.state`. ccomidi instances subscribe and pull the
  snapshot at start. There is **no** `state.json` and no
  `voicegroup_state.c` writer.
- Slot payload today: `{program: int, name: string}`. Both `parseSlotsField`
  in `code-src/service.ts:63` and `parseSlots` in
  `code-src/ccomidi_voices_service.ts:53` drop everything else.
- Patcher prep work for the envelope UI is already in place
  (`scripts/gen_ccomidi_amxd.py:413-510`):
  - Four `live.dial`s (Attack/Decay/Sustain/Release) wired to rows `r5..r8`
    with `r{N}_v0 $1` into `[ccomidi]`.
  - `Send Env` `live.toggle` (persisted Live param) drives `r5_en..r8_en`
    enable bits via `attr_msg`, with `r #0-sync` re-bang on row 5.
- The `[route]` after `[v8 ccomidi_voices.js]` is still `slots program` and
  the toggle has no path back into v8.

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

## Bus relay — `code-src/service.ts`

`parseSlotsField` (lines 63-75) currently coerces to `{program, name}` and
discards the rest. Extend the `Slot` interface and parser to preserve
envelope:

```ts
type Envelope = { attack: number; decay: number; sustain: number; release: number } | null;
interface Slot { program: number; name: string; envelope: Envelope; }
```

Validation:

- `envelope === null` → keep as `null`.
- `envelope` is a four-key object with finite numeric `attack`, `decay`,
  `sustain`, `release` → keep, coerce each to `int`.
- Otherwise → coerce to `null` and `deps.post()` a diagnostic naming the
  slot. Don't drop the slot.

`StateBroadcast.slots` is `Slot[]` so the change is transparent to the
broadcast/snapshot path; the encoded JSON automatically includes the new
field.

## ccomidi service — `code-src/ccomidi_voices_service.ts`

Mirror the slot/envelope types from `service.ts` (or pull them through a
shared file in `code-src/` if the duplication starts mattering — not
required for this change).

`parseSlots` (lines 53-65): same validation as above; treat malformed
envelopes as `null` with a `post()`.

`select(idx)` (currently emits only `program <p>` at line 170): after the
`program` emit, fan out envelope info as new outlet messages so the
patcher's expanded `[route]` can drive the toggle / dials directly.

- `slots[idx].envelope === null` (keysplit):
  - `senvactive 0` — set the toggle's `@active` attribute, greying it.
  - `senvtoggle 0` — force the toggle's value off.
- Else:
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

## Patcher — `scripts/gen_ccomidi_amxd.py`

Build on the existing envelope section (`build_envelope_section`,
`gen_ccomidi_amxd.py:413-510`):

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

## File changes summary

Engine repo (location TBD — not under `/Users/sallegrezza/dev/`; locate
before editing):
- **Edit** `voicegroup/voicegroup_types.h` — only to confirm voice-type
  codes; no change unless missing.

This repo (`/Users/sallegrezza/dev/cProjects/poryaaaa-m4l/`):
- **Edit** `source/audio/poryaaaa~/poryaaaa~.c` — `porya_emit_status` adds
  `envelope` per slot; bump JSON buffer; add
  `voice_type_supports_envelope`.
- **Edit** `code-src/service.ts` — `Slot.envelope`; `parseSlotsField`
  preserves and validates it.
- **Edit** `code-src/ccomidi_voices_service.ts` — `Slot.envelope`;
  `parseSlots`; `select` fan-out; new `senvchanged` method; track selected
  index.
- **Edit** `code-src/ccomidi_voices.ts` — wire the `senvchanged` inbound
  handler; expose it on the v8 entry.
- **Edit** `code-src/test/ccomidi_voices.test.ts` — see Tests section.
- **Edit** `scripts/gen_ccomidi_amxd.py` — expand `[route]`; add
  `senvchanged` cord from toggle into v8; resolve scaling.
- (Possibly) **Edit** `code-src/test/service.test.ts` — if
  `parseSlotsField` validation gets a dedicated test.

## Verification

1. Engine rebuild emits `envelope` per slot (sample / square / noise /
   programmable-wave / cry → object with four ints; keysplit / keysplit-all
   → JSON `null`). Confirm by feeding the JSON status to the v8 console or
   inspecting the snapshot Dict in real time.
2. `npm test && npm run check && npm run build` clean in this repo.
3. Rebuild `poryaaaa~` external (CMake `Release`). Regenerate
   `devices/ccomidi.amxd`.
   `python3 scripts/amxd_inspect.py devices/ccomidi.amxd validate` returns
   ok. `cords --from-text v8` shows the new `senvtoggle / senvactive /
   dialset` outlets.
4. In Live, on a track running `ccomidi` with `poryaaaa` on a sibling
   track:
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

## Critical files

- `source/audio/poryaaaa~/poryaaaa~.c` — `porya_emit_status` (the JSON
  emitter)
- `code-src/service.ts` — `parseSlotsField`, `StateBroadcast`
- `code-src/ccomidi_voices_service.ts` — `parseSlots`, `select`,
  new `senvchanged`
- `code-src/test/_mocks.ts` — `MockBus` (sufficient as-is)
- `scripts/gen_ccomidi_amxd.py` — `build_envelope_section`, the post-v8
  `[route]`
