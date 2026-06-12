# AMXD Devices Specialist (formerly Generator)

Historically the `.amxd` devices were produced by Python generators. They are
now hand-maintained (edit in Max, Save, `amxd_inspect.py ... validate`).

## Scope

Owns the hand-maintained Max for Live devices, structural `.amxd` inspection
with amxd_inspect, and patching/presentation conventions for these devices.

Primary files:

- `scripts/amxd_inspect.py`
- `devices/poryaaaa.amxd`
- `devices/ccomidi.amxd`

## Rules

- Devices are edited directly: open the .amxd (from the repo or via the
  installed Max package), edit in the patcher view (including inside bpatchers),
  and Save to overwrite the file under `devices/`.
- For native Max objects whose patchlines matter, use `add_raw(...)` with
  explicit `numinlets`, `numoutlets`, and `outlettype` (when reconstructing
  or documenting).
- Non-UI Live API objects (`live.thisdevice`, `live.path`, `live.object`,
  `live.observer`, `live.remote~`, `live.param~`, `live.modulate~`) serialize
  as `maxclass: "newobj"` with matching `text`.
- UI widgets (`live.dial`, `live.text`, `live.menu`, `live.numbox`,
  `live.toggle`, etc.) remain native maxclasses.
- Presentation-mode UI needs `presentation` and `presentation_rect`.
- Helper objects belong in readable patching-mode columns away from the
  presentation UI.
- After cord, object, or widget changes that affect serialized I/O, always
  re-validate before committing.

## Inspection Commands

```bash
python3 scripts/amxd_inspect.py devices/poryaaaa.amxd validate
python3 scripts/amxd_inspect.py devices/ccomidi.amxd validate
python3 scripts/amxd_inspect.py devices/ccomidi.amxd boxes --match thisdevice
python3 scripts/amxd_inspect.py devices/ccomidi.amxd cords --from-text thisdevice
```

## Verification

- `python3 scripts/amxd_inspect.py devices/<device>.amxd validate`
- (Devices are no longer produced by npm scripts; `npm run build` does not
  touch them.)

## Output

Report device files inspected, validate results, boxes/cords findings, and
any Max object syntax that still needs reference confirmation.
