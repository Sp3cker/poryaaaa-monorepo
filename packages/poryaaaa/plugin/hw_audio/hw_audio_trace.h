#ifndef HW_AUDIO_TRACE_H
#define HW_AUDIO_TRACE_H

#include <stddef.h>

#include <stdint.h>

#include "hw_audio.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define HW_AUDIO_GBA_CLOCK_HZ 16777216u
#define HW_AUDIO_GBA_IO_BASE 0x04000000u

    /* Hardware-comparison events use absolute GBA cycles in mGBA's signed
     * 32-bit timing range and an explicit same-cycle order. SAMPLE events
     * make the DAC cadence observable instead of inferring a host rate. */
    typedef enum
    {
        HW_AUDIO_TRACE_WRITE = 1,
        HW_AUDIO_TRACE_SAMPLE = 2,
        HW_AUDIO_TRACE_TIMER = 3,
    } HwAudioTraceEventKind;

    typedef struct
    {
        uint64_t cycle;
        uint32_t order;
        HwAudioTraceEventKind kind;
        uint8_t width;
        uint32_t address;
        uint32_t value;
    } HwAudioTraceEvent;

    typedef struct
    {
        uint64_t cycle;
        int16_t left;
        int16_t right;
    } HwAudioNativeFrame;
    typedef struct
    {
        int8_t fifo_a;
        int8_t fifo_b;
    } HwAudioTraceFifoSample;

    typedef enum
    {
        HW_AUDIO_TRACE_OK = 0,
        HW_AUDIO_TRACE_INVALID_ARGUMENT,
        HW_AUDIO_TRACE_OUT_OF_ORDER,
        HW_AUDIO_TRACE_UNSUPPORTED_WIDTH,
        HW_AUDIO_TRACE_UNSUPPORTED_ADDRESS,
    } HwAudioTraceStatus;

    /* Establishes GBA reset state for a trace replay. This is intentionally
     * separate from hw_audio_reset(), whose defaults mirror m4aSoundInit. */
    void hw_audio_trace_reset(HwAudio* hw);

    /* Applies one ordered hardware event. The PSG advances from the prior
     * trace cycle to this event's absolute cycle before the event is applied.
     * SAMPLE observes and writes exactly one current native DAC frame; WRITE
     * and TIMER events leave `frame` zeroed. FIFO writes are 32-bit
     * little-endian values at the GBA FIFO addresses; TIMER value 0 or 1
     * drains channels selecting that timer. */
    HwAudioTraceStatus hw_audio_trace_apply(HwAudio* hw, const HwAudioTraceEvent* event, HwAudioNativeFrame* frame);
    /* Precomputes the FIFO bytes used by every SAMPLE in a complete event
     * sequence. mGBA renders native samples in 1024-cycle blocks, so a timer
     * later in one sample interval can determine that interval's byte. */
    HwAudioTraceStatus hw_audio_trace_schedule_fifo_samples(const HwAudioTraceEvent* events,
                                                            size_t event_count,
                                                            HwAudioTraceFifoSample* samples,
                                                            size_t sample_capacity,
                                                            size_t* sample_count);

    /* Applies one event while using a precomputed mGBA FIFO-bin value for a
     * SAMPLE. Non-SAMPLE events and all channel progression remain identical
     * to hw_audio_trace_apply(). */
    HwAudioTraceStatus hw_audio_trace_apply_fifo_sample(HwAudio* hw,
                                                        const HwAudioTraceEvent* event,
                                                        const HwAudioTraceFifoSample* fifo_sample,
                                                        HwAudioNativeFrame* frame);

    const char* hw_audio_trace_status_string(HwAudioTraceStatus status);

#ifdef __cplusplus
}
#endif

#endif
