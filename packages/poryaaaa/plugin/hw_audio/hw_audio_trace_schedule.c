#include "hw_audio_trace.h"
#include "hw_pcm.h"

#include <stdbool.h>
#include <string.h>

enum
{
    TRACE_SAMPLE_BLOCK_CYCLES = 1024,
    TRACE_MAX_BLOCK_SAMPLES = 16,
};

typedef struct
{
    int8_t samples[TRACE_MAX_BLOCK_SAMPLES];
} ScheduledFifoSamples;

typedef struct
{
    size_t output_index;
    uint8_t slot;
} PendingSample;

typedef struct
{
    HwPcm pcm;
    ScheduledFifoSamples fifo_a;
    ScheduledFifoSamples fifo_b;
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

/* Fill the native-sample suffix affected by a shared PCM timer clock. */
static void schedule_fifo_clock(FifoSchedule* schedule,
                                ScheduledFifoSamples* samples,
                                const HwPcmFifo* fifo,
                                const HwAudioTraceEvent* event)
{
    unsigned sample_count = 2u << schedule->resolution;
    unsigned interval_shift = 9u - schedule->resolution;
    uint32_t cycles_late =
        (event->order & PORYAAAA_TRACE_ORDER_EXTENDED) ? event->order & PORYAAAA_TRACE_ORDER_DELAY_MASK : 0u;
    int64_t sample_event_until = (int64_t)schedule->block_end - (int64_t)event->cycle - cycles_late;
    int64_t remaining_slots = sample_event_until - 1 + (1u << interval_shift);
    remaining_slots >>= interval_shift;
    if (remaining_slots > (int64_t)sample_count)
        remaining_slots = sample_count;
    if (remaining_slots > 0)
    {
        unsigned first_slot = sample_count - (unsigned)remaining_slots;
        memset(&samples->samples[first_slot], fifo->held_sample, sample_count - first_slot);
    }
}

/* Identify the driver-written PSG ranges whose same-cycle transaction must
 * complete before an overdue candidate SAMPLE is observed. */
bool hw_audio_trace_event_is_cgb_batch_write(const HwAudioTraceEvent* event)
{
    if (!event || event->kind != HW_AUDIO_TRACE_WRITE)
        return false;
    const uint32_t offset = event->address - PORYAAAA_GBA_IO_BASE;
    return (offset >= 0x60u && offset <= 0x65u) || (offset >= 0x68u && offset <= 0x6Du) ||
           (offset >= 0x70u && offset <= 0x75u) || (offset >= 0x78u && offset <= 0x7Du) ||
           (offset >= 0x80u && offset <= 0x81u) || (offset >= 0x90u && offset <= 0x9Fu);
}

/* Apply only PCM-relevant writes through the shared DirectSound state. */
static bool apply_schedule_write(FifoSchedule* schedule, const HwAudioTraceEvent* event)
{
    M4ARegWrite pcm_event = {.value = event->value};
    switch (event->address)
    {
    case PORYAAAA_GBA_IO_BASE + 0x82:
        pcm_event.reg = M4A_REG_SOUNDCNT_H;
        hw_pcm_apply_event(&schedule->pcm, &pcm_event);
        return true;
    case PORYAAAA_GBA_IO_BASE + 0x84:
        pcm_event.reg = M4A_REG_NR52;
        hw_pcm_apply_event(&schedule->pcm, &pcm_event);
        return true;
    case PORYAAAA_GBA_IO_BASE + 0x88:
        schedule->resolution = (uint8_t)((event->value >> 14u) & 3u);
        return true;
    case PORYAAAA_GBA_IO_BASE + 0xA0:
        if (event->width != 4)
            return false;
        pcm_event.reg = M4A_REG_FIFO_A;
        hw_pcm_apply_event(&schedule->pcm, &pcm_event);
        return true;
    case PORYAAAA_GBA_IO_BASE + 0xA4:
        if (event->width != 4)
            return false;
        pcm_event.reg = M4A_REG_FIFO_B;
        hw_pcm_apply_event(&schedule->pcm, &pcm_event);
        return true;
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
    bool candidate_cgb_batch = false;
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
            if (event->value <= 1u)
            {
                const bool clock_a =
                    schedule.pcm.master_enabled && schedule.pcm.route_a && schedule.pcm.timer_a == event->value;
                const bool clock_b =
                    schedule.pcm.master_enabled && schedule.pcm.route_b && schedule.pcm.timer_b == event->value;
                hw_pcm_clock_timer(&schedule.pcm, (uint8_t)event->value);
                if (clock_a)
                    schedule_fifo_clock(&schedule, &schedule.fifo_a, &schedule.pcm.fifo_a, event);
                if (clock_b)
                    schedule_fifo_clock(&schedule, &schedule.fifo_b, &schedule.pcm.fifo_b, event);
            }
        }
        else if (event->kind == HW_AUDIO_TRACE_SAMPLE)
            ok = schedule_sample(&schedule, event);
        else
            ok = false;
        if (!ok)
            return event->kind == HW_AUDIO_TRACE_WRITE && (event->address == PORYAAAA_GBA_IO_BASE + 0xA0 ||
                                                           event->address == PORYAAAA_GBA_IO_BASE + 0xA4)
                       ? HW_AUDIO_TRACE_UNSUPPORTED_WIDTH
                       : HW_AUDIO_TRACE_INVALID_ARGUMENT;
        if (event->kind == HW_AUDIO_TRACE_WRITE && hw_audio_trace_event_is_cgb_batch_write(event))
            candidate_cgb_batch = true;
        bool final_event_at_cycle = index + 1u == event_count || events[index + 1u].cycle != event->cycle;
        if (candidate_cgb_batch && final_event_at_cycle)
        {
            if (!emit_pending_samples(&schedule))
                return HW_AUDIO_TRACE_INVALID_ARGUMENT;
            candidate_cgb_batch = false;
        }
        schedule.prior_cycle = event->cycle;
        schedule.prior_order = event->order;
        schedule.position_valid = true;
    }
    if (!emit_pending_samples(&schedule))
        return HW_AUDIO_TRACE_INVALID_ARGUMENT;
    *sample_count = schedule.output_count;
    return HW_AUDIO_TRACE_OK;
}
