#include "voicegroup_load_session.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define VG_INITIAL_OWNER_CAP 64

/* ---- WaveCache ---- */

void wave_cache_init(WaveCache* cache)
{
    if (cache)
    {
        cache->count = 0;
    }
}

WaveData* wave_cache_find(const WaveCache* cache, const char* absPath)
{
    if (!cache || !absPath)
    {
        return NULL;
    }
    for (int i = 0; i < cache->count; i++)
    {
        if (strcmp(cache->entries[i].absPath, absPath) == 0)
        {
            return cache->entries[i].wd;
        }
    }
    return NULL;
}

void wave_cache_insert(WaveCache* cache, const char* absPath, WaveData* wd)
{
    if (!cache || !absPath || !wd)
    {
        return;
    }
    if (cache->count >= WAVE_CACHE_CAPACITY)
    {
        return;
    }
    strncpy(cache->entries[cache->count].absPath, absPath, WAVE_CACHE_MAX_PATH - 1);
    cache->entries[cache->count].absPath[WAVE_CACHE_MAX_PATH - 1] = '\0';
    cache->entries[cache->count].wd = wd;
    cache->count++;
}

/* ---- transactional owner registrars (session-private) ---- */

static bool session_register_wavedata(LoadedVoiceGroup* owner, WaveData* wd)
{
    if (!owner || !wd)
    {
        return false;
    }
    if (owner->waveDataCount >= owner->waveDataCapacity)
    {
        size_t nc = owner->waveDataCapacity ? (size_t)owner->waveDataCapacity * 2 : VG_INITIAL_OWNER_CAP;
        WaveData** np = (WaveData**)realloc(owner->waveDatas, nc * sizeof(WaveData*));
        if (!np)
            return false;
        owner->waveDatas = np;
        owner->waveDataCapacity = (int)nc;
    }
    owner->waveDatas[owner->waveDataCount++] = wd;
    return true;
}

static bool session_register_prog(LoadedVoiceGroup* owner, uint32_t* pw)
{
    if (!owner || !pw)
    {
        return false;
    }
    if (owner->progWaveCount >= owner->progWaveCapacity)
    {
        size_t nc = owner->progWaveCapacity ? (size_t)owner->progWaveCapacity * 2 : VG_INITIAL_OWNER_CAP;
        uint32_t** np = (uint32_t**)realloc(owner->progWaves, nc * sizeof(uint32_t*));
        if (!np)
            return false;
        owner->progWaves = np;
        owner->progWaveCapacity = (int)nc;
    }
    owner->progWaves[owner->progWaveCount++] = pw;
    return true;
}

/* ---- single parameterized round engine (wav → aif → bin → prog) ---- */

enum
{
    ROUND_WAV = 0,
    ROUND_AIF = 1,
    ROUND_BIN = 2,
    ROUND_PROG = 3
};

typedef struct
{
    VoicegroupFileBlob* blobs;
    void** decoded;
    void** finalForIdx;
    size_t count;
} VgRoundBuffers;

static void session_round_cleanup(const VoicegroupFileIo* io, VgRoundBuffers* round)
{
    if (round->decoded)
    {
        for (size_t i = 0; i < round->count; i++)
        {
            if (round->decoded[i])
            {
                free(round->decoded[i]);
            }
        }
    }
    free(round->decoded);
    free(round->finalForIdx);
    if (round->blobs)
    {
        vg_batch_release(io, round->blobs, round->count);
        free(round->blobs);
    }
}

static void* session_decode_asset(int kind, const VoicegroupFileBlob* blob, const char* path, bool* hardFailure)
{
    switch (kind)
    {
    case ROUND_WAV:
        return vg_asset_decode_wav(blob->data, blob->size, path, hardFailure);
    case ROUND_AIF:
        return vg_asset_decode_aiff(blob->data, blob->size, path, hardFailure);
    case ROUND_BIN:
        return vg_asset_decode_bin(blob->data, blob->size, path, hardFailure);
    case ROUND_PROG:
        return vg_asset_decode_prog(blob->data, blob->size, path, hardFailure);
    default:
        return NULL;
    }
}

static bool session_round_decode(VgRoundBuffers* round, const VgDedup* dedup, int kind)
{
    for (size_t i = 0; i < round->count; i++)
    {
        VoicegroupFileBlob* blob = &round->blobs[i];
        if (!blob->found)
        {
            continue;
        }
        if (!blob->data)
        {
            continue;
        }
        bool hardFailure = false;
        round->decoded[i] = session_decode_asset(kind, blob, dedup->paths[i], &hardFailure);
        if (hardFailure)
        {
            return false;
        }
    }
    return true;
}

static bool session_adopt_wave(VgLoadSession* s, const char* path, void** decoded, WaveData** result)
{
    WaveData* cached = wave_cache_find(s->cache, path);
    if (cached)
    {
        free(*decoded);
        *decoded = NULL;
        *result = cached;
        return true;
    }

    WaveData* wd = (WaveData*)*decoded;
    if (!session_register_wavedata(s->owner, wd))
    {
        return false;
    }
    wave_cache_insert(s->cache, path, wd);
    *decoded = NULL;
    *result = wd;
    return true;
}

static bool
session_bind_wave_round(VgLoadSession* s, const VgDedup* dedup, int kind, bool* waveDone, VgRoundBuffers* round)
{
    for (size_t i = 0; i < s->waveCount; i++)
    {
        if (waveDone && waveDone[i])
        {
            continue;
        }
        struct VgWaveBind* binding = &s->waves[i];
        int index = -1;
        switch (kind)
        {
        case ROUND_WAV:
            index = binding->wavIdx;
            break;
        case ROUND_AIF:
            index = binding->aifIdx;
            break;
        case ROUND_BIN:
            index = binding->binIdx;
            break;
        }
        if (index < 0 || (size_t)index >= round->count)
        {
            continue;
        }
        WaveData* wd = (WaveData*)round->finalForIdx[index];
        if (!wd)
        {
            if (!round->decoded[index])
            {
                continue;
            }
            if (!session_adopt_wave(s, dedup->paths[index], &round->decoded[index], &wd))
            {
                return false;
            }
            round->finalForIdx[index] = wd;
        }
        *binding->slot = wd;
        if (waveDone)
        {
            waveDone[i] = true;
        }
    }
    return true;
}

static bool session_bind_prog_round(VgLoadSession* s, VgRoundBuffers* round)
{
    for (size_t i = 0; i < s->progCount; i++)
    {
        struct VgProgBind* binding = &s->progs[i];
        int index = binding->idx;
        if (index < 0 || (size_t)index >= round->count)
        {
            continue;
        }
        if (*binding->slot)
        {
            continue;
        }
        uint32_t* pw = (uint32_t*)round->finalForIdx[index];
        if (!pw)
        {
            if (!round->decoded[index])
            {
                continue;
            }
            pw = (uint32_t*)round->decoded[index];
            if (!session_register_prog(s->owner, pw))
            {
                return false;
            }
            round->finalForIdx[index] = pw;
            round->decoded[index] = NULL;
        }
        *binding->slot = pw;
    }
    return true;
}

static bool session_run_round(VgLoadSession* s, VgDedup* dedup, int kind, bool* waveDone)
{
    if (!s)
    {
        return false;
    }
    if (!dedup)
    {
        return false;
    }
    if (dedup->count == 0)
    {
        return true;
    }
    if (kind < ROUND_WAV)
    {
        return false;
    }
    if (kind > ROUND_PROG)
    {
        return false;
    }

    VgRoundBuffers round = {0};
    round.count = dedup->count;
    round.blobs = (VoicegroupFileBlob*)calloc(dedup->count, sizeof(*round.blobs));
    bool ok = round.blobs != NULL;
    char err[512];
    if (ok)
    {
        ok = vg_batch_read(s->io, dedup, round.blobs, err, sizeof(err));
    }
    if (ok)
    {
        round.decoded = (void**)calloc(round.count, sizeof(*round.decoded));
        if (round.decoded)
        {
            round.finalForIdx = (void**)calloc(round.count, sizeof(*round.finalForIdx));
        }
        ok = round.finalForIdx != NULL;
    }
    if (ok)
    {
        ok = session_round_decode(&round, dedup, kind);
    }
    if (ok)
    {
        if (kind == ROUND_PROG)
        {
            ok = session_bind_prog_round(s, &round);
        }
        else
        {
            ok = session_bind_wave_round(s, dedup, kind, waveDone, &round);
        }
    }
    session_round_cleanup(s->io, &round);
    return ok;
}

/* ---- public session API ---- */

void vg_load_session_init(VgLoadSession* s, const VoicegroupFileIo* io, LoadedVoiceGroup* owner, WaveCache* cache)
{
    if (!s)
    {
        return;
    }
    memset(s, 0, sizeof(*s));
    s->io = io;
    s->owner = owner;
    s->cache = cache;
    vg_dedup_init(&s->wavDedup);
    vg_dedup_init(&s->aifDedup);
    vg_dedup_init(&s->binDedup);
    vg_dedup_init(&s->progDedup);
    s->waves = NULL;
    s->waveCount = s->waveCap = 0;
    s->progs = NULL;
    s->progCount = s->progCap = 0;
}

void vg_load_session_deinit(VgLoadSession* s)
{
    if (!s)
    {
        return;
    }
    free(s->waves);
    free(s->progs);
    vg_dedup_deinit(&s->wavDedup);
    vg_dedup_deinit(&s->aifDedup);
    vg_dedup_deinit(&s->binDedup);
    vg_dedup_deinit(&s->progDedup);
    memset(s, 0, sizeof(*s));
}

static bool path_valid_len(const char* p)
{
    if (!p || !p[0])
    {
        return true;
    }
    return strlen(p) < (size_t)VG_MAX_PATH_LEN;
}

static bool session_register_dedup_path(VgDedup* dedup, const char* path, int* index)
{
    *index = -1;
    if (!path)
    {
        return true;
    }
    if (!path[0])
    {
        return true;
    }
    if (!vg_dedup_add(dedup, path))
    {
        return false;
    }
    *index = vg_dedup_find(dedup, path);
    return *index >= 0;
}

static bool session_register_wave_paths(VgLoadSession* s,
                                        const char* wavAbs,
                                        const char* aifAbs,
                                        const char* binAbs,
                                        int* wavIndex,
                                        int* aifIndex,
                                        int* binIndex)
{
    if (!session_register_dedup_path(&s->wavDedup, wavAbs, wavIndex))
    {
        return false;
    }
    if (!session_register_dedup_path(&s->aifDedup, aifAbs, aifIndex))
    {
        return false;
    }
    if (!session_register_dedup_path(&s->binDedup, binAbs, binIndex))
    {
        return false;
    }
    return *wavIndex >= 0 || *aifIndex >= 0 || *binIndex >= 0;
}

static bool session_ensure_wave_binding_capacity(VgLoadSession* s)
{
    if (s->waveCount < s->waveCap)
    {
        return true;
    }
    size_t newCapacity = s->waveCap ? s->waveCap * 2 : 8;
    struct VgWaveBind* bindings = (struct VgWaveBind*)realloc(s->waves, newCapacity * sizeof(*bindings));
    if (!bindings)
    {
        return false;
    }
    s->waves = bindings;
    s->waveCap = newCapacity;
    return true;
}

static bool session_ensure_prog_binding_capacity(VgLoadSession* s)
{
    if (s->progCount < s->progCap)
    {
        return true;
    }
    size_t newCapacity = s->progCap ? s->progCap * 2 : 8;
    struct VgProgBind* bindings = (struct VgProgBind*)realloc(s->progs, newCapacity * sizeof(*bindings));
    if (!bindings)
    {
        return false;
    }
    s->progs = bindings;
    s->progCap = newCapacity;
    return true;
}

bool vg_load_session_add_wave(
    VgLoadSession* s, WaveData** slot, const char* wavAbs, const char* aifAbs, const char* binAbs)
{
    if (!s || !slot)
    {
        return false;
    }
    if (!path_valid_len(wavAbs) || !path_valid_len(aifAbs) || !path_valid_len(binAbs))
    {
        return false;
    }

    int wavIndex;
    int aifIndex;
    int binIndex;
    if (!session_register_wave_paths(s, wavAbs, aifAbs, binAbs, &wavIndex, &aifIndex, &binIndex))
    {
        return false;
    }
    if (!session_ensure_wave_binding_capacity(s))
    {
        return false;
    }
    struct VgWaveBind* binding = &s->waves[s->waveCount];
    binding->slot = slot;
    binding->wavIdx = wavIndex;
    binding->aifIdx = aifIndex;
    binding->binIdx = binIndex;
    s->waveCount++;
    return true;
}

bool vg_load_session_add_prog(VgLoadSession* s, uint32_t** slot, const char* absPath)
{
    if (!s || !slot || !absPath || !absPath[0])
    {
        return false;
    }
    if (!path_valid_len(absPath))
    {
        return false;
    }
    int index;
    if (!session_register_dedup_path(&s->progDedup, absPath, &index))
    {
        return false;
    }
    if (!session_ensure_prog_binding_capacity(s))
    {
        return false;
    }
    struct VgProgBind* binding = &s->progs[s->progCount];
    binding->slot = slot;
    binding->idx = index;
    s->progCount++;
    return true;
}

bool vg_load_session_execute(VgLoadSession* s)
{
    if (!s || !s->io || !s->owner || !s->cache)
    {
        return false;
    }

    bool* waveDone = NULL;
    if (s->waveCount)
    {
        waveDone = (bool*)calloc(s->waveCount, sizeof(bool));
        if (!waveDone)
            return false;
    }

    bool ok = true;
    if (ok)
        ok = session_run_round(s, &s->wavDedup, ROUND_WAV, waveDone);
    if (ok)
        ok = session_run_round(s, &s->aifDedup, ROUND_AIF, waveDone);
    if (ok)
        ok = session_run_round(s, &s->binDedup, ROUND_BIN, waveDone);
    if (ok)
        ok = session_run_round(s, &s->progDedup, ROUND_PROG, NULL);

    free(waveDone);
    return ok;
}

static void vg_dedup_truncate(VgDedup* d, size_t newCount)
{
    if (!d || newCount >= d->count)
    {
        return;
    }
    for (size_t i = newCount; i < d->count; i++)
    {
        free(d->paths[i]);
    }
    d->count = newCount;
}

VgLoadSessionCheckpoint vg_load_session_checkpoint(const VgLoadSession* s)
{
    VgLoadSessionCheckpoint cp = {0};
    if (!s)
    {
        return cp;
    }
    cp.waveCount = s->waveCount;
    cp.progCount = s->progCount;
    cp.wavDedupCount = s->wavDedup.count;
    cp.aifDedupCount = s->aifDedup.count;
    cp.binDedupCount = s->binDedup.count;
    cp.progDedupCount = s->progDedup.count;
    return cp;
}

void vg_load_session_rollback(VgLoadSession* s, VgLoadSessionCheckpoint cp)
{
    if (!s)
    {
        return;
    }
    if (s->waveCount > cp.waveCount)
        s->waveCount = cp.waveCount;
    if (s->progCount > cp.progCount)
        s->progCount = cp.progCount;
    vg_dedup_truncate(&s->wavDedup, cp.wavDedupCount);
    vg_dedup_truncate(&s->aifDedup, cp.aifDedupCount);
    vg_dedup_truncate(&s->binDedup, cp.binDedupCount);
    vg_dedup_truncate(&s->progDedup, cp.progDedupCount);
}

bool vg_load_session_push_location(VgLoadSession* s, const char* filePath, const char* label)
{
    if (!s || !filePath || s->activeCount >= VG_ACTIVE_LOC_CAP)
    {
        return false;
    }
    VgActiveLoc* loc = &s->activeLocs[s->activeCount];
    strncpy(loc->filePath, filePath, VG_MAX_PATH_LEN - 1);
    loc->filePath[VG_MAX_PATH_LEN - 1] = '\0';
    if (label)
    {
        strncpy(loc->label, label, MAX_SYMBOL_LEN - 1);
        loc->label[MAX_SYMBOL_LEN - 1] = '\0';
    }
    else
        loc->label[0] = '\0';
    s->activeCount++;
    return true;
}

void vg_load_session_pop_location(VgLoadSession* s)
{
    if (!s || s->activeCount <= 0)
    {
        return;
    }
    s->activeCount--;
    memset(&s->activeLocs[s->activeCount], 0, sizeof(VgActiveLoc));
}

bool vg_load_session_is_active(const VgLoadSession* s, const char* filePath, const char* label)
{
    if (!s || !filePath)
    {
        return false;
    }
    const char* wantLabel = label ? label : "";
    for (int i = 0; i < s->activeCount; i++)
    {
        if (strcmp(s->activeLocs[i].filePath, filePath) == 0 && strcmp(s->activeLocs[i].label, wantLabel) == 0)
            return true;
    }
    return false;
}
