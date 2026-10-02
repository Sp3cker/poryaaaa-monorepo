#ifndef VOICEGROUP_LOAD_SESSION_H
#define VOICEGROUP_LOAD_SESSION_H

#include "voicegroup_loader.h"
#include "voicegroup_asset_batch.h"

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Per-load lookup of bank-owned WaveData; the cache never owns sample allocations.
 * Entries beyond WAVE_CACHE_CAPACITY remain bank-owned but are not cached. */
#define WAVE_CACHE_CAPACITY 128
#define WAVE_CACHE_MAX_PATH VG_MAX_PATH_LEN

typedef struct {
    char absPath[WAVE_CACHE_MAX_PATH];
    WaveData* wd;
} WaveCacheEntry;

typedef struct WaveCache {
    WaveCacheEntry entries[WAVE_CACHE_CAPACITY];
    int count;
} WaveCache;

void wave_cache_init(WaveCache* cache);
WaveData* wave_cache_find(const WaveCache* cache, const char* absPath);
void wave_cache_insert(WaveCache* cache, const char* absPath, WaveData* wd);

/* A load session owns candidate paths and typed destination bindings for one bank
 * or sample set. Checkpoints roll back planning only, before execution begins.
 * Candidate indices remain stable during execution. PCM formats run in priority
 * order; each round requests the union of candidates needed by unresolved bindings.
 * Transport buffers are transient, while selected samples are owned by the bank.
 * Programmable waves use a separate typed pass without PCM fallback state. */
#define VG_ACTIVE_LOC_CAP 32
typedef struct {
    char filePath[VG_MAX_PATH_LEN];
    char label[MAX_SYMBOL_LEN];
} VgActiveLoc;

typedef struct {
    size_t waveCount;
    size_t progCount;
    size_t wavDedupCount;
    size_t aifDedupCount;
    size_t binDedupCount;
    size_t progDedupCount;
} VgLoadSessionCheckpoint;

typedef struct VgLoadSession VgLoadSession;

struct VgWaveBind {
    WaveData** slot;
    int wavIdx;
    int aifIdx;
    int binIdx;
};

struct VgProgBind {
    uint32_t** slot;
    int idx;
};

struct VgLoadSession {
    const VoicegroupFileIo* io;
    LoadedVoiceGroup* owner;
    WaveCache* cache;

    struct VgWaveBind* waves;
    size_t waveCount;
    size_t waveCap;

    struct VgProgBind* progs;
    size_t progCount;
    size_t progCap;

    VgDedup wavDedup;
    VgDedup aifDedup;
    VgDedup binDedup;
    VgDedup progDedup;

    VgActiveLoc activeLocs[VG_ACTIVE_LOC_CAP];
    int activeCount;
};

void vg_load_session_init(VgLoadSession* s, const VoicegroupFileIo* io, LoadedVoiceGroup* owner, WaveCache* cache);
void vg_load_session_deinit(VgLoadSession* s);

/* Record a DirectSound sample binding. Any of wavAbs/aifAbs/binAbs may be
 * NULL or empty for that slot. Rejects path truncation (>= VG_MAX_PATH_LEN)
 * as a hard failure. Returns false on allocation or truncation failure; the
 * caller must treat it as a transactional hard failure and unwind the whole
 * load (no partial bank/set is published). */
bool vg_load_session_add_wave(
    VgLoadSession* s,
    WaveData** slot,
    const char* wavAbs,
    const char* aifAbs,
    const char* binAbs);

/* Record a programmable-wave binding. absPath must be non-empty. Same
 * truncation/allocation failure contract as add_wave. */
bool vg_load_session_add_prog(
    VgLoadSession* s, uint32_t** slot, const char* absPath);

/* Execute PCM fallback (WAV -> AIFF -> BIN), then programmable-wave reads.
 * Only needed paths are batched; missing or malformed assets permit fallback.
 * Decoded samples are registered once with the owner and shared by referring slots.
 * Returns false on allocation or required transport failure. Earlier rounds may
 * already have populated the owner: the caller MUST discard the entire bank/set
 * on failure, never publish partial results or retry this session.
 * The project context can start a fresh session after failure.
 */
bool vg_load_session_execute(VgLoadSession* s);

VgLoadSessionCheckpoint vg_load_session_checkpoint(const VgLoadSession* s);
void vg_load_session_rollback(VgLoadSession* s, VgLoadSessionCheckpoint cp);
bool vg_load_session_push_location(
    VgLoadSession* s, const char* filePath, const char* label);
void vg_load_session_pop_location(VgLoadSession* s);
bool vg_load_session_is_active(
    const VgLoadSession* s, const char* filePath, const char* label);

#ifdef __cplusplus
}
#endif

#endif /* VOICEGROUP_LOAD_SESSION_H */
