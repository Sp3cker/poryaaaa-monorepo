#include "hw_pcm.h"

#include <string.h>

enum
{
    HW_PCM_FIFO_WORDS = 8,
    HW_PCM_SOUND_A_FIFO_RESET = 0x0800,
    HW_PCM_SOUND_B_FIFO_RESET = 0x8000,
};

static void hw_pcm_fifo_reset(HwPcmFifo* fifo)
{
    fifo->read_index = 0;
    fifo->write_index = 0;
}

void hw_pcm_init(HwPcm* pcm, uint32_t unused_render_rate_hz)
{
    (void)unused_render_rate_hz;
    if (!pcm)
        return;
    memset(pcm, 0, sizeof(*pcm));
    pcm->route_a = true;
    pcm->route_b = true;
    pcm->master_enabled = true;
}

void hw_pcm_fifo_write_word(HwPcmFifo* fifo, uint32_t word)
{
    if (!fifo)
        return;
    fifo->words[fifo->write_index] = word;
    fifo->write_index = (uint8_t)((fifo->write_index + 1u) % HW_PCM_FIFO_WORDS);
}

static void hw_pcm_clock_fifo(HwPcmFifo* fifo)
{
    if (!fifo->internal_remaining && fifo->read_index != fifo->write_index)
    {
        fifo->internal_sample = fifo->words[fifo->read_index];
        fifo->read_index = (uint8_t)((fifo->read_index + 1u) % HW_PCM_FIFO_WORDS);
        fifo->internal_remaining = 4;
    }

    /* mGBA exposes the low byte, then shifts the little-endian word.  Once
     * four bytes drain, an empty timer leaves the zeroed shift register
     * audible rather than retaining a GBATEK-style last-byte hold. */
    fifo->held_sample = (int8_t)fifo->internal_sample;
    if (fifo->internal_remaining)
    {
        fifo->internal_sample >>= 8u;
        fifo->internal_remaining--;
    }
}

void hw_pcm_clock_timer(HwPcm* pcm, uint8_t timer)
{
    if (!pcm || timer > 1u || !pcm->master_enabled)
        return;
    if (pcm->route_a && pcm->timer_a == timer)
        hw_pcm_clock_fifo(&pcm->fifo_a);
    if (pcm->route_b && pcm->timer_b == timer)
        hw_pcm_clock_fifo(&pcm->fifo_b);
}

void hw_pcm_apply_event(HwPcm* pcm, const M4ARegWrite* ev)
{
    if (!pcm || !ev)
        return;

    switch (ev->reg)
    {
    case M4A_REG_FIFO_A:
        hw_pcm_fifo_write_word(&pcm->fifo_a, ev->value);
        break;
    case M4A_REG_FIFO_B:
        hw_pcm_fifo_write_word(&pcm->fifo_b, ev->value);
        break;
    case M4A_REG_TIMER_0:
        hw_pcm_clock_timer(pcm, 0);
        break;
    case M4A_REG_TIMER_1:
        hw_pcm_clock_timer(pcm, 1);
        break;
    case M4A_REG_SOUNDCNT_H:
        pcm->timer_a = (uint8_t)((ev->value >> 10u) & 1u);
        pcm->timer_b = (uint8_t)((ev->value >> 14u) & 1u);
        pcm->route_a = (ev->value & 0x0300u) != 0;
        pcm->route_b = (ev->value & 0x3000u) != 0;
        if (ev->value & HW_PCM_SOUND_A_FIFO_RESET)
            hw_pcm_fifo_reset(&pcm->fifo_a);
        if (ev->value & HW_PCM_SOUND_B_FIFO_RESET)
            hw_pcm_fifo_reset(&pcm->fifo_b);
        break;
    case M4A_REG_NR52:
        pcm->master_enabled = (ev->value & 0x80u) != 0;
        break;
    default:
        break;
    }
}

void hw_pcm_render(const HwPcm* pcm, int8_t* out_a, int8_t* out_b, int frames)
{
    if (frames <= 0)
        return;
    const int8_t sample_a = pcm ? pcm->fifo_a.held_sample : 0;
    const int8_t sample_b = pcm ? pcm->fifo_b.held_sample : 0;
    if (out_a)
        for (int i = 0; i < frames; i++)
            out_a[i] = sample_a;
    if (out_b)
        for (int i = 0; i < frames; i++)
            out_b[i] = sample_b;
}
