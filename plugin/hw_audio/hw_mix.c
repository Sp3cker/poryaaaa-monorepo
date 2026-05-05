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

/* PSG bus volume code → linear factor (SOUNDCNT_H bits 1-0).
 *   00 = 25%, 01 = 50%, 10 = 100%, 11 = reserved (treat as 100%). */
static const float kPsgVolFactor[4] = { 0.25f, 0.50f, 1.00f, 1.00f };

void hw_mix_render(const HwMixBus *mix,
                   const float *in_sq1,
                   const float *in_sq2,
                   const float *in_wave,
                   const float *in_noise,
                   const float *in_dma_a,
                   const float *in_dma_b,
                   float *outL, float *outR, int frames) {
    if (frames <= 0) return;

    /* Per-side scalars: NR50 master vol (0..7 → 0..1) × SOUNDCNT_H PSG
     * volume code.  Computed once per render call; SOUNDCNT_L / H
     * events are applied at segment boundaries by the caller, so
     * within a segment these are constant. */
    const float master_l = (float)mix->master_vol_left  / 7.0f;
    const float master_r = (float)mix->master_vol_right / 7.0f;
    const float psg_vol  = kPsgVolFactor[mix->psg_volume_code & 3];
    const float a_vol    = (mix->dma_a_vol_code ? 1.0f : 0.5f);
    const float b_vol    = (mix->dma_b_vol_code ? 1.0f : 0.5f);

    /* NR51 pan-mask bit positions: sq1=bit0, sq2=bit1, wave=bit2, noise=bit3.
     * Pre-multiply with the side scalars so the inner loop is a sum of
     * masked products, no per-sample branches. */
    const float sq1_l   = ((mix->pan_mask_left  & 0x01) ? master_l * psg_vol : 0.0f);
    const float sq1_r   = ((mix->pan_mask_right & 0x01) ? master_r * psg_vol : 0.0f);
    const float sq2_l   = ((mix->pan_mask_left  & 0x02) ? master_l * psg_vol : 0.0f);
    const float sq2_r   = ((mix->pan_mask_right & 0x02) ? master_r * psg_vol : 0.0f);
    const float wave_l  = ((mix->pan_mask_left  & 0x04) ? master_l * psg_vol : 0.0f);
    const float wave_r  = ((mix->pan_mask_right & 0x04) ? master_r * psg_vol : 0.0f);
    const float noise_l = ((mix->pan_mask_left  & 0x08) ? master_l * psg_vol : 0.0f);
    const float noise_r = ((mix->pan_mask_right & 0x08) ? master_r * psg_vol : 0.0f);

    const float a_l = (mix->dma_a_left  ? a_vol : 0.0f);
    const float a_r = (mix->dma_a_right ? a_vol : 0.0f);
    const float b_l = (mix->dma_b_left  ? b_vol : 0.0f);
    const float b_r = (mix->dma_b_right ? b_vol : 0.0f);

    /* Per-channel headroom budget.  PSG channels (now unipolar 0..env_vol/
     * 15 per hw_psg.c) sum positively across up to 4 channels per side.
     * DMA channels are signed [-1, +1].  kPsgChanScale=0.25 caps the
     * 4-channel PSG sum at ~+1 of DAC space; kDmaChanScale=0.5 caps each
     * DMA at ±0.5 so DMA A+B sum at ±1.  Absolute level tuning vs mGBA
     * captures is a follow-on parity item — see plan §12 "PSG unipolar
     * synth rework". */
    const float kPsgChanScale = 0.25f;
    const float kDmaChanScale = 0.5f;

    /* SOUNDBIAS _applyBias model (mirrors mGBA src/gba/audio.c:343,
     * `_applyBias()`).  Real GBA DAC is unsigned 10-bit (0..0x3FF) with
     * `bias_level` (default 0x200) as the DC reference.  mGBA's logic is:
     *   sample += bias_level
     *   if (sample >= 0x400) sample = 0x3FF
     *   else if (sample < 0)  sample = 0
     *   return sample - bias_level   // signed output, no embedded DC
     *
     * In our normalized [-1, +1] DAC-bipolar space (where value V
     * corresponds to DAC integer V * 0x200):
     *   bias_norm = bias_level / 0x200       (default = 1.0)
     *   max_norm  = 0x3FF / 0x200            (≈ 1.998, post-shift max)
     *   threshold = 0x400 / 0x200            (= 2.0,   clip-to-max edge)
     *
     * Per-sample:
     *   shifted = signal + bias_norm
     *   if (shifted >= threshold) shifted = max_norm
     *   else if (shifted < 0)      shifted = 0
     *   output = shifted - bias_norm
     *
     * For default bias=0x200 this is mathematically equivalent to
     * "clip signal to [-1, +1023/512]" with no DC offset.  For non-
     * default bias the clip window remains the same width but slides
     * relative to the input — which is what mGBA does, and which the
     * earlier `bias_offset += clip` form got slightly wrong (it left
     * the bias offset embedded in the output for non-default bias). */
    const float bias_norm = (float)mix->bias_level / 512.0f;
    const float max_norm  = 1023.0f / 512.0f;
    const float threshold = 1024.0f / 512.0f;   /* = 2.0 */

    for (int i = 0; i < frames; i++) {
        float L = 0.0f, R = 0.0f;

        if (in_sq1) {
            float s = in_sq1[i] * kPsgChanScale;
            L += s * sq1_l;
            R += s * sq1_r;
        }
        if (in_sq2) {
            float s = in_sq2[i] * kPsgChanScale;
            L += s * sq2_l;
            R += s * sq2_r;
        }
        if (in_wave) {
            float s = in_wave[i] * kPsgChanScale;
            L += s * wave_l;
            R += s * wave_r;
        }
        if (in_noise) {
            float s = in_noise[i] * kPsgChanScale;
            L += s * noise_l;
            R += s * noise_r;
        }
        if (in_dma_a) {
            float s = in_dma_a[i] * kDmaChanScale;
            L += s * a_l;
            R += s * a_r;
        }
        if (in_dma_b) {
            float s = in_dma_b[i] * kDmaChanScale;
            L += s * b_l;
            R += s * b_r;
        }

        /* _applyBias: shift, clip to unsigned 10-bit DAC, subtract bias. */
        float L_dac = L + bias_norm;
        float R_dac = R + bias_norm;
        if (L_dac >= threshold) L_dac = max_norm;
        else if (L_dac < 0.0f)  L_dac = 0.0f;
        if (R_dac >= threshold) R_dac = max_norm;
        else if (R_dac < 0.0f)  R_dac = 0.0f;
        L = L_dac - bias_norm;
        R = R_dac - bias_norm;

        if (outL) outL[i] = L;
        if (outR) outR[i] = R;
    }
}
