# Pushback v1: mGBA-Faithful Audio Scaffold Review

## Position

The proposed `plugin/m4a/` and `plugin/hw_audio/` split is the right high-level boundary. Real m4a is software running on the GBA CPU: it walks music state, computes envelope/pan/pitch, mixes DirectSound PCM, applies software reverb, and writes audio registers / FIFO data. mGBA's side is the hardware model: PSG state, FIFO drain/sample hold, SOUNDCNT routing and scaling, SOUNDBIAS timing/clipping, and final output sampling.

My concern is not the split. My concern is that the current scaffold contract is still too snapshot-oriented to be a strong foundation for "sounds like mGBA." mGBA is write-timing-sensitive: it advances/samples audio before many register writes, then applies those writes in order. A once-per-vblank register snapshot with trigger booleans loses information that can be audible.

## Concerns

### 1. Register snapshots are too coarse for mGBA parity

The planned `M4ARegisterFile` gives `hw_audio` the latest state plus trigger flags. mGBA does not work from only the latest state. In `gba_audio.c`, most sound register writes call `GBAAudioSample(...)` immediately before mutating PSG/audio state, then forward the write into `gb_audio.c` routines. That means the output is segmented by write times.

For example, writes to SOUND1/SOUND2/SOUND3/SOUND4, SOUNDCNT_L, SOUNDCNT_X, and SOUNDBIAS all sample before applying the write. Wave RAM writes call `GBAudioRun` before mutating wave RAM. This ordering is core to how mGBA preserves phase and discontinuities around register changes.

Recommendation: do not make the long-term API only "latest register state." The scaffold should leave room for ordered register-write events with offsets inside the render window. If that is too much for the first pass, document that the snapshot contract is provisional and must be replaced or extended before PSG parity work is considered complete.

### 2. Trigger booleans are underpowered

`trigger_sq1`, `trigger_sq2`, `trigger_wave`, and `trigger_noise` collapse write events into one bit per channel. That loses:

- whether more than one trigger happened in a render span,
- whether a trigger happened before or after a frequency/duty/envelope write,
- the sample offset of the restart,
- the difference between an NRx4 restart and other relevant write edges such as NR30 DAC toggles.

This matters because mGBA restart handlers reset or reload hardware state. Square/noise restart affects phase/LFSR/envelope/length behavior. Wave behavior depends heavily on DAC enable, RAM writes, bank state, and restart order.

Recommendation: model triggers as ordered write events, not state booleans. At minimum, replace booleans with a small event queue or an event bit plus a sample/frame offset and documented write order.

### 3. `M4APcmRing` is software DMA-buffer output, not the hardware FIFO

The plan correctly moves DirectSound voice mixing into `plugin/m4a/`, but the naming and contract still risk conflating the m4a PCM DMA buffer with the GBA FIFO hardware.

Real m4a writes mixed/clamped bytes into its PCM DMA buffer. Hardware DMA then writes words into FIFO A/B. mGBA models FIFO A/B as small word FIFOs with read/write cursors, an `internalSample`, `internalRemaining`, byte shifting, and SOUNDBIAS-paced output samples.

Recommendation: keep `M4APcmRing` as the driver-side m4a DMA buffer, but state explicitly that `hw_audio` owns a separate FIFO/DMA adapter stage:

- consume the m4a ring as DMA source,
- write FIFO A/B words in the same channel/routing order the game config uses,
- drain bytes according to timer/SOUNDBIAS behavior,
- preserve FIFO sample-hold state across host render calls.

Do not treat `ring_a` / `ring_b` as already being the hardware FIFO output.

### 4. SOUNDBIAS must be a first-class timing source

The scaffold includes `bias_sampling_cycle`, which is good, but the plan should make this central rather than incidental. In mGBA, SOUNDBIAS resolution changes `audio->sampleInterval`; `GBAAudioSample` uses that interval to decide how many hardware samples to generate, and FIFO channel sample arrays are filled relative to that timing.

mGBA 0.10 also calls out dynamic audio-rate changes and FIFO/PSG sampling at SOUNDBIAS-controlled frequency as meaningful audio fixes. If poryaaaa wants mGBA-like output, SOUNDBIAS is not just an output clip/bias field. It defines the internal hardware sample cadence.

Recommendation: `hw_audio` should make SOUNDBIAS resolution the owner of internal hardware sample rate. If the implementation chooses a fixed high internal rate like 131072 Hz, it still needs an explicit compatibility argument for how 32768/65536/131072/262144 Hz SOUNDBIAS modes are reproduced.

### 5. Noise coalescing appears version/style-sensitive

The current poryaaaa v1 comments refer to mGBA-style noise coalescing. In the local mGBA reference I reviewed, `GBAudioSamplePSG` uses `_coalesceNoiseChannel` for non-GBA GB style, but for `GB_AUDIO_GBA` it uses the current noise sample shifted. That means "mGBA-style sub-sample averaging" may not be correct for GBA mode, or it may depend on the exact mGBA revision being targeted.

Recommendation: before carrying this behavior into `hw_audio`, verify against the exact mGBA source revision and output path poryaaaa is trying to match. If mGBA GBA mode does not coalesce noise in the target revision, poryaaaa should not preserve v1's coalescing just because it sounded subjectively closer in one capture.

### 6. Sweep ownership is right, but conflicts with snapshot overwrite

`Thoughts and Concerns.md` correctly flags that NR10 sweep is hardware-ticked. The updated plan says the chip owns sweep state, which is the right direction. But a register snapshot with `sq1_freq` always present can accidentally overwrite chip-owned sweep results every vblank.

Recommendation: the driver should only emit a frequency write event when m4a actually writes NR13/NR14. The chip should then own subsequent sweep frequency mutation until the next real write/restart. This is another reason an event stream is safer than a continuously authoritative snapshot.

### 7. Wave declick should be tied to hardware write edges, not note semantics

The plan is right to reject v1's software fade-out declick as not hardware-faithful. But "wave declick is in hardware" is only useful if the chip sees the actual NR30 DAC toggles, wave RAM writes, and restart order. If the driver only communicates `wave_dac_on` as a current bool, the chip may miss the edge that creates the discontinuity.

Recommendation: represent NR30 off/on and wave RAM writes as ordered events. Declick/click behavior should be a consequence of those events, not a note-off path or a synthetic fade.

### 8. m4a fidelity also means preserving write order inside `CgbSound`

The plan says driver-side code should mirror real m4a routines, which is correct. For audio parity, this needs to include the order in which `CgbSound` writes NRx registers, not just the final computed values. If the driver computes final fields and emits one register snapshot, it can still be "m4a-like" structurally while failing mGBA-like hardware behavior.

Recommendation: when porting `m4a_cgb_sound`, emit write events in the same order as m4a writes registers. Let `hw_audio` consume those writes like mGBA consumes MMIO writes.

## Suggested Contract Adjustment

Keep the scaffold simple, but define the long-term contract as:

- `plugin/m4a/` owns m4a state and produces:
  - driver-side PCM DMA buffer/ring updates,
  - ordered audio register/FIFO write events with offsets relative to the current render span,
  - a current register snapshot for debugging/tests only.
- `plugin/hw_audio/` owns:
  - PSG phase/envelope/sweep/length/LFSR/wave-RAM hardware state,
  - FIFO A/B state and sample hold,
  - SOUNDCNT routing/scaling,
  - SOUNDBIAS timing/clip,
  - output resampling to host rate.

If a snapshot remains in the public API, document it as a cache of current state, not the authoritative timing interface.

## Test Expectations

Before accepting "sounds like mGBA" as achieved, add parity tests or scripts that compare against mGBA captures:

- PSG-only square, wave, and noise renders at 32768 Hz and 65536 Hz SOUNDBIAS modes.
- DirectSound-only FIFO sample-hold tests with known byte patterns.
- Mixed PSG + DirectSound routing tests for SOUNDCNT_L/SOUNDCNT_H.
- Register-edge tests: NRx4 retrigger timing, NR30 DAC toggle, wave RAM rewrite with DAC off, SOUNDBIAS rate change.
- Real song/capture comparisons only after the low-level tests pass.

Subjective song matching is useful, but it should not be the first line of defense; too many wrong implementations can sound close on one song and fail on another.

## Sources Reviewed

- `HW_AUDIO_SCAFFOLD_PLAN.md`
- `Thoughts and Concerns.md`
- `plugin/m4a_engine.c`
- `plugin/m4a_channel.c`
- Local mGBA reference files:
  - `/tmp/mgba-ref/gba_audio.c`
  - `/tmp/mgba-ref/gb_audio.c`
- mGBA upstream source:
  - `src/gba/audio.c` in https://github.com/mgba-emu/mgba
  - `src/gb/audio.c` in https://github.com/mgba-emu/mgba
- mGBA 0.10.0 release/audio notes:
  - https://mgba.io/2022/10/11/mgba-0.10.0/

## Bottom Line

The module split is sound. The author should keep it. But for mGBA faithfulness, the driver-to-chip boundary should be based on ordered audio writes and hardware-owned timing/state, not only on vblank snapshots plus trigger flags. Otherwise the rewrite may be cleaner than v1 while still missing the behaviors that make mGBA sound the way it does.
