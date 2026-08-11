#include "hw_audio_trace.h"

#include <stdbool.h>
#include <string.h>

#define TRACE_ORDER_EXTENDED 0x80000000u
#define TRACE_ORDER_DELAY_MASK 0xFFFFu

enum
{
    TRACE_FIFO_WORD_CAPACITY = 8,
    TRACE_SAMPLE_BLOCK_CYCLES = 1024,
    TRACE_MAX_BLOCK_SAMPLES = 16,
};

typedef struct
{
    uint32_t words[TRACE_FIFO_WORD_CAPACITY];
    uint32_t internal_sample;
    int8_t samples[TRACE_MAX_BLOCK_SAMPLES];
    uint8_t read_index;
    uint8_t write_index;
    uint8_t internal_remaining;
} ScheduledFifo;

typedef struct
{
    size_t output_index;
    uint8_t slot;
} PendingSample;

typedef struct
{
    ScheduledFifo fifo_a;
    ScheduledFifo fifo_b;
    HwAudioTraceFifoSample* output;
    size_t output_capacity;
    size_t output_count;
    PendingSample pending[TRACE_MAX_BLOCK_SAMPLES];
    size_t pending_count;
    uint64_t prior_sample_cycle;
    uint64_t block_start;
    uint64_t block_end;
    uint64_t prior_cycle;
    uint32_t prior_order;
    uint8_t resolution;
    uint8_t timer_a;
    uint8_t timer_b;
    bool enabled;
    bool route_a;
    bool route_b;
    bool position_valid;
    bool sample_position_valid;
} FifoSchedule;

/* Copy finalized native slots to their corresponding explicit SAMPLE events. */
static bool emit_pending_samples(FifoSchedule* schedule)
{
    for (size_t index = 0; index < schedule->pending_count; index++)
    {
        PendingSample pending = schedule->pending[index];
        if (pending.output_index >= schedule->output_capacity)
            return false;
        schedule->output[pending.output_index] = (HwAudioTraceFifoSample){
            .fifo_a = schedule->fifo_a.samples[pending.slot],
            .fifo_b = schedule->fifo_b.samples[pending.slot],
        };
    }
    schedule->pending_count = 0;
    return true;
}

/* mGBA carries the final sample slot across every slot of the next block. */
static bool finish_sample_block(FifoSchedule* schedule)
{
    if (!emit_pending_samples(schedule))
        return false;
    unsigned sample_count = 2u << schedule->resolution;
    memset(schedule->fifo_a.samples, schedule->fifo_a.samples[sample_count - 1u], sizeof(schedule->fifo_a.samples));
    memset(schedule->fifo_b.samples, schedule->fifo_b.samples[sample_count - 1u], sizeof(schedule->fifo_b.samples));
    return true;
}

/* Advance 1024-cycle mGBA audio blocks in the current observed source phase. */
static bool advance_sample_blocks(FifoSchedule* schedule, uint64_t cycle)
{
    while (cycle >= schedule->block_end)
    {
        if (!finish_sample_block(schedule))
            return false;
        schedule->block_start = schedule->block_end;
        schedule->block_end += TRACE_SAMPLE_BLOCK_CYCLES;
    }
    return true;
}
/* Explicit samples are the only global-cycle view of mGBA's mutable
 * lastSample/sampleIndex state. Rebase an empty prospective block when that
 * state reports a new phase instead of requiring a zero-aligned grid. */
static bool rebase_sample_block(FifoSchedule* schedule, uint64_t cycle)
{
    if (schedule->pending_count)
        return false;
    schedule->block_start = cycle;
    schedule->block_end = cycle + TRACE_SAMPLE_BLOCK_CYCLES;
    return true;
}

/* mGBA advances this modulo-8 write pointer even when it meets the read pointer. */
static bool push_fifo_word(ScheduledFifo* fifo, uint32_t value)
{
    fifo->words[fifo->write_index] = value;
    fifo->write_index = (uint8_t)((fifo->write_index + 1u) % TRACE_FIFO_WORD_CAPACITY);
    return true;
}

/* Fill the mGBA native-sample suffix affected by one selected timer clock. */
static void clock_fifo(FifoSchedule* schedule, ScheduledFifo* fifo, const HwAudioTraceEvent* event)
{
    if (!fifo->internal_remaining && fifo->read_index != fifo->write_index)
    {
        fifo->internal_sample = fifo->words[fifo->read_index];
        fifo->read_index = (uint8_t)((fifo->read_index + 1u) % TRACE_FIFO_WORD_CAPACITY);
        fifo->internal_remaining = 4;
    }

    unsigned sample_count = 2u << schedule->resolution;
    unsigned interval_shift = 9u - schedule->resolution;
    uint32_t cycles_late = (event->order & TRACE_ORDER_EXTENDED) ? event->order & TRACE_ORDER_DELAY_MASK : 0u;
    int64_t sample_event_until = (int64_t)schedule->block_end - (int64_t)event->cycle - cycles_late;
    int64_t remaining_slots = sample_event_until - 1 + (1u << interval_shift);
    remaining_slots >>= interval_shift;
    if (remaining_slots > (int64_t)sample_count)
        remaining_slots = sample_count;
    if (remaining_slots > 0)
    {
        unsigned first_slot = sample_count - (unsigned)remaining_slots;
        memset(&fifo->samples[first_slot], (int8_t)fifo->internal_sample, sample_count - first_slot);
    }
    if (fifo->internal_remaining)
    {
        fifo->internal_sample >>= 8u;
        fifo->internal_remaining--;
    }
}

/* Apply writes that determine FIFO scheduling without duplicating the audio mixer. */
static bool apply_schedule_write(FifoSchedule* schedule, const HwAudioTraceEvent* event)
{
    bool sample_barrier =
        (event->address >= HW_AUDIO_GBA_IO_BASE + 0x60 && event->address <= HW_AUDIO_GBA_IO_BASE + 0x80) ||
        event->address == HW_AUDIO_GBA_IO_BASE + 0x84 || event->address == HW_AUDIO_GBA_IO_BASE + 0x88;
    if (sample_barrier && !emit_pending_samples(schedule))
        return false;

    switch (event->address)
    {
    case HW_AUDIO_GBA_IO_BASE + 0x82:
        schedule->timer_a = (uint8_t)((event->value >> 10u) & 1u);
        schedule->timer_b = (uint8_t)((event->value >> 14u) & 1u);
        schedule->route_a = (event->value & 0x0300u) != 0;
        schedule->route_b = (event->value & 0x3000u) != 0;
        if (event->value & 0x0800u)
        {
            schedule->fifo_a.read_index = 0;
            schedule->fifo_a.write_index = 0;
        }
        if (event->value & 0x8000u)
        {
            schedule->fifo_b.read_index = 0;
            schedule->fifo_b.write_index = 0;
        }
        return true;
    case HW_AUDIO_GBA_IO_BASE + 0x84:
        schedule->enabled = (event->value & 0x80u) != 0;
        return true;
    case HW_AUDIO_GBA_IO_BASE + 0x88:
        schedule->resolution = (uint8_t)((event->value >> 14u) & 3u);
        return true;
    case HW_AUDIO_GBA_IO_BASE + 0xA0:
        return event->width == 4 && push_fifo_word(&schedule->fifo_a, event->value);
    case HW_AUDIO_GBA_IO_BASE + 0xA4:
        return event->width == 4 && push_fifo_word(&schedule->fifo_b, event->value);
    default:
        return true;
    }
}

/* Record the explicit native slot; pinned mGBA can rephase a source block. */
static bool schedule_sample(FifoSchedule* schedule, const HwAudioTraceEvent* event)
{
    if (schedule->pending_count == TRACE_MAX_BLOCK_SAMPLES || schedule->output_count >= schedule->output_capacity)
        return false;
    unsigned sample_count = 2u << schedule->resolution;
    uint64_t sample_interval = TRACE_SAMPLE_BLOCK_CYCLES / sample_count;
    uint64_t offset = event->cycle - schedule->block_start;
    if (offset % sample_interval != 0 || offset / sample_interval >= sample_count)
    {
        if (!rebase_sample_block(schedule, event->cycle))
            return false;
        offset = 0;
    }
    uint8_t slot = (uint8_t)(offset / sample_interval);
    if (schedule->sample_position_valid && event->cycle == schedule->prior_sample_cycle)
        return false;
    schedule->prior_sample_cycle = event->cycle;
    schedule->sample_position_valid = true;
    schedule->pending[schedule->pending_count++] = (PendingSample){
        .output_index = schedule->output_count++,
        .slot = slot,
    };
    return true;
}

/* Resolve future-in-interval FIFO timers before the replay emits native frames. */
HwAudioTraceStatus hw_audio_trace_schedule_fifo_samples(const HwAudioTraceEvent* events,
                                                        size_t event_count,
                                                        HwAudioTraceFifoSample* samples,
                                                        size_t sample_capacity,
                                                        size_t* sample_count)
{
    if ((!events && event_count) || !samples || !sample_count)
        return HW_AUDIO_TRACE_INVALID_ARGUMENT;
    FifoSchedule schedule = {
        .output = samples,
        .output_capacity = sample_capacity,
        .block_end = TRACE_SAMPLE_BLOCK_CYCLES,
    };
    for (size_t index = 0; index < event_count; index++)
    {
        const HwAudioTraceEvent* event = &events[index];
        if (event->cycle > INT32_MAX)
            return HW_AUDIO_TRACE_INVALID_ARGUMENT;
        if (schedule.position_valid && (event->cycle < schedule.prior_cycle ||
                                        (event->cycle == schedule.prior_cycle && event->order <= schedule.prior_order)))
            return HW_AUDIO_TRACE_OUT_OF_ORDER;
        if (!advance_sample_blocks(&schedule, event->cycle))
            return HW_AUDIO_TRACE_INVALID_ARGUMENT;
        bool ok = true;
        if (event->kind == HW_AUDIO_TRACE_WRITE)
            ok = apply_schedule_write(&schedule, event);
        else if (event->kind == HW_AUDIO_TRACE_TIMER)
        {
            if (schedule.enabled && schedule.route_a && schedule.timer_a == event->value)
                clock_fifo(&schedule, &schedule.fifo_a, event);
            if (schedule.enabled && schedule.route_b && schedule.timer_b == event->value)
                clock_fifo(&schedule, &schedule.fifo_b, event);
        }
        else if (event->kind == HW_AUDIO_TRACE_SAMPLE)
            ok = schedule_sample(&schedule, event);
        else
            ok = false;
        if (!ok)
            return event->kind == HW_AUDIO_TRACE_WRITE && (event->address == HW_AUDIO_GBA_IO_BASE + 0xA0 ||
                                                           event->address == HW_AUDIO_GBA_IO_BASE + 0xA4)
                       ? HW_AUDIO_TRACE_UNSUPPORTED_WIDTH
                       : HW_AUDIO_TRACE_INVALID_ARGUMENT;
        schedule.prior_cycle = event->cycle;
        schedule.prior_order = event->order;
        schedule.position_valid = true;
    }
    if (!emit_pending_samples(&schedule))
        return HW_AUDIO_TRACE_INVALID_ARGUMENT;
    *sample_count = schedule.output_count;
    return HW_AUDIO_TRACE_OK;
}
