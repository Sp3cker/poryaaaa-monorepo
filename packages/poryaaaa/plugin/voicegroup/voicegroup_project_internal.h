#ifndef VOICEGROUP_PROJECT_INTERNAL_H
#define VOICEGROUP_PROJECT_INTERNAL_H

#include "voicegroup_project.h"
#include "voicegroup_core.h"

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum
    {
        PROJECT_STALE,
        PROJECT_FRESH,
        PROJECT_REFRESH_FAILED,
    } ProjectState;

    typedef struct
    {
        void** items;
        size_t count;
        size_t capacity;
    } ResultArena;

    typedef struct
    {
        ResultArena arena;
        VoicegroupProjectResult view;
    } ProjectResultStorage;

    typedef struct
    {
        ResultArena arena;
        VoicegroupLoadResult view;
        LoadedVoiceGroup* bank;
    } LoadResultStorage;

    typedef struct
    {
        ResultArena arena;
        VoicegroupAssetResult view;
        LoadedVoiceGroup* bank;
    } AssetResultStorage;

    typedef struct
    {
        AssetResultStorage* storage;
    } AssetCacheEntry;

    typedef struct
    {
        VoicegroupCoreProjectIndex* index;
        ProjectResultStorage* snapshot;
        AssetCacheEntry* assetCache;
        size_t assetCacheCount;
        size_t assetCacheCapacity;
    } ProjectGeneration;

    struct VoicegroupProject
    {
        char* root;
        ProjectState state;
        ProjectGeneration* generation;
        ProjectResultStorage* failure;
    };

    struct VoicegroupSynthOverlay
    {
        VoicegroupCoreSynthOverlay* core;
    };

    char* duplicate_bytes(const char* value, size_t length);

    void* arena_alloc(ResultArena* arena, size_t size);
    char* arena_copy_string(ResultArena* arena, const char* value);

    ProjectResultStorage* project_storage_create(void);
    void project_storage_dispose(ProjectResultStorage* storage);
    LoadResultStorage* load_storage_create(void);
    void load_storage_dispose(LoadResultStorage* storage);
    AssetResultStorage* asset_storage_create(VoicegroupAssetKind kind, const char* symbol, size_t symbolLen);
    void asset_storage_dispose(AssetResultStorage* storage);

    bool copy_diagnostic(ResultArena* arena, VoicegroupDiagnostic* destination, const VoicegroupDiagnostic* source);
    bool copy_project_result(ProjectResultStorage* destination, const VoicegroupProjectResult* source);
    bool
    copy_core_diagnostic(ResultArena* arena, VoicegroupDiagnostic* destination, const VoicegroupCoreDiagnostic* source);
    bool project_storage_copy_core_snapshot(ProjectResultStorage* storage,
                                            const VoicegroupCoreProjectSnapshotResult* snapshot);
    bool add_simple_diagnostic(ResultArena* arena,
                               VoicegroupDiagnostic** diagnostics,
                               size_t* count,
                               const char* code,
                               const char* message,
                               uint32_t scope,
                               const char* sourcePath,
                               const char* assetPath,
                               bool hasSlot,
                               size_t slot);

    void generation_clear_asset_cache(ProjectGeneration* generation);
    bool ensure_generation(VoicegroupProject* project);

#ifdef __cplusplus
}
#endif

#endif /* VOICEGROUP_PROJECT_INTERNAL_H */
