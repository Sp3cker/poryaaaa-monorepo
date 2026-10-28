# MIDI Quick Look preview and offline voicegroup bank

On macOS, this standalone package builds a view-based Quick Look preview extension
and a minimal AppKit host. Spacebar previews stream MIDI immediately through the
poryaaaa engine to AVAudioEngine. A compact piano roll shows notes, playback position,
elapsed/total time, and supports click-and-drag seeking.
The deterministic C11 bank baker and loader remain usable independently.

## Build, regenerate, and test

Run from the poryaaaa monorepo root:

```sh
cmake -S packages/poryaaaa-quicklook -B packages/poryaaaa-quicklook/build
cmake --build packages/poryaaaa-quicklook/build
packages/poryaaaa-quicklook/build/bake_voicegroup \
  /Users/spencer/dev/hearth-test hanabi \
  packages/poryaaaa-quicklook/assets/hanabi.pbank
ctest --test-dir packages/poryaaaa-quicklook/build --output-on-failure
PQL_HEARTH_ROOT=/Users/spencer/dev/hearth-test \
  ctest --test-dir packages/poryaaaa-quicklook/build --output-on-failure
```

The baker accepts `<projectRoot> <voicegroupName> <outputPath>`, loads the bank
once, and prints unique resource counts and total output bytes. Load, allocation,
size-overflow, I/O, and unmapped-pointer failures print to stderr and exit nonzero.
The output directory must already exist. Resource identity maps preserve the
first occurrence in each loader array; shared pointers are serialized only once.
Padding and reserved bytes are zero, so identical loader output produces identical
bytes. Equal content at different addresses is deliberately not deduplicated.

## macOS preview and headless renderer

The same CMake configure/build commands above build `PoryaaaaQuickLook.app`,
`PoryaaaaPreview.appex`, `quicklook_render`, and `test_stream`. Swift 6.4 and the
installed Xcode macOS SDK are required. The deployment target defaults to macOS 15.
The Swift driver is invoked by CMake custom commands so the existing Unix Makefiles
build tree remains supported; the host and extension are native `MACOSX_BUNDLE`
targets. No Swift package manager build or bridging header is involved.

```sh
packages/poryaaaa-quicklook/build/quicklook_render \
  packages/ccomidi/mus_shop.mid /tmp/mus_shop.wav
ctest --test-dir packages/poryaaaa-quicklook/build --output-on-failure
```

The destination WAV must not already exist. The CLI prints duration, frame count,
unclipped engine peak, and the number of discarded SMF chunks. It returns nonzero
on parse, bank, engine, or output errors. New partial WAVs are deleted on failure.

CMake embeds `assets/hanabi.pbank` as a static byte array in the extension binary,
not as a bundle resource. The shared engine renders 44100 Hz stereo planar float;
the offline adapter converts it to PCM16 WAV. Both use engine-native
volume/reverb defaults. Tracks prime to program zero except for the solo-track rule
below. Total playback duration
is capped at 300 seconds (13,230,000 frames), including any tail. Natural endings
receive up to two seconds of tail within that limit; songs reaching the cap stop
at exactly 300 seconds without dispatching events at or beyond the boundary.
After the first event at or beyond the cap is encountered, cooperative production stops
instead of draining the remaining MIDI. SMF chunks beyond the first 16 are dropped,
and channel numbers do not override chunk-to-engine-track identity.

For SMF **type 1**, exactly one retained chunk containing a positive-velocity
note-on is treated as a solo musical track. Its engine track is forced to hanabi
program **1** (`voice_keysplit voicegroup_piano_keysplit`) for the whole playback:
it primes to 1 and all later program changes for that track are suppressed.
Conductor-only chunks and velocity-zero note-ons do not count; discarded chunks
do not count either. Detection shares the existing cursor prepass, with no new
scan. Type 0 and files with multiple note-bearing chunks retain normal programs.
A genuinely drum-only single musical track also receives piano, intentionally.
The rule lives in the shared core and therefore applies to live and offline output.

Loop markers are ignored. Format 2, SMPTE timing, zero PPQN, malformed/truncated
events, and files with no translated channel events produce typed errors.
End-of-track is a cursor terminator, not a scheduled engine event. An allocation-free
initial cursor pass records the final channel tick, then resets the cursors for
normal streaming. Tempo events beyond that tick are ignored; tempos before later
channel events retain their exact timing. This approved extra O(file-size) scan
prevents unbounded conductor-only padding from extending preview duration without
buffering arbitrary numbers of tempo events. The real `mus_shop.mid` fixture
contains both EOT and tempo metadata at a padding tick around 268 million.

MIDI bytes are read-only `mmap` storage. `MidiArena` makes one aligned allocation
for 16 cursors (including peeked event values), the 8192-event FIFO, two 2048-frame
float planes, and PCM scratch. `StreamEngineCore` owns the bank, engine, sample
clock, dispatch, cap, tail, and pull rendering shared by both output adapters.
Its internal 2048-frame/event slices preserve engine call boundaries even when
callers pull 512 frames, keeping offline WAV output byte-identical.

`StreamPlayer` fills and pulls cooperatively, converts to PCM16, and writes WAV
blocks directly with POSIX `write`. `LivePlayer` instead uses a dedicated producer
queue to parse/fill until the FIFO is full or at least one second ahead of the
consumed sample clock; only this queue sleeps briefly when caught up. Reader,
cursors, and producer horizon belong exclusively to that queue. The
AVAudioSourceNode callback exclusively owns dispatch, engine mutation, sample
clock, and the internal float cache. Events cross through the acquire/release
SPSC FIFO; atomics publish the future-event watermark, consumed count, EOF/cap,
and finished state. The watermark prevents advancing over unpublished events,
including same-tick bursts larger than the FIFO. An underrun returns silence
without advancing musical time. No parsing, allocation, locks, logging, or
Objective-C calls occur in the pull path.

### Piano roll and seeking

After audio starts, the preview launches a cancellable background `MidiNoteIndex`
build with a **separate** mmap/arena/reader. It never borrows the live FIFO, cursors,
or engine. Merged sample timestamps retain the reader's tempo-anchor arithmetic.
Note-on/off pairs are matched FIFO per (engine track, key), including retriggers
and velocity-zero offs. Notes left open at EOF close at the last channel sample.
The note list and musical duration stop at the 300-second cap; duration excludes
the playback tail. The ready, immutable index is then published on the main actor.
Index construction allocates its display arrays, never audio-thread storage.

`PianoRollView` draws one subtly track-colored rectangle per note across the
musical duration, with pitch 127 at the top and 0 at the bottom. A 20 Hz main-run-loop
timer polls the atomic playback clock; it is invalidated on hide/detach/deinit.
The monospaced elapsed/total label and playhead clamp to musical duration during
the final audio tail. The default preview strip is 480×160. The roll accepts the
first mouse click even when unfocused and seeks immediately on mouse-down.
Dragging keeps the playhead under the pointer and coalesces targets until they
move at least 0.25 seconds or 100 ms has elapsed; the polling timer also flushes
small pending moves. Main-actor synchronous commits cannot overlap. Mouse-up
always commits the final exact position.

Running `LivePlayer.seek(toSample:)` calls keep AVAudioEngine and the same source
node running and attached: no stop/detach/attach/prepare/start cycle. A single
atomic gate encodes muted and callback-active bits. The callback CAS-enters only
when unmuted, otherwise writes silence, and preserves a mute request on exit.
Main mutes, cancels/joins the producer, and yields in 100-microsecond increments
until the old consumer exits. Only then may reset/chase touch shared state.
`StreamEngineCore.seek(toSample:)` uses `m4a_engine_reset`: the C implementation
destroys/recreates every driver, hardware, voice, and reverb allocation, so no
old voices or reverb survive. It restores the bank/defaults/solo-piano priming,
uses lightweight `MidiStreamReader.rewind()` (cached prepass/solo-track metadata,
cursor/tempo reset only), resets FIFO indices and all clocks/flags, and chases events
strictly before the target. Tempo, program, CC, and pitch-bend state are dispatched
(still honoring solo piano); historical note-on/off events are discarded.
**Notes straddling the target stay silent.** Future notes start normally.
Targets beyond EOF or at the absolute cap finish and pull silence; positions
are capped at 300 seconds. Main synchronously prefills once, restarts production,
then ungates the existing callback; no lock or wait runs in the audio callback.
Only an already stopped preview (focus loss, finish, or replacement) takes the
approved cold path: load/reuse a session, seek/prefill, and perform a real initial
engine prepare/start. Thus first-click scrubbing can restart a stopped preview,
but merely regaining focus still never resumes playback.

The offline renderer remains on the same pull interface and its unseeked output
remains byte-identical. Headless tests cover exact indexed notes/tempo/retriggers,
open-note/cap closure, forward state chase, exact-zero backward-seek ghost checks,
beyond-end silence, the solo-piano rule, reader-rewind equivalence, stable source
identity/running engine across seeks, and a generous 250 ms core-seek latency budget.


The source supplies 44100 Hz stereo float to AVAudioEngine's mixer, which adapts
to the output device format. After EOF plus tail (or the absolute cap), pulls
produce silence. A main-run-loop timer observes first-block/finished flags and
stops about 0.5 seconds after finish. View disappearance and deinit also stop
idempotently: stop audio callbacks, all-sound-off, cancel/join the producer, release.
The roll view also observes hide/detach directly, since Quick Look may embed it
without forwarding NSViewController appearance callbacks.
Those main-actor callbacks invoke `LivePlayer.stop()` synchronously: audio engine
stop and all-sound-off complete in the same call stack before the hook returns.
There is no asynchronous dispatch between view disappearance and audio silence.
The view also observes `NSWindow.didResignKeyNotification` for its own attached
window. Losing focus synchronously stops playback even while the preview remains
open. The observer follows window changes and is removed on detach and deinit;
other windows' notifications cannot stop this preview. Becoming key again never
resumes audio: re-invoke the preview or explicitly scrub to restart it.
Finder can also retain old preview controllers/views across selection changes.
A main-actor weak active-player slot therefore stops the previous player before
starting its replacement: exactly one preview per extension process is audible.
Hide/detach remains the primary teardown; the slot is the retained-view backstop.
A stale controller's later stop clears the slot only when it still owns it.
The sole small unchecked Sendable transfer owns this SPSC core; its safety relies
on these fixed roles and atomic handoffs, not a lock on the audio thread.

`test_stream` uses Swift Testing and covers tempo-anchor arithmetic, stable merging,
running status/velocity-zero translation, skipped payloads, track limits, FIFO
wrap/full/EOF behavior, sequential non-silent renders and object release, RIFF
sizes/PCM peak, dense same-tick bursts, EOT/trailing-tempo padding, duration caps,
and malformed-input cleanup. It also independently writes WAV from planar pulls
(synthetic chunk sizes 1/512/4096; real MIDI 512), verifies post-finish silence,
and checks `mus_shop.mid` against SHA-256
`8069a8327c15e8619d20634e770b6c6faaf09eeec6ed5993caf37c4a5167a66c`
(3,002,245 frames). CTest supplies `PQL_TEST_MIDI`; absent fixtures explicitly skip.
The live structural tests really start/seek/stop AVAudioEngine and verify successive
players replace their predecessors without stale stops losing active ownership.
They explicitly log a skip if the environment cannot start an output device.

The installed SDK's `QuickLookUI.framework/Headers/QLPreviewingController.h:20–30`
specifies that view-based previews implement `preparePreviewOfFile(at:)` on their
view controller (the file callback is declared at line 64). The principal class
is exactly `@objc(PoryaaaaQuickLookController)`, an NSViewController hosting the
piano-roll strip. It uses async preparation to map/validate MIDI off the main actor,
then starts playback before indexing; errors bridge to the SDK completion's NSError.
`QLIsDataBasedPreview` is absent. No temporary WAV or system audio player is used.

The MIDI content type is `public.midi-audio`, empirically checked with:

```sh
mdls -name kMDItemContentType packages/ccomidi/mus_shop.mid
```

The host advertises Viewer/Alternate document handling, embeds the extension in
`Contents/PlugIns`, and is ad-hoc signed after its sandboxed extension. To install
for Finder discovery, copy `build/PoryaaaaQuickLook.app` into `/Applications`, then
check `pluginkit -m -A -D -p com.apple.quicklook.preview`. The standalone build-tree
extension is also signed. The host adds no custom controls; the preview roll supports seeking.

```sh
ditto packages/poryaaaa-quicklook/build/PoryaaaaQuickLook.app /Applications/PoryaaaaQuickLook.app
pluginkit -a /Applications/PoryaaaaQuickLook.app/Contents/PlugIns/PoryaaaaPreview.appex
qlmanage -p -c public.midi-audio packages/ccomidi/mus_shop.mid
log show --last 2m --style compact --predicate 'subsystem == "com.poryaaaa.quicklook"'
```

Playback logs `started` (file and device format), `first rendered block`,
`finished`, `stopped` (rendered sample count), and `seek` (target sample/seconds and
`elapsedMs` from seek entry to ungate, including cold start when required).
Index readiness logs note count, musical duration, and build timing. All logging
stays outside the render callback.
Headless tests and logs establish playback progress, not audibility: only the
user's ear in Finder can verify that the intended sound is actually heard.
Terminating `qlmanage` can kill its extension before Cocoa disappearance/deinit
callbacks run, so that harness shutdown need not emit `stopped`; process death
also destroys CoreAudio output. Check natural completion and view disappearance
separately for graceful stop evidence rather than claiming a termination log.

## Wire format v1

All offsets are absolute from the beginning of the blob. All integers are
little-endian, encoded field by field. Byte arrays and samples are copied raw.
Programmable waves are copied as four raw `uint32_t` words (16 bytes); the baker
checks host byte order and rejects big-endian hosts before loading the project.
The runtime decodes all integers, including programmable words, explicitly.

The 112-byte header consists of:

| Byte offset | Field |
| --- | --- |
| 0 | Eight-byte magic `PQLBANK1` (no terminator) |
| 8 | `uint32 version = 1` |
| 12 | `uint32 voiceCount = 128` |
| 16 | `uint32 subGroupCount` |
| 20 | `uint32 waveCount` |
| 24 | `uint32 progWaveCount` |
| 28 | `uint32 keySplitTableCount` |
| 32 | Five descriptors: each `uint64 offset`, `uint64 byteSize` |

Descriptors and sections are ordered: voices, subGroups, waves, progWaves,
keySplitTables. Every section starts at an eight-byte-aligned offset, after the
header and the preceding section. Empty sections have zero size; they may share
an offset with the next section. Alignment padding is not part of the preceding
section, except padding between sample payloads inside the waves section.

### ToneDataBlob (32 bytes)

| Byte offset | Field |
| --- | --- |
| 0–3 | `uint8 type, key, length, panSweep` |
| 4–7 | `uint8 attack, decay, sustain, release` |
| 8 | `uint64 ptr0` |
| 16 | `uint64 ptr1` (zero or keysplit-table slot offset) |
| 24 | `uint8 ptr0Kind` |
| 25–31 | Seven reserved zero bytes |

**Approved clarification to the initial format brief:** byte 24 is a tagged
pointer kind rather than an entirely zero `uint64 reserved`. Existing engine
square and noise voices store scalar duty/period values in the pointer union.
The tag preserves those values without guessing meaning from voice type:

| Kind | Meaning / validation |
| --- | --- |
| 0 | NULL; `ptr0` must be zero |
| 1 | Scalar pseudo-pointer; `0 < ptr0 < 4096`; restored verbatim via `uintptr_t` |
| 2 | Wave; offset of a registered 24-byte WaveDataBlob header |
| 3 | Programmable wave; offset of a 16-byte slot in progWaves |
| 4 | Subgroup; offset of a full 128-tone array in subGroups |

Baker classification order is NULL, scalar below 4096, identity lookup in wave
headers, programmable waves, then subgroups. A non-NULL real pointer absent from
these registered arrays is a hard error. `ptr1` is independently mapped against
registered keysplit tables; unmapped non-NULL tables are also hard errors.
This layout remains version 1 for this package's initial generated asset; it is
not interchangeable with an untagged interpretation of the original brief.

The voices section is 128 tones (4096 bytes). The subGroups section contains
`subGroupCount` arrays of 128 tones each, subgroup-major, with 4096-byte stride.

### WaveDataBlob (24 bytes) and samples

| Byte offset | Field |
| --- | --- |
| 0 | `uint16 type` |
| 2 | `uint16 status` |
| 4 | `uint32 freq` |
| 8 | `uint32 loopStart` |
| 12 | `uint32 size` (sample byte count) |
| 16 | `uint64 dataOffset` |

The waves section begins with `waveCount` contiguous headers, then sample payloads
in wave index order, each starting on an eight-byte boundary. Every payload must
lie entirely inside the waves section, after its header array. The remaining
sections contain `progWaveCount * 16` raw bytes and `keySplitTableCount * 128`
raw bytes respectively.

## Runtime API and ownership

Include `poryaaaa_quicklook_bank.h` and link `poryaaaa_quicklook_bank` (which links
`poryaaaa::engine`).

- `PqlBank* pql_bank_load(const uint8_t* blob, size_t size)` validates the complete
  layout, resource counts, reference tags, object-slot boundaries, sample ranges,
  and reserved bytes before fixing anything up. It returns NULL on invalid input
  or allocation failure. Arithmetic uses subtraction-based bounds checks to
  avoid overflow. The caller's bytes are never modified.
- `ToneData* pql_bank_voices(PqlBank*)` returns the bank's 128 borrowed engine-ready
  voices, or NULL for a NULL handle. Pass these to `m4a_engine_set_voicegroup`.
- `void pql_bank_free(PqlBank*)` frees the backing buffer and handle; NULL is safe.

There are exactly two runtime allocations: the opaque handle and one owned copy
of the blob containing every native tone, wave header, programmable wave, table,
and sample. Native structs are constructed field by field inside their wire
slots; structure sizes and alignment are statically checked to fit. The caller
may discard the input immediately after load. Keep the bank alive until all
engines referencing its voices and active samples have stopped.

## Test coverage and loader parity

The blob-only test reads the generated asset without the source project, checks
DirectSound/programmable/keysplit resources, destroys its input copy to prove
ownership, and renders eight 2048-frame stereo blocks with peak above 0.01.
Malformed header, truncated sections, overlap, reference/tag/granularity, reserved
byte, and sample-range cases must be rejected.

With `PQL_HEARTH_ROOT` set, the test also calls the existing loader directly,
compares all reachable tone fields/resource payloads, then compares 32768 stereo
frames sample-exactly across two engines playing the same DirectSound note and
slot-zero key 36 sequence. Without the environment variable, parity prints an
explicit skip message while blob-only checks still run.

**Approved test correction:** hanabi slot 0 (`VOICE_KEYSPLIT_ALL`) has a subgroup
but a NULL keysplit table, exactly as the existing loader produces. Ordinary
`VOICE_KEYSPLIT` slots 1–6 must have both subgroup and table resolved. Fabricating
a slot-zero table would violate source-loader parity.
