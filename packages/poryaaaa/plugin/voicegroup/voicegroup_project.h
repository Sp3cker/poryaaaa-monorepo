#ifndef VOICEGROUP_PROJECT_H
#define VOICEGROUP_PROJECT_H

#include "voicegroup_loader.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct VoicegroupProject VoicegroupProject;
    typedef struct VoicegroupSynthOverlay VoicegroupSynthOverlay;

    typedef struct
    {
        const char* code;
        const char* message;
        uint32_t scope;
        const char* source_path;
        const char* asset_path;
        bool has_range;
        size_t start_line;
        size_t start_column;
        size_t end_line;
        size_t end_column;
        bool has_slot;
        size_t slot;
    } VoicegroupDiagnostic;

    typedef struct
    {
        uint32_t kind;
        const char* symbol;
        const char* display_name;
        const char* source_path;
        const char* asset_path;
        const char* const* dependency_paths;
        size_t dependency_path_count;
        const char* subgroup;
        const char* table;
        const char* drumkit;
        bool has_adsr;
        uint8_t adsr[4];
        bool has_synth;
        uint8_t synth_desc[6];
    } VoicegroupCatalogEntry;

    typedef struct
    {
        bool succeeded;
        const VoicegroupDiagnostic* diagnostics;
        size_t diagnostic_count;
        const VoicegroupCatalogEntry* catalog;
        size_t catalog_count;
        const char* const* content_paths;
        size_t content_path_count;
        const char* const* dependency_paths;
        size_t dependency_path_count;
        const char* const* watch_paths;
        size_t watch_path_count;
        void* _private_storage;
    } VoicegroupProjectResult;

    typedef enum
    {
        VG_LOAD_SAVED,
        VG_LOAD_SOURCE,
    } VoicegroupLoadMode;

    typedef struct
    {
        VoicegroupLoadMode mode;
        const char* bank_name;
        size_t bank_name_len;
        const char* relative_path;
        size_t relative_path_len;
        const char* source_bytes;
        size_t source_len;
        const VoicegroupSynthOverlay* overlay;
    } VoicegroupLoadRequest;

    typedef struct
    {
        bool succeeded;
        const VoicegroupDiagnostic* diagnostics;
        size_t diagnostic_count;
        void* _private_storage;
    } VoicegroupLoadResult;

    typedef struct
    {
        const ToneData* subgroup;
        size_t subgroup_count;
        const uint8_t* table;
        size_t table_count;
    } VoicegroupKeysplitAsset;

    typedef struct
    {
        uint32_t kind;
        const char* symbol;
        const void* payload;
        size_t payload_len;
        const uint8_t* synth_desc;
        VoicegroupKeysplitAsset keysplit;
        bool has_loop;
        size_t loop_start;
        size_t loop_length;
        uint32_t sample_rate;
        size_t frame_count;
        const VoicegroupDiagnostic* diagnostics;
        size_t diagnostic_count;
        void* _private_storage;
    } VoicegroupAssetResult;

    /* ---- family 1: own / refresh / stale the project ---- */
    VoicegroupProject* voicegroup_project_open(const char* root, size_t root_len);
    void voicegroup_project_refresh(VoicegroupProject* project, VoicegroupProjectResult* out);
    void voicegroup_project_mark_stale(VoicegroupProject* project);
    void voicegroup_project_result_free(VoicegroupProjectResult* result);
    void voicegroup_project_free(VoicegroupProject* project);

    /* ---- family 2: materialize one saved or unsaved bank ---- */
    VoicegroupLoadResult voicegroup_project_load(VoicegroupProject* project, const VoicegroupLoadRequest* request);
    LoadedVoiceGroup* voicegroup_load_result_take(VoicegroupLoadResult* result);
    void voicegroup_load_result_free(VoicegroupLoadResult* result);

    /* ---- family 3: load one picker asset on demand ---- */
    typedef enum
    {
        VG_ASSET_DIRECT_SOUND = 0,
        VG_ASSET_PROG_WAVE = 1,
        VG_ASSET_KEYSPLIT = 2,
    } VoicegroupAssetKind;

    VoicegroupAssetResult voicegroup_project_load_asset(VoicegroupProject* project,
                                                        VoicegroupAssetKind kind,
                                                        const char* symbol,
                                                        size_t symbol_len);
    void voicegroup_asset_result_free(VoicegroupAssetResult* result);

    /* ---- transient synth definitions for source previews ---- */
    VoicegroupSynthOverlay* voicegroup_synth_overlay_create(void);
    void voicegroup_synth_overlay_add(VoicegroupSynthOverlay* overlay,
                                      const char* name,
                                      size_t name_len,
                                      uint8_t descriptor[6]);
    void voicegroup_synth_overlay_free(VoicegroupSynthOverlay* overlay);

#ifdef __cplusplus
}
#endif

#endif /* VOICEGROUP_PROJECT_H */
