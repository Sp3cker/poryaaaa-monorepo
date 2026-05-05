#include "hw_pcm.h"

#include <string.h>

void hw_pcm_init(HwPcm *pcm, float render_rate) {
    memset(pcm, 0, sizeof(*pcm));
    pcm->render_rate = render_rate;
    /* Default quirk_rate matches SOUNDBIAS sampling_cycle = 0
     * (32768 Hz, the Pokemon Emerald default).  HwAudio overrides
     * this when sampling_cycle changes. */
    pcm->quirk_rate  = 32768;
    /* Negative sentinels force the first render sample to populate
     * held_pcm/held_quirk from ring[0] / pcm head before any output
     * is produced.  Without this, the read-on-integer-crossing model
     * would skip index 0 entirely. */
    pcm->pcm_last_int   = -1;
    pcm->quirk_last_int = -1;
}

void hw_pcm_set_render_rate(HwPcm *pcm, float render_rate) {
    pcm->render_rate = render_rate;
}

void hw_pcm_set_quirk_rate(HwPcm *pcm, int quirk_rate) {
    if (quirk_rate <= 0) return;
    pcm->quirk_rate = quirk_rate;
}

void hw_pcm_apply_event(HwPcm *pcm, const M4ARegWrite *ev) {
    if (!pcm || !ev) return;
    /* PCM_PUBLISH advances the publish gate by one vblank's worth of
     * ring samples.  No payload — the increment is constant.  See
     * the field doc on `pcm_published_through` in hw_pcm.h and the
     * "DirectSound PCM event/ring timing" blocking gate in
     * HW_AUDIO_SCAFFOLD_PLAN.md. */
    if (ev->reg == M4A_REG_PCM_PUBLISH) {
        pcm->pcm_published_through += (uint64_t)M4A_PCM_SAMPLES_PER_VBLANK;
        pcm->publish_seen = true;
    }
    /* SOUNDCNT_H DMA routing/vol bits land on HwMixBus, not here. */
}

void hw_pcm_render(HwPcm *pcm, const M4APcmRing *ring,
                   float *out_a, float *out_b, int frames) {
    if (frames <= 0 || !ring) {
        if (out_a) memset(out_a, 0, (size_t)frames * sizeof(float));
        if (out_b) memset(out_b, 0, (size_t)frames * sizeof(float));
        return;
    }
    /* Sanity gate: only bail when rates are unusable.  Notably do NOT
     * bail on `ring->write_cursor == 0` — the per-sample loop is the
     * single source of truth for advancing pcm_pos and quirk_pos, and
     * skipping it here on the first few render calls (before the
     * driver's first vblank fires) leaves the clocks out of sync with
     * the cumulative input/output count and breaks chunk-size
     * invariance.  When write_cursor is 0 the publish gate naturally
     * blocks every read (pcm_published_through is also 0), so output
     * stays silent through the loop without an early return. */
    if (pcm->render_rate <= 0.0f || pcm->quirk_rate <= 0) {
        if (out_a) memset(out_a, 0, (size_t)frames * sizeof(float));
        if (out_b) memset(out_b, 0, (size_t)frames * sizeof(float));
        return;
    }

    /* Two-stage drain (§12 step 5, plan §6b):
     *
     *   M4APcmRing  ──[pcm_rate]──▶  held_pcm   (HwDmaToFifo head byte)
     *                                   │
     *                              ──[quirk_rate]──▶  held_quirk  (HwFifoDrain
     *                                                              quirk-rate
     *                                                              snapshot)
     *                                                     │
     *                                                ──[render_rate]──▶ output
     *
     * Per render sample: snapshot the CURRENT integer position of
     * each clock and re-populate the held byte if that integer has
     * advanced since the last sample.  The position then advances
     * for the next sample.  This "read-at-current-position" model is
     * what makes the first sample correctly read ring[0] (vs an
     * "advance-then-check-crossing" model that would skip index 0).
     *
     * For Pokemon Emerald (pcm = 13379 Hz, quirk = 32768 Hz) this
     * collapses behaviorally to a single S&H — the quirk cadence is
     * far above the pcm cadence, so every quirk tick re-snapshots
     * essentially the same head byte until pcm advances.  For
     * ROMhacks pushing pcm above quirk Nyquist, the quirk-rate
     * S&H acts as a low-pass at quirk/2, matching real DAC cadence. */
    double pcm_step   = (double)ring->pcm_rate_hz   / (double)pcm->render_rate;
    double quirk_step = (double)pcm->quirk_rate     / (double)pcm->render_rate;

    for (int i = 0; i < frames; i++) {
        /* HwDmaToFifo: read ring[floor(pcm_pos)] when published, then
         * advance the pcm-rate clock.  When pcm_int catches up to
         * pcm_published_through (FIFO underrun — driver hasn't
         * stamped this vblank yet, or its PUBLISH event lands later
         * within the current render call), hold the last byte AND
         * pause pcm_pos advancement.  pcm_pos's documented semantics
         * is "cumulative pcm-samples consumed" (see field doc), so
         * pausing it during underrun matches reality: when the FIFO
         * underflows, no new sample is consumed.  The quirk clock
         * still ticks (DAC keeps drawing at PCM rate) and presents
         * the held byte — same as real hardware. */
        int64_t pcm_int = (int64_t)pcm->pcm_pos;
        bool    pcm_published = (uint64_t)pcm_int < pcm->pcm_published_through;
        if (pcm_published && pcm_int != pcm->pcm_last_int) {
            size_t idx = (size_t)pcm_int % M4A_PCM_DMA_BUF_SIZE;
            pcm->held_pcm_a = ring->ring_a[idx];
            pcm->held_pcm_b = ring->ring_b[idx];
            pcm->pcm_last_int = pcm_int;
        }

        /* HwFifoDrain: snapshot held_pcm into held_quirk at quirk
         * cadence (whenever the quirk-rate clock's integer advances). */
        int64_t quirk_int = (int64_t)pcm->quirk_pos;
        if (quirk_int != pcm->quirk_last_int) {
            pcm->held_quirk_a = pcm->held_pcm_a;
            pcm->held_quirk_b = pcm->held_pcm_b;
            pcm->quirk_last_int = quirk_int;
        }

        /* Output: sign-extend s8 → float.  ±127 → ±~1.0.  Routing +
         * volume code applied by HwMixBus, not here. */
        if (out_a) out_a[i] = (float)pcm->held_quirk_a / 128.0f;
        if (out_b) out_b[i] = (float)pcm->held_quirk_b / 128.0f;

        /* Advance the quirk (DAC) clock unconditionally.  Advance
         * the pcm clock only when the FIFO had data to consume —
         * see comment above. */
        if (pcm_published) pcm->pcm_pos += pcm_step;
        pcm->quirk_pos += quirk_step;
    }
}
