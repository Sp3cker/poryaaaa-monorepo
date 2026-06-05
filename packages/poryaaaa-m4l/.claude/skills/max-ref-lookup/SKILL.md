# Max Object Reference Lookup

Before referencing a Max/MSP or Max for Live object by name in a recommendation, patcher diff, or explanation, read its reference doc so the description, inlets/outlets, and message vocabulary are correct.

## Trigger

Any time you are about to mention a Max object the user might place in a patcher — examples: `[umenu]`, `[t]`, `[bang]`, `[r]`, `[s]`, `[receive]`, `[send]`, `[messnamed]`, `[dict]`, `[pattr]`, `[live.thisdevice]`, `[v8]`, `[js]`, `[route]`, `[live.banks]`, `[chucker~]`. Brackets in the source don't matter — the trigger is the name itself.

Skip when:
- The name appears only inside a quoted error message or pasted log, not as a recommendation.
- You're referring to a JS class or API (`Dict`, `Global`, `MaxobjListener`) — those live in `docs/Max9-JS-API-en.md`, not `docs/max-ref/`.
- The user is describing existing patcher contents and you're not making claims about them.

## Lookup procedure

1. Search `docs/max-ref/max/<name>.md` first (standard Max objects).
2. If not found, try `docs/max-ref/m4l/<name>.md` (Live-specific objects, prefixed `live.*`).
3. For Jitter graphics objects (`jit.gl.*`, `jit.matrix`, `jit.window`, `jit.world`, `jit.anim.*`, `jit.gen`, `jit.expr`, `jit.pix`), try `docs/max-ref/jitter/<name>.md`.
4. Visual reference: `docs/max-ref/screenshots/max/<name>.png` or `docs/max-ref/screenshots/m4l/<name>.png`.

For ambiguous short names (`t`, `r`, `s`, `b`), prefer the long form — `trigger`, `receive`, `send`, `bang` — since the doc files are named long-form when present.

```bash
ls docs/max-ref/max/<name>.md docs/max-ref/m4l/<name>.md docs/max-ref/jitter/<name>.md 2>/dev/null
```

The jitter set is curated — it covers the rendering pipeline (`jit.gl.*`), core matrix/window (`jit.matrix`, `jit.window`, `jit.world`), animation (`jit.anim.*`), and gen/expr/pix. Physics (`jit.phys.*`) and 2D format converters (`jit.argb2ayuv` etc.) are NOT extracted — pull from `/Applications/Max.app/Contents/Resources/C74/docs/refpages/jit-ref/<name>.maxref.xml` directly, or run `npx tsx code-src/tools/extract-max-help.ts jitter <name>` to add them.

Read the file with the Read tool; do not just glob.

## When the doc is missing

Several common objects (`t`, `r`, `s`, `bang`, `route`) have no file in `docs/max-ref/`. In that case:
- Say so explicitly: "no entry in `docs/max-ref/` for `[<name>]`, going from prior knowledge".
- Do not invent message vocabulary or inlet counts. Stick to behavior you can verify from `docs/Max9-JS-API-en.md`, `docs/max-gotchas/`, or the project's own patchers under `devices/` and `scripts/gen_*_amxd.py`.

## What to extract

From the `.md`, capture only what the current task needs:
- Inlets/outlets table when you're proposing patcher wiring.
- Messages list when you're recommending a specific message (e.g., `append`, `setsymbol`, `clear` for `[umenu]`).
- "Attributes" / "Arguments" when you're suggesting object-box arguments.

Don't paste the whole doc. Cite the file path so the user can open it.
