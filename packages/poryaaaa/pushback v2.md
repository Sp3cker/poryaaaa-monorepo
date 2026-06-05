# Pushback v2: Response to Pushback v1

Written in response to `pushback v1.md`.  Each section mirrors v1's
structure; numbering tracks v1's numbered concerns.

## Position

Agreement on the architectural shape.  Agreement that the snapshot
contract is provisional and not sufficient for bit-exact mGBA parity.
Disagreement on urgency: snapshot is sufficient for poryaaaa's primary
target (Pokemon Emerald music playback) and the v2 scaffold (which
produces silence), so the event-stream contract is gated on Layer 1
PSG parity work, not on the scaffold landing.

Three concerns convert to immediate plan changes (3, 4, the test
ordering).  Five convert to a single deferred change — the event-stream
contract — landing as a "Layer 1.5" pass before any PSG parity claim.
One (5, noise coalescing) is converted to a verification gate before the
Layer 1 noise port.

---

## Per-concern responses

### Re: 1. Register snapshots are too coarse

Accepted as long-term direction; deferred for the scaffold.  Snapshot is
the v2-scaffold contract because the scaffold produces silence — the
absence of ordering doesn't bite when there's nothing to render.

Plan now marks the snapshot as **provisional**.  A sibling event-stream
API is sketched in a new §6c ("Contract evolution: ordered register-write
events"), and is the Layer 1.5 contract evolution.  PSG parity claims
gated on event stream landing.  The snapshot stays as a public read
accessor for state that doesn't care about timing (UI, params, debug),
but it ceases to be the authoritative timing interface for the chip.

### Re: 2. Trigger booleans are underpowered

Accepted, same disposition as #1.  Trigger booleans live in the v2
scaffold contract; an event-stream queue replaces them at Layer 1.5.
The provisional language already in §6a ("snapshot-boundary semantics
for v2 … if Layer 1 PSG parity tests show audible phase noise from this,
extend the contract with a per-trigger sample offset") is upgraded from
a soft "if measured" to a hard precondition: PSG parity is not claimed
until events land.

For Pokemon Emerald specifically: CgbSound runs once per vblank, MO_VOL
retriggers at most once per vblank, and song data does not exercise fast
retriggers.  Booleans are sufficient for Emerald.  Booleans are not
sufficient for general m4a parity (ROMhacks, other m4a games, edge tests).

### Re: 3. `M4APcmRing` is software DMA-buffer, not the hardware FIFO

Accepted unconditionally; a real architectural gap.  My text
("chip S&Hs from `pcm_rate_hz`") collapsed three distinct chip-side
stages into one hand-wave.

Plan now splits the chip-side PCM path into three:

- `HwDmaToFifo` — drains the m4a ring at PCM rate, writes 32-bit words
  into the FIFO_A/B word FIFOs.  Models DMA hardware.
- `HwFifoDrain` — drains FIFO byte-by-byte at SOUNDBIAS-derived sample
  cadence; outputs sign-extended s8 samples per FIFO.
- `HwResample` — internal mix rate → host rate.

For Pokemon Emerald the path collapses to a single S&H from 13379 Hz to
SOUNDBIAS rate (no exotic FIFO behaviour), but the abstraction is now
correct and ROMhack/edge cases get the right model.

`M4APcmRing` keeps its name and its driver-side semantics — it is still
the m4a PCM DMA buffer.  The chip-side FIFO is internal to `hw_audio/`
and is not exposed in the driver→chip contract.

### Re: 4. SOUNDBIAS must be a first-class timing source

Accepted with refinement.  mGBA's `audio->sampleInterval = 0x200 >> resolution`
makes SOUNDBIAS resolution the internal sample rate driver
(32768 / 65536 / 131072 / 262144 Hz).

Plan now distinguishes two rates inside `hw_audio/`:

- **Quirk rate** = SOUNDBIAS-derived rate.  Hardware artifacts (noise
  sub-sample averaging, PCM S&H, NR32 quantization, declick) operate at
  this rate; this is where the GBA's actual audio chip produces samples.
- **Output mix rate** = 131072 Hz fixed.  After quirks, the per-channel
  signals upsample to 131072 Hz for clean polyphase resampling to host.

For SOUNDBIAS=0 (Pokemon Emerald, 32768 Hz), the quirk rate is 32768 Hz
— matching v1's current "CGB native" path.  For higher SOUNDBIAS modes,
quirks operate at the higher rate; output upsample target is unchanged.

This addresses your concern that 131072 Hz is "incidental" — it now
isn't; it's purely an output-stage upsample target chosen for clean host
resampling.  The artifact-producing rate is SOUNDBIAS-derived, as in
mGBA.

### Re: 5. Noise coalescing appears version/style-sensitive

Accepted as an open verification gate, not a plan change yet.

Memory `project_cgb_channel_mismatch.md` says v1's coalesce matches
mGBA captures within 1.5 dB at 4–16 kHz.  But as you note, in
`gb_audio.c` the `GB_AUDIO_GBA` mode uses the current LFSR sample
shifted, while `_coalesceNoiseChannel` is for DMG/CGB.  If our 1.5 dB
match is against the resampler's averaging effect rather than actual
hardware coalescing, the coalesce is wrong and we should drop it.

Plan adds a verification step before the Layer 1 noise port: capture
mGBA noise at native 32768 Hz (bypassing the sinc resampler), compare
to v1's coalesce output bit-for-bit.  If the match is from the
resampler, drop coalesce; replace with the GBA-mode "current sample
shifted" path from `gb_audio.c`.

### Re: 6. Sweep ownership is right, but conflicts with snapshot overwrite

Accepted.  The conflict you describe is real: chip-owns-sweep +
driver-writes-`sq1_freq`-every-snapshot creates ambiguity ("did the
driver write a new freq, or echo the unchanged value?").  And m4a does
rewrite freq for pitch-bend and LFO vibrato — "only on trigger" is
wrong too.

Resolved automatically by the event-stream contract: a freq write is a
real event; absence means chip's sweep keeps advancing freq
uncontested.  This is one more pin pointing toward events as the
long-term contract.

For the scaffold (silence), the issue doesn't bite.  For the Layer 1
square port, it must be solved — which means the event-stream is a
gate on Layer 1 square's PSG parity, not a Layer 1.5 luxury.

### Re: 7. Wave declick should be tied to hardware write edges

Accepted, same disposition as #1.  Snapshot's `wave_dac_on=true` doesn't
capture the off→on transition edge or the wave-RAM-rewrite-while-DAC-off
sequence.  Event stream captures both.  v2-scaffold uses snapshot;
Layer 1 wave port consumes events.

The chip-side declick remains a *consequence* of NR30 toggle events,
not a software fade — that part of the plan is unchanged.

### Re: 8. m4a fidelity also means preserving write order inside `CgbSound`

Accepted.  The Layer 1 m4a port emits register-write events in
`CgbSound`'s actual order (NR10 → NR11 → NRx2 → NR13 → NR14-trigger).
Chip processes them in order, with each write taking effect at its
emitted sample offset.

Snapshot is computed *as a side effect* of the event stream, not
maintained independently.  At any point in time, "current snapshot" =
"final state after all events emitted up to now."

---

## Suggested Contract Adjustment

Adopted with one caveat on the snapshot's role.

Public API at Layer 1.5:

- **`plugin/m4a/`** owns m4a state and produces:
  - `M4APcmRing` writes (driver-side PCM DMA buffer)
  - **Ordered audio register-write events** with sample offsets relative
    to the current render span (the new authoritative interface)
  - **Current register snapshot** as a read accessor for non-timing
    consumers (UI, params, debug, some tests) — explicitly a cache, not
    the authoritative interface.
- **`plugin/hw_audio/`** owns:
  - PSG phase / envelope / sweep / length / LFSR / wave-RAM hardware
    state
  - FIFO A/B state and sample-hold (split into `HwDmaToFifo`,
    `HwFifoDrain`)
  - SOUNDCNT routing/scaling
  - SOUNDBIAS-driven internal sample timing
  - Output resampling to host rate

**Caveat:** the snapshot accessor remains in the public API for
non-timing consumers.  Your framing in v1 (§ "Suggested Contract
Adjustment") said "snapshot for debugging/tests only" — agreed in
spirit, but in practice the snapshot is also useful to UI code reading
"what's m4a doing right now."  Documenting it as a non-authoritative
cache addresses your real concern (don't time the chip from the
snapshot) without pushing it out of the API.

---

## Test Expectations

Adopted in the order you proposed.  Plan §12 verification reorders to:

1. PSG-only square / wave / noise renders at 32768 Hz and 65536 Hz
   SOUNDBIAS modes (chip-only, canned register snapshots / events).
2. DirectSound-only FIFO sample-hold tests with known byte patterns.
3. Mixed PSG + DirectSound routing tests for SOUNDCNT_L / SOUNDCNT_H.
4. Register-edge tests: NRx4 retrigger timing, NR30 DAC toggle, wave
   RAM rewrite with DAC off, SOUNDBIAS resolution change.
5. Real song / capture comparisons last (full-mix A/B vs mGBA Qt).

Subjective song matching becomes the final gate, not the first line of
defense.  Driver-only and chip-only flag combinations (§8 in the plan)
make 1–4 trivially writable as unit tests — no full pipeline needed.

---

## One mild pushback

Your v1 framing implies "all of this is required for mGBA parity,
period."  It is — for *general* parity.  But poryaaaa's primary target
(per memory `user_poryaaaa_usage.md`: voice-player workflow, Pokemon
Emerald songs) exercises almost none of the hard cases:

- No dynamic SOUNDBIAS changes (Emerald sets resolution=0 at boot,
  never touches it).
- No MO_VOL retrigger more than once per vblank (CgbSound runs once,
  retriggers at most once per envelope event).
- No DAC-on wave-RAM rewrites (m4a always writes wave RAM with NR30=0).
- No song-level fast retriggers (note-on density is bounded by m4a's
  vblank tick, ≈59.7 Hz).

For Emerald, snapshot semantics *happen to match* event semantics
because the events arrive at deterministic per-vblank boundaries with
fixed within-vblank order.  So events are **necessary for general
parity** but **not necessary for Emerald audibility**.

We're building toward general parity (it's the long-term target), but
the scaffold doesn't need to ship with the event stream.  It needs to
ship with the door open so events can land at Layer 1 / 1.5 without
contract churn.

---

## Bottom line

| Issue | Disposition |
|---|---|
| Module split | Agreed, kept |
| Snapshot as long-term contract | Rejected — event stream replaces it at Layer 1.5 |
| Snapshot as scaffold contract | Kept with provisional warning |
| Snapshot as debug/UI accessor | Kept (caveat above) |
| PCM ring vs FIFO | Split into `HwDmaToFifo` / `HwFifoDrain` / `HwResample` |
| SOUNDBIAS as internal rate driver | Adopted (quirks at SOUNDBIAS rate; 131072 Hz only for output upsample) |
| Trigger booleans → ordered events | Layer 1.5; provisional today |
| Wave declick → hardware edges | Same as triggers |
| Sweep + freq disambiguation | Resolved by events at Layer 1.5 |
| CgbSound write order preserved | Accepted; events emitted in m4a order |
| Noise coalescing version-check | Verification gate before Layer 1 noise port |
| Test ordering: edge tests first | Adopted; §12 reordered |

Plan file (`HW_AUDIO_SCAFFOLD_PLAN.md`) updated to reflect each of the
above.  Awaiting v2 review.
