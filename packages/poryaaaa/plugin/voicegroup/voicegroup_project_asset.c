#include "voicegroup_project_internal.h"

#include "vg_wav.h"

#include <stdlib.h>
#include <string.h>

static const VoicegroupCatalogEntry*
find_catalog_entry(const VoicegroupProject* project, VoicegroupAssetKind kind, const char* symbol)
{
    if (!project || !project->generation || !symbol)
        return NULL;
    const VoicegroupProjectResult* snapshot = &project->generation->snapshot->view;
    for (size_t i = 0; i < snapshot->catalog_count; i++)
    {
        const VoicegroupCatalogEntry* entry = &snapshot->catalog[i];
        bool matches = false;
        if (kind == VG_ASSET_DIRECT_SOUND)
            matches = entry->kind == VOICEGROUP_CORE_CATALOG_ENTRY_KIND_DIRECT_SOUND ||
                      entry->kind == VOICEGROUP_CORE_CATALOG_ENTRY_KIND_SYNTH;
        else if (kind == VG_ASSET_PROG_WAVE)
            matches = entry->kind == VOICEGROUP_CORE_CATALOG_ENTRY_KIND_PROGRAMMABLE_WAVE;
        else if (kind == VG_ASSET_KEYSPLIT)
            matches = entry->kind == VOICEGROUP_CORE_CATALOG_ENTRY_KIND_KEYSPLIT;
        if (matches && strcmp(entry->symbol, symbol) == 0)
            return entry;
    }
    return NULL;
}

static void asset_set_failure(AssetResultStorage* storage, const char* code, const char* message, const char* assetPath)
{
    VoicegroupDiagnostic* diagnostic = arena_alloc(&storage->arena, sizeof(*diagnostic));
    if (!diagnostic)
        return;
    memset(diagnostic, 0, sizeof(*diagnostic));
    diagnostic->code = arena_copy_string(&storage->arena, code);
    diagnostic->message = arena_copy_string(&storage->arena, message);
    diagnostic->scope = 2;
    diagnostic->asset_path = arena_copy_string(&storage->arena, assetPath);
    storage->view.diagnostics = diagnostic;
    storage->view.diagnostic_count = 1;
}

static void asset_copy_project_failure(AssetResultStorage* storage, const VoicegroupProject* project)
{
    if (!project || !project->failure || project->failure->view.diagnostic_count == 0)
    {
        asset_set_failure(storage, "project.refresh_failed", "voicegroup project refresh failed", NULL);
        return;
    }
    size_t count = project->failure->view.diagnostic_count;
    VoicegroupDiagnostic* diagnostics = arena_alloc(&storage->arena, count * sizeof(*diagnostics));
    if (!diagnostics)
        return;
    for (size_t i = 0; i < count; i++)
        if (!copy_diagnostic(&storage->arena, &diagnostics[i], &project->failure->view.diagnostics[i]))
            return;
    storage->view.diagnostics = diagnostics;
    storage->view.diagnostic_count = count;
}

static void remap_tone_resources(ToneData* tone, const LoadedVoiceGroup* source, const LoadedVoiceGroup* destination)
{
    void* resource = tone->wav;
    for (int i = 0; i < source->waveDataCount; i++)
        if (resource == source->waveDatas[i])
        {
            tone->wav = destination->waveDatas[i];
            break;
        }
    for (int i = 0; i < source->progWaveCount; i++)
        if (resource == source->progWaves[i])
        {
            tone->wavePointer = destination->progWaves[i];
            break;
        }
    for (int i = 0; i < source->subGroupCount; i++)
        if (resource == source->subGroups[i])
        {
            tone->subGroup = destination->subGroups[i];
            break;
        }
    for (int i = 0; i < source->keySplitTableCount; i++)
        if (tone->keySplitTable == source->keySplitTables[i])
        {
            tone->keySplitTable = destination->keySplitTables[i];
            break;
        }
}

static LoadedVoiceGroup* clone_loaded_voicegroup(const LoadedVoiceGroup* source)
{
    if (!source || source->waveDataCount < 0 || source->progWaveCount < 0 || source->subGroupCount < 0 ||
        source->keySplitTableCount < 0)
        return NULL;
    LoadedVoiceGroup* destination = calloc(1, sizeof(*destination));
    if (!destination)
        return NULL;
    memcpy(destination->voices, source->voices, sizeof(destination->voices));
    memcpy(destination->voiceSampleNames, source->voiceSampleNames, sizeof(destination->voiceSampleNames));

    destination->waveDataCount = source->waveDataCount;
    destination->waveDataCapacity = source->waveDataCount;
    if (source->waveDataCount > 0)
    {
        destination->waveDatas = calloc((size_t)source->waveDataCount, sizeof(*destination->waveDatas));
        if (!destination->waveDatas)
            goto fail;
        for (int i = 0; i < source->waveDataCount; i++)
        {
            const WaveData* wave = source->waveDatas[i];
            if (!wave)
                continue;
            size_t dataBytes = wave->size > 0 ? (size_t)wave->size + 1 : 16;
            if (dataBytes > SIZE_MAX - sizeof(*wave))
                goto fail;
            WaveData* copy = malloc(sizeof(*copy) + dataBytes);
            if (!copy)
                goto fail;
            *copy = *wave;
            copy->data = (int8_t*)(copy + 1);
            memcpy(copy->data, wave->data, dataBytes);
            destination->waveDatas[i] = copy;
        }
    }

    destination->progWaveCount = source->progWaveCount;
    destination->progWaveCapacity = source->progWaveCount;
    if (source->progWaveCount > 0)
    {
        destination->progWaves = calloc((size_t)source->progWaveCount, sizeof(*destination->progWaves));
        if (!destination->progWaves)
            goto fail;
        for (int i = 0; i < source->progWaveCount; i++)
        {
            if (!source->progWaves[i])
                continue;
            destination->progWaves[i] = malloc(16);
            if (!destination->progWaves[i])
                goto fail;
            memcpy(destination->progWaves[i], source->progWaves[i], 16);
        }
    }

    destination->subGroupCount = source->subGroupCount;
    destination->subGroupCapacity = source->subGroupCount;
    if (source->subGroupCount > 0)
    {
        destination->subGroups = calloc((size_t)source->subGroupCount, sizeof(*destination->subGroups));
        if (!destination->subGroups)
            goto fail;
        for (int i = 0; i < source->subGroupCount; i++)
        {
            if (!source->subGroups[i])
                continue;
            destination->subGroups[i] = malloc(VOICEGROUP_SIZE * sizeof(ToneData));
            if (!destination->subGroups[i])
                goto fail;
            memcpy(destination->subGroups[i], source->subGroups[i], VOICEGROUP_SIZE * sizeof(ToneData));
        }
    }

    destination->keySplitTableCount = source->keySplitTableCount;
    destination->keySplitTableCapacity = source->keySplitTableCount;
    if (source->keySplitTableCount > 0)
    {
        destination->keySplitTables = calloc((size_t)source->keySplitTableCount, sizeof(*destination->keySplitTables));
        if (!destination->keySplitTables)
            goto fail;
        for (int i = 0; i < source->keySplitTableCount; i++)
        {
            if (!source->keySplitTables[i])
                continue;
            destination->keySplitTables[i] = malloc(VOICEGROUP_SIZE);
            if (!destination->keySplitTables[i])
                goto fail;
            memcpy(destination->keySplitTables[i], source->keySplitTables[i], VOICEGROUP_SIZE);
        }
    }

    for (size_t i = 0; i < VOICEGROUP_SIZE; i++)
        remap_tone_resources(&destination->voices[i], source, destination);
    for (int group = 0; group < destination->subGroupCount; group++)
        if (destination->subGroups[group])
            for (size_t slot = 0; slot < VOICEGROUP_SIZE; slot++)
                remap_tone_resources(&destination->subGroups[group][slot], source, destination);
    return destination;

fail:
    voicegroup_free(destination);
    return NULL;
}

static bool copy_asset_result(AssetResultStorage* destination, const AssetResultStorage* source)
{
    destination->view.has_loop = source->view.has_loop;
    destination->view.loop_start = source->view.loop_start;
    destination->view.loop_length = source->view.loop_length;
    destination->view.sample_rate = source->view.sample_rate;
    destination->view.frame_count = source->view.frame_count;
    if (source->view.payload_len > 0)
    {
        void* payload = arena_alloc(&destination->arena, source->view.payload_len);
        if (!payload)
            return false;
        memcpy(payload, source->view.payload, source->view.payload_len);
        destination->view.payload = payload;
        destination->view.payload_len = source->view.payload_len;
    }
    if (source->view.synth_desc)
    {
        uint8_t* descriptor = arena_alloc(&destination->arena, 6);
        if (!descriptor)
            return false;
        memcpy(descriptor, source->view.synth_desc, 6);
        destination->view.synth_desc = descriptor;
    }
    if (source->bank)
    {
        destination->bank = clone_loaded_voicegroup(source->bank);
        if (!destination->bank)
            return false;
        destination->view.keysplit.subgroup = destination->bank->voices;
        destination->view.keysplit.subgroup_count = source->view.keysplit.subgroup_count;
    }
    if (source->view.keysplit.table_count > 0)
    {
        uint8_t* table = arena_alloc(&destination->arena, source->view.keysplit.table_count);
        if (!table)
            return false;
        memcpy(table, source->view.keysplit.table, source->view.keysplit.table_count);
        destination->view.keysplit.table = table;
        destination->view.keysplit.table_count = source->view.keysplit.table_count;
    }
    return true;
}

static const AssetResultStorage*
generation_find_cached_asset(const ProjectGeneration* generation, VoicegroupAssetKind kind, const char* symbol)
{
    if (!generation)
        return NULL;
    for (size_t i = 0; i < generation->assetCacheCount; i++)
    {
        const AssetResultStorage* cached = generation->assetCache[i].storage;
        if (cached && cached->view.kind == (uint32_t)kind && strcmp(cached->view.symbol, symbol) == 0)
            return cached;
    }
    return NULL;
}

static void generation_store_asset(ProjectGeneration* generation, const AssetResultStorage* source)
{
    if (!generation || !source || source->view.diagnostic_count != 0)
        return;
    AssetResultStorage* copy =
        asset_storage_create((VoicegroupAssetKind)source->view.kind, source->view.symbol, strlen(source->view.symbol));
    if (!copy || !copy_asset_result(copy, source))
    {
        asset_storage_dispose(copy);
        return;
    }
    if (generation->assetCacheCount >= generation->assetCacheCapacity)
    {
        size_t capacity = generation->assetCacheCapacity ? generation->assetCacheCapacity * 2 : 16;
        if (capacity < generation->assetCacheCapacity || capacity > SIZE_MAX / sizeof(*generation->assetCache))
        {
            asset_storage_dispose(copy);
            return;
        }
        AssetCacheEntry* cache = realloc(generation->assetCache, capacity * sizeof(*cache));
        if (!cache)
        {
            asset_storage_dispose(copy);
            return;
        }
        generation->assetCache = cache;
        generation->assetCacheCapacity = capacity;
    }
    generation->assetCache[generation->assetCacheCount++].storage = copy;
}

void generation_clear_asset_cache(ProjectGeneration* generation)
{
    if (!generation)
        return;
    for (size_t i = 0; i < generation->assetCacheCount; i++)
        asset_storage_dispose(generation->assetCache[i].storage);
    free(generation->assetCache);
    generation->assetCache = NULL;
    generation->assetCacheCount = 0;
    generation->assetCacheCapacity = 0;
}

VoicegroupAssetResult voicegroup_project_load_asset(VoicegroupProject* project,
                                                    VoicegroupAssetKind kind,
                                                    const char* symbol,
                                                    size_t symbol_len)
{
    VoicegroupAssetResult empty = {0};
    AssetResultStorage* storage = asset_storage_create(kind, symbol, symbol_len);
    if (!storage)
        return empty;
    char* name = duplicate_bytes(symbol ? symbol : "", symbol ? symbol_len : 0);
    if (!name)
    {
        asset_storage_dispose(storage);
        return empty;
    }
    if (!project || !name[0] ||
        (kind != VG_ASSET_DIRECT_SOUND && kind != VG_ASSET_PROG_WAVE && kind != VG_ASSET_KEYSPLIT))
    {
        asset_set_failure(storage, "asset.invalid_request", "asset request is not valid", NULL);
        free(name);
        return storage->view;
    }
    if (project->state == PROJECT_REFRESH_FAILED)
    {
        asset_copy_project_failure(storage, project);
        free(name);
        return storage->view;
    }
    if (project->state == PROJECT_STALE && !ensure_generation(project))
    {
        asset_copy_project_failure(storage, project);
        free(name);
        return storage->view;
    }
    if (!project->generation)
    {
        asset_set_failure(storage, "project.no_generation", "voicegroup project has no retained index", NULL);
        free(name);
        return storage->view;
    }

    const VoicegroupCatalogEntry* entry = find_catalog_entry(project, kind, name);
    if (!entry)
    {
        asset_set_failure(storage, "asset.not_found", "asset symbol is not present in the project index", NULL);
        free(name);
        return storage->view;
    }

    const AssetResultStorage* cached = generation_find_cached_asset(project->generation, kind, name);
    if (cached)
    {
        if (!copy_asset_result(storage, cached))
            asset_set_failure(storage, "asset.out_of_memory", "cached asset could not be copied", entry->asset_path);
        free(name);
        return storage->view;
    }

    if (kind == VG_ASSET_DIRECT_SOUND && entry->has_synth)
    {
        uint8_t* descriptor = arena_alloc(&storage->arena, 6);
        if (!descriptor)
            asset_set_failure(storage, "asset.out_of_memory", "synth descriptor could not be retained", NULL);
        else
        {
            memcpy(descriptor, entry->synth_desc, 6);
            storage->view.synth_desc = descriptor;
        }
        generation_store_asset(project->generation, storage);
        free(name);
        return storage->view;
    }

    if (kind == VG_ASSET_DIRECT_SOUND)
    {
        WaveData* wave = vg_load_sample(project->root, entry->asset_path ? entry->asset_path : "");
        if (!wave)
            asset_set_failure(
                storage, "asset.decode_failed", "DirectSound sample could not be decoded", entry->asset_path);
        else
        {
            storage->view.payload_len = wave->size;
            void* payload = arena_alloc(&storage->arena, wave->size);
            if (!payload && wave->size > 0)
                asset_set_failure(
                    storage, "asset.out_of_memory", "decoded sample could not be retained", entry->asset_path);
            else
            {
                if (wave->size > 0)
                    memcpy(payload, wave->data, wave->size);
                else
                {
                    uint8_t* descriptor = arena_alloc(&storage->arena, 6);
                    if (descriptor)
                    {
                        memcpy(descriptor, wave->data, 6);
                        storage->view.synth_desc = descriptor;
                    }
                }
                storage->view.payload = payload;
                storage->view.has_loop = (wave->status & 0x4000) != 0;
                storage->view.loop_start = wave->loopStart;
                storage->view.loop_length = wave->size >= wave->loopStart ? wave->size - wave->loopStart : 0;
                storage->view.sample_rate = wave->freq;
                storage->view.frame_count = wave->size;
            }
            free(wave);
        }
        generation_store_asset(project->generation, storage);
        free(name);
        return storage->view;
    }

    if (kind == VG_ASSET_PROG_WAVE)
    {
        uint32_t* wave = vg_load_prog_wave(project->root, entry->asset_path ? entry->asset_path : "");
        if (!wave)
            asset_set_failure(
                storage, "asset.decode_failed", "programmable-wave asset could not be decoded", entry->asset_path);
        else
        {
            void* payload = arena_alloc(&storage->arena, 16);
            if (!payload)
                asset_set_failure(
                    storage, "asset.out_of_memory", "programmable-wave asset could not be retained", entry->asset_path);
            else
            {
                memcpy(payload, wave, 16);
                storage->view.payload = payload;
                storage->view.payload_len = 16;
                storage->view.frame_count = 32;
            }
            free(wave);
        }
        generation_store_asset(project->generation, storage);
        free(name);
        return storage->view;
    }

    uint8_t* table = arena_alloc(&storage->arena, VOICEGROUP_SIZE);
    if (!table || !entry->table || !entry->subgroup)
    {
        asset_set_failure(storage, "asset.keysplit_unresolved", "keysplit row has no table or subgroup", entry->table);
        free(name);
        return storage->view;
    }
    if (!voicegroup_core_project_index_keysplit_table(project->generation->index, entry->table, table))
    {
        asset_set_failure(
            storage, "asset.keysplit_unresolved", "keysplit table is not present in the project index", entry->table);
        free(name);
        return storage->view;
    }

    VoicegroupCoreBankResult* subgroupResult = NULL;
    VoicegroupCoreStatus status =
        voicegroup_core_project_index_load_program_bank(project->generation->index, entry->subgroup, &subgroupResult);
    VoicegroupMaterializationReport report = {0};
    if (status != VOICEGROUP_CORE_STATUS_OK || !subgroupResult ||
        !voicegroup_core_bank_result_has_bank(subgroupResult) ||
        voicegroup_core_bank_result_diagnostic_count(subgroupResult) != 0 ||
        !voicegroup_materialize_core_bank(
            project->root, project->generation->index, subgroupResult, &storage->bank, &report))
    {
        asset_set_failure(
            storage, "asset.keysplit_failed", "keysplit subgroup could not be materialized", entry->subgroup);
    }
    else
    {
        storage->view.keysplit.subgroup = storage->bank->voices;
        storage->view.keysplit.subgroup_count = VOICEGROUP_SIZE;
        storage->view.keysplit.table = table;
        storage->view.keysplit.table_count = VOICEGROUP_SIZE;
    }
    voicegroup_materialization_report_free(&report);
    voicegroup_core_bank_result_free(subgroupResult);
    generation_store_asset(project->generation, storage);
    free(name);
    return storage->view;
}
