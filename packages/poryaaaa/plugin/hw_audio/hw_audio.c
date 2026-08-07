#include "hw_audio.h"
#include "hw_psg.h"
#include "hw_pcm.h"
#include "hw_mix.h"
#include "hw_resample.h"

#include <stdlib.h>
#include <string.h>

/* Inner chunk size for the internal-rate render loop.  Each segment of
 * the host-rate render is broken into chunks of this many internal
 * samples; PSG/PCM/mix produce into the per-channel scratch buffers,
 * the resampler then drains to host.  Bounded to avoid bloating the
 * HwAudio struct — at HW_AUDIO_INTERNAL_CHUNK=1024 the six per-channel
 * scratch buffers add up to 24 KB. */
#define HW_AUDIO_INTERNAL_CHUNK 1024

struct HwAudio
{
    float host_rate;
    int internal_rate;
    HwPsgSynth psg;      /* sq1, sq2, wave, noise — render-rate synth */
    HwPcm pcm;           /* two-stage drain: HwDmaToFifo + HwFifoDrain */
    HwMixBus mix;        /* SOUNDCNT_L/H + SOUNDBIAS bias/clip stage */
    HwResample resample; /* DAC cadence -> host rate through current mGBA's sinc frontend */

    /* Per-channel solo/mute mask.  Bits HW_AUDIO_SOLO_* gate whether
     * each channel's pre-mix buffer feeds hw_mix_render — masked-off
     * channels go in as NULL and contribute zero.  Used by the
     * mGBA-capture parity workflow (matches the patched mGBA tool's
     * channel set so a single name selects the same channel on both
     * sides). */
    uint32_t solo_mask;

    /* Per-channel scratch at internal rate.  PSG synth writes 4, PCM
     * drain writes 2, mix bus consumes all 6 to produce stereo into
     * mix_l/mix_r.  Living on the chip struct avoids a multi-tens-of-
     * KB stack frame in render-event call sites. */
    float scratch_sq1[HW_AUDIO_INTERNAL_CHUNK];
    float scratch_sq2[HW_AUDIO_INTERNAL_CHUNK];
    float scratch_wave[HW_AUDIO_INTERNAL_CHUNK];
    float scratch_noise[HW_AUDIO_INTERNAL_CHUNK];
    float scratch_dma_a[HW_AUDIO_INTERNAL_CHUNK];
    float scratch_dma_b[HW_AUDIO_INTERNAL_CHUNK];
    float mix_l[HW_AUDIO_INTERNAL_CHUNK];
    float mix_r[HW_AUDIO_INTERNAL_CHUNK];
};

/* SOUNDBIAS-derived quirk rate.
 * 32768 / 65536 / 131072 / 262144 Hz for sampling_cycle 0 / 1 / 2 / 3.
 * Used by HwFifoDrain to sample the PCM FIFO head. */
static int chip_quirk_rate(uint8_t sampling_cycle)
{
    return 32768 << (sampling_cycle & 0x3);
}

/* mGBA samples the complete GBA mix at the SOUNDBIAS-selected DAC cadence. */
static int chip_internal_rate(uint8_t sampling_cycle)
{
    return chip_quirk_rate(sampling_cycle);
}

/* Apply SOUNDBIAS cadence changes to every clocked audio component. */
static void hw_audio_sync_rates_from_mix(HwAudio* hw)
{
    int desired_internal_rate = chip_internal_rate(hw->mix.sampling_cycle);
    int desired_quirk_rate = chip_quirk_rate(hw->mix.sampling_cycle);

    if (desired_internal_rate != hw->internal_rate)
    {
        hw->internal_rate = desired_internal_rate;
        hw_psg_set_render_rate(&hw->psg, (float)hw->internal_rate);
        hw_pcm_set_render_rate(&hw->pcm, (float)hw->internal_rate);

        /* Rate changes define a new resampler epoch.  The old ring was
         * filled at the previous input cadence, so keep the transition
         * local by flushing and rebuilding here. */
        hw_resample_init(&hw->resample, (double)hw->internal_rate, (double)hw->host_rate);
    }

    hw_pcm_set_quirk_rate(&hw->pcm, desired_quirk_rate);
}

HwAudio* hw_audio_create(float host_sample_rate)
{
    HwAudio* hw = (HwAudio*)calloc(1, sizeof(*hw));
    if (!hw)
        return NULL;
    hw->host_rate = host_sample_rate;
    hw->solo_mask = HW_AUDIO_SOLO_FULL;
    hw_mix_init(&hw->mix); /* establishes m4a's sampling_cycle = 1 */
    hw->internal_rate = chip_internal_rate(hw->mix.sampling_cycle);
    hw_psg_init(&hw->psg, (float)hw->internal_rate);
    hw_pcm_init(&hw->pcm, (float)hw->internal_rate);
    hw_pcm_set_quirk_rate(&hw->pcm, chip_quirk_rate(hw->mix.sampling_cycle));
    hw_resample_init(&hw->resample, (double)hw->internal_rate, (double)host_sample_rate);
    return hw;
}

void hw_audio_reset(HwAudio* hw)
{
    if (!hw)
        return;
    hw_mix_init(&hw->mix);
    hw->internal_rate = chip_internal_rate(hw->mix.sampling_cycle);
    hw_psg_init(&hw->psg, (float)hw->internal_rate);
    hw_pcm_init(&hw->pcm, (float)hw->internal_rate);
    hw_pcm_set_quirk_rate(&hw->pcm, chip_quirk_rate(hw->mix.sampling_cycle));
    hw_resample_init(&hw->resample, (double)hw->internal_rate, (double)hw->host_rate);
}

void hw_audio_sync_psg_timing(HwAudio* destination, const HwAudio* source)
{
    if (!destination || !source || destination == source)
        return;

    destination->mix = source->mix;
    if (destination->internal_rate != source->internal_rate)
    {
        destination->internal_rate = source->internal_rate;
        hw_resample_init(&destination->resample, (double)destination->internal_rate, (double)destination->host_rate);
    }
    hw_psg_set_render_rate(&destination->psg, source->psg.render_rate);
    hw_pcm_set_render_rate(&destination->pcm, source->pcm.render_rate);
    hw_pcm_set_quirk_rate(&destination->pcm, source->pcm.quirk_rate);
    destination->psg.master_enabled = source->psg.master_enabled;
    destination->psg.frame_seq_step = source->psg.frame_seq_step;
    destination->psg.frame_seq_accum = source->psg.frame_seq_accum;
    destination->psg.frame_seq_ticks = source->psg.frame_seq_ticks;
    destination->psg.frame_seq_length_ticks = source->psg.frame_seq_length_ticks;
    destination->psg.frame_seq_sweep_ticks = source->psg.frame_seq_sweep_ticks;
    destination->psg.frame_seq_envelope_ticks = source->psg.frame_seq_envelope_ticks;
}

void hw_audio_clone_psg_lane(HwAudio* destination, const HwAudio* source, int lane)
{
    if (!destination || !source || destination == source || lane < 0 || lane > 3)
        return;

    hw_audio_sync_psg_timing(destination, source);
    switch (lane)
    {
    case 0:
        destination->psg.sq1_timer_cycles = source->psg.sq1_timer_cycles;
        destination->psg.sq1_duty_index = source->psg.sq1_duty_index;
        destination->psg.sq1_freq = source->psg.sq1_freq;
        destination->psg.sq1_sweep_shadow_freq = source->psg.sq1_sweep_shadow_freq;
        destination->psg.sq1_sweep_time = source->psg.sq1_sweep_time;
        destination->psg.sq1_sweep_shift = source->psg.sq1_sweep_shift;
        destination->psg.sq1_sweep_timer = source->psg.sq1_sweep_timer;
        destination->psg.sq1_sweep_decrease = source->psg.sq1_sweep_decrease;
        destination->psg.sq1_sweep_enabled = source->psg.sq1_sweep_enabled;
        destination->psg.sq1_sweep_occurred = source->psg.sq1_sweep_occurred;
        destination->psg.sq1_duty = source->psg.sq1_duty;
        destination->psg.sq1_length_counter = source->psg.sq1_length_counter;
        destination->psg.sq1_length_enabled = source->psg.sq1_length_enabled;
        destination->psg.sq1_envelope = source->psg.sq1_envelope;
        destination->psg.sq1_dac_enabled = source->psg.sq1_dac_enabled;
        destination->psg.sq1_enabled = source->psg.sq1_enabled;
        break;
    case 1:
        destination->psg.sq2_timer_cycles = source->psg.sq2_timer_cycles;
        destination->psg.sq2_duty_index = source->psg.sq2_duty_index;
        destination->psg.sq2_freq = source->psg.sq2_freq;
        destination->psg.sq2_duty = source->psg.sq2_duty;
        destination->psg.sq2_length_counter = source->psg.sq2_length_counter;
        destination->psg.sq2_length_enabled = source->psg.sq2_length_enabled;
        destination->psg.sq2_envelope = source->psg.sq2_envelope;
        destination->psg.sq2_enabled = source->psg.sq2_enabled;
        break;
    case 2:
        destination->psg.wave_phase = source->psg.wave_phase;
        destination->psg.wave_freq = source->psg.wave_freq;
        destination->psg.wave_vol_code = source->psg.wave_vol_code;
        destination->psg.wave_enabled = source->psg.wave_enabled;
        destination->psg.wave_dac_on = source->psg.wave_dac_on;
        destination->psg.wave_length_counter = source->psg.wave_length_counter;
        destination->psg.wave_length_enabled = source->psg.wave_length_enabled;
        memcpy(destination->psg.wave_ram, source->psg.wave_ram, sizeof(destination->psg.wave_ram));
        break;
    case 3:
        destination->psg.noise_lfsr = source->psg.noise_lfsr;
        destination->psg.noise_phase = source->psg.noise_phase;
        destination->psg.noise_clock_shift = source->psg.noise_clock_shift;
        destination->psg.noise_divisor_code = source->psg.noise_divisor_code;
        destination->psg.noise_last_sample = source->psg.noise_last_sample;
        destination->psg.noise_width_7bit = source->psg.noise_width_7bit;
        destination->psg.noise_envelope = source->psg.noise_envelope;
        destination->psg.noise_enabled = source->psg.noise_enabled;
        destination->psg.noise_length_counter = source->psg.noise_length_counter;
        destination->psg.noise_length_enabled = source->psg.noise_length_enabled;
        break;
    }
}

void hw_audio_destroy(HwAudio* hw)
{
    free(hw);
}

int hw_audio_internal_rate(const HwAudio* hw)
{
    return hw ? hw->internal_rate : 0;
}

void hw_audio_set_solo_mask(HwAudio* hw, uint32_t mask)
{
    if (!hw)
        return;
    /* Empty mask would silence everything; treat as "no override
     * requested" → restore default full mix.  Clamp to the 6 valid
     * channel bits to ignore bits the caller doesn't know about. */
    uint32_t valid = mask & (uint32_t)HW_AUDIO_SOLO_FULL;
    hw->solo_mask = valid ? valid : (uint32_t)HW_AUDIO_SOLO_FULL;
}

uint32_t hw_audio_get_solo_mask(const HwAudio* hw)
{
    return hw ? hw->solo_mask : (uint32_t)HW_AUDIO_SOLO_FULL;
}

void hw_audio_set_host_rate(HwAudio* hw, float hz)
{
    if (!hw)
        return;
    hw->host_rate = hz;
    /* PSG/PCM/mix continue at the chip-internal rate; only the
     * resampler's output side changes when the host changes.  We
     * also reset the resampler state and our cumulative trackers
     * because the input/output rate ratio just changed — keeping the
     * old phase would map old internal samples to a new host rate
     * and create an audible glitch.  Callers that swap host rate
     * mid-stream get one block of resampler-warmup latency. */
    hw_resample_init(&hw->resample, (double)hw->internal_rate, (double)hz);
}

/* Render `internal_count` chip-internal samples through PSG → PCM →
 * mix-bus into mix_l/mix_r, feed them to the resampler, and drain up
 * to `max_host` host outputs into outL/outR + offset.  Returns the
 * number of host samples actually produced. */
static int render_internal_chunk(HwAudio* hw,
                                 const M4APcmRing* pcm_ring,
                                 float* outL,
                                 float* outR,
                                 int host_offset,
                                 int internal_count,
                                 int max_host)
{
    if (internal_count <= 0 || max_host <= 0)
        return 0;

    hw_psg_render(&hw->psg, hw->scratch_sq1, hw->scratch_sq2, hw->scratch_wave, hw->scratch_noise, internal_count);
    hw_pcm_render(&hw->pcm, pcm_ring, hw->scratch_dma_a, hw->scratch_dma_b, internal_count);
    /* Solo mask: zero the buffer pointer for any channel whose
     * solo bit is clear so hw_mix_render treats it as silent.  PSG
     * and PCM are still rendered unconditionally so their internal
     * state (envelopes, LFSR, pcm_pos, etc) stays in sync with the
     * cumulative input timeline regardless of solo selection. */
    const uint32_t m = hw->solo_mask;
    hw_mix_render(&hw->mix,
                  (m & HW_AUDIO_SOLO_SQ1) ? hw->scratch_sq1 : NULL,
                  (m & HW_AUDIO_SOLO_SQ2) ? hw->scratch_sq2 : NULL,
                  (m & HW_AUDIO_SOLO_WAVE) ? hw->scratch_wave : NULL,
                  (m & HW_AUDIO_SOLO_NOISE) ? hw->scratch_noise : NULL,
                  (m & HW_AUDIO_SOLO_DMA_A) ? hw->scratch_dma_a : NULL,
                  (m & HW_AUDIO_SOLO_DMA_B) ? hw->scratch_dma_b : NULL,
                  hw->mix_l,
                  hw->mix_r,
                  internal_count);

    return hw_resample_process(&hw->resample,
                               hw->mix_l,
                               hw->mix_r,
                               internal_count,
                               outL ? outL + host_offset : NULL,
                               outR ? outR + host_offset : NULL,
                               max_host);
}

/* Render a contiguous span of `seg_internal` chip-internal samples
 * (chunked at HW_AUDIO_INTERNAL_CHUNK).  Drains to outL/outR up to
 * `target_host - *rendered_host` host samples; *rendered_host advances
 * by however many the resampler produced. */
static void render_segment(HwAudio* hw,
                           const M4APcmRing* pcm_ring,
                           float* outL,
                           float* outR,
                           int seg_internal,
                           int* rendered_host,
                           int target_host)
{
    int remaining = seg_internal;
    while (remaining > 0)
    {
        int chunk = remaining;
        if (chunk > HW_AUDIO_INTERNAL_CHUNK)
            chunk = HW_AUDIO_INTERNAL_CHUNK;

        int drain_max = target_host - *rendered_host;
        if (drain_max < 0)
            drain_max = 0;

        int produced = render_internal_chunk(hw, pcm_ring, outL, outR, *rendered_host, chunk, drain_max);
        *rendered_host += produced;
        remaining -= chunk;
    }
}

void hw_audio_render(HwAudio* hw, M4ARegisterFile* regs, const M4APcmRing* pcm, float* outL, float* outR, int frames)
{
    (void)hw;
    (void)pcm;

    /* Snapshot-driven render — superseded by hw_audio_render_events()
     * at Layer 1.5.  This function deliberately does NOT synthesise;
     * its only remaining job is to consume edge-trigger latches per
     * plan §6a so call sites that haven't migrated to the event API
     * still satisfy the driver→chip contract (the driver relies on
     * trigger_* clearing — e.g. trigger_sq2 must not refire on
     * subsequent vblanks without a fresh MO_VOL).  All real audio
     * goes through hw_audio_render_events(). */
    if (regs)
    {
        regs->trigger_sq1 = false;
        regs->trigger_sq2 = false;
        regs->trigger_wave = false;
        regs->trigger_noise = false;
    }

    if (frames <= 0)
        return;
    if (outL)
        memset(outL, 0, (size_t)frames * sizeof(float));
    if (outR)
        memset(outR, 0, (size_t)frames * sizeof(float));
}

static void render_to_host_offset(
    HwAudio* hw, const M4APcmRing* pcm_ring, float* outL, float* outR, int target_host, int* rendered_host)
{
    int host_delta = target_host - *rendered_host;
    if (host_delta <= 0)
        return;

    int produced = hw_resample_process(&hw->resample,
                                       NULL,
                                       NULL,
                                       0,
                                       outL ? outL + *rendered_host : NULL,
                                       outR ? outR + *rendered_host : NULL,
                                       host_delta);
    *rendered_host += produced;

    int remaining = target_host - *rendered_host;
    if (remaining > 0)
    {
        int internal_to_render = hw_resample_inputs_needed(&hw->resample, remaining);
        render_segment(hw, pcm_ring, outL, outR, internal_to_render, rendered_host, target_host);
    }

    /* Preserve the event timeline when a freshly reset resampler has
     * startup latency.  Later segments must start writing at target_host,
     * not at an earlier index. */
    while (*rendered_host < target_host)
    {
        if (outL)
            outL[*rendered_host] = 0.0f;
        if (outR)
            outR[*rendered_host] = 0.0f;
        (*rendered_host)++;
    }
}

void hw_audio_render_events(
    HwAudio* hw, const M4ARegWriteBatch* events, const M4APcmRing* pcm, float* outL, float* outR, int frames)
{
    if (frames <= 0)
        return;
    if (!hw)
    {
        if (outL)
            memset(outL, 0, (size_t)frames * sizeof(float));
        if (outR)
            memset(outR, 0, (size_t)frames * sizeof(float));
        return;
    }
    if (hw->host_rate <= 0.0f || hw->internal_rate <= 0)
    {
        if (outL)
            memset(outL, 0, (size_t)frames * sizeof(float));
        if (outR)
            memset(outR, 0, (size_t)frames * sizeof(float));
        return;
    }

    /* Catch any SOUNDBIAS sampling_cycle written by a prior call before
     * rendering this call's first span.  Same-call SOUNDBIAS events are
     * handled inside the event loop after HwMixBus consumes the event. */
    hw_audio_sync_rates_from_mix(hw);

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
    if (events)
    {
        for (size_t i = 0; i < events->count; i++)
        {
            if (events->events[i].reg == M4A_REG_PCM_PUBLISH)
            {
                has_publish = true;
                break;
            }
        }
    }
    if (!has_publish && !hw->pcm.publish_seen && pcm && pcm->write_cursor > hw->pcm.pcm_published_through)
    {
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
     * Per stage:
     *   - PSG (sq1, sq2, wave, noise) consumes NRxx events for chans
     *     1-4 + NR52 master enable.  Synthesises at internal_rate.
     *   - PCM consumes only the FIFO drain; routing/scaling moved to
     *     mix bus when step 8 landed.  S&H at internal_rate.
     *   - HwMixBus consumes NR50/NR51, SOUNDCNT_H, SOUNDBIAS.  Combines
     *     the six mono buffers, applies bias-add+clip, produces stereo.
     *   - HwResample drains stereo DAC samples through current mGBA's sinc frontend. */
    int rendered_host = 0;

    if (events)
    {
        for (size_t i = 0; i < events->count; i++)
        {
            const M4ARegWrite* ev = &events->events[i];
            int H = (int)ev->sample_offset;
            if (H > frames)
                H = frames;
            if (H < 0)
                H = 0;

            render_to_host_offset(hw, pcm, outL, outR, H, &rendered_host);

            hw_psg_apply_event(&hw->psg, ev);
            hw_pcm_apply_event(&hw->pcm, ev);
            hw_mix_apply_event(&hw->mix, ev);

            if (ev->reg == M4A_REG_SOUNDBIAS)
                hw_audio_sync_rates_from_mix(hw);
        }
    }

    render_to_host_offset(hw, pcm, outL, outR, frames, &rendered_host);
}
