#include "test_assert.h"

#include "voicegroup/voicegroup_loader.h"
#include "voicegroup/vg_wav.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif

static bool write_bytes(const char *path, const unsigned char *bytes, size_t count)
{
    FILE *f = fopen(path, "wb");
    if (!f)
        return false;
    bool ok = fwrite(bytes, 1, count, f) == count;
    fclose(f);
    return ok;
}

static bool write_text_file(const char *path, const char *text)
{
    FILE *f = fopen(path, "w");
    if (!f)
        return false;
    fputs(text, f);
    fclose(f);
    return true;
}

static bool read_text_file(const char *path, char *buf, size_t bufSize)
{
    FILE *f = fopen(path, "r");
    if (!f)
        return false;
    size_t n = fread(buf, 1, bufSize - 1, f);
    buf[n] = '\0';
    fclose(f);
    return true;
}

static void make_dir(const char *path)
{
#ifdef _WIN32
    _mkdir(path);
#else
    mkdir(path, 0755);
#endif
}

static void remove_dir(const char *path)
{
#ifdef _WIN32
    _rmdir(path);
#else
    rmdir(path);
#endif
}

static bool export_has_exact_layout(const char *text,
                                    const char *voicegroupName,
                                    const char *expected[12])
{
    char copy[8192];
    snprintf(copy, sizeof(copy), "%s", text);

    char expectedHeader[VG_MAX_LINE];
    snprintf(expectedHeader, sizeof(expectedHeader), "voice_group %s", voicegroupName);

    char *line = strtok(copy, "\n");
    if (!line || strcmp(line, expectedHeader) != 0)
        return false;

    for (int index = 0; index < 12; index++) {
        line = strtok(NULL, "\n");
        if (!line)
            return false;
        char expectedLine[VG_MAX_LINE];
        snprintf(expectedLine, sizeof(expectedLine), "\t%s", expected[index]);
        if (strcmp(line, expectedLine) != 0)
            return false;
    }

    return strtok(NULL, "\n") == NULL;
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

static void test_voicegroup_channel_export_per_file(void)
{
    printf("Testing voicegroup channel export: per-file source...\n");

    const char *root = "poryaaaa_channel_export_test";
    const char *soundDir = "poryaaaa_channel_export_test/sound";
    const char *voicegroupDir = "poryaaaa_channel_export_test/sound/voicegroups";
    const char *sourcePath = "poryaaaa_channel_export_test/sound/voicegroups/source.inc";
    char outputPath[VG_MAX_PATH_LEN];
    char output[8192];
    uint8_t programs[12];

    make_dir(root);
    make_dir(soundDir);
    make_dir(voicegroupDir);

    ASSERT(write_text_file(sourcePath,
        "\tvoice_directsound 60, 0, sample_a, 1, 2, 3, 4 @ sample a\n"
        "\tvoice_square_1 61, 0, 1, 2, 3, 4, 5, 6 @ square b\n"
        "\tvoice_noise 62, 0, 1, 3, 4, 5, 6 @ noise c\n"),
        "per-file voicegroup source writes");

    for (int i = 0; i < 12; i++)
        programs[i] = (uint8_t)i;
    programs[0] = 2;
    programs[1] = 0;
    programs[2] = 127;

    ASSERT(voicegroup_channel_export_default_path(root, "source",
                                                  outputPath, sizeof(outputPath)),
           "default channel export path is built");
    ASSERT(voicegroup_export_channel_remap(root, "source", NULL, programs, outputPath),
           "per-file channel export succeeds");
    ASSERT(read_text_file(outputPath, output, sizeof(output)),
           "per-file channel export output reads");
    {
        const char *placeholder = "voice_square_1 60, 0, 0, 0, 0, 0, 0, 0 @ unused";
        const char *expected[12] = {
            "voice_noise 62, 0, 1, 3, 4, 5, 6 @ noise c",
            "voice_directsound 60, 0, sample_a, 1, 2, 3, 4 @ sample a",
            placeholder,
            placeholder,
            placeholder,
            placeholder,
            placeholder,
            placeholder,
            placeholder,
            placeholder,
            placeholder,
            placeholder,
        };
        ASSERT(export_has_exact_layout(output, "source", expected),
               "per-file export writes header and exactly 12 tab-indented voices in channel order");
    }

    remove(outputPath);
    remove(sourcePath);
    remove_dir(voicegroupDir);
    remove_dir(soundDir);
    remove_dir(root);
}

static void test_voicegroup_channel_export_monolithic(void)
{
    printf("Testing voicegroup channel export: monolithic source...\n");

    const char *root = "poryaaaa_channel_export_mono_test";
    const char *soundDir = "poryaaaa_channel_export_mono_test/sound";
    const char *sourcePath = "poryaaaa_channel_export_mono_test/sound/voice_groups.inc";
    char outputPath[VG_MAX_PATH_LEN];
    char output[8192];
    uint8_t programs[12];

    make_dir(root);
    make_dir(soundDir);

    ASSERT(write_text_file(sourcePath,
        "other::\n"
        "\tvoice_square_2 60, 0, 1, 2, 3, 4, 5 @ other\n"
        "\t.align 2\n"
        "main::\n"
        "\tvoice_group main, 4\n"
        "\tvoice_programmable_wave 64, 0, wave_a, 1, 2, 3, 4 @ wave a\n"
        "\tvoice_keysplit voicegroup_main_keysplit, keysplit_main @ split\n"
        "\t.align 2\n"),
        "monolithic voicegroup source writes");

    for (int i = 0; i < 12; i++)
        programs[i] = 127;
    programs[0] = 4;
    programs[1] = 5;

    ASSERT(voicegroup_channel_export_default_path(root, "main",
                                                  outputPath, sizeof(outputPath)),
           "monolithic default channel export path is built");
    ASSERT(voicegroup_export_channel_remap(root, "main", NULL, programs, outputPath),
           "monolithic channel export succeeds");
    ASSERT(read_text_file(outputPath, output, sizeof(output)),
           "monolithic channel export output reads");
    {
        const char *placeholder = "voice_square_1 60, 0, 0, 0, 0, 0, 0, 0 @ unused";
        const char *expected[12] = {
            "voice_programmable_wave 64, 0, wave_a, 1, 2, 3, 4 @ wave a",
            "voice_keysplit voicegroup_main_keysplit, keysplit_main @ split",
            placeholder,
            placeholder,
            placeholder,
            placeholder,
            placeholder,
            placeholder,
            placeholder,
            placeholder,
            placeholder,
            placeholder,
        };
        ASSERT(export_has_exact_layout(output, "main", expected),
               "monolithic export writes header and exactly 12 tab-indented voices in channel order");
    }

    remove(outputPath);
    remove_dir("poryaaaa_channel_export_mono_test/sound/voicegroups");
    remove(sourcePath);
    remove_dir(soundDir);
    remove_dir(root);
}

void test_voicegroup_loader_run_all(void)
{
    test_voicegroup_symbol_map_growth();
    test_voicegroup_bad_asset_examples();
    test_voicegroup_channel_export_per_file();
    test_voicegroup_channel_export_monolithic();
}
