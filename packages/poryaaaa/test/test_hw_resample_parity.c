#include "hw_audio/hw_resample.h"
#include "test_assert.h"

#include <mgba-util/audio-buffer.h>
#include <mgba-util/audio-resampler.h>

#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum
{
    PARITY_INPUT_FRAMES = 257,
    PARITY_SOURCE_CAPACITY = 1024,
    PARITY_OUTPUT_CAPACITY = 2048,
};

typedef struct
{
    HwResample poryaaaa;
    struct mAudioBuffer mgba_source;
    struct mAudioBuffer mgba_destination;
    struct mAudioResampler mgba;
    float left[PARITY_OUTPUT_CAPACITY];
    float right[PARITY_OUTPUT_CAPACITY];
    int count;
} FrontendParityRun;

static int16_t host_sample_to_pcm16(float sample)
{
    long value = lrintf(sample * 32768.0f);
    if (value > INT16_MAX)
        value = INT16_MAX;
    else if (value < INT16_MIN)
        value = INT16_MIN;
    return (int16_t)value;
}

static void fill_native_stereo(int16_t* left, int16_t* right, int count)
{
    static const int16_t samples[] = {
        -24576,
        -23040,
        -16384,
        -4096,
        -1,
        0,
        1,
        4096,
        16384,
        23040,
        24528,
    };
    const int sample_count = (int)(sizeof(samples) / sizeof(samples[0]));
    for (int i = 0; i < count; ++i)
    {
        left[i] = samples[i % sample_count];
        right[i] = samples[(i * 7 + 3) % sample_count];
    }
}

static void parity_run_init(FrontendParityRun* run, double source_rate, double destination_rate)
{
    memset(run, 0, sizeof(*run));
    hw_resample_init(&run->poryaaaa, source_rate, destination_rate);

    mAudioBufferInit(&run->mgba_source, PARITY_SOURCE_CAPACITY, 2);
    mAudioBufferInit(&run->mgba_destination, PARITY_OUTPUT_CAPACITY, 2);
    mAudioResamplerInit(&run->mgba, mINTERPOLATOR_SINC);
    mAudioResamplerSetSource(&run->mgba, &run->mgba_source, source_rate, true);
    mAudioResamplerSetDestination(&run->mgba, &run->mgba_destination, destination_rate);

    ASSERT_NEAR(run->mgba.lowWaterMark, 8.0, 0.0, "mGBA sinc low-water mark is eight frames");
    ASSERT_NEAR(run->mgba.highWaterMark, 8.0, 0.0, "mGBA sinc high-water mark is eight frames");
}

static void parity_run_deinit(FrontendParityRun* run)
{
    mAudioResamplerDeinit(&run->mgba);
    mAudioBufferDeinit(&run->mgba_destination);
    mAudioBufferDeinit(&run->mgba_source);
}

static void parity_run_process(FrontendParityRun* run, const int16_t* left, const int16_t* right, int input_count)
{
    int16_t input[PARITY_SOURCE_CAPACITY * 2];
    int16_t mgba_output[PARITY_OUTPUT_CAPACITY * 2];
    ASSERT(input_count >= 0 && input_count <= PARITY_SOURCE_CAPACITY, "test input fits mGBA source buffer");
    ASSERT(run->count < PARITY_OUTPUT_CAPACITY, "test output buffer has remaining capacity");

    if (input_count > 0)
    {
        for (int i = 0; i < input_count; ++i)
        {
            input[i * 2] = left ? left[i] : 0;
            input[i * 2 + 1] = right ? right[i] : 0;
        }
        ASSERT_EQ((int)mAudioBufferWrite(&run->mgba_source, input, (size_t)input_count),
                  input_count,
                  "mGBA source accepts the identical native PCM16 frames");
    }

    const int poryaaaa_count = hw_resample_process(&run->poryaaaa,
                                                   left,
                                                   right,
                                                   input_count,
                                                   run->left + run->count,
                                                   run->right + run->count,
                                                   PARITY_OUTPUT_CAPACITY - run->count);
    const int mgba_count = (int)mAudioResamplerProcess(&run->mgba);
    ASSERT_EQ(poryaaaa_count, mgba_count, "poryaaaa and pinned mGBA produce the same host-frame count");
    ASSERT_EQ((int)mAudioBufferRead(&run->mgba_destination, mgba_output, (size_t)mgba_count),
              mgba_count,
              "read every pinned mGBA frontend frame");

    for (int i = 0; i < mgba_count; ++i)
    {
        ASSERT_EQ(host_sample_to_pcm16(run->left[run->count + i]),
                  mgba_output[i * 2],
                  "left host float converts exactly to mGBA PCM16");
        ASSERT_EQ(host_sample_to_pcm16(run->right[run->count + i]),
                  mgba_output[i * 2 + 1],
                  "right host float converts exactly to mGBA PCM16");
    }

    run->count += poryaaaa_count;
    ASSERT_EQ(run->poryaaaa.available,
              (int)mAudioBufferAvailable(&run->mgba_source),
              "source retention matches mGBA low-water consumption");
    ASSERT_NEAR(run->poryaaaa.timestamp, run->mgba.timestamp, 1e-12, "shared frontend timestamp matches mGBA");
}

static void parity_run_set_rates(FrontendParityRun* run, double source_rate, double destination_rate)
{
    const double timestamp = run->poryaaaa.timestamp;
    hw_resample_set_rates(&run->poryaaaa, source_rate, destination_rate);
    mAudioResamplerSetSource(&run->mgba, &run->mgba_source, source_rate, true);
    mAudioResamplerSetDestination(&run->mgba, &run->mgba_destination, destination_rate);
    ASSERT_NEAR(run->poryaaaa.timestamp, timestamp, 0.0, "poryaaaa rate update retains frontend phase");
    ASSERT_NEAR(run->mgba.timestamp, timestamp, 1e-12, "mGBA rate update retains frontend phase");
}

static void feed_partition(FrontendParityRun* run, const int16_t* left, const int16_t* right, int count, int partition)
{
    if (partition <= 0)
    {
        parity_run_process(run, left, right, count);
        return;
    }

    for (int offset = 0; offset < count;)
    {
        int chunk = count - offset;
        if (chunk > partition)
            chunk = partition;
        parity_run_process(run, left + offset, right + offset, chunk);
        offset += chunk;
    }
}

static void test_startup_watermarks_and_drain(void)
{
    printf("Testing frontend parity: mGBA sinc startup watermarks and drain...\n");
    int16_t left[17];
    int16_t right[17];
    fill_native_stereo(left, right, 17);

    FrontendParityRun run;
    parity_run_init(&run, 65536.0, 65536.0);
    ASSERT_EQ(hw_resample_inputs_needed(&run.poryaaaa, 1), 9, "first sinc output needs nine real source frames");
    parity_run_process(&run, left, right, 8);
    ASSERT_EQ(run.count, 0, "eight real frames remain below mGBA high-water mark");
    ASSERT_EQ(run.poryaaaa.input_l[0], left[0], "startup keeps native PCM16 without injected source zeros");
    ASSERT_EQ(hw_resample_inputs_needed(&run.poryaaaa, 1), 1, "one more native frame releases the first host frame");

    parity_run_process(&run, left + 8, right + 8, 1);
    ASSERT_EQ(run.count, 1, "ninth real source frame releases the startup sample");
    ASSERT_NEAR(run.poryaaaa.timestamp, 1.0, 0.0, "startup sample advances the shared timestamp once");

    parity_run_process(&run, left + 9, right + 9, 8);
    ASSERT_EQ(run.count, 9, "equal-rate frontend emits all readable frames before the final high-water stall");
    ASSERT_EQ(run.poryaaaa.available, 16, "low-water consumption keeps eight frames of history after a drop");
    ASSERT_NEAR(
        run.poryaaaa.timestamp, 8.0, 0.0, "low-water drop preserves the timestamp relative to retained history");

    const int before_drain = run.count;
    parity_run_process(&run, NULL, NULL, 0);
    ASSERT_EQ(run.count, before_drain, "drain does not invent zero-padded tail samples beyond mGBA's high-water stall");
    parity_run_deinit(&run);
}

static void test_partitions_match_pinned_mgba_at_all_rates(void)
{
    printf("Testing frontend parity: one-shot and chunked native PCM16 at all rates...\n");
    static const double destination_rates[] = {65536.0, 48000.0, 44100.0};
    int16_t left[PARITY_INPUT_FRAMES];
    int16_t right[PARITY_INPUT_FRAMES];
    fill_native_stereo(left, right, PARITY_INPUT_FRAMES);

    for (size_t rate_index = 0; rate_index < sizeof(destination_rates) / sizeof(destination_rates[0]); ++rate_index)
    {
        FrontendParityRun one_shot;
        FrontendParityRun single_frames;
        FrontendParityRun thirty_seven_frames;
        parity_run_init(&one_shot, 65536.0, destination_rates[rate_index]);
        parity_run_init(&single_frames, 65536.0, destination_rates[rate_index]);
        parity_run_init(&thirty_seven_frames, 65536.0, destination_rates[rate_index]);

        feed_partition(&one_shot, left, right, PARITY_INPUT_FRAMES, 0);
        feed_partition(&single_frames, left, right, PARITY_INPUT_FRAMES, 1);
        feed_partition(&thirty_seven_frames, left, right, PARITY_INPUT_FRAMES, 37);

        ASSERT_EQ(single_frames.count, one_shot.count, "one-frame partition preserves total host frames");
        ASSERT_EQ(thirty_seven_frames.count, one_shot.count, "37-frame partition preserves total host frames");
        ASSERT(memcmp(single_frames.left, one_shot.left, (size_t)one_shot.count * sizeof(one_shot.left[0])) == 0,
               "one-frame partition preserves every left PCM16 result");
        ASSERT(memcmp(single_frames.right, one_shot.right, (size_t)one_shot.count * sizeof(one_shot.right[0])) == 0,
               "one-frame partition preserves every right PCM16 result");
        ASSERT(memcmp(thirty_seven_frames.left, one_shot.left, (size_t)one_shot.count * sizeof(one_shot.left[0])) == 0,
               "37-frame partition preserves every left PCM16 result");
        ASSERT(memcmp(thirty_seven_frames.right, one_shot.right, (size_t)one_shot.count * sizeof(one_shot.right[0])) ==
                   0,
               "37-frame partition preserves every right PCM16 result");

        parity_run_process(&one_shot, NULL, NULL, 0);
        parity_run_process(&single_frames, NULL, NULL, 0);
        parity_run_process(&thirty_seven_frames, NULL, NULL, 0);
        ASSERT_EQ(single_frames.count, one_shot.count, "one-frame final drain matches mGBA's stalled tail");
        ASSERT_EQ(thirty_seven_frames.count, one_shot.count, "37-frame final drain matches mGBA's stalled tail");

        parity_run_deinit(&thirty_seven_frames);
        parity_run_deinit(&single_frames);
        parity_run_deinit(&one_shot);
    }
}

static void test_rate_change_preserves_fractional_phase(void)
{
    printf("Testing frontend parity: source-rate changes retain fractional phase...\n");
    int16_t left[PARITY_INPUT_FRAMES];
    int16_t right[PARITY_INPUT_FRAMES];
    fill_native_stereo(left, right, PARITY_INPUT_FRAMES);

    FrontendParityRun run;
    parity_run_init(&run, 65536.0, 48000.0);
    feed_partition(&run, left, right, 73, 37);
    ASSERT(fabs(run.poryaaaa.timestamp - floor(run.poryaaaa.timestamp)) > 0.01,
           "rate change fixture reaches a fractional frontend timestamp");
    parity_run_set_rates(&run, 32768.0, 48000.0);
    feed_partition(&run, left + 73, right + 73, PARITY_INPUT_FRAMES - 73, 37);
    parity_run_process(&run, NULL, NULL, 0);
    parity_run_deinit(&run);
}

static void test_channels_keep_independent_pcm16_history(void)
{
    printf("Testing frontend parity: left and right histories are independent...\n");
    int16_t left[PARITY_INPUT_FRAMES];
    int16_t right[PARITY_INPUT_FRAMES] = {0};
    fill_native_stereo(left, right, PARITY_INPUT_FRAMES);
    memset(right, 0, sizeof(right));

    FrontendParityRun run;
    parity_run_init(&run, 65536.0, 44100.0);
    feed_partition(&run, left, right, PARITY_INPUT_FRAMES, 37);
    for (int i = 0; i < run.count; ++i)
        ASSERT_EQ(run.right[i], 0, "silent right native PCM16 input stays silent after left resampling");
    parity_run_deinit(&run);
}

int tests_run = 0;
int tests_passed = 0;

int main(void)
{
    test_startup_watermarks_and_drain();
    test_partitions_match_pinned_mgba_at_all_rates();
    test_rate_change_preserves_fractional_phase();
    test_channels_keep_independent_pcm16_history();
    printf("frontend parity: %d/%d tests passed\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
