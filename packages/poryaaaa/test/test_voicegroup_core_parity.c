#include "test_assert.h"

#include "voicegroup/voicegroup_loader.h"
#include "voicegroup_core.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#    include <direct.h>
#else
#    include <unistd.h>
#endif

static bool make_dir(const char* path)
{
#ifdef _WIN32
    return _mkdir(path) == 0 || errno == EEXIST;
#else
    return mkdir(path, 0777) == 0 || errno == EEXIST;
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

static bool write_text_file(const char* path, const char* text)
{
    FILE* f = fopen(path, "w");
    if (!f)
        return false;
    fputs(text, f);
    fclose(f);
    return true;
}

static bool write_bytes(const char* path, const uint8_t* bytes, size_t count)
{
    FILE* f = fopen(path, "wb");
    if (!f)
        return false;
    bool ok = fwrite(bytes, 1, count, f) == count;
    fclose(f);
    return ok;
}

/* Generate the bank sequentially so the first exercised program occupies the
 * intended MIDI slot without relying on the removed voice_group start index. */
static bool write_parity_voicegroups(const char* path)
{
    FILE* f = fopen(path, "w");
    if (!f)
        return false;

    bool ok = fputs("main::\n\tvoice_group main\n", f) >= 0;
    for (int slot = 0; slot < 36 && ok; slot++)
        ok = fputs("\tvoice_square_1 60, 0, 0, 2, 0, 0, 15, 0\n", f) >= 0;
    if (ok)
    {
        ok = fputs("\tvoice_directsound_no_resample 60, 7, SharedSample, 255, 252, 0, 115\n"
                   "\tvoice_programmable_wave 61, 12, PulseWave, 5, 2, 15, 3\n"
                   "\tvoice_square_1_alt 62, 0, 5, 3, 1, 2, 15, 3\n"
                   "\tvoice_square_2 63, 0, 2, 1, 2, 15, 3\n"
                   "\tvoice_noise_alt 64, 0, 1, 1, 2, 15, 3\n"
                   "\tvoice_keysplit voicegroup_child, keysplit_main @ Main Split\n"
                   "\tvoice_keysplit_all voicegroup_drums @ All Drums\n"
                   "\n"
                   "child::\n"
                   "\tvoice_group child\n"
                   "\tvoice_square_1 60, 0, 3, 2, 1, 2, 8, 3\n"
                   "\n"
                   "drums::\n"
                   "\tvoice_group drums\n"
                   "\tvoice_noise 36, 0, 1, 1, 2, 8, 3\n",
                   f) >= 0;
    }
    if (fclose(f) != 0)
        ok = false;
    return ok;
}

static void cleanup_fixture(void)
{
    remove("poryaaaa_vg_core_parity/sound/voice_groups.inc");
    remove("poryaaaa_vg_core_parity/sound/direct_sound/shared.bin");
    remove("poryaaaa_vg_core_parity/sound/direct_sound_data.inc");
    remove("poryaaaa_vg_core_parity/sound/programmable_wave/pulse.pcm");
    remove("poryaaaa_vg_core_parity/sound/programmable_wave_data.inc");
    remove("poryaaaa_vg_core_parity/sound/keysplit_tables.inc");
    remove_dir("poryaaaa_vg_core_parity/sound/direct_sound");
    remove_dir("poryaaaa_vg_core_parity/sound/programmable_wave");
    remove_dir("poryaaaa_vg_core_parity/sound");
    remove_dir("poryaaaa_vg_core_parity");
}

static bool write_parity_fixture(void)
{
    cleanup_fixture();
    if (!make_dir("poryaaaa_vg_core_parity") || !make_dir("poryaaaa_vg_core_parity/sound") ||
        !make_dir("poryaaaa_vg_core_parity/sound/direct_sound") ||
        !make_dir("poryaaaa_vg_core_parity/sound/programmable_wave"))
        return false;

    static const uint8_t directsound_bin[] = {
        0x00, 0x00, 0x00, 0x00, 0x43, 0x34, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x81, 0x01, 0x7F, 0x00,
    };
    static const uint8_t prog_wave_pcm[] = {
        0x01,
        0x23,
        0x45,
        0x67,
        0x89,
        0xAB,
        0xCD,
        0xEF,
        0xFE,
        0xDC,
        0xBA,
        0x98,
        0x76,
        0x54,
        0x32,
        0x10,
    };

    return write_bytes(
               "poryaaaa_vg_core_parity/sound/direct_sound/shared.bin", directsound_bin, sizeof(directsound_bin)) &&
           write_bytes(
               "poryaaaa_vg_core_parity/sound/programmable_wave/pulse.pcm", prog_wave_pcm, sizeof(prog_wave_pcm)) &&
           write_text_file("poryaaaa_vg_core_parity/sound/direct_sound_data.inc",
                           "SharedSample::\n"
                           "\t.incbin \"sound/direct_sound/shared.bin\"\n") &&
           write_text_file("poryaaaa_vg_core_parity/sound/programmable_wave_data.inc",
                           "PulseWave::\n"
                           "\t.incbin \"sound/programmable_wave/pulse.pcm\"\n") &&
           write_text_file("poryaaaa_vg_core_parity/sound/keysplit_tables.inc",
                           "keysplit main, 0\n"
                           "\tsplit 1, 64\n"
                           "\tsplit 2, 127\n") &&
           write_parity_voicegroups("poryaaaa_vg_core_parity/sound/voice_groups.inc");
}

static void read_core_display_name(const VoicegroupCoreBankResult* result, size_t slot, char* buffer, size_t buffer_len)
{
    voicegroup_core_bank_result_program_display_name(result, slot, buffer, buffer_len);
}

static void compare_envelope(uint8_t actual_attack,
                             uint8_t actual_decay,
                             uint8_t actual_sustain,
                             uint8_t actual_release,
                             uint8_t expected_attack,
                             uint8_t expected_decay,
                             uint8_t expected_sustain,
                             uint8_t expected_release,
                             const char* label)
{
    ASSERT_EQ(actual_attack, expected_attack, label);
    ASSERT_EQ(actual_decay, expected_decay, label);
    ASSERT_EQ(actual_sustain, expected_sustain, label);
    ASSERT_EQ(actual_release, expected_release, label);
}

static void test_voicegroup_core_matches_c_voicegroup_load(void)
{
    printf("Testing voicegroup-core parity: selected bank matches C voicegroup_load...\n");
    ASSERT(write_parity_fixture(), "parity fixture writes");

    LoadedVoiceGroup* c_vg = voicegroup_load("poryaaaa_vg_core_parity", "main");
    ASSERT(c_vg != NULL, "C voicegroup_load succeeds");

    VoicegroupCoreProjectIndex* index = NULL;
    ASSERT(voicegroup_core_project_index_load("poryaaaa_vg_core_parity", &index) == VOICEGROUP_CORE_STATUS_OK,
           "Rust project index loads through C ABI");
    ASSERT(index != NULL, "Rust project index handle returned");

    VoicegroupCoreBankResult* result = NULL;
    ASSERT(voicegroup_core_project_index_load_program_bank(index, "main", &result) == VOICEGROUP_CORE_STATUS_OK,
           "Rust selected bank loads through C ABI");
    ASSERT(result != NULL, "Rust bank result handle returned");
    ASSERT(voicegroup_core_bank_result_has_bank(result), "Rust bank result has bank");
    ASSERT_EQ(voicegroup_core_bank_result_diagnostic_count(result), 0, "Rust bank result has no diagnostics");

    if (c_vg && result)
    {
        char name[VG_MAX_VOICE_SAMPLE_NAME];

        VoicegroupCoreDirectSoundProgram ds = {0};
        ASSERT(voicegroup_core_bank_result_program_direct_sound(result, 36, &ds), "Rust slot 36 is DirectSound");
        ASSERT_EQ(
            voicegroup_core_bank_result_program_type_code(result, 36), c_vg->voices[36].type, "slot 36 type code");
        ASSERT_EQ(ds.key, c_vg->voices[36].key, "slot 36 key");
        ASSERT_EQ(ds.pan, c_vg->voices[36].panSweep & 0x7F, "slot 36 pan");
        compare_envelope(ds.attack,
                         ds.decay,
                         ds.sustain,
                         ds.release,
                         c_vg->voices[36].attack,
                         c_vg->voices[36].decay,
                         c_vg->voices[36].sustain,
                         c_vg->voices[36].release,
                         "slot 36 envelope");
        read_core_display_name(result, 36, name, sizeof(name));
        ASSERT(strcmp(name, c_vg->voiceSampleNames[36]) == 0, "slot 36 display name");

        VoicegroupCoreProgrammableWaveProgram pw = {0};
        ASSERT(voicegroup_core_bank_result_program_programmable_wave(result, 37, &pw), "Rust slot 37 is prog wave");
        ASSERT_EQ(
            voicegroup_core_bank_result_program_type_code(result, 37), c_vg->voices[37].type, "slot 37 type code");
        ASSERT_EQ(pw.key, c_vg->voices[37].key, "slot 37 key");
        ASSERT_EQ(pw.pan, c_vg->voices[37].panSweep & 0x7F, "slot 37 pan");
        compare_envelope(pw.attack,
                         pw.decay,
                         pw.sustain,
                         pw.release,
                         c_vg->voices[37].attack,
                         c_vg->voices[37].decay,
                         c_vg->voices[37].sustain,
                         c_vg->voices[37].release,
                         "slot 37 envelope");
        read_core_display_name(result, 37, name, sizeof(name));
        ASSERT(strcmp(name, c_vg->voiceSampleNames[37]) == 0, "slot 37 display name");

        VoicegroupCoreSquare1Program sq1 = {0};
        ASSERT(voicegroup_core_bank_result_program_square1(result, 38, &sq1), "Rust slot 38 is square 1");
        ASSERT_EQ(
            voicegroup_core_bank_result_program_type_code(result, 38), c_vg->voices[38].type, "slot 38 type code");
        ASSERT_EQ(sq1.key, c_vg->voices[38].key, "slot 38 key");
        ASSERT_EQ(sq1.sweep, c_vg->voices[38].panSweep, "slot 38 sweep");
        ASSERT_EQ(sq1.duty, (uint8_t)(uintptr_t)c_vg->voices[38].wavePointer, "slot 38 duty");
        compare_envelope(sq1.attack,
                         sq1.decay,
                         sq1.sustain,
                         sq1.release,
                         c_vg->voices[38].attack,
                         c_vg->voices[38].decay,
                         c_vg->voices[38].sustain,
                         c_vg->voices[38].release,
                         "slot 38 envelope");

        VoicegroupCoreSquare2Program sq2 = {0};
        ASSERT(voicegroup_core_bank_result_program_square2(result, 39, &sq2), "Rust slot 39 is square 2");
        ASSERT_EQ(
            voicegroup_core_bank_result_program_type_code(result, 39), c_vg->voices[39].type, "slot 39 type code");
        ASSERT_EQ(sq2.key, c_vg->voices[39].key, "slot 39 key");
        ASSERT_EQ(sq2.duty, (uint8_t)(uintptr_t)c_vg->voices[39].wavePointer, "slot 39 duty");
        compare_envelope(sq2.attack,
                         sq2.decay,
                         sq2.sustain,
                         sq2.release,
                         c_vg->voices[39].attack,
                         c_vg->voices[39].decay,
                         c_vg->voices[39].sustain,
                         c_vg->voices[39].release,
                         "slot 39 envelope");

        VoicegroupCoreNoiseProgram noise = {0};
        ASSERT(voicegroup_core_bank_result_program_noise(result, 40, &noise), "Rust slot 40 is noise");
        ASSERT_EQ(
            voicegroup_core_bank_result_program_type_code(result, 40), c_vg->voices[40].type, "slot 40 type code");
        ASSERT_EQ(noise.key, c_vg->voices[40].key, "slot 40 key");
        ASSERT_EQ(noise.period, (uint8_t)(uintptr_t)c_vg->voices[40].wavePointer, "slot 40 period");
        compare_envelope(noise.attack,
                         noise.decay,
                         noise.sustain,
                         noise.release,
                         c_vg->voices[40].attack,
                         c_vg->voices[40].decay,
                         c_vg->voices[40].sustain,
                         c_vg->voices[40].release,
                         "slot 40 envelope");

        VoicegroupCoreKeysplitProgram keysplit = {{0}};
        ASSERT(voicegroup_core_bank_result_program_keysplit(result, 41, &keysplit), "Rust slot 41 is keysplit");
        ASSERT_EQ(
            voicegroup_core_bank_result_program_type_code(result, 41), c_vg->voices[41].type, "slot 41 type code");
        ASSERT(c_vg->voices[41].subGroup != NULL, "C slot 41 has subgroup");
        ASSERT(c_vg->voices[41].keySplitTable != NULL, "C slot 41 has keysplit table");
        ASSERT(memcmp(keysplit.table, c_vg->voices[41].keySplitTable, sizeof(keysplit.table)) == 0, "slot 41 table");
        voicegroup_core_bank_result_program_sub_voicegroup(result, 41, name, sizeof(name));
        ASSERT(strcmp(name, "child") == 0, "slot 41 sub-voicegroup canonical name");
        read_core_display_name(result, 41, name, sizeof(name));
        ASSERT(strcmp(name, c_vg->voiceSampleNames[41]) == 0, "slot 41 display name");

        ASSERT_EQ(
            voicegroup_core_bank_result_program_type_code(result, 42), c_vg->voices[42].type, "slot 42 type code");
        ASSERT(c_vg->voices[42].subGroup != NULL, "C slot 42 has subgroup");
        voicegroup_core_bank_result_program_sub_voicegroup(result, 42, name, sizeof(name));
        ASSERT(strcmp(name, "drums") == 0, "slot 42 sub-voicegroup canonical name");
        read_core_display_name(result, 42, name, sizeof(name));
        ASSERT(strcmp(name, c_vg->voiceSampleNames[42]) == 0, "slot 42 display name");
    }

    voicegroup_core_bank_result_free(result);
    voicegroup_core_project_index_free(index);
    voicegroup_free(c_vg);
    cleanup_fixture();
}

void test_voicegroup_core_parity_run_all(void)
{
    test_voicegroup_core_matches_c_voicegroup_load();
}
