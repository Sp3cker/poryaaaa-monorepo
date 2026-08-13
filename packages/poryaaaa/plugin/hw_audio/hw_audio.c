#include "hw_audio.h"
#include "audio_trace_format.h"
#if PORYAAAA_HW_AUDIO_TRACE
#    include "hw_audio_trace.h"
#endif
#include "hw_psg.h"
#include "hw_pcm.h"
#include "hw_mix.h"
#include "hw_resample.h"

#include <limits.h>
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
    HwPcm pcm;           /* canonical FIFO A/B timer-held-byte model */
    HwMixBus mix;        /* SOUNDCNT_L/H + SOUNDBIAS bias/clip stage */
    HwResample resample; /* DAC cadence -> host rate through current mGBA's sinc frontend */

    /* Production renderer's absolute hardware position.  DAC samples occur
     * on integral SOUNDBIAS cadence boundaries; a partial interval is
     * advanced through the PSG before a same-cycle register write. */
    uint64_t live_cycle;
    uint32_t dac_cycle_remainder;
    bool live_sample_pending;

    /* Per-channel solo/mute mask.  Bits HW_AUDIO_SOLO_* gate whether
     * each channel's pre-mix buffer feeds hw_mix_render — masked-off
     * channels go in as NULL and contribute zero.  Used by the
     * mGBA-capture parity workflow (matches the patched mGBA tool's
     * channel set so a single name selects the same channel on both
     * sides). */
    uint32_t solo_mask;

#if PORYAAAA_HW_AUDIO_TRACE
    /* Absolute trace position is independent of the production host-frame
     * renderer. It rejects reordered captures before they reach chip state. */
    uint64_t trace_cycle;
    uint32_t trace_order;
    bool trace_position_valid;
    HwPcm trace_pcm;
    bool trace_reset_frame_pending;
#endif

    /* Native channel values stay integral through the GBA DAC. Only the
     * final PCM16 stereo pair is converted for the host resampler. */
    uint8_t scratch_sq1[HW_AUDIO_INTERNAL_CHUNK];
    uint8_t scratch_sq2[HW_AUDIO_INTERNAL_CHUNK];
    uint8_t scratch_wave[HW_AUDIO_INTERNAL_CHUNK];
    uint8_t scratch_noise[HW_AUDIO_INTERNAL_CHUNK];
    int8_t scratch_dma_a[HW_AUDIO_INTERNAL_CHUNK];
    int8_t scratch_dma_b[HW_AUDIO_INTERNAL_CHUNK];
    int16_t native_l[HW_AUDIO_INTERNAL_CHUNK];
    int16_t native_r[HW_AUDIO_INTERNAL_CHUNK];
};

/* mGBA starts empty: the sinc's negative source indices are lookup-zero,
 * while its eight-frame high-water mark stalls output until frame nine. */
static void reset_frontend(HwAudio* hw)
{
    hw_resample_init(&hw->resample, (double)hw->internal_rate, (double)hw->host_rate);
}

/* SOUNDBIAS-derived quirk rate.
 * 32768 / 65536 / 131072 / 262144 Hz for sampling_cycle 0 / 1 / 2 / 3.
 * Used by HwFifoDrain to sample the PCM FIFO head. */

/* Map an absolute cycle span to the first host frame at or after it without
 * allowing a large cycle count to overflow the fixed-rate multiplication. */
static int host_frames_through_cycle(uint64_t cycles, uint32_t host_rate)
{
    uint64_t whole_seconds = cycles / PORYAAAA_GBA_CLOCK_HZ;
    uint64_t remainder = cycles % PORYAAAA_GBA_CLOCK_HZ;
    if (host_rate == 0 || whole_seconds > (uint64_t)INT_MAX / host_rate)
        return INT_MAX;
    uint64_t frames = whole_seconds * host_rate;
    frames += (remainder * host_rate + PORYAAAA_GBA_CLOCK_HZ - 1u) / PORYAAAA_GBA_CLOCK_HZ;
    return frames > INT_MAX ? INT_MAX : (int)frames;
}
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

    if (desired_internal_rate != hw->internal_rate)
    {
        hw->internal_rate = desired_internal_rate;
        hw_psg_set_render_rate(&hw->psg, (float)hw->internal_rate);

        /* mGBA changes the stream rate in place: buffered source history and
         * the fractional frontend timestamp continue under the new step. */
        hw_resample_set_rates(&hw->resample, (double)hw->internal_rate, (double)hw->host_rate);
        hw->dac_cycle_remainder = 0;
        hw->live_sample_pending = true;
    }
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
    hw_pcm_init(&hw->pcm, (uint32_t)hw->internal_rate);

    reset_frontend(hw);
    hw->live_sample_pending = true;
    return hw;
}

void hw_audio_reset(HwAudio* hw)
{
    if (!hw)
        return;
    hw_mix_init(&hw->mix);
    hw->internal_rate = chip_internal_rate(hw->mix.sampling_cycle);
    hw_psg_init(&hw->psg, (float)hw->internal_rate);
    hw_pcm_init(&hw->pcm, (uint32_t)hw->internal_rate);
    reset_frontend(hw);
    hw->live_cycle = 0;
    hw->dac_cycle_remainder = 0;
    hw->live_sample_pending = true;
#if PORYAAAA_HW_AUDIO_TRACE
    hw->trace_cycle = 0;
    hw->trace_order = 0;
    hw->trace_position_valid = false;
    hw->trace_reset_frame_pending = false;
#endif
}

void hw_audio_sync_psg_timing(HwAudio* destination, const HwAudio* source)
{
    if (!destination || !source || destination == source)
        return;

    destination->mix = source->mix;
    if (destination->internal_rate != source->internal_rate)
    {
        destination->internal_rate = source->internal_rate;
        hw_resample_set_rates(
            &destination->resample, (double)destination->internal_rate, (double)destination->host_rate);
    }
    hw_psg_set_render_rate(&destination->psg, source->psg.render_rate);

    destination->psg.master_enabled = source->psg.master_enabled;
    destination->psg.frame_seq_step = source->psg.frame_seq_step;
    destination->psg.frame_seq_accum = source->psg.frame_seq_accum;
    destination->psg.frame_seq_cycle_remainder = source->psg.frame_seq_cycle_remainder;
    destination->psg.frame_seq_ticks = source->psg.frame_seq_ticks;
    destination->psg.frame_seq_length_ticks = source->psg.frame_seq_length_ticks;
    destination->psg.frame_seq_sweep_ticks = source->psg.frame_seq_sweep_ticks;
    destination->live_cycle = source->live_cycle;
    destination->dac_cycle_remainder = source->dac_cycle_remainder;
    destination->live_sample_pending = source->live_sample_pending;
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
        destination->psg.wave_cycles_until_update = source->psg.wave_cycles_until_update;
        destination->psg.wave_pending_cycles = source->psg.wave_pending_cycles;
        destination->psg.wave_freq = source->psg.wave_freq;
        destination->psg.wave_vol_code = source->psg.wave_vol_code;
        destination->psg.wave_sample = source->psg.wave_sample;
        destination->psg.wave_enabled = source->psg.wave_enabled;
        destination->psg.wave_dac_on = source->psg.wave_dac_on;
        destination->psg.wave_bank = source->psg.wave_bank;
        destination->psg.wave_size = source->psg.wave_size;
        destination->psg.wave_length_counter = source->psg.wave_length_counter;
        destination->psg.wave_length_enabled = source->psg.wave_length_enabled;
        memcpy(destination->psg.wave_ram, source->psg.wave_ram, sizeof(destination->psg.wave_ram));
        break;
    case 3:
        destination->psg.noise_lfsr = source->psg.noise_lfsr;
        destination->psg.noise_timer_cycles = source->psg.noise_timer_cycles;
        destination->psg.noise_pending_cycles = source->psg.noise_pending_cycles;
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
    /* Match mGBA's destination-rate update: retain source history and phase,
     * and use the new step on the next frontend processing call. */
    hw_resample_set_rates(&hw->resample, (double)hw->internal_rate, (double)hw->host_rate);
}

/* Apply mGBA's integer routing and DAC stage after channel production. */
static void mix_native_chunk(HwAudio* hw, int internal_count)
{
    const uint32_t mask = hw->solo_mask;
    hw_mix_render(&hw->mix,
                  (mask & HW_AUDIO_SOLO_SQ1) ? hw->scratch_sq1 : NULL,
                  (mask & HW_AUDIO_SOLO_SQ2) ? hw->scratch_sq2 : NULL,
                  (mask & HW_AUDIO_SOLO_WAVE) ? hw->scratch_wave : NULL,
                  (mask & HW_AUDIO_SOLO_NOISE) ? hw->scratch_noise : NULL,
                  (mask & HW_AUDIO_SOLO_DMA_A) ? hw->scratch_dma_a : NULL,
                  (mask & HW_AUDIO_SOLO_DMA_B) ? hw->scratch_dma_b : NULL,
                  hw->native_l,
                  hw->native_r,
                  internal_count);
}

/* Feed the finished native PCM16 DAC observation directly into the
 * host-only frontend. Oscillator time advances separately by cycle spans. */
static void render_dac_sample(HwAudio* hw, float* outL, float* outR, int* rendered_host, int target_host)
{
    hw_psg_sample(&hw->psg, &hw->scratch_sq1[0], &hw->scratch_sq2[0], &hw->scratch_wave[0], &hw->scratch_noise[0]);
    hw_pcm_render(&hw->pcm, &hw->scratch_dma_a[0], &hw->scratch_dma_b[0], 1);
    mix_native_chunk(hw, 1);

    int max_host = target_host - *rendered_host;
    if (max_host < 0)
        max_host = 0;
    *rendered_host += hw_resample_process(&hw->resample,
                                          hw->native_l,
                                          hw->native_r,
                                          1,
                                          outL ? outL + *rendered_host : NULL,
                                          outR ? outR + *rendered_host : NULL,
                                          max_host);
}

/* Preserve real-time frontend latency independently of host block partitioning.
 * Drain any ready sinc output before zero-filling host deadlines that passed
 * while mGBA's eight-frame source watermark was still stalled. */
static void fill_host_through_cycle(
    HwAudio* hw, float* outL, float* outR, int* rendered_host, int target_host, uint64_t block_begin_cycle)
{
    int due = host_frames_through_cycle(hw->live_cycle - block_begin_cycle, (uint32_t)(hw->host_rate + 0.5f));
    if (due > target_host)
        due = target_host;
    if (due <= *rendered_host)
        return;

    *rendered_host += hw_resample_process(&hw->resample,
                                          NULL,
                                          NULL,
                                          0,
                                          outL ? outL + *rendered_host : NULL,
                                          outR ? outR + *rendered_host : NULL,
                                          due - *rendered_host);
    while (*rendered_host < due)
    {
        if (outL)
            outL[*rendered_host] = 0.0f;
        if (outR)
            outR[*rendered_host] = 0.0f;
        (*rendered_host)++;
    }
}

/* Advance the live chip to an absolute event boundary, producing only the
 * DAC samples that occur on or before that boundary. */
static bool render_to_cycle(HwAudio* hw,
                            float* outL,
                            float* outR,
                            uint64_t target_cycle,
                            uint64_t block_begin_cycle,
                            int* rendered_host,
                            int target_host)
{
    if (target_cycle < hw->live_cycle)
        return false;

    if (hw->live_sample_pending && target_cycle > hw->live_cycle)
    {
        fill_host_through_cycle(hw, outL, outR, rendered_host, target_host, block_begin_cycle);
        render_dac_sample(hw, outL, outR, rendered_host, target_host);
        hw->live_sample_pending = false;
    }

    const uint32_t cycles_per_dac_sample = PORYAAAA_GBA_CLOCK_HZ / (uint32_t)hw->internal_rate;
    while (hw->live_cycle < target_cycle)
    {
        const uint32_t until_dac = cycles_per_dac_sample - hw->dac_cycle_remainder;
        const uint64_t remaining = target_cycle - hw->live_cycle;
        const uint32_t advance = remaining < until_dac ? (uint32_t)remaining : until_dac;
        hw_psg_advance_cycles(&hw->psg, advance, true, true, true, true);
        hw->live_cycle += advance;
        hw->dac_cycle_remainder += advance;
        if (hw->dac_cycle_remainder == cycles_per_dac_sample)
        {
            hw->dac_cycle_remainder = 0;
            if (hw->live_cycle == target_cycle)
                hw->live_sample_pending = true;
            else
            {
                fill_host_through_cycle(hw, outL, outR, rendered_host, target_host, block_begin_cycle);
                render_dac_sample(hw, outL, outR, rendered_host, target_host);
            }
        }
    }
    return true;
}

/* Legacy snapshot entrypoint only consumes trigger latches; production audio
 * must use the absolute-cycle event stream. */
void hw_audio_render(HwAudio* hw, M4ARegisterFile* regs, const M4APcmRing* pcm, float* outL, float* outR, int frames)
{
    (void)hw;
    (void)pcm;
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

/* Render one host block over the batch's explicit absolute cycle interval.
 * Host frames size only the output buffer; they never determine event time. */
void hw_audio_render_events(HwAudio* hw, const M4ARegWriteBatch* events, float* outL, float* outR, int frames)
{
    if (frames <= 0)
        return;
    if (!hw || !events || (!events->events && events->count != 0) || events->end_cycle < events->begin_cycle ||
        hw->host_rate <= 0.0f || hw->internal_rate <= 0)
    {
        if (outL)
            memset(outL, 0, (size_t)frames * sizeof(float));
        if (outR)
            memset(outR, 0, (size_t)frames * sizeof(float));
        return;
    }

    if (events->begin_cycle != hw->live_cycle)
    {
        if (hw->live_cycle != 0 || !hw->live_sample_pending)
        {
            if (outL)
                memset(outL, 0, (size_t)frames * sizeof(float));
            if (outR)
                memset(outR, 0, (size_t)frames * sizeof(float));
            return;
        }
        hw->live_cycle = events->begin_cycle;
        hw->dac_cycle_remainder = 0;
    }
    hw_audio_sync_rates_from_mix(hw);

    int rendered_host = 0;
    uint64_t previous_cycle = events->begin_cycle;
    uint32_t previous_order = 0;
    bool have_previous = false;
    for (size_t i = 0; i < events->count; i++)
    {
        const M4ARegWrite* ev = &events->events[i];
        bool rate_change = ev->reg == M4A_REG_SOUNDBIAS && ((ev->value >> 14u) & 3u) != hw->mix.sampling_cycle;
        int event_host_limit = frames;
        if (rate_change)
        {
            event_host_limit =
                host_frames_through_cycle(ev->cycle - events->begin_cycle, (uint32_t)(hw->host_rate + 0.5f));
            if (event_host_limit > frames)
                event_host_limit = frames;
        }
        if (ev->cycle < events->begin_cycle || ev->cycle > events->end_cycle ||
            (have_previous &&
             (ev->cycle < previous_cycle || (ev->cycle == previous_cycle && ev->order <= previous_order))) ||
            !render_to_cycle(hw, outL, outR, ev->cycle, events->begin_cycle, &rendered_host, event_host_limit))
        {
            if (outL)
                memset(outL, 0, (size_t)frames * sizeof(float));
            if (outR)
                memset(outR, 0, (size_t)frames * sizeof(float));
            return;
        }

        if (rate_change)
        {
            rendered_host += hw_resample_process(&hw->resample,
                                                 NULL,
                                                 NULL,
                                                 0,
                                                 outL ? outL + rendered_host : NULL,
                                                 outR ? outR + rendered_host : NULL,
                                                 event_host_limit - rendered_host);
            while (rendered_host < event_host_limit)
            {
                if (outL)
                    outL[rendered_host] = 0.0f;
                if (outR)
                    outR[rendered_host] = 0.0f;
                rendered_host++;
            }
        }

        hw_psg_apply_event(&hw->psg, ev);
        hw_pcm_apply_event(&hw->pcm, ev);
        hw_mix_apply_event(&hw->mix, ev);
        if (ev->reg == M4A_REG_SOUNDBIAS)
            hw_audio_sync_rates_from_mix(hw);

        previous_cycle = ev->cycle;
        previous_order = ev->order;
        have_previous = true;
    }

    if (!render_to_cycle(hw, outL, outR, events->end_cycle, events->begin_cycle, &rendered_host, frames))
    {
        if (outL)
            memset(outL, 0, (size_t)frames * sizeof(float));
        if (outR)
            memset(outR, 0, (size_t)frames * sizeof(float));
        return;
    }

    if (rendered_host < frames)
    {
        rendered_host += hw_resample_process(&hw->resample,
                                             NULL,
                                             NULL,
                                             0,
                                             outL ? outL + rendered_host : NULL,
                                             outR ? outR + rendered_host : NULL,
                                             frames - rendered_host);
    }
    while (rendered_host < frames)
    {
        if (outL)
            outL[rendered_host] = 0.0f;
        if (outR)
            outR[rendered_host] = 0.0f;
        rendered_host++;
    }
}

#if PORYAAAA_HW_AUDIO_TRACE
/* Apply one existing poryaaaa register event to every owning chip module. */
static void apply_chip_event(HwAudio* hw, M4ARegId reg, uint32_t value)
{
    M4ARegWrite event = {.cycle = 0, .reg = reg, .value = value, .order = 0};
    hw_psg_apply_event(&hw->psg, &event);
    hw_pcm_apply_event(&hw->pcm, &event);
    hw_mix_apply_event(&hw->mix, &event);
    if (reg == M4A_REG_SOUNDBIAS)
        hw_audio_sync_rates_from_mix(hw);
}

static void apply_trace_pcm_event(HwAudio* hw, M4ARegId reg, uint32_t value)
{
    M4ARegWrite event = {.cycle = 0, .reg = reg, .value = value, .order = 0};
    hw_pcm_apply_event(&hw->trace_pcm, &event);
}
/* Decode the byte and halfword audio-register calls exposed by mGBA's GBA audio module. */
static HwAudioTraceStatus apply_trace_register_write(HwAudio* hw, uint32_t address, uint8_t width, uint32_t value)
{
    if (width == 1)
    {
        switch (address)
        {
        case PORYAAAA_GBA_IO_BASE + 0x62:
            apply_chip_event(hw, M4A_REG_NR11, value & 0xFFu);
            break;
        case PORYAAAA_GBA_IO_BASE + 0x63:
            apply_chip_event(hw, M4A_REG_NR12, value & 0xFFu);
            break;
        case PORYAAAA_GBA_IO_BASE + 0x64:
            apply_chip_event(hw, M4A_REG_NR13, value & 0xFFu);
            break;
        case PORYAAAA_GBA_IO_BASE + 0x65:
            apply_chip_event(hw, M4A_REG_NR14, value & 0xFFu);
            break;
        case PORYAAAA_GBA_IO_BASE + 0x68:
            apply_chip_event(hw, M4A_REG_NR21, value & 0xFFu);
            break;
        case PORYAAAA_GBA_IO_BASE + 0x69:
            apply_chip_event(hw, M4A_REG_NR22, value & 0xFFu);
            break;
        case PORYAAAA_GBA_IO_BASE + 0x6C:
            apply_chip_event(hw, M4A_REG_NR23, value & 0xFFu);
            break;
        case PORYAAAA_GBA_IO_BASE + 0x6D:
            apply_chip_event(hw, M4A_REG_NR24, value & 0xFFu);
            break;
        case PORYAAAA_GBA_IO_BASE + 0x72:
            apply_chip_event(hw, M4A_REG_NR31, value & 0xFFu);
            break;
        case PORYAAAA_GBA_IO_BASE + 0x73:
            apply_chip_event(hw, M4A_REG_NR32, value & 0xFFu);
            break;
        case PORYAAAA_GBA_IO_BASE + 0x74:
            apply_chip_event(hw, M4A_REG_NR33, value & 0xFFu);
            break;
        case PORYAAAA_GBA_IO_BASE + 0x75:
            apply_chip_event(hw, M4A_REG_NR34, value & 0xFFu);
            break;
        case PORYAAAA_GBA_IO_BASE + 0x78:
            apply_chip_event(hw, M4A_REG_NR41, value & 0xFFu);
            break;
        case PORYAAAA_GBA_IO_BASE + 0x79:
            apply_chip_event(hw, M4A_REG_NR42, value & 0xFFu);
            break;
        case PORYAAAA_GBA_IO_BASE + 0x7C:
            apply_chip_event(hw, M4A_REG_NR43, value & 0xFFu);
            break;
        case PORYAAAA_GBA_IO_BASE + 0x7D:
            apply_chip_event(hw, M4A_REG_NR44, value & 0xFFu);
            break;
        default:
            return HW_AUDIO_TRACE_UNSUPPORTED_ADDRESS;
        }
        return HW_AUDIO_TRACE_OK;
    }

    if (width != 2)
        return HW_AUDIO_TRACE_UNSUPPORTED_WIDTH;
    switch (address)
    {
    case PORYAAAA_GBA_IO_BASE + 0x60:
        apply_chip_event(hw, M4A_REG_NR10, value & 0xFFu);
        break;
    case PORYAAAA_GBA_IO_BASE + 0x62:
        apply_chip_event(hw, M4A_REG_NR11, value & 0xFFu);
        apply_chip_event(hw, M4A_REG_NR12, (value >> 8) & 0xFFu);
        break;
    case PORYAAAA_GBA_IO_BASE + 0x64:
        apply_chip_event(hw, M4A_REG_NR13, value & 0xFFu);
        apply_chip_event(hw, M4A_REG_NR14, (value >> 8) & 0xFFu);
        break;
    case PORYAAAA_GBA_IO_BASE + 0x68:
        apply_chip_event(hw, M4A_REG_NR21, value & 0xFFu);
        apply_chip_event(hw, M4A_REG_NR22, (value >> 8) & 0xFFu);
        break;
    case PORYAAAA_GBA_IO_BASE + 0x6C:
        apply_chip_event(hw, M4A_REG_NR23, value & 0xFFu);
        apply_chip_event(hw, M4A_REG_NR24, (value >> 8) & 0xFFu);
        break;
    case PORYAAAA_GBA_IO_BASE + 0x70:
        apply_chip_event(hw, M4A_REG_NR30, value & 0xFFu);
        break;
    case PORYAAAA_GBA_IO_BASE + 0x72:
        apply_chip_event(hw, M4A_REG_NR31, value & 0xFFu);
        apply_chip_event(hw, M4A_REG_NR32, (value >> 8) & 0xFFu);
        break;
    case PORYAAAA_GBA_IO_BASE + 0x74:
        apply_chip_event(hw, M4A_REG_NR33, value & 0xFFu);
        apply_chip_event(hw, M4A_REG_NR34, (value >> 8) & 0xFFu);
        break;
    case PORYAAAA_GBA_IO_BASE + 0x78:
        apply_chip_event(hw, M4A_REG_NR41, value & 0xFFu);
        apply_chip_event(hw, M4A_REG_NR42, (value >> 8) & 0xFFu);
        break;
    case PORYAAAA_GBA_IO_BASE + 0x7C:
        apply_chip_event(hw, M4A_REG_NR43, value & 0xFFu);
        apply_chip_event(hw, M4A_REG_NR44, (value >> 8) & 0xFFu);
        break;
    case PORYAAAA_GBA_IO_BASE + 0x80:
        apply_chip_event(hw, M4A_REG_NR50, value & 0xFFu);
        apply_chip_event(hw, M4A_REG_NR51, (value >> 8) & 0xFFu);
        break;
    case PORYAAAA_GBA_IO_BASE + 0x82:
        apply_chip_event(hw, M4A_REG_SOUNDCNT_H, value & 0xFFFFu);
        apply_trace_pcm_event(hw, M4A_REG_SOUNDCNT_H, value & 0xFFFFu);
        break;
    case PORYAAAA_GBA_IO_BASE + 0x84:
        apply_chip_event(hw, M4A_REG_NR52, value & 0xFFu);
        apply_trace_pcm_event(hw, M4A_REG_NR52, value & 0xFFu);
        break;
    case PORYAAAA_GBA_IO_BASE + 0x88:
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
    if (address < PORYAAAA_GBA_IO_BASE + 0x90 || address + width > PORYAAAA_GBA_IO_BASE + 0xA0)
        return HW_AUDIO_TRACE_UNSUPPORTED_ADDRESS;

    for (uint8_t byte_index = 0; byte_index < width; byte_index++)
    {
        uint32_t wave_offset = address - (PORYAAAA_GBA_IO_BASE + 0x90) + byte_index;
        uint32_t byte = (value >> (byte_index * 8u)) & 0xFFu;
        apply_chip_event(hw, M4A_REG_WAVE_RAM_BYTE, (wave_offset << 8) | byte);
    }
    return HW_AUDIO_TRACE_OK;
}

/* FIFO trace writes use the same modulo-8 word model as live audio. */
static HwAudioTraceStatus apply_trace_fifo_write(HwPcmFifo* fifo, uint8_t width, uint32_t value)
{
    if (width != 4)
        return HW_AUDIO_TRACE_UNSUPPORTED_WIDTH;
    hw_pcm_fifo_write_word(fifo, value);
    return HW_AUDIO_TRACE_OK;
}

/* Expose one explicit native SAMPLE without advancing the trace clock. */
static void render_trace_sample(HwAudio* hw, const HwAudioTraceFifoSample* fifo_sample)
{
    hw_psg_sample(&hw->psg, &hw->scratch_sq1[0], &hw->scratch_sq2[0], &hw->scratch_wave[0], &hw->scratch_noise[0]);
    int8_t fifo_a = fifo_sample ? fifo_sample->fifo_a : hw->trace_pcm.fifo_a.held_sample;
    int8_t fifo_b = fifo_sample ? fifo_sample->fifo_b : hw->trace_pcm.fifo_b.held_sample;
    hw->scratch_dma_a[0] = fifo_a;
    hw->scratch_dma_b[0] = fifo_b;
    mix_native_chunk(hw, 1);
}

/* Consume mGBA's separately scheduled reset callback exactly once. */
static void consume_trace_reset_frame_event(HwAudio* hw)
{
    if (!hw->trace_reset_frame_pending)
        return;
    hw_psg_run_zero_time_frame_event(&hw->psg);
    hw->trace_reset_frame_pending = false;
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
    hw_pcm_init(&hw->pcm, (uint32_t)hw->internal_rate);
    hw_pcm_init(&hw->trace_pcm, 0);
    hw->trace_pcm.master_enabled = false;
    reset_frontend(hw);
    hw->live_cycle = 0;
    hw->dac_cycle_remainder = 0;
    hw->live_sample_pending = true;
    hw->trace_cycle = 0;
    hw->trace_order = 0;
    hw->trace_position_valid = false;
    hw->trace_reset_frame_pending = true;
}

static bool trace_supports_byte_register(uint32_t address)
{
    switch (address)
    {
    case PORYAAAA_GBA_IO_BASE + 0x62:
    case PORYAAAA_GBA_IO_BASE + 0x63:
    case PORYAAAA_GBA_IO_BASE + 0x64:
    case PORYAAAA_GBA_IO_BASE + 0x65:
    case PORYAAAA_GBA_IO_BASE + 0x68:
    case PORYAAAA_GBA_IO_BASE + 0x69:
    case PORYAAAA_GBA_IO_BASE + 0x6C:
    case PORYAAAA_GBA_IO_BASE + 0x6D:
    case PORYAAAA_GBA_IO_BASE + 0x72:
    case PORYAAAA_GBA_IO_BASE + 0x73:
    case PORYAAAA_GBA_IO_BASE + 0x74:
    case PORYAAAA_GBA_IO_BASE + 0x75:
    case PORYAAAA_GBA_IO_BASE + 0x78:
    case PORYAAAA_GBA_IO_BASE + 0x79:
    case PORYAAAA_GBA_IO_BASE + 0x7C:
    case PORYAAAA_GBA_IO_BASE + 0x7D:
        return true;
    default:
        return false;
    }
}

static bool trace_supports_halfword_register(uint32_t address)
{
    switch (address)
    {
    case PORYAAAA_GBA_IO_BASE + 0x60:
    case PORYAAAA_GBA_IO_BASE + 0x62:
    case PORYAAAA_GBA_IO_BASE + 0x64:
    case PORYAAAA_GBA_IO_BASE + 0x68:
    case PORYAAAA_GBA_IO_BASE + 0x6C:
    case PORYAAAA_GBA_IO_BASE + 0x70:
    case PORYAAAA_GBA_IO_BASE + 0x72:
    case PORYAAAA_GBA_IO_BASE + 0x74:
    case PORYAAAA_GBA_IO_BASE + 0x78:
    case PORYAAAA_GBA_IO_BASE + 0x7C:
    case PORYAAAA_GBA_IO_BASE + 0x80:
    case PORYAAAA_GBA_IO_BASE + 0x82:
    case PORYAAAA_GBA_IO_BASE + 0x84:
    case PORYAAAA_GBA_IO_BASE + 0x88:
        return true;
    default:
        return false;
    }
}

static HwAudioTraceStatus validate_trace_write(const HwAudioTraceEvent* event)
{
    if ((event->width == 1 && event->value > UINT8_MAX) || (event->width == 2 && event->value > UINT16_MAX))
        return HW_AUDIO_TRACE_INVALID_ARGUMENT;
    if (event->address >= PORYAAAA_GBA_IO_BASE + 0x90 && event->address < PORYAAAA_GBA_IO_BASE + 0xA0)
    {
        if (event->width != 1 && event->width != 2 && event->width != 4)
            return HW_AUDIO_TRACE_UNSUPPORTED_WIDTH;
        return event->address + event->width <= PORYAAAA_GBA_IO_BASE + 0xA0 ? HW_AUDIO_TRACE_OK
                                                                            : HW_AUDIO_TRACE_UNSUPPORTED_ADDRESS;
    }
    if (event->address == PORYAAAA_GBA_IO_BASE + 0xA0 || event->address == PORYAAAA_GBA_IO_BASE + 0xA4)
        return event->width == 4 ? HW_AUDIO_TRACE_OK : HW_AUDIO_TRACE_UNSUPPORTED_WIDTH;
    if (event->width == 1)
        return trace_supports_byte_register(event->address) ? HW_AUDIO_TRACE_OK : HW_AUDIO_TRACE_UNSUPPORTED_ADDRESS;
    if (event->width != 2)
        return HW_AUDIO_TRACE_UNSUPPORTED_WIDTH;
    return trace_supports_halfword_register(event->address) ? HW_AUDIO_TRACE_OK : HW_AUDIO_TRACE_UNSUPPORTED_ADDRESS;
}

static HwAudioTraceStatus validate_trace_event(const HwAudioTraceEvent* event)
{
    if (event->cycle > INT32_MAX)
        return HW_AUDIO_TRACE_INVALID_ARGUMENT;
    switch (event->kind)
    {
    case HW_AUDIO_TRACE_WRITE:
        return validate_trace_write(event);
    case HW_AUDIO_TRACE_SAMPLE:
        return event->width == 0 && event->address == 0 && event->value == 0 ? HW_AUDIO_TRACE_OK
                                                                             : HW_AUDIO_TRACE_INVALID_ARGUMENT;
    case HW_AUDIO_TRACE_TIMER:
        return event->width == 0 && event->address == 0 && event->value <= 1 ? HW_AUDIO_TRACE_OK
                                                                             : HW_AUDIO_TRACE_INVALID_ARGUMENT;
    default:
        return HW_AUDIO_TRACE_INVALID_ARGUMENT;
    }
}

static HwAudioTraceStatus apply_trace_event(HwAudio* hw,
                                            const HwAudioTraceEvent* event,
                                            const HwAudioTraceFifoSample* fifo_sample,
                                            HwAudioNativeFrame* frame,
                                            bool observe_sample)
{
    if (!hw || !event || !frame)
        return HW_AUDIO_TRACE_INVALID_ARGUMENT;
    memset(frame, 0, sizeof(*frame));

    if (hw->trace_position_valid &&
        (event->cycle < hw->trace_cycle || (event->cycle == hw->trace_cycle && event->order <= hw->trace_order)))
        return HW_AUDIO_TRACE_OUT_OF_ORDER;

    HwAudioTraceStatus status = validate_trace_event(event);
    if (status != HW_AUDIO_TRACE_OK)
        return status;

    if (hw->trace_reset_frame_pending && (event->cycle > 0 || event->kind == HW_AUDIO_TRACE_SAMPLE))
        consume_trace_reset_frame_event(hw);

    uint64_t prior_cycle = hw->trace_position_valid ? hw->trace_cycle : 0;
    uint64_t cycle_delta = event->cycle - prior_cycle;
    bool stale_sq1 = hw->psg.sq1_timer_cycles + cycle_delta > 0x40000000u;
    bool stale_sq2 = hw->psg.sq2_timer_cycles + cycle_delta > 0x40000000u;
    bool clock_sq1 = false;
    bool clock_sq2 = false;
    bool clock_wave = false;
    bool clock_noise = false;
    bool preapply_wave_bank = false;
    if (event->kind == HW_AUDIO_TRACE_SAMPLE)
    {
        clock_sq1 = stale_sq1 || (hw->psg.sq1_enabled && hw->psg.sq1_envelope.dead != 2);
        clock_sq2 = stale_sq2 || (hw->psg.sq2_enabled && hw->psg.sq2_envelope.dead != 2);
        clock_wave = true;
        clock_noise = true;
    }
    else if (event->kind == HW_AUDIO_TRACE_WRITE)
    {
        if (event->address >= PORYAAAA_GBA_IO_BASE + 0x60 && event->address <= PORYAAAA_GBA_IO_BASE + 0x65)
            clock_sq1 = true;
        else if (event->address >= PORYAAAA_GBA_IO_BASE + 0x68 && event->address <= PORYAAAA_GBA_IO_BASE + 0x6D)
            clock_sq2 = true;
        else if (event->address == PORYAAAA_GBA_IO_BASE + 0x80)
        {
            clock_sq1 = stale_sq1 || (hw->psg.sq1_enabled && hw->psg.sq1_envelope.dead != 2);
            clock_sq2 = stale_sq2 || (hw->psg.sq2_enabled && hw->psg.sq2_envelope.dead != 2);
        }
        if ((event->address >= PORYAAAA_GBA_IO_BASE + 0x72 && event->address <= PORYAAAA_GBA_IO_BASE + 0x75) ||
            event->address == PORYAAAA_GBA_IO_BASE + 0x80 ||
            (event->address >= PORYAAAA_GBA_IO_BASE + 0x90 && event->address < PORYAAAA_GBA_IO_BASE + 0xA0))
            clock_wave = true;
        preapply_wave_bank = event->address == PORYAAAA_GBA_IO_BASE + 0x70;
        if ((event->address >= PORYAAAA_GBA_IO_BASE + 0x78 && event->address <= PORYAAAA_GBA_IO_BASE + 0x7D) ||
            event->address == PORYAAAA_GBA_IO_BASE + 0x80)
            clock_noise = true;
    }
    bool defer_terminal_frame = event->kind == HW_AUDIO_TRACE_SAMPLE && !observe_sample &&
                                (event->order & PORYAAAA_TRACE_ORDER_EXTENDED) != 0u &&
                                (event->order & PORYAAAA_TRACE_ORDER_DELAY_MASK) != 0u;
    if (defer_terminal_frame)
        hw_psg_advance_staged_sample_cycles(&hw->psg, cycle_delta, clock_sq1, clock_sq2, clock_wave, clock_noise);
    else
        hw_psg_advance_cycles(&hw->psg, cycle_delta, clock_sq1, clock_sq2, clock_wave, clock_noise);
    if (preapply_wave_bank)
    {
        /* Frame events consume the old bank first; NR30 then selects bank
         * and size before its terminal forced Wave run. */
        hw->psg.wave_size = (event->value & 0x20u) != 0;
        hw->psg.wave_bank = (event->value & 0x40u) != 0;
        hw_psg_advance_cycles(&hw->psg, 0, false, false, true, false);
    }
    if (event->kind == HW_AUDIO_TRACE_WRITE)
    {
        if (event->address >= PORYAAAA_GBA_IO_BASE + 0x90 && event->address < PORYAAAA_GBA_IO_BASE + 0xA0)
        {
            status = apply_trace_wave_write(hw, event->address, event->width, event->value);
        }
        else if (event->address == PORYAAAA_GBA_IO_BASE + 0xA0)
        {
            status = apply_trace_fifo_write(&hw->trace_pcm.fifo_a, event->width, event->value);
        }
        else if (event->address == PORYAAAA_GBA_IO_BASE + 0xA4)
        {
            status = apply_trace_fifo_write(&hw->trace_pcm.fifo_b, event->width, event->value);
        }
        else
        {
            status = apply_trace_register_write(hw, event->address, event->width, event->value);
        }
        if (status == HW_AUDIO_TRACE_OK && hw->trace_reset_frame_pending && event->cycle == 0 &&
            event->address == PORYAAAA_GBA_IO_BASE + 0x84 && hw->psg.master_enabled)
            consume_trace_reset_frame_event(hw);
    }
    else if (event->kind == HW_AUDIO_TRACE_SAMPLE)
    {
        if (event->width != 0 || event->address != 0 || event->value != 0)
            status = HW_AUDIO_TRACE_INVALID_ARGUMENT;
        else if (observe_sample)
        {
            render_trace_sample(hw, fifo_sample);
            frame->cycle = event->cycle;
            frame->left = hw->native_l[0];
            frame->right = hw->native_r[0];
        }
    }
    else if (event->kind == HW_AUDIO_TRACE_TIMER)
    {
        if (event->width != 0 || event->address != 0 || event->value > 1)
            status = HW_AUDIO_TRACE_INVALID_ARGUMENT;
        else
            hw_pcm_clock_timer(&hw->trace_pcm, (uint8_t)event->value);
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

HwAudioTraceStatus hw_audio_trace_apply(HwAudio* hw, const HwAudioTraceEvent* event, HwAudioNativeFrame* frame)
{
    return apply_trace_event(hw, event, NULL, frame, true);
}

HwAudioTraceStatus hw_audio_trace_apply_fifo_sample(HwAudio* hw,
                                                    const HwAudioTraceEvent* event,
                                                    const HwAudioTraceFifoSample* fifo_sample,
                                                    HwAudioNativeFrame* frame)
{
    if (!fifo_sample || !event || event->kind != HW_AUDIO_TRACE_SAMPLE)
        return HW_AUDIO_TRACE_INVALID_ARGUMENT;
    return apply_trace_event(hw, event, fifo_sample, frame, true);
}

HwAudioTraceStatus hw_audio_trace_stage_sample(HwAudio* hw, const HwAudioTraceEvent* event, HwAudioNativeFrame* frame)
{
    if (!event || event->kind != HW_AUDIO_TRACE_SAMPLE)
        return HW_AUDIO_TRACE_INVALID_ARGUMENT;
    return apply_trace_event(hw, event, NULL, frame, false);
}

HwAudioTraceStatus hw_audio_trace_observe_sample(HwAudio* hw,
                                                 uint64_t cycle,
                                                 const HwAudioTraceFifoSample* fifo_sample,
                                                 HwAudioNativeFrame* frame)
{
    if (!hw || !fifo_sample || !frame || cycle > INT32_MAX)
        return HW_AUDIO_TRACE_INVALID_ARGUMENT;
    render_trace_sample(hw, fifo_sample);
    frame->cycle = cycle;
    frame->left = hw->native_l[0];
    frame->right = hw->native_r[0];
    return HW_AUDIO_TRACE_OK;
}

void hw_audio_trace_finish_sample_observation(HwAudio* hw, uint64_t observation_cycle)
{
    if (hw && observation_cycle >= hw->trace_cycle)
        hw_psg_run_deferred_frame_event(&hw->psg, observation_cycle - hw->trace_cycle);
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
    }
    return "unknown trace status";
}
#endif
