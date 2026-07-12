#include <dlfcn.h>
#include <inttypes.h>
#include <libretro.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
    SAMPLE_RATE = 32768,
    CHANNEL_COUNT = 2
};

typedef struct
{
    const char* corePath;
    const char* romPath;
    const char* outputPath;
    const char* manifestPath;
    const char* interpolation;
    const char* filtering;
    bool channels[6];
    double durationSeconds;
} Options;

typedef struct
{
    FILE* wav;
    uint64_t requestedFrames;
    uint64_t writtenFrames;
    uint64_t squareSum;
    uint16_t peak;
} Capture;

typedef struct
{
    void* handle;
    void (*setEnvironment)(retro_environment_t);
    void (*setVideoRefresh)(retro_video_refresh_t);
    void (*setAudioSample)(retro_audio_sample_t);
    void (*setAudioSampleBatch)(retro_audio_sample_batch_t);
    void (*setInputPoll)(retro_input_poll_t);
    void (*setInputState)(retro_input_state_t);
    void (*getSystemInfo)(struct retro_system_info*);
    void (*getSystemAvInfo)(struct retro_system_av_info*);
    void (*init)(void);
    void (*deinit)(void);
    bool (*loadGame)(const struct retro_game_info*);
    void (*unloadGame)(void);
    void (*run)(void);
} Core;

static Options gOptions;
static Capture gCapture;

/* Prints the intentionally small command-line surface of the recorder. */
static void usage(const char* program)
{
    fprintf(stderr,
            "Usage: %s --core FILE --rom FILE --output FILE [options]\n"
            "Options:\n"
            "  --duration-seconds S       Capture duration (default: 2)\n"
            "  --solo LIST                Keep named channels only\n"
            "  --mute LIST                Disable named channels\n"
            "  --interpolation VALUE      enabled or disabled (default: enabled)\n"
            "  --filtering N              0 through 10 (default: 5)\n"
            "  --manifest FILE            Write capture provenance\n",
            program);
}

/* Maps stable capture names to VBA-M's six libretro sound switches. */
static bool applyChannelList(const char* text, bool enabled, bool clearFirst)
{
    static const char* names[] = {"sq1", "sq2", "wave", "noise", "fifo-a", "fifo-b"};
    char* copy = strdup(text);
    char* token;
    char* state;
    bool found = false;
    if (!copy)
        return false;
    if (clearFirst)
        memset(gOptions.channels, 0, sizeof(gOptions.channels));
    for (token = strtok_r(copy, ",", &state); token; token = strtok_r(NULL, ",", &state))
    {
        found = false;
        for (size_t i = 0; i < 6; ++i)
        {
            if (strcmp(token, names[i]) == 0)
            {
                gOptions.channels[i] = enabled;
                found = true;
                break;
            }
        }
        if (!found)
        {
            fprintf(stderr, "Unknown channel: %s\n", token);
            free(copy);
            return false;
        }
    }
    free(copy);
    return found;
}

/* Parses and validates all settings before the emulator core is loaded. */
static bool parseOptions(int argc, char** argv)
{
    gOptions.durationSeconds = 2.0;
    gOptions.interpolation = "enabled";
    gOptions.filtering = "5";
    memset(gOptions.channels, 1, sizeof(gOptions.channels));
    for (int i = 1; i < argc; ++i)
    {
        if (i + 1 >= argc)
        {
            usage(argv[0]);
            return false;
        }
        const char* value = argv[++i];
        if (strcmp(argv[i - 1], "--core") == 0)
            gOptions.corePath = value;
        else if (strcmp(argv[i - 1], "--rom") == 0)
            gOptions.romPath = value;
        else if (strcmp(argv[i - 1], "--output") == 0)
            gOptions.outputPath = value;
        else if (strcmp(argv[i - 1], "--manifest") == 0)
            gOptions.manifestPath = value;
        else if (strcmp(argv[i - 1], "--duration-seconds") == 0)
            gOptions.durationSeconds = strtod(value, NULL);
        else if (strcmp(argv[i - 1], "--interpolation") == 0)
            gOptions.interpolation = value;
        else if (strcmp(argv[i - 1], "--filtering") == 0)
            gOptions.filtering = value;
        else if (strcmp(argv[i - 1], "--solo") == 0)
        {
            if (!applyChannelList(value, true, true))
                return false;
        }
        else if (strcmp(argv[i - 1], "--mute") == 0)
        {
            if (!applyChannelList(value, false, false))
                return false;
        }
        else
        {
            fprintf(stderr, "Unknown option: %s\n", argv[i - 1]);
            return false;
        }
    }
    if (!gOptions.corePath || !gOptions.romPath || !gOptions.outputPath || gOptions.durationSeconds <= 0.0)
    {
        usage(argv[0]);
        return false;
    }
    if (strcmp(gOptions.interpolation, "enabled") != 0 && strcmp(gOptions.interpolation, "disabled") != 0)
    {
        fprintf(stderr, "Interpolation must be enabled or disabled\n");
        return false;
    }
    char* end;
    long filtering = strtol(gOptions.filtering, &end, 10);
    if (*end || filtering < 0 || filtering > 10)
    {
        fprintf(stderr, "Filtering must be an integer from 0 through 10\n");
        return false;
    }
    return true;
}

/* Supplies only the frontend services and core options VBA-M needs headlessly. */
static bool environment(unsigned command, void* data)
{
    static const char* channelKeys[] = {
        "vbam_sound_1", "vbam_sound_2", "vbam_sound_3", "vbam_sound_4", "vbam_sound_5", "vbam_sound_6"};
    switch (command)
    {
    case RETRO_ENVIRONMENT_GET_CAN_DUPE:
        *(bool*)data = true;
        return true;
    case RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION:
        *(unsigned*)data = 2;
        return true;
    case RETRO_ENVIRONMENT_GET_LANGUAGE:
        *(unsigned*)data = RETRO_LANGUAGE_ENGLISH;
        return true;
    case RETRO_ENVIRONMENT_GET_VARIABLE:
    {
        struct retro_variable* variable = data;
        if (strcmp(variable->key, "vbam_soundinterpolation") == 0)
            variable->value = gOptions.interpolation;
        else if (strcmp(variable->key, "vbam_soundfiltering") == 0)
            variable->value = gOptions.filtering;
        else
        {
            variable->value = NULL;
            for (size_t i = 0; i < 6; ++i)
            {
                if (strcmp(variable->key, channelKeys[i]) == 0)
                {
                    variable->value = gOptions.channels[i] ? "enabled" : "disabled";
                    break;
                }
            }
        }
        return variable->value != NULL;
    }
    case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:
        *(bool*)data = false;
        return true;
    case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
    case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
        *(const char**)data = ".";
        return true;
    case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
    case RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME:
    case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:
    case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO:
    case RETRO_ENVIRONMENT_SET_MEMORY_MAPS:
    case RETRO_ENVIRONMENT_SET_SERIALIZATION_QUIRKS:
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2:
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2_INTL:
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY:
        return true;
    default:
        return false;
    }
}

/* Discards video while still acknowledging each emulated frame. */
static void videoRefresh(const void* data, unsigned width, unsigned height, size_t pitch)
{
    (void)data;
    (void)width;
    (void)height;
    (void)pitch;
}

/* Records the fallback single-frame audio callback if a core uses it. */
static void audioSample(int16_t left, int16_t right)
{
    int16_t frame[] = {left, right};
    if (gCapture.writtenFrames < gCapture.requestedFrames)
        fwrite(frame, sizeof(frame), 1, gCapture.wav);
    if (gCapture.writtenFrames < gCapture.requestedFrames)
        ++gCapture.writtenFrames;
}

/* Copies VBA-M's interleaved PCM16 stereo output into the capture WAV. */
static size_t audioBatch(const int16_t* data, size_t frames)
{
    size_t remaining = (size_t)(gCapture.requestedFrames - gCapture.writtenFrames);
    size_t accepted = frames < remaining ? frames : remaining;
    if (accepted)
        fwrite(data, sizeof(*data) * CHANNEL_COUNT, accepted, gCapture.wav);
    for (size_t i = 0; i < accepted * CHANNEL_COUNT; ++i)
    {
        int32_t sample = data[i];
        uint16_t magnitude = (uint16_t)(sample < 0 ? -sample : sample);
        if (magnitude > gCapture.peak)
            gCapture.peak = magnitude;
        gCapture.squareSum += (uint64_t)(sample * sample);
    }
    gCapture.writtenFrames += accepted;
    return frames;
}

/* Provides neutral controller state to the audio-only ROM. */
static void inputPoll(void) {}
static int16_t inputState(unsigned port, unsigned device, unsigned index, unsigned id)
{
    (void)port;
    (void)device;
    (void)index;
    (void)id;
    return 0;
}

/* Loads the required libretro entry points from a VBA-M core dylib. */
static bool loadCore(Core* core)
{
    core->handle = dlopen(gOptions.corePath, RTLD_NOW | RTLD_LOCAL);
    if (!core->handle)
    {
        fprintf(stderr, "Could not load VBA-M core: %s\n", dlerror());
        return false;
    }
#define LOAD(name, field)                                                                                              \
    do                                                                                                                 \
    {                                                                                                                  \
        *(void**)(&core->field) = dlsym(core->handle, name);                                                           \
        if (!core->field)                                                                                              \
        {                                                                                                              \
            fprintf(stderr, "Missing core symbol: %s\n", name);                                                        \
            return false;                                                                                              \
        }                                                                                                              \
    } while (0)
    LOAD("retro_set_environment", setEnvironment);
    LOAD("retro_set_video_refresh", setVideoRefresh);
    LOAD("retro_set_audio_sample", setAudioSample);
    LOAD("retro_set_audio_sample_batch", setAudioSampleBatch);
    LOAD("retro_set_input_poll", setInputPoll);
    LOAD("retro_set_input_state", setInputState);
    LOAD("retro_get_system_info", getSystemInfo);
    LOAD("retro_get_system_av_info", getSystemAvInfo);
    LOAD("retro_init", init);
    LOAD("retro_deinit", deinit);
    LOAD("retro_load_game", loadGame);
    LOAD("retro_unload_game", unloadGame);
    LOAD("retro_run", run);
#undef LOAD
    return true;
}

/* Reads a ROM into the memory-backed form requested by VBA-M's libretro core. */
static void* readFile(const char* path, size_t* size)
{
    FILE* file = fopen(path, "rb");
    if (!file || fseek(file, 0, SEEK_END) != 0)
        return NULL;
    long length = ftell(file);
    rewind(file);
    void* data = length > 0 ? malloc((size_t)length) : NULL;
    if (!data || fread(data, 1, (size_t)length, file) != (size_t)length)
    {
        free(data);
        data = NULL;
    }
    fclose(file);
    *size = data ? (size_t)length : 0;
    return data;
}

/* Writes or patches the canonical 44-byte little-endian PCM WAV header. */
static void writeWavHeader(FILE* file, uint32_t frames)
{
    uint32_t dataBytes = frames * CHANNEL_COUNT * sizeof(int16_t);
    uint32_t riffBytes = dataBytes + 36;
    uint32_t byteRate = SAMPLE_RATE * CHANNEL_COUNT * sizeof(int16_t);
    uint16_t format = 1, channels = CHANNEL_COUNT, bits = 16;
    uint32_t rate = SAMPLE_RATE;
    uint16_t blockAlign = CHANNEL_COUNT * sizeof(int16_t);
    rewind(file);
    fwrite("RIFF", 1, 4, file);
    fwrite(&riffBytes, 4, 1, file);
    fwrite("WAVEfmt ", 1, 8, file);
    uint32_t fmtSize = 16;
    fwrite(&fmtSize, 4, 1, file);
    fwrite(&format, 2, 1, file);
    fwrite(&channels, 2, 1, file);
    fwrite(&rate, 4, 1, file);
    fwrite(&byteRate, 4, 1, file);
    fwrite(&blockAlign, 2, 1, file);
    fwrite(&bits, 2, 1, file);
    fwrite("data", 1, 4, file);
    fwrite(&dataBytes, 4, 1, file);
}

/* Captures enough emulated frames to satisfy the exact requested audio length. */
static bool capture(Core* core, const struct retro_system_info* systemInfo)
{
    size_t romSize;
    void* rom = readFile(gOptions.romPath, &romSize);
    if (!rom)
    {
        fprintf(stderr, "Could not read ROM: %s\n", gOptions.romPath);
        return false;
    }
    gCapture.requestedFrames = (uint64_t)(gOptions.durationSeconds * SAMPLE_RATE + 0.5);
    gCapture.wav = fopen(gOptions.outputPath, "wb+");
    if (!gCapture.wav)
    {
        free(rom);
        return false;
    }
    writeWavHeader(gCapture.wav, 0);
    struct retro_game_info game = {gOptions.romPath, rom, romSize, NULL};
    if (!core->loadGame(&game))
    {
        fprintf(stderr, "VBA-M rejected ROM: %s\n", gOptions.romPath);
        fclose(gCapture.wav);
        free(rom);
        return false;
    }
    struct retro_system_av_info avInfo;
    core->getSystemAvInfo(&avInfo);
    if (avInfo.timing.sample_rate != SAMPLE_RATE)
    {
        fprintf(stderr, "Unexpected VBA-M sample rate: %.3f\n", avInfo.timing.sample_rate);
        core->unloadGame();
        fclose(gCapture.wav);
        free(rom);
        return false;
    }
    while (gCapture.writtenFrames < gCapture.requestedFrames)
        core->run();
    core->unloadGame();
    writeWavHeader(gCapture.wav, (uint32_t)gCapture.writtenFrames);
    fclose(gCapture.wav);
    free(rom);
    if (!gCapture.peak)
    {
        fprintf(stderr, "VBA-M produced a silent capture\n");
        return false;
    }
    if (gOptions.manifestPath)
    {
        FILE* manifest = fopen(gOptions.manifestPath, "w");
        if (!manifest)
            return false;
        fprintf(manifest,
                "manifest_version=1\ncore_name=%s\ncore_version=%s\ncore_path=%s\nrom_path=%s\n",
                systemInfo->library_name,
                systemInfo->library_version,
                gOptions.corePath,
                gOptions.romPath);
        fprintf(manifest,
                "sample_rate_hz=%d\nchannels=2\nsample_format=pcm_s16le\nduration_frames=%" PRIu64 "\n",
                SAMPLE_RATE,
                gCapture.writtenFrames);
        fprintf(manifest, "interpolation=%s\nfiltering=%s\n", gOptions.interpolation, gOptions.filtering);
        static const char* names[] = {"sq1", "sq2", "wave", "noise", "fifo-a", "fifo-b"};
        for (size_t i = 0; i < 6; ++i)
            fprintf(manifest, "channel.%s=%s\n", names[i], gOptions.channels[i] ? "enabled" : "disabled");
        fclose(manifest);
    }
    return true;
}

int main(int argc, char** argv)
{
    Core core = {0};
    if (!parseOptions(argc, argv) || !loadCore(&core))
        return 2;
    core.setEnvironment(environment);
    core.setVideoRefresh(videoRefresh);
    core.setAudioSample(audioSample);
    core.setAudioSampleBatch(audioBatch);
    core.setInputPoll(inputPoll);
    core.setInputState(inputState);
    core.init();
    struct retro_system_info systemInfo;
    core.getSystemInfo(&systemInfo);
    bool ok = capture(&core, &systemInfo);
    if (ok)
    {
        printf("VBA-M %s: wrote %" PRIu64 " stereo frames at %d Hz to %s (peak %.6f)\n",
               systemInfo.library_version,
               gCapture.writtenFrames,
               SAMPLE_RATE,
               gOptions.outputPath,
               (double)gCapture.peak / 32768.0);
    }
    core.deinit();
    dlclose(core.handle);
    if (!ok)
        return 1;
    return 0;
}
