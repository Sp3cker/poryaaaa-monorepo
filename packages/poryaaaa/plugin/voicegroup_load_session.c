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

enum { ROUND_WAV = 0, ROUND_AIF = 1, ROUND_BIN = 2, ROUND_PROG = 3 };

static bool session_run_round(VgLoadSession* s, VgDedup* dedup, int kind, bool* waveDone)
{
    if (!s || !dedup)
        return false;
    if (dedup->count == 0)
        return true;
    if (kind < ROUND_WAV || kind > ROUND_PROG)
        return false;

    bool isProg = (kind == ROUND_PROG);
    size_t n = dedup->count;

    VoicegroupFileBlob* blobs = (VoicegroupFileBlob*)calloc(n, sizeof(VoicegroupFileBlob));
    if (!blobs)
        return false;

    char err[512];
    bool readOk = vg_batch_read(s->io, dedup, blobs, err, sizeof(err));
    if (!readOk)
    {
        vg_batch_release(s->io, blobs, n);
        free(blobs);
        return false;
    }

    void** decoded = (void**)calloc(n, sizeof(void*));
    void** finalForIdx = (void**)calloc(n, sizeof(void*));
    if (!decoded || !finalForIdx)
    {
        for (size_t i = 0; i < n; i++)
        {
            if (decoded && decoded[i])
            {
                free(decoded[i]);
            }
        }
        free(decoded);
        free(finalForIdx);
        vg_batch_release(s->io, blobs, n);
        free(blobs);
        return false;
    }

    bool ok = true;
    for (size_t i = 0; i < n && ok; i++)
    {
        if (!blobs[i].found || !blobs[i].data)
            continue;
        const char* path = dedup->paths[i];
        bool hardFailure = false;
        if (kind == ROUND_WAV)
            decoded[i] =
                vg_asset_decode_wav(blobs[i].data, blobs[i].size, path, &hardFailure);
        else if (kind == ROUND_AIF)
            decoded[i] =
                vg_asset_decode_aiff(blobs[i].data, blobs[i].size, path, &hardFailure);
        else if (kind == ROUND_BIN)
            decoded[i] =
                vg_asset_decode_bin(blobs[i].data, blobs[i].size, path, &hardFailure);
        else
            decoded[i] =
                vg_asset_decode_prog(blobs[i].data, blobs[i].size, path, &hardFailure);
        if (hardFailure)
            ok = false;
    }

    if (!isProg)
    {
        for (size_t b = 0; b < s->waveCount && ok; b++)
        {
            if (waveDone && waveDone[b])
                continue;
            int idx = -1;
            if (kind == ROUND_WAV)
                idx = s->waves[b].wavIdx;
            else if (kind == ROUND_AIF)
                idx = s->waves[b].aifIdx;
            else
                idx = s->waves[b].binIdx;
            if (idx < 0 || (size_t)idx >= n)
                continue;
            if (finalForIdx[idx])
            {
                *s->waves[b].slot = (WaveData*)finalForIdx[idx];
                if (waveDone)
                    waveDone[b] = true;
                continue;
            }
            if (!decoded[idx])
                continue;
            const char* path = dedup->paths[idx];
            WaveData* cached = wave_cache_find(s->cache, path);
            WaveData* wd = NULL;
            if (cached)
            {
                wd = cached;
                free(decoded[idx]);
                decoded[idx] = NULL;
            }
            else
            {
                wd = (WaveData*)decoded[idx];
                if (!session_register_wavedata(s->owner, wd))
                {
                    ok = false;
                    break;
                }
                wave_cache_insert(s->cache, path, wd);
                decoded[idx] = NULL;
            }
            finalForIdx[idx] = wd;
            *s->waves[b].slot = wd;
            if (waveDone)
                waveDone[b] = true;
        }
    }
    else
    {
        for (size_t b = 0; b < s->progCount && ok; b++)
        {
            int idx = s->progs[b].idx;
            if (idx < 0 || (size_t)idx >= n)
                continue;
            if (*s->progs[b].slot)
                continue;
            if (finalForIdx[idx])
            {
                *s->progs[b].slot = (uint32_t*)finalForIdx[idx];
                continue;
            }
            if (!decoded[idx])
                continue;
            uint32_t* pw = (uint32_t*)decoded[idx];
            if (!session_register_prog(s->owner, pw))
            {
                ok = false;
                break;
            }
            finalForIdx[idx] = pw;
            decoded[idx] = NULL;
            *s->progs[b].slot = pw;
        }
    }

    for (size_t i = 0; i < n; i++)
    {
        if (decoded[i])
        {
            free(decoded[i]);
        }
    }
    free(decoded);
    free(finalForIdx);
    vg_batch_release(s->io, blobs, n);
    free(blobs);
    return ok;
}

/* ---- public session API ---- */

void vg_load_session_init(
    VgLoadSession* s, const VoicegroupFileIo* io, LoadedVoiceGroup* owner, WaveCache* cache)
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

bool vg_load_session_add_wave(
    VgLoadSession* s,
    WaveData** slot,
    const char* wavAbs,
    const char* aifAbs,
    const char* binAbs)
{
    if (!s || !slot)
    {
        return false;
    }
    if (!path_valid_len(wavAbs) || !path_valid_len(aifAbs) || !path_valid_len(binAbs))
    {
        return false;
    }

    int wIdx = -1, aIdx = -1, bIdx = -1;
    if (wavAbs && wavAbs[0])
    {
        if (!vg_dedup_add(&s->wavDedup, wavAbs))
            return false;
        wIdx = vg_dedup_find(&s->wavDedup, wavAbs);
        if (wIdx < 0)
            return false;
    }
    if (aifAbs && aifAbs[0])
    {
        if (!vg_dedup_add(&s->aifDedup, aifAbs))
            return false;
        aIdx = vg_dedup_find(&s->aifDedup, aifAbs);
        if (aIdx < 0)
            return false;
    }
    if (binAbs && binAbs[0])
    {
        if (!vg_dedup_add(&s->binDedup, binAbs))
            return false;
        bIdx = vg_dedup_find(&s->binDedup, binAbs);
        if (bIdx < 0)
            return false;
    }
    if (wIdx < 0 && aIdx < 0 && bIdx < 0)
        return false;

    if (s->waveCount >= s->waveCap)
    {
        size_t nc = s->waveCap ? s->waveCap * 2 : 8;
        struct VgWaveBind* np = (struct VgWaveBind*)realloc(s->waves, nc * sizeof(*np));
        if (!np)
            return false;
        s->waves = np;
        s->waveCap = nc;
    }
    s->waves[s->waveCount].slot = slot;
    s->waves[s->waveCount].wavIdx = wIdx;
    s->waves[s->waveCount].aifIdx = aIdx;
    s->waves[s->waveCount].binIdx = bIdx;
    s->waveCount++;
    return true;
}

bool vg_load_session_add_prog(
    VgLoadSession* s, uint32_t** slot, const char* absPath)
{
    if (!s || !slot || !absPath || !absPath[0])
    {
        return false;
    }
    if (!path_valid_len(absPath))
    {
        return false;
    }
    if (!vg_dedup_add(&s->progDedup, absPath))
        return false;
    int idx = vg_dedup_find(&s->progDedup, absPath);
    if (idx < 0)
        return false;
    if (s->progCount >= s->progCap)
    {
        size_t nc = s->progCap ? s->progCap * 2 : 8;
        struct VgProgBind* np = (struct VgProgBind*)realloc(s->progs, nc * sizeof(*np));
        if (!np)
            return false;
        s->progs = np;
        s->progCap = nc;
    }
    s->progs[s->progCount].slot = slot;
    s->progs[s->progCount].idx = idx;
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

bool vg_load_session_push_location(
    VgLoadSession* s, const char* filePath, const char* label)
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

bool vg_load_session_is_active(
    const VgLoadSession* s, const char* filePath, const char* label)
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
