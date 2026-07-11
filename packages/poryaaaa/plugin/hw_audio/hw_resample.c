/* blip_buf 1.1.0-compatible frontend used by mGBA 0.10.5.
 * Copyright (C) 2003-2009 Shay Green. Filter table and algorithm are
 * adapted under the GNU Lesser General Public License, version 2.1 or,
 * at your option, any later version. This code comes without warranty. */
#include "hw_resample.h"

#include <assert.h>
#include <math.h>
#include <string.h>

#define GBA_AUDIO_CLOCK 16777216.0
#define BLIP_TIME_BITS 52
#define BLIP_PRE_SHIFT 32
#define BLIP_FRAC_BITS 20
#define BLIP_PHASE_BITS 5
#define BLIP_PHASE_COUNT (1 << BLIP_PHASE_BITS)
#define BLIP_DELTA_BITS 15
#define BLIP_DELTA_UNIT (1 << BLIP_DELTA_BITS)
#define BLIP_BASS_SHIFT 9
#define BLIP_HALF_WIDTH 8

static const uint64_t kTimeUnit = (uint64_t)1 << BLIP_TIME_BITS;

static const int16_t kBlipStep[BLIP_PHASE_COUNT + 1][BLIP_HALF_WIDTH] = {
    {43, -115, 350, -488, 1136, -914, 5861, 21022}, {44, -118, 348, -473, 1076, -799, 5274, 21001},
    {45, -121, 344, -454, 1011, -677, 4706, 20936}, {46, -122, 336, -431, 942, -549, 4156, 20829},
    {47, -123, 327, -404, 868, -418, 3629, 20679},  {47, -122, 316, -375, 792, -285, 3124, 20488},
    {47, -120, 303, -344, 714, -151, 2644, 20256},  {46, -117, 289, -310, 634, -17, 2188, 19985},
    {46, -114, 273, -275, 553, 117, 1758, 19675},   {44, -108, 255, -237, 471, 247, 1356, 19327},
    {43, -103, 237, -199, 390, 373, 981, 18944},    {42, -98, 218, -160, 310, 495, 633, 18527},
    {40, -91, 198, -121, 231, 611, 314, 18078},     {38, -84, 178, -81, 153, 722, 22, 17599},
    {36, -76, 157, -43, 80, 824, -241, 17092},      {34, -68, 135, -3, 8, 919, -476, 16558},
    {32, -61, 115, 34, -60, 1006, -683, 16001},     {29, -52, 94, 70, -123, 1083, -862, 15422},
    {27, -44, 73, 106, -184, 1152, -1015, 14824},   {25, -36, 53, 139, -239, 1211, -1142, 14210},
    {22, -27, 34, 170, -290, 1261, -1244, 13582},   {20, -20, 16, 199, -335, 1301, -1322, 12942},
    {18, -12, -3, 226, -375, 1331, -1376, 12293},   {15, -4, -19, 250, -410, 1351, -1408, 11638},
    {13, 3, -35, 272, -439, 1361, -1419, 10979},    {11, 9, -49, 292, -464, 1362, -1410, 10319},
    {9, 16, -63, 309, -483, 1354, -1383, 9660},     {7, 22, -75, 322, -496, 1337, -1339, 9005},
    {6, 26, -85, 333, -504, 1312, -1280, 8355},     {4, 31, -94, 341, -507, 1278, -1205, 7713},
    {3, 35, -102, 347, -506, 1238, -1119, 7082},    {1, 40, -110, 350, -499, 1190, -1021, 6464},
    {0, 43, -115, 350, -488, 1136, -914, 5861},
};

/* Map a GBA CPU timestamp onto blip_buf's fixed output-sample timeline. */
static uint64_t fixed_time(const HwResample* rs, uint64_t clocks)
{
#if defined(__SIZEOF_INT128__)
    __uint128_t value = (__uint128_t)clocks * rs->factor + rs->offset;
    return (uint64_t)(value >> BLIP_PRE_SHIFT);
#else
    long double value = (long double)clocks * (long double)rs->factor + (long double)rs->offset;
    return (uint64_t)(value / 4294967296.0L);
#endif
}

/* Distribute one DAC step across the same impulse kernel mGBA uses. */
static void add_delta(HwResample* rs, int* buffer, uint32_t input_time, int delta)
{
    uint64_t clocks = (uint64_t)input_time * rs->clocks_per_input;
    uint32_t fixed = (uint32_t)fixed_time(rs, clocks);
    int output_index = rs->available + (int)(fixed >> BLIP_FRAC_BITS);
    int phase_shift = BLIP_FRAC_BITS - BLIP_PHASE_BITS;
    int phase = (fixed >> phase_shift) & (BLIP_PHASE_COUNT - 1);
    int interpolation = (fixed >> (phase_shift - BLIP_DELTA_BITS)) & (BLIP_DELTA_UNIT - 1);
    int delta2 = (delta * interpolation) >> BLIP_DELTA_BITS;
    delta -= delta2;

    assert(output_index <= HW_RESAMPLE_BUFFER_SIZE + 2);
    for (int i = 0; i < BLIP_HALF_WIDTH; i++)
        buffer[output_index + i] += kBlipStep[phase][i] * delta + kBlipStep[phase + 1][i] * delta2;
    for (int i = 0; i < BLIP_HALF_WIDTH; i++)
    {
        int table_index = BLIP_HALF_WIDTH - 1 - i;
        buffer[output_index + BLIP_HALF_WIDTH + i] += kBlipStep[BLIP_PHASE_COUNT - phase][table_index] * delta +
                                                      kBlipStep[BLIP_PHASE_COUNT - phase - 1][table_index] * delta2;
    }
}

/* Commit one input block while preserving fractional output time. */
static void end_frame(HwResample* rs, uint32_t input_count)
{
    uint64_t clocks = (uint64_t)input_count * rs->clocks_per_input;
#if defined(__SIZEOF_INT128__)
    __uint128_t value = (__uint128_t)clocks * rs->factor + rs->offset;
    rs->available += (int)(value >> BLIP_TIME_BITS);
    rs->offset = (uint64_t)value & (kTimeUnit - 1);
#else
    long double value = (long double)clocks * (long double)rs->factor + (long double)rs->offset;
    uint64_t whole = (uint64_t)(value / (long double)kTimeUnit);
    rs->available += (int)whole;
    rs->offset = (uint64_t)(value - (long double)whole * (long double)kTimeUnit);
#endif
    assert(rs->available <= HW_RESAMPLE_BUFFER_SIZE);
}

/* Match blip_buf's signed 16-bit output saturation. */
static int clamp_sample(int sample)
{
    if (sample > 32767)
        return 32767;
    if (sample < -32768)
        return -32768;
    return sample;
}

/* Integrate deltas and apply blip_buf's 511/512 DC-blocking pole. */
static int read_samples(HwResample* rs, float* out_l, float* out_r, int count)
{
    if (count > rs->available)
        count = rs->available;

    int sum_l = rs->integrator_l;
    int sum_r = rs->integrator_r;
    for (int i = 0; i < count; i++)
    {
        int sample_l = clamp_sample(sum_l >> BLIP_DELTA_BITS);
        int sample_r = clamp_sample(sum_r >> BLIP_DELTA_BITS);
        sum_l += rs->delta_l[i];
        sum_r += rs->delta_r[i];
        if (out_l)
            out_l[i] = sample_l / 32768.0f;
        if (out_r)
            out_r[i] = sample_r / 32768.0f;
        sum_l -= sample_l << (BLIP_DELTA_BITS - BLIP_BASS_SHIFT);
        sum_r -= sample_r << (BLIP_DELTA_BITS - BLIP_BASS_SHIFT);
    }
    rs->integrator_l = sum_l;
    rs->integrator_r = sum_r;

    if (count > 0)
    {
        int remaining = rs->available + HW_RESAMPLE_EXTRA - count;
        rs->available -= count;
        memmove(rs->delta_l, rs->delta_l + count, (size_t)remaining * sizeof(rs->delta_l[0]));
        memmove(rs->delta_r, rs->delta_r + count, (size_t)remaining * sizeof(rs->delta_r[0]));
        memset(rs->delta_l + remaining, 0, (size_t)count * sizeof(rs->delta_l[0]));
        memset(rs->delta_r + remaining, 0, (size_t)count * sizeof(rs->delta_r[0]));
    }
    return count;
}

/* Quantize the chip mix into the signed DAC domain consumed by blip_buf. */
static int float_to_sample(float sample)
{
    long value = lrintf(sample * 32768.0f);
    if (value > 32767)
        value = 32767;
    else if (value < -32768)
        value = -32768;
    return (int)value;
}

/* Derive mGBA's fixed clock-to-output ratio for the selected DAC cadence. */
void hw_resample_set_rates(HwResample* rs, double input_rate, double output_rate)
{
    if (input_rate <= 0.0 || output_rate <= 0.0)
    {
        rs->factor = 1;
        rs->clocks_per_input = 1;
        return;
    }

    double factor = (double)kTimeUnit * output_rate / GBA_AUDIO_CLOCK;
    rs->factor = (uint64_t)factor;
    if ((double)rs->factor < factor)
        rs->factor++;

    double clocks = GBA_AUDIO_CLOCK / input_rate;
    rs->clocks_per_input = (uint32_t)(clocks + 0.5);
    if (rs->clocks_per_input == 0)
        rs->clocks_per_input = 1;
}

/* Reset all streaming state for a new input/output rate epoch. */
void hw_resample_init(HwResample* rs, double input_rate, double output_rate)
{
    memset(rs, 0, sizeof(*rs));
    hw_resample_set_rates(rs, input_rate, output_rate);
    rs->offset = rs->factor / 2;
}

/* Compute the minimum DAC input count needed for a requested host span. */
int hw_resample_inputs_needed(const HwResample* rs, int output_samples)
{
    if (output_samples <= 0 || rs->factor == 0)
        return 0;

#if defined(__SIZEOF_INT128__)
    __uint128_t needed = (__uint128_t)(uint32_t)output_samples * kTimeUnit;
    uint64_t clocks = 0;
    if (needed > rs->offset)
        clocks = (uint64_t)((needed - rs->offset + rs->factor - 1) / rs->factor);
#else
    long double needed = (long double)output_samples * (long double)kTimeUnit;
    uint64_t clocks = 0;
    if (needed > rs->offset)
        clocks = (uint64_t)ceill((needed - rs->offset) / (long double)rs->factor);
#endif
    return (int)((clocks + rs->clocks_per_input - 1) / rs->clocks_per_input);
}

/* Feed DAC steps and drain the host samples made readable by that time. */
int hw_resample_process(
    HwResample* rs, const float* in_l, const float* in_r, int in_n, float* out_l, float* out_r, int max_out)
{
    if (in_n < 0 || max_out < 0)
        return 0;

    for (int i = 0; i < in_n; i++)
    {
        int sample_l = float_to_sample(in_l ? in_l[i] : 0.0f);
        int sample_r = float_to_sample(in_r ? in_r[i] : 0.0f);
        int delta_l = sample_l - rs->last_input_l;
        int delta_r = sample_r - rs->last_input_r;
        if (delta_l)
            add_delta(rs, rs->delta_l, (uint32_t)i, delta_l);
        if (delta_r)
            add_delta(rs, rs->delta_r, (uint32_t)i, delta_r);
        rs->last_input_l = sample_l;
        rs->last_input_r = sample_r;
    }
    if (in_n > 0)
        end_frame(rs, (uint32_t)in_n);

    return read_samples(rs, out_l, out_r, max_out);
}
