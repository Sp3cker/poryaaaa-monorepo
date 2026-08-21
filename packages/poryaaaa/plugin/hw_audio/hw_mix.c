#include "hw_mix.h"

#include <string.h>

void hw_mix_init(HwMixBus* mix)
{
    memset(mix, 0, sizeof(*mix));

    /* Driver-side defaults match m4a_driver_create (mirrors Pokemon
     * Emerald m4a.c:352): NR50 master full both sides, NR51 routes
     * everything to both sides, PSG bus 100%, DMA A→right and
     * DMA B→left both at 100%, SOUNDBIAS at canonical 0x200. */
    mix->master_vol_left = 7;
    mix->master_vol_right = 7;
    mix->pan_mask_left = 0x0F;
    mix->pan_mask_right = 0x0F;
    mix->psg_volume_code = 2; /* 100% */
    mix->dma_a_vol_code = 1;
    mix->dma_b_vol_code = 1;
    mix->dma_a_right = true;
    mix->dma_b_left = true;
    mix->bias_level = 0x200;
    mix->sampling_cycle = 1; /* m4a_init selects the 65536 Hz DAC rate */
}

void hw_mix_apply_event(HwMixBus* mix, const M4ARegWrite* ev)
{
    uint32_t v = ev->value;
    switch (ev->reg)
    {
    case M4A_REG_NR50:
        mix->master_vol_right = (uint8_t)(v & 0x07);
        mix->master_vol_left = (uint8_t)((v >> 4) & 0x07);
        break;
    case M4A_REG_NR51:
        mix->pan_mask_right = (uint8_t)(v & 0x0F);
        mix->pan_mask_left = (uint8_t)((v >> 4) & 0x0F);
        break;
    case M4A_REG_SOUNDCNT_H:
        mix->psg_volume_code = (uint8_t)(v & 0x03);
        mix->dma_a_vol_code = (uint8_t)((v >> 2) & 0x01);
        mix->dma_b_vol_code = (uint8_t)((v >> 3) & 0x01);
        mix->dma_a_right = (v & (1u << 8)) != 0;
        mix->dma_a_left = (v & (1u << 9)) != 0;
        mix->dma_b_right = (v & (1u << 12)) != 0;
        mix->dma_b_left = (v & (1u << 13)) != 0;
        break;
    case M4A_REG_SOUNDBIAS:
        /* bits 1-9 = bias_level (10-bit; bit 0 is read-only / always 0).
         * bits 14-15 = sampling cycle = quirk-rate selector. */
        mix->bias_level = (uint16_t)(v & 0x03FF);
        mix->sampling_cycle = (uint8_t)((v >> 14) & 0x03);
        break;
    default:
        break;
    }
}

/* Apply mGBA's 10-bit GBA DAC bias, clip, and fixed 0x100 master volume. */
static int16_t gba_apply_bias(int32_t sample, uint16_t bias_level)
{
    sample += bias_level;
    if (sample >= 0x400)
        sample = 0x3FF;
    else if (sample < 0)
        sample = 0;
    return (int16_t)(((sample - bias_level) * 0x100 * 3) >> 4);
}

void hw_mix_render(const HwMixBus* mix,
                   const uint8_t* in_sq1,
                   const uint8_t* in_sq2,
                   const uint8_t* in_wave,
                   const uint8_t* in_noise,
                   const int8_t* in_dma_a,
                   const int8_t* in_dma_b,
                   int16_t* outL,
                   int16_t* outR,
                   int frames)
{
    if (frames <= 0)
        return;

    const uint8_t psg_shift = (uint8_t)(4u - (mix->psg_volume_code & 3u));
    const int32_t dma_a_scale = mix->dma_a_vol_code ? 4 : 2;
    const int32_t dma_b_scale = mix->dma_b_vol_code ? 4 : 2;

    for (int i = 0; i < frames; i++)
    {
        int32_t left_psg = 0;
        int32_t right_psg = 0;

        if (in_sq1)
        {
            if (mix->pan_mask_left & 0x01u)
                left_psg += in_sq1[i];
            if (mix->pan_mask_right & 0x01u)
                right_psg += in_sq1[i];
        }
        if (in_sq2)
        {
            if (mix->pan_mask_left & 0x02u)
                left_psg += in_sq2[i];
            if (mix->pan_mask_right & 0x02u)
                right_psg += in_sq2[i];
        }
        if (in_wave)
        {
            if (mix->pan_mask_left & 0x04u)
                left_psg += in_wave[i];
            if (mix->pan_mask_right & 0x04u)
                right_psg += in_wave[i];
        }
        if (in_noise)
        {
            if (mix->pan_mask_left & 0x08u)
                left_psg += in_noise[i];
            if (mix->pan_mask_right & 0x08u)
                right_psg += in_noise[i];
        }

        int32_t left = ((left_psg << 3) * (mix->master_vol_left + 1)) >> psg_shift;
        int32_t right = ((right_psg << 3) * (mix->master_vol_right + 1)) >> psg_shift;
        if (in_dma_a)
        {
            if (mix->dma_a_left)
                left += in_dma_a[i] * dma_a_scale;
            if (mix->dma_a_right)
                right += in_dma_a[i] * dma_a_scale;
        }
        if (in_dma_b)
        {
            if (mix->dma_b_left)
                left += in_dma_b[i] * dma_b_scale;
            if (mix->dma_b_right)
                right += in_dma_b[i] * dma_b_scale;
        }

        if (outL)
            outL[i] = gba_apply_bias(left, mix->bias_level);
        if (outR)
            outR[i] = gba_apply_bias(right, mix->bias_level);
    }
}
