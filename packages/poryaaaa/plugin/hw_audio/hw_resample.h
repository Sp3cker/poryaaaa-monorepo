#ifndef HW_RESAMPLE_H
#define HW_RESAMPLE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define HW_RESAMPLE_BUFFER_SIZE 32768
#define HW_RESAMPLE_EXTRA 18

    /* Streaming stereo port of the blip_buf frontend bundled with mGBA 0.10.5. */
    typedef struct
    {
        uint64_t factor;
        uint64_t offset;
        uint32_t clocks_per_input;
        int available;
        int integrator_l;
        int integrator_r;
        int last_input_l;
        int last_input_r;
        int delta_l[HW_RESAMPLE_BUFFER_SIZE + HW_RESAMPLE_EXTRA];
        int delta_r[HW_RESAMPLE_BUFFER_SIZE + HW_RESAMPLE_EXTRA];
    } HwResample;

    void hw_resample_init(HwResample* rs, double input_rate, double output_rate);
    void hw_resample_set_rates(HwResample* rs, double input_rate, double output_rate);

    /* Return the DAC input samples needed to make `output_samples` more host samples readable. */
    int hw_resample_inputs_needed(const HwResample* rs, int output_samples);

    /* Feed DAC samples and drain up to max_out mGBA-frontend samples. */
    int hw_resample_process(
        HwResample* rs, const float* in_l, const float* in_r, int in_n, float* out_l, float* out_r, int max_out);

#ifdef __cplusplus
}
#endif

#endif
