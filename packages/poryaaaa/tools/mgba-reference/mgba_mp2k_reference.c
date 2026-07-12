#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__clang__)
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wextra-semi"
#endif
#include <mgba/flags.h>
#include <mgba/core/blip_buf.h>
#include <mgba/core/core.h>
#include <mgba/core/interface.h>
#include <mgba/core/log.h>
#include <mgba/core/version.h>
#include <mgba/gba/core.h>
#if defined(__clang__)
#    pragma clang diagnostic pop
#endif

#define MP2K_MAGIC 0x68736D53u
#define GBA_FRAMES_PER_SECOND 60u
#define REFERENCE_OUTPUT_RATE 65536u
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

#define FIXTURE_VOICE_OFFSET 0x00u
#define FIXTURE_HEADER_OFFSET 0x20u
#define FIXTURE_TRACK_OFFSET 0x40u
#define FIXTURE_RUNNER_OFFSET 0x80u

typedef struct Options
{
    const char* romPath;
    const char* outputPath;
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
} Options;

typedef struct Recorder
{
    struct mAVStream stream;
    FILE* output;
    uint64_t framesWritten;
    uint64_t targetFrames;
    double sumSquares;
    int peak;
    unsigned sampleRate;
    bool capturing;
    bool writeFailed;
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
            "Options:\n"
            "  --duration-seconds S       Capture length (default: 2)\n"
            "  --boot-timeout-seconds S   MP2K initialization timeout (default: 10)\n"
            "  --note N                   MIDI note 0-127 (default: 60)\n"
            "  --velocity N               MP2K velocity 0-127 (default: 127)\n"
            "  --volume N                 Track volume 0-127 (default: 127)\n"
            "  --pan N                    MP2K pan 0-127 (default: 64)\n"
            "  --solo LIST                Keep only named hardware channels\n"
            "  --mute LIST                Disable named hardware channels\n"
            "                             Names: sq1,sq2,wave,noise,fifo-a,fifo-b,\n"
            "                             psg,directsound,all (comma-separated)\n"
            "  --mplay-all-stop ADDRESS    Stop existing players before a voice fixture\n"
            "  --require-max-chans N       Fail unless MP2K configures this PCM channel count\n"
            "  --fixture-address ADDRESS  EWRAM scratch base (default: 0x0203F000)\n",
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
    return options->romPath != NULL && options->outputPath != NULL && options->mplayInfo != 0u &&
           options->soundInfo != 0u && (voiceMode || songMode || bootSongMode);
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

/* Drains the same band-limited buffers consumed by mGBA audio frontends. */
static void post_audio_buffer(struct mAVStream* stream, struct blip_t* left, struct blip_t* right)
{
    Recorder* recorder = (Recorder*)stream;
    if (!recorder->capturing)
    {
        blip_clear(left);
        blip_clear(right);
        return;
    }

    int16_t samples[AUDIO_BUFFER_FRAMES * 2u];
    while (recorder->framesWritten < recorder->targetFrames && !recorder->writeFailed)
    {
        int available = blip_samples_avail(left);
        int rightAvailable = blip_samples_avail(right);
        if (rightAvailable < available)
            available = rightAvailable;
        if (available <= 0)
            break;

        uint64_t remaining = recorder->targetFrames - recorder->framesWritten;
        int frames = available;
        if (frames > (int)AUDIO_BUFFER_FRAMES)
            frames = (int)AUDIO_BUFFER_FRAMES;
        if ((uint64_t)frames > remaining)
            frames = (int)remaining;

        int leftRead = blip_read_samples(left, samples, frames, true);
        int rightRead = blip_read_samples(right, samples + 1, frames, true);
        if (leftRead != frames || rightRead != frames)
        {
            recorder->writeFailed = true;
            break;
        }
        for (int i = 0; i < frames; i++)
            write_audio_frame(recorder, samples[i * 2], samples[i * 2 + 1]);
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
        return core->writeRegister(core, "r0", &songId) && core->writeRegister(core, "lr", &runnerAddress) &&
               core->writeRegister(core, "pc", &startAddress);
    }

    int32_t mplayInfo = (int32_t)options->mplayInfo;
    int32_t headerAddress = (int32_t)(options->fixtureAddress + FIXTURE_HEADER_OFFSET);
    int32_t startAddress = (int32_t)(options->mplayStart & ~1u);
    return core->writeRegister(core, "r0", &mplayInfo) && core->writeRegister(core, "r1", &headerAddress) &&
           core->writeRegister(core, "lr", &runnerAddress) && core->writeRegister(core, "pc", &startAddress);
}

static bool wait_for_runner(struct mCore* core, const Options* options);

/* Stops boot-time players so a hardware FIFO capture contains only the injected voice. */
static bool stop_existing_audio(struct mCore* core, const Options* options)
{
    if (options->mplayAllStop == 0u)
        return true;

    int32_t runnerAddress = (int32_t)(options->fixtureAddress + FIXTURE_RUNNER_OFFSET + 1u);
    int32_t stopAddress = (int32_t)(options->mplayAllStop & ~1u);
    if (!core->writeRegister(core, "lr", &runnerAddress) || !core->writeRegister(core, "pc", &stopAddress) ||
        !wait_for_runner(core, options))
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
static bool wait_for_mp2k(struct mCore* core, const Options* options)
{
    uint64_t maxFrames = (uint64_t)(options->bootTimeoutSeconds * GBA_FRAMES_PER_SECOND + 0.5);
    for (uint64_t frame = 0u; frame < maxFrames; ++frame)
    {
        core->runFrame(core);
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
static bool wait_for_sound_main_idle(struct mCore* core, const Options* options)
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
        core->runLoop(core);
    }
    return false;
}

/* Steps an injected ROM call until it returns to the EWRAM runner. */
static bool wait_for_runner(struct mCore* core, const Options* options)
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
        core->runLoop(core);
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

/* Opens an mGBA frontend-rate WAV before an emulator capture begins. */
static bool begin_capture(Recorder* recorder, const Options* options)
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

/* Finalizes a capture only when every requested frontend frame was written. */
static bool finish_capture(Recorder* recorder)
{
    recorder->capturing = false;
    if (recorder->writeFailed || recorder->framesWritten != recorder->targetFrames)
    {
        return false;
    }

    return write_wav_header(recorder->output, recorder->sampleRate, recorder->framesWritten) &&
           fflush(recorder->output) == 0;
}

/* Runs the complete emulator until the requested frontend audio frame count is captured. */
static bool capture_reference(struct mCore* core, Recorder* recorder, const Options* options)
{
    if (!begin_capture(recorder, options))
        return false;

    while (recorder->framesWritten < recorder->targetFrames && !recorder->writeFailed)
    {
        core->runFrame(core);
        if (!wait_for_sound_main_idle(core, options))
            return false;
    }
    return finish_capture(recorder);
}

/* Arms during the ROM's silent preroll and proves that its configured song advanced. */
static bool capture_boot_song(struct mCore* core, Recorder* recorder, const Options* options)
{
    core->runFrame(core);
    if (!begin_capture(recorder, options))
        return false;

    bool sawSoundDriver = false;
    bool sawExpectedSong = false;
    bool trackAdvanced = false;
    bool maxChansValidated = !options->hasRequiredMaxChans;
    uint32_t initialTrackAddress = core->busRead32(core, options->songAddress + 8u);
    while (recorder->framesWritten < recorder->targetFrames && !recorder->writeFailed)
    {
        core->runFrame(core);
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
    }

    if (!finish_capture(recorder))
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

    Recorder recorder = {
        .sampleRate = REFERENCE_OUTPUT_RATE,
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
    mStandardLoggerInit(&logger);
    mStandardLoggerConfig(&logger, &core->config);
    mLogSetDefaultLogger(&logger.d);
    core->setAVStream(core, &recorder.stream);
    if (!mCoreLoadFile(core, options.romPath))
    {
        fprintf(stderr, "Could not load ROM: %s\n", options.romPath);
        goto cleanup;
    }

    core->reset(core);
    blip_set_rates(core->getAudioChannel(core, 0), core->frequency(core), recorder.sampleRate);
    blip_set_rates(core->getAudioChannel(core, 1), core->frequency(core), recorder.sampleRate);
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

    if (!wait_for_mp2k(core, &options))
    {
        fprintf(stderr,
                "MP2K did not initialize at 0x%08" PRIX32 " within %.2f seconds\n",
                options.mplayInfo,
                options.bootTimeoutSeconds);
        goto unload;
    }
    if (!wait_for_sound_main_idle(core, &options))
    {
        fprintf(stderr, "The ROM's SoundMain did not return to its idle state\n");
        goto unload;
    }

    if (options.voiceAddress != 0u)
        inject_song(core, &options);
    else
        core->busWrite16(core, options.fixtureAddress + FIXTURE_RUNNER_OFFSET, 0xE7FEu);
    if (!stop_existing_audio(core, &options))
    {
        fprintf(stderr, "Could not stop boot-time audio before the reference fixture\n");
        goto unload;
    }
    if (!start_fixture(core, &options))
    {
        fprintf(stderr, "Could not transfer emulated CPU control to the ROM sound driver\n");
        goto unload;
    }
    if (!wait_for_runner(core, &options))
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
    if (recorder.peak == 0)
    {
        fprintf(stderr, "mGBA produced a silent capture for song header 0x%08" PRIX32 "\n", expectedHeader);
        goto unload;
    }

    double rms = sqrt(recorder.sumSquares / (double)(recorder.framesWritten * 2u)) / 32768.0;
    printf("mGBA %s: wrote %" PRIu64 " stereo frames at %u Hz to %s (channels 0x%02" PRIX32 ", peak %.6f, RMS %.6f)\n",
           projectVersion,
           recorder.framesWritten,
           recorder.sampleRate,
           options.outputPath,
           options.enabledChannels,
           (double)recorder.peak / 32768.0,
           rms);
    result = 0;

unload:
    core->unloadROM(core);
cleanup:
    if (recorder.output != NULL && fclose(recorder.output) != 0)
        result = 1;
    mCoreConfigDeinit(&core->config);
    core->deinit(core);
    mLogSetDefaultLogger(NULL);
    mStandardLoggerDeinit(&logger);
    return result;
}
