#ifndef HW_AUDIO_INTERNAL_H
#define HW_AUDIO_INTERNAL_H

/* Forward declarations for chip stages.  See HW_AUDIO_SCAFFOLD_PLAN.md §7b.
 *
 * Rates inside the chip:
 *   quirk rate       = 32768 << bias_sampling_cycle (32k/65k/131k/262k Hz)
 *                      — read from SOUNDBIAS, stored on HwMixBus.
 *   internal rate    = max(131072, quirk_rate).  131072 Hz for
 *                      sampling_cycle 0/1/2; 262144 Hz for sampling_
 *                      cycle 3.  Synced from sampling_cycle at start
 *                      of each render call (boot-time-only target —
 *                      mid-call SOUNDBIAS changes deferred to next
 *                      render boundary; see hw_audio.c).
 *   host rate        = whatever the caller passes to hw_audio_set_host_rate.
 *                      Polyphase resampler bridges internal → host.
 */

/* PSG path — square 1/2, wave, noise.  Synthesises at the chip-internal
 * render rate into 4 mono per-channel buffers (hw_psg.c). */
typedef struct HwPsgSynth   HwPsgSynth;

/* PCM path — two-stage drain (§12 step 5, plan §6b, closed
 * 2026-04-29).  HwDmaToFifo reads M4APcmRing at pcm_rate_hz into the
 * FIFO head byte; HwFifoDrain snapshots the head at the SOUNDBIAS-
 * derived quirk_rate.  Both stages share the HwPcm struct in hw_pcm.c.
 * Output is held_quirk sign-extended at internal_rate cadence;
 * routing/scaling lives on HwMixBus.  hw_pcm.c writes 2 mono per-FIFO
 * buffers. */
typedef struct HwDmaToFifo  HwDmaToFifo;
typedef struct HwFifoDrain  HwFifoDrain;

/* Mix bus — SOUNDCNT_H/L routing/scaling + SOUNDBIAS bias add/clip.
 * Landed at §12 step 8 (2026-04-29) in hw_mix.c.  Combines 4 PSG mono
 * + 2 DMA mono buffers into stereo, applies the GBA's unsigned 10-bit
 * DAC bias-add/clip pipeline at the chip-internal rate. */
typedef struct HwMixBus     HwMixBus;

/* Polyphase resampler — chip-internal rate → host rate.  Landed at
 * §12 step 9 (2026-04-29) in hw_resample.c: windowed-sinc, TAPS=32,
 * PHASES=64, Hann window, kernel cut at min(internal,host)/2,
 * DC-gain-normalized.  Kernel + state rebuild when internal_rate
 * changes between render calls (i.e. SOUNDBIAS sampling_cycle
 * transitions).  hw_audio.c drives it via cumulative sample-clock
 * accounting (block-size invariant) so chip-time advances in
 * lock-step with host frames regardless of how the caller chunks
 * render windows. */
typedef struct HwResample   HwResample;

#endif
