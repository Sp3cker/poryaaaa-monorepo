#ifndef HW_AUDIO_H
#define HW_AUDIO_H

#include "m4a/m4a_register_file.h"
#include "m4a/m4a_pcm_ring.h" /* debug snapshot API only */
#include "m4a/m4a_driver.h"   /* M4ARegWriteBatch / M4ARegWrite types */
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* GBA audio chip emulation. Mirrors mGBA's gb_audio.c / gba_audio.c.
     *
     * Status (2026-08-13):
     *   - PSG square / wave / noise: synthesised at mGBA's
     *     SOUNDBIAS-selected DAC cadence (`32768 << sampling_cycle`),
     *     updated at register-event offsets.
     *   - PCM DirectSound: canonical FIFO A/B words and explicit TIMER 0/1
     *     events. The live chip and trace replay share the GBA's modulo-8
     *     word FIFO, little-endian byte shifts, and empty shift-register
     *     behavior; no production path reads the m4a software ring.
     *   - Output frontend: the installed mGBA 0.10.5 fixed blip path. Native
     *     stereo PCM16 is submitted at SOUNDBIAS DAC timestamps, then paired
     *     PCM16 is converted to host floats only after the exact read.
     *
     * Mono mGBA capture pairs prove the square/noise waveform and level
     * path used by the reference tools. Whole-engine parity still depends
     * on the song parser, PCM, wave, timing, and reverb paths. */
    typedef struct HwAudio HwAudio;

    HwAudio* hw_audio_create(float host_sample_rate);
    void hw_audio_destroy(HwAudio* hw);

    /* Reinitializes chip runtime in place without allocating. */
    void hw_audio_reset(HwAudio* hw);

    /* Copies the shared PSG clock, host-frame deadline phase, and mixer
     * routing state without copying oscillator lanes, PCM FIFO, or frontend
     * audio. */
    void hw_audio_sync_psg_timing(HwAudio* destination, const HwAudio* source);

    /* Aligns a reset sidecar frontend with the source timeline without
     * copying source PCM16. Queued source presentation time is represented
     * as silence, so solo/invert output never contains primary mix samples. */
    void hw_audio_sync_sidecar_frontend_timing(HwAudio* destination, const HwAudio* source);

    /* Copies one active PSG lane's oscillator, envelope, sweep/LFSR, length,
     * and routing runtime without emitting a trigger.  `lane` is sq1..noise
     * (0..3). */
    void hw_audio_clone_psg_lane(HwAudio* destination, const HwAudio* source, int lane);

    /* DEBUG / TEST VISIBILITY ONLY - not part of the production chip
     * timing contract. Returns the chip's current internal render rate
     * (PSG/PCM/mix synth rate, before the fixed mGBA PCM16 frontend). It is
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

    /* Explicit optional pre-deposition antialias filter. It is disabled by
     * default, is not part of installed-mGBA parity, and does not replace the
     * fixed PCM16 blip frontend. */
    void hw_audio_set_resample_antialias(HwAudio* hw, int enabled);

    /* DEBUG / TEST ONLY. This snapshot latch-consumer is intentionally not
     * a production frontend; production callers must use
     * hw_audio_render_events(). */
    void
    hw_audio_render(HwAudio* hw, M4ARegisterFile* regs, const M4APcmRing* pcm, float* outL, float* outR, int frames);

    /* Cycle-domain production API. Direct runtimes own paired M4ADriver and
     * HwAudio instances. `events` carries an explicit absolute [begin_cycle,
     * end_cycle] interval. The renderer advances chip state to every ordered
     * event cycle before applying it; `frames` only sizes the public host
     * output buffer and never timestamps a register write.
     *
     * The first interval after creation or reset must begin at cycle zero.
     * A nonzero seek has no recoverable host-frame phase in this API and is
     * rendered as silence; callers must reset and replay the preceding
     * interval instead.
     *
     * Canonical direct-render sequence:
     *   1. m4a_advance(drv, frames)              // queue cycle writes
     *   2. m4a_get_pending_writes(drv)           // obtain that write batch
     *   3. hw_audio_render_events(...)           // render the batch interval
     *   4. m4a_consume_writes(drv)               // begin the next interval
     *
     * The register snapshot is observable for UI/debug only; hardware timing
     * consumes the ordered event stream. */
    void hw_audio_render_events(HwAudio* hw, const M4ARegWriteBatch* events, float* outL, float* outR, int frames);

#ifdef __cplusplus
}
#endif

#endif
