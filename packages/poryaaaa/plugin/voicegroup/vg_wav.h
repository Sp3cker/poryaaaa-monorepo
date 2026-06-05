#ifndef VG_WAV_H
#define VG_WAV_H

#include "voicegroup_types.h"   /* WaveData */
#include "vg_paths.h"           /* VG_MAX_PATH_LEN */

#include <stdint.h>

/* ---- Wave dedup cache ---- */

/*
 * Skips reloading the same .wav referenced from multiple voice slots
 * inside one voicegroup. Not persistent across voicegroup_load().
 */
#define VG_WAVE_CACHE_CAPACITY 128

typedef struct {
    char absPath[VG_MAX_PATH_LEN];
    WaveData *wd;
} WaveCacheEntry;

typedef struct {
    WaveCacheEntry entries[VG_WAVE_CACHE_CAPACITY];
    int count;
} WaveCache;

void vg_wave_cache_init(WaveCache *cache);
WaveData *vg_wave_cache_find(const WaveCache *cache, const char *absPath);
void vg_wave_cache_insert(WaveCache *cache, const char *absPath, WaveData *wd);

/* ---- Sample loaders ---- */

/*
 * Load a RIFF/WAVE sample from an absolute path. Parses fmt, smpl,
 * agbp, agbl, and data chunks; converts 8/16/24/32-bit integer PCM
 * and 32/64-bit float formats down to the GBA's 8-bit signed sample
 * representation. NULL on failure.
 */
WaveData *vg_load_wav_file(const char *absoluteWavPath);

/*
 * Load a raw GBA .bin sample (16-byte header + sample bytes) from a
 * project-relative path. NULL on failure.
 */
WaveData *vg_load_bin_sample(const char *projectRoot, const char *relativeBinPath);

/*
 * Primary sample loader: given a .bin reference, tries a sibling .wav
 * file first (same basename) and falls back to the .bin loader if the
 * .wav is missing or malformed. This is what voicegroup parsing
 * should call for DirectSound samples.
 */
WaveData *vg_load_sample(const char *projectRoot, const char *relativeBinPath);

/*
 * Load a programmable-wave .pcm file (16 bytes, 32 × 4-bit samples).
 * Returns a malloc'd uint32_t[4]; caller frees.
 */
uint32_t *vg_load_prog_wave(const char *projectRoot, const char *relativePath);

#endif /* VG_WAV_H */
