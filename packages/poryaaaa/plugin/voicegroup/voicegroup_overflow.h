#ifndef VOICEGROUP_OVERFLOW_H
#define VOICEGROUP_OVERFLOW_H

#include "vg_wav.h"
#include "voicegroup_core.h"
#include "voicegroup_loader.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct CoreMaterializeContext
{
    const char* projectRoot;
    const VoicegroupCoreProjectIndex* index;
    LoadedVoiceGroup* owner;
    WaveCache waveCache;
    VoicegroupMaterializationReport* report;
    char** activeSubgroups;
    size_t activeSubgroupCount;
    size_t activeSubgroupCapacity;
    bool strict;
    bool suppressSubgroups;
    size_t slotLimit;
} CoreMaterializeContext;

bool voicegroup_materializer_append_failure(VoicegroupMaterializationReport* report,
                                            size_t slot,
                                            const char* assetPath,
                                            const char* message);
bool voicegroup_materializer_subgroup_enter(CoreMaterializeContext* ctx, const char* name, size_t slot);
void voicegroup_materializer_subgroup_leave(CoreMaterializeContext* ctx);
bool voicegroup_materializer_register_subgroup(LoadedVoiceGroup* vg, ToneData* subgroup);
bool voicegroup_materializer_materialize_core_bank(CoreMaterializeContext* ctx,
                                                   const VoicegroupCoreBankResult* result,
                                                   ToneData voices[VOICEGROUP_SIZE],
                                                   char names[VOICEGROUP_SIZE][VG_MAX_VOICE_SAMPLE_NAME]);

bool voicegroup_load_core_subgroup(CoreMaterializeContext* ctx,
                                   const char* subVoicegroup,
                                   size_t slot,
                                   ToneData** outSubgroup);

#endif
