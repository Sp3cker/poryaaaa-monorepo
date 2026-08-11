/* Current-mGBA-compatible 16-tap windowed-sinc audio resampler.
 *
 * The interpolation kernel follows mGBA's MPL-2.0-licensed
 * src/util/interpolator.c at commit afd6f14eaf8bd35214ed3fb9dc69a92bfc3877a9.
 * This file is therefore provided under the Mozilla Public License 2.0.
 * A copy is available at https://mozilla.org/MPL/2.0/. */
#include "hw_resample.h"

#include <assert.h>
#include <math.h>
#include <stdatomic.h>
#include <string.h>

enum
{
    SINC_RESOLUTION = 8192,
    SINC_WIDTH = 8,
    SINC_SAMPLES = SINC_RESOLUTION * SINC_WIDTH,
};

static double g_sinc_lut[SINC_SAMPLES + 1];
static double g_window_lut[SINC_SAMPLES + 1];
static atomic_int g_lut_state;

/* Build mGBA's shared sinc and three-term Nuttall lookup tables once. */
static void init_luts(void)
{
    int expected = 0;
    if (atomic_compare_exchange_strong_explicit(&g_lut_state, &expected, 1, memory_order_acq_rel, memory_order_acquire))
    {
        const double pi = 3.14159265358979323846;
        const double dy = pi / SINC_SAMPLES;
        const double dx = dy * SINC_WIDTH;
        double x = dx;
        double y = dy;

        g_sinc_lut[0] = 0.0;
        g_window_lut[0] = 1.0;
        for (int i = 1; i <= SINC_SAMPLES; i++, x += dx, y += dy)
        {
            g_sinc_lut[i] = x < SINC_WIDTH ? sin(x) / x : 0.0;
            g_window_lut[i] = 0.40897 + 0.5 * cos(y) + 0.09103 * cos(2.0 * y);
        }
        atomic_store_explicit(&g_lut_state, 2, memory_order_release);
        return;
    }

    while (atomic_load_explicit(&g_lut_state, memory_order_acquire) != 2)
    {
    }
}

static int sample_at(const int16_t* input, int available, int index)
{
    if (index < 0 || index >= available)
        return 0;
    return input[index];
}

/* Match mGBA's normalized 16-tap sinc interpolation at one source timestamp. */
static int16_t interpolate(const HwResample* rs, const int16_t* input, double time)
{
    const int index = (int)time;
    const double subsample = time - floor(time);
    const double sample_step = rs->source_rate / rs->destination_rate;
    const unsigned step = sample_step < 1.0 ? (unsigned)(SINC_RESOLUTION * sample_step) : SINC_RESOLUTION;
    const unsigned y_shift = (unsigned)(subsample * step);
    const unsigned x_shift = (unsigned)(subsample * SINC_RESOLUTION);
    double sum = 0.0;
    double kernel_sum = 0.0;

    for (int i = 1 - SINC_WIDTH; i <= SINC_WIDTH; i++)
    {
        unsigned window = (unsigned)(i >= 0 ? i : -i) * SINC_RESOLUTION;
        window = y_shift > window ? y_shift - window : window - y_shift;

        unsigned sinc = (unsigned)(i >= 0 ? i : -i) * step;
        sinc = x_shift > sinc ? x_shift - sinc : sinc - x_shift;

        const double kernel = g_sinc_lut[sinc] * g_window_lut[window];
        kernel_sum += kernel;
        sum += sample_at(input, rs->available, index + i) * kernel;
    }
    return (int16_t)(sum / kernel_sum);
}

void hw_resample_init(HwResample* rs, double input_rate, double output_rate)
{
    init_luts();
    memset(rs, 0, sizeof(*rs));
    hw_resample_set_rates(rs, input_rate, output_rate);
}

void hw_resample_set_rates(HwResample* rs, double input_rate, double output_rate)
{
    rs->source_rate = input_rate;
    rs->destination_rate = output_rate;
}

int hw_resample_inputs_needed(const HwResample* rs, int output_samples)
{
    if (output_samples <= 0 || rs->source_rate <= 0.0 || rs->destination_rate <= 0.0)
        return 0;

    const double sample_step = rs->source_rate / rs->destination_rate;
    const double last_timestamp = rs->timestamp + (output_samples - 1) * sample_step;
    const int required = (int)floor(last_timestamp + SINC_WIDTH) + 1;
    return required > rs->available ? required - rs->available : 0;
}

int hw_resample_process(
    HwResample* rs, const int16_t* in_l, const int16_t* in_r, int in_n, float* out_l, float* out_r, int max_out)
{
    if (in_n < 0 || max_out < 0 || rs->source_rate <= 0.0 || rs->destination_rate <= 0.0)
        return 0;

    assert(rs->available + in_n <= HW_RESAMPLE_BUFFER_SIZE);
    for (int i = 0; i < in_n; i++)
    {
        rs->input_l[rs->available + i] = in_l ? in_l[i] : 0;
        rs->input_r[rs->available + i] = in_r ? in_r[i] : 0;
    }
    rs->available += in_n;

    const double sample_step = rs->source_rate / rs->destination_rate;
    int produced = 0;
    while (produced < max_out && rs->timestamp + SINC_WIDTH < rs->available)
    {
        if (out_l)
            out_l[produced] = interpolate(rs, rs->input_l, rs->timestamp) / 32768.0f;
        if (out_r)
            out_r[produced] = interpolate(rs, rs->input_r, rs->timestamp) / 32768.0f;
        rs->timestamp += sample_step;
        produced++;
    }

    if (rs->timestamp > SINC_WIDTH)
    {
        int drop = (int)(rs->timestamp - SINC_WIDTH);
        if (drop > rs->available)
            drop = rs->available;
        const int remaining = rs->available - drop;
        memmove(rs->input_l, rs->input_l + drop, (size_t)remaining * sizeof(rs->input_l[0]));
        memmove(rs->input_r, rs->input_r + drop, (size_t)remaining * sizeof(rs->input_r[0]));
        rs->available = remaining;
        rs->timestamp -= drop;
    }
    return produced;
}
