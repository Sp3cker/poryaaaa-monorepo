#ifndef M4A_PCM_RING_H
#define M4A_PCM_RING_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/* Ring buffer carrying m4a's software-mixed (post-reverb, post-clamp) PCM.
 *
 * The default constants remain the canonical Pokemon Emerald geometry.  A
 * driver may select another bounded geometry at runtime; pcm_rate_hz and
 * pcm_dma_buf_size describe that geometry.  pcm_samples_per_vblank records
 * the exact count in the latest published block (zero before the first
 * publish). */
#define M4A_PCM_DMA_BUF_SIZE 1584
#define M4A_PCM_SAMPLES_PER_VBLANK 224
#define M4A_PCM_RATE_HZ 13379

/* Static capacity for a host-following mix rate through 192 kHz.  The DMA
 * buffer keeps the default's duration when the active PCM rate changes. */
#define M4A_PCM_MAX_RATE_HZ 192000
#define M4A_PCM_MAX_SAMPLES_PER_VBLANK 3215
#define M4A_PCM_MAX_DMA_BUF_SIZE \
    ((M4A_PCM_MAX_RATE_HZ * M4A_PCM_DMA_BUF_SIZE + M4A_PCM_RATE_HZ - 1) / M4A_PCM_RATE_HZ)

    typedef struct
    {
        int8_t ring_a[M4A_PCM_MAX_DMA_BUF_SIZE];
        int8_t ring_b[M4A_PCM_MAX_DMA_BUF_SIZE];
        uint64_t write_cursor;
        uint32_t pcm_rate_hz;
        uint32_t pcm_samples_per_vblank; /* exact latest PUBLISH count */
        uint32_t pcm_dma_buf_size;
    } M4APcmRing;

#ifdef __cplusplus
}
#endif

#endif
