#ifndef HW_AUDIO_H
#define HW_AUDIO_H

#include "m4a/m4a_register_file.h"
#include "m4a/m4a_pcm_ring.h"
#include "m4a/m4a_driver.h"   /* M4ARegWriteBatch / M4ARegWrite types */

#ifdef __cplusplus
extern "C" {
#endif

/* GBA audio chip emulation.  Mirrors mGBA's gb_audio.c / gba_audio.c.
 *
 * Status (2026-04-29):
 *   - PSG square / wave / noise: synthesised at the chip-internal
 *     render rate (`max(131072, 32768 << sampling_cycle)` per plan
 *     §7b — 131072 Hz for SOUNDBIAS sampling_cycle 0/1/2; 262144 Hz
 *     for sampling_cycle 3).  Sampling_cycle synced from HwMixBus at
 *     start-of-render-call (boot-time-only target — see hw_audio.c).
 *   - PCM DirectSound: two-stage drain (§12 step 5 closed).
 *     HwDmaToFifo reads M4APcmRing at pcm_rate_hz; HwFifoDrain
 *     snapshots the FIFO head at SOUNDBIAS-derived quirk_rate
 *     (32k/65k/131k/262k Hz); output at internal_rate is the held
 *     quirk byte sign-extended.
 *   - Mix bus: full SOUNDCNT_L (NR50/NR51) + SOUNDCNT_H + SOUNDBIAS
 *     bias-add / clip pipeline at internal rate (§12 step 8 + 9).
 *   - Output resampler: windowed-sinc polyphase from internal rate
 *     to host rate (§12 step 9, hw_resample.c).  Kernel rebuilds
 *     when sampling_cycle changes between render calls.  Cumulative
 *     sample-clock accounting in hw_audio.c gives block-size
 *     invariance regardless of how the caller chunks render windows.
 *
 * Audibility ≠ parity.  The chip is audible end-to-end, properly
 * band-limited at host_rate/2, and structurally complete; but it is
 * NOT yet a fully faithful model of mGBA / real hardware until step
 * 10b (mGBA capture-comparison parity — self-consistency tests at
 * §12.10a only prove pipeline coherence, not match against a
 * reference) closes.  See plan §12 "blocking gates" list.  Don't
 * make spectral / level claims against mGBA captures from this
 * build. */
typedef struct HwAudio HwAudio;

HwAudio *hw_audio_create(float host_sample_rate);
void     hw_audio_destroy(HwAudio *hw);
void     hw_audio_set_host_rate(HwAudio *hw, float hz);

/* ⚠ DEBUG / TEST VISIBILITY ONLY — not part of the production chip
 * timing contract.  Returns the chip's current internal render rate
 * (PSG/PCM/mix synth rate, before the polyphase resampler).  Per
 * plan §7b this is `max(131072, 32768 << sampling_cycle)` — 131072
 * Hz for SOUNDBIAS sampling_cycle 0/1/2, 262144 Hz for sampling_
 * cycle 3.  Exposed because it caught a real class of cadence-
 * switching bug (a fixed-rate implementation that ignored
 * sampling_cycle would silently still produce audible output for
 * typical low-frequency test signals — only an explicit rate
 * assertion can prove the switch wired up).  Production callers
 * should not depend on this; the rate may change with future
 * SOUNDBIAS work or scope refinements.
 *
 * Snapshot at start-of-call: this reflects whatever sampling_cycle
 * was set by SOUNDBIAS events on prior render calls.  Mid-call
 * SOUNDBIAS changes don't affect this until the next render call
 * (boot-time-only target — see hw_audio.c). */
int      hw_audio_internal_rate(const HwAudio *hw);

/* Per-channel solo / mute mask for parity capture work.  Each bit
 * gates whether that channel's pre-mix buffer feeds the mix bus —
 * masked-off channels are passed as NULL to hw_mix_render, which
 * treats them as silent.  Channel names + bit positions match the
 * patched mGBA headless tool's `--solo` / `--mute` channel set
 * (`tools/captures/mgba-headless-channel-mute/`), so a single name
 * can drive both sides of a parity comparison.
 *
 * SOUNDCNT_L / H routing + scaling and SOUNDBIAS bias-add still run
 * over whatever channels are enabled — soloing wave, for instance,
 * still gets you the wave channel routed/clipped exactly as the
 * full mix would route/clip it, just with the other channels
 * zeroed.  Default mask is `HW_AUDIO_SOLO_FULL` (all 6 bits set —
 * normal mix). */
typedef enum {
    HW_AUDIO_SOLO_SQ1     = 1u << 0,
    HW_AUDIO_SOLO_SQ2     = 1u << 1,
    HW_AUDIO_SOLO_WAVE    = 1u << 2,
    HW_AUDIO_SOLO_NOISE   = 1u << 3,
    HW_AUDIO_SOLO_DMA_A   = 1u << 4,
    HW_AUDIO_SOLO_DMA_B   = 1u << 5,
    HW_AUDIO_SOLO_PSG     = HW_AUDIO_SOLO_SQ1 | HW_AUDIO_SOLO_SQ2
                          | HW_AUDIO_SOLO_WAVE | HW_AUDIO_SOLO_NOISE,
    HW_AUDIO_SOLO_DSOUND  = HW_AUDIO_SOLO_DMA_A | HW_AUDIO_SOLO_DMA_B,
    HW_AUDIO_SOLO_FULL    = HW_AUDIO_SOLO_PSG | HW_AUDIO_SOLO_DSOUND,
} HwAudioSoloBits;

void hw_audio_set_solo_mask(HwAudio *hw, uint32_t mask);
uint32_t hw_audio_get_solo_mask(const HwAudio *hw);

/* LEGACY SNAPSHOT API — superseded by hw_audio_render_events() at
 * Layer 1.5 (§12 step 3, closed).  No production v2 caller routes
 * through this function.  It is retained as a no-render trigger-
 * consumption path for any caller that hasn't migrated: writes zeros
 * to outL/outR; clears trigger_sq1/sq2/wave/noise on the mutable
 * register file (see HW_AUDIO_SCAFFOLD_PLAN.md §6a).  Will be removed
 * once the scaffold-era integration tests migrate to the event API.
 *
 * `regs` is non-const so the trigger latches can be cleared.  Pass
 * via m4a_get_register_file_mut(drv); m4a_get_register_file() remains
 * const for non-timing consumers. */
void hw_audio_render(HwAudio *hw,
                     M4ARegisterFile *regs,
                     const M4APcmRing *pcm,
                     float *outL, float *outR, int frames);

/* LAYER 1.5 API — event-driven.  Authoritative interface used by all
 * production v2 call sites (CLAP process, headless export, CLI render,
 * unit tests).  Chip iterates the batch in non-decreasing
 * sample_offset order, applies each register write at its offset, and
 * renders each segment with the resulting register state — exactly as
 * mGBA does with GBAAudioSample() + write.
 *
 * Caller convention:
 *   1. m4a_advance(drv, frames)             // queue events
 *   2. hw_audio_render_events(...)          // consume + render
 *   3. m4a_consume_writes(drv)              // clear queue + reset offset
 *
 * The driver's snapshot (M4ARegisterFile) is computed as a side effect
 * of CgbSound's same writes and remains queryable via
 * m4a_get_register_file() for non-timing consumers (UI, debug).  hw_audio
 * MUST NOT use the snapshot for timing-sensitive logic from this API
 * onwards — that defeats the whole point of the event stream. */
void hw_audio_render_events(HwAudio *hw,
                            const M4ARegWriteBatch *events,
                            const M4APcmRing *pcm,
                            float *outL, float *outR, int frames);

#ifdef __cplusplus
}
#endif

#endif
