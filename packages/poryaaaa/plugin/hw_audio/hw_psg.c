#include "hw_psg.h"

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

#define HW_PSG_FRAME_SEQ_HZ 512.0

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

static void hw_psg_clock_envelope(HwPsgEnvelope* envelope)
{
    if (envelope->dead)
        return;

    if (envelope->next_step > 0)
        envelope->next_step--;
    if (envelope->next_step > 0)
        return;

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
}

static void hw_psg_frame_envelope(HwPsgSynth* psg)
{
    psg->frame_seq_envelope_ticks++;
    if (psg->sq1_enabled)
        hw_psg_clock_envelope(&psg->sq1_envelope);
    if (psg->sq2_enabled)
        hw_psg_clock_envelope(&psg->sq2_envelope);
    if (psg->noise_enabled)
        hw_psg_clock_envelope(&psg->noise_envelope);
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

static void hw_psg_advance_frame_sequencer(HwPsgSynth* psg)
{
    if (psg->render_rate <= 0.0f)
        return;
    psg->frame_seq_accum += HW_PSG_FRAME_SEQ_HZ / (double)psg->render_rate;
    while (psg->frame_seq_accum >= 1.0)
    {
        psg->frame_seq_accum -= 1.0;
        hw_psg_tick_frame_sequencer(psg);
    }
}

/* Power-off clears channel registers but preserves mGBA's lazy square-clock origin. */
static void hw_psg_clear_channel_state(HwPsgSynth* psg)
{
    psg->wave_cycles_until_update = 0;
    psg->wave_pending_cycles = 0;
    psg->wave_sample = 0;

    psg->sq1_freq = 0;
    psg->sq2_freq = 0;
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
    psg->noise_phase = 0;
    psg->noise_clock_shift = 0;
    psg->noise_timer_cycles = 0;
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
static void hw_psg_advance_square_cycles(
    uint64_t* timer_cycles, uint8_t* duty_index, uint16_t frequency, uint64_t cycles, bool clock)
{
    *timer_cycles += cycles;
    if (!clock || frequency >= 2048)
        return;

    uint32_t period = 16u * (2048u - (frequency & 0x07FFu));
    uint64_t steps = *timer_cycles / period;
    *timer_cycles %= period;
    *duty_index = (uint8_t)((*duty_index + steps) & 7u);
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

static void hw_psg_clock_noise(HwPsgSynth* psg)
{
    uint16_t lsb = psg->noise_lfsr & 1u;
    uint16_t coeff = psg->noise_width_7bit ? 0x0060u : 0x6000u;
    psg->noise_lfsr >>= 1;
    psg->noise_lfsr ^= (uint16_t)(lsb * coeff);
    psg->noise_last_sample = (uint8_t)lsb;
}

static void hw_psg_advance_noise_cycles(HwPsgSynth* psg, uint64_t cycles)
{
    if (!psg->noise_enabled)
        return;

    uint32_t period = (psg->noise_divisor_code ? 64u * psg->noise_divisor_code : 32u) << psg->noise_clock_shift;
    uint64_t clocks = cycles / period;
    uint64_t elapsed = (uint64_t)psg->noise_timer_cycles + cycles % period;
    clocks += elapsed / period;
    psg->noise_timer_cycles = (uint32_t)(elapsed % period);

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

/* Retain lazy Wave cycles between forced events, but consume them before
 * each frame tick so length expiry gates the remaining suffix. */
static void hw_psg_advance_wave_and_frame_cycles(HwPsgSynth* psg, uint64_t cycles, bool clock_wave)
{
    const uint32_t frame_period = 16777216u / 512u;
    while (cycles > 0)
    {
        uint32_t until_frame = frame_period - psg->frame_seq_cycle_remainder;
        uint64_t chunk = cycles < until_frame ? cycles : until_frame;
        psg->wave_pending_cycles += chunk;
        psg->frame_seq_cycle_remainder = (uint16_t)(psg->frame_seq_cycle_remainder + chunk);
        cycles -= chunk;
        if (psg->frame_seq_cycle_remainder == frame_period)
        {
            hw_psg_force_wave(psg);
            psg->frame_seq_cycle_remainder = 0;
            hw_psg_tick_frame_sequencer(psg);
        }
    }
    if (clock_wave)
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

void hw_psg_advance_cycles(HwPsgSynth* psg, uint64_t cycles, bool clock_sq1, bool clock_sq2, bool clock_wave)
{
    if (!psg)
        return;

    hw_psg_advance_square_cycles(
        &psg->sq1_timer_cycles, &psg->sq1_duty_index, psg->sq1_freq, cycles, psg->master_enabled && clock_sq1);
    hw_psg_advance_square_cycles(
        &psg->sq2_timer_cycles, &psg->sq2_duty_index, psg->sq2_freq, cycles, psg->master_enabled && clock_sq2);
    if (!psg->master_enabled)
        return;
    if (cycles)
        hw_psg_advance_noise_cycles(psg, cycles);
    hw_psg_advance_wave_and_frame_cycles(psg, cycles, clock_wave);
}

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

void hw_psg_sample(const HwPsgSynth* psg, float* out_sq1, float* out_sq2, float* out_wave, float* out_noise)
{
    if (!psg->master_enabled)
    {
        if (out_sq1)
            *out_sq1 = 0.0f;
        if (out_sq2)
            *out_sq2 = 0.0f;
        if (out_wave)
            *out_wave = 0.0f;
        if (out_noise)
            *out_noise = 0.0f;
        return;
    }

    if (out_sq1)
    {
        float bit = (kDutyPatterns[psg->sq1_duty] >> psg->sq1_duty_index) & 1u ? 1.0f : 0.0f;
        *out_sq1 = psg->sq1_enabled ? bit * (psg->sq1_envelope.current_volume / 15.0f) : 0.0f;
    }
    if (out_sq2)
    {
        float bit = (kDutyPatterns[psg->sq2_duty] >> psg->sq2_duty_index) & 1u ? 1.0f : 0.0f;
        *out_sq2 = psg->sq2_enabled ? bit * (psg->sq2_envelope.current_volume / 15.0f) : 0.0f;
    }
    if (out_wave)
    {
        *out_wave =
            psg->wave_enabled && psg->wave_dac_on && psg->wave_freq < 2048 ? (float)psg->wave_sample / 15.0f : 0.0f;
    }
    if (out_noise)
    {
        float bit = psg->noise_last_sample ? 1.0f : 0.0f;
        *out_noise = psg->noise_enabled ? bit * (psg->noise_envelope.current_volume / 15.0f) : 0.0f;
    }
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
    {
        uint32_t addr = (v >> 8) & 0x0F;
        uint8_t byte = (uint8_t)(v & 0xFF);
        uint32_t bank = psg->master_enabled ? !psg->wave_bank : 1u;
        psg->wave_ram[bank * 16u + addr] = byte;
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
            psg->noise_lfsr = psg->noise_width_7bit ? 0x007Fu : 0x7FFFu;
            psg->noise_last_sample = 0;
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
            bool was_enabled = psg->master_enabled;
            psg->master_enabled = false;
            hw_psg_clear_channel_state(psg);
            if (was_enabled)
                hw_psg_reset_frame_sequencer(psg, 7);
        }
        else if (!psg->master_enabled)
        {
            psg->master_enabled = true;
            hw_psg_reset_frame_sequencer(psg, 7);
        }
        break;
    default:
        break;
    }
}

void hw_psg_render(HwPsgSynth* psg, float* out_sq1, float* out_sq2, float* out_wave, float* out_noise, int frames)
{
    if (frames <= 0)
        return;
    if (!psg->master_enabled)
    {
        /* NR52 master-disable: powered-down DACs.  Zero all outputs. */
        if (out_sq1)
            memset(out_sq1, 0, (size_t)frames * sizeof(float));
        if (out_sq2)
            memset(out_sq2, 0, (size_t)frames * sizeof(float));
        if (out_wave)
            memset(out_wave, 0, (size_t)frames * sizeof(float));
        if (out_noise)
            memset(out_noise, 0, (size_t)frames * sizeof(float));
        return;
    }

    uint32_t gba_cycles_per_sample = psg->render_rate > 0.0f ? (uint32_t)(16777216.0f / psg->render_rate + 0.5f) : 0;

    /* Noise timer: noise_freq_hz = 524288 / divisor / 2^(shift+1), where
     * divisor = (code == 0 ? 0.5 : code).  Convert to clocks-per-host-sample,
     * split into whole-clocks (advanced unconditionally) + fractional
     * (advanced when noise_phase overflows).  At render rate (131072 Hz)
     * noise_freq can still exceed Nyquist by ~4× — we step the LFSR
     * through every whole clock but only sample the latest LSB per
     * output frame. The downstream current mGBA sinc frontend interpolates
     * the resulting DAC samples at the host rate. */
    int noise_whole_clocks = 0;
    uint32_t noise_phase_inc = 0;
    if (psg->noise_enabled && psg->render_rate > 0.0f)
    {
        float divisor = (psg->noise_divisor_code == 0) ? 0.5f : (float)psg->noise_divisor_code;
        float noise_hz = 524288.0f / divisor / (float)(1u << (psg->noise_clock_shift + 1u));
        double clocks_per_sample = (double)noise_hz / (double)psg->render_rate;
        if (clocks_per_sample < 0.0)
            clocks_per_sample = 0.0;
        noise_whole_clocks = (int)clocks_per_sample;
        double frac = clocks_per_sample - (double)noise_whole_clocks;
        if (frac < 0.0)
            frac = 0.0;
        if (frac > 1.0)
            frac = 1.0;
        noise_phase_inc = (uint32_t)(frac * 4294967296.0);
    }

    for (int i = 0; i < frames; i++)
    {
        /* Square 1 — mGBA GBA-mode unipolar.  In gb_audio.c
         * `GBAudioSamplePSG` the GBA path uses `dcOffset = 0` and each
         * `audio->chN.sample` is the unsigned current channel value
         * (square: env_vol when duty bit is high, 0 when low).  We
         * mirror that as a [0, env_vol/15] float here so the chip
         * output has the positive DC offset that real hardware leaks
         * through `_applyBias`.  bipolar synth was a poryaaaa
         * simplification that broke per-channel + full-mix DC parity
         * against mGBA captures (~3.4% full-scale on littleroot_test). */
        if (out_sq1)
        {
            float s = 0.0f;
            if (psg->sq1_enabled)
            {
                float bit = (kDutyPatterns[psg->sq1_duty] >> psg->sq1_duty_index) & 1u ? 1.0f : 0.0f;
                s = bit * (psg->sq1_envelope.current_volume / 15.0f);
            }
            out_sq1[i] = s;
        }

        /* Square 2 — same unipolar convention as Square 1. */
        if (out_sq2)
        {
            float s = 0.0f;
            if (psg->sq2_enabled)
            {
                float bit = (kDutyPatterns[psg->sq2_duty] >> psg->sq2_duty_index) & 1u ? 1.0f : 0.0f;
                s = bit * (psg->sq2_envelope.current_volume / 15.0f);
            }
            out_sq2[i] = s;
        }

        if (gba_cycles_per_sample && psg->sq1_freq < 2048)
        {
            uint32_t period = 16u * (2048u - psg->sq1_freq);
            psg->sq1_timer_cycles += gba_cycles_per_sample;
            if (psg->sq1_timer_cycles >= period)
            {
                uint64_t steps = psg->sq1_timer_cycles / period;
                psg->sq1_timer_cycles -= steps * period;
                psg->sq1_duty_index = (uint8_t)((psg->sq1_duty_index + steps) & 7u);
            }
        }
        if (gba_cycles_per_sample && psg->sq2_freq < 2048)
        {
            uint32_t period = 16u * (2048u - psg->sq2_freq);
            psg->sq2_timer_cycles += gba_cycles_per_sample;
            if (psg->sq2_timer_cycles >= period)
            {
                uint64_t steps = psg->sq2_timer_cycles / period;
                psg->sq2_timer_cycles -= steps * period;
                psg->sq2_duty_index = (uint8_t)((psg->sq2_duty_index + steps) & 7u);
            }
        }

        /* Wave — mGBA clocks the selected GBA bank before observing the
         * DAC sample at this internal timestamp. */
        if (gba_cycles_per_sample)
            hw_psg_advance_wave_cycles(psg, gba_cycles_per_sample);
        if (out_wave)
        {
            out_wave[i] =
                psg->wave_enabled && psg->wave_dac_on && psg->wave_freq < 2048 ? (float)psg->wave_sample / 15.0f : 0.0f;
        }

        /* Noise follows mGBA 0.10.5: emit the old low bit, shift, then
         * XOR that bit into taps 5/6 or 13/14. */
        if (psg->noise_enabled)
        {
            int extra = 0;
            uint32_t prev_phase = psg->noise_phase;
            psg->noise_phase += noise_phase_inc;
            if (psg->noise_phase < prev_phase)
                extra = 1;
            int clocks = noise_whole_clocks + extra;
            uint16_t coeff = psg->noise_width_7bit ? 0x0060u : 0x6000u;
            for (int c = 0; c < clocks; c++)
            {
                uint16_t lsb = psg->noise_lfsr & 1u;
                psg->noise_lfsr >>= 1;
                psg->noise_lfsr ^= (uint16_t)(lsb * coeff);
                psg->noise_last_sample = (uint8_t)lsb;
            }
            if (out_noise)
            {
                float bit = psg->noise_last_sample ? 1.0f : 0.0f;
                out_noise[i] = bit * ((float)psg->noise_envelope.current_volume / 15.0f);
            }
        }
        else if (out_noise)
        {
            out_noise[i] = 0.0f;
        }

        /* mGBA samples PSG output before running the frame event.  Keep
         * the tick at the end of the internal sample so future audible
         * length/sweep/envelope hooks affect the following sample, not
         * the boundary sample that preceded the frame event. */
        hw_psg_advance_frame_sequencer(psg);
    }
}
