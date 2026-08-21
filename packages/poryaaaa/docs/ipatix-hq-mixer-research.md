# iPatix HQ Mixer Research

> **Implementation planning, not a parity claim.** Source comparison pinned to
> iPatix `gba-hq-mixer` commit
> [`2bd31435101e4c3a4af568a6ed33d7cc9f1a6da9`](https://github.com/ipatix/gba-hq-mixer/commit/2bd31435101e4c3a4af568a6ed33d7cc9f1a6da9).

## Conclusion

poryaaaa does not need a second mixer. Its current `m4a_pcm.c` already implements
most of iPatix's defining HQ arithmetic behind `m4a_sound_main_ram`: packed
16-bit stereo accumulation, linear interpolation, one noise-shaped conversion
to the GBA's 8-bit FIFO streams, reverse playback, and Camelot/Golden Sun
zero-length synth descriptors. The source comments that call this path
"vanilla Sappy" are materially misleading.

The first implementation work should therefore be a semantic audit and clean
cutover of the remaining mismatches, not a parallel `HqMixer` abstraction. The
largest confirmed gaps are iPatix's reverb feedback order and Pokémon BDPCM
sample decoding.

## What the HQ mixer changes

Vanilla Pokémon MP2K clears or seeds its two 8-bit DMA buffers, then accumulates
each voice directly into those output bytes. Its reverb also reads and writes
the 8-bit DMA buffers before channel mixing. See pokeemerald
[`SoundMainRAM`](https://github.com/pret/pokeemerald/blob/master/src/m4a_1.s#L86-L145)
and its per-channel byte-lane accumulation
([`m4a_1.s`](https://github.com/pret/pokeemerald/blob/master/src/m4a_1.s#L395-L648)).
Every voice can therefore introduce another quantization step.

The HQ mixer instead uses a separate stereo work buffer requiring four bytes per
output frame. It accumulates all voices there and converts to the two 8-bit DMA
streams once, after mixing. iPatix describes this motivation and memory contract
in the project
[`README`](https://github.com/ipatix/gba-hq-mixer/blob/2bd31435101e4c3a4af568a6ed33d7cc9f1a6da9/README.md).
The implementation packs the two signed 16-bit lanes into one `00LL00RR` word
and updates both with one ARM multiply-accumulate
([`m4a_hq_mixer.s:399-430`](https://github.com/ipatix/gba-hq-mixer/blob/2bd31435101e4c3a4af568a6ed33d7cc9f1a6da9/m4a_hq_mixer.s#L399-L430)).
Its final stage clamps each lane to an effective signed 30-bit numerator, emits
one signed 8-bit sample, and carries seven discarded bits into the next sample
as first-order error feedback
([`m4a_hq_mixer.s:944-1005`](https://github.com/ipatix/gba-hq-mixer/blob/2bd31435101e4c3a4af568a6ed33d7cc9f1a6da9/m4a_hq_mixer.s#L944-L1005)).
The hardware output remains 8-bit; the improvement is avoiding per-voice
quantization.

The assembly also optimizes for ARM7TDMI and GBA memory timing: packed stereo
math, unrolled loops, dynamically selected/self-modified fast loops, optional
DMA sample prefetch, and IWRAM placement
([`README`](https://github.com/ipatix/gba-hq-mixer/blob/2bd31435101e4c3a4af568a6ed33d7cc9f1a6da9/README.md),
[`m4a_hq_mixer.s:610-749`](https://github.com/ipatix/gba-hq-mixer/blob/2bd31435101e4c3a4af568a6ed33d7cc9f1a6da9/m4a_hq_mixer.s#L610-L749)).
Those techniques are ROM performance features, not observable mixer semantics;
they should not be ported to desktop C unless profiling proves a need.

Additional source modes are part of this revision:

- Camelot synth descriptors select pulse, pseudo-saw, or triangle generators
  ([`m4a_hq_mixer.s:1080-1166`](https://github.com/ipatix/gba-hq-mixer/blob/2bd31435101e4c3a4af568a6ed33d7cc9f1a6da9/m4a_hq_mixer.s#L1080-L1166)).
- Pokémon's block delta PCM path decodes 64 samples from each 33-byte block
  ([`m4a_hq_mixer.s:447-608`](https://github.com/ipatix/gba-hq-mixer/blob/2bd31435101e4c3a4af568a6ed33d7cc9f1a6da9/m4a_hq_mixer.s#L447-L608)).
- Reverse, fixed-frequency, looped, and resampled sources use specialized paths
  before converging on the same HQ buffer
  ([`m4a_hq_mixer.s:399-430`](https://github.com/ipatix/gba-hq-mixer/blob/2bd31435101e4c3a4af568a6ed33d7cc9f1a6da9/m4a_hq_mixer.s#L399-L430)).

Golden Sun's descriptor extension is related but not synonymous with the HQ
mixer. iPatix cites Golden Sun as an inspiration; the first-party `agbplay`
implementation identifies its synth instruments independently by zero loop and
zero length, with descriptor byte 1 selecting PWM, saw, or triangle
([`MP2KChnPCM.cpp:19-40`](https://github.com/ipatix/agbplay/blob/0960aadec72dddbefc144216886d86bef220a0bb/src/agbplay/MP2KChnPCM.cpp#L19-L40)).

## What poryaaaa already has

- `M4ADriver::pcmMixPacked` stores one `00LL00RR` work word per output frame;
  `mix_pcm_sample` uses the same deliberate packed-lane carry behavior
  (`plugin/m4a/m4a_internal.h:304-314`, `plugin/m4a/m4a_pcm.c:283-288`).
- `downsample_pcm_block` performs the HQ effective-30-bit clamp, byte extraction,
  and independent seven-bit error carry (`plugin/m4a/m4a_pcm.c:298-325`).
- `render_channel` implements the HQ Q9.23 linear interpolation and fast/general
  source paths (`plugin/m4a/m4a_pcm.c:476-565`).
- `m4a_drv_pcm_start` and `render_synth_channel` implement the zero-length
  Camelot descriptor convention and its three oscillators
  (`plugin/m4a/m4a_pcm.c:20-86`, `438-471`).

Confirmed gaps:

1. **Reverb state and ordering.** iPatix downconverts the current HQ buffer into
   DMA output, derives the four-tap wet term from historical 8-bit DMA samples,
   and writes that wet term into the HQ work buffer as the next frame's seed
   ([`m4a_hq_mixer.s:944-1055`](https://github.com/ipatix/gba-hq-mixer/blob/2bd31435101e4c3a4af568a6ed33d7cc9f1a6da9/m4a_hq_mixer.s#L944-L1055)).
   poryaaaa instead clears `pcmMixPacked`, mixes and downconverts, then adds a
   wet value directly to the quantized lanes in the same render and writes an
   explicit int8 delay line (`plugin/m4a/m4a_pcm.c:213-280`, `568-637`). The
   two state machines are not equivalent around quantization, saturation, or
   frame boundaries.
2. **BDPCM.** poryaaaa notices `wav->type & 0x80` only to disable its fast path;
   its common source cursor still reads those encoded bytes as ordinary PCM
   (`plugin/m4a/m4a_pcm.c:493-555`). No decoder exists.
3. **Unverified edges.** Channel initialization, exact loop wrap, fixed-rate
   paths, reverse endpoints, synth phase, and packed-lane overflow resemble the
   HQ source but require oracle fixtures before being called bit-exact.

## Staged implementation

Keep the external seam at `m4a_sound_main_ram(M4ADriver *)`. Do not add a runtime
vanilla/HQ selector unless the product genuinely needs two shipped behaviors.
The existing function already contains the useful internal stages.

1. **Pin the target and oracle.** Use iPatix revision 4.0 at the commit above.
   Build a private reference ROM with that exact mixer and capture DirectSound
   FIFO writes through the existing mGBA observation harness. Record the mixer
   commit, ROM hash, fixture identity, PCM rate, reverb level, and polyphony.
2. **Lock existing HQ arithmetic.** Add exact FIFO-byte fixtures for silence,
   one voice, many voices, positive/negative saturation, sub-unity and greater-
   than-unity pitch, fixed frequency, loop wrap, reverse playback, and all three
   synths. This distinguishes mixer parity from frontend resampling.
3. **Replace reverb semantics.** Make `pcmMixPacked` persistent across blocks.
   Mix voices into the prior HQ wet seed, downconvert once, write FIFO bytes,
   then replace each consumed HQ word with the next frame's wet seed. When
   reverb is disabled, replace consumed words with zero. Remove the old
   post-quantization `sound_main_ram_reverb` state only after exact no-reverb and
   reverb fixtures pass.
4. **Add BDPCM only if required by supported voicegroups.** Extend the internal
   source cursor to decode the iPatix 33-byte/64-sample block format while
   preserving forward, reverse, loop, and interpolation state. Do not expose the
   decoder through a new public interface.
5. **Resolve remaining edges from evidence.** Migrate each discrepancy exposed
   by the fixtures. Do not port ARM self-modification, IWRAM placement, or DMA
   prefetch into host C.

## Validation gates

- Compare canonical FIFO A/B bytes and their VBlank/refill ordering. WAV
  correlation, gain fitting, DC removal, or frontend alignment cannot prove
  mixer parity.
- Treat mGBA as the endpoint oracle, not as another MP2K mixer: its hardware
  stage combines DirectSound and PSG before the SOUNDBIAS clamp
  ([`audio.c:384-432`](https://github.com/mgba-emu/mgba/blob/438c77387d4419523d72aac71b3a43e2b06eb3c3/src/gba/audio.c#L384-L432)).
- Run every fixture twice and require deterministic output.
- Cover reverb zero/nonzero, first block, steady state, DMA-ring wrap, silence
  tail, and heavy mixes that hit both clamp limits.
- Keep native mGBA capture and poryaaaa hardware replay separate, as required by
  `docs/arch-parity-fix-plan.md`; a frontend WAV match is a different claim.
- After exact synthetic fixtures pass, render a full song with the same ROM
  mixer, voicegroup, volume, reverb, PCM rate, and polyphony as an end-to-end
  smoke test.

## License

The pinned repository changed its project license from GPLv3 to MIT in commit
[`2bd3143`](https://github.com/ipatix/gba-hq-mixer/commit/2bd31435101e4c3a4af568a6ed33d7cc9f1a6da9), with the author's statement that
all current contributors agreed. The assembly's top comment still says GPLv3,
so preserve the pinned revision and MIT notice when adapting substantial code;
resolve the stale file header before redistribution if project policy requires
unambiguous provenance.
