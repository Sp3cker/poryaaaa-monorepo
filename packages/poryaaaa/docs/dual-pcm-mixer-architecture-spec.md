# Dual PCM Mixer Architecture Specification

Status: agreed architecture; implementation not started.

This specification replaces the assumption that the current
`plugin/m4a/m4a_pcm.c` is either vanilla Sappy or complete iPatix. It is a
hybrid audit input only. The finished product contains two source-owned,
runtime-selectable PCM mixers named `ipatix` and `sappy`.

Supporting research: [`ipatix-hq-mixer-research.md`](ipatix-hq-mixer-research.md).

## Source definitions

The algorithms are defined by these immutable references:

- iPatix HQ Mixer revision 4.0 at
  [`2bd31435101e4c3a4af568a6ed33d7cc9f1a6da9`](https://github.com/ipatix/gba-hq-mixer/tree/2bd31435101e4c3a4af568a6ed33d7cc9f1a6da9).
  Reference fixtures use `POKE_CHN_INIT=1`, `ENABLE_STEREO=1`,
  `ENABLE_REVERB=1`, and `ENABLE_DMA=1`. The upstream pin has reverb disabled;
  the fixture tooling owns and hashes the one-line reverb patch.
- Vanilla Sappy from pokeemerald `src/m4a_1.s` and
  `include/gba/m4a_internal.h` at
  [`9a83a2bbe8e097e62c00f1dbd56849766775d7b6`](https://github.com/pret/pokeemerald/tree/9a83a2bbe8e097e62c00f1dbd56849766775d7b6).
- Native observations use mGBA
  [`afd6f14eaf8bd35214ed3fb9dc69a92bfc3877a9`](https://github.com/mgba-emu/mgba/tree/afd6f14eaf8bd35214ed3fb9dc69a92bfc3877a9)
  plus `tools/mgba-reference/mgba-audio-observation.patch`, SHA-256
  `3e5dec217b04917733767339e95dbb2b1eb64f292345603e69c57c47ce64a017`.

Source-parity claims cover 1–12 PCM voices. The existing capacity of 13–15 is a
poryaaaa extension: it receives deterministic stress coverage but is never
reported as source parity.

## Product contract

- Exactly two values exist: `ipatix` and `sappy`. There is no `auto`, `hybrid`,
  compatibility, or fallback mode.
- Fresh drivers and absent configuration select `ipatix`.
- iPatix is completed and proved through poryaaaa's full audio stack first.
  Sappy is not exposed while it is absent or incomplete.
- Sappy is then implemented independently. It never calls an iPatix symbol,
  reads iPatix-private state, or substitutes iPatix arithmetic.
- Selection becomes user-visible only after both independent oracle suites pass.
- A live mode change hard-stops PCM voices at the next SoundMain/VBlank entry.
  CGB voices continue. Already-published PCM ring bytes drain; the new adapter
  replaces only its normally scheduled output segment. There is no crossfade or
  active-voice conversion because either would introduce a third, non-source
  mixer behavior.

## Architecture decision

`m4a_pcm.c` becomes a mode-neutral deep module. It owns orchestration; two
separate translation units own the algorithms.

```text
m4a_track / engine / driver configuration
                  |
                  v
          m4a_pcm.c dispatcher
          - pending-mode commit
          - VBlank block geometry
          - seven lifecycle seams
          - ring publication
             /             \
            v               v
m4a_pcm_ipatix.c     m4a_pcm_sappy.c
complete HQ stack    complete vanilla stack
             \             /
              v           v
        common FIFO-source ring
                  |
                  v
       m4a_main timer/DMA scheduler
```

A direct enum switch is used instead of an operations table. There are exactly
two closed modes, and dispatch occurs only at named block/lifecycle seams. A
function table would add indirection and state without enabling a required use
case. Duplicating the entire `M4ADriver` is also rejected because VBlank,
DMA/FIFO, event, and cycle scheduling must remain one authority.

A single mixer with mode branches inside its arithmetic is prohibited. That
would recreate the current hybrid and make source provenance untestable.

## File ownership

### Core driver

| File | Ownership |
| --- | --- |
| `plugin/m4a/m4a_pcm.c` | Mode request/commit, common block geometry, direct dispatch, common ring publication. No envelope, cursor, decode, synth, accumulation, quantization, or reverb arithmetic. |
| `plugin/m4a/m4a_pcm_internal.h` | Private dispatcher contract, logical channel-control types, per-mode state types, and adapter declarations. |
| `plugin/m4a/m4a_pcm_ipatix.c` | Complete iPatix channel lifecycle, source traversal, Camelot synths, BDPCM, packed HQ accumulation, error feedback, reverb, and downconversion. |
| `plugin/m4a/m4a_pcm_sappy.c` | Complete vanilla lifecycle, compressed cache/decoder, source traversal, direct 8-bit DMA-lane accumulation, and DMA-domain reverb. |
| `plugin/m4a/m4a_internal.h` | `M4ADriver` mode tags plus common cadence/FIFO/ring state and bounded mode-state union. The existing hybrid channel and scratch fields are removed. |
| `plugin/m4a/m4a_driver.h` | Public two-value enum and allocation-free set/get contract. |
| `plugin/m4a/m4a_driver.c` | Explicit iPatix default, validated request storage, lifecycle reset, and rate-reconfiguration integration. |
| `plugin/m4a/m4a_track.c` | Uses dispatcher lifecycle seams; no direct copy of mode-private cursor, synth, or decode state. |
| `plugin/m4a/m4a_main.c` | Includes the internal PCM contract. VBlank and FIFO scheduling otherwise remain unchanged. |
| `plugin/m4a/CMakeLists.txt` | Builds dispatcher and both implementation files. |

### Product layers

- `plugin/m4a_engine.h` and `plugin/m4a_engine.c`: apply one mode to primary,
  shadow, and audition drivers. Replace raw PCM-channel snapshots and shadow
  copies with dispatcher snapshot/clone operations.
- C CLAP frontend: `plugin/m4a_plugin.{c,h}`, `plugin/m4a_params.{c,h}`, and
  `plugin/m4a_gui.{cpp,h}`.
- Rust CLAP frontend: `plugin/build.rs`, `plugin/src/ffi.rs`,
  `plugin/src/config.rs`, `plugin/src/params.rs`, `plugin/src/runtime.rs`,
  `plugin/src/plugin.rs`, and `plugin/src/editor.rs`.
- Offline renderer: `cmd/poryaaaa_render.c`.
- Configuration/documentation: `poryaaaa.cfg.example` and `README.md`.
- Build/gates: package `CMakeLists.txt`, `scripts/test-poryaaaa.sh`, mixer tests,
  and `tools/mgba-reference/pcm-mixer-oracle/`.

Both CLAP frontends remain supported. Removing one is outside this work.

## Channel and state ownership

The current `M4ADriverPcmChan` is not retained unchanged. It combines a
normalized status model with current-renderer cursor, interpolation, loop, and
Camelot state. Treating it as a common source ABI would preserve the hybrid.

The replacement uses the source-canonical status encoding:

```c
START = 0x80
STOP  = 0x40
LOOP  = 0x10
IEC   = 0x04
ENV   = 0x03
ON    = START | STOP | IEC | ENV
```

`ON` is an aggregate active mask and excludes `LOOP`. Every status producer,
predicate, test, and public snapshot must migrate; no compatibility alias
remains.

These are the pinned source meanings, not the current local macro names.
Pokeemerald defines `START=0x80` and `ON` as the aggregate mask
([`m4a_internal.h`](https://github.com/pret/pokeemerald/blob/9a83a2bbe8e097e62c00f1dbd56849766775d7b6/include/gba/m4a_internal.h));
iPatix names the same bit `FLAG_CHN_INIT`
([`m4a_hq_mixer.s:94-101`](https://github.com/ipatix/gba-hq-mixer/blob/2bd31435101e4c3a4af568a6ed33d7cc9f1a6da9/m4a_hq_mixer.s#L94-L101)).
The local `ON=0x80`/`START=0x20` normalization is part of the hybrid being
removed, not the migration target.

| Ownership | Fields/state |
| --- | --- |
| Common logical control | Status, voice/type flags, right/left volume, ADSR bytes, key, current envelope value, pseudo-echo volume/length, MIDI key, velocity, priority, rhythm pan, gate time, wave pointer, and track index—but only after both pins confirm identical meaning. This is a host logical model, not binary ABI compatibility. |
| iPatix channel-private | Current source position, preceding interpolation sample, remaining count, Q9.23 phase/step, loop caches, BDPCM state, Camelot oscillator phase/filter/duty, and any iPatix-derived envelope outputs. |
| Sappy channel-private | Native source cursor/count/fractional state, compressed-data cache and indices, loop/end state, and any vanilla-derived envelope outputs. |
| iPatix global-private | Persistent packed HQ work buffer, left/right discarded-bit state, and reverb seeds/history required by the pinned algorithm. |
| Sappy global-private | Direct 8-bit DMA mixing/reverb state and source-required decode scratch. |
| Common hardware publication | `M4APcmRing`, write geometry, DMA/FIFO cursors, timer schedule, cycle state, and ordered register events. Neither adapter owns their timing. |

Pitch, volume, note-off, allocation, and status writes remain direct common
control only after source comparison plus fixtures prove identical semantics.
Otherwise they receive a named dispatcher operation. The default is to keep
behavior separate, not to deduplicate plausible-looking code.

All bounded scratch is embedded in `M4ADriver`; mode selection, commit, and
rendering allocate nothing.

## Internal interface

Dispatch occurs at seven semantic seams:

1. enter/reset mode;
2. render one VBlank block;
3. start a PCM channel;
4. update effective pitch when adapter-private state requires it;
5. inherit playback position for portamento;
6. clone a channel into a shadow driver in the same mode; and
7. produce a normalized public/debug snapshot.

There is no mixer-mode branch inside a channel or sample loop. Source-type,
endpoint, and decode branches required by either pinned algorithm remain local
to that adapter.

`m4a_drv_pcm_start` gains `M4ADriver *` so it can dispatch initialization.
`m4a_track.c`'s portamento cursor transfer and `m4a_engine.c`'s whole-record
victim/shadow copies are removed. Same-mode clone copies all private voice
state. Cross-mode clone is rejected. The normalized snapshot contains only
observable engine fields and never exposes adapter storage.

The public driver interface adds:

```c
typedef enum M4APcmMixerMode {
    M4A_PCM_MIXER_IPATIX = 0,
    M4A_PCM_MIXER_SAPPY = 1,
} M4APcmMixerMode;

bool m4a_driver_set_pcm_mixer_mode(M4ADriver *drv, M4APcmMixerMode mode);
M4APcmMixerMode m4a_driver_get_pcm_mixer_mode(const M4ADriver *drv);
```

The setter validates and queues a request under the driver's existing
single-thread ownership. It does not make the driver generally thread-safe.
The getter reports the active, not merely requested, mode. The engine setter
queues the same value for primary, shadow, and audition drivers.

Every new function receives a short comment explaining its worth. Trivial
one-line predicates remain inline at their callsites per package rules.

## Switching transaction

A valid request is last-request-wins until the next `m4a_sound_main_ram` entry.
Requesting the active mode cancels an uncommitted change. Invalid enum values are
rejected without mutation.

At the boundary, when requested differs from active:

1. stop all PCM channel controls;
2. clear both adapters' channel/global private storage;
3. zero every unpublished ring slot outside each lane's absolute
   `[fifo_source_cursor, pcm.write_cursor)` published window;
4. install the requested active mode;
5. render the normally scheduled block through the new adapter at the existing
   `pcm.write_cursor`; a voice-free block writes zeros to that segment; and
6. advance the common ring publication metadata exactly as an ordinary block.

The common PCM ring is not cleared wholesale. For FIFO A and B independently,
bytes in the already-published absolute window remain immutable until the
existing DMA/FIFO cursor consumes them; every other slot is reset so no stale
future sample survives the mode change. The new adapter then overwrites only
the segment assigned by the normal scheduler, explicitly zeroing unwritten
samples in that segment. This gives a hard stop at the next published
SoundMain block without corrupting an earlier DMA window.

The transaction preserves:

- CGB channels;
- `pcm_vblank_remainder`;
- DMA counter and period;
- ring geometry and the pre-transaction `pcm.write_cursor`;
- FIFO source/read/write/internal cursors;
- current cycle and every next-cycle deadline; and
- queued FIFO payloads, register events, same-cycle order, and event-drop count.

The ordinary publication step then advances `pcm.write_cursor` by the rendered
frame count. Already-published FIFO bytes drain normally. The change affects
content at a defined SoundMain boundary without retiming hardware.

A PCM note started after a request but before its boundary belongs to the old
mode and is cut by the hard reset. Notes started after commit use the new mode.
This ordering is documented and fixture-tested; no hidden note queue is added.

## iPatix implementation specification

The first shipping milestone contains the new architecture with iPatix as the
only selectable implementation. The current hybrid is moved only as audit
input, then corrected against the pinned source.

The completed adapter must implement, in source order and with explicit-width C
arithmetic:

1. Pokémon channel initialization and canonical status transition;
2. attack/decay/sustain/release and pseudo-echo state;
3. master × envelope × left/right gain calculation;
4. fixed, forward, reverse, looped, and Q9.23 linearly interpolated traversal;
5. exact endpoint, look-ahead, padding, and loop-entry behavior;
6. 33-byte/64-sample Pokémon BDPCM decode across interpolation, loop, and
   reverse boundaries;
7. zero-length Camelot PWM, pseudo-saw, and triangle descriptors;
8. packed `00LL00RR` accumulation with source-defined carry/wrap behavior;
9. signed overflow detection and effective 30-bit clamp;
10. one final signed 8-bit conversion with independent seven-bit discarded-bit
    feedback
    ([`m4a_hq_mixer.s:944-1005`](https://github.com/ipatix/gba-hq-mixer/blob/2bd31435101e4c3a4af568a6ed33d7cc9f1a6da9/m4a_hq_mixer.s#L944-L1005));
11. iPatix reverb ordering: publish the current downsampled DMA bytes, read the
    historical four-tap 8-bit DMA terms, and replace the consumed HQ word with
    the next block's wet seed
    ([`m4a_hq_mixer.s:944-1055`](https://github.com/ipatix/gba-hq-mixer/blob/2bd31435101e4c3a4af568a6ed33d7cc9f1a6da9/m4a_hq_mixer.s#L944-L1055)); and
12. common ring publication without altering DMA/FIFO timing.

ARM self-modification, IWRAM placement, loop-patching, and DMA prefetch are ROM
performance mechanisms, not host semantics, and are not ported.

The iPatix gate covers silence, one/many voices, both clamp signs, sub-/equal-/
greater-than-unity rate, fixed rate, forward/reverse endpoints, loop wrap,
BDPCM block/look-ahead boundaries, all three synths, pseudo-echo, reverb first/
steady/tail blocks, ring wrap, and deliberate packed-lane overflow cases.

No Sappy user value or stub exists at this milestone. CMake and Rust `build.rs`
already compile the split iPatix path so both frontends remain buildable.

## Vanilla Sappy implementation specification

Only after iPatix's mandatory gate passes, implement Sappy from the vanilla pin
without copying iPatix helpers.

The adapter must implement:

1. canonical START-to-envelope initialization and channel lifecycle;
2. vanilla ADSR and pseudo-echo transitions;
3. source-exact fixed, resampled, forward, reverse, loop, end, and padding paths;
4. the vanilla compressed-data buffer/cache and decoder;
5. vanilla gain/truncation at each voice;
6. accumulation of each voice directly into signed 8-bit DMA lanes with exact
   byte-lane order, wrap/saturation, and rounding behavior;
7. vanilla zero/reverb DMA-buffer seeding and four-tap reverb in the 8-bit
   domain; and
8. common FIFO-source ring publication without changing its schedule.

Sappy contains no packed HQ buffer, discarded-bit feedback, iPatix reverb seed,
or Camelot synth fallback. A zero-length Camelot descriptor follows the pinned
vanilla sample-end behavior. Divergence fixtures must demonstrate expected
Sappy/iPatix differences; equality alone is insufficient evidence that two
implementations exist.

## Full-stack mode exposure

Selection is wired only after both algorithm gates pass.

### Configuration and renderer

`poryaaaa.cfg` accepts exactly:

```ini
pcm_mixer=ipatix
# or
pcm_mixer=sappy
```

Surrounding value whitespace is trimmed; comparison is case-sensitive; duplicate
keys are last-wins. A missing file remains nonfatal. A present invalid value is
fatal for that instance and never becomes a mode.

The C plugin loader returns a bounded diagnostic; initialization logs through
the CLAP host logger with stderr fallback and returns false. Rust returns
`io::ErrorKind::InvalidData`, retains/logs the diagnostic, and fails
`initialize`. Both parsers test missing, valid, whitespace, duplicate,
case-mismatch, and invalid cases.

`poryaaaa_render --pcm-mixer ipatix|sappy` applies before any note or advance.
An invalid value prints the common mode diagnostic and exits nonzero. The
renderer does not consume the plugin-adjacent configuration file.

### CLAP parameters and editors

Both frontends expose one stepped, non-automatable global mixer parameter.
Changing it remains host-visible and state-persistent, but sample-accurate
algorithm automation is intentionally unsupported because switching hard-stops
voices.

The C frontend has one atomic parameter mirror. Editor changes use a CLAP
parameter gesture plus `request_flush`/output-event transaction; host and state
events win according to event order; only the audio thread calls the driver
setter.

Rust adds a typed `EnumParam<MixerMode>` with parameter ID `pcm_mixer` and stable
variant IDs `ipatix` and `sappy`. `ffi.rs` maps those IDs to the C enum;
`runtime.rs` applies changes; `editor.rs` uses the normal parameter setter, not
a direct runtime mutation.

Fresh-instance configuration seeds the parameter. Restored host state overrides
configuration.

The engine setter is audio-thread-only. UI and host threads update their
frontend's parameter mirror/queue; the audio thread converts the ordered
parameter event into driver requests before the next SoundMain boundary.

### C state version 3

Version 3 is a bounded, padding-free little-endian transaction. Its 16-byte
header is seven ASCII bytes `PORYM4A`, one NUL byte, `u32 version = 3`, and
`u32 payload_size`. `payload_size` is the exact count of bytes after the header.
Every `u32` below is little-endian and unaligned bytes are decoded explicitly.
The payload is exactly:

1. `u32 project_root_len`, then UTF-8 bytes;
2. `u32 voicegroup_len`, then UTF-8 bytes;
3. `u8 volume`;
4. `u8 reverb`;
5. `u8 mixer_mode` (`0=ipatix`, `1=sappy`);
6. `u8 reserved` (must be zero);
7. sixteen `u8` track programs;
8. `u8 recorder_armed` (`0` or `1`); and
9. `u32 recorder_path_len`, then UTF-8 bytes.

String lengths are byte counts, may be zero, exclude a terminator, and must be
less than their destination capacities: 512 bytes for project root, 256 for
voicegroup, and 512 for recorder path. Strings must be RFC 3629 UTF-8 without
embedded NUL, overlong encodings, or surrogate code points. The loader reads
exactly `payload_size` bytes and then requires EOF. Missing bytes, invalid
strings, invalid mode/reserved/boolean values, payload-size mismatch, or
trailing bytes reject the entire transaction without mutating live state.

The loader stages the first eight bytes. Exact magic selects v3, after which it
reads and validates the remaining eight header bytes. Otherwise a buffered
legacy reader replays all staged bytes from offset zero. First legacy `u32 == 2`
selects the existing v2 decoder; every other value is the unversioned root
length. The irreducible unversioned root-length-2 collision selects v2 and fails
if the remainder is not valid v2. Before changing the loader, check in valid and
truncated fixtures that freeze the current `state_save`/`state_load` v2 and
unversioned layouts. Both older decoders remain bounded transactions and force
`ipatix`, regardless of configuration.

### Rust state migration

Implement static `Plugin::filter_state(state: &mut PluginState)`. The parameter
ID is exactly `pcm_mixer`; stable lowercase string IDs are `ipatix` and `sappy`;
compatible integer values are `0=ipatix` and `1=sappy`. Normalize valid integer
values to the corresponding string ID. If the parameter is absent or its type,
string ID, or integer value is invalid, replace it with `ipatix` and call the
process-global `log::warn!` logger initialized by the Rust plugin entrypoint.
The static hook has no `Result`, so this deterministic repair avoids config
leakage and an unapproved `third_party` change. It is state migration, not an
algorithm fallback.

## Oracle artifacts and mandatory gates

### Reference generation

Add `tools/mgba-reference/pcm-mixer-oracle/` with:

- `inputs/*.s`: self-contained synthetic GBA fixture sources;
- `oracle.ld`;
- `cases.json` and `cases.schema.json`;
- `manifest.schema.json`;
- `ipatix-enable-reverb.patch`;
- `generate_pcm_mixer_oracles.py`; and
- `validate_pcm_mixer_oracles.py`.

`cases.json` is an object with schema string
`poryaaaa.pcm-mixer-cases`, version integer `1`, and a `cases` array. Each case
contains exactly:

- `id`: unique `[a-z0-9-]+` string;
- `mode`: `ipatix` or `sappy`;
- `source`: a relative `inputs/*.s` path; and
- `settings`: integer `pcm_rate_hz`, `max_channels`, `master_volume`,
  `voice_volume`, and `reverb`.

Generation command, run from `packages/poryaaaa`:

```sh
python3 tools/mgba-reference/pcm-mixer-oracle/generate_pcm_mixer_oracles.py \
  --ipatix-source /path/to/gba-hq-mixer \
  --sappy-source /path/to/pokeemerald \
  --mgba-source /path/to/mgba-audio-reference \
  --output test/fixtures/m4a_pcm
```

The generator verifies clean source pins and patch hashes, builds each source
with `oracle.ld`, and writes this exact tree:

```text
test/fixtures/m4a_pcm/<mode>/<id>/manifest.json
test/fixtures/m4a_pcm/<mode>/<id>/expected.trace
```

`manifest.schema.json` sets `additionalProperties: false` at every object. Each
manifest contains:

- `schema`: string `poryaaaa.pcm-mixer-oracle`;
- `version`: integer `1`;
- `case`: string matching the case ID and `mode`: enum string;
- `reference.repository`, `.commit`, `.source_tree_sha256`, and nullable
  `.patch_sha256` strings;
- `reference.feature_flags`: for iPatix, exactly integer
  `POKE_CHN_INIT=1`, `ENABLE_STEREO=1`, `ENABLE_REVERB=1`, and `ENABLE_DMA=1`;
  for Sappy, an empty object;
- `toolchain.assembler`, `.linker`, `.objcopy`, and `.readelf`: each tool's
  complete UTF-8 `--version` standard output, with CRLF normalized to LF and
  trailing line breaks removed;
- `observation.mgba_commit` and `.patch_sha256` strings;
- `hashes.generator`, `.cases`, `.elf`, `.rom`, `.input`, and `.trace`;
- the five integer `settings` values; and
- `range.begin_cycle`/`.end_cycle` unsigned 64-bit integers,
  `.row_count` unsigned 32-bit integer, and `.event_hash` string.

Every hash is lowercase hexadecimal SHA-256 of raw bytes. `generator` hashes
`generate_pcm_mixer_oracles.py`, `cases` hashes `cases.json`, `input` hashes the
case's `.s` file, and `trace` hashes complete raw `expected.trace` bytes.
`event_hash` hashes the canonical UTF-8 concatenation of only the compared FIFO
`WRITE` rows, preserving each complete row and terminating each with LF.

Canonical traces reuse `PORYAAAA_AUDIO_TRACE 1`. The parser validates `CLOCK`,
`BEGIN`, `WRITE`, `TIMER`, and `END` syntax, scope, monotonic cycles, and
same-cycle order. Mixer comparison filters FIFO A/B `WRITE` rows and requires
exact equality of cycle, order, address, width, and value; `row_count` is the
number of compared rows. Reference generation is maintainer-only; candidate
tests need no external repository or ROM.

### Candidate oracle gate

Add `poryaaaa_pcm_mixer_fixture_runner`, a C executable linked to the candidate
driver. The dedicated `poryaaaa_pcm_mixer_oracle_tests` custom build target
depends on that runner and the checked-in fixtures; its registered CTest
`poryaaaa_pcm_mixer_oracle` invokes the strict Python validator. This split
keeps JSON/trace parsing out of the candidate driver and shipping targets.
From `packages/poryaaaa`, the normative validator invocation is:

```sh
python3 tools/mgba-reference/pcm-mixer-oracle/validate_pcm_mixer_oracles.py \
  --fixtures test/fixtures/m4a_pcm \
  --runner build/poryaaaa_pcm_mixer_fixture_runner
```

The validator strictly reads each manifest/trace, runs the candidate twice from
a fresh driver, and compares the filtered FIFO rows. It does not link trace text
code into shipping targets or `poryaaaa_unit_tests`.

Missing fixtures, unknown schema fields, hash mismatches, malformed rows, count
mismatches, output mismatches, and nondeterminism fail; nothing skips. Both
candidate traces must equal the golden FIFO rows and each other.

Register the gate with CTest under `poryaaaa_pcm_mixer_oracle` and run it
unconditionally from `scripts/test-poryaaaa.sh`.

### No-allocation gate

Add a single-thread `poryaaaa_pcm_mixer_no_alloc_tests` executable. It recompiles
the complete `PORYAAAA_M4A_DRIVER_SOURCES` set plus both adapter translation
units; it must not reuse already-compiled driver objects. A force-included
`test_allocator_wrap.h` redirects `malloc`, `calloc`, `realloc`, and `free` to
four `uint64_t` call counters on Darwin/ELF (`-include`) and MSVC (`/FI`).
Compile `test_allocator_wrap.c` separately without the forced include so it can
delegate to the CRT.

After driver creation and fixture setup, snapshot all four counters. The first
render after the snapshot, same-mode requests, a real mode-change request,
boundary commit, and subsequent render are all inside the measured interval.
Each individual counter must have delta zero, including `free` and `realloc`.
Register the test with CTest as `poryaaaa_pcm_mixer_no_alloc` and run it
unconditionally.

### Canonical package command

Update `scripts/test-poryaaaa.sh` to run, under its existing
`set -euo pipefail`:

```sh
cmake --build packages/poryaaaa/build --target \
  poryaaaa_unit_tests \
  poryaaaa_pcm_mixer_oracle_tests \
  poryaaaa_pcm_mixer_no_alloc_tests
packages/poryaaaa/build/poryaaaa_unit_tests
ctest --test-dir packages/poryaaaa/build --output-on-failure \
  -R '^poryaaaa_pcm_mixer_(oracle|no_alloc)$'
cargo test --manifest-path packages/poryaaaa/plugin/Cargo.toml
```

The script computes absolute paths from `repo_root` as it does today. Missing
targets, fixtures, or test registrations therefore fail instead of skipping.

CMake compile/link checks retain C frontend coverage. Rust tests prevent
`build.rs`, FFI, config, parameter, runtime, state, and editor paths from
rotting. A full-song comparison remains an explicitly external diagnostic
because no redistributable song fixture is checked in; it is not a hermetic
completion gate.

## Switch verification

Exact switch cases cover:

- both directions and same-mode no-op;
- last-request-wins and cancellation back to active;
- request-plus-note ordering;
- bytes immediately before and after the boundary;
- ring wrap;
- active ordinary PCM, compressed PCM, and iPatix synth voices;
- uninterrupted CGB output;
- preserved scheduler/FIFO/cycle snapshots; and
- zero additional dropped events.

## Implementation order and completion gates

### Phase 1: architecture plus complete iPatix

1. Add the dispatcher and replacement state model.
2. Canonicalize status encoding and migrate every caller/test.
3. Split iPatix into its translation unit; update CMake and Rust `build.rs`.
4. Complete every iPatix algorithm stage listed above.
5. Add and pass the mandatory iPatix oracle and allocation gates twice.
6. Keep both frontends and the renderer working with iPatix as the only mode.

Gate: exact iPatix FIFO matrices pass; no Sappy selector or stub exists.

### Phase 2: independent vanilla Sappy

1. Implement the vanilla adapter from the pinned source, not from iPatix C.
2. Add its compressed/source/reverb/direct-byte fixtures.
3. Add deliberate divergence cases against iPatix.
4. Pass the Sappy oracle twice and the no-allocation gate.

Gate: both complete implementations exist, with no cross-algorithm symbol use.

### Phase 3: activate selection full-stack

1. Enable the driver/engine enum and hard-reset transaction.
2. Wire C CLAP, Rust CLAP, both editors, config, state migration, and renderer.
3. Add switch and legacy-state goldens.
4. Update README and configuration example.
5. Delete the hybrid fields, code, comments, temporary paths, and obsolete tests.

Gate: the canonical package command passes with no skip; invalid values fail;
old state restores iPatix; both modes switch at the documented boundary without
retiming the hardware model.

## Final acceptance criteria

The work is complete only when:

- `m4a_pcm_ipatix.c` and `m4a_pcm_sappy.c` each contain a complete,
  source-proven implementation;
- neither implementation calls the other;
- `m4a_pcm.c` contains no mixer arithmetic;
- no current hybrid field/comment/path survives;
- both frontends, config/state/editors, and renderer select the same two values;
- exact, checked-in FIFO oracles pass twice for both modes;
- the allocation and switch gates pass;
- timing/FIFO/event behavior is invariant across selection; and
- source-parity claims remain bounded to the recorded fixture matrix and 1–12
  voices.
