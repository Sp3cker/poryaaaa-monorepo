# poryaaaa Max for Live

The Max for Live context owns Ableton-set-scoped device state, patcher UI/control routing, Node-for-Max sidecars, and WebSocket voicegroup transport for poryaaaa and ccomidi devices.

## Language

**Live-set voicegroup selection**:
The project root and voicegroup bank selected by a poryaaaa Max for Live device for one Ableton Live set. This selection is per-device/per-set state restored from device-saved state, not a global last-used project root.
_Avoid_: Global project selection, shared project root, app-wide voicegroup selection

**Project root**:
The filesystem root of the poryaaaa project used by a device in the current Live set.
_Avoid_: Workspace, global root

**Voicegroup bank**:
The selected `.inc` voicegroup bank name within the current project root.
_Avoid_: Preset, patch
