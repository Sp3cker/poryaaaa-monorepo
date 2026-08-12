#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#    define _POSIX_C_SOURCE 200809L
#endif

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "audio_trace_format.h"
#if defined(_WIN32)
#    include <io.h>
#    include <process.h>
#    include <sys/stat.h>
#    include <windows.h>
#else
#    include <sys/stat.h>
#    include <unistd.h>
#endif

#if defined(_WIN32) && !defined(ESTALE)
#    define ESTALE EIO
#endif
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
#define GBA_CYCLES_PER_FRAME 280896u
#define OBSERVATION_BUFFER_CAPACITY 65536u
#define MGBA_MASTER_VOLUME 0x100u
#define AUDIO_BUFFER_FRAMES 2048u
#define TONE_DATA_SIZE 12u
#define PSW_WAVEFORM_SIZE 16u
#define DIRECTSOUND_WAVE_HEADER_SIZE 16u
#define GBA_ROM_START 0x08000000u
#define GBA_ROM_END 0x0A000000u
#define DRIVER_SCENARIO_CAPTURE_TAIL_CYCLES 8192u
#define DRIVER_SCENARIO_START_FRAMES 9u
#define DRIVER_SCENARIO_ENVELOPE_FRAMES 15u
#define DRIVER_SCENARIO_PITCH_FRAMES 12u
#define DRIVER_SCENARIO_VOLUME_PAN_FRAMES 12u
#define DRIVER_SCENARIO_RETRIGGER_FRAMES 15u
#define DRIVER_SCENARIO_RELEASE_FRAMES 14u
#define DRIVER_TRACK_CAPACITY 32u
#define GBA_REG_DISPSTAT 0x04000004u
#define GBA_REG_VCOUNT 0x04000006u
#define GBA_REG_SOUNDCNT_H 0x04000082u
#define GBA_REG_TM0CNT_H 0x04000102u
#define GBA_REG_IE 0x04000200u
#define GBA_REG_IME 0x04000208u
#define GBA_DISPSTAT_VBLANK_IRQ 0x0008u
#define GBA_DISPSTAT_VCOUNT_IRQ 0x0020u
#define GBA_DISPSTAT_VCOUNT_LINE_150 0x9600u
#define GBA_IE_VBLANK 0x0001u
#define GBA_IE_VCOUNT 0x0004u
#define GBA_FIFO_RESET_BITS 0x8800u
#define GBA_REG_FIFO_A 0x040000A0u
#define GBA_REG_FIFO_B 0x040000A4u

#define AUDIO_CHANNEL_SQ1 (1u << 0u)
#define AUDIO_CHANNEL_SQ2 (1u << 1u)
#define AUDIO_CHANNEL_WAVE (1u << 2u)
#define AUDIO_CHANNEL_NOISE (1u << 3u)
#define AUDIO_CHANNEL_FIFO_A (1u << 4u)
#define AUDIO_CHANNEL_FIFO_B (1u << 5u)
#define AUDIO_CHANNEL_PSG (AUDIO_CHANNEL_SQ1 | AUDIO_CHANNEL_SQ2 | AUDIO_CHANNEL_WAVE | AUDIO_CHANNEL_NOISE)
#define AUDIO_CHANNEL_DIRECTSOUND (AUDIO_CHANNEL_FIFO_A | AUDIO_CHANNEL_FIFO_B)
#define AUDIO_CHANNEL_ALL (AUDIO_CHANNEL_PSG | AUDIO_CHANNEL_DIRECTSOUND)

#define FIXTURE_VOICE_OFFSET 0x00u
#define FIXTURE_HEADER_OFFSET 0x20u
#define FIXTURE_TRACK_OFFSET 0x40u
#define FIXTURE_RUNNER_OFFSET 0x80u

typedef enum CaptureStage
{
    CAPTURE_STAGE_FRONTEND = 1u << 0u,
    CAPTURE_STAGE_NATIVE = 1u << 1u,
} CaptureStage;

typedef enum DriverScenario
{
    DRIVER_SCENARIO_NONE,
    DRIVER_SCENARIO_START,
    DRIVER_SCENARIO_ENVELOPE,
    DRIVER_SCENARIO_PITCH,
    DRIVER_SCENARIO_VOLUME_PAN,
    DRIVER_SCENARIO_RETRIGGER,
    DRIVER_SCENARIO_RELEASE,
} DriverScenario;

typedef enum DriverFamily
{
    DRIVER_FAMILY_NONE,
    DRIVER_FAMILY_DIRECTSOUND,
    DRIVER_FAMILY_SQ1,
    DRIVER_FAMILY_SQ2,
    DRIVER_FAMILY_PSW,
} DriverFamily;

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

typedef struct DriverFixtureIdentity
{
    bool captured;
    uint8_t toneData[TONE_DATA_SIZE];
    uint8_t normalizedToneData[TONE_DATA_SIZE];
    uint8_t waveform[PSW_WAVEFORM_SIZE];
    char toneDataSha256[65];
    char familyPayloadSha256[65];
    char waveformSha256[65];
    uint32_t payloadAddress;
    uint32_t payloadSize;
    uint8_t resolvedType;
    DriverFamily family;
    uint32_t soloMask;
} DriverFixtureIdentity;

typedef struct DriverTrack
{
    uint8_t bytes[DRIVER_TRACK_CAPACITY];
    size_t length;
    size_t loopOffset;
} DriverTrack;

typedef struct Options
{
    const char* romPath;
    const char* outputPath;
    const char* traceOutputPath;
    const char* nativeOutputPrefix;
    uint32_t mplayStart;
    uint32_t mplayAllStop;
    uint32_t m4aVSyncOff;
    uint32_t m4aVSyncOn;
    uint32_t m4aVSync;
    uint32_t songStart;
    uint32_t songAddress;
    uint32_t songId;
    uint32_t mplayInfo;
    uint32_t soundInfo;
    uint32_t voiceAddress;
    uint32_t fixtureAddress;
    DriverScenario driverScenario;
    const char* elfPath;
    const char* voicegroupSymbol;
    uint32_t voiceIndex;
    bool hasVoiceIndex;
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
    FILE* nativeManifestOutput;
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
    uint64_t nativeBeginCycle;
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
    bool nativeWriteFailed;
    bool tracePublished;
    bool nativePcmPublished;
    bool nativeCyclesPublished;
    bool nativeManifestPublished;
    bool nativePublicationFailed;
    DriverFixtureIdentity driverIdentity;
} Recorder;

typedef struct NativeFileIdentity
{
#if defined(_WIN32)
    DWORD volumeSerialNumber;
    DWORD fileIndexHigh;
    DWORD fileIndexLow;
#else
    dev_t device;
    ino_t inode;
#endif
} NativeFileIdentity;

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
            "  --m4a-vsync-off ADDRESS   ROM m4aSoundVSyncOff for a clean driver DMA epoch\n"
            "  --m4a-vsync-on ADDRESS    ROM m4aSoundVSyncOn paired with --m4a-vsync-off\n"
            "  --m4a-vsync ADDRESS       ROM m4aSoundVSync callback used to align DMA ring 0\n"
            "  --require-max-chans N     Fail unless MP2K configures this PCM channel count\n"
            "  --fixture-address ADDRESS EWRAM scratch base (default: 0x0203F000)\n"
            "  --scenario NAME           Fixed driver scenario: start, envelope, pitch, volume-pan,\n"
            "                            retrigger, or release; requires voice mode and native/both capture\n"
            "  --elf FILE                ELF matching the ROM (with --scenario)\n"
            "  --voicegroup-symbol NAME  Voicegroup symbol (with --scenario)\n"
            "  --voice-index N           Zero-based voice index 0-127 (with --scenario)\n"
            "  --dump-driver-track NAME  Print default fixed track bytes for a focused behavior test\n"
            "  --dump-driver-span NAME   Print a fixed driver measurement span\n"
            "  --dump-driver-family TYPE Print the family and solo mask for one ToneData type\n",
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

/* Parses the fixed lifecycle enum; arbitrary event scripts are intentionally unsupported. */
static bool parse_driver_scenario(const char* text, DriverScenario* result)
{
    if (strcmp(text, "start") == 0)
        *result = DRIVER_SCENARIO_START;
    else if (strcmp(text, "envelope") == 0)
        *result = DRIVER_SCENARIO_ENVELOPE;
    else if (strcmp(text, "pitch") == 0)
        *result = DRIVER_SCENARIO_PITCH;
    else if (strcmp(text, "volume-pan") == 0)
        *result = DRIVER_SCENARIO_VOLUME_PAN;
    else if (strcmp(text, "retrigger") == 0)
        *result = DRIVER_SCENARIO_RETRIGGER;
    else if (strcmp(text, "release") == 0)
        *result = DRIVER_SCENARIO_RELEASE;
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
        else if (strcmp(name, "--m4a-vsync-off") == 0)
        {
            if (!parse_u32(value, &options->m4aVSyncOff))
                return false;
        }
        else if (strcmp(name, "--m4a-vsync-on") == 0)
        {
            if (!parse_u32(value, &options->m4aVSyncOn))
                return false;
        }
        else if (strcmp(name, "--m4a-vsync") == 0)
        {
            if (!parse_u32(value, &options->m4aVSync))
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
        else if (strcmp(name, "--scenario") == 0)
        {
            if (!parse_driver_scenario(value, &options->driverScenario))
                return false;
        }
        else if (strcmp(name, "--elf") == 0)
            options->elfPath = value;
        else if (strcmp(name, "--voicegroup-symbol") == 0)
            options->voicegroupSymbol = value;
        else if (strcmp(name, "--voice-index") == 0)
        {
            if (!parse_u32(value, &options->voiceIndex) || options->voiceIndex > 127u)
                return false;
            options->hasVoiceIndex = true;
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
    bool driverScenarioRequested = options->driverScenario != DRIVER_SCENARIO_NONE;
    if (driverScenarioRequested &&
        (!voiceMode || !nativeCapture || options->elfPath == NULL || options->voicegroupSymbol == NULL ||
         !options->hasVoiceIndex || options->voiceIndex > 127u || options->m4aVSyncOff == 0u ||
         options->m4aVSyncOn == 0u || options->m4aVSync == 0u))
    {
        return false;
    }
    if (!driverScenarioRequested &&
        (options->elfPath != NULL || options->voicegroupSymbol != NULL || options->hasVoiceIndex))
    {
        return false;
    }
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

/* Hashes one in-memory byte array with the recorder's portable SHA-256. */
static void sha256_bytes(const uint8_t* data, size_t count, char output[65])
{
    Sha256 sha;
    sha256_init(&sha);
    sha256_update(&sha, data, count);
    sha256_finish(&sha, output);
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

/* Closes an exclusive staging descriptor on the active host platform. */
static int native_close_descriptor(int descriptor)
{
#if defined(_WIN32)
    return _close(descriptor);
#else
    return close(descriptor);
#endif
}

/* Converts an exclusive staging descriptor into a binary read/write stream. */
static FILE* native_fdopen_binary_update(int descriptor)
{
#if defined(_WIN32)
    return _fdopen(descriptor, "w+b");
#else
    return fdopen(descriptor, "w+b");
#endif
}

#if defined(_WIN32)
/* Maps Windows publication errors to the recorder's errno diagnostics. */
static void native_set_windows_errno(DWORD error)
{
    if (error == ERROR_FILE_EXISTS || error == ERROR_ALREADY_EXISTS)
        errno = EEXIST;
    else if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND)
        errno = ENOENT;
    else if (error == ERROR_ACCESS_DENIED || error == ERROR_SHARING_VIOLATION)
        errno = EACCES;
    else
        errno = EIO;
}

/* Recovers the Windows handle for one still-open recorder staging stream. */
static HANDLE native_stream_handle(FILE* input)
{
    if (input == NULL)
    {
        errno = EINVAL;
        return INVALID_HANDLE_VALUE;
    }
    int descriptor = _fileno(input);
    intptr_t rawHandle = descriptor < 0 ? -1 : _get_osfhandle(descriptor);
    if (rawHandle == -1)
    {
        errno = EBADF;
        return INVALID_HANDLE_VALUE;
    }
    return (HANDLE)rawHandle;
}

/* Renames an open staging stream while refusing to replace an existing final. */
static bool native_rename_stream_noreplace(FILE* input, const char* finalPath)
{
    HANDLE handle = native_stream_handle(input);
    if (handle == INVALID_HANDLE_VALUE)
        return false;
    int wideCount = MultiByteToWideChar(CP_ACP, 0, finalPath, -1, NULL, 0);
    if (wideCount <= 0)
    {
        native_set_windows_errno(GetLastError());
        return false;
    }
    size_t informationSize = sizeof(FILE_RENAME_INFO) + ((size_t)wideCount - 1u) * sizeof(WCHAR);
    if (informationSize > UINT32_MAX)
    {
        errno = ENOMEM;
        return false;
    }
    FILE_RENAME_INFO* information = calloc(1u, informationSize);
    if (information == NULL)
        return false;
    information->ReplaceIfExists = FALSE;
    information->FileNameLength = (DWORD)((size_t)(wideCount - 1) * sizeof(WCHAR));
    bool converted = MultiByteToWideChar(CP_ACP, 0, finalPath, -1, information->FileName, wideCount) != 0;
    bool renamed =
        converted && SetFileInformationByHandle(handle, FileRenameInfo, information, (DWORD)informationSize) != 0;
    DWORD error = renamed ? ERROR_SUCCESS : GetLastError();
    free(information);
    if (!renamed)
    {
        native_set_windows_errno(error);
        return false;
    }
    return true;
}

/* Marks this exact stream for deletion without resolving a competing pathname. */
static bool native_delete_stream(FILE* input)
{
    HANDLE handle = native_stream_handle(input);
    if (handle == INVALID_HANDLE_VALUE)
        return false;
    FILE_DISPOSITION_INFO information = {.DeleteFile = TRUE};
    if (!SetFileInformationByHandle(handle, FileDispositionInfo, &information, sizeof(information)))
    {
        native_set_windows_errno(GetLastError());
        return false;
    }
    return true;
}
#endif

/* Captures the identity of an open stream before it is published or cleaned up. */
static bool native_stream_identity(FILE* input, NativeFileIdentity* identity)
{
    if (input == NULL || identity == NULL)
    {
        errno = EINVAL;
        return false;
    }
#if defined(_WIN32)
    HANDLE handle = native_stream_handle(input);
    if (handle == INVALID_HANDLE_VALUE)
        return false;
    BY_HANDLE_FILE_INFORMATION information;
    if (!GetFileInformationByHandle(handle, &information))
    {
        native_set_windows_errno(GetLastError());
        return false;
    }
    *identity = (NativeFileIdentity){
        .volumeSerialNumber = information.dwVolumeSerialNumber,
        .fileIndexHigh = information.nFileIndexHigh,
        .fileIndexLow = information.nFileIndexLow,
    };
#else
    struct stat information;
    if (fstat(fileno(input), &information) != 0)
        return false;
    *identity = (NativeFileIdentity){
        .device = information.st_dev,
        .inode = information.st_ino,
    };
#endif
    return true;
}

/* Checks that a pathname still names the exact file object owned by this stream. */
static bool native_path_matches_identity(const char* path, const NativeFileIdentity* expected)
{
    if (path == NULL || expected == NULL)
    {
        errno = EINVAL;
        return false;
    }
#if defined(_WIN32)
    HANDLE handle = CreateFileA(path,
                                FILE_READ_ATTRIBUTES,
                                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                NULL,
                                OPEN_EXISTING,
                                FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT,
                                NULL);
    if (handle == INVALID_HANDLE_VALUE)
    {
        native_set_windows_errno(GetLastError());
        return false;
    }
    BY_HANDLE_FILE_INFORMATION information;
    bool queried = GetFileInformationByHandle(handle, &information) != 0;
    DWORD error = queried ? ERROR_SUCCESS : GetLastError();
    CloseHandle(handle);
    if (!queried)
    {
        native_set_windows_errno(error);
        return false;
    }
    if (information.dwVolumeSerialNumber != expected->volumeSerialNumber ||
        information.nFileIndexHigh != expected->fileIndexHigh || information.nFileIndexLow != expected->fileIndexLow)
    {
        errno = ESTALE;
        return false;
    }
#else
    struct stat information;
    if (lstat(path, &information) != 0)
        return false;
    if (information.st_dev != expected->device || information.st_ino != expected->inode)
    {
        errno = ESTALE;
        return false;
    }
#endif
    return true;
}

/* Binds a staging or final pathname to its recorder-owned open stream. */
static bool native_stream_path_matches(FILE* input, const char* path)
{
    NativeFileIdentity identity;
    return native_stream_identity(input, &identity) && native_path_matches_identity(path, &identity);
}

/* Removes only an entry proven to name this recorder's open stream. */
static bool native_remove_matching_stream_path(FILE* input, const char* path)
{
    if (!native_stream_path_matches(input, path))
        return false;
#if defined(_WIN32)
    return native_delete_stream(input);
#else
    return unlink(path) == 0;
#endif
}

/* Opens a collision-free sibling staging stream without following a pre-existing path. */
static FILE* native_open_unique_temp(const char* finalPath, char** tempPath)
{
    *tempPath = NULL;
#if defined(_WIN32)
    size_t finalLength = strlen(finalPath);
    if (finalLength > SIZE_MAX - 64u)
    {
        errno = ENOMEM;
        return NULL;
    }
    char* path = malloc(finalLength + 64u);
    if (path == NULL)
        return NULL;
    for (unsigned attempt = 0u; attempt < 1024u; ++attempt)
    {
        unsigned high = 0u;
        unsigned low = 0u;
        if (rand_s(&high) != 0 || rand_s(&low) != 0)
        {
            free(path);
            errno = EIO;
            return NULL;
        }
        int count = snprintf(path, finalLength + 64u, "%s.tmp.%08X%08X", finalPath, high, low);
        if (count < 0 || (size_t)count >= finalLength + 64u)
        {
            free(path);
            errno = ENOMEM;
            return NULL;
        }
        HANDLE handle = CreateFileA(path,
                                    GENERIC_READ | GENERIC_WRITE | DELETE,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE,
                                    NULL,
                                    CREATE_NEW,
                                    FILE_ATTRIBUTE_NORMAL,
                                    NULL);
        if (handle != INVALID_HANDLE_VALUE)
        {
            int descriptor = _open_osfhandle((intptr_t)handle, _O_RDWR | _O_BINARY);
            if (descriptor >= 0)
            {
                FILE* output = native_fdopen_binary_update(descriptor);
                if (output != NULL)
                {
                    *tempPath = path;
                    return output;
                }
                int savedErrno = errno;
                native_close_descriptor(descriptor);
                DeleteFileA(path);
                free(path);
                errno = savedErrno;
                return NULL;
            }
            int savedErrno = errno;
            CloseHandle(handle);
            DeleteFileA(path);
            free(path);
            errno = savedErrno;
            return NULL;
        }
        DWORD error = GetLastError();
        if (error != ERROR_FILE_EXISTS && error != ERROR_ALREADY_EXISTS)
        {
            native_set_windows_errno(error);
            break;
        }
    }
    int savedErrno = errno;
    free(path);
    errno = savedErrno;
    return NULL;
#else
    char* path = path_with_suffix(finalPath, ".tmp.XXXXXX");
    if (path == NULL)
    {
        errno = ENOMEM;
        return NULL;
    }
    int descriptor = mkstemp(path);
    if (descriptor < 0)
    {
        int savedErrno = errno;
        free(path);
        errno = savedErrno;
        return NULL;
    }
    FILE* output = native_fdopen_binary_update(descriptor);
    if (output != NULL)
    {
        *tempPath = path;
        return output;
    }
    int savedErrno = errno;
    native_close_descriptor(descriptor);
    unlink(path);
    free(path);
    errno = savedErrno;
    return NULL;
#endif
}

/* Publishes one verified staging stream without replacing an existing final path. */
static bool native_publish_noreplace(FILE* stagedFile, const char* tempPath, const char* finalPath, bool* published)
{
    *published = false;
    if (fflush(stagedFile) != 0 || !native_stream_path_matches(stagedFile, tempPath))
        return false;
#if defined(_WIN32)
    if (!native_rename_stream_noreplace(stagedFile, finalPath))
        return false;
    *published = true;
    return native_stream_path_matches(stagedFile, finalPath);
#else
    if (link(tempPath, finalPath) != 0)
        return false;
    *published = true;
    return native_stream_path_matches(stagedFile, finalPath);
#endif
}

/* Drops the moved Windows staging name after its stream has a verified final name. */
static void native_retire_published_temp_path(char** tempPath)
{
#if defined(_WIN32)
    free(*tempPath);
    *tempPath = NULL;
#else
    (void)tempPath;
#endif
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
    *order = PORYAAAA_TRACE_ORDER_EXTENDED | (recorder->tracePosition.order << PORYAAAA_TRACE_ORDER_SEQUENCE_SHIFT);
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
    if (observation->cyclesLate > PORYAAAA_TRACE_ORDER_DELAY_MASK ||
        !next_trace_position(recorder, observation->cycle, &order))
        return false;
    order |= observation->cyclesLate;
    return fprintf(recorder->traceOutput,
                   "TIMER %" PRIu64 " %" PRIu32 " %" PRIu32 "\n",
                   observation->cycle,
                   order,
                   observation->value) > 0;
}

/* Serializes sample observation lateness without adding a second trace time domain. */
static bool write_trace_sample(Recorder* recorder, const struct GBAAudioObservation* observation)
{
    uint32_t order = 0u;
    if (observation->cyclesLate > PORYAAAA_TRACE_ORDER_DELAY_MASK ||
        !next_trace_position(recorder, observation->cycle, &order))
        return false;
    order |= observation->cyclesLate;
    return fprintf(recorder->traceOutput, "SAMPLE %" PRIu64 " %" PRIu32 "\n", observation->cycle, order) > 0;
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

/* Returns the logical MP2K VBlank count that defines each fixed lifecycle. */
static uint32_t driver_scenario_logical_vblanks(DriverScenario scenario)
{
    switch (scenario)
    {
    case DRIVER_SCENARIO_START:
        return 1u;
    case DRIVER_SCENARIO_ENVELOPE:
        return 6u;
    case DRIVER_SCENARIO_PITCH:
    case DRIVER_SCENARIO_VOLUME_PAN:
        return 4u;
    case DRIVER_SCENARIO_RETRIGGER:
        return 5u;
    case DRIVER_SCENARIO_RELEASE:
        return 6u;
    case DRIVER_SCENARIO_NONE:
        return 0u;
    }
    return 0u;
}

/* Returns the native frame span, including one post-action frame for observation. */
static uint32_t driver_scenario_span_frames(DriverScenario scenario)
{
    switch (scenario)
    {
    case DRIVER_SCENARIO_START:
        return DRIVER_SCENARIO_START_FRAMES;
    case DRIVER_SCENARIO_ENVELOPE:
        return DRIVER_SCENARIO_ENVELOPE_FRAMES;
    case DRIVER_SCENARIO_PITCH:
        return DRIVER_SCENARIO_PITCH_FRAMES;
    case DRIVER_SCENARIO_VOLUME_PAN:
        return DRIVER_SCENARIO_VOLUME_PAN_FRAMES;
    case DRIVER_SCENARIO_RETRIGGER:
        return DRIVER_SCENARIO_RETRIGGER_FRAMES;
    case DRIVER_SCENARIO_RELEASE:
        return DRIVER_SCENARIO_RELEASE_FRAMES;
    case DRIVER_SCENARIO_NONE:
        return 0u;
    }
    return 0u;
}

/* Keep action scheduling fixed while retaining the final SoundMain callback
 * after its native VBlank phase and one replay-slack SAMPLE period. */
static uint64_t driver_scenario_span_cycles(DriverScenario scenario)
{
    if (scenario == DRIVER_SCENARIO_NONE)
        return 0u;
    uint64_t span =
        (uint64_t)driver_scenario_span_frames(scenario) * GBA_CYCLES_PER_FRAME + DRIVER_SCENARIO_CAPTURE_TAIL_CYCLES;
    return ((span + 511u) & ~UINT64_C(511)) + 512u;
}

/* Opens the measurement interval after every preceding reset/setup event was traced. */
static bool begin_native_capture(struct mCore* core, Recorder* recorder, const Options* options)
{
    if (!drain_observations(core, recorder))
        return false;
    uint64_t startCycle = next_sample_cycle(core);
    uint64_t durationCycles = driver_scenario_span_cycles(options->driverScenario);
    if (durationCycles == 0u)
    {
        /* Legacy captures keep their user-duration interval; a fixed driver
           transaction adds one replay slack SAMPLE period after its callback tail. */
        double captureCycles = options->durationSeconds * (double)PORYAAAA_GBA_CLOCK_HZ;
        if (captureCycles > (double)(UINT64_MAX - startCycle))
            return false;
        durationCycles = (uint64_t)(captureCycles + 0.5);
    }
    if (durationCycles == 0u || !write_trace_marker(recorder, "BEGIN", startCycle))
        return false;
    recorder->nativeBeginCycle = startCycle;
    recorder->nativeEndCycle = startCycle + durationCycles;
    recorder->nativeCapturing = true;
    return true;
}

/* Opens exclusive native staging streams; final destinations are never prepared by deletion. */
static bool prepare_native_capture(Recorder* recorder, const Options* options)
{
    recorder->observationCapacity = OBSERVATION_BUFFER_CAPACITY;
    recorder->observations = malloc(recorder->observationCapacity * sizeof(*recorder->observations));
    recorder->nativePcmPath = path_with_suffix(options->nativeOutputPrefix, ".pcm");
    recorder->nativeCyclesPath = path_with_suffix(options->nativeOutputPrefix, ".cycles");
    recorder->nativeManifestPath = path_with_suffix(options->nativeOutputPrefix, ".json");
    if (recorder->observations == NULL || recorder->nativePcmPath == NULL || recorder->nativeCyclesPath == NULL ||
        recorder->nativeManifestPath == NULL || strcmp(options->traceOutputPath, recorder->nativePcmPath) == 0 ||
        strcmp(options->traceOutputPath, recorder->nativeCyclesPath) == 0 ||
        strcmp(options->traceOutputPath, recorder->nativeManifestPath) == 0 ||
        (options->elfPath != NULL && (strcmp(options->elfPath, options->traceOutputPath) == 0 ||
                                      strcmp(options->elfPath, recorder->nativePcmPath) == 0 ||
                                      strcmp(options->elfPath, recorder->nativeCyclesPath) == 0 ||
                                      strcmp(options->elfPath, recorder->nativeManifestPath) == 0)))
    {
        errno = EINVAL;
        return false;
    }
    recorder->traceOutput = native_open_unique_temp(options->traceOutputPath, &recorder->traceTempPath);
    recorder->nativePcmOutput = native_open_unique_temp(recorder->nativePcmPath, &recorder->nativePcmTempPath);
    recorder->nativeCyclesOutput = native_open_unique_temp(recorder->nativeCyclesPath, &recorder->nativeCyclesTempPath);
    recorder->nativeManifestOutput =
        native_open_unique_temp(recorder->nativeManifestPath, &recorder->nativeManifestTempPath);
    if (recorder->traceOutput == NULL || recorder->nativePcmOutput == NULL || recorder->nativeCyclesOutput == NULL ||
        recorder->nativeManifestOutput == NULL ||
        fprintf(recorder->traceOutput, PORYAAAA_AUDIO_TRACE_HEADER "\n" PORYAAAA_AUDIO_TRACE_CLOCK_LINE "\n") < 0)
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

/* Returns the stable family label consumed by the generic lifecycle harness. */
static const char* driver_family_name(DriverFamily family)
{
    switch (family)
    {
    case DRIVER_FAMILY_DIRECTSOUND:
        return "directsound";
    case DRIVER_FAMILY_SQ1:
        return "sq1";
    case DRIVER_FAMILY_SQ2:
        return "sq2";
    case DRIVER_FAMILY_PSW:
        return "psw";
    case DRIVER_FAMILY_NONE:
        return NULL;
    }
    return NULL;
}

/* Returns the canonical manifest name for a fixed driver lifecycle. */
static const char* driver_scenario_name(DriverScenario scenario)
{
    switch (scenario)
    {
    case DRIVER_SCENARIO_START:
        return "start";
    case DRIVER_SCENARIO_ENVELOPE:
        return "envelope";
    case DRIVER_SCENARIO_PITCH:
        return "pitch";
    case DRIVER_SCENARIO_VOLUME_PAN:
        return "volume-pan";
    case DRIVER_SCENARIO_RETRIGGER:
        return "retrigger";
    case DRIVER_SCENARIO_RELEASE:
        return "release";
    case DRIVER_SCENARIO_NONE:
        return NULL;
    }
    return NULL;
}

/* Describes externally meaningful lifecycle controls without asserting a bus transaction oracle. */
static const char* driver_scenario_high_level_action(DriverScenario scenario)
{
    switch (scenario)
    {
    case DRIVER_SCENARIO_START:
        return "note-on at tick 0";
    case DRIVER_SCENARIO_ENVELOPE:
        return "note-on at tick 0; sustain through tick 6";
    case DRIVER_SCENARIO_PITCH:
        return "note-on at tick 0; pitch bend +16 at tick 2; sustain through tick 4";
    case DRIVER_SCENARIO_VOLUME_PAN:
        return "note-on at tick 0; volume 32 and pan 127 at tick 2; sustain through tick 4";
    case DRIVER_SCENARIO_RETRIGGER:
        return "note-on at tick 0; note-off at tick 2; note-on at tick 3; sustain through tick 5";
    case DRIVER_SCENARIO_RELEASE:
        return "note-on at tick 0; note-off at tick 2; release through tick 6";
    case DRIVER_SCENARIO_NONE:
        return NULL;
    }
    return NULL;
}

/* Appends resolved ROM fixture provenance while leaving the transaction oracle independent. */
static bool write_driver_manifest_fields(FILE* output,
                                         const Options* options,
                                         const DriverFixtureIdentity* identity,
                                         uint64_t scenarioBeginCycle,
                                         uint64_t scenarioEndCycle,
                                         const char elfSha256[65],
                                         const char pcmSha256[65],
                                         const char cyclesSha256[65])
{
    const char* family = driver_family_name(identity->family);
    const char* scenario = driver_scenario_name(options->driverScenario);
    const char* highLevelAction = driver_scenario_high_level_action(options->driverScenario);
    bool ok =
        family != NULL && scenario != NULL && highLevelAction != NULL && fputs("  \"elf_sha256\": ", output) >= 0 &&
        write_json_string(output, elfSha256) && fputs(",\n", output) >= 0 &&
        fputs("  \"voicegroup_symbol\": ", output) >= 0 && write_json_string(output, options->voicegroupSymbol) &&
        fputs(",\n", output) >= 0 && fprintf(output, "  \"voice_index\": %" PRIu32 ",\n", options->voiceIndex) > 0 &&
        fprintf(output, "  \"rom_voice_address\": %" PRIu32 ",\n", options->voiceAddress) > 0 &&
        fputs("  \"family\": ", output) >= 0 && write_json_string(output, family) && fputs(",\n", output) >= 0 &&
        fputs("  \"tone_data_sha256\": ", output) >= 0 && write_json_string(output, identity->toneDataSha256) &&
        fputs(",\n", output) >= 0 && fputs("  \"family_payload_sha256\": ", output) >= 0 &&
        write_json_string(output, identity->familyPayloadSha256) && fputs(",\n", output) >= 0 &&
        fprintf(output, "  \"family_payload_size\": %" PRIu32 ",\n", identity->payloadSize) > 0 &&
        fprintf(output, "  \"resolved_type\": %u,\n", (unsigned)identity->resolvedType) > 0 &&
        fputs("  \"scenario\": ", output) >= 0 && write_json_string(output, scenario) && fputs(",\n", output) >= 0 &&
        fprintf(output,
                "  \"scenario_logical_vblanks\": %u,\n",
                (unsigned)driver_scenario_logical_vblanks(options->driverScenario)) > 0 &&
        fprintf(output,
                "  \"scenario_capture_frames\": %u,\n",
                (unsigned)driver_scenario_span_frames(options->driverScenario)) > 0 &&
        fprintf(output,
                "  \"scenario_span_frames\": %u,\n",
                (unsigned)driver_scenario_span_frames(options->driverScenario)) > 0 &&
        fprintf(output,
                "  \"scenario_span_cycles\": %" PRIu64 ",\n",
                driver_scenario_span_cycles(options->driverScenario)) > 0 &&
        fprintf(output, "  \"scenario_begin_cycle\": %" PRIu64 ",\n", scenarioBeginCycle) > 0 &&
        fprintf(output, "  \"scenario_end_cycle\": %" PRIu64 ",\n", scenarioEndCycle) > 0 &&
        fputs("  \"high_level_action\": ", output) >= 0 && write_json_string(output, highLevelAction) &&
        fputs(",\n", output) >= 0 && fprintf(output, "  \"note\": %u,\n", (unsigned)options->note) > 0 &&
        fprintf(output, "  \"velocity\": %u,\n", (unsigned)options->velocity) > 0 &&
        fprintf(output, "  \"volume\": %u,\n", (unsigned)options->volume) > 0 &&
        fprintf(output, "  \"pan\": %u,\n", (unsigned)options->pan) > 0 && fputs("  \"pcm_sha256\": ", output) >= 0 &&
        write_json_string(output, pcmSha256) && fputs(",\n", output) >= 0 &&
        fputs("  \"cycles_sha256\": ", output) >= 0 && write_json_string(output, cyclesSha256);
    if (identity->family == DRIVER_FAMILY_PSW)
    {
        ok = ok && fputs(",\n", output) >= 0 && fputs("  \"waveform_sha256\": ", output) >= 0 &&
             write_json_string(output, identity->waveformSha256);
    }
    return ok;
}

/* Emits the exact native capture schema shared with poryaaaa_audio_trace. */
static bool write_native_manifest(const Recorder* recorder,
                                  const Options* options,
                                  const DriverFixtureIdentity* identity,
                                  const char romSha256[65],
                                  const char traceSha256[65],
                                  const char elfSha256[65],
                                  const char pcmSha256[65],
                                  const char cyclesSha256[65])
{
    FILE* output = recorder->nativeManifestOutput;
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
                      PORYAAAA_GBA_CLOCK_HZ,
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
         write_json_string(output, PORYAAAA_MGBA_RECORDER_COMPILE_FLAGS) && fputs(",\n", output) >= 0;
    ok = ok && fputs("  \"rom_sha256\": ", output) >= 0 && write_json_string(output, romSha256) &&
         fputs(",\n", output) >= 0 && fputs("  \"trace_sha256\": ", output) >= 0 &&
         write_json_string(output, traceSha256);
    if (identity != NULL)
        ok = ok && fputs(",\n", output) >= 0 &&
             write_driver_manifest_fields(output,
                                          options,
                                          identity,
                                          recorder->nativeBeginCycle,
                                          recorder->nativeEndCycle,
                                          elfSha256,
                                          pcmSha256,
                                          cyclesSha256);
    ok = ok && fputs("\n}\n", output) >= 0;
    if (fflush(output) != 0)
        ok = false;
    return ok;
}

/* Publishes a complete native sibling set without replacing any destination. */
static bool finish_native_capture(Recorder* recorder, const Options* options)
{
    if (recorder->nativeWriteFailed || !recorder->nativeFinished || recorder->nativeFramesWritten == 0u ||
        recorder->nativePeak == 0 || recorder->traceOutput == NULL || recorder->nativePcmOutput == NULL ||
        recorder->nativeCyclesOutput == NULL || recorder->nativeManifestOutput == NULL)
    {
        return false;
    }
    bool outputsFlushed = fflush(recorder->traceOutput) == 0 && fflush(recorder->nativePcmOutput) == 0 &&
                          fflush(recorder->nativeCyclesOutput) == 0;
    if (!outputsFlushed)
        return false;

    char romSha256[65];
    char traceSha256[65];
    char elfSha256[65];
    char pcmSha256[65];
    char cyclesSha256[65];
    const DriverFixtureIdentity* identity =
        options->driverScenario != DRIVER_SCENARIO_NONE ? &recorder->driverIdentity : NULL;
    bool hashesOk = sha256_file(options->romPath, romSha256) && sha256_file(recorder->traceTempPath, traceSha256);
    if (identity != NULL)
    {
        hashesOk = hashesOk && sha256_file(options->elfPath, elfSha256) &&
                   sha256_file(recorder->nativePcmTempPath, pcmSha256) &&
                   sha256_file(recorder->nativeCyclesTempPath, cyclesSha256);
    }
    if (!hashesOk ||
        !write_native_manifest(recorder, options, identity, romSha256, traceSha256, elfSha256, pcmSha256, cyclesSha256))
    {
        return false;
    }
    if (!native_publish_noreplace(
            recorder->traceOutput, recorder->traceTempPath, options->traceOutputPath, &recorder->tracePublished))
    {
        goto publication_failed;
    }
    native_retire_published_temp_path(&recorder->traceTempPath);
    if (!native_publish_noreplace(recorder->nativePcmOutput,
                                  recorder->nativePcmTempPath,
                                  recorder->nativePcmPath,
                                  &recorder->nativePcmPublished))
    {
        goto publication_failed;
    }
    native_retire_published_temp_path(&recorder->nativePcmTempPath);
    if (!native_publish_noreplace(recorder->nativeCyclesOutput,
                                  recorder->nativeCyclesTempPath,
                                  recorder->nativeCyclesPath,
                                  &recorder->nativeCyclesPublished))
    {
        goto publication_failed;
    }
    native_retire_published_temp_path(&recorder->nativeCyclesTempPath);
    if (!native_publish_noreplace(recorder->nativeManifestOutput,
                                  recorder->nativeManifestTempPath,
                                  recorder->nativeManifestPath,
                                  &recorder->nativeManifestPublished))
    {
        goto publication_failed;
    }
    native_retire_published_temp_path(&recorder->nativeManifestTempPath);
    return true;

publication_failed:
    recorder->nativePublicationFailed = true;
    return false;
}

/* Releases only recorder-owned entries, retaining complete stages after publication failures. */
static void discard_native_capture(Recorder* recorder, const Options* options, bool successful)
{
    if (!successful)
    {
        if (recorder->nativeManifestPublished)
            native_remove_matching_stream_path(recorder->nativeManifestOutput, recorder->nativeManifestPath);
        if (recorder->nativeCyclesPublished)
            native_remove_matching_stream_path(recorder->nativeCyclesOutput, recorder->nativeCyclesPath);
        if (recorder->nativePcmPublished)
            native_remove_matching_stream_path(recorder->nativePcmOutput, recorder->nativePcmPath);
        if (recorder->tracePublished)
            native_remove_matching_stream_path(recorder->traceOutput, options->traceOutputPath);
    }
    if (!recorder->nativePublicationFailed)
    {
        if (recorder->traceTempPath != NULL)
            native_remove_matching_stream_path(recorder->traceOutput, recorder->traceTempPath);
        if (recorder->nativePcmTempPath != NULL)
            native_remove_matching_stream_path(recorder->nativePcmOutput, recorder->nativePcmTempPath);
        if (recorder->nativeCyclesTempPath != NULL)
            native_remove_matching_stream_path(recorder->nativeCyclesOutput, recorder->nativeCyclesTempPath);
        if (recorder->nativeManifestTempPath != NULL)
            native_remove_matching_stream_path(recorder->nativeManifestOutput, recorder->nativeManifestTempPath);
    }
    if (recorder->traceOutput != NULL)
        fclose(recorder->traceOutput);
    if (recorder->nativePcmOutput != NULL)
        fclose(recorder->nativePcmOutput);
    if (recorder->nativeCyclesOutput != NULL)
        fclose(recorder->nativeCyclesOutput);
    if (recorder->nativeManifestOutput != NULL)
        fclose(recorder->nativeManifestOutput);
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
    recorder->nativeManifestOutput = NULL;
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

/* Resolves only the hardware families covered by the fixed differential matrix. */
static bool resolve_driver_family(uint8_t type, DriverFamily* family, uint32_t* soloMask)
{
    switch (type)
    {
    case 0x00u:
        *family = DRIVER_FAMILY_DIRECTSOUND;
        *soloMask = AUDIO_CHANNEL_DIRECTSOUND;
        return true;
    case 0x01u:
        *family = DRIVER_FAMILY_SQ1;
        *soloMask = AUDIO_CHANNEL_SQ1;
        return true;
    case 0x02u:
        *family = DRIVER_FAMILY_SQ2;
        *soloMask = AUDIO_CHANNEL_SQ2;
        return true;
    case 0x03u:
    case 0x0Bu:
        *family = DRIVER_FAMILY_PSW;
        *soloMask = AUDIO_CHANNEL_WAVE;
        return true;
    default:
        return false;
    }
}

/* Rejects malformed ROM spans before payload hashing can read a mirrored bus address. */
static bool is_gba_rom_span(uint32_t address, uint32_t size)
{
    return address >= GBA_ROM_START && address <= GBA_ROM_END && size <= GBA_ROM_END - address;
}

/* Hashes the original ROM payload incrementally so DirectSound identity needs no host allocation. */
static bool sha256_rom_bytes(struct mCore* core, uint32_t address, uint32_t size, char output[65])
{
    if (!is_gba_rom_span(address, size))
        return false;
    Sha256 sha;
    sha256_init(&sha);
    uint8_t buffer[256];
    uint32_t offset = 0u;
    while (offset < size)
    {
        uint32_t count = size - offset;
        if (count > sizeof(buffer))
            count = sizeof(buffer);
        for (uint32_t i = 0u; i < count; ++i)
            buffer[i] = (uint8_t)core->busRead8(core, address + offset + i);
        sha256_update(&sha, buffer, count);
        offset += count;
    }
    sha256_finish(&sha, output);
    return true;
}

/* Reads a GBA little-endian pointer or size from the byte-exact ROM representation. */
static uint32_t read_le_u32(const uint8_t bytes[4])
{
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8u) | ((uint32_t)bytes[2] << 16u) | ((uint32_t)bytes[3] << 24u);
}

/* Captures the real ToneData and only payload needed to prove the resolved family fixture. */
static bool capture_driver_identity(struct mCore* core, const Options* options, DriverFixtureIdentity* identity)
{
    for (uint32_t i = 0u; i < TONE_DATA_SIZE; ++i)
        identity->toneData[i] = (uint8_t)core->busRead8(core, options->voiceAddress + i);
    identity->resolvedType = identity->toneData[0];
    if (!resolve_driver_family(identity->resolvedType, &identity->family, &identity->soloMask))
    {
        fprintf(stderr,
                "Driver fixture at 0x%08" PRIX32 " has unsupported ToneData type 0x%02X\n",
                options->voiceAddress,
                (unsigned)identity->resolvedType);
        return false;
    }
    memcpy(identity->normalizedToneData, identity->toneData, TONE_DATA_SIZE);
    memset(identity->normalizedToneData + 4u, 0, 4u);
    sha256_bytes(identity->normalizedToneData, TONE_DATA_SIZE, identity->toneDataSha256);
    if (identity->family == DRIVER_FAMILY_PSW)
    {
        identity->payloadAddress = read_le_u32(identity->toneData + 4u);
        identity->payloadSize = PSW_WAVEFORM_SIZE;
        if (!sha256_rom_bytes(core, identity->payloadAddress, identity->payloadSize, identity->familyPayloadSha256))
            return false;
        for (uint32_t i = 0u; i < PSW_WAVEFORM_SIZE; ++i)
            identity->waveform[i] = (uint8_t)core->busRead8(core, identity->payloadAddress + i);
        sha256_bytes(identity->waveform, PSW_WAVEFORM_SIZE, identity->waveformSha256);
    }
    else if (identity->family == DRIVER_FAMILY_DIRECTSOUND)
    {
        uint8_t header[DIRECTSOUND_WAVE_HEADER_SIZE];
        identity->payloadAddress = read_le_u32(identity->toneData + 4u);
        if (!is_gba_rom_span(identity->payloadAddress, DIRECTSOUND_WAVE_HEADER_SIZE))
            return false;
        for (uint32_t i = 0u; i < DIRECTSOUND_WAVE_HEADER_SIZE; ++i)
            header[i] = (uint8_t)core->busRead8(core, identity->payloadAddress + i);
        uint32_t sampleSize = read_le_u32(header + 12u);
        if (sampleSize > UINT32_MAX - DIRECTSOUND_WAVE_HEADER_SIZE)
            return false;
        identity->payloadSize = DIRECTSOUND_WAVE_HEADER_SIZE + sampleSize;
        if (!sha256_rom_bytes(core, identity->payloadAddress, identity->payloadSize, identity->familyPayloadSha256))
            return false;
    }
    else
    {
        const uint8_t squarePayload[] = {
            identity->toneData[3],
            identity->toneData[4],
            identity->toneData[8],
            identity->toneData[9],
            identity->toneData[10],
            identity->toneData[11],
        };
        identity->payloadSize = 6u;
        sha256_bytes(squarePayload, sizeof(squarePayload), identity->familyPayloadSha256);
    }
    identity->captured = true;
    return true;
}

/* Appends bytes to the compact MP2K track without admitting arbitrary scripts. */
static bool append_driver_track_bytes(DriverTrack* track, const uint8_t* bytes, size_t length)
{
    if (length > sizeof(track->bytes) - track->length)
        return false;
    memcpy(track->bytes + track->length, bytes, length);
    track->length += length;
    return true;
}

/* Builds fixed MP2K controls independently of any expected hardware transactions. */
static bool build_driver_fixture_track(const Options* options, DriverTrack* track)
{
    *track = (DriverTrack){0};
    const uint8_t prefix[] = {
        0xBCu,
        0u, /* KEYSH 0 */
        0xBBu,
        (options->driverScenario == DRIVER_SCENARIO_NONE || options->driverScenario == DRIVER_SCENARIO_START) ? 60u
                                                                                                              : 75u,
        0xBDu,
        0u, /* VOICE 0 */
        0xBEu,
        options->volume, /* VOL */
        0xBFu,
        options->pan, /* PAN */
        0xCFu,
        options->note,
        options->velocity, /* TIE */
    };
    if (!append_driver_track_bytes(track, prefix, sizeof(prefix)))
        return false;
    switch (options->driverScenario)
    {
    case DRIVER_SCENARIO_NONE:
    case DRIVER_SCENARIO_START:
    {
        const uint8_t commands[] = {0xB0u, 0xB2u, 0u, 0u, 0u, 0u};
        track->loopOffset = track->length;
        return append_driver_track_bytes(track, commands, sizeof(commands));
    }
    case DRIVER_SCENARIO_ENVELOPE:
    {
        const uint8_t commands[] = {0x86u, 0xB2u, 0u, 0u, 0u, 0u}; /* W06; GOTO */
        track->loopOffset = track->length;
        return append_driver_track_bytes(track, commands, sizeof(commands));
    }
    case DRIVER_SCENARIO_PITCH:
    {
        const uint8_t beforeBend[] = {0x82u, 0xC0u, 0x50u};         /* W02; BEND +16 */
        const uint8_t afterBend[] = {0x82u, 0xB2u, 0u, 0u, 0u, 0u}; /* W02; GOTO */
        if (!append_driver_track_bytes(track, beforeBend, sizeof(beforeBend)))
            return false;
        track->loopOffset = track->length;
        return append_driver_track_bytes(track, afterBend, sizeof(afterBend));
    }
    case DRIVER_SCENARIO_VOLUME_PAN:
    {
        const uint8_t beforeControls[] = {0x82u, 0xBEu, 32u, 0xBFu, 127u}; /* W02; VOL; PAN */
        const uint8_t afterControls[] = {0x82u, 0xB2u, 0u, 0u, 0u, 0u};    /* W02; GOTO */
        if (!append_driver_track_bytes(track, beforeControls, sizeof(beforeControls)))
            return false;
        track->loopOffset = track->length;
        return append_driver_track_bytes(track, afterControls, sizeof(afterControls));
    }
    case DRIVER_SCENARIO_RETRIGGER:
    {
        const uint8_t beforeRetrigger[] = {
            0x82u, 0xCEu, options->note, 0x81u, 0xCFu, options->note, options->velocity}; /* W02; EOT; W01; TIE */
        const uint8_t afterRetrigger[] = {0x82u, 0xB2u, 0u, 0u, 0u, 0u};                  /* W02; GOTO */
        if (!append_driver_track_bytes(track, beforeRetrigger, sizeof(beforeRetrigger)))
            return false;
        track->loopOffset = track->length;
        return append_driver_track_bytes(track, afterRetrigger, sizeof(afterRetrigger));
    }
    case DRIVER_SCENARIO_RELEASE:
    {
        const uint8_t beforeRelease[] = {0x82u, 0xCEu, options->note}; /* W02; EOT */
        const uint8_t afterRelease[] = {0x84u, 0xB2u, 0u, 0u, 0u, 0u}; /* W04; GOTO */
        if (!append_driver_track_bytes(track, beforeRelease, sizeof(beforeRelease)))
            return false;
        track->loopOffset = track->length;
        return append_driver_track_bytes(track, afterRelease, sizeof(afterRelease));
    }
    }
    return false;
}

/* Writes a relocation-safe one-track driver fixture in EWRAM. */
static bool inject_fixture_track(struct mCore* core, const Options* options)
{
    DriverTrack track;
    if (!build_driver_fixture_track(options, &track) || track.length < sizeof(uint32_t) ||
        track.loopOffset >= track.length - sizeof(uint32_t))
    {
        return false;
    }
    uint32_t headerAddress = options->fixtureAddress + FIXTURE_HEADER_OFFSET;
    uint32_t trackAddress = options->fixtureAddress + FIXTURE_TRACK_OFFSET;
    uint32_t loopAddress = trackAddress + (uint32_t)track.loopOffset;
    size_t pointerOffset = track.length - sizeof(uint32_t);
    for (uint32_t i = 0u; i < sizeof(loopAddress); ++i)
        track.bytes[pointerOffset + i] = (uint8_t)(loopAddress >> (i * 8u));
    core->busWrite32(core, headerAddress, 1u); /* trackCount=1, blockCount=priority=reverb=0 */
    core->busWrite32(core, headerAddress + 4u, options->fixtureAddress + FIXTURE_VOICE_OFFSET);
    core->busWrite32(core, headerAddress + 8u, trackAddress);
    for (size_t i = 0u; i < track.length; ++i)
        core->busWrite8(core, trackAddress + (uint32_t)i, track.bytes[i]);
    return true;
}

/* Exposes fixed bytecode so focused tests verify control dispatch without a bus oracle. */
static bool dump_driver_fixture_track(const char* scenarioName)
{
    DriverScenario scenario;
    if (!parse_driver_scenario(scenarioName, &scenario))
        return false;
    Options options = {
        .fixtureAddress = 0x0203F000u,
        .driverScenario = scenario,
        .note = 60u,
        .velocity = 127u,
        .volume = 127u,
        .pan = 64u,
    };
    DriverTrack track;
    if (!build_driver_fixture_track(&options, &track) || track.length < sizeof(uint32_t) ||
        track.loopOffset >= track.length - sizeof(uint32_t))
    {
        return false;
    }
    uint32_t loopAddress = options.fixtureAddress + FIXTURE_TRACK_OFFSET + (uint32_t)track.loopOffset;
    size_t pointerOffset = track.length - sizeof(uint32_t);
    for (uint32_t i = 0u; i < sizeof(loopAddress); ++i)
        track.bytes[pointerOffset + i] = (uint8_t)(loopAddress >> (i * 8u));
    for (size_t i = 0u; i < track.length; ++i)
    {
        if (printf("%02X", track.bytes[i]) < 0)
            return false;
    }
    return putchar('\n') != EOF;
}

/* Exposes fixed measurement spans to the focused adapter behavior test. */
static bool dump_driver_scenario_span(const char* scenarioName)
{
    DriverScenario scenario;
    if (!parse_driver_scenario(scenarioName, &scenario))
        return false;
    return printf("%" PRIu64 "\n", driver_scenario_span_cycles(scenario)) >= 0;
}

/* Exposes type dispatch so focused tests cover every accepted family without a ROM fixture. */
static bool dump_driver_family(const char* typeText)
{
    uint32_t type = 0u;
    DriverFamily family = DRIVER_FAMILY_NONE;
    uint32_t soloMask = 0u;
    if (!parse_u32(typeText, &type) || type > UINT8_MAX || !resolve_driver_family((uint8_t)type, &family, &soloMask))
        return false;
    const char* name = driver_family_name(family);
    return name != NULL && printf("%s %" PRIu32 "\n", name, soloMask) >= 0;
}

/* Builds a one-track song in EWRAM for deterministic isolated playback. */
static bool inject_song(struct mCore* core, const Options* options, const DriverFixtureIdentity* identity)
{
    uint32_t fixtureVoiceAddress = options->fixtureAddress + FIXTURE_VOICE_OFFSET;
    switch (options->driverScenario)
    {
    case DRIVER_SCENARIO_NONE:
        inject_voice(core, options->voiceAddress, fixtureVoiceAddress);
        break;
    case DRIVER_SCENARIO_START:
    case DRIVER_SCENARIO_ENVELOPE:
    case DRIVER_SCENARIO_PITCH:
    case DRIVER_SCENARIO_VOLUME_PAN:
    case DRIVER_SCENARIO_RETRIGGER:
    case DRIVER_SCENARIO_RELEASE:
        if (!identity->captured)
            return false;
        for (uint32_t i = 0u; i < TONE_DATA_SIZE; ++i)
            core->busWrite8(core, fixtureVoiceAddress + i, identity->toneData[i]);
        for (uint32_t i = 0u; i < TONE_DATA_SIZE; ++i)
        {
            if (core->busRead8(core, fixtureVoiceAddress + i) != identity->toneData[i])
                return false;
        }
        break;
    }
    if (!inject_fixture_track(core, options))
        return false;
    uint32_t runnerAddress = options->fixtureAddress + FIXTURE_RUNNER_OFFSET;
    core->busWrite16(core, runnerAddress, 0xE7FEu); /* b runner; VBlank IRQ preempts it */
    return true;
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

static bool
wait_for_runner_ident(struct mCore* core, Recorder* recorder, const Options* options, uint32_t expectedSoundIdent);
static bool wait_for_runner(struct mCore* core, Recorder* recorder, const Options* options);

/* Invokes one no-argument ROM routine and verifies its expected SoundInfo lock state. */
static bool call_rom_void(
    struct mCore* core, Recorder* recorder, const Options* options, uint32_t address, uint32_t expectedSoundIdent)
{
    int32_t runnerAddress = (int32_t)(options->fixtureAddress + FIXTURE_RUNNER_OFFSET + 1u);
    return core->writeRegister(core, "lr", runnerAddress) &&
           core->writeRegister(core, "pc", (int32_t)(address & ~1u)) &&
           wait_for_runner_ident(core, recorder, options, expectedSoundIdent);
}

/* Reinitializes the real MP2K DMA streams, then pauses timer 0 so the
 * next natural VCount callback can arm a fresh seven-frame epoch. */
static bool restart_pcm_dma(struct mCore* core, Recorder* recorder, const Options* options, uint16_t* timerControl)
{
    if (!call_rom_void(core, recorder, options, options->m4aVSyncOff, MP2K_MAGIC + 10u))
        return false;

    if (!call_rom_void(core, recorder, options, options->m4aVSyncOn, MP2K_MAGIC))
        return false;

    *timerControl = core->busRead16(core, GBA_REG_TM0CNT_H);
    core->busWrite16(core, GBA_REG_TM0CNT_H, 0u);
    uint16_t soundControl = core->busRead16(core, GBA_REG_SOUNDCNT_H);
    core->busWrite16(core, GBA_REG_SOUNDCNT_H, soundControl | GBA_FIFO_RESET_BITS);
    core->busWrite16(core, GBA_REG_SOUNDCNT_H, soundControl);
    uint64_t resetCycle = current_gba_cycle(core);
    do
    {
        core->runLoop(core);
        if (!drain_observations(core, recorder))
            return false;
    } while (next_sample_cycle(core) <= resetCycle);
    return true;
}

/* Stops boot-time players and drains the FIFO reset before arming the fixture. */
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
    core->busWrite8(core, options->soundInfo + 14u, 0u);
    core->busWrite8(core, options->soundInfo + 15u, 0u);
    for (uint32_t sample = 0u; sample < 3168u; ++sample)
        core->busWrite8(core, options->soundInfo + 848u + sample, 0u);
    uint16_t soundControl = core->busRead16(core, GBA_REG_SOUNDCNT_H);
    uint64_t resetCycle = current_gba_cycle(core);
    core->busWrite16(core, GBA_REG_SOUNDCNT_H, soundControl | GBA_FIFO_RESET_BITS);
    core->busWrite16(core, GBA_REG_SOUNDCNT_H, soundControl);

    /* Let the next mGBA sample boundary pass while every player is stopped.
       Its reset writes must be setup records before the fixture marker. */
    do
    {
        core->runLoop(core);
        if (!drain_observations(core, recorder))
            return false;
    } while (next_sample_cycle(core) <= resetCycle);
    return true;
}

/* Boots until MP2K and both audio-driving video interrupt paths are live. */
static bool wait_for_mp2k(struct mCore* core, Recorder* recorder, const Options* options)
{
    uint64_t maxFrames = (uint64_t)(options->bootTimeoutSeconds * GBA_FRAMES_PER_SECOND + 0.5);
    for (uint64_t frame = 0u; frame < maxFrames; ++frame)
    {
        if (!run_frame_and_drain(core, recorder))
            return false;
        const uint16_t dispstat = core->busRead16(core, GBA_REG_DISPSTAT);
        const uint16_t enabledInterrupts = core->busRead16(core, GBA_REG_IE);
        if (core->busRead32(core, options->mplayInfo + 52u) == MP2K_MAGIC &&
            (dispstat & (GBA_DISPSTAT_VBLANK_IRQ | GBA_DISPSTAT_VCOUNT_IRQ | 0xFF00u)) ==
                (GBA_DISPSTAT_VBLANK_IRQ | GBA_DISPSTAT_VCOUNT_IRQ | GBA_DISPSTAT_VCOUNT_LINE_150) &&
            (enabledInterrupts & (GBA_IE_VBLANK | GBA_IE_VCOUNT)) == (GBA_IE_VBLANK | GBA_IE_VCOUNT) &&
            (core->busRead16(core, GBA_REG_IME) & 0x1u) != 0u)
        {
            return true;
        }
    }
    return false;
}

/* Steps through the video IRQ epilogue so later fixture calls do not strand it. */
static bool wait_for_sound_main_idle(struct mCore* core, Recorder* recorder, const Options* options)
{
    for (uint32_t iteration = 0u; iteration < 1000u; ++iteration)
    {
        int32_t cpsr = 0;
        int32_t programCounter = 0;
        if (!core->readRegister(core, "cpsr", &cpsr) || !core->readRegister(core, "pc", &programCounter))
            return false;

        const uint16_t enabledInterrupts = core->busRead16(core, GBA_REG_IE);
        if (core->busRead32(core, options->soundInfo) == MP2K_MAGIC && (cpsr & 0x3F) == 0x3F &&
            (uint32_t)programCounter >= 0x02000000u &&
            (enabledInterrupts & (GBA_IE_VBLANK | GBA_IE_VCOUNT)) == (GBA_IE_VBLANK | GBA_IE_VCOUNT) &&
            (core->busRead16(core, GBA_REG_IME) & 0x1u) != 0u)
        {
            return true;
        }
        if (!run_loop_and_drain(core, recorder))
            return false;
    }
    return false;
}

/* Steps an injected ROM call until it returns with the requested SoundInfo state. */
static bool
wait_for_runner_ident(struct mCore* core, Recorder* recorder, const Options* options, uint32_t expectedSoundIdent)
{
    for (uint32_t iteration = 0u; iteration < 1000u; ++iteration)
    {
        int32_t programCounter = 0;
        if (!core->readRegister(core, "pc", &programCounter))
            return false;
        if ((uint32_t)programCounter >= options->fixtureAddress + FIXTURE_RUNNER_OFFSET &&
            (uint32_t)programCounter < options->fixtureAddress + FIXTURE_RUNNER_OFFSET + 4u &&
            core->busRead32(core, options->soundInfo) == expectedSoundIdent)
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
            ", sound=0x%08" PRIX32 ", expected=0x%08" PRIX32 ")\n",
            (uint32_t)programCounter,
            (uint32_t)cpsr,
            core->busRead32(core, options->mplayInfo + 52u),
            core->busRead32(core, options->soundInfo),
            expectedSoundIdent);
    return false;
}

/* Waits for ordinary ROM calls, which must leave SoundInfo unlocked. */
static bool wait_for_runner(struct mCore* core, Recorder* recorder, const Options* options)
{
    return wait_for_runner_ident(core, recorder, options, MP2K_MAGIC);
}

/* Cross one frame with both MP2K callbacks locked, then stop in VBlank so each
 * fixed fixture has one stable begin phase. */
static bool align_driver_capture_after_vblank(struct mCore* core, Recorder* recorder, const Options* options)
{
    const uint32_t lockedIdent = MP2K_MAGIC + 10u;
    core->busWrite32(core, options->soundInfo, lockedIdent);
    bool aligned = run_frame_and_drain(core, recorder);
    bool reachedVBlank = false;
    for (uint32_t iteration = 0u; aligned && iteration < 1000u; ++iteration)
    {
        int32_t cpsr = 0;
        int32_t programCounter = 0;
        const uint16_t vcount = core->busRead16(core, GBA_REG_VCOUNT);
        if (!core->readRegister(core, "cpsr", &cpsr) || !core->readRegister(core, "pc", &programCounter))
        {
            aligned = false;
            break;
        }
        if (vcount >= 160u && (cpsr & 0x3F) == 0x3F &&
            (uint32_t)programCounter >= options->fixtureAddress + FIXTURE_RUNNER_OFFSET &&
            (uint32_t)programCounter < options->fixtureAddress + FIXTURE_RUNNER_OFFSET + 4u)
        {
            reachedVBlank = true;
            break;
        }
        aligned = run_loop_and_drain(core, recorder);
    }
    core->busWrite32(core, options->soundInfo, MP2K_MAGIC);
    return aligned && reachedVBlank;
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
        (nativeRequested && !recorder->nativeCapturing && !begin_native_capture(core, recorder, options)))
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
    if (argc == 3 && strcmp(argv[1], "--dump-driver-track") == 0)
        return dump_driver_fixture_track(argv[2]) ? 0 : 2;
    if (argc == 3 && strcmp(argv[1], "--dump-driver-span") == 0)
        return dump_driver_scenario_span(argv[2]) ? 0 : 2;
    if (argc == 3 && strcmp(argv[1], "--dump-driver-family") == 0)
        return dump_driver_family(argv[2]) ? 0 : 2;

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

    core->reset(core);
    if (options.driverScenario != DRIVER_SCENARIO_NONE)
    {
        if (!capture_driver_identity(core, &options, &recorder.driverIdentity))
        {
            fprintf(stderr, "Driver fixture identity validation failed\n");
            goto unload;
        }
        if (options.hasMute || (options.hasSolo && options.enabledChannels != recorder.driverIdentity.soloMask))
        {
            fprintf(stderr, "Fixed driver scenarios derive the solo mask from the resolved ToneData family\n");
            goto unload;
        }
        options.enabledChannels = recorder.driverIdentity.soloMask;
    }
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
    if (nativeRequested)
    {
        if (!prepare_native_capture(&recorder, &options))
        {
            fprintf(stderr, "Could not open native trace artifacts: %s\n", strerror(errno));
            goto unload;
        }
#if PORYAAAA_MGBA_NATIVE_CAPTURE_AVAILABLE
        GBAAudioSetObservationSink(&((struct GBA*)core->board)->audio, &recorder.observationSink);
#endif
        reset_observations(&recorder);
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
    {
        if (!inject_song(core, &options, &recorder.driverIdentity))
        {
            fprintf(stderr, "Unsupported driver fixture scenario\n");
            goto unload;
        }
    }
    else
        core->busWrite16(core, options.fixtureAddress + FIXTURE_RUNNER_OFFSET, 0xE7FEu);
    if (!stop_existing_audio(core, &recorder, &options))
    {
        fprintf(stderr, "Could not stop boot-time audio before the reference fixture\n");
        goto unload;
    }
    uint16_t pcmTimerControl = 0u;
    if (options.driverScenario != DRIVER_SCENARIO_NONE && !restart_pcm_dma(core, &recorder, &options, &pcmTimerControl))
    {
        fprintf(stderr, "Could not align MP2K DirectSound DMA before the reference fixture\n");
        goto unload;
    }
    if (options.driverScenario != DRIVER_SCENARIO_NONE && !align_driver_capture_after_vblank(core, &recorder, &options))
    {
        fprintf(stderr, "Could not align the lifecycle fixture after VBlank\n");
        goto unload;
    }
    if (options.driverScenario != DRIVER_SCENARIO_NONE)
    {
        if (!begin_native_capture(core, &recorder, &options))
        {
            fprintf(stderr, "Could not open the aligned lifecycle capture interval\n");
            goto unload;
        }
        core->busWrite16(core, GBA_REG_TM0CNT_H, pcmTimerControl);
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
