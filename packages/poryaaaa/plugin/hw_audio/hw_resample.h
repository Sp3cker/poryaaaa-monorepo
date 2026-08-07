#ifndef HW_RESAMPLE_H
#define HW_RESAMPLE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define HW_RESAMPLE_BUFFER_SIZE 32768

    /* Streaming stereo port of current mGBA's 16-tap windowed-sinc frontend. */
    typedef struct
    {
        double source_rate;
        double destination_rate;
        double timestamp;
        int available;
        int16_t input_l[HW_RESAMPLE_BUFFER_SIZE];
        int16_t input_r[HW_RESAMPLE_BUFFER_SIZE];
    } HwResample;

    void hw_resample_init(HwResample* rs, double input_rate, double output_rate);

    /* Return the DAC input samples needed to make `output_samples` more host samples readable. */
    int hw_resample_inputs_needed(const HwResample* rs, int output_samples);

    /* Feed DAC samples and drain up to max_out current-mGBA frontend samples. */
    int hw_resample_process(
        HwResample* rs, const float* in_l, const float* in_r, int in_n, float* out_l, float* out_r, int max_out);

#ifdef __cplusplus
}
#endif

#endif
