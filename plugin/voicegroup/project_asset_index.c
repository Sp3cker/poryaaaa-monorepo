#include "project_asset_index.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

ProjectAssetIndex *project_asset_index_create(void)
{
    ProjectAssetIndex *idx = calloc(1, sizeof(ProjectAssetIndex));
    return idx;
}

void project_asset_index_destroy(ProjectAssetIndex *idx)
{
    if (!idx) return;
    free(idx->directsoundAssets);
    free(idx->progWaveAssets);
    free(idx);
}

static void clear_catalog(ProjectAssetIndex *idx)
{
    free(idx->directsoundAssets);
    idx->directsoundAssets = NULL;
    idx->directsoundCount = 0;
    free(idx->progWaveAssets);
    idx->progWaveAssets = NULL;
    idx->progWaveCount = 0;
}

bool project_asset_index_rebuild(ProjectAssetIndex *idx,
                                 const char *projectRoot,
                                 const VoicegroupLoaderConfig *config)
{
    clear_catalog(idx);

    VoicegroupProjectAssets assets;
    if (!voicegroup_loader_collect_project_assets(projectRoot, config, &assets))
        return false;

    /* Transfer ownership of the arrays */
    idx->directsoundAssets = assets.directsound;
    idx->directsoundCount = assets.directsoundCount;
    idx->progWaveAssets = assets.progWave;
    idx->progWaveCount = assets.progWaveCount;
    return true;
}

void project_asset_index_set_override(ProjectAssetIndex *idx, int voiceIndex,
                                      ProjectAssetKind kind, const char *fileName)
{
    if (voiceIndex < 0 || voiceIndex >= VOICEGROUP_SIZE) return;
    idx->overrides[voiceIndex].active = true;
    idx->overrides[voiceIndex].kind = kind;
    strncpy(idx->overrides[voiceIndex].fileName, fileName,
            sizeof(idx->overrides[voiceIndex].fileName) - 1);
    idx->overrides[voiceIndex].fileName[sizeof(idx->overrides[voiceIndex].fileName) - 1] = '\0';
}

void project_asset_index_clear_override(ProjectAssetIndex *idx, int voiceIndex)
{
    if (voiceIndex < 0 || voiceIndex >= VOICEGROUP_SIZE) return;
    memset(&idx->overrides[voiceIndex], 0, sizeof(idx->overrides[voiceIndex]));
}

void project_asset_index_clear_all_overrides(ProjectAssetIndex *idx)
{
    memset(idx->overrides, 0, sizeof(idx->overrides));
}

const ProjectAssetEntry *project_asset_index_find(const ProjectAssetIndex *idx,
                                                  ProjectAssetKind kind,
                                                  const char *fileName)
{
    const ProjectAssetEntry *arr;
    int count;
    if (kind == PROJECT_ASSET_DIRECTSOUND) {
        arr = idx->directsoundAssets;
        count = idx->directsoundCount;
    } else {
        arr = idx->progWaveAssets;
        count = idx->progWaveCount;
    }
    for (int i = 0; i < count; i++) {
        if (strcmp(arr[i].fileName, fileName) == 0)
            return &arr[i];
    }
    return NULL;
}

static bool voice_is_directsound(uint8_t type)
{
    uint8_t base = type & ~VOICE_TYPE_FIX;
    return base == VOICE_DIRECTSOUND || base == VOICE_DIRECTSOUND_ALT;
}

static bool voice_is_prog_wave(uint8_t type)
{
    uint8_t base = type & ~VOICE_TYPE_FIX;
    return base == VOICE_PROGRAMMABLE_WAVE;
}

bool project_asset_index_apply_overrides(const ProjectAssetIndex *idx,
                                         const char *projectRoot,
                                         LoadedVoiceGroup *vg)
{
    bool allOk = true;
    for (int i = 0; i < VOICEGROUP_SIZE; i++) {
        const ProjectAssetOverride *ov = &idx->overrides[i];
        if (!ov->active) continue;

        const ProjectAssetEntry *entry = project_asset_index_find(idx, ov->kind, ov->fileName);
        if (!entry) {
            fprintf(stderr, "project_asset_index: override for voice %d: asset '%s' not found\n",
                    i, ov->fileName);
            allOk = false;
            continue;
        }

        ToneData *voice = &vg->voices[i];
        if (ov->kind == PROJECT_ASSET_DIRECTSOUND && voice_is_directsound(voice->type)) {
            WaveData *wd = voicegroup_loader_load_sample(projectRoot, entry->relPath, vg);
            if (wd) {
                voice->wav = wd;
            } else {
                fprintf(stderr, "project_asset_index: failed to load sample '%s' for voice %d\n",
                        entry->fileName, i);
                allOk = false;
            }
        } else if (ov->kind == PROJECT_ASSET_PROG_WAVE && voice_is_prog_wave(voice->type)) {
            uint32_t *pw = voicegroup_loader_load_prog_wave(projectRoot, entry->relPath, vg);
            if (pw) {
                voice->wavePointer = pw;
            } else {
                fprintf(stderr, "project_asset_index: failed to load wave '%s' for voice %d\n",
                        entry->fileName, i);
                allOk = false;
            }
        } else {
            fprintf(stderr, "project_asset_index: type mismatch for voice %d override\n", i);
            allOk = false;
        }
    }
    return allOk;
}
