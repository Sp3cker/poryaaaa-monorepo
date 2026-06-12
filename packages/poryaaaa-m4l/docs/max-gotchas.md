# Max / M4L gotchas

Things that tripped me up while building `ccomidi.amxd` and `poryaaaa~.amxd`. Each section is a real bug I caused, the symptom, and the fix.

---

## `pack` / `pak` argument syntax

**Bug:** `[pak s s]` was emitting the literal symbols `s s` instead of values pushed in via inlets.

**Cause:** Single-letter `s` is parsed by `pack`/`pak` as a **literal default symbol** named `"s"`, not as a type indicator.

**Fix:** Use the full word `symbol`. The literal-vs-type-indicator rules:

| Argument   | Interpretation                    |
| ---------- | --------------------------------- |
| `0`        | int slot, default 0               |
| `0.0`      | float slot, default 0.0           |
| `symbol`   | symbol slot, empty default        |
| `s`        | **symbol slot, default `"s"`** ← gotcha |
| `apple`    | symbol slot, default `"apple"`    |

**`pack` vs `pak`:**

- `pack` — only inlet 0 triggers output. Other inlets are sticky-only.
- `pak`  — every inlet triggers output.

For "type into two fields, fire once when the second is committed," `pack symbol symbol` with the trigger field on inlet 0 is what you want. With `pak`, typing in the sticky field would also fire.

---

## `textedit` output format

**Bug:** A `[textedit] → [pack symbol symbol]` chain was passing `"text"` as the slot value, ignoring what the user typed.

**Cause:** `[textedit]`'s left outlet emits a list with the literal symbol `text` as the message selector, followed by the typed content as one or more atoms (split on whitespace). When this hits a generic inlet, only the first atom (`text`) gets stored.

**Fix:** Strip the prefix with `[route text]` and recombine multi-atom output with `[tosymbol]`:

```
[textedit] → [route text] → [tosymbol] → [pack ...]
```

`[route text]` outlet 0 emits everything after the `text` selector. `[tosymbol]` joins the resulting atoms back into a single symbol so paths with spaces survive intact.

---

## Reordering pack output via `[message]` substitution

**Bug:** Pack outputs `[<slot 0>, <slot 1>]` in slot order, which conflicted with the order the downstream message handler expected.

**Why this matters:** `pack`'s slot 0 is the **trigger** inlet. If UX requires the second-typed field to fire the action, that field has to live in slot 0 → it appears first in the output list. But the receiving method might want the args in the other order.

**Fix:** Reorder in a downstream `[message]` box with `$N` substitution:

```python
# pack outputs [<name>, <root>]   (name in slot 0, the trigger)
# we want to emit "voicegroup <root> <name>"
[message: voicegroup $2 $1]
```

`$1` references the first arg of the incoming list, `$2` the second. Cheaper than swapping inlets and changing UX semantics.

---

## `live.observer` syntax

**Bug:** `[live.observer @path live_set @property is_playing]` never fired. In at least one generated `.amxd`, `[live.observer live_set is_playing]` also produced no output on Live transport changes.

**Cause:** `@path` / `@property` aren't honored as box attributes (or they're version-specific, or only valid via `attrname value` messages, etc.). The positional path/property form can also fail to bind reliably at device load.

**Fix:** Bind explicitly through `[live.path]`, and send the path after `[live.thisdevice]`'s left outlet fires:

**Output format:** `live.observer <property>` emits the **bare value** when the property changes. No `<property> <value>` prefix. So no `[route]` needed downstream:

```
[live.thisdevice] left outlet → [path live_set] → [live.path] → [live.observer is_playing] → [prepend transport] → [<your object>]
```

For 0→1 edge detection, route via `[sel 1]` which fires bang only when the input is `1`. (In our case we delegated edge detection to the receiving wrapper instead.)

---

## `.amxd` binary file format

A `.amxd` is a `.maxpat` (JSON) wrapped in a small chunked binary header. Hand-rolled this in Python; verified against factory devices.

```
0x00  "ampf"                4 bytes — magic
0x04  04 00 00 00           u32 LE — length of next chunk's content
0x08  4-byte device kind:
        "aaaa"  Audio Effect
        "mmmm"  MIDI Effect
        "iiii"  Instrument
0x0C  "meta"                4 bytes — chunk type
0x10  04 00 00 00           u32 LE — meta content length (always 4)
0x14  4 bytes meta value:
        01 00 00 00 = 1     Raw JSON payload (what we generate)
        07 00 00 00 = 7     Compressed/packed (factory devices, mx@c chunk)
0x18  "ptch"                4 bytes — chunk type
0x1C  4 bytes u32 LE        Length of patcher content following
0x20+ patcher content       JSON bytes (or compressed mx@c-prefixed data)
```

For the committed devices, meta value `1` + raw JSON is the layout used. The JSON itself is a standard `.maxpat` document: `{"patcher": {"boxes": [...], "lines": [...], ...}}`.

**Indentation:** Max-saved files use tab indentation. `json.dumps(obj, indent="\t")` matches.

**py2max binary packer doesn't work for instruments.** py2max's `pack_amxd` (also invoked by `Patcher.save_as` when the suffix is `.amxd`) wraps the JSON in an `mx@c` sub-block instead of the simple `meta` chunk shown above. Max accepts that envelope for `mmmm` (midi_effect) and `aaaa` (audio_effect) devices, but **rejects it for `iiii` (instrument)** with the cryptic error `parsing object, possible missing initial '{' character: line=1, char=2, text='...m'`. That error means Max gave up on the binary parse and read the file as raw JSON, hitting the `m` of `ampf` at byte 1 (1-indexed).

Historically the (now-removed) generators used a custom factory packer to emit the correct layout. Current devices are saved directly from Max, which produces the right ampf+meta+ptch+JSON container for instruments.

---

## Patching mode vs. presentation mode

Live's M4L device strip displays **only presentation mode**. The chaotic patching-mode view that shows up when you double-click into the patcher is irrelevant to end-users.

For each box that should appear in Live's UI, set:

```json
"presentation": 1,
"presentation_rect": [x, y, w, h]
```

Helper objects (message boxes, `[print]` taps, etc.) only need `patching_rect`. Stack them off-screen in a tidy column at high `x` coords (e.g. `x=1000+`) so patching mode stays readable too.

---

## `t_pxobject` with zero audio inlets (synth pattern)

For an audio-output-only external (e.g., a synth driven by MIDI messages):

```c
dsp_setup((t_pxobject *)x, 0);   // 0 audio inlets — leftmost is a message inlet
outlet_new(x, "signal");          // signal outlets get registered in REVERSE
outlet_new(x, "signal");          // so 1st call → outlet 1 (right), 2nd → outlet 0 (left)
```

Important consequences:

1. **Methods registered with `class_addmethod` work normally** — `int`, `list`, custom selectors all dispatch to user methods on the leftmost inlet.
2. **`[midiin]` outputs raw MIDI bytes as ints**, one per outlet emission. Connect `[midiin]` directly to the wrapper's leftmost inlet and dispatch via the `int` method — no `[midiparse]` in the chain (which drops sysex/XCMD bytes).
3. **Outlet ordering is reversed.** First `outlet_new` becomes the rightmost outlet (highest index). Verify channel mapping for stereo output: `outs[0]` is the LEFT signal if the LAST `outlet_new` was for left.

---

## MIDI in M4L: byte-stream + running status

`[midiin]` outputs each MIDI byte as a separate `int`. To assemble events you implement a running-status state machine inside the receiving external. Key points:

- **Status byte (`b & 0x80`):** store as current status, reset data-byte counter, set `bytesNeeded` to 1 (`0xCx`/`0xDx` = PC/ChanPress) or 2 (everything else).
- **Data byte (`b < 0x80`):** append to buffer; when count == bytesNeeded, dispatch and reset counter (status persists for running-status repeats).
- **System realtime (`b >= 0xF8`):** swallow without disturbing running status.
- **System common (`b >= 0xF0`):** clears running status. SysEx (`0xF0`) opens a swallow-until-`0xF7` state.
- **Orphan data byte (no preceding status):** drop silently.
- **Mid-message status byte:** abandon the partial message; the new status starts fresh.

XCMD-style bytes ride on regular CCs (e.g., `0xBn` with CC# 29/30/31 in the m4a engine), so they survive the parser unchanged — provided no `[midiparse]` is upstream stripping them.

---

## Live transport detection

To re-emit state on every Play-press:

```
[live.observer live_set is_playing]   ─→ <bare 0/1 on transport state change>
        │
        ↓
[prepend transport]
        ↓
[<your object that has a `transport` method that edge-detects internally>]
```

For receiving-side edge detection, the wrapper tracks the previous state and only fires on `0→1` transitions. Using `[sel 1]` upstream works too but moves the state into the patch.

**Initial sendall on device load:** wire `[live.thisdevice]` left outlet → `[delay 500]` → `[message sendall]` → `<your object>`. The left outlet fires when the Max Device is initialized; the 500ms delay buys time for downstream devices on other tracks to also finish loading.

---

## Live API LiveSet paths used so far

- `live_set` is the root LiveSet object.
- `live_set is_playing` — bool, true while transport is playing.
- `live_set tempo` — float, current tempo in BPM.

Other paths follow the standard LOM tree (`live_set tracks N`, `live_set tracks N devices M`, etc.).

---

## Order of execution: `[trigger]` outlets fire RIGHT-to-LEFT

`[t b b]` has 2 outlets. **Outlet 1 (right) fires first, outlet 0 (left) fires second.** This is the only way Max enforces deterministic execution order between concurrent banged patchcords.

If you need to update a sticky pack slot before triggering pack on inlet 0:

```
[Load button] → [t b b]
                ├─ outlet 1 (fires first) → bang the textedit feeding the sticky slot
                └─ outlet 0 (fires second) → bang the textedit feeding the trigger slot
```

---

## v8 `Folder` is not the right tool for arbitrary user paths in M4L

**Bug:** `[v8]` reading a user-picked decomp directory via the JS `Folder` API returned zero entries even though the path existed and was full of files.

**What I tried, none of which worked reliably:**

- HFS form (`Macintosh HD:/Users/.../voicegroups`) — Folder appears to choke on the space in the volume name.
- POSIX form (`/Users/.../voicegroups`) — empty enumeration; M4L sandboxing may be blocking reads outside Max-known directories.
- `dir.typelist = []; dir.reset()` — docs say `[]` defaults to "all files" but in practice setting + resetting after construction didn't change behaviour.

**Fix:** Use Node for Max (`[node.script]`) and require `node:fs` directly. Node's `fs.readdirSync` / `fs.readFileSync` have no such restrictions, and the script can be unit-tested with a mock `fs` against `node --test`. The voicegroup implementation lives in `code-src/poryaaaa_voicegroup_server.ts` and `code-src/poryaaaa-node/`.

**Bridging note:** `[node.script]` has a single outlet. To fan out to multiple downstream destinations, prefix every `maxApi.outlet(...)` with a routing tag (e.g. `outlet("bank", "append", name)`) and use `[route bank preset]` in the patcher.

**Boot-timing gotcha:** by default `[node.script]` does **not** start the Node process until it receives `script start`. Send anything else first and Max prints `node.script: Node script not ready can't handle message <selector>` and **drops the message**. Two fixes, applied together for safety:

1. Declare the box as `[node.script foo.js @autostart 1]` so it boots when the patch loads.
2. Wire `[live.thisdevice]` left outlet → `[script start]` → node.script. Idempotent if the script is already running, and survives the case where `@autostart` is overridden or the patch is reloaded with the script crashed.

Even with autostart, Node takes a moment to boot. If your UX lets the user trigger work within the first ~500ms of patch load, gate it through a `[delay 500]` after `[live.thisdevice]`'s left outlet — same pattern as the device-load `sendall`.

---

## `plugout~ Out~ N` vs `plugout~ N` (wrong syntax = mono left)

**Bug:** `[poryaaaa~]` was outputting only on the left channel even though the wrapper's perform routine clearly writes distinct samples to `outs[0]` and `outs[1]`. Right channel: silent.

**Cause:** I had wired the audio outs as `[plugout~ Out~ 1]` and `[plugout~ Out~ 2]`. The `Out~` token is `vst~`-host plugin syntax — when you load a VST plugin with `[vst~]`, the hosted plugin can have *named* audio outlets like `Out~`, and `[plugout~ Out~ 1]` says "the channel-1 output of the hosted plugin's `Out~` outlet." This has **nothing to do with M4L instrument outputs**. Max parsed the bad args as garbage; either both plugouts ended up on channel 1 (so L+R got mixed into the left output) or the right plugout silently failed to bind. Both produce the same symptom: audio only in left.

**Fix:** Use the bare numeric form for M4L instrument outputs:

```
[plugout~ 1]    ; left
[plugout~ 2]    ; right
```

Diagnostic if you see this again: swap the `outlet=0`/`outlet=1` wires to the two plugouts. If audio jumps from left to right, the plugouts are fine and the wrapper's perform routine is mono. If audio stays on left regardless, the right plugout box is broken — check its text.

---

## `live.indicator` doesn't exist

**Bug:** Used `[live.indicator]` for 16 channel-activity LEDs. Max printed `live.indicator: no such object` 16 times on patch load and the boxes rendered as the broken-object red box.

**Cause:** There is no `live.*` LED widget. The `live.*` family is `dial / numbox / toggle / text / tab / menu / grid / button / gain~ / meter~ / scope~ / thisdevice / observer / path / object / remote~ / colors`. No indicator.

**Fix:** Plain Max `[button]` — a small filled circle with a built-in flash animation when banged. Works in M4L presentation mode. Re-bang during the flash retriggers cleanly. `[button]` also re-emits the bang from its outlet; leave it unwired.

`[led]` (also a regular Max object) is an alternative if you want explicit on/off rather than auto-flash, but you have to schedule the off via `[pipe N]`, and re-triggering during a pending pipe gets fiddly.

---

## Engine-side gotcha: silent note drops

Distinct from Max issues but worth recording: `m4a_engine_note_on` will silently no-op if `resolve_voice` returns NULL. The most common cause is a `voice_keysplit_all` (drumset) program where the played key isn't in the drum-key-map. Symptoms: dispatcher logs the note, but no audible sound.

**Diagnostic:** if test-note works on key 60 but melodic clip notes on B4/A4/E5 don't, check the voicegroup's program 0 — it's likely a `keysplit_all` drumset. Send a Program Change to a melodic program (`voice_keysplit`, `voice_square_*`, `voice_directsound`) before playing.

---

## Device patcher metadata and validation (historical note)

Historically the `.amxd` devices were produced by Python generators using py2max. They are now hand-maintained: edit directly in Max and Save over the file in `devices/`, then run

```
python3 scripts/amxd_inspect.py devices/<name>.amxd validate
```

(and `boxes` / `cords` queries as needed) before committing the binary.

When editing, make sure serialized inlet/outlet counts on objects like `[route ...]` or `[sel ...]` (and comments that receive messages) match the patchcords you add. Use `amxd_inspect.py ... cords` + `validate` after changes; dangling or out-of-range patchlines are reported automatically. This is the current process (no generators or "regens").

The py2max-era bug (incomplete `numinlets`/`numoutlets`/`outlettype` metadata causing Max to silently drop patchcords on load) is the reason we validate after every structural edit. Helper objects should still live in a far-right column in patching view.

Current checklist items that still apply:
- All `live.*` widgets shown in Live have `presentation` + `presentation_rect`.
- `[live.thisdevice]` left outlet typically drives instance-local sync (e.g. `s #0-sync` / `r #0-sync` fanout for widgets).
- Test in a fresh Live project (Live caches device state at load time).
