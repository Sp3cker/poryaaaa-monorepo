  # Py2Max Patch Layout
The patching mode view should be well organized, laying out controls in logical units.
  When editing `scripts/gen_*_amxd.py`, treat patching-mode layout as part of the deliverable.

  Generated Max patchers should be organized by signal/control flow, not just by creation order.

  For each generated patch:
  - Group related objects into clear blocks: boot/state, UI input, routing, engine/audio/MIDI, monitoring/debug.
  - Place routing hubs near the center of the flow.
  - Keep persistence/state objects together.
  - Keep monitoring-only branches visually separate from the main control/audio path.
  - Prefer short, mostly direct patch cords. Avoid long crossing wires where moving objects would make the graph clearer.
  - Add small patching-mode comments as section labels for major blocks.
  - After regenerating, run `scripts/.venv/bin/python scripts/amxd_inspect.py <device> validate`.