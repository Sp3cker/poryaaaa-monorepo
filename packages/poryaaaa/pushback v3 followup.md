# Pushback v3 follow-up: ingress wired

In response to the expert's review of the scaffold landing.

> The v2 render path is wired, but the v2 driver is not the recipient
> of events/config yet.

Accepted; option (a) chosen — v2-side dispatch added now rather than
deferred to Layer 1.  The driver stubs already covered most of the
ingress, so this was a small, mechanical change that finishes the
scaffold rather than letting it carry an unwired-ingress liability into
Layer 1.

---

## What landed

### 1. M4ADriver API extended with the v1 ingress surface

`plugin/m4a/m4a_driver.h` now declares stubs for everything the v1
engine takes:

```c
void m4a_driver_set_xcmd_callback(M4ADriver*, M4ADriverXcmdFn, void *ctx);
void m4a_driver_set_voicegroup(M4ADriver*, ToneData *vg);
void m4a_driver_refresh_voices(M4ADriver*);
void m4a_note_on(M4ADriver*, int track, uint8_t key, uint8_t velocity);
void m4a_note_off(M4ADriver*, int track, uint8_t key);
void m4a_cc(M4ADriver*, int track, uint8_t cc, uint8_t value);
void m4a_pitch_bend(M4ADriver*, int track, int16_t bend);   /* matches v1's int16_t */
void m4a_program_change(M4ADriver*, int track, uint8_t program);
void m4a_all_notes_off(M4ADriver*, int track);
void m4a_all_sound_off(M4ADriver*);
void m4a_set_song_volume(M4ADriver*, uint8_t volume);
void m4a_set_tempo_bpm(M4ADriver*, double bpm);
```

All scaffold-stage no-ops; the call signatures are deliberately identical
to the v1 surface so each ingress site is a verbatim mirror.

`M4ADriverXcmdFn` is a sibling typedef of `M4AEngineXcmdFn`
(`void(void*, int, uint8_t, uint32_t)`) so `plugin_engine_xcmd` casts
into both without dragging `m4a_engine.h` into `m4a_driver.h`.

### 2. Every v1 ingress call site mirrored

Pattern at each site:

```c
m4a_engine_note_on(&data->engine, channel, msg[1], msg[2]);
m4a_note_on(data->m4a_v2, channel, msg[1], msg[2]);
```

Counts:

| File | v1 ingress calls | v2 mirror calls |
|---|---|---|
| `plugin/m4a_plugin.c` | 17 | 17 |
| `cmd/poryaaaa_render.c` | 9 | 9 |
| `test/test_engine.c` | 4 | 4 |
| `test/test_wav_export.c` | 17 | 17 |
| **total** | **47** | **47** |

In the plugin, `M4ADriver*` lives per-instance in `M4APluginData`
created in `plugin_activate`, matching the existing `M4AEngine`
lifecycle.  In `poryaaaa_render` and `test_wav_export.c`, the driver
is a file-scope static lazily initialised at the first ingress.

### 3. Lifecycle gates split: driver vs chip

Historical scaffold note: at this point in the rewrite, the driver and chip
could still be toggled separately to isolate responsibilities.  That was useful
while validating Layer 1 driver ingress and Layer 2 chip rendering, but it is no
longer the current product shape.

Current cutover state: `M4ADriver` and `HwAudio` are both linked and owned
unconditionally, and render call sites use the v2 event-stream path directly.

If driver-only or chip-only build coverage is needed again, it should be added
as an explicit comparison harness rather than a product-wide feature flag.

### 4. Plan §9 updated

Plan now documents:
- the per-flag lifecycle gates;
- the ingress-mirroring pattern with the example code;
- the per-instance `M4ADriver*` ownership in `M4APluginData`.

---

## Verification rerun

```
build/poryaaaa_unit_tests             (default)        86/86 pass

build/poryaaaa_render … --output /tmp/v2_silent2.wav
  → 655360 data bytes, 0 non-zero samples
```

CLAP plugin built clean under the historical scaffold configurations.

---

## Bottom line

| Item | Status |
|---|---|
| 1 — driver-side ingress surface | M4ADriver API extended (12 stubs added) |
| 2 — every v1 ingress mirrored into v2 | 47 sites across 4 files |
| 3 — driver/chip lifecycle gates split | Both `OFF`/`OFF`, `ON`/`OFF`, `OFF`/`ON`, `ON`/`ON` build & run |
| 4 — plan §9 documents new pattern | Updated |

Layer 1 can now land into `plugin/m4a/` and immediately be exercised
through the existing entry points (CLAP, render CLI, test harnesses)
with the v2 driver enabled unconditionally.  No further ingress plumbing required at the
boundary.

Holler if anything else looks off.
