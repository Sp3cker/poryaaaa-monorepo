# Native Audio Parity Harness Specification

Scope: opt-in comparison infrastructure for `packages/poryaaaa`. This document
specifies how to obtain and interpret parity evidence; it does not authorize
changes to poryaaaa or mGBA audio arithmetic.

## Claims are separate

| Claim | Required comparison | Bound of a passing result |
| --- | --- | --- |
| Oracle | Full mGBA native capture versus the pinned mGBA trace replay | Every frame and cycle in one selected `BEGIN`/`END` capture; not all games or emulator frontends. |
| Hardware | Pinned mGBA replay versus poryaaaa replay of the same trace | Every frame, cycle, left sample, and right sample in that selected trace; not driver parity. |
| Driver | Reference and candidate lifecycle traces, then their independent hardware replays | The fixed 30-cell matrix, twice independently; not instruction-cycle parity or arbitrary MP2K state. |
| Renderer | `poryaaaa_render` under the fixed profile in the reference-tool README | 60,000 stereo frames (1.25 seconds at 48 kHz); not a whole-ROM or hardware-parity claim. |
| Frontend | Explicitly labeled waveform diagnostics | The selected 512-sample local window after diagnostic alignment; not stereo, timing, hardware, or driver proof. |

Do not use a passing claim in one row as evidence for another row. In
particular, a bounded hardware replay pass does not establish end-to-end driver
parity.

## Pinned oracle policy

The authoritative oracle is mGBA commit
`afd6f14eaf8bd35214ed3fb9dc69a92bfc3877a9`, built from a dedicated external
worktree. Apply the repository's
[`mgba-audio-observation.patch`](../tools/mgba-reference/mgba-audio-observation.patch)
to a clean worktree at that commit. The patch is observation-only: its sink
must not allocate, write files, or alter emulation scheduling.

An installed mGBA library is frontend-only and non-authoritative. It must not
be used for native oracle or hardware-parity evidence. The external ROM, ELF,
and decomp checkout must match each other and are inputs, not repository
artifacts. Use placeholders such as `/path/to/hearth-test` and
`/path/to/mgba-audio-reference`; do not encode a developer's local path.

Record the mGBA commit, observation-patch SHA-256, compiler identity, ROM and
ELF hashes, trace hash, and PCM/cycle hashes in every accepted manifest. A
mGBA worktree with changes other than the reviewed observation patch is not an
oracle.

## Canonical trace contract

The trace is UTF-8 text beginning with:

```text
PORYAAAA_AUDIO_TRACE 1
CLOCK 16777216
```

Its events are ordered by `(absolute_gba_cycle, same_cycle_order)`:

```text
WRITE cycle order width address value
TIMER cycle order timer_id
SAMPLE cycle order
BEGIN cycle order
END cycle order
```

`WRITE` payloads are little-endian GBA bus values. Audio registers occupy
`0x04000060` through `0x04000088`, Wave RAM is `0x04000090` through
`0x0400009F`, and FIFO writes are 32-bit writes at `0x040000A0` and
`0x040000A4`. `TIMER` denotes DirectSound byte consumption. FIFO writes caused
by a timer underflow precede its timer event at the same cycle.

`SAMPLE` records a native DAC frame at the exact observed mGBA point. Events
before `BEGIN` initialize state but are unmeasured. The extended order encoding
uses bit 31 as its marker; bits 30--16 are the same-cycle sequence and bits
15--0 are `cyclesLate` for `TIMER` only. Unmarked traces have zero lateness.
Recorders reject more than 32,768 events at one cycle or timer lateness above
65,535 cycles.

Each native capture prefix produces `.pcm` (interleaved signed little-endian
PCM16), `.cycles` (little-endian `uint64` GBA cycle per frame), and `.json`
(format, clock, frame count, cycle range, channel mask, and provenance).

## Target topology

Production remains:

```text
m4a_advance -> m4a_get_pending_writes -> hw_audio_render_events -> m4a_consume_writes
```

It has no trace parser, scheduler, recorder, or writer dependency. The same
HwAudio/HwPsg sources are separately compiled into an opt-in trace-enabled
variant for `poryaaaa_audio_trace`; it is not an adapter around a production
`HwAudio`. The pinned-mGBA recorder and `mgba_audio_trace_replay` are opt-in
consumers of the canonical trace. Driver lifecycle tooling writes the candidate
M4A transaction trace through its dedicated trace-writer target, not the normal
renderer.

This separation preserves event ordering, GBA-cycle timing, FIFO behavior, PSG
clocks, SOUNDBIAS cadence, mix arithmetic, and resampler phase in the shipped
interface.

## Reproducible gates

### Prerequisites

- A clean external mGBA worktree at the pinned revision with the observation
  patch applied.
- A matching, already-built `/path/to/hearth-test` ROM and ELF.
- The package's existing `build/` directory configured with
  `-DPORYAAAA_BUILD_TRACE_TOOLS=ON`,
  `-DPORYAAAA_BUILD_MGBA_REFERENCE=ON`, and
  `-DPORYAAAA_MGBA_SOURCE=/path/to/mgba-audio-reference`.

`packages/poryaaaa/.gitignore` already ignores `build/` and every documented
build-output descendant. Do not add child-directory ignore rules. Generated
traces, PCM, cycles, WAV files, manifests, failed-run directories, and local
research evidence stay below `build/` or outside the repository.

### Oracle and hardware replay

From `packages/poryaaaa`, build the explicitly requested opt-in targets, record
a native trace/capture with the full-ROM recorder, then run both replay paths:

```bash
cmake --build build --target mgba_mp2k_reference mgba_audio_trace_replay poryaaaa_audio_trace
./build/mgba_audio_trace_replay --input /tmp/song.trace --output-prefix /tmp/mgba-replay
./build/poryaaaa_audio_trace --input /tmp/song.trace --output-prefix /tmp/poryaaaa-replay
python3 tools/mgba-reference/native_compare.py /tmp/mgba-native.json /tmp/mgba-replay.json
python3 tools/mgba-reference/native_compare.py /tmp/mgba-native.json /tmp/poryaaaa-replay.json
```

Both comparisons require identical frame counts, cycles, and stereo integers.
No mono fold, lag search, DC removal, gain fitting, polarity correction, or
normalization is permitted in either gate. A missing pinned worktree or matching
ROM/ELF is a missing prerequisite, not a passing result.

### Driver lifecycle

`validate_driver_matrix.py` is the single top-level external parity gate. It
runs the five canonical fixtures (PSW normal and alternate, DirectSound, Square
1, Square 2) through six fixed lifecycle scenarios, giving 30 cells; every cell
runs twice and must be deterministic. It checks exact transaction order,
payload, logical state, reference native replay, reference hardware replay, and
candidate hardware replay.

```bash
cmake --build build --target mgba_mp2k_reference mgba_audio_trace_replay poryaaaa_audio_trace poryaaaa_driver_trace
python3 tools/mgba-reference/validate_driver_matrix.py \
  --decomp /path/to/hearth-test \
  --output-dir build/driver-validation/lifecycle
```

The matrix does not claim raw ARM instruction-cycle equivalence, arbitrary
fixture coverage, timer-1 routing, or reference-versus-candidate whole-engine
parity. It atomically publishes an accepted result only after both runs agree;
failed diagnostics remain in the sibling `.failed` directory.

### Optional hardware stress

`validate_hardware_stress.py` is an opt-in diagnostic until it is deliberately
promoted. Its four 3.0-second cases observe 196,608 frames over 50,331,648 GBA
cycles at a 256-cycle cadence. A successful run strengthens sampled hardware
coverage for those cases only; it is neither the external gate nor a universal
hardware/driver claim.

## Failure handling

Stop at the first failed prerequisite or comparison. Preserve the failing
manifest, input trace hash, first differing event/sample, source ordinal, and
observed cycle. Do not tune gain, filters, resampling, reverb, timing, or song
parameters to make a comparison pass. Waveform analysis is diagnostic only and
must remain labeled with its local scope.
