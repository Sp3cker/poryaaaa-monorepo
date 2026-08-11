#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef PORYAAAA_MGBA_BASE_REVISION
#    define PORYAAAA_MGBA_BASE_REVISION "unverified"
#endif
#ifndef PORYAAAA_MGBA_OBSERVATION_PATCH_SHA256
#    define PORYAAAA_MGBA_OBSERVATION_PATCH_SHA256 "unverified"
#endif
#ifndef PORYAAAA_MGBA_SOURCE_DIRTY
#    define PORYAAAA_MGBA_SOURCE_DIRTY 0
#endif
#ifndef PORYAAAA_MGBA_NATIVE_CAPTURE_AVAILABLE
#    define PORYAAAA_MGBA_NATIVE_CAPTURE_AVAILABLE 0
#endif
#ifndef PORYAAAA_MGBA_SOURCE_POLICY
#    define PORYAAAA_MGBA_SOURCE_POLICY "unverified"
#endif
#ifndef PORYAAAA_MGBA_RECORDER_COMPILER
#    define PORYAAAA_MGBA_RECORDER_COMPILER "unverified"
#endif
#ifndef PORYAAAA_MGBA_RECORDER_COMPILE_FLAGS
#    define PORYAAAA_MGBA_RECORDER_COMPILE_FLAGS "unverified"
#endif

#if !PORYAAAA_MGBA_NATIVE_CAPTURE_AVAILABLE
#    define GBAAudioObservation PoryaaaaFrontendOnlyAudioObservation
#    define GBAAudioObservationKind PoryaaaaFrontendOnlyAudioObservationKind
#    define GBAAudioObservationSink PoryaaaaFrontendOnlyAudioObservationSink
#    define GBA_AUDIO_OBSERVATION_WRITE PORYAAAA_FRONTEND_ONLY_AUDIO_OBSERVATION_WRITE
#    define GBA_AUDIO_OBSERVATION_TIMER PORYAAAA_FRONTEND_ONLY_AUDIO_OBSERVATION_TIMER
#    define GBA_AUDIO_OBSERVATION_SAMPLE PORYAAAA_FRONTEND_ONLY_AUDIO_OBSERVATION_SAMPLE
#endif

#if defined(__clang__)
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wextra-semi"
#endif
#include <mgba/flags.h>
#include <mgba-util/audio-buffer.h>
#include <mgba/core/core.h>
#include <mgba/core/interface.h>
#include <mgba/core/log.h>
#include <mgba/core/version.h>
#include <mgba/gba/core.h>
#include <mgba/internal/gba/gba.h>
#if defined(__clang__)
#    pragma clang diagnostic pop
#endif

#if !PORYAAAA_MGBA_NATIVE_CAPTURE_AVAILABLE
#    undef GBAAudioObservation
#    undef GBAAudioObservationKind
#    undef GBAAudioObservationSink
#    undef GBA_AUDIO_OBSERVATION_WRITE
#    undef GBA_AUDIO_OBSERVATION_TIMER
#    undef GBA_AUDIO_OBSERVATION_SAMPLE
enum GBAAudioObservationKind
{
    GBA_AUDIO_OBSERVATION_WRITE,
    GBA_AUDIO_OBSERVATION_TIMER,
    GBA_AUDIO_OBSERVATION_SAMPLE,
};

struct GBAAudioObservation
{
    uint64_t cycle;
    enum GBAAudioObservationKind kind;
    uint8_t width;
    uint32_t address;
    uint32_t value;
    uint32_t cyclesLate;
    int16_t left;
    int16_t right;
};

struct GBAAudioObservationSink
{
    void* context;
    void (*reset)(void* context);
    void (*emit)(void* context, const struct GBAAudioObservation* observation);
};
#endif

#define GBA_FRAMES_PER_SECOND 60u
#define GBA_CLOCK_HZ 16777216u
#define OBSERVATION_BUFFER_CAPACITY 65536u
#define MGBA_MASTER_VOLUME 0x100u
#define AUDIO_BUFFER_FRAMES 2048u
#define TONE_DATA_SIZE 12u
#define GBA_REG_DISPSTAT 0x04000004u
#define GBA_REG_SOUNDCNT_H 0x04000082u
#define GBA_REG_IE 0x04000200u
#define GBA_REG_IME 0x04000208u
#define GBA_FIFO_RESET_BITS 0x8800u

#define AUDIO_CHANNEL_SQ1 (1u << 0u)
#define AUDIO_CHANNEL_SQ2 (1u << 1u)
#define AUDIO_CHANNEL_WAVE (1u << 2u)
#define AUDIO_CHANNEL_NOISE (1u << 3u)
#define AUDIO_CHANNEL_FIFO_A (1u << 4u)
#define AUDIO_CHANNEL_FIFO_B (1u << 5u)
#define AUDIO_CHANNEL_PSG (AUDIO_CHANNEL_SQ1 | AUDIO_CHANNEL_SQ2 | AUDIO_CHANNEL_WAVE | AUDIO_CHANNEL_NOISE)
#define AUDIO_CHANNEL_DIRECTSOUND (AUDIO_CHANNEL_FIFO_A | AUDIO_CHANNEL_FIFO_B)
#define AUDIO_CHANNEL_ALL (AUDIO_CHANNEL_PSG | AUDIO_CHANNEL_DIRECTSOUND)

#define TRACE_ORDER_EXTENDED 0x80000000u
#define TRACE_ORDER_SEQUENCE_SHIFT 16u
#define TRACE_ORDER_DELAY_MASK 0xFFFFu

#define FIXTURE_VOICE_OFFSET 0x00u
#define FIXTURE_HEADER_OFFSET 0x20u
#define FIXTURE_TRACK_OFFSET 0x40u
#define FIXTURE_RUNNER_OFFSET 0x80u

typedef enum CaptureStage
{
    CAPTURE_STAGE_FRONTEND = 1u << 0u,
    CAPTURE_STAGE_NATIVE = 1u << 1u,
} CaptureStage;

typedef struct TracePosition
{
    bool valid;
    uint64_t cycle;
    uint32_t order;
} TracePosition;

typedef struct PendingTimer
{
    bool pending;
    uint32_t sequence;
    struct GBAAudioObservation observation;
} PendingTimer;

typedef struct Options
{
    const char* romPath;
    const char* outputPath;
    const char* traceOutputPath;
    const char* nativeOutputPrefix;
    uint32_t mplayStart;
    uint32_t mplayAllStop;
    uint32_t songStart;
    uint32_t songAddress;
    uint32_t songId;
    uint32_t mplayInfo;
    uint32_t soundInfo;
    uint32_t voiceAddress;
    uint32_t fixtureAddress;
    double durationSeconds;
    double bootTimeoutSeconds;
    uint8_t note;
    uint8_t velocity;
    uint8_t volume;
    uint8_t pan;
    uint8_t requiredMaxChans;
    uint32_t enabledChannels;
    bool hasSongId;
    bool bootSong;
    bool hasSolo;
    bool hasMute;
    bool hasRequiredMaxChans;
    CaptureStage captureStage;
} Options;

typedef struct Recorder
{
    struct mAVStream stream;
    struct GBAAudioObservationSink observationSink;
    FILE* output;
    FILE* traceOutput;
    FILE* nativePcmOutput;
    FILE* nativeCyclesOutput;
    char* nativePcmPath;
    char* nativeCyclesPath;
    char* nativeManifestPath;
    char* traceTempPath;
    char* nativePcmTempPath;
    char* nativeCyclesTempPath;
    char* nativeManifestTempPath;
    struct GBAAudioObservation* observations;
    size_t observationCount;
    size_t observationCapacity;
    uint64_t framesWritten;
    uint64_t targetFrames;
    uint64_t nativeFramesWritten;
    uint64_t nativeFirstCycle;
    uint64_t nativeLastCycle;
    uint64_t nativeEndCycle;
    double sumSquares;
    int peak;
    int nativePeak;
    unsigned sampleRate;
    uint32_t pendingTimerSequence;
    TracePosition tracePosition;
    PendingTimer pendingTimers[2];
    bool capturing;
    bool writeFailed;
    bool observationOverflow;
    bool nativeCapturing;
    bool nativeFinished;
    bool nativePublished;
    bool nativeWriteFailed;
} Recorder;

/* Shows the symbol-address contract needed to invoke the ROM's real MP2K driver. */
static void print_usage(const char* program)
{
    fprintf(stderr,
            "Usage: %s --rom FILE --output FILE --mplay-start ADDRESS\n"
            "          --mplay-info ADDRESS --sound-info ADDRESS\n"
            "          --voice-address ADDRESS [options]\n"
            "   or: %s --rom FILE --output FILE --song-start ADDRESS\n"
            "          --song-address ADDRESS --song-id N\n"
            "          --mplay-info ADDRESS --sound-info ADDRESS [options]\n"
            "   or: %s --rom FILE --output FILE --boot-song\n"
            "          --song-address ADDRESS --mplay-info ADDRESS\n"
            "          --sound-info ADDRESS [options]\n"
            "\n"
            "--output is required for frontend/both capture. Native/both capture also\n"
            "requires --trace-output and --native-output-prefix.\n"
            "\n"
            "Options:\n"
            "  --capture-stage STAGE     frontend (default), native, or both\n"
            "  --trace-output FILE       Native trace output\n"
            "  --native-output-prefix P  Native P.pcm, P.cycles, and P.json artifacts\n"
            "  --duration-seconds S      Capture length (default: 2)\n"
            "  --boot-timeout-seconds S  MP2K initialization timeout (default: 10)\n"
            "  --note N                  MIDI note 0-127 (default: 60)\n"
            "  --velocity N              MP2K velocity 0-127 (default: 127)\n"
            "  --volume N                Track volume 0-127 (default: 127)\n"
            "  --pan N                   MP2K pan 0-127 (default: 64)\n"
            "  --solo LIST               Keep only named hardware channels\n"
            "  --mute LIST               Disable named hardware channels\n"
            "                            Names: sq1,sq2,wave,noise,fifo-a,fifo-b,\n"
            "                            psg,directsound,all (comma-separated)\n"
            "  --mplay-all-stop ADDRESS  Stop existing players before a voice fixture\n"
            "  --require-max-chans N     Fail unless MP2K configures this PCM channel count\n"
            "  --fixture-address ADDRESS EWRAM scratch base (default: 0x0203F000)\n",
            program,
            program,
            program);
}

/* Parses an unsigned command-line value while rejecting truncation and junk. */
static bool parse_u32(const char* text, uint32_t* result)
{
    char* end = NULL;
    errno = 0;
    unsigned long long value = strtoull(text, &end, 0);
    if (errno != 0 || end == text || *end != '\0' || value > UINT32_MAX)
        return false;
    *result = (uint32_t)value;
    return true;
}

/* Parses a bounded MP2K byte value used by the synthetic track. */
static bool parse_midi_byte(const char* text, uint8_t* result)
{
    uint32_t value = 0;
    if (!parse_u32(text, &value) || value > 127u)
        return false;
    *result = (uint8_t)value;
    return true;
}

/* Parses a positive duration so frame-count calculations remain meaningful. */
static bool parse_duration(const char* text, double* result)
{
    char* end = NULL;
    errno = 0;
    double value = strtod(text, &end);
    if (errno != 0 || end == text || *end != '\0' || !isfinite(value) || value <= 0.0)
        return false;
    *result = value;
    return true;
}

/* Parses a capture stage without letting native artifacts masquerade as WAV output. */
static bool parse_capture_stage(const char* text, CaptureStage* result)
{
    if (strcmp(text, "frontend") == 0)
        *result = CAPTURE_STAGE_FRONTEND;
    else if (strcmp(text, "native") == 0)
        *result = CAPTURE_STAGE_NATIVE;
    else if (strcmp(text, "both") == 0)
        *result = CAPTURE_STAGE_FRONTEND | CAPTURE_STAGE_NATIVE;
    else
        return false;
    return true;
}

/* Maps stable channel names to mGBA's six GBA hardware audio channels. */
static bool parse_channel_mask(const char* text, uint32_t* result)
{
    char buffer[128];
    size_t length = strlen(text);
    if (length == 0u || length >= sizeof(buffer))
        return false;
    memcpy(buffer, text, length + 1u);

    uint32_t mask = 0u;
    for (char* name = strtok(buffer, ","); name != NULL; name = strtok(NULL, ","))
    {
        if (strcmp(name, "ch1") == 0 || strcmp(name, "sq1") == 0 || strcmp(name, "square1") == 0)
            mask |= AUDIO_CHANNEL_SQ1;
        else if (strcmp(name, "ch2") == 0 || strcmp(name, "sq2") == 0 || strcmp(name, "square2") == 0)
            mask |= AUDIO_CHANNEL_SQ2;
        else if (strcmp(name, "ch3") == 0 || strcmp(name, "wave") == 0)
            mask |= AUDIO_CHANNEL_WAVE;
        else if (strcmp(name, "ch4") == 0 || strcmp(name, "noise") == 0)
            mask |= AUDIO_CHANNEL_NOISE;
        else if (strcmp(name, "fifo-a") == 0 || strcmp(name, "fifoa") == 0 || strcmp(name, "dma-a") == 0)
            mask |= AUDIO_CHANNEL_FIFO_A;
        else if (strcmp(name, "fifo-b") == 0 || strcmp(name, "fifob") == 0 || strcmp(name, "dma-b") == 0)
            mask |= AUDIO_CHANNEL_FIFO_B;
        else if (strcmp(name, "psg") == 0)
            mask |= AUDIO_CHANNEL_PSG;
        else if (strcmp(name, "directsound") == 0 || strcmp(name, "fifo") == 0 || strcmp(name, "dma") == 0)
            mask |= AUDIO_CHANNEL_DIRECTSOUND;
        else if (strcmp(name, "all") == 0 || strcmp(name, "full") == 0)
            mask |= AUDIO_CHANNEL_ALL;
        else
            return false;
    }

    *result = mask;
    return mask != 0u;
}

/* Reads all recorder arguments without introducing a second configuration format. */
static bool parse_options(int argc, char** argv, Options* options)
{
    *options = (Options){
        .fixtureAddress = 0x0203F000u,
        .durationSeconds = 2.0,
        .bootTimeoutSeconds = 10.0,
        .note = 60u,
        .velocity = 127u,
        .volume = 127u,
        .pan = 64u,
        .enabledChannels = AUDIO_CHANNEL_ALL,
        .captureStage = CAPTURE_STAGE_FRONTEND,
    };

    for (int i = 1; i < argc; ++i)
    {
        const char* name = argv[i];
        if (strcmp(name, "--boot-song") == 0)
        {
            options->bootSong = true;
            continue;
        }
        if (i + 1 >= argc)
            return false;
        const char* value = argv[++i];
        if (strcmp(name, "--rom") == 0)
            options->romPath = value;
        else if (strcmp(name, "--output") == 0)
            options->outputPath = value;
        else if (strcmp(name, "--trace-output") == 0)
            options->traceOutputPath = value;
        else if (strcmp(name, "--native-output-prefix") == 0)
            options->nativeOutputPrefix = value;
        else if (strcmp(name, "--capture-stage") == 0)
        {
            if (!parse_capture_stage(value, &options->captureStage))
                return false;
        }
        else if (strcmp(name, "--mplay-start") == 0)
        {
            if (!parse_u32(value, &options->mplayStart))
                return false;
        }
        else if (strcmp(name, "--mplay-all-stop") == 0)
        {
            if (!parse_u32(value, &options->mplayAllStop))
                return false;
        }
        else if (strcmp(name, "--song-start") == 0)
        {
            if (!parse_u32(value, &options->songStart))
                return false;
        }
        else if (strcmp(name, "--song-address") == 0)
        {
            if (!parse_u32(value, &options->songAddress))
                return false;
        }
        else if (strcmp(name, "--song-id") == 0)
        {
            if (!parse_u32(value, &options->songId) || options->songId > UINT16_MAX)
                return false;
            options->hasSongId = true;
        }
        else if (strcmp(name, "--mplay-info") == 0)
        {
            if (!parse_u32(value, &options->mplayInfo))
                return false;
        }
        else if (strcmp(name, "--sound-info") == 0)
        {
            if (!parse_u32(value, &options->soundInfo))
                return false;
        }
        else if (strcmp(name, "--voice-address") == 0)
        {
            if (!parse_u32(value, &options->voiceAddress))
                return false;
        }
        else if (strcmp(name, "--fixture-address") == 0)
        {
            if (!parse_u32(value, &options->fixtureAddress))
                return false;
        }
        else if (strcmp(name, "--require-max-chans") == 0)
        {
            uint32_t maxChans = 0u;
            if (!parse_u32(value, &maxChans) || maxChans == 0u || maxChans > 12u)
                return false;
            options->requiredMaxChans = (uint8_t)maxChans;
            options->hasRequiredMaxChans = true;
        }
        else if (strcmp(name, "--duration-seconds") == 0)
        {
            if (!parse_duration(value, &options->durationSeconds))
                return false;
        }
        else if (strcmp(name, "--boot-timeout-seconds") == 0)
        {
            if (!parse_duration(value, &options->bootTimeoutSeconds))
                return false;
        }
        else if (strcmp(name, "--note") == 0)
        {
            if (!parse_midi_byte(value, &options->note))
                return false;
        }
        else if (strcmp(name, "--velocity") == 0)
        {
            if (!parse_midi_byte(value, &options->velocity))
                return false;
        }
        else if (strcmp(name, "--volume") == 0)
        {
            if (!parse_midi_byte(value, &options->volume))
                return false;
        }
        else if (strcmp(name, "--pan") == 0)
        {
            if (!parse_midi_byte(value, &options->pan))
                return false;
        }
        else if (strcmp(name, "--solo") == 0)
        {
            uint32_t mask = 0u;
            if (options->hasSolo || options->hasMute || !parse_channel_mask(value, &mask))
                return false;
            options->enabledChannels = mask;
            options->hasSolo = true;
        }
        else if (strcmp(name, "--mute") == 0)
        {
            uint32_t mask = 0u;
            if (options->hasSolo || !parse_channel_mask(value, &mask))
                return false;
            options->enabledChannels &= ~mask;
            options->hasMute = true;
        }
        else
            return false;
    }

    bool voiceMode = !options->bootSong && options->mplayStart != 0u && options->voiceAddress != 0u &&
                     options->songStart == 0u && options->songAddress == 0u && !options->hasSongId;
    bool songMode = options->songStart != 0u && options->songAddress != 0u && options->hasSongId &&
                    !options->bootSong && options->mplayStart == 0u && options->voiceAddress == 0u;
    bool bootSongMode = options->bootSong && options->songAddress != 0u && options->songStart == 0u &&
                        options->mplayStart == 0u && options->voiceAddress == 0u && !options->hasSongId;
    bool frontendCapture = (options->captureStage & CAPTURE_STAGE_FRONTEND) != 0u;
    bool nativeCapture = (options->captureStage & CAPTURE_STAGE_NATIVE) != 0u;
    bool outputConfiguration =
        (!frontendCapture || options->outputPath != NULL) &&
        (!nativeCapture || (options->traceOutputPath != NULL && options->nativeOutputPrefix != NULL));
    return options->romPath != NULL && outputConfiguration && options->mplayInfo != 0u && options->soundInfo != 0u &&
           (voiceMode || songMode || bootSongMode);
}

/* Applies a final-output channel mask without changing MP2K voice scheduling. */
static bool apply_audio_channel_mask(struct mCore* core, uint32_t mask)
{
    if (core->enableAudioChannel == NULL)
        return false;
    for (size_t channel = 0u; channel < 6u; ++channel)
        core->enableAudioChannel(core, channel, (mask & (1u << channel)) != 0u);
    return true;
}

/* Writes one stereo frame and updates capture health statistics. */
static void write_audio_frame(Recorder* recorder, int16_t left, int16_t right)
{
    if (!recorder->capturing || recorder->framesWritten >= recorder->targetFrames || recorder->writeFailed)
        return;

    uint8_t bytes[4] = {
        (uint8_t)((uint16_t)left & 0xFFu),
        (uint8_t)(((uint16_t)left >> 8u) & 0xFFu),
        (uint8_t)((uint16_t)right & 0xFFu),
        (uint8_t)(((uint16_t)right >> 8u) & 0xFFu),
    };
    if (fwrite(bytes, sizeof(bytes), 1u, recorder->output) != 1u)
    {
        recorder->writeFailed = true;
        return;
    }

    int leftMagnitude = left < 0 ? -(int)left : (int)left;
    int rightMagnitude = right < 0 ? -(int)right : (int)right;
    if (leftMagnitude > recorder->peak)
        recorder->peak = leftMagnitude;
    if (rightMagnitude > recorder->peak)
        recorder->peak = rightMagnitude;

    double leftSample = (double)left;
    double rightSample = (double)right;
    recorder->sumSquares += leftSample * leftSample + rightSample * rightSample;
    recorder->framesWritten++;
}

/* Drains pinned mGBA's interleaved frontend buffer for legacy WAV capture. */
static void post_audio_buffer(struct mAVStream* stream, struct mAudioBuffer* buffer)
{
    Recorder* recorder = (Recorder*)stream;
    if (!recorder->capturing)
    {
        mAudioBufferClear(buffer);
        return;
    }

    int16_t samples[AUDIO_BUFFER_FRAMES * 2u];
    while (recorder->framesWritten < recorder->targetFrames && !recorder->writeFailed)
    {
        size_t available = mAudioBufferAvailable(buffer);
        if (available == 0u)
            break;

        uint64_t remaining = recorder->targetFrames - recorder->framesWritten;
        size_t frames = available;
        if (frames > AUDIO_BUFFER_FRAMES)
            frames = AUDIO_BUFFER_FRAMES;
        if ((uint64_t)frames > remaining)
            frames = (size_t)remaining;

        if (mAudioBufferRead(buffer, samples, frames) != frames)
        {
            recorder->writeFailed = true;
            break;
        }
        for (size_t i = 0u; i < frames; ++i)
            write_audio_frame(recorder, samples[i * 2u], samples[i * 2u + 1u]);
    }
}

/* Writes or rewrites the canonical 44-byte stereo PCM WAV header. */
static bool write_wav_header(FILE* output, unsigned sampleRate, uint64_t frames)
{
    uint64_t dataBytes64 = frames * 4u;
    if (dataBytes64 > UINT32_MAX - 36u)
        return false;

    uint32_t dataBytes = (uint32_t)dataBytes64;
    uint32_t riffBytes = dataBytes + 36u;
    uint32_t byteRate = sampleRate * 4u;
    uint8_t header[44] = {
        'R',
        'I',
        'F',
        'F',
        (uint8_t)(riffBytes & 0xFFu),
        (uint8_t)((riffBytes >> 8u) & 0xFFu),
        (uint8_t)((riffBytes >> 16u) & 0xFFu),
        (uint8_t)((riffBytes >> 24u) & 0xFFu),
        'W',
        'A',
        'V',
        'E',
        'f',
        'm',
        't',
        ' ',
        16u,
        0u,
        0u,
        0u,
        1u,
        0u,
        2u,
        0u,
        (uint8_t)(sampleRate & 0xFFu),
        (uint8_t)((sampleRate >> 8u) & 0xFFu),
        (uint8_t)((sampleRate >> 16u) & 0xFFu),
        (uint8_t)((sampleRate >> 24u) & 0xFFu),
        (uint8_t)(byteRate & 0xFFu),
        (uint8_t)((byteRate >> 8u) & 0xFFu),
        (uint8_t)((byteRate >> 16u) & 0xFFu),
        (uint8_t)((byteRate >> 24u) & 0xFFu),
        4u,
        0u,
        16u,
        0u,
        'd',
        'a',
        't',
        'a',
        (uint8_t)(dataBytes & 0xFFu),
        (uint8_t)((dataBytes >> 8u) & 0xFFu),
        (uint8_t)((dataBytes >> 16u) & 0xFFu),
        (uint8_t)((dataBytes >> 24u) & 0xFFu),
    };

    return fseek(output, 0, SEEK_SET) == 0 && fwrite(header, sizeof(header), 1u, output) == 1u;
}

typedef struct Sha256
{
    uint32_t state[8];
    uint64_t bits;
    size_t used;
    uint8_t block[64];
} Sha256;

/* Rotate right without invoking undefined shifts at the word width. */
static uint32_t rotate_right(uint32_t value, unsigned amount)
{
    return (value >> amount) | (value << (32u - amount));
}

/* Compress one SHA-256 block for self-describing native capture metadata. */
static void sha256_transform(Sha256* sha)
{
    static const uint32_t constants[64] = {
        0x428A2F98u, 0x71374491u, 0xB5C0FBCFu, 0xE9B5DBA5u, 0x3956C25Bu, 0x59F111F1u, 0x923F82A4u, 0xAB1C5ED5u,
        0xD807AA98u, 0x12835B01u, 0x243185BEu, 0x550C7DC3u, 0x72BE5D74u, 0x80DEB1FEu, 0x9BDC06A7u, 0xC19BF174u,
        0xE49B69C1u, 0xEFBE4786u, 0x0FC19DC6u, 0x240CA1CCu, 0x2DE92C6Fu, 0x4A7484AAu, 0x5CB0A9DCu, 0x76F988DAu,
        0x983E5152u, 0xA831C66Du, 0xB00327C8u, 0xBF597FC7u, 0xC6E00BF3u, 0xD5A79147u, 0x06CA6351u, 0x14292967u,
        0x27B70A85u, 0x2E1B2138u, 0x4D2C6DFCu, 0x53380D13u, 0x650A7354u, 0x766A0ABBu, 0x81C2C92Eu, 0x92722C85u,
        0xA2BFE8A1u, 0xA81A664Bu, 0xC24B8B70u, 0xC76C51A3u, 0xD192E819u, 0xD6990624u, 0xF40E3585u, 0x106AA070u,
        0x19A4C116u, 0x1E376C08u, 0x2748774Cu, 0x34B0BCB5u, 0x391C0CB3u, 0x4ED8AA4Au, 0x5B9CCA4Fu, 0x682E6FF3u,
        0x748F82EEu, 0x78A5636Fu, 0x84C87814u, 0x8CC70208u, 0x90BEFFFAu, 0xA4506CEBu, 0xBEF9A3F7u, 0xC67178F2u,
    };
    uint32_t words[64];
    for (unsigned index = 0u; index < 16u; ++index)
    {
        unsigned offset = index * 4u;
        words[index] = ((uint32_t)sha->block[offset] << 24u) | ((uint32_t)sha->block[offset + 1u] << 16u) |
                       ((uint32_t)sha->block[offset + 2u] << 8u) | sha->block[offset + 3u];
    }
    for (unsigned index = 16u; index < 64u; ++index)
    {
        uint32_t s0 =
            rotate_right(words[index - 15u], 7u) ^ rotate_right(words[index - 15u], 18u) ^ (words[index - 15u] >> 3u);
        uint32_t s1 =
            rotate_right(words[index - 2u], 17u) ^ rotate_right(words[index - 2u], 19u) ^ (words[index - 2u] >> 10u);
        words[index] = words[index - 16u] + s0 + words[index - 7u] + s1;
    }

    uint32_t a = sha->state[0];
    uint32_t b = sha->state[1];
    uint32_t c = sha->state[2];
    uint32_t d = sha->state[3];
    uint32_t e = sha->state[4];
    uint32_t f = sha->state[5];
    uint32_t g = sha->state[6];
    uint32_t h = sha->state[7];
    for (unsigned index = 0u; index < 64u; ++index)
    {
        uint32_t s1 = rotate_right(e, 6u) ^ rotate_right(e, 11u) ^ rotate_right(e, 25u);
        uint32_t choice = (e & f) ^ (~e & g);
        uint32_t temporary1 = h + s1 + choice + constants[index] + words[index];
        uint32_t s0 = rotate_right(a, 2u) ^ rotate_right(a, 13u) ^ rotate_right(a, 22u);
        uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
        uint32_t temporary2 = s0 + majority;
        h = g;
        g = f;
        f = e;
        e = d + temporary1;
        d = c;
        c = b;
        b = a;
        a = temporary1 + temporary2;
    }
    sha->state[0] += a;
    sha->state[1] += b;
    sha->state[2] += c;
    sha->state[3] += d;
    sha->state[4] += e;
    sha->state[5] += f;
    sha->state[6] += g;
    sha->state[7] += h;
}

/* Hash arbitrary artifacts without depending on the host's command-line tools. */
static void sha256_init(Sha256* sha)
{
    *sha = (Sha256){
        .state =
            {0x6A09E667u, 0xBB67AE85u, 0x3C6EF372u, 0xA54FF53Au, 0x510E527Fu, 0x9B05688Cu, 0x1F83D9ABu, 0x5BE0CD19u},
    };
}

static void sha256_update(Sha256* sha, const uint8_t* data, size_t count)
{
    while (count != 0u)
    {
        size_t space = sizeof(sha->block) - sha->used;
        size_t copy = count < space ? count : space;
        memcpy(sha->block + sha->used, data, copy);
        sha->used += copy;
        sha->bits += (uint64_t)copy * 8u;
        data += copy;
        count -= copy;
        if (sha->used == sizeof(sha->block))
        {
            sha256_transform(sha);
            sha->used = 0u;
        }
    }
}

static void sha256_finish(Sha256* sha, char output[65])
{
    static const char hex[] = "0123456789abcdef";
    uint64_t bits = sha->bits;
    sha->block[sha->used++] = 0x80u;
    if (sha->used > 56u)
    {
        memset(sha->block + sha->used, 0, sizeof(sha->block) - sha->used);
        sha256_transform(sha);
        sha->used = 0u;
    }
    memset(sha->block + sha->used, 0, 56u - sha->used);
    for (unsigned index = 0u; index < 8u; ++index)
        sha->block[63u - index] = (uint8_t)(bits >> (index * 8u));
    sha256_transform(sha);
    for (unsigned index = 0u; index < 32u; ++index)
    {
        uint8_t byte = (uint8_t)(sha->state[index / 4u] >> (24u - (index % 4u) * 8u));
        output[index * 2u] = hex[byte >> 4u];
        output[index * 2u + 1u] = hex[byte & 0x0Fu];
    }
    output[64] = '\0';
}

/* Produce the portable SHA-256 values carried in the mGBA native manifest. */
static bool sha256_file(const char* path, char output[65])
{
    FILE* input = fopen(path, "rb");
    if (input == NULL)
        return false;

    Sha256 sha;
    sha256_init(&sha);
    uint8_t buffer[4096];
    size_t read = 0u;
    while ((read = fread(buffer, 1u, sizeof(buffer), input)) != 0u)
        sha256_update(&sha, buffer, read);
    bool ok = !ferror(input);
    if (fclose(input) != 0)
        ok = false;
    if (ok)
        sha256_finish(&sha, output);
    return ok;
}

/* Writes one manifest string without allowing compiler flags to break JSON. */
static bool write_json_string(FILE* output, const char* text)
{
    if (fputc('"', output) == EOF)
        return false;
    for (const unsigned char* cursor = (const unsigned char*)text; *cursor != '\0'; ++cursor)
    {
        if (*cursor == '"' || *cursor == '\\')
        {
            if (fputc('\\', output) == EOF || fputc(*cursor, output) == EOF)
                return false;
        }
        else if (*cursor < 0x20u)
        {
            if (fprintf(output, "\\u%04X", *cursor) < 0)
                return false;
        }
        else if (fputc(*cursor, output) == EOF)
        {
            return false;
        }
    }
    return fputc('"', output) != EOF;
}

/* Allocates one sibling artifact name from an unbounded native output prefix. */
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

/* Reset only recorder-owned buffered observations when the core resets. */
static void reset_observations(void* context)
{
    Recorder* recorder = context;
    recorder->observationCount = 0u;
    recorder->observationOverflow = false;
    recorder->pendingTimerSequence = 0u;
    memset(recorder->pendingTimers, 0, sizeof(recorder->pendingTimers));
}

/* Copies a synchronous mGBA observation into caller-owned preallocated storage. */
static void record_observation(void* context, const struct GBAAudioObservation* observation)
{
    Recorder* recorder = context;
    if (recorder->observationCount == recorder->observationCapacity)
    {
        recorder->observationOverflow = true;
        return;
    }
    recorder->observations[recorder->observationCount++] = *observation;
}

/* Assigns the trace's only same-cycle order domain outside the audio callback. */
static bool next_trace_position(Recorder* recorder, uint64_t cycle, uint32_t* order)
{
    if (!recorder->tracePosition.valid || cycle > recorder->tracePosition.cycle)
    {
        recorder->tracePosition = (TracePosition){
            .valid = true,
            .cycle = cycle,
            .order = 0u,
        };
    }
    else if (cycle == recorder->tracePosition.cycle)
    {
        if (recorder->tracePosition.order == 0x7FFFu)
            return false;
        recorder->tracePosition.order++;
    }
    else
    {
        return false;
    }
    *order = TRACE_ORDER_EXTENDED | (recorder->tracePosition.order << TRACE_ORDER_SEQUENCE_SHIFT);
    return true;
}

/* Writes an ordered marker owned by the recorder rather than by mGBA. */
static bool write_trace_marker(Recorder* recorder, const char* marker, uint64_t cycle)
{
    uint32_t order = 0u;
    return next_trace_position(recorder, cycle, &order) &&
           fprintf(recorder->traceOutput, "%s %" PRIu64 " %" PRIu32 "\n", marker, cycle, order) > 0;
}

/* Serializes one register write using the replay reader's canonical grammar. */
static bool write_trace_write(Recorder* recorder, const struct GBAAudioObservation* observation)
{
    uint32_t order = 0u;
    return next_trace_position(recorder, observation->cycle, &order) &&
           fprintf(recorder->traceOutput,
                   "WRITE %" PRIu64 " %" PRIu32 " %u 0x%08" PRIX32 " 0x%08" PRIX32 "\n",
                   observation->cycle,
                   order,
                   (unsigned)observation->width,
                   observation->address,
                   observation->value) > 0;
}

/* Serializes timer callback lateness without adding a second trace time domain. */
static bool write_trace_timer(Recorder* recorder, const struct GBAAudioObservation* observation)
{
    uint32_t order = 0u;
    if (observation->cyclesLate > TRACE_ORDER_DELAY_MASK || !next_trace_position(recorder, observation->cycle, &order))
        return false;
    order |= observation->cyclesLate;
    return fprintf(recorder->traceOutput,
                   "TIMER %" PRIu64 " %" PRIu32 " %" PRIu32 "\n",
                   observation->cycle,
                   order,
                   observation->value) > 0;
}

/* Writes a logical native sample event; PCM payload stays in the binary artifact. */
static bool write_trace_sample(Recorder* recorder, const struct GBAAudioObservation* observation)
{
    uint32_t order = 0u;
    return next_trace_position(recorder, observation->cycle, &order) &&
           fprintf(recorder->traceOutput, "SAMPLE %" PRIu64 " %" PRIu32 "\n", observation->cycle, order) > 0;
}

/* Writes the native PCM and cycle records directly from one SAMPLE observation. */
static bool write_native_sample(Recorder* recorder, const struct GBAAudioObservation* observation)
{
    if (recorder->nativeFramesWritten != 0u && observation->cycle <= recorder->nativeLastCycle)
        return false;
    uint16_t left = (uint16_t)observation->left;
    uint16_t right = (uint16_t)observation->right;
    uint8_t pcm[4] = {
        (uint8_t)(left & 0xFFu),
        (uint8_t)(left >> 8u),
        (uint8_t)(right & 0xFFu),
        (uint8_t)(right >> 8u),
    };
    uint8_t cycles[8];
    for (unsigned index = 0u; index < sizeof(cycles); ++index)
        cycles[index] = (uint8_t)(observation->cycle >> (index * 8u));
    if (fwrite(pcm, sizeof(pcm), 1u, recorder->nativePcmOutput) != 1u ||
        fwrite(cycles, sizeof(cycles), 1u, recorder->nativeCyclesOutput) != 1u)
    {
        return false;
    }
    int leftMagnitude = observation->left < 0 ? -(int)observation->left : (int)observation->left;
    int rightMagnitude = observation->right < 0 ? -(int)observation->right : (int)observation->right;
    if (leftMagnitude > recorder->nativePeak)
        recorder->nativePeak = leftMagnitude;
    if (rightMagnitude > recorder->nativePeak)
        recorder->nativePeak = rightMagnitude;
    if (recorder->nativeFramesWritten == 0u)
        recorder->nativeFirstCycle = observation->cycle;
    recorder->nativeLastCycle = observation->cycle;
    recorder->nativeFramesWritten++;
    return true;
}

/* Flushes deferred per-FIFO timer events in the order mGBA first observed them. */
static bool flush_pending_timers(Recorder* recorder)
{
    for (;;)
    {
        int selected = -1;
        for (unsigned index = 0u; index < 2u; ++index)
        {
            if (recorder->pendingTimers[index].pending &&
                (selected < 0 ||
                 recorder->pendingTimers[index].sequence < recorder->pendingTimers[(unsigned)selected].sequence))
            {
                selected = (int)index;
            }
        }
        if (selected < 0)
            return true;
        PendingTimer* timer = &recorder->pendingTimers[(unsigned)selected];
        timer->pending = false;
        if (!write_trace_timer(recorder, &timer->observation))
            return false;
    }
}

/* Identifies the only writes allowed to stay before deferred same-cycle timers. */
static bool is_fifo_write(const struct GBAAudioObservation* observation)
{
    return observation->kind == GBA_AUDIO_OBSERVATION_WRITE && observation->width == 4u &&
           (observation->address == 0x040000A0u || observation->address == 0x040000A4u);
}

/* A FIFO write may only bypass timers whose consumption point has this same cycle. */
static bool timers_match_fifo_cycle(const Recorder* recorder, uint64_t cycle)
{
    for (unsigned index = 0u; index < 2u; ++index)
    {
        if (recorder->pendingTimers[index].pending && recorder->pendingTimers[index].observation.cycle != cycle)
            return false;
    }
    return true;
}

/* Finds the logical underflow cycle for a contiguous DMA FIFO refill burst. */
static bool pending_timer_cycle(const Recorder* recorder, uint64_t* cycle)
{
    bool found = false;
    for (unsigned index = 0u; index < 2u; ++index)
    {
        if (recorder->pendingTimers[index].pending &&
            (!found || recorder->pendingTimers[index].observation.cycle < *cycle))
        {
            *cycle = recorder->pendingTimers[index].observation.cycle;
            found = true;
        }
    }
    return found;
}

/* Applies the required FIFO-DMA flattening before recording a trace observation. */
static bool write_observation(Recorder* recorder, const struct GBAAudioObservation* observation)
{
    if (observation->kind == GBA_AUDIO_OBSERVATION_TIMER)
    {
        if (observation->value > 1u)
            return false;
        if (!timers_match_fifo_cycle(recorder, observation->cycle))
        {
            if (!flush_pending_timers(recorder))
                return false;
        }
        PendingTimer* timer = &recorder->pendingTimers[observation->value];
        if (!timer->pending)
        {
            timer->pending = true;
            timer->sequence = ++recorder->pendingTimerSequence;
            timer->observation = *observation;
        }
        return true;
    }

    if (is_fifo_write(observation))
    {
        uint64_t cycle = 0u;
        if (pending_timer_cycle(recorder, &cycle))
        {
            struct GBAAudioObservation dmaWrite = *observation;
            dmaWrite.cycle = cycle;
            return write_trace_write(recorder, &dmaWrite);
        }
    }
    if (!flush_pending_timers(recorder))
        return false;
    if (observation->kind == GBA_AUDIO_OBSERVATION_WRITE)
        return write_trace_write(recorder, observation);
    if (observation->kind == GBA_AUDIO_OBSERVATION_SAMPLE)
    {
        if (!write_trace_sample(recorder, observation))
            return false;
        return !recorder->nativeCapturing || write_native_sample(recorder, observation);
    }
    return false;
}

/* Restores logical cycle order while preserving callback order within each cycle. */
static void sort_observations(Recorder* recorder)
{
    for (size_t index = 1u; index < recorder->observationCount; ++index)
    {
        struct GBAAudioObservation observation = recorder->observations[index];
        size_t insertion = index;
        while (insertion > 0u && recorder->observations[insertion - 1u].cycle > observation.cycle)
        {
            recorder->observations[insertion] = recorder->observations[insertion - 1u];
            --insertion;
        }
        recorder->observations[insertion] = observation;
    }
}

/* Reads mGBA's absolute GBA timing domain for recorder-owned capture markers. */
static uint64_t current_gba_cycle(const struct mCore* core)
{
    const struct GBA* gba = core->board;
    return mTimingGlobalTime(&gba->timing);
}

/* Finds the first logical sample cycle that mGBA has not emitted yet. */
static uint64_t next_sample_cycle(const struct mCore* core)
{
    const struct GBA* gba = core->board;
    const struct GBAAudio* audio = &gba->audio;
    int64_t timestamp = (int64_t)audio->lastSample + (int64_t)audio->sampleIndex * audio->sampleInterval;
    int64_t delta = timestamp - (int64_t)mTimingCurrentTime(&gba->timing);
    uint64_t current = current_gba_cycle(core);
    if (delta < 0)
    {
        uint64_t magnitude = (uint64_t)(-delta);
        return magnitude > current ? 0u : current - magnitude;
    }
    return current + (uint64_t)delta;
}

/* Drains only cycles older than mGBA's next possible delayed sample callback. */
static bool drain_observations(const struct mCore* core, Recorder* recorder)
{
    if (recorder->observationOverflow)
        return false;
    if (recorder->nativeFinished)
    {
        recorder->observationCount = 0u;
        return true;
    }

    sort_observations(recorder);
    uint64_t safeCycle = next_sample_cycle(core);
    size_t processed = 0u;
    while (processed < recorder->observationCount && recorder->observations[processed].cycle < safeCycle)
    {
        const struct GBAAudioObservation* observation = &recorder->observations[processed];
        if (recorder->nativeCapturing && observation->cycle >= recorder->nativeEndCycle)
        {
            if (!flush_pending_timers(recorder) || !write_trace_marker(recorder, "END", recorder->nativeEndCycle))
            {
                recorder->nativeWriteFailed = true;
                return false;
            }
            recorder->nativeCapturing = false;
            recorder->nativeFinished = true;
            recorder->observationCount = 0u;
            return true;
        }
        if (!write_observation(recorder, observation))
        {
            recorder->nativeWriteFailed = true;
            return false;
        }
        ++processed;
    }
    if (!flush_pending_timers(recorder))
    {
        recorder->nativeWriteFailed = true;
        return false;
    }
    recorder->observationCount -= processed;
    memmove(recorder->observations,
            recorder->observations + processed,
            recorder->observationCount * sizeof(*recorder->observations));
    return true;
}

/* Opens the measurement interval after every preceding reset/setup event was traced. */
static bool begin_native_capture(struct mCore* core, Recorder* recorder, const Options* options)
{
    if (!drain_observations(core, recorder))
        return false;
    uint64_t startCycle = next_sample_cycle(core);
    double captureCycles = options->durationSeconds * (double)GBA_CLOCK_HZ;
    if (captureCycles > (double)(UINT64_MAX - startCycle))
        return false;
    uint64_t durationCycles = (uint64_t)(captureCycles + 0.5);
    if (durationCycles == 0u || !write_trace_marker(recorder, "BEGIN", startCycle))
        return false;
    recorder->nativeEndCycle = startCycle + durationCycles;
    recorder->nativeCapturing = true;
    return true;
}

/* Opens all native outputs before reset; the observation callback only copies records. */
static bool prepare_native_capture(Recorder* recorder, const Options* options)
{
    recorder->observationCapacity = OBSERVATION_BUFFER_CAPACITY;
    recorder->observations = malloc(recorder->observationCapacity * sizeof(*recorder->observations));
    recorder->nativePcmPath = path_with_suffix(options->nativeOutputPrefix, ".pcm");
    recorder->nativeCyclesPath = path_with_suffix(options->nativeOutputPrefix, ".cycles");
    recorder->nativeManifestPath = path_with_suffix(options->nativeOutputPrefix, ".json");
    recorder->traceTempPath = path_with_suffix(options->traceOutputPath, ".tmp");
    recorder->nativePcmTempPath = path_with_suffix(options->nativeOutputPrefix, ".pcm.tmp");
    recorder->nativeCyclesTempPath = path_with_suffix(options->nativeOutputPrefix, ".cycles.tmp");
    recorder->nativeManifestTempPath = path_with_suffix(options->nativeOutputPrefix, ".json.tmp");
    if (recorder->observations == NULL || recorder->nativePcmPath == NULL || recorder->nativeCyclesPath == NULL ||
        recorder->nativeManifestPath == NULL || recorder->traceTempPath == NULL ||
        recorder->nativePcmTempPath == NULL || recorder->nativeCyclesTempPath == NULL ||
        recorder->nativeManifestTempPath == NULL || strcmp(options->traceOutputPath, recorder->nativePcmPath) == 0 ||
        strcmp(options->traceOutputPath, recorder->nativeCyclesPath) == 0 ||
        strcmp(options->traceOutputPath, recorder->nativeManifestPath) == 0 ||
        strcmp(options->traceOutputPath, recorder->nativePcmTempPath) == 0 ||
        strcmp(options->traceOutputPath, recorder->nativeCyclesTempPath) == 0 ||
        strcmp(options->traceOutputPath, recorder->nativeManifestTempPath) == 0)
    {
        return false;
    }
    remove(options->traceOutputPath);
    remove(recorder->nativePcmPath);
    remove(recorder->nativeCyclesPath);
    remove(recorder->nativeManifestPath);
    remove(recorder->traceTempPath);
    remove(recorder->nativePcmTempPath);
    remove(recorder->nativeCyclesTempPath);
    remove(recorder->nativeManifestTempPath);
    recorder->traceOutput = fopen(recorder->traceTempPath, "wb");
    recorder->nativePcmOutput = fopen(recorder->nativePcmTempPath, "wb");
    recorder->nativeCyclesOutput = fopen(recorder->nativeCyclesTempPath, "wb");
    if (recorder->traceOutput == NULL || recorder->nativePcmOutput == NULL || recorder->nativeCyclesOutput == NULL ||
        fprintf(recorder->traceOutput, "PORYAAAA_AUDIO_TRACE 1\nCLOCK %u\n", GBA_CLOCK_HZ) < 0)
    {
        return false;
    }
    recorder->observationSink = (struct GBAAudioObservationSink){
        .context = recorder,
        .reset = reset_observations,
        .emit = record_observation,
    };
    return true;
}

/* Emits the exact native capture schema shared with poryaaaa_audio_trace. */
static bool write_native_manifest(const Recorder* recorder,
                                  const Options* options,
                                  const char romSha256[65],
                                  const char traceSha256[65])
{
    FILE* output = fopen(recorder->nativeManifestTempPath, "wb");
    if (output == NULL)
        return false;
    bool ok = fprintf(output,
                      "{\n"
                      "  \"format\": \"poryaaaa-native-capture\",\n"
                      "  \"version\": 1,\n"
                      "  \"source\": \"mgba-full\",\n"
                      "  \"clock_hz\": %u,\n"
                      "  \"channels\": 2,\n"
                      "  \"sample_format\": \"s16le\",\n"
                      "  \"cycle_format\": \"u64le\",\n"
                      "  \"frame_count\": %" PRIu64 ",\n"
                      "  \"first_cycle\": %" PRIu64 ",\n"
                      "  \"last_cycle\": %" PRIu64 ",\n"
                      "  \"solo_mask\": %" PRIu32 ",\n"
                      "  \"audio_channel_mask\": %" PRIu32 ",\n"
                      "  \"mgba_master_volume\": %u,\n"
                      "  \"bios_mode\": \"hle\",\n"
                      "  \"mgba_dirty\": %s,\n",
                      GBA_CLOCK_HZ,
                      recorder->nativeFramesWritten,
                      recorder->nativeFirstCycle,
                      recorder->nativeLastCycle,
                      options->enabledChannels,
                      options->enabledChannels,
                      MGBA_MASTER_VOLUME,
                      PORYAAAA_MGBA_SOURCE_DIRTY ? "true" : "false") > 0;
    ok = ok && fputs("  \"mgba_source_policy\": ", output) >= 0 &&
         write_json_string(output, PORYAAAA_MGBA_SOURCE_POLICY) && fputs(",\n", output) >= 0 &&
         fputs("  \"mgba_commit\": ", output) >= 0 && write_json_string(output, PORYAAAA_MGBA_BASE_REVISION) &&
         fputs(",\n", output) >= 0 && fputs("  \"mgba_base_revision\": ", output) >= 0 &&
         write_json_string(output, PORYAAAA_MGBA_BASE_REVISION) && fputs(",\n", output) >= 0 &&
         fputs("  \"mgba_observation_patch_sha256\": ", output) >= 0 &&
         write_json_string(output, PORYAAAA_MGBA_OBSERVATION_PATCH_SHA256) && fputs(",\n", output) >= 0 &&
         fputs("  \"compiler\": ", output) >= 0 && write_json_string(output, PORYAAAA_MGBA_RECORDER_COMPILER) &&
         fputs(",\n", output) >= 0 && fputs("  \"compiler_flags\": ", output) >= 0 &&
         write_json_string(output, PORYAAAA_MGBA_RECORDER_COMPILE_FLAGS) && fputs(",\n", output) >= 0 &&
         fputs("  \"rom_sha256\": ", output) >= 0 && write_json_string(output, romSha256) &&
         fputs(",\n", output) >= 0 && fputs("  \"trace_sha256\": ", output) >= 0 &&
         write_json_string(output, traceSha256) && fputs("\n}\n", output) >= 0;
    if (fflush(output) != 0)
        ok = false;
    if (fclose(output) != 0)
        ok = false;
    if (!ok)
        remove(recorder->nativeManifestTempPath);
    return ok;
}

/* Publishes complete native siblings only after the measurement and trace are complete. */
static bool finish_native_capture(Recorder* recorder, const Options* options)
{
    if (recorder->nativeWriteFailed || !recorder->nativeFinished || recorder->nativeFramesWritten == 0u ||
        recorder->nativePeak == 0 || recorder->traceOutput == NULL || recorder->nativePcmOutput == NULL ||
        recorder->nativeCyclesOutput == NULL)
    {
        return false;
    }
    bool traceClosed = fclose(recorder->traceOutput) == 0;
    recorder->traceOutput = NULL;
    bool pcmClosed = fclose(recorder->nativePcmOutput) == 0;
    recorder->nativePcmOutput = NULL;
    bool cyclesClosed = fclose(recorder->nativeCyclesOutput) == 0;
    recorder->nativeCyclesOutput = NULL;
    if (!traceClosed || !pcmClosed || !cyclesClosed)
        return false;

    char romSha256[65];
    char traceSha256[65];
    if (!sha256_file(options->romPath, romSha256) || !sha256_file(recorder->traceTempPath, traceSha256) ||
        !write_native_manifest(recorder, options, romSha256, traceSha256))
    {
        return false;
    }
    remove(options->traceOutputPath);
    remove(recorder->nativePcmPath);
    remove(recorder->nativeCyclesPath);
    remove(recorder->nativeManifestPath);
    if (rename(recorder->traceTempPath, options->traceOutputPath) != 0 ||
        rename(recorder->nativePcmTempPath, recorder->nativePcmPath) != 0 ||
        rename(recorder->nativeCyclesTempPath, recorder->nativeCyclesPath) != 0 ||
        rename(recorder->nativeManifestTempPath, recorder->nativeManifestPath) != 0)
    {
        remove(options->traceOutputPath);
        remove(recorder->nativePcmPath);
        remove(recorder->nativeCyclesPath);
        remove(recorder->nativeManifestPath);
        return false;
    }
    recorder->nativePublished = true;
    return true;
}

/* Releases partially written native artifacts after any unsuccessful recorder path. */
static void discard_native_capture(Recorder* recorder, const Options* options, bool successful)
{
    if (recorder->traceOutput != NULL)
        fclose(recorder->traceOutput);
    if (recorder->nativePcmOutput != NULL)
        fclose(recorder->nativePcmOutput);
    if (recorder->nativeCyclesOutput != NULL)
        fclose(recorder->nativeCyclesOutput);
    if (recorder->traceTempPath != NULL)
        remove(recorder->traceTempPath);
    if (recorder->nativePcmTempPath != NULL)
        remove(recorder->nativePcmTempPath);
    if (recorder->nativeCyclesTempPath != NULL)
        remove(recorder->nativeCyclesTempPath);
    if (recorder->nativeManifestTempPath != NULL)
        remove(recorder->nativeManifestTempPath);
    if (recorder->nativePublished && !successful)
    {
        remove(options->traceOutputPath);
        remove(recorder->nativePcmPath);
        remove(recorder->nativeCyclesPath);
        remove(recorder->nativeManifestPath);
    }
    free(recorder->observations);
    free(recorder->nativePcmPath);
    free(recorder->nativeCyclesPath);
    free(recorder->nativeManifestPath);
    free(recorder->traceTempPath);
    free(recorder->nativePcmTempPath);
    free(recorder->nativeCyclesTempPath);
    free(recorder->nativeManifestTempPath);
    recorder->traceOutput = NULL;
    recorder->nativePcmOutput = NULL;
    recorder->nativeCyclesOutput = NULL;
}

/* Runs one full frame then drains observations while no mGBA callback is active. */
static bool run_frame_and_drain(struct mCore* core, Recorder* recorder)
{
    core->runFrame(core);
    return !recorder->nativeWriteFailed && drain_observations(core, recorder);
}

/* Runs one core loop then drains observations while no mGBA callback is active. */
static bool run_loop_and_drain(struct mCore* core, Recorder* recorder)
{
    core->runLoop(core);
    return !recorder->nativeWriteFailed && drain_observations(core, recorder);
}

/* Copies an existing ROM ToneData entry so keysplits and sample pointers remain real. */
static void inject_voice(struct mCore* core, uint32_t sourceAddress, uint32_t destinationAddress)
{
    for (uint32_t i = 0u; i < TONE_DATA_SIZE; ++i)
        core->busWrite8(core, destinationAddress + i, (uint8_t)core->busRead8(core, sourceAddress + i));
}

/* Builds a one-track tied-note song in EWRAM for deterministic isolated playback. */
static void inject_song(struct mCore* core, const Options* options)
{
    uint32_t voiceAddress = options->fixtureAddress + FIXTURE_VOICE_OFFSET;
    uint32_t headerAddress = options->fixtureAddress + FIXTURE_HEADER_OFFSET;
    uint32_t trackAddress = options->fixtureAddress + FIXTURE_TRACK_OFFSET;
    uint32_t loopAddress = trackAddress + 13u;

    inject_voice(core, options->voiceAddress, voiceAddress);

    core->busWrite32(core, headerAddress, 1u); /* trackCount=1, blockCount=priority=reverb=0 */
    core->busWrite32(core, headerAddress + 4u, voiceAddress);
    core->busWrite32(core, headerAddress + 8u, trackAddress);

    const uint8_t commands[] = {
        0xBCu,
        0u, /* KEYSH 0 */
        0xBBu,
        60u, /* TEMPO 120 BPM */
        0xBDu,
        0u, /* VOICE 0 */
        0xBEu,
        options->volume, /* VOL */
        0xBFu,
        options->pan, /* PAN */
        0xCFu,
        options->note,
        options->velocity, /* TIE */
        0xB0u,             /* W96 */
        0xB2u,             /* GOTO loop */
    };
    for (size_t i = 0u; i < sizeof(commands); ++i)
        core->busWrite8(core, trackAddress + (uint32_t)i, commands[i]);
    uint32_t gotoAddress = trackAddress + (uint32_t)sizeof(commands);
    for (uint32_t i = 0u; i < sizeof(loopAddress); ++i)
        core->busWrite8(core, gotoAddress + i, (uint8_t)(loopAddress >> (i * 8u)));

    uint32_t runnerAddress = options->fixtureAddress + FIXTURE_RUNNER_OFFSET;
    core->busWrite16(core, runnerAddress, 0xE7FEu); /* b runner; VBlank IRQ preempts it */
}

/* Starts either the injected voice fixture or a real ROM song. */
static bool start_fixture(struct mCore* core, const Options* options)
{
    int32_t runnerAddress = (int32_t)(options->fixtureAddress + FIXTURE_RUNNER_OFFSET + 1u);
    if (options->songStart != 0u)
    {
        int32_t songId = (int32_t)options->songId;
        int32_t startAddress = (int32_t)(options->songStart & ~1u);
        return core->writeRegister(core, "r0", songId) && core->writeRegister(core, "lr", runnerAddress) &&
               core->writeRegister(core, "pc", startAddress);
    }

    int32_t mplayInfo = (int32_t)options->mplayInfo;
    int32_t headerAddress = (int32_t)(options->fixtureAddress + FIXTURE_HEADER_OFFSET);
    int32_t startAddress = (int32_t)(options->mplayStart & ~1u);
    return core->writeRegister(core, "r0", mplayInfo) && core->writeRegister(core, "r1", headerAddress) &&
           core->writeRegister(core, "lr", runnerAddress) && core->writeRegister(core, "pc", startAddress);
}

static bool wait_for_runner(struct mCore* core, Recorder* recorder, const Options* options);

/* Stops boot-time players so a hardware FIFO capture contains only the injected voice. */
static bool stop_existing_audio(struct mCore* core, Recorder* recorder, const Options* options)
{
    if (options->mplayAllStop == 0u)
        return true;

    int32_t runnerAddress = (int32_t)(options->fixtureAddress + FIXTURE_RUNNER_OFFSET + 1u);
    int32_t stopAddress = (int32_t)(options->mplayAllStop & ~1u);
    if (!core->writeRegister(core, "lr", runnerAddress) || !core->writeRegister(core, "pc", stopAddress) ||
        !wait_for_runner(core, recorder, options))
    {
        return false;
    }

    for (uint32_t channel = 0u; channel < 12u; ++channel)
        core->busWrite8(core, options->soundInfo + 80u + channel * 64u, 0u);
    core->busWrite8(core, options->soundInfo + 5u, 0u);
    for (uint32_t sample = 0u; sample < 3168u; ++sample)
        core->busWrite8(core, options->soundInfo + 848u + sample, 0u);
    uint16_t soundControl = core->busRead16(core, GBA_REG_SOUNDCNT_H);
    core->busWrite16(core, GBA_REG_SOUNDCNT_H, soundControl | GBA_FIFO_RESET_BITS);
    core->busWrite16(core, GBA_REG_SOUNDCNT_H, soundControl);
    return true;
}

/* Boots until MP2K and the ROM's real VBlank interrupt path are both live. */
static bool wait_for_mp2k(struct mCore* core, Recorder* recorder, const Options* options)
{
    uint64_t maxFrames = (uint64_t)(options->bootTimeoutSeconds * GBA_FRAMES_PER_SECOND + 0.5);
    for (uint64_t frame = 0u; frame < maxFrames; ++frame)
    {
        if (!run_frame_and_drain(core, recorder))
            return false;
        if (core->busRead32(core, options->mplayInfo + 52u) == MP2K_MAGIC &&
            (core->busRead16(core, GBA_REG_DISPSTAT) & 0x8u) != 0u &&
            (core->busRead16(core, GBA_REG_IE) & 0x1u) != 0u && (core->busRead16(core, GBA_REG_IME) & 0x1u) != 0u)
        {
            return true;
        }
    }
    return false;
}

/* Steps past runFrame's video boundary so SoundMain is not abandoned while locked. */
static bool wait_for_sound_main_idle(struct mCore* core, Recorder* recorder, const Options* options)
{
    for (uint32_t iteration = 0u; iteration < 1000u; ++iteration)
    {
        int32_t cpsr = 0;
        int32_t programCounter = 0;
        if (!core->readRegister(core, "cpsr", &cpsr) || !core->readRegister(core, "pc", &programCounter))
            return false;

        if (core->busRead32(core, options->soundInfo) == MP2K_MAGIC && (cpsr & 0x3F) == 0x3F &&
            (uint32_t)programCounter >= 0x02000000u)
        {
            return true;
        }
        if (!run_loop_and_drain(core, recorder))
            return false;
    }
    return false;
}

/* Steps an injected ROM call until it returns to the EWRAM runner. */
static bool wait_for_runner(struct mCore* core, Recorder* recorder, const Options* options)
{
    for (uint32_t iteration = 0u; iteration < 1000u; ++iteration)
    {
        int32_t programCounter = 0;
        if (!core->readRegister(core, "pc", &programCounter))
            return false;
        if ((uint32_t)programCounter >= options->fixtureAddress + FIXTURE_RUNNER_OFFSET &&
            (uint32_t)programCounter < options->fixtureAddress + FIXTURE_RUNNER_OFFSET + 4u &&
            core->busRead32(core, options->soundInfo) == MP2K_MAGIC)
        {
            return true;
        }
        if (!run_loop_and_drain(core, recorder))
            return false;
    }
    int32_t cpsr = 0;
    int32_t programCounter = 0;
    core->readRegister(core, "cpsr", &cpsr);
    core->readRegister(core, "pc", &programCounter);
    fprintf(stderr,
            "Emulated ROM call timed out (pc=0x%08" PRIX32 ", cpsr=0x%08" PRIX32 ", mplay=0x%08" PRIX32
            ", sound=0x%08" PRIX32 ")\n",
            (uint32_t)programCounter,
            (uint32_t)cpsr,
            core->busRead32(core, options->mplayInfo + 52u),
            core->busRead32(core, options->soundInfo));
    return false;
}

/* Opens the legacy frontend WAV at the rate exposed by pinned mGBA. */
static bool begin_frontend_capture(Recorder* recorder, const Options* options)
{
    if (recorder->sampleRate == 0u)
        return false;

    recorder->targetFrames = (uint64_t)(options->durationSeconds * recorder->sampleRate + 0.5);
    if (recorder->targetFrames == 0u || recorder->targetFrames > (UINT32_MAX - 36u) / 4u)
        return false;

    recorder->output = fopen(options->outputPath, "wb+");
    if (recorder->output == NULL || !write_wav_header(recorder->output, recorder->sampleRate, 0u))
        return false;

    recorder->capturing = true;
    return true;
}

/* Finalizes the legacy frontend WAV only after every requested frame arrived. */
static bool finish_frontend_capture(Recorder* recorder)
{
    recorder->capturing = false;
    if (recorder->writeFailed || recorder->framesWritten != recorder->targetFrames)
    {
        return false;
    }

    return write_wav_header(recorder->output, recorder->sampleRate, recorder->framesWritten) &&
           fflush(recorder->output) == 0;
}

/* Captures requested frontend/native stages from one full-ROM execution. */
static bool capture_reference(struct mCore* core, Recorder* recorder, const Options* options)
{
    bool frontendRequested = (options->captureStage & CAPTURE_STAGE_FRONTEND) != 0u;
    bool nativeRequested = (options->captureStage & CAPTURE_STAGE_NATIVE) != 0u;
    if ((frontendRequested && !begin_frontend_capture(recorder, options)) ||
        (nativeRequested && !begin_native_capture(core, recorder, options)))
    {
        return false;
    }

    bool frontendFinished = !frontendRequested;
    recorder->nativeFinished = !nativeRequested;
    while (!frontendFinished || !recorder->nativeFinished)
    {
        if (!run_frame_and_drain(core, recorder) || !wait_for_sound_main_idle(core, recorder, options))
            return false;
        if (!frontendFinished && recorder->framesWritten == recorder->targetFrames)
        {
            if (!finish_frontend_capture(recorder))
                return false;
            frontendFinished = true;
        }
    }
    return !nativeRequested || finish_native_capture(recorder, options);
}

/* Arms during the ROM's silent preroll and proves that its configured song advanced. */
static bool capture_boot_song(struct mCore* core, Recorder* recorder, const Options* options)
{
    if (!run_frame_and_drain(core, recorder))
        return false;

    bool frontendRequested = (options->captureStage & CAPTURE_STAGE_FRONTEND) != 0u;
    bool nativeRequested = (options->captureStage & CAPTURE_STAGE_NATIVE) != 0u;
    if ((frontendRequested && !begin_frontend_capture(recorder, options)) ||
        (nativeRequested && !begin_native_capture(core, recorder, options)))
    {
        return false;
    }

    bool sawSoundDriver = false;
    bool sawExpectedSong = false;
    bool trackAdvanced = false;
    bool maxChansValidated = !options->hasRequiredMaxChans;
    bool frontendFinished = !frontendRequested;
    recorder->nativeFinished = !nativeRequested;
    uint32_t initialTrackAddress = core->busRead32(core, options->songAddress + 8u);
    while (!frontendFinished || !recorder->nativeFinished)
    {
        if (!run_frame_and_drain(core, recorder))
            return false;
        if (core->busRead32(core, options->soundInfo) == MP2K_MAGIC)
        {
            sawSoundDriver = true;
            if (!maxChansValidated)
            {
                uint8_t observedMaxChans = (uint8_t)core->busRead8(core, options->soundInfo + 6u);
                if (observedMaxChans != options->requiredMaxChans)
                {
                    fprintf(stderr,
                            "MP2K maxChans mismatch: expected %u, observed %u\n",
                            options->requiredMaxChans,
                            observedMaxChans);
                    return false;
                }
                fprintf(stderr, "MP2K maxChans validated: %u\n", observedMaxChans);
                maxChansValidated = true;
            }
        }
        if (core->busRead32(core, options->mplayInfo) == options->songAddress)
        {
            sawExpectedSong = true;
            uint32_t tracksAddress = core->busRead32(core, options->mplayInfo + 44u);
            if (tracksAddress != 0u && core->busRead32(core, tracksAddress + 64u) != initialTrackAddress)
                trackAdvanced = true;
        }
        if (!frontendFinished && recorder->framesWritten == recorder->targetFrames)
        {
            if (!finish_frontend_capture(recorder))
                return false;
            frontendFinished = true;
        }
    }

    if (nativeRequested && !finish_native_capture(recorder, options))
        return false;
    if (!sawSoundDriver || !sawExpectedSong || !trackAdvanced || !maxChansValidated)
    {
        fprintf(stderr,
                "Audio-only ROM validation failed (sound=%d, song=%d, advanced=%d)\n",
                sawSoundDriver,
                sawExpectedSong,
                trackAdvanced);
        return false;
    }
    return true;
}

/* Owns the full mGBA lifecycle so no emulator state leaks into production code. */
int main(int argc, char** argv)
{
    Options options;
    if (!parse_options(argc, argv, &options))
    {
        print_usage(argv[0]);
        return 2;
    }
    const bool nativeRequested = (options.captureStage & CAPTURE_STAGE_NATIVE) != 0u;
    const bool frontendRequested = (options.captureStage & CAPTURE_STAGE_FRONTEND) != 0u;

#if !PORYAAAA_MGBA_NATIVE_CAPTURE_AVAILABLE
    if (nativeRequested)
    {
        fprintf(stderr,
                "Native capture requires the authoritative pinned PORYAAAA_MGBA_SOURCE build; "
                "installed-library mode is frontend-only.\n");
        return 2;
    }
#endif

    Recorder recorder = {
        .stream =
            {
                .postAudioBuffer = post_audio_buffer,
            },
    };
    struct mCore* core = GBACoreCreate();
    if (core == NULL || !core->init(core))
    {
        fprintf(stderr, "Could not initialize mGBA for %s\n", options.romPath);
        return 1;
    }

    int result = 1;
    struct mStandardLogger logger = {0};
    mCoreInitConfig(core, "poryaaaa-mgba-reference");
    mCoreConfigSetDefaultIntValue(&core->config, "logLevel", 0);
    if (nativeRequested)
    {
        mCoreConfigSetOverrideIntValue(&core->config, "useBios", 0);
        mCoreConfigSetOverrideIntValue(&core->config, "volume", MGBA_MASTER_VOLUME);
    }
    mStandardLoggerInit(&logger);
    mStandardLoggerConfig(&logger, &core->config);
    mLogSetDefaultLogger(&logger.d);
    if (frontendRequested)
        core->setAVStream(core, &recorder.stream);
    if (!mCoreLoadFile(core, options.romPath))
    {
        fprintf(stderr, "Could not load ROM: %s\n", options.romPath);
        goto cleanup;
    }

    if (nativeRequested)
    {
        if (!prepare_native_capture(&recorder, &options))
        {
            fprintf(stderr, "Could not open native trace artifacts: %s\n", strerror(errno));
            goto cleanup;
        }
#if PORYAAAA_MGBA_NATIVE_CAPTURE_AVAILABLE
        GBAAudioSetObservationSink(&((struct GBA*)core->board)->audio, &recorder.observationSink);
#endif
    }

    core->reset(core);
    if (nativeRequested)
        reset_observations(&recorder);
    if (frontendRequested)
    {
        recorder.sampleRate = core->audioSampleRate(core);
        if (recorder.sampleRate == 0u)
        {
            fprintf(stderr, "mGBA did not expose a frontend audio sample rate\n");
            goto unload;
        }
    }
    if (!apply_audio_channel_mask(core, options.enabledChannels))
    {
        fprintf(stderr, "mGBA core does not expose GBA audio channel controls\n");
        goto unload;
    }
    uint32_t expectedHeader =
        options.songAddress != 0u ? options.songAddress : options.fixtureAddress + FIXTURE_HEADER_OFFSET;
    if (options.bootSong)
    {
        if (!capture_boot_song(core, &recorder, &options))
        {
            fprintf(stderr, "Audio-only ROM reference capture failed\n");
            goto unload;
        }
        goto validate_capture;
    }

    if (!wait_for_mp2k(core, &recorder, &options))
    {
        fprintf(stderr,
                "MP2K did not initialize at 0x%08" PRIX32 " within %.2f seconds\n",
                options.mplayInfo,
                options.bootTimeoutSeconds);
        goto unload;
    }
    if (!wait_for_sound_main_idle(core, &recorder, &options))
    {
        fprintf(stderr, "The ROM's SoundMain did not return to its idle state\n");
        goto unload;
    }

    if (options.voiceAddress != 0u)
        inject_song(core, &options);
    else
        core->busWrite16(core, options.fixtureAddress + FIXTURE_RUNNER_OFFSET, 0xE7FEu);
    if (!stop_existing_audio(core, &recorder, &options))
    {
        fprintf(stderr, "Could not stop boot-time audio before the reference fixture\n");
        goto unload;
    }
    if (!start_fixture(core, &options))
    {
        fprintf(stderr, "Could not transfer emulated CPU control to the ROM sound driver\n");
        goto unload;
    }
    if (!wait_for_runner(core, &recorder, &options))
    {
        fprintf(stderr, "The ROM sound driver did not return to the reference runner\n");
        goto unload;
    }
    if (core->busRead32(core, options.mplayInfo) != expectedHeader)
    {
        fprintf(stderr, "The ROM sound driver did not accept song header 0x%08" PRIX32 "\n", expectedHeader);
        goto unload;
    }
    uint32_t tracksAddress = core->busRead32(core, options.mplayInfo + 44u);
    uint32_t initialTrackAddress = core->busRead32(core, expectedHeader + 8u);

    if (!capture_reference(core, &recorder, &options))
    {
        fprintf(stderr, "mGBA reference capture failed\n");
        goto unload;
    }
    if (core->busRead32(core, tracksAddress + 64u) == initialTrackAddress)
    {
        int32_t cpsr = 0;
        int32_t programCounter = 0;
        core->readRegister(core, "cpsr", &cpsr);
        core->readRegister(core, "pc", &programCounter);
        fprintf(stderr,
                "The ROM's MPlayMain did not advance the injected track "
                "(sound=0x%08" PRIX32 ", flags=0x%02" PRIX32 ", cpsr=0x%08" PRIX32 ", pc=0x%08" PRIX32 ")\n",
                core->busRead32(core, options.soundInfo),
                core->busRead8(core, tracksAddress),
                (uint32_t)cpsr,
                (uint32_t)programCounter);
        goto unload;
    }
validate_capture:
    if ((frontendRequested && recorder.peak == 0) || (nativeRequested && recorder.nativePeak == 0))
    {
        fprintf(stderr, "mGBA produced a silent capture for song header 0x%08" PRIX32 "\n", expectedHeader);
        goto unload;
    }

    if (frontendRequested)
    {
        double rms = sqrt(recorder.sumSquares / (double)(recorder.framesWritten * 2u)) / 32768.0;
        printf("mGBA %s: wrote %" PRIu64 " frontend stereo frames at %u Hz to %s "
               "(channels 0x%02" PRIX32 ", peak %.6f, RMS %.6f)\n",
               projectVersion,
               recorder.framesWritten,
               recorder.sampleRate,
               options.outputPath,
               options.enabledChannels,
               (double)recorder.peak / 32768.0,
               rms);
    }
    if (nativeRequested)
    {
        printf("mGBA %s: wrote %" PRIu64 " native stereo frames to %s.[pcm|cycles|json] "
               "(channels 0x%02" PRIX32 ", peak %.6f)\n",
               projectVersion,
               recorder.nativeFramesWritten,
               options.nativeOutputPrefix,
               options.enabledChannels,
               (double)recorder.nativePeak / 32768.0);
    }
    result = 0;

unload:
    core->unloadROM(core);
cleanup:
    if (recorder.output != NULL && fclose(recorder.output) != 0)
        result = 1;
    discard_native_capture(&recorder, &options, result == 0);
    mCoreConfigDeinit(&core->config);
    core->deinit(core);
    mLogSetDefaultLogger(NULL);
    mStandardLoggerDeinit(&logger);
    return result;
}
