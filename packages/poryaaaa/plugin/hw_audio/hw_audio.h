#ifndef HW_AUDIO_H
#define HW_AUDIO_H

#include "m4a/m4a_register_file.h"
#include "m4a/m4a_pcm_ring.h"
#include "m4a/m4a_driver.h" /* M4ARegWriteBatch / M4ARegWrite types */

#ifdef __cplusplus
extern "C"
{
#endif

    /* GBA audio chip emulation.  Mirrors mGBA's gb_audio.c / gba_audio.c.
     *
     * Status (2026-07-09):
     *   - PSG square / wave / noise: synthesised at mGBA's
     *     SOUNDBIAS-selected DAC cadence (`32768 << sampling_cycle`),
     *     updated at register-event offsets.
     *   - PCM DirectSound: two-stage drain (§12 step 5 closed).
     *     HwDmaToFifo reads M4APcmRing at pcm_rate_hz; HwFifoDrain
     *     snapshots the FIFO head at the SOUNDBIAS-derived quirk rate
     *     (32k/65k/131k/262k Hz); output at internal_rate is the held
     *     quirk byte sign-extended.
     *   - Mix bus: full SOUNDCNT_L (NR50/NR51) + SOUNDCNT_H + SOUNDBIAS
     *     bias-add / clip pipeline at internal rate (§12 step 8 + 9).
     *   - Output frontend: streaming port of mGBA 0.10.5's bundled
     *     blip_buf 1.1.0 impulse kernel, clock mapping, clipping, and
     *     511/512 DC-blocking pole. Cumulative sample-clock accounting
     *     keeps output invariant under host block-size changes.
     *
     * Mono mGBA capture pairs prove the square/noise waveform and level
     * path used by the reference tools. Whole-engine parity still depends
     * on the song parser, PCM, wave, timing, and reverb paths. */
    typedef struct HwAudio HwAudio;

    HwAudio* hw_audio_create(float host_sample_rate);
    void hw_audio_destroy(HwAudio* hw);
    void hw_audio_set_host_rate(HwAudio* hw, float hz);

    /* DEBUG / TEST VISIBILITY ONLY - not part of the production chip
     * timing contract.  Returns the chip's current internal render rate
     * (PSG/PCM/mix synth rate, before the mGBA blip frontend).  This is
     * `32768 << sampling_cycle`: 32768, 65536, 131072, or 262144 Hz.
     * Exposed because it caught
     * a real class of cadence-
     * switching bug (a fixed-rate implementation that ignored
     * sampling_cycle would silently still produce audible output for
     * typical low-frequency test signals — only an explicit rate
     * assertion can prove the switch wired up).  Production callers
     * should not depend on this; the rate may change with future
     * SOUNDBIAS work or scope refinements.
     *
     * Reflects SOUNDBIAS sampling_cycle after prior writes and same-call
     * SOUNDBIAS events. */
    int hw_audio_internal_rate(const HwAudio* hw);

    /* Per-channel solo / mute mask for parity capture work.  Each bit
     * gates whether that channel's pre-mix buffer feeds the mix bus —
     * masked-off channels are passed as NULL to hw_mix_render, which
     * treats them as silent.  Channel names + bit positions match the
     * patched mGBA headless tool's `--solo` / `--mute` channel set
     * (`tools/mgba-reference/`), so a single name
     * can drive both sides of a parity comparison.
     *
     * SOUNDCNT_L / H routing + scaling and SOUNDBIAS bias-add still run
     * over whatever channels are enabled — soloing wave, for instance,
     * still gets you the wave channel routed/clipped exactly as the
     * full mix would route/clip it, just with the other channels
     * zeroed.  Default mask is `HW_AUDIO_SOLO_FULL` (all 6 bits set —
     * normal mix). */
    typedef enum
    {
        HW_AUDIO_SOLO_SQ1 = 1u << 0,
        HW_AUDIO_SOLO_SQ2 = 1u << 1,
        HW_AUDIO_SOLO_WAVE = 1u << 2,
        HW_AUDIO_SOLO_NOISE = 1u << 3,
        HW_AUDIO_SOLO_DMA_A = 1u << 4,
        HW_AUDIO_SOLO_DMA_B = 1u << 5,
        HW_AUDIO_SOLO_PSG = HW_AUDIO_SOLO_SQ1 | HW_AUDIO_SOLO_SQ2 | HW_AUDIO_SOLO_WAVE | HW_AUDIO_SOLO_NOISE,
        HW_AUDIO_SOLO_DSOUND = HW_AUDIO_SOLO_DMA_A | HW_AUDIO_SOLO_DMA_B,
        HW_AUDIO_SOLO_FULL = HW_AUDIO_SOLO_PSG | HW_AUDIO_SOLO_DSOUND,
    } HwAudioSoloBits;

    void hw_audio_set_solo_mask(HwAudio* hw, uint32_t mask);
    uint32_t hw_audio_get_solo_mask(const HwAudio* hw);

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
    void
    hw_audio_render(HwAudio* hw, M4ARegisterFile* regs, const M4APcmRing* pcm, float* outL, float* outR, int frames);

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
    void hw_audio_render_events(
        HwAudio* hw, const M4ARegWriteBatch* events, const M4APcmRing* pcm, float* outL, float* outR, int frames);

#ifdef __cplusplus
}
#endif

#endif
