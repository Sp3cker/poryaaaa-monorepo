#include "hw_audio_trace_text.h"

#include <limits.h>
#include <string.h>

typedef struct
{
    bool valid;
    uint64_t cycle;
    uint32_t order;
} TracePosition;

static bool advance_position(TracePosition* position, uint64_t cycle, uint32_t order)
{
    if (position->valid && (cycle < position->cycle || (cycle == position->cycle && order <= position->order)))
        return false;
    position->valid = true;
    position->cycle = cycle;
    position->order = order;
    return true;
}

/* Parse one unsigned decimal token without accepting signs or overflow. */
static bool parse_u64_decimal(const char* text, uint64_t* result)
{
    if (*text == '\0')
        return false;
    uint64_t value = 0;
    for (const char* character = text; *character != '\0'; character++)
    {
        if (*character < '0' || *character > '9')
            return false;
        uint64_t digit = (uint64_t)(*character - '0');
        if (value > (UINT64_MAX - digit) / 10u)
            return false;
        value = value * 10u + digit;
    }
    *result = value;
    return true;
}

/* Parse one canonical 0x-prefixed unsigned hexadecimal token. */
static bool parse_u32_hex(const char* text, uint32_t* result)
{
    if (text[0] != '0' || text[1] != 'x' || text[2] == '\0')
        return false;
    uint32_t value = 0;
    for (const char* character = text + 2; *character != '\0'; character++)
    {
        uint32_t digit;
        if (*character >= '0' && *character <= '9')
            digit = (uint32_t)(*character - '0');
        else if (*character >= 'A' && *character <= 'F')
            digit = (uint32_t)(*character - 'A') + 10u;
        else if (*character >= 'a' && *character <= 'f')
            digit = (uint32_t)(*character - 'a') + 10u;
        else
            return false;
        if (value > (UINT32_MAX - digit) / 16u)
            return false;
        value = value * 16u + digit;
    }
    *result = value;
    return true;
}

/* Split the exact trace token grammar: one ASCII space and no trailing whitespace. */
static bool split_trace_line(char* line, char* tokens[6], size_t* count)
{
    *count = 0;
    if (*line == '\0')
        return false;
    char* cursor = line;
    while (*cursor != '\0')
    {
        if (*count == 6u || *cursor == ' ' || *cursor == '\t' || *cursor == '\r')
            return false;
        tokens[(*count)++] = cursor;
        while (*cursor != '\0' && *cursor != ' ')
        {
            if (*cursor == '\t' || *cursor == '\r')
                return false;
            cursor++;
        }
        if (*cursor == '\0')
            break;
        *cursor++ = '\0';
        if (*cursor == '\0' || *cursor == ' ')
            return false;
    }
    return true;
}
static HwAudioTraceTextStatus
visit_record(HwAudioTraceTextVisitor visitor, void* context, const HwAudioTraceTextRecord* record, unsigned* error_line)
{
    if (visitor && !visitor(context, record))
    {
        if (error_line)
            *error_line = record->line_number;
        return HW_AUDIO_TRACE_TEXT_VISITOR_FAILED;
    }
    return HW_AUDIO_TRACE_TEXT_OK;
}

HwAudioTraceTextStatus
hw_audio_trace_text_read(FILE* input, HwAudioTraceTextVisitor visitor, void* context, unsigned* error_line)
{
    if (error_line)
        *error_line = 0u;
    if (!input)
        return HW_AUDIO_TRACE_TEXT_READ_FAILED;

    char line[HW_AUDIO_TRACE_TEXT_LINE_CAPACITY];
    if (!fgets(line, sizeof(line), input) || strcmp(line, "PORYAAAA_AUDIO_TRACE 1\n") != 0)
        return HW_AUDIO_TRACE_TEXT_INVALID_HEADER;

    bool clock_seen = false;
    bool measurement_open = false;
    bool measurement_closed = false;
    TracePosition position = {0};
    unsigned line_number = 1u;
    while (fgets(line, sizeof(line), input))
    {
        line_number++;
        size_t length = strlen(line);
        if (!length || line[length - 1u] != '\n')
        {
            if (error_line)
                *error_line = line_number;
            return HW_AUDIO_TRACE_TEXT_LINE_TOO_LONG;
        }
        line[length - 1u] = '\0';
        if (line[0] == '#' || line[0] == '\0' || line[0] == '\r')
            continue;

        char* tokens[6];
        size_t token_count = 0;
        if (!split_trace_line(line, tokens, &token_count))
        {
            if (error_line)
                *error_line = line_number;
            return HW_AUDIO_TRACE_TEXT_INVALID_EVENT;
        }

        uint64_t parsed = 0;
        if (strcmp(tokens[0], "CLOCK") == 0)
        {
            if (token_count != 2u || !parse_u64_decimal(tokens[1], &parsed) || clock_seen || position.valid ||
                parsed != HW_AUDIO_GBA_CLOCK_HZ)
            {
                if (error_line)
                    *error_line = line_number;
                return HW_AUDIO_TRACE_TEXT_INVALID_CLOCK;
            }
            clock_seen = true;
            continue;
        }
        if (!clock_seen)
        {
            if (error_line)
                *error_line = line_number;
            return HW_AUDIO_TRACE_TEXT_EVENT_BEFORE_CLOCK;
        }

        uint64_t cycle = 0;
        uint64_t order_value = 0;
        bool marker = strcmp(tokens[0], "BEGIN") == 0 || strcmp(tokens[0], "END") == 0;
        if (token_count < 3u || !parse_u64_decimal(tokens[1], &cycle) || !parse_u64_decimal(tokens[2], &order_value) ||
            order_value > UINT32_MAX)
        {
            if (error_line)
                *error_line = line_number;
            return marker ? HW_AUDIO_TRACE_TEXT_INVALID_MARKER : HW_AUDIO_TRACE_TEXT_INVALID_EVENT;
        }
        uint32_t order = (uint32_t)order_value;
        HwAudioTraceTextRecord record = {
            .cycle = cycle,
            .order = order,
            .line_number = line_number,
        };

        if (strcmp(tokens[0], "BEGIN") == 0 && token_count == 3u)
        {
            if (measurement_open || measurement_closed || !advance_position(&position, cycle, order))
            {
                if (error_line)
                    *error_line = line_number;
                return HW_AUDIO_TRACE_TEXT_INVALID_MARKER;
            }
            measurement_open = true;
            record.kind = HW_AUDIO_TRACE_TEXT_BEGIN;
        }
        else if (strcmp(tokens[0], "END") == 0 && token_count == 3u)
        {
            if (!measurement_open || measurement_closed || !advance_position(&position, cycle, order))
            {
                if (error_line)
                    *error_line = line_number;
                return HW_AUDIO_TRACE_TEXT_INVALID_MARKER;
            }
            measurement_open = false;
            measurement_closed = true;
            record.kind = HW_AUDIO_TRACE_TEXT_END;
        }
        else
        {
            if (measurement_closed)
            {
                if (error_line)
                    *error_line = line_number;
                return HW_AUDIO_TRACE_TEXT_EVENT_AFTER_END;
            }

            HwAudioTraceEvent event = {0};
            if (strcmp(tokens[0], "WRITE") == 0 && token_count == 6u)
            {
                uint64_t width = 0;
                uint32_t address = 0;
                uint32_t value = 0;
                if (!parse_u64_decimal(tokens[3], &width) || width > UINT8_MAX || !parse_u32_hex(tokens[4], &address) ||
                    !parse_u32_hex(tokens[5], &value) || !advance_position(&position, cycle, order))
                {
                    if (error_line)
                        *error_line = line_number;
                    return HW_AUDIO_TRACE_TEXT_INVALID_EVENT;
                }
                event = (HwAudioTraceEvent){
                    .cycle = cycle,
                    .order = order,
                    .kind = HW_AUDIO_TRACE_WRITE,
                    .width = (uint8_t)width,
                    .address = address,
                    .value = value,
                };
            }
            else if (strcmp(tokens[0], "SAMPLE") == 0 && token_count == 3u)
            {
                if (!advance_position(&position, cycle, order))
                {
                    if (error_line)
                        *error_line = line_number;
                    return HW_AUDIO_TRACE_TEXT_INVALID_EVENT;
                }
                event = (HwAudioTraceEvent){
                    .cycle = cycle,
                    .order = order,
                    .kind = HW_AUDIO_TRACE_SAMPLE,
                };
            }
            else if (strcmp(tokens[0], "TIMER") == 0 && token_count == 4u)
            {
                uint64_t timer = 0;
                if (!parse_u64_decimal(tokens[3], &timer) || timer > 1u || !advance_position(&position, cycle, order))
                {
                    if (error_line)
                        *error_line = line_number;
                    return HW_AUDIO_TRACE_TEXT_INVALID_EVENT;
                }
                event = (HwAudioTraceEvent){
                    .cycle = cycle,
                    .order = order,
                    .kind = HW_AUDIO_TRACE_TIMER,
                    .value = (uint32_t)timer,
                };
            }
            else
            {
                if (error_line)
                    *error_line = line_number;
                return HW_AUDIO_TRACE_TEXT_INVALID_EVENT;
            }
            record.kind = HW_AUDIO_TRACE_TEXT_EVENT;
            record.event = event;
        }

        HwAudioTraceTextStatus status = visit_record(visitor, context, &record, error_line);
        if (status != HW_AUDIO_TRACE_TEXT_OK)
            return status;
    }
    if (ferror(input))
        return HW_AUDIO_TRACE_TEXT_READ_FAILED;
    if (!clock_seen || measurement_open || !measurement_closed)
        return HW_AUDIO_TRACE_TEXT_INCOMPLETE_INTERVAL;
    return HW_AUDIO_TRACE_TEXT_OK;
}

const char* hw_audio_trace_text_status_string(HwAudioTraceTextStatus status)
{
    switch (status)
    {
    case HW_AUDIO_TRACE_TEXT_OK:
        return "ok";
    case HW_AUDIO_TRACE_TEXT_INVALID_HEADER:
        return "trace must begin with PORYAAAA_AUDIO_TRACE 1";
    case HW_AUDIO_TRACE_TEXT_LINE_TOO_LONG:
        return "trace line exceeds the maximum length";
    case HW_AUDIO_TRACE_TEXT_INVALID_CLOCK:
        return "invalid CLOCK declaration";
    case HW_AUDIO_TRACE_TEXT_EVENT_BEFORE_CLOCK:
        return "trace event precedes CLOCK declaration";
    case HW_AUDIO_TRACE_TEXT_INVALID_MARKER:
        return "invalid BEGIN or END marker";
    case HW_AUDIO_TRACE_TEXT_EVENT_AFTER_END:
        return "trace event follows END marker";
    case HW_AUDIO_TRACE_TEXT_INVALID_EVENT:
        return "invalid trace event";
    case HW_AUDIO_TRACE_TEXT_VISITOR_FAILED:
        return "trace visitor rejected a record";
    case HW_AUDIO_TRACE_TEXT_INCOMPLETE_INTERVAL:
        return "trace requires one closed measurement interval";
    case HW_AUDIO_TRACE_TEXT_READ_FAILED:
        return "could not read trace";
    }
    return "unknown trace parser status";
}
