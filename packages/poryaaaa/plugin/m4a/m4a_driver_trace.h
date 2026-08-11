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
        uint32_t previous_order;
        uint16_t soundcnt_l;
        bool position_valid;
        bool open;
    } M4ADriverTraceWriter;

    /* Opens a canonical hardware-event interval. Events already carry
     * absolute GBA cycles and stable same-cycle orders. `soundcnt_l`
     * preserves the driver's control-register image without a setup write. */
    bool m4a_driver_trace_begin(
        M4ADriverTraceWriter* writer, FILE* output, uint64_t begin_cycle, uint64_t end_cycle, uint16_t soundcnt_l);

    /* Converts driver events to canonical hardware trace records: GBA WRITE
     * records for registers/FIFO words and TIMER records for overflows. */
    bool m4a_driver_trace_write_batch(M4ADriverTraceWriter* writer, const M4ARegWriteBatch* batch);

    bool m4a_driver_trace_end(M4ADriverTraceWriter* writer);

#ifdef __cplusplus
}
#endif

#endif
