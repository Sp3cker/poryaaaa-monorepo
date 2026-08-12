#include "m4a_internal.h"

#include <string.h>

/* Wave channel volume codes — NR32 byte mapped to gCgb3Vol values via the
 * envelope volume index.  Real m4a writes gCgb3Vol[envVol] into NR32. */
static const uint8_t gCgb3Vol[] = {
    0x00,
    0x00,
    0x60,
    0x60,
    0x60,
    0x60,
    0x40,
    0x40,
    0x40,
    0x40,
    0x80,
    0x80,
    0x80,
    0x80,
    0x20,
    0x20,
};

/* CgbPan — pokeemerald m4a.c equivalent.  Decides hard-left / hard-right /
 * center routing based on the L/R software volumes.  Returns 1 if the
 * channel is hard-panned (in which case ch->pan is set to 0x0F or 0xF0);
 * returns 0 for centered output. */
static int cgb_pan(M4ADriverCgbChan* ch)
{
    uint32_t rightVolume = ch->rightVolume;
    uint32_t leftVolume = ch->leftVolume;

    if (rightVolume >= leftVolume)
    {
        if (rightVolume / 2 >= leftVolume)
        {
            ch->pan = 0x0F;
            return 1;
        }
    }
    else
    {
        if (leftVolume / 2 >= rightVolume)
        {
            ch->pan = 0xF0;
            return 1;
        }
    }
    return 0;
}

/* ChnVolSetCgb — pokeemerald m4a.c equivalent.  Converts the software L/R
 * volumes (post-velocity, post-track-vol, post-pan) into the hardware
 * 4-bit envelope goal and the NR51 pan routing byte. */
void m4a_chn_vol_set_cgb(M4ADriverCgbChan* ch, M4ADriverTrack* track)
{
    (void)track;
    if (!cgb_pan(ch))
    {
        ch->pan = 0xFF;
        ch->envelopeGoal = (uint32_t)(ch->leftVolume + ch->rightVolume) / 16;
    }
    else
    {
        ch->envelopeGoal = (uint32_t)(ch->leftVolume + ch->rightVolume) / 16;
        if (ch->envelopeGoal > 15)
            ch->envelopeGoal = 15;
    }
    ch->sustainGoal = (ch->envelopeGoal * ch->sustain + 15) >> 4;
    ch->pan &= ch->panMask;
}

/* CgbSound channel start — pokeemerald m4a.c equivalent.  Sets up envelope
 * state for the attack phase and queues a full register rewrite (MO_PIT |
 * MO_VOL) so the next CgbSound tick emits the ROM's wave start transaction. */
void m4a_drv_cgb_start(M4ADriverCgbChan* ch)
{
    ch->status = M4A_CHN_ON | M4A_CHN_ENV_ATTACK;
    ch->modify = M4A_MO_PIT | M4A_MO_VOL;
    ch->freshStart = true;
    ch->envelopeCounter = ch->attack;
    if (ch->attack == 0)
    {
        ch->envelopeVolume = ch->envelopeGoal;
        ch->status = M4A_CHN_ON | M4A_CHN_ENV_DECAY;
        ch->envelopeCounter = ch->decay;
        ch->envelopeStepTimeAndDir = ch->decay & 0x07u;
        if (ch->decay == 0)
        {
            if (ch->sustain == 0)
            {
                ch->status = M4A_CHN_ON | M4A_CHN_ENV_RELEASE;
            }
            else
            {
                ch->envelopeVolume = ch->sustainGoal;
                ch->status = M4A_CHN_ON | M4A_CHN_ENV_SUSTAIN;
                ch->envelopeStepTimeAndDir = 0x08u;
            }
        }
    }
    else
    {
        ch->envelopeVolume = 0;
        ch->envelopeStepTimeAndDir = (ch->attack & 0x07u) | 0x08u;
    }
}

/* Decode the wave channel's NR32 volume code (gCgb3Vol byte) into the
 * decoded `wave_volume_shift` field used by hw_audio.  Convention extended
 * beyond the plan's 0=mute / 1=100% / 2=50% / 3=25% to also cover the
 * 75% mode (NR32 bit 7 set). */
static uint8_t wave_vol_shift_from_nr32(uint8_t nr32)
{
    if (nr32 == 0)
        return 0; /* mute */
    if (nr32 & 0x80)
        return 4; /* 75% mode */
    switch ((nr32 >> 5) & 3)
    {
    case 1:
        return 1; /* 100% */
    case 2:
        return 2; /* 50%  */
    case 3:
        return 3; /* 25%  */
    default:
        return 0;
    }
}

/* Encode this CGB channel's pan byte into the per-channel bit of the
 * register file's NR51 pan masks.  bit_i = 1 iff this channel sends to
 * that side. */
static void apply_pan_to_regs(M4ARegisterFile* regs, int chanIdx, uint8_t pan)
{
    uint8_t bit = (uint8_t)(1 << chanIdx);
    if (pan & 0xF0)
        regs->pan_mask_left |= bit;
    else
        regs->pan_mask_left &= (uint8_t)~bit;
    if (pan & 0x0F)
        regs->pan_mask_right |= bit;
    else
        regs->pan_mask_right &= (uint8_t)~bit;
}

/* Encode and emit a Layer-1.5 NR51 event reflecting the current pan
 * masks.  Called whenever an envelope/volume update changes a channel's
 * routing; CgbOscOff does not modify NR51. */
static void emit_nr51_event(M4ADriver* drv)
{
    uint32_t nr51 = ((uint32_t)drv->regs.pan_mask_left << 4) | (uint32_t)drv->regs.pan_mask_right;
    m4a_internal_emit_event(drv, M4A_REG_NR51, nr51);
}

/* NRx2 combines MP2K's shadow volume with the active hardware envelope phase. */
static uint32_t encode_nrx2(uint8_t envVol, uint8_t stepTimeAndDir)
{
    return ((uint32_t)(envVol & 0x0F) << 4) | (stepTimeAndDir & 0x0Fu);
}

/* NRx4 byte: bit 7 = trigger, bit 6 = length-enable, bits 2-0 = freq hi.
 * For noise (NR44) bits 2-0 are unused. */
static uint32_t encode_nrx4(uint16_t freq, bool trigger, bool lengthEnable)
{
    return (trigger ? 0x80u : 0u) | (lengthEnable ? 0x40u : 0u) | ((uint32_t)(freq >> 8) & 0x07u);
}

static uint16_t cgb_pitch_freq_for_registers(M4ADriver* drv, const M4ADriverCgbChan* ch)
{
    uint16_t freq = ch->frequency;

    if ((ch->voiceType & VOICE_TYPE_FIX) && ch->type != 4)
    {
        if (drv->regs.bias_sampling_cycle == 0)
        {
            freq = (uint16_t)((freq + 2u) & 0x07FCu);
        }
        else if (drv->regs.bias_sampling_cycle == 1)
        {
            freq = (uint16_t)((freq + 1u) & 0x07FEu);
        }
    }

    return freq;
}

static void emit_duty_write(M4ADriver* drv, const M4ADriverCgbChan* ch)
{
    M4ARegisterFile* r = &drv->regs;
    switch (ch->type)
    {
    case 1:
        r->sq1_duty = ch->dutyCycle & 0x03;
        r->sq1_length = ch->length & 0x3F;
        m4a_internal_emit_event(drv, M4A_REG_NR11, ((uint32_t)(ch->dutyCycle & 0x03) << 6) | (ch->length & 0x3F));
        break;
    case 2:
        r->sq2_duty = ch->dutyCycle & 0x03;
        r->sq2_length = ch->length & 0x3F;
        m4a_internal_emit_event(drv, M4A_REG_NR21, ((uint32_t)(ch->dutyCycle & 0x03) << 6) | (ch->length & 0x3F));
        break;
    default:
        break;
    }
}

/* Emit the channel initialization writes that CgbSound performs before pitch. */
static void emit_start_write(M4ADriver* drv, M4ADriverCgbChan* ch)
{
    M4ARegisterFile* r = &drv->regs;
    switch (ch->type)
    {
    case 1:
    {
        r->sq1_sweep_pace = (ch->sweep >> 4) & 0x07;
        r->sq1_sweep_dir = (ch->sweep & 0x08) ? -1 : +1;
        r->sq1_sweep_step = ch->sweep & 0x07;
        m4a_internal_emit_event(drv, M4A_REG_NR10, ch->sweep);
        emit_duty_write(drv, ch);
        break;
    }
    case 2:
    {
        emit_duty_write(drv, ch);
        break;
    }
    case 3:
        if (ch->wavePointer && ch->wavePointer != ch->loadedWavePointer)
        {
            m4a_internal_emit_event(drv, M4A_REG_NR30, 0x40u);
            for (uint32_t i = 0; i < 4; i++)
                m4a_internal_emit_event(drv, (M4ARegId)(M4A_REG_WAVE_RAM_WORD_0 + i), ch->wavePointer[i]);
            memcpy(r->wave_ram, ch->wavePointer, sizeof(r->wave_ram));
            ch->loadedWavePointer = ch->wavePointer;
        }
        r->wave_length = ch->length;
        r->wave_length_enable = ch->length != 0;
        m4a_internal_emit_event(drv, M4A_REG_NR30, 0);
        m4a_internal_emit_event(drv, M4A_REG_NR31, ch->length);
        break;
    case 4:
    {
        r->noise_length = ch->length & 0x3F;
        r->noise_clock_shift = (ch->frequency >> 4) & 0x0F;
        r->noise_divisor_code = ch->frequency & 0x07;
        r->noise_width_7bit = (ch->frequency & 0x08) != 0;
        m4a_internal_emit_event(drv, M4A_REG_NR41, ch->length & 0x3F);
        m4a_internal_emit_event(drv, M4A_REG_NR43, ch->frequency & 0xFF);
        break;
    }
    }
}

/* Emit CgbSound's pan, hardware-envelope, and triggered control writes. */
static void emit_vol_write(M4ADriver* drv, M4ADriverCgbChan* ch, int idx)
{
    M4ARegisterFile* r = &drv->regs;
    bool lengthEnable = ch->length != 0;

    apply_pan_to_regs(r, idx, ch->pan);
    if (ch->type != 3)
        emit_nr51_event(drv);
    switch (ch->type)
    {
    case 1:
    {
        uint16_t freq = cgb_pitch_freq_for_registers(drv, ch);
        r->sq1_env_volume = ch->envelopeVolume & 0x0F;
        r->sq1_enabled = (ch->status & M4A_CHN_ON) != 0;
        r->trigger_sq1 = true;
        m4a_internal_emit_event(drv, M4A_REG_NR12, encode_nrx2(ch->envelopeVolume, ch->envelopeStepTimeAndDir));
        m4a_internal_emit_event(drv, M4A_REG_NR14, encode_nrx4(freq, /*trigger=*/true, lengthEnable));
        break;
    }
    case 2:
    {
        uint16_t freq = cgb_pitch_freq_for_registers(drv, ch);
        r->sq2_env_volume = ch->envelopeVolume & 0x0F;
        r->sq2_enabled = (ch->status & M4A_CHN_ON) != 0;
        r->trigger_sq2 = true;
        m4a_internal_emit_event(drv, M4A_REG_NR22, encode_nrx2(ch->envelopeVolume, ch->envelopeStepTimeAndDir));
        m4a_internal_emit_event(drv, M4A_REG_NR24, encode_nrx4(freq, /*trigger=*/true, lengthEnable));
        break;
    }
    case 3:
    {
        uint16_t freq = cgb_pitch_freq_for_registers(drv, ch);
        uint8_t nr32 = gCgb3Vol[ch->envelopeVolume & 0x0F];
        r->wave_volume_shift = wave_vol_shift_from_nr32(nr32);
        r->wave_enabled = (ch->status & M4A_CHN_ON) != 0;
        r->trigger_wave = ch->freshStart;
        emit_nr51_event(drv);
        m4a_internal_emit_event(drv, M4A_REG_NR32, nr32);
        if (ch->freshStart)
        {
            r->wave_dac_on = true;
            m4a_internal_emit_event(drv, M4A_REG_NR30, 0x80u);
            m4a_internal_emit_event(drv, M4A_REG_NR34, encode_nrx4(freq, /*trigger=*/true, lengthEnable));
        }
        break;
    }
    case 4:
        r->noise_env_volume = ch->envelopeVolume & 0x0F;
        r->noise_enabled = (ch->status & M4A_CHN_ON) != 0;
        r->trigger_noise = true;
        m4a_internal_emit_event(drv, M4A_REG_NR42, encode_nrx2(ch->envelopeVolume, ch->envelopeStepTimeAndDir));
        m4a_internal_emit_event(drv, M4A_REG_NR44, encode_nrx4(0, /*trigger=*/true, lengthEnable));
        break;
    }
}

/* Write pitch-only fields for one CGB channel.  Called when MO_PIT fires
 * (pitch bend, LFO vibrato, sweep echo) without an envelope event.  No
 * trigger latch — the chip continues with its current LFSR / phase /
 * sweep state.  Emits NRx3 + NRx4-no-trigger events. */
static void emit_pit_write(M4ADriver* drv, M4ADriverCgbChan* ch)
{
    M4ARegisterFile* r = &drv->regs;
    bool lengthEnable = ch->length != 0;
    switch (ch->type)
    {
    case 1:
    {
        uint16_t freq = cgb_pitch_freq_for_registers(drv, ch);
        r->sq1_freq = freq & 0x07FF;
        m4a_internal_emit_event(drv, M4A_REG_NR13, freq & 0xFF);
        m4a_internal_emit_event(drv, M4A_REG_NR14, encode_nrx4(freq, /*trigger=*/false, lengthEnable));
        break;
    }
    case 2:
    {
        uint16_t freq = cgb_pitch_freq_for_registers(drv, ch);
        r->sq2_freq = freq & 0x07FF;
        m4a_internal_emit_event(drv, M4A_REG_NR23, freq & 0xFF);
        m4a_internal_emit_event(drv, M4A_REG_NR24, encode_nrx4(freq, /*trigger=*/false, lengthEnable));
        break;
    }
    case 3:
    {
        uint16_t freq = cgb_pitch_freq_for_registers(drv, ch);
        r->wave_freq = freq & 0x07FF;
        m4a_internal_emit_event(drv, M4A_REG_NR33, freq & 0xFF);
        m4a_internal_emit_event(drv, M4A_REG_NR34, encode_nrx4(freq, ch->freshStart, lengthEnable));
        break;
    }
    case 4:
        /* Noise pitch is encoded in NR43; CgbSound follows it with NR44
         * carrying the current control bits and no trigger. */
        r->noise_clock_shift = (ch->frequency >> 4) & 0x0F;
        r->noise_divisor_code = ch->frequency & 0x07;
        r->noise_width_7bit = (ch->frequency & 0x08) != 0;
        m4a_internal_emit_event(drv, M4A_REG_NR43, ch->frequency & 0xFF);
        m4a_internal_emit_event(drv, M4A_REG_NR44, encode_nrx4(0, /*trigger=*/false, lengthEnable));
        break;
    }
}

/* Applies the ROM's CgbOscOff writes while marking the driver channel free. */
void m4a_drv_cgb_disable(M4ADriver* drv, M4ADriverCgbChan* ch, int idx)
{
    (void)idx;
    M4ARegisterFile* r = &drv->regs;
    switch (ch->type)
    {
    case 1:
        r->sq1_enabled = false;
        r->sq1_env_volume = 0;
        r->trigger_sq1 = false;
        m4a_internal_emit_event(drv, M4A_REG_NR12, 0x08u);
        m4a_internal_emit_event(drv, M4A_REG_NR14, 0x80u);
        break;
    case 2:
        r->sq2_enabled = false;
        r->sq2_env_volume = 0;
        r->trigger_sq2 = false;
        m4a_internal_emit_event(drv, M4A_REG_NR22, 0x08u);
        m4a_internal_emit_event(drv, M4A_REG_NR24, 0x80u);
        break;
    case 3:
        r->wave_enabled = false;
        r->wave_dac_on = false;
        r->trigger_wave = false;
        m4a_internal_emit_event(drv, M4A_REG_NR30, 0);
        break;
    case 4:
        r->noise_enabled = false;
        r->noise_env_volume = 0;
        r->trigger_noise = false;
        m4a_internal_emit_event(drv, M4A_REG_NR42, 0x08u);
        m4a_internal_emit_event(drv, M4A_REG_NR44, 0x80u);
        break;
    }
}

/* Tick one CGB channel's envelope.  c15==0 every 15 vblanks fires a double
 * step, matching m4a's 1/64 s envelope-counter schedule.  At end of tick
 * the `modify` field tells us what to write to the register file. */
static void tick_one(M4ADriver* drv, M4ADriverCgbChan* ch, int idx)
{
    if (!(ch->status & M4A_CHN_ON))
        return;

    if (ch->freshStart)
        emit_start_write(drv, ch);

    if (ch->status & M4A_CHN_IEC)
    {
        ch->pseudoEchoLength--;
        if ((int8_t)ch->pseudoEchoLength <= 0)
        {
            ch->status = 0;
            m4a_drv_cgb_disable(drv, ch, idx);
            return;
        }
        goto done;
    }

    if ((ch->status & M4A_CHN_STOP) && (ch->status & M4A_CHN_ENV_MASK))
    {
        ch->status &= ~M4A_CHN_ENV_MASK; /* → ENV_RELEASE */
        ch->envelopeCounter = ch->release;
        if (ch->release != 0)
        {
            if (ch->type != 3)
                ch->envelopeStepTimeAndDir = ch->release & 0x07u;
            ch->modify |= M4A_MO_VOL;
            goto done;
        }
        else
        {
            goto pseudo_echo;
        }
    }

    {
        int doubleStep = (drv->c15 == 0) ? 1 : 0;
        int steps = 0;

    step_repeat:
        if (ch->envelopeCounter == 0)
        {
            m4a_chn_vol_set_cgb(ch, &drv->tracks[ch->trackIndex]);
            uint8_t envState = ch->status & M4A_CHN_ENV_MASK;
            if (ch->type == 3)
                ch->modify |= M4A_MO_VOL;

            if (envState == M4A_CHN_ENV_RELEASE)
            {
                ch->envelopeVolume--;
                if ((int8_t)ch->envelopeVolume <= 0)
                {
                pseudo_echo:
                    ch->envelopeVolume = ((ch->envelopeGoal * ch->pseudoEchoVolume) + 0xFF) >> 8;
                    if (ch->envelopeVolume)
                    {
                        ch->status |= M4A_CHN_IEC;
                        ch->modify |= M4A_MO_VOL;
                        if (ch->type != 3)
                            ch->envelopeStepTimeAndDir = 0x08u;
                        goto done;
                    }
                    else
                    {
                        ch->status = 0;
                        m4a_drv_cgb_disable(drv, ch, idx);
                        return;
                    }
                }
                ch->envelopeCounter = ch->release;
            }
            else if (envState == M4A_CHN_ENV_SUSTAIN)
            {
                /* The ROM updates only its software sustain volume here.
                 * A hardware volume transaction requires an independent
                 * modify flag from the track or another envelope phase. */
                ch->envelopeVolume = ch->sustainGoal;
                ch->envelopeCounter = 7;
            }
            else if (envState == M4A_CHN_ENV_DECAY)
            {
                ch->envelopeVolume--;
                if ((int8_t)ch->envelopeVolume <= (int8_t)ch->sustainGoal)
                {
                    if (ch->sustain == 0)
                    {
                        ch->status &= ~M4A_CHN_ENV_MASK; /* → RELEASE */
                        goto pseudo_echo;
                    }
                    ch->status--; /* DECAY → SUSTAIN */
                    ch->modify |= M4A_MO_VOL;
                    if (ch->type != 3)
                        ch->envelopeStepTimeAndDir = 0x08u;
                    ch->envelopeVolume = ch->sustainGoal;
                    ch->envelopeCounter = 7;
                    goto done;
                }
                ch->envelopeCounter = ch->decay;
            }
            else
            { /* ATTACK */
                ch->envelopeVolume++;
                if (ch->envelopeVolume >= ch->envelopeGoal)
                {
                    ch->status--; /* ATTACK → DECAY */
                    ch->envelopeCounter = ch->decay;
                    if (ch->decay != 0)
                    {
                        ch->modify |= M4A_MO_VOL;
                        ch->envelopeVolume = ch->envelopeGoal;
                        if (ch->type != 3)
                            ch->envelopeStepTimeAndDir = ch->decay & 0x07u;
                    }
                    else
                    {
                        if (ch->sustain == 0)
                        {
                            ch->status &= ~M4A_CHN_ENV_MASK;
                            goto pseudo_echo;
                        }
                        ch->status--;
                        ch->envelopeVolume = ch->sustainGoal;
                        ch->envelopeCounter = 7;
                        ch->modify |= M4A_MO_VOL;
                        if (ch->type != 3)
                            ch->envelopeStepTimeAndDir = 0x08u;
                    }
                    goto done;
                }
                ch->envelopeCounter = ch->attack;
            }
        }

        ch->envelopeCounter--;
        if (doubleStep && steps == 0)
        {
            steps = 1;
            goto step_repeat;
        }
    }

done:
    if (ch->modify & M4A_MO_DUTY)
        emit_duty_write(drv, ch);

    /* CgbSound applies pitch before pan/envelope writes on each tick. */
    if (ch->modify & M4A_MO_PIT)
        emit_pit_write(drv, ch);
    if (ch->modify & M4A_MO_VOL)
        emit_vol_write(drv, ch, idx);
    ch->modify = 0;
    ch->freshStart = false;
}

/* CgbSound — pokeemerald m4a.c equivalent.  One-shot per-vblank tick of all
 * four CGB channels.  Caller (m4a_advance) maintains drv->c15. */
void m4a_cgb_sound(M4ADriver* drv)
{
    if (!drv)
        return;
    for (int i = 0; i < M4A_MAX_CGB_CHANNELS; i++)
    {
        tick_one(drv, &drv->cgb[i], i);
    }
}
