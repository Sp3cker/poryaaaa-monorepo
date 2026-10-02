#include "test_assert.h"
#include "voicegroup_loader.h"

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* Distinct PCM and BIN loop metadata make choosing the wrong format observable. */
static const uint8_t wavBytes[] = {
    'R',  'I',  'F', 'F', 40,   0,    0, 0, 'W', 'A', 'V', 'E', 'f', 'm', 't', ' ', 16, 0, 0, 0, 1,   0,   1,   0,
    0x22, 0x56, 0,   0,   0x22, 0x56, 0, 0, 1,   0,   8,   0,   'd', 'a', 't', 'a', 4,  0, 0, 0, 139, 140, 141, 142,
};
/* Five AIFF frames include the trailing guard frame, leaving four PCM samples. */
static const uint8_t aifBytes[] = {
    'F', 'O', 'R', 'M', 0, 0,  0, 52, 'A',  'I',  'F',  'F',  'C', 'O', 'M', 'M', 0,  0,  0,   18,
    0,   1,   0,   0,   0, 5,  0, 8,  0x40, 0x0d, 0xac, 0x44, 0,   0,   0,   0,   0,  0,  'S', 'S',
    'N', 'D', 0,   0,   0, 13, 0, 0,  0,    0,    0,    0,    0,   0,   22,  23,  24, 25, 25,  0,
};
static const uint8_t binBytes[] = {0, 0, 0, 0x40, 0, 0, 0, 1, 1, 0, 0, 0, 4, 0, 0, 0, 33, 34, 35, 36};
static const uint8_t progBytes[16] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef, 0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10};
static const uint8_t malformed[] = {0};

static const struct
{
    const char* path;
    const uint8_t* data;
    size_t size;
} assets[] = {
    {"good.wav", wavBytes, sizeof(wavBytes)},
    {"good.aif", aifBytes, sizeof(aifBytes)},
    {"good.bin", binBytes, sizeof(binBytes)},
    {"aiff_invalid.wav", malformed, sizeof(malformed)},
    {"aiff_invalid.aif", aifBytes, sizeof(aifBytes)},
    {"aiff_invalid.bin", binBytes, sizeof(binBytes)},
    {"aiff_missing.wav", NULL, 0},
    {"aiff_missing.aif", aifBytes, sizeof(aifBytes)},
    {"aiff_missing.bin", binBytes, sizeof(binBytes)},
    {"bin_invalid.wav", NULL, 0},
    {"bin_invalid.aif", malformed, sizeof(malformed)},
    {"bin_invalid.bin", binBytes, sizeof(binBytes)},
    {"bin_missing.wav", malformed, sizeof(malformed)},
    {"bin_missing.aif", NULL, 0},
    {"bin_missing.bin", binBytes, sizeof(binBytes)},
    {"absent.wav", NULL, 0},
    {"absent.aif", NULL, 0},
    {"absent.bin", NULL, 0},
    {"wave.pcm", progBytes, sizeof(progBytes)},
};

enum
{
    ASSET_COUNT = sizeof(assets) / sizeof(assets[0])
};
typedef struct
{
    const char* root;
    int reads[ASSET_COUNT];
    int populated;
    int released;
    int failedBatchPopulated;
    const char* failPath;
} TestReader;

/* Assembly uses real sibling stems; only encoded asset transport is substituted. */
static bool read_batch(
    void* user, const char* const* paths, size_t count, VoicegroupFileBlob* out, char* error, size_t errorCapacity)
{
    TestReader* reader = user;
    size_t rootLength = strlen(reader->root);
    int populated = 0;
    for (size_t i = 0; i < count; i++)
    {
        size_t asset = 0;
        if (strncmp(paths[i], reader->root, rootLength) == 0 && paths[i][rootLength] == '/')
        {
            while (asset < ASSET_COUNT && strcmp(paths[i] + rootLength + 1, assets[asset].path) != 0)
                asset++;
        }
        else
            asset = ASSET_COUNT;
        if (asset == ASSET_COUNT)
        {
            if (error && errorCapacity)
                snprintf(error, errorCapacity, "unexpected asset path: %s", paths[i]);
            return false;
        }
        reader->reads[asset]++;
        if (assets[asset].data)
        {
            uint8_t* data = malloc(assets[asset].size);
            if (!data)
                return false;
            memcpy(data, assets[asset].data, assets[asset].size);
            out[i] = (VoicegroupFileBlob){data, assets[asset].size, true};
            reader->populated++;
            populated++;
        }
        if (reader->failPath && strcmp(assets[asset].path, reader->failPath) == 0)
        {
            reader->failedBatchPopulated = populated;
            if (error && errorCapacity)
                snprintf(error, errorCapacity, "injected transport failure");
            return false;
        }
    }
    return true;
}

/* Failed batches retain ownership of their populated blobs until this callback. */
static void release_batch(void* user, VoicegroupFileBlob* blobs, size_t count)
{
    TestReader* reader = user;
    for (size_t i = 0; i < count; i++)
    {
        if (blobs[i].data)
        {
            free(blobs[i].data);
            reader->released++;
        }
        blobs[i] = (VoicegroupFileBlob){0};
    }
}

/* Path names, rather than batch ordering or asset-table ordinals, identify reads. */
static int reads_for(const TestReader* reader, const char* path)
{
    for (size_t i = 0; i < ASSET_COUNT; i++)
        if (strcmp(assets[i].path, path) == 0)
            return reader->reads[i];
    ASSERT(false, "read assertion names a known fixture path");
    return -1;
}

/* Keep discovery and parsing real while making the fixture independent of external projects. */
static bool write_fixture(const char* path, const char* text)
{
    FILE* file = fopen(path, "wb");
    if (!file)
        return false;
    size_t size = strlen(text);
    bool ok = fwrite(text, 1, size, file) == size;
    return fclose(file) == 0 && ok;
}

/* Expected PCM and GBA header words come from the encoded fixture, not another loader. */
static void
check_wave(const WaveData* wave, const int8_t expected[4], uint32_t freq, uint16_t status, uint32_t loopStart)
{
    ASSERT(wave != NULL, "sample resolves after project teardown");
    if (!wave)
        return;
    ASSERT(wave->size == 4 && wave->data && memcmp(wave->data, expected, 4) == 0, "expected decoded PCM");
    ASSERT(wave->type == 0 && wave->freq == freq && wave->status == status && wave->loopStart == loopStart,
           "expected sample frequency and loop metadata");
}

/* One mixed bank covers malformed files, soft misses, aliases, and a missing sample. */
static void check_bank(const LoadedVoiceGroup* bank, bool mixed)
{
    ASSERT(bank != NULL, "public project load returns a complete bank");
    if (!bank)
        return;
    const int8_t wavPcm[4] = {11, 12, 13, 14};
    const int8_t aifPcm[4] = {22, 23, 24, 25};
    const int8_t binPcm[4] = {33, 34, 35, 36};
    check_wave(bank->voices[0].wav, wavPcm, 22050u * 1024u, 0, 0);
    if (!mixed)
    {
        ASSERT(bank->waveDataCount == 1, "WAV-only bank owns one sample");
        return;
    }
    check_wave(bank->voices[1].wav, aifPcm, 22050u * 1024u, 0, 0);
    check_wave(bank->voices[2].wav, binPcm, 16384u * 1024u, 0x4000, 1);
    check_wave(bank->voices[3].wav, aifPcm, 22050u * 1024u, 0, 0);
    check_wave(bank->voices[4].wav, binPcm, 16384u * 1024u, 0x4000, 1);
    ASSERT(bank->voices[5].wav == bank->voices[0].wav, "duplicate symbols share the WAV allocation");
    ASSERT(bank->voices[6].wav == bank->voices[1].wav, "distinct symbols sharing a path share the AIFF allocation");
    ASSERT(bank->voices[7].wav == NULL, "absent sample remains unresolved without rejecting the bank");
    ASSERT(bank->voices[8].type == VOICE_PROGRAMMABLE_WAVE && bank->voices[8].wavePointer &&
               memcmp(bank->voices[8].wavePointer, progBytes, sizeof(progBytes)) == 0,
           "programmable wave bytes remain valid after project teardown");
    ASSERT(bank->waveDataCount == 5 && bank->progWaveCount == 1, "bank owns only unique resolved sample paths");
}

/* Three public loads prove skipped fallback reads, mixed resolution, and transactional retry. */
void test_native_fallbacks(void)
{
    char root[] = "/tmp/poryaaaa-native-fallbacks-XXXXXX";
    bool madeRoot = mkdtemp(root) != NULL;
    ASSERT(madeRoot, "create temporary fallback project");
    if (!madeRoot)
        return;
    char sound[VG_MAX_PATH_LEN], dsMap[VG_MAX_PATH_LEN], pwMap[VG_MAX_PATH_LEN], bankPath[VG_MAX_PATH_LEN];
    snprintf(sound, sizeof(sound), "%s/sound", root);
    snprintf(dsMap, sizeof(dsMap), "%s/sound/direct_sound_data.inc", root);
    snprintf(pwMap, sizeof(pwMap), "%s/sound/programmable_wave_data.inc", root);
    snprintf(bankPath, sizeof(bankPath), "%s/sound/voice_groups.inc", root);
    bool ready = mkdir(sound, 0700) == 0;
    ASSERT(ready, "create sound directory");
    if (ready)
    {
        ready = write_fixture(dsMap,
                              "Good::\n\t.incbin \"good.bin\"\n"
                              "AiffInvalid::\n\t.incbin \"aiff_invalid.bin\"\n"
                              "AiffAlias::\n\t.incbin \"aiff_invalid.bin\"\n"
                              "AiffMissing::\n\t.incbin \"aiff_missing.bin\"\n"
                              "BinInvalid::\n\t.incbin \"bin_invalid.bin\"\n"
                              "BinMissing::\n\t.incbin \"bin_missing.bin\"\n"
                              "Absent::\n\t.incbin \"absent.bin\"\n") &&
                write_fixture(pwMap, "PulseWave::\n\t.incbin \"wave.pcm\"\n");
        ASSERT(ready, "write assembly symbol maps");
    }
    const char* allWav = "voice_group allwav\n\tvoice_directsound 60, 0, Good, 255, 252, 0, 115\n";
    const char* mixed = "voice_group mixed\n"
                        "\tvoice_directsound 60, 0, Good, 255, 252, 0, 115\n"
                        "\tvoice_directsound 60, 0, AiffInvalid, 255, 252, 0, 115\n"
                        "\tvoice_directsound 60, 0, BinInvalid, 255, 252, 0, 115\n"
                        "\tvoice_directsound 60, 0, AiffMissing, 255, 252, 0, 115\n"
                        "\tvoice_directsound 60, 0, BinMissing, 255, 252, 0, 115\n"
                        "\tvoice_directsound 60, 0, Good, 255, 252, 0, 115\n"
                        "\tvoice_directsound 60, 0, AiffAlias, 255, 252, 0, 115\n"
                        "\tvoice_directsound 60, 0, Absent, 255, 252, 0, 115\n"
                        "\tvoice_programmable_wave 60, 0, PulseWave, 5, 2, 15, 3\n";
    enum
    {
        ALL_WAV,
        MIXED,
        TRANSPORT_FAILURE
    };
    for (int scenario = ALL_WAV; ready && scenario <= TRANSPORT_FAILURE; scenario++)
    {
        printf("Testing native fallback: %s...\n",
               scenario == ALL_WAV ? "all WAV"
               : scenario == MIXED ? "mixed and shared"
                                   : "transport failure and retry");
        bool written = write_fixture(bankPath, scenario == ALL_WAV ? allWav : mixed);
        ASSERT(written, "write real voicegroup assembly");
        if (!written)
            break;
        TestReader reader = {.root = root,
                             .failPath = scenario == ALL_WAV             ? "good.aif"
                                         : scenario == TRANSPORT_FAILURE ? "bin_missing.bin"
                                                                         : NULL};
        VoicegroupFileIo io = {&reader, read_batch, release_batch};
        VoicegroupProject* project = voicegroup_project_open(root, NULL, &io);
        ASSERT(project != NULL, "open project through public API");
        if (!project)
            continue;
        VoicegroupTarget target = {bankPath, NULL};
        LoadedVoiceGroup* bank = voicegroup_project_load(project, &target);
        ASSERT(reader.populated == reader.released, "release every populated blob on success or failure");
        if (scenario == TRANSPORT_FAILURE)
        {
            ASSERT(bank == NULL, "required BIN transport failure publishes no partial bank");
            ASSERT(reader.failedBatchPopulated > 0 && reads_for(&reader, "bin_missing.bin") == 1,
                   "inject failure in a required lower-priority batch with populated blobs");
            ASSERT(reads_for(&reader, "wave.pcm") == 0, "transport failure stops later asset rounds");
            voicegroup_free(bank);
            reader = (TestReader){.root = root};
            bank = voicegroup_project_load(project, &target);
            ASSERT(reader.populated == reader.released, "retry releases every populated blob");
        }
        voicegroup_project_free(project);
        check_bank(bank, scenario != ALL_WAV);
        ASSERT(reads_for(&reader, "good.wav") == 1, "read shared WAV once");
        ASSERT(reads_for(&reader, "good.aif") == 0 && reads_for(&reader, "good.bin") == 0,
               "do not request unused WAV fallbacks, even when their transport would fail");
        if (scenario != ALL_WAV)
        {
            ASSERT(reads_for(&reader, "aiff_invalid.wav") == 1 && reads_for(&reader, "aiff_invalid.aif") == 1 &&
                       reads_for(&reader, "aiff_invalid.bin") == 0,
                   "deduplicate malformed WAV and shared successful AIFF; skip its BIN");
            ASSERT(reads_for(&reader, "aiff_missing.wav") == 1 && reads_for(&reader, "aiff_missing.aif") == 1 &&
                       reads_for(&reader, "aiff_missing.bin") == 0,
                   "missing WAV selects AIFF without reading BIN");
            ASSERT(reads_for(&reader, "bin_invalid.bin") == 1 && reads_for(&reader, "bin_missing.bin") == 1,
                   "invalid and missing AIFF select their required BIN paths");
            ASSERT(reads_for(&reader, "absent.wav") == 1 && reads_for(&reader, "absent.aif") == 1 &&
                       reads_for(&reader, "absent.bin") == 1 && reads_for(&reader, "wave.pcm") == 1,
                   "soft misses exhaust fallbacks and programmable wave is read once");
        }
        voicegroup_free(bank);
    }
    remove(bankPath);
    remove(dsMap);
    remove(pwMap);
    rmdir(sound);
    rmdir(root);
}
