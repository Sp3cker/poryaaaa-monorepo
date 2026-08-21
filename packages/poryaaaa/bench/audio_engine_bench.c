#include "hw_audio/hw_audio.h"
#include "m4a/m4a_driver.h"

#include <errno.h>
#include <float.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef enum
{
    SCENARIO_ALL,
    SCENARIO_CHIP_SQ2,
    SCENARIO_CHIP_PCM,
    SCENARIO_DRIVER_PSG,
} BenchScenario;

typedef struct
{
    BenchScenario scenario;
    int sample_rate;
    int seconds;
    int warmup_seconds;
    int block;
    int repeat;
} BenchOptions;

typedef struct
{
    double sink;
} BenchState;

enum
{
    BENCH_VOICEGROUP_SIZE = 128,
};
/* Map host-frame endpoints into the shared absolute GBA cycle domain. */
static uint64_t bench_gba_cycles_for_frames(uint64_t frames, uint32_t sample_rate)
{
    return frames * M4A_GBA_CYCLES_PER_SECOND / sample_rate;
}

/* Form one explicit hardware-event interval for a benchmark host block. */
static M4ARegWriteBatch bench_event_batch(M4ARegWrite* events, size_t count, uint64_t begin_cycle, uint64_t end_cycle)
{
    uint64_t previous_cycle = 0;
    uint32_t order = 0;
    for (size_t i = 0; i < count; i++)
    {
        if (i != 0 && events[i].cycle == previous_cycle)
            order++;
        else
            order = 0;
        events[i].order = order;
        previous_cycle = events[i].cycle;
    }
    return (M4ARegWriteBatch){
        .events = events,
        .count = count,
        .begin_cycle = begin_cycle,
        .end_cycle = end_cycle,
    };
}

/* Parse a positive integer option and reject partial strings. */
static bool parse_positive_int(const char* text, int* out)
{
    char* end = NULL;
    errno = 0;
    long value = strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || value <= 0 || value > INT32_MAX)
    {
        return false;
    }
    *out = (int)value;
    return true;
}

/* Keep usage text beside the parser so invalid profiling runs fail early. */
static void print_usage(const char* exe)
{
    fprintf(stderr,
            "Usage: %s [--scenario all|chip-sq2|chip-pcm|driver-psg] [--sample-rate HZ]\n"
            "          [--seconds SECONDS] [--warmup-seconds SECONDS] [--block FRAMES] [--repeat COUNT]\n",
            exe);
}

/* Convert CLI strings to benchmark options without accepting unknown knobs. */
static bool parse_args(int argc, char** argv, BenchOptions* out)
{
    *out = (BenchOptions){
        .scenario = SCENARIO_ALL,
        .sample_rate = 44100,
        .seconds = 60,
        .warmup_seconds = 2,
        .block = 512,
        .repeat = 100,
    };

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--scenario") == 0 && i + 1 < argc)
        {
            const char* value = argv[++i];
            if (strcmp(value, "all") == 0)
                out->scenario = SCENARIO_ALL;
            else if (strcmp(value, "chip-sq2") == 0)
                out->scenario = SCENARIO_CHIP_SQ2;
            else if (strcmp(value, "chip-pcm") == 0)
                out->scenario = SCENARIO_CHIP_PCM;
            else if (strcmp(value, "driver-psg") == 0)
                out->scenario = SCENARIO_DRIVER_PSG;
            else
                return false;
        }
        else if (strcmp(argv[i], "--sample-rate") == 0 && i + 1 < argc)
        {
            if (!parse_positive_int(argv[++i], &out->sample_rate))
                return false;
        }
        else if (strcmp(argv[i], "--seconds") == 0 && i + 1 < argc)
        {
            if (!parse_positive_int(argv[++i], &out->seconds))
                return false;
        }
        else if (strcmp(argv[i], "--warmup-seconds") == 0 && i + 1 < argc)
        {
            if (!parse_positive_int(argv[++i], &out->warmup_seconds))
                return false;
        }
        else if (strcmp(argv[i], "--block") == 0 && i + 1 < argc)
        {
            if (!parse_positive_int(argv[++i], &out->block))
                return false;
        }
        else if (strcmp(argv[i], "--repeat") == 0 && i + 1 < argc)
        {
            if (!parse_positive_int(argv[++i], &out->repeat))
                return false;
        }
        else
        {
            return false;
        }
    }

    return out->block <= M4A_RECOMMENDED_MAX_ADVANCE_FRAMES;
}

/* Use a monotonic clock for elapsed benchmark timing. */
static double now_seconds(void)
{
#if defined(CLOCK_MONOTONIC)
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
#else
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
#endif
}

/* Touch output buffers so optimized benchmark builds cannot ignore work. */
static double consume_audio(const float* left, const float* right, int frames)
{
    double sum = 0.0;
    for (int i = 0; i < frames; i += 97)
    {
        sum += (double)left[i] + (double)right[i];
    }
    return sum;
}

/* Render chip-only SQ2 so Instruments can isolate PSG/chip cost. */
static bool run_chip_sq2(int sample_rate, int frames, int block, float* left, float* right, BenchState* state)
{
    HwAudio* hw = hw_audio_create((float)sample_rate);
    if (!hw)
        return false;

    M4ARegWrite setup[] = {
        {0, M4A_REG_NR52, 0x80, 0},
        {0, M4A_REG_NR50, 0x77, 1},
        {0, M4A_REG_NR51, 0x22, 2},
        {0, M4A_REG_SOUNDCNT_H, 0x02, 3},
        {0, M4A_REG_NR21, 0x80, 4},
        {0, M4A_REG_NR22, 0xF8, 5},
        {0, M4A_REG_NR23, 1700 & 0xFF, 6},
        {0, M4A_REG_NR24, 0x80 | ((1700 >> 8) & 7), 7},
    };

    for (int done = 0; done < frames;)
    {
        int n = frames - done < block ? frames - done : block;
        M4ARegWriteBatch batch =
            bench_event_batch(done == 0 ? setup : NULL,
                              done == 0 ? sizeof(setup) / sizeof(setup[0]) : 0,
                              bench_gba_cycles_for_frames((uint64_t)done, (uint32_t)sample_rate),
                              bench_gba_cycles_for_frames((uint64_t)(done + n), (uint32_t)sample_rate));
        hw_audio_render_events(hw, &batch, left, right, n);
        state->sink += consume_audio(left, right, n);
        done += n;
    }

    hw_audio_destroy(hw);
    return true;
}

/* Render chip-only PCM so Instruments can isolate DirectSound chip cost. */
static bool run_chip_pcm(int sample_rate, int frames, int block, float* left, float* right, BenchState* state)
{
    HwAudio* hw = hw_audio_create((float)sample_rate);
    if (!hw)
        return false;

    M4ARegWrite setup[] = {
        {0, M4A_REG_SOUNDCNT_H, (1u << 8) | (1u << 9) | (1u << 12) | (1u << 13) | (1u << 2) | (1u << 3), 0},
        {0, M4A_REG_FIFO_A, 0x7F40C000u, 1},
        {0, M4A_REG_FIFO_B, 0x8040C000u, 2},
        {0, M4A_REG_TIMER_0, 0, 3},
    };

    for (int done = 0; done < frames;)
    {
        int n = frames - done < block ? frames - done : block;
        M4ARegWriteBatch batch =
            bench_event_batch(done == 0 ? setup : NULL,
                              done == 0 ? sizeof(setup) / sizeof(setup[0]) : 0,
                              bench_gba_cycles_for_frames((uint64_t)done, (uint32_t)sample_rate),
                              bench_gba_cycles_for_frames((uint64_t)(done + n), (uint32_t)sample_rate));
        hw_audio_render_events(hw, &batch, left, right, n);
        state->sink += consume_audio(left, right, n);
        done += n;
    }

    hw_audio_destroy(hw);
    return true;
}

/* Render through the real driver-to-chip event path used by production. */
static bool run_driver_psg(int sample_rate, int frames, int block, float* left, float* right, BenchState* state)
{
    M4ADriver* drv = m4a_driver_create((float)sample_rate);
    HwAudio* hw = hw_audio_create((float)sample_rate);
    ToneData voices[BENCH_VOICEGROUP_SIZE];

    if (!drv || !hw)
    {
        if (drv)
            m4a_driver_destroy(drv);
        if (hw)
            hw_audio_destroy(hw);
        return false;
    }

    memset(voices, 0, sizeof(voices));
    voices[0].type = VOICE_SQUARE_2;
    voices[0].key = 60;
    voices[0].panSweep = 0x40;
    voices[0].attack = 0x00;
    voices[0].decay = 0x00;
    voices[0].sustain = 0x0F;
    voices[0].release = 0x00;

    m4a_driver_set_voicegroup(drv, voices);
    m4a_program_change(drv, 0, 0);
    m4a_cc(drv, 0, 7, 127);
    m4a_cc(drv, 0, 10, 64);
    m4a_note_on(drv, 0, 60, 100);

    for (int done = 0; done < frames;)
    {
        int n = frames - done < block ? frames - done : block;
        m4a_advance(drv, n);
        hw_audio_render_events(hw, m4a_get_pending_writes(drv), left, right, n);
        m4a_consume_writes(drv);
        state->sink += consume_audio(left, right, n);
        done += n;
    }

    m4a_all_sound_off(drv);
    hw_audio_destroy(hw);
    m4a_driver_destroy(drv);
    return true;
}

/* Dispatch one scenario for the requested simulated duration. */
static bool run_scenario(
    BenchScenario scenario, int sample_rate, int seconds, int block, float* left, float* right, BenchState* state)
{
    int frames = sample_rate * seconds;
    switch (scenario)
    {
    case SCENARIO_CHIP_SQ2:
        return run_chip_sq2(sample_rate, frames, block, left, right, state);
    case SCENARIO_CHIP_PCM:
        return run_chip_pcm(sample_rate, frames, block, left, right, state);
    case SCENARIO_DRIVER_PSG:
        return run_driver_psg(sample_rate, frames, block, left, right, state);
    case SCENARIO_ALL:
        return false;
    }
    return false;
}

/* Keep CSV labels stable for scripts and Instruments run notes. */
static const char* scenario_name(BenchScenario scenario)
{
    switch (scenario)
    {
    case SCENARIO_CHIP_SQ2:
        return "chip-sq2";
    case SCENARIO_CHIP_PCM:
        return "chip-pcm";
    case SCENARIO_DRIVER_PSG:
        return "driver-psg";
    case SCENARIO_ALL:
        return "all";
    }
    return "unknown";
}

/* Warm up, measure one scenario, and print one CSV row. */
static bool
measure_scenario(const BenchOptions* opt, BenchScenario scenario, int repeat_index, float* left, float* right)
{
    BenchState state = {0.0};
    if (!run_scenario(scenario, opt->sample_rate, opt->warmup_seconds, opt->block, left, right, &state))
        return false;

    double start = now_seconds();
    if (!run_scenario(scenario, opt->sample_rate, opt->seconds, opt->block, left, right, &state))
        return false;
    double elapsed = now_seconds() - start;

    int frames = opt->sample_rate * opt->seconds;
    double ns_per_frame = elapsed * 1000000000.0 / (double)frames;
    double realtime_factor = (double)opt->seconds / elapsed;
    printf("%s,%d,%d,%d,%d,%d,%.9f,%.3f,%.3f\n",
           scenario_name(scenario),
           opt->sample_rate,
           opt->block,
           opt->seconds,
           repeat_index,
           frames,
           elapsed,
           ns_per_frame,
           realtime_factor);

    if (state.sink == DBL_MAX)
        fprintf(stderr, "unreachable sink: %.17g\n", state.sink);
    return true;
}

int main(int argc, char** argv)
{
    BenchOptions opt;
    if (!parse_args(argc, argv, &opt))
    {
        print_usage(argv[0]);
        fprintf(stderr, "Note: --block must be between 1 and %d frames.\n", M4A_RECOMMENDED_MAX_ADVANCE_FRAMES);
        return 2;
    }

    float* left = (float*)calloc((size_t)opt.block, sizeof(float));
    float* right = (float*)calloc((size_t)opt.block, sizeof(float));
    if (!left || !right)
    {
        free(left);
        free(right);
        fprintf(stderr, "failed to allocate %d-frame output buffers\n", opt.block);
        return 1;
    }

    printf("scenario,sample_rate,block,seconds,repeat,rendered_frames,elapsed_seconds,ns_per_frame,realtime_factor\n");

    BenchScenario scenarios[] = {SCENARIO_CHIP_SQ2, SCENARIO_CHIP_PCM, SCENARIO_DRIVER_PSG};
    int scenario_count = opt.scenario == SCENARIO_ALL ? 3 : 1;
    for (int i = 0; i < scenario_count; i++)
    {
        BenchScenario scenario = opt.scenario == SCENARIO_ALL ? scenarios[i] : opt.scenario;
        for (int r = 1; r <= opt.repeat; r++)
        {
            if (!measure_scenario(&opt, scenario, r, left, right))
            {
                fprintf(stderr, "scenario failed: %s\n", scenario_name(scenario));
                free(left);
                free(right);
                return 1;
            }
        }
    }

    free(left);
    free(right);
    return 0;
}
