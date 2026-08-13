# Port poryaaaa from CLAP plugin to Max for Live device

## Context

`poryaaaa` is a C/C++ plugin library that emulates the GameBoy Advance `m4a` sound driver (reimplemented from pokeemerald, **not** linked to mGBA). It generates audio from MIDI input and is built into `poryaaaa.clap`.

You want to consume the same driver inside Ableton Live as a Max for Live device. The work is feasible because the DSP engine is cleanly separated into `M4ADriver` (`plugin/m4a/m4a_driver.h`) and `HwAudio` (`plugin/hw_audio/hw_audio.h`), taking MIDI driver inputs and producing stereo audio blocks via hardware emulation. We will replace the CLAP layer with a Max MSP external (`poryaaaa~.mxo`), wrap it in an `.amxd` device patch, and drop the ImGui GUI in favour of Max patcher widgets.

Scope decisions: **drop ImGui** (use live.dial / live.menu in the patch instead), **macOS only** (universal .mxo).

## Target layout

New external lives at:
```
/Users/sallegrezza/dev/cProjects/max-sdk/source/audio/poryaaaa~/
├── poryaaaa~.c           # the only new source file — MSP wrapper
├── CMakeLists.txt        # pulls driver/hw_audio sources by relative path; no copying
└── poryaaaa~.maxhelp     # optional
```

Driver sources are referenced in-place from `/Users/sallegrezza/dev/cProjects/poryaaaa/plugin/`.

## Critical files

**Reference (read-only, do not edit):**
- `/Users/sallegrezza/dev/cProjects/poryaaaa/plugin/m4a/m4a_driver.h` — driver public API surface
- `/Users/sallegrezza/dev/cProjects/poryaaaa/plugin/hw_audio/hw_audio.h` — hardware audio synthesis & rendering surface
- `/Users/sallegrezza/dev/cProjects/poryaaaa/plugin/voicegroup/voicegroup_loader.h` — `voicegroup_load` / `voicegroup_free`
- `/Users/sallegrezza/dev/cProjects/max-sdk/source/audio/simplemsp~/simplemsp~.c` — canonical MSP external skeleton
- `/Users/sallegrezza/dev/cProjects/max-sdk/source/audio/simplemsp~/CMakeLists.txt` — canonical CMake pattern

**To create:**
- `/Users/sallegrezza/dev/cProjects/max-sdk/source/audio/poryaaaa~/poryaaaa~.c`
- `/Users/sallegrezza/dev/cProjects/max-sdk/source/audio/poryaaaa~/CMakeLists.txt`

## Step 1 — Initialise the Max SDK base submodule [done]

The directory `/Users/sallegrezza/dev/cProjects/max-sdk/source/max-sdk-base/` is set up. Run:
```bash
git -C /Users/sallegrezza/dev/cProjects/max-sdk submodule update --init --recursive
```

## Step 2 — `CMakeLists.txt` for `poryaaaa~` [done]

Pulls driver, hw_audio, and voicegroup sources from the poryaaaa tree without copying. **Excludes** CLAP integration (`m4a_plugin.c`, `m4a_params.c`, `m4a_gui.cpp`).

```cmake
include(${CMAKE_CURRENT_SOURCE_DIR}/../../max-sdk-base/script/max-pretarget.cmake)

set(PORYA_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../poryaaaa")

include_directories(
    "${MAX_SDK_INCLUDES}"
    "${MAX_SDK_MSP_INCLUDES}"
    "${PORYA_ROOT}/plugin"
    "${PORYA_ROOT}/plugin/m4a"
    "${PORYA_ROOT}/plugin/hw_audio"
    "${PORYA_ROOT}/plugin/voicegroup"
)

file(GLOB LOCAL_SRC "*.c" "*.h")
file(GLOB M4A_SRC "${PORYA_ROOT}/plugin/m4a/*.c")
file(GLOB HW_SRC "${PORYA_ROOT}/plugin/hw_audio/*.c")
file(GLOB VG_SRC "${PORYA_ROOT}/plugin/voicegroup/*.c")

add_library(${PROJECT_NAME} MODULE ${LOCAL_SRC} ${M4A_SRC} ${HW_SRC} ${VG_SRC})
set_target_properties(${PROJECT_NAME} PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../max-sdk-base/script/max-posttarget.cmake)
```

C11 is safer than C99 — the driver and voicegroup loader use modern C features.

## Step 3 — `poryaaaa~.c` skeleton [done]

```c
#include "ext.h"
#include "ext_obex.h"
#include "z_dsp.h"
#include "m4a/m4a_driver.h"
#include "hw_audio/hw_audio.h"
#include "voicegroup/voicegroup_loader.h"

typedef struct _porya {
    t_pxobject ob;             // MUST be first
    M4ADriver *driver;
    HwAudio *hwAudio;
    LoadedVoiceGroup *loadedVg;
    float *scratchL, *scratchR;
    long scratchFrames;
    double samplerate;
    // ATTR_SAVE state
    t_symbol *vgRoot;
    t_symbol *vgName;
    long progSlot[16];
    long songVolume;
    long reverbAmount;
    long maxPcmChannels;
} t_porya;
```

`ext_main` registers the class and these methods:

```c
class_addmethod(c, (method)porya_dsp64,     "dsp64",      A_CANT, 0);
class_addmethod(c, (method)porya_assist,    "assist",     A_CANT, 0);
class_addmethod(c, (method)porya_int,       "int",        A_LONG, 0);   // raw MIDI byte from [midiin]
class_addmethod(c, (method)porya_midievent, "midievent",  A_GIMME, 0);  // 3-byte event from a message box
class_addmethod(c, (method)porya_program,   "program",    A_LONG, A_LONG, 0);
class_addmethod(c, (method)porya_voicegroup,"voicegroup", A_SYM, A_SYM, 0);
class_addmethod(c, (method)porya_tempo,     "tempo",      A_FLOAT, 0);
class_addmethod(c, (method)porya_panic,     "panic",      0);
```

**MIDI ingress is raw bytes only.** `[midiin]` emits one int per MIDI byte; `porya_int` runs a small running-status state machine and assembles channel-voice events, which a shared `porya_dispatch_event(status, d1, d2)` helper routes into the driver (`m4a_driver_note_on`, `m4a_driver_note_off`, `m4a_driver_cc` including XCMD on 29/30/31, `m4a_driver_program_change`, `m4a_driver_pitch_bend`). No `[midiparse]` in the chain — keeps XCMD bytes intact and removes Max-side parsing as a failure surface. SysEx and system-realtime are swallowed. Channel comes from the status nibble, so multiple ccomidi tracks routed into one `poryaaaa~` address its 16 m4a tracks naturally.

Lifecycle:
- `porya_new(s, argc, argv)` — `dsp_setup(x, 0)` (synth: zero audio inlets); `outlet_new(x,"signal")` ×2 stereo; `x->driver = m4a_driver_create()`; `x->hwAudio = hw_audio_create(sys_getsr())`.
- `porya_free(x)` — `m4a_driver_destroy(x->driver)`, `hw_audio_destroy(x->hwAudio)`, `voicegroup_free(loadedVg)`, `sysmem_freeptr` scratch buffers, `dsp_free`.
- `porya_dsp64(x, dsp64, count, sr, maxvec, flags)` — reallocate scratch when `maxvec`/`sr` change; update hardware sample rate if needed; `object_method(dsp64, gensym("dsp_add64"), x, porya_perform64, 0, NULL)`.
- `porya_perform64(...)` — hot path:
  ```c
  m4a_driver_advance(x->driver, sampleframes);
  hw_audio_render(x->hwAudio, x->driver, x->scratchL, x->scratchR, sampleframes);
  double *outL = outs[0]; double *outR = outs[1];
  for (long i = 0; i < sampleframes; i++) { outL[i] = x->scratchL[i]; outR[i] = x->scratchR[i]; }
  ```

## Step 4 — Attributes (persisted with patch)

All marked `ATTR_SAVE`:
- `prog0` … `prog15` — long, 0..127. Each setter calls `m4a_driver_program_change(x->driver, trackIndex, val)`.
- `songvol` 0..127 → `m4a_driver_set_song_volume`
- `reverb` 0..127 → `m4a_driver_cc(x->driver, ch, 91, val)` broadcast to all 16 channels
- `maxpcm` 1..12 → updates max PCM channel allocation on driver
- Voicegroup root/name are not saved as external attrs. Node owns restore and
  sends `voicegroup <root> <bank>` only after syntax validation.

## Step 5 — Voicegroup loading off the audio thread

`voicegroup_load` walks the filesystem and parses .s + .wav files — must NOT run on the audio thread. The `voicegroup` message handler defers to a worker:

```c
void porya_voicegroup(t_porya *x, t_symbol *root, t_symbol *name) {
    t_atom av[2]; atom_setsym(&av[0], root); atom_setsym(&av[1], name);
    defer_low(x, (method)porya_voicegroup_do, NULL, 2, av);
}
static void porya_voicegroup_do(t_porya *x, t_symbol *s, short ac, t_atom *av) {
    LoadedVoiceGroup *vg = voicegroup_load(atom_getsym(av+0)->s_name,
                                           atom_getsym(av+1)->s_name, NULL);
    if (!vg) { object_error((t_object*)x, "voicegroup load failed"); return; }
    LoadedVoiceGroup *old = x->loadedVg;
    m4a_driver_all_sound_off(x->driver);          // silence before pointer swap
    m4a_driver_set_voicegroup(x->driver, vg->voices);
    x->loadedVg = vg;
    if (old) voicegroup_free(old);
    x->vgRoot = atom_getsym(av+0); x->vgName = atom_getsym(av+1);
}
```

`defer_low` runs on Max's low-priority thread — safe for I/O. The brief `all_sound_off` before the swap avoids audible glitches.

## Step 6 — `.amxd` patch contract (you assemble this in Max)

The external's contract:
- **Inlet 0:** raw MIDI bytes (ints from `[midiin]`) and control messages (`midievent`, `program`, `voicegroup`, `tempo`, `panic`)
- **Outlets:** 0 = signal L, 1 = signal R
- **Attributes (automatable):** `prog0`..`prog15`, `songvol`, `reverb`, `maxpcm`

Suggested patch wiring inside the `.amxd`:
```
[node.script poryaaaa_voicegroup_server.js] → [route voicegroup] → [prepend voicegroup] → [poryaaaa~]
[plugsync~] → [snapshot~ 100] → [tempo $1] → [poryaaaa~]
[midiin] → [poryaaaa~]
[poryaaaa~] → [live.gain~] → [plugout~ 1 2]
```
The raw `[midiin] → [poryaaaa~]` wire carries every MIDI byte (status + data, including XCMD CCs) straight into the driver — no `[midiparse]` step to drop or reshape anything. `live.dial`s in the device UI bind to the attributes for Live automation.

## Step 7 — Build & install [done]

From `/Users/sallegrezza/dev/cProjects/max-sdk`:
```bash
cmake -B build -G Xcode
cmake --build build --target poryaaaa_tilde --config Release
codesign --force --deep -s - build/.../poryaaaa~.mxo   # required on Apple Silicon
```
The `.mxo` lands in the SDK's `externals/` folder. Copy or symlink to `~/Documents/Max\ 8/Packages/poryaaaa/externals/poryaaaa~.mxo` so Max can find it.

## Verification

End-to-end test in a blank Max patcher (before wrapping as `.amxd`):
1. **Audio out** — `[ezdac~]` connected; `[poryaaaa~] → [ezdac~]` on both channels; turn on dac.
2. **Voicegroup load** — `[message: voicegroup ~/dev/pokeemerald voicegroup001] → [poryaaaa~]`; expect no `object_error` post.
3. **Note response** — `[midiin] → [poryaaaa~]`; play a key on any connected MIDI controller, hear sound. For a no-controller smoke test: `[message: midievent 144 60 100] → [poryaaaa~]` (note on, ch 0, key 60, vel 100).
4. **Program change** — `[message: prog0 25] → [poryaaaa~]`, play track 0, instrument changes.
5. **Tempo** — `[message: tempo 140.] → [poryaaaa~]`; LFO/vibrato rate matches.
6. **State persistence** — set attrs, save patch, quit Max, reopen → attrs restored, voicegroup reloaded automatically.
7. **M4L** — wrap as `.amxd`, drop on a Live MIDI track, automate `prog0` from a clip envelope.
8. **Vector size sanity** — set Max signal vector to 16, 64, 1024; no crashes, audio identical.

## Risks & open items

1. **Samplerate change.** Max can call `dsp64` repeatedly with new sample rates. Ensure `hw_audio` sample rate is updated dynamically or reset cleanly when sr changes.
2. **Voicegroup pointer swap race.** The deferred handler swaps `driver->voiceGroup` while the audio thread reads it. The `all_sound_off` call before the swap silences active voices, making any partially-read pointer benign for one tick.
3. **CLAP-saved patch state is not loadable** in the M4L device (different stream format). Document as a known incompatibility — users rebuild patches in Live.
4. **Denormal flushing.** Driver doesn't set FZ/DAZ. On Apple Silicon, set FPCR FZ bit at `dsp64` entry only if profiling shows denormal stalls.
