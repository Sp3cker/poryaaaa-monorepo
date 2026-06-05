#ifndef M4A_PCM_RING_H
#define M4A_PCM_RING_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Ring buffer carrying m4a's software-mixed (post-reverb, post-clamp) PCM.
 *
 * Driver writes M4A_PCM_SAMPLES_PER_VBLANK bytes per vblank into
 * ring_a/ring_b at write_cursor % M4A_PCM_DMA_BUF_SIZE.  Chip's DMA
 * model reads at pcm_rate_hz.  See HW_AUDIO_SCAFFOLD_PLAN.md §6b.
 *
 * Constants match the vanilla Pokemon Emerald sound mode (index 4 in
 * gPcmSamplesPerVBlankTable: 224 samples per vblank → ≈13379 Hz). */
#define M4A_PCM_DMA_BUF_SIZE        1584
#define M4A_PCM_SAMPLES_PER_VBLANK  224
#define M4A_PCM_RATE_HZ             13379

typedef struct {
    int8_t   ring_a[M4A_PCM_DMA_BUF_SIZE];
    int8_t   ring_b[M4A_PCM_DMA_BUF_SIZE];
    uint64_t write_cursor;
    uint32_t pcm_rate_hz;
} M4APcmRing;

#ifdef __cplusplus
}
#endif

#endif
