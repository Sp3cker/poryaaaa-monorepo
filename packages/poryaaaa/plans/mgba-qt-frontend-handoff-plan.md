# Installed mGBA 0.10.5 Host-Facing Sound Handoff Plan

## Decision and claim boundary

Replace the obsolete current-mGBA resampler plan with one implementation of the
**installed mGBA 0.10.5 Qt post-native audio path**. The production module keeps
the existing `HwResample` name, but it becomes a deliberately small-interface,
deep module: `HwAudio` submits native PCM16 deltas and reads paired PCM16; no
host learns about phase, watermarks, blip storage, or feedback.

This work is a **Frontend** implementation and diagnostic claim only. It must
continue to obey the four claim boundaries in
[`docs/arch-parity-fix-plan.md`](../docs/arch-parity-fix-plan.md):

| Claim | This plan may establish | This plan does not establish |
| --- | --- | --- |
| Frontend | Exact 0.10.5 post-native PCM16 behavior and a bounded live host-output diagnostic. | Hardware mix, PSG/DirectSound, timer, ROM, or driver correctness. |
| Hardware | Nothing. Do not change a native-trace hash or replay result to make this pass. | End-to-end frontend behavior. |
| Driver | Nothing. Do not use lifecycle traces as a frontend oracle. | Native instruction-cycle equivalence. |
| Oracle / Renderer | Nothing. The existing pinned observation oracle and renderer profile retain their own contracts. | Installed-application playback behavior. |

A native/replay hash movement while making this change is a regression to stop
and investigate, not a frontend success. A live waveform capture is a
**diagnostic only**: it must be labeled with its exact local window, device,
settings, and alignment and must not be promoted to Hardware, Driver, Oracle,
or Renderer evidence.

## Exact target provenance

The source specification is the untouched upstream mGBA tag `0.10.5`, commit
`26b7884bc25a5933960f3cdcd98bac1ae14d42e2`, checked out at:

```text
/Users/spencer/dev/cProjects/mgba-0.10.5-reference
```

Use this checkout only for the independent frontend reference target below. It
is not the observation-patched mGBA checkout used by the native oracle. The
relevant source files are:

- `src/gba/audio.c` — post-mix delta submission, `0x800`-clock frame accounting,
  `audioBuffers` producer gate, SOUNDBIAS cadence, and the upstream native
  master-volume conversion.
- `src/platform/qt/AudioDevice.cpp` and `AudioProcessorQt.cpp` — Qt effective
  output rate, paired read, and interleaved signed PCM16 handoff.
- `src/third-party/blip_buf/blip_buf.c` and `include/mgba/core/blip_buf.h` —
  the exact old blip frontend, including its 64-bit branch, coefficient table,
  interpolation, integrator, feedback, and buffer rules.
- `src/core/config.c`, `src/platform/qt/ConfigController.cpp`,
  `SettingsView.cpp`, and `CoreController.cpp` — configuration provenance.

The installed comparison application is:

```text
/Applications/mGBA.app/Contents/MacOS/mGBA
bundle id: com.endrift.mgba-qt
version: 0.10.5
```

Before any live comparison, record the application binary SHA-256, bundle
version, selected CoreAudio device name/rate/layout, and the following effective
configuration values from **`~/.config/mgba/config.ini`**, section `[ports.qt]`:

```ini
sampleRate=48000
audioBuffers=1536
volume=102
mute=0
fpsTarget=59.72750056960583
```

`qt.ini` is not the effective core configuration file for this comparison. Do
not silently substitute a value found there. The Qt requested rate is 48000 Hz;
the actual selected CoreAudio stream rate remains a runtime fact to record, not
an assumption.

### Volume is upstream hardware-side state

The installed `volume=102` with `mute=0` sets mGBA's native
`GBAAudio.masterVolume` before the seam this plan implements. In
`src/gba/audio.c`, `_applyBias` bias-clamps the hardware mix then evaluates:

```text
((clampedSample - SOUNDBIAS.bias) * masterVolume * 3) >> 4
```

That is already native PCM16. It is **not** a blip coefficient, frontend gain,
float scale, or DAW-volume calculation. Live comparisons must control and
record `volume=102`, normal speed, and `mute=0`; frontend unit vectors start
from supplied native PCM16 and must never bake `102` into their oracle or
implementation. Fast-forward volume/mute and a mute transition are out of
scope.

At the configured `fpsTarget`, mGBA's float `fauxClock` is `1.0f`; the expected
48 kHz rate factor is `12884901888000`. Preserve the float calculation anyway:
the equality is a property of this configuration, not an excuse to simplify the
code.

## Scope after native PCM16

The source-exact portion starts immediately after the hardware mixer has
produced its signed, bias-adjusted, master-volume-adjusted native stereo PCM16.
In poryaaaa that is `hw_mix_render()` in
`plugin/hw_audio/hw_mix.c`. This file is an input-boundary anchor; it is not
part of this implementation change unless its output type stops being signed
PCM16.

```text
hardware PSG / FIFO / SOUNDCNT / SOUNDBIAS / masterVolume
                         │
                         │ signed native PCM16 L,R at DAC timestamps
                         ▼
        source-exact mGBA 0.10.5 blip frontend (this plan)
                         │
                         │ paired signed PCM16 L,R
                         ▼
      necessary fixed-block DAW adaptation in HwAudio (this plan)
                         │
                         │ planar host float: pcm16 / 32768.0f
                         ▼
                CLAP, M4AEngine, porydaw, renderer, M4L
```

### Explicit non-goals

- Do not change PSG, wave, noise, FIFO, timer, SOUNDCNT, SOUNDBIAS mix
  arithmetic, `pcmMixRate`, reverb, M4A driver scheduling, or native trace
  tooling.
- Do not copy Qt's `usleep(100)`, short `QIODevice::readData()` return,
  `QAudioSink` device-open policy, device selection, or its fixed startup
  request rate into a realtime fixed-block callback.
- Do not implement FPS stretching as a DAW policy. The source rate calculation
  contains Qt's `fauxClock` only because the exact source behavior requires it;
  a DAW never changes it to follow a transport/video FPS target.
- Do not add a host-local resampler, a QIODevice analogue, gain stage, EQ,
  analog IIR, DC blocker, normalizer, mono fold, tolerance comparison, or
  latency search.
- Do not use libretro's optional low-pass behavior; it is not on this Qt path.
- Do not use installed mGBA as the native hardware oracle, modify native
  manifests, or commit live PCM/WAV/recordings.
- Do not make the optional anti-alias feature a default behavior or call it
  parity. It is isolated below.

## Source-ordered behavior to implement

This section is normative. Preserve operation order; matching an impulse,
windowed-sinc shape, or approximate frequency response is insufficient.

### 1. Native PCM16 becomes paired deltas at a shared timestamp

`HwAudio::render_dac_sample()` continues to obtain `int16_t native_l` and
`native_r` from `hw_mix_render()`. It calls the exact frontend submit operation
with the current DAC period in GBA clocks.

1. Consult **only** the left blip's availability. If
   `left.available >= audio_buffers`, reject the entire stereo input. A rejected
   input changes no table cells, neither last-PCM history, the shared clock, nor
   either channel's availability.
2. On an accepted input, calculate independent signed deltas
   `native_l - last_l` and `native_r - last_r`; deposit both at the same current
   `clock`; then store both new last values.
3. Advance the one shared `clock` by the DAC period.
4. If `clock >= 0x800`, call end-frame on left then right with exactly `0x800`,
   then subtract one `0x800`. Do not use a loop: one subtraction is the source
   behavior for the permitted SOUNDBIAS periods.

The poryaaaa normal sampling-cycle-1 cadence is 256 GBA clocks. Explicit
SOUNDBIAS test transitions use periods 512 (cycle 0), 128 (cycle 2), and 64
(cycle 3). mGBA's reset default 512 clocks does not authorize changing
poryaaaa's native default.

### 2. Old blip fixed timing and rate factor

The installed arm64 source takes blip_buf's 64-bit branch. Implement the same
portable fixed-width form, not `unsigned long`:

```text
pre_shift = 32
time_bits = 52
time_unit = 1ULL << 52
frac_bits = 20
phase_bits = 5
phase_count = 32
half_width = 8
delta_bits = 15
bass_shift = 9
feedback_shift = delta_bits - bass_shift = 6
frame_clocks = 0x800
storage_samples = 0x4000
buffer_extra = half_width * 2 + 2 = 18
```

For each rate change, reproduce Qt plus `blip_set_rates` in this order:

```c
float faux = 1.0f * (float)16777216 /
              ((float)280896 * fps_target * 1.0f);
double effective_output_rate = (double)host_rate_hz * (double)faux;
double raw_factor = (double)(UINT64_C(1) << 52) * effective_output_rate /
                    (double)16777216;
uint64_t factor = (uint64_t)raw_factor;
if (factor < raw_factor)
    ++factor;                 /* source ceiling rule */
```

Assign `factor` to both channels only. Do not reset or alter `offset`,
`available`, `integrator`, shared delta `clock`, last PCM histories, or optional
AA history during a normal host-rate change. The `uint32_t host_rate_hz` accepted
by `HwAudio` is the one and only host-rate representation: use that exact value
for `host_frames_through_cycle`, deadline mapping, and this factor calculation.
Do not keep a separately rounded integer deadline rate beside a float or double
resampler rate. Reject a non-finite, non-positive, non-integral host request at
the public `float` boundary rather than rounding it. Validate the resulting
factor and `0x800 * factor + offset` before storing state; the admitted domain
must prove `audio_buffers + one_frame_growth + 16 <= 0x4000 + 18`, so a release
build cannot write outside blip storage. Runtime bounds checks remain active in
release and fail the internal operation before any cell write; they must not
clamp, wrap, fabricate audio, or proceed after an out-of-domain request.

#### Settled rate and storage contracts

`HwResample` stores the admitted host rate as one integral `uint32_t`; no
second rounded deadline rate is maintained. The production ceiling check uses
the faux-clock ratio (including a deliberately non-integral probe), then
validates the resulting factor and storage bound before state is committed.
This is a release-safety contract, not permission to round an invalid public
request.

The host adapter is bounded and no-drop: it drains available paired PCM16 into
fixed caller blocks, carries a bounded remainder between calls, starts muted,
and holds the last chronological frame only when the frontend is temporarily
short. It must never discard a native DAC observation to satisfy a host block
size.

### 3. Exact 33 × 8 coefficient table

Transcribe these rows verbatim to one flat `static const int16_t
bl_step[33 * 8]`. A flat table makes source's intentional cross-row lookups
well-defined in C. Do not synthesize coefficients at runtime or substitute a
Nuttall/window/sinc generator. Row 32 is required for the phase-zero reverse
lookup.

```text
{ 43,-115, 350,-488,1136,-914,5861,21022}
{ 44,-118, 348,-473,1076,-799,5274,21001}
{ 45,-121, 344,-454,1011,-677,4706,20936}
{ 46,-122, 336,-431, 942,-549,4156,20829}
{ 47,-123, 327,-404, 868,-418,3629,20679}
{ 47,-122, 316,-375, 792,-285,3124,20488}
{ 47,-120, 303,-344, 714,-151,2644,20256}
{ 46,-117, 289,-310, 634, -17,2188,19985}
{ 46,-114, 273,-275, 553, 117,1758,19675}
{ 44,-108, 255,-237, 471, 247,1356,19327}
{ 43,-103, 237,-199, 390, 373, 981,18944}
{ 42, -98, 218,-160, 310, 495, 633,18527}
{ 40, -91, 198,-121, 231, 611, 314,18078}
{ 38, -84, 178, -81, 153, 722,  22,17599}
{ 36, -76, 157, -43,  80, 824,-241,17092}
{ 34, -68, 135,  -3,   8, 919,-476,16558}
{ 32, -61, 115,  34, -60,1006,-683,16001}
{ 29, -52,  94,  70,-123,1083,-862,15422}
{ 27, -44,  73, 106,-184,1152,-1015,14824}
{ 25, -36,  53, 139,-239,1211,-1142,14210}
{ 22, -27,  34, 170,-290,1261,-1244,13582}
{ 20, -20,  16, 199,-335,1301,-1322,12942}
{ 18, -12,  -3, 226,-375,1331,-1376,12293}
{ 15,  -4, -19, 250,-410,1351,-1408,11638}
{ 13,   3, -35, 272,-439,1361,-1419,10979}
{ 11,   9, -49, 292,-464,1362,-1410,10319}
{  9,  16, -63, 309,-483,1354,-1383, 9660}
{  7,  22, -75, 322,-496,1337,-1339, 9005}
{  6,  26, -85, 333,-504,1312,-1280, 8355}
{  4,  31, -94, 341,-507,1278,-1205, 7713}
{  3,  35,-102, 347,-506,1238,-1119, 7082}
{  1,  40,-110, 350,-499,1190,-1021, 6464}
{  0,  43,-115, 350,-488,1136, -914, 5861}
```

### 4. Delta deposition and interpolation

For one accepted delta and one channel:

```text
fixed32 = (uint32_t)((time * factor + offset) >> 32)
out_index = available + (fixed32 >> 20)
phase = (fixed32 >> 15) & 31
interp = fixed32 & 32767
delta2 = arithmetic_floor_shift(delta * interp, 15)
delta -= delta2
```

Use `row = &bl_step[phase * 8]` and `reverse = &bl_step[(32 - phase) * 8]`.
Deposit sixteen cells in this exact order:

- cells 0 through 7 use `row[i] * delta + row[8 + i] * delta2`;
- cells 8 through 15 use `reverse[7 - i] * delta + reverse[7 - i - 8] *
  delta2` for `i = 0..7`.

The `row[8 + i]` and negative reverse offsets deliberately span neighboring
flat rows exactly as blip_buf does. Require the source bound
`out <= storage_samples + 2` before the 16 writes. Use `int64_t` for a
coefficient product and prospective cell sum, check that its result fits the
stored `int32_t`, then store it. Implement a portable arithmetic-floor right
shift for negative values and encode the source signed-16 clamp explicitly;
never depend on implementation-defined signed shifts or replace them with
saturation/wrapping behavior.

### 5. End frame, paired read, clamp, integrator, and float conversion

End frame separately on each channel:

```text
off = 0x800 * factor + offset
available += off >> 52
offset = off & ((1ULL << 52) - 1)
assert available <= 0x4000
```

A paired read requests `n = min(max_frames, left.available, right.available)`.
For every requested output frame and separately for left and right, perform the
following exact order:

1. `s = arithmetic_floor_shift(integrator, 15)`.
2. Add the current buffer cell to `integrator`.
3. Apply blip_buf's `CLAMP`: if `s` is not exactly representable as signed
   16-bit, set `s = arithmetic_floor_shift(s, 16) ^ 32767`. This is not a
   conventional saturating cast.
4. Write signed PCM16 `s`.
5. Apply feedback after the write: `integrator -= s << 6`.

Then shift `available + 18 - n` cells to the front, zero the final `n` cells,
and reduce availability by `n`, independently for left and right. `Qt` writes
left with stride two, then right with stride two; planar production output is
equivalent only if both channel read counts are equal and every packed test
comparison uses `L0,R0,L1,R1,...`.

Only after this exact PCM16 read does `HwAudio` convert each output to float:

```c
out_l[i] = (float)pcm_l[i] * (1.0f / 32768.0f);
out_r[i] = (float)pcm_r[i] * (1.0f / 32768.0f);
```

There is no pre-read float filter, float interpolation, float feedback, or
float-to-PCM round-trip on the shipped path.

## Production module contract and state

### Files to edit

| File | Required change |
| --- | --- |
| `packages/poryaaaa/plugin/hw_audio/hw_resample.h` | Replace the generic floating-stream declaration with the exact state and PCM16 interface below. |
| `packages/poryaaaa/plugin/hw_audio/hw_resample.c` | Replace the current approximate double/Nuttall streaming implementation with the source-ordered fixed-point implementation above. |
| `packages/poryaaaa/plugin/hw_audio/hw_audio.c` | Own DAC submission, bounded PCM16 scratch drains, fixed-block adaptation, reset/rate/SOUNDBIAS behavior, and the sole frontend call sites. |
| `packages/poryaaaa/plugin/hw_audio/hw_audio.h` | Correct the frontend comments and retain only the explicit optional-AA control. |
| `packages/poryaaaa/plugin/m4a_engine.c` | Remove the approximate host float high-pass loop from `m4a_engine_process`; keep it a direct shared-frontend consumer. |
| `packages/poryaaaa/plugin/m4a_engine.h` | Delete obsolete frontend/filter state listed below. Retain non-frontend public driver fields. |
| `packages/poryaaaa/cmd/poryaaaa_render.c` | Route unchanged through the shared output; make its AA option explicitly opt-in/non-parity or remove it with its test if the option is intentionally retired. Do not add a renderer resampler. |
| `packages/poryaaaa/CMakeLists.txt` | Replace the old nested frontend parity target with the independent, clean-source 0.10.5 target below. |
| `packages/poryaaaa/test/test_hw_resample_parity.c` | Replace every old harness test with the exact reference-adapter matrix below. |
| `packages/poryaaaa/test/test_engine.c` | Remove obsolete approximate expectations and make integration invariance strict. |
| `packages/poryaaaa/test/test_porydaw_engine.c` | Replace the legacy float-filter test with the shared-engine equivalence/reset test below. |

Read-only call-site anchors that must be audited but do not need new frontend
logic are:

- `packages/poryaaaa/plugin/m4a_plugin.c` (CLAP activation, reset, and process);
- `packages/poryaaaa/plugin/src/runtime.rs`, `process.rs`, and `plugin.rs`
  (Rust porydaw ownership, scheduling, and planar handoff);
- `packages/poryaaaa-m4l/source/audio/poryaaaa~/poryaaaa~.cpp` (M4L
  double-precision handoff);
- `packages/poryaaaa/plugin/hw_audio/hw_mix.c` (native PCM16 seam input).

### Header-visible exact state

`HwAudio` embeds `HwResample`, so keep state visible in
`hw_resample.h` for the focused reference tests; do not add getters, callbacks,
or a second adapter. The following is the full declaration contract; include
`<stdbool.h>`, `<stdint.h>`, and `<limits.h>` in the header:

```c
enum {
    HW_RESAMPLE_GBA_CLOCK_HZ = 16777216u,
    HW_RESAMPLE_FRAME_CLOCKS = 0x800u,
    HW_RESAMPLE_BLIP_STORAGE_SAMPLES = 0x4000u,
    HW_RESAMPLE_HALF_WIDTH = 8u,
    HW_RESAMPLE_END_FRAME_EXTRA = 2u,
    HW_RESAMPLE_BLIP_BUFFER_EXTRA = 18u,
    HW_RESAMPLE_MAX_AUDIO_BUFFERS = 0x2000u,
    HW_RESAMPLE_AA_TAPS = 49u,
};

typedef struct {
    uint64_t factor;
    uint64_t offset;
    uint32_t available;
    int32_t integrator;
    int32_t samples[HW_RESAMPLE_BLIP_STORAGE_SAMPLES +
                    HW_RESAMPLE_BLIP_BUFFER_EXTRA];
} HwResampleBlip;

typedef struct {
    bool enabled;
    uint32_t input_rate_hz;
    uint32_t output_rate_hz;
    uint32_t newest;
    double coefficients[HW_RESAMPLE_AA_TAPS];
    double history_l[HW_RESAMPLE_AA_TAPS];
    double history_r[HW_RESAMPLE_AA_TAPS];
} HwResampleAntialias;

typedef struct {
    HwResampleBlip left;
    HwResampleBlip right;
    uint64_t clock;
    int16_t last_l;
    int16_t last_r;
    uint32_t audio_buffers;
    uint32_t host_rate_hz;
    float fps_target;
    HwResampleAntialias aa; /* Optional; excluded from the default path. */
} HwResample;

void hw_resample_init(HwResample*, uint32_t host_rate_hz,
                      float fps_target, uint32_t audio_buffers);
void hw_resample_set_antialias_input_rate(HwResample*, uint32_t dac_rate_hz);
void hw_resample_reset(HwResample*);
void hw_resample_set_output_rate(HwResample*, uint32_t host_rate_hz);
void hw_resample_set_audio_buffers(HwResample*, uint32_t audio_buffers);
void hw_resample_submit(HwResample*, int16_t left, int16_t right,
                        uint32_t dac_period_cycles);
uint32_t hw_resample_read_pcm16(HwResample*, int16_t* left, int16_t* right,
                                uint32_t max_frames);
void hw_resample_set_antialias(HwResample*, bool enabled);
```

No render-time allocation is permitted: add two fixed `int16_t` scratch arrays
on `HwAudio` sized to its existing internal drain chunk.

### State invariants and lifecycle rules

- There are two complete blips: independent factor, offset, availability,
  integrator, and 16384+18 cells. The left side alone is the producer gate;
  paired submission/read preserves equal availability in normal operation.
- `audio_buffers=1536` is a producer gate, **not** allocation capacity. Keep
  physical storage at 16384+18. A completed frame can legitimately take
  availability just above 1536.
- Initial construction zeroes the full state, sets both factors, clears each
  blip (`offset = factor / 2`, available/integrator/cells zero), sets clock and
  last PCM values to zero, and leaves AA disabled.
- `hw_resample_reset` clears each blip and sets shared clock to zero but
  deliberately preserves `last_l`/`last_r`, just as the source reset path does.
  It also clears AA history if AA was enabled but preserves that explicit user
  preference.
- `hw_resample_set_audio_buffers` validates/clamps to `0x2000`, stores the new
  gate, clears both blips, and resets the shared clock. It does **not** change
  last PCM histories. The installed setting is 1536.
- `hw_resample_set_output_rate` updates factor only. It does not clear buffers,
  reset phase, or recalculate because SOUNDBIAS changed.
- `HwAudio::reset_frontend` construction initializes with the validated
  `host_rate_hz`, `59.72750056960583f`, and `1536`. `hw_audio_reset` calls
  `hw_resample_reset` and separately clears host adaptation state. The exact
  blip integrators are therefore reset for **every** consumer, including
  renderer and trace hosts; no M4AEngine-local filter state remains.
- `hw_audio_sync_rates_from_mix` and `hw_audio_sync_psg_timing` continue to
  update synthesizer scheduling/cadence, but must not call
  `hw_resample_set_output_rate`. A SOUNDBIAS write changes later delta
  timestamps only; the blip factor remains tied to the 16,777,216 Hz core
  clock. If—and only if—optional AA is enabled, these paths also call the
  separate `hw_resample_set_antialias_input_rate` to recompute optional AA
  coefficients without clearing AA history/cursor or any exact blip state.

### Necessary DAW fixed-block adaptation

Qt may short-read or sleep while its blip buffers have too little data. A CLAP,
M4L, renderer, or porydaw host must return the requested fixed block without
sleeping. Keep this adaptation in `HwAudio`, after exact PCM16 production:

1. `drain_frontend()` reads available PCM16 in bounded scratch chunks and
   converts it to float only after each paired exact read.
2. Before the first real frontend PCM16 frame, write zeros for a requested host
   frame with no available output. This represents host-side startup mute, not
   a fabricated input sample.
3. Once at least one real frame has been emitted, a requested host frame with
   no new frontend output repeats the last emitted left/right float pair.
4. Never feed a source zero merely to force output, change native timestamps,
   force an end frame, or put host-hold state in `HwResample`.

Store `last_host_l`, `last_host_r`, and `have_frontend_sample` in `HwAudio`.
Clear them on `hw_audio_reset`, not on a host-rate update. This is necessary
DAW adaptation, intentionally different from Qt short-read/sleep behavior; the
source-exact claim ends at the PCM16 read.

## Shipped-host cutover

Every shipped caller remains on the one deep `HwAudio` seam:

```text
m4a_advance → m4a_get_pending_writes → hw_audio_render_events → m4a_consume_writes
```

| Consumer | File(s) | Required result |
| --- | --- | --- |
| CLAP C plug-in | `plugin/m4a_plugin.c` | Its activation/reset/process drives the shared `M4ADriver`/`HwAudio` seam directly and returns its host floats. No sink policy, resampler, or filter is added. |
| C `M4AEngine` | `plugin/m4a_engine.c`, `.h` | `m4a_engine_process` remains a direct caller of `hw_audio_render_events`; remove its approximate float high-pass, then expose exactly the shared float block. |
| Rust porydaw path | `plugin/src/runtime.rs`, `process.rs`, `plugin.rs` | Preserve the paired driver/HwAudio ownership, reset/reconfigure order, sample-accurate event scheduling, and planar handoff. Audit that fallback silence is only a pre-init/error path, never a second frontend. |
| Offline renderer | `cmd/poryaaaa_render.c` | Continue to drive `HwAudio` and do final WAV packing only. It must not use a renderer-only rate conversion or default AA. |
| M4L | `../poryaaaa-m4l/source/audio/poryaaaa~/poryaaaa~.cpp` | Keep its direct shared frontend call and float-to-double host copy. No Max-local resampler/filter is permitted. |

For all rows, use `lsp` references before changing an exported declaration and
migrate every call site in the same change. There must be one production caller
of `hw_resample_submit`/`read_pcm16`: `hw_audio.c`.

### Remove obsolete state and behavior cleanly

Delete these fields from `M4AEngine` and all initialization/use sites after LSP
confirms no outside owner:

```text
pcmResampleAccum
pcmPrevL, pcmPrevR
pcmCurL, pcmCurR
lowPassLeft, lowPassRight
legacyHighPassDcLeft, legacyHighPassDcRight
```

Delete the `left/right - legacyHighPassDc...` loop in
`m4a_engine_process`; it is an approximate host float filter and is not the
0.10.5 blip feedback. Keep `pcmMixRate`: it is driver DirectSound geometry,
not a host frontend rate. Keep `analogFilter` only as its existing M4A driver
configuration contract; this plan must not give it a new frontend consumer.

Replace the complete old `HwResample` floating-stream state:

```text
source_rate, destination_rate, timestamp, available,
input_l/input_r, g_sinc_lut, g_window_lut,
aa_state_pos and the old double AA histories/coefficients
```

with the fixed state above. If optional AA is retained, reintroduce only
AA-specific state nested behind `antialias_enabled`; it must not share or alter
the default PCM16 timeline.

### Optional AA isolation

`hw_audio_set_resample_antialias` may remain only as an explicit, default-off
non-parity feature. It receives native PCM16 before delta deposition and has
its own 49-tap history; it never substitutes for blip interpolation or
feedback. Enabling/disabling it clears only its own history and documents its
24-native-frame group delay. A host-rate or DAC-cadence change may recompute
its coefficients when AA is enabled, but **must preserve** its history and
cursor—never recreate the zero-pad transient or alter exact blip state. Map a
finite FIR result to PCM16 with one explicitly tested round-to-nearest rule
after range checking; do not use an implicit C truncation. The optional
producer/read contract must account for its declared 24-frame latency rather
than reusing a sinc-only `inputs_needed` estimate.

It must not be invoked by CLAP, M4AEngine, Rust, renderer, or M4L defaults. Its
tests must independently check disabled no-op behavior, DC and near-Nyquist
passband behavior, stopband attenuation, a rate/cadence change without history
reset, and distinct left/right buffers. No AA output is compared to the mGBA
reference. If no supported product uses the control after the audit, remove the
control, renderer flag, and AA-only test rather than retaining a no-op or
hidden alternate frontend.

## Independent 0.10.5 reference target

The reference target prevents production and test code from sharing the same
implementation bug. It is separate from `PORYAAAA_BUILD_MGBA_REFERENCE` and
from the observation-patched oracle.

1. In `packages/poryaaaa/CMakeLists.txt`, remove the current nested
   `poryaaaa_frontend_parity_tests` block that includes mGBA audio-buffer /
   audio-resampler headers, receives mGBA definitions, and links `mgba`.
2. Add this opt-in configuration outside that block:

   ```cmake
   option(PORYAAAA_BUILD_FRONTEND_PARITY_TESTS
          "Build exact mGBA 0.10.5 blip_buf frontend parity tests" OFF)
   set(PORYAAAA_MGBA_0105_SOURCE "" CACHE PATH
       "Untouched mGBA 0.10.5 source checkout for frontend parity")
   set(PORYAAAA_MGBA_0105_EXPECTED_REVISION
       "26b7884bc25a5933960f3cdcd98bac1ae14d42e2" CACHE STRING
       "Pinned mGBA 0.10.5 revision")
   ```

3. When enabled, require that source path, `CMakeLists.txt`, and `.git` exist;
   require `git -C <source> rev-parse HEAD` equals the expected revision; reject
   every tracked diff and every untracked path. Reject the observation-patched
   source even if its commit happens to match.
4. Build only
   `src/third-party/blip_buf/blip_buf.c` as a C11 static library named
   `poryaaaa_mgba0105_blip_reference`, with that checkout's `include` directory
   private to the library. Do not apply poryaaaa warning/Werror settings to
   untouched third-party source.
5. Privately rename every public blip symbol on the reference library:
   `blip_new`, `blip_set_rates`, `blip_clear`, `blip_add_delta`,
   `blip_add_delta_fast`, `blip_clocks_needed`, `blip_end_frame`,
   `blip_samples_avail`, `blip_read_samples`, and `blip_delete` become
   `poryaaaa_mgba0105_<name>`. Rename all, even unused ones.
6. Build `poryaaaa_frontend_parity_tests` from only
   `test/test_hw_resample_parity.c`, with normal first-party C11 warnings;
   include only `plugin` and `test`; link only `hw_audio`, the renamed static
   reference library, and `m` if the test itself needs it. Register it with
   `add_test` within the opt-in block.
7. The test declares opaque `struct blip_t` and renamed prototypes locally (or
   in a test-only local header). It must not include mGBA headers, inherit mGBA
   compile definitions, link `mgba`, or place reference source in a shipped
   target.

## Strict test replacement matrix

`test/test_hw_resample_parity.c` must have two independent adapters:

- `RefFrontend` owns two opaque renamed `blip_t*`, `last_l`, `last_r`, `clock`,
  `audio_buffers`, `host_rate`, and `fps_target`. Its submit code follows the
  source producer-gate/delta/frame order and its read invokes each renamed
  `blip_read_samples` into an interleaved buffer at strides two.
- The production adapter invokes only `hw_resample_init`, `submit`,
  `read_pcm16`, `set_output_rate`, and `reset`. It may pack its planar PCM16
  only in assertion code. It must not contain duplicated table, delta,
  interpolation, or feedback logic.

Every read asserts exact returned count, byte-for-byte interleaved PCM16, and
bitwise float equality to `(float)oracle_pcm16 / 32768.0f`. Test every
intermediate read and the final concatenated stream. `ASSERT_NEAR`, peak-only
checks, `lrintf`, skipped startup, warmup exclusions, and tolerance windows are
forbidden.

Use `GBA_HZ=16777216`, frame clocks `2048`, storage `16384`, host `48000`,
`audioBuffers=1536`, and `fpsTarget=59.72750056960583f`. The named tests are:

| Test | Exact setup and required catches |
| --- | --- |
| `test_arm64_factor_clear_and_rollover` | Submit `{0,4096,-4096,12000,-12000,32767,-32768,1}` at 256 clocks, then the next eight phase-vector values. At the first rollover require `clock==0`, `available==5`, `offset==3876723380715520`; after the second require `available==11`, `offset==3243404683116544`. Require factor `12884901888000` and initial offset `6442450944000`. Catches 32-bit timing, clock order, lost fractional offset, and source padding. |
| `test_factor_uses_legacy_ceiling_rule` | Direct primitive probe with host rate `48000.1`, FPS `59.0f`, gate 1536. Independently calculate Qt's float faux clock and source ceiling. On arm64 require factor `13043806758541` and `offset=factor/2`. This catches truncation hidden by the integral 48 kHz factor. |
| `test_all_phases_delta_interpolation_table_stereo_and_short_chunks` | Use `V={0,4096,-4096,12000,-12000,32767,-32768,1,-1,24576,-24576,8191,-8192,16384,-16384,0,32767,-32768,1234,-5678,22222,-22222,73,-73,30000,-30000,15123,-15123,7,-7,2048,-2048}`; L is `V[i]`, R is `V[(7*i+3)&31]`, then zeros through input 87. Compare one chunk with partitions `{1,7,8,3,5,8,1,15,40}` and reads cycling `{1,3,7,2,11,5}`. At 48 kHz/256 the phase step is 23, so every table row is exercised. Catches table/reverse/sign/interleave/history/memmove defects. |
| `test_integer_clamp_and_six_bit_feedback` | Submit 8×`-32768`, 64×`32767`, 64×`-32768`, 64×`32767`, 64×0 at 256 clocks; right is the distinct safe negated sequence. Read both with small repeating caps `{1,1,4,2,7,3}` and once whole. Require both signed extrema and nonzero post-transition tail. Catches ordinary saturation, wrong feedback shift/order, float high-pass, and reset integrator. |
| `test_saturation_gate_drops_samples_without_advancing_delta_history` | Without reads, submit 263 complete 8-sample frames of L=-12000/R=9000 at 256 clocks; require `available==1541`. Submit 17 rejected alternating ±30000 stereo inputs, read six, then submit eight L=12000/R=-9000 inputs and drain. Require clock and last histories remained `0`, `-12000`, `9000` while gated and the resumed delta is +24000/-18000. Catches capacity/gate/history corruption. |
| `test_reset_matches_blip_clear_and_preserves_last_pcm_history` | After a 12000/-9000 frame and read, reset both adapters; an identical next frame is zero-delta while a fresh frontend is not. Then submit -12000/9000. Require source-exact output and post-reset state: available/integrator/clock zero, `offset=factor/2`, retained last PCM. |
| `test_host_rate_and_soundbias_changes_keep_the_legacy_timeline` | With live offset/integrator, change host 48000→44100 without clear and compare subsequent 64 phase-vector inputs drained in `{2,1,8,3,5}`. Separately submit frame-aligned cadence segments 16×256, 32×64, 16×128, 8×512; factor must remain unchanged across every SOUNDBIAS cadence segment. Catches re-rate/reset on cadence changes. |

Replace—not retain under changed comments—the existing parity harness tests:

```text
test_startup_watermarks_and_drain
test_partitions_match_pinned_mgba_at_all_rates
test_rate_change_preserves_fractional_phase
test_channels_keep_independent_pcm16_history
test_antialias_rejects_source_tone_above_host_nyquist
```

The first four describe the removed modern frontend harness; the fifth moves to
the optional-AA isolation test or is deleted with AA.

In `test/test_engine.c`:

- delete `test_chip_output_preserves_dc`; the premise conflicts with blip
  feedback;
- delete `test_hw_resample_matches_mgba_sinc_impulse` and its 24-sample float
  golden, then remove its no-longer-needed resample include unless another exact
  state assertion needs it;
- rewrite `test_chip_canned_block_size_invariance`,
  `test_chip_canned_cycle_boundary_partition_invariance`, and
  `test_chip_canned_soundbias_mid_call_matches_split_call` to compare all host
  floats with `memcmp`, including startup zeros and hold-last frames—remove the
  64-sample warmup exclusion and `1e-4` tolerance;
- retain the existing canned SQ2 event stream. For its SOUNDBIAS integration
  case, render 768+768 frames at 48 kHz, put the cycle-3 SOUNDBIAS write at
  `test_gba_cycles_for_host_frames(768, 48000)`, end at
  `test_gba_cycles_for_host_frames(1536, 48000)`, and require internal rate
  262144 plus exact 1536-frame equality between one-batch and split forms;
- retain silent tests only as exact zero checks and remove the old “within 10%”
  resampler-level explanation.

In `test/test_porydaw_engine.c`, remove the entire legacy-filter continuity and
mean-DC assertion function. Replace it with a shared-front-end equivalence
contract: configure two fresh engines with a square-2 fixture, program 0,
CC7=127, CC10=64, and note-on 60/127. Render one through
`m4a_engine_process` in `{257,2048,1791}` frames; render the other through the
canonical pending-write/HwAudio sequence at the same boundaries. Require a
non-silent signal and exact `memcmp` L/R over all 4096 frames. Reset both,
reapply the same public setup, and require both the second A/B pair and the
first/second A stream to be byte-identical.

If AA remains, add a non-parity `test_engine.c` test using 4096 native frames
of `{0,21213,-30000,21213,0,-21213,30000,-21213}` at 65536 Hz into host 44100.
Default and explicit-AA-off must match the reference exactly. AA-on must use
separate left/right output arrays, differ only under its documented optional
contract, demonstrate DC and near-Nyquist passband behavior plus stopband
attenuation, and preserve history across a controlled output-rate/DAC-cadence
change. Do not put it in the reference parity executable.

## Licensing and attribution checkpoint

The exact table and algorithms come from `blip_buf 1.1.0`, copyright
2003–2009 Shay Green, under LGPL-2.1-or-later as stated in
`src/third-party/blip_buf/blip_buf.c`. Before merging a direct port:

1. Have the repository license owner determine whether the translated fixed
   implementation is a derivative of that library and apply the required
   distribution obligations; do not assert a clean-room exception merely
   because the code was transcribed.
2. Preserve/update the existing
   `packages/poryaaaa/plugin/hw_audio/LICENSE.blip_buf` with the complete
   LGPL-2.1 text and add prominent source/copyright/version/change attribution
   beside the port and in the packaged third-party notice location used by this
   project.
3. Keep the untouched source only in the opt-in test target; do not install it
   or link it into a shipped target.
4. Make the source provenance, table version, and licensing decision review
   blockers. A numerical match does not waive attribution or license duties.

## Implementation waves

Subagents skip formatters, linters, builds, and project-wide suites. The final
integrator runs the focused commands once after the review wave.

1. **Wave 0 — `scout` (explorer/reconnaissance):** independently verify the
   0.10.5 checkout revision/cleanliness, installed binary/config snapshot,
   every `HwResample` and zero/hold output site, exported-symbol references,
   old test registration, and precise shipped-host call paths. Produce a
   file:line inventory; make no edits.
2. **Wave 1 — `task`:** implement the `hw_resample.h/.c` fixed PCM16 module and
   the `hw_audio.c/.h` integration together. The contract above is fixed;
   remove the old generic process API in the same wave and migrate every
   production caller. Do not add host policy to the module.
3. **Wave 2 — `task`:** replace `test_hw_resample_parity.c` and add the isolated
   CMake reference target. It may proceed in parallel with Wave 1 only after
   agreeing on the header contract. It owns no production code and must link
   the untouched reference only.
4. **Wave 3 — `sonic`:** after LSP references, remove obsolete `M4AEngine`
   fields/high-pass loop and replace the named engine/porydaw tests. Audit all
   caller files listed above; this wave must not create a new host algorithm.
5. **Wave 4 — `reviewer`:** adversarially inspect source order, width/overflow,
   reset/rate/SOUNDBIAS/gate rules, module depth, license attribution, CMake
   isolation, test independence, and every shipper. Block any retained
   approximate filter, modern-resampler reference, tolerance/warmup escape,
   or claim-boundary leakage.
6. **Wave 5 — `task` with a separate `reviewer`:** perform the controlled
   runtime diagnostic procedure, preserve the settings/provenance and first
   mismatch, then have the reviewer verify its labeling and confirm that no
   diagnostic is represented as Hardware/Driver/Oracle evidence.

## Runtime frontend diagnostic procedure

This is optional after exact unit parity passes; it never replaces the strict
reference target.

1. Quit mGBA. Record the Info.plist version and `shasum -a 256` of the app
   executable. Inspect `~/.config/mgba/config.ini` and set/record exactly the
   five target values above; use normal speed (not fast forward), mute off, and
   volume 102. Reopen the application and re-check the effective values in its
   Qt settings UI before capture.
2. In Audio MIDI Setup, make a named two-channel loopback input (for example,
   `BlackHole 2ch`) the exact device used to record mGBA's output. Configure it
   at 48000 Hz/2 channels. Record the device name, format, clock source, and
   the actual mGBA output device/rate shown at runtime. Do not assume Qt's
   requested rate won negotiation.
3. Launch the installed binary with the recorded ROM, reach the documented
   Music Test state, and preserve a screenshot before recording. For the known
   comparison state, record `Music ID 0561 MUS-SQUARE`; if a different state is
   used, record its complete navigation and screenshot instead of relabeling it.
4. Capture the loopback PCM with a named device, for example:

   ```bash
   mkdir -p build/frontend-live
   ffmpeg -f avfoundation -i ':BlackHole 2ch' -ac 2 -ar 48000 \
     -c:a pcm_s16le -t 10 build/frontend-live/mgba-0105-live.wav
   ```

   Start capture first, perform the fixed UI transition, then stop after the
   fixed duration. Store the command, duration, device format, ROM hash,
   screenshot, config snapshot, and WAV SHA-256 under `build/frontend-live/`.
5. Render the comparable poryaaaa fixture at 48 kHz using the shared frontend,
   capture its WAV command/commit/config, and choose one manually documented
   onset-aligned 512-sample local stereo window. Compare integer PCM16 samples
   only after that documented alignment; do not shift-search, normalize, remove
   DC, fit gain, fold mono, or discard mismatches. Preserve the first mismatch.

The live capture only diagnoses installed frontend handoff behavior. Device
mixing, latency, user navigation, CoreAudio rate conversion, or missing
loopback prerequisites make the diagnostic unavailable, not passing.

### Runtime result status and authority

Any completed installed-application capture and aligned PCM16 comparison is
retained as a **Frontend diagnostic** with its recorded binary, settings,
device, fixture, window, alignment, and first mismatch. Its result describes
that local runtime handoff only. It is not an authoritative Hardware, Driver,
Oracle, or Renderer result; a mismatch cannot justify changing native math,
the exact blip path, or the strict parity gate. Conversely, an unavailable
device or capture prerequisite is an unavailable diagnostic, not a pass.

## Acceptance criteria and exact verification commands

The implementation is complete only when all of the following are true:

1. The source checkout passes the clean revision requirement and the reference
   target uses only untouched 0.10.5 blip_buf—not a newer implementation.
2. All seven named parity vectors pass with exact read counts, PCM16 bytes, and
   float bits. The installed factor/offset/rollover values and the nonintegral
   ceiling probe are asserted.
3. Gate, reset, rate change, and every SOUNDBIAS cadence transition meet the
   state invariants without re-rate/reset or changed last-PCM history.
4. `HwAudio` is the sole production post-native frontend owner. All shipped
   hosts return its float output; startup is muted and later shortages hold the
   last frame without fabricated source PCM.
5. The approximate `M4AEngine` float high-pass and every listed obsolete field
   and test are gone. `pcmMixRate` and `analogFilter` retain only their stated
   non-frontend roles.
6. Optional AA is disabled by default and isolated from all parity claims, or
   it is removed completely with its option/test.
7. The independent reference source, CMake target, tests, and production
   target have no mGBA symbol/include/definition leakage. Licensing/attribution
   review is complete.
8. A runtime diagnostic, if prerequisites exist, records the installed version,
   binary hash, effective `config.ini`, volume/mute, device, fixture, command,
   local window, and first mismatch; it is labeled Frontend diagnostic only.

From `packages/poryaaaa`, use these exact verification commands after the
implementation and review waves (the source path is deliberately explicit):

```bash
MGBA_0105=/Users/spencer/dev/cProjects/mgba-0.10.5-reference
git -C "$MGBA_0105" rev-parse HEAD
git -C "$MGBA_0105" diff --quiet
test -z "$(git -C "$MGBA_0105" ls-files --others --exclude-standard)"

cmake -S . -B build \
  -DPORYAAAA_BUILD_FRONTEND_PARITY_TESTS=ON \
  -DPORYAAAA_MGBA_0105_SOURCE="$MGBA_0105" \
  -DPORYAAAA_MGBA_0105_EXPECTED_REVISION=26b7884bc25a5933960f3cdcd98bac1ae14d42e2
cmake --build build --target poryaaaa_frontend_parity_tests poryaaaa_unit_tests
ctest --test-dir build --output-on-failure -R '^poryaaaa_frontend_parity_tests$'
./build/poryaaaa_frontend_parity_tests
./build/poryaaaa_unit_tests

Before a live diagnostic, verify provenance and exact effective settings without
reading `qt.ini`:

```bash
/usr/libexec/PlistBuddy -c 'Print :CFBundleShortVersionString' \
  /Applications/mGBA.app/Contents/Info.plist
shasum -a 256 /Applications/mGBA.app/Contents/MacOS/mGBA
bun -e '
const text = await Bun.file(`${process.env.HOME}/.config/mgba/config.ini`).text();
const section = text.split("[ports.qt]\n", 2)[1]?.split(/\n\[/, 1)[0] ?? "";
const actual = Object.fromEntries(
  section.split("\n").filter(Boolean).map((line) => line.split("=", 2)),
);
const expected = {
  sampleRate: "48000",
  audioBuffers: "1536",
  volume: "102",
  mute: "0",
  fpsTarget: "59.72750056960583",
};
for (const [key, value] of Object.entries(expected)) {
  if (actual[key] !== value) throw new Error(`${key}: ${actual[key]} != ${value}`);
}
console.log(expected);
'
```

Do not run formatting, lint, repo-wide gates, native replay matrices, or a live
capture as a substitute for these focused frontend checks.
