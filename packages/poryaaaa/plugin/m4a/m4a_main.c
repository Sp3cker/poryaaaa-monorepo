#include "m4a_internal.h"
#include "m4a_pcm_internal.h"
#include "m4a_cpu_cycles.h"
#include "../m4a_tables.h"

/* Forward decls from m4a_cgb.c / m4a_pcm.c */
extern void m4a_cgb_sound(M4ADriver* drv);
extern void m4a_sound_main_ram(M4ADriver* drv);

/* Predict the next paired FIFO refill from the actual timer and queue state. */
static int64_t cpu_first_fifo_refill(const M4ADriver* drv, int64_t* period)
{
    const uint32_t samples = drv->pcm_max_samples_per_vblank ? drv->pcm_max_samples_per_vblank
                                                              : M4A_PCM_SAMPLES_PER_VBLANK;
    const int64_t timer = M4A_VBLANK_CYCLES / samples;
    unsigned queued = (drv->pcm_fifo_a_write + 8u - drv->pcm_fifo_a_read) % 8u;
    unsigned remaining = drv->pcm_fifo_a_internal_remaining;
    int64_t next = (int64_t)drv->next_pcm_timer_cycle;
    *period = 16 * timer;
    while (queued >= 4u)
    {
        if (remaining == 0)
        {
            --queued;
            remaining = 4;
        }
        --remaining;
        next += timer;
    }
    return next;
}

/* Count FIFO stalls over a CPU-work span, allowing stalls to extend the span. */
static int64_t cpu_fifo_stall(int64_t first, int64_t period, int64_t from, int64_t work)
{
    if (period <= 0 || work <= 0)
        return 0;
    if (first <= from)
        first += ((from - first) / period + 1) * period;
    int64_t stall = 0;
    for (int64_t t = first; t <= from + work + stall; t += period)
        stall += M4A_CYC_FIFO_DMA_STALL;
    return stall;
}

/* DMA3 overlaps reduce FIFO stalls on the HQ fast path. */
static int64_t cpu_mixer_stall(const M4ADriver* drv, int64_t first, int64_t period, int64_t ram, int64_t work)
{
    if (period <= 0 || work <= M4A_MIX_EXIT_BX)
        return 0;
    if (first <= ram)
        first += ((ram - first) / period + 1) * period;
    int64_t stall = 0;
    int copy = 0;
    const int copies = drv->active_pcm_mode == M4A_PCM_MIXER_IPATIX ? drv->cpu_hq_dma_count : 0;
    for (int64_t t = first; t <= ram + work + stall - M4A_MIX_EXIT_BX; t += period)
    {
        int64_t charge = M4A_CYC_FIFO_DMA_STALL;
        for (; copy < copies; ++copy)
        {
            int64_t p = t - (ram + drv->cpu_hq_dma_start[copy] + stall);
            int64_t length = drv->cpu_hq_dma_len[copy];
            if (p < -3)
                break;
            if (p >= length)
                continue;
            if (p < 0)
                charge = M4A_HQMIX_FIFO_PRE;
            else if (p <= 7)
                charge = M4A_HQMIX_FIFO_FIRST + p;
            else if (p < length - 1)
                charge = M4A_HQMIX_FIFO_WORD + (p - 8) % 5;
            else
                charge = M4A_HQMIX_FIFO_LAST;
            break;
        }
        stall += charge;
    }
    return stall;
}

/* A preview shares physical channels with the song, so its frames are invalid. */
static bool cpu_frame_has_unmodelled_channels(const M4ADriver* drv)
{
    for (int i = 0; i < M4A_MAX_CGB_CHANNELS; ++i)
        if ((drv->cgb[i].status & M4A_CHN_ON) && drv->cgb[i].cpu_audition)
            return true;
    for (int i = 0; i < M4A_MAX_PCM_CHANNELS; ++i)
    {
        const M4ADriverPcmChan* ch = &drv->pcmChans[i];
        if (!(ch->status & M4A_CHN_ON))
            continue;
        if (ch->cpu_audition ||
            (drv->active_pcm_mode == M4A_PCM_MIXER_SAPPY && !(ch->type & VOICE_TYPE_FIX) &&
             !ch->cpu_raw_frequency_valid))
            return true;
    }
    return false;
}

/* The cost model requires the ROM sound-mode rate, block and DMA geometry. */
static bool cpu_pcm_geometry_supported(const M4ADriver* drv)
{
    for (int mode = 0; mode < 12; ++mode)
    {
        const uint32_t samples = gPcmSamplesPerVBlankTable[mode];
        const uint32_t rate = (597275u * samples + 5000u) / 10000u;
        if (drv->pcm_rate_hz != rate)
            continue;
        const uint32_t ring_size = drv->active_pcm_mode == M4A_PCM_MIXER_SAPPY || mode == 3
                                       ? M4A_PCM_DMA_BUF_SIZE
                                       : 7u * samples;
        return drv->pcm_max_samples_per_vblank == samples && drv->pcm.pcm_samples_per_vblank == samples &&
               drv->pcm_dma_buf_size == ring_size;
    }
    return false;
}

/* Publish one immutable completed SoundMain entry to the render-owned ring. */
static void cpu_publish_frame(M4ADriver* drv, int counter)
{
    int64_t period;
    const int64_t first = cpu_first_fifo_refill(drv, &period);
    const int64_t sm = (int64_t)drv->current_cycle + M4A_CYC_M4A_SOUNDMAIN;
    const int64_t player = (int64_t)drv->current_cycle + drv->cpu_seq.mpEntry;
    int64_t seq = m4a_est_publish(drv);
    seq += cpu_fifo_stall(first, period, player, sm + seq - player);
    const int64_t cgb_start = sm + seq;
    int64_t cgb = drv->cpu_cgb_cycles + M4A_CYC_CGB_TAIL +
                  (counter <= 1 ? M4A_CYC_SM_TO_RAM_FIRST : M4A_CYC_SM_TO_RAM);
    cgb += cpu_fifo_stall(first, period, cgb_start, cgb);
    const int64_t ram = cgb_start + cgb;
    int64_t mixer = drv->cpu_mix_cycles;
    mixer += cpu_mixer_stall(drv, first, period, ram, mixer);
    M4AFrameCost frame = {
        .sequencer = (int32_t)seq,
        .cgb = (int32_t)cgb,
        .mixer = (int32_t)mixer,
        .total = (int32_t)(seq + cgb + mixer),
        .valid = cpu_pcm_geometry_supported(drv) && !drv->cpu_frame_contaminated &&
                 !cpu_frame_has_unmodelled_channels(drv),
        .sequencerEstimated = true,
        .cycle = drv->current_cycle,
    };
    drv->cpu_cost_history[drv->cpu_cost_seq % M4A_CPU_COST_HISTORY] = frame;
    ++drv->cpu_cost_seq;
    drv->cpu_frame_contaminated = false;
}

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
    if (drv->cpu_cost_enabled)
    {
        drv->cpu_mix_cycles = 0;
        drv->cpu_cgb_cycles = 0;
        drv->cpu_hq_dma_count = 0;
        drv->cpu_frame_contaminated |= cpu_frame_has_unmodelled_channels(drv);
    }
    int ticks = 0;
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
        ++ticks;
        m4a_internal_lfo_tick(drv);
    }

    if (drv->cpu_cost_enabled)
        m4a_est_frame(drv, ticks);
    m4a_internal_effects_tick(drv);

    const int counter = drv->pcm_dma_counter;
    m4a_cgb_sound(drv);
    m4a_sound_main_ram(drv);
    if (drv->cpu_cost_enabled)
        cpu_publish_frame(drv, counter);
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
