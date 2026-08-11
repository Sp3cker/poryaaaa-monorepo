#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__clang__)
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wextra-semi"
#endif
#include <mgba/core/config.h>
#include <mgba/core/core.h>
#include <mgba/core/timing.h>
#include <mgba/gba/core.h>
#include <mgba/internal/gba/audio.h>
#include <mgba/internal/gba/gba.h>
#include <mgba/internal/gba/io.h>
#if defined(__clang__)
#    pragma clang diagnostic pop
#endif

#ifndef PORYAAAA_MGBA_BASE_REVISION
#    define PORYAAAA_MGBA_BASE_REVISION "unverified"
#endif
#ifndef PORYAAAA_MGBA_OBSERVATION_PATCH_SHA256
#    define PORYAAAA_MGBA_OBSERVATION_PATCH_SHA256 "unverified"
#endif

#define GBA_CLOCK_HZ 16777216u
#define TRACE_LINE_CAPACITY 512u
#define TRACE_ORDER_EXTENDED 0x80000000u
#define TRACE_ORDER_DELAY_MASK 0xFFFFu

#define AUDIO_CHANNEL_SQ1 (1u << 0u)
#define AUDIO_CHANNEL_SQ2 (1u << 1u)
#define AUDIO_CHANNEL_WAVE (1u << 2u)
#define AUDIO_CHANNEL_NOISE (1u << 3u)
#define AUDIO_CHANNEL_FIFO_A (1u << 4u)
#define AUDIO_CHANNEL_FIFO_B (1u << 5u)
#define AUDIO_CHANNEL_PSG (AUDIO_CHANNEL_SQ1 | AUDIO_CHANNEL_SQ2 | AUDIO_CHANNEL_WAVE | AUDIO_CHANNEL_NOISE)
#define AUDIO_CHANNEL_DIRECTSOUND (AUDIO_CHANNEL_FIFO_A | AUDIO_CHANNEL_FIFO_B)
#define AUDIO_CHANNEL_ALL (AUDIO_CHANNEL_PSG | AUDIO_CHANNEL_DIRECTSOUND)

typedef struct
{
    const char* inputPath;
    const char* outputPrefix;
    uint32_t soloMask;
} Options;

typedef struct
{
    bool valid;
    uint64_t cycle;
    uint32_t order;
} TracePosition;

typedef struct
{
    uint64_t cycle;
    bool capture;
} ExpectedSample;

typedef struct
{
    struct mCore* core;
    struct GBAAudioObservationSink observationSink;
    FILE* pcm;
    FILE* cycles;
    ExpectedSample* expectedSamples;
    size_t expectedCount;
    size_t expectedHead;
    size_t expectedCapacity;
    uint64_t frameCount;
    uint64_t firstCycle;
    uint64_t lastCycle;
    size_t observedTimers[2];
    unsigned lineNumber;
    bool failed;
} Replay;

/* Keep the replay command narrow enough for comparison scripts. */
static void print_usage(const char* program)
{
    fprintf(stderr,
            "Usage: %s --input TRACE --output-prefix PREFIX [--solo CHANNELS]\n"
            "CHANNELS: sq1,sq2,wave,noise,fifo-a,fifo-b,psg,directsound,all\n",
            program);
}

/* Parse the six stable mGBA GBA audio channel names used by native captures. */
static bool parse_solo_mask(const char* text, uint32_t* mask)
{
    size_t length = strlen(text);
    if (length == 0u || length >= 128u)
        return false;
    char buffer[128];
    memcpy(buffer, text, length + 1u);
    uint32_t parsed = 0u;
    for (char* name = strtok(buffer, ","); name != NULL; name = strtok(NULL, ","))
    {
        if (strcmp(name, "sq1") == 0)
            parsed |= AUDIO_CHANNEL_SQ1;
        else if (strcmp(name, "sq2") == 0)
            parsed |= AUDIO_CHANNEL_SQ2;
        else if (strcmp(name, "wave") == 0)
            parsed |= AUDIO_CHANNEL_WAVE;
        else if (strcmp(name, "noise") == 0)
            parsed |= AUDIO_CHANNEL_NOISE;
        else if (strcmp(name, "fifo-a") == 0 || strcmp(name, "dma-a") == 0)
            parsed |= AUDIO_CHANNEL_FIFO_A;
        else if (strcmp(name, "fifo-b") == 0 || strcmp(name, "dma-b") == 0)
            parsed |= AUDIO_CHANNEL_FIFO_B;
        else if (strcmp(name, "psg") == 0)
            parsed |= AUDIO_CHANNEL_PSG;
        else if (strcmp(name, "directsound") == 0)
            parsed |= AUDIO_CHANNEL_DIRECTSOUND;
        else if (strcmp(name, "all") == 0)
            parsed |= AUDIO_CHANNEL_ALL;
        else
            return false;
    }
    if (parsed == 0u)
        return false;
    *mask = parsed;
    return true;
}

/* Reject duplicate, partial, and unknown command-line options before opening artifacts. */
static bool parse_options(int argc, char** argv, Options* options)
{
    *options = (Options){
        .soloMask = AUDIO_CHANNEL_ALL,
    };
    bool soloSeen = false;
    for (int index = 1; index < argc; ++index)
    {
        if (strcmp(argv[index], "--input") == 0 && index + 1 < argc && options->inputPath == NULL)
            options->inputPath = argv[++index];
        else if (strcmp(argv[index], "--output-prefix") == 0 && index + 1 < argc && options->outputPrefix == NULL)
            options->outputPrefix = argv[++index];
        else if (strcmp(argv[index], "--solo") == 0 && index + 1 < argc && !soloSeen)
        {
            if (!parse_solo_mask(argv[++index], &options->soloMask))
                return false;
            soloSeen = true;
        }
        else
            return false;
    }
    return options->inputPath != NULL && options->outputPrefix != NULL;
}

/* Allocate one artifact name without imposing a platform path-length limit. */
static char* path_with_suffix(const char* prefix, const char* suffix)
{
    size_t prefixLength = strlen(prefix);
    size_t suffixLength = strlen(suffix);
    if (prefixLength > SIZE_MAX - suffixLength - 1u)
        return NULL;
    char* path = malloc(prefixLength + suffixLength + 1u);
    if (path == NULL)
        return NULL;
    memcpy(path, prefix, prefixLength);
    memcpy(path + prefixLength, suffix, suffixLength + 1u);
    return path;
}

/* Parse a trace decimal token without accepting signs, whitespace, or overflow. */
static bool parse_u64_decimal(const char* text, uint64_t* result)
{
    if (*text == '\0')
        return false;
    uint64_t value = 0u;
    for (const char* character = text; *character != '\0'; ++character)
    {
        if (*character < '0' || *character > '9')
            return false;
        uint64_t digit = (uint64_t)(*character - '0');
        if (value > (UINT64_MAX - digit) / 10u)
            return false;
        value = value * 10u + digit;
    }
    *result = value;
    return true;
}

/* Parse a trace hexadecimal token without accepting signs, whitespace, or overflow. */
static bool parse_u32_hex(const char* text, uint32_t* result)
{
    if (text[0] != '0' || text[1] != 'x' || text[2] == '\0')
        return false;
    uint32_t value = 0u;
    for (const char* character = text + 2; *character != '\0'; ++character)
    {
        uint32_t digit = 0u;
        if (*character >= '0' && *character <= '9')
            digit = (uint32_t)(*character - '0');
        else if (*character >= 'A' && *character <= 'F')
            digit = (uint32_t)(*character - 'A') + 10u;
        else if (*character >= 'a' && *character <= 'f')
            digit = (uint32_t)(*character - 'a') + 10u;
        else
            return false;
        if (value > (UINT32_MAX - digit) / 16u)
            return false;
        value = value * 16u + digit;
    }
    *result = value;
    return true;
}

/* Split one exact grammar line: tokens use one ASCII space and no trailing whitespace. */
static bool split_trace_line(char* line, char* tokens[6], size_t* count)
{
    *count = 0u;
    if (*line == '\0')
        return false;
    char* cursor = line;
    while (*cursor != '\0')
    {
        if (*count == 6u || *cursor == ' ' || *cursor == '\t' || *cursor == '\r')
            return false;
        tokens[(*count)++] = cursor;
        while (*cursor != '\0' && *cursor != ' ')
        {
            if (*cursor == '\t' || *cursor == '\r')
                return false;
            ++cursor;
        }
        if (*cursor == '\0')
            break;
        *cursor++ = '\0';
        if (*cursor == '\0' || *cursor == ' ')
            return false;
    }
    return true;
}

/* Require the one global same-cycle ordering domain used by the recorder. */
static bool advance_position(TracePosition* position, uint64_t cycle, uint32_t order)
{
    if (position->valid && (cycle < position->cycle || (cycle == position->cycle && order <= position->order)))
        return false;
    *position = (TracePosition){
        .valid = true,
        .cycle = cycle,
        .order = order,
    };
    return true;
}

/* Record the first replay failure with the source line that caused it. */
static void fail_replay(Replay* replay, const char* message)
{
    if (!replay->failed)
        fprintf(stderr, "Trace line %u: %s\n", replay->lineNumber, message);
    replay->failed = true;
}

/* Keep every emitted native sample paired with a prior explicit SAMPLE marker. */
static bool append_expected_sample(Replay* replay, uint64_t cycle, bool capture)
{
    if (replay->expectedCount == replay->expectedCapacity)
    {
        size_t capacity = replay->expectedCapacity == 0u ? 256u : replay->expectedCapacity * 2u;
        if (capacity < replay->expectedCapacity || capacity > SIZE_MAX / sizeof(*replay->expectedSamples))
            return false;
        ExpectedSample* samples = realloc(replay->expectedSamples, capacity * sizeof(*samples));
        if (samples == NULL)
            return false;
        replay->expectedSamples = samples;
        replay->expectedCapacity = capacity;
    }
    replay->expectedSamples[replay->expectedCount++] = (ExpectedSample){
        .cycle = cycle,
        .capture = capture,
    };
    return true;
}

/* Serialize one observed signed stereo frame in the canonical little-endian form. */
static bool write_native_frame(Replay* replay, uint64_t cycle, int16_t left, int16_t right)
{
    if (replay->frameCount != 0u && cycle <= replay->lastCycle)
        return false;
    uint16_t leftBits = (uint16_t)left;
    uint16_t rightBits = (uint16_t)right;
    uint8_t pcm[] = {
        (uint8_t)(leftBits & 0xFFu),
        (uint8_t)(leftBits >> 8u),
        (uint8_t)(rightBits & 0xFFu),
        (uint8_t)(rightBits >> 8u),
    };
    uint8_t cycles[8];
    for (unsigned index = 0u; index < sizeof(cycles); ++index)
        cycles[index] = (uint8_t)(cycle >> (index * 8u));
    if (fwrite(pcm, sizeof(pcm), 1u, replay->pcm) != 1u || fwrite(cycles, sizeof(cycles), 1u, replay->cycles) != 1u)
        return false;
    if (replay->frameCount == 0u)
        replay->firstCycle = cycle;
    replay->lastCycle = cycle;
    replay->frameCount++;
    return true;
}

/* Observe mGBA's native post-bias samples instead of reconstructing its mixer. */
static void emit_observation(void* context, const struct GBAAudioObservation* observation)
{
    Replay* replay = context;
    if (replay->failed)
        return;
    if (observation->kind == GBA_AUDIO_OBSERVATION_TIMER)
    {
        if (observation->value > 1u)
            fail_replay(replay, "mGBA emitted an invalid FIFO timer selection");
        else
            replay->observedTimers[observation->value]++;
        return;
    }
    if (observation->kind != GBA_AUDIO_OBSERVATION_SAMPLE)
        return;
    if (replay->expectedHead == replay->expectedCount)
    {
        fail_replay(replay, "mGBA emitted a SAMPLE not present in the trace");
        return;
    }
    ExpectedSample expected = replay->expectedSamples[replay->expectedHead];
    if (observation->cycle != expected.cycle)
    {
        fail_replay(replay, "mGBA emitted a SAMPLE at a different cycle than the trace");
        return;
    }
    replay->expectedHead++;
    if (expected.capture && !write_native_frame(replay, observation->cycle, observation->left, observation->right))
        fail_replay(replay, "could not write native capture");
}

/* The replay does not retain observations across the mGBA hardware reset. */
static void reset_observations(void* context)
{
    Replay* replay = context;
    replay->observedTimers[0] = 0u;
    replay->observedTimers[1] = 0u;
}

/* Advance the real mGBA timing queue to a trace-cycle boundary. */
static bool advance_mgba_to(Replay* replay, uint64_t cycle)
{
    if (cycle > INT32_MAX)
    {
        fail_replay(replay, "cycle exceeds mGBA's replay timing range");
        return false;
    }
    struct GBA* gba = replay->core->board;
    int32_t current = mTimingCurrentTime(&gba->timing);
    int32_t target = (int32_t)cycle;
    if (target < current)
    {
        fail_replay(replay, "mGBA timing moved backwards");
        return false;
    }
    (void)mTimingTick(&gba->timing, target - current);
    return !replay->failed && mTimingCurrentTime(&gba->timing) == target;
}

/* Reproduce the sample-event offset from mGBA's late full-core timer callback. */
static bool sample_event_until_after_lateness(Replay* replay, uint32_t cyclesLate, int32_t* result)
{
    struct GBA* gba = replay->core->board;
    int64_t until = (int64_t)mTimingUntil(&gba->timing, &gba->audio.sampleEvent) - cyclesLate;
    if (until < INT32_MIN || until > INT32_MAX)
    {
        fail_replay(replay, "sample-event offset exceeds mGBA's replay timing range");
        return false;
    }
    *result = (int32_t)until;
    return true;
}

/* Identify byte writes whose mGBA I/O path preserves byte-wide audio semantics. */
static bool is_audio_byte_register(uint32_t address)
{
    switch (address - GBA_BASE_IO)
    {
    case GBA_REG_SOUND1CNT_HI:
    case GBA_REG_SOUND1CNT_HI + 1u:
    case GBA_REG_SOUND1CNT_X:
    case GBA_REG_SOUND1CNT_X + 1u:
    case GBA_REG_SOUND2CNT_LO:
    case GBA_REG_SOUND2CNT_LO + 1u:
    case GBA_REG_SOUND2CNT_HI:
    case GBA_REG_SOUND2CNT_HI + 1u:
    case GBA_REG_SOUND3CNT_HI:
    case GBA_REG_SOUND3CNT_HI + 1u:
    case GBA_REG_SOUND3CNT_X:
    case GBA_REG_SOUND3CNT_X + 1u:
    case GBA_REG_SOUND4CNT_LO:
    case GBA_REG_SOUND4CNT_LO + 1u:
    case GBA_REG_SOUND4CNT_HI:
    case GBA_REG_SOUND4CNT_HI + 1u:
        return true;
    default:
        return false;
    }
}

/* Identify exact halfword audio-register entrypoints supported by the recorder. */
static bool is_audio_halfword_register(uint32_t address)
{
    switch (address - GBA_BASE_IO)
    {
    case GBA_REG_SOUND1CNT_LO:
    case GBA_REG_SOUND1CNT_HI:
    case GBA_REG_SOUND1CNT_X:
    case GBA_REG_SOUND2CNT_LO:
    case GBA_REG_SOUND2CNT_HI:
    case GBA_REG_SOUND3CNT_LO:
    case GBA_REG_SOUND3CNT_HI:
    case GBA_REG_SOUND3CNT_X:
    case GBA_REG_SOUND4CNT_LO:
    case GBA_REG_SOUND4CNT_HI:
    case GBA_REG_SOUNDCNT_LO:
    case GBA_REG_SOUNDCNT_HI:
    case GBA_REG_SOUNDCNT_X:
    case GBA_REG_SOUNDBIAS:
        return true;
    default:
        return false;
    }
}

/* Dispatch each supported audio write through the same mGBA I/O entrypoint as the core. */
static bool apply_write(Replay* replay, unsigned width, uint32_t address, uint32_t value)
{
    struct GBA* gba = replay->core->board;
    if (address < GBA_BASE_IO)
        return false;
    uint32_t offset = address - GBA_BASE_IO;
    if (width == 1u)
    {
        if (value > UINT8_MAX || !is_audio_byte_register(address))
            return false;
        GBAIOWrite8(gba, offset, (uint8_t)value);
        return true;
    }
    if (width == 2u)
    {
        if (value > UINT16_MAX || !is_audio_halfword_register(address))
            return false;
        GBAIOWrite(gba, offset, (uint16_t)value);
        return true;
    }
    if (width == 4u)
    {
        switch (offset)
        {
        case GBA_REG_WAVE_RAM0_LO:
        case GBA_REG_WAVE_RAM1_LO:
        case GBA_REG_WAVE_RAM2_LO:
        case GBA_REG_WAVE_RAM3_LO:
        case GBA_REG_FIFO_A_LO:
        case GBA_REG_FIFO_B_LO:
            GBAIOWrite32(gba, offset, value);
            return true;
        default:
            return false;
        }
    }
    return false;
}

/* Consume every direct-sound FIFO selected by one timer without allowing mGBA DMA refill. */
static bool apply_timer(Replay* replay, uint32_t timer, int32_t sampleEventUntil)
{
    struct GBAAudio* audio = &((struct GBA*)replay->core->board)->audio;
    bool selectA = audio->enable && (audio->chALeft || audio->chARight) && audio->chATimer == (timer != 0u);
    bool selectB = audio->enable && (audio->chBLeft || audio->chBRight) && audio->chBTimer == (timer != 0u);
    if (!selectA && !selectB)
        return false;
    size_t observedBefore = replay->observedTimers[timer];
    if (selectA)
        GBAAudioSampleFIFOWithSampleEvent(audio, 0, 0, sampleEventUntil);
    if (selectB)
        GBAAudioSampleFIFOWithSampleEvent(audio, 1, 0, sampleEventUntil);
    return !replay->failed && replay->observedTimers[timer] > observedBefore;
}

/* Apply the requested final output mask without changing channel state progression. */
static bool apply_solo_mask(struct mCore* core, uint32_t mask)
{
    if (core->enableAudioChannel == NULL)
        return false;
    for (size_t channel = 0u; channel < 6u; ++channel)
        core->enableAudioChannel(core, channel, (mask & (1u << channel)) != 0u);
    return true;
}

/* Initialize a no-ROM GBA board exactly through mGBA's normal hardware-reset path. */
static bool initialize_replay(Replay* replay, uint32_t soloMask)
{
    replay->core = GBACoreCreate();
    if (replay->core == NULL || !replay->core->init(replay->core))
        return false;
    mCoreInitConfig(replay->core, "poryaaaa-mgba-trace-replay");
    replay->core->reset(replay->core);
    struct GBA* gba = replay->core->board;
    replay->observationSink = (struct GBAAudioObservationSink){
        .context = replay,
        .reset = reset_observations,
        .emit = emit_observation,
    };
    GBAAudioSetObservationSink(&gba->audio, &replay->observationSink);
    /* The trace already contains each FIFO refill from the full-core oracle. */
    gba->audio.chA.dmaSource = 0;
    gba->audio.chB.dmaSource = 0;
    return apply_solo_mask(replay->core, soloMask);
}

/* Release the standalone mGBA core after detaching the callback-owned replay state. */
static void deinitialize_replay(Replay* replay)
{
    if (replay->core == NULL)
        return;
    GBAAudioSetObservationSink(&((struct GBA*)replay->core->board)->audio, NULL);
    mCoreConfigDeinit(&replay->core->config);
    replay->core->deinit(replay->core);
    replay->core = NULL;
}

/* Parse and replay every trace event while mGBA's own timing queue produces native samples. */
static bool replay_trace(Replay* replay, const Options* options)
{
    FILE* input = fopen(options->inputPath, "rb");
    if (input == NULL)
    {
        fprintf(stderr, "Could not open trace: %s\n", strerror(errno));
        return false;
    }

    bool ok = true;
    bool measurementOpen = false;
    bool measurementClosed = false;
    TracePosition position = {0};
    char line[TRACE_LINE_CAPACITY];
    if (fgets(line, sizeof(line), input) == NULL || strcmp(line, "PORYAAAA_AUDIO_TRACE 1\n") != 0)
    {
        fprintf(stderr, "Trace must begin with PORYAAAA_AUDIO_TRACE 1\n");
        ok = false;
    }
    replay->lineNumber = 1u;
    if (ok)
    {
        if (fgets(line, sizeof(line), input) == NULL || strcmp(line, "CLOCK 16777216\n") != 0)
        {
            fprintf(stderr, "Trace line 2 must be CLOCK 16777216\n");
            ok = false;
        }
        replay->lineNumber = 2u;
    }

    while (ok && fgets(line, sizeof(line), input) != NULL)
    {
        replay->lineNumber++;
        size_t length = strlen(line);
        if (length == 0u || line[length - 1u] != '\n')
        {
            fail_replay(replay, "trace line is unterminated or exceeds the grammar limit");
            ok = false;
            break;
        }
        line[length - 1u] = '\0';
        char* tokens[6];
        size_t tokenCount = 0u;
        if (!split_trace_line(line, tokens, &tokenCount))
        {
            fail_replay(replay, "trace line violates the token grammar");
            ok = false;
            break;
        }

        uint64_t cycle = 0u;
        uint64_t order64 = 0u;
        if (tokenCount < 3u || !parse_u64_decimal(tokens[1], &cycle) || !parse_u64_decimal(tokens[2], &order64) ||
            order64 > UINT32_MAX)
        {
            fail_replay(replay, "trace event has an invalid cycle or order");
            ok = false;
            break;
        }
        uint32_t order = (uint32_t)order64;
        if (!advance_position(&position, cycle, order))
        {
            fail_replay(replay, "trace events are not strictly ordered");
            ok = false;
            break;
        }
        if (measurementClosed)
        {
            fail_replay(replay, "trace event follows END");
            ok = false;
            break;
        }
        if (!advance_mgba_to(replay, cycle))
        {
            ok = false;
            break;
        }

        if (strcmp(tokens[0], "BEGIN") == 0 && tokenCount == 3u)
        {
            if (measurementOpen || measurementClosed)
            {
                fail_replay(replay, "BEGIN does not open exactly one measurement");
                ok = false;
                break;
            }
            measurementOpen = true;
            continue;
        }
        if (strcmp(tokens[0], "END") == 0 && tokenCount == 3u)
        {
            if (!measurementOpen || replay->expectedHead != replay->expectedCount)
            {
                fail_replay(replay, "END has missing native samples");
                ok = false;
                break;
            }
            measurementOpen = false;
            measurementClosed = true;
            continue;
        }
        if (strcmp(tokens[0], "SAMPLE") == 0 && tokenCount == 3u)
        {
            if (!append_expected_sample(replay, cycle, measurementOpen))
            {
                fail_replay(replay, "could not retain expected SAMPLE");
                ok = false;
                break;
            }
            continue;
        }
        if (strcmp(tokens[0], "TIMER") == 0 && tokenCount == 4u)
        {
            uint64_t timer = 0u;
            uint32_t cyclesLate = (order & TRACE_ORDER_EXTENDED) != 0u ? order & TRACE_ORDER_DELAY_MASK : 0u;
            int32_t sampleEventUntil = 0;
            if (!parse_u64_decimal(tokens[3], &timer) || timer > 1u ||
                !sample_event_until_after_lateness(replay, cyclesLate, &sampleEventUntil) ||
                !apply_timer(replay, (uint32_t)timer, sampleEventUntil))
            {
                fail_replay(replay, "TIMER cannot be consumed from the traced FIFO state");
                ok = false;
                break;
            }
            continue;
        }
        if (strcmp(tokens[0], "WRITE") == 0 && tokenCount == 6u)
        {
            uint64_t width = 0u;
            uint32_t address = 0u;
            uint32_t value = 0u;
            if (!parse_u64_decimal(tokens[3], &width) || width > UINT_MAX || !parse_u32_hex(tokens[4], &address) ||
                !parse_u32_hex(tokens[5], &value) || !apply_write(replay, (unsigned)width, address, value))
            {
                fail_replay(replay, "WRITE has an unsupported address, width, or value");
                ok = false;
                break;
            }
            continue;
        }
        fail_replay(replay, "unrecognized trace event");
        ok = false;
        break;
    }

    if (ok && ferror(input))
    {
        fprintf(stderr, "Could not read trace: %s\n", strerror(errno));
        ok = false;
    }
    if (fclose(input) != 0)
        ok = false;
    if (ok && (!measurementClosed || measurementOpen || replay->expectedHead != replay->expectedCount ||
               replay->frameCount == 0u))
    {
        fprintf(stderr, "Trace requires one closed measurement and at least one captured SAMPLE\n");
        ok = false;
    }
    return ok && !replay->failed;
}

/* Emit the canonical metadata consumed by native_compare.py. */
static bool write_manifest(const char* path, const Replay* replay, uint32_t soloMask)
{
    FILE* output = fopen(path, "wb");
    if (output == NULL)
        return false;
    int written = fprintf(output,
                          "{\n"
                          "  \"format\": \"poryaaaa-native-capture\",\n"
                          "  \"version\": 1,\n"
                          "  \"source\": \"mgba-clone\",\n"
                          "  \"clock_hz\": %u,\n"
                          "  \"channels\": 2,\n"
                          "  \"sample_format\": \"s16le\",\n"
                          "  \"cycle_format\": \"u64le\",\n"
                          "  \"frame_count\": %" PRIu64 ",\n"
                          "  \"first_cycle\": %" PRIu64 ",\n"
                          "  \"last_cycle\": %" PRIu64 ",\n"
                          "  \"solo_mask\": %" PRIu32 ",\n"
                          "  \"mgba_base_revision\": \"%s\",\n"
                          "  \"mgba_observation_patch_sha256\": \"%s\"\n"
                          "}\n",
                          GBA_CLOCK_HZ,
                          replay->frameCount,
                          replay->firstCycle,
                          replay->lastCycle,
                          soloMask,
                          PORYAAAA_MGBA_BASE_REVISION,
                          PORYAAAA_MGBA_OBSERVATION_PATCH_SHA256);
    bool ok = written > 0;
    if (fflush(output) != 0)
        ok = false;
    if (fclose(output) != 0)
        ok = false;
    if (!ok)
        remove(path);
    return ok;
}

/* Publish all three artifacts only after full trace replay and manifest serialization succeed. */
int main(int argc, char** argv)
{
    Options options;
    if (!parse_options(argc, argv, &options))
    {
        print_usage(argv[0]);
        return 2;
    }

    char* pcmPath = path_with_suffix(options.outputPrefix, ".pcm");
    char* cyclesPath = path_with_suffix(options.outputPrefix, ".cycles");
    char* manifestPath = path_with_suffix(options.outputPrefix, ".json");
    char* pcmTempPath = path_with_suffix(options.outputPrefix, ".pcm.tmp");
    char* cyclesTempPath = path_with_suffix(options.outputPrefix, ".cycles.tmp");
    char* manifestTempPath = path_with_suffix(options.outputPrefix, ".json.tmp");
    if (pcmPath == NULL || cyclesPath == NULL || manifestPath == NULL || pcmTempPath == NULL ||
        cyclesTempPath == NULL || manifestTempPath == NULL)
    {
        fprintf(stderr, "Could not allocate artifact paths\n");
        free(pcmPath);
        free(cyclesPath);
        free(manifestPath);
        free(pcmTempPath);
        free(cyclesTempPath);
        free(manifestTempPath);
        return 1;
    }

    Replay replay = {0};
    replay.pcm = fopen(pcmTempPath, "wb");
    replay.cycles = fopen(cyclesTempPath, "wb");
    bool ok = replay.pcm != NULL && replay.cycles != NULL;
    if (!ok)
        fprintf(stderr, "Could not open native capture artifacts: %s\n", strerror(errno));
    if (ok && !initialize_replay(&replay, options.soloMask))
    {
        fprintf(stderr, "Could not initialize mGBA hardware-reset audio replay\n");
        ok = false;
    }
    if (ok)
        ok = replay_trace(&replay, &options);
    if (replay.pcm != NULL)
    {
        if (fflush(replay.pcm) != 0)
            ok = false;
        if (fclose(replay.pcm) != 0)
            ok = false;
    }
    replay.pcm = NULL;
    if (replay.cycles != NULL)
    {
        if (fflush(replay.cycles) != 0)
            ok = false;
        if (fclose(replay.cycles) != 0)
            ok = false;
    }
    replay.cycles = NULL;
    if (ok)
        ok = write_manifest(manifestTempPath, &replay, options.soloMask);
    deinitialize_replay(&replay);

    if (ok)
    {
        remove(pcmPath);
        remove(cyclesPath);
        remove(manifestPath);
        bool pcmPublished = rename(pcmTempPath, pcmPath) == 0;
        bool cyclesPublished = rename(cyclesTempPath, cyclesPath) == 0;
        bool manifestPublished = rename(manifestTempPath, manifestPath) == 0;
        ok = pcmPublished && cyclesPublished && manifestPublished;
        if (!ok)
        {
            remove(pcmPath);
            remove(cyclesPath);
            remove(manifestPath);
        }
    }
    if (!ok)
    {
        remove(pcmTempPath);
        remove(cyclesTempPath);
        remove(manifestTempPath);
    }
    free(replay.expectedSamples);
    free(pcmPath);
    free(cyclesPath);
    free(manifestPath);
    free(pcmTempPath);
    free(cyclesTempPath);
    free(manifestTempPath);
    return ok ? 0 : 1;
}
