#include "hw_mix.h"

#include <string.h>

void hw_mix_init(HwMixBus *mix) {
    memset(mix, 0, sizeof(*mix));

    /* Driver-side defaults match m4a_driver_create (mirrors Pokemon
     * Emerald m4a.c:352): NR50 master full both sides, NR51 routes
     * everything to both sides, PSG bus 100%, DMA A→right and
     * DMA B→left both at 100%, SOUNDBIAS at canonical 0x200. */
    mix->master_vol_left  = 7;
    mix->master_vol_right = 7;
    mix->pan_mask_left    = 0x0F;
    mix->pan_mask_right   = 0x0F;
    mix->psg_volume_code  = 2;        /* 100% */
    mix->dma_a_vol_code   = 1;
    mix->dma_b_vol_code   = 1;
    mix->dma_a_right      = true;
    mix->dma_b_left       = true;
    mix->bias_level       = 0x200;
    mix->sampling_cycle   = 0;        /* quirk rate 32768 Hz */
}

void hw_mix_apply_event(HwMixBus *mix, const M4ARegWrite *ev) {
    uint32_t v = ev->value;
    switch (ev->reg) {
    case M4A_REG_NR50:
        mix->master_vol_right = (uint8_t)(v & 0x07);
        mix->master_vol_left  = (uint8_t)((v >> 4) & 0x07);
        break;
    case M4A_REG_NR51:
        mix->pan_mask_right = (uint8_t)(v & 0x0F);
        mix->pan_mask_left  = (uint8_t)((v >> 4) & 0x0F);
        break;
    case M4A_REG_SOUNDCNT_H:
        mix->psg_volume_code = (uint8_t)(v & 0x03);
        mix->dma_a_vol_code  = (uint8_t)((v >> 2) & 0x01);
        mix->dma_b_vol_code  = (uint8_t)((v >> 3) & 0x01);
        mix->dma_a_right     = (v & (1u << 8))  != 0;
        mix->dma_a_left      = (v & (1u << 9))  != 0;
        mix->dma_b_right     = (v & (1u << 12)) != 0;
        mix->dma_b_left      = (v & (1u << 13)) != 0;
        break;
    case M4A_REG_SOUNDBIAS:
        /* bits 1-9 = bias_level (10-bit; bit 0 is read-only / always 0).
         * bits 14-15 = sampling cycle = quirk-rate selector. */
        mix->bias_level     = (uint16_t)(v & 0x03FF);
        mix->sampling_cycle = (uint8_t)((v >> 14) & 0x03);
        break;
    default:
        break;
    }
}

static float gba_apply_bias(float sample, uint16_t bias_level) {
    sample += (float)bias_level;
    if (sample >= 1024.0f) {
        sample = 1023.0f;
    } else if (sample < 0.0f) {
        sample = 0.0f;
    }
    return sample - (float)bias_level;
}

void hw_mix_render(const HwMixBus *mix,
                   const float *in_sq1,
                   const float *in_sq2,
                   const float *in_wave,
                   const float *in_noise,
                   const float *in_dma_a,
                   const float *in_dma_b,
                   float *outL, float *outR, int frames) {
    if (frames <= 0) return;

    /* From here until the final output write, L/R are mGBA sample counts:
     * the same signed integer domain that gba/audio.c passes into
     * _applyBias().  That keeps the GBA DAC clip point in its native unit
     * and avoids pre-scaling individual voices into final float output. */
    const uint8_t psg_code = (uint8_t)(mix->psg_volume_code & 3);
    const float psg_shift = (float)(1u << (4u - psg_code));
    const float psg_unit = (15.0f * 8.0f) / psg_shift;
    const float dma_a_unit = mix->dma_a_vol_code ? 512.0f : 256.0f;
    const float dma_b_unit = mix->dma_b_vol_code ? 512.0f : 256.0f;

    /* NR51 pan-mask bit positions: sq1=bit0, sq2=bit1, wave=bit2, noise=bit3.
     * Pre-multiply with the side scalars so the inner loop is a sum of
     * masked products, no per-sample branches. */
    const float psg_l = psg_unit * (float)(mix->master_vol_left  + 1);
    const float psg_r = psg_unit * (float)(mix->master_vol_right + 1);
    const float sq1_l   = ((mix->pan_mask_left  & 0x01) ? psg_l : 0.0f);
    const float sq1_r   = ((mix->pan_mask_right & 0x01) ? psg_r : 0.0f);
    const float sq2_l   = ((mix->pan_mask_left  & 0x02) ? psg_l : 0.0f);
    const float sq2_r   = ((mix->pan_mask_right & 0x02) ? psg_r : 0.0f);
    const float wave_l  = ((mix->pan_mask_left  & 0x04) ? psg_l : 0.0f);
    const float wave_r  = ((mix->pan_mask_right & 0x04) ? psg_r : 0.0f);
    const float noise_l = ((mix->pan_mask_left  & 0x08) ? psg_l : 0.0f);
    const float noise_r = ((mix->pan_mask_right & 0x08) ? psg_r : 0.0f);

    const float a_l = (mix->dma_a_left  ? dma_a_unit : 0.0f);
    const float a_r = (mix->dma_a_right ? dma_a_unit : 0.0f);
    const float b_l = (mix->dma_b_left  ? dma_b_unit : 0.0f);
    const float b_r = (mix->dma_b_right ? dma_b_unit : 0.0f);

    /* mGBA's default GBA masterVolume is 0x100; _applyBias returns
     * (sample * masterVolume * 3) >> 4.  Normalize int16 output here. */
    const float output_scale = 48.0f / 32768.0f;

    for (int i = 0; i < frames; i++) {
        float L = 0.0f, R = 0.0f;

        if (in_sq1) {
            L += in_sq1[i] * sq1_l;
            R += in_sq1[i] * sq1_r;
        }
        if (in_sq2) {
            L += in_sq2[i] * sq2_l;
            R += in_sq2[i] * sq2_r;
        }
        if (in_wave) {
            L += in_wave[i] * wave_l;
            R += in_wave[i] * wave_r;
        }
        if (in_noise) {
            L += in_noise[i] * noise_l;
            R += in_noise[i] * noise_r;
        }
        if (in_dma_a) {
            L += in_dma_a[i] * a_l;
            R += in_dma_a[i] * a_r;
        }
        if (in_dma_b) {
            L += in_dma_b[i] * b_l;
            R += in_dma_b[i] * b_r;
        }

        if (outL) outL[i] = gba_apply_bias(L, mix->bias_level) * output_scale;
        if (outR) outR[i] = gba_apply_bias(R, mix->bias_level) * output_scale;
    }
}
