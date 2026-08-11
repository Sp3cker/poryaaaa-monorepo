#ifndef M4A_DRIVER_TRACE_H
#define M4A_DRIVER_TRACE_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "m4a_driver.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct
    {
        FILE* output;
        uint64_t begin_cycle;
        uint64_t end_cycle;
        uint64_t previous_cycle;
        uint64_t skipped_pcm_events;
        uint32_t previous_order;
        uint16_t soundcnt_l;
        bool position_valid;
        bool open;
    } M4ADriverTraceWriter;

    /* Opens a canonical hardware-event interval. The caller supplies GBA cycles
     * because M4ARegWrite offsets are host-frame relative. `soundcnt_l` preserves
     * the driver's current control-register image without adding a setup write. */
    bool m4a_driver_trace_begin(
        M4ADriverTraceWriter* writer, FILE* output, uint64_t begin_cycle, uint64_t end_cycle, uint16_t soundcnt_l);

    /* Maps each emitted CGB/control M4ARegWrite directly to one normalized GBA
     * WRITE record. PCM_PUBLISH and PCM_RESET are intentionally skipped: they do
     * not identify a FIFO bus write or TIMER edge. */
    bool m4a_driver_trace_write_batch(M4ADriverTraceWriter* writer,
                                      const M4ARegWriteBatch* batch,
                                      uint64_t render_start_cycle,
                                      uint32_t cycles_per_frame);

    bool m4a_driver_trace_end(M4ADriverTraceWriter* writer);

#ifdef __cplusplus
}
#endif

#endif
