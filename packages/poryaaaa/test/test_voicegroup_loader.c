#include "test_assert.h"

#include "voicegroup/voicegroup_loader.h"
#include "voicegroup/voicegroup_project_state.h"
#include "voicegroup/vg_voice_macro.h"
#include "voicegroup/vg_wav.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#    include <direct.h>
#else
#    include <unistd.h>
#endif

static void remove_dir(const char* path);

static void set_test_env(const char* name, const char* value)
{
#ifdef _WIN32
    _putenv_s(name, value);
#else
    setenv(name, value, 1);
#endif
}

static const char* test_state_path(void)
{
#ifdef _WIN32
    return "poryaaaa_state_test_home\\poryaaaa\\projects.json";
#elif defined(__APPLE__)
    return "poryaaaa_state_test_home/Library/Application Support/poryaaaa/projects.json";
#else
    return "poryaaaa_state_test_home/poryaaaa/projects.json";
#endif
}

static void cleanup_test_state_home(void)
{
#ifdef _WIN32
    remove_dir("poryaaaa_state_test_home\\poryaaaa");
#elif defined(__APPLE__)
    remove_dir("poryaaaa_state_test_home/Library/Application Support/poryaaaa");
    remove_dir("poryaaaa_state_test_home/Library/Application Support");
    remove_dir("poryaaaa_state_test_home/Library");
#else
    remove_dir("poryaaaa_state_test_home/poryaaaa");
#endif
    remove_dir("poryaaaa_state_test_home");
}

static void test_voice_macro_match_uses_ordered_table(void)
{
    printf("Testing voice macro matching: ordered table...\n");
    const VoicegroupMacro* macro = NULL;
    const char* args = NULL;
    ASSERT(vg_voice_macro_match("voice_directsound_no_resample 60, 0, Sample, 0, 0, 0, 0", &macro, &args),
           "specific directsound macro matches");
    ASSERT(macro && macro->typeCode == VOICE_DIRECTSOUND_NO_RESAMPLE, "specific directsound macro wins");
    ASSERT(strcmp(args, "60, 0, Sample, 0, 0, 0, 0") == 0, "macro args skip keyword whitespace");
    ASSERT(!vg_voice_macro_match("voice_noise_altitude 60", &macro, &args), "macro match requires whitespace boundary");
}

static void test_voicegroup_project_state_default_path(void)
{
    printf("Testing voicegroup project state: default path helper...\n");
    char path[512];
    set_test_env("HOME", "poryaaaa_state_test_home");
    ASSERT(voicegroup_project_state_default_path(path, sizeof(path)), "default path resolves");
    ASSERT(strcmp(path, test_state_path()) == 0, "default path matches projects.json contract");
}

static bool write_bytes(const char* path, const unsigned char* bytes, size_t count)
{
    FILE* f = fopen(path, "wb");
    if (!f)
        return false;
    bool ok = fwrite(bytes, 1, count, f) == count;
    fclose(f);
    return ok;
}

static bool write_text_file(const char* path, const char* text)
{
    FILE* f = fopen(path, "w");
    if (!f)
        return false;
    fputs(text, f);
    fclose(f);
    return true;
}

static bool read_text_file(const char* path, char* buf, size_t bufSize)
{
    FILE* f = fopen(path, "r");
    if (!f)
        return false;
    size_t n = fread(buf, 1, bufSize - 1, f);
    buf[n] = '\0';
    fclose(f);
    return true;
}

static void make_dir(const char* path)
{
#ifdef _WIN32
    _mkdir(path);
#else
    mkdir(path, 0755);
#endif
}

static void remove_dir(const char* path)
{
#ifdef _WIN32
    _rmdir(path);
#else
    rmdir(path);
#endif
}

static bool export_has_exact_layout(const char* text, const char* voicegroupName, const char* expected[12])
{
    char copy[8192];
    snprintf(copy, sizeof(copy), "%s", text);

    char expectedHeader[VG_MAX_LINE];
    snprintf(expectedHeader, sizeof(expectedHeader), "voice_group %s", voicegroupName);

    char* line = strtok(copy, "\n");
    if (!line || strcmp(line, expectedHeader) != 0)
        return false;

    for (int index = 0; index < 12; index++)
    {
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

static void test_voicegroup_project_state_writes_drumset_without_loading_samples(void)
{
    printf("Testing voicegroup project state: drumset metadata without sample loading...\n");

    const char* root = "poryaaaa_state_project";
    const char* soundDir = "poryaaaa_state_project/sound";
    const char* voicegroupDir = "poryaaaa_state_project/sound/voicegroups";
    const char* dataPath = "poryaaaa_state_project/sound/direct_sound_data.inc";
    const char* mainPath = "poryaaaa_state_project/sound/voicegroups/main.inc";
    const char* drumPath = "poryaaaa_state_project/sound/voicegroups/drumset.inc";
    const char* statePath = test_state_path();
    char output[8192];

    make_dir(root);
    make_dir(soundDir);
    make_dir(voicegroupDir);

    ASSERT(write_text_file(dataPath,
                           "DirectSoundWaveData_Kick::\n"
                           "\t.incbin \"sound/direct_sound/kick.bin\"\n"
                           "DirectSoundWaveData_Snare::\n"
                           "\t.incbin \"sound/direct_sound/snare.bin\"\n"),
           "direct sound data source writes");
    ASSERT(write_text_file(mainPath, "\tvoice_keysplit_all voicegroup_drumset @ Drums\n"), "main voicegroup writes");
    ASSERT(write_text_file(drumPath,
                           "\tvoice_group drumset, 36\n"
                           "\tvoice_directsound 60, 0, DirectSoundWaveData_Kick, 1, 2, 3, 4\n"
                           "\tvoice_directsound 62, 0, DirectSoundWaveData_Snare, 1, 2, 3, 4\n"),
           "drumset voicegroup writes");

    VoicegroupProjectState state;
    ASSERT(voicegroup_project_state_collect(root, "main", NULL, &state), "project state collection succeeds");

#ifdef _WIN32
    set_test_env("APPDATA", "poryaaaa_state_test_home");
#else
    set_test_env("HOME", "poryaaaa_state_test_home");
#    if !defined(__APPLE__)
    set_test_env("XDG_CONFIG_HOME", "poryaaaa_state_test_home");
#    endif
#endif

    ASSERT(voicegroup_project_state_write(root, "main", &state), "project state writer succeeds");
    ASSERT(read_text_file(statePath, output, sizeof(output)), "state output reads");
    ASSERT(strstr(output, "\"typeCode\": 128") != NULL, "drumset slot writes keysplit-all typeCode");
    ASSERT(strstr(output, "\"drumset\"") != NULL, "drumset key is written");
    ASSERT(strstr(output, "{\"note\": 36, \"name\": \"kick.bin\"}") != NULL, "first drum pad writes note/name");
    ASSERT(strstr(output, "{\"note\": 37, \"name\": \"snare.bin\"}") != NULL, "second drum pad writes note/name");
    ASSERT(strstr(output, "\"attack\"") == NULL, "drum pad entries do not write extra fields");

    voicegroup_project_state_free(&state);
    remove(statePath);
    cleanup_test_state_home();
    remove(drumPath);
    remove(mainPath);
    remove(dataPath);
    remove_dir(voicegroupDir);
    remove_dir(soundDir);
    remove_dir(root);
}

static void test_voicegroup_loader_rejects_bad_voice_macro(void)
{
    printf("Testing voicegroup loader: malformed voice macro fails parse...\n");

    const char* root = "poryaaaa_state_bad_macro";
    const char* soundDir = "poryaaaa_state_bad_macro/sound";
    const char* voicegroupDir = "poryaaaa_state_bad_macro/sound/voicegroups";
    const char* mainPath = "poryaaaa_state_bad_macro/sound/voicegroups/main.inc";

    make_dir(root);
    make_dir(soundDir);
    make_dir(voicegroupDir);

    ASSERT(write_text_file(mainPath, "\tvoice_directsounnd 60, 0, Typo, 1, 2, 3, 4\n"), "bad voicegroup writes");

    LoadedVoiceGroup* vg = voicegroup_load(root, "main", NULL);
    ASSERT(vg == NULL, "bad voicegroup load fails");
    ASSERT(strstr(voicegroup_loader_last_error(), "malformed voice macro") != NULL, "bad voicegroup load stores error");
    ASSERT(strstr(voicegroup_loader_last_error(), "voice_directsounnd") != NULL, "bad voicegroup load stores bad line");

    remove(mainPath);
    remove_dir(voicegroupDir);
    remove_dir(soundDir);
    remove_dir(root);
}

static void test_voicegroup_project_state_rejects_missing_drumset(void)
{
    printf("Testing voicegroup project state: missing drumset fails collection...\n");

    const char* root = "poryaaaa_state_missing_drumset";
    const char* soundDir = "poryaaaa_state_missing_drumset/sound";
    const char* voicegroupDir = "poryaaaa_state_missing_drumset/sound/voicegroups";
    const char* mainPath = "poryaaaa_state_missing_drumset/sound/voicegroups/main.inc";

    make_dir(root);
    make_dir(soundDir);
    make_dir(voicegroupDir);

    ASSERT(write_text_file(mainPath, "\tvoice_keysplit_all voicegroup_missing @ Missing\n"), "main voicegroup writes");

    VoicegroupProjectState state;
    ASSERT(!voicegroup_project_state_collect(root, "main", NULL, &state), "missing drumset collection fails");

    remove(mainPath);
    remove_dir(voicegroupDir);
    remove_dir(soundDir);
    remove_dir(root);
}

static void test_voicegroup_project_state_marks_defined_source_slots(void)
{
    printf("Testing voicegroup project state: defined source slots and raw-symbol fallback...\n");

    const char* root = "poryaaaa_state_defined_slots";
    const char* soundDir = "poryaaaa_state_defined_slots/sound";
    const char* voicegroupDir = "poryaaaa_state_defined_slots/sound/voicegroups";
    const char* mainPath = "poryaaaa_state_defined_slots/sound/voicegroups/main.inc";

    make_dir(root);
    make_dir(soundDir);
    make_dir(voicegroupDir);

    ASSERT(write_text_file(mainPath,
                           "\tvoice_group main, 4\n"
                           "\tvoice_directsound 60, 0, MissingFromMap, 1, 2, 3, 4\n"),
           "offset voicegroup writes");

    VoicegroupProjectState state;
    ASSERT(voicegroup_project_state_collect(root, "main", NULL, &state), "defined slot collection succeeds");
    ASSERT(!state.slots[0].defined, "empty source slot is not defined");
    ASSERT(state.slots[4].defined, "source slot with voice macro is defined");
    ASSERT(state.slots[4].typeCode == VOICE_DIRECTSOUND, "defined slot forwards type code");
    ASSERT(strcmp(state.slots[4].name, "MissingFromMap") == 0, "missing map falls back to raw symbol label");

    voicegroup_project_state_free(&state);
    remove(mainPath);
    remove_dir(voicegroupDir);
    remove_dir(soundDir);
    remove_dir(root);
}

static void test_voicegroup_symbol_map_growth(void)
{
    printf("Testing voicegroup loader: symbol map growth...\n");

    const char* path = "poryaaaa_voicegroup_symbols_test.inc";
    FILE* f = fopen(path, "w");
    ASSERT(f != NULL, "temporary symbol file opens for writing");
    if (!f)
        return;

    for (int i = 0; i < 70; i++)
    {
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
    if (assets.directsoundCount == 70)
    {
        ASSERT(strcmp(assets.directsound[69].symbol, "sample_69") == 0, "last grown symbol entry is preserved");
        ASSERT(strcmp(assets.directsound[69].fileName, "sample_69.bin") == 0,
               "last grown symbol basename is preserved");
    }

    voicegroup_loader_free_project_assets(&assets);
}

static void test_voicegroup_bad_asset_examples(void)
{
    printf("Testing voicegroup loader: bad WAV/BIN examples...\n");

    const char* badWavPath = "poryaaaa_bad_sample.wav";
    const char* badBinPath = "poryaaaa_bad_sample.bin";

    const unsigned char badWav[] = {
        'R',
        'I',
        'F',
        'F',
        4,
        0,
        0,
        0,
        'W',
        'A',
        'V',
        'E',
    };
    const unsigned char badBin[] = {
        0x00,
        0x00,
        0x00,
        0x00,
    };

    ASSERT(write_bytes(badWavPath, badWav, sizeof(badWav)), "bad WAV example writes");
    ASSERT(write_bytes(badBinPath, badBin, sizeof(badBin)), "bad BIN example writes");

    WaveData* badWavData = vg_load_wav_file(badWavPath);
    WaveData* badBinData = vg_load_bin_sample(".", badBinPath);

    ASSERT(badWavData == NULL, "bad WAV example is rejected");
    ASSERT(badBinData == NULL, "bad BIN example is rejected");

    remove(badWavPath);
    remove(badBinPath);
}

static void test_voicegroup_channel_export_per_file(void)
{
    printf("Testing voicegroup channel export: per-file source...\n");

    const char* root = "poryaaaa_channel_export_test";
    const char* soundDir = "poryaaaa_channel_export_test/sound";
    const char* voicegroupDir = "poryaaaa_channel_export_test/sound/voicegroups";
    const char* sourcePath = "poryaaaa_channel_export_test/sound/voicegroups/source.inc";
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

    ASSERT(voicegroup_channel_export_default_path(root, "source", outputPath, sizeof(outputPath)),
           "default channel export path is built");
    ASSERT(voicegroup_export_channel_remap(root, "source", NULL, programs, outputPath),
           "per-file channel export succeeds");
    ASSERT(read_text_file(outputPath, output, sizeof(output)), "per-file channel export output reads");
    {
        const char* placeholder = "voice_square_1 60, 0, 0, 0, 0, 0, 0, 0 @ unused";
        const char* expected[12] = {
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

static void test_voicegroup_channel_export_combined(void)
{
    printf("Testing voicegroup channel export: combined source...\n");

    const char* root = "poryaaaa_channel_export_mono_test";
    const char* soundDir = "poryaaaa_channel_export_mono_test/sound";
    const char* sourcePath = "poryaaaa_channel_export_mono_test/sound/voice_groups.inc";
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
           "combined voicegroup source writes");

    for (int i = 0; i < 12; i++)
        programs[i] = 127;
    programs[0] = 4;
    programs[1] = 5;

    ASSERT(voicegroup_channel_export_default_path(root, "main", outputPath, sizeof(outputPath)),
           "combined default channel export path is built");
    ASSERT(voicegroup_export_channel_remap(root, "main", NULL, programs, outputPath),
           "combined channel export succeeds");
    ASSERT(read_text_file(outputPath, output, sizeof(output)), "combined channel export output reads");
    {
        const char* placeholder = "voice_square_1 60, 0, 0, 0, 0, 0, 0, 0 @ unused";
        const char* expected[12] = {
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
               "combined export writes header and exactly 12 tab-indented voices in channel order");
    }

    remove(outputPath);
    remove_dir("poryaaaa_channel_export_mono_test/sound/voicegroups");
    remove(sourcePath);
    remove_dir(soundDir);
    remove_dir(root);
}

void test_voicegroup_loader_run_all(void)
{
    test_voice_macro_match_uses_ordered_table();
    test_voicegroup_project_state_default_path();
    test_voicegroup_project_state_writes_drumset_without_loading_samples();
    test_voicegroup_loader_rejects_bad_voice_macro();
    test_voicegroup_project_state_rejects_missing_drumset();
    test_voicegroup_project_state_marks_defined_source_slots();
    test_voicegroup_symbol_map_growth();
    test_voicegroup_bad_asset_examples();
    test_voicegroup_channel_export_per_file();
    test_voicegroup_channel_export_combined();
}
