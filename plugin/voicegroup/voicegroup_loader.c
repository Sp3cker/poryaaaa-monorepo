#include "voicegroup_loader.h"

#include "vg_discovery.h"
#include "vg_keysplit.h"
#include "vg_log.h"
#include "vg_paths.h"
#include "vg_symbols.h"
#include "vg_wav.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <dirent.h>
#include <sys/stat.h>

#define INITIAL_CAPACITY 64

typedef struct {
    char filePath[VG_MAX_PATH_LEN];
    char label[VG_MAX_SYMBOL_LEN];  /* non-empty if inside a monolithic file */
    int found;
} VoicegroupLocation;

static int parse_voicegroup_file(const char *projectRoot, const char *filePath,
                                  const char *startLabel,
                                  LoadedVoiceGroup *vg,
                                  const SymbolMap *dsMap, const SymbolMap *pwMap,
                                  const KeySplitMap *ksMap,
                                  const ProjectDiscovery *disc,
                                  WaveCache *waveCache);

/* Helper: register a WaveData in the loaded voicegroup for later cleanup */
static void vg_register_wavedata(LoadedVoiceGroup *vg, WaveData *wd)
{
    if (vg->waveDataCount >= vg->waveDataCapacity) {
        vg->waveDataCapacity = vg->waveDataCapacity ? vg->waveDataCapacity * 2 : INITIAL_CAPACITY;
        vg->waveDatas = realloc(vg->waveDatas, sizeof(WaveData *) * vg->waveDataCapacity);
    }
    vg->waveDatas[vg->waveDataCount++] = wd;
}

static void vg_register_progwave(LoadedVoiceGroup *vg, uint32_t *pw)
{
    if (vg->progWaveCount >= vg->progWaveCapacity) {
        vg->progWaveCapacity = vg->progWaveCapacity ? vg->progWaveCapacity * 2 : INITIAL_CAPACITY;
        vg->progWaves = realloc(vg->progWaves, sizeof(uint32_t *) * vg->progWaveCapacity);
    }
    vg->progWaves[vg->progWaveCount++] = pw;
}

static void vg_register_subgroup(LoadedVoiceGroup *vg, ToneData *sg)
{
    if (vg->subGroupCount >= vg->subGroupCapacity) {
        vg->subGroupCapacity = vg->subGroupCapacity ? vg->subGroupCapacity * 2 : INITIAL_CAPACITY;
        vg->subGroups = realloc(vg->subGroups, sizeof(ToneData *) * vg->subGroupCapacity);
    }
    vg->subGroups[vg->subGroupCount++] = sg;
}

static void vg_register_keysplittable(LoadedVoiceGroup *vg, uint8_t *ks)
{
    if (vg->keySplitTableCount >= vg->keySplitTableCapacity) {
        vg->keySplitTableCapacity = vg->keySplitTableCapacity ? vg->keySplitTableCapacity * 2 : INITIAL_CAPACITY;
        vg->keySplitTables = realloc(vg->keySplitTables, sizeof(uint8_t *) * vg->keySplitTableCapacity);
    }
    vg->keySplitTables[vg->keySplitTableCount++] = ks;
}


/* ---- Sample fallback resolution ---- */

/*
 * Try to find and load a .wav sample by searching discovered wav directories.
 */
static WaveData *resolve_sample_from_wav_dirs(const char *symbol,
                                               const ProjectDiscovery *disc)
{
    for (int i = 0; i < disc->wavSampleDirs.count; i++) {
        char wavPath[VG_MAX_PATH_LEN];
        snprintf(wavPath, sizeof(wavPath), "%s%c%s.wav", disc->wavSampleDirs.paths[i], VG_PATH_SEP, symbol);
        WaveData *wd = vg_load_wav_file(wavPath);
        if (wd) return wd;
    }
    return NULL;
}

/*
 * Unified sample resolution: try symbol map first, then fallback to wav dirs.
 * Uses waveCache to avoid loading the same file more than once.
 * Registers newly loaded WaveData with vg; cache hits are NOT re-registered.
 */
static WaveData *resolve_and_load_sample(const char *projectRoot, const char *symbol,
                                          const SymbolMap *dsMap, const ProjectDiscovery *disc,
                                          LoadedVoiceGroup *vg, WaveCache *waveCache)
{
    const char *samplePath = vg_symbol_map_find(dsMap, symbol);
    if (samplePath) {
        /* Build the absolute .wav path to use as cache key */
        char relWavPath[VG_MAX_PATH_LEN];
        strncpy(relWavPath, samplePath, VG_MAX_PATH_LEN - 1);
        relWavPath[VG_MAX_PATH_LEN - 1] = '\0';
        size_t pathLen = strlen(relWavPath);
        if (pathLen >= 4 && strcmp(relWavPath + pathLen - 4, ".bin") == 0) {
            relWavPath[pathLen - 3] = 'w';
            relWavPath[pathLen - 2] = 'a';
            relWavPath[pathLen - 1] = 'v';
        }
        char absWavPath[VG_MAX_PATH_LEN];
        vg_build_path(absWavPath, sizeof(absWavPath), projectRoot, relWavPath);

        WaveData *cached = vg_wave_cache_find(waveCache, absWavPath);
        if (cached) return cached;

        WaveData *wd = vg_load_sample(projectRoot, samplePath);
        if (wd) {
            vg_register_wavedata(vg, wd);
            vg_wave_cache_insert(waveCache, absWavPath, wd);
            return wd;
        }
    }
    /* Fallback: search wav directories */
    if (disc) {
        for (int i = 0; i < disc->wavSampleDirs.count; i++) {
            char wavPath[VG_MAX_PATH_LEN];
            snprintf(wavPath, sizeof(wavPath), "%s%c%s.wav",
                     disc->wavSampleDirs.paths[i], VG_PATH_SEP, symbol);
            WaveData *cached = vg_wave_cache_find(waveCache, wavPath);
            if (cached) return cached;
            WaveData *wd = vg_load_wav_file(wavPath);
            if (wd) {
                vg_register_wavedata(vg, wd);
                vg_wave_cache_insert(waveCache, wavPath, wd);
                return wd;
            }
        }
    }
    return NULL;
}

/* ---- Flexible voicegroup finding ---- */

/* Returns 1 if the last path component of dirPath equals name. */
static int dir_last_component_is(const char *dirPath, const char *name)
{
    size_t dlen = strlen(dirPath);
    size_t nlen = strlen(name);
    if (nlen > dlen) return 0;
    const char *tail = dirPath + dlen - nlen;
    if (strcmp(tail, name) != 0) return 0;
    if (tail == dirPath) return 1;
    char c = *(tail - 1);
    return c == '/' || c == '\\';
}

/*
 * Search for a voicegroup by name across all discovered locations.
 */
static VoicegroupLocation find_voicegroup(const char *projectRoot,
                                           const char *vgName,
                                           const ProjectDiscovery *disc)
{
    VoicegroupLocation loc;
    memset(&loc, 0, sizeof(loc));

    char path[VG_MAX_PATH_LEN];

    /* 1. Individual files in discovered voicegroup directories */
    for (int i = 0; i < disc->voicegroupDirs.count; i++) {
        /* Try <dir>/<name>.inc */
        snprintf(path, sizeof(path), "%s%c%s.inc", disc->voicegroupDirs.paths[i], VG_PATH_SEP, vgName);
        if (vg_file_exists(path)) {
            strncpy(loc.filePath, path, VG_MAX_PATH_LEN - 1);
            loc.found = 1;
            return loc;
        }
        /* Try <dir>/<name>.s */
        snprintf(path, sizeof(path), "%s%c%s.s", disc->voicegroupDirs.paths[i], VG_PATH_SEP, vgName);
        if (vg_file_exists(path)) {
            strncpy(loc.filePath, path, VG_MAX_PATH_LEN - 1);
            loc.found = 1;
            return loc;
        }
    }

    /* 2. Keysplit/drumset suffix conventions.
     *
     * IMPORTANT: only search inside directories whose last path component is
     * "keysplits" (or "drumsets"), and also try an explicit
     * <voicegroupDir>/keysplits/<base>.inc probe.  Searching every voicegroup
     * dir would find the *main* <base>.inc file (e.g. petalburg.inc) instead
     * of the keysplit sub-voicegroup, causing infinite recursion.
     */

    {
        const char *suffix = strstr(vgName, "_keysplit");
        if (suffix) {
            char baseName[VG_MAX_SYMBOL_LEN];
            int baseLen = (int)(suffix - vgName);
            if (baseLen > 0 && baseLen < VG_MAX_SYMBOL_LEN) {
                memcpy(baseName, vgName, baseLen);
                baseName[baseLen] = '\0';
                /* Explicit <dir>/keysplits/<base>.inc probe for each voicegroup dir */
                for (int i = 0; i < disc->voicegroupDirs.count; i++) {
                    snprintf(path, sizeof(path), "%s%ckeysplits%c%s.inc",
                             disc->voicegroupDirs.paths[i], VG_PATH_SEP, VG_PATH_SEP, baseName);
                    if (vg_file_exists(path)) {
                        strncpy(loc.filePath, path, VG_MAX_PATH_LEN - 1);
                        loc.found = 1;
                        return loc;
                    }
                    snprintf(path, sizeof(path), "%s%ckeysplits%c%s.s",
                             disc->voicegroupDirs.paths[i], VG_PATH_SEP, VG_PATH_SEP, baseName);
                    if (vg_file_exists(path)) {
                        strncpy(loc.filePath, path, VG_MAX_PATH_LEN - 1);
                        loc.found = 1;
                        return loc;
                    }
                }
                /* Also check dirs that are themselves named "keysplits" */
                for (int i = 0; i < disc->voicegroupDirs.count; i++) {
                    if (!dir_last_component_is(disc->voicegroupDirs.paths[i], "keysplits"))
                        continue;
                    snprintf(path, sizeof(path), "%s%c%s.inc", disc->voicegroupDirs.paths[i], VG_PATH_SEP, baseName);
                    if (vg_file_exists(path)) {
                        strncpy(loc.filePath, path, VG_MAX_PATH_LEN - 1);
                        loc.found = 1;
                        return loc;
                    }
                    snprintf(path, sizeof(path), "%s%c%s.s", disc->voicegroupDirs.paths[i], VG_PATH_SEP, baseName);
                    if (vg_file_exists(path)) {
                        strncpy(loc.filePath, path, VG_MAX_PATH_LEN - 1);
                        loc.found = 1;
                        return loc;
                    }
                }
            }
        }
    }
    {
        const char *suffix = strstr(vgName, "_drumset");
        if (suffix) {
            char baseName[VG_MAX_SYMBOL_LEN];
            int baseLen = (int)(suffix - vgName);
            if (baseLen > 0 && baseLen < VG_MAX_SYMBOL_LEN) {
                memcpy(baseName, vgName, baseLen);
                baseName[baseLen] = '\0';
                /* Explicit <dir>/drumsets/<base>.inc probe for each voicegroup dir */
                for (int i = 0; i < disc->voicegroupDirs.count; i++) {
                    snprintf(path, sizeof(path), "%s%cdrumsets%c%s.inc",
                             disc->voicegroupDirs.paths[i], VG_PATH_SEP, VG_PATH_SEP, baseName);
                    if (vg_file_exists(path)) {
                        strncpy(loc.filePath, path, VG_MAX_PATH_LEN - 1);
                        loc.found = 1;
                        return loc;
                    }
                    snprintf(path, sizeof(path), "%s%cdrumsets%c%s.s",
                             disc->voicegroupDirs.paths[i], VG_PATH_SEP, VG_PATH_SEP, baseName);
                    if (vg_file_exists(path)) {
                        strncpy(loc.filePath, path, VG_MAX_PATH_LEN - 1);
                        loc.found = 1;
                        return loc;
                    }
                }
                /* Also check dirs that are themselves named "drumsets" */
                for (int i = 0; i < disc->voicegroupDirs.count; i++) {
                    if (!dir_last_component_is(disc->voicegroupDirs.paths[i], "drumsets"))
                        continue;
                    snprintf(path, sizeof(path), "%s%c%s.inc", disc->voicegroupDirs.paths[i], VG_PATH_SEP, baseName);
                    if (vg_file_exists(path)) {
                        strncpy(loc.filePath, path, VG_MAX_PATH_LEN - 1);
                        loc.found = 1;
                        return loc;
                    }
                    snprintf(path, sizeof(path), "%s%c%s.s", disc->voicegroupDirs.paths[i], VG_PATH_SEP, baseName);
                    if (vg_file_exists(path)) {
                        strncpy(loc.filePath, path, VG_MAX_PATH_LEN - 1);
                        loc.found = 1;
                        return loc;
                    }
                }
            }
        }
    }


    /* 3. Also try vg_<name>.s and vg_<name>.inc patterns (eventide convention) */
    for (int i = 0; i < disc->voicegroupDirs.count; i++) {
        snprintf(path, sizeof(path), "%s%cvg_%s.inc", disc->voicegroupDirs.paths[i], VG_PATH_SEP, vgName);
        if (vg_file_exists(path)) {
            strncpy(loc.filePath, path, VG_MAX_PATH_LEN - 1);
            loc.found = 1;
            return loc;
        }
        snprintf(path, sizeof(path), "%s%cvg_%s.s", disc->voicegroupDirs.paths[i], VG_PATH_SEP, vgName);
        if (vg_file_exists(path)) {
            strncpy(loc.filePath, path, VG_MAX_PATH_LEN - 1);
            loc.found = 1;
            return loc;
        }
    }

    /* 4. Monolithic files: scan for <name>:: label */
    for (int i = 0; i < disc->monolithicVGFiles.count; i++) {
        FILE *f = fopen(disc->monolithicVGFiles.paths[i], "r");
        if (!f) continue;

        char searchLabel[VG_MAX_SYMBOL_LEN + 4];
        snprintf(searchLabel, sizeof(searchLabel), "%s::", vgName);

        char line[VG_MAX_LINE];
        while (fgets(line, sizeof(line), f)) {
            vg_strip_comment(line);
            char *trimmed = vg_ltrim(line);
            if (strstr(trimmed, searchLabel) == trimmed) {
                strncpy(loc.filePath, disc->monolithicVGFiles.paths[i], VG_MAX_PATH_LEN - 1);
                strncpy(loc.label, vgName, VG_MAX_SYMBOL_LEN - 1);
                loc.found = 1;
                fclose(f);
                return loc;
            }
        }
        fclose(f);
    }

    return loc;
}

/* ---- Voicegroup parsing ---- */

/*
 * Load a sub-voicegroup (for keysplit/keysplit_all references).
 */
static ToneData *load_sub_voicegroup(const char *projectRoot, const char *vgSymbol,
                                      LoadedVoiceGroup *vg,
                                      const SymbolMap *dsMap, const SymbolMap *pwMap,
                                      const KeySplitMap *ksMap,
                                      const ProjectDiscovery *disc,
                                      WaveCache *waveCache)
{
    const char *name = vgSymbol;
    if (strncmp(name, "voicegroup_", 11) == 0)
        name += 11;

    VoicegroupLocation loc = find_voicegroup(projectRoot, name, disc);
    if (!loc.found) {
        vg_err("cannot find sub-voicegroup '%s'", vgSymbol);
        return NULL;
    }

    ToneData *subVg = calloc(VOICEGROUP_SIZE, sizeof(ToneData));
    if (!subVg) return NULL;

    ToneData savedVoices[VOICEGROUP_SIZE];
    memcpy(savedVoices, vg->voices, sizeof(savedVoices));
    memset(vg->voices, 0, sizeof(vg->voices));

    const char *startLabel = loc.label[0] ? loc.label : NULL;
    if (parse_voicegroup_file(projectRoot, loc.filePath, startLabel,
                               vg, dsMap, pwMap, ksMap, disc, waveCache) != 0) {
        free(subVg);
        memcpy(vg->voices, savedVoices, sizeof(savedVoices));
        return NULL;
    }

    memcpy(subVg, vg->voices, sizeof(ToneData) * VOICEGROUP_SIZE);
    memcpy(vg->voices, savedVoices, sizeof(savedVoices));

    vg_register_subgroup(vg, subVg);
    return subVg;
}

/*
 * Parse a voicegroup file and populate the ToneData array.
 *
 * When startLabel is non-NULL, scanning starts at the "<startLabel>::" label
 * and stops when a new label or .align 2 is encountered (monolithic file mode).
 * When startLabel is NULL, the entire file is parsed (individual file mode).
 */
static int parse_voicegroup_file(const char *projectRoot, const char *filePath,
                                  const char *startLabel,
                                  LoadedVoiceGroup *vg,
                                  const SymbolMap *dsMap, const SymbolMap *pwMap,
                                  const KeySplitMap *ksMap,
                                  const ProjectDiscovery *disc,
                                  WaveCache *waveCache)
{
    vg_log("parse_voicegroup_file: '%s' label='%s'", filePath, startLabel ? startLabel : "(none)");
    FILE *f = fopen(filePath, "r");
    if (!f) {
        vg_err("cannot open %s", filePath);
        return -1;
    }

    char line[VG_MAX_LINE];
    int voiceIndex = 0;
    int inSection = (startLabel == NULL); /* if no startLabel, parse from the beginning */
    int voicesParsedInSection = 0;

    /* If startLabel is set, build the search string */
    char searchLabel[VG_MAX_SYMBOL_LEN + 4];
    if (startLabel) {
        snprintf(searchLabel, sizeof(searchLabel), "%s::", startLabel);
    }

    while (fgets(line, sizeof(line), f) && voiceIndex < VOICEGROUP_SIZE) {
        vg_strip_comment(line);
        vg_rtrim(line);
        char *trimmed = vg_ltrim(line);

        if (trimmed[0] == '\0')
            continue;

        /* When looking for a start label, skip until we find it */
        if (startLabel && !inSection) {
            if (strstr(trimmed, searchLabel) == trimmed) {
                inSection = 1;
            }
            continue;
        }

        /* In monolithic mode, stop at the next label or .align 2 after parsing voices */
        if (startLabel && inSection && voicesParsedInSection > 0) {
            /* Check for a new label (word followed by ::) */
            char *cc = strstr(trimmed, "::");
            if (cc && cc > trimmed && !isspace((unsigned char)trimmed[0])) {
                break;
            }
            /* Check for .align 2 which separates voicegroups */
            if (strncmp(trimmed, ".align", 6) == 0) {
                break;
            }
        }

        /* Parse voice_group declaration for starting_note offset */
        if (strncmp(trimmed, "voice_group ", 12) == 0) {
            char vgDeclName[VG_MAX_SYMBOL_LEN];
            int startingNote = 0;
            if (sscanf(trimmed + 12, "%[^,\n], %d", vgDeclName, &startingNote) >= 2) {
                if (startingNote > 0 && startingNote < VOICEGROUP_SIZE)
                    voiceIndex = startingNote;
            }
            continue;
        }

        /* voice_directsound variants */
        if (strncmp(trimmed, "voice_directsound_no_resample ", 30) == 0) {
            int key, pan, attack, decay, sustain, release;
            char sampleSymbol[VG_MAX_SYMBOL_LEN];
            if (sscanf(trimmed + 30, "%d, %d, %[^,], %d, %d, %d, %d",
                       &key, &pan, sampleSymbol, &attack, &decay, &sustain, &release) == 7) {
                vg_rtrim(sampleSymbol);
                ToneData *td = &vg->voices[voiceIndex];
                td->type = VOICE_DIRECTSOUND_NO_RESAMPLE;
                td->key = (uint8_t)key;
                td->panSweep = pan ? (0x80 | pan) : 0;
                td->attack = (uint8_t)attack;
                td->decay = (uint8_t)decay;
                td->sustain = (uint8_t)sustain;
                td->release = (uint8_t)release;

                WaveData *wd = resolve_and_load_sample(projectRoot, sampleSymbol, dsMap, disc, vg, waveCache);
                if (wd) {
                    td->wav = wd;
                }
            }
            voiceIndex++;
            voicesParsedInSection++;
        } else if (strncmp(trimmed, "voice_directsound_alt ", 22) == 0) {
            int key, pan, attack, decay, sustain, release;
            char sampleSymbol[VG_MAX_SYMBOL_LEN];
            if (sscanf(trimmed + 22, "%d, %d, %[^,], %d, %d, %d, %d",
                       &key, &pan, sampleSymbol, &attack, &decay, &sustain, &release) == 7) {
                vg_rtrim(sampleSymbol);
                ToneData *td = &vg->voices[voiceIndex];
                td->type = VOICE_DIRECTSOUND_ALT;
                td->key = (uint8_t)key;
                td->panSweep = pan ? (0x80 | pan) : 0;
                td->attack = (uint8_t)attack;
                td->decay = (uint8_t)decay;
                td->sustain = (uint8_t)sustain;
                td->release = (uint8_t)release;

                WaveData *wd = resolve_and_load_sample(projectRoot, sampleSymbol, dsMap, disc, vg, waveCache);
                if (wd) {
                    td->wav = wd;
                }
            }
            voiceIndex++;
            voicesParsedInSection++;
        } else if (strncmp(trimmed, "voice_directsound ", 18) == 0) {
            int key, pan, attack, decay, sustain, release;
            char sampleSymbol[VG_MAX_SYMBOL_LEN];
            if (sscanf(trimmed + 18, "%d, %d, %[^,], %d, %d, %d, %d",
                       &key, &pan, sampleSymbol, &attack, &decay, &sustain, &release) == 7) {
                vg_rtrim(sampleSymbol);
                ToneData *td = &vg->voices[voiceIndex];
                td->type = VOICE_DIRECTSOUND;
                td->key = (uint8_t)key;
                td->panSweep = pan ? (0x80 | pan) : 0;
                td->attack = (uint8_t)attack;
                td->decay = (uint8_t)decay;
                td->sustain = (uint8_t)sustain;
                td->release = (uint8_t)release;

                WaveData *wd = resolve_and_load_sample(projectRoot, sampleSymbol, dsMap, disc, vg, waveCache);
                if (wd) {
                    td->wav = wd;
                }
            }
            voiceIndex++;
            voicesParsedInSection++;
        }
        /* voice_square_1 */
        else if (strncmp(trimmed, "voice_square_1_alt ", 19) == 0) {
            int key, pan, sweep, duty, attack, decay, sustain, release;
            if (sscanf(trimmed + 19, "%d, %d, %d, %d, %d, %d, %d, %d",
                       &key, &pan, &sweep, &duty, &attack, &decay, &sustain, &release) == 8) {
                ToneData *td = &vg->voices[voiceIndex];
                td->type = VOICE_SQUARE_1_ALT;
                td->key = (uint8_t)key;
                td->panSweep = (uint8_t)sweep;
                td->wavePointer = (uint32_t *)(uintptr_t)(duty & 0x03);
                td->attack = (uint8_t)(attack & 0x07);
                td->decay = (uint8_t)(decay & 0x07);
                td->sustain = (uint8_t)(sustain & 0x0F);
                td->release = (uint8_t)(release & 0x07);
            }
            voiceIndex++;
            voicesParsedInSection++;
        } else if (strncmp(trimmed, "voice_square_1 ", 15) == 0) {
            int key, pan, sweep, duty, attack, decay, sustain, release;
            if (sscanf(trimmed + 15, "%d, %d, %d, %d, %d, %d, %d, %d",
                       &key, &pan, &sweep, &duty, &attack, &decay, &sustain, &release) == 8) {
                ToneData *td = &vg->voices[voiceIndex];
                td->type = VOICE_SQUARE_1;
                td->key = (uint8_t)key;
                td->panSweep = (uint8_t)sweep;
                td->wavePointer = (uint32_t *)(uintptr_t)(duty & 0x03);
                td->attack = (uint8_t)(attack & 0x07);
                td->decay = (uint8_t)(decay & 0x07);
                td->sustain = (uint8_t)(sustain & 0x0F);
                td->release = (uint8_t)(release & 0x07);
            }
            voiceIndex++;
            voicesParsedInSection++;
        }
        /* voice_square_2 */
        else if (strncmp(trimmed, "voice_square_2_alt ", 19) == 0) {
            int key, pan, duty, attack, decay, sustain, release;
            if (sscanf(trimmed + 19, "%d, %d, %d, %d, %d, %d, %d",
                       &key, &pan, &duty, &attack, &decay, &sustain, &release) == 7) {
                ToneData *td = &vg->voices[voiceIndex];
                td->type = VOICE_SQUARE_2_ALT;
                td->key = (uint8_t)key;
                td->panSweep = 0;
                td->wavePointer = (uint32_t *)(uintptr_t)(duty & 0x03);
                td->attack = (uint8_t)(attack & 0x07);
                td->decay = (uint8_t)(decay & 0x07);
                td->sustain = (uint8_t)(sustain & 0x0F);
                td->release = (uint8_t)(release & 0x07);
            }
            voiceIndex++;
            voicesParsedInSection++;
        } else if (strncmp(trimmed, "voice_square_2 ", 15) == 0) {
            int key, pan, duty, attack, decay, sustain, release;
            if (sscanf(trimmed + 15, "%d, %d, %d, %d, %d, %d, %d",
                       &key, &pan, &duty, &attack, &decay, &sustain, &release) == 7) {
                ToneData *td = &vg->voices[voiceIndex];
                td->type = VOICE_SQUARE_2;
                td->key = (uint8_t)key;
                td->panSweep = 0;
                td->wavePointer = (uint32_t *)(uintptr_t)(duty & 0x03);
                td->attack = (uint8_t)(attack & 0x07);
                td->decay = (uint8_t)(decay & 0x07);
                td->sustain = (uint8_t)(sustain & 0x0F);
                td->release = (uint8_t)(release & 0x07);
            }
            voiceIndex++;
            voicesParsedInSection++;
        }
        /* voice_programmable_wave */
        else if (strncmp(trimmed, "voice_programmable_wave_alt ", 27) == 0) {
            int key, pan, attack, decay, sustain, release;
            char waveSymbol[VG_MAX_SYMBOL_LEN];
            if (sscanf(trimmed + 27, "%d, %d, %[^,], %d, %d, %d, %d",
                       &key, &pan, waveSymbol, &attack, &decay, &sustain, &release) == 7) {
                vg_rtrim(waveSymbol);
                ToneData *td = &vg->voices[voiceIndex];
                td->type = VOICE_PROGRAMMABLE_WAVE_ALT;
                td->key = (uint8_t)key;
                td->attack = (uint8_t)(attack & 0x07);
                td->decay = (uint8_t)(decay & 0x07);
                td->sustain = (uint8_t)(sustain & 0x0F);
                td->release = (uint8_t)(release & 0x07);

                const char *wavePath = vg_symbol_map_find(pwMap, waveSymbol);
                if (wavePath) {
                    uint32_t *pw = vg_load_prog_wave(projectRoot, wavePath);
                    if (pw) {
                        td->wavePointer = pw;
                        vg_register_progwave(vg, pw);
                    }
                }
            }
            voiceIndex++;
            voicesParsedInSection++;
        } else if (strncmp(trimmed, "voice_programmable_wave ", 23) == 0) {
            int key, pan, attack, decay, sustain, release;
            char waveSymbol[VG_MAX_SYMBOL_LEN];
            if (sscanf(trimmed + 23, "%d, %d, %[^,], %d, %d, %d, %d",
                       &key, &pan, waveSymbol, &attack, &decay, &sustain, &release) == 7) {
                vg_rtrim(waveSymbol);
                ToneData *td = &vg->voices[voiceIndex];
                td->type = VOICE_PROGRAMMABLE_WAVE;
                td->key = (uint8_t)key;
                td->attack = (uint8_t)(attack & 0x07);
                td->decay = (uint8_t)(decay & 0x07);
                td->sustain = (uint8_t)(sustain & 0x0F);
                td->release = (uint8_t)(release & 0x07);

                const char *wavePath = vg_symbol_map_find(pwMap, waveSymbol);
                if (wavePath) {
                    uint32_t *pw = vg_load_prog_wave(projectRoot, wavePath);
                    if (pw) {
                        td->wavePointer = pw;
                        vg_register_progwave(vg, pw);
                    }
                }
            }
            voiceIndex++;
            voicesParsedInSection++;
        }
        /* voice_noise */
        else if (strncmp(trimmed, "voice_noise_alt ", 16) == 0) {
            int key, pan, period, attack, decay, sustain, release;
            if (sscanf(trimmed + 16, "%d, %d, %d, %d, %d, %d, %d",
                       &key, &pan, &period, &attack, &decay, &sustain, &release) == 7) {
                ToneData *td = &vg->voices[voiceIndex];
                td->type = VOICE_NOISE_ALT;
                td->key = (uint8_t)key;
                td->wavePointer = (uint32_t *)(uintptr_t)(period & 0x01);
                td->attack = (uint8_t)(attack & 0x07);
                td->decay = (uint8_t)(decay & 0x07);
                td->sustain = (uint8_t)(sustain & 0x0F);
                td->release = (uint8_t)(release & 0x07);
            }
            voiceIndex++;
            voicesParsedInSection++;
        } else if (strncmp(trimmed, "voice_noise ", 12) == 0) {
            int key, pan, period, attack, decay, sustain, release;
            if (sscanf(trimmed + 12, "%d, %d, %d, %d, %d, %d, %d",
                       &key, &pan, &period, &attack, &decay, &sustain, &release) == 7) {
                ToneData *td = &vg->voices[voiceIndex];
                td->type = VOICE_NOISE;
                td->key = (uint8_t)key;
                td->wavePointer = (uint32_t *)(uintptr_t)(period & 0x01);
                td->attack = (uint8_t)(attack & 0x07);
                td->decay = (uint8_t)(decay & 0x07);
                td->sustain = (uint8_t)(sustain & 0x0F);
                td->release = (uint8_t)(release & 0x07);
            }
            voiceIndex++;
            voicesParsedInSection++;
        }
        /* voice_keysplit */
        else if (strncmp(trimmed, "voice_keysplit_all ", 19) == 0) {
            char vgSymbol[VG_MAX_SYMBOL_LEN];
            if (sscanf(trimmed + 19, "%s", vgSymbol) == 1) {
                vg_rtrim(vgSymbol);
                ToneData *td = &vg->voices[voiceIndex];
                td->type = VOICE_KEYSPLIT_ALL;

                ToneData *subVg = load_sub_voicegroup(projectRoot, vgSymbol,
                                                       vg, dsMap, pwMap, ksMap, disc, waveCache);
                td->subGroup = subVg;
            }
            voiceIndex++;
            voicesParsedInSection++;
        } else if (strncmp(trimmed, "voice_keysplit ", 15) == 0) {
            char vgSymbol[VG_MAX_SYMBOL_LEN];
            char ksSymbol[VG_MAX_SYMBOL_LEN];
            if (sscanf(trimmed + 15, "%[^,], %s", vgSymbol, ksSymbol) == 2) {
                vg_rtrim(vgSymbol);
                vg_rtrim(ksSymbol);
                ToneData *td = &vg->voices[voiceIndex];
                td->type = VOICE_KEYSPLIT;

                ToneData *subVg = load_sub_voicegroup(projectRoot, vgSymbol,
                                                       vg, dsMap, pwMap, ksMap, disc, waveCache);
                td->subGroup = subVg;

                KeySplitDef *ksDef = vg_keysplit_map_find(ksMap, ksSymbol);
                if (ksDef) {
                    uint8_t *table = malloc(128);
                    memcpy(table, ksDef->table, 128);
                    td->keySplitTable = table;
                    vg_register_keysplittable(vg, table);
                }
            }
            voiceIndex++;
            voicesParsedInSection++;
        }
        /* cry / cry_reverse */
        else if (strncmp(trimmed, "cry_reverse ", 12) == 0) {
            char sampleSymbol[VG_MAX_SYMBOL_LEN];
            if (sscanf(trimmed + 12, "%s", sampleSymbol) == 1) {
                vg_rtrim(sampleSymbol);
                ToneData *td = &vg->voices[voiceIndex];
                td->type = VOICE_CRY_REVERSE;
                td->key = 60;
                td->attack = 0xFF;
                td->decay = 0;
                td->sustain = 0xFF;
                td->release = 0;

                const char *samplePath = vg_symbol_map_find(dsMap, sampleSymbol);
                if (samplePath) {
                    WaveData *wd = vg_load_bin_sample(projectRoot, samplePath);
                    if (wd) {
                        td->wav = wd;
                        vg_register_wavedata(vg, wd);
                    }
                }
            }
            voiceIndex++;
            voicesParsedInSection++;
        } else if (strncmp(trimmed, "cry ", 4) == 0) {
            char sampleSymbol[VG_MAX_SYMBOL_LEN];
            if (sscanf(trimmed + 4, "%s", sampleSymbol) == 1) {
                vg_rtrim(sampleSymbol);
                ToneData *td = &vg->voices[voiceIndex];
                td->type = VOICE_CRY;
                td->key = 60;
                td->attack = 0xFF;
                td->decay = 0;
                td->sustain = 0xFF;
                td->release = 0;

                const char *samplePath = vg_symbol_map_find(dsMap, sampleSymbol);
                if (samplePath) {
                    WaveData *wd = vg_load_bin_sample(projectRoot, samplePath);
                    if (wd) {
                        td->wav = wd;
                        vg_register_wavedata(vg, wd);
                    }
                }
            }
            voiceIndex++;
            voicesParsedInSection++;
        }
    }

    vg_log("parse_voicegroup_file: done, voiceIndex=%d", voiceIndex);
    fclose(f);
    return 0;
}

/*
 * Main entry point: load a voicegroup from a project.
 */
LoadedVoiceGroup *voicegroup_load(const char *projectRoot, const char *voicegroupName,
                                   const VoicegroupLoaderConfig *config)
{
    vg_log("voicegroup_load: start root='%s' vg='%s'", projectRoot, voicegroupName);

    LoadedVoiceGroup *vg = calloc(1, sizeof(LoadedVoiceGroup));
    if (!vg) return NULL;

    /* Heap-allocate ProjectDiscovery: ~96 KB on the stack would risk overflow
     * in Reaper's plugin-load thread (Windows default: 1 MB stack). */
    ProjectDiscovery *disc = calloc(1, sizeof(ProjectDiscovery));
    if (!disc) {
        voicegroup_free(vg);
        return NULL;
    }

    /* Discover project structure */
    vg_log("voicegroup_load: calling vg_discover_project");
    vg_discover_project(projectRoot, config, disc);
    vg_log("voicegroup_load: discover done - dsFiles=%d pwFiles=%d ksFiles=%d vgDirs=%d monoFiles=%d wavDirs=%d",
           disc->directSoundDataFiles.count, disc->progWaveDataFiles.count,
           disc->keySplitTableFiles.count, disc->voicegroupDirs.count,
           disc->monolithicVGFiles.count, disc->wavSampleDirs.count);

    /* Per-load WaveData deduplication cache */
    WaveCache waveCache;
    vg_wave_cache_init(&waveCache);

    /* Parse symbol maps from all discovered files */
    SymbolMap dsMap, pwMap;
    KeySplitMap ksMap;
    vg_symbol_map_init(&dsMap);
    vg_symbol_map_init(&pwMap);
    vg_keysplit_map_init(&ksMap);

    vg_log("voicegroup_load: parsing symbol maps");
    vg_parse_direct_sound_data(disc, &dsMap);
    vg_log("voicegroup_load: dsMap entries=%d", dsMap.count);
    vg_parse_prog_wave_data(disc, &pwMap);
    vg_log("voicegroup_load: pwMap entries=%d", pwMap.count);
    vg_parse_keysplit_tables(disc, &ksMap);
    vg_log("voicegroup_load: ksMap entries=%d", ksMap.count);

    /* Find the voicegroup */
    vg_log("voicegroup_load: searching for voicegroup '%s'", voicegroupName);
    VoicegroupLocation loc = find_voicegroup(projectRoot, voicegroupName, disc);
    if (!loc.found) {
        vg_log("voicegroup_load: voicegroup '%s' not found", voicegroupName);
        vg_err("cannot find voicegroup '%s'", voicegroupName);
        goto fail;
    }
    vg_log("voicegroup_load: found at '%s' label='%s'", loc.filePath, loc.label);

    /* Parse the voicegroup */
    const char *startLabel = loc.label[0] ? loc.label : NULL;
    vg_log("voicegroup_load: parsing voicegroup file");
    if (parse_voicegroup_file(projectRoot, loc.filePath, startLabel,
                               vg, &dsMap, &pwMap, &ksMap, disc, &waveCache) != 0) {
        vg_log("voicegroup_load: parse_voicegroup_file failed");
        goto fail;
    }
    vg_log("voicegroup_load: done OK");

    vg_symbol_map_free(&dsMap);
    vg_symbol_map_free(&pwMap);
    vg_keysplit_map_free(&ksMap);
    free(disc);
    return vg;

fail:
    vg_symbol_map_free(&dsMap);
    vg_symbol_map_free(&pwMap);
    vg_keysplit_map_free(&ksMap);
    free(disc);
    voicegroup_free(vg);
    return NULL;
}

void voicegroup_free(LoadedVoiceGroup *vg)
{
    if (!vg) return;

    for (int i = 0; i < vg->waveDataCount; i++)
        free(vg->waveDatas[i]);
    free(vg->waveDatas);

    for (int i = 0; i < vg->progWaveCount; i++)
        free(vg->progWaves[i]);
    free(vg->progWaves);

    for (int i = 0; i < vg->subGroupCount; i++)
        free(vg->subGroups[i]);
    free(vg->subGroups);

    for (int i = 0; i < vg->keySplitTableCount; i++)
        free(vg->keySplitTables[i]);
    free(vg->keySplitTables);

    free(vg);
}

/* ---- Project asset collection ---- */

/* Helper: extract the basename from a path (after last / or \). */
static const char *path_basename(const char *path)
{
    const char *last = path;
    for (const char *p = path; *p; p++) {
        if (*p == '/' || *p == '\\')
            last = p + 1;
    }
    return last;
}

bool voicegroup_loader_collect_project_assets(const char *projectRoot,
                                              const VoicegroupLoaderConfig *config,
                                              VoicegroupProjectAssets *out)
{
    memset(out, 0, sizeof(*out));

    ProjectDiscovery *disc = calloc(1, sizeof(ProjectDiscovery));
    if (!disc) return false;
    vg_discover_project(projectRoot, config, disc);

    /* Parse symbol maps */
    SymbolMap dsMap, pwMap;
    vg_symbol_map_init(&dsMap);
    vg_symbol_map_init(&pwMap);
    vg_parse_direct_sound_data(disc, &dsMap);
    vg_parse_prog_wave_data(disc, &pwMap);

    /* Build DirectSound asset array */
    if (dsMap.count > 0) {
        out->directsound = calloc((size_t)dsMap.count, sizeof(ProjectAssetEntry));
        if (out->directsound) {
            for (int i = 0; i < dsMap.count; i++) {
                ProjectAssetEntry *e = &out->directsound[out->directsoundCount];
                e->kind = PROJECT_ASSET_DIRECTSOUND;
                strncpy(e->symbol, dsMap.entries[i].symbol, sizeof(e->symbol) - 1);
                strncpy(e->relPath, dsMap.entries[i].filePath, sizeof(e->relPath) - 1);
                const char *base = path_basename(dsMap.entries[i].filePath);
                strncpy(e->fileName, base, sizeof(e->fileName) - 1);
                out->directsoundCount++;
            }
        }
    }

    /* Build ProgrammableWave asset array */
    if (pwMap.count > 0) {
        out->progWave = calloc((size_t)pwMap.count, sizeof(ProjectAssetEntry));
        if (out->progWave) {
            for (int i = 0; i < pwMap.count; i++) {
                ProjectAssetEntry *e = &out->progWave[out->progWaveCount];
                e->kind = PROJECT_ASSET_PROG_WAVE;
                strncpy(e->symbol, pwMap.entries[i].symbol, sizeof(e->symbol) - 1);
                strncpy(e->relPath, pwMap.entries[i].filePath, sizeof(e->relPath) - 1);
                const char *base = path_basename(pwMap.entries[i].filePath);
                strncpy(e->fileName, base, sizeof(e->fileName) - 1);
                out->progWaveCount++;
            }
        }
    }

    vg_symbol_map_free(&dsMap);
    vg_symbol_map_free(&pwMap);
    free(disc);
    return true;
}

void voicegroup_loader_free_project_assets(VoicegroupProjectAssets *assets)
{
    free(assets->directsound);
    free(assets->progWave);
    memset(assets, 0, sizeof(*assets));
}

WaveData *voicegroup_loader_load_sample(const char *projectRoot,
                                        const char *relPath,
                                        LoadedVoiceGroup *vg)
{
    /* Try .wav first (substitute .bin extension) */
    WaveData *wd = vg_load_sample(projectRoot, relPath);
    if (!wd) {
        /* Fallback to raw .bin */
        wd = vg_load_bin_sample(projectRoot, relPath);
    }
    if (wd) {
        vg_register_wavedata(vg, wd);
    }
    return wd;
}

uint32_t *voicegroup_loader_load_prog_wave(const char *projectRoot,
                                           const char *relPath,
                                           LoadedVoiceGroup *vg)
{
    uint32_t *pw = vg_load_prog_wave(projectRoot, relPath);
    if (pw) {
        vg_register_progwave(vg, pw);
    }
    return pw;
}
