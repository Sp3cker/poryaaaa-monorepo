#ifndef HW_AUDIO_TRACE_TEXT_H
#define HW_AUDIO_TRACE_TEXT_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "hw_audio_trace.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define HW_AUDIO_TRACE_TEXT_LINE_CAPACITY 512

    typedef enum
    {
        HW_AUDIO_TRACE_TEXT_BEGIN,
        HW_AUDIO_TRACE_TEXT_END,
        HW_AUDIO_TRACE_TEXT_EVENT,
    } HwAudioTraceTextRecordKind;

    typedef struct
    {
        HwAudioTraceTextRecordKind kind;
        unsigned line_number;
        uint64_t cycle;
        uint32_t order;
        HwAudioTraceEvent event;
    } HwAudioTraceTextRecord;

    typedef bool (*HwAudioTraceTextVisitor)(void* context, const HwAudioTraceTextRecord* record);

    typedef enum
    {
        HW_AUDIO_TRACE_TEXT_OK,
        HW_AUDIO_TRACE_TEXT_INVALID_HEADER,
        HW_AUDIO_TRACE_TEXT_LINE_TOO_LONG,
        HW_AUDIO_TRACE_TEXT_INVALID_CLOCK,
        HW_AUDIO_TRACE_TEXT_EVENT_BEFORE_CLOCK,
        HW_AUDIO_TRACE_TEXT_INVALID_MARKER,
        HW_AUDIO_TRACE_TEXT_EVENT_AFTER_END,
        HW_AUDIO_TRACE_TEXT_INVALID_EVENT,
        HW_AUDIO_TRACE_TEXT_VISITOR_FAILED,
        HW_AUDIO_TRACE_TEXT_INCOMPLETE_INTERVAL,
        HW_AUDIO_TRACE_TEXT_READ_FAILED,
    } HwAudioTraceTextStatus;

    /* Reads the versioned trace grammar shared by the mGBA recorder and poryaaaa
     * replay. The visitor receives BEGIN/END records and WRITE/TIMER/SAMPLE events
     * in validated total order. A trace needs one closed measurement interval, but
     * not a SAMPLE event. */
    HwAudioTraceTextStatus
    hw_audio_trace_text_read(FILE* input, HwAudioTraceTextVisitor visitor, void* context, unsigned* error_line);

    const char* hw_audio_trace_text_status_string(HwAudioTraceTextStatus status);

#ifdef __cplusplus
}
#endif

#endif
