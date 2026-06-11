# ccomidi.legatobend MVP Plan

## Goal

Create a standalone Max MIDI external named `ccomidi.legatobend` that turns
single-channel legato note gestures into generated pitch-bend ramps while
keeping the downstream note identity anchored to the first note in the phrase.

The patch/device is out of scope for the MVP. The external will receive raw
MIDI bytes from `[midiin]` through an `int` inlet and emit raw MIDI bytes through
one outlet for `[midiout]`.

## Behavior Contract

- One independent single-voice portamento phrase per MIDI channel.
- Control messages:
  - `bend_time <ms>`
  - `bend_curve linear|easing`
- `bend_time 0` disables the transform. In disabled mode, MIDI bytes pass
  through unchanged.
- Default bend time is `80` ms.
- Default bend curve is `linear`.
- Generated ramp ticks are fixed at `5` ms.
- The first note-on in an idle channel is the anchor and passes through.
- Any later note-on before phrase end becomes the current glide target. Its
  note-on is suppressed.
- If a new target arrives while bending, it replaces the previous target.
  Superseded note-offs are ignored.
- Pitch-bend values are absolute relative to the anchor note and MP2K pitch
  math. MP2K computes semitone offset as `bend * bendRange / 64`, so generated
  bend MSB is `64 + round((target_note - anchor_note) * 64 / bendRange)`,
  clamped to `0..127`.
- Per-channel bend range defaults to MP2K's `2`; incoming CC `0x14`/BENDR
  updates that channel's scale and passes through unchanged.
- Ramps use the selected bend curve. `linear` uses raw time progress; `easing`
  uses smoothstep ease-in-out. Internally they may use fractional progress;
  emitted bend bytes use the prototype 0..127 pitch-bend MSB mapping.
- If the target note-off arrives while the anchor key is still held, ramp back
  to neutral `64` and keep the anchor sounding.
- If the target note-off arrives after the anchor key was released, emit the
  anchor note-off and leave the current bend in place for the release tail.
  Reset bend to `64` immediately before the next clean anchor note-on.
- If the anchor note-off arrives with no active target, pass it through and end
  the phrase.
- If the anchor note-off arrives while a target is active, suppress it and keep
  the phrase alive until the target ends.
- Avoid normalizing passthrough MIDI. Generated note-offs use `0x80 note 0`.
- Non-note MIDI messages pass through unchanged, including incoming pitch bend.
- The MVP parser owns channel-voice messages, running status, realtime bytes,
  system common messages up to three bytes, and raw SysEx byte passthrough.
- Invalid Ableton-impossible note ordering is not a recovery target. Keep the
  state machine direct; use assertions for internal invariants where useful.

## Implementation Shape

Add a new external directory:

`source/midi/ccomidi.legatobend/`

Files:

- `legatobend_core.hpp/.cpp`: pure C++ per-channel phrase and ramp logic.
- `legatobend_parser.hpp/.cpp`: independent raw MIDI byte parser.
- `ccomidi.legatobend.cpp`: Max wrapper, int inlet, outlet, `bend_time`, clock.
- `tests/test_legatobend_core.cpp`: gesture-level tests.
- `CMakeLists.txt`: module target and plain executable test target.

Do not depend on the existing `source/midi/ccomidi` parser or bend helpers.
`ccomidi` is a separate moving target.

## Verification

Success criteria:

1. The plan document exists before source implementation.
2. The new test executable covers the core musical gestures:
   - disabled passthrough
   - non-legato anchor note passthrough
   - short glide returning from in-progress bend
   - anchor released during glide, target release ends phrase
   - retargeted glide ignores older target release
3. `npm run build:externals` succeeds.
4. The new focused test executable succeeds from the build tree.
5. A thermo-nuclear code quality review runs on the resulting changes, and any
   actionable blockers are addressed.
