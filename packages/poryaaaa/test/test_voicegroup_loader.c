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
    ASSERT(macro->kind == VG_MACRO_DIRECTSOUND_NO_RESAMPLE, "no-resample directsound keeps distinct macro kind");
    ASSERT(strcmp(args, "60, 0, Sample, 0, 0, 0, 0") == 0, "macro args skip keyword whitespace");
    ASSERT(vg_voice_macro_match("voice_directsound_alt 60, 0, Sample, 0, 0, 0, 0", &macro, &args),
           "alt directsound macro matches");
    ASSERT(macro && macro->typeCode == VOICE_DIRECTSOUND_ALT, "alt directsound forwards reverse type code");
    ASSERT(macro->kind == VG_MACRO_DIRECTSOUND_ALT, "alt directsound keeps distinct macro kind");
    ASSERT(vg_voice_macro_match("voice_directsound 60, 0, Sample, 0, 0, 0, 0", &macro, &args),
           "base directsound macro matches");
    ASSERT(macro && macro->typeCode == VOICE_DIRECTSOUND, "base directsound forwards normal type code");
    ASSERT(macro->kind == VG_MACRO_DIRECTSOUND, "base directsound keeps normal macro kind");
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

static void test_voicegroup_project_state_writes_drumset_without_loading_samples(void)
{
    printf("Testing voicegroup project state: drumset metadata without sample loading...\n");

    const char* root = "poryaaaa_state_project";
    const char* soundDir = "poryaaaa_state_project/sound";
    const char* dataPath = "poryaaaa_state_project/sound/direct_sound_data.inc";
    const char* voiceGroupsPath = "poryaaaa_state_project/sound/voice_groups.inc";
    const char* statePath = test_state_path();
    char output[8192];

    make_dir(root);
    make_dir(soundDir);

    ASSERT(write_text_file(dataPath,
                           "DirectSoundWaveData_Kick::\n"
                           "\t.incbin \"sound/direct_sound/kick.bin\"\n"
                           "DirectSoundWaveData_Snare::\n"
                           "\t.incbin \"sound/direct_sound/snare.bin\"\n"),
           "direct sound data source writes");
    ASSERT(write_text_file(voiceGroupsPath,
                           "main::\n"
                           "\tvoice_keysplit_all voicegroup_drumset @ Drums\n"
                           "\n"
                           "drumset::\n"
                           "\tvoice_group drumset, 36\n"
                           "\tvoice_directsound 60, 0, DirectSoundWaveData_Kick, 1, 2, 3, 4\n"
                           "\tvoice_directsound 62, 0, DirectSoundWaveData_Snare, 1, 2, 3, 4\n"),
           "voice_groups source writes");

    VoicegroupProjectState state;
    ASSERT(voicegroup_project_state_collect(root, "main", &state), "project state collection succeeds");

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
    remove(voiceGroupsPath);
    remove(dataPath);
    remove_dir(soundDir);
    remove_dir(root);
}

static void test_voicegroup_loader_rejects_bad_voice_macro(void)
{
    printf("Testing voicegroup loader: malformed voice macro fails parse...\n");

    const char* root = "poryaaaa_state_bad_macro";
    const char* soundDir = "poryaaaa_state_bad_macro/sound";
    const char* voiceGroupsPath = "poryaaaa_state_bad_macro/sound/voice_groups.inc";

    make_dir(root);
    make_dir(soundDir);

    ASSERT(write_text_file(voiceGroupsPath, "main::\n\tvoice_directsounnd 60, 0, Typo, 1, 2, 3, 4\n"),
           "bad voicegroup writes");

    LoadedVoiceGroup* vg = voicegroup_load(root, "main");
    ASSERT(vg == NULL, "bad voicegroup load fails");
    ASSERT(strstr(voicegroup_loader_last_error(), "malformed voice macro") != NULL, "bad voicegroup load stores error");
    ASSERT(strstr(voicegroup_loader_last_error(), "voice_directsounnd") != NULL, "bad voicegroup load stores bad line");

    remove(voiceGroupsPath);
    remove_dir(soundDir);
    remove_dir(root);
}

static void test_voicegroup_preserves_directsound_variant_type_codes(void)
{
    printf("Testing voicegroup loader/state: DirectSound variant type codes...\n");

    const char* root = "poryaaaa_ds_variant_state";
    const char* soundDir = "poryaaaa_ds_variant_state/sound";
    const char* sampleDir = "poryaaaa_ds_variant_state/sound/direct_sound";
    const char* samplePath = "poryaaaa_ds_variant_state/sound/direct_sound/shared.bin";
    const char* dataPath = "poryaaaa_ds_variant_state/sound/direct_sound_data.inc";
    const char* voiceGroupsPath = "poryaaaa_ds_variant_state/sound/voice_groups.inc";
    const unsigned char sample[] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 16, 32, 48,
    };

    make_dir(root);
    make_dir(soundDir);
    make_dir(sampleDir);

    ASSERT(write_bytes(samplePath, sample, sizeof(sample)), "directsound sample writes");
    ASSERT(write_text_file(dataPath,
                           "SharedSample::\n"
                           "\t.incbin \"sound/direct_sound/shared.bin\"\n"),
           "directsound data source writes");
    ASSERT(write_text_file(voiceGroupsPath,
                           "main::\n"
                           "\tvoice_directsound 60, 0, SharedSample, 1, 2, 3, 4\n"
                           "\tvoice_directsound_alt 60, 0, SharedSample, 1, 2, 3, 4\n"
                           "\tvoice_directsound_no_resample 60, 0, SharedSample, 1, 2, 3, 4\n"),
           "directsound variant voicegroup writes");

    LoadedVoiceGroup* vg = voicegroup_load(root, "main");
    ASSERT(vg != NULL, "directsound variant voicegroup loads");
    if (vg)
    {
        ASSERT(vg->voices[0].type == VOICE_DIRECTSOUND, "base directsound slot keeps normal type");
        ASSERT(vg->voices[1].type == VOICE_DIRECTSOUND_ALT, "alt directsound slot keeps reverse type");
        ASSERT(vg->voices[2].type == VOICE_DIRECTSOUND_NO_RESAMPLE, "no-resample directsound slot keeps fixed type");
        voicegroup_free(vg);
    }

    VoicegroupProjectState state;
    ASSERT(voicegroup_project_state_collect(root, "main", &state), "directsound variant state collects");
    ASSERT(state.slots[0].typeCode == VOICE_DIRECTSOUND, "state keeps normal directsound type");
    ASSERT(state.slots[1].typeCode == VOICE_DIRECTSOUND_ALT, "state keeps alt directsound type");
    ASSERT(state.slots[2].typeCode == VOICE_DIRECTSOUND_NO_RESAMPLE, "state keeps no-resample directsound type");
    ASSERT(strcmp(state.slots[0].name, "shared.bin") == 0, "normal directsound uses shared sample name");
    ASSERT(strcmp(state.slots[1].name, "shared.bin") == 0, "alt directsound uses shared sample name");
    ASSERT(strcmp(state.slots[2].name, "shared.bin") == 0, "no-resample directsound uses shared sample name");

    voicegroup_project_state_free(&state);
    remove(voiceGroupsPath);
    remove(dataPath);
    remove(samplePath);
    remove_dir(sampleDir);
    remove_dir(soundDir);
    remove_dir(root);
}

static void test_voicegroup_project_state_rejects_missing_drumset(void)
{
    printf("Testing voicegroup project state: missing drumset fails collection...\n");

    const char* root = "poryaaaa_state_missing_drumset";
    const char* soundDir = "poryaaaa_state_missing_drumset/sound";
    const char* voiceGroupsPath = "poryaaaa_state_missing_drumset/sound/voice_groups.inc";

    make_dir(root);
    make_dir(soundDir);

    ASSERT(write_text_file(voiceGroupsPath, "main::\n\tvoice_keysplit_all voicegroup_missing @ Missing\n"),
           "main voicegroup writes");

    VoicegroupProjectState state;
    ASSERT(!voicegroup_project_state_collect(root, "main", &state), "missing drumset collection fails");

    remove(voiceGroupsPath);
    remove_dir(soundDir);
    remove_dir(root);
}

static void test_voicegroup_project_state_marks_defined_source_slots(void)
{
    printf("Testing voicegroup project state: defined source slots and raw-symbol fallback...\n");

    const char* root = "poryaaaa_state_defined_slots";
    const char* soundDir = "poryaaaa_state_defined_slots/sound";
    const char* voiceGroupsPath = "poryaaaa_state_defined_slots/sound/voice_groups.inc";

    make_dir(root);
    make_dir(soundDir);

    ASSERT(write_text_file(voiceGroupsPath,
                           "main::\n"
                           "\tvoice_group main, 4\n"
                           "\tvoice_directsound 60, 0, MissingFromMap, 1, 2, 3, 4\n"),
           "offset voicegroup writes");

    VoicegroupProjectState state;
    ASSERT(voicegroup_project_state_collect(root, "main", &state), "defined slot collection succeeds");
    ASSERT(!state.slots[0].defined, "empty source slot is not defined");
    ASSERT(state.slots[4].defined, "source slot with voice macro is defined");
    ASSERT(state.slots[4].typeCode == VOICE_DIRECTSOUND, "defined slot forwards type code");
    ASSERT(strcmp(state.slots[4].name, "MissingFromMap") == 0, "missing map falls back to raw symbol label");

    voicegroup_project_state_free(&state);
    remove(voiceGroupsPath);
    remove_dir(soundDir);
    remove_dir(root);
}

static void test_voicegroup_symbol_map_growth(void)
{
    printf("Testing voicegroup loader: symbol map growth...\n");

    const char* root = "poryaaaa_voicegroup_symbols_project";
    const char* soundDir = "poryaaaa_voicegroup_symbols_project/sound";
    const char* path = "poryaaaa_voicegroup_symbols_project/sound/direct_sound_data.inc";
    make_dir(root);
    make_dir(soundDir);

    FILE* f = fopen(path, "w");
    ASSERT(f != NULL, "temporary symbol file opens for writing");
    if (!f)
    {
        remove_dir(soundDir);
        remove_dir(root);
        return;
    }

    for (int i = 0; i < 70; i++)
    {
        fprintf(f, "sample_%02d::\n", i);
        fprintf(f, "\t.incbin \"sound/direct_sound/sample_%02d.bin\"\n", i);
    }
    fclose(f);

    VoicegroupProjectAssets assets;
    memset(&assets, 0, sizeof(assets));
    bool ok = voicegroup_loader_collect_project_assets(root, &assets);
    remove(path);
    remove_dir(soundDir);
    remove_dir(root);

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

void test_voicegroup_loader_run_all(void)
{
    test_voice_macro_match_uses_ordered_table();
    test_voicegroup_project_state_default_path();
    test_voicegroup_project_state_writes_drumset_without_loading_samples();
    test_voicegroup_loader_rejects_bad_voice_macro();
    test_voicegroup_preserves_directsound_variant_type_codes();
    test_voicegroup_project_state_rejects_missing_drumset();
    test_voicegroup_project_state_marks_defined_source_slots();
    test_voicegroup_symbol_map_growth();
    test_voicegroup_bad_asset_examples();
}
