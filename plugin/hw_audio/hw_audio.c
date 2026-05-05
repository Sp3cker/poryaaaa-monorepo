#include "hw_audio.h"
#include "hw_psg.h"
#include "hw_pcm.h"
#include "hw_mix.h"
#include "hw_resample.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Floor for the chip-internal render rate.  SOUNDBIAS sampling_cycle
 * 0/1/2 all pin to this floor (the resampler is always downsampling
 * to host); sampling_cycle = 3 bumps internal_rate to 262144 Hz per
 * plan §7b's `max(131072, quirk_rate)` rule.  See chip_internal_rate(). */
#define HW_AUDIO_INTERNAL_RATE_FLOOR 131072

/* Inner chunk size for the internal-rate render loop.  Each segment of
 * the host-rate render is broken into chunks of this many internal
 * samples; PSG/PCM/mix produce into the per-channel scratch buffers,
 * the resampler then drains to host.  Bounded to avoid bloating the
 * HwAudio struct — at HW_AUDIO_INTERNAL_CHUNK=1024 the six per-channel
 * scratch buffers add up to 24 KB. */
#define HW_AUDIO_INTERNAL_CHUNK 1024

struct HwAudio {
    float        host_rate;
    int          internal_rate;
    HwPsgSynth   psg;       /* sq1, sq2, wave, noise — render-rate synth */
    HwPcm        pcm;       /* two-stage drain: HwDmaToFifo + HwFifoDrain */
    HwMixBus     mix;       /* SOUNDCNT_L/H + SOUNDBIAS bias/clip stage */
    HwResample   resample;  /* internal_rate → host_rate (step 9) */

    /* Per-channel solo/mute mask.  Bits HW_AUDIO_SOLO_* gate whether
     * each channel's pre-mix buffer feeds hw_mix_render — masked-off
     * channels go in as NULL and contribute zero.  Used by the
     * mGBA-capture parity workflow (matches the patched mGBA tool's
     * channel set so a single name selects the same channel on both
     * sides). */
    uint32_t     solo_mask;

    /* Cumulative sample-clock trackers used to keep PSG/PCM/mix
     * advance in lock-step with host frames REQUESTED, regardless of
     * how the caller chunks render calls.  Each render call computes
     * a target total of internal samples derived ONLY from the
     * cumulative host-frame total, not the current call's frame count
     * — so e.g. one 4096-frame call and four 1024-frame calls feed
     * the same total internal samples (modulo ≤ 1 sample integer
     * rounding) and produce equivalent output.  Without this
     * accounting, each call's `round(frames * step) + lookahead`
     * would over-advance chip state by `lookahead` per call,
     * producing audible pitch/timing drift. */
    int64_t      total_inputs_pushed;     /* cumulative internal samples fed to resampler */
    int64_t      total_outputs_target;    /* cumulative host frames requested */

    /* Per-channel scratch at internal rate.  PSG synth writes 4, PCM
     * drain writes 2, mix bus consumes all 6 to produce stereo into
     * mix_l/mix_r.  Living on the chip struct avoids a multi-tens-of-
     * KB stack frame in render-event call sites. */
    float scratch_sq1  [HW_AUDIO_INTERNAL_CHUNK];
    float scratch_sq2  [HW_AUDIO_INTERNAL_CHUNK];
    float scratch_wave [HW_AUDIO_INTERNAL_CHUNK];
    float scratch_noise[HW_AUDIO_INTERNAL_CHUNK];
    float scratch_dma_a[HW_AUDIO_INTERNAL_CHUNK];
    float scratch_dma_b[HW_AUDIO_INTERNAL_CHUNK];
    float mix_l        [HW_AUDIO_INTERNAL_CHUNK];
    float mix_r        [HW_AUDIO_INTERNAL_CHUNK];

    /* Outstanding chip-side parity gates — see plan §12 blocking-gates list:
     *   - mGBA capture-comparison parity (step 10b) — self-consistency
     *     tests landed at §12.10a but match-against-reference is open */
};

/* SOUNDBIAS-derived quirk rate (the chip's actual DAC cadence).
 * 32768 / 65536 / 131072 / 262144 Hz for sampling_cycle 0 / 1 / 2 / 3.
 * Used by HwFifoDrain to bridge pcm_rate → internal mix rate; also
 * the floor argument to chip_internal_rate(). */
static int chip_quirk_rate(uint8_t sampling_cycle) {
    return 32768 << (sampling_cycle & 0x3);
}

/* Per plan §7b, the chip-internal render rate is
 *   internal_rate = max(131072, quirk_rate)
 * sampling_cycle = 0/1/2 → quirk 32k/65k/131k → internal pinned at
 * the floor 131072 Hz.  sampling_cycle = 3 → quirk 262144 → internal
 * bumps to 262144 Hz so the chip never has to downsample its own
 * synth output before the resampler hits host. */
static int chip_internal_rate(uint8_t sampling_cycle) {
    int q = chip_quirk_rate(sampling_cycle);
    return q > HW_AUDIO_INTERNAL_RATE_FLOOR ? q : HW_AUDIO_INTERNAL_RATE_FLOOR;
}

HwAudio *hw_audio_create(float host_sample_rate) {
    HwAudio *hw = (HwAudio *)calloc(1, sizeof(*hw));
    if (!hw) return NULL;
    hw->host_rate            = host_sample_rate;
    hw->solo_mask            = HW_AUDIO_SOLO_FULL;
    hw_mix_init(&hw->mix);   /* establishes default sampling_cycle = 0 */
    hw->internal_rate        = chip_internal_rate(hw->mix.sampling_cycle);
    hw->total_inputs_pushed  = 0;
    hw->total_outputs_target = 0;
    hw_psg_init(&hw->psg, (float)hw->internal_rate);
    hw_pcm_init(&hw->pcm, (float)hw->internal_rate);
    hw_pcm_set_quirk_rate(&hw->pcm, chip_quirk_rate(hw->mix.sampling_cycle));
    hw_resample_init(&hw->resample,
                     (double)hw->internal_rate, (double)host_sample_rate);
    return hw;
}

void hw_audio_destroy(HwAudio *hw) {
    free(hw);
}

int hw_audio_internal_rate(const HwAudio *hw) {
    return hw ? hw->internal_rate : 0;
}

void hw_audio_set_solo_mask(HwAudio *hw, uint32_t mask) {
    if (!hw) return;
    /* Empty mask would silence everything; treat as "no override
     * requested" → restore default full mix.  Clamp to the 6 valid
     * channel bits to ignore bits the caller doesn't know about. */
    uint32_t valid = mask & (uint32_t)HW_AUDIO_SOLO_FULL;
    hw->solo_mask = valid ? valid : (uint32_t)HW_AUDIO_SOLO_FULL;
}

uint32_t hw_audio_get_solo_mask(const HwAudio *hw) {
    return hw ? hw->solo_mask : (uint32_t)HW_AUDIO_SOLO_FULL;
}

void hw_audio_set_host_rate(HwAudio *hw, float hz) {
    if (!hw) return;
    hw->host_rate = hz;
    /* PSG/PCM/mix continue at the chip-internal rate; only the
     * resampler's output side changes when the host changes.  We
     * also reset the resampler state and our cumulative trackers
     * because the input/output rate ratio just changed — keeping the
     * old phase would map old internal samples to a new host rate
     * and create an audible glitch.  Callers that swap host rate
     * mid-stream get one block of resampler-warmup latency. */
    hw_resample_init(&hw->resample,
                     (double)hw->internal_rate, (double)hz);
    hw->total_inputs_pushed  = 0;
    hw->total_outputs_target = 0;
}

/* Render `internal_count` chip-internal samples through PSG → PCM →
 * mix-bus into mix_l/mix_r, feed them to the resampler, and drain up
 * to `max_host` host outputs into outL/outR + offset.  Returns the
 * number of host samples actually produced. */
static int render_internal_chunk(HwAudio *hw,
                                 const M4APcmRing *pcm_ring,
                                 float *outL, float *outR, int host_offset,
                                 int internal_count, int max_host) {
    if (internal_count <= 0 || max_host <= 0) return 0;

    hw_psg_render(&hw->psg,
                  hw->scratch_sq1, hw->scratch_sq2,
                  hw->scratch_wave, hw->scratch_noise,
                  internal_count);
    hw_pcm_render(&hw->pcm, pcm_ring,
                  hw->scratch_dma_a, hw->scratch_dma_b,
                  internal_count);
    /* Solo mask: zero the buffer pointer for any channel whose
     * solo bit is clear so hw_mix_render treats it as silent.  PSG
     * and PCM are still rendered unconditionally so their internal
     * state (envelopes, LFSR, pcm_pos, etc) stays in sync with the
     * cumulative input timeline regardless of solo selection. */
    const uint32_t m = hw->solo_mask;
    hw_mix_render(&hw->mix,
                  (m & HW_AUDIO_SOLO_SQ1)   ? hw->scratch_sq1   : NULL,
                  (m & HW_AUDIO_SOLO_SQ2)   ? hw->scratch_sq2   : NULL,
                  (m & HW_AUDIO_SOLO_WAVE)  ? hw->scratch_wave  : NULL,
                  (m & HW_AUDIO_SOLO_NOISE) ? hw->scratch_noise : NULL,
                  (m & HW_AUDIO_SOLO_DMA_A) ? hw->scratch_dma_a : NULL,
                  (m & HW_AUDIO_SOLO_DMA_B) ? hw->scratch_dma_b : NULL,
                  hw->mix_l, hw->mix_r, internal_count);

    return hw_resample_process(&hw->resample,
                               hw->mix_l, hw->mix_r, internal_count,
                               outL ? outL + host_offset : NULL,
                               outR ? outR + host_offset : NULL,
                               max_host);
}

/* Render a contiguous span of `seg_internal` chip-internal samples
 * (chunked at HW_AUDIO_INTERNAL_CHUNK).  Drains to outL/outR up to
 * `target_host - *rendered_host` host samples; *rendered_host advances
 * by however many the resampler produced. */
static void render_segment(HwAudio *hw,
                           const M4APcmRing *pcm_ring,
                           float *outL, float *outR,
                           int seg_internal,
                           int *rendered_host, int target_host) {
    int remaining = seg_internal;
    while (remaining > 0) {
        int chunk = remaining;
        if (chunk > HW_AUDIO_INTERNAL_CHUNK) chunk = HW_AUDIO_INTERNAL_CHUNK;

        int drain_max = target_host - *rendered_host;
        if (drain_max < 0) drain_max = 0;

        int produced = render_internal_chunk(hw, pcm_ring,
                                             outL, outR, *rendered_host,
                                             chunk, drain_max);
        *rendered_host += produced;
        remaining -= chunk;
    }
}

void hw_audio_render(HwAudio *hw,
                     M4ARegisterFile *regs,
                     const M4APcmRing *pcm,
                     float *outL, float *outR, int frames) {
    (void)hw; (void)pcm;

    /* Snapshot-driven render — superseded by hw_audio_render_events()
     * at Layer 1.5.  This function deliberately does NOT synthesise;
     * its only remaining job is to consume edge-trigger latches per
     * plan §6a so call sites that haven't migrated to the event API
     * still satisfy the driver→chip contract (the driver relies on
     * trigger_* clearing — e.g. trigger_sq2 must not refire on
     * subsequent vblanks without a fresh MO_VOL).  All real audio
     * goes through hw_audio_render_events(). */
    if (regs) {
        regs->trigger_sq1   = false;
        regs->trigger_sq2   = false;
        regs->trigger_wave  = false;
        regs->trigger_noise = false;
    }

    if (frames <= 0) return;
    if (outL) memset(outL, 0, (size_t)frames * sizeof(float));
    if (outR) memset(outR, 0, (size_t)frames * sizeof(float));
}

/* Map a cumulative host-output count to the cumulative internal-input
 * count required to produce them, given the resampler's step and
 * fixed initial-phase / lookahead constants.
 *
 * Resampler init is `output_pos = HW_RESAMPLE_TAPS/2 - 1` and the
 * produce-condition is `input_idx ≥ floor(output_pos) + HW_RESAMPLE_TAPS/2 + 1`.
 * For N outputs (N ≥ 1), the last output sits at output_pos =
 * (TAPS/2 - 1) + (N-1)*step, so the smallest input_idx that can produce
 * it is `floor((TAPS/2 - 1) + (N-1)*step) + (TAPS/2 + 1)`.
 *
 * Block-size invariance follows: cumulative inputs depends only on
 * cumulative N, not on per-call frame counts.  Per-call delta is just
 * the difference between two such totals — no floor is taken on the
 * intermediate per-call float, so the same total render produces the
 * same total internal samples regardless of how it's chunked. */
static int64_t inputs_for_total_outputs(int64_t total_outputs, double step) {
    if (total_outputs <= 0) return 0;
    const int64_t init_offset = (int64_t)(HW_RESAMPLE_TAPS / 2 - 1);  /* 15 */
    const int64_t lookahead   = (int64_t)(HW_RESAMPLE_TAPS / 2 + 1);  /* 17 */
    double frac_pos = (double)init_offset + (double)(total_outputs - 1) * step;
    int64_t floor_pos;
    if (frac_pos >= 0.0) floor_pos = (int64_t)frac_pos;
    else                 floor_pos = (int64_t)frac_pos
                                     - ((double)((int64_t)frac_pos) > frac_pos ? 1 : 0);
    return floor_pos + lookahead;
}

void hw_audio_render_events(HwAudio *hw,
                            const M4ARegWriteBatch *events,
                            const M4APcmRing *pcm,
                            float *outL, float *outR, int frames) {
    if (frames <= 0) return;
    if (!hw) {
        if (outL) memset(outL, 0, (size_t)frames * sizeof(float));
        if (outR) memset(outR, 0, (size_t)frames * sizeof(float));
        return;
    }
    if (hw->host_rate <= 0.0f || hw->internal_rate <= 0) {
        if (outL) memset(outL, 0, (size_t)frames * sizeof(float));
        if (outR) memset(outR, 0, (size_t)frames * sizeof(float));
        return;
    }

    /* Sync chip-internal render rate with SOUNDBIAS sampling_cycle.
     * The mix bus is the source of truth for SOUNDBIAS state — its
     * sampling_cycle field is updated by SOUNDBIAS events as they're
     * applied during render_events.  We snapshot the value at the
     * START of this call: if sampling_cycle changed since the last
     * call (e.g. driver setup wrote SOUNDBIAS in a previous render
     * call's events), update internal_rate, push to PSG/PCM, and
     * re-init the resampler for the new ratio.
     *
     * Boot-time-only target restriction: a SOUNDBIAS event applied
     * mid-call updates HwMixBus immediately but is NOT picked up by
     * the chip's rate machinery until the next render boundary.
     * This is an explicit scope choice — Pokemon Emerald and the
     * ROMhacks the v2 chip targets configure SOUNDBIAS at boot and
     * never change it during playback, so the snapshot-at-start-of-
     * call approach has no observable effect on the target catalogue.
     * Tests follow a setup-then-play pattern (one render call to
     * apply SOUNDBIAS, then real renders), and
     * test_chip_canned_soundbias_internal_rate_switches asserts this
     * boundary explicitly.  Lifting the restriction (mid-call rate
     * changes) would require flushing the resampler ring + rebuilding
     * cumulative trackers at the SOUNDBIAS event point — punted
     * until a real workload demands it. */
    int desired_internal_rate = chip_internal_rate(hw->mix.sampling_cycle);
    int desired_quirk_rate    = chip_quirk_rate(hw->mix.sampling_cycle);
    if (desired_internal_rate != hw->internal_rate) {
        hw->internal_rate = desired_internal_rate;
        hw_psg_set_render_rate(&hw->psg, (float)hw->internal_rate);
        hw_pcm_set_render_rate(&hw->pcm, (float)hw->internal_rate);
        /* Re-init resampler: kernel rebuilds for new ratio, ring
         * clears, output_pos resets.  Cumulative trackers reset so
         * the next call starts a fresh sample-clock relationship. */
        hw_resample_init(&hw->resample,
                         (double)hw->internal_rate, (double)hw->host_rate);
        hw->total_inputs_pushed  = 0;
        hw->total_outputs_target = 0;
    }
    /* quirk_rate tracks sampling_cycle independently of internal_rate
     * (sampling_cycle 0/1/2 all map to the same internal_rate floor of
     * 131072 but produce DIFFERENT quirk rates 32k/65k/131k that drive
     * HwFifoDrain's S&H cadence).  Always push the current quirk rate
     * — cheap if unchanged. */
    hw_pcm_set_quirk_rate(&hw->pcm, desired_quirk_rate);

    /* PCM publish-gate fallback for canned-mode callers.
     *
     * Production driver emits one M4A_REG_PCM_PUBLISH event per vblank
     * inside m4a_sound_main_ram, stamped with the vblank firing offset;
     * the chip's hw_pcm advances `pcm_published_through` when applying
     * these events, so reads from the ring stay clamped to data that
     * was actually published before the current sample_offset.
     *
     * Chip-only canned tests (and other callers that pre-populate
     * `ring->write_cursor` directly without going through the driver
     * event pipeline) never emit PUBLISH events.  For those calls,
     * snap `pcm_published_through` to the ring's `write_cursor` so
     * the entire pre-populated ring is readable from sample 0.
     *
     * Two discriminators must BOTH allow the snap:
     *   - this batch has no PCM_PUBLISH events, AND
     *   - publish_seen has never been latched (no PUBLISH ever fired).
     *
     * Without the first check, the very first production render call
     * would snap publish forward to write_cursor (which already
     * reflects ALL of m4a_advance's vblanks) AND apply each PUBLISH
     * event on top — double-counting.  Without the second, production
     * chunks shorter than a vblank period (no events) would re-snap
     * mid-stream and re-introduce the cross-vblank leak.  Both
     * canned-mode and post-first-PUBLISH-with-no-events-this-call
     * cases are handled cleanly by combining them. */
    bool has_publish = false;
    if (events) {
        for (size_t i = 0; i < events->count; i++) {
            if (events->events[i].reg == M4A_REG_PCM_PUBLISH) {
                has_publish = true;
                break;
            }
        }
    }
    if (!has_publish && !hw->pcm.publish_seen && pcm
        && pcm->write_cursor > hw->pcm.pcm_published_through) {
        hw->pcm.pcm_published_through = pcm->write_cursor;
    }

    /* Walk events in non-decreasing sample_offset order.  Each event's
     * sample_offset is in HOST frames; the chip-internal pipeline runs
     * at internal_rate, so we map host-offset → internal-offset via the
     * rate ratio.  Per segment we render a span of internal samples
     * through PSG → PCM → mix bus and feed them to the resampler,
     * which produces host samples on demand.  Apply each event to all
     * three subsystems at the segment boundary.
     *
     * Sample-clock accounting (step-9 fix): the count of internal
     * samples to render this call is derived from the *cumulative*
     * host-frame total, NOT this call's `frames`.  Each call advances
     * `total_outputs_target` by `frames` and renders just enough
     * internal samples to keep `total_inputs_pushed = inputs_for_total
     * _outputs(target)`.  This makes chip-time advance lock-step with
     * the host clock no matter how the caller chunks its render
     * window — one 4096-frame call vs four 1024-frame calls feed the
     * same total internal samples and produce equivalent output past
     * resampler warmup.
     *
     * Per stage:
     *   - PSG (sq1, sq2, wave, noise) consumes NRxx events for chans
     *     1-4 + NR52 master enable.  Synthesises at internal_rate.
     *   - PCM consumes only the FIFO drain; routing/scaling moved to
     *     mix bus when step 8 landed.  S&H at internal_rate.
     *   - HwMixBus consumes NR50/NR51, SOUNDCNT_H, SOUNDBIAS.  Combines
     *     the six mono buffers, applies bias-add+clip, produces stereo.
     *   - HwResample drains stereo internal-rate samples and produces
     *     stereo host-rate samples (windowed-sinc polyphase, §12 step 9). */
    const double  step                  = (double)hw->internal_rate / (double)hw->host_rate;
    const int64_t prev_outputs_target   = hw->total_outputs_target;
    const int64_t new_outputs_target    = prev_outputs_target + (int64_t)frames;
    const int64_t target_inputs_total   = inputs_for_total_outputs(new_outputs_target, step);
    int64_t       internal_to_render_64 = target_inputs_total - hw->total_inputs_pushed;
    if (internal_to_render_64 < 0) internal_to_render_64 = 0;
    const int     internal_to_render    = (int)internal_to_render_64;

    int rendered_host     = 0;
    int rendered_internal = 0;

    if (events) {
        for (size_t i = 0; i < events->count; i++) {
            const M4ARegWrite *ev = &events->events[i];
            int H = (int)ev->sample_offset;
            if (H > frames) H = frames;
            if (H < 0)      H = 0;

            /* Map event's host-offset H to its required cumulative
             * input count via the same formula, then convert to a
             * within-call offset by subtracting the cumulative count
             * pushed BEFORE this call. */
            int64_t event_target_outputs = prev_outputs_target + (int64_t)H;
            int64_t event_target_inputs  = inputs_for_total_outputs(event_target_outputs, step);
            int64_t target_within_call_64 = event_target_inputs - hw->total_inputs_pushed;
            if (target_within_call_64 > internal_to_render_64)
                target_within_call_64 = internal_to_render_64;
            if (target_within_call_64 < (int64_t)rendered_internal)
                target_within_call_64 = (int64_t)rendered_internal;
            int target_within_call = (int)target_within_call_64;

            int seg_internal = target_within_call - rendered_internal;
            if (seg_internal > 0) {
                /* Drain at most enough outputs to reach this event's
                 * host offset H within the current call. */
                render_segment(hw, pcm, outL, outR,
                               seg_internal, &rendered_host, H);
                rendered_internal += seg_internal;
            }

            hw_psg_apply_event(&hw->psg, ev);
            hw_pcm_apply_event(&hw->pcm, ev);
            hw_mix_apply_event(&hw->mix, ev);
        }
    }

    int tail_internal = internal_to_render - rendered_internal;
    if (tail_internal > 0) {
        render_segment(hw, pcm, outL, outR,
                       tail_internal, &rendered_host, frames);
    }

    /* Update cumulative trackers.  total_inputs_pushed advances by
     * exactly the count we fed to the resampler this call. */
    hw->total_inputs_pushed  += (int64_t)internal_to_render;
    hw->total_outputs_target  = new_outputs_target;

    /* If the resampler's start-of-session latency under-produced,
     * pad the remaining host samples with silence.  Only happens
     * during the first call's warmup region. */
    while (rendered_host < frames) {
        if (outL) outL[rendered_host] = 0.0f;
        if (outR) outR[rendered_host] = 0.0f;
        rendered_host++;
    }
}
