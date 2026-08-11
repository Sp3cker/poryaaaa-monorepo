#ifndef HW_PCM_H
#define HW_PCM_H

#include <stdbool.h>
#include <stdint.h>

#include "m4a/m4a_driver.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /* DirectSound's hardware-visible state.  A FIFO is eight 32-bit words;
     * the modulo pointers deliberately alias full and empty, as mGBA does.
     * `internal_sample` shifts out one little-endian byte per selected timer
     * overflow.  FIFO reset changes only the queue pointers. */
    typedef struct
    {
        uint32_t words[8];
        uint32_t internal_sample;
        uint8_t read_index;
        uint8_t write_index;
        uint8_t internal_remaining;
        int8_t held_sample;
    } HwPcmFifo;

    typedef struct
    {
        HwPcmFifo fifo_a;
        HwPcmFifo fifo_b;
        uint8_t timer_a;
        uint8_t timer_b;
        bool route_a;
        bool route_b;
        bool master_enabled;
    } HwPcm;

    /* Live m4a defaults have DirectSound enabled; trace reset overrides this
     * from SOUNDCNT_X before replaying hardware events. */
    void hw_pcm_init(HwPcm* pcm, uint32_t unused_render_rate_hz);

    /* Applies SOUNDCNT_H, SOUNDCNT_X, FIFO word writes, and TIMER_0/1
     * consumption.  TIMER events consume only FIFOs routed to L or R and
     * selecting the event timer. */
    void hw_pcm_apply_event(HwPcm* pcm, const M4ARegWrite* ev);

    void hw_pcm_fifo_write_word(HwPcmFifo* fifo, uint32_t word);
    void hw_pcm_clock_timer(HwPcm* pcm, uint8_t timer);

    /* Output is the currently held signed byte.  DAC cadence does not drain
     * the FIFO; only explicit timer events do. */
    void hw_pcm_render(const HwPcm* pcm, int8_t* out_a, int8_t* out_b, int frames);

#ifdef __cplusplus
}
#endif

#endif
