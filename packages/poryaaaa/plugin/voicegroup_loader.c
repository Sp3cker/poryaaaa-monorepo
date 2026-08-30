#include "voicegroup_loader.h"
#include "voicegroup_asset_batch.h"
#include "voicegroup_load_session.h"
#include <limits.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <time.h>
#include <stdarg.h>
#include <sys/stat.h>
#ifdef _WIN32
#    define WIN32_LEAN_AND_MEAN
#    include <windows.h>
#else
#    include <dirent.h>
#endif

#ifdef _WIN32
#    define PATH_SEP '\\'
#else
#    define PATH_SEP '/'
#endif

/* MSVC's sys/stat.h lacks the POSIX S_IS* predicate macros. */
#ifndef S_ISREG
#    define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#endif
#ifndef S_ISDIR
#    define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#endif

#ifdef _WIN32
/*
 * Minimal dirent shim over FindFirstFile.  Only d_name and d_type are used in
 * this file, and directory iteration is strictly sequential (never
 * concurrent), so a single static dirent entry per stream is sufficient.
 */

/* POSIX d_type values the loader uses; define for the shim. */
#    define DT_UNKNOWN 0
#    define DT_DIR 4
#    define DT_REG 8
#    define DT_LNK 10

struct dirent
{
    char d_name[MAX_PATH];
    unsigned char d_type;
};

typedef struct
{
    HANDLE handle;
    WIN32_FIND_DATAA findData;
    int first;
    struct dirent entry;
} DIR;

static DIR* opendir(const char* path)
{
    char pattern[MAX_PATH];
    if (snprintf(pattern, sizeof(pattern), "%s\\*", path) >= (int)sizeof(pattern))
        return NULL;
    DIR* d = (DIR*)calloc(1, sizeof(DIR));
    if (!d)
        return NULL;
    d->handle = FindFirstFileExA(
        pattern, FindExInfoBasic, &d->findData, FindExSearchNameMatch, NULL, FIND_FIRST_EX_LARGE_FETCH);
    if (d->handle == INVALID_HANDLE_VALUE)
    {
        free(d);
        return NULL;
    }
    d->first = 1;
    return d;
}

static struct dirent* readdir(DIR* d)
{
    if (d->first)
        d->first = 0;
    else if (!FindNextFileA(d->handle, &d->findData))
        return NULL;
    snprintf(d->entry.d_name, sizeof(d->entry.d_name), "%s", d->findData.cFileName);
    DWORD attrs = d->findData.dwFileAttributes;
    if (attrs & FILE_ATTRIBUTE_REPARSE_POINT)
        d->entry.d_type = DT_LNK; /* forces the stat() fallback below */
    else if (attrs & FILE_ATTRIBUTE_DIRECTORY)
        d->entry.d_type = DT_DIR;
    else
        d->entry.d_type = DT_REG;
    return &d->entry;
}

static int closedir(DIR* d)
{
    FindClose(d->handle);
    free(d);
    return 0;
}
#endif /* _WIN32 */

#define MAX_LINE 1024
#define MAX_PATH_LEN 512
#ifndef MAX_SYMBOL_LEN
#define MAX_SYMBOL_LEN 256
#endif
#define INITIAL_CAPACITY 64

#define MAX_DISCOVERED_PATHS 32

/* ---- Diagnostic logging ---- */

static const char* s_vgLogPath = NULL;

void voicegroup_loader_set_log_path(const char* path)
{
    s_vgLogPath = path;
}

static void vg_log(const char* fmt, ...)
{
    if (!s_vgLogPath)
        return;
    FILE* f = fopen(s_vgLogPath, "a");
    if (!f)
        return;
    time_t t = time(NULL);
    char tbuf[32];
    strftime(tbuf, sizeof(tbuf), "%H:%M:%S", localtime(&t));
    fprintf(f, "[%s] vg_loader: ", tbuf);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fputc('\n', f);
    fclose(f);
}

/* ---- Discovery data structures ---- */

typedef struct
{
    char paths[MAX_DISCOVERED_PATHS][MAX_PATH_LEN];
    int count;
} PathList;

typedef struct
{
    PathList directSoundDataFiles; /* paths to direct_sound_data.inc files */
    PathList progWaveDataFiles;    /* paths to programmable_wave_data.inc files */
    PathList keySplitTableFiles;   /* paths to keysplit_tables.inc files */
    PathList voicegroupDirs;       /* directories with individual .inc/.s voicegroup files */
    PathList monolithicVGFiles;    /* files containing multiple voicegroups (voice_groups.inc) */
    PathList wavSampleDirs;        /* directories with .wav sample files */
    /* Lazy deep scan (see discovery_ensure_deep_scan) */
    char projectRoot[MAX_PATH_LEN];
    const VoicegroupLoaderConfig* cfg; /* borrowed; valid for the load call */
    int deepScanned;
} ProjectDiscovery;

typedef struct
{
    char filePath[MAX_PATH_LEN];
    char label[MAX_SYMBOL_LEN]; /* non-empty if inside a monolithic file */
    int found;
} VoicegroupLocation;

/* ---- Symbol maps ---- */

typedef struct
{
    char symbol[MAX_SYMBOL_LEN];
    char filePath[MAX_PATH_LEN];
    /* Inline Golden Sun synth definition (set_synth_* macros) instead of a
     * sample file.  synthDesc holds the 6 descriptor bytes that follow a
     * zero-size WaveData header (0x80, type, then 4 pulse parameters). */
    uint8_t isSynth;
    uint8_t synthDesc[6];
} SymbolMapping;

typedef struct
{
    SymbolMapping* entries;
    int count;
    int capacity;
} SymbolMap;

typedef struct
{
    char name[MAX_SYMBOL_LEN];
    int startingNote;
    uint8_t table[128];
    int maxNote;
} KeySplitDef;

typedef struct
{
    KeySplitDef* entries;
    int count;
    int capacity;
    /* How many ProjectDiscovery keySplitTableFiles entries have been parsed
     * into this map; the lazy deep scan appends files past this index. */
    int parsedFileCount;
} KeySplitMap;

/*
 * Persistent project context (voicegroup_project_open/free). Owns copied
 * root/config/adapter storage plus one ProjectDiscovery and the parsed
 * DirectSound/programmable-wave/keysplit maps for one generation. Loads from
 * the context reuse all of it; the lazy sound/ deep scan still runs at most
 * once per context, on first lookup miss. Returned banks and sample sets are
 * self-contained and stay valid after the context is freed.
 */
struct VoicegroupProject
{
    char projectRoot[VG_MAX_PATH_LEN]; /* owned copy */
    VoicegroupLoaderConfig config;     /* owned copy; the discovery borrows it */
    VoicegroupFileIo fileIo;           /* owned copy of the adapter */
    ProjectDiscovery* disc;            /* heap: ~96 KB, kept off plugin threads' stacks */
    SymbolMap dsMap;
    SymbolMap pwMap;
    KeySplitMap ksMap;
};

/* Forward declarations */
static bool vg_register_wavedata(LoadedVoiceGroup* vg, WaveData* wd);
static bool vg_register_subgroup(LoadedVoiceGroup* vg, ToneData* sg);
static bool vg_register_keysplittable(LoadedVoiceGroup* vg, uint8_t* ks);
static bool build_path(char* dest, size_t destSize, const char* base, const char* relative);
static const uint8_t* symbol_map_find_synth(const SymbolMap* map, const char* symbol);
static void discovery_ensure_deep_scan(ProjectDiscovery* disc);
static WaveData* load_wav_from_path(const char* absoluteWavPath, bool* hardFailure);
static WaveData* load_aif_from_path(const char* absoluteAifPath, bool* hardFailure);
static int file_exists(const char* path);
static void strip_comment(char* s);
static char* ltrim(char* s);
static void rtrim(char* s);
static VoicegroupLocation find_voicegroup(const char* projectRoot, const char* vgName, ProjectDiscovery* disc);
static int next_included_voicegroup(const char* projectRoot, const char* currentFilePath, char* outPath, size_t outSize);
static int load_sub_voicegroup_session(const char* projectRoot, const char* vgSymbol, LoadedVoiceGroup* vgReg, VgLoadSession* session, const SymbolMap* dsMap, const SymbolMap* pwMap, KeySplitMap* ksMap, ProjectDiscovery* disc, WaveCache* waveCache, ToneData** outSub);
static int parse_voicegroup_file_session(
    const char* projectRoot,
    const char* filePath,
    const char* startLabel,
    ToneData* destVoices,
    char (*destNames)[VG_VOICE_NAME_LEN],
    LoadedVoiceGroup* vgReg,
    VgLoadSession* session,
    const SymbolMap* dsMap,
    const SymbolMap* pwMap,
    KeySplitMap* ksMap,
    ProjectDiscovery* disc,
    WaveCache* waveCache,
    int startIndex,
    int contiguousFill,
    int noSubRecurse);

static WaveData* build_synth_wavedata(const uint8_t desc[6], const char* symbol, LoadedVoiceGroup* owner, WaveCache* cache)
{
    char cacheKey[MAX_PATH_LEN];
    snprintf(cacheKey, sizeof(cacheKey), "synth-macro:%s", symbol);
    WaveData* cached = wave_cache_find(cache, cacheKey);
    if (cached)
    {
        return cached;
    }
    WaveData* wd = (WaveData*)calloc(1, sizeof(WaveData) + 17);
    if (!wd) return NULL;
    wd->type = 0;
    wd->status = 0x4000;
    wd->freq = 0x01058920;
    wd->loopStart = 0;
    wd->size = 0;
    wd->data = (int8_t*)((uint8_t*)wd + sizeof(WaveData));
    memcpy(wd->data, desc, 6);
    if (!vg_register_wavedata(owner, wd))
    {
        free(wd);
        return NULL;
    }
    wave_cache_insert(cache, cacheKey, wd);
    return wd;
}
static void set_voice_display_name(char dest[VG_VOICE_NAME_LEN], const char* symbol)
{
    if (!dest || !symbol)
    {
        if (dest)
        {
            dest[0] = '\0';
        }
        return;
    }
    const char* p = symbol;
    static const char* prefixes[] =
    {
        "DirectSoundWaveData_",
        "ProgrammableWaveData_",
        "voicegroup_"
    };
    for (size_t i = 0; i < 3; i++)
    {
        size_t L = strlen(prefixes[i]);
        if (strncmp(p, prefixes[i], L) == 0 && p[L])
        {
            p += L;
            break;
        }
    }
    strncpy(dest, p, VG_VOICE_NAME_LEN - 1);
    dest[VG_VOICE_NAME_LEN - 1] = '\0';
}
static int vg_extract_comma_symbol(const char** p, char* out, size_t outSize)
{
    const char* s = *p;
    while (*s == ' ' || *s == '\t')
    {
        s++;
    }
    const char* comma = strchr(s, ',');
    if (!comma)
    {
        return 1;
    }
    const char* end = comma;
    while (end > s && isspace((unsigned char)end[-1]))
    {
        end--;
    }
    const char* start = s;
    while (start < end && isspace((unsigned char)*start))
    {
        start++;
    }
    size_t len = end - start;
    if (len == 0)
    {
        return 1;
    }
    if (len >= outSize)
    {
        return -1;
    }
    memcpy(out, start, len);
    out[len] = '\0';
    *p = comma + 1;
    return 0;
}

static int vg_extract_eol_symbol(const char** p, char* out, size_t outSize)
{
    const char* s = *p;
    while (*s == ' ' || *s == '\t')
    {
        s++;
    }
    const char* end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1]))
    {
        end--;
    }
    size_t len = end - s;
    if (len == 0)
    {
        return 1;
    }
    if (len >= outSize)
    {
        return -1;
    }
    memcpy(out, s, len);
    out[len] = '\0';
    *p = end;
    return 0;
}

static bool vg_parse_next_int(const char** p, int* out)
{
    const char* s = *p;
    while (*s == ' ' || *s == '\t')
    {
        s++;
    }
    if (*s == '\0')
    {
        return false;
    }
    char* end = NULL;
    errno = 0;
    long v = strtol(s, &end, 0);
    if (end == s)
    {
        return false;
    }
    if (errno == ERANGE)
    {
        return false;
    }
    if (v < INT_MIN || v > INT_MAX)
    {
        return false;
    }
    *out = (int)v;
    *p = end;
    return true;
}

static bool vg_expect_comma(const char** p)
{
    const char* s = *p;
    while (*s == ' ' || *s == '\t')
    {
        s++;
    }
    if (*s != ',')
    {
        return false;
    }
    *p = s + 1;
    return true;
}
static bool vg_section_label_valid(const char* label)
{
    if (!label || !label[0])
    {
        return true;
    }
    size_t len = strlen(label);
    if (len >= MAX_SYMBOL_LEN)
    {
        return false;
    }
    for (size_t i = 0; i < len; i++)
    {
        char c = label[i];
        if (isspace((unsigned char)c) || c == ':' || c == ',' )
        {
            return false;
        }
    }
    return true;
}



static bool vg_make_bin_variant(const char* srcPath, const char* newExt, char* dst, size_t dstSize)
{
    size_t len = strlen(srcPath);
    if (len < 4 || strcmp(srcPath + len - 4, ".bin") != 0)
    {
        return false;
    }
    size_t baseLen = len - 4;
    size_t extLen = strlen(newExt);
    if (baseLen + extLen + 1 > dstSize)
    {
        return false;
    }
    memcpy(dst, srcPath, baseLen);
    memcpy(dst + baseLen, newExt, extLen + 1);
    return true;
}

static bool build_wave_abs_paths(const char* projectRoot, const char* samplePath, char wavAbs[VG_MAX_PATH_LEN], char aifAbs[VG_MAX_PATH_LEN], char binAbs[VG_MAX_PATH_LEN])
{
    char relWav[MAX_PATH_LEN] = {0};
    char relAif[MAX_PATH_LEN] = {0};
    bool haveWav = vg_make_bin_variant(samplePath, ".wav", relWav, sizeof(relWav));
    bool haveAif = vg_make_bin_variant(samplePath, ".aif", relAif, sizeof(relAif));
    if (!build_path(binAbs, VG_MAX_PATH_LEN, projectRoot, samplePath))
    {
        return false;
    }
    if (haveWav)
    {
        if (!build_path(wavAbs, VG_MAX_PATH_LEN, projectRoot, relWav))
        {
            return false;
        }
    }
    else
    {
        wavAbs[0] = '\0';
    }
    if (haveAif)
    {
        if (!build_path(aifAbs, VG_MAX_PATH_LEN, projectRoot, relAif))
        {
            return false;
        }
    }
    else
    {
        aifAbs[0] = '\0';
    }
    return true;
}
static void symbol_map_free(SymbolMap* map);
static bool symbol_map_add(SymbolMap* map, const char* symbol, const char* path);
static const char* symbol_map_find(const SymbolMap* map, const char* symbol);

static void keysplit_map_init(KeySplitMap* map);
static void keysplit_map_free(KeySplitMap* map);
static KeySplitDef* keysplit_map_find(const KeySplitMap* map, const char* name);

static int parse_direct_sound_data_file(const char* filePath, const char* projectRoot, SymbolMap* map);
static int parse_programmable_wave_data_file(const char* filePath, const char* projectRoot, SymbolMap* map);
static int parse_keysplit_tables_file(const char* filePath, KeySplitMap* map);


/* Helper: strip trailing whitespace/newline */
static char* ltrim(char* s)
{
    while (*s && isspace((unsigned char)*s))
        s++;
    return s;
}

static void rtrim(char* s)
{
    int len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1]))
    {
        s[--len] = '\0';
    }
}

/* Helper: strip inline comments (@ or //) */
static void strip_comment(char* s)
{
    char* p = strchr(s, '@');
    if (p)
        *p = '\0';
    p = strstr(s, "//");
    if (p)
        *p = '\0';
}

/* Helper: build a path. Returns false if the result would be truncated — the
 * caller must treat truncation as a hard failure rather than an alias. */
static bool build_path(char* dest, size_t destSize, const char* base, const char* relative)
{
    if (!dest || destSize == 0 || !base || !relative)
    {
        if (dest && destSize)
        {
            dest[0] = '\0';
        }
        return false;
    }
    size_t baseLen = strlen(base);
    size_t relLen = strlen(relative);
    if (baseLen + 1 + relLen >= destSize)
    {
        dest[0] = '\0';
        return false;
    }
    memcpy(dest, base, baseLen);
    dest[baseLen] = PATH_SEP;
    memcpy(dest + baseLen + 1, relative, relLen + 1);
    for (char* p = dest; *p; p++)
    {
        if (*p == '/' || *p == '\\')
        {
            *p = PATH_SEP;
        }
    }
    return true;
}

/* Helper: try to open a file path, return 1 if it exists, 0 otherwise */
static int file_exists(const char* path)
{
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

/* Helper: check if a path is a directory */
static int is_directory(const char* path)
{
    struct stat st;
    if (stat(path, &st) != 0)
        return 0;
    return S_ISDIR(st.st_mode);
}

/*
 * Entry-type check that avoids a stat() when readdir already told us.
 * DT_UNKNOWN/DT_LNK (possible on Linux for some filesystems, and for
 * symlinks/reparse points everywhere) fall back to stat(), which follows
 * links -- matching the previous behavior exactly.
 */
static int dirent_is_dir(const char* parentPath, const struct dirent* ent)
{
    if (ent->d_type == DT_DIR)
        return 1;
    if (ent->d_type == DT_REG)
        return 0;
    char p[MAX_PATH_LEN];
    build_path(p, sizeof(p), parentPath, ent->d_name);
    return is_directory(p);
}

/* Helper: add a path to a PathList if not already present and not full */
static void pathlist_add(PathList* list, const char* path)
{
    if (list->count >= MAX_DISCOVERED_PATHS)
        return;
    for (int i = 0; i < list->count; i++)
    {
        if (strcmp(list->paths[i], path) == 0)
            return;
    }
    strncpy(list->paths[list->count], path, MAX_PATH_LEN - 1);
    list->paths[list->count][MAX_PATH_LEN - 1] = '\0';
    list->count++;
}

/* Helper: check if a string ends with a given suffix (case-insensitive) */
static int str_ends_with_ci(const char* str, const char* suffix)
{
    size_t slen = strlen(str);
    size_t sufflen = strlen(suffix);
    if (sufflen > slen)
        return 0;
    for (size_t i = 0; i < sufflen; i++)
    {
        if (tolower((unsigned char)str[slen - sufflen + i]) != tolower((unsigned char)suffix[i]))
            return 0;
    }
    return 1;
}

/* Helper: register a WaveData in the loaded voicegroup for later cleanup */
static bool vg_register_wavedata(LoadedVoiceGroup* vg, WaveData* wd)
{
    if (!vg || !wd) return false;
    if (vg->waveDataCount >= vg->waveDataCapacity)
    {
        size_t nc = vg->waveDataCapacity ? (size_t)vg->waveDataCapacity * 2 : INITIAL_CAPACITY;
        WaveData** np = (WaveData**)realloc(vg->waveDatas, nc * sizeof(WaveData*));
        if (!np) return false;
        vg->waveDatas = np;
        vg->waveDataCapacity = (int)nc;
    }
    vg->waveDatas[vg->waveDataCount++] = wd;
    return true;
}


static bool vg_register_subgroup(LoadedVoiceGroup* vg, ToneData* sg)
{
    if (!vg || !sg) return false;
    if (vg->subGroupCount >= vg->subGroupCapacity)
    {
        size_t nc = vg->subGroupCapacity ? (size_t)vg->subGroupCapacity * 2 : INITIAL_CAPACITY;
        ToneData** np = (ToneData**)realloc(vg->subGroups, nc * sizeof(ToneData*));
        if (!np) return false;
        vg->subGroups = np;
        vg->subGroupCapacity = (int)nc;
    }
    vg->subGroups[vg->subGroupCount++] = sg;
    return true;
}

static bool vg_register_keysplittable(LoadedVoiceGroup* vg, uint8_t* ks)
{
    if (!vg || !ks) return false;
    if (vg->keySplitTableCount >= vg->keySplitTableCapacity)
    {
        size_t nc = vg->keySplitTableCapacity ? (size_t)vg->keySplitTableCapacity * 2 : INITIAL_CAPACITY;
        uint8_t** np = (uint8_t**)realloc(vg->keySplitTables, nc * sizeof(uint8_t*));
        if (!np) return false;
        vg->keySplitTables = np;
        vg->keySplitTableCapacity = (int)nc;
    }
    vg->keySplitTables[vg->keySplitTableCount++] = ks;
    return true;
}

/*
 * Symbol map implementation
 */
static void symbol_map_init(SymbolMap* map)
{
    map->entries = NULL;
    map->count = 0;
    map->capacity = 0;
}

static void symbol_map_free(SymbolMap* map)
{
    free(map->entries);
    map->entries = NULL;
    map->count = 0;
    map->capacity = 0;
}

static bool symbol_map_add(SymbolMap* map, const char* symbol, const char* path)
{
    if (map->count >= map->capacity)
    {
        size_t newCap = map->capacity ? (size_t)map->capacity * 2 : INITIAL_CAPACITY;
        if (newCap > (size_t)INT_MAX)
        {
            return false;
        }
        if (newCap > SIZE_MAX / sizeof(SymbolMapping))
        {
            return false;
        }
        SymbolMapping* np =
            (SymbolMapping*)realloc(map->entries, sizeof(SymbolMapping) * newCap);
        if (!np)
        {
            return false;
        }
        map->entries = np;
        map->capacity = (int)newCap;
    }
    memset(&map->entries[map->count], 0, sizeof(SymbolMapping));
    strncpy(map->entries[map->count].symbol, symbol, MAX_SYMBOL_LEN - 1);
    map->entries[map->count].symbol[MAX_SYMBOL_LEN - 1] = '\0';
    strncpy(map->entries[map->count].filePath, path, MAX_PATH_LEN - 1);
    map->entries[map->count].filePath[MAX_PATH_LEN - 1] = '\0';
    map->count++;
    return true;
}

static bool symbol_map_add_synth(SymbolMap* map, const char* symbol, const uint8_t desc[6])
{
    if (!symbol_map_add(map, symbol, ""))
    {
        return false;
    }
    map->entries[map->count - 1].isSynth = 1;
    memcpy(map->entries[map->count - 1].synthDesc, desc, 6);
    return true;
}

/* Returns the file path for a symbol, or NULL if unknown.  Inline synth
 * entries have no file and are deliberately not returned here; use
 * symbol_map_find_synth for those. */
static const char* symbol_map_find(const SymbolMap* map, const char* symbol)
{
    for (int i = 0; i < map->count; i++)
    {
        if (strcmp(map->entries[i].symbol, symbol) == 0)
            return map->entries[i].isSynth ? NULL : map->entries[i].filePath;
    }
    return NULL;
}

/* Returns the 6 synth descriptor bytes for an inline synth symbol, or NULL. */
static const uint8_t* symbol_map_find_synth(const SymbolMap* map, const char* symbol)
{
    for (int i = 0; i < map->count; i++)
    {
        if (strcmp(map->entries[i].symbol, symbol) == 0)
            return map->entries[i].isSynth ? map->entries[i].synthDesc : NULL;
    }
    return NULL;
}

/*
 * Keysplit map implementation
 */
static void keysplit_map_init(KeySplitMap* map)
{
    map->entries = NULL;
    map->count = 0;
    map->capacity = 0;
    map->parsedFileCount = 0;
}

static void keysplit_map_free(KeySplitMap* map)
{
    free(map->entries);
    map->entries = NULL;
    map->count = 0;
    map->capacity = 0;
}

static KeySplitDef* keysplit_map_find(const KeySplitMap* map, const char* name)
{
    for (int i = 0; i < map->count; i++)
    {
        if (strcmp(map->entries[i].name, name) == 0)
            return &map->entries[i];
    }
    return NULL;
}

/* ---- Directory scanning helpers ---- */

/*
 * Check the first 50 lines of a file for voice macro keywords
 * (voice_directsound, voice_square, voice_keysplit, etc.).
 */
static int file_has_voice_macros(const char* filePath)
{
    FILE* f = fopen(filePath, "r");
    if (!f)
        return 0;
    char line[MAX_LINE];
    int lineCount = 0;
    while (fgets(line, sizeof(line), f) && lineCount < 50)
    {
        if (strstr(line, "voice_directsound") || strstr(line, "voice_square") ||
            strstr(line, "voice_programmable_wave") || strstr(line, "voice_noise") || strstr(line, "voice_keysplit") ||
            strstr(line, "voice_group"))
        {
            fclose(f);
            return 1;
        }
        lineCount++;
    }
    fclose(f);
    return 0;
}

/*
 * Probe for keysplit-table data adjacent to a voicegroup-style directory.
 * Adds <dir>/keysplit_tables.{s,inc} and any .s/.inc files inside <dir>/keysplits/
 * to the keySplitTableFiles list. Safe to call repeatedly (pathlist_add dedups).
 *
 * This supports project layouts (e.g. the eventide pokeemerald fork) that keep
 * keysplit tables next to their voicegroups rather than in sound/keysplit_tables.inc.
 * It is purely additive for standard pokeemerald/pokefirered layouts: if those
 * files/dirs don't exist it is a no-op, and files in a keysplits/ subdir that turn
 * out to be sub-voicegroup definitions rather than keysplit tables are harmless --
 * parse_keysplit_tables_file only acts on lines beginning with "keysplit ".
 */
static void probe_keysplit_data_in_dir(const char* dirPath, ProjectDiscovery* out)
{
    char p[MAX_PATH_LEN];

    build_path(p, sizeof(p), dirPath, "keysplit_tables.inc");
    if (file_exists(p))
        pathlist_add(&out->keySplitTableFiles, p);
    build_path(p, sizeof(p), dirPath, "keysplit_tables.s");
    if (file_exists(p))
        pathlist_add(&out->keySplitTableFiles, p);

    char ksDir[MAX_PATH_LEN];
    build_path(ksDir, sizeof(ksDir), dirPath, "keysplits");
    if (is_directory(ksDir))
    {
        DIR* d = opendir(ksDir);
        if (d)
        {
            struct dirent* ent;
            while ((ent = readdir(d)) != NULL)
            {
                if (ent->d_name[0] == '.')
                    continue;
                if (!str_ends_with_ci(ent->d_name, ".s") && !str_ends_with_ci(ent->d_name, ".inc"))
                    continue;
                char fp[MAX_PATH_LEN];
                build_path(fp, sizeof(fp), ksDir, ent->d_name);
                pathlist_add(&out->keySplitTableFiles, fp);
            }
            closedir(d);
        }
    }
}

/* Facts about one directory, collected in a single readdir() pass. */
#define MAX_DIRENT_NAME 260

typedef struct
{
    int hasWavOrAif;
    int hasKeysplitTablesInc; /* entry named exactly "keysplit_tables.inc" */
    int hasKeysplitTablesS;   /* entry named exactly "keysplit_tables.s"  */
    int hasKeysplitsSubdir;   /* directory entry named exactly "keysplits" */
    char macroCandidates[5][MAX_DIRENT_NAME];
    int macroCandidateCount;          /* first 5 .inc/.s files, readdir order */
    char (*subdirs)[MAX_DIRENT_NAME]; /* heap; readdir order */
    int subdirCount, subdirCapacity;
} DirFacts;

/*
 * Recursively discover voicegroup dirs, wav sample dirs, and keysplit table
 * files under dirPath, enumerating each directory exactly once: collect all
 * facts in a single readdir() pass, apply them (parent's adds before its
 * children's, so PathList ordering matches the old multi-pass scan), then
 * recurse into subdirectories in readdir order.
 */
static void discover_scan_tree(const char* dirPath, int depth, int maxDepth, ProjectDiscovery* out)
{
    DirFacts facts;
    memset(&facts, 0, sizeof(facts));

    DIR* d = opendir(dirPath);
    if (d)
    {
        struct dirent* ent;
        while ((ent = readdir(d)) != NULL)
        {
            if (ent->d_name[0] == '.')
                continue;
            if (dirent_is_dir(dirPath, ent))
            {
                if (strcmp(ent->d_name, "keysplits") == 0)
                    facts.hasKeysplitsSubdir = 1;
                if (facts.subdirCount >= facts.subdirCapacity)
                {
                    size_t newCap =
                        facts.subdirCapacity ? (size_t)facts.subdirCapacity * 2 : INITIAL_CAPACITY;
                    char (*np)[MAX_DIRENT_NAME] = (char(*)[MAX_DIRENT_NAME])realloc(
                        facts.subdirs, sizeof(*facts.subdirs) * newCap);
                    if (!np)
                    {
                        free(facts.subdirs);
                        facts.subdirs = NULL;
                        facts.subdirCapacity = 0;
                        closedir(d);
                        return;
                    }
                    facts.subdirs = np;
                    facts.subdirCapacity = (int)newCap;
                }
                snprintf(facts.subdirs[facts.subdirCount], sizeof(facts.subdirs[0]), "%s", ent->d_name);
                facts.subdirCount++;
            }
            else
            {
                if (str_ends_with_ci(ent->d_name, ".wav") || str_ends_with_ci(ent->d_name, ".aif"))
                    facts.hasWavOrAif = 1;
                if (strcmp(ent->d_name, "keysplit_tables.inc") == 0)
                    facts.hasKeysplitTablesInc = 1;
                else if (strcmp(ent->d_name, "keysplit_tables.s") == 0)
                    facts.hasKeysplitTablesS = 1;
                if (facts.macroCandidateCount < 5 &&
                    (str_ends_with_ci(ent->d_name, ".inc") || str_ends_with_ci(ent->d_name, ".s")))
                {
                    snprintf(facts.macroCandidates[facts.macroCandidateCount],
                             sizeof(facts.macroCandidates[0]),
                             "%s",
                             ent->d_name);
                    facts.macroCandidateCount++;
                }
            }
        }
        closedir(d);
    }

    char p[MAX_PATH_LEN];

    for (int i = 0; i < facts.macroCandidateCount; i++)
    {
        build_path(p, sizeof(p), dirPath, facts.macroCandidates[i]);
        if (file_has_voice_macros(p))
        {
            pathlist_add(&out->voicegroupDirs, dirPath);
            break;
        }
    }

    if (facts.hasWavOrAif)
        pathlist_add(&out->wavSampleDirs, dirPath);

    /* The directory listing already proved these exist; no stat() probes. */
    if (facts.hasKeysplitTablesInc)
    {
        build_path(p, sizeof(p), dirPath, "keysplit_tables.inc");
        pathlist_add(&out->keySplitTableFiles, p);
    }
    if (facts.hasKeysplitTablesS)
    {
        build_path(p, sizeof(p), dirPath, "keysplit_tables.s");
        pathlist_add(&out->keySplitTableFiles, p);
    }

    if (facts.hasKeysplitsSubdir)
    {
        /* Enumerate <dirPath>/keysplits here even though the recursion below
         * may also visit it: a keysplits/ sitting at depth maxDepth + 1 is out
         * of the recursion's reach, and pathlist_add dedups the overlap when
         * it isn't. */
        char ksDir[MAX_PATH_LEN];
        build_path(ksDir, sizeof(ksDir), dirPath, "keysplits");
        DIR* ks = opendir(ksDir);
        if (ks)
        {
            struct dirent* ent;
            while ((ent = readdir(ks)) != NULL)
            {
                if (ent->d_name[0] == '.')
                    continue;
                if (!str_ends_with_ci(ent->d_name, ".s") && !str_ends_with_ci(ent->d_name, ".inc"))
                    continue;
                build_path(p, sizeof(p), ksDir, ent->d_name);
                pathlist_add(&out->keySplitTableFiles, p);
            }
            closedir(ks);
        }
    }

    if (depth < maxDepth)
    {
        for (int i = 0; i < facts.subdirCount; i++)
        {
            char subPath[MAX_PATH_LEN];
            build_path(subPath, sizeof(subPath), dirPath, facts.subdirs[i]);
            discover_scan_tree(subPath, depth + 1, maxDepth, out);
        }
    }
    free(facts.subdirs);
}

/*
 * Check if a file is a monolithic voicegroup file (contains multiple labeled voicegroups).
 * Heuristic: file has multiple `<word>::` labels AND contains voice macros,
 * but is NOT just a list of .include directives pointing to a voicegroups/ subdir.
 */
static int is_monolithic_voicegroup_file(const char* filePath)
{
    FILE* f = fopen(filePath, "r");
    if (!f)
        return 0;

    char line[MAX_LINE];
    int labelCount = 0;
    int voiceMacroCount = 0;
    int includeCount = 0;
    int lineCount = 0;

    while (fgets(line, sizeof(line), f) && lineCount < 500)
    {
        strip_comment(line);
        rtrim(line);
        char* trimmed = ltrim(line);

        if (strstr(trimmed, "::") && trimmed[0] != '.' && trimmed[0] != '\0')
        {
            labelCount++;
        }
        if (strstr(trimmed, "voice_directsound") || strstr(trimmed, "voice_square") ||
            strstr(trimmed, "voice_programmable_wave") || strstr(trimmed, "voice_noise") ||
            strstr(trimmed, "voice_keysplit") || strstr(trimmed, "voice_group"))
        {
            voiceMacroCount++;
        }
        if (strstr(trimmed, ".include"))
        {
            includeCount++;
        }
        lineCount++;
    }
    fclose(f);

    /* It's monolithic if it has multiple labels AND voice macros,
     * and it's NOT primarily a hub of .include directives */
    if (labelCount >= 2 && voiceMacroCount > 0 && voiceMacroCount > includeCount)
    {
        return 1;
    }
    return 0;
}

/* ---- Project discovery ---- */

static void discover_project(const char* projectRoot, const VoicegroupLoaderConfig* cfg, ProjectDiscovery* out)
{
    memset(out, 0, sizeof(ProjectDiscovery));
    snprintf(out->projectRoot, sizeof(out->projectRoot), "%s", projectRoot);
    out->cfg = cfg;

    char path[MAX_PATH_LEN];
    char soundDir[MAX_PATH_LEN];
    build_path(soundDir, sizeof(soundDir), projectRoot, "sound");
    vg_log("discover_project: soundDir='%s' exists=%d", soundDir, is_directory(soundDir));

    /* 1. Config overrides first (prepended) */
    if (cfg)
    {
        for (int i = 0; i < cfg->soundDataPathCount && i < VG_CONFIG_PATH_CAP; i++)
        {
            build_path(path, sizeof(path), projectRoot, cfg->soundDataPaths[i]);
            if (file_exists(path))
                pathlist_add(&out->directSoundDataFiles, path);
        }
        for (int i = 0; i < cfg->voicegroupPathCount && i < VG_CONFIG_PATH_CAP; i++)
        {
            build_path(path, sizeof(path), projectRoot, cfg->voicegroupPaths[i]);
            if (is_directory(path))
            {
                /* If it's a directory, add as voicegroup dir and scan for voice macros */
                pathlist_add(&out->voicegroupDirs, path);
                /* Also check if files inside are monolithic */
                DIR* d = opendir(path);
                if (d)
                {
                    struct dirent* ent;
                    while ((ent = readdir(d)) != NULL)
                    {
                        if (ent->d_name[0] == '.')
                            continue;
                        if (str_ends_with_ci(ent->d_name, ".inc") || str_ends_with_ci(ent->d_name, ".s"))
                        {
                            char fpath[MAX_PATH_LEN];
                            build_path(fpath, sizeof(fpath), path, ent->d_name);
                            if (is_monolithic_voicegroup_file(fpath))
                                pathlist_add(&out->monolithicVGFiles, fpath);
                        }
                    }
                    closedir(d);
                }
                probe_keysplit_data_in_dir(path, out);
            }
            else if (file_exists(path))
            {
                /* It's a file - check if it's monolithic or a voicegroup dir entry */
                if (is_monolithic_voicegroup_file(path))
                    pathlist_add(&out->monolithicVGFiles, path);
            }
        }
        for (int i = 0; i < cfg->sampleDirCount && i < VG_CONFIG_PATH_CAP; i++)
        {
            build_path(path, sizeof(path), projectRoot, cfg->sampleDirs[i]);
            if (is_directory(path))
                pathlist_add(&out->wavSampleDirs, path);
        }
    }

    /* 2. Standard direct_sound_data.inc, programmable_wave_data.inc, keysplit_tables.inc */
    build_path(path, sizeof(path), projectRoot, "sound/direct_sound_data.inc");
    if (file_exists(path))
        pathlist_add(&out->directSoundDataFiles, path);

    /* Inline Golden Sun synth definitions (pokeemerald-expansion layout);
     * parsed by the same direct_sound_data parser, which recognizes the
     * set_synth_* macros. */
    build_path(path, sizeof(path), projectRoot, "sound/direct_sound_synth_data.inc");
    if (file_exists(path))
        pathlist_add(&out->directSoundDataFiles, path);

    build_path(path, sizeof(path), projectRoot, "sound/programmable_wave_data.inc");
    if (file_exists(path))
        pathlist_add(&out->progWaveDataFiles, path);

    build_path(path, sizeof(path), projectRoot, "sound/keysplit_tables.inc");
    if (file_exists(path))
        pathlist_add(&out->keySplitTableFiles, path);

    /* 3. Standard voicegroup directories */
    build_path(path, sizeof(path), projectRoot, "sound/voicegroups");
    if (is_directory(path))
    {
        pathlist_add(&out->voicegroupDirs, path);
        /* Also add keysplits/ and drumsets/ subdirs */
        char subPath[MAX_PATH_LEN];
        build_path(subPath, sizeof(subPath), path, "keysplits");
        if (is_directory(subPath))
            pathlist_add(&out->voicegroupDirs, subPath);
        build_path(subPath, sizeof(subPath), path, "drumsets");
        if (is_directory(subPath))
            pathlist_add(&out->voicegroupDirs, subPath);
    }

    /* 4. The recursive scan under sound/ for voicegroup dirs and wav dirs is
     * deferred to discovery_ensure_deep_scan(): it runs only when a lookup
     * misses the eager entries above (stock layouts never need it). */

    /* 5. Check for monolithic voicegroup files */
    build_path(path, sizeof(path), projectRoot, "sound/voice_groups.inc");
    vg_log("discover_project: checking monolithic '%s' exists=%d", path, file_exists(path));
    if (file_exists(path) && is_monolithic_voicegroup_file(path))
        pathlist_add(&out->monolithicVGFiles, path);
}

/*
 * Run discover_project's deferred recursive sound/ scan (step 4), at most
 * once per discovery. The scan exists only to support nonstandard project
 * layouts (e.g. the eventide fork); on stock projects every lookup is
 * satisfied by the eager entries and this never runs. Deferral preserves
 * behavior: the scan appends to PathLists that already hold the eager
 * entries, and every consumer iterates them in order, first-hit-wins, so
 * eager hits resolve identically with or without the scanned tail.
 *
 * NOTE: this is lazy work within a single voicegroup_load(_samples) call,
 * not cross-call caching — disc still lives and dies with the call.
 */
static void discovery_ensure_deep_scan(ProjectDiscovery* disc)
{
    if (disc->deepScanned)
        return;
    disc->deepScanned = 1;
    vg_log("discovery: deep scan triggered");
    char soundDir[MAX_PATH_LEN];
    build_path(soundDir, sizeof(soundDir), disc->projectRoot, "sound");
    if (is_directory(soundDir))
        discover_scan_tree(soundDir, 0, 3, disc);
    vg_log("discovery: deep scan done, vgDirs=%d wavDirs=%d", disc->voicegroupDirs.count, disc->wavSampleDirs.count);
}

/* ---- Symbol data file parsing (parameterized by file path) ---- */

/*
 * Parse a "set_synth_*" macro line into a Golden Sun synth descriptor (the 6
 * bytes that follow a zero-size WaveData header).  Returns 1 and fills desc
 * on success, 0 if the line is not a synth macro.  Recognized macros, with
 * the pokeemerald-expansion names and the preferred aliases:
 *   set_synth_custom p1, p2, p3, p4   /  set_synth_pulse p1, p2, p3, p4
 *   set_synth_25                      /  set_synth_saw
 *   set_synth_50                      /  set_synth_triangle
 * For the pulse macros: p1 = base duty cycle, p2 = duty LFO step per frame,
 * p3 = modulation amount, p4 = duty LFO phase offset (0x0-0xFF each).
 */
static int parse_synth_macro_line(const char* trimmed, uint8_t desc[6])
{
    static const struct
    {
        const char* name;
        uint8_t type;
        int hasParams;
    } kMacros[] = {
        {"set_synth_custom", 0, 1},
        {"set_synth_pulse", 0, 1},
        {"set_synth_25", 1, 0},
        {"set_synth_saw", 1, 0},
        {"set_synth_50", 2, 0},
        {"set_synth_triangle", 2, 0},
    };

    for (size_t m = 0; m < sizeof(kMacros) / sizeof(kMacros[0]); m++)
    {
        size_t len = strlen(kMacros[m].name);
        if (strncmp(trimmed, kMacros[m].name, len) != 0)
            continue;
        char next = trimmed[len];
        if (next != '\0' && next != ' ' && next != '\t')
            continue; /* avoid e.g. "set_synth_50" matching "set_synth_5" */

        memset(desc, 0, 6);
        desc[0] = 0x80;
        desc[1] = kMacros[m].type;
        if (kMacros[m].hasParams)
        {
            const char* p = trimmed + len;
            for (int n = 0; n < 4; n++)
            {
                while (*p == ' ' || *p == '\t' || *p == ',')
                    p++;
                if (!*p)
                    break;
                desc[2 + n] = (uint8_t)strtoul(p, (char**)&p, 0);
            }
        }
        return 1;
    }
    return 0;
}

/*
 * If a (trimmed) line begins with a "Name::" or "Name:" label, copy the name
 * into out and return 1.  GAS accepts both forms — a single colon merely
 * makes the symbol file-local, which still assembles and links because
 * sample data lives in the same assembly unit as the voicegroups that
 * reference it.  Voicegroup labels elsewhere stay strictly "::": songs
 * reference those from separate assembly units.
 */
static int parse_sample_label(const char* trimmed, char* out, size_t outSize)
{
    size_t nameLen = strspn(trimmed,
                            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                            "abcdefghijklmnopqrstuvwxyz0123456789_");
    if (nameLen == 0 || trimmed[nameLen] != ':')
        return 0;
    if (nameLen >= outSize)
        return -1;
    memcpy(out, trimmed, nameLen);
    out[nameLen] = '\0';
    return 1;
}

/*
 * Parse a direct_sound_data .inc file.
 * Builds symbol name -> file path mapping.
 */
static int parse_direct_sound_data_file(const char* filePath, const char* projectRoot, SymbolMap* map)
{
    (void)projectRoot; /* paths inside the file are relative to projectRoot, stored as-is */
    FILE* f = fopen(filePath, "r");
    if (!f)
    {
        fprintf(stderr, "voicegroup_loader: cannot open %s\n", filePath);
        return -1;
    }

    char line[MAX_LINE];
    char currentSymbol[MAX_SYMBOL_LEN] = {0};

    while (fgets(line, sizeof(line), f))
    {
        strip_comment(line);
        rtrim(line);
        char* trimmed = ltrim(line);

        /* Look for "label::" / "label:" lines */
        int lab = parse_sample_label(trimmed, currentSymbol, MAX_SYMBOL_LEN);
        if (lab == -1)
        {
            fclose(f);
            return -1;
        }
        if (lab == 1)
            continue;
        /* Look for .incbin lines */
        if (currentSymbol[0] && strstr(trimmed, ".incbin"))
        {
            char* quote1 = strchr(trimmed, '"');
            if (quote1)
            {
                quote1++;
                char* quote2 = strchr(quote1, '"');
                if (quote2)
                {
                    *quote2 = '\0';
                    if (!symbol_map_add(map, currentSymbol, quote1))
                    {
                        fclose(f);
                        return -1;
                    }
                }
            }
            currentSymbol[0] = '\0';
        }
        /* Inline Golden Sun synth definitions (set_synth_* macros) */
        else if (currentSymbol[0])
        {
            uint8_t desc[6];
            if (parse_synth_macro_line(trimmed, desc))
            {
                if (!symbol_map_add_synth(map, currentSymbol, desc))
                {
                    fclose(f);
                    return -1;
                }
                currentSymbol[0] = '\0';
            }
        }
    }

    fclose(f);
    return 0;
}

/*
 * Parse a programmable_wave_data .inc file.
 */
static int parse_programmable_wave_data_file(const char* filePath, const char* projectRoot, SymbolMap* map)
{
    (void)projectRoot;
    FILE* f = fopen(filePath, "r");
    if (!f)
    {
        fprintf(stderr, "voicegroup_loader: cannot open %s\n", filePath);
        return -1;
    }

    char line[MAX_LINE];
    char currentSymbol[MAX_SYMBOL_LEN] = {0};

    while (fgets(line, sizeof(line), f))
    {
        strip_comment(line);
        rtrim(line);
        char* trimmed = ltrim(line);

        int lab = parse_sample_label(trimmed, currentSymbol, MAX_SYMBOL_LEN);
        if (lab == -1)
        {
            fclose(f);
            return -1;
        }
        if (lab == 1)
            continue;
        if (currentSymbol[0] && strstr(trimmed, ".incbin"))
        {
            char* quote1 = strchr(trimmed, '"');
            if (quote1)
            {
                quote1++;
                char* quote2 = strchr(quote1, '"');
                if (quote2)
                {
                    *quote2 = '\0';
                    if (!symbol_map_add(map, currentSymbol, quote1))
                    {
                        fclose(f);
                        return -1;
                    }
                }
            }
            currentSymbol[0] = '\0';
        }
    }

    fclose(f);
    return 0;
}

/*
 * Parse a keysplit_tables .inc file.
 */
static int parse_keysplit_tables_file(const char* filePath, KeySplitMap* map)
{
    FILE* f = fopen(filePath, "r");
    if (!f)
    {
        fprintf(stderr, "voicegroup_loader: cannot open %s\n", filePath);
        return -1;
    }

    char line[MAX_LINE];
    KeySplitDef* current = NULL;
    int lastNote = 0;

    while (fgets(line, sizeof(line), f))
    {
        strip_comment(line);
        rtrim(line);
        char* trimmed = ltrim(line);

        if (strncmp(trimmed, "keysplit ", 9) == 0)
        {
            const char* q = trimmed + 9;
            char name[MAX_SYMBOL_LEN];
            int rc = vg_extract_comma_symbol(&q, name, sizeof(name));
            if (rc != 0)
            {
                fclose(f);
                return -1;
            }
            int startNote = 0;
            if (!vg_parse_next_int(&q, &startNote))
            {
                fclose(f);
                return -1;
            }
            const char* tail = q;
            while (*tail == ' ' || *tail == '\t')
            {
                tail++;
            }
            if (*tail != '\0')
            {
                fclose(f);
                return -1;
            }
            if (strlen(name) >= MAX_SYMBOL_LEN - 9)
            {
                fclose(f);
                return -1;
            }
            rtrim(name);
            if (startNote < 0 || startNote > 127)
            {
                fclose(f);
                return -1;
            }
            if (map->count >= map->capacity)
            {
                size_t newCap = map->capacity ? (size_t)map->capacity * 2 : INITIAL_CAPACITY;
                KeySplitDef* np =
                    (KeySplitDef*)realloc(map->entries, sizeof(KeySplitDef) * newCap);
                if (!np)
                {
                    fclose(f);
                    return -1;
                }
                map->entries = np;
                map->capacity = (int)newCap;
            }
            current = &map->entries[map->count];
            memset(current, 0, sizeof(KeySplitDef));
            {
                int n = snprintf(current->name, MAX_SYMBOL_LEN, "keysplit_%s", name);
                if (n < 0 || n >= MAX_SYMBOL_LEN)
                {
                    fclose(f);
                    return -1;
                }
            }
            current->startingNote = startNote;
            current->maxNote = 0;
            lastNote = startNote;
            map->count++;
        }
        else if (strncmp(trimmed, "split ", 6) == 0 && current)
        {
            const char* q = trimmed + 6;
            int index = 0;
            int endNote = 0;
            if (!vg_parse_next_int(&q, &index))
            {
                fclose(f);
                return -1;
            }
            if (!vg_expect_comma(&q))
            {
                fclose(f);
                return -1;
            }
            if (!vg_parse_next_int(&q, &endNote))
            {
                fclose(f);
                return -1;
            }
            const char* tail = q;
            while (*tail == ' ' || *tail == '\t')
            {
                tail++;
            }
            if (*tail != '\0')
            {
                fclose(f);
                return -1;
            }
            if (index < 0 || index > 127)
            {
                fclose(f);
                return -1;
            }
            if (endNote < 0 || endNote > 128)
            {
                fclose(f);
                return -1;
            }
            if (lastNote < 0 || lastNote > 128)
            {
                fclose(f);
                return -1;
            }
            if (endNote < lastNote)
            {
                fclose(f);
                return -1;
            }
            for (int n = lastNote; n < endNote; n++)
            {
                current->table[n] = (uint8_t)index;
            }
            lastNote = endNote;
            if (endNote > current->maxNote)
            {
                current->maxNote = endNote;
            }
        }
        else if (strncmp(trimmed, ".set ", 5) == 0)
        {
            const char* q = trimmed + 5;
            char name[MAX_SYMBOL_LEN];
            int rc = vg_extract_comma_symbol(&q, name, sizeof(name));
            if (rc != 0)
            {
                fclose(f);
                return -1;
            }
            while (*q == ' ' || *q == '\t')
            {
                q++;
            }
            if (*q != '.')
            {
                fclose(f);
                return -1;
            }
            q++;
            while (*q == ' ' || *q == '\t')
            {
                q++;
            }
            if (*q != '-')
            {
                fclose(f);
                return -1;
            }
            q++;
            int startNote = 0;
            if (!vg_parse_next_int(&q, &startNote))
            {
                fclose(f);
                return -1;
            }
            const char* tail = q;
            while (*tail == ' ' || *tail == '\t')
            {
                tail++;
            }
            if (*tail != '\0')
            {
                fclose(f);
                return -1;
            }
            if (strlen(name) >= MAX_SYMBOL_LEN)
            {
                fclose(f);
                return -1;
            }
            rtrim(name);
            if (startNote < 0 || startNote > 127)
            {
                fclose(f);
                return -1;
            }
            if (map->count >= map->capacity)
            {
                size_t newCap = map->capacity ? (size_t)map->capacity * 2 : INITIAL_CAPACITY;
                KeySplitDef* np =
                    (KeySplitDef*)realloc(map->entries, sizeof(KeySplitDef) * newCap);
                if (!np)
                {
                    fclose(f);
                    return -1;
                }
                map->entries = np;
                map->capacity = (int)newCap;
            }
            current = &map->entries[map->count];
            memset(current, 0, sizeof(KeySplitDef));
            strncpy(current->name, name, MAX_SYMBOL_LEN - 1);
            current->name[MAX_SYMBOL_LEN - 1] = '\0';
            current->startingNote = startNote;
            current->maxNote = 0;
            lastNote = startNote;
            map->count++;
        }
        else if (strncmp(trimmed, ".byte ", 6) == 0 && current)
        {
            const char* q = trimmed + 6;
            while (*q)
            {
                while (*q == ' ' || *q == '\t' || *q == ',')
                {
                    q++;
                }
                if (*q == '\0')
                {
                    break;
                }
                int val = 0;
                if (!vg_parse_next_int(&q, &val))
                {
                    fclose(f);
                    return -1;
                }
                if (val < 0 || val > 127)
                {
                    fclose(f);
                    return -1;
                }
                if (lastNote < 0 || lastNote >= 128)
                {
                    fclose(f);
                    return -1;
                }
                current->table[lastNote] = (uint8_t)val;
                if (lastNote > current->maxNote)
                {
                    current->maxNote = lastNote;
                }
                lastNote++;
            }
        }
    }

    fclose(f);
    return 0;
}

/* Wrappers that iterate over all discovered paths */

static bool parse_all_direct_sound_data(
    const ProjectDiscovery* disc, const char* projectRoot, SymbolMap* map)
{
    for (int i = 0; i < disc->directSoundDataFiles.count; i++)
    {
        if (parse_direct_sound_data_file(disc->directSoundDataFiles.paths[i], projectRoot, map) != 0)
        {
            return false;
        }
    }
    return true;
}

static bool parse_all_programmable_wave_data(
    const ProjectDiscovery* disc, const char* projectRoot, SymbolMap* map)
{
    for (int i = 0; i < disc->progWaveDataFiles.count; i++)
    {
        if (parse_programmable_wave_data_file(
                disc->progWaveDataFiles.paths[i], projectRoot, map)
            != 0)
        {
            return false;
        }
    }
    return true;
}

static bool parse_keysplit_tables_range(
    const ProjectDiscovery* disc, KeySplitMap* map, int fromIndex)
{
    for (int i = fromIndex; i < disc->keySplitTableFiles.count; i++)
    {
        if (parse_keysplit_tables_file(disc->keySplitTableFiles.paths[i], map) != 0)
        {
            return false;
        }
    }
    map->parsedFileCount = disc->keySplitTableFiles.count;
    return true;
}

static bool parse_all_keysplit_tables(const ProjectDiscovery* disc, KeySplitMap* map)
{
    return parse_keysplit_tables_range(disc, map, 0);
}


static bool keysplit_map_find_or_rescan_checked(
    KeySplitMap* map, const char* name, ProjectDiscovery* disc, KeySplitDef** out)
{
    KeySplitDef* ks = keysplit_map_find(map, name);
    if (ks || !disc)
    {
        if (out)
        {
            *out = ks;
        }
        return true;
    }
    if (disc->deepScanned && map->parsedFileCount >= disc->keySplitTableFiles.count)
    {
        if (out)
        {
            *out = NULL;
        }
        return true;
    }
    discovery_ensure_deep_scan(disc);
    int savedCount = map->count;
    int savedParsed = map->parsedFileCount;
    if (!parse_keysplit_tables_range(disc, map, map->parsedFileCount))
    {
        for (int i = savedCount; i < map->count; i++)
        {
            memset(&map->entries[i], 0, sizeof(KeySplitDef));
        }
        map->count = savedCount;
        map->parsedFileCount = savedParsed;
        if (out)
        {
            *out = NULL;
        }
        return false;
    }
    ks = keysplit_map_find(map, name);
    if (out)
    {
        *out = ks;
    }
    return true;
}

/* ---- Sample loading ---- */

/*
 * Load a .wav file from an absolute path.
 * Parses RIFF/WAVE fmt, smpl, agbp, agbl, and data chunks.
 */
static WaveData* load_wav_from_path(const char* absoluteWavPath, bool* hardFailure)
{
    return vg_asset_load_wav_file(absoluteWavPath, hardFailure);
}
static WaveData* load_aif_from_path(const char* absoluteAifPath, bool* hardFailure)
{
    return vg_asset_load_aiff_file(absoluteAifPath, hardFailure);
}


static int resolve_and_load_sample_serial(const char* projectRoot,
                                          const char* symbol,
                                          const SymbolMap* dsMap,
                                          ProjectDiscovery* disc,
                                          LoadedVoiceGroup* vg,
                                          WaveCache* waveCache,
                                          WaveData** outWd)
{
    if (outWd)
        *outWd = NULL;
    const uint8_t* synthDesc = symbol_map_find_synth(dsMap, symbol);
    if (synthDesc)
    {
        char cacheKey[MAX_PATH_LEN];
        snprintf(cacheKey, sizeof(cacheKey), "synth-macro:%s", symbol);
        WaveData* cached = wave_cache_find(waveCache, cacheKey);
        if (cached)
        {
            if (outWd)
                *outWd = cached;
            return 1;
        }
        uint32_t dataSize = 16;
        WaveData* wd = calloc(1, sizeof(WaveData) + dataSize + 1);
        if (!wd)
            return -1;
        wd->type = 0;
        wd->status = 0x4000;
        wd->freq = 0x01058920;
        wd->loopStart = 0;
        wd->size = 0;
        wd->data = (int8_t*)((uint8_t*)wd + sizeof(WaveData));
        memcpy(wd->data, synthDesc, 6);
        if (!vg_register_wavedata(vg, wd))
        {
            free(wd);
            return -1;
        }
        wave_cache_insert(waveCache, cacheKey, wd);
        if (outWd)
            *outWd = wd;
        return 1;
    }
    /* Fallback: search sample directories (.wav, then .aif) */
    if (disc)
    {
        if (!disc->deepScanned)
            discovery_ensure_deep_scan(disc);
        for (int i = 0; i < disc->wavSampleDirs.count; i++)
        {
            for (int fmt = 0; fmt < 2; fmt++)
            {
                char wavPath[MAX_PATH_LEN];
                snprintf(wavPath,
                         sizeof(wavPath),
                         "%s%c%s.%s",
                         disc->wavSampleDirs.paths[i],
                         PATH_SEP,
                         symbol,
                         fmt == 0 ? "wav" : "aif");
                WaveData* cached = wave_cache_find(waveCache, wavPath);
                if (cached)
                {
                    if (outWd)
                        *outWd = cached;
                    return 1;
                }
                bool hardFailure = false;
                WaveData* wd = fmt == 0 ? load_wav_from_path(wavPath, &hardFailure)
                                        : load_aif_from_path(wavPath, &hardFailure);
                if (hardFailure)
                    return -1;
                if (wd)
                {
                    if (!vg_register_wavedata(vg, wd))
                    {
                        free(wd);
                        return -1;
                    }
                    wave_cache_insert(waveCache, wavPath, wd);
                    if (outWd)
                        *outWd = wd;
                    return 1;
                }
            }
        }
    }
    return 0;
}

/* ---- Flexible voicegroup finding ---- */

/* Returns 1 if the last path component of dirPath equals name. */
static int dir_last_component_is(const char* dirPath, const char* name)
{
    size_t dlen = strlen(dirPath);
    size_t nlen = strlen(name);
    if (nlen > dlen)
        return 0;
    const char* tail = dirPath + dlen - nlen;
    if (strcmp(tail, name) != 0)
        return 0;
    if (tail == dirPath)
        return 1;
    char c = *(tail - 1);
    return c == '/' || c == '\\';
}

/*
 * Search for a voicegroup by name across all currently discovered locations.
 */
static VoicegroupLocation
find_voicegroup_probe(const char* projectRoot, const char* vgName, const ProjectDiscovery* disc)
{
    VoicegroupLocation loc;
    memset(&loc, 0, sizeof(loc));

    char path[MAX_PATH_LEN];

    /* 1. Individual files in discovered voicegroup directories */
    for (int i = 0; i < disc->voicegroupDirs.count; i++)
    {
        /* Try <dir>/<name>.inc */
        snprintf(path, sizeof(path), "%s%c%s.inc", disc->voicegroupDirs.paths[i], PATH_SEP, vgName);
        if (file_exists(path))
        {
            strncpy(loc.filePath, path, MAX_PATH_LEN - 1);
            loc.found = 1;
            return loc;
        }
        /* Try <dir>/<name>.s */
        snprintf(path, sizeof(path), "%s%c%s.s", disc->voicegroupDirs.paths[i], PATH_SEP, vgName);
        if (file_exists(path))
        {
            strncpy(loc.filePath, path, MAX_PATH_LEN - 1);
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
        const char* suffix = strstr(vgName, "_keysplit");
        if (suffix)
        {
            char baseName[MAX_SYMBOL_LEN];
            int baseLen = (int)(suffix - vgName);
            if (baseLen > 0 && baseLen < MAX_SYMBOL_LEN)
            {
                memcpy(baseName, vgName, baseLen);
                baseName[baseLen] = '\0';
                /* Explicit <dir>/keysplits/<base>.inc probe for each voicegroup dir */
                for (int i = 0; i < disc->voicegroupDirs.count; i++)
                {
                    snprintf(path,
                             sizeof(path),
                             "%s%ckeysplits%c%s.inc",
                             disc->voicegroupDirs.paths[i],
                             PATH_SEP,
                             PATH_SEP,
                             baseName);
                    if (file_exists(path))
                    {
                        strncpy(loc.filePath, path, MAX_PATH_LEN - 1);
                        loc.found = 1;
                        return loc;
                    }
                    snprintf(path,
                             sizeof(path),
                             "%s%ckeysplits%c%s.s",
                             disc->voicegroupDirs.paths[i],
                             PATH_SEP,
                             PATH_SEP,
                             baseName);
                    if (file_exists(path))
                    {
                        strncpy(loc.filePath, path, MAX_PATH_LEN - 1);
                        loc.found = 1;
                        return loc;
                    }
                }
                /* Also check dirs that are themselves named "keysplits" */
                for (int i = 0; i < disc->voicegroupDirs.count; i++)
                {
                    if (!dir_last_component_is(disc->voicegroupDirs.paths[i], "keysplits"))
                        continue;
                    snprintf(path, sizeof(path), "%s%c%s.inc", disc->voicegroupDirs.paths[i], PATH_SEP, baseName);
                    if (file_exists(path))
                    {
                        strncpy(loc.filePath, path, MAX_PATH_LEN - 1);
                        loc.found = 1;
                        return loc;
                    }
                    snprintf(path, sizeof(path), "%s%c%s.s", disc->voicegroupDirs.paths[i], PATH_SEP, baseName);
                    if (file_exists(path))
                    {
                        strncpy(loc.filePath, path, MAX_PATH_LEN - 1);
                        loc.found = 1;
                        return loc;
                    }
                }
            }
        }
    }
    {
        const char* suffix = strstr(vgName, "_drumset");
        if (suffix)
        {
            char baseName[MAX_SYMBOL_LEN];
            int baseLen = (int)(suffix - vgName);
            /* The drumset file keeps whatever follows "_drumset" (e.g.
             * voicegroup_emerald_drumset_1 -> drumsets/emerald_1.inc, and
             * voicegroup_frlg_drumset -> drumsets/frlg.inc), so splice the
             * "_drumset" infix out rather than truncating the name at it. */
            const char* tail = suffix + 8; /* strlen("_drumset") */
            if (baseLen > 0 && baseLen + (int)strlen(tail) < MAX_SYMBOL_LEN)
            {
                memcpy(baseName, vgName, baseLen);
                strcpy(baseName + baseLen, tail);
                /* Explicit <dir>/drumsets/<base>.inc probe for each voicegroup dir */
                for (int i = 0; i < disc->voicegroupDirs.count; i++)
                {
                    snprintf(path,
                             sizeof(path),
                             "%s%cdrumsets%c%s.inc",
                             disc->voicegroupDirs.paths[i],
                             PATH_SEP,
                             PATH_SEP,
                             baseName);
                    if (file_exists(path))
                    {
                        strncpy(loc.filePath, path, MAX_PATH_LEN - 1);
                        loc.found = 1;
                        return loc;
                    }
                    snprintf(path,
                             sizeof(path),
                             "%s%cdrumsets%c%s.s",
                             disc->voicegroupDirs.paths[i],
                             PATH_SEP,
                             PATH_SEP,
                             baseName);
                    if (file_exists(path))
                    {
                        strncpy(loc.filePath, path, MAX_PATH_LEN - 1);
                        loc.found = 1;
                        return loc;
                    }
                }
                /* Also check dirs that are themselves named "drumsets" */
                for (int i = 0; i < disc->voicegroupDirs.count; i++)
                {
                    if (!dir_last_component_is(disc->voicegroupDirs.paths[i], "drumsets"))
                        continue;
                    snprintf(path, sizeof(path), "%s%c%s.inc", disc->voicegroupDirs.paths[i], PATH_SEP, baseName);
                    if (file_exists(path))
                    {
                        strncpy(loc.filePath, path, MAX_PATH_LEN - 1);
                        loc.found = 1;
                        return loc;
                    }
                    snprintf(path, sizeof(path), "%s%c%s.s", disc->voicegroupDirs.paths[i], PATH_SEP, baseName);
                    if (file_exists(path))
                    {
                        strncpy(loc.filePath, path, MAX_PATH_LEN - 1);
                        loc.found = 1;
                        return loc;
                    }
                }
            }
        }
    }

    /* 3. Also try vg_<name>.s and vg_<name>.inc patterns (eventide convention) */
    for (int i = 0; i < disc->voicegroupDirs.count; i++)
    {
        snprintf(path, sizeof(path), "%s%cvg_%s.inc", disc->voicegroupDirs.paths[i], PATH_SEP, vgName);
        if (file_exists(path))
        {
            strncpy(loc.filePath, path, MAX_PATH_LEN - 1);
            loc.found = 1;
            return loc;
        }
        snprintf(path, sizeof(path), "%s%cvg_%s.s", disc->voicegroupDirs.paths[i], PATH_SEP, vgName);
        if (file_exists(path))
        {
            strncpy(loc.filePath, path, MAX_PATH_LEN - 1);
            loc.found = 1;
            return loc;
        }
    }

    /* 4. Monolithic files: scan for <name>:: label */
    for (int i = 0; i < disc->monolithicVGFiles.count; i++)
    {
        if (strlen(vgName) >= MAX_SYMBOL_LEN)
        {
            continue;
        }
        FILE* f = fopen(disc->monolithicVGFiles.paths[i], "r");
        if (!f)
        {
            continue;
        }

        char searchLabel[MAX_SYMBOL_LEN + 4];
        int sl = snprintf(searchLabel, sizeof(searchLabel), "%s::", vgName);
        if (sl < 0 || (size_t)sl >= sizeof(searchLabel))
        {
            fclose(f);
            continue;
        }
        size_t searchLen = (size_t)sl;

        char line[MAX_LINE];
        while (fgets(line, sizeof(line), f))
        {
            strip_comment(line);
            char* trimmed = ltrim(line);
            if (strncmp(trimmed, searchLabel, searchLen) == 0)
            {
                char c = trimmed[searchLen];
                if (c == '\0' || isspace((unsigned char)c))
                {
                    strncpy(loc.filePath, disc->monolithicVGFiles.paths[i], MAX_PATH_LEN - 1);
                    loc.filePath[MAX_PATH_LEN - 1] = '\0';
                    strncpy(loc.label, vgName, MAX_SYMBOL_LEN - 1);
                    loc.label[MAX_SYMBOL_LEN - 1] = '\0';
                    loc.found = 1;
                    fclose(f);
                    return loc;
                }
            }
        }
        fclose(f);
    }

    return loc;
}

/*
 * Search for a voicegroup by name; on a miss, run the deferred deep scan
 * (which can add voicegroup dirs from nonstandard layouts) and probe again.
 */
static VoicegroupLocation find_voicegroup(const char* projectRoot, const char* vgName, ProjectDiscovery* disc)
{
    VoicegroupLocation loc = find_voicegroup_probe(projectRoot, vgName, disc);
    if (!loc.found && !disc->deepScanned)
    {
        discovery_ensure_deep_scan(disc);
        loc = find_voicegroup_probe(projectRoot, vgName, disc);
    }
    return loc;
}

/* ---- Voicegroup parsing ---- */

/* Helper: last path component (handles both separators). */
static const char* path_basename(const char* path)
{
    const char* base = path;
    for (const char* p = path; *p; p++)
    {
        if (*p == '/' || *p == '\\')
            base = p + 1;
    }
    return base;
}

/*
 * ROM-contiguity successor of a per-file voicegroup: voicegroups are
 * assembled back to back in voice_groups.inc include order, so indexing past
 * one group's end reads the next included group's entries on the GBA.
 * Finds the .include line matching currentFilePath's basename and returns
 * (in outPath) the absolute path of the next included file that exists.
 */
static int next_included_voicegroup(const char* projectRoot, const char* currentFilePath, char* outPath, size_t outSize)
{
    static const char* indexFiles[] = {"sound/voice_groups.inc", "sound/voicegroups.inc"};
    const char* curBase = path_basename(currentFilePath);

    for (size_t i = 0; i < sizeof(indexFiles) / sizeof(indexFiles[0]); i++)
    {
        char indexPath[MAX_PATH_LEN];
        build_path(indexPath, sizeof(indexPath), projectRoot, indexFiles[i]);
        FILE* f = fopen(indexPath, "r");
        if (!f)
            continue;

        char line[MAX_LINE];
        int foundCurrent = 0;
        while (fgets(line, sizeof(line), f))
        {
            strip_comment(line);
            char* trimmed = ltrim(line);
            char incPath[MAX_PATH_LEN];
            if (sscanf(trimmed, ".include \"%511[^\"]\"", incPath) != 1)
                continue;
            if (foundCurrent)
            {
                char absPath[MAX_PATH_LEN];
                build_path(absPath, sizeof(absPath), projectRoot, incPath);
                if (file_exists(absPath))
                {
                    strncpy(outPath, absPath, outSize - 1);
                    outPath[outSize - 1] = '\0';
                    fclose(f);
                    return 1;
                }
                /* Included file missing on disk: contiguity is unknowable */
                break;
            }
            if (strcmp(path_basename(incPath), curBase) == 0)
                foundCurrent = 1;
        }
        fclose(f);
        if (foundCurrent)
            break; /* current file located in this index; don't try others */
    }
    return 0;
}


static int load_sub_voicegroup_session(const char* projectRoot,
                                       const char* vgSymbol,
                                       LoadedVoiceGroup* vgReg,
                                       VgLoadSession* session,
                                       const SymbolMap* dsMap,
                                       const SymbolMap* pwMap,
                                       KeySplitMap* ksMap,
                                       ProjectDiscovery* disc,
                                       WaveCache* waveCache,
                                       ToneData** outSub)
{
    if (outSub)
        *outSub = NULL;
    const char* name = vgSymbol;
    if (strncmp(name, "voicegroup_", 11) == 0)
        name += 11;
    VoicegroupLocation loc = find_voicegroup(projectRoot, name, disc);
    if (!loc.found)
    {
        fprintf(stderr, "voicegroup_loader: cannot find sub-voicegroup '%s'\n", vgSymbol);
        return 0;
    }
    const char* startLabel = loc.label[0] ? loc.label : NULL;
    if (vg_load_session_is_active(session, loc.filePath, startLabel))
    {
        fprintf(stderr, "voicegroup_loader: cycle detected for sub-voicegroup '%s' at %s:%s\n", vgSymbol, loc.filePath, startLabel ? startLabel : "(none)");
        return -1;
    }
    ToneData* subVg = (ToneData*)calloc(VOICEGROUP_SIZE, sizeof(ToneData));
    if (!subVg)
        return -1;
    if (!vg_register_subgroup(vgReg, subVg))
    {
        free(subVg);
        return -1;
    }
    if (!vg_load_session_push_location(session, loc.filePath, startLabel))
    {
        return -1;
    }
    VgLoadSessionCheckpoint cp = vg_load_session_checkpoint(session);
    char dummyNames[VOICEGROUP_SIZE][VG_VOICE_NAME_LEN];
    memset(dummyNames, 0, sizeof(dummyNames));
    int endIndex = parse_voicegroup_file_session(projectRoot, loc.filePath, startLabel, subVg, dummyNames, vgReg, session, dsMap, pwMap, ksMap, disc, waveCache, 0, 1, 0);
    if (endIndex > 0 && !startLabel)
    {
        char curPath[MAX_PATH_LEN];
        strncpy(curPath, loc.filePath, sizeof(curPath) - 1);
        curPath[sizeof(curPath) - 1] = '\0';
        for (int hops = 0; endIndex < VOICEGROUP_SIZE && hops < VOICEGROUP_SIZE; hops++)
        {
            char nextPath[MAX_PATH_LEN];
            if (!next_included_voicegroup(projectRoot, curPath, nextPath, sizeof(nextPath)))
                break;
            int r = parse_voicegroup_file_session(projectRoot, nextPath, NULL, subVg, dummyNames, vgReg, session, dsMap, pwMap, ksMap, disc, waveCache, endIndex, 0, 1);
            if (r < 0)
            {
                vg_load_session_rollback(session, cp);
                vg_load_session_pop_location(session);
                return -1;
            }
            if (r <= endIndex)
                break;
            endIndex = r;
            strncpy(curPath, nextPath, sizeof(curPath) - 1);
            curPath[sizeof(curPath) - 1] = '\0';
        }
    }
    if (endIndex < 0)
    {
        vg_load_session_rollback(session, cp);
        vg_load_session_pop_location(session);
        return -1;
    }
    vg_load_session_pop_location(session);
    if (outSub)
        *outSub = subVg;
    return 1;
}

/*
 * Parse a voicegroup file and populate the ToneData array.
 *
 * When startLabel is non-NULL, scanning starts at the "<startLabel>::" label
 * and stops when a new label or .align 2 is encountered (monolithic file mode).
 * When startLabel is NULL, the entire file is parsed (individual file mode).
 *
 * Slot numbering begins at startIndex; returns the slot index after the last
 * parsed voice, or -1 if the file can't be opened.
 *
 * contiguousFill emulates ROM layout for sub-voicegroups: instead of stopping
 * at the section end, parsing continues across label/.align boundaries so
 * indexing past a group's end reaches the neighbors assembled after it, as it
 * does on the GBA (old-style drumsets rely on this: e.g. a 29-voice
 * voicegroup001 whose kick at key 36 is really voicegroup002's slot 7).
 * Voices beyond the first boundary — and every voice when noSubRecurse is set
 * (used for cross-file continuation) — do not load nested sub-voicegroups:
 * the hardware never substitutes a keysplit twice, and not recursing there
 * keeps include-order cycles (group A's overflow reaching a keysplit back
 * into A) from looping forever.
 */

static int parse_voicegroup_file_session(const char* projectRoot,
                                         const char* filePath,
                                         const char* startLabel,
                                         ToneData* destVoices,
                                         char (*destNames)[VG_VOICE_NAME_LEN],
                                         LoadedVoiceGroup* vgReg,
                                         VgLoadSession* session,
                                         const SymbolMap* dsMap,
                                         const SymbolMap* pwMap,
                                         KeySplitMap* ksMap,
                                         ProjectDiscovery* disc,
                                         WaveCache* waveCache,
                                         int startIndex,
                                         int contiguousFill,
                                         int noSubRecurse)
{
    vg_log("parse_voicegroup_file_session: '%s' label='%s' start=%d", filePath, startLabel ? startLabel : "(none)", startIndex);
    FILE* f = fopen(filePath, "r");
    if (!f)
    {
        fprintf(stderr, "voicegroup_loader: cannot open %s\n", filePath);
        return -1;
    }
    int voiceIndex = startIndex;
    int inSection = (startLabel == NULL);
    int labelFound = (startLabel == NULL);
    int voicesParsedInSection = 0;
    int inContinuation = 0;
    char searchLabel[MAX_SYMBOL_LEN + 4];
    size_t searchLen = 0;
    if (startLabel)
    {
        if (!vg_section_label_valid(startLabel))
        {
            fclose(f);
            return -1;
        }
        int sl = snprintf(searchLabel, sizeof(searchLabel), "%s::", startLabel);
        if (sl < 0 || (size_t)sl >= sizeof(searchLabel))
        {
            fclose(f);
            return -1;
        }
        searchLen = (size_t)sl;
    }
    bool hardFail = false;
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f) && voiceIndex < VOICEGROUP_SIZE && !hardFail)
    {
        strip_comment(line);
        rtrim(line);
        char* trimmed = ltrim(line);
        if (trimmed[0] == '\0')
        {
            continue;
        }
        if (startLabel && !inSection)
        {
            if (searchLen > 0 && strncmp(trimmed, searchLabel, searchLen) == 0)
            {
                char c = trimmed[searchLen];
                if (c == '\0' || isspace((unsigned char)c))
                {
                    inSection = 1;
                    labelFound = 1;
                }
            }
            continue;
        }
        if (startLabel && inSection && voicesParsedInSection > 0 && !inContinuation)
        {
            char* cc = strstr(trimmed, "::");
            int boundary = (cc && cc > trimmed && !isspace((unsigned char)trimmed[0])) || strncmp(trimmed, ".align", 6) == 0;
            if (boundary)
            {
                if (!contiguousFill)
                {
                    break;
                }
                inContinuation = 1;
            }
        }
        if (strncmp(trimmed, "voice_group ", 12) == 0)
        {
            if (inContinuation || noSubRecurse)
            {
                break;
            }
            char vgDeclName[MAX_SYMBOL_LEN];
            int startingNote = 0;
            const char* p = trimmed + 12;
            int rc = vg_extract_comma_symbol(&p, vgDeclName, sizeof(vgDeclName));
            if (rc == -1)
            {
                hardFail = true;
                break;
            }
            if (rc == 0 && vg_parse_next_int(&p, &startingNote))
            {
                if (startingNote > 0 && startingNote < VOICEGROUP_SIZE)
                {
                    voiceIndex = startingNote;
                }
            }
            continue;
        }
        if (strncmp(trimmed, "voice_directsound_no_resample ", 30) == 0 || strncmp(trimmed, "voice_directsound_alt ", 22) == 0 || strncmp(trimmed, "voice_directsound ", 18) == 0)
        {
            int off = 0;
            uint8_t vtype = VOICE_DIRECTSOUND;
            if (strncmp(trimmed, "voice_directsound_no_resample ", 30) == 0)
            {
                off = 30;
                vtype = VOICE_DIRECTSOUND_NO_RESAMPLE;
            }
            else if (strncmp(trimmed, "voice_directsound_alt ", 22) == 0)
            {
                off = 22;
                vtype = VOICE_DIRECTSOUND_ALT;
            }
            else
            {
                off = 18;
                vtype = VOICE_DIRECTSOUND;
            }
            const char* p = trimmed + off;
            int key = 0;
            int pan = 0;
            int attack = 0;
            int decay = 0;
            int sustain = 0;
            int release = 0;
            char sampleSymbol[MAX_SYMBOL_LEN];
            if (!vg_parse_next_int(&p, &key) || !vg_expect_comma(&p) || !vg_parse_next_int(&p, &pan) || !vg_expect_comma(&p))
            {
                voiceIndex++;
                voicesParsedInSection++;
                continue;
            }
            int rc = vg_extract_comma_symbol(&p, sampleSymbol, sizeof(sampleSymbol));
            if (rc == -1)
            {
                hardFail = true;
                break;
            }
            if (rc != 0 || !vg_parse_next_int(&p, &attack) || !vg_expect_comma(&p) || !vg_parse_next_int(&p, &decay) || !vg_expect_comma(&p) || !vg_parse_next_int(&p, &sustain) || !vg_expect_comma(&p) || !vg_parse_next_int(&p, &release))
            {
                voiceIndex++;
                voicesParsedInSection++;
                continue;
            }
            if (destNames && voiceIndex >= 0 && voiceIndex < VOICEGROUP_SIZE)
            {
                set_voice_display_name(destNames[voiceIndex], sampleSymbol);
            }
            ToneData* td = &destVoices[voiceIndex];
            td->type = vtype;
            td->key = (uint8_t)key;
            td->panSweep = pan ? (0x80 | pan) : 0;
            td->attack = (uint8_t)attack;
            td->decay = (uint8_t)decay;
            td->sustain = (uint8_t)sustain;
            td->release = (uint8_t)release;
            const uint8_t* synthDesc = symbol_map_find_synth(dsMap, sampleSymbol);
            if (synthDesc)
            {
                WaveData* wd = build_synth_wavedata(synthDesc, sampleSymbol, vgReg, waveCache);
                if (!wd)
                {
                    hardFail = true;
                    break;
                }
                td->wav = wd;
            }
            else
            {
                const char* samplePath = symbol_map_find(dsMap, sampleSymbol);
                if (samplePath)
                {
                    char wavAbs[VG_MAX_PATH_LEN];
                    char aifAbs[VG_MAX_PATH_LEN];
                    char binAbs[VG_MAX_PATH_LEN];
                    if (!build_wave_abs_paths(projectRoot, samplePath, wavAbs, aifAbs, binAbs))
                    {
                        hardFail = true;
                        break;
                    }
                    const char* w = wavAbs[0] ? wavAbs : NULL;
                    const char* a = aifAbs[0] ? aifAbs : NULL;
                    if (!vg_load_session_add_wave(session, &td->wav, w, a, binAbs))
                    {
                        hardFail = true;
                        break;
                    }
                }
                else
                {
                    WaveData* wd = NULL;
                    int res = resolve_and_load_sample_serial(projectRoot, sampleSymbol, dsMap, disc, vgReg, waveCache, &wd);
                    if (res == -1)
                    {
                        hardFail = true;
                        break;
                    }
                    if (res == 1)
                    {
                        td->wav = wd;
                    }
                }
            }
            voiceIndex++;
            voicesParsedInSection++;
        }
        else if (strncmp(trimmed, "voice_square_1_alt ", 19) == 0)
        {
            const char* p = trimmed + 19;
            int key = 0;
            int pan = 0;
            int sweep = 0;
            int duty = 0;
            int attack = 0;
            int decay = 0;
            int sustain = 0;
            int release = 0;
            bool ok = true;
            ok = ok && vg_parse_next_int(&p, &key);
            ok = ok && vg_expect_comma(&p);
            ok = ok && vg_parse_next_int(&p, &pan);
            ok = ok && vg_expect_comma(&p);
            ok = ok && vg_parse_next_int(&p, &sweep);
            ok = ok && vg_expect_comma(&p);
            ok = ok && vg_parse_next_int(&p, &duty);
            ok = ok && vg_expect_comma(&p);
            ok = ok && vg_parse_next_int(&p, &attack);
            ok = ok && vg_expect_comma(&p);
            ok = ok && vg_parse_next_int(&p, &decay);
            ok = ok && vg_expect_comma(&p);
            ok = ok && vg_parse_next_int(&p, &sustain);
            ok = ok && vg_expect_comma(&p);
            ok = ok && vg_parse_next_int(&p, &release);
            if (ok)
            {
                ToneData* td = &destVoices[voiceIndex];
                td->type = VOICE_SQUARE_1_ALT;
                td->key = (uint8_t)key;
                td->panSweep = (uint8_t)sweep;
                td->wavePointer = (uint32_t*)(uintptr_t)(duty & 0x03);
                td->attack = (uint8_t)(attack & 0x07);
                td->decay = (uint8_t)(decay & 0x07);
                td->sustain = (uint8_t)(sustain & 0x0F);
                td->release = (uint8_t)(release & 0x07);
            }
            voiceIndex++;
            voicesParsedInSection++;
        }
        else if (strncmp(trimmed, "voice_square_1 ", 15) == 0)
        {
            const char* p = trimmed + 15;
            int key = 0;
            int pan = 0;
            int sweep = 0;
            int duty = 0;
            int attack = 0;
            int decay = 0;
            int sustain = 0;
            int release = 0;
            bool ok = true;
            ok = ok && vg_parse_next_int(&p, &key);
            ok = ok && vg_expect_comma(&p);
            ok = ok && vg_parse_next_int(&p, &pan);
            ok = ok && vg_expect_comma(&p);
            ok = ok && vg_parse_next_int(&p, &sweep);
            ok = ok && vg_expect_comma(&p);
            ok = ok && vg_parse_next_int(&p, &duty);
            ok = ok && vg_expect_comma(&p);
            ok = ok && vg_parse_next_int(&p, &attack);
            ok = ok && vg_expect_comma(&p);
            ok = ok && vg_parse_next_int(&p, &decay);
            ok = ok && vg_expect_comma(&p);
            ok = ok && vg_parse_next_int(&p, &sustain);
            ok = ok && vg_expect_comma(&p);
            ok = ok && vg_parse_next_int(&p, &release);
            if (ok)
            {
                ToneData* td = &destVoices[voiceIndex];
                td->type = VOICE_SQUARE_1;
                td->key = (uint8_t)key;
                td->panSweep = (uint8_t)sweep;
                td->wavePointer = (uint32_t*)(uintptr_t)(duty & 0x03);
                td->attack = (uint8_t)(attack & 0x07);
                td->decay = (uint8_t)(decay & 0x07);
                td->sustain = (uint8_t)(sustain & 0x0F);
                td->release = (uint8_t)(release & 0x07);
            }
            voiceIndex++;
            voicesParsedInSection++;
        }
        else if (strncmp(trimmed, "voice_square_2_alt ", 19) == 0)
        {
            const char* p = trimmed + 19;
            int key = 0;
            int pan = 0;
            int duty = 0;
            int attack = 0;
            int decay = 0;
            int sustain = 0;
            int release = 0;
            bool ok = true;
            ok = ok && vg_parse_next_int(&p, &key);
            ok = ok && vg_expect_comma(&p);
            ok = ok && vg_parse_next_int(&p, &pan);
            ok = ok && vg_expect_comma(&p);
            ok = ok && vg_parse_next_int(&p, &duty);
            ok = ok && vg_expect_comma(&p);
            ok = ok && vg_parse_next_int(&p, &attack);
            ok = ok && vg_expect_comma(&p);
            ok = ok && vg_parse_next_int(&p, &decay);
            ok = ok && vg_expect_comma(&p);
            ok = ok && vg_parse_next_int(&p, &sustain);
            ok = ok && vg_expect_comma(&p);
            ok = ok && vg_parse_next_int(&p, &release);
            if (ok)
            {
                ToneData* td = &destVoices[voiceIndex];
                td->type = VOICE_SQUARE_2_ALT;
                td->key = (uint8_t)key;
                td->panSweep = 0;
                td->wavePointer = (uint32_t*)(uintptr_t)(duty & 0x03);
                td->attack = (uint8_t)(attack & 0x07);
                td->decay = (uint8_t)(decay & 0x07);
                td->sustain = (uint8_t)(sustain & 0x0F);
                td->release = (uint8_t)(release & 0x07);
            }
            voiceIndex++;
            voicesParsedInSection++;
        }
        else if (strncmp(trimmed, "voice_square_2 ", 15) == 0)
        {
            const char* p = trimmed + 15;
            int key = 0;
            int pan = 0;
            int duty = 0;
            int attack = 0;
            int decay = 0;
            int sustain = 0;
            int release = 0;
            bool ok = true;
            ok = ok && vg_parse_next_int(&p, &key);
            ok = ok && vg_expect_comma(&p);
            ok = ok && vg_parse_next_int(&p, &pan);
            ok = ok && vg_expect_comma(&p);
            ok = ok && vg_parse_next_int(&p, &duty);
            ok = ok && vg_expect_comma(&p);
            ok = ok && vg_parse_next_int(&p, &attack);
            ok = ok && vg_expect_comma(&p);
            ok = ok && vg_parse_next_int(&p, &decay);
            ok = ok && vg_expect_comma(&p);
            ok = ok && vg_parse_next_int(&p, &sustain);
            ok = ok && vg_expect_comma(&p);
            ok = ok && vg_parse_next_int(&p, &release);
            if (ok)
            {
                ToneData* td = &destVoices[voiceIndex];
                td->type = VOICE_SQUARE_2;
                td->key = (uint8_t)key;
                td->panSweep = 0;
                td->wavePointer = (uint32_t*)(uintptr_t)(duty & 0x03);
                td->attack = (uint8_t)(attack & 0x07);
                td->decay = (uint8_t)(decay & 0x07);
                td->sustain = (uint8_t)(sustain & 0x0F);
                td->release = (uint8_t)(release & 0x07);
            }
            voiceIndex++;
            voicesParsedInSection++;
        }
        else if (strncmp(trimmed, "voice_programmable_wave_alt ", 27) == 0)
        {
            const char* p = trimmed + 27;
            int key = 0;
            int pan = 0;
            int attack = 0;
            int decay = 0;
            int sustain = 0;
            int release = 0;
            char waveSymbol[MAX_SYMBOL_LEN];
            if (!vg_parse_next_int(&p, &key) || !vg_expect_comma(&p) || !vg_parse_next_int(&p, &pan) || !vg_expect_comma(&p))
            {
                voiceIndex++;
                voicesParsedInSection++;
                continue;
            }
            int rc = vg_extract_comma_symbol(&p, waveSymbol, sizeof(waveSymbol));
            if (rc == -1)
            {
                hardFail = true;
                break;
            }
            if (rc != 0 || !vg_parse_next_int(&p, &attack) || !vg_expect_comma(&p) || !vg_parse_next_int(&p, &decay) || !vg_expect_comma(&p) || !vg_parse_next_int(&p, &sustain) || !vg_expect_comma(&p) || !vg_parse_next_int(&p, &release))
            {
                voiceIndex++;
                voicesParsedInSection++;
                continue;
            }
            if (destNames && voiceIndex >= 0 && voiceIndex < VOICEGROUP_SIZE)
            {
                set_voice_display_name(destNames[voiceIndex], waveSymbol);
            }
            ToneData* td = &destVoices[voiceIndex];
            td->type = VOICE_PROGRAMMABLE_WAVE_ALT;
            td->key = (uint8_t)key;
            td->attack = (uint8_t)(attack & 0x07);
            td->decay = (uint8_t)(decay & 0x07);
            td->sustain = (uint8_t)(sustain & 0x0F);
            td->release = (uint8_t)(release & 0x07);
            const char* wavePath = symbol_map_find(pwMap, waveSymbol);
            if (wavePath)
            {
                char absPath[VG_MAX_PATH_LEN];
                if (!build_path(absPath, sizeof(absPath), projectRoot, wavePath))
                {
                    hardFail = true;
                    break;
                }
                if (!vg_load_session_add_prog(session, &td->wavePointer, absPath))
                {
                    hardFail = true;
                    break;
                }
            }
            voiceIndex++;
            voicesParsedInSection++;
        }
        else if (strncmp(trimmed, "voice_programmable_wave ", 23) == 0)
        {
            const char* p = trimmed + 23;
            int key = 0;
            int pan = 0;
            int attack = 0;
            int decay = 0;
            int sustain = 0;
            int release = 0;
            char waveSymbol[MAX_SYMBOL_LEN];
            if (!vg_parse_next_int(&p, &key) || !vg_expect_comma(&p) || !vg_parse_next_int(&p, &pan) || !vg_expect_comma(&p))
            {
                voiceIndex++;
                voicesParsedInSection++;
                continue;
            }
            int rc = vg_extract_comma_symbol(&p, waveSymbol, sizeof(waveSymbol));
            if (rc == -1)
            {
                hardFail = true;
                break;
            }
            if (rc != 0 || !vg_parse_next_int(&p, &attack) || !vg_expect_comma(&p) || !vg_parse_next_int(&p, &decay) || !vg_expect_comma(&p) || !vg_parse_next_int(&p, &sustain) || !vg_expect_comma(&p) || !vg_parse_next_int(&p, &release))
            {
                voiceIndex++;
                voicesParsedInSection++;
                continue;
            }
            if (destNames && voiceIndex >= 0 && voiceIndex < VOICEGROUP_SIZE)
            {
                set_voice_display_name(destNames[voiceIndex], waveSymbol);
            }
            ToneData* td = &destVoices[voiceIndex];
            td->type = VOICE_PROGRAMMABLE_WAVE;
            td->key = (uint8_t)key;
            td->attack = (uint8_t)(attack & 0x07);
            td->decay = (uint8_t)(decay & 0x07);
            td->sustain = (uint8_t)(sustain & 0x0F);
            td->release = (uint8_t)(release & 0x07);
            const char* wavePath = symbol_map_find(pwMap, waveSymbol);
            if (wavePath)
            {
                char absPath[VG_MAX_PATH_LEN];
                if (!build_path(absPath, sizeof(absPath), projectRoot, wavePath))
                {
                    hardFail = true;
                    break;
                }
                if (!vg_load_session_add_prog(session, &td->wavePointer, absPath))
                {
                    hardFail = true;
                    break;
                }
            }
            voiceIndex++;
            voicesParsedInSection++;
        }
        else if (strncmp(trimmed, "voice_noise_alt ", 16) == 0)
        {
            const char* p = trimmed + 16;
            int key = 0;
            int pan = 0;
            int period = 0;
            int attack = 0;
            int decay = 0;
            int sustain = 0;
            int release = 0;
            bool ok = true;
            ok = ok && vg_parse_next_int(&p, &key);
            ok = ok && vg_expect_comma(&p);
            ok = ok && vg_parse_next_int(&p, &pan);
            ok = ok && vg_expect_comma(&p);
            ok = ok && vg_parse_next_int(&p, &period);
            ok = ok && vg_expect_comma(&p);
            ok = ok && vg_parse_next_int(&p, &attack);
            ok = ok && vg_expect_comma(&p);
            ok = ok && vg_parse_next_int(&p, &decay);
            ok = ok && vg_expect_comma(&p);
            ok = ok && vg_parse_next_int(&p, &sustain);
            ok = ok && vg_expect_comma(&p);
            ok = ok && vg_parse_next_int(&p, &release);
            if (ok)
            {
                ToneData* td = &destVoices[voiceIndex];
                td->type = VOICE_NOISE_ALT;
                td->key = (uint8_t)key;
                td->wavePointer = (uint32_t*)(uintptr_t)(period & 0x01);
                td->attack = (uint8_t)(attack & 0x07);
                td->decay = (uint8_t)(decay & 0x07);
                td->sustain = (uint8_t)(sustain & 0x0F);
                td->release = (uint8_t)(release & 0x07);
            }
            voiceIndex++;
            voicesParsedInSection++;
        }
        else if (strncmp(trimmed, "voice_noise ", 12) == 0)
        {
            const char* p = trimmed + 12;
            int key = 0;
            int pan = 0;
            int period = 0;
            int attack = 0;
            int decay = 0;
            int sustain = 0;
            int release = 0;
            bool ok = true;
            ok = ok && vg_parse_next_int(&p, &key);
            ok = ok && vg_expect_comma(&p);
            ok = ok && vg_parse_next_int(&p, &pan);
            ok = ok && vg_expect_comma(&p);
            ok = ok && vg_parse_next_int(&p, &period);
            ok = ok && vg_expect_comma(&p);
            ok = ok && vg_parse_next_int(&p, &attack);
            ok = ok && vg_expect_comma(&p);
            ok = ok && vg_parse_next_int(&p, &decay);
            ok = ok && vg_expect_comma(&p);
            ok = ok && vg_parse_next_int(&p, &sustain);
            ok = ok && vg_expect_comma(&p);
            ok = ok && vg_parse_next_int(&p, &release);
            if (ok)
            {
                ToneData* td = &destVoices[voiceIndex];
                td->type = VOICE_NOISE;
                td->key = (uint8_t)key;
                td->wavePointer = (uint32_t*)(uintptr_t)(period & 0x01);
                td->attack = (uint8_t)(attack & 0x07);
                td->decay = (uint8_t)(decay & 0x07);
                td->sustain = (uint8_t)(sustain & 0x0F);
                td->release = (uint8_t)(release & 0x07);
            }
            voiceIndex++;
            voicesParsedInSection++;
        }
        else if (strncmp(trimmed, "voice_keysplit_all ", 19) == 0)
        {
            const char* p = trimmed + 19;
            char vgSymbol[MAX_SYMBOL_LEN];
            int rc = vg_extract_eol_symbol(&p, vgSymbol, sizeof(vgSymbol));
            if (rc == -1)
            {
                hardFail = true;
                break;
            }
            if (rc != 0)
            {
                voiceIndex++;
                voicesParsedInSection++;
                continue;
            }
            if (destNames && voiceIndex >= 0 && voiceIndex < VOICEGROUP_SIZE)
            {
                set_voice_display_name(destNames[voiceIndex], vgSymbol);
            }
            ToneData* td = &destVoices[voiceIndex];
            td->type = VOICE_KEYSPLIT_ALL;
            if (!noSubRecurse && !inContinuation)
            {
                ToneData* subVg = NULL;
                int subRes = load_sub_voicegroup_session(projectRoot, vgSymbol, vgReg, session, dsMap, pwMap, ksMap, disc, waveCache, &subVg);
                if (subRes == -1)
                {
                    hardFail = true;
                    break;
                }
                td->subGroup = subVg;
            }
            voiceIndex++;
            voicesParsedInSection++;
        }
        else if (strncmp(trimmed, "voice_keysplit ", 15) == 0)
        {
            const char* p = trimmed + 15;
            char vgSymbol[MAX_SYMBOL_LEN];
            char ksSymbol[MAX_SYMBOL_LEN];
            int rc1 = vg_extract_comma_symbol(&p, vgSymbol, sizeof(vgSymbol));
            if (rc1 == -1)
            {
                hardFail = true;
                break;
            }
            if (rc1 != 0)
            {
                voiceIndex++;
                voicesParsedInSection++;
                continue;
            }
            int rc2 = vg_extract_eol_symbol(&p, ksSymbol, sizeof(ksSymbol));
            if (rc2 == -1)
            {
                hardFail = true;
                break;
            }
            if (rc2 != 0)
            {
                voiceIndex++;
                voicesParsedInSection++;
                continue;
            }
            if (destNames && voiceIndex >= 0 && voiceIndex < VOICEGROUP_SIZE)
            {
                set_voice_display_name(destNames[voiceIndex], vgSymbol);
            }
            ToneData* td = &destVoices[voiceIndex];
            td->type = VOICE_KEYSPLIT;
            if (!noSubRecurse && !inContinuation)
            {
                ToneData* subVg = NULL;
                int subRes = load_sub_voicegroup_session(projectRoot, vgSymbol, vgReg, session, dsMap, pwMap, ksMap, disc, waveCache, &subVg);
                if (subRes == -1)
                {
                    hardFail = true;
                    break;
                }
                td->subGroup = subVg;
            }
            KeySplitDef* ksDef = NULL;
            bool ksOk = keysplit_map_find_or_rescan_checked(
                ksMap, ksSymbol, disc, &ksDef);
            if (!ksOk)
            {
                hardFail = true;
                break;
            }
            if (ksDef)
            {
                uint8_t* table = (uint8_t*)malloc(128);
                if (!table)
                {
                    hardFail = true;
                    break;
                }
                memcpy(table, ksDef->table, 128);
                if (!vg_register_keysplittable(vgReg, table))
                {
                    free(table);
                    hardFail = true;
                    break;
                }
                td->keySplitTable = table;
            }
            voiceIndex++;
            voicesParsedInSection++;
        }
        else if (strncmp(trimmed, "cry_reverse ", 12) == 0)
        {
            const char* p = trimmed + 12;
            char sampleSymbol[MAX_SYMBOL_LEN];
            int rc = vg_extract_eol_symbol(&p, sampleSymbol, sizeof(sampleSymbol));
            if (rc == -1)
            {
                hardFail = true;
                break;
            }
            if (rc != 0)
            {
                voiceIndex++;
                voicesParsedInSection++;
                continue;
            }
            if (destNames && voiceIndex >= 0 && voiceIndex < VOICEGROUP_SIZE)
            {
                set_voice_display_name(destNames[voiceIndex], sampleSymbol);
            }
            ToneData* td = &destVoices[voiceIndex];
            td->type = VOICE_CRY_REVERSE;
            td->key = 60;
            td->attack = 0xFF;
            td->decay = 0;
            td->sustain = 0xFF;
            td->release = 0;
            const char* samplePath = symbol_map_find(dsMap, sampleSymbol);
            if (samplePath)
            {
                char binAbs[VG_MAX_PATH_LEN];
                if (!build_path(binAbs, sizeof(binAbs), projectRoot, samplePath))
                {
                    hardFail = true;
                    break;
                }
                if (!vg_load_session_add_wave(session, &td->wav, NULL, NULL, binAbs))
                {
                    hardFail = true;
                    break;
                }
            }
            voiceIndex++;
            voicesParsedInSection++;
        }
        else if (strncmp(trimmed, "cry ", 4) == 0)
        {
            const char* p = trimmed + 4;
            char sampleSymbol[MAX_SYMBOL_LEN];
            int rc = vg_extract_eol_symbol(&p, sampleSymbol, sizeof(sampleSymbol));
            if (rc == -1)
            {
                hardFail = true;
                break;
            }
            if (rc != 0)
            {
                voiceIndex++;
                voicesParsedInSection++;
                continue;
            }
            if (destNames && voiceIndex >= 0 && voiceIndex < VOICEGROUP_SIZE)
            {
                set_voice_display_name(destNames[voiceIndex], sampleSymbol);
            }
            ToneData* td = &destVoices[voiceIndex];
            td->type = VOICE_CRY;
            td->key = 60;
            td->attack = 0xFF;
            td->decay = 0;
            td->sustain = 0xFF;
            td->release = 0;
            const char* samplePath = symbol_map_find(dsMap, sampleSymbol);
            if (samplePath)
            {
                char binAbs[VG_MAX_PATH_LEN];
                if (!build_path(binAbs, sizeof(binAbs), projectRoot, samplePath))
                {
                    hardFail = true;
                    break;
                }
                if (!vg_load_session_add_wave(session, &td->wav, NULL, NULL, binAbs))
                {
                    hardFail = true;
                    break;
                }
            }
            voiceIndex++;
            voicesParsedInSection++;
        }
    }
    vg_log("parse_voicegroup_file_session: done, voiceIndex=%d hardFail=%d labelFound=%d", voiceIndex, hardFail, labelFound);
    fclose(f);
    if (hardFail)
    {
        return -1;
    }
    if (startLabel && !labelFound)
    {
        return -1;
    }
    return voiceIndex;
}

/* ---- Project context: open/free, shared load helpers, one-shot delegates ---- */

/*
 * Config copies are validated before storage: counts must name entries that
 * actually exist in the fixed arrays, and every used path must be
 * NUL-terminated.
 */
static int vg_config_is_valid(const VoicegroupLoaderConfig* config)
{
    if (!config)
        return 1;
    if (config->soundDataPathCount < 0 || config->soundDataPathCount > VG_CONFIG_PATH_CAP)
        return 0;
    if (config->voicegroupPathCount < 0 || config->voicegroupPathCount > VG_CONFIG_PATH_CAP)
        return 0;
    if (config->sampleDirCount < 0 || config->sampleDirCount > VG_CONFIG_PATH_CAP)
        return 0;
    for (int i = 0; i < config->soundDataPathCount; i++)
        if (!memchr(config->soundDataPaths[i], '\0', VG_MAX_PATH_LEN))
            return 0;
    for (int i = 0; i < config->voicegroupPathCount; i++)
        if (!memchr(config->voicegroupPaths[i], '\0', VG_MAX_PATH_LEN))
            return 0;
    for (int i = 0; i < config->sampleDirCount; i++)
        if (!memchr(config->sampleDirs[i], '\0', VG_MAX_PATH_LEN))
            return 0;
    return 1;
}

/* ---- Internal serial stdio adapter ---- */

/*
 * One-shot entry points back their temporary context with this adapter: each
 * batch reads whole files one at a time via stdio, matching the pre-context
 * implementation. An unopenable path is a soft miss (found=0, blob stays
 * zeroed); allocation or stream errors are hard failures. Directory
 * enumeration, discovery, and voicegroup text reads stay internal and do not
 * go through this adapter; only mapped sample/programmable-wave asset batches
 * use it.
 */
static bool vg_stdio_read_batch(
    void* user, const char* const* paths, size_t count, VoicegroupFileBlob* out, char* error, size_t errorCapacity)
{
    (void)user;
    if ((!paths && count != 0) || (!out && count != 0))
    {
        if (error && errorCapacity)
            snprintf(error, errorCapacity, "vg_stdio: invalid batch");
        return false;
    }
    if (count > (size_t)INT_MAX)
    {
        if (error && errorCapacity)
            snprintf(error, errorCapacity, "vg_stdio: batch too large");
        return false;
    }
    for (size_t i = 0; i < count; i++)
        out[i] = (VoicegroupFileBlob){0};
    if (error && errorCapacity)
        error[0] = '\0';

    for (size_t i = 0; i < count; i++)
    {
        const char* path = paths[i];
        if (!path)
        {
            if (error && errorCapacity)
                snprintf(error, errorCapacity, "vg_stdio: null path at %zu", i);
            return false;
        }
        FILE* f = fopen(path, "rb");
        if (!f)
            continue; /* soft miss: out[i] stays zeroed */
        if (fseek(f, 0, SEEK_END) != 0)
        {
            fclose(f);
            if (error && errorCapacity)
                snprintf(error, errorCapacity, "seek failed for %s", path);
            return false;
        }
        long fileSize = ftell(f);
        if (fileSize < 0)
        {
            fclose(f);
            if (error && errorCapacity)
                snprintf(error, errorCapacity, "size probe failed for %s", path);
            return false;
        }
        if ((uint64_t)fileSize > (uint64_t)SIZE_MAX - 1)
        {
            fclose(f);
            if (error && errorCapacity)
                snprintf(error, errorCapacity, "file too large: %s", path);
            return false;
        }
        if ((uint64_t)fileSize > (uint64_t)PTRDIFF_MAX)
        {
            fclose(f);
            if (error && errorCapacity)
                snprintf(error, errorCapacity, "file too large: %s", path);
            return false;
        }
        rewind(f);
        size_t usize = (size_t)fileSize;
        uint8_t* data = (uint8_t*)malloc(usize + 1);
        if (!data)
        {
            fclose(f);
            if (error && errorCapacity)
                snprintf(error, errorCapacity, "out of memory reading %s", path);
            return false;
        }
        size_t got = usize ? fread(data, 1, usize, f) : 0;
        if (got != usize)
        {
            free(data);
            fclose(f);
            if (error && errorCapacity)
                snprintf(error, errorCapacity, "short read on %s", path);
            return false;
        }
        if (ferror(f))
        {
            free(data);
            fclose(f);
            if (error && errorCapacity)
                snprintf(error, errorCapacity, "read error on %s", path);
            return false;
        }
        fclose(f);
        data[usize] = 0; /* NUL tail so text consumers can walk the blob */
        out[i].data = data;
        out[i].size = usize;
        out[i].found = 1;
    }
    return true;
}

static void vg_stdio_release_batch(void* user, VoicegroupFileBlob* blobs, size_t count)
{
    (void)user;
    if (!blobs)
        return;
    for (size_t i = 0; i < count; i++)
    {
        free(blobs[i].data);
        blobs[i] = (VoicegroupFileBlob){0};
    }
}

/* The serial stdio fileIo handed to one-shot temporary contexts. */
static const VoicegroupFileIo* vg_stdio_file_io(void)
{
    static const VoicegroupFileIo io = {
        NULL,
        vg_stdio_read_batch,
        vg_stdio_release_batch,
    };
    return &io;
}

void voicegroup_project_free(VoicegroupProject* project)
{
    if (!project)
        return;
    symbol_map_free(&project->dsMap);
    symbol_map_free(&project->pwMap);
    keysplit_map_free(&project->ksMap);
    free(project->disc);
    free(project);
}

VoicegroupProject*
voicegroup_project_open(const char* projectRoot, const VoicegroupLoaderConfig* config, const VoicegroupFileIo* fileIo)
{
    if (!projectRoot || projectRoot[0] == '\0')
    {
        vg_log("voicegroup_project_open: missing projectRoot");
        return NULL;
    }
    if (strlen(projectRoot) >= VG_MAX_PATH_LEN)
    {
        vg_log("voicegroup_project_open: projectRoot too long (%d max)", VG_MAX_PATH_LEN - 1);
        return NULL;
    }
    if (!fileIo || !fileIo->readBatch || !fileIo->releaseBatch)
    {
        vg_log("voicegroup_project_open: incomplete fileIo adapter");
        return NULL;
    }
    if (!vg_config_is_valid(config))
    {
        vg_log("voicegroup_project_open: invalid config counts or paths");
        return NULL;
    }

    VoicegroupProject* project = calloc(1, sizeof(VoicegroupProject));
    if (!project)
        return NULL;

    /* All three inputs are copied: the context never borrows caller memory. */
    snprintf(project->projectRoot, sizeof(project->projectRoot), "%s", projectRoot);
    if (config)
        project->config = *config;
    project->fileIo = *fileIo;

    /* Heap-allocate ProjectDiscovery: ~96 KB on the stack would risk overflow
     * in Reaper's plugin-load thread (Windows default: 1 MB stack). */
    project->disc = calloc(1, sizeof(ProjectDiscovery));
    if (!project->disc)
    {
        voicegroup_project_free(project);
        return NULL;
    }

    /* Discover project structure */
    vg_log("voicegroup_project_open: calling discover_project");
    discover_project(project->projectRoot, &project->config, project->disc);
    vg_log(
        "voicegroup_project_open: discover done - dsFiles=%d pwFiles=%d ksFiles=%d vgDirs=%d monoFiles=%d wavDirs=%d",
        project->disc->directSoundDataFiles.count,
        project->disc->progWaveDataFiles.count,
        project->disc->keySplitTableFiles.count,
        project->disc->voicegroupDirs.count,
        project->disc->monolithicVGFiles.count,
        project->disc->wavSampleDirs.count);

    /* Parse the global symbol maps once; every load from this context reuses
     * them (plus the discovery) instead of re-parsing per call. */
    vg_log("voicegroup_project_open: parsing symbol maps");
    symbol_map_init(&project->dsMap);
    symbol_map_init(&project->pwMap);
    keysplit_map_init(&project->ksMap);
    if (!parse_all_direct_sound_data(project->disc, project->projectRoot, &project->dsMap))
    {
        vg_log("voicegroup_project_open: dsMap parse failed");
        voicegroup_project_free(project);
        return NULL;
    }
    vg_log("voicegroup_project_open: dsMap entries=%d", project->dsMap.count);
    if (!parse_all_programmable_wave_data(project->disc, project->projectRoot, &project->pwMap))
    {
        vg_log("voicegroup_project_open: pwMap parse failed");
        voicegroup_project_free(project);
        return NULL;
    }
    vg_log("voicegroup_project_open: pwMap entries=%d", project->pwMap.count);
    if (!parse_all_keysplit_tables(project->disc, &project->ksMap))
    {
        vg_log("voicegroup_project_open: ksMap parse failed");
        voicegroup_project_free(project);
        return NULL;
    }
    vg_log("voicegroup_project_open: ksMap entries=%d", project->ksMap.count);

    return project;
}

/*
 * Shared bank loader: parse one concrete file (and optional section label)
 * into a fresh LoadedVoiceGroup using the context's discovery and maps.
 * Extracted from the original one-shot voicegroup_load body; both the exact
 * target path and the name-resolved one-shot path land here.
 */

static LoadedVoiceGroup* project_load_location(VoicegroupProject* project, const char* filePath, const char* startLabel)
{
    LoadedVoiceGroup* vg = (LoadedVoiceGroup*)calloc(1, sizeof(LoadedVoiceGroup));
    if (!vg)
        return NULL;
    WaveCache waveCache;
    wave_cache_init(&waveCache);
    VgLoadSession session;
    vg_load_session_init(&session, &project->fileIo, vg, &waveCache);
    if (!vg_load_session_push_location(&session, filePath, startLabel))
    {
        vg_load_session_deinit(&session);
        voicegroup_free(vg);
        return NULL;
    }
    int r = parse_voicegroup_file_session(project->projectRoot, filePath, startLabel, vg->voices, vg->voiceNames, vg, &session, &project->dsMap, &project->pwMap, &project->ksMap, project->disc, &waveCache, 0, 0, 0);
    vg_load_session_pop_location(&session);
    if (r < 0)
    {
        vg_load_session_deinit(&session);
        voicegroup_free(vg);
        return NULL;
    }
    if (!vg_load_session_execute(&session))
    {
        vg_load_session_deinit(&session);
        voicegroup_free(vg);
        return NULL;
    }
    vg_load_session_deinit(&session);
    return vg;
}

LoadedVoiceGroup* voicegroup_project_load(VoicegroupProject* project, const VoicegroupTarget* target)
{
    if (!project || !target || !target->filePath || target->filePath[0] == '\0')
    {
        vg_log("voicegroup_project_load: missing project or target");
        return NULL;
    }

    const char* sectionLabel = (target->sectionLabel && target->sectionLabel[0]) ? target->sectionLabel : NULL;

    if (sectionLabel && !vg_section_label_valid(sectionLabel))
    {
        vg_log("voicegroup_project_load: section label too long or invalid");
        return NULL;
    }

    vg_log("voicegroup_project_load: file='%s' section='%s'", target->filePath, sectionLabel ? sectionLabel : "(none)");

    /* Exact target: the path is used verbatim, with no alias probing, and is
     * attempted exactly once. A missing file is a hard miss (NULL), not a
     * soft empty bank. */
    if (!file_exists(target->filePath))
    {
        vg_log("voicegroup_project_load: target file not found");
        return NULL;
    }

    return project_load_location(project, target->filePath, sectionLabel);
}


/*
 * Shared sample-set loader over the context's discovery and maps. Extracted
 * from the original one-shot voicegroup_load_samples body; the maps and
 * discovery arrive pre-built from the context instead of being rebuilt here.
 */

static LoadedSampleSet* project_load_samples(VoicegroupProject* project,
                                             const char* const* sampleSymbols,
                                             int sampleCount,
                                             const char* const* waveSymbols,
                                             int waveCount,
                                             const char* const* keysplitSymbols,
                                             const char* const* keysplitTableSymbols,
                                             int keysplitCount)
{
    LoadedSampleSet* set = (LoadedSampleSet*)calloc(1, sizeof(LoadedSampleSet));
    if (!set) return NULL;
    set->container = (LoadedVoiceGroup*)calloc(1, sizeof(LoadedVoiceGroup));
    set->waves = (WaveData**)calloc(sampleCount>0?sampleCount:1, sizeof(WaveData*));
    set->progWaves = (uint32_t**)calloc(waveCount>0?waveCount:1, sizeof(uint32_t*));
    set->keysplits = (LoadedKeysplit*)calloc(keysplitCount>0?keysplitCount:1, sizeof(LoadedKeysplit));
    if (!set->container || !set->waves || !set->progWaves || !set->keysplits)
    {
        voicegroup_free_samples(set);
        return NULL;
    }
    set->count = sampleCount;
    set->progWaveCount = waveCount;
    set->keysplitCount = keysplitCount;
    WaveCache waveCache;
    wave_cache_init(&waveCache);
    VgLoadSession session;
    vg_load_session_init(&session, &project->fileIo, set->container, &waveCache);
    bool hardFail = false;
    for (int i = 0; i < sampleCount && !hardFail; i++)
    {
        const char* sym = sampleSymbols[i];
        const uint8_t* synthDesc = symbol_map_find_synth(&project->dsMap, sym);
        if (synthDesc)
        {
            WaveData* wd = build_synth_wavedata(synthDesc, sym, set->container, &waveCache);
            if (!wd)
            {
                hardFail = true;
                break;
            }
            set->waves[i] = wd;
            continue;
        }
        const char* samplePath = symbol_map_find(&project->dsMap, sym);
        if (samplePath)
        {
            char wavAbs[VG_MAX_PATH_LEN];
            char aifAbs[VG_MAX_PATH_LEN];
            char binAbs[VG_MAX_PATH_LEN];
            if (!build_wave_abs_paths(project->projectRoot, samplePath, wavAbs, aifAbs, binAbs))
            {
                hardFail = true;
                break;
            }
            const char* w = wavAbs[0] ? wavAbs : NULL;
            const char* a = aifAbs[0] ? aifAbs : NULL;
            if (!vg_load_session_add_wave(&session, &set->waves[i], w, a, binAbs))
            {
                hardFail = true;
                break;
            }
        }
        else
        {
            WaveData* wd = NULL;
            int res = resolve_and_load_sample_serial(project->projectRoot, sym, &project->dsMap, project->disc, set->container, &waveCache, &wd);
            if (res == -1)
            {
                hardFail = true;
                break;
            }
            if (res == 1)
            {
                set->waves[i] = wd;
            }
        }
    }
    for (int i = 0; i < waveCount && !hardFail; i++)
    {
        const char* wavePath = symbol_map_find(&project->pwMap, waveSymbols[i]);
        if (!wavePath)
        {
            continue;
        }
        char absPath[VG_MAX_PATH_LEN];
        if (!build_path(absPath, sizeof(absPath), project->projectRoot, wavePath))
        {
            hardFail = true;
            break;
        }
        if (!vg_load_session_add_prog(&session, &set->progWaves[i], absPath))
        {
            hardFail = true;
            break;
        }
    }
    for (int i = 0; i < keysplitCount && !hardFail; i++)
    {
        ToneData* subVg = NULL;
        int subRes = load_sub_voicegroup_session(project->projectRoot, keysplitSymbols[i], set->container, &session, &project->dsMap, &project->pwMap, &project->ksMap, project->disc, &waveCache, &subVg);
        if (subRes == -1)
        {
            hardFail = true;
            break;
        }
        set->keysplits[i].subGroup = subVg;
        const KeySplitDef* ksDef = NULL;
        bool ksOk = keysplit_map_find_or_rescan_checked(
            &project->ksMap, keysplitTableSymbols[i], project->disc, (KeySplitDef**)&ksDef);
        if (!ksOk)
        {
            hardFail = true;
            break;
        }
        if (ksDef)
        {
            uint8_t* table = (uint8_t*)malloc(128);
            if (!table)
            {
                hardFail = true;
                break;
            }
            memcpy(table, ksDef->table, 128);
            if (!vg_register_keysplittable(set->container, table))
            {
                free(table);
                hardFail = true;
                break;
            }
            set->keysplits[i].table = table;
        }
    }
    if (hardFail)
    {
        vg_load_session_deinit(&session);
        voicegroup_free_samples(set);
        return NULL;
    }
    if (!vg_load_session_execute(&session))
    {
        vg_load_session_deinit(&session);
        voicegroup_free_samples(set);
        return NULL;
    }
    vg_load_session_deinit(&session);
    return set;
}

LoadedSampleSet* voicegroup_project_load_samples(VoicegroupProject* project,
                                                 const char* const* sampleSymbols,
                                                 int sampleCount,
                                                 const char* const* waveSymbols,
                                                 int waveCount,
                                                 const char* const* keysplitSymbols,
                                                 const char* const* keysplitTableSymbols,
                                                 int keysplitCount)
{
    if (!project)
        return NULL;
    if (sampleCount < 0 || waveCount < 0 || keysplitCount < 0)
    {
        vg_log("voicegroup_project_load_samples: negative count");
        return NULL;
    }
    if ((sampleCount > 0 && !sampleSymbols) || (waveCount > 0 && !waveSymbols) ||
        (keysplitCount > 0 && (!keysplitSymbols || !keysplitTableSymbols)))
    {
        vg_log("voicegroup_project_load_samples: missing symbol array");
        return NULL;
    }
    return project_load_samples(project,
                                sampleSymbols,
                                sampleCount,
                                waveSymbols,
                                waveCount,
                                keysplitSymbols,
                                keysplitTableSymbols,
                                keysplitCount);
}

/*
 * Main entry point: load a voicegroup from a project.
 *
 * Thin delegate: build a temporary context over the internal serial stdio
 * adapter, resolve the voicegroup name through it, then run the same shared
 * loader the context API uses. Nothing outlives the call except the returned
 * (self-contained) bank.
 */
LoadedVoiceGroup*
voicegroup_load(const char* projectRoot, const char* voicegroupName, const VoicegroupLoaderConfig* config)
{
    vg_log("voicegroup_load: start root='%s' vg='%s'", projectRoot, voicegroupName);
    if (!voicegroupName || voicegroupName[0] == '\0')
    {
        vg_log("voicegroup_load: missing voicegroupName");
        return NULL;
    }

    VoicegroupProject* project = voicegroup_project_open(projectRoot, config, vg_stdio_file_io());
    if (!project)
        return NULL;

    /* Find the voicegroup */
    vg_log("voicegroup_load: searching for voicegroup '%s'", voicegroupName);
    VoicegroupLocation loc = find_voicegroup(project->projectRoot, voicegroupName, project->disc);
    if (!loc.found)
    {
        vg_log("voicegroup_load: voicegroup '%s' not found", voicegroupName);
        fprintf(stderr, "voicegroup_loader: cannot find voicegroup '%s'\n", voicegroupName);
        voicegroup_project_free(project);
        return NULL;
    }
    vg_log("voicegroup_load: found at '%s' label='%s'", loc.filePath, loc.label);

    LoadedVoiceGroup* vg = project_load_location(project, loc.filePath, loc.label[0] ? loc.label : NULL);
    voicegroup_project_free(project);
    if (vg)
        vg_log("voicegroup_load: done OK");
    return vg;
}

/*
 * Thin delegate: temporary context + the shared sample-set loader. Results
 * are self-contained and outlive the context teardown.
 */
LoadedSampleSet* voicegroup_load_samples(const char* projectRoot,
                                         const char* const* sampleSymbols,
                                         int sampleCount,
                                         const char* const* waveSymbols,
                                         int waveCount,
                                         const char* const* keysplitSymbols,
                                         const char* const* keysplitTableSymbols,
                                         int keysplitCount,
                                         const VoicegroupLoaderConfig* config)
{
    VoicegroupProject* project = voicegroup_project_open(projectRoot, config, vg_stdio_file_io());
    if (!project)
        return NULL;

    LoadedSampleSet* set = voicegroup_project_load_samples(project,
                                                           sampleSymbols,
                                                           sampleCount,
                                                           waveSymbols,
                                                           waveCount,
                                                           keysplitSymbols,
                                                           keysplitTableSymbols,
                                                           keysplitCount);
    voicegroup_project_free(project);
    return set;
}

void voicegroup_free_samples(LoadedSampleSet* set)
{
    if (!set)
        return;
    voicegroup_free(set->container);
    free(set->waves);
    free(set->progWaves);
    free(set->keysplits);
    free(set);
}

void voicegroup_free(LoadedVoiceGroup* vg)
{
    if (!vg)
        return;

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
