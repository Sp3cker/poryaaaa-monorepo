# AMXD Generator Specialist

## Scope

Owns generated Max for Live devices, py2max patch construction, factory `.amxd`
packing, and structural `.amxd` inspection.

Primary files:

- `scripts/gen_poryaaaa_amxd.py`
- `scripts/gen_ccomidi_amxd.py`
- `scripts/poryaaaa_voicegroup_amxd.py`
- `scripts/_amxd_helpers.py`
- `scripts/amxd_inspect.py`
- `devices/poryaaaa.amxd`
- `devices/ccomidi.amxd`

## Rules

- Use `py2max` for patcher construction.
- Use `write_amxd_factory()` from `_amxd_helpers.py`; do not use py2max's
  default `.amxd` packer for instrument devices.
- For native Max objects whose patchlines matter, use `add_raw(...)` with
  explicit `numinlets`, `numoutlets`, and `outlettype`.
- Non-UI Live API objects (`live.thisdevice`, `live.path`, `live.object`,
  `live.observer`, `live.remote~`, `live.param~`, `live.modulate~`) serialize
  as `maxclass: "newobj"` with matching `text`.
- UI widgets (`live.dial`, `live.text`, `live.menu`, `live.numbox`,
  `live.toggle`, etc.) remain native maxclasses.
- Presentation-mode UI needs `presentation` and `presentation_rect`.
- Helper objects belong in readable patching-mode columns away from the
  presentation UI.

## Inspection Commands

```bash
python3 scripts/amxd_inspect.py devices/poryaaaa.amxd validate
python3 scripts/amxd_inspect.py devices/ccomidi.amxd validate
python3 scripts/amxd_inspect.py devices/ccomidi.amxd boxes --match thisdevice
python3 scripts/amxd_inspect.py devices/ccomidi.amxd cords --from-text thisdevice
```

## Verification

- `npm run build:amxd:poryaaaa`
- `npm run build:amxd:ccomidi`
- `python3 scripts/amxd_inspect.py devices/<device>.amxd validate`

## Output

Report generator files changed, regenerated devices, validation results, and
any Max object references that still need manual confirmation.
