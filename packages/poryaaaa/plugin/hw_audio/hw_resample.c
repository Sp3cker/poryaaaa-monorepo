/*
 * THIRD-PARTY PROVENANCE: blip_buf 1.1.0
 *
 * This file contains a source-ordered port/adaptation of blip_buf 1.1.0,
 * Copyright (C) 2003-2009 Shay Green, licensed under the GNU Lesser General
 * Public License version 2.1 or later (LGPL-2.1-or-later). Poryaaaa adapted
 * it on 2026-08-13 for the exact mGBA 0.10.5 Qt post-native PCM16 handoff;
 * the default path intentionally preserves the source algorithm. See the
 * existing packages/poryaaaa/plugin/hw_audio/LICENSE.blip_buf for the license
 * text and terms. The optional antialias prefilter below is poryaaaa-specific
 * and deliberately outside the default, source-exact path.
 */
#include "hw_resample.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#define HW_RESAMPLE_TIME_BITS 52u
#define HW_RESAMPLE_TIME_UNIT (UINT64_C(1) << HW_RESAMPLE_TIME_BITS)
#define HW_RESAMPLE_FRAC_BITS 20u
#define HW_RESAMPLE_PHASE_BITS 5u
#define HW_RESAMPLE_PHASE_COUNT 32u
#define HW_RESAMPLE_DELTA_BITS 15u
#define HW_RESAMPLE_DELTA_UNIT (UINT32_C(1) << HW_RESAMPLE_DELTA_BITS)
#define HW_RESAMPLE_BASS_SHIFT 9u
#define HW_RESAMPLE_FEEDBACK_SHIFT (HW_RESAMPLE_DELTA_BITS - HW_RESAMPLE_BASS_SHIFT)
#define HW_RESAMPLE_DEFAULT_AA_INPUT_RATE_HZ (HW_RESAMPLE_GBA_CLOCK_HZ / 256u)

/* Sinc_Generator(0.9, 0.55, 4.5), from blip_buf 1.1.0. This must remain flat:
 * the interpolation intentionally reads across adjacent rows. */
static const int16_t bl_step[(HW_RESAMPLE_PHASE_COUNT + 1u) * HW_RESAMPLE_HALF_WIDTH] = {
    43, -115, 350,  -488, 1136, -914, 5861,  21022, 44, -118, 348,  -473, 1076, -799, 5274,  21001,
    45, -121, 344,  -454, 1011, -677, 4706,  20936, 46, -122, 336,  -431, 942,  -549, 4156,  20829,
    47, -123, 327,  -404, 868,  -418, 3629,  20679, 47, -122, 316,  -375, 792,  -285, 3124,  20488,
    47, -120, 303,  -344, 714,  -151, 2644,  20256, 46, -117, 289,  -310, 634,  -17,  2188,  19985,
    46, -114, 273,  -275, 553,  117,  1758,  19675, 44, -108, 255,  -237, 471,  247,  1356,  19327,
    43, -103, 237,  -199, 390,  373,  981,   18944, 42, -98,  218,  -160, 310,  495,  633,   18527,
    40, -91,  198,  -121, 231,  611,  314,   18078, 38, -84,  178,  -81,  153,  722,  22,    17599,
    36, -76,  157,  -43,  80,   824,  -241,  17092, 34, -68,  135,  -3,   8,    919,  -476,  16558,
    32, -61,  115,  34,   -60,  1006, -683,  16001, 29, -52,  94,   70,   -123, 1083, -862,  15422,
    27, -44,  73,   106,  -184, 1152, -1015, 14824, 25, -36,  53,   139,  -239, 1211, -1142, 14210,
    22, -27,  34,   170,  -290, 1261, -1244, 13582, 20, -20,  16,   199,  -335, 1301, -1322, 12942,
    18, -12,  -3,   226,  -375, 1331, -1376, 12293, 15, -4,   -19,  250,  -410, 1351, -1408, 11638,
    13, 3,    -35,  272,  -439, 1361, -1419, 10979, 11, 9,    -49,  292,  -464, 1362, -1410, 10319,
    9,  16,   -63,  309,  -483, 1354, -1383, 9660,  7,  22,   -75,  322,  -496, 1337, -1339, 9005,
    6,  26,   -85,  333,  -504, 1312, -1280, 8355,  4,  31,   -94,  341,  -507, 1278, -1205, 7713,
    3,  35,   -102, 347,  -506, 1238, -1119, 7082,  1,  40,   -110, 350,  -499, 1190, -1021, 6464,
    0,  43,   -115, 350,  -488, 1136, -914,  5861,
};

typedef struct
{
    uint32_t out_index;
    int32_t values[HW_RESAMPLE_HALF_WIDTH * 2u];
} HwResampleDelta;

typedef struct
{
    uint64_t offset;
    uint32_t available;
} HwResampleEndFrame;

static int64_t arithmetic_floor_shift(int64_t value, unsigned shift)
{
    const int64_t divisor = INT64_C(1) << shift;
    const int64_t quotient = value / divisor;
    const int64_t remainder = value % divisor;
    return value < 0 && remainder != 0 ? quotient - 1 : quotient;
}

static bool int64_fits_int32(int64_t value)
{
    return value >= INT32_MIN && value <= INT32_MAX;
}

static void clear_blip(HwResampleBlip* blip)
{
    blip->offset = blip->factor / 2u;
    blip->available = 0;
    blip->integrator = 0;
    memset(blip->samples, 0, sizeof(blip->samples));
}

static void clear_antialias_history(HwResampleAntialias* aa)
{
    aa->newest = 0;
    memset(aa->history_l, 0, sizeof(aa->history_l));
    memset(aa->history_r, 0, sizeof(aa->history_r));
}

static bool factor_fits_state(const HwResample* rs, uint64_t factor, uint32_t audio_buffers)
{
    const HwResampleBlip* blips[2] = {&rs->left, &rs->right};
    const uint64_t factor_frame_limit = UINT64_MAX / HW_RESAMPLE_FRAME_CLOCKS;

    if (factor == 0 || factor > factor_frame_limit)
        return false;
    for (size_t i = 0; i < sizeof(blips) / sizeof(blips[0]); ++i)
    {
        if (blips[i]->offset >= HW_RESAMPLE_TIME_UNIT || blips[i]->available > HW_RESAMPLE_BLIP_STORAGE_SAMPLES ||
            factor > (UINT64_MAX - blips[i]->offset) / HW_RESAMPLE_FRAME_CLOCKS)
            return false;
    }

    const uint64_t frame = factor * HW_RESAMPLE_FRAME_CLOCKS;
    const uint64_t whole = frame / HW_RESAMPLE_TIME_UNIT;
    const uint64_t remainder = frame % HW_RESAMPLE_TIME_UNIT;
    const uint64_t frame_growth = whole + (remainder != 0);
    return (uint64_t)audio_buffers + frame_growth + HW_RESAMPLE_HALF_WIDTH * 2u <=
           HW_RESAMPLE_BLIP_STORAGE_SAMPLES + HW_RESAMPLE_BLIP_BUFFER_EXTRA;
}

static bool calculate_factor(const HwResample* rs, uint32_t host_rate_hz, uint64_t* factor)
{
    if (host_rate_hz == 0 || !isfinite(rs->fps_target) || rs->fps_target <= 0.0f)
        return false;

    const float faux_clock = 1.0f * (float)HW_RESAMPLE_GBA_CLOCK_HZ / ((float)280896 * rs->fps_target * 1.0f);
    const double effective_output_rate = (double)host_rate_hz * (double)faux_clock;
    const double raw_factor = (double)HW_RESAMPLE_TIME_UNIT * effective_output_rate / (double)HW_RESAMPLE_GBA_CLOCK_HZ;
    if (!isfinite(raw_factor) || raw_factor <= 0.0 || raw_factor >= (double)UINT64_MAX)
        return false;

    uint64_t next_factor = (uint64_t)raw_factor;
    if ((double)next_factor < raw_factor)
    {
        if (next_factor == UINT64_MAX)
            return false;
        ++next_factor;
    }
    if (!factor_fits_state(rs, next_factor, rs->audio_buffers))
        return false;

    *factor = next_factor;
    return true;
}

static void rebuild_antialias_coefficients(HwResampleAntialias* aa)
{
    const double pi = 3.14159265358979323846;
    const int half = (int)(HW_RESAMPLE_AA_TAPS - 1u) / 2;
    double cutoff = 1.0;
    double sum = 0.0;

    if (aa->input_rate_hz != 0 && aa->output_rate_hz != 0)
    {
        const double ratio = (double)aa->output_rate_hz / (double)aa->input_rate_hz;
        cutoff = ratio < 1.0 ? ratio : 1.0;
    }
    for (uint32_t tap = 0; tap < HW_RESAMPLE_AA_TAPS; ++tap)
    {
        const double time = (double)((int)tap - half);
        const double unit = time / (double)half;
        const double window = 0.40897 + 0.5 * cos(pi * unit) + 0.09103 * cos(2.0 * pi * unit);
        const double coefficient = time == 0.0 ? cutoff * window : sin(pi * cutoff * time) / (pi * time) * window;
        aa->coefficients[tap] = coefficient;
        sum += coefficient;
    }
    if (!isfinite(sum) || sum == 0.0)
    {
        memset(aa->coefficients, 0, sizeof(aa->coefficients));
        return;
    }
    for (uint32_t tap = 0; tap < HW_RESAMPLE_AA_TAPS; ++tap)
        aa->coefficients[tap] /= sum;
}

static double antialias_filter(const HwResampleAntialias* aa, const double* history, uint32_t newest, int16_t input)
{
    double result = 0.0;
    for (uint32_t tap = 0; tap < HW_RESAMPLE_AA_TAPS; ++tap)
    {
        uint32_t index = newest + HW_RESAMPLE_AA_TAPS - tap;
        if (index >= HW_RESAMPLE_AA_TAPS)
            index -= HW_RESAMPLE_AA_TAPS;
        const double sample = tap == 0 ? (double)input : history[index];
        result += aa->coefficients[tap] * sample;
    }
    return result;
}

/* The optional FIR has an explicit nearest-integer mapping. Ties move away
 * from zero; no implicit floating-to-integer truncation is used. */
static bool antialias_pcm16(double sample, int16_t* result)
{
    if (!isfinite(sample) || sample < (double)INT16_MIN - 0.5 || sample >= (double)INT16_MAX + 0.5)
        return false;

    const double rounded = sample >= 0.0 ? floor(sample + 0.5) : ceil(sample - 0.5);
    if (rounded < (double)INT16_MIN || rounded > (double)INT16_MAX)
        return false;
    *result = (int16_t)rounded;
    return true;
}

static bool prepare_delta(const HwResampleBlip* blip, uint64_t time, int32_t delta, HwResampleDelta* prepared)
{
    if (blip->factor == 0 || blip->available > HW_RESAMPLE_BLIP_STORAGE_SAMPLES ||
        blip->offset >= HW_RESAMPLE_TIME_UNIT || (time != 0 && blip->factor > (UINT64_MAX - blip->offset) / time))
        return false;

    const uint64_t position = time * blip->factor + blip->offset;
    const uint64_t fixed64 = position >> 32u;
    if (fixed64 > UINT32_MAX)
        return false;

    const uint32_t fixed = (uint32_t)fixed64;
    const uint32_t output_offset = fixed >> HW_RESAMPLE_FRAC_BITS;
    if (output_offset > UINT32_MAX - blip->available)
        return false;
    prepared->out_index = blip->available + output_offset;
    if (prepared->out_index > HW_RESAMPLE_BLIP_STORAGE_SAMPLES + HW_RESAMPLE_END_FRAME_EXTRA)
        return false;

    const uint32_t phase = (fixed >> (HW_RESAMPLE_DELTA_BITS)) & (HW_RESAMPLE_PHASE_COUNT - 1u);
    const uint32_t interpolation = fixed & (HW_RESAMPLE_DELTA_UNIT - 1u);
    const int32_t delta2 = (int32_t)arithmetic_floor_shift((int64_t)delta * interpolation, HW_RESAMPLE_DELTA_BITS);
    const int32_t delta1 = delta - delta2;
    const int16_t* row = &bl_step[phase * HW_RESAMPLE_HALF_WIDTH];
    const int16_t* reverse = &bl_step[(HW_RESAMPLE_PHASE_COUNT - phase) * HW_RESAMPLE_HALF_WIDTH];

    for (uint32_t i = 0; i < HW_RESAMPLE_HALF_WIDTH; ++i)
    {
        const int64_t first = (int64_t)row[i] * delta1 + (int64_t)row[HW_RESAMPLE_HALF_WIDTH + i] * delta2;
        const int64_t second =
            (int64_t)reverse[HW_RESAMPLE_HALF_WIDTH - 1u - i] * delta1 + (int64_t)reverse[-1 - (int)i] * delta2;
        const int64_t first_value = (int64_t)blip->samples[prepared->out_index + i] + first;
        const int64_t second_value = (int64_t)blip->samples[prepared->out_index + HW_RESAMPLE_HALF_WIDTH + i] + second;
        if (!int64_fits_int32(first_value) || !int64_fits_int32(second_value))
            return false;
        prepared->values[i] = (int32_t)first_value;
        prepared->values[HW_RESAMPLE_HALF_WIDTH + i] = (int32_t)second_value;
    }
    return true;
}

static void commit_delta(HwResampleBlip* blip, const HwResampleDelta* prepared)
{
    for (uint32_t i = 0; i < HW_RESAMPLE_HALF_WIDTH * 2u; ++i)
        blip->samples[prepared->out_index + i] = prepared->values[i];
}

static bool prepare_end_frame(const HwResampleBlip* blip, HwResampleEndFrame* prepared)
{
    if (blip->factor == 0 || blip->available > HW_RESAMPLE_BLIP_STORAGE_SAMPLES ||
        blip->offset >= HW_RESAMPLE_TIME_UNIT || blip->factor > (UINT64_MAX - blip->offset) / HW_RESAMPLE_FRAME_CLOCKS)
        return false;

    const uint64_t offset = blip->factor * HW_RESAMPLE_FRAME_CLOCKS + blip->offset;
    const uint64_t added = offset >> HW_RESAMPLE_TIME_BITS;
    if (added > HW_RESAMPLE_BLIP_STORAGE_SAMPLES - blip->available)
        return false;
    prepared->offset = offset & (HW_RESAMPLE_TIME_UNIT - 1u);
    prepared->available = blip->available + (uint32_t)added;
    return true;
}

static bool next_pcm16(const HwResampleBlip* blip, uint32_t index, int16_t* sample, int32_t* next_integrator)
{
    const int32_t integrator = blip->integrator;
    int32_t output = (int32_t)arithmetic_floor_shift(integrator, HW_RESAMPLE_DELTA_BITS);
    const int64_t summed = (int64_t)integrator + blip->samples[index];
    if (!int64_fits_int32(summed))
        return false;

    if (output < INT16_MIN || output > INT16_MAX)
        output = (int32_t)(arithmetic_floor_shift(output, 16u) ^ INT16_MAX);
    const int64_t feedback = (int64_t)output * (INT64_C(1) << HW_RESAMPLE_FEEDBACK_SHIFT);
    const int64_t updated = summed - feedback;
    if (!int64_fits_int32(updated))
        return false;

    *sample = (int16_t)output;
    *next_integrator = (int32_t)updated;
    return true;
}

static bool remove_samples(HwResampleBlip* blip, uint32_t count)
{
    if (count > blip->available || blip->available > HW_RESAMPLE_BLIP_STORAGE_SAMPLES)
        return false;

    const uint32_t remaining = blip->available + HW_RESAMPLE_BLIP_BUFFER_EXTRA - count;
    memmove(&blip->samples[0], &blip->samples[count], (size_t)remaining * sizeof(blip->samples[0]));
    memset(&blip->samples[remaining], 0, (size_t)count * sizeof(blip->samples[0]));
    blip->available -= count;
    return true;
}

void hw_resample_init(HwResample* rs, uint32_t host_rate_hz, float fps_target, uint32_t audio_buffers)
{
    if (rs == NULL)
        return;

    memset(rs, 0, sizeof(*rs));
    rs->audio_buffers = audio_buffers > HW_RESAMPLE_MAX_AUDIO_BUFFERS ? HW_RESAMPLE_MAX_AUDIO_BUFFERS : audio_buffers;
    rs->host_rate_hz = host_rate_hz;
    rs->fps_target = fps_target;
    rs->aa.input_rate_hz = HW_RESAMPLE_DEFAULT_AA_INPUT_RATE_HZ;
    rs->aa.output_rate_hz = host_rate_hz;

    uint64_t factor;
    if (calculate_factor(rs, host_rate_hz, &factor))
    {
        rs->left.factor = factor;
        rs->right.factor = factor;
    }
    clear_blip(&rs->left);
    clear_blip(&rs->right);
}

void hw_resample_set_antialias_input_rate(HwResample* rs, uint32_t dac_rate_hz)
{
    if (rs == NULL || dac_rate_hz == 0)
        return;

    rs->aa.input_rate_hz = dac_rate_hz;
    if (rs->aa.enabled)
        rebuild_antialias_coefficients(&rs->aa);
}

void hw_resample_reset(HwResample* rs)
{
    if (rs == NULL)
        return;

    clear_blip(&rs->left);
    clear_blip(&rs->right);
    rs->clock = 0;
    if (rs->aa.enabled)
        clear_antialias_history(&rs->aa);
}

void hw_resample_set_output_rate(HwResample* rs, uint32_t host_rate_hz)
{
    if (rs == NULL)
        return;

    uint64_t factor;
    if (!calculate_factor(rs, host_rate_hz, &factor))
        return;

    rs->left.factor = factor;
    rs->right.factor = factor;
    rs->host_rate_hz = host_rate_hz;
    rs->aa.output_rate_hz = host_rate_hz;
    if (rs->aa.enabled)
        rebuild_antialias_coefficients(&rs->aa);
}

void hw_resample_set_audio_buffers(HwResample* rs, uint32_t audio_buffers)
{
    if (rs == NULL)
        return;

    const uint32_t next_buffers =
        audio_buffers > HW_RESAMPLE_MAX_AUDIO_BUFFERS ? HW_RESAMPLE_MAX_AUDIO_BUFFERS : audio_buffers;
    if (rs->left.factor != 0 && !factor_fits_state(rs, rs->left.factor, next_buffers))
        return;
    rs->audio_buffers = next_buffers;
    clear_blip(&rs->left);
    clear_blip(&rs->right);
    rs->clock = 0;
}

void hw_resample_submit(HwResample* rs, int16_t left, int16_t right, uint32_t dac_period_cycles)
{
    if (rs == NULL || rs->left.available >= rs->audio_buffers ||
        rs->left.available > HW_RESAMPLE_BLIP_STORAGE_SAMPLES ||
        rs->right.available > HW_RESAMPLE_BLIP_STORAGE_SAMPLES || rs->left.factor == 0 || rs->right.factor == 0 ||
        dac_period_cycles > UINT64_MAX - rs->clock)
        return;

    const uint64_t next_clock = rs->clock + dac_period_cycles;
    HwResampleEndFrame left_end;
    HwResampleEndFrame right_end;
    const bool end_frame = next_clock >= HW_RESAMPLE_FRAME_CLOCKS;
    if (end_frame && (!prepare_end_frame(&rs->left, &left_end) || !prepare_end_frame(&rs->right, &right_end)))
        return;

    int16_t submitted_left = left;
    int16_t submitted_right = right;
    uint32_t aa_newest = 0;
    if (rs->aa.enabled)
    {
        aa_newest = rs->aa.newest + 1u;
        if (aa_newest == HW_RESAMPLE_AA_TAPS)
            aa_newest = 0;
        if (!antialias_pcm16(antialias_filter(&rs->aa, rs->aa.history_l, aa_newest, left), &submitted_left) ||
            !antialias_pcm16(antialias_filter(&rs->aa, rs->aa.history_r, aa_newest, right), &submitted_right))
            return;
    }

    HwResampleDelta left_delta;
    HwResampleDelta right_delta;
    if (!prepare_delta(&rs->left, rs->clock, (int32_t)submitted_left - rs->last_l, &left_delta) ||
        !prepare_delta(&rs->right, rs->clock, (int32_t)submitted_right - rs->last_r, &right_delta))
        return;

    commit_delta(&rs->left, &left_delta);
    commit_delta(&rs->right, &right_delta);
    if (rs->aa.enabled)
    {
        rs->aa.history_l[aa_newest] = (double)left;
        rs->aa.history_r[aa_newest] = (double)right;
        rs->aa.newest = aa_newest;
    }
    rs->last_l = submitted_left;
    rs->last_r = submitted_right;
    rs->clock = next_clock;
    if (end_frame)
    {
        rs->left.offset = left_end.offset;
        rs->left.available = left_end.available;
        rs->right.offset = right_end.offset;
        rs->right.available = right_end.available;
        rs->clock -= HW_RESAMPLE_FRAME_CLOCKS;
    }
}

uint32_t hw_resample_read_pcm16(HwResample* rs, int16_t* left, int16_t* right, uint32_t max_frames)
{
    if (rs == NULL || left == NULL || right == NULL || rs->left.available > HW_RESAMPLE_BLIP_STORAGE_SAMPLES ||
        rs->right.available > HW_RESAMPLE_BLIP_STORAGE_SAMPLES)
        return 0;

    uint32_t count = max_frames;
    if (count > rs->left.available)
        count = rs->left.available;
    if (count > rs->right.available)
        count = rs->right.available;

    uint32_t read = 0;
    for (; read < count; ++read)
    {
        int16_t left_sample;
        int16_t right_sample;
        int32_t left_integrator;
        int32_t right_integrator;
        if (!next_pcm16(&rs->left, read, &left_sample, &left_integrator) ||
            !next_pcm16(&rs->right, read, &right_sample, &right_integrator))
            break;

        left[read] = left_sample;
        right[read] = right_sample;
        rs->left.integrator = left_integrator;
        rs->right.integrator = right_integrator;
    }
    if (read == 0)
        return 0;
    if (!remove_samples(&rs->left, read) || !remove_samples(&rs->right, read))
        return 0;
    return read;
}

void hw_resample_set_antialias(HwResample* rs, bool enabled)
{
    if (rs == NULL || rs->aa.enabled == enabled)
        return;

    rs->aa.enabled = enabled;
    clear_antialias_history(&rs->aa);
    if (enabled)
        rebuild_antialias_coefficients(&rs->aa);
}
