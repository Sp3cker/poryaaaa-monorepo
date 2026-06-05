#include "test_assert.h"

#include "voicegroup/voicegroup_loader.h"
#include "voicegroup/vg_wav.h"

#include <stdio.h>
#include <string.h>

static bool write_bytes(const char *path, const unsigned char *bytes, size_t count)
{
    FILE *f = fopen(path, "wb");
    if (!f)
        return false;
    bool ok = fwrite(bytes, 1, count, f) == count;
    fclose(f);
    return ok;
}

static void test_voicegroup_symbol_map_growth(void)
{
    printf("Testing voicegroup loader: symbol map growth...\n");

    const char *path = "poryaaaa_voicegroup_symbols_test.inc";
    FILE *f = fopen(path, "w");
    ASSERT(f != NULL, "temporary symbol file opens for writing");
    if (!f)
        return;

    for (int i = 0; i < 70; i++) {
        fprintf(f, "sample_%02d::\n", i);
        fprintf(f, "\t.incbin \"sound/direct_sound/sample_%02d.bin\"\n", i);
    }
    fclose(f);

    VoicegroupLoaderConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    snprintf(cfg.soundDataPaths[0], sizeof(cfg.soundDataPaths[0]), "%s", path);
    cfg.soundDataPathCount = 1;

    VoicegroupProjectAssets assets;
    memset(&assets, 0, sizeof(assets));
    bool ok = voicegroup_loader_collect_project_assets(".", &cfg, &assets);
    remove(path);

    ASSERT(ok, "project asset collection succeeds");
    ASSERT_EQ(assets.directsoundCount, 70, "all grown symbol-map entries collected");
    if (assets.directsoundCount == 70) {
        ASSERT(strcmp(assets.directsound[69].symbol, "sample_69") == 0,
               "last grown symbol entry is preserved");
        ASSERT(strcmp(assets.directsound[69].fileName, "sample_69.bin") == 0,
               "last grown symbol basename is preserved");
    }

    voicegroup_loader_free_project_assets(&assets);
}

static void test_voicegroup_bad_asset_examples(void)
{
    printf("Testing voicegroup loader: bad WAV/BIN examples...\n");

    const char *badWavPath = "poryaaaa_bad_sample.wav";
    const char *badBinPath = "poryaaaa_bad_sample.bin";

    const unsigned char badWav[] = {
        'R', 'I', 'F', 'F',
        4, 0, 0, 0,
        'W', 'A', 'V', 'E',
    };
    const unsigned char badBin[] = {
        0x00, 0x00, 0x00, 0x00,
    };

    ASSERT(write_bytes(badWavPath, badWav, sizeof(badWav)),
           "bad WAV example writes");
    ASSERT(write_bytes(badBinPath, badBin, sizeof(badBin)),
           "bad BIN example writes");

    WaveData *badWavData = vg_load_wav_file(badWavPath);
    WaveData *badBinData = vg_load_bin_sample(".", badBinPath);

    ASSERT(badWavData == NULL, "bad WAV example is rejected");
    ASSERT(badBinData == NULL, "bad BIN example is rejected");

    remove(badWavPath);
    remove(badBinPath);
}

void test_voicegroup_loader_run_all(void)
{
    test_voicegroup_symbol_map_growth();
    test_voicegroup_bad_asset_examples();
}
