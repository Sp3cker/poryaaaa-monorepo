#ifndef VG_WAV_H
#define VG_WAV_H

#include "voicegroup_types.h" /* WaveData */
#include "vg_paths.h"         /* VG_MAX_PATH_LEN */

#include <stddef.h>
#include <stdint.h>

/* ---- Wave dedup cache ---- */

/*
 * The cache is scoped to one bank materialization.  It owns only its path
 * keys; WaveData remains owned by LoadedVoiceGroup.
 */
typedef struct
{
    char* absPath;
    WaveData* wd;
} WaveCacheEntry;

typedef struct
{
    WaveCacheEntry* entries;
    size_t count;
    size_t capacity;
} WaveCache;

void vg_wave_cache_init(WaveCache* cache);
void vg_wave_cache_free(WaveCache* cache);
WaveData* vg_wave_cache_find(const WaveCache* cache, const char* absPath);
void vg_wave_cache_insert(WaveCache* cache, const char* absPath, WaveData* wd);

/* ---- Sample loaders ---- */

/*
 * Load a RIFF/WAVE sample from an absolute path. Parses fmt, smpl,
 * agbp, agbl, and data chunks; converts 8/16/24/32-bit integer PCM
 * and 32/64-bit float formats down to the GBA's 8-bit signed sample
 * representation. NULL on failure.
 */
WaveData* vg_load_wav_file(const char* absoluteWavPath);

/*
 * Load an AIFF sample from an absolute path. Supports the mono 8/16-bit
 * forms emitted by the legacy aif2pcm pipeline.
 */
WaveData* vg_load_aif_file(const char* absoluteAifPath);

/*
 * Load a raw GBA .bin sample (16-byte header + sample bytes) from a
 * project-relative path. NULL on failure.
 */
WaveData* vg_load_bin_sample(const char* projectRoot, const char* relativeBinPath);

/*
 * Primary sample loader: given a .bin reference, tries sibling .wav then
 * .aif files and finally falls back to the .bin loader. This is what
 * voicegroup parsing should call for DirectSound samples.
 */
WaveData* vg_load_sample(const char* projectRoot, const char* relativeBinPath);

/*
 * Load a programmable-wave .pcm file (16 bytes, 32 × 4-bit samples).
 * Returns a malloc'd uint32_t[4]; caller frees.
 */
uint32_t* vg_load_prog_wave(const char* projectRoot, const char* relativePath);

#endif /* VG_WAV_H */
