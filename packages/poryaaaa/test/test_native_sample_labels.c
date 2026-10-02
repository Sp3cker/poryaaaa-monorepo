#include "test_assert.h"
#include "voicegroup_loader.h"

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* Write real assembly and sample assets so checks exercise the public loader. */
static bool write_fixture(const char* path, const void* data, size_t size)
{
    FILE* file = fopen(path, "wb");
    if (!file)
        return false;
    bool ok = fwrite(data, 1, size, file) == size;
    return fclose(file) == 0 && ok;
}

/* Both map parsers must preserve label grammar, decoded bytes, and length errors. */
int test_native_sample_labels(void)
{
    char root[] = "/tmp/poryaaaa-sample-labels-XXXXXX";
    if (!mkdtemp(root))
        return 1;
    char sound[512], dsMap[512], pwMap[512], sample[512], wave[512];
    snprintf(sound, sizeof(sound), "%s/sound", root);
    snprintf(dsMap, sizeof(dsMap), "%s/sound/direct_sound_data.inc", root);
    snprintf(pwMap, sizeof(pwMap), "%s/sound/programmable_wave_data.inc", root);
    snprintf(sample, sizeof(sample), "%s/sample.bin", root);
    snprintf(wave, sizeof(wave), "%s/wave.pcm", root);
    if (mkdir(sound, 0700) != 0)
    {
        rmdir(root);
        return 1;
    }
    const unsigned char sampleBytes[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 16, 32, 48};
    const unsigned char waveBytes[] = {
        0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef, 0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10};
    ASSERT(write_fixture(sample, sampleBytes, sizeof(sampleBytes)), "write DirectSound fixture");
    ASSERT(write_fixture(wave, waveBytes, sizeof(waveBytes)), "write programmable-wave fixture");

    char longest[VG_MAX_SYMBOL_LEN], oversized[VG_MAX_SYMBOL_LEN + 1];
    memset(longest, 'a', sizeof(longest) - 1);
    longest[sizeof(longest) - 1] = '\0';
    memset(oversized, 'a', sizeof(oversized) - 1);
    oversized[sizeof(oversized) - 1] = '\0';
    const struct
    {
        const char* name;
        const char* suffix;
        bool resolves;
        bool rejectsProject;
    } cases[] = {
        {"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_", ":", true, false},
        {"_", "::", true, false},
        {"9", ":", true, false},
        {"Mixed_09", ":: @ comment", true, false},
        {longest, ":", true, false},
        {oversized, "::", false, true},
        {oversized, "", false, false},
        {"", ":", false, false},
        {"NoColon", "", false, false},
        {"Space", " :", false, false},
        {"Bad-Name", ":", false, false},
        {"Bad.Name", ":", false, false},
        {"Bad/Name", ":", false, false},
        {"Bad[Name", ":", false, false},
        {"Bad`Name", ":", false, false},
        {"Bad{Name", ":", false, false},
        {"Bad\x80Name", ":", false, false},
        {"Bad\xffName", ":", false, false},
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        printf("Sample label case %zu\n", i);
        char text[1024];
        snprintf(text, sizeof(text), "\t%s%s\n\t.incbin \"sample.bin\"\n", cases[i].name, cases[i].suffix);
        ASSERT(write_fixture(dsMap, text, strlen(text)), "write DirectSound symbol map");
        /* Test each parser's error path independently, not just the first map. */
        for (int programmable = 0; programmable < 2; programmable++)
        {
            if (programmable)
            {
                ASSERT(write_fixture(dsMap, "", 0), "clear DirectSound map");
                snprintf(text, sizeof(text), "\t%s%s\n\t.incbin \"wave.pcm\"\n", cases[i].name, cases[i].suffix);
            }
            ASSERT(write_fixture(pwMap, programmable ? text : "", programmable ? strlen(text) : 0),
                   "write programmable-wave symbol map");
            const char* symbol = cases[i].name;
            LoadedSampleSet* set = voicegroup_load_samples(root,
                                                           programmable ? NULL : &symbol,
                                                           programmable ? 0 : 1,
                                                           programmable ? &symbol : NULL,
                                                           programmable ? 1 : 0,
                                                           NULL,
                                                           NULL,
                                                           0,
                                                           NULL);
            if (cases[i].rejectsProject)
                ASSERT(set == NULL, "overlong label rejects project rather than truncating");
            else
            {
                ASSERT(set != NULL, "valid map or non-label line permits project open");
                if (set)
                {
                    if (programmable)
                    {
                        ASSERT((set->progWaves[0] != NULL) == cases[i].resolves, "wave label resolution");
                        if (set->progWaves[0])
                            ASSERT(memcmp(set->progWaves[0], waveBytes, sizeof(waveBytes)) == 0,
                                   "wave bytes preserved");
                    }
                    else
                    {
                        ASSERT((set->waves[0] != NULL) == cases[i].resolves, "sample label resolution");
                        if (set->waves[0])
                            ASSERT(memcmp(set->waves[0]->data, sampleBytes + 16, 4) == 0, "sample bytes preserved");
                    }
                }
            }
            voicegroup_free_samples(set);
        }
    }
    /* Mixed record lengths must survive map growth without changing first-definition precedence. */
    char otherSample[512], otherWave[512];
    snprintf(otherSample, sizeof(otherSample), "%s/other.bin", root);
    snprintf(otherWave, sizeof(otherWave), "%s/other.pcm", root);
    unsigned char otherSampleBytes[sizeof(sampleBytes)];
    memcpy(otherSampleBytes, sampleBytes, sizeof(sampleBytes));
    otherSampleBytes[16] = 91;
    const unsigned char otherWaveBytes[16] = {0xfe, 0xdc, 0xba, 0x98};
    ASSERT(write_fixture(otherSample, otherSampleBytes, sizeof(otherSampleBytes)), "write distinct late PCM fixture");
    ASSERT(write_fixture(otherWave, otherWaveBytes, sizeof(otherWaveBytes)), "write distinct late wave fixture");
    FILE* dsFile = fopen(dsMap, "w");
    FILE* pwFile = fopen(pwMap, "w");
    bool written = dsFile && pwFile;
    if (written)
    {
        fprintf(dsFile, "FirstFile::\n.incbin \"sample.bin\"\nFirstSynth::\nset_synth_25\n");
        fprintf(pwFile, "FirstFile::\n.incbin \"wave.pcm\"\n");
        for (int i = 0; i < 1536; i++)
        {
            char padding[64];
            size_t length = (size_t)i % (sizeof(padding) - 1);
            memset(padding, 'x', length);
            padding[length] = '\0';
            fprintf(dsFile, "Unused%d%s::\n", i, padding);
            if (i % 3 == 0)
                fprintf(dsFile, "set_synth_custom 1, 2, 3, 4\n");
            else
                fprintf(dsFile, ".incbin \"unused/%s%d.bin\"\n", padding, i);
            fprintf(pwFile, "Unused%d%s::\n.incbin \"unused/%s%d.pcm\"\n", i, padding, padding, i);
        }
        fprintf(dsFile,
                "LastFile::\n.incbin \"other.bin\"\nLastSynth::\nset_synth_custom 5, 6, 7, 8\n"
                "FirstFile::\nset_synth_50\nFirstSynth::\n.incbin \"other.bin\"\n");
        fprintf(pwFile, "LastWave::\n.incbin \"other.pcm\"\nFirstFile::\n.incbin \"other.pcm\"\n");
        written = !ferror(dsFile) && !ferror(pwFile);
    }
    if (dsFile)
        written = fclose(dsFile) == 0 && written;
    if (pwFile)
        written = fclose(pwFile) == 0 && written;
    ASSERT(written, "write large mixed symbol maps");
    if (written)
    {
        const char* samples[] = {"FirstFile", "FirstSynth", "LastFile", "LastSynth"};
        const char* waves[] = {"FirstFile", "LastWave"};
        /* This public convenience call destroys its project context before returning. */
        LoadedSampleSet* set = voicegroup_load_samples(root, samples, 4, waves, 2, NULL, NULL, 0, NULL);
        ASSERT(set != NULL, "load early and late definitions through the public interface");
        if (set)
        {
            const unsigned char firstSynth[] = {0x80, 1, 0, 0, 0, 0};
            const unsigned char lastSynth[] = {0x80, 0, 5, 6, 7, 8};
            ASSERT(set->waves[0] && set->waves[0]->size == 4 && memcmp(set->waves[0]->data, sampleBytes + 16, 4) == 0,
                   "first file definition wins over a later synth after map growth and teardown");
            ASSERT(set->waves[1] && set->waves[1]->size == 0 &&
                       memcmp(set->waves[1]->data, firstSynth, sizeof(firstSynth)) == 0,
                   "first synth definition wins over a later file after map growth and teardown");
            ASSERT(set->waves[2] && set->waves[2]->size == 4 &&
                       memcmp(set->waves[2]->data, otherSampleBytes + 16, 4) == 0,
                   "late PCM record resolves its own path after map growth and teardown");
            ASSERT(set->waves[3] && set->waves[3]->size == 0 &&
                       memcmp(set->waves[3]->data, lastSynth, sizeof(lastSynth)) == 0,
                   "late synth retains its parameters after map growth and teardown");
            ASSERT(set->progWaves[0] && memcmp(set->progWaves[0], waveBytes, sizeof(waveBytes)) == 0,
                   "programmable map has independent names and preserves its first definition");
            ASSERT(set->progWaves[1] && memcmp(set->progWaves[1], otherWaveBytes, sizeof(otherWaveBytes)) == 0,
                   "late programmable record resolves its own path after map growth and teardown");
        }
        voicegroup_free_samples(set);
    }
    remove(otherSample);
    remove(otherWave);
    remove(dsMap);
    remove(pwMap);
    remove(sample);
    remove(wave);
    rmdir(sound);
    rmdir(root);
    return 0;
}
