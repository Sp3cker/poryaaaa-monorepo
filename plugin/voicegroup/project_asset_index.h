#ifndef PROJECT_ASSET_INDEX_H
#define PROJECT_ASSET_INDEX_H

#include "voicegroup_loader.h"

typedef struct {
    bool active;
    ProjectAssetKind kind;
    char fileName[256];
} ProjectAssetOverride;

typedef struct {
    ProjectAssetEntry *directsoundAssets;
    int directsoundCount;

    ProjectAssetEntry *progWaveAssets;
    int progWaveCount;

    ProjectAssetOverride overrides[VOICEGROUP_SIZE];
} ProjectAssetIndex;

ProjectAssetIndex *project_asset_index_create(void);
void project_asset_index_destroy(ProjectAssetIndex *idx);

/*
 * Rebuild the asset catalog from a project. Frees any previous catalog data.
 */
bool project_asset_index_rebuild(ProjectAssetIndex *idx,
                                 const char *projectRoot,
                                 const VoicegroupLoaderConfig *config);

void project_asset_index_set_override(ProjectAssetIndex *idx, int voiceIndex,
                                      ProjectAssetKind kind, const char *fileName);
void project_asset_index_clear_override(ProjectAssetIndex *idx, int voiceIndex);
void project_asset_index_clear_all_overrides(ProjectAssetIndex *idx);

/*
 * Find an asset entry by kind and fileName. Returns NULL if not found.
 */
const ProjectAssetEntry *project_asset_index_find(const ProjectAssetIndex *idx,
                                                  ProjectAssetKind kind,
                                                  const char *fileName);

/*
 * Apply all active overrides to a loaded voicegroup.
 * For each override, loads the replacement sample and swaps wav/wavePointer.
 * Returns true if all overrides were applied successfully.
 */
bool project_asset_index_apply_overrides(const ProjectAssetIndex *idx,
                                         const char *projectRoot,
                                         LoadedVoiceGroup *vg);

#endif /* PROJECT_ASSET_INDEX_H */
