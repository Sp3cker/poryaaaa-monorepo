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

static bool bank_result_has_error(const VoicegroupCoreBankResult* result)
{
    return result && voicegroup_core_bank_result_diagnostic_count(result) != 0;
}

static void report_bank_result_error(const VoicegroupCoreBankResult* result, const char* bankName)
{
    char code[128] = {0};
    char message[512] = {0};
    if (result && voicegroup_core_bank_result_diagnostic_count(result) > 0)
    {
        voicegroup_core_bank_result_diagnostic_code(result, 0, code, sizeof(code));
        voicegroup_core_bank_result_diagnostic_message(result, 0, message, sizeof(message));
    }
    if (message[0])
        vg_err("%s: %s", code[0] ? code : "voicegroup diagnostic", message);
    else
        vg_err("voicegroup '%s' has blocking diagnostics", bankName ? bankName : "");
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

    VoicegroupCoreProjectIndex* index = NULL;
    if (voicegroup_core_project_index_load(projectRoot, &index) != VOICEGROUP_CORE_STATUS_OK || !index)
    {
        vg_err("voicegroup-core failed to index project: %s", projectRoot);
        return NULL;
    }

    VoicegroupCoreBankResult* result = NULL;
    if (voicegroup_core_project_index_load_program_bank(index, voicegroupName, &result) != VOICEGROUP_CORE_STATUS_OK ||
        !result)
    {
        vg_err("voicegroup-core failed to load voicegroup '%s'", voicegroupName);
        voicegroup_core_project_index_free(index);
        return NULL;
    }
    if (!voicegroup_core_bank_result_has_bank(result) || bank_result_has_error(result))
    {
        report_bank_result_error(result, voicegroupName);
        voicegroup_core_bank_result_free(result);
        voicegroup_core_project_index_free(index);
        return NULL;
    }

    LoadedVoiceGroup* vg = NULL;
    bool ok = voicegroup_materialize_core_bank(projectRoot, index, result, &vg, NULL);
    voicegroup_core_bank_result_free(result);
    voicegroup_core_project_index_free(index);
    if (!ok)
    {
        if (!voicegroup_loader_last_error()[0])
            vg_err("voicegroup materialization failed");
        return NULL;
    }
    vg_log("voicegroup_load: done OK");
    return vg;
}

ToneData* voicegroup_loaded_voices(LoadedVoiceGroup* vg)
{
    if (!vg)
        return NULL;
    return vg->voices;
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
