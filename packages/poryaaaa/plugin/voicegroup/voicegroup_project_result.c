#include "voicegroup_project_internal.h"

#include <stdlib.h>
#include <string.h>

char* duplicate_bytes(const char* value, size_t length)
{
    char* copy = malloc(length + 1);
    if (!copy)
        return NULL;
    if (length > 0)
        memcpy(copy, value, length);
    copy[length] = '\0';
    return copy;
}

static void arena_dispose(ResultArena* arena)
{
    if (!arena)
        return;
    for (size_t i = 0; i < arena->count; i++)
        free(arena->items[i]);
    free(arena->items);
    memset(arena, 0, sizeof(*arena));
}

void* arena_alloc(ResultArena* arena, size_t size)
{
    if (size == 0)
        size = 1;
    void* allocation = calloc(1, size);
    if (!allocation)
        return NULL;
    if (arena->count >= arena->capacity)
    {
        size_t next = arena->capacity ? arena->capacity * 2 : 16;
        if (next < arena->capacity || next > SIZE_MAX / sizeof(*arena->items))
        {
            free(allocation);
            return NULL;
        }
        void** items = realloc(arena->items, next * sizeof(*items));
        if (!items)
        {
            free(allocation);
            return NULL;
        }
        arena->items = items;
        arena->capacity = next;
    }
    arena->items[arena->count++] = allocation;
    return allocation;
}

char* arena_copy_string(ResultArena* arena, const char* value)
{
    if (!value)
        return NULL;
    char* copy = arena_alloc(arena, strlen(value) + 1);
    if (copy)
        memcpy(copy, value, strlen(value) + 1);
    return copy;
}

static char* arena_copy_bytes(ResultArena* arena, const char* value, size_t length)
{
    char* copy = arena_alloc(arena, length + 1);
    if (!copy)
        return NULL;
    if (length > 0)
        memcpy(copy, value, length);
    copy[length] = '\0';
    return copy;
}

void project_storage_dispose(ProjectResultStorage* storage)
{
    if (!storage)
        return;
    arena_dispose(&storage->arena);
    free(storage);
}

void load_storage_dispose(LoadResultStorage* storage)
{
    if (!storage)
        return;
    if (storage->bank)
        voicegroup_free(storage->bank);
    arena_dispose(&storage->arena);
    free(storage);
}

void asset_storage_dispose(AssetResultStorage* storage)
{
    if (!storage)
        return;
    if (storage->bank)
        voicegroup_free(storage->bank);
    arena_dispose(&storage->arena);
    free(storage);
}

ProjectResultStorage* project_storage_create(void)
{
    ProjectResultStorage* storage = calloc(1, sizeof(*storage));
    if (storage)
        storage->view._private_storage = storage;
    return storage;
}

LoadResultStorage* load_storage_create(void)
{
    LoadResultStorage* storage = calloc(1, sizeof(*storage));
    if (storage)
        storage->view._private_storage = storage;
    return storage;
}

AssetResultStorage* asset_storage_create(VoicegroupAssetKind kind, const char* symbol, size_t symbolLen)
{
    AssetResultStorage* storage = calloc(1, sizeof(*storage));
    if (!storage)
        return NULL;
    storage->view.kind = (uint32_t)kind;
    size_t safeLength = symbol ? symbolLen : 0;
    storage->view.symbol = arena_copy_bytes(&storage->arena, symbol ? symbol : "", safeLength);
    if (!storage->view.symbol)
    {
        asset_storage_dispose(storage);
        return NULL;
    }
    storage->view._private_storage = storage;
    return storage;
}

static bool
copy_string_array(ResultArena* arena, const char* const* source, size_t count, const char* const** destination)
{
    *destination = NULL;
    if (count == 0)
        return true;
    char** paths = arena_alloc(arena, count * sizeof(*paths));
    if (!paths)
        return false;
    for (size_t i = 0; i < count; i++)
    {
        paths[i] = arena_copy_string(arena, source[i]);
        if (!paths[i])
            return false;
    }
    *destination = (const char* const*)paths;
    return true;
}

bool copy_diagnostic(ResultArena* arena, VoicegroupDiagnostic* destination, const VoicegroupDiagnostic* source)
{
    *destination = *source;
    destination->code = arena_copy_string(arena, source->code);
    destination->message = arena_copy_string(arena, source->message);
    destination->source_path = arena_copy_string(arena, source->source_path);
    destination->asset_path = arena_copy_string(arena, source->asset_path);
    return destination->code && destination->message && (!source->source_path || destination->source_path) &&
           (!source->asset_path || destination->asset_path);
}

static bool copy_catalog(ResultArena* arena,
                         const VoicegroupCatalogEntry* source,
                         size_t count,
                         VoicegroupCatalogEntry** destination)
{
    *destination = NULL;
    if (count == 0)
        return true;
    VoicegroupCatalogEntry* entries = arena_alloc(arena, count * sizeof(*entries));
    if (!entries)
        return false;
    for (size_t i = 0; i < count; i++)
    {
        entries[i] = source[i];
        entries[i].symbol = arena_copy_string(arena, source[i].symbol);
        entries[i].display_name = arena_copy_string(arena, source[i].display_name);
        entries[i].source_path = arena_copy_string(arena, source[i].source_path);
        entries[i].asset_path = arena_copy_string(arena, source[i].asset_path);
        entries[i].subgroup = arena_copy_string(arena, source[i].subgroup);
        entries[i].table = arena_copy_string(arena, source[i].table);
        entries[i].drumkit = arena_copy_string(arena, source[i].drumkit);
        if (!entries[i].symbol || !entries[i].display_name || (source[i].source_path && !entries[i].source_path) ||
            (source[i].asset_path && !entries[i].asset_path) || (source[i].subgroup && !entries[i].subgroup) ||
            (source[i].table && !entries[i].table) || (source[i].drumkit && !entries[i].drumkit))
            return false;
        const char* const* dependencies = NULL;
        if (!copy_string_array(arena, source[i].dependency_paths, source[i].dependency_path_count, &dependencies))
            return false;
        entries[i].dependency_paths = dependencies;
    }
    *destination = entries;
    return true;
}

static bool copy_family_adsr(ResultArena* arena,
                             const VoicegroupFamilyAdsr* source,
                             size_t count,
                             const VoicegroupFamilyAdsr** destination)
{
    *destination = NULL;
    if (count == 0)
        return true;
    VoicegroupFamilyAdsr* entries = arena_alloc(arena, count * sizeof(*entries));
    if (!entries)
        return false;
    for (size_t i = 0; i < count; i++)
    {
        entries[i] = source[i];
        entries[i].family = arena_copy_string(arena, source[i].family);
        if (!entries[i].family)
            return false;
    }
    *destination = entries;
    return true;
}

static bool
copy_project_arrays(ResultArena* arena, VoicegroupProjectResult* destination, const VoicegroupProjectResult* source)
{
    VoicegroupCatalogEntry* catalog = NULL;
    if (!copy_catalog(arena, source->catalog, source->catalog_count, &catalog))
        return false;
    destination->catalog = catalog;
    destination->catalog_count = source->catalog_count;

    const VoicegroupFamilyAdsr* familyAdsr = NULL;
    if (!copy_family_adsr(arena, source->family_adsr, source->family_adsr_count, &familyAdsr))
        return false;
    destination->family_adsr = familyAdsr;
    destination->family_adsr_count = source->family_adsr_count;

    const char* const* paths = NULL;
    if (!copy_string_array(arena, source->synth_macro_words, source->synth_macro_word_count, &paths))
        return false;
    destination->synth_macro_words = paths;
    destination->synth_macro_word_count = source->synth_macro_word_count;

    if (!copy_string_array(arena, source->content_paths, source->content_path_count, &paths))
        return false;
    destination->content_paths = paths;
    destination->content_path_count = source->content_path_count;
    if (!copy_string_array(arena, source->dependency_paths, source->dependency_path_count, &paths))
        return false;
    destination->dependency_paths = paths;
    destination->dependency_path_count = source->dependency_path_count;
    if (!copy_string_array(arena, source->watch_paths, source->watch_path_count, &paths))
        return false;
    destination->watch_paths = paths;
    destination->watch_path_count = source->watch_path_count;
    return true;
}

bool copy_project_result(ProjectResultStorage* destination, const VoicegroupProjectResult* source)
{
    destination->view.succeeded = source->succeeded;
    destination->view.diagnostic_count = source->diagnostic_count;
    if (source->diagnostic_count > 0)
    {
        VoicegroupDiagnostic* diagnostics =
            arena_alloc(&destination->arena, source->diagnostic_count * sizeof(*diagnostics));
        if (!diagnostics)
            return false;
        for (size_t i = 0; i < source->diagnostic_count; i++)
            if (!copy_diagnostic(&destination->arena, &diagnostics[i], &source->diagnostics[i]))
                return false;
        destination->view.diagnostics = diagnostics;
    }
    return copy_project_arrays(&destination->arena, &destination->view, source);
}

bool copy_core_diagnostic(ResultArena* arena, VoicegroupDiagnostic* destination, const VoicegroupCoreDiagnostic* source)
{
    memset(destination, 0, sizeof(*destination));
    destination->code = arena_copy_string(arena, source->code);
    destination->message = arena_copy_string(arena, source->message);
    destination->scope = (uint32_t)source->scope;
    destination->source_path = arena_copy_string(arena, source->source_path);
    destination->asset_path = arena_copy_string(arena, source->asset_path);
    destination->has_range = source->has_range;
    destination->start_line = source->range.start.line;
    destination->start_column = source->range.start.column;
    destination->end_line = source->range.end.line;
    destination->end_column = source->range.end.column;
    destination->has_slot = source->has_slot;
    destination->slot = source->slot;
    return destination->code && destination->message && (!source->source_path || destination->source_path) &&
           (!source->asset_path || destination->asset_path);
}

static bool copy_core_diagnostics(ResultArena* arena,
                                  const VoicegroupCoreDiagnostic* source,
                                  size_t count,
                                  VoicegroupDiagnostic** destination)
{
    *destination = NULL;
    if (count == 0)
        return true;
    VoicegroupDiagnostic* diagnostics = arena_alloc(arena, count * sizeof(*diagnostics));
    if (!diagnostics)
        return false;
    for (size_t i = 0; i < count; i++)
        if (!copy_core_diagnostic(arena, &diagnostics[i], &source[i]))
            return false;
    *destination = diagnostics;
    return true;
}

static bool copy_core_catalog(ResultArena* arena,
                              const VoicegroupCoreCatalogEntry* source,
                              size_t count,
                              VoicegroupCatalogEntry** destination)
{
    *destination = NULL;
    if (count == 0)
        return true;
    VoicegroupCatalogEntry* entries = arena_alloc(arena, count * sizeof(*entries));
    if (!entries)
        return false;
    for (size_t i = 0; i < count; i++)
    {
        memset(&entries[i], 0, sizeof(entries[i]));
        entries[i].kind = (uint32_t)source[i].kind;
        entries[i].symbol = arena_copy_string(arena, source[i].symbol);
        entries[i].display_name = arena_copy_string(arena, source[i].display_name);
        entries[i].source_path = arena_copy_string(arena, source[i].source_path);
        entries[i].asset_path = arena_copy_string(arena, source[i].asset_path);
        entries[i].subgroup = arena_copy_string(arena, source[i].subgroup);
        entries[i].table = arena_copy_string(arena, source[i].table);
        entries[i].drumkit = arena_copy_string(arena, source[i].drumkit);
        entries[i].dependency_path_count = source[i].dependency_path_count;
        if (!copy_string_array(
                arena, source[i].dependency_paths, source[i].dependency_path_count, &entries[i].dependency_paths) ||
            !entries[i].symbol || !entries[i].display_name || (source[i].source_path && !entries[i].source_path) ||
            (source[i].asset_path && !entries[i].asset_path) || (source[i].subgroup && !entries[i].subgroup) ||
            (source[i].table && !entries[i].table) || (source[i].drumkit && !entries[i].drumkit))
            return false;
        entries[i].has_adsr = source[i].has_adsr;
        memcpy(entries[i].adsr, source[i].adsr, sizeof(entries[i].adsr));
        entries[i].has_synth = source[i].has_synth;
        memcpy(entries[i].synth_desc, source[i].synth_desc, sizeof(entries[i].synth_desc));
    }
    *destination = entries;
    return true;
}

bool project_storage_copy_core_snapshot(ProjectResultStorage* storage,
                                        const VoicegroupCoreProjectSnapshotResult* snapshot)
{
    size_t count = 0;
    const VoicegroupCoreCatalogEntry* catalog = voicegroup_core_project_snapshot_result_catalog(snapshot, &count);
    storage->view.succeeded = voicegroup_core_project_snapshot_result_succeeded(snapshot);
    storage->view.catalog_count = count;
    VoicegroupCatalogEntry* copiedCatalog = NULL;
    if (!copy_core_catalog(&storage->arena, catalog, count, &copiedCatalog))
        return false;
    storage->view.catalog = copiedCatalog;

    const VoicegroupCoreFamilyAdsr* familyAdsr = voicegroup_core_project_snapshot_result_family_adsr(snapshot, &count);
    storage->view.family_adsr = NULL;
    storage->view.family_adsr_count = count;
    if (count > 0)
    {
        VoicegroupFamilyAdsr* copiedFamilyAdsr = arena_alloc(&storage->arena, count * sizeof(*copiedFamilyAdsr));
        if (!copiedFamilyAdsr)
            return false;
        for (size_t i = 0; i < count; i++)
        {
            copiedFamilyAdsr[i].family = arena_copy_string(&storage->arena, familyAdsr[i].family);
            memcpy(copiedFamilyAdsr[i].adsr, familyAdsr[i].adsr, sizeof(copiedFamilyAdsr[i].adsr));
            if (!copiedFamilyAdsr[i].family)
                return false;
        }
        storage->view.family_adsr = copiedFamilyAdsr;
    }

    const char* const* macroWords = voicegroup_core_project_snapshot_result_synth_macro_words(snapshot, &count);
    storage->view.synth_macro_word_count = count;
    if (!copy_string_array(&storage->arena, macroWords, count, &storage->view.synth_macro_words))
        return false;

    const VoicegroupCoreDiagnostic* diagnostics = voicegroup_core_project_snapshot_result_diagnostics(snapshot, &count);
    storage->view.diagnostic_count = count;
    VoicegroupDiagnostic* copiedDiagnostics = NULL;
    if (!copy_core_diagnostics(&storage->arena, diagnostics, count, &copiedDiagnostics))
        return false;
    storage->view.diagnostics = copiedDiagnostics;

    const char* const* paths = voicegroup_core_project_snapshot_result_content_paths(snapshot, &count);
    storage->view.content_path_count = count;
    if (!copy_string_array(&storage->arena, paths, count, &storage->view.content_paths))
        return false;
    paths = voicegroup_core_project_snapshot_result_dependency_paths(snapshot, &count);
    storage->view.dependency_path_count = count;
    if (!copy_string_array(&storage->arena, paths, count, &storage->view.dependency_paths))
        return false;
    paths = voicegroup_core_project_snapshot_result_watch_paths(snapshot, &count);
    storage->view.watch_path_count = count;
    if (!copy_string_array(&storage->arena, paths, count, &storage->view.watch_paths))
        return false;
    return true;
}

bool add_simple_diagnostic(ResultArena* arena,
                           VoicegroupDiagnostic** diagnostics,
                           size_t* count,
                           const char* code,
                           const char* message,
                           uint32_t scope,
                           const char* sourcePath,
                           const char* assetPath,
                           bool hasSlot,
                           size_t slot)
{
    VoicegroupDiagnostic* item = arena_alloc(arena, sizeof(*item));
    if (!item)
        return false;
    memset(item, 0, sizeof(*item));
    item->code = arena_copy_string(arena, code);
    item->message = arena_copy_string(arena, message);
    item->scope = scope;
    item->source_path = arena_copy_string(arena, sourcePath);
    item->asset_path = arena_copy_string(arena, assetPath);
    item->has_slot = hasSlot;
    item->slot = slot;
    if (!item->code || !item->message || (sourcePath && !item->source_path) || (assetPath && !item->asset_path))
        return false;
    *diagnostics = item;
    *count = 1;
    return true;
}

void voicegroup_project_result_free(VoicegroupProjectResult* result)
{
    if (!result)
        return;
    project_storage_dispose((ProjectResultStorage*)result->_private_storage);
    *result = (VoicegroupProjectResult){0};
}

void voicegroup_load_result_free(VoicegroupLoadResult* result)
{
    if (!result)
        return;
    load_storage_dispose((LoadResultStorage*)result->_private_storage);
    *result = (VoicegroupLoadResult){0};
}

void voicegroup_asset_result_free(VoicegroupAssetResult* result)
{
    if (!result)
        return;
    asset_storage_dispose((AssetResultStorage*)result->_private_storage);
    *result = (VoicegroupAssetResult){0};
}