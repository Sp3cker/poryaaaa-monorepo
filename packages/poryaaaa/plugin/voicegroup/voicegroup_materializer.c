#include "voicegroup_loader.h"

#include "vg_alloc.h"
#include "vg_keysplit.h"
#include "vg_log.h"
#include "vg_parser.h"
#include "vg_wav.h"
#include "voicegroup_overflow.h"
#include "voicegroup_core.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define INITIAL_CAPACITY 64
#define MAX_SUBGROUP_DEPTH 128

static char* duplicate_string(const char* value)
{
    size_t length = strlen(value) + 1;
    char* copy = malloc(length);
    if (copy)
        memcpy(copy, value, length);
    return copy;
}

static bool next_capacity(int current, int needed, int* out)
{
    int newCapacity = current ? current : INITIAL_CAPACITY;
    while (newCapacity < needed)
    {
        if (newCapacity > INT_MAX / 2)
            return false;
        newCapacity *= 2;
    }
    *out = newCapacity;
    return true;
}

static bool register_wavedata(LoadedVoiceGroup* vg, WaveData* wd)
{
    if (vg->waveDataCount >= vg->waveDataCapacity)
    {
        int newCapacity;
        if (!next_capacity(vg->waveDataCapacity, vg->waveDataCount + 1, &newCapacity))
            return false;
        WaveData** items = vg_realloc_array(vg->waveDatas, (size_t)newCapacity, sizeof(*vg->waveDatas));
        if (!items)
            return false;
        vg->waveDatas = items;
        vg->waveDataCapacity = newCapacity;
    }
    vg->waveDatas[vg->waveDataCount++] = wd;
    return true;
}

static bool register_subgroup(LoadedVoiceGroup* vg, ToneData* subgroup)
{
    if (vg->subGroupCount >= vg->subGroupCapacity)
    {
        int newCapacity;
        if (!next_capacity(vg->subGroupCapacity, vg->subGroupCount + 1, &newCapacity))
            return false;
        ToneData** items = vg_realloc_array(vg->subGroups, (size_t)newCapacity, sizeof(*vg->subGroups));
        if (!items)
            return false;
        vg->subGroups = items;
        vg->subGroupCapacity = newCapacity;
    }
    vg->subGroups[vg->subGroupCount++] = subgroup;
    return true;
}

static bool register_keysplit_table(LoadedVoiceGroup* vg, uint8_t* table)
{
    if (vg->keySplitTableCount >= vg->keySplitTableCapacity)
    {
        int newCapacity;
        if (!next_capacity(vg->keySplitTableCapacity, vg->keySplitTableCount + 1, &newCapacity))
            return false;
        uint8_t** items = vg_realloc_array(vg->keySplitTables, (size_t)newCapacity, sizeof(*vg->keySplitTables));
        if (!items)
            return false;
        vg->keySplitTables = items;
        vg->keySplitTableCapacity = newCapacity;
    }
    vg->keySplitTables[vg->keySplitTableCount++] = table;
    return true;
}

static bool
append_failure(VoicegroupMaterializationReport* report, size_t slot, const char* assetPath, const char* message)
{
    if (!report)
        return true;
    if (report->count >= report->capacity)
    {
        size_t next = report->capacity ? report->capacity * 2 : 8;
        if (next < report->capacity || next > SIZE_MAX / sizeof(*report->failures))
            return false;
        VoicegroupMaterializationFailure* failures = realloc(report->failures, next * sizeof(*failures));
        if (!failures)
            return false;
        report->failures = failures;
        report->capacity = next;
    }
    VoicegroupMaterializationFailure* failure = &report->failures[report->count];
    memset(failure, 0, sizeof(*failure));
    failure->slot = slot;
    failure->assetPath = duplicate_string(assetPath ? assetPath : "");
    failure->message = duplicate_string(message ? message : "voicegroup materialization failed");
    if (!failure->assetPath || !failure->message)
    {
        free(failure->assetPath);
        free(failure->message);
        memset(failure, 0, sizeof(*failure));
        return false;
    }
    report->count++;
    return true;
}

void voicegroup_materialization_report_free(VoicegroupMaterializationReport* report)
{
    if (!report)
        return;
    for (size_t i = 0; i < report->count; i++)
    {
        free(report->failures[i].assetPath);
        free(report->failures[i].message);
    }
    free(report->failures);
    memset(report, 0, sizeof(*report));
}

static void read_core_display_name(const VoicegroupCoreBankResult* result, size_t slot, char* buffer, size_t bufferLen)
{
    if (bufferLen == 0)
        return;
    buffer[0] = '\0';
    voicegroup_core_bank_result_program_display_name(result, slot, buffer, bufferLen);
}

static void read_core_relative_path(const VoicegroupCoreBankResult* result, size_t slot, char* buffer, size_t bufferLen)
{
    if (bufferLen == 0)
        return;
    buffer[0] = '\0';
    voicegroup_core_bank_result_program_relative_path(result, slot, buffer, bufferLen);
}

static void
read_core_sub_voicegroup(const VoicegroupCoreBankResult* result, size_t slot, char* buffer, size_t bufferLen)
{
    if (bufferLen == 0)
        return;
    buffer[0] = '\0';
    voicegroup_core_bank_result_program_sub_voicegroup(result, slot, buffer, bufferLen);
}

static void set_slot_name(char names[VOICEGROUP_SIZE][VG_MAX_VOICE_SAMPLE_NAME], size_t slot, const char* name)
{
    if (!names || slot >= VOICEGROUP_SIZE || !name)
        return;
    strncpy(names[slot], name, VG_MAX_VOICE_SAMPLE_NAME - 1);
    names[slot][VG_MAX_VOICE_SAMPLE_NAME - 1] = '\0';
}

static bool subgroup_enter(CoreMaterializeContext* ctx, const char* name, size_t slot)
{
    for (size_t i = 0; i < ctx->activeSubgroupCount; i++)
        if (strcmp(ctx->activeSubgroups[i], name) == 0)
        {
            append_failure(ctx->report, slot, name, "cyclic keysplit subgroup reference");
            return false;
        }
    if (ctx->activeSubgroupCount >= MAX_SUBGROUP_DEPTH)
    {
        append_failure(ctx->report, slot, name, "keysplit subgroup nesting exceeds the safety limit");
        return false;
    }
    if (ctx->activeSubgroupCount >= ctx->activeSubgroupCapacity)
    {
        size_t next = ctx->activeSubgroupCapacity ? ctx->activeSubgroupCapacity * 2 : 8;
        if (next < ctx->activeSubgroupCapacity || next > SIZE_MAX / sizeof(*ctx->activeSubgroups))
            return false;
        char** names = realloc(ctx->activeSubgroups, next * sizeof(*names));
        if (!names)
            return false;
        ctx->activeSubgroups = names;
        ctx->activeSubgroupCapacity = next;
    }
    ctx->activeSubgroups[ctx->activeSubgroupCount] = duplicate_string(name);
    if (!ctx->activeSubgroups[ctx->activeSubgroupCount])
        return false;
    ctx->activeSubgroupCount++;
    return true;
}

static void subgroup_leave(CoreMaterializeContext* ctx)
{
    if (ctx->activeSubgroupCount == 0)
        return;
    free(ctx->activeSubgroups[--ctx->activeSubgroupCount]);
}

static WaveData* load_sample_cached(CoreMaterializeContext* ctx, const char* relativePath, size_t slot)
{
    char absolutePath[VG_MAX_PATH_LEN];
    vg_build_path(absolutePath, sizeof(absolutePath), ctx->projectRoot, relativePath);
    if (absolutePath[0])
    {
        WaveData* cached = vg_wave_cache_find(&ctx->waveCache, absolutePath);
        if (cached)
            return cached;
    }
    WaveData* wd = voicegroup_loader_load_sample(ctx->projectRoot, relativePath, ctx->owner);
    if (!wd)
    {
        append_failure(ctx->report, slot, relativePath, "sample could not be decoded");
        return NULL;
    }
    if (absolutePath[0])
        vg_wave_cache_insert(&ctx->waveCache, absolutePath, wd);
    return wd;
}

static bool materialize_core_bank(CoreMaterializeContext* ctx,
                                  const VoicegroupCoreBankResult* result,
                                  ToneData voices[VOICEGROUP_SIZE],
                                  char names[VOICEGROUP_SIZE][VG_MAX_VOICE_SAMPLE_NAME]);

static WaveData* make_synth_wave(const uint8_t descriptor[6])
{
    const size_t descriptorBytes = 16;
    size_t totalBytes;
    if (!vg_size_add(sizeof(WaveData), descriptorBytes, &totalBytes) || !vg_size_add(totalBytes, 1, &totalBytes))
        return NULL;
    WaveData* wd = calloc(1, totalBytes);
    if (!wd)
        return NULL;
    wd->type = 0;
    wd->status = 0x4000;
    wd->freq = 0x01058920;
    wd->size = 0;
    wd->data = (int8_t*)((uint8_t*)wd + sizeof(WaveData));
    memcpy(wd->data, descriptor, 6);
    return wd;
}

static bool materialize_directsound(CoreMaterializeContext* ctx,
                                    const VoicegroupCoreBankResult* result,
                                    size_t slot,
                                    ToneData* voice)
{
    VoicegroupCoreDirectSoundProgram program = {0};
    if (!voicegroup_core_bank_result_program_direct_sound(result, slot, &program))
        return false;

    voice->type = voicegroup_core_bank_result_program_type_code(result, slot);
    voice->key = program.key;
    voice->panSweep = program.pan ? (uint8_t)(0x80 | program.pan) : 0;
    voice->attack = program.attack;
    voice->decay = program.decay;
    voice->sustain = program.sustain;
    voice->release = program.release;

    WaveData* wd = NULL;
    bool registerWave = false;
    if (program.has_synth)
    {
        wd = make_synth_wave(program.synth_desc);
        registerWave = true;
    }
    else
    {
        char relPath[VG_MAX_PATH_LEN];
        read_core_relative_path(result, slot, relPath, sizeof(relPath));
        if (relPath[0])
            wd = load_sample_cached(ctx, relPath, slot);
        else
            append_failure(ctx->report, slot, "", "direct-sound asset has no source path");
    }
    if (!wd)
        return !ctx->strict;
    if (registerWave && !register_wavedata(ctx->owner, wd))
    {
        free(wd);
        return false;
    }
    voice->wav = wd;
    return true;
}

static bool materialize_programmable_wave(CoreMaterializeContext* ctx,
                                          const VoicegroupCoreBankResult* result,
                                          size_t slot,
                                          ToneData* voice)
{
    VoicegroupCoreProgrammableWaveProgram program = {0};
    if (!voicegroup_core_bank_result_program_programmable_wave(result, slot, &program))
        return false;

    voice->type = voicegroup_core_bank_result_program_type_code(result, slot);
    voice->key = program.key;
    voice->panSweep = program.pan ? (uint8_t)(0x80 | program.pan) : 0;
    voice->attack = program.attack;
    voice->decay = program.decay;
    voice->sustain = program.sustain;
    voice->release = program.release;

    char relPath[VG_MAX_PATH_LEN];
    read_core_relative_path(result, slot, relPath, sizeof(relPath));
    if (!relPath[0])
    {
        append_failure(ctx->report, slot, "", "programmable-wave asset has no source path");
        return !ctx->strict;
    }
    uint32_t* wave = voicegroup_loader_load_prog_wave(ctx->projectRoot, relPath, ctx->owner);
    if (!wave)
    {
        append_failure(ctx->report, slot, relPath, "programmable-wave asset could not be decoded");
        return !ctx->strict;
    }
    voice->wavePointer = wave;
    return true;
}

static bool materialize_square1(const VoicegroupCoreBankResult* result, size_t slot, ToneData* voice)
{
    VoicegroupCoreSquare1Program program = {0};
    if (!voicegroup_core_bank_result_program_square1(result, slot, &program))
        return false;
    voice->type = voicegroup_core_bank_result_program_type_code(result, slot);
    voice->key = program.key;
    voice->panSweep = program.sweep;
    voice->wavePointer = (uint32_t*)(uintptr_t)program.duty;
    voice->attack = program.attack;
    voice->decay = program.decay;
    voice->sustain = program.sustain;
    voice->release = program.release;
    return true;
}

static bool materialize_square2(const VoicegroupCoreBankResult* result, size_t slot, ToneData* voice)
{
    VoicegroupCoreSquare2Program program = {0};
    if (!voicegroup_core_bank_result_program_square2(result, slot, &program))
        return false;
    voice->type = voicegroup_core_bank_result_program_type_code(result, slot);
    voice->key = program.key;
    voice->wavePointer = (uint32_t*)(uintptr_t)program.duty;
    voice->attack = program.attack;
    voice->decay = program.decay;
    voice->sustain = program.sustain;
    voice->release = program.release;
    return true;
}

static bool materialize_noise(const VoicegroupCoreBankResult* result, size_t slot, ToneData* voice)
{
    VoicegroupCoreNoiseProgram program = {0};
    if (!voicegroup_core_bank_result_program_noise(result, slot, &program))
        return false;
    voice->type = voicegroup_core_bank_result_program_type_code(result, slot);
    voice->key = program.key;
    voice->wavePointer = (uint32_t*)(uintptr_t)program.period;
    voice->attack = program.attack;
    voice->decay = program.decay;
    voice->sustain = program.sustain;
    voice->release = program.release;
    return true;
}

static bool
materialize_keysplit(CoreMaterializeContext* ctx, const VoicegroupCoreBankResult* result, size_t slot, ToneData* voice)
{
    VoicegroupCoreKeysplitProgram program = {{0}};
    if (!voicegroup_core_bank_result_program_keysplit(result, slot, &program))
        return false;

    char subgroupName[VG_MAX_PATH_LEN];
    read_core_sub_voicegroup(result, slot, subgroupName, sizeof(subgroupName));
    voice->type = voicegroup_core_bank_result_program_type_code(result, slot);
    if (subgroupName[0] && !ctx->suppressSubgroups &&
        !voicegroup_load_core_subgroup(ctx, subgroupName, slot, (ToneData**)&voice->subGroup))
        return false;

    uint8_t* table = vg_malloc_array(VOICEGROUP_SIZE, sizeof(*table));
    if (!table)
        return false;
    memcpy(table, program.table, VOICEGROUP_SIZE);
    if (!register_keysplit_table(ctx->owner, table))
    {
        free(table);
        return false;
    }
    voice->keySplitTable = table;
    return true;
}

static bool materialize_keysplit_all(CoreMaterializeContext* ctx,
                                     const VoicegroupCoreBankResult* result,
                                     size_t slot,
                                     ToneData* voice)
{
    char subgroupName[VG_MAX_PATH_LEN];
    read_core_sub_voicegroup(result, slot, subgroupName, sizeof(subgroupName));
    voice->type = voicegroup_core_bank_result_program_type_code(result, slot);
    if (subgroupName[0] && !ctx->suppressSubgroups &&
        !voicegroup_load_core_subgroup(ctx, subgroupName, slot, (ToneData**)&voice->subGroup))
        return false;
    return true;
}

static bool
materialize_cry(CoreMaterializeContext* ctx, const VoicegroupCoreBankResult* result, size_t slot, ToneData* voice)
{
    voice->type = voicegroup_core_bank_result_program_type_code(result, slot);
    voice->key = 60;
    voice->attack = 0xff;
    voice->sustain = 0xff;
    char relPath[VG_MAX_PATH_LEN];
    read_core_relative_path(result, slot, relPath, sizeof(relPath));

    if (!relPath[0])
        return true;
    WaveData* wd = load_sample_cached(ctx, relPath, slot);
    if (!wd)
        return !ctx->strict;
    voice->wav = wd;
    return true;
}

static bool materialize_core_bank(CoreMaterializeContext* ctx,
                                  const VoicegroupCoreBankResult* result,
                                  ToneData voices[VOICEGROUP_SIZE],
                                  char names[VOICEGROUP_SIZE][VG_MAX_VOICE_SAMPLE_NAME])
{
    bool ok = true;
    size_t slotLimit = ctx->slotLimit < VOICEGROUP_SIZE ? ctx->slotLimit : VOICEGROUP_SIZE;
    for (size_t slot = 0; slot < slotLimit; slot++)
    {
        ToneData* voice = &voices[slot];
        char displayName[VG_MAX_VOICE_SAMPLE_NAME];
        read_core_display_name(result, slot, displayName, sizeof(displayName));
        set_slot_name(names, slot, displayName);

        bool slotOk = true;
        switch (voicegroup_core_bank_result_program_kind(result, slot))
        {
        case VOICEGROUP_CORE_PROGRAM_KIND_EMPTY:
            break;
        case VOICEGROUP_CORE_PROGRAM_KIND_DIRECT_SOUND:
            slotOk = materialize_directsound(ctx, result, slot, voice);
            break;
        case VOICEGROUP_CORE_PROGRAM_KIND_PROGRAMMABLE_WAVE:
            slotOk = materialize_programmable_wave(ctx, result, slot, voice);
            break;
        case VOICEGROUP_CORE_PROGRAM_KIND_SQUARE1:
            slotOk = materialize_square1(result, slot, voice);
            break;
        case VOICEGROUP_CORE_PROGRAM_KIND_SQUARE2:
            slotOk = materialize_square2(result, slot, voice);
            break;
        case VOICEGROUP_CORE_PROGRAM_KIND_NOISE:
            slotOk = materialize_noise(result, slot, voice);
            break;
        case VOICEGROUP_CORE_PROGRAM_KIND_KEYSPLIT:
            slotOk = materialize_keysplit(ctx, result, slot, voice);
            break;
        case VOICEGROUP_CORE_PROGRAM_KIND_KEYSPLIT_ALL:
            slotOk = materialize_keysplit_all(ctx, result, slot, voice);
            break;
        case VOICEGROUP_CORE_PROGRAM_KIND_CRY:
            slotOk = materialize_cry(ctx, result, slot, voice);
            break;
        }
        if (!slotOk)
            ok = false;
    }
    return ok;
}

bool voicegroup_materialize_core_bank(const char* projectRoot,
                                      const VoicegroupCoreProjectIndex* index,
                                      const VoicegroupCoreBankResult* result,
                                      LoadedVoiceGroup** out,
                                      VoicegroupMaterializationReport* report)
{
    if (!out || !projectRoot || !index || !result)
        return false;
    *out = NULL;
    LoadedVoiceGroup* vg = calloc(1, sizeof(*vg));
    if (!vg)
        return false;

    CoreMaterializeContext ctx = {
        .projectRoot = projectRoot,
        .index = index,
        .owner = vg,
        .report = report,
        .strict = report != NULL,
        .slotLimit = VOICEGROUP_SIZE,
    };
    vg_wave_cache_init(&ctx.waveCache);
    bool ok = materialize_core_bank(&ctx, result, vg->voices, vg->voiceSampleNames);
    vg_wave_cache_free(&ctx.waveCache);
    for (size_t i = 0; i < ctx.activeSubgroupCount; i++)
        free(ctx.activeSubgroups[i]);
    free(ctx.activeSubgroups);

    if (!ok)
    {
        voicegroup_free(vg);
        return false;
    }
    *out = vg;
    return true;
}
bool voicegroup_materializer_append_failure(VoicegroupMaterializationReport* report,
                                            size_t slot,
                                            const char* assetPath,
                                            const char* message)
{
    return append_failure(report, slot, assetPath, message);
}

bool voicegroup_materializer_subgroup_enter(CoreMaterializeContext* ctx, const char* name, size_t slot)
{
    return subgroup_enter(ctx, name, slot);
}

void voicegroup_materializer_subgroup_leave(CoreMaterializeContext* ctx)
{
    subgroup_leave(ctx);
}

bool voicegroup_materializer_register_subgroup(LoadedVoiceGroup* vg, ToneData* subgroup)
{
    return register_subgroup(vg, subgroup);
}

bool voicegroup_materializer_materialize_core_bank(CoreMaterializeContext* ctx,
                                                   const VoicegroupCoreBankResult* result,
                                                   ToneData voices[VOICEGROUP_SIZE],
                                                   char names[VOICEGROUP_SIZE][VG_MAX_VOICE_SAMPLE_NAME])
{
    return materialize_core_bank(ctx, result, voices, names);
}
