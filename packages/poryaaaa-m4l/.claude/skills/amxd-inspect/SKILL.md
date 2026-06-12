# .amxd Patcher Inspection

When you need to look inside a `.amxd` device (poryaaaa.amxd or ccomidi.amxd) after a hand-edit in Max — to verify a box's JSON shape, audit which cords source a given object, or chase down "patchcord source not found" warnings — use `scripts/amxd_inspect.py`.

## Trigger

- The user reports Max console warnings like `patchcord source not found`, `outlet out of range`, or anything implying the .amxd JSON has structural problems.
- You hand-edited a device in Max (open the .amxd, make changes, Save) and want to confirm boxes/cords/validate before committing.
- You're answering a "what does the patcher actually contain?" question (structural query on the hand-maintained device).

Skip when: a plain `rg` answers the question without needing the binary parse.

## Usage

```bash
# List boxes — id, maxclass, text. --match filters by substring (case-insensitive).
python3 scripts/amxd_inspect.py devices/ccomidi.amxd boxes
python3 scripts/amxd_inspect.py devices/ccomidi.amxd boxes --match thisdevice

# Full JSON for one box.
python3 scripts/amxd_inspect.py devices/ccomidi.amxd box obj-1

# Patchlines, optionally filtered. ID filters are exact; --from-text / --to-text
# are case-insensitive substrings against `maxclass + text`.
python3 scripts/amxd_inspect.py devices/ccomidi.amxd cords --from-text thisdevice
python3 scripts/amxd_inspect.py devices/ccomidi.amxd cords --to obj-116
python3 scripts/amxd_inspect.py devices/ccomidi.amxd cords --from-text umenu --to-text v8

# Walk every patchline; report dangling endpoints and out-of-range outlets/inlets.
python3 scripts/amxd_inspect.py devices/ccomidi.amxd validate

# Full parsed patcher JSON (rarely needed; prefer the targeted subcommands).
python3 scripts/amxd_inspect.py devices/ccomidi.amxd raw
```


## Common diagnostics

- **"patchcord source not found"** in Max console: run `validate`. If it reports `ok`, the patcher JSON is structurally clean and the warning means Max failed to *instantiate* a box at load. Inspect the suspect box with `box <id>` and compare its `maxclass` shape to a known-good device (`devices/poryaaaa.amxd`). Native Live objects (`live.thisdevice`, `live.banks`, `live.path`, `live.observer`) must be `maxclass: "newobj"` with `text: "<name>"`, not `maxclass: "<name>"`.
- **Outlet / inlet mismatch**: `validate` reports out-of-range outlet/inlet indices against each box's `numoutlets`/`numinlets`. Useful after editing `add_raw(numoutlets=N, ...)` calls.
- **"who's wired to X?"**: `cords --to-text X` (or `--from-text X`) — faster than reading the gen script and tracing variables.

## When extending the inspector

Stay focused on structural .amxd questions. Don't add commands that duplicate `rg`/`grep` on the source `.py` files, and don't add interactive output — the script is for one-shot tool calls in this agent loop.
