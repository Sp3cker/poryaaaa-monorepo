#ifndef HW_PCM_H
#define HW_PCM_H

#include <stdbool.h>
#include <stdint.h>

#include "m4a/m4a_driver.h"     /* M4ARegWrite */
#include "m4a/m4a_pcm_ring.h"   /* M4APcmRing */

#ifdef __cplusplus
extern "C" {
#endif

/* DirectSound PCM path — mirrors mGBA's FIFO drain.
 *
 * ⚠ Self-consistency tested but mGBA / hardware parity NOT proven —
 * see plan §12.10b.  Don't make PCM-parity claims against v1 / mGBA
 * / hardware from this file until that gate closes.
 *
 * §12 step 5 (closed): two-stage chain per plan §6b.
 *
 *   M4APcmRing  ──[pcm_rate_hz]──▶  held_pcm_a/b   (HwDmaToFifo)
 *                                       │
 *                                  ──[quirk_rate]──▶  held_quirk_a/b
 *                                                       (HwFifoDrain)
 *                                                              │
 *                                                  ──[render_rate]──▶
 *                                                       per-sample output
 *
 *   HwDmaToFifo:  reads M4APcmRing at pcm_rate_hz (≈13379 Hz for
 *                 Pokemon Emerald).  The FIFO head byte (held_pcm_a /
 *                 held_pcm_b) updates whenever the pcm-rate clock
 *                 crosses an integer.  pcm_pos shared between A and B.
 *   HwFifoDrain:  snapshots the FIFO head byte at the SOUNDBIAS-
 *                 derived quirk_rate (32k/65k/131k/262k Hz) into
 *                 held_quirk_a / held_quirk_b.  This is the bridging
 *                 stage between pcm_rate (driver clock) and the chip's
 *                 internal mix rate.  When pcm_rate > quirk_rate / 2
 *                 (ROMhacks pushing high PCM rates), the quirk-rate
 *                 S&H low-pass-filters the PCM at quirk Nyquist —
 *                 matching real DAC cadence.
 *   Output:       held_quirk sign-extended to ±~1.0, held across
 *                 render samples within the same quirk-rate interval.
 *
 * For Pokemon Emerald defaults (pcm=13379, quirk=32768) the two
 * stages collapse behaviourally to a single S&H — the quirk cadence
 * is far above pcm cadence, so nearly every quirk tick re-snapshots
 * the same head byte until pcm advances.  The split matters for
 * ROMhacks setting non-default pcm_rate or sampling_cycle.
 *
 * Open gates remaining in plan §12 "blocking gates":
 *   - step 10b — mGBA capture-comparison parity (self-consistency
 *     landed at §12.10a but doesn't prove match against reference).
 *   - steps 1-2 audit — driver-side `MPlayMain` / `ply_*` + LFO/MODT
 *     and PCM/reverb verification before whole-song A/B. */

typedef struct {
    /* HwDmaToFifo state — the FIFO's currently-presented byte.
     * M4APcmRing is read at pcm_rate_hz cadence; the head byte
     * persists across pcm-rate intervals (real GBA holds the FIFO
     * head until the next DMA refill).
     *
     * pcm_pos is monotonic (cumulative pcm-samples consumed) so it
     * survives across hw_audio_render_events boundaries — no glitch
     * every host buffer.  M4APcmRing's write_cursor advances
     * independently; we read at floor(pcm_pos) % M4A_PCM_DMA_BUF_SIZE.
     * pcm_pos shared between A and B (single pcm-rate clock; the two
     * FIFOs differ only in which ring side they track).
     *
     * pcm_last_int caches the integer index for which held_pcm was
     * most recently populated.  Initialized to a negative sentinel
     * so the first render sample (pcm_pos = 0) triggers a fresh read
     * of ring[0] — without the sentinel, an "advance-then-compare"
     * model would skip ring[0] entirely (the first integer crossing
     * would be 0→1, missing index 0). */
    double  pcm_pos;
    int64_t pcm_last_int;
    int8_t  held_pcm_a;      /* most recent byte read from ring_a at pcm_rate */
    int8_t  held_pcm_b;      /* most recent byte read from ring_b at pcm_rate */

    /* HwFifoDrain state — byte sampled from the FIFO head at the
     * SOUNDBIAS-derived quirk_rate cadence.  This is the bridging
     * stage between pcm_rate (driver clock) and the chip's internal
     * mix rate: the byte is held across quirk-rate intervals; render
     * samples within the same quirk-rate interval get the same
     * value.  When pcm_rate > quirk_rate / 2 (e.g. ROMhacks pushing
     * higher PCM rates than vanilla Pokemon Emerald), the quirk-rate
     * S&H low-pass-filters the PCM at quirk Nyquist — matching real
     * hardware's DAC cadence.
     *
     * quirk_last_int has the same negative-sentinel semantics as
     * pcm_last_int, so the first render sample (quirk_pos = 0)
     * triggers a snapshot at quirk index 0. */
    double  quirk_pos;
    int64_t quirk_last_int;
    int8_t  held_quirk_a;
    int8_t  held_quirk_b;

    /* Render rate (chip-internal).  Driven by HwAudio: this is the
     * chip-internal rate `max(131072, 32768 << sampling_cycle)` —
     * 131072 Hz for SOUNDBIAS sampling_cycle 0/1/2; 262144 Hz for
     * sampling_cycle 3 — NOT the host rate.  Output is then resampled
     * to host by hw_resample.c. */
    float   render_rate;

    /* SOUNDBIAS-derived quirk rate (32k/65k/131k/262k Hz) at which
     * HwFifoDrain samples the FIFO head.  Driven by HwAudio when
     * sampling_cycle changes between render calls. */
    int     quirk_rate;

    /* Publish gate — cumulative PCM samples the driver has stamped
     * as "available" via M4A_REG_PCM_PUBLISH events.  Each PUBLISH
     * event advances by M4A_PCM_SAMPLES_PER_VBLANK at its
     * sample_offset; reads from the ring are clamped to
     * `floor(pcm_pos) < pcm_published_through`.  When the read
     * clock overruns (PUBLISH events lagging behind pcm_pos), the
     * FIFO underruns and we hold the last byte read — matches
     * real-hardware DAC behaviour when DMA stalls. */
    uint64_t pcm_published_through;

    /* True once the chip has ever received a PCM_PUBLISH event.
     * Used to lock the chip into event-driven publish mode after
     * the driver has spoken — once any PUBLISH has been applied,
     * subsequent calls (including ones whose batch happens to
     * contain no PUBLISH events, e.g. when a host chunk is shorter
     * than a vblank period) must NOT fall back to the canned-mode
     * write_cursor snap.  Chip-only canned tests that pre-populate
     * `ring->write_cursor` without driver involvement keep
     * publish_seen=false and rely on the per-call fallback in
     * hw_audio_render_events. */
    bool publish_seen;
} HwPcm;

void hw_pcm_init(HwPcm *pcm, float render_rate);
void hw_pcm_set_render_rate(HwPcm *pcm, float render_rate);
void hw_pcm_set_quirk_rate(HwPcm *pcm, int quirk_rate);

/* Apply the relevant subset of M4ARegWrite events.  PCM presently
 * needs no event subscriptions — SOUNDCNT_H DMA routing + volume
 * codes belong to HwMixBus (see hw_mix.h).  Retained as a no-op
 * for symmetry with hw_psg_apply_event, in case future PCM-side
 * registers (e.g. FIFO_A/B writes from CPU) need to land here. */
void hw_pcm_apply_event(HwPcm *pcm, const M4ARegWrite *ev);

/* Render `frames` chip-internal-rate per-FIFO mono samples through
 * the two-stage drain: HwDmaToFifo reads M4APcmRing.ring_a/ring_b
 * at pcm_rate_hz cadence, HwFifoDrain snapshots the head at
 * quirk_rate cadence, output is the held byte sign-extended and
 * held across render samples.  Outputs are dipolar in [-1, +1]
 * (s8 / 128.0) with NO routing or volume applied — those are
 * HwMixBus's job.  Buffers are OVERWRITTEN, not summed.  Pass NULL
 * for either to skip its work. */
void hw_pcm_render(HwPcm *pcm, const M4APcmRing *ring,
                   float *out_a, float *out_b, int frames);

#ifdef __cplusplus
}
#endif

#endif
