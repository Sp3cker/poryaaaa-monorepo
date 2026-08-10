#include "hw_audio.h"
#include "hw_audio_trace.h"
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

typedef struct
{
    uint8_t bytes[32];
    uint8_t read_index;
    uint8_t write_index;
    uint8_t count;
    int8_t held_sample;
} HwAudioTraceFifo;

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

    /* Absolute trace position is independent of the production host-frame
     * renderer. It rejects reordered captures before they reach chip state. */
    uint64_t trace_cycle;
    uint32_t trace_order;
    bool trace_position_valid;
    HwAudioTraceFifo trace_fifo_a;
    HwAudioTraceFifo trace_fifo_b;
    uint8_t trace_timer_a;
    uint8_t trace_timer_b;

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
    hw->trace_cycle = 0;
    hw->trace_order = 0;
    hw->trace_position_valid = false;
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

/* Apply the shared routing and DAC stage after a producer fills channel scratch. */
static void mix_chip_chunk(HwAudio* hw, int internal_count)
{
    const uint32_t mask = hw->solo_mask;
    hw_mix_render(&hw->mix,
                  (mask & HW_AUDIO_SOLO_SQ1) ? hw->scratch_sq1 : NULL,
                  (mask & HW_AUDIO_SOLO_SQ2) ? hw->scratch_sq2 : NULL,
                  (mask & HW_AUDIO_SOLO_WAVE) ? hw->scratch_wave : NULL,
                  (mask & HW_AUDIO_SOLO_NOISE) ? hw->scratch_noise : NULL,
                  (mask & HW_AUDIO_SOLO_DMA_A) ? hw->scratch_dma_a : NULL,
                  (mask & HW_AUDIO_SOLO_DMA_B) ? hw->scratch_dma_b : NULL,
                  hw->mix_l,
                  hw->mix_r,
                  internal_count);
}

/* Produce chip-native stereo without involving the sinc frontend. */
static void render_chip_chunk(HwAudio* hw, const M4APcmRing* pcm_ring, int internal_count)
{
    hw_psg_render(&hw->psg, hw->scratch_sq1, hw->scratch_sq2, hw->scratch_wave, hw->scratch_noise, internal_count);
    hw_pcm_render(&hw->pcm, pcm_ring, hw->scratch_dma_a, hw->scratch_dma_b, internal_count);
    mix_chip_chunk(hw, internal_count);
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

    render_chip_chunk(hw, pcm_ring, internal_count);

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

/* Apply one existing poryaaaa register event to every owning chip module. */
static void apply_chip_event(HwAudio* hw, M4ARegId reg, uint32_t value)
{
    M4ARegWrite event = {0, reg, value};
    hw_psg_apply_event(&hw->psg, &event);
    hw_pcm_apply_event(&hw->pcm, &event);
    hw_mix_apply_event(&hw->mix, &event);
    if (reg == M4A_REG_SOUNDBIAS)
        hw_audio_sync_rates_from_mix(hw);
}

/* Decode the halfword audio-register calls exposed by mGBA's GBA audio module. */
static HwAudioTraceStatus apply_trace_register_write(HwAudio* hw, uint32_t address, uint8_t width, uint32_t value)
{
    if (width != 2)
        return HW_AUDIO_TRACE_UNSUPPORTED_WIDTH;

    switch (address)
    {
    case HW_AUDIO_GBA_IO_BASE + 0x60:
        apply_chip_event(hw, M4A_REG_NR10, value & 0xFFu);
        break;
    case HW_AUDIO_GBA_IO_BASE + 0x62:
        apply_chip_event(hw, M4A_REG_NR11, value & 0xFFu);
        apply_chip_event(hw, M4A_REG_NR12, (value >> 8) & 0xFFu);
        break;
    case HW_AUDIO_GBA_IO_BASE + 0x64:
        apply_chip_event(hw, M4A_REG_NR13, value & 0xFFu);
        apply_chip_event(hw, M4A_REG_NR14, (value >> 8) & 0xFFu);
        break;
    case HW_AUDIO_GBA_IO_BASE + 0x68:
        apply_chip_event(hw, M4A_REG_NR21, value & 0xFFu);
        apply_chip_event(hw, M4A_REG_NR22, (value >> 8) & 0xFFu);
        break;
    case HW_AUDIO_GBA_IO_BASE + 0x6C:
        apply_chip_event(hw, M4A_REG_NR23, value & 0xFFu);
        apply_chip_event(hw, M4A_REG_NR24, (value >> 8) & 0xFFu);
        break;
    case HW_AUDIO_GBA_IO_BASE + 0x70:
        apply_chip_event(hw, M4A_REG_NR30, value & 0xFFu);
        break;
    case HW_AUDIO_GBA_IO_BASE + 0x72:
        apply_chip_event(hw, M4A_REG_NR31, value & 0xFFu);
        apply_chip_event(hw, M4A_REG_NR32, (value >> 8) & 0xFFu);
        break;
    case HW_AUDIO_GBA_IO_BASE + 0x74:
        apply_chip_event(hw, M4A_REG_NR33, value & 0xFFu);
        apply_chip_event(hw, M4A_REG_NR34, (value >> 8) & 0xFFu);
        break;
    case HW_AUDIO_GBA_IO_BASE + 0x78:
        apply_chip_event(hw, M4A_REG_NR41, value & 0xFFu);
        apply_chip_event(hw, M4A_REG_NR42, (value >> 8) & 0xFFu);
        break;
    case HW_AUDIO_GBA_IO_BASE + 0x7C:
        apply_chip_event(hw, M4A_REG_NR43, value & 0xFFu);
        apply_chip_event(hw, M4A_REG_NR44, (value >> 8) & 0xFFu);
        break;
    case HW_AUDIO_GBA_IO_BASE + 0x80:
        apply_chip_event(hw, M4A_REG_NR50, value & 0xFFu);
        apply_chip_event(hw, M4A_REG_NR51, (value >> 8) & 0xFFu);
        break;
    case HW_AUDIO_GBA_IO_BASE + 0x82:
        apply_chip_event(hw, M4A_REG_SOUNDCNT_H, value & 0xFFFFu);
        hw->trace_timer_a = (uint8_t)((value >> 10) & 1u);
        hw->trace_timer_b = (uint8_t)((value >> 14) & 1u);
        if (value & (1u << 11))
            memset(&hw->trace_fifo_a, 0, sizeof(hw->trace_fifo_a));
        if (value & (1u << 15))
            memset(&hw->trace_fifo_b, 0, sizeof(hw->trace_fifo_b));
        break;
    case HW_AUDIO_GBA_IO_BASE + 0x84:
        apply_chip_event(hw, M4A_REG_NR52, value & 0xFFu);
        break;
    case HW_AUDIO_GBA_IO_BASE + 0x88:
        apply_chip_event(hw, M4A_REG_SOUNDBIAS, value & 0xFFFFu);
        break;
    default:
        return HW_AUDIO_TRACE_UNSUPPORTED_ADDRESS;
    }
    return HW_AUDIO_TRACE_OK;
}

/* Apply a visible-bank Wave RAM write in little-endian bus order. */
static HwAudioTraceStatus apply_trace_wave_write(HwAudio* hw, uint32_t address, uint8_t width, uint32_t value)
{
    if (width != 1 && width != 2 && width != 4)
        return HW_AUDIO_TRACE_UNSUPPORTED_WIDTH;
    if (address < HW_AUDIO_GBA_IO_BASE + 0x90 || address + width > HW_AUDIO_GBA_IO_BASE + 0xA0)
        return HW_AUDIO_TRACE_UNSUPPORTED_ADDRESS;

    for (uint8_t byte_index = 0; byte_index < width; byte_index++)
    {
        uint32_t wave_offset = address - (HW_AUDIO_GBA_IO_BASE + 0x90) + byte_index;
        uint32_t byte = (value >> (byte_index * 8u)) & 0xFFu;
        apply_chip_event(hw, M4A_REG_WAVE_RAM_BYTE, (wave_offset << 8) | byte);
    }
    return HW_AUDIO_TRACE_OK;
}

/* Push one DMA word into a trace FIFO without silently overwriting samples. */
static HwAudioTraceStatus apply_trace_fifo_write(HwAudioTraceFifo* fifo, uint8_t width, uint32_t value)
{
    if (width != 4)
        return HW_AUDIO_TRACE_UNSUPPORTED_WIDTH;
    if (fifo->count > 28)
        return HW_AUDIO_TRACE_FIFO_OVERFLOW;

    for (unsigned byte_index = 0; byte_index < 4; byte_index++)
    {
        fifo->bytes[fifo->write_index] = (uint8_t)(value >> (byte_index * 8u));
        fifo->write_index = (uint8_t)((fifo->write_index + 1u) & 31u);
        fifo->count++;
    }
    return HW_AUDIO_TRACE_OK;
}

/* A selected timer consumes one signed byte and otherwise preserves the hold. */
static void clock_trace_fifo(HwAudioTraceFifo* fifo)
{
    if (!fifo->count)
        return;
    fifo->held_sample = (int8_t)fifo->bytes[fifo->read_index];
    fifo->read_index = (uint8_t)((fifo->read_index + 1u) & 31u);
    fifo->count--;
}

/* Render one explicit native SAMPLE using the trace-driven FIFO holds. */
static void render_trace_sample(HwAudio* hw)
{
    hw_psg_render(&hw->psg, hw->scratch_sq1, hw->scratch_sq2, hw->scratch_wave, hw->scratch_noise, 1);
    hw->scratch_dma_a[0] = (float)hw->trace_fifo_a.held_sample / 128.0f;
    hw->scratch_dma_b[0] = (float)hw->trace_fifo_b.held_sample / 128.0f;
    mix_chip_chunk(hw, 1);
}

/* Quantize the current native float representation into mGBA's signed PCM16 unit. */
static int16_t native_float_to_pcm16(float sample)
{
    float scaled = sample * 32768.0f;
    if (scaled >= 32767.0f)
        return INT16_MAX;
    if (scaled <= -32768.0f)
        return INT16_MIN;
    return (int16_t)(scaled >= 0.0f ? scaled + 0.5f : scaled - 0.5f);
}

void hw_audio_trace_reset(HwAudio* hw)
{
    if (!hw)
        return;

    memset(&hw->mix, 0, sizeof(hw->mix));
    hw->mix.bias_level = 0x200;
    hw->mix.sampling_cycle = 0;
    hw->internal_rate = chip_internal_rate(hw->mix.sampling_cycle);
    hw_psg_init(&hw->psg, (float)hw->internal_rate);
    apply_chip_event(hw, M4A_REG_NR52, 0);
    hw_pcm_init(&hw->pcm, (float)hw->internal_rate);
    hw_pcm_set_quirk_rate(&hw->pcm, chip_quirk_rate(hw->mix.sampling_cycle));
    hw_resample_init(&hw->resample, (double)hw->internal_rate, (double)hw->host_rate);
    hw->trace_cycle = 0;
    hw->trace_order = 0;
    hw->trace_position_valid = false;
    memset(&hw->trace_fifo_a, 0, sizeof(hw->trace_fifo_a));
    memset(&hw->trace_fifo_b, 0, sizeof(hw->trace_fifo_b));
    hw->trace_timer_a = 0;
    hw->trace_timer_b = 0;
}

HwAudioTraceStatus hw_audio_trace_apply(HwAudio* hw, const HwAudioTraceEvent* event, HwAudioNativeFrame* frame)
{
    if (!hw || !event || !frame)
        return HW_AUDIO_TRACE_INVALID_ARGUMENT;
    memset(frame, 0, sizeof(*frame));

    if (hw->trace_position_valid &&
        (event->cycle < hw->trace_cycle || (event->cycle == hw->trace_cycle && event->order <= hw->trace_order)))
        return HW_AUDIO_TRACE_OUT_OF_ORDER;

    HwAudioTraceStatus status = HW_AUDIO_TRACE_OK;
    if (event->kind == HW_AUDIO_TRACE_WRITE)
    {
        if (event->address >= HW_AUDIO_GBA_IO_BASE + 0x90 && event->address < HW_AUDIO_GBA_IO_BASE + 0xA0)
        {
            status = apply_trace_wave_write(hw, event->address, event->width, event->value);
        }
        else if (event->address == HW_AUDIO_GBA_IO_BASE + 0xA0)
        {
            status = apply_trace_fifo_write(&hw->trace_fifo_a, event->width, event->value);
        }
        else if (event->address == HW_AUDIO_GBA_IO_BASE + 0xA4)
        {
            status = apply_trace_fifo_write(&hw->trace_fifo_b, event->width, event->value);
        }
        else
        {
            status = apply_trace_register_write(hw, event->address, event->width, event->value);
        }
    }
    else if (event->kind == HW_AUDIO_TRACE_SAMPLE)
    {
        if (event->width != 0 || event->address != 0 || event->value != 0)
            status = HW_AUDIO_TRACE_INVALID_ARGUMENT;
        else
        {
            render_trace_sample(hw);
            frame->cycle = event->cycle;
            frame->left = native_float_to_pcm16(hw->mix_l[0]);
            frame->right = native_float_to_pcm16(hw->mix_r[0]);
        }
    }
    else if (event->kind == HW_AUDIO_TRACE_TIMER)
    {
        if (event->width != 0 || event->address != 0 || event->value > 1)
            status = HW_AUDIO_TRACE_INVALID_ARGUMENT;
        else
        {
            if (hw->trace_timer_a == event->value)
                clock_trace_fifo(&hw->trace_fifo_a);
            if (hw->trace_timer_b == event->value)
                clock_trace_fifo(&hw->trace_fifo_b);
        }
    }
    else
    {
        status = HW_AUDIO_TRACE_INVALID_ARGUMENT;
    }

    if (status == HW_AUDIO_TRACE_OK)
    {
        hw->trace_cycle = event->cycle;
        hw->trace_order = event->order;
        hw->trace_position_valid = true;
    }
    return status;
}

const char* hw_audio_trace_status_string(HwAudioTraceStatus status)
{
    switch (status)
    {
    case HW_AUDIO_TRACE_OK:
        return "ok";
    case HW_AUDIO_TRACE_INVALID_ARGUMENT:
        return "invalid argument";
    case HW_AUDIO_TRACE_OUT_OF_ORDER:
        return "event is not strictly ordered";
    case HW_AUDIO_TRACE_UNSUPPORTED_WIDTH:
        return "unsupported write width";
    case HW_AUDIO_TRACE_UNSUPPORTED_ADDRESS:
        return "unsupported audio address";
    case HW_AUDIO_TRACE_FIFO_OVERFLOW:
        return "FIFO write exceeds 32-byte capacity";
    }
    return "unknown trace status";
}
