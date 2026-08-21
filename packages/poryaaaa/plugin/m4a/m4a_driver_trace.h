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

    /* Focused-adapter variant of m4a_driver_trace_begin: emits canonical setup
     * WRITEs through the normal register mapping before BEGIN.  BEGIN follows
     * the setup at the next same-cycle order. */
    bool m4a_driver_trace_begin_with_setup(M4ADriverTraceWriter* writer,
                                           FILE* output,
                                           uint64_t begin_cycle,
                                           uint64_t end_cycle,
                                           uint16_t soundcnt_l,
                                           const M4ARegWrite* setup,
                                           size_t setup_count);

    /* Converts driver events to canonical hardware trace records: GBA WRITE
     * records for registers/FIFO words and TIMER records for overflows. */
    bool m4a_driver_trace_write_batch(M4ADriverTraceWriter* writer, const M4ARegWriteBatch* batch);

    /* Writes one canonical SAMPLE observation on a caller-supplied native DAC
     * cadence.  SAMPLEs make the reset-state sample clock observable so both
     * replay engines compare the same power-on stream; the writer validates
     * the measurement window and strict total order exactly like any event. */
    bool m4a_driver_trace_write_sample(M4ADriverTraceWriter* writer, uint64_t cycle, uint32_t order);

    bool m4a_driver_trace_end(M4ADriverTraceWriter* writer);

#ifdef __cplusplus
}
#endif

#endif
