#include "voicegroup_loader.h"

#include "vg_alloc.h"
#include "vg_discovery.h"
#include "vg_keysplit.h"
#include "vg_log.h"
#include "vg_paths.h"
#include "vg_parser.h"
#include "vg_symbols.h"
#include "vg_wav.h"
#include "voicegroup_core.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define INITIAL_CAPACITY 64

typedef struct
{
    const char* projectRoot;
    const VoicegroupCoreProjectIndex* index;
    LoadedVoiceGroup* owner;
} CoreMaterializeContext;

static bool next_capacity(int current, int needed, int* out)
{
    int newCapacity = current ? current : INITIAL_CAPACITY;
    while (newCapacity < needed)
    {
        if (newCapacity > INT_MAX / 2)
            return false;
        newCapacity *= 2;
    }
    *out = newCapacity;
    return true;
}

static bool register_wavedata(LoadedVoiceGroup* vg, WaveData* wd)
{
    if (vg->waveDataCount >= vg->waveDataCapacity)
    {
        int newCapacity;
        if (!next_capacity(vg->waveDataCapacity, vg->waveDataCount + 1, &newCapacity))
            return false;
        WaveData** items = vg_realloc_array(vg->waveDatas, (size_t)newCapacity, sizeof(*vg->waveDatas));
        if (!items)
            return false;
        vg->waveDatas = items;
        vg->waveDataCapacity = newCapacity;
    }
    vg->waveDatas[vg->waveDataCount++] = wd;
    return true;
}

static bool register_subgroup(LoadedVoiceGroup* vg, ToneData* subgroup)
{
    if (vg->subGroupCount >= vg->subGroupCapacity)
    {
        int newCapacity;
        if (!next_capacity(vg->subGroupCapacity, vg->subGroupCount + 1, &newCapacity))
            return false;
        ToneData** items = vg_realloc_array(vg->subGroups, (size_t)newCapacity, sizeof(*vg->subGroups));
        if (!items)
            return false;
        vg->subGroups = items;
        vg->subGroupCapacity = newCapacity;
    }
    vg->subGroups[vg->subGroupCount++] = subgroup;
    return true;
}

static bool register_keysplit_table(LoadedVoiceGroup* vg, uint8_t* table)
{
    if (vg->keySplitTableCount >= vg->keySplitTableCapacity)
    {
        int newCapacity;
        if (!next_capacity(vg->keySplitTableCapacity, vg->keySplitTableCount + 1, &newCapacity))
            return false;
        uint8_t** items = vg_realloc_array(vg->keySplitTables, (size_t)newCapacity, sizeof(*vg->keySplitTables));
        if (!items)
            return false;
        vg->keySplitTables = items;
        vg->keySplitTableCapacity = newCapacity;
    }
    vg->keySplitTables[vg->keySplitTableCount++] = table;
    return true;
}

static void read_core_display_name(const VoicegroupCoreBankResult* result, size_t slot, char* buffer, size_t bufferLen)
{
    if (bufferLen == 0)
        return;
    buffer[0] = '\0';
    voicegroup_core_bank_result_program_display_name(result, slot, buffer, bufferLen);
}

static void read_core_relative_path(const VoicegroupCoreBankResult* result, size_t slot, char* buffer, size_t bufferLen)
{
    if (bufferLen == 0)
        return;
    buffer[0] = '\0';
    voicegroup_core_bank_result_program_relative_path(result, slot, buffer, bufferLen);
}

static void
read_core_sub_voicegroup(const VoicegroupCoreBankResult* result, size_t slot, char* buffer, size_t bufferLen)
{
    if (bufferLen == 0)
        return;
    buffer[0] = '\0';
    voicegroup_core_bank_result_program_sub_voicegroup(result, slot, buffer, bufferLen);
}

static void set_slot_name(char names[VOICEGROUP_SIZE][VG_MAX_VOICE_SAMPLE_NAME], size_t slot, const char* name)
{
    if (!names || slot >= VOICEGROUP_SIZE || !name)
        return;
    strncpy(names[slot], name, VG_MAX_VOICE_SAMPLE_NAME - 1);
    names[slot][VG_MAX_VOICE_SAMPLE_NAME - 1] = '\0';
}

static bool bank_result_has_error(const VoicegroupCoreBankResult* result)
{
    size_t count = voicegroup_core_bank_result_diagnostic_count(result);
    for (size_t i = 0; i < count; i++)
    {
        if (voicegroup_core_bank_result_diagnostic_severity(result, i) == VOICEGROUP_CORE_DIAGNOSTIC_SEVERITY_ERROR)
            return true;
    }
    return false;
}

static void report_bank_result_error(const VoicegroupCoreBankResult* result, const char* fallback)
{
    char message[512];
    message[0] = '\0';
    size_t count = result ? voicegroup_core_bank_result_diagnostic_count(result) : 0;
    for (size_t i = 0; i < count; i++)
    {
        if (voicegroup_core_bank_result_diagnostic_severity(result, i) == VOICEGROUP_CORE_DIAGNOSTIC_SEVERITY_ERROR)
        {
            voicegroup_core_bank_result_diagnostic_message(result, i, message, sizeof(message));
            break;
        }
    }
    vg_err("%s", message[0] ? message : fallback);
}

static bool materialize_core_bank(CoreMaterializeContext* ctx,
                                  const VoicegroupCoreBankResult* result,
                                  ToneData voices[VOICEGROUP_SIZE],
                                  char names[VOICEGROUP_SIZE][VG_MAX_VOICE_SAMPLE_NAME]);

static bool load_core_subgroup(CoreMaterializeContext* ctx, const char* subVoicegroup, ToneData** outSubgroup)
{
    *outSubgroup = NULL;
    VoicegroupCoreBankResult* result = NULL;
    if (voicegroup_core_project_index_load_program_bank(ctx->index, subVoicegroup, &result) !=
            VOICEGROUP_CORE_STATUS_OK ||
        !result)
    {
        vg_err("voicegroup-core failed to load sub-voicegroup '%s'", subVoicegroup);
        return false;
    }
    if (!voicegroup_core_bank_result_has_bank(result) || bank_result_has_error(result))
    {
        report_bank_result_error(result, "sub-voicegroup could not be loaded");
        voicegroup_core_bank_result_free(result);
        return false;
    }

    ToneData* subgroup = vg_malloc_array(VOICEGROUP_SIZE, sizeof(ToneData));
    if (!subgroup)
    {
        voicegroup_core_bank_result_free(result);
        return false;
    }
    memset(subgroup, 0, sizeof(ToneData) * VOICEGROUP_SIZE);

    if (!materialize_core_bank(ctx, result, subgroup, NULL))
    {
        free(subgroup);
        voicegroup_core_bank_result_free(result);
        return false;
    }
    voicegroup_core_bank_result_free(result);

    if (!register_subgroup(ctx->owner, subgroup))
    {
        free(subgroup);
        return false;
    }
    *outSubgroup = subgroup;
    return true;
}

static void materialize_directsound(CoreMaterializeContext* ctx,
                                    const VoicegroupCoreBankResult* result,
                                    size_t slot,
                                    ToneData* voice)
{
    VoicegroupCoreDirectSoundProgram program = {0};
    if (!voicegroup_core_bank_result_program_direct_sound(result, slot, &program))
        return;

    voice->type = voicegroup_core_bank_result_program_type_code(result, slot);
    voice->key = program.key;
    voice->panSweep = program.pan ? (uint8_t)(0x80 | program.pan) : 0;
    voice->attack = program.attack;
    voice->decay = program.decay;
    voice->sustain = program.sustain;
    voice->release = program.release;

    char relPath[VG_MAX_PATH_LEN];
    read_core_relative_path(result, slot, relPath, sizeof(relPath));
    if (relPath[0])
        voice->wav = voicegroup_loader_load_sample(ctx->projectRoot, relPath, ctx->owner);
}

static void materialize_programmable_wave(CoreMaterializeContext* ctx,
                                          const VoicegroupCoreBankResult* result,
                                          size_t slot,
                                          ToneData* voice)
{
    VoicegroupCoreProgrammableWaveProgram program = {0};
    if (!voicegroup_core_bank_result_program_programmable_wave(result, slot, &program))
        return;

    voice->type = voicegroup_core_bank_result_program_type_code(result, slot);
    voice->key = program.key;
    voice->panSweep = program.pan ? (uint8_t)(0x80 | program.pan) : 0;
    voice->attack = program.attack;
    voice->decay = program.decay;
    voice->sustain = program.sustain;
    voice->release = program.release;

    char relPath[VG_MAX_PATH_LEN];
    read_core_relative_path(result, slot, relPath, sizeof(relPath));
    if (relPath[0])
        voice->wavePointer = voicegroup_loader_load_prog_wave(ctx->projectRoot, relPath, ctx->owner);
}

static void materialize_square1(const VoicegroupCoreBankResult* result, size_t slot, ToneData* voice)
{
    VoicegroupCoreSquare1Program program = {0};
    if (!voicegroup_core_bank_result_program_square1(result, slot, &program))
        return;
    voice->type = voicegroup_core_bank_result_program_type_code(result, slot);
    voice->key = program.key;
    voice->panSweep = program.sweep;
    voice->wavePointer = (uint32_t*)(uintptr_t)program.duty;
    voice->attack = program.attack;
    voice->decay = program.decay;
    voice->sustain = program.sustain;
    voice->release = program.release;
}

static void materialize_square2(const VoicegroupCoreBankResult* result, size_t slot, ToneData* voice)
{
    VoicegroupCoreSquare2Program program = {0};
    if (!voicegroup_core_bank_result_program_square2(result, slot, &program))
        return;
    voice->type = voicegroup_core_bank_result_program_type_code(result, slot);
    voice->key = program.key;
    voice->panSweep = 0;
    voice->wavePointer = (uint32_t*)(uintptr_t)program.duty;
    voice->attack = program.attack;
    voice->decay = program.decay;
    voice->sustain = program.sustain;
    voice->release = program.release;
}

static void materialize_noise(const VoicegroupCoreBankResult* result, size_t slot, ToneData* voice)
{
    VoicegroupCoreNoiseProgram program = {0};
    if (!voicegroup_core_bank_result_program_noise(result, slot, &program))
        return;
    voice->type = voicegroup_core_bank_result_program_type_code(result, slot);
    voice->key = program.key;
    voice->wavePointer = (uint32_t*)(uintptr_t)program.period;
    voice->attack = program.attack;
    voice->decay = program.decay;
    voice->sustain = program.sustain;
    voice->release = program.release;
}

static bool
materialize_keysplit(CoreMaterializeContext* ctx, const VoicegroupCoreBankResult* result, size_t slot, ToneData* voice)
{
    VoicegroupCoreKeysplitProgram program = {{0}};
    if (!voicegroup_core_bank_result_program_keysplit(result, slot, &program))
        return false;

    char subVoicegroup[256];
    read_core_sub_voicegroup(result, slot, subVoicegroup, sizeof(subVoicegroup));

    voice->type = voicegroup_core_bank_result_program_type_code(result, slot);
    if (subVoicegroup[0] && !load_core_subgroup(ctx, subVoicegroup, (ToneData**)&voice->subGroup))
        return false;

    uint8_t* table = vg_malloc_array(128, sizeof(*table));
    if (!table)
        return false;
    memcpy(table, program.table, 128);
    if (!register_keysplit_table(ctx->owner, table))
    {
        free(table);
        return false;
    }
    voice->keySplitTable = table;
    return true;
}

static bool materialize_keysplit_all(CoreMaterializeContext* ctx,
                                     const VoicegroupCoreBankResult* result,
                                     size_t slot,
                                     ToneData* voice)
{
    char subVoicegroup[256];
    read_core_sub_voicegroup(result, slot, subVoicegroup, sizeof(subVoicegroup));
    voice->type = voicegroup_core_bank_result_program_type_code(result, slot);
    if (subVoicegroup[0] && !load_core_subgroup(ctx, subVoicegroup, (ToneData**)&voice->subGroup))
        return false;
    return true;
}

static bool
materialize_cry(CoreMaterializeContext* ctx, const VoicegroupCoreBankResult* result, size_t slot, ToneData* voice)
{
    voice->type = voicegroup_core_bank_result_program_type_code(result, slot);
    voice->key = 60;
    voice->attack = 0xFF;
    voice->decay = 0;
    voice->sustain = 0xFF;
    voice->release = 0;

    char relPath[VG_MAX_PATH_LEN];
    read_core_relative_path(result, slot, relPath, sizeof(relPath));
    if (!relPath[0])
        return true;

    WaveData* wd = vg_load_bin_sample(ctx->projectRoot, relPath);
    if (!wd)
        return true;
    if (!register_wavedata(ctx->owner, wd))
    {
        free(wd);
        return false;
    }
    voice->wav = wd;
    return true;
}

static bool materialize_core_bank(CoreMaterializeContext* ctx,
                                  const VoicegroupCoreBankResult* result,
                                  ToneData voices[VOICEGROUP_SIZE],
                                  char names[VOICEGROUP_SIZE][VG_MAX_VOICE_SAMPLE_NAME])
{
    for (size_t slot = 0; slot < VOICEGROUP_SIZE; slot++)
    {
        ToneData* voice = &voices[slot];
        char displayName[VG_MAX_VOICE_SAMPLE_NAME];
        read_core_display_name(result, slot, displayName, sizeof(displayName));
        set_slot_name(names, slot, displayName);

        switch (voicegroup_core_bank_result_program_kind(result, slot))
        {
        case VOICEGROUP_CORE_PROGRAM_KIND_EMPTY:
            break;
        case VOICEGROUP_CORE_PROGRAM_KIND_DIRECT_SOUND:
            materialize_directsound(ctx, result, slot, voice);
            break;
        case VOICEGROUP_CORE_PROGRAM_KIND_PROGRAMMABLE_WAVE:
            materialize_programmable_wave(ctx, result, slot, voice);
            break;
        case VOICEGROUP_CORE_PROGRAM_KIND_SQUARE1:
            materialize_square1(result, slot, voice);
            break;
        case VOICEGROUP_CORE_PROGRAM_KIND_SQUARE2:
            materialize_square2(result, slot, voice);
            break;
        case VOICEGROUP_CORE_PROGRAM_KIND_NOISE:
            materialize_noise(result, slot, voice);
            break;
        case VOICEGROUP_CORE_PROGRAM_KIND_KEYSPLIT:
            if (!materialize_keysplit(ctx, result, slot, voice))
                return false;
            break;
        case VOICEGROUP_CORE_PROGRAM_KIND_KEYSPLIT_ALL:
            if (!materialize_keysplit_all(ctx, result, slot, voice))
                return false;
            break;
        case VOICEGROUP_CORE_PROGRAM_KIND_CRY:
            if (!materialize_cry(ctx, result, slot, voice))
                return false;
            break;
        }
    }
    return true;
}

/*
 * Load a checked program bank through voicegroup-core, then materialize
 * poryaaaa-owned ToneData, samples, programmable waves, keysplit tables, and
 * subgroups. Rust owns project discovery and voicegroup syntax; C owns audio
 * runtime allocations and voicegroup_free() lifetime.
 */
LoadedVoiceGroup* voicegroup_load(const char* projectRoot, const char* voicegroupName)
{
    vg_clear_error();
    vg_log("voicegroup_load: start root='%s' vg='%s'",
           projectRoot ? projectRoot : "(null)",
           voicegroupName ? voicegroupName : "(null)");

    if (!projectRoot || !voicegroupName)
    {
        vg_err("voicegroup load requires project root and voicegroup name");
        return NULL;
    }
    if (!vg_is_directory(projectRoot))
    {
        vg_err("Bad project root: does not exist or is not a directory: %s", projectRoot);
        return NULL;
    }

    char expectedPath[VG_MAX_PATH_LEN];
    vg_build_path(expectedPath, sizeof(expectedPath), projectRoot, "sound/voice_groups.inc");
    if (!vg_file_exists(expectedPath))
    {
        vg_err("Bad project root: missing sound/voice_groups.inc: %s", expectedPath);
        return NULL;
    }

    LoadedVoiceGroup* vg = calloc(1, sizeof(LoadedVoiceGroup));
    if (!vg)
        return NULL;

    VoicegroupCoreProjectIndex* index = NULL;
    if (voicegroup_core_project_index_load(projectRoot, &index) != VOICEGROUP_CORE_STATUS_OK || !index)
    {
        vg_err("voicegroup-core failed to index project: %s", projectRoot);
        voicegroup_free(vg);
        return NULL;
    }

    VoicegroupCoreBankResult* result = NULL;
    if (voicegroup_core_project_index_load_program_bank(index, voicegroupName, &result) != VOICEGROUP_CORE_STATUS_OK ||
        !result)
    {
        vg_err("voicegroup-core failed to load voicegroup '%s'", voicegroupName);
        voicegroup_core_project_index_free(index);
        voicegroup_free(vg);
        return NULL;
    }
    if (!voicegroup_core_bank_result_has_bank(result) || bank_result_has_error(result))
    {
        report_bank_result_error(result, "voicegroup could not be loaded");
        voicegroup_core_bank_result_free(result);
        voicegroup_core_project_index_free(index);
        voicegroup_free(vg);
        return NULL;
    }

    CoreMaterializeContext ctx = {
        .projectRoot = projectRoot,
        .index = index,
        .owner = vg,
    };
    bool ok = materialize_core_bank(&ctx, result, vg->voices, vg->voiceSampleNames);

    voicegroup_core_bank_result_free(result);
    voicegroup_core_project_index_free(index);

    if (!ok)
    {
        if (!voicegroup_loader_last_error()[0])
            vg_err("voicegroup materialization failed");
        voicegroup_free(vg);
        return NULL;
    }
    vg_log("voicegroup_load: done OK");
    return vg;
}

void voicegroup_free(LoadedVoiceGroup* vg)
{
    if (!vg)
        return;

    for (int i = 0; i < vg->waveDataCount; i++)
        free(vg->waveDatas[i]);
    free(vg->waveDatas);

    for (int i = 0; i < vg->progWaveCount; i++)
        free(vg->progWaves[i]);
    free(vg->progWaves);

    for (int i = 0; i < vg->subGroupCount; i++)
        free(vg->subGroups[i]);
    free(vg->subGroups);

    for (int i = 0; i < vg->keySplitTableCount; i++)
        free(vg->keySplitTables[i]);
    free(vg->keySplitTables);

    free(vg);
}

/* ---- Project asset collection ---- */

static void fill_asset_entry(ProjectAssetEntry* out, ProjectAssetKind kind, const SymbolMapping* src)
{
    out->kind = kind;
    strncpy(out->symbol, src->symbol, sizeof(out->symbol) - 1);
    strncpy(out->relPath, src->filePath, sizeof(out->relPath) - 1);
    strncpy(out->fileName, vg_path_basename(src->filePath), sizeof(out->fileName) - 1);
}

static bool build_asset_array(const SymbolMap* map, ProjectAssetKind kind, ProjectAssetEntry** outArray, int* outCount)
{
    if (map->count <= 0)
        return true;
    ProjectAssetEntry* arr = vg_malloc_array((size_t)map->count, sizeof(ProjectAssetEntry));
    if (!arr)
        return false;
    memset(arr, 0, sizeof(ProjectAssetEntry) * (size_t)map->count);
    for (int i = 0; i < map->count; i++)
        fill_asset_entry(&arr[i], kind, &map->entries[i]);
    *outArray = arr;
    *outCount = map->count;
    return true;
}

bool voicegroup_loader_collect_project_assets(const char* projectRoot, VoicegroupProjectAssets* out)
{
    memset(out, 0, sizeof(*out));

    ProjectDiscovery* disc = calloc(1, sizeof(ProjectDiscovery));
    if (!disc)
        return false;
    vg_discover_project(projectRoot, disc);

    SymbolMap dsMap, pwMap;
    vg_symbol_map_init(&dsMap);
    vg_symbol_map_init(&pwMap);
    bool ok = vg_parse_direct_sound_data(disc, &dsMap) && vg_parse_prog_wave_data(disc, &pwMap);

    if (ok)
        ok = build_asset_array(&dsMap, PROJECT_ASSET_DIRECTSOUND, &out->directsound, &out->directsoundCount);
    if (ok)
        ok = build_asset_array(&pwMap, PROJECT_ASSET_PROG_WAVE, &out->progWave, &out->progWaveCount);

    vg_symbol_map_free(&dsMap);
    vg_symbol_map_free(&pwMap);
    free(disc);
    if (!ok)
        voicegroup_loader_free_project_assets(out);
    return ok;
}

void voicegroup_loader_free_project_assets(VoicegroupProjectAssets* assets)
{
    free(assets->directsound);
    free(assets->progWave);
    memset(assets, 0, sizeof(*assets));
}
