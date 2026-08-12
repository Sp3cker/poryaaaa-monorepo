#include "hw_psg.h"
#include "audio_trace_format.h"

#include <string.h>

#if defined(__clang__) || defined(__GNUC__)
#    define HW_PSG_FALLTHROUGH __attribute__((fallthrough))
#else
#    define HW_PSG_FALLTHROUGH
#endif

/* GB square duty patterns.  Top 3 bits of the 32-bit phase index into
 * an 8-bit pattern; bit value (0/1) → channel amplitude.  Matches mGBA
 * _squareChannelDuty index order; bit 0 = first emitted sample. */
static const uint8_t kDutyPatterns[4] = {
    0x80, /* 12.5%: 0000_0001 */
    0x81, /* 25%:   1000_0001 */
    0xE1, /* 50%:   1000_0111 */
    0x7E, /* 75%:   0111_1110 */
};

/* Refresh one square DAC latch at an mGBA sample-producing event. */
static void hw_psg_refresh_square_sample(uint8_t* sample, uint8_t duty, uint8_t duty_index, uint8_t current_volume)
{
    bool high = ((kDutyPatterns[duty & 3u] >> (duty_index & 7u)) & 1u) != 0;
    *sample = high ? current_volume : 0;
}

/* mGBA's five-clock normal-width noise fast path uses these feedback masks. */
static const uint8_t kNoiseBatchMasks[0x40] = {
    0x3F, 0x3E, 0x3C, 0x3D, 0x39, 0x38, 0x3A, 0x3B, 0x33, 0x32, 0x30, 0x31, 0x35, 0x34, 0x36, 0x37,
    0x27, 0x26, 0x24, 0x25, 0x21, 0x20, 0x22, 0x23, 0x2B, 0x2A, 0x28, 0x29, 0x2D, 0x2C, 0x2E, 0x2F,
    0x0F, 0x0E, 0x0C, 0x0D, 0x09, 0x08, 0x0A, 0x0B, 0x03, 0x02, 0x00, 0x01, 0x05, 0x04, 0x06, 0x07,
    0x17, 0x16, 0x14, 0x15, 0x11, 0x10, 0x12, 0x13, 0x1B, 0x1A, 0x18, 0x19, 0x1D, 0x1C, 0x1E, 0x1F,
};

/* Establish the absolute reset-time cadence. Trace replay models mGBA's
 * separately scheduled zero-time frame callback at its observed boundary. */
static void hw_psg_reset_frame_sequencer(HwPsgSynth* psg, uint8_t step)
{
    psg->frame_seq_step = (uint8_t)(step & 7u);
    psg->frame_seq_cycle_remainder = 0;
    psg->frame_seq_accum = 0.0;
    psg->frame_seq_ticks = 0;
    psg->frame_seq_length_ticks = 0;
    psg->frame_seq_sweep_ticks = 0;
    psg->frame_seq_envelope_ticks = 0;
}

static void hw_psg_frame_length(HwPsgSynth* psg)
{
    psg->frame_seq_length_ticks++;
    if (psg->sq1_length_enabled && psg->sq1_length_counter > 0 && --psg->sq1_length_counter == 0)
        psg->sq1_enabled = false;
    if (psg->sq2_length_enabled && psg->sq2_length_counter > 0 && --psg->sq2_length_counter == 0)
        psg->sq2_enabled = false;
    if (psg->wave_length_enabled && psg->wave_length_counter > 0 && --psg->wave_length_counter == 0)
        psg->wave_enabled = false;
    if (psg->noise_length_enabled && psg->noise_length_counter > 0 && --psg->noise_length_counter == 0)
        psg->noise_enabled = false;
}

static bool hw_psg_update_sq1_sweep(HwPsgSynth* psg, bool initial)
{
    if (initial || psg->sq1_sweep_time != 8)
    {
        int freq = psg->sq1_sweep_shadow_freq;
        int delta = freq >> psg->sq1_sweep_shift;

        if (psg->sq1_sweep_decrease)
        {
            freq -= delta;
            if (!initial && freq >= 0)
            {
                psg->sq1_sweep_shadow_freq = (uint16_t)freq;
                psg->sq1_freq = (uint16_t)freq;
            }
        }
        else
        {
            freq += delta;
            if (freq >= 2048)
            {
                return false;
            }
            if (!initial && psg->sq1_sweep_shift)
            {
                psg->sq1_sweep_shadow_freq = (uint16_t)freq;
                psg->sq1_freq = (uint16_t)freq;
                if (!hw_psg_update_sq1_sweep(psg, true))
                {
                    return false;
                }
            }
        }
        psg->sq1_sweep_occurred = true;
    }
    psg->sq1_sweep_timer = psg->sq1_sweep_time;
    return true;
}

static void hw_psg_frame_sweep(HwPsgSynth* psg)
{
    psg->frame_seq_sweep_ticks++;
    if (!psg->sq1_sweep_enabled)
        return;
    if (psg->sq1_sweep_timer > 0)
    {
        psg->sq1_sweep_timer--;
    }
    if (psg->sq1_sweep_timer == 0)
    {
        if (!hw_psg_update_sq1_sweep(psg, false))
        {
            psg->sq1_enabled = false;
        }
    }
}

static void hw_psg_update_envelope_dead(HwPsgEnvelope* envelope)
{
    if (envelope->step_time == 0)
    {
        envelope->dead = envelope->current_volume ? 1 : 2;
    }
    else if (!envelope->direction && envelope->current_volume == 0)
    {
        envelope->dead = 2;
    }
    else if (envelope->direction && envelope->current_volume == 15)
    {
        envelope->dead = 1;
    }
    else if (envelope->dead)
    {
        envelope->next_step = envelope->step_time;
        envelope->dead = 0;
    }
}

/* Decode NRx2 without reloading current volume; NRx4 trigger does that. */
static bool hw_psg_write_envelope(HwPsgEnvelope* envelope, uint8_t value)
{
    envelope->step_time = value & 0x07u;
    envelope->direction = (value & 0x08u) != 0;
    envelope->initial_volume = (value >> 4) & 0x0Fu;
    envelope->current_volume &= 0x0Fu;
    hw_psg_update_envelope_dead(envelope);
    return envelope->initial_volume || envelope->direction;
}

/* Reload the hardware envelope exactly when an NRx4 trigger is written. */
static bool hw_psg_reset_envelope(HwPsgEnvelope* envelope)
{
    envelope->current_volume = envelope->initial_volume;
    envelope->next_step = envelope->step_time;
    hw_psg_update_envelope_dead(envelope);
    return envelope->initial_volume || envelope->direction;
}

static bool hw_psg_clock_envelope(HwPsgEnvelope* envelope)
{
    if (envelope->dead)
        return false;

    if (envelope->next_step > 0)
        envelope->next_step--;
    if (envelope->next_step > 0)
        return false;

    if (envelope->direction)
        envelope->current_volume++;
    else
        envelope->current_volume--;

    if (envelope->current_volume >= 15)
    {
        envelope->current_volume = 15;
        envelope->dead = 1;
    }
    else if (envelope->current_volume == 0)
    {
        envelope->dead = 2;
    }
    else
    {
        envelope->next_step = envelope->step_time;
    }
    return true;
}

static void hw_psg_frame_envelope(HwPsgSynth* psg)
{
    psg->frame_seq_envelope_ticks++;
    if (psg->sq1_enabled && hw_psg_clock_envelope(&psg->sq1_envelope))
    {
        hw_psg_refresh_square_sample(
            &psg->sq1_sample, psg->sq1_duty, psg->sq1_duty_index, psg->sq1_envelope.current_volume);
    }
    if (psg->sq2_enabled && hw_psg_clock_envelope(&psg->sq2_envelope))
    {
        hw_psg_refresh_square_sample(
            &psg->sq2_sample, psg->sq2_duty, psg->sq2_duty_index, psg->sq2_envelope.current_volume);
    }
    if (psg->noise_enabled)
        (void)hw_psg_clock_envelope(&psg->noise_envelope);
}

static void hw_psg_tick_frame_sequencer(HwPsgSynth* psg)
{
    psg->frame_seq_step = (uint8_t)((psg->frame_seq_step + 1u) & 7u);
    psg->frame_seq_ticks++;

    switch (psg->frame_seq_step)
    {
    case 2:
    case 6:
        hw_psg_frame_sweep(psg);
        HW_PSG_FALLTHROUGH;
    case 0:
    case 4:
        hw_psg_frame_length(psg);
        break;
    case 7:
        hw_psg_frame_envelope(psg);
        break;
    default:
        break;
    }
}

#if PORYAAAA_HW_AUDIO_TRACE
void hw_psg_run_zero_time_frame_event(HwPsgSynth* psg)
{
    if (psg && psg->master_enabled)
        hw_psg_tick_frame_sequencer(psg);
}
#endif

/* Advance the reset-time frame-event phase by an exact GBA-cycle span. */
static void hw_psg_advance_frame_sequencer(HwPsgSynth* psg, uint64_t cycles)
{
    const uint32_t frame_period = PORYAAAA_GBA_CLOCK_HZ / 512u;
    while (cycles > 0)
    {
        uint32_t until_frame = frame_period - psg->frame_seq_cycle_remainder;
        uint64_t chunk = cycles < until_frame ? cycles : until_frame;
        psg->frame_seq_cycle_remainder = (uint16_t)(psg->frame_seq_cycle_remainder + chunk);
        cycles -= chunk;
        if (psg->frame_seq_cycle_remainder == frame_period)
        {
            psg->frame_seq_cycle_remainder = 0;
            if (psg->master_enabled)
                hw_psg_tick_frame_sequencer(psg);
        }
    }
    psg->frame_seq_accum = (double)psg->frame_seq_cycle_remainder / (double)frame_period;
}

/* Power-off clears channel registers but preserves mGBA's lazy square-clock origin. */
static void hw_psg_clear_channel_state(HwPsgSynth* psg)
{
    psg->wave_cycles_until_update = 0;
    psg->wave_pending_cycles = 0;
    psg->wave_sample = 0;

    psg->sq1_freq = 0;
    psg->sq2_freq = 0;
    psg->sq1_sample = 0;
    psg->sq2_sample = 0;
    psg->wave_freq = 0;

    psg->sq1_sweep_shadow_freq = 0;
    psg->sq1_sweep_time = 8;
    psg->sq1_sweep_shift = 0;
    psg->sq1_sweep_timer = 0;
    psg->sq1_sweep_decrease = false;
    psg->sq1_sweep_enabled = false;
    psg->sq1_sweep_occurred = false;

    psg->sq1_duty = 0;
    psg->sq2_duty = 0;
    psg->sq1_length_counter = 0;
    psg->sq2_length_counter = 0;
    psg->sq1_length_enabled = false;
    psg->sq2_length_enabled = false;

    psg->sq1_envelope = (HwPsgEnvelope){.dead = 2};
    psg->sq2_envelope = (HwPsgEnvelope){.dead = 2};
    psg->wave_vol_code = 0;
    psg->wave_bank = false;
    psg->wave_size = false;

    psg->sq1_dac_enabled = false;
    psg->sq1_enabled = false;
    psg->sq2_enabled = false;
    psg->wave_enabled = false;
    psg->wave_dac_on = false;
    psg->wave_length_counter = 0;
    psg->wave_length_enabled = false;

    psg->noise_lfsr = 0;
    psg->noise_clock_shift = 0;
    psg->noise_timer_cycles = 0;
    psg->noise_pending_cycles = 0;
    psg->noise_divisor_code = 0;
    psg->noise_last_sample = 0;
    psg->noise_width_7bit = false;
    psg->noise_envelope = (HwPsgEnvelope){.dead = 2};
    psg->noise_enabled = false;
    psg->noise_length_counter = 0;
    psg->noise_length_enabled = false;
}

/* Apply mGBA's integer GBA wave-volume transform to one 4-bit sample. */
static uint8_t hw_psg_apply_wave_volume(uint8_t sample, uint8_t volume_code)
{
    uint8_t volume = (uint8_t)((volume_code >> 5) & 7u);
    if (volume > 3)
        sample = (uint8_t)(sample + (sample << 1));

    switch (volume)
    {
    case 0:
        return sample >> 4;
    case 1:
        return sample;
    case 2:
        return sample >> 1;
    default:
        return sample >> 2;
    }
}

/* Retain elapsed cycles while mGBA's lazy square clock is inactive, then
 * consume them with the frequency in effect at the next forced update. */
static bool hw_psg_advance_square_cycles(
    uint64_t* timer_cycles, uint8_t* duty_index, uint16_t frequency, uint64_t cycles, bool clock)
{
    *timer_cycles += cycles;
    if (!clock || frequency >= 2048)
        return false;

    uint32_t period = 16u * (2048u - (frequency & 0x07FFu));
    uint64_t steps = *timer_cycles / period;
    *timer_cycles %= period;
    *duty_index = (uint8_t)((*duty_index + steps) & 7u);
    return steps != 0;
}

/* Load one GBA Wave RAM word without depending on host byte order. */
static uint32_t hw_psg_read_wave_word(const HwPsgSynth* psg, int index)
{
    const uint8_t* bytes = &psg->wave_ram[index * 4];
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) | ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
}

/* Store one rotated GBA Wave RAM word in bus little-endian order. */
static void hw_psg_write_wave_word(HwPsgSynth* psg, int index, uint32_t value)
{
    uint8_t* bytes = &psg->wave_ram[index * 4];
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8);
    bytes[2] = (uint8_t)(value >> 16);
    bytes[3] = (uint8_t)(value >> 24);
}

/* Rotate the selected GBA wave bank once and latch its newly exposed nibble. */
static void hw_psg_clock_wave(HwPsgSynth* psg)
{
    int start = 7;
    int end = 0;
    if (!psg->wave_size)
    {
        if (psg->wave_bank)
            end = 4;
        else
            start = 3;
    }

    uint32_t bits_carry = hw_psg_read_wave_word(psg, end) & 0x000000F0u;
    for (int index = start; index >= end; index--)
    {
        uint32_t word = hw_psg_read_wave_word(psg, index);
        uint32_t bits = word & 0x000000F0u;
        word = ((word & 0x0F0F0F0Fu) << 4) | ((word & 0xF0F0F000u) >> 12);
        word |= bits_carry << 20;
        hw_psg_write_wave_word(psg, index, word);
        bits_carry = bits;
    }
    psg->wave_sample = hw_psg_apply_wave_volume((uint8_t)(bits_carry >> 4), psg->wave_vol_code);
}

/* Run mGBA's delayed GBA wave clock, including its 24-cycle trigger latency. */
static void hw_psg_advance_wave_cycles(HwPsgSynth* psg, uint64_t cycles)
{
    if (!psg->wave_enabled || !psg->wave_dac_on || psg->wave_freq >= 2048 || !psg->wave_cycles_until_update)
        return;
    if (cycles < psg->wave_cycles_until_update)
    {
        psg->wave_cycles_until_update -= (uint32_t)cycles;
        return;
    }

    uint32_t period = 8u * (2048u - (psg->wave_freq & 0x07FFu));
    uint64_t elapsed = cycles - psg->wave_cycles_until_update;
    uint64_t clocks = 1u + elapsed / period;
    psg->wave_cycles_until_update = period - (uint32_t)(elapsed % period);
    uint64_t clocks_to_apply = clocks & (psg->wave_size ? 0x3Fu : 0x1Fu);
    while (clocks_to_apply > 0)
    {
        hw_psg_clock_wave(psg);
        clocks_to_apply--;
    }
}

/* Clock channel 4 with mGBA GBA-mode feedback and output polarity. */
static void hw_psg_clock_noise(HwPsgSynth* psg)
{
    uint16_t feedback = (psg->noise_lfsr ^ (psg->noise_lfsr >> 1u) ^ 1u) & 1u;
    uint16_t coeff = psg->noise_width_7bit ? 0x4040u : 0x4000u;
    psg->noise_lfsr >>= 1u;
    if (feedback)
        psg->noise_lfsr |= coeff;
    else
        psg->noise_lfsr &= (uint16_t)~coeff;
    psg->noise_last_sample = (uint8_t)feedback;
}

/* Advance noise clocks at mGBA's exact trigger-relative origin. */
static void hw_psg_advance_noise_cycles(HwPsgSynth* psg, uint64_t cycles)
{
    if (!psg->noise_enabled)
        return;

    uint32_t period = (psg->noise_divisor_code ? 64u * psg->noise_divisor_code : 32u) << psg->noise_clock_shift;
    uint64_t clocks = cycles / period;
    uint64_t elapsed = (uint64_t)psg->noise_timer_cycles + cycles % period;
    clocks += elapsed / period;
    psg->noise_timer_cycles = (uint32_t)(elapsed % period);

    if (!psg->noise_width_7bit)
    {
        while (clocks >= 5)
        {
            uint16_t bits = psg->noise_lfsr & 0x3Fu;
            psg->noise_lfsr >>= 5u;
            psg->noise_lfsr |= (uint16_t)((0x4000u * kNoiseBatchMasks[bits]) >> 4u);
            psg->noise_lfsr &= 0x7FFFu;
            psg->noise_last_sample = kNoiseBatchMasks[bits] & 1u;
            clocks -= 5;
        }
    }
    while (clocks > 0)
    {
        hw_psg_clock_noise(psg);
        clocks--;
    }
}

/* Consume every Wave cycle retained since mGBA last ran channel 3. */
static void hw_psg_force_wave(HwPsgSynth* psg)
{
    hw_psg_advance_wave_cycles(psg, psg->wave_pending_cycles);
    psg->wave_pending_cycles = 0;
}

#if PORYAAAA_HW_AUDIO_TRACE
/* Consume callback lookahead before advancing a channel on the nominal trace. */
static uint64_t hw_psg_consume_trace_lookahead(uint64_t* lookahead, uint64_t cycles)
{
    uint64_t consumed = cycles < *lookahead ? cycles : *lookahead;
    *lookahead -= consumed;
    return cycles - consumed;
}
#endif

/* Retain lazy Wave cycles between forced events and keep the frame-event
 * phase on its absolute 32,768-cycle cadence while NR52 is disabled. A
 * delayed SAMPLE can retain its terminal event until observation completes. */
static void
hw_psg_advance_wave_and_frame_cycles(HwPsgSynth* psg, uint64_t cycles, bool clock_wave, bool defer_terminal_frame)
{
    const uint32_t frame_period = PORYAAAA_GBA_CLOCK_HZ / 512u;
    while (cycles > 0)
    {
        uint32_t until_frame = frame_period - psg->frame_seq_cycle_remainder;
        uint64_t chunk = cycles < until_frame ? cycles : until_frame;
        if (psg->master_enabled)
        {
#if PORYAAAA_HW_AUDIO_TRACE
            uint64_t wave_cycles = hw_psg_consume_trace_lookahead(&psg->wave_trace_lookahead, chunk);
#else
            uint64_t wave_cycles = chunk;
#endif
            psg->wave_pending_cycles += wave_cycles;
        }
        psg->frame_seq_cycle_remainder = (uint16_t)(psg->frame_seq_cycle_remainder + chunk);
        cycles -= chunk;
        if (psg->frame_seq_cycle_remainder == frame_period)
        {
#if PORYAAAA_HW_AUDIO_TRACE
            if (psg->frame_seq_event_deferred)
            {
                if (psg->master_enabled)
                    hw_psg_tick_frame_sequencer(psg);
                psg->frame_seq_event_deferred = false;
            }
#endif
            if (psg->master_enabled)
            {
                hw_psg_force_wave(psg);
#if PORYAAAA_HW_AUDIO_TRACE
                if (defer_terminal_frame && cycles == 0)
                    psg->frame_seq_event_deferred = true;
                else
#else
                (void)defer_terminal_frame;
#endif
                    hw_psg_tick_frame_sequencer(psg);
            }
            psg->frame_seq_cycle_remainder = 0;
        }
    }
    psg->frame_seq_accum = (double)psg->frame_seq_cycle_remainder / (double)frame_period;
    if (psg->master_enabled && clock_wave)
        hw_psg_force_wave(psg);
}

void hw_psg_init(HwPsgSynth* psg, float render_rate)
{
    memset(psg, 0, sizeof(*psg));
    psg->render_rate = render_rate;
    psg->sq1_sweep_time = 8;
    /* m4aSoundInit enables NR52 from the powered-off state. mGBA models
     * that transition by setting frame=7, so the first 512 Hz tick is 0. */
    hw_psg_reset_frame_sequencer(psg, 7);
    /* Match the driver's register-file defaults (m4a_driver_create) so
     * the chip starts in a "configured" state matching what real m4a
     * writes during init: NR52 master-enable on (NR50/NR51/SOUNDCNT_H
     * defaults are owned by HwMixBus, see hw_mix_init). */
    psg->master_enabled = true;
    /* NR44 trigger installs mGBA's width-specific LFSR state. */
    psg->noise_lfsr = 0;
}

void hw_psg_set_render_rate(HwPsgSynth* psg, float render_rate)
{
    psg->render_rate = render_rate;
}

/* Share channel-cycle advancement while selecting terminal frame ordering. */
static void hw_psg_advance_cycles_internal(HwPsgSynth* psg,
                                           uint64_t cycles,
                                           bool clock_sq1,
                                           bool clock_sq2,
                                           bool clock_wave,
                                           bool clock_noise,
                                           bool defer_terminal_frame)
{
    if (!psg)
        return;

#if PORYAAAA_HW_AUDIO_TRACE
    uint64_t sq1_cycles = hw_psg_consume_trace_lookahead(&psg->sq1_trace_lookahead, cycles);
    uint64_t sq2_cycles = hw_psg_consume_trace_lookahead(&psg->sq2_trace_lookahead, cycles);
#else
    uint64_t sq1_cycles = cycles;
    uint64_t sq2_cycles = cycles;
#endif
    if (hw_psg_advance_square_cycles(
            &psg->sq1_timer_cycles, &psg->sq1_duty_index, psg->sq1_freq, sq1_cycles, psg->master_enabled && clock_sq1))
    {
        hw_psg_refresh_square_sample(
            &psg->sq1_sample, psg->sq1_duty, psg->sq1_duty_index, psg->sq1_envelope.current_volume);
    }
    if (hw_psg_advance_square_cycles(
            &psg->sq2_timer_cycles, &psg->sq2_duty_index, psg->sq2_freq, sq2_cycles, psg->master_enabled && clock_sq2))
    {
        hw_psg_refresh_square_sample(
            &psg->sq2_sample, psg->sq2_duty, psg->sq2_duty_index, psg->sq2_envelope.current_volume);
    }
    if (psg->master_enabled)
    {
        psg->noise_pending_cycles += cycles;
        if (clock_noise)
        {
            hw_psg_advance_noise_cycles(psg, psg->noise_pending_cycles);
            psg->noise_pending_cycles = 0;
        }
    }
    hw_psg_advance_wave_and_frame_cycles(psg, cycles, clock_wave, defer_terminal_frame);
}

void hw_psg_advance_cycles(
    HwPsgSynth* psg, uint64_t cycles, bool clock_sq1, bool clock_sq2, bool clock_wave, bool clock_noise)
{
    hw_psg_advance_cycles_internal(psg, cycles, clock_sq1, clock_sq2, clock_wave, clock_noise, false);
}

#if PORYAAAA_HW_AUDIO_TRACE
void hw_psg_advance_staged_sample_cycles(
    HwPsgSynth* psg, uint64_t cycles, bool clock_sq1, bool clock_sq2, bool clock_wave, bool clock_noise)
{
    hw_psg_advance_cycles_internal(psg, cycles, clock_sq1, clock_sq2, clock_wave, clock_noise, true);
}

void hw_psg_run_deferred_frame_event(HwPsgSynth* psg, uint64_t observation_lookahead)
{
    if (!psg || !psg->frame_seq_event_deferred)
        return;
    if (psg->master_enabled)
    {
        bool clock_sq1 = psg->sq1_timer_cycles + observation_lookahead > 0x40000000u ||
                         (psg->sq1_enabled && psg->sq1_envelope.dead != 2);
        bool clock_sq2 = psg->sq2_timer_cycles + observation_lookahead > 0x40000000u ||
                         (psg->sq2_enabled && psg->sq2_envelope.dead != 2);
        if (hw_psg_advance_square_cycles(
                &psg->sq1_timer_cycles, &psg->sq1_duty_index, psg->sq1_freq, observation_lookahead, clock_sq1))
        {
            hw_psg_refresh_square_sample(
                &psg->sq1_sample, psg->sq1_duty, psg->sq1_duty_index, psg->sq1_envelope.current_volume);
        }
        if (hw_psg_advance_square_cycles(
                &psg->sq2_timer_cycles, &psg->sq2_duty_index, psg->sq2_freq, observation_lookahead, clock_sq2))
        {
            hw_psg_refresh_square_sample(
                &psg->sq2_sample, psg->sq2_duty, psg->sq2_duty_index, psg->sq2_envelope.current_volume);
        }
        psg->wave_pending_cycles += observation_lookahead;
        hw_psg_force_wave(psg);
        psg->sq1_trace_lookahead += observation_lookahead;
        psg->sq2_trace_lookahead += observation_lookahead;
        psg->wave_trace_lookahead += observation_lookahead;
        hw_psg_tick_frame_sequencer(psg);
    }
    psg->frame_seq_event_deferred = false;
}
#endif

void hw_psg_get_frame_sequencer_debug(const HwPsgSynth* psg, HwPsgFrameSequencerDebug* out)
{
    if (!out)
        return;
    out->frame_step = psg->frame_seq_step;
    out->frame_accum = psg->frame_seq_accum;
    out->frame_ticks = psg->frame_seq_ticks;
    out->length_ticks = psg->frame_seq_length_ticks;
    out->sweep_ticks = psg->frame_seq_sweep_ticks;
    out->envelope_ticks = psg->frame_seq_envelope_ticks;
}

void hw_psg_sample(const HwPsgSynth* psg, uint8_t* out_sq1, uint8_t* out_sq2, uint8_t* out_wave, uint8_t* out_noise)
{
    if (!psg->master_enabled)
    {
        if (out_sq1)
            *out_sq1 = 0;
        if (out_sq2)
            *out_sq2 = 0;
        if (out_wave)
            *out_wave = 0;
        if (out_noise)
            *out_noise = 0;
        return;
    }

    if (out_sq1)
        *out_sq1 = psg->sq1_enabled ? psg->sq1_sample : 0;
    if (out_sq2)
        *out_sq2 = psg->sq2_enabled ? psg->sq2_sample : 0;
    if (out_wave)
        *out_wave = psg->wave_sample;
    if (out_noise)
        *out_noise = psg->noise_enabled && psg->noise_last_sample ? psg->noise_envelope.current_volume : 0;
}

static void hw_psg_write_wave_ram_byte(HwPsgSynth* psg, uint32_t offset, uint8_t value)
{
    uint32_t bank = psg->master_enabled ? !psg->wave_bank : 1u;
    psg->wave_ram[bank * 16u + offset] = value;
}

void hw_psg_apply_event(HwPsgSynth* psg, const M4ARegWrite* ev)
{
    uint32_t v = ev->value;
    switch (ev->reg)
    {
    /* ---- Square 1 (NR10..NR14) ---- */
    case M4A_REG_NR10:
    {
        bool old_decrease = psg->sq1_sweep_decrease;
        psg->sq1_sweep_shift = (uint8_t)(v & 0x07);
        psg->sq1_sweep_decrease = (v & 0x08) != 0;
        if (psg->sq1_sweep_occurred && old_decrease && !psg->sq1_sweep_decrease)
        {
            psg->sq1_enabled = false;
        }
        psg->sq1_sweep_occurred = false;
        psg->sq1_sweep_time = (uint8_t)((v >> 4) & 0x07);
        if (!psg->sq1_sweep_time)
            psg->sq1_sweep_time = 8;
        break;
    }
    case M4A_REG_NR11:
        psg->sq1_duty = (uint8_t)((v >> 6) & 0x03);
        psg->sq1_length_counter = (uint16_t)(64u - (v & 0x3Fu));
        break;
    case M4A_REG_NR12:
        psg->sq1_dac_enabled = hw_psg_write_envelope(&psg->sq1_envelope, (uint8_t)v);
        if (!psg->sq1_dac_enabled)
            psg->sq1_enabled = false; /* NRx2 == 0 → DAC off */
        break;
    case M4A_REG_NR13:
        psg->sq1_freq = (uint16_t)((psg->sq1_freq & 0x0700) | (v & 0xFF));
        break;
    case M4A_REG_NR14:
        psg->sq1_freq = (uint16_t)((psg->sq1_freq & 0x00FF) | ((v & 0x07) << 8));
        psg->sq1_length_enabled = (v & 0x40) != 0;
        if (v & 0x80)
        {
            /* NRx4 trigger: re-arm (envelope already loaded by NR12; phase
             * is preserved per real GB hardware). */
            if (psg->sq1_length_counter == 0)
                psg->sq1_length_counter = 64;
            psg->sq1_enabled = hw_psg_reset_envelope(&psg->sq1_envelope);
            psg->sq1_sweep_shadow_freq = psg->sq1_freq;
            psg->sq1_sweep_timer = psg->sq1_sweep_time;
            psg->sq1_sweep_enabled = (psg->sq1_sweep_timer != 8) || psg->sq1_sweep_shift;
            psg->sq1_sweep_occurred = false;
            if (psg->sq1_enabled && psg->sq1_sweep_shift)
            {
                if (!hw_psg_update_sq1_sweep(psg, true))
                {
                    psg->sq1_enabled = false;
                }
            }
            hw_psg_refresh_square_sample(
                &psg->sq1_sample, psg->sq1_duty, psg->sq1_duty_index, psg->sq1_envelope.current_volume);
        }
        break;

    /* ---- Square 2 (NR21..NR24) ---- */
    case M4A_REG_NR21:
        psg->sq2_duty = (uint8_t)((v >> 6) & 0x03);
        psg->sq2_length_counter = (uint16_t)(64u - (v & 0x3Fu));
        break;
    case M4A_REG_NR22:
        if (!hw_psg_write_envelope(&psg->sq2_envelope, (uint8_t)v))
            psg->sq2_enabled = false;
        break;
    case M4A_REG_NR23:
        psg->sq2_freq = (uint16_t)((psg->sq2_freq & 0x0700) | (v & 0xFF));
        break;
    case M4A_REG_NR24:
        psg->sq2_freq = (uint16_t)((psg->sq2_freq & 0x00FF) | ((v & 0x07) << 8));
        psg->sq2_length_enabled = (v & 0x40) != 0;
        if (v & 0x80)
        {
            if (psg->sq2_length_counter == 0)
                psg->sq2_length_counter = 64;
            psg->sq2_enabled = hw_psg_reset_envelope(&psg->sq2_envelope);
            hw_psg_refresh_square_sample(
                &psg->sq2_sample, psg->sq2_duty, psg->sq2_duty_index, psg->sq2_envelope.current_volume);
        }
        break;

    /* ---- Wave (NR30..NR34) + wave RAM ---- */
    case M4A_REG_NR30:
        psg->wave_size = (v & 0x20) != 0;
        psg->wave_bank = (v & 0x40) != 0;
        psg->wave_dac_on = (v & 0x80) != 0;
        if (!psg->wave_dac_on)
            psg->wave_enabled = false;
        break;
    case M4A_REG_NR31:
        psg->wave_length_counter = (uint16_t)(256u - (v & 0xFFu));
        break;
    case M4A_REG_NR32:
        psg->wave_vol_code = (uint8_t)v;
        break;
    case M4A_REG_NR33:
        psg->wave_freq = (uint16_t)((psg->wave_freq & 0x0700) | (v & 0xFF));
        break;
    case M4A_REG_NR34:
        psg->wave_freq = (uint16_t)((psg->wave_freq & 0x00FF) | ((v & 0x07) << 8));
        psg->wave_length_enabled = (v & 0x40) != 0;
        if (v & 0x80)
        {
            if (psg->wave_length_counter == 0)
                psg->wave_length_counter = 256;
            psg->wave_enabled = psg->wave_dac_on;
        }
        if (psg->wave_enabled)
            psg->wave_cycles_until_update = 24u + 8u * (2048u - (psg->wave_freq & 0x07FFu));
        else if (v & 0x80)
            psg->wave_cycles_until_update = 0;
        break;
    case M4A_REG_WAVE_RAM_BYTE:
        hw_psg_write_wave_ram_byte(psg, (v >> 8) & 0x0F, (uint8_t)v);
        break;
    case M4A_REG_WAVE_RAM_WORD_0:
    case M4A_REG_WAVE_RAM_WORD_1:
    case M4A_REG_WAVE_RAM_WORD_2:
    case M4A_REG_WAVE_RAM_WORD_3:
    {
        uint32_t offset = 4u * (ev->reg - M4A_REG_WAVE_RAM_WORD_0);
        for (uint32_t i = 0; i < 4; i++)
            hw_psg_write_wave_ram_byte(psg, offset + i, (uint8_t)(v >> (8u * i)));
        break;
    }

    /* ---- Noise (NR41..NR44) ---- */
    case M4A_REG_NR41:
        psg->noise_length_counter = (uint16_t)(64u - (v & 0x3Fu));
        break;
    case M4A_REG_NR42:
        /* DAC gating: NRx2 with top 5 bits all zero disables the channel
         * (env vol = 0 AND direction = 0).  Mirrors square channels. */
        if (!hw_psg_write_envelope(&psg->noise_envelope, (uint8_t)v))
            psg->noise_enabled = false;
        break;
    case M4A_REG_NR43:
        psg->noise_clock_shift = (uint8_t)((v >> 4) & 0x0F);
        psg->noise_width_7bit = (v & 0x08) != 0;
        psg->noise_divisor_code = (uint8_t)(v & 0x07);
        break;
    case M4A_REG_NR44:
        psg->noise_length_enabled = (v & 0x40) != 0;
        if (v & 0x80)
        {
            /* GBA mGBA restarts the feedback register and its clock origin;
             * the latched sample remains until the first newly timed clock. */
            psg->noise_lfsr = 0;
            psg->noise_timer_cycles = 0;
            psg->noise_pending_cycles = 0;
            if (psg->noise_length_counter == 0)
                psg->noise_length_counter = 64;
            psg->noise_enabled = hw_psg_reset_envelope(&psg->noise_envelope);
        }
        break;

    /* NR52 master-enable gates the channel DACs at the synth stage
     * (real GB powers down the DAC paths when this bit is clear).
     * NR50, NR51, SOUNDCNT_H PSG vol bits, and SOUNDBIAS land on the
     * mix-bus stage (HwMixBus), not here — see hw_mix.h. */
    case M4A_REG_NR52:
        if ((v & 0x80) == 0)
        {
            psg->master_enabled = false;
            hw_psg_clear_channel_state(psg);
        }
        else if (!psg->master_enabled)
        {
            psg->master_enabled = true;
            /* mGBA sets frame=7 on enable, but its reset-time event remains
             * on the pre-existing absolute cadence. */
            psg->frame_seq_step = 7;
        }
        break;
    default:
        break;
    }
}

void hw_psg_render(
    HwPsgSynth* psg, uint8_t* out_sq1, uint8_t* out_sq2, uint8_t* out_wave, uint8_t* out_noise, int frames)
{
    if (frames <= 0)
        return;

    uint32_t gba_cycles_per_sample =
        psg->render_rate > 0.0f ? (uint32_t)((float)PORYAAAA_GBA_CLOCK_HZ / psg->render_rate + 0.5f) : 0;
    if (!psg->master_enabled)
    {
        if (out_sq1)
            memset(out_sq1, 0, (size_t)frames * sizeof(*out_sq1));
        if (out_sq2)
            memset(out_sq2, 0, (size_t)frames * sizeof(*out_sq2));
        if (out_wave)
            memset(out_wave, 0, (size_t)frames * sizeof(*out_wave));
        if (out_noise)
            memset(out_noise, 0, (size_t)frames * sizeof(*out_noise));
        for (int i = 0; i < frames; i++)
            hw_psg_advance_frame_sequencer(psg, gba_cycles_per_sample);
        return;
    }

    for (int i = 0; i < frames; i++)
    {
        if (out_sq1)
            out_sq1[i] = psg->sq1_enabled ? psg->sq1_sample : 0;

        if (out_sq2)
            out_sq2[i] = psg->sq2_enabled ? psg->sq2_sample : 0;

        if (gba_cycles_per_sample &&
            hw_psg_advance_square_cycles(
                &psg->sq1_timer_cycles, &psg->sq1_duty_index, psg->sq1_freq, gba_cycles_per_sample, true))
        {
            hw_psg_refresh_square_sample(
                &psg->sq1_sample, psg->sq1_duty, psg->sq1_duty_index, psg->sq1_envelope.current_volume);
        }
        if (gba_cycles_per_sample &&
            hw_psg_advance_square_cycles(
                &psg->sq2_timer_cycles, &psg->sq2_duty_index, psg->sq2_freq, gba_cycles_per_sample, true))
        {
            hw_psg_refresh_square_sample(
                &psg->sq2_sample, psg->sq2_duty, psg->sq2_duty_index, psg->sq2_envelope.current_volume);
        }

        /* Wave — mGBA clocks the selected GBA bank before observing the
         * DAC sample at this internal timestamp. */
        if (gba_cycles_per_sample)
            hw_psg_advance_wave_cycles(psg, gba_cycles_per_sample);
        if (out_wave)
            out_wave[i] = psg->wave_sample;

        /* Noise follows the same exact-cycle path as trace replay. */
        if (gba_cycles_per_sample)
            hw_psg_advance_noise_cycles(psg, gba_cycles_per_sample);
        if (out_noise)
            out_noise[i] = psg->noise_enabled && psg->noise_last_sample ? psg->noise_envelope.current_volume : 0;

        /* mGBA samples PSG output before running the frame event. Keep the
         * tick at the end of the internal sample so it affects the next DAC
         * observation, never the one that preceded the frame boundary. */
        hw_psg_advance_frame_sequencer(psg, gba_cycles_per_sample);
    }
}
