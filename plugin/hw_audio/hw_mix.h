#ifndef HW_MIX_H
#define HW_MIX_H

#include <stdbool.h>
#include <stdint.h>

#include "m4a/m4a_driver.h"   /* M4ARegWrite */

#ifdef __cplusplus
extern "C" {
#endif

/* GBA mix bus — combines four PSG mono channels (sq1/sq2/wave/noise)
 * and two DMA mono FIFOs (A/B) into stereo, applies SOUNDCNT_L
 * master + per-channel pan, SOUNDCNT_H PSG bus volume + DMA bus
 * volumes + DMA L/R routing, then runs the SOUNDBIAS bias-add + clip
 * stage that mirrors the GBA's unsigned 10-bit DAC range.
 *
 * Stage placement vs the rest of the chip (per plan §3 / §12 step 8):
 *   PSG synth      → 4 mono per-channel buffers   (hw_psg.c)
 *   PCM two-stage  → 2 mono per-FIFO buffers      (hw_pcm.c)
 *   ↓
 *   THIS MODULE — SOUNDCNT_L/H routing + scaling, SOUNDBIAS bias+clip
 *   ↓                 (operates at chip-internal rate post-§12.9)
 *   Stereo float at chip-internal rate → polyphase resampler → host
 *
 * ⚠ Self-consistency tested but mGBA / hardware parity NOT proven —
 * see plan §12.10b.  This module computes the correct mix-bus + DAC
 * math at the chip-internal rate (`max(131072, 32768 << sampling_
 * cycle)` per plan §7b — driven by SOUNDBIAS), and the polyphase
 * resampler downstream brings it to host rate with anti-alias band-
 * limiting.  Don't make spectral / level / per-channel comparisons
 * against mGBA captures from this module's output until §12.10b
 * lands.
 *
 *
 * Register decoding reference (mGBA gba_audio.c + GBATEK):
 *
 *   SOUNDCNT_L (NR50/NR51 in real GB nomenclature; lives at 0x4000080):
 *     bits  0-2  master_vol_right   0..7 (right-side PSG master)
 *     bits  4-6  master_vol_left    0..7
 *     bits  8-11 pan_mask_right     bit i = chan (i+1) routed right
 *     bits 12-15 pan_mask_left      bit i = chan (i+1) routed left
 *
 *   SOUNDCNT_H  (0x4000082):
 *     bits  0-1  psg_volume_code    00=25%, 01=50%, 10=100%, 11=reserved
 *     bit   2    dma_a_vol_code     0=50%, 1=100%
 *     bit   3    dma_b_vol_code     0=50%, 1=100%
 *     bit   8    dma_a_enable_right
 *     bit   9    dma_a_enable_left
 *     bit  12    dma_b_enable_right
 *     bit  13    dma_b_enable_left
 *     (bit 11 = DMA A timer select, bit 15 = DMA B timer; not relevant
 *      to mix.  bits 4-7 = unused.  bit 14 reset FIFOs — TBD.)
 *
 *   SOUNDBIAS (0x4000088):
 *     bits  1-9  bias_level         0..0x3FF (default 0x200 = mid-DAC)
 *     bits 14-15 sampling_cycle     0..3 → quirk rate 32k/65k/131k/262k
 *
 * The bias-add + clip stage models the GBA's PWM DAC: the chip
 * outputs unsigned 10-bit values 0..0x3FF, with bias_level (typically
 * 0x200) as the DC reference.  Mix-bus output (in [-1, +1]) is
 * mapped into "DAC-normalized" space relative to bias_level, the
 * 10-bit range is enforced (clamping anything that would underflow
 * to 0 or overflow past 0x3FF), then mapped back to a bipolar float
 * signal centered on (bias_level - 0x200) / 0x200.  Default bias
 * 0x200 produces symmetric ±1 clipping; non-default bias offsets DC. */

typedef struct {
    /* SOUNDCNT_L (NR50 / NR51) — PSG master vol + per-PSG-channel pan. */
    uint8_t  master_vol_left;     /* 0..7 */
    uint8_t  master_vol_right;
    uint8_t  pan_mask_left;       /* bit i = channel (i+1) routed left  */
    uint8_t  pan_mask_right;

    /* SOUNDCNT_H — PSG/DMA bus volumes + DMA L/R routing. */
    uint8_t  psg_volume_code;     /* 0..2 (3 reserved → treat as 100%) */
    uint8_t  dma_a_vol_code;      /* 0 = 50%, 1 = 100% */
    uint8_t  dma_b_vol_code;
    bool     dma_a_left;
    bool     dma_a_right;
    bool     dma_b_left;
    bool     dma_b_right;

    /* SOUNDBIAS — DAC bias level + sampling cycle (quirk rate). */
    uint16_t bias_level;          /* 0..0x3FF, default 0x200 */
    uint8_t  sampling_cycle;      /* 0..3 (32k/65k/131k/262k) */
} HwMixBus;

void hw_mix_init(HwMixBus *mix);

/* Apply one M4ARegWrite event.  Subscribes to NR50/NR51, SOUNDCNT_H,
 * SOUNDBIAS.  Other events are silently ignored (PSG/PCM handle theirs). */
void hw_mix_apply_event(HwMixBus *mix, const M4ARegWrite *ev);

/* Combine the six per-channel mono buffers into stereo with full
 * SOUNDCNT_L/H routing+scaling + SOUNDBIAS bias-add + clip pipeline.
 * Each input is host-rate dipolar in approximately [-1, +1] (PSG:
 * dipolar duty/wave/noise × env_vol/15; PCM: s8 / 128.0).
 *
 * Outputs are WRITTEN (not summed) to outL/outR.  Pass NULL for any
 * input buffer to treat that channel as silent. */
void hw_mix_render(const HwMixBus *mix,
                   const float *in_sq1,
                   const float *in_sq2,
                   const float *in_wave,
                   const float *in_noise,
                   const float *in_dma_a,
                   const float *in_dma_b,
                   float *outL, float *outR, int frames);

#ifdef __cplusplus
}
#endif

#endif
