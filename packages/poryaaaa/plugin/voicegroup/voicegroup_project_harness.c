#include "voicegroup_project.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition)                                                                                               \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(condition))                                                                                              \
        {                                                                                                              \
            fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #condition);                            \
            exit(EXIT_FAILURE);                                                                                        \
        }                                                                                                              \
    } while (0)

#ifdef _WIN32
int main(void)
{
    fprintf(stderr, "voicegroup project harness requires a POSIX temporary directory\n");
    return 0;
}
#else
#    include <sys/stat.h>
#    include <unistd.h>

static void write_text(const char* root, const char* relative, const char* text)
{
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", root, relative);
    FILE* file = fopen(path, "wb");
    CHECK(file);
    CHECK(fwrite(text, 1, strlen(text), file) == strlen(text));
    CHECK(fclose(file) == 0);
}

static void write_bytes(const char* root, const char* relative, const uint8_t* bytes, size_t count)
{
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", root, relative);
    FILE* file = fopen(path, "wb");
    CHECK(file);
    CHECK(fwrite(bytes, 1, count, file) == count);
    CHECK(fclose(file) == 0);
}

static void remove_file(const char* root, const char* relative)
{
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", root, relative);
    CHECK(unlink(path) == 0);
}

static void put_be16(uint8_t* bytes, uint16_t value)
{
    bytes[0] = (uint8_t)(value >> 8);
    bytes[1] = (uint8_t)value;
}

static void put_be32(uint8_t* bytes, uint32_t value)
{
    bytes[0] = (uint8_t)(value >> 24);
    bytes[1] = (uint8_t)(value >> 16);
    bytes[2] = (uint8_t)(value >> 8);
    bytes[3] = (uint8_t)value;
}
static void put_le16(uint8_t* bytes, uint16_t value)
{
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8);
}

static void put_le32(uint8_t* bytes, uint32_t value)
{
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8);
    bytes[2] = (uint8_t)(value >> 16);
    bytes[3] = (uint8_t)(value >> 24);
}

static void write_wav(const char* root, const char* relative)
{
    uint8_t bytes[48] = {0};
    memcpy(bytes, "RIFF", 4);
    put_le32(bytes + 4, 40);
    memcpy(bytes + 8, "WAVE", 4);
    memcpy(bytes + 12, "fmt ", 4);
    put_le32(bytes + 16, 16);
    put_le16(bytes + 20, 1);
    put_le16(bytes + 22, 1);
    put_le32(bytes + 24, 8000);
    put_le32(bytes + 28, 8000);
    put_le16(bytes + 32, 1);
    put_le16(bytes + 34, 8);
    memcpy(bytes + 36, "data", 4);
    put_le32(bytes + 40, 3);
    bytes[44] = 0x90;
    bytes[45] = 0xa0;
    bytes[46] = 0xb0;
    write_bytes(root, relative, bytes, sizeof(bytes));
}

static void write_aiff(const char* root, const char* relative)
{
    enum
    {
        frameCount = 0x10005,
        dataOffset = 54,
        fileSize = dataOffset + frameCount + 1,
    };
    uint8_t* bytes = calloc(fileSize, 1);
    static const uint8_t sampleRate[10] = {0x40, 0x0b, 0xfa, 0, 0, 0, 0, 0, 0, 0};
    CHECK(bytes);
    memcpy(bytes, "FORM", 4);
    put_be32(bytes + 4, fileSize - 8);
    memcpy(bytes + 8, "AIFF", 4);
    memcpy(bytes + 12, "COMM", 4);
    put_be32(bytes + 16, 18);
    put_be16(bytes + 20, 1);
    put_be32(bytes + 22, frameCount);
    put_be16(bytes + 26, 8);
    memcpy(bytes + 28, sampleRate, sizeof(sampleRate));
    memcpy(bytes + 38, "SSND", 4);
    put_be32(bytes + 42, frameCount + 8);
    for (size_t i = 0; i < frameCount; i++)
        bytes[dataOffset + i] = (uint8_t)(0x40 + (i & 0x3f));
    write_bytes(root, relative, bytes, fileSize);
    free(bytes);
}

static const VoicegroupCatalogEntry*
find_entry(const VoicegroupProjectResult* snapshot, uint32_t kind, const char* symbol)
{
    for (size_t i = 0; i < snapshot->catalog_count; i++)
        if (snapshot->catalog[i].kind == kind && (!symbol || strcmp(snapshot->catalog[i].symbol, symbol) == 0))
            return &snapshot->catalog[i];
    return NULL;
}

static void check_source_preview(VoicegroupProject* project, const char* source, uint8_t expectedFirstByte)
{
    VoicegroupLoadRequest request = {
        .mode = VG_LOAD_SOURCE,
        .bank_name = "preview",
        .bank_name_len = 7,
        .relative_path = "sound/voicegroups/preview.inc",
        .relative_path_len = strlen("sound/voicegroups/preview.inc"),
        .source_bytes = source,
        .source_len = strlen(source),
    };
    VoicegroupLoadResult result = voicegroup_project_load(project, &request);
    CHECK(result.succeeded);
    LoadedVoiceGroup* bank = voicegroup_load_result_take(&result);
    CHECK(bank);
    CHECK(bank->waveDataCount > 0 && bank->waveDatas[0]);
    CHECK(bank->waveDatas[0]->size == 3);
    CHECK((uint8_t)bank->waveDatas[0]->data[0] == expectedFirstByte);
    voicegroup_load_result_free(&result);
    CHECK(result._private_storage == NULL);
    voicegroup_free(bank);
}

static int has_diagnostic_code(const VoicegroupDiagnostic* diagnostics, size_t count, const char* code)
{
    for (size_t i = 0; i < count; i++)
        if (diagnostics[i].code && strcmp(diagnostics[i].code, code) == 0)
            return 1;
    return 0;
}

int main(void)
{
    char rootTemplate[] = "/tmp/voicegroup-project-harness-XXXXXX";
    char* root = mkdtemp(rootTemplate);
    CHECK(root);
    char sound[1024];
    char direct[1024];
    char prog[1024];
    char asmRoot[1024];
    char macros[1024];
    snprintf(sound, sizeof(sound), "%s/sound", root);
    snprintf(direct, sizeof(direct), "%s/sound/direct_sound_samples", root);
    snprintf(prog, sizeof(prog), "%s/sound/programmable_wave_samples", root);
    snprintf(asmRoot, sizeof(asmRoot), "%s/asm", root);
    snprintf(macros, sizeof(macros), "%s/asm/macros", root);
    CHECK(mkdir(sound, 0700) == 0);
    CHECK(mkdir(direct, 0700) == 0);
    CHECK(mkdir(prog, 0700) == 0);
    CHECK(mkdir(asmRoot, 0700) == 0);
    CHECK(mkdir(macros, 0700) == 0);

    const char* voiceGroups = "main::\n"
                              "\tvoice_directsound 60, 0, DirectSoundWave, 255, 0, 255, 4\n"
                              "\tvoice_directsound 60, 0, DirectSoundWave, 255, 0, 255, 0\n"
                              "\tvoice_directsound 60, 0, SynthWave, 255, 0, 255, 0\n"
                              "\tvoice_programmable_wave 60, 0, ProgrammableWave, 7, 0, 7, 0\n"
                              "\tvoice_keysplit voicegroup_sub, keysplit_test\n"
                              "sub::\n"
                              "\tvoice_square_1 60, 0, 0, 0, 0, 0, 0, 3\n";
    write_text(root, "sound/voice_groups.inc", voiceGroups);
    write_text(root,
               "sound/direct_sound_data.inc",
               "DirectSoundWave::\n\t.incbin \"sound/direct_sound_samples/kick.bin\"\n"
               "AiffWave::\n\t.incbin \"sound/direct_sound_samples/aiff.bin\"\n"
               "WavWave::\n\t.incbin \"sound/direct_sound_samples/wav.bin\"\n"
               "ZeroBinSynth::\n\t.incbin \"sound/direct_sound_samples/zero_bin_synth.bin\"\n"
               "ZeroWav::\n\t.incbin \"sound/direct_sound_samples/zero_wav.bin\"\n"
               "ZeroAif::\n\t.incbin \"sound/direct_sound_samples/zero_aif.bin\"\n");
    write_text(root, "sound/direct_sound_synth_data.inc", "SynthWave::\n\tset_synth_custom 1, 2, 3, 4\n");
    write_text(root,
               "asm/macros/music_voice.inc",
               ".macro set_synth_custom\n.endm\n"
               ".macro set_synth_pulse\n.endm\n"
               ".macro set_synth_25\n.endm\n"
               ".macro set_synth_saw\n.endm\n"
               ".macro set_synth_50\n.endm\n"
               ".macro set_synth_triangle\n.endm\n"
               "set_synth_pulse 1, 2, 3, 4\n"
               ".macro set_synth_500\n.endm\n"
               "@ .macro set_synth_custom_v2\n");
    write_text(root,
               "sound/programmable_wave_data.inc",
               "ProgrammableWave::\n\t.incbin \"sound/programmable_wave_samples/wave.pcm\"\n");
    write_text(root, "sound/keysplit_tables.inc", "keysplit test, 0\nsplit 0, 128\n");
    uint8_t bin[19] = {0};
    bin[5] = 0x04;
    bin[12] = 3;
    bin[16] = 0x11;
    bin[17] = 0x22;
    bin[18] = 0x33;
    write_bytes(root, "sound/direct_sound_samples/kick.bin", bin, sizeof(bin));
    uint8_t aiffBin[19];
    memcpy(aiffBin, bin, sizeof(aiffBin));
    aiffBin[16] = 0x66;
    write_bytes(root, "sound/direct_sound_samples/aiff.bin", aiffBin, sizeof(aiffBin));
    write_wav(root, "sound/direct_sound_samples/wav.wav");
    write_aiff(root, "sound/direct_sound_samples/wav.aif");
    uint8_t wavBin[19];
    memcpy(wavBin, bin, sizeof(wavBin));
    wavBin[16] = 0x77;
    write_bytes(root, "sound/direct_sound_samples/wav.bin", wavBin, sizeof(wavBin));
    uint8_t zeroBinSynth[32] = {0};
    zeroBinSynth[16] = 0x80;
    zeroBinSynth[17] = 0x02;
    zeroBinSynth[18] = 0x01;
    zeroBinSynth[19] = 0x02;
    zeroBinSynth[20] = 0x03;
    zeroBinSynth[21] = 0x04;
    write_bytes(root, "sound/direct_sound_samples/zero_bin_synth.bin", zeroBinSynth, sizeof(zeroBinSynth));
    write_aiff(root, "sound/direct_sound_samples/aiff.aif");
    uint8_t wave[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
    write_bytes(root, "sound/programmable_wave_samples/wave.pcm", wave, sizeof(wave));
    uint8_t empty = 0;
    write_bytes(root, "sound/direct_sound_samples/zero_wav.wav", &empty, 0);
    write_bytes(root, "sound/direct_sound_samples/zero_aif.aif", &empty, 0);

    VoicegroupProject* project = voicegroup_project_open(root, strlen(root));
    CHECK(project);
    VoicegroupProjectResult snapshot = {0};
    voicegroup_project_refresh(project, &snapshot);
    CHECK(snapshot.succeeded);
    CHECK(snapshot.diagnostic_count == 0);
    CHECK(snapshot.catalog_count >= 4);
    const VoicegroupCatalogEntry* directEntry = find_entry(&snapshot, 1, "DirectSoundWave");
    const VoicegroupCatalogEntry* progEntry = find_entry(&snapshot, 2, "ProgrammableWave");
    const VoicegroupCatalogEntry* keysplitEntry = find_entry(&snapshot, 3, "keysplit_test");
    CHECK(directEntry && progEntry && keysplitEntry);
    CHECK(strcmp(directEntry->symbol, "DirectSoundWave") == 0);
    CHECK(strcmp(progEntry->symbol, "ProgrammableWave") == 0);
    CHECK(strcmp(keysplitEntry->symbol, "keysplit_test") == 0);
    CHECK(snapshot.family_adsr_count == 2 && strcmp(snapshot.family_adsr[0].family, "directsound") == 0 &&
          snapshot.family_adsr[0].adsr[3] == 4 && strcmp(snapshot.family_adsr[1].family, "square_1") == 0);
    CHECK(snapshot.synth_macro_word_count == 6 && strcmp(snapshot.synth_macro_words[0], "set_synth_25") == 0 &&
          strcmp(snapshot.synth_macro_words[5], "set_synth_triangle") == 0);
    int macroWatch = 0;
    for (size_t i = 0; i < snapshot.watch_path_count; i++)
        macroWatch |= strcmp(snapshot.watch_paths[i], "asm/macros") == 0;
    CHECK(macroWatch);
    VoicegroupAssetResult first =
        voicegroup_project_load_asset(project, VG_ASSET_DIRECT_SOUND, directEntry->symbol, strlen(directEntry->symbol));
    VoicegroupAssetResult second =
        voicegroup_project_load_asset(project, VG_ASSET_DIRECT_SOUND, directEntry->symbol, strlen(directEntry->symbol));
    CHECK(first.diagnostic_count == 0 && second.diagnostic_count == 0);
    CHECK(first.payload && second.payload && first.payload_len == 3 && second.payload_len == 3);
    CHECK(first.payload != second.payload);
    CHECK(memcmp(first.payload, second.payload, 3) == 0);
    ((uint8_t*)first.payload)[0] = 0xee;
    CHECK(((const uint8_t*)second.payload)[0] == 0x11);
    VoicegroupAssetResult cached =
        voicegroup_project_load_asset(project, VG_ASSET_DIRECT_SOUND, directEntry->symbol, strlen(directEntry->symbol));
    CHECK(cached.diagnostic_count == 0 && cached.payload != second.payload);
    CHECK(((const uint8_t*)cached.payload)[0] == 0x11);

    bin[16] = 0x44;
    write_bytes(root, "sound/direct_sound_samples/kick.bin", bin, sizeof(bin));
    voicegroup_project_mark_stale(project);
    VoicegroupAssetResult changed =
        voicegroup_project_load_asset(project, VG_ASSET_DIRECT_SOUND, directEntry->symbol, strlen(directEntry->symbol));
    CHECK(changed.diagnostic_count == 0 && changed.payload_len == 3);
    CHECK(((const uint8_t*)changed.payload)[0] == 0x44);
    voicegroup_asset_result_free(&first);
    voicegroup_asset_result_free(&second);
    voicegroup_asset_result_free(&cached);
    voicegroup_asset_result_free(&changed);

    VoicegroupAssetResult progResult =
        voicegroup_project_load_asset(project, VG_ASSET_PROG_WAVE, progEntry->symbol, strlen(progEntry->symbol));
    CHECK(progResult.diagnostic_count == 0 && progResult.payload_len == 16);
    voicegroup_asset_result_free(&progResult);
    VoicegroupAssetResult keysplitResult =
        voicegroup_project_load_asset(project, VG_ASSET_KEYSPLIT, keysplitEntry->symbol, strlen(keysplitEntry->symbol));
    CHECK(keysplitResult.diagnostic_count == 0);
    CHECK(keysplitResult.keysplit.subgroup_count == VOICEGROUP_SIZE);
    CHECK(keysplitResult.keysplit.table_count == VOICEGROUP_SIZE);
    voicegroup_asset_result_free(&keysplitResult);

    VoicegroupAssetResult aiff =
        voicegroup_project_load_asset(project, VG_ASSET_DIRECT_SOUND, "AiffWave", strlen("AiffWave"));
    CHECK(aiff.diagnostic_count == 0 && aiff.payload && aiff.payload_len == 0x10004);
    CHECK(aiff.frame_count == 0x10004);
    CHECK(((const uint8_t*)aiff.payload)[0] == 0x40);
    CHECK(((const uint8_t*)aiff.payload)[0x10003] == 0x43);
    voicegroup_asset_result_free(&aiff);
    VoicegroupAssetResult wavAsset =
        voicegroup_project_load_asset(project, VG_ASSET_DIRECT_SOUND, "WavWave", strlen("WavWave"));
    CHECK(wavAsset.diagnostic_count == 0 && wavAsset.payload_len == 3 && wavAsset.frame_count == 3);
    CHECK(((const uint8_t*)wavAsset.payload)[0] == 0x10);
    voicegroup_asset_result_free(&wavAsset);
    VoicegroupAssetResult zeroBin =
        voicegroup_project_load_asset(project, VG_ASSET_DIRECT_SOUND, "ZeroBinSynth", strlen("ZeroBinSynth"));
    static const uint8_t expectedZeroBin[6] = {0x80, 0x02, 0x01, 0x02, 0x03, 0x04};
    CHECK(zeroBin.diagnostic_count == 0 && zeroBin.synth_desc);
    CHECK(memcmp(zeroBin.synth_desc, expectedZeroBin, sizeof(expectedZeroBin)) == 0);
    voicegroup_asset_result_free(&zeroBin);
    VoicegroupAssetResult zeroWav =
        voicegroup_project_load_asset(project, VG_ASSET_DIRECT_SOUND, "ZeroWav", strlen("ZeroWav"));
    CHECK(zeroWav.diagnostic_count > 0);
    voicegroup_asset_result_free(&zeroWav);
    VoicegroupAssetResult zeroAif =
        voicegroup_project_load_asset(project, VG_ASSET_DIRECT_SOUND, "ZeroAif", strlen("ZeroAif"));
    CHECK(zeroAif.diagnostic_count > 0);
    voicegroup_asset_result_free(&zeroAif);
    const char* previewSource = "voice_group preview\n"
                                "\tvoice_directsound 60, 0, DirectSoundWave, 255, 0, 255, 0\n";
    check_source_preview(project, previewSource, 0x44);
    VoicegroupAssetResult nullAsset = voicegroup_project_load_asset(project, VG_ASSET_DIRECT_SOUND, NULL, 0);
    CHECK(nullAsset.diagnostic_count > 0);
    voicegroup_asset_result_free(&nullAsset);
    VoicegroupAssetResult invalidAsset =
        voicegroup_project_load_asset(project, (VoicegroupAssetKind)99, "DirectSoundWave", strlen("DirectSoundWave"));
    CHECK(invalidAsset.diagnostic_count > 0);
    voicegroup_asset_result_free(&invalidAsset);

    VoicegroupLoadRequest savedRequest = {
        .mode = VG_LOAD_SAVED,
        .bank_name = "main",
        .bank_name_len = 4,
    };
    VoicegroupLoadResult load = voicegroup_project_load(project, &savedRequest);
    CHECK(load.succeeded);
    LoadedVoiceGroup* taken = voicegroup_load_result_take(&load);
    CHECK(taken && taken->waveDataCount == 2);
    CHECK(taken->voices[0].wav && taken->voices[0].wav == taken->voices[1].wav);
    static const uint8_t expectedSynth[6] = {0x80, 0x00, 0x01, 0x02, 0x03, 0x04};
    CHECK(taken->voices[2].wav && taken->voices[2].wav->size == 0);
    CHECK(memcmp(taken->voices[2].wav->data, expectedSynth, sizeof(expectedSynth)) == 0);
    uint8_t preservedByte = (uint8_t)taken->waveDatas[0]->data[0];
    voicegroup_load_result_free(&load);
    CHECK(load._private_storage == NULL);
    CHECK((uint8_t)taken->waveDatas[0]->data[0] == preservedByte);
    voicegroup_free(taken);
    VoicegroupLoadResult untaken = voicegroup_project_load(project, &savedRequest);
    CHECK(untaken.succeeded);
    voicegroup_load_result_free(&untaken);
    CHECK(!untaken.succeeded && untaken._private_storage == NULL);

    const char* invalidDisk = "main::\n\tvoice_directsound 60, 0, MissingSample, 255, 0, 255, 0\n";
    write_text(root, "sound/voice_groups.inc", invalidDisk);
    voicegroup_project_mark_stale(project);
    VoicegroupAssetResult invalidGenerationAsset =
        voicegroup_project_load_asset(project, VG_ASSET_DIRECT_SOUND, "DirectSoundWave", strlen("DirectSoundWave"));
    CHECK(invalidGenerationAsset.diagnostic_count == 0 && invalidGenerationAsset.payload_len == 3);
    CHECK(((const uint8_t*)invalidGenerationAsset.payload)[0] == 0x44);
    voicegroup_asset_result_free(&invalidGenerationAsset);

    VoicegroupProjectResult invalidSnapshot = {0};
    voicegroup_project_refresh(project, &invalidSnapshot);
    CHECK(invalidSnapshot.succeeded && invalidSnapshot.diagnostic_count > 0);
    voicegroup_project_result_free(&invalidSnapshot);

    VoicegroupLoadResult invalidLoad = voicegroup_project_load(project, &savedRequest);
    CHECK(!invalidLoad.succeeded && invalidLoad.diagnostic_count > 0 && invalidLoad.diagnostics &&
          invalidLoad.diagnostics[0].code && invalidLoad.diagnostics[0].message);
    voicegroup_load_result_free(&invalidLoad);

    write_text(root, "sound/voice_groups.inc", voiceGroups);
    voicegroup_project_mark_stale(project);
    VoicegroupAssetResult restoredAsset =
        voicegroup_project_load_asset(project, VG_ASSET_DIRECT_SOUND, "DirectSoundWave", strlen("DirectSoundWave"));
    CHECK(restoredAsset.diagnostic_count == 0 && restoredAsset.payload_len == 3);
    CHECK(((const uint8_t*)restoredAsset.payload)[0] == 0x44);
    voicegroup_asset_result_free(&restoredAsset);

    write_text(root, "sound/voice_groups.inc", invalidDisk);
    voicegroup_project_mark_stale(project);
    VoicegroupAssetResult secondInvalidAsset =
        voicegroup_project_load_asset(project, VG_ASSET_DIRECT_SOUND, "DirectSoundWave", strlen("DirectSoundWave"));
    CHECK(secondInvalidAsset.diagnostic_count == 0 && secondInvalidAsset.payload_len == 3);
    CHECK(((const uint8_t*)secondInvalidAsset.payload)[0] == 0x44);
    voicegroup_asset_result_free(&secondInvalidAsset);

    write_text(root, "sound/voice_groups.inc", voiceGroups);
    VoicegroupProjectResult recovered = {0};
    voicegroup_project_refresh(project, &recovered);
    CHECK(recovered.succeeded && recovered.diagnostic_count == 0);
    voicegroup_project_result_free(&recovered);
    VoicegroupAssetResult explicitRetry =
        voicegroup_project_load_asset(project, VG_ASSET_DIRECT_SOUND, "DirectSoundWave", strlen("DirectSoundWave"));
    CHECK(explicitRetry.diagnostic_count == 0 && explicitRetry.payload_len == 3);
    CHECK(((const uint8_t*)explicitRetry.payload)[0] == 0x44);
    voicegroup_asset_result_free(&explicitRetry);
    const uint8_t invalidIndexSource[] = {0xff};
    write_bytes(root, "sound/voice_groups.inc", invalidIndexSource, sizeof(invalidIndexSource));
    voicegroup_project_mark_stale(project);
    VoicegroupAssetResult invalidIndexSourceAsset =
        voicegroup_project_load_asset(project, VG_ASSET_DIRECT_SOUND, "DirectSoundWave", strlen("DirectSoundWave"));
    CHECK(invalidIndexSourceAsset.diagnostic_count > 0 && invalidIndexSourceAsset.diagnostics);
    CHECK(has_diagnostic_code(
        invalidIndexSourceAsset.diagnostics, invalidIndexSourceAsset.diagnostic_count, "project.index_load_failed"));
    voicegroup_asset_result_free(&invalidIndexSourceAsset);

    VoicegroupProjectResult invalidIndexSourceSnapshot = {0};
    voicegroup_project_refresh(project, &invalidIndexSourceSnapshot);
    CHECK(!invalidIndexSourceSnapshot.succeeded && invalidIndexSourceSnapshot.diagnostic_count > 0 &&
          invalidIndexSourceSnapshot.diagnostics);
    CHECK(has_diagnostic_code(invalidIndexSourceSnapshot.diagnostics,
                              invalidIndexSourceSnapshot.diagnostic_count,
                              "project.index_load_failed"));
    voicegroup_project_result_free(&invalidIndexSourceSnapshot);
    check_source_preview(project, previewSource, 0x44);

    write_text(root, "sound/voice_groups.inc", voiceGroups);
    voicegroup_project_mark_stale(project);
    VoicegroupAssetResult rearmedAsset =
        voicegroup_project_load_asset(project, VG_ASSET_DIRECT_SOUND, "DirectSoundWave", strlen("DirectSoundWave"));
    CHECK(rearmedAsset.diagnostic_count == 0 && rearmedAsset.payload_len == 3);
    CHECK(((const uint8_t*)rearmedAsset.payload)[0] == 0x44);
    voicegroup_asset_result_free(&rearmedAsset);

    voicegroup_project_result_free(&snapshot);
    CHECK(!snapshot._private_storage && !snapshot.family_adsr && !snapshot.synth_macro_words);
    voicegroup_project_free(project);

    char rootBTemplate[] = "/tmp/voicegroup-project-harness-b-XXXXXX";
    char* rootB = mkdtemp(rootBTemplate);
    CHECK(rootB);
    char soundB[1024];
    char directB[1024];
    snprintf(soundB, sizeof(soundB), "%s/sound", rootB);
    snprintf(directB, sizeof(directB), "%s/sound/direct_sound_samples", rootB);
    CHECK(mkdir(soundB, 0700) == 0);
    CHECK(mkdir(directB, 0700) == 0);
    write_text(rootB, "sound/voice_groups.inc", "main::\n\tvoice_directsound 60, 0, OtherWave, 255, 0, 255, 0\n");
    write_text(
        rootB, "sound/direct_sound_data.inc", "OtherWave::\n\t.incbin \"sound/direct_sound_samples/other.bin\"\n");
    write_text(rootB, "sound/programmable_wave_data.inc", "");
    write_text(rootB, "sound/keysplit_tables.inc", "");
    uint8_t otherBin[19];
    memcpy(otherBin, bin, sizeof(otherBin));
    otherBin[16] = 0x66;
    write_bytes(rootB, "sound/direct_sound_samples/other.bin", otherBin, sizeof(otherBin));
    VoicegroupProject* projectB = voicegroup_project_open(rootB, strlen(rootB));
    CHECK(projectB);
    VoicegroupProjectResult snapshotB = {0};
    voicegroup_project_refresh(projectB, &snapshotB);
    CHECK(snapshotB.succeeded);
    VoicegroupLoadResult loadB = voicegroup_project_load(projectB, &savedRequest);
    CHECK(loadB.succeeded);
    LoadedVoiceGroup* bankB = voicegroup_load_result_take(&loadB);
    CHECK(bankB && bankB->voices[0].wav && (uint8_t)bankB->voices[0].wav->data[0] == 0x66);
    voicegroup_load_result_free(&loadB);
    voicegroup_free(bankB);
    voicegroup_project_result_free(&snapshotB);
    voicegroup_project_free(projectB);

    const char* cycleGroups = "cycle::\n\tvoice_keysplit cycle, keysplit_test\n";
    write_text(root, "sound/voice_groups.inc", cycleGroups);
    VoicegroupProject* cycleProject = voicegroup_project_open(root, strlen(root));
    CHECK(cycleProject);
    VoicegroupProjectResult cycleSnapshot = {0};
    voicegroup_project_refresh(cycleProject, &cycleSnapshot);
    CHECK(cycleSnapshot.succeeded);
    VoicegroupLoadRequest cycleRequest = {
        .mode = VG_LOAD_SAVED,
        .bank_name = "cycle",
        .bank_name_len = 5,
    };
    VoicegroupLoadResult cycleLoad = voicegroup_project_load(cycleProject, &cycleRequest);
    CHECK(!cycleLoad.succeeded && cycleLoad.diagnostic_count > 0);
    int cycleDiagnostic = 0;
    for (size_t i = 0; i < cycleLoad.diagnostic_count; i++)
        if (cycleLoad.diagnostics[i].message && strstr(cycleLoad.diagnostics[i].message, "cyclic"))
            cycleDiagnostic = 1;
    CHECK(cycleDiagnostic);
    voicegroup_load_result_free(&cycleLoad);
    voicegroup_project_result_free(&cycleSnapshot);
    voicegroup_project_free(cycleProject);

    char rootCTemplate[] = "/tmp/voicegroup-project-harness-contiguous-XXXXXX";
    char* rootC = mkdtemp(rootCTemplate);
    CHECK(rootC);
    char soundC[1024];
    char voiceDirC[1024];
    char directC[1024];
    snprintf(soundC, sizeof(soundC), "%s/sound", rootC);
    snprintf(voiceDirC, sizeof(voiceDirC), "%s/sound/voicegroups", rootC);
    snprintf(directC, sizeof(directC), "%s/sound/direct_sound_samples", rootC);
    CHECK(mkdir(soundC, 0700) == 0);
    CHECK(mkdir(voiceDirC, 0700) == 0);
    CHECK(mkdir(directC, 0700) == 0);
    write_text(rootC,
               "sound/voice_groups.inc",
               ".include \"sound/voicegroups/main.inc\"\n"
               ".include \"sound/voicegroups/short.inc\"\n"
               ".include \"sound/voicegroups/next.inc\"\n");
    write_text(rootC,
               "sound/voicegroups/main.inc",
               "voice_group main\n"
               "\tvoice_keysplit_all voicegroup_short\n");
    write_text(rootC,
               "sound/voicegroups/short.inc",
               "short::\n"
               "\tvoice_directsound 60, 0, ShortWave, 1, 2, 3, 4\n");
    write_text(rootC,
               "sound/voicegroups/next.inc",
               "next::\n"
               "\tvoice_directsound 62, 3, NextWave, 5, 6, 7, 8\n"
               "\tvoice_keysplit_all voicegroup_short\n");
    write_text(rootC,
               "sound/direct_sound_data.inc",
               "ShortWave::\n\t.incbin \"sound/direct_sound_samples/short.bin\"\n"
               "NextWave::\n\t.incbin \"sound/direct_sound_samples/next.bin\"\n");
    write_text(rootC, "sound/direct_sound_synth_data.inc", "");
    write_text(rootC, "sound/programmable_wave_data.inc", "");
    write_text(rootC, "sound/keysplit_tables.inc", "");
    uint8_t shortSample[19] = {0};
    shortSample[5] = 0x04;
    shortSample[12] = 3;
    shortSample[16] = 0xa1;
    shortSample[17] = 0xa2;
    shortSample[18] = 0xa3;
    write_bytes(rootC, "sound/direct_sound_samples/short.bin", shortSample, sizeof(shortSample));
    uint8_t nextSample[19] = {0};
    nextSample[5] = 0x04;
    nextSample[12] = 3;
    nextSample[16] = 0xb1;
    nextSample[17] = 0xb2;
    nextSample[18] = 0xb3;
    write_bytes(rootC, "sound/direct_sound_samples/next.bin", nextSample, sizeof(nextSample));

    VoicegroupProject* contiguousProject = voicegroup_project_open(rootC, strlen(rootC));
    CHECK(contiguousProject);
    VoicegroupProjectResult contiguousSnapshot = {0};
    voicegroup_project_refresh(contiguousProject, &contiguousSnapshot);
    CHECK(contiguousSnapshot.succeeded && contiguousSnapshot.diagnostic_count == 0);
    VoicegroupLoadRequest contiguousRequest = {
        .mode = VG_LOAD_SAVED,
        .bank_name = "main",
        .bank_name_len = 4,
    };
    VoicegroupLoadResult contiguousLoad = voicegroup_project_load(contiguousProject, &contiguousRequest);
    CHECK(contiguousLoad.succeeded && contiguousLoad.diagnostic_count == 0);
    LoadedVoiceGroup* contiguousBank = voicegroup_load_result_take(&contiguousLoad);
    CHECK(contiguousBank);
    CHECK(contiguousBank->voices[0].type == VOICE_KEYSPLIT_ALL);
    CHECK(contiguousBank->voices[0].subGroup && contiguousBank->subGroupCount == 1);
    ToneData* contiguousSubgroup = (ToneData*)contiguousBank->voices[0].subGroup;
    CHECK(contiguousSubgroup[0].type == VOICE_DIRECTSOUND);
    CHECK(contiguousSubgroup[0].key == 60 && contiguousSubgroup[0].panSweep == 0);
    CHECK(contiguousSubgroup[0].attack == 1 && contiguousSubgroup[0].decay == 2 && contiguousSubgroup[0].sustain == 3 &&
          contiguousSubgroup[0].release == 4);
    CHECK(contiguousSubgroup[0].wav == contiguousBank->waveDatas[0]);
    CHECK(contiguousSubgroup[0].wav->size == 3);
    CHECK(memcmp(contiguousSubgroup[0].wav->data, "\xa1\xa2\xa3", 3) == 0);
    CHECK(contiguousSubgroup[1].type == VOICE_DIRECTSOUND);
    CHECK(contiguousSubgroup[1].key == 62 && contiguousSubgroup[1].panSweep == 0x83);
    CHECK(contiguousSubgroup[1].attack == 5 && contiguousSubgroup[1].decay == 6 && contiguousSubgroup[1].sustain == 7 &&
          contiguousSubgroup[1].release == 8);
    CHECK(contiguousSubgroup[1].wav == contiguousBank->waveDatas[1]);
    CHECK(contiguousSubgroup[1].wav->size == 3);
    CHECK(memcmp(contiguousSubgroup[1].wav->data, "\xb1\xb2\xb3", 3) == 0);
    CHECK(contiguousSubgroup[2].type == VOICE_KEYSPLIT_ALL);
    CHECK(contiguousSubgroup[2].subGroup == NULL && contiguousSubgroup[2].keySplitTable == NULL);
    voicegroup_load_result_free(&contiguousLoad);
    voicegroup_free(contiguousBank);
    voicegroup_project_result_free(&contiguousSnapshot);
    voicegroup_project_free(contiguousProject);
    remove_file(root, "sound/voice_groups.inc");
    remove_file(root, "sound/direct_sound_data.inc");
    remove_file(root, "sound/direct_sound_synth_data.inc");
    remove_file(root, "sound/programmable_wave_data.inc");
    remove_file(root, "sound/keysplit_tables.inc");
    remove_file(root, "sound/direct_sound_samples/kick.bin");
    remove_file(root, "sound/direct_sound_samples/aiff.bin");
    remove_file(root, "sound/direct_sound_samples/aiff.aif");
    remove_file(root, "sound/direct_sound_samples/wav.bin");
    remove_file(root, "sound/direct_sound_samples/wav.wav");
    remove_file(root, "sound/direct_sound_samples/wav.aif");
    remove_file(root, "sound/direct_sound_samples/zero_bin_synth.bin");
    remove_file(root, "sound/direct_sound_samples/zero_wav.wav");
    remove_file(root, "sound/direct_sound_samples/zero_aif.aif");
    remove_file(root, "sound/programmable_wave_samples/wave.pcm");
    remove_file(root, "asm/macros/music_voice.inc");
    CHECK(rmdir(macros) == 0);
    CHECK(rmdir(asmRoot) == 0);
    CHECK(rmdir(direct) == 0);
    CHECK(rmdir(prog) == 0);
    CHECK(rmdir(sound) == 0);
    CHECK(rmdir(root) == 0);
    remove_file(rootB, "sound/voice_groups.inc");
    remove_file(rootB, "sound/direct_sound_data.inc");
    remove_file(rootB, "sound/programmable_wave_data.inc");
    remove_file(rootB, "sound/keysplit_tables.inc");
    remove_file(rootB, "sound/direct_sound_samples/other.bin");
    CHECK(rmdir(directB) == 0);
    CHECK(rmdir(soundB) == 0);
    CHECK(rmdir(rootB) == 0);
    remove_file(rootC, "sound/voice_groups.inc");
    remove_file(rootC, "sound/voicegroups/main.inc");
    remove_file(rootC, "sound/voicegroups/short.inc");
    remove_file(rootC, "sound/voicegroups/next.inc");
    remove_file(rootC, "sound/direct_sound_data.inc");
    remove_file(rootC, "sound/direct_sound_synth_data.inc");
    remove_file(rootC, "sound/programmable_wave_data.inc");
    remove_file(rootC, "sound/keysplit_tables.inc");
    remove_file(rootC, "sound/direct_sound_samples/short.bin");
    remove_file(rootC, "sound/direct_sound_samples/next.bin");
    CHECK(rmdir(directC) == 0);
    CHECK(rmdir(voiceDirC) == 0);
    CHECK(rmdir(soundC) == 0);
    CHECK(rmdir(rootC) == 0);
    return 0;
}
#endif
