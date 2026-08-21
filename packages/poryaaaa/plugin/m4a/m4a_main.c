#include "m4a_internal.h"
#include "m4a_pcm_internal.h"

/* Forward decls from m4a_cgb.c / m4a_pcm.c */
extern void m4a_cgb_sound(M4ADriver* drv);
extern void m4a_sound_main_ram(M4ADriver* drv);

/* SoundMain — pokeemerald m4a.c equivalent.  Fires once per vblank.
 * Tick c15 counter, drive tempo accumulator (which fires LFO ticks),
 * then run the CGB envelope/length tick + the PCM software mixer.
 *
 * MPlayMain's song-walk lives in the DAW (poryaaaa is a player, not a
 * sequencer), so we do not advance MIDI events here.  Everything else
 * the real m4a driver does inside MPlayMain — LFO advancement, the
 * per-track derived state — happens via the LFO tick + the
 * vol/pit/pan CC handlers in m4a_track.c. */
static void m4a_sound_main(M4ADriver* drv)
{
    if (drv->c15 > 0)
        drv->c15--;
    else
        drv->c15 = 14;

    /* Tempo accumulator: tempoC += tempoI per vblank; one LFO tick
     * fires per 150 accumulated units, matching the ROM driver's
     * ticks-per-vblank rate. */
    drv->tempoC += drv->tempoI;
    while (drv->tempoC >= 150)
    {
        drv->tempoC -= 150;
        m4a_internal_lfo_tick(drv);
    }

    m4a_internal_effects_tick(drv);

    m4a_cgb_sound(drv);
    m4a_sound_main_ram(drv);
}

enum
{
    M4A_FIFO_WORDS = 8,
    M4A_FIFO_REFILL_WORDS = 4,
    M4A_FIFO_REFILL_BYTES = M4A_FIFO_REFILL_WORDS * 4,
};

static uint8_t fifo_word_count(uint8_t read_index, uint8_t write_index)
{
    return (uint8_t)((write_index + M4A_FIFO_WORDS - read_index) % M4A_FIFO_WORDS);
}

static uint32_t pack_fifo_word(const int8_t* ring, uint64_t cursor, uint32_t size)
{
    uint32_t word = 0;
    for (uint32_t byte = 0; byte < 4; byte++)
        word |= (uint32_t)(uint8_t)ring[(cursor + byte) % size] << (byte * 8u);
    return word;
}

/* Emit one four-word DMA transfer and advance the modulo-eight FIFO model. */
static void
write_fifo_burst(M4ADriver* drv, M4ARegId fifo_reg, const int8_t* ring, uint64_t* source_cursor, uint8_t* write_index)
{
    const uint32_t size = drv->pcm_dma_buf_size;
    if (size == 0u)
        return;

    for (uint32_t word = 0; word < M4A_FIFO_REFILL_WORDS; word++)
    {
        m4a_internal_emit_event(drv, fifo_reg, pack_fifo_word(ring, *source_cursor + word * 4u, size));
        *write_index = (uint8_t)((*write_index + 1u) % M4A_FIFO_WORDS);
    }
    *source_cursor += M4A_FIFO_REFILL_BYTES;
}

/* Start special-timing DMA only while fewer than four words remain queued. */
static void refill_fifo(M4ADriver* drv,
                        M4ARegId fifo_reg,
                        const int8_t* ring,
                        uint64_t* source_cursor,
                        uint8_t read_index,
                        uint8_t* write_index)
{
    if (fifo_word_count(read_index, *write_index) >= M4A_FIFO_REFILL_WORDS)
        return;
    write_fifo_burst(drv, fifo_reg, ring, source_cursor, write_index);
}

static bool consume_fifo(uint8_t* read_index, uint8_t* internal_remaining, bool queued_word_available)
{
    if (!*internal_remaining && queued_word_available)
    {
        *read_index = (uint8_t)((*read_index + 1u) % M4A_FIFO_WORDS);
        *internal_remaining = 4;
    }
    if (!*internal_remaining)
        return false;
    (*internal_remaining)--;
    return true;
}

/* A DirectSound timer overflow requests four DMA words when the modulo-8
 * FIFO has fewer than four queued words.  A DMA request caused by an empty
 * FIFO completes after that timer callback, so its first byte is consumed by
 * the next callback. */
static void m4a_pcm_timer_overflow(M4ADriver* drv)
{
    const bool fifo_a_had_word = fifo_word_count(drv->pcm_fifo_a_read, drv->pcm_fifo_a_write) != 0u;
    const bool fifo_b_had_word = fifo_word_count(drv->pcm_fifo_b_read, drv->pcm_fifo_b_write) != 0u;
    refill_fifo(drv,
                M4A_REG_FIFO_A,
                drv->pcm.ring_a,
                &drv->pcm_fifo_a_source_cursor,
                drv->pcm_fifo_a_read,
                &drv->pcm_fifo_a_write);
    refill_fifo(drv,
                M4A_REG_FIFO_B,
                drv->pcm.ring_b,
                &drv->pcm_fifo_b_source_cursor,
                drv->pcm_fifo_b_read,
                &drv->pcm_fifo_b_write);
    const bool consumed_a = consume_fifo(&drv->pcm_fifo_a_read, &drv->pcm_fifo_a_internal_remaining, fifo_a_had_word);
    const bool consumed_b = consume_fifo(&drv->pcm_fifo_b_read, &drv->pcm_fifo_b_internal_remaining, fifo_b_had_word);
    if (consumed_a || consumed_b)
        m4a_internal_emit_event(drv, M4A_REG_TIMER_0, 0);
}
/* Advance MP2K's seven-frame DMA epoch at the line-150 VCount callback.
 * The disable/re-enable sequence reloads each special-timing DMA source; its
 * preceding enabled-to-enabled START_NOW write is ignored by mGBA. */
static void m4a_sound_vsync(M4ADriver* drv)
{
    if (drv->pcm_dma_counter > 1u)
    {
        drv->pcm_dma_counter--;
        return;
    }

    drv->pcm_dma_counter = drv->pcm_dma_period;
    drv->pcm_fifo_a_source_cursor = 0u;
    drv->pcm_fifo_b_source_cursor = 0u;
}

static bool advance_pcm_timer(M4ADriver* drv)
{
    const uint32_t samples_per_vblank =
        drv->pcm_max_samples_per_vblank ? drv->pcm_max_samples_per_vblank : M4A_PCM_SAMPLES_PER_VBLANK;
    const uint32_t timer_period = M4A_VBLANK_CYCLES / samples_per_vblank;
    if (drv->next_pcm_timer_cycle > UINT64_MAX - timer_period)
        return false;
    drv->next_pcm_timer_cycle += timer_period;
    return true;
}

/* Advance host frames through the canonical GBA-cycle clock.  VBlank mixer
 * updates and DirectSound timer overflows share one ordered timeline. */
void m4a_advance(M4ADriver* drv, int host_frames)
{
    if (!drv || host_frames <= 0 || drv->host_rate_hz == 0)
        return;

    uint64_t scaled_cycles = (uint64_t)(uint32_t)host_frames * M4A_GBA_CYCLES_PER_SECOND + drv->host_cycle_remainder;
    uint64_t elapsed_cycles = scaled_cycles / drv->host_rate_hz;
    drv->host_cycle_remainder = scaled_cycles % drv->host_rate_hz;
    if (elapsed_cycles > UINT64_MAX - drv->current_cycle)
        return;

    const uint64_t end_cycle = drv->current_cycle + elapsed_cycles;
    while (drv->next_vblank_cycle <= end_cycle || drv->next_vcount_cycle <= end_cycle ||
           drv->next_pcm_timer_cycle <= end_cycle)
    {
        uint64_t next_cycle =
            drv->next_vcount_cycle < drv->next_vblank_cycle ? drv->next_vcount_cycle : drv->next_vblank_cycle;
        if (drv->next_pcm_timer_cycle < next_cycle)
            next_cycle = drv->next_pcm_timer_cycle;
        drv->current_cycle = next_cycle;
        drv->event_cycle = next_cycle;
        drv->event_next_order = 0;

        if (drv->next_vcount_cycle == next_cycle)
        {
            m4a_sound_vsync(drv);
            if (drv->next_vcount_cycle > UINT64_MAX - M4A_VBLANK_CYCLES)
                return;
            drv->next_vcount_cycle += M4A_VBLANK_CYCLES;
        }
        if (drv->next_vblank_cycle == next_cycle)
        {
            m4a_sound_main(drv);
            if (drv->next_vblank_cycle > UINT64_MAX - M4A_VBLANK_CYCLES)
                return;
            drv->next_vblank_cycle += M4A_VBLANK_CYCLES;
        }
        if (drv->next_pcm_timer_cycle == next_cycle)
        {
            m4a_pcm_timer_overflow(drv);
            if (!advance_pcm_timer(drv))
                return;
        }
    }
    drv->current_cycle = end_cycle;
}
