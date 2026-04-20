#include "voicegroup_loader.h"

#include "vg_discovery.h"
#include "vg_keysplit.h"
#include "vg_log.h"
#include "vg_parser.h"
#include "vg_symbols.h"

#include <stdlib.h>
#include <string.h>

/*
 * High-level orchestrator.
 *
 * Steps:
 *   1. Walk the project tree (vg_discovery).
 *   2. Parse every discovered direct_sound_data.inc,
 *      programmable_wave_data.inc, keysplit_tables.inc.
 *   3. Find the named voicegroup and parse it (vg_parser), populating
 *      the returned LoadedVoiceGroup's voices[] plus its owned
 *      sample/prog-wave/sub-voicegroup/keysplit-table arrays.
 *
 * Each step is one module call. This function exists to own the
 * temporary state (maps, discovery) that all three steps share, and
 * to free it in the right order.
 */
LoadedVoiceGroup *voicegroup_load(const char *projectRoot, const char *voicegroupName,
                                  const VoicegroupLoaderConfig *config)
{
    vg_log("voicegroup_load: start root='%s' vg='%s'", projectRoot, voicegroupName);

    LoadedVoiceGroup *vg = calloc(1, sizeof(LoadedVoiceGroup));
    if (!vg) return NULL;

    /* ProjectDiscovery is ~96 KB — keep it off the stack so we don't
     * risk overflow on Windows hosts where Reaper's plugin-load
     * thread has a 1 MB stack. */
    ProjectDiscovery *disc = calloc(1, sizeof(ProjectDiscovery));
    if (!disc) {
        voicegroup_free(vg);
        return NULL;
    }

    vg_discover_project(projectRoot, config, disc);
    vg_log("voicegroup_load: discover done - dsFiles=%d pwFiles=%d ksFiles=%d vgDirs=%d monoFiles=%d",
           disc->directSoundDataFiles.count, disc->progWaveDataFiles.count,
           disc->keySplitTableFiles.count, disc->voicegroupDirs.count,
           disc->monolithicVGFiles.count);

    SymbolMap dsMap, pwMap;
    KeySplitMap ksMap;
    vg_symbol_map_init(&dsMap);
    vg_symbol_map_init(&pwMap);
    vg_keysplit_map_init(&ksMap);

    vg_parse_direct_sound_data(disc, &dsMap);
    vg_parse_prog_wave_data(disc, &pwMap);
    vg_parse_keysplit_tables(disc, &ksMap);
    vg_log("voicegroup_load: symbol maps - ds=%d pw=%d ks=%d",
           dsMap.count, pwMap.count, ksMap.count);

    int rc = vg_parse_voicegroup(projectRoot, voicegroupName, vg,
                                 &dsMap, &pwMap, &ksMap, disc);

    vg_symbol_map_free(&dsMap);
    vg_symbol_map_free(&pwMap);
    vg_keysplit_map_free(&ksMap);
    free(disc);

    if (rc != 0) {
        voicegroup_free(vg);
        return NULL;
    }
    vg_log("voicegroup_load: done OK");
    return vg;
}

void voicegroup_free(LoadedVoiceGroup *vg)
{
    if (!vg) return;

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

static void fill_asset_entry(ProjectAssetEntry *out, ProjectAssetKind kind,
                             const SymbolMapping *src)
{
    out->kind = kind;
    strncpy(out->symbol, src->symbol, sizeof(out->symbol) - 1);
    strncpy(out->relPath, src->filePath, sizeof(out->relPath) - 1);
    strncpy(out->fileName, vg_path_basename(src->filePath), sizeof(out->fileName) - 1);
}

static void build_asset_array(const SymbolMap *map, ProjectAssetKind kind,
                              ProjectAssetEntry **outArray, int *outCount)
{
    if (map->count <= 0) return;
    ProjectAssetEntry *arr = calloc((size_t)map->count, sizeof(ProjectAssetEntry));
    if (!arr) return;
    for (int i = 0; i < map->count; i++)
        fill_asset_entry(&arr[i], kind, &map->entries[i]);
    *outArray = arr;
    *outCount = map->count;
}

bool voicegroup_loader_collect_project_assets(const char *projectRoot,
                                              const VoicegroupLoaderConfig *config,
                                              VoicegroupProjectAssets *out)
{
    memset(out, 0, sizeof(*out));

    ProjectDiscovery *disc = calloc(1, sizeof(ProjectDiscovery));
    if (!disc) return false;
    vg_discover_project(projectRoot, config, disc);

    SymbolMap dsMap, pwMap;
    vg_symbol_map_init(&dsMap);
    vg_symbol_map_init(&pwMap);
    vg_parse_direct_sound_data(disc, &dsMap);
    vg_parse_prog_wave_data(disc, &pwMap);

    build_asset_array(&dsMap, PROJECT_ASSET_DIRECTSOUND,
                      &out->directsound, &out->directsoundCount);
    build_asset_array(&pwMap, PROJECT_ASSET_PROG_WAVE,
                      &out->progWave, &out->progWaveCount);

    vg_symbol_map_free(&dsMap);
    vg_symbol_map_free(&pwMap);
    free(disc);
    return true;
}

void voicegroup_loader_free_project_assets(VoicegroupProjectAssets *assets)
{
    free(assets->directsound);
    free(assets->progWave);
    memset(assets, 0, sizeof(*assets));
}
