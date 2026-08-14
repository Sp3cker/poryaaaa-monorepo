#include "hw_audio/hw_resample.h"
#include "test_assert.h"

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* This file deliberately declares the renamed 0.10.5 interface itself. It
 * must not inherit mGBA headers, feature definitions, or implementation code. */
typedef struct blip_t blip_t;

blip_t* poryaaaa_mgba0105_blip_new(int sample_count);
void poryaaaa_mgba0105_blip_set_rates(blip_t* blip, double clock_rate, double sample_rate);
void poryaaaa_mgba0105_blip_clear(blip_t* blip);
void poryaaaa_mgba0105_blip_add_delta(blip_t* blip, unsigned int clock_time, int delta);
void poryaaaa_mgba0105_blip_end_frame(blip_t* blip, unsigned int clock_duration);
int poryaaaa_mgba0105_blip_samples_avail(const blip_t* blip);
int poryaaaa_mgba0105_blip_read_samples(blip_t* blip, short out[], int count, int stereo);
void poryaaaa_mgba0105_blip_delete(blip_t* blip);

enum
{
    PARITY_HOST_RATE_HZ = 48000,
    PARITY_AUDIO_BUFFERS = 1536,
    PARITY_OUTPUT_CAPACITY = 4096,
    PARITY_PHASE_VALUES = 32,
};

static const float kFpsTarget = 59.72750056960583f;

static const int16_t kPhaseValues[PARITY_PHASE_VALUES] = {
    0,    4096,  -4096, 12000,  -12000, 32767,  -32768, 1,    -1,    24576, -24576,
    8191, -8192, 16384, -16384, 0,      32767,  -32768, 1234, -5678, 22222, -22222,
    73,   -73,   30000, -30000, 15123,  -15123, 7,      -7,   2048,  -2048,
};

typedef struct
{
    blip_t* left;
    blip_t* right;
    int16_t last_l;
    int16_t last_r;
    uint64_t clock;
    uint32_t audio_buffers;
    double host_rate_hz;
    float fps_target;
} RefFrontend;

typedef struct
{
    RefFrontend reference;
    HwResample production;
    short reference_chunk[PARITY_OUTPUT_CAPACITY * 2];
    int16_t production_left[PARITY_OUTPUT_CAPACITY];
    int16_t production_right[PARITY_OUTPUT_CAPACITY];
    int16_t reference_stream[PARITY_OUTPUT_CAPACITY * 2];
    int16_t production_stream[PARITY_OUTPUT_CAPACITY * 2];
    uint32_t reference_count;
    uint32_t production_count;
} FrontendParityRun;

static void ref_frontend_set_output_rate(RefFrontend* reference, double host_rate_hz)
{
    const float faux = 1.0f * (float)HW_RESAMPLE_GBA_CLOCK_HZ / ((float)280896 * reference->fps_target * 1.0f);
    const double effective_output_rate = host_rate_hz * (double)faux;

    reference->host_rate_hz = host_rate_hz;
    poryaaaa_mgba0105_blip_set_rates(reference->left, (double)HW_RESAMPLE_GBA_CLOCK_HZ, effective_output_rate);
    poryaaaa_mgba0105_blip_set_rates(reference->right, (double)HW_RESAMPLE_GBA_CLOCK_HZ, effective_output_rate);
}

static void ref_frontend_init(RefFrontend* reference, double host_rate_hz, float fps_target, uint32_t audio_buffers)
{
    memset(reference, 0, sizeof(*reference));
    reference->left = poryaaaa_mgba0105_blip_new(HW_RESAMPLE_BLIP_STORAGE_SAMPLES);
    reference->right = poryaaaa_mgba0105_blip_new(HW_RESAMPLE_BLIP_STORAGE_SAMPLES);
    if (!reference->left || !reference->right)
    {
        fputs("unable to allocate mGBA 0.10.5 blip reference\n", stderr);
        exit(1);
    }

    reference->audio_buffers = audio_buffers;
    reference->fps_target = fps_target;
    ref_frontend_set_output_rate(reference, host_rate_hz);
    poryaaaa_mgba0105_blip_clear(reference->left);
    poryaaaa_mgba0105_blip_clear(reference->right);
}

static void ref_frontend_deinit(RefFrontend* reference)
{
    poryaaaa_mgba0105_blip_delete(reference->left);
    poryaaaa_mgba0105_blip_delete(reference->right);
    memset(reference, 0, sizeof(*reference));
}

static uint32_t ref_frontend_available(const RefFrontend* reference)
{
    return (uint32_t)poryaaaa_mgba0105_blip_samples_avail(reference->left);
}

static void ref_frontend_submit(RefFrontend* reference, int16_t left, int16_t right, uint32_t dac_period_cycles)
{
    if (ref_frontend_available(reference) >= reference->audio_buffers)
        return;

    poryaaaa_mgba0105_blip_add_delta(reference->left, (unsigned int)reference->clock, (int)left - reference->last_l);
    poryaaaa_mgba0105_blip_add_delta(reference->right, (unsigned int)reference->clock, (int)right - reference->last_r);
    reference->last_l = left;
    reference->last_r = right;
    reference->clock += dac_period_cycles;
    if (reference->clock >= HW_RESAMPLE_FRAME_CLOCKS)
    {
        poryaaaa_mgba0105_blip_end_frame(reference->left, HW_RESAMPLE_FRAME_CLOCKS);
        poryaaaa_mgba0105_blip_end_frame(reference->right, HW_RESAMPLE_FRAME_CLOCKS);
        reference->clock -= HW_RESAMPLE_FRAME_CLOCKS;
    }
}

static uint32_t ref_frontend_read(RefFrontend* reference, short* interleaved, uint32_t max_frames)
{
    uint32_t frames = ref_frontend_available(reference);
    const uint32_t right_available = (uint32_t)poryaaaa_mgba0105_blip_samples_avail(reference->right);
    if (right_available < frames)
        frames = right_available;
    if (max_frames < frames)
        frames = max_frames;

    const int left_read = poryaaaa_mgba0105_blip_read_samples(reference->left, interleaved, (int)frames, 1);
    const int right_read = poryaaaa_mgba0105_blip_read_samples(reference->right, interleaved + 1, (int)frames, 1);
    ASSERT_EQ(left_read, (int)frames, "reference left read returns the requested paired count");
    ASSERT_EQ(right_read, (int)frames, "reference right read returns the requested paired count");
    return frames;
}

static void ref_frontend_reset(RefFrontend* reference)
{
    poryaaaa_mgba0105_blip_clear(reference->left);
    poryaaaa_mgba0105_blip_clear(reference->right);
    reference->clock = 0;
}

static void parity_run_init(FrontendParityRun* run)
{
    memset(run, 0, sizeof(*run));
    ref_frontend_init(&run->reference, PARITY_HOST_RATE_HZ, kFpsTarget, PARITY_AUDIO_BUFFERS);
    hw_resample_init(&run->production, PARITY_HOST_RATE_HZ, kFpsTarget, PARITY_AUDIO_BUFFERS);
}

static void parity_run_deinit(FrontendParityRun* run)
{
    ref_frontend_deinit(&run->reference);
}

static void parity_run_assert_availability(const FrontendParityRun* run)
{
    ASSERT(run->production.left.available == ref_frontend_available(&run->reference),
           "left production availability matches the independent reference");
    ASSERT(run->production.right.available == (uint32_t)poryaaaa_mgba0105_blip_samples_avail(run->reference.right),
           "right production availability matches the independent reference");
}

static void parity_run_submit(FrontendParityRun* run, int16_t left, int16_t right, uint32_t dac_period_cycles)
{
    ref_frontend_submit(&run->reference, left, right, dac_period_cycles);
    hw_resample_submit(&run->production, left, right, dac_period_cycles);
    parity_run_assert_availability(run);
}

static void parity_run_read(FrontendParityRun* run, uint32_t max_frames)
{
    ASSERT(max_frames <= PARITY_OUTPUT_CAPACITY, "parity read fits the test scratch buffers");
    if (max_frames > PARITY_OUTPUT_CAPACITY)
        return;

    const uint32_t reference_read = ref_frontend_read(&run->reference, run->reference_chunk, max_frames);
    const uint32_t production_read =
        hw_resample_read_pcm16(&run->production, run->production_left, run->production_right, max_frames);
    ASSERT(production_read == reference_read, "production and reference return the exact same PCM16 frame count");

    if (run->reference_count + reference_read > PARITY_OUTPUT_CAPACITY ||
        run->production_count + production_read > PARITY_OUTPUT_CAPACITY)
    {
        ASSERT(0, "parity stream fits the fixed test output storage");
        return;
    }

    for (uint32_t i = 0; i < reference_read; ++i)
    {
        run->reference_stream[(run->reference_count + i) * 2] = run->reference_chunk[i * 2];
        run->reference_stream[(run->reference_count + i) * 2 + 1] = run->reference_chunk[i * 2 + 1];
    }
    for (uint32_t i = 0; i < production_read; ++i)
    {
        run->production_stream[(run->production_count + i) * 2] = run->production_left[i];
        run->production_stream[(run->production_count + i) * 2 + 1] = run->production_right[i];
    }
    run->reference_count += reference_read;
    run->production_count += production_read;

    const uint32_t compared = reference_read < production_read ? reference_read : production_read;
    ASSERT(memcmp(run->reference_chunk,
                  run->production_stream + (run->production_count - production_read) * 2,
                  (size_t)compared * 2 * sizeof(int16_t)) == 0,
           "every intermediate paired PCM16 read is byte-exact");
    parity_run_assert_availability(run);
}

static void parity_run_assert_stream(const FrontendParityRun* run, const char* message)
{
    ASSERT(run->production_count == run->reference_count,
           "production and reference retain the same final PCM16 frame count");
    if (run->production_count == run->reference_count)
    {
        ASSERT(memcmp(run->production_stream,
                      run->reference_stream,
                      (size_t)run->production_count * 2 * sizeof(int16_t)) == 0,
               message);
    }
}

static void parity_run_clear_stream(FrontendParityRun* run)
{
    run->reference_count = 0;
    run->production_count = 0;
}

static void parity_run_drain(FrontendParityRun* run, const uint32_t* caps, uint32_t cap_count)
{
    uint32_t reads = 0;
    while (ref_frontend_available(&run->reference) != 0 || run->production.left.available != 0)
    {
        ASSERT(reads < PARITY_OUTPUT_CAPACITY, "parity drain makes forward progress");
        if (reads >= PARITY_OUTPUT_CAPACITY)
            return;
        parity_run_read(run, caps[reads % cap_count]);
        ++reads;
    }
    parity_run_assert_stream(run, "final interleaved PCM16 stream is byte-exact");
}

static void parity_run_reset(FrontendParityRun* run)
{
    ref_frontend_reset(&run->reference);
    hw_resample_reset(&run->production);
}

static void submit_constant(FrontendParityRun* run, int16_t left, int16_t right, uint32_t count, uint32_t period)
{
    for (uint32_t i = 0; i < count; ++i)
        parity_run_submit(run, left, right, period);
}

static int16_t safe_negate(int16_t sample)
{
    return sample == INT16_MIN ? INT16_MAX : (int16_t)-sample;
}

static int stream_contains(const int16_t* stream, uint32_t frames, int16_t sample)
{
    for (uint32_t i = 0; i < frames * 2; ++i)
    {
        if (stream[i] == sample)
            return 1;
    }
    return 0;
}

static int stream_tail_is_nonzero(const int16_t* stream, uint32_t frames, uint32_t tail_frames)
{
    if (tail_frames > frames)
        tail_frames = frames;
    for (uint32_t i = (frames - tail_frames) * 2; i < frames * 2; ++i)
    {
        if (stream[i] != 0)
            return 1;
    }
    return 0;
}
static uint32_t submit_optional_antialias(HwResample* resample,
                                          const int16_t* input_l,
                                          const int16_t* input_r,
                                          uint32_t input_count,
                                          int16_t* output_l,
                                          int16_t* output_r)
{
    uint32_t output_count = 0;
    for (uint32_t input = 0; input < input_count; ++input)
    {
        hw_resample_submit(resample, input_l[input], input_r[input], 256);
        while (resample->left.available != 0 || resample->right.available != 0)
        {
            ASSERT(output_count < PARITY_OUTPUT_CAPACITY, "optional AA output fits fixed test storage");
            if (output_count >= PARITY_OUTPUT_CAPACITY)
                return output_count;
            const uint32_t read = hw_resample_read_pcm16(
                resample, output_l + output_count, output_r + output_count, PARITY_OUTPUT_CAPACITY - output_count);
            ASSERT(read != 0, "optional AA drains every paired frontend frame");
            if (read == 0)
                return output_count;
            output_count += read;
        }
    }
    return output_count;
}

static int pcm_tail_peak_abs(const int16_t* samples, uint32_t frames)
{
    int peak = 0;
    for (uint32_t frame = frames / 2; frame < frames; ++frame)
    {
        const int value = samples[frame];
        const int magnitude = value < 0 ? -value : value;
        if (magnitude > peak)
            peak = magnitude;
    }
    return peak;
}

static void test_arm64_factor_clear_and_rollover(void)
{
    printf("Testing frontend parity: arm64 factor, clear, and rollover...\n");
    static const int16_t first_frame[] = {0, 4096, -4096, 12000, -12000, 32767, -32768, 1};
    const uint32_t caps[] = {1, 3, 7, 2, 11, 5};
    FrontendParityRun run;
    parity_run_init(&run);

    ASSERT(run.production.left.factor == UINT64_C(12884901888000), "48 kHz installed factor uses 64-bit fixed timing");
    ASSERT(run.production.right.factor == UINT64_C(12884901888000),
           "both channels start with the installed 64-bit factor");
    ASSERT(run.production.left.offset == UINT64_C(6442450944000),
           "clear initializes left offset to factor divided by two");
    ASSERT(run.production.right.offset == UINT64_C(6442450944000),
           "clear initializes right offset to factor divided by two");

    for (uint32_t i = 0; i < sizeof(first_frame) / sizeof(first_frame[0]); ++i)
        parity_run_submit(&run, first_frame[i], kPhaseValues[(i * 7 + 3) & 31], 256);

    ASSERT(run.production.clock == 0, "first complete frame rolls the shared clock to zero");
    ASSERT(run.production.left.available == 5, "first rollover makes five samples available");
    ASSERT(run.production.right.available == 5, "first rollover keeps paired availability");
    ASSERT(run.production.left.offset == UINT64_C(3876723380715520),
           "first rollover retains the installed fractional left offset");
    ASSERT(run.production.right.offset == UINT64_C(3876723380715520),
           "first rollover retains the installed fractional right offset");

    for (uint32_t i = 8; i < 16; ++i)
        parity_run_submit(&run, kPhaseValues[i], kPhaseValues[(i * 7 + 3) & 31], 256);

    ASSERT(run.production.clock == 0, "second complete frame also performs one rollover subtraction");
    ASSERT(run.production.left.available == 11, "second rollover makes eleven samples available");
    ASSERT(run.production.right.available == 11, "second rollover remains stereo paired");
    ASSERT(run.production.left.offset == UINT64_C(3243404683116544),
           "second rollover preserves the source fractional offset");
    ASSERT(run.production.right.offset == UINT64_C(3243404683116544),
           "second rollover preserves the right fractional offset");

    parity_run_drain(&run, caps, (uint32_t)(sizeof(caps) / sizeof(caps[0])));
    parity_run_deinit(&run);
}

static void test_factor_uses_production_legacy_ceiling_path(void)
{
    printf("Testing frontend parity: production nonintegral faux-clock factor...\n");
    HwResample production;
    hw_resample_init(&production, 48000u, 59.0f, PARITY_AUDIO_BUFFERS);

    ASSERT(production.host_rate_hz == 48000u, "production factor accepts the integral public host-rate API");
    ASSERT(production.left.factor == UINT64_C(13043779584000),
           "production factor retains the nonintegral Qt faux-clock ratio");
    ASSERT(production.right.factor == UINT64_C(13043779584000),
           "production applies the ceiling factor to both PCM channels");
    ASSERT(production.left.offset == UINT64_C(6521889792000), "production clear derives the left factor-half offset");
    ASSERT(production.right.offset == UINT64_C(6521889792000), "production clear derives the right factor-half offset");
}

static void test_all_phases_delta_interpolation_table_stereo_and_short_chunks(void)
{
    printf("Testing frontend parity: all phases, stereo, and short chunks...\n");
    enum
    {
        INPUT_COUNT = 88,
    };
    static const uint32_t partitions[] = {1, 7, 8, 3, 5, 8, 1, 15, 40};
    static const uint32_t caps[] = {1, 3, 7, 2, 11, 5};
    int16_t left[INPUT_COUNT];
    int16_t right[INPUT_COUNT];
    for (uint32_t i = 0; i < INPUT_COUNT; ++i)
    {
        left[i] = i < PARITY_PHASE_VALUES ? kPhaseValues[i] : 0;
        right[i] = i < PARITY_PHASE_VALUES ? kPhaseValues[(7 * i + 3) & 31] : 0;
    }

    FrontendParityRun one_chunk;
    parity_run_init(&one_chunk);
    for (uint32_t i = 0; i < INPUT_COUNT; ++i)
        parity_run_submit(&one_chunk, left[i], right[i], 256);
    parity_run_drain(&one_chunk, caps, (uint32_t)(sizeof(caps) / sizeof(caps[0])));

    FrontendParityRun partitioned;
    parity_run_init(&partitioned);
    uint32_t input = 0;
    uint32_t cap = 0;
    for (uint32_t partition = 0; partition < sizeof(partitions) / sizeof(partitions[0]); ++partition)
    {
        for (uint32_t i = 0; i < partitions[partition]; ++i)
            parity_run_submit(&partitioned, left[input + i], right[input + i], 256);
        input += partitions[partition];
        parity_run_read(&partitioned, caps[cap++ % (sizeof(caps) / sizeof(caps[0]))]);
    }
    ASSERT(input == INPUT_COUNT, "phase partitions cover exactly the 88 prescribed inputs");
    parity_run_drain(&partitioned, caps, (uint32_t)(sizeof(caps) / sizeof(caps[0])));

    ASSERT(one_chunk.production_count == partitioned.production_count,
           "one chunk and prescribed short partitions have the same PCM16 count");
    if (one_chunk.production_count == partitioned.production_count)
    {
        ASSERT(memcmp(one_chunk.production_stream,
                      partitioned.production_stream,
                      (size_t)one_chunk.production_count * 2 * sizeof(int16_t)) == 0,
               "one chunk and prescribed short partitions have the same interleaved PCM16 bytes");
    }
    parity_run_deinit(&partitioned);
    parity_run_deinit(&one_chunk);
}

static void test_integer_clamp_and_six_bit_feedback(void)
{
    printf("Testing frontend parity: integer clamp and six-bit feedback...\n");
    static const uint32_t caps[] = {1, 1, 4, 2, 7, 3};
    FrontendParityRun run;
    parity_run_init(&run);

    for (uint32_t i = 0; i < 8 + 64 + 64 + 64 + 64; ++i)
    {
        int16_t sample;
        if (i < 8)
            sample = INT16_MIN;
        else if (i < 72)
            sample = INT16_MAX;
        else if (i < 136)
            sample = INT16_MIN;
        else if (i < 200)
            sample = INT16_MAX;
        else
            sample = 0;
        parity_run_submit(&run, sample, safe_negate(sample), 256);
    }

    for (uint32_t i = 0; i < sizeof(caps) / sizeof(caps[0]); ++i)
        parity_run_read(&run, caps[i]);
    parity_run_read(&run, PARITY_OUTPUT_CAPACITY);
    parity_run_assert_stream(&run, "clamp and feedback stream is byte-exact");

    ASSERT(stream_contains(run.production_stream, run.production_count, INT16_MIN),
           "fixed blip clamp emits signed PCM16 minimum");
    ASSERT(stream_contains(run.production_stream, run.production_count, INT16_MAX),
           "fixed blip clamp emits signed PCM16 maximum");
    ASSERT(stream_tail_is_nonzero(run.production_stream, run.production_count, 16),
           "six-bit feedback leaves a nonzero post-transition tail");
    parity_run_deinit(&run);
}

static void test_saturation_gate_drops_samples_without_advancing_delta_history(void)
{
    printf("Testing frontend parity: saturation gate and delta history...\n");
    const uint32_t caps[] = {6, 1, 3, 7, 2, 11, 5};
    FrontendParityRun run;
    parity_run_init(&run);

    for (uint32_t frame = 0; frame < 263; ++frame)
        submit_constant(&run, -12000, 9000, 8, 256);

    ASSERT(run.production.left.available == 1541,
           "263 complete frames reach the prescribed producer-gate availability");
    ASSERT(run.production.right.available == 1541, "producer gate keeps stereo availability paired");
    ASSERT(run.production.clock == 0, "complete producer-gate frames leave the shared clock at zero");
    ASSERT(run.production.last_l == -12000, "accepted input establishes left delta history");
    ASSERT(run.production.last_r == 9000, "accepted input establishes right delta history");

    for (uint32_t i = 0; i < 17; ++i)
    {
        const int16_t left = (i & 1) ? -30000 : 30000;
        const int16_t right = (i & 1) ? 30000 : -30000;
        parity_run_submit(&run, left, right, 256);
    }
    ASSERT(run.production.left.available == 1541, "rejected inputs do not grow left availability");
    ASSERT(run.production.right.available == 1541, "rejected inputs do not grow right availability");
    ASSERT(run.production.clock == 0, "rejected inputs do not advance the shared clock");
    ASSERT(run.production.last_l == -12000, "rejected inputs do not alter left delta history");
    ASSERT(run.production.last_r == 9000, "rejected inputs do not alter right delta history");

    parity_run_read(&run, caps[0]);
    const int16_t previous_l = run.production.last_l;
    const int16_t previous_r = run.production.last_r;
    submit_constant(&run, 12000, -9000, 8, 256);
    ASSERT((int)run.production.last_l - previous_l == 24000,
           "first resumed left input deposits the required +24000 delta");
    ASSERT((int)run.production.last_r - previous_r == -18000,
           "first resumed right input deposits the required -18000 delta");

    parity_run_drain(&run, caps + 1, (uint32_t)(sizeof(caps) / sizeof(caps[0]) - 1));
    parity_run_deinit(&run);
}

static void test_reset_matches_blip_clear_and_preserves_last_pcm_history(void)
{
    printf("Testing frontend parity: reset clear and retained PCM history...\n");
    const uint32_t caps[] = {1, 3, 7, 2, 11, 5};
    FrontendParityRun reset_run;
    FrontendParityRun fresh_run;
    parity_run_init(&reset_run);
    parity_run_init(&fresh_run);

    submit_constant(&reset_run, 12000, -9000, 8, 256);
    parity_run_drain(&reset_run, caps, (uint32_t)(sizeof(caps) / sizeof(caps[0])));
    parity_run_reset(&reset_run);
    parity_run_clear_stream(&reset_run);

    ASSERT(reset_run.production.left.available == 0, "reset clears left availability");
    ASSERT(reset_run.production.right.available == 0, "reset clears right availability");
    ASSERT(reset_run.production.left.integrator == 0, "reset clears left integrator");
    ASSERT(reset_run.production.right.integrator == 0, "reset clears right integrator");
    ASSERT(reset_run.production.clock == 0, "reset clears the shared clock");
    ASSERT(reset_run.production.left.offset == reset_run.production.left.factor / 2,
           "reset restores left factor-half offset");
    ASSERT(reset_run.production.right.offset == reset_run.production.right.factor / 2,
           "reset restores right factor-half offset");
    ASSERT(reset_run.production.last_l == 12000, "reset retains left PCM delta history");
    ASSERT(reset_run.production.last_r == -9000, "reset retains right PCM delta history");

    submit_constant(&reset_run, 12000, -9000, 8, 256);
    submit_constant(&fresh_run, 12000, -9000, 8, 256);
    parity_run_read(&reset_run, PARITY_OUTPUT_CAPACITY);
    parity_run_read(&fresh_run, PARITY_OUTPUT_CAPACITY);
    parity_run_assert_stream(&reset_run, "post-reset zero-delta frame is byte-exact");
    parity_run_assert_stream(&fresh_run, "fresh frame is byte-exact");

    ASSERT(!stream_tail_is_nonzero(reset_run.production_stream, reset_run.production_count, reset_run.production_count),
           "identical input after reset is a zero-delta PCM16 frame");
    ASSERT(stream_tail_is_nonzero(fresh_run.production_stream, fresh_run.production_count, fresh_run.production_count),
           "identical input on a fresh frontend is not a zero-delta frame");

    parity_run_clear_stream(&reset_run);
    parity_run_clear_stream(&fresh_run);
    submit_constant(&reset_run, -12000, 9000, 8, 256);
    submit_constant(&fresh_run, -12000, 9000, 8, 256);
    parity_run_drain(&reset_run, caps, (uint32_t)(sizeof(caps) / sizeof(caps[0])));
    parity_run_drain(&fresh_run, caps, (uint32_t)(sizeof(caps) / sizeof(caps[0])));
    parity_run_deinit(&fresh_run);
    parity_run_deinit(&reset_run);
}

static void test_host_rate_and_soundbias_changes_keep_the_legacy_timeline(void)
{
    printf("Testing frontend parity: host-rate and SOUNDBIAS timeline...\n");
    static const uint32_t caps[] = {2, 1, 8, 3, 5};
    FrontendParityRun rate_run;
    parity_run_init(&rate_run);

    submit_constant(&rate_run, 12000, -9000, 8, 256);
    parity_run_read(&rate_run, 5);
    ASSERT(rate_run.production.left.integrator != 0 || rate_run.production.right.integrator != 0,
           "host-rate change starts from a live blip integrator");

    const uint64_t clock = rate_run.production.clock;
    const uint64_t offset_l = rate_run.production.left.offset;
    const uint64_t offset_r = rate_run.production.right.offset;
    const uint32_t available_l = rate_run.production.left.available;
    const uint32_t available_r = rate_run.production.right.available;
    const int32_t integrator_l = rate_run.production.left.integrator;
    const int32_t integrator_r = rate_run.production.right.integrator;
    const int16_t last_l = rate_run.production.last_l;
    const int16_t last_r = rate_run.production.last_r;
    const uint64_t old_factor = rate_run.production.left.factor;

    ref_frontend_set_output_rate(&rate_run.reference, 44100.0);
    hw_resample_set_output_rate(&rate_run.production, 44100);
    ASSERT(rate_run.production.left.factor == UINT64_C(11838003609600),
           "integral 44.1 kHz host-rate change installs the expected left factor");
    ASSERT(rate_run.production.right.factor == UINT64_C(11838003609600),
           "integral 44.1 kHz host-rate change installs the expected right factor");
    ASSERT(rate_run.production.left.factor != old_factor, "host-rate change actually changes the factor");
    ASSERT(rate_run.production.clock == clock, "host-rate change does not reset the shared clock");
    ASSERT(rate_run.production.left.offset == offset_l, "host-rate change retains left fractional offset");
    ASSERT(rate_run.production.right.offset == offset_r, "host-rate change retains right fractional offset");
    ASSERT(rate_run.production.left.available == available_l, "host-rate change retains left availability");
    ASSERT(rate_run.production.right.available == available_r, "host-rate change retains right availability");
    ASSERT(rate_run.production.left.integrator == integrator_l, "host-rate change retains left integrator");
    ASSERT(rate_run.production.right.integrator == integrator_r, "host-rate change retains right integrator");
    ASSERT(rate_run.production.last_l == last_l, "host-rate change retains left PCM history");
    ASSERT(rate_run.production.last_r == last_r, "host-rate change retains right PCM history");
    parity_run_assert_availability(&rate_run);

    for (uint32_t i = 0; i < 64; ++i)
    {
        parity_run_submit(&rate_run, kPhaseValues[i & 31], kPhaseValues[(7 * i + 3) & 31], 256);
        parity_run_read(&rate_run, caps[i % (sizeof(caps) / sizeof(caps[0]))]);
    }
    parity_run_drain(&rate_run, caps, (uint32_t)(sizeof(caps) / sizeof(caps[0])));
    parity_run_deinit(&rate_run);

    FrontendParityRun cadence_run;
    parity_run_init(&cadence_run);
    const uint64_t factor = cadence_run.production.left.factor;
    static const struct
    {
        uint32_t count;
        uint32_t period;
    } segments[] = {
        {16, 256},
        {32, 64},
        {16, 128},
        {8, 512},
    };
    uint32_t input = 0;
    uint32_t cap = 0;
    for (uint32_t segment = 0; segment < sizeof(segments) / sizeof(segments[0]); ++segment)
    {
        for (uint32_t i = 0; i < segments[segment].count; ++i)
        {
            parity_run_submit(
                &cadence_run, kPhaseValues[input & 31], kPhaseValues[(7 * input + 3) & 31], segments[segment].period);
            ++input;
        }
        ASSERT(cadence_run.production.left.factor == factor,
               "SOUNDBIAS cadence changes do not alter the left blip factor");
        ASSERT(cadence_run.production.right.factor == factor,
               "SOUNDBIAS cadence changes do not alter the right blip factor");
        parity_run_read(&cadence_run, caps[cap++ % (sizeof(caps) / sizeof(caps[0]))]);
    }
    parity_run_drain(&cadence_run, caps, (uint32_t)(sizeof(caps) / sizeof(caps[0])));
    parity_run_deinit(&cadence_run);
}
/* Optional AA is deliberately tested through HwResample alone: it must not
 * participate in the independent mGBA 0.10.5 byte-exact comparisons above. */
static void test_optional_antialias_keeps_stereo_streams_independent(void)
{
    printf("Testing frontend optional AA: independent left/right streams...\n");
    enum
    {
        INPUT_COUNT = 192,
    };
    int16_t input_l[INPUT_COUNT];
    int16_t input_r[INPUT_COUNT];
    int16_t zero[INPUT_COUNT] = {0};
    int16_t stereo_l[PARITY_OUTPUT_CAPACITY];
    int16_t stereo_r[PARITY_OUTPUT_CAPACITY];
    int16_t left_only_l[PARITY_OUTPUT_CAPACITY];
    int16_t left_only_r[PARITY_OUTPUT_CAPACITY];
    int16_t right_only_l[PARITY_OUTPUT_CAPACITY];
    int16_t right_only_r[PARITY_OUTPUT_CAPACITY];
    HwResample stereo;
    HwResample left_only;
    HwResample right_only;

    for (uint32_t input = 0; input < INPUT_COUNT; ++input)
    {
        input_l[input] = 12000;
        input_r[input] = (input & 1u) != 0 ? 16000 : -16000;
    }
    hw_resample_init(&stereo, PARITY_HOST_RATE_HZ, kFpsTarget, PARITY_AUDIO_BUFFERS);
    hw_resample_init(&left_only, PARITY_HOST_RATE_HZ, kFpsTarget, PARITY_AUDIO_BUFFERS);
    hw_resample_init(&right_only, PARITY_HOST_RATE_HZ, kFpsTarget, PARITY_AUDIO_BUFFERS);
    hw_resample_set_antialias_input_rate(&stereo, 65536u);
    hw_resample_set_antialias_input_rate(&left_only, 65536u);
    hw_resample_set_antialias_input_rate(&right_only, 65536u);
    hw_resample_set_antialias(&stereo, true);
    hw_resample_set_antialias(&left_only, true);
    hw_resample_set_antialias(&right_only, true);

    const uint32_t stereo_count = submit_optional_antialias(&stereo, input_l, input_r, INPUT_COUNT, stereo_l, stereo_r);
    const uint32_t left_only_count =
        submit_optional_antialias(&left_only, input_l, zero, INPUT_COUNT, left_only_l, left_only_r);
    const uint32_t right_only_count =
        submit_optional_antialias(&right_only, zero, input_r, INPUT_COUNT, right_only_l, right_only_r);

    ASSERT(stereo.aa.enabled && left_only.aa.enabled && right_only.aa.enabled,
           "optional AA coverage explicitly enables the non-parity path");
    ASSERT(stereo_count == left_only_count && stereo_count == right_only_count && stereo_count != 0,
           "independent AA runs release the same paired PCM16 frame count");
    if (stereo_count == left_only_count && stereo_count == right_only_count)
    {
        ASSERT(memcmp(stereo_l, left_only_l, (size_t)stereo_count * sizeof(*stereo_l)) == 0,
               "left AA output depends only on left native PCM16 input");
        ASSERT(memcmp(stereo_r, right_only_r, (size_t)stereo_count * sizeof(*stereo_r)) == 0,
               "right AA output depends only on right native PCM16 input");
    }
    ASSERT(left_only.last_r == 0 && right_only.last_l == 0,
           "silent independent channel remains silent through optional AA");
    ASSERT(stereo.last_l != stereo.last_r, "distinct left/right inputs retain distinct filtered samples");
}

static void test_optional_antialias_passband_and_stopband(void)
{
    printf("Testing frontend optional AA: DC passband and near-Nyquist stopband...\n");
    enum
    {
        INPUT_COUNT = 256,
    };
    int16_t dc_l[INPUT_COUNT];
    int16_t nyquist_l[INPUT_COUNT];
    int16_t zero[INPUT_COUNT] = {0};
    int16_t dc_output_l[PARITY_OUTPUT_CAPACITY];
    int16_t dc_output_r[PARITY_OUTPUT_CAPACITY];
    int16_t nyquist_output_l[PARITY_OUTPUT_CAPACITY];
    int16_t nyquist_output_r[PARITY_OUTPUT_CAPACITY];
    HwResample dc;
    HwResample nyquist;

    for (uint32_t input = 0; input < INPUT_COUNT; ++input)
    {
        dc_l[input] = 12000;
        nyquist_l[input] = (input & 1u) != 0 ? 12000 : -12000;
    }
    hw_resample_init(&dc, PARITY_HOST_RATE_HZ, kFpsTarget, PARITY_AUDIO_BUFFERS);
    hw_resample_init(&nyquist, PARITY_HOST_RATE_HZ, kFpsTarget, PARITY_AUDIO_BUFFERS);
    hw_resample_set_antialias_input_rate(&dc, 65536u);
    hw_resample_set_antialias_input_rate(&nyquist, 65536u);
    hw_resample_set_antialias(&dc, true);
    hw_resample_set_antialias(&nyquist, true);

    const uint32_t dc_count = submit_optional_antialias(&dc, dc_l, zero, INPUT_COUNT, dc_output_l, dc_output_r);
    const uint32_t nyquist_count =
        submit_optional_antialias(&nyquist, nyquist_l, zero, INPUT_COUNT, nyquist_output_l, nyquist_output_r);
    const int dc_tail_peak = pcm_tail_peak_abs(dc_output_l, dc_count);
    const int nyquist_tail_peak = pcm_tail_peak_abs(nyquist_output_l, nyquist_count);
    const int nyquist_last = nyquist.last_l < 0 ? -(int)nyquist.last_l : nyquist.last_l;

    ASSERT(dc_count == nyquist_count && dc_count != 0, "AA passband and stopband runs release paired PCM16");
    ASSERT(dc.last_l > 11000, "optional AA preserves the settled DC passband level");
    ASSERT(nyquist_last < 1000, "optional AA rejects the settled near-Nyquist native input");
    ASSERT(dc_tail_peak > 1000, "optional AA leaves a measurable DC PCM16 passband response");
    ASSERT(nyquist_tail_peak * 8 < dc_tail_peak, "optional AA strongly attenuates the near-Nyquist PCM16 stopband");
}

static void test_optional_antialias_preserves_history_across_rate_changes(void)
{
    printf("Testing frontend optional AA: history across cadence and host-rate changes...\n");
    enum
    {
        INPUT_COUNT = HW_RESAMPLE_AA_TAPS + 15,
    };
    int16_t seed_l[INPUT_COUNT];
    int16_t seed_r[INPUT_COUNT];
    int16_t next_l[INPUT_COUNT];
    int16_t next_r[INPUT_COUNT];
    int16_t discard_l[PARITY_OUTPUT_CAPACITY];
    int16_t discard_r[PARITY_OUTPUT_CAPACITY];
    int16_t preserved_l[PARITY_OUTPUT_CAPACITY];
    int16_t preserved_r[PARITY_OUTPUT_CAPACITY];
    int16_t fresh_l[PARITY_OUTPUT_CAPACITY];
    int16_t fresh_r[PARITY_OUTPUT_CAPACITY];
    double saved_history_l[HW_RESAMPLE_AA_TAPS];
    double saved_history_r[HW_RESAMPLE_AA_TAPS];
    HwResample preserved;
    HwResample fresh;

    for (uint32_t input = 0; input < INPUT_COUNT; ++input)
    {
        seed_l[input] = 12000;
        seed_r[input] = -7000;
        next_l[input] = 9000;
        next_r[input] = -4000;
    }
    hw_resample_init(&preserved, PARITY_HOST_RATE_HZ, kFpsTarget, PARITY_AUDIO_BUFFERS);
    hw_resample_set_antialias_input_rate(&preserved, 65536u);
    hw_resample_set_antialias(&preserved, true);
    (void)submit_optional_antialias(&preserved, seed_l, seed_r, INPUT_COUNT, discard_l, discard_r);
    memcpy(saved_history_l, preserved.aa.history_l, sizeof(saved_history_l));
    memcpy(saved_history_r, preserved.aa.history_r, sizeof(saved_history_r));
    const uint32_t saved_newest = preserved.aa.newest;

    hw_resample_set_antialias_input_rate(&preserved, 131072u);
    hw_resample_set_output_rate(&preserved, 44100u);
    hw_resample_init(&fresh, 44100u, kFpsTarget, PARITY_AUDIO_BUFFERS);
    hw_resample_set_antialias_input_rate(&fresh, 131072u);
    hw_resample_set_antialias(&fresh, true);

    ASSERT(preserved.aa.input_rate_hz == 131072u && preserved.aa.output_rate_hz == 44100u,
           "cadence and host-rate changes rebuild AA coefficients at the new rates");
    ASSERT(preserved.aa.newest == saved_newest, "cadence and host-rate changes retain AA ring position");
    ASSERT(memcmp(saved_history_l, preserved.aa.history_l, sizeof(saved_history_l)) == 0,
           "cadence and host-rate changes retain left AA history");
    ASSERT(memcmp(saved_history_r, preserved.aa.history_r, sizeof(saved_history_r)) == 0,
           "cadence and host-rate changes retain right AA history");

    const uint32_t preserved_count =
        submit_optional_antialias(&preserved, next_l, next_r, INPUT_COUNT, preserved_l, preserved_r);
    const uint32_t fresh_count = submit_optional_antialias(&fresh, next_l, next_r, INPUT_COUNT, fresh_l, fresh_r);
    ASSERT(preserved_count == fresh_count && preserved_count != 0,
           "post-change AA histories release the same number of paired PCM16 frames");
    if (preserved_count == fresh_count)
    {
        ASSERT(memcmp(preserved_l, fresh_l, (size_t)preserved_count * sizeof(*preserved_l)) != 0,
               "preserved left AA history remains audible after cadence and host-rate changes");
        ASSERT(memcmp(preserved_r, fresh_r, (size_t)preserved_count * sizeof(*preserved_r)) != 0,
               "preserved right AA history remains audible after cadence and host-rate changes");
    }
}

int tests_run = 0;
int tests_passed = 0;

int main(void)
{
    test_arm64_factor_clear_and_rollover();
    test_factor_uses_production_legacy_ceiling_path();
    test_all_phases_delta_interpolation_table_stereo_and_short_chunks();
    test_integer_clamp_and_six_bit_feedback();
    test_saturation_gate_drops_samples_without_advancing_delta_history();
    test_reset_matches_blip_clear_and_preserves_last_pcm_history();
    test_host_rate_and_soundbias_changes_keep_the_legacy_timeline();
    test_optional_antialias_keeps_stereo_streams_independent();
    test_optional_antialias_passband_and_stopband();
    test_optional_antialias_preserves_history_across_rate_changes();
    printf("frontend parity: %d/%d tests passed\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
