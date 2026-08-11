#include "m4a_driver_trace.h"

#include <inttypes.h>
#include <limits.h>
#include <string.h>

#include "hw_audio/hw_audio_trace.h"

/* Accept the driver's absolute position without synthesising a replacement
 * order: trace replay must observe the same same-cycle sequence. */
static bool accept_position(M4ADriverTraceWriter* writer, uint64_t cycle, uint32_t order)
{
    if (!writer->position_valid || cycle > writer->previous_cycle)
    {
        writer->previous_cycle = cycle;
        writer->previous_order = order;
        writer->position_valid = true;
        return true;
    }
    if (cycle != writer->previous_cycle || order <= writer->previous_order)
        return false;

    writer->previous_order = order;
    return true;
}

/* Preserve emitted ordering while using mGBA's observed access width per register. */
static bool map_register_write(
    M4ADriverTraceWriter* writer, const M4ARegWrite* source, uint8_t* width, uint32_t* address, uint32_t* value)
{
    *value = source->value;
    switch (source->reg)
    {
    case M4A_REG_NR10:
        *width = 2u;
        *address = HW_AUDIO_GBA_IO_BASE + 0x60u;
        return true;
    case M4A_REG_NR11:
        *width = 1u;
        *address = HW_AUDIO_GBA_IO_BASE + 0x62u;
        return true;
    case M4A_REG_NR12:
        *width = 1u;
        *address = HW_AUDIO_GBA_IO_BASE + 0x63u;
        return true;
    case M4A_REG_NR13:
        *width = 1u;
        *address = HW_AUDIO_GBA_IO_BASE + 0x64u;
        return true;
    case M4A_REG_NR14:
        *width = 1u;
        *address = HW_AUDIO_GBA_IO_BASE + 0x65u;
        return true;
    case M4A_REG_NR21:
        *width = 1u;
        *address = HW_AUDIO_GBA_IO_BASE + 0x68u;
        return true;
    case M4A_REG_NR22:
        *width = 1u;
        *address = HW_AUDIO_GBA_IO_BASE + 0x69u;
        return true;
    case M4A_REG_NR23:
        *width = 1u;
        *address = HW_AUDIO_GBA_IO_BASE + 0x6Cu;
        return true;
    case M4A_REG_NR24:
        *width = 1u;
        *address = HW_AUDIO_GBA_IO_BASE + 0x6Du;
        return true;
    case M4A_REG_NR30:
        *width = 2u;
        *address = HW_AUDIO_GBA_IO_BASE + 0x70u;
        return true;
    case M4A_REG_NR31:
        *width = 1u;
        *address = HW_AUDIO_GBA_IO_BASE + 0x72u;
        return true;
    case M4A_REG_NR32:
        *width = 1u;
        *address = HW_AUDIO_GBA_IO_BASE + 0x73u;
        return true;
    case M4A_REG_NR33:
        *width = 1u;
        *address = HW_AUDIO_GBA_IO_BASE + 0x74u;
        return true;
    case M4A_REG_NR34:
        *width = 1u;
        *address = HW_AUDIO_GBA_IO_BASE + 0x75u;
        return true;
    case M4A_REG_NR41:
        *width = 1u;
        *address = HW_AUDIO_GBA_IO_BASE + 0x78u;
        return true;
    case M4A_REG_NR42:
        *width = 1u;
        *address = HW_AUDIO_GBA_IO_BASE + 0x79u;
        return true;
    case M4A_REG_NR43:
        *width = 1u;
        *address = HW_AUDIO_GBA_IO_BASE + 0x7Cu;
        return true;
    case M4A_REG_NR44:
        *width = 1u;
        *address = HW_AUDIO_GBA_IO_BASE + 0x7Du;
        return true;
    case M4A_REG_NR50:
        writer->soundcnt_l = (uint16_t)((writer->soundcnt_l & 0xFF00u) | (source->value & 0xFFu));
        *width = 2u;
        *address = HW_AUDIO_GBA_IO_BASE + 0x80u;
        *value = writer->soundcnt_l;
        return true;
    case M4A_REG_NR51:
        writer->soundcnt_l = (uint16_t)((writer->soundcnt_l & 0x00FFu) | ((source->value & 0xFFu) << 8u));
        *width = 2u;
        *address = HW_AUDIO_GBA_IO_BASE + 0x80u;
        *value = writer->soundcnt_l;
        return true;
    case M4A_REG_NR52:
        *width = 2u;
        *address = HW_AUDIO_GBA_IO_BASE + 0x84u;
        return true;
    case M4A_REG_SOUNDCNT_H:
        *width = 2u;
        *address = HW_AUDIO_GBA_IO_BASE + 0x82u;
        return true;
    case M4A_REG_SOUNDBIAS:
        *width = 2u;
        *address = HW_AUDIO_GBA_IO_BASE + 0x88u;
        return true;
    case M4A_REG_WAVE_RAM_BYTE:
    {
        uint32_t offset = source->value >> 8u;
        if (offset >= 16u)
            return false;
        *width = 1u;
        *address = HW_AUDIO_GBA_IO_BASE + 0x90u + offset;
        *value = source->value & 0xFFu;
        return true;
    }
    case M4A_REG_FIFO_A:
        *width = 4u;
        *address = HW_AUDIO_GBA_IO_BASE + 0xA0u;
        return true;
    case M4A_REG_FIFO_B:
        *width = 4u;
        *address = HW_AUDIO_GBA_IO_BASE + 0xA4u;
        return true;
    case M4A_REG_TIMER_0:
    case M4A_REG_TIMER_1:
        return false;
    }
    return false;
}

bool m4a_driver_trace_begin(
    M4ADriverTraceWriter* writer, FILE* output, uint64_t begin_cycle, uint64_t end_cycle, uint16_t soundcnt_l)
{
    if (!writer || !output || end_cycle <= begin_cycle)
        return false;
    memset(writer, 0, sizeof(*writer));
    writer->output = output;
    writer->begin_cycle = begin_cycle;
    writer->end_cycle = end_cycle;
    writer->soundcnt_l = soundcnt_l;
    uint32_t order = 0u;
    if (!accept_position(writer, begin_cycle, order) ||
        fprintf(output,
                "PORYAAAA_AUDIO_TRACE 1\nCLOCK %u\nBEGIN %" PRIu64 " %" PRIu32 "\n",
                HW_AUDIO_GBA_CLOCK_HZ,
                begin_cycle,
                order) < 0)
    {
        return false;
    }
    writer->open = true;
    return true;
}

bool m4a_driver_trace_write_batch(M4ADriverTraceWriter* writer, const M4ARegWriteBatch* batch)
{
    if (!writer || !writer->open || !batch)
        return false;
    for (size_t index = 0; index < batch->count; index++)
    {
        const M4ARegWrite* source = &batch->events[index];
        if (source->cycle < writer->begin_cycle || source->cycle > writer->end_cycle)
            continue;
        if (source->reg == M4A_REG_TIMER_0 || source->reg == M4A_REG_TIMER_1)
        {
            const uint32_t timer = source->reg == M4A_REG_TIMER_0 ? 0u : 1u;
            if (!accept_position(writer, source->cycle, source->order) ||
                fprintf(writer->output,
                        "TIMER %" PRIu64 " %" PRIu32 " %" PRIu32 "\n",
                        source->cycle,
                        source->order,
                        timer) < 0)
            {
                return false;
            }
            continue;
        }

        uint8_t width = 0u;
        uint32_t address = 0u;
        uint32_t value = 0u;
        if (!map_register_write(writer, source, &width, &address, &value) ||
            !accept_position(writer, source->cycle, source->order) ||
            fprintf(writer->output,
                    "WRITE %" PRIu64 " %" PRIu32 " %u 0x%08" PRIX32 " 0x%08" PRIX32 "\n",
                    source->cycle,
                    source->order,
                    (unsigned)width,
                    address,
                    value) < 0)
        {
            return false;
        }
    }
    return true;
}

bool m4a_driver_trace_end(M4ADriverTraceWriter* writer)
{
    if (!writer || !writer->open)
        return false;
    uint32_t order = writer->previous_cycle == writer->end_cycle ? writer->previous_order + 1u : 0u;
    if (!accept_position(writer, writer->end_cycle, order) ||
        fprintf(writer->output, "END %" PRIu64 " %" PRIu32 "\n", writer->end_cycle, order) < 0 ||
        fflush(writer->output) != 0)
    {
        return false;
    }
    writer->open = false;
    return true;
}
