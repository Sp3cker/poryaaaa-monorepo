# Codex Workflow Map

These are not slash commands. Treat them as common user intents.

| User intent | Codex workflow |
| --- | --- |
| build everything | Load Verification, run `npm run build` if appropriate |
| check TypeScript | Load TypeScript V8 and/or Node transport, run `npm run check` |
| test transport or recorder | Load relevant TS specialist, run `npm test` or focused `node --test ...` when possible |
| edit poryaaaa device | Open `devices/poryaaaa.amxd` in Max, edit + Save, then `python3 scripts/amxd_inspect.py devices/poryaaaa.amxd validate` (AMXD devices) |
| edit ccomidi device | Open `devices/ccomidi.amxd` in Max, edit + Save, then `python3 scripts/amxd_inspect.py devices/ccomidi.amxd validate` (AMXD devices) |
| inspect an .amxd | Load AMXD devices, use `scripts/amxd_inspect.py` |
| fix Max object syntax | Load Max/M4L reference, inspect `docs/max-ref/` and `docs/max-gotchas.md` |
| work across domains | Load Router, spawn subagents when domains are independent |
| review current work | Load Verification and Critic protocol; lead with findings |
