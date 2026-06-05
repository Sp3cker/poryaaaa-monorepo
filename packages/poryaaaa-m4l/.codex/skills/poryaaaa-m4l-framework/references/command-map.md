# Codex Workflow Map

These are not slash commands. Treat them as common user intents.

| User intent | Codex workflow |
| --- | --- |
| build everything | Load Verification, run `npm run build` if appropriate |
| check TypeScript | Load TypeScript V8 and/or Node transport, run `npm run check` |
| test transport or recorder | Load relevant TS specialist, run `npm test` or focused `node --test ...` when possible |
| rebuild poryaaaa device | Load AMXD generator, run `npm run build:amxd:poryaaaa`, validate `devices/poryaaaa.amxd` |
| rebuild ccomidi device | Load AMXD generator, run `npm run build:amxd:ccomidi`, validate `devices/ccomidi.amxd` |
| inspect an .amxd | Load AMXD generator, use `scripts/amxd_inspect.py` |
| fix Max object syntax | Load Max/M4L reference, inspect `docs/max-ref/` and `docs/max-gotchas.md` |
| work across domains | Load Router, spawn subagents when domains are independent |
| review current work | Load Verification and Critic protocol; lead with findings |
