#include "voicegroup_overflow.h"

#include "vg_alloc.h"

#include <stdlib.h>
#include <string.h>

static size_t bank_extent(const VoicegroupCoreBankResult* result)
{
    size_t extent = 0;
    for (size_t slot = 0; slot < VOICEGROUP_SIZE; slot++)
        if (voicegroup_core_bank_result_program_kind(result, slot) != VOICEGROUP_CORE_PROGRAM_KIND_EMPTY)
            extent = slot + 1;
    return extent;
}

static bool
load_bank_result(CoreMaterializeContext* ctx, const char* bankName, size_t slot, VoicegroupCoreBankResult** outResult)
{
    *outResult = NULL;
    if (voicegroup_core_project_index_load_program_bank(ctx->index, bankName, outResult) != VOICEGROUP_CORE_STATUS_OK ||
        !*outResult)
    {
        voicegroup_materializer_append_failure(ctx->report, slot, bankName, "voicegroup bank could not be loaded");
        return false;
    }
    if (!voicegroup_core_bank_result_has_bank(*outResult) ||
        voicegroup_core_bank_result_diagnostic_count(*outResult) != 0)
    {
        voicegroup_materializer_append_failure(ctx->report, slot, bankName, "voicegroup bank has blocking diagnostics");
        voicegroup_core_bank_result_free(*outResult);
        *outResult = NULL;
        return false;
    }
    return true;
}

static bool materialize_overflow_bank(CoreMaterializeContext* ctx,
                                      const VoicegroupCoreBankResult* result,
                                      ToneData* destination,
                                      size_t* extent)
{
    size_t sourceExtent = bank_extent(result);
    if (sourceExtent == 0)
        return false;

    size_t room = VOICEGROUP_SIZE - *extent;
    size_t copyCount = sourceExtent < room ? sourceExtent : room;
    if (copyCount == 0)
        return false;

    ToneData source[VOICEGROUP_SIZE];
    memset(source, 0, sizeof(source));
    bool oldSuppressSubgroups = ctx->suppressSubgroups;
    size_t oldSlotLimit = ctx->slotLimit;
    ctx->suppressSubgroups = true;
    ctx->slotLimit = copyCount;
    bool ok = voicegroup_materializer_materialize_core_bank(ctx, result, source, NULL);
    ctx->suppressSubgroups = oldSuppressSubgroups;
    ctx->slotLimit = oldSlotLimit;
    if (!ok)
        return false;

    memcpy(destination + *extent, source, copyCount * sizeof(*source));
    *extent += copyCount;
    return true;
}

bool voicegroup_load_core_subgroup(CoreMaterializeContext* ctx,
                                   const char* subVoicegroup,
                                   size_t slot,
                                   ToneData** outSubgroup)
{
    *outSubgroup = NULL;
    if (!voicegroup_materializer_subgroup_enter(ctx, subVoicegroup, slot))
        return false;

    VoicegroupCoreBankResult* result = NULL;
    if (!load_bank_result(ctx, subVoicegroup, slot, &result))
    {
        voicegroup_materializer_subgroup_leave(ctx);
        return false;
    }

    ToneData* subgroup = vg_malloc_array(VOICEGROUP_SIZE, sizeof(ToneData));
    if (!subgroup)
    {
        voicegroup_core_bank_result_free(result);
        voicegroup_materializer_subgroup_leave(ctx);
        return false;
    }
    memset(subgroup, 0, sizeof(ToneData) * VOICEGROUP_SIZE);

    bool ok = voicegroup_materializer_materialize_core_bank(ctx, result, subgroup, NULL);
    size_t extent = bank_extent(result);
    voicegroup_core_bank_result_free(result);
    if (ok && extent < VOICEGROUP_SIZE)
    {
        char subgroupName[VG_MAX_PATH_LEN];
        char sourcePath[VG_MAX_PATH_LEN];
        size_t continuationCount = voicegroup_core_project_index_bank_continuation_count(ctx->index, subVoicegroup);
        for (size_t ordinal = 0; ok && extent < VOICEGROUP_SIZE && ordinal < continuationCount; ordinal++)
        {
            subgroupName[0] = '\0';
            sourcePath[0] = '\0';
            if (!voicegroup_core_project_index_bank_continuation(ctx->index,
                                                                 subVoicegroup,
                                                                 ordinal,
                                                                 subgroupName,
                                                                 sizeof(subgroupName),
                                                                 sourcePath,
                                                                 sizeof(sourcePath)))
            {
                voicegroup_materializer_append_failure(
                    ctx->report, slot, sourcePath, "voicegroup adjacency could not be resolved");
                ok = false;
                break;
            }
            if (!subgroupName[0])
            {
                voicegroup_materializer_append_failure(
                    ctx->report, slot, sourcePath, "voicegroup adjacency has no indexed bank");
                ok = false;
                break;
            }

            VoicegroupCoreBankResult* nextResult = NULL;
            if (!load_bank_result(ctx, subgroupName, slot, &nextResult))
            {
                ok = false;
                break;
            }
            if (!materialize_overflow_bank(ctx, nextResult, subgroup, &extent))
            {
                voicegroup_materializer_append_failure(
                    ctx->report, slot, subgroupName, "voicegroup adjacency made no progress");
                ok = false;
            }
            voicegroup_core_bank_result_free(nextResult);
        }
    }

    voicegroup_materializer_subgroup_leave(ctx);
    if (!ok)
    {
        free(subgroup);
        return false;
    }
    if (!voicegroup_materializer_register_subgroup(ctx->owner, subgroup))
    {
        free(subgroup);
        return false;
    }
    *outSubgroup = subgroup;
    return true;
}
