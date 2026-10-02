#include "voicegroup_load_session.h"

#include <assert.h>
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

/* ---- Stable candidate indices; only the transport span is compacted ---- */

typedef enum
{
    ROUND_WAV,
    ROUND_AIF,
    ROUND_BIN
} VgWaveFormat;

/* Owns transport storage only. Candidate strings and decoded samples live elsewhere. */
typedef struct
{
    bool* needed; /* Source-indexed mask, frozen before reading; NULL only when no paths are requested. */
    size_t sourceCount;
    const char** selectedPaths; /* Tail of the blobs allocation; strings remain session-owned. */
    VoicegroupFileBlob* blobs;  /* Owns the blob array and the borrowed-path span. */
    size_t readCount;           /* Number of set mask entries; blobs follow their ascending source order. */
} VgAssetReads;

/* Release every adapter blob, including partially populated failed batches. */
static void session_reads_cleanup(const VoicegroupFileIo* io, VgAssetReads* reads)
{
    if (reads->blobs)
    {
        vg_batch_release(io, reads->blobs, reads->readCount);
        free(reads->blobs);
    }
    free(reads->needed);
}

/* Union requests from all unresolved bindings without changing their candidate indices. */
static bool session_request_asset(VgAssetReads* reads, int index)
{
    if (index < 0 || (size_t)index >= reads->sourceCount)
        return true;
    if (!reads->needed)
    {
        reads->needed = calloc(reads->sourceCount, sizeof(*reads->needed));
        if (!reads->needed)
            return false;
    }
    if (!reads->needed[index])
    {
        reads->needed[index] = true;
        reads->readCount++;
    }
    return true;
}

/* Walk the frozen mask in source order, avoiding a separate index-map allocation. */
static bool session_read_assets(const VoicegroupFileIo* io, const VgDedup* source, VgAssetReads* reads)
{
    assert(source->count == reads->sourceCount);
    assert(reads->needed && reads->readCount && reads->readCount <= reads->sourceCount);
    const size_t bytesPerRead = sizeof(*reads->blobs) + sizeof(*reads->selectedPaths);
    if (reads->readCount > SIZE_MAX / bytesPerRead)
        return false;
    /* The blob array already needs storage; append the path span in that same allocation. */
    _Static_assert(sizeof(VoicegroupFileBlob) % _Alignof(const char*) == 0,
                   "the blob-array tail must align the path span");
    reads->blobs = calloc(reads->readCount, bytesPerRead);
    if (!reads->blobs)
        return false;
    reads->selectedPaths = (const char**)(reads->blobs + reads->readCount);
    size_t next = 0;
    for (size_t i = 0; i < reads->sourceCount; i++)
    {
        if (reads->needed[i])
        {
            assert(next < reads->readCount);
            reads->selectedPaths[next++] = vg_dedup_path(source, i);
        }
    }
    assert(next == reads->readCount);
    char error[512];
    return vg_batch_read(io, reads->selectedPaths, reads->readCount, reads->blobs, error, sizeof(error));
}

/* Keep PCM format-to-candidate selection identical during planning and binding. */
static int session_wave_index(const struct VgWaveBind* binding, VgWaveFormat format)
{
    switch (format)
    {
    case ROUND_WAV:
        return binding->wavIdx;
    case ROUND_AIF:
        return binding->aifIdx;
    case ROUND_BIN:
        return binding->binIdx;
    }
    return -1;
}

/* Adopt each decoded wave immediately; the result table only borrows bank-owned pointers. */
static bool session_decode_waves(
    VgLoadSession* s, const VgDedup* source, VgWaveFormat format, const VgAssetReads* reads, WaveData** waves)
{
    assert(source->count == reads->sourceCount);
    size_t next = 0;
    for (size_t i = 0; i < source->count; i++)
    {
        if (!reads->needed[i])
            continue;
        assert(next < reads->readCount);
        const VoicegroupFileBlob* blob = &reads->blobs[next++];
        if (!blob->found || !blob->data)
            continue;
        const char* path = vg_dedup_path(source, i);
        bool hardFailure = false;
        WaveData* wave = NULL;
        switch (format)
        {
        case ROUND_WAV:
            wave = vg_asset_decode_wav(blob->data, blob->size, path, &hardFailure);
            break;
        case ROUND_AIF:
            wave = vg_asset_decode_aiff(blob->data, blob->size, path, &hardFailure);
            break;
        case ROUND_BIN:
            wave = vg_asset_decode_bin(blob->data, blob->size, path, &hardFailure);
            break;
        }
        if (hardFailure)
        {
            free(wave);
            return false;
        }
        if (!wave)
            continue;
        WaveData* cached = wave_cache_find(s->cache, path);
        if (cached)
        {
            free(wave);
            waves[i] = cached;
        }
        else
        {
            if (!session_register_wavedata(s->owner, wave))
            {
                free(wave);
                return false;
            }
            wave_cache_insert(s->cache, path, wave);
            waves[i] = wave;
        }
    }
    assert(next == reads->readCount);
    return true;
}

/* Try one PCM format only for unresolved bindings, then fan out the selected shared waves. */
static bool session_run_wave_round(VgLoadSession* s, const VgDedup* source, VgWaveFormat format, bool* waveDone)
{
    VgAssetReads reads = {.sourceCount = source->count};
    WaveData** waves = NULL;
    bool ok = true;
    for (size_t i = 0; ok && i < s->waveCount; i++)
    {
        if (!waveDone[i])
            ok = session_request_asset(&reads, session_wave_index(&s->waves[i], format));
    }
    if (ok && reads.readCount)
    {
        waves = calloc(source->count, sizeof(*waves));
        ok = waves && session_read_assets(s->io, source, &reads) &&
             session_decode_waves(s, source, format, &reads, waves);
        for (size_t i = 0; ok && i < s->waveCount; i++)
        {
            if (waveDone[i])
                continue;
            struct VgWaveBind* binding = &s->waves[i];
            int index = session_wave_index(binding, format);
            if (index >= 0 && (size_t)index < source->count && waves[index])
            {
                *binding->slot = waves[index];
                waveDone[i] = true;
            }
        }
    }
    free(waves);
    session_reads_cleanup(s->io, &reads);
    return ok;
}

/* Programmable waves have one format and no PCM cache or fallback state. */
static bool session_run_prog_round(VgLoadSession* s)
{
    const VgDedup* source = &s->progDedup;
    VgAssetReads reads = {.sourceCount = source->count};
    uint32_t** waves = NULL;
    uint32_t** decoded = NULL;
    bool ok = true;
    for (size_t i = 0; ok && i < s->progCount; i++)
    {
        if (!*s->progs[i].slot)
            ok = session_request_asset(&reads, s->progs[i].idx);
    }
    if (ok && reads.readCount)
    {
        waves = calloc(source->count, sizeof(*waves));
        decoded = calloc(source->count, sizeof(*decoded));
        ok = waves && decoded && session_read_assets(s->io, source, &reads);
        size_t next = 0;
        for (size_t i = 0; ok && i < source->count; i++)
        {
            if (!reads.needed[i])
                continue;
            assert(next < reads.readCount);
            const VoicegroupFileBlob* blob = &reads.blobs[next++];
            if (!blob->found || !blob->data)
                continue;
            bool hardFailure = false;
            decoded[i] = vg_asset_decode_prog(blob->data, blob->size, vg_dedup_path(source, i), &hardFailure);
            ok = !hardFailure;
        }
        assert(!ok || next == reads.readCount);
        for (size_t i = 0; ok && i < s->progCount; i++)
        {
            struct VgProgBind* binding = &s->progs[i];
            int index = binding->idx;
            if (*binding->slot || index < 0 || (size_t)index >= source->count)
                continue;
            if (!waves[index] && decoded[index])
            {
                if (!session_register_prog(s->owner, decoded[index]))
                {
                    ok = false;
                    break;
                }
                waves[index] = decoded[index];
                decoded[index] = NULL;
            }
            *binding->slot = waves[index];
        }
    }
    /* A destination may have several recorded definitions: only the first success binds. */
    for (size_t i = 0; decoded && i < source->count; i++)
        free(decoded[i]);
    free(decoded);
    free(waves);
    session_reads_cleanup(s->io, &reads);
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
    *index = vg_dedup_add(dedup, path);
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

    bool ok = session_run_wave_round(s, &s->wavDedup, ROUND_WAV, waveDone);
    if (ok)
        ok = session_run_wave_round(s, &s->aifDedup, ROUND_AIF, waveDone);
    if (ok)
        ok = session_run_wave_round(s, &s->binDedup, ROUND_BIN, waveDone);
    if (ok)
        ok = session_run_prog_round(s);

    free(waveDone);
    return ok;
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
