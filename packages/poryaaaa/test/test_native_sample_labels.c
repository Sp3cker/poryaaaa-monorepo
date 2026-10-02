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
    remove(dsMap);
    remove(pwMap);
    remove(sample);
    remove(wave);
    rmdir(sound);
    rmdir(root);
    return 0;
}
