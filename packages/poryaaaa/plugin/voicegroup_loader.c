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
#    define MAX_SYMBOL_LEN 256
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
static int
next_included_voicegroup(const char* projectRoot, const char* currentFilePath, char* outPath, size_t outSize);
static int load_sub_voicegroup_session(const char* projectRoot,
                                       const char* vgSymbol,
                                       LoadedVoiceGroup* vgReg,
                                       VgLoadSession* session,
                                       const SymbolMap* dsMap,
                                       const SymbolMap* pwMap,
                                       KeySplitMap* ksMap,
                                       ProjectDiscovery* disc,
                                       WaveCache* waveCache,
                                       ToneData** outSub);
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
                                         int noSubRecurse);

static WaveData*
build_synth_wavedata(const uint8_t desc[6], const char* symbol, LoadedVoiceGroup* owner, WaveCache* cache)
{
    char cacheKey[MAX_PATH_LEN];
    snprintf(cacheKey, sizeof(cacheKey), "synth-macro:%s", symbol);
    WaveData* cached = wave_cache_find(cache, cacheKey);
    if (cached)
    {
        return cached;
    }
    WaveData* wd = (WaveData*)calloc(1, sizeof(WaveData) + 17);
    if (!wd)
        return NULL;
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
    static const char* prefixes[] = {"DirectSoundWaveData_", "ProgrammableWaveData_", "voicegroup_"};
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
        if (isspace((unsigned char)c) || c == ':' || c == ',')
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

static bool build_wave_abs_paths(const char* projectRoot,
                                 const char* samplePath,
                                 char wavAbs[VG_MAX_PATH_LEN],
                                 char aifAbs[VG_MAX_PATH_LEN],
                                 char binAbs[VG_MAX_PATH_LEN])
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
    if (!vg || !wd)
        return false;
    if (vg->waveDataCount >= vg->waveDataCapacity)
    {
        size_t nc = vg->waveDataCapacity ? (size_t)vg->waveDataCapacity * 2 : INITIAL_CAPACITY;
        WaveData** np = (WaveData**)realloc(vg->waveDatas, nc * sizeof(WaveData*));
        if (!np)
            return false;
        vg->waveDatas = np;
        vg->waveDataCapacity = (int)nc;
    }
    vg->waveDatas[vg->waveDataCount++] = wd;
    return true;
}

static bool vg_register_subgroup(LoadedVoiceGroup* vg, ToneData* sg)
{
    if (!vg || !sg)
        return false;
    if (vg->subGroupCount >= vg->subGroupCapacity)
    {
        size_t nc = vg->subGroupCapacity ? (size_t)vg->subGroupCapacity * 2 : INITIAL_CAPACITY;
        ToneData** np = (ToneData**)realloc(vg->subGroups, nc * sizeof(ToneData*));
        if (!np)
            return false;
        vg->subGroups = np;
        vg->subGroupCapacity = (int)nc;
    }
    vg->subGroups[vg->subGroupCount++] = sg;
    return true;
}

static bool vg_register_keysplittable(LoadedVoiceGroup* vg, uint8_t* ks)
{
    if (!vg || !ks)
        return false;
    if (vg->keySplitTableCount >= vg->keySplitTableCapacity)
    {
        size_t nc = vg->keySplitTableCapacity ? (size_t)vg->keySplitTableCapacity * 2 : INITIAL_CAPACITY;
        uint8_t** np = (uint8_t**)realloc(vg->keySplitTables, nc * sizeof(uint8_t*));
        if (!np)
            return false;
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
        SymbolMapping* np = (SymbolMapping*)realloc(map->entries, sizeof(SymbolMapping) * newCap);
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
static void discover_scan_tree(const char* dirPath, int depth, int maxDepth, ProjectDiscovery* out);

static bool dirfacts_add_subdir(DirFacts* facts, const char* name)
{
    if (facts->subdirCount >= facts->subdirCapacity)
    {
        size_t newCap = facts->subdirCapacity ? (size_t)facts->subdirCapacity * 2 : INITIAL_CAPACITY;
        char (*paths)[MAX_DIRENT_NAME] =
            (char (*)[MAX_DIRENT_NAME])realloc(facts->subdirs, sizeof(*facts->subdirs) * newCap);
        if (!paths)
            return false;
        facts->subdirs = paths;
        facts->subdirCapacity = (int)newCap;
    }
    snprintf(facts->subdirs[facts->subdirCount], sizeof(facts->subdirs[0]), "%s", name);
    facts->subdirCount++;
    return true;
}

static bool collect_dir_fact(DirFacts* facts, const char* dirPath, const struct dirent* ent)
{
    if (ent->d_name[0] == '.')
        return true;
    if (dirent_is_dir(dirPath, ent))
    {
        if (strcmp(ent->d_name, "keysplits") == 0)
            facts->hasKeysplitsSubdir = 1;
        return dirfacts_add_subdir(facts, ent->d_name);
    }
    if (str_ends_with_ci(ent->d_name, ".wav"))
        facts->hasWavOrAif = 1;
    else if (str_ends_with_ci(ent->d_name, ".aif"))
        facts->hasWavOrAif = 1;
    if (strcmp(ent->d_name, "keysplit_tables.inc") == 0)
        facts->hasKeysplitTablesInc = 1;
    else if (strcmp(ent->d_name, "keysplit_tables.s") == 0)
        facts->hasKeysplitTablesS = 1;
    if (facts->macroCandidateCount >= 5)
        return true;
    if (!str_ends_with_ci(ent->d_name, ".inc"))
    {
        if (!str_ends_with_ci(ent->d_name, ".s"))
            return true;
    }
    snprintf(facts->macroCandidates[facts->macroCandidateCount], sizeof(facts->macroCandidates[0]), "%s", ent->d_name);
    facts->macroCandidateCount++;
    return true;
}

static bool collect_dir_facts(const char* dirPath, DirFacts* facts)
{
    DIR* dir = opendir(dirPath);
    if (!dir)
        return true;
    struct dirent* ent;
    while ((ent = readdir(dir)) != NULL)
    {
        if (collect_dir_fact(facts, dirPath, ent))
            continue;
        free(facts->subdirs);
        facts->subdirs = NULL;
        facts->subdirCapacity = 0;
        closedir(dir);
        return false;
    }
    closedir(dir);
    return true;
}

static void discover_macro_directory(const char* dirPath, const DirFacts* facts, ProjectDiscovery* out)
{
    char path[MAX_PATH_LEN];
    for (int i = 0; i < facts->macroCandidateCount; i++)
    {
        build_path(path, sizeof(path), dirPath, facts->macroCandidates[i]);
        if (!file_has_voice_macros(path))
            continue;
        pathlist_add(&out->voicegroupDirs, dirPath);
        return;
    }
}

static void discover_keysplit_subdir(const char* dirPath, ProjectDiscovery* out)
{
    char keysplitDir[MAX_PATH_LEN];
    build_path(keysplitDir, sizeof(keysplitDir), dirPath, "keysplits");
    DIR* dir = opendir(keysplitDir);
    if (!dir)
        return;
    char path[MAX_PATH_LEN];
    struct dirent* ent;
    while ((ent = readdir(dir)) != NULL)
    {
        if (ent->d_name[0] == '.')
            continue;
        if (!str_ends_with_ci(ent->d_name, ".s"))
        {
            if (!str_ends_with_ci(ent->d_name, ".inc"))
                continue;
        }
        build_path(path, sizeof(path), keysplitDir, ent->d_name);
        pathlist_add(&out->keySplitTableFiles, path);
    }
    closedir(dir);
}

static void discover_dir_facts(const char* dirPath, const DirFacts* facts, ProjectDiscovery* out)
{
    discover_macro_directory(dirPath, facts, out);
    if (facts->hasWavOrAif)
        pathlist_add(&out->wavSampleDirs, dirPath);
    char path[MAX_PATH_LEN];
    if (facts->hasKeysplitTablesInc)
    {
        build_path(path, sizeof(path), dirPath, "keysplit_tables.inc");
        pathlist_add(&out->keySplitTableFiles, path);
    }
    if (facts->hasKeysplitTablesS)
    {
        build_path(path, sizeof(path), dirPath, "keysplit_tables.s");
        pathlist_add(&out->keySplitTableFiles, path);
    }
    if (facts->hasKeysplitsSubdir)
        discover_keysplit_subdir(dirPath, out);
}

static void
discover_subdirectories(const char* dirPath, int depth, int maxDepth, const DirFacts* facts, ProjectDiscovery* out)
{
    if (depth >= maxDepth)
        return;
    for (int i = 0; i < facts->subdirCount; i++)
    {
        char subPath[MAX_PATH_LEN];
        build_path(subPath, sizeof(subPath), dirPath, facts->subdirs[i]);
        discover_scan_tree(subPath, depth + 1, maxDepth, out);
    }
}

static void discover_scan_tree(const char* dirPath, int depth, int maxDepth, ProjectDiscovery* out)
{
    DirFacts facts;
    memset(&facts, 0, sizeof(facts));
    if (!collect_dir_facts(dirPath, &facts))
        return;
    discover_dir_facts(dirPath, &facts, out);
    discover_subdirectories(dirPath, depth, maxDepth, &facts, out);
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

static int config_path_count(int count)
{
    if (count < 0)
        return 0;
    if (count > VG_CONFIG_PATH_CAP)
        return VG_CONFIG_PATH_CAP;
    return count;
}

static void discover_config_monolithic_files(const char* dirPath, ProjectDiscovery* out)
{
    DIR* dir = opendir(dirPath);
    if (!dir)
        return;
    struct dirent* ent;
    while ((ent = readdir(dir)) != NULL)
    {
        if (ent->d_name[0] == '.')
            continue;
        if (!str_ends_with_ci(ent->d_name, ".inc"))
        {
            if (!str_ends_with_ci(ent->d_name, ".s"))
                continue;
        }
        char filePath[MAX_PATH_LEN];
        build_path(filePath, sizeof(filePath), dirPath, ent->d_name);
        if (is_monolithic_voicegroup_file(filePath))
            pathlist_add(&out->monolithicVGFiles, filePath);
    }
    closedir(dir);
}

static void discover_config_voicegroup_path(const char* projectRoot, const char* relativePath, ProjectDiscovery* out)
{
    char path[MAX_PATH_LEN];
    build_path(path, sizeof(path), projectRoot, relativePath);
    if (is_directory(path))
    {
        pathlist_add(&out->voicegroupDirs, path);
        discover_config_monolithic_files(path, out);
        probe_keysplit_data_in_dir(path, out);
        return;
    }
    if (file_exists(path))
    {
        if (is_monolithic_voicegroup_file(path))
            pathlist_add(&out->monolithicVGFiles, path);
    }
}

static void discover_config_paths(const char* projectRoot, const VoicegroupLoaderConfig* cfg, ProjectDiscovery* out)
{
    if (!cfg)
        return;
    char path[MAX_PATH_LEN];
    int count = config_path_count(cfg->soundDataPathCount);
    for (int i = 0; i < count; i++)
    {
        build_path(path, sizeof(path), projectRoot, cfg->soundDataPaths[i]);
        if (file_exists(path))
            pathlist_add(&out->directSoundDataFiles, path);
    }
    count = config_path_count(cfg->voicegroupPathCount);
    for (int i = 0; i < count; i++)
        discover_config_voicegroup_path(projectRoot, cfg->voicegroupPaths[i], out);
    count = config_path_count(cfg->sampleDirCount);
    for (int i = 0; i < count; i++)
    {
        build_path(path, sizeof(path), projectRoot, cfg->sampleDirs[i]);
        if (is_directory(path))
            pathlist_add(&out->wavSampleDirs, path);
    }
}

static void discover_standard_data_file(const char* projectRoot, const char* relativePath, PathList* files)
{
    char path[MAX_PATH_LEN];
    build_path(path, sizeof(path), projectRoot, relativePath);
    if (file_exists(path))
        pathlist_add(files, path);
}

static void discover_standard_data_files(const char* projectRoot, ProjectDiscovery* out)
{
    discover_standard_data_file(projectRoot, "sound/direct_sound_data.inc", &out->directSoundDataFiles);
    discover_standard_data_file(projectRoot, "sound/direct_sound_synth_data.inc", &out->directSoundDataFiles);
    discover_standard_data_file(projectRoot, "sound/programmable_wave_data.inc", &out->progWaveDataFiles);
    discover_standard_data_file(projectRoot, "sound/keysplit_tables.inc", &out->keySplitTableFiles);
}

static void discover_standard_voicegroup_dirs(const char* projectRoot, ProjectDiscovery* out)
{
    char voicegroupDir[MAX_PATH_LEN];
    build_path(voicegroupDir, sizeof(voicegroupDir), projectRoot, "sound/voicegroups");
    if (!is_directory(voicegroupDir))
        return;
    pathlist_add(&out->voicegroupDirs, voicegroupDir);
    char subPath[MAX_PATH_LEN];
    build_path(subPath, sizeof(subPath), voicegroupDir, "keysplits");
    if (is_directory(subPath))
        pathlist_add(&out->voicegroupDirs, subPath);
    build_path(subPath, sizeof(subPath), voicegroupDir, "drumsets");
    if (is_directory(subPath))
        pathlist_add(&out->voicegroupDirs, subPath);
}

static void discover_standard_monolithic_file(const char* projectRoot, ProjectDiscovery* out)
{
    char path[MAX_PATH_LEN];
    build_path(path, sizeof(path), projectRoot, "sound/voice_groups.inc");
    vg_log("discover_project: checking monolithic '%s' exists=%d", path, file_exists(path));
    if (!file_exists(path))
        return;
    if (is_monolithic_voicegroup_file(path))
        pathlist_add(&out->monolithicVGFiles, path);
}

static void discover_project(const char* projectRoot, const VoicegroupLoaderConfig* cfg, ProjectDiscovery* out)
{
    memset(out, 0, sizeof(ProjectDiscovery));
    snprintf(out->projectRoot, sizeof(out->projectRoot), "%s", projectRoot);
    out->cfg = cfg;
    char soundDir[MAX_PATH_LEN];
    build_path(soundDir, sizeof(soundDir), projectRoot, "sound");
    vg_log("discover_project: soundDir='%s' exists=%d", soundDir, is_directory(soundDir));
    discover_config_paths(projectRoot, cfg, out);
    discover_standard_data_files(projectRoot, out);
    discover_standard_voicegroup_dirs(projectRoot, out);
    discover_standard_monolithic_file(projectRoot, out);
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
typedef struct
{
    KeySplitMap* map;
    KeySplitDef* current;
    int lastNote;
} KeySplitParser;

typedef enum
{
    KEYSPLIT_LINE_UNHANDLED,
    KEYSPLIT_LINE_HANDLED,
    KEYSPLIT_LINE_INVALID,
} KeySplitLineResult;

static const char* vg_skip_horizontal_space(const char* p)
{
    while (*p == ' ' || *p == '\t')
        p++;
    return p;
}

static bool vg_line_finished(const char* p)
{
    return *vg_skip_horizontal_space(p) == '\0';
}

static KeySplitDef* keysplit_map_next(KeySplitMap* map)
{
    if (map->count >= map->capacity)
    {
        size_t newCap = map->capacity ? (size_t)map->capacity * 2 : INITIAL_CAPACITY;
        KeySplitDef* entries = (KeySplitDef*)realloc(map->entries, sizeof(KeySplitDef) * newCap);
        if (!entries)
            return NULL;
        map->entries = entries;
        map->capacity = (int)newCap;
    }
    return &map->entries[map->count];
}

static bool keysplit_begin_macro(KeySplitParser* parser, char* name, int startNote)
{
    if (strlen(name) >= MAX_SYMBOL_LEN - 9)
        return false;
    if (startNote < 0)
        return false;
    if (startNote > 127)
        return false;
    rtrim(name);
    KeySplitDef* current = keysplit_map_next(parser->map);
    if (!current)
        return false;
    memset(current, 0, sizeof(*current));
    int written = snprintf(current->name, sizeof(current->name), "keysplit_%s", name);
    if (written < 0)
        return false;
    if (written >= MAX_SYMBOL_LEN)
        return false;
    current->startingNote = startNote;
    parser->current = current;
    parser->lastNote = startNote;
    parser->map->count++;
    return true;
}

static bool keysplit_begin_set(KeySplitParser* parser, char* name, int startNote)
{
    if (strlen(name) >= MAX_SYMBOL_LEN)
        return false;
    if (startNote < 0)
        return false;
    if (startNote > 127)
        return false;
    rtrim(name);
    KeySplitDef* current = keysplit_map_next(parser->map);
    if (!current)
        return false;
    memset(current, 0, sizeof(*current));
    strncpy(current->name, name, MAX_SYMBOL_LEN - 1);
    current->name[MAX_SYMBOL_LEN - 1] = '\0';
    current->startingNote = startNote;
    parser->current = current;
    parser->lastNote = startNote;
    parser->map->count++;
    return true;
}

static KeySplitLineResult parse_keysplit_macro_line(KeySplitParser* parser, const char* trimmed)
{
    if (strncmp(trimmed, "keysplit ", 9) != 0)
        return KEYSPLIT_LINE_UNHANDLED;
    const char* p = trimmed + 9;
    char name[MAX_SYMBOL_LEN];
    if (vg_extract_comma_symbol(&p, name, sizeof(name)) != 0)
        return KEYSPLIT_LINE_INVALID;
    int startNote = 0;
    if (!vg_parse_next_int(&p, &startNote))
        return KEYSPLIT_LINE_INVALID;
    if (!vg_line_finished(p))
        return KEYSPLIT_LINE_INVALID;
    if (!keysplit_begin_macro(parser, name, startNote))
        return KEYSPLIT_LINE_INVALID;
    return KEYSPLIT_LINE_HANDLED;
}

static bool keysplit_split_values_are_valid(int index, int endNote, int lastNote)
{
    if (index < 0)
        return false;
    if (index > 127)
        return false;
    if (endNote < 0)
        return false;
    if (endNote > 128)
        return false;
    if (lastNote < 0)
        return false;
    if (lastNote > 128)
        return false;
    return endNote >= lastNote;
}

static KeySplitLineResult parse_keysplit_split_line(KeySplitParser* parser, const char* trimmed)
{
    if (strncmp(trimmed, "split ", 6) != 0)
        return KEYSPLIT_LINE_UNHANDLED;
    if (!parser->current)
        return KEYSPLIT_LINE_HANDLED;
    const char* p = trimmed + 6;
    int index = 0;
    int endNote = 0;
    if (!vg_parse_next_int(&p, &index))
        return KEYSPLIT_LINE_INVALID;
    if (!vg_expect_comma(&p))
        return KEYSPLIT_LINE_INVALID;
    if (!vg_parse_next_int(&p, &endNote))
        return KEYSPLIT_LINE_INVALID;
    if (!vg_line_finished(p))
        return KEYSPLIT_LINE_INVALID;
    if (!keysplit_split_values_are_valid(index, endNote, parser->lastNote))
        return KEYSPLIT_LINE_INVALID;
    for (int note = parser->lastNote; note < endNote; note++)
        parser->current->table[note] = (uint8_t)index;
    parser->lastNote = endNote;
    if (endNote > parser->current->maxNote)
        parser->current->maxNote = endNote;
    return KEYSPLIT_LINE_HANDLED;
}

static KeySplitLineResult parse_keysplit_set_line(KeySplitParser* parser, const char* trimmed)
{
    if (strncmp(trimmed, ".set ", 5) != 0)
        return KEYSPLIT_LINE_UNHANDLED;
    const char* p = trimmed + 5;
    char name[MAX_SYMBOL_LEN];
    if (vg_extract_comma_symbol(&p, name, sizeof(name)) != 0)
        return KEYSPLIT_LINE_INVALID;
    p = vg_skip_horizontal_space(p);
    if (*p != '.')
        return KEYSPLIT_LINE_INVALID;
    p = vg_skip_horizontal_space(p + 1);
    if (*p != '-')
        return KEYSPLIT_LINE_INVALID;
    p++;
    int startNote = 0;
    if (!vg_parse_next_int(&p, &startNote))
        return KEYSPLIT_LINE_INVALID;
    if (!vg_line_finished(p))
        return KEYSPLIT_LINE_INVALID;
    if (!keysplit_begin_set(parser, name, startNote))
        return KEYSPLIT_LINE_INVALID;
    return KEYSPLIT_LINE_HANDLED;
}

static KeySplitLineResult parse_keysplit_bytes_line(KeySplitParser* parser, const char* trimmed)
{
    if (strncmp(trimmed, ".byte ", 6) != 0)
        return KEYSPLIT_LINE_UNHANDLED;
    if (!parser->current)
        return KEYSPLIT_LINE_HANDLED;
    const char* p = trimmed + 6;
    while (*p)
    {
        while (*p == ' ' || *p == '\t' || *p == ',')
            p++;
        if (*p == '\0')
            break;
        int value = 0;
        if (!vg_parse_next_int(&p, &value))
            return KEYSPLIT_LINE_INVALID;
        if (value < 0)
            return KEYSPLIT_LINE_INVALID;
        if (value > 127)
            return KEYSPLIT_LINE_INVALID;
        if (parser->lastNote < 0)
            return KEYSPLIT_LINE_INVALID;
        if (parser->lastNote >= 128)
            return KEYSPLIT_LINE_INVALID;
        parser->current->table[parser->lastNote] = (uint8_t)value;
        if (parser->lastNote > parser->current->maxNote)
            parser->current->maxNote = parser->lastNote;
        parser->lastNote++;
    }
    return KEYSPLIT_LINE_HANDLED;
}

static KeySplitLineResult parse_keysplit_line(KeySplitParser* parser, const char* trimmed)
{
    KeySplitLineResult result = parse_keysplit_macro_line(parser, trimmed);
    if (result != KEYSPLIT_LINE_UNHANDLED)
        return result;
    result = parse_keysplit_split_line(parser, trimmed);
    if (result != KEYSPLIT_LINE_UNHANDLED)
        return result;
    result = parse_keysplit_set_line(parser, trimmed);
    if (result != KEYSPLIT_LINE_UNHANDLED)
        return result;
    return parse_keysplit_bytes_line(parser, trimmed);
}

static int parse_keysplit_tables_file(const char* filePath, KeySplitMap* map)
{
    FILE* file = fopen(filePath, "r");
    if (!file)
    {
        fprintf(stderr, "voicegroup_loader: cannot open %s\n", filePath);
        return -1;
    }
    KeySplitParser parser = {map, NULL, 0};
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), file))
    {
        strip_comment(line);
        rtrim(line);
        KeySplitLineResult result = parse_keysplit_line(&parser, ltrim(line));
        if (result != KEYSPLIT_LINE_INVALID)
            continue;
        fclose(file);
        return -1;
    }
    fclose(file);
    return 0;
}

/* Wrappers that iterate over all discovered paths */

static bool parse_all_direct_sound_data(const ProjectDiscovery* disc, const char* projectRoot, SymbolMap* map)
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

static bool parse_all_programmable_wave_data(const ProjectDiscovery* disc, const char* projectRoot, SymbolMap* map)
{
    for (int i = 0; i < disc->progWaveDataFiles.count; i++)
    {
        if (parse_programmable_wave_data_file(disc->progWaveDataFiles.paths[i], projectRoot, map) != 0)
        {
            return false;
        }
    }
    return true;
}

static bool parse_keysplit_tables_range(const ProjectDiscovery* disc, KeySplitMap* map, int fromIndex)
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

static bool
keysplit_map_find_or_rescan_checked(KeySplitMap* map, const char* name, ProjectDiscovery* disc, KeySplitDef** out)
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

static int resolve_synth_sample(
    const SymbolMap* dsMap, const char* symbol, LoadedVoiceGroup* vg, WaveCache* waveCache, WaveData** outWd)
{
    const uint8_t* synthDesc = symbol_map_find_synth(dsMap, symbol);
    if (!synthDesc)
        return 0;
    WaveData* wave = build_synth_wavedata(synthDesc, symbol, vg, waveCache);
    if (!wave)
        return -1;
    if (outWd)
        *outWd = wave;
    return 1;
}

static int
load_sample_candidate(const char* path, bool isAif, LoadedVoiceGroup* vg, WaveCache* waveCache, WaveData** outWd)
{
    WaveData* cached = wave_cache_find(waveCache, path);
    if (cached)
    {
        if (outWd)
            *outWd = cached;
        return 1;
    }
    bool hardFailure = false;
    WaveData* wave = NULL;
    if (isAif)
        wave = load_aif_from_path(path, &hardFailure);
    else
        wave = load_wav_from_path(path, &hardFailure);
    if (hardFailure)
        return -1;
    if (!wave)
        return 0;
    if (!vg_register_wavedata(vg, wave))
    {
        free(wave);
        return -1;
    }
    wave_cache_insert(waveCache, path, wave);
    if (outWd)
        *outWd = wave;
    return 1;
}

static int search_sample_directories(
    const char* symbol, ProjectDiscovery* disc, LoadedVoiceGroup* vg, WaveCache* waveCache, WaveData** outWd)
{
    if (!disc)
        return 0;
    if (!disc->deepScanned)
        discovery_ensure_deep_scan(disc);
    for (int i = 0; i < disc->wavSampleDirs.count; i++)
    {
        char path[MAX_PATH_LEN];
        snprintf(path, sizeof(path), "%s%c%s.wav", disc->wavSampleDirs.paths[i], PATH_SEP, symbol);
        int result = load_sample_candidate(path, false, vg, waveCache, outWd);
        if (result != 0)
            return result;
        snprintf(path, sizeof(path), "%s%c%s.aif", disc->wavSampleDirs.paths[i], PATH_SEP, symbol);
        result = load_sample_candidate(path, true, vg, waveCache, outWd);
        if (result != 0)
            return result;
    }
    return 0;
}

static int resolve_and_load_sample_serial(const char* projectRoot,
                                          const char* symbol,
                                          const SymbolMap* dsMap,
                                          ProjectDiscovery* disc,
                                          LoadedVoiceGroup* vg,
                                          WaveCache* waveCache,
                                          WaveData** outWd)
{
    (void)projectRoot;
    if (outWd)
        *outWd = NULL;
    int result = resolve_synth_sample(dsMap, symbol, vg, waveCache, outWd);
    if (result != 0)
        return result;
    return search_sample_directories(symbol, disc, vg, waveCache, outWd);
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
static void set_voicegroup_file_location(VoicegroupLocation* location, const char* filePath)
{
    memset(location, 0, sizeof(*location));
    strncpy(location->filePath, filePath, MAX_PATH_LEN - 1);
    location->found = 1;
}

static bool find_voicegroup_file(
    const char* dirPath, const char* subdir, const char* prefix, const char* name, VoicegroupLocation* location)
{
    char path[MAX_PATH_LEN];
    if (subdir)
        snprintf(path, sizeof(path), "%s%c%s%c%s%s.inc", dirPath, PATH_SEP, subdir, PATH_SEP, prefix, name);
    else
        snprintf(path, sizeof(path), "%s%c%s%s.inc", dirPath, PATH_SEP, prefix, name);
    if (file_exists(path))
    {
        set_voicegroup_file_location(location, path);
        return true;
    }
    if (subdir)
        snprintf(path, sizeof(path), "%s%c%s%c%s%s.s", dirPath, PATH_SEP, subdir, PATH_SEP, prefix, name);
    else
        snprintf(path, sizeof(path), "%s%c%s%s.s", dirPath, PATH_SEP, prefix, name);
    if (!file_exists(path))
        return false;
    set_voicegroup_file_location(location, path);
    return true;
}

static bool find_voicegroup_in_directories(const PathList* directories,
                                           const char* prefix,
                                           const char* name,
                                           VoicegroupLocation* location)
{
    for (int i = 0; i < directories->count; i++)
    {
        if (find_voicegroup_file(directories->paths[i], NULL, prefix, name, location))
            return true;
    }
    return false;
}

static bool find_sub_voicegroup_in_directories(const PathList* directories,
                                               const char* subdir,
                                               const char* name,
                                               VoicegroupLocation* location)
{
    for (int i = 0; i < directories->count; i++)
    {
        if (find_voicegroup_file(directories->paths[i], subdir, "", name, location))
            return true;
    }
    for (int i = 0; i < directories->count; i++)
    {
        if (!dir_last_component_is(directories->paths[i], subdir))
            continue;
        if (find_voicegroup_file(directories->paths[i], NULL, "", name, location))
            return true;
    }
    return false;
}

static bool make_keysplit_voicegroup_name(const char* vgName, char baseName[MAX_SYMBOL_LEN])
{
    const char* suffix = strstr(vgName, "_keysplit");
    if (!suffix)
        return false;
    int baseLength = (int)(suffix - vgName);
    if (baseLength <= 0)
        return false;
    if (baseLength >= MAX_SYMBOL_LEN)
        return false;
    memcpy(baseName, vgName, (size_t)baseLength);
    baseName[baseLength] = '\0';
    return true;
}

static bool make_drumset_voicegroup_name(const char* vgName, char baseName[MAX_SYMBOL_LEN])
{
    const char* suffix = strstr(vgName, "_drumset");
    if (!suffix)
        return false;
    int baseLength = (int)(suffix - vgName);
    const char* tail = suffix + 8;
    if (baseLength <= 0)
        return false;
    if (baseLength + (int)strlen(tail) >= MAX_SYMBOL_LEN)
        return false;
    memcpy(baseName, vgName, (size_t)baseLength);
    strcpy(baseName + baseLength, tail);
    return true;
}

static bool find_keysplit_voicegroup(const ProjectDiscovery* disc, const char* vgName, VoicegroupLocation* location)
{
    char baseName[MAX_SYMBOL_LEN];
    if (!make_keysplit_voicegroup_name(vgName, baseName))
        return false;
    return find_sub_voicegroup_in_directories(&disc->voicegroupDirs, "keysplits", baseName, location);
}

static bool find_drumset_voicegroup(const ProjectDiscovery* disc, const char* vgName, VoicegroupLocation* location)
{
    char baseName[MAX_SYMBOL_LEN];
    if (!make_drumset_voicegroup_name(vgName, baseName))
        return false;
    return find_sub_voicegroup_in_directories(&disc->voicegroupDirs, "drumsets", baseName, location);
}

static bool monolithic_voicegroup_has_label(FILE* file, const char* label, size_t labelLength)
{
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), file))
    {
        strip_comment(line);
        char* trimmed = ltrim(line);
        if (strncmp(trimmed, label, labelLength) != 0)
            continue;
        char trailing = trimmed[labelLength];
        if (trailing == '\0')
            return true;
        if (isspace((unsigned char)trailing))
            return true;
    }
    return false;
}

static bool find_monolithic_voicegroup(const PathList* files, const char* vgName, VoicegroupLocation* location)
{
    if (strlen(vgName) >= MAX_SYMBOL_LEN)
        return false;
    char label[MAX_SYMBOL_LEN + 4];
    int labelLength = snprintf(label, sizeof(label), "%s::", vgName);
    if (labelLength < 0)
        return false;
    if ((size_t)labelLength >= sizeof(label))
        return false;
    for (int i = 0; i < files->count; i++)
    {
        FILE* file = fopen(files->paths[i], "r");
        if (!file)
            continue;
        bool found = monolithic_voicegroup_has_label(file, label, (size_t)labelLength);
        fclose(file);
        if (!found)
            continue;
        set_voicegroup_file_location(location, files->paths[i]);
        strncpy(location->label, vgName, MAX_SYMBOL_LEN - 1);
        return true;
    }
    return false;
}

/*
 * Search for a voicegroup by name across all currently discovered locations.
 */
static VoicegroupLocation
find_voicegroup_probe(const char* projectRoot, const char* vgName, const ProjectDiscovery* disc)
{
    (void)projectRoot;
    VoicegroupLocation location;
    memset(&location, 0, sizeof(location));
    if (find_voicegroup_in_directories(&disc->voicegroupDirs, "", vgName, &location))
        return location;
    if (find_keysplit_voicegroup(disc, vgName, &location))
        return location;
    if (find_drumset_voicegroup(disc, vgName, &location))
        return location;
    if (find_voicegroup_in_directories(&disc->voicegroupDirs, "vg_", vgName, &location))
        return location;
    find_monolithic_voicegroup(&disc->monolithicVGFiles, vgName, &location);
    return location;
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

typedef struct
{
    const char* projectRoot;
    LoadedVoiceGroup* vgReg;
    VgLoadSession* session;
    const SymbolMap* dsMap;
    const SymbolMap* pwMap;
    KeySplitMap* ksMap;
    ProjectDiscovery* disc;
    WaveCache* waveCache;
} SubVoicegroupContext;

static int continue_sub_voicegroup(const SubVoicegroupContext* context,
                                   ToneData* voices,
                                   char (*names)[VG_VOICE_NAME_LEN],
                                   int endIndex,
                                   const char* initialPath)
{
    char currentPath[MAX_PATH_LEN];
    strncpy(currentPath, initialPath, sizeof(currentPath) - 1);
    currentPath[sizeof(currentPath) - 1] = '\0';
    for (int hops = 0; hops < VOICEGROUP_SIZE; hops++)
    {
        if (endIndex >= VOICEGROUP_SIZE)
            break;
        char nextPath[MAX_PATH_LEN];
        if (!next_included_voicegroup(context->projectRoot, currentPath, nextPath, sizeof(nextPath)))
            break;
        int nextIndex = parse_voicegroup_file_session(context->projectRoot,
                                                      nextPath,
                                                      NULL,
                                                      voices,
                                                      names,
                                                      context->vgReg,
                                                      context->session,
                                                      context->dsMap,
                                                      context->pwMap,
                                                      context->ksMap,
                                                      context->disc,
                                                      context->waveCache,
                                                      endIndex,
                                                      0,
                                                      1);
        if (nextIndex < 0)
            return -1;
        if (nextIndex <= endIndex)
            break;
        endIndex = nextIndex;
        strncpy(currentPath, nextPath, sizeof(currentPath) - 1);
        currentPath[sizeof(currentPath) - 1] = '\0';
    }
    return endIndex;
}

static int
parse_sub_voicegroup(const SubVoicegroupContext* context, const VoicegroupLocation* location, ToneData* voices)
{
    const char* startLabel = location->label[0] ? location->label : NULL;
    char names[VOICEGROUP_SIZE][VG_VOICE_NAME_LEN];
    memset(names, 0, sizeof(names));
    int endIndex = parse_voicegroup_file_session(context->projectRoot,
                                                 location->filePath,
                                                 startLabel,
                                                 voices,
                                                 names,
                                                 context->vgReg,
                                                 context->session,
                                                 context->dsMap,
                                                 context->pwMap,
                                                 context->ksMap,
                                                 context->disc,
                                                 context->waveCache,
                                                 0,
                                                 1,
                                                 0);
    if (endIndex <= 0)
        return endIndex;
    if (startLabel)
        return endIndex;
    return continue_sub_voicegroup(context, voices, names, endIndex, location->filePath);
}

static int load_sub_voicegroup_location(const SubVoicegroupContext* context,
                                        const char* symbol,
                                        const VoicegroupLocation* location,
                                        ToneData** outSub)
{
    const char* startLabel = location->label[0] ? location->label : NULL;
    if (vg_load_session_is_active(context->session, location->filePath, startLabel))
    {
        fprintf(stderr,
                "voicegroup_loader: cycle detected for sub-voicegroup '%s' at %s:%s\n",
                symbol,
                location->filePath,
                startLabel ? startLabel : "(none)");
        return -1;
    }
    ToneData* subgroup = (ToneData*)calloc(VOICEGROUP_SIZE, sizeof(ToneData));
    if (!subgroup)
        return -1;
    if (!vg_register_subgroup(context->vgReg, subgroup))
    {
        free(subgroup);
        return -1;
    }
    if (!vg_load_session_push_location(context->session, location->filePath, startLabel))
        return -1;
    VgLoadSessionCheckpoint checkpoint = vg_load_session_checkpoint(context->session);
    int endIndex = parse_sub_voicegroup(context, location, subgroup);
    if (endIndex < 0)
    {
        vg_load_session_rollback(context->session, checkpoint);
        vg_load_session_pop_location(context->session);
        return -1;
    }
    vg_load_session_pop_location(context->session);
    if (outSub)
        *outSub = subgroup;
    return 1;
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
    VoicegroupLocation location = find_voicegroup(projectRoot, name, disc);
    if (!location.found)
    {
        fprintf(stderr, "voicegroup_loader: cannot find sub-voicegroup '%s'\n", vgSymbol);
        return 0;
    }
    SubVoicegroupContext context = {
        projectRoot,
        vgReg,
        session,
        dsMap,
        pwMap,
        ksMap,
        disc,
        waveCache,
    };
    return load_sub_voicegroup_location(&context, vgSymbol, &location, outSub);
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

typedef struct
{
    const char* projectRoot;
    ToneData* voices;
    char (*names)[VG_VOICE_NAME_LEN];
    LoadedVoiceGroup* registry;
    VgLoadSession* session;
    const SymbolMap* directSoundMap;
    const SymbolMap* programmableWaveMap;
    KeySplitMap* keysplitMap;
    ProjectDiscovery* discovery;
    WaveCache* waveCache;
    int noSubRecurse;
} VoiceParseContext;

typedef struct
{
    int voiceIndex;
    int voicesParsedInSection;
    int inContinuation;
} VoiceParseProgress;

typedef struct
{
    const char* startLabel;
    char searchLabel[MAX_SYMBOL_LEN + 4];
    size_t searchLength;
    int inSection;
    int labelFound;
    int contiguousFill;
} VoicegroupSection;

typedef enum
{
    VG_LINE_UNHANDLED,
    VG_LINE_METADATA,
    VG_LINE_CONSUMED,
    VG_LINE_HARD_FAIL,
    VG_LINE_STOP,
} VoiceLineResult;

typedef enum
{
    VG_ARGUMENTS_SOFT_MISS,
    VG_ARGUMENTS_VALID,
    VG_ARGUMENTS_HARD_FAIL,
} VoiceArgumentsResult;

typedef struct
{
    int key;
    int pan;
    int attack;
    int decay;
    int sustain;
    int release;
    char symbol[MAX_SYMBOL_LEN];
} SampleVoiceArguments;

static void set_parsed_voice_name(const VoiceParseContext* context, int voiceIndex, const char* symbol)
{
    if (!context->names)
        return;
    if (voiceIndex < 0)
        return;
    if (voiceIndex >= VOICEGROUP_SIZE)
        return;
    set_voice_display_name(context->names[voiceIndex], symbol);
}

static bool vg_parse_int_list(const char** p, int* values, int count)
{
    for (int i = 0; i < count; i++)
    {
        if (!vg_parse_next_int(p, &values[i]))
            return false;
        if (i + 1 == count)
            continue;
        if (!vg_expect_comma(p))
            return false;
    }
    return true;
}

static VoiceArgumentsResult parse_sample_voice_arguments(const char* p, SampleVoiceArguments* arguments)
{
    if (!vg_parse_next_int(&p, &arguments->key))
        return VG_ARGUMENTS_SOFT_MISS;
    if (!vg_expect_comma(&p))
        return VG_ARGUMENTS_SOFT_MISS;
    if (!vg_parse_next_int(&p, &arguments->pan))
        return VG_ARGUMENTS_SOFT_MISS;
    if (!vg_expect_comma(&p))
        return VG_ARGUMENTS_SOFT_MISS;
    int symbolResult = vg_extract_comma_symbol(&p, arguments->symbol, sizeof(arguments->symbol));
    if (symbolResult == -1)
        return VG_ARGUMENTS_HARD_FAIL;
    if (symbolResult != 0)
        return VG_ARGUMENTS_SOFT_MISS;
    int envelope[4];
    if (!vg_parse_int_list(&p, envelope, 4))
        return VG_ARGUMENTS_SOFT_MISS;
    arguments->attack = envelope[0];
    arguments->decay = envelope[1];
    arguments->sustain = envelope[2];
    arguments->release = envelope[3];
    return VG_ARGUMENTS_VALID;
}

static bool resolve_directsound_wave(const VoiceParseContext* context, ToneData* tone, const char* symbol)
{
    const uint8_t* synthDesc = symbol_map_find_synth(context->directSoundMap, symbol);
    if (synthDesc)
    {
        tone->wav = build_synth_wavedata(synthDesc, symbol, context->registry, context->waveCache);
        return tone->wav != NULL;
    }
    const char* samplePath = symbol_map_find(context->directSoundMap, symbol);
    if (samplePath)
    {
        char wavPath[VG_MAX_PATH_LEN];
        char aifPath[VG_MAX_PATH_LEN];
        char binPath[VG_MAX_PATH_LEN];
        if (!build_wave_abs_paths(context->projectRoot, samplePath, wavPath, aifPath, binPath))
            return false;
        const char* wav = NULL;
        const char* aif = NULL;
        if (wavPath[0])
            wav = wavPath;
        if (aifPath[0])
            aif = aifPath;
        return vg_load_session_add_wave(context->session, &tone->wav, wav, aif, binPath);
    }
    WaveData* wave = NULL;
    int result = resolve_and_load_sample_serial(context->projectRoot,
                                                symbol,
                                                context->directSoundMap,
                                                context->discovery,
                                                context->registry,
                                                context->waveCache,
                                                &wave);
    if (result == -1)
        return false;
    if (result == 1)
        tone->wav = wave;
    return true;
}

static VoiceLineResult parse_directsound_voice(const VoiceParseContext* context, const char* trimmed, int voiceIndex)
{
    const char* p = NULL;
    uint8_t type = VOICE_DIRECTSOUND;
    if (strncmp(trimmed, "voice_directsound_no_resample ", 30) == 0)
    {
        p = trimmed + 30;
        type = VOICE_DIRECTSOUND_NO_RESAMPLE;
    }
    else if (strncmp(trimmed, "voice_directsound_alt ", 22) == 0)
    {
        p = trimmed + 22;
        type = VOICE_DIRECTSOUND_ALT;
    }
    else if (strncmp(trimmed, "voice_directsound ", 18) == 0)
    {
        p = trimmed + 18;
    }
    else
    {
        return VG_LINE_UNHANDLED;
    }
    SampleVoiceArguments arguments;
    VoiceArgumentsResult argumentsResult = parse_sample_voice_arguments(p, &arguments);
    if (argumentsResult == VG_ARGUMENTS_HARD_FAIL)
        return VG_LINE_HARD_FAIL;
    if (argumentsResult != VG_ARGUMENTS_VALID)
        return VG_LINE_CONSUMED;
    set_parsed_voice_name(context, voiceIndex, arguments.symbol);
    ToneData* tone = &context->voices[voiceIndex];
    tone->type = type;
    tone->key = (uint8_t)arguments.key;
    tone->panSweep = arguments.pan ? (0x80 | arguments.pan) : 0;
    tone->attack = (uint8_t)arguments.attack;
    tone->decay = (uint8_t)arguments.decay;
    tone->sustain = (uint8_t)arguments.sustain;
    tone->release = (uint8_t)arguments.release;
    if (!resolve_directsound_wave(context, tone, arguments.symbol))
        return VG_LINE_HARD_FAIL;
    return VG_LINE_CONSUMED;
}

static VoiceLineResult
populate_square_voice(const VoiceParseContext* context, const char* p, int voiceIndex, uint8_t type, bool isSquareOne)
{
    int values[8];
    int valueCount = 7;
    if (isSquareOne)
        valueCount = 8;
    if (!vg_parse_int_list(&p, values, valueCount))
        return VG_LINE_CONSUMED;
    ToneData* tone = &context->voices[voiceIndex];
    tone->type = type;
    tone->key = (uint8_t)values[0];
    if (isSquareOne)
    {
        tone->panSweep = (uint8_t)values[2];
        tone->wavePointer = (uint32_t*)(uintptr_t)(values[3] & 0x03);
        tone->attack = (uint8_t)(values[4] & 0x07);
        tone->decay = (uint8_t)(values[5] & 0x07);
        tone->sustain = (uint8_t)(values[6] & 0x0F);
        tone->release = (uint8_t)(values[7] & 0x07);
        return VG_LINE_CONSUMED;
    }
    tone->panSweep = 0;
    tone->wavePointer = (uint32_t*)(uintptr_t)(values[2] & 0x03);
    tone->attack = (uint8_t)(values[3] & 0x07);
    tone->decay = (uint8_t)(values[4] & 0x07);
    tone->sustain = (uint8_t)(values[5] & 0x0F);
    tone->release = (uint8_t)(values[6] & 0x07);
    return VG_LINE_CONSUMED;
}

static VoiceLineResult parse_square_voice(const VoiceParseContext* context, const char* trimmed, int voiceIndex)
{
    if (strncmp(trimmed, "voice_square_1_alt ", 19) == 0)
        return populate_square_voice(context, trimmed + 19, voiceIndex, VOICE_SQUARE_1_ALT, true);
    if (strncmp(trimmed, "voice_square_1 ", 15) == 0)
        return populate_square_voice(context, trimmed + 15, voiceIndex, VOICE_SQUARE_1, true);
    if (strncmp(trimmed, "voice_square_2_alt ", 19) == 0)
        return populate_square_voice(context, trimmed + 19, voiceIndex, VOICE_SQUARE_2_ALT, false);
    if (strncmp(trimmed, "voice_square_2 ", 15) == 0)
        return populate_square_voice(context, trimmed + 15, voiceIndex, VOICE_SQUARE_2, false);
    return VG_LINE_UNHANDLED;
}

static bool resolve_programmable_wave(const VoiceParseContext* context, ToneData* tone, const char* symbol)
{
    const char* wavePath = symbol_map_find(context->programmableWaveMap, symbol);
    if (!wavePath)
        return true;
    char absolutePath[VG_MAX_PATH_LEN];
    if (!build_path(absolutePath, sizeof(absolutePath), context->projectRoot, wavePath))
        return false;
    return vg_load_session_add_prog(context->session, &tone->wavePointer, absolutePath);
}

static VoiceLineResult
parse_programmable_wave_voice(const VoiceParseContext* context, const char* trimmed, int voiceIndex)
{
    const char* p = NULL;
    uint8_t type = VOICE_PROGRAMMABLE_WAVE;
    if (strncmp(trimmed, "voice_programmable_wave_alt ", 27) == 0)
    {
        p = trimmed + 27;
        type = VOICE_PROGRAMMABLE_WAVE_ALT;
    }
    else if (strncmp(trimmed, "voice_programmable_wave ", 23) == 0)
    {
        p = trimmed + 23;
    }
    else
    {
        return VG_LINE_UNHANDLED;
    }
    SampleVoiceArguments arguments;
    VoiceArgumentsResult argumentsResult = parse_sample_voice_arguments(p, &arguments);
    if (argumentsResult == VG_ARGUMENTS_HARD_FAIL)
        return VG_LINE_HARD_FAIL;
    if (argumentsResult != VG_ARGUMENTS_VALID)
        return VG_LINE_CONSUMED;
    set_parsed_voice_name(context, voiceIndex, arguments.symbol);
    ToneData* tone = &context->voices[voiceIndex];
    tone->type = type;
    tone->key = (uint8_t)arguments.key;
    tone->attack = (uint8_t)(arguments.attack & 0x07);
    tone->decay = (uint8_t)(arguments.decay & 0x07);
    tone->sustain = (uint8_t)(arguments.sustain & 0x0F);
    tone->release = (uint8_t)(arguments.release & 0x07);
    if (!resolve_programmable_wave(context, tone, arguments.symbol))
        return VG_LINE_HARD_FAIL;
    return VG_LINE_CONSUMED;
}

static VoiceLineResult
populate_noise_voice(const VoiceParseContext* context, const char* p, int voiceIndex, uint8_t type)
{
    int values[7];
    if (!vg_parse_int_list(&p, values, 7))
        return VG_LINE_CONSUMED;
    ToneData* tone = &context->voices[voiceIndex];
    tone->type = type;
    tone->key = (uint8_t)values[0];
    tone->wavePointer = (uint32_t*)(uintptr_t)(values[2] & 0x01);
    tone->attack = (uint8_t)(values[3] & 0x07);
    tone->decay = (uint8_t)(values[4] & 0x07);
    tone->sustain = (uint8_t)(values[5] & 0x0F);
    tone->release = (uint8_t)(values[6] & 0x07);
    return VG_LINE_CONSUMED;
}

static VoiceLineResult parse_noise_voice(const VoiceParseContext* context, const char* trimmed, int voiceIndex)
{
    if (strncmp(trimmed, "voice_noise_alt ", 16) == 0)
        return populate_noise_voice(context, trimmed + 16, voiceIndex, VOICE_NOISE_ALT);
    if (strncmp(trimmed, "voice_noise ", 12) == 0)
        return populate_noise_voice(context, trimmed + 12, voiceIndex, VOICE_NOISE);
    return VG_LINE_UNHANDLED;
}

static bool
load_keysplit_subgroup(const VoiceParseContext* context, ToneData* tone, const char* symbol, int inContinuation)
{
    if (context->noSubRecurse)
        return true;
    if (inContinuation)
        return true;
    ToneData* subgroup = NULL;
    int result = load_sub_voicegroup_session(context->projectRoot,
                                             symbol,
                                             context->registry,
                                             context->session,
                                             context->directSoundMap,
                                             context->programmableWaveMap,
                                             context->keysplitMap,
                                             context->discovery,
                                             context->waveCache,
                                             &subgroup);
    if (result == -1)
        return false;
    tone->subGroup = subgroup;
    return true;
}

static bool copy_keysplit_table(const VoiceParseContext* context, ToneData* tone, const char* symbol)
{
    KeySplitDef* definition = NULL;
    if (!keysplit_map_find_or_rescan_checked(context->keysplitMap, symbol, context->discovery, &definition))
        return false;
    if (!definition)
        return true;
    uint8_t* table = (uint8_t*)malloc(128);
    if (!table)
        return false;
    memcpy(table, definition->table, 128);
    if (!vg_register_keysplittable(context->registry, table))
    {
        free(table);
        return false;
    }
    tone->keySplitTable = table;
    return true;
}

static VoiceLineResult
parse_keysplit_all_voice(const VoiceParseContext* context, const char* trimmed, int voiceIndex, int inContinuation)
{
    if (strncmp(trimmed, "voice_keysplit_all ", 19) != 0)
        return VG_LINE_UNHANDLED;
    const char* p = trimmed + 19;
    char symbol[MAX_SYMBOL_LEN];
    int symbolResult = vg_extract_eol_symbol(&p, symbol, sizeof(symbol));
    if (symbolResult == -1)
        return VG_LINE_HARD_FAIL;
    if (symbolResult != 0)
        return VG_LINE_CONSUMED;
    set_parsed_voice_name(context, voiceIndex, symbol);
    ToneData* tone = &context->voices[voiceIndex];
    tone->type = VOICE_KEYSPLIT_ALL;
    if (!load_keysplit_subgroup(context, tone, symbol, inContinuation))
        return VG_LINE_HARD_FAIL;
    return VG_LINE_CONSUMED;
}

static VoiceLineResult
parse_keysplit_voice(const VoiceParseContext* context, const char* trimmed, int voiceIndex, int inContinuation)
{
    if (strncmp(trimmed, "voice_keysplit ", 15) != 0)
        return VG_LINE_UNHANDLED;
    const char* p = trimmed + 15;
    char subgroupSymbol[MAX_SYMBOL_LEN];
    int subgroupResult = vg_extract_comma_symbol(&p, subgroupSymbol, sizeof(subgroupSymbol));
    if (subgroupResult == -1)
        return VG_LINE_HARD_FAIL;
    if (subgroupResult != 0)
        return VG_LINE_CONSUMED;
    char keysplitSymbol[MAX_SYMBOL_LEN];
    int keysplitResult = vg_extract_eol_symbol(&p, keysplitSymbol, sizeof(keysplitSymbol));
    if (keysplitResult == -1)
        return VG_LINE_HARD_FAIL;
    if (keysplitResult != 0)
        return VG_LINE_CONSUMED;
    set_parsed_voice_name(context, voiceIndex, subgroupSymbol);
    ToneData* tone = &context->voices[voiceIndex];
    tone->type = VOICE_KEYSPLIT;
    if (!load_keysplit_subgroup(context, tone, subgroupSymbol, inContinuation))
        return VG_LINE_HARD_FAIL;
    if (!copy_keysplit_table(context, tone, keysplitSymbol))
        return VG_LINE_HARD_FAIL;
    return VG_LINE_CONSUMED;
}

static bool resolve_cry_wave(const VoiceParseContext* context, ToneData* tone, const char* symbol)
{
    const char* samplePath = symbol_map_find(context->directSoundMap, symbol);
    if (!samplePath)
        return true;
    char binPath[VG_MAX_PATH_LEN];
    if (!build_path(binPath, sizeof(binPath), context->projectRoot, samplePath))
        return false;
    return vg_load_session_add_wave(context->session, &tone->wav, NULL, NULL, binPath);
}

static VoiceLineResult parse_cry_voice(const VoiceParseContext* context, const char* trimmed, int voiceIndex)
{
    const char* p = NULL;
    uint8_t type = VOICE_CRY;
    if (strncmp(trimmed, "cry_reverse ", 12) == 0)
    {
        p = trimmed + 12;
        type = VOICE_CRY_REVERSE;
    }
    else if (strncmp(trimmed, "cry ", 4) == 0)
    {
        p = trimmed + 4;
    }
    else
    {
        return VG_LINE_UNHANDLED;
    }
    char symbol[MAX_SYMBOL_LEN];
    int symbolResult = vg_extract_eol_symbol(&p, symbol, sizeof(symbol));
    if (symbolResult == -1)
        return VG_LINE_HARD_FAIL;
    if (symbolResult != 0)
        return VG_LINE_CONSUMED;
    set_parsed_voice_name(context, voiceIndex, symbol);
    ToneData* tone = &context->voices[voiceIndex];
    tone->type = type;
    tone->key = 60;
    tone->attack = 0xFF;
    tone->decay = 0;
    tone->sustain = 0xFF;
    tone->release = 0;
    if (!resolve_cry_wave(context, tone, symbol))
        return VG_LINE_HARD_FAIL;
    return VG_LINE_CONSUMED;
}

static VoiceLineResult
parse_voice_macro(const VoiceParseContext* context, const char* trimmed, int voiceIndex, int inContinuation)
{
    VoiceLineResult result = parse_directsound_voice(context, trimmed, voiceIndex);
    if (result != VG_LINE_UNHANDLED)
        return result;
    result = parse_square_voice(context, trimmed, voiceIndex);
    if (result != VG_LINE_UNHANDLED)
        return result;
    result = parse_programmable_wave_voice(context, trimmed, voiceIndex);
    if (result != VG_LINE_UNHANDLED)
        return result;
    result = parse_noise_voice(context, trimmed, voiceIndex);
    if (result != VG_LINE_UNHANDLED)
        return result;
    result = parse_keysplit_all_voice(context, trimmed, voiceIndex, inContinuation);
    if (result != VG_LINE_UNHANDLED)
        return result;
    result = parse_keysplit_voice(context, trimmed, voiceIndex, inContinuation);
    if (result != VG_LINE_UNHANDLED)
        return result;
    return parse_cry_voice(context, trimmed, voiceIndex);
}

static VoiceLineResult
parse_voice_group_metadata(const VoiceParseContext* context, const char* trimmed, VoiceParseProgress* progress)
{
    if (strncmp(trimmed, "voice_group ", 12) != 0)
        return VG_LINE_UNHANDLED;
    if (progress->inContinuation)
        return VG_LINE_STOP;
    if (context->noSubRecurse)
        return VG_LINE_STOP;
    const char* p = trimmed + 12;
    char name[MAX_SYMBOL_LEN];
    int symbolResult = vg_extract_comma_symbol(&p, name, sizeof(name));
    if (symbolResult == -1)
        return VG_LINE_HARD_FAIL;
    if (symbolResult == 0)
    {
        int startingNote = 0;
        if (vg_parse_next_int(&p, &startingNote))
        {
            if (startingNote > 0)
            {
                if (startingNote < VOICEGROUP_SIZE)
                    progress->voiceIndex = startingNote;
            }
        }
    }
    return VG_LINE_METADATA;
}

static bool initialize_voicegroup_section(VoicegroupSection* section, const char* startLabel, int contiguousFill)
{
    memset(section, 0, sizeof(*section));
    section->startLabel = startLabel;
    section->contiguousFill = contiguousFill;
    if (!startLabel)
    {
        section->inSection = 1;
        section->labelFound = 1;
        return true;
    }
    if (!vg_section_label_valid(startLabel))
        return false;
    int length = snprintf(section->searchLabel, sizeof(section->searchLabel), "%s::", startLabel);
    if (length < 0)
        return false;
    if ((size_t)length >= sizeof(section->searchLabel))
        return false;
    section->searchLength = (size_t)length;
    return true;
}

static bool voicegroup_line_is_section_label(const VoicegroupSection* section, const char* trimmed)
{
    if (section->searchLength == 0)
        return false;
    if (strncmp(trimmed, section->searchLabel, section->searchLength) != 0)
        return false;
    char trailing = trimmed[section->searchLength];
    if (trailing == '\0')
        return true;
    return isspace((unsigned char)trailing);
}

static bool voicegroup_line_is_section_boundary(const char* trimmed)
{
    char* separator = strstr(trimmed, "::");
    if (separator)
    {
        if (separator > trimmed)
        {
            if (!isspace((unsigned char)trimmed[0]))
                return true;
        }
    }
    return strncmp(trimmed, ".align", 6) == 0;
}

static VoiceLineResult
advance_voicegroup_section(VoicegroupSection* section, VoiceParseProgress* progress, const char* trimmed)
{
    if (!section->startLabel)
        return VG_LINE_METADATA;
    if (!section->inSection)
    {
        if (voicegroup_line_is_section_label(section, trimmed))
        {
            section->inSection = 1;
            section->labelFound = 1;
        }
        return VG_LINE_UNHANDLED;
    }
    if (progress->voicesParsedInSection <= 0)
        return VG_LINE_METADATA;
    if (progress->inContinuation)
        return VG_LINE_METADATA;
    if (!voicegroup_line_is_section_boundary(trimmed))
        return VG_LINE_METADATA;
    if (!section->contiguousFill)
        return VG_LINE_STOP;
    progress->inContinuation = 1;
    return VG_LINE_METADATA;
}
static VoiceLineResult parse_voicegroup_content_line(const VoiceParseContext* context,
                                                     VoicegroupSection* section,
                                                     VoiceParseProgress* progress,
                                                     const char* trimmed)
{
    VoiceLineResult sectionResult = advance_voicegroup_section(section, progress, trimmed);
    if (sectionResult == VG_LINE_UNHANDLED)
        return VG_LINE_UNHANDLED;
    if (sectionResult == VG_LINE_STOP)
        return VG_LINE_STOP;
    VoiceLineResult metadataResult = parse_voice_group_metadata(context, trimmed, progress);
    if (metadataResult != VG_LINE_UNHANDLED)
        return metadataResult;
    VoiceLineResult voiceResult = parse_voice_macro(context, trimmed, progress->voiceIndex, progress->inContinuation);
    if (voiceResult == VG_LINE_HARD_FAIL)
        return VG_LINE_HARD_FAIL;
    if (voiceResult == VG_LINE_CONSUMED)
    {
        progress->voiceIndex++;
        progress->voicesParsedInSection++;
    }
    return VG_LINE_UNHANDLED;
}

static bool voicegroup_section_is_missing(const VoicegroupSection* section)
{
    if (!section->startLabel)
        return false;
    return !section->labelFound;
}

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
    vg_log("parse_voicegroup_file_session: '%s' label='%s' start=%d",
           filePath,
           startLabel ? startLabel : "(none)",
           startIndex);
    FILE* file = fopen(filePath, "r");
    if (!file)
    {
        fprintf(stderr, "voicegroup_loader: cannot open %s\n", filePath);
        return -1;
    }
    VoicegroupSection section;
    if (!initialize_voicegroup_section(&section, startLabel, contiguousFill))
    {
        fclose(file);
        return -1;
    }
    VoiceParseContext context = {
        projectRoot,
        destVoices,
        destNames,
        vgReg,
        session,
        dsMap,
        pwMap,
        ksMap,
        disc,
        waveCache,
        noSubRecurse,
    };
    VoiceParseProgress progress = {startIndex, 0, 0};
    bool hardFail = false;
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), file))
    {
        if (progress.voiceIndex >= VOICEGROUP_SIZE)
            break;
        strip_comment(line);
        rtrim(line);
        char* trimmed = ltrim(line);
        if (trimmed[0] == '\0')
            continue;
        VoiceLineResult result = parse_voicegroup_content_line(&context, &section, &progress, trimmed);
        if (result == VG_LINE_HARD_FAIL)
        {
            hardFail = true;
            break;
        }
        if (result == VG_LINE_STOP)
            break;
    }
    vg_log("parse_voicegroup_file_session: done, voiceIndex=%d hardFail=%d labelFound=%d",
           progress.voiceIndex,
           hardFail,
           section.labelFound);
    fclose(file);
    if (hardFail)
        return -1;
    if (voicegroup_section_is_missing(&section))
        return -1;
    return progress.voiceIndex;
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
static void vg_stdio_set_error(char* error, size_t errorCapacity, const char* format, ...)
{
    if (!error)
        return;
    if (!errorCapacity)
        return;
    va_list arguments;
    va_start(arguments, format);
    vsnprintf(error, errorCapacity, format, arguments);
    va_end(arguments);
}

static bool vg_stdio_prepare_batch(
    const char* const* paths, size_t count, VoicegroupFileBlob* out, char* error, size_t errorCapacity)
{
    if (count != 0)
    {
        if (!paths)
        {
            vg_stdio_set_error(error, errorCapacity, "vg_stdio: invalid batch");
            return false;
        }
        if (!out)
        {
            vg_stdio_set_error(error, errorCapacity, "vg_stdio: invalid batch");
            return false;
        }
    }
    if (count > (size_t)INT_MAX)
    {
        vg_stdio_set_error(error, errorCapacity, "vg_stdio: batch too large");
        return false;
    }
    for (size_t i = 0; i < count; i++)
        out[i] = (VoicegroupFileBlob){0};
    if (error)
    {
        if (errorCapacity)
            error[0] = '\0';
    }
    return true;
}

static bool vg_stdio_measure_file(FILE* file, const char* path, size_t* size, char* error, size_t errorCapacity)
{
    if (fseek(file, 0, SEEK_END) != 0)
    {
        fclose(file);
        vg_stdio_set_error(error, errorCapacity, "seek failed for %s", path);
        return false;
    }
    long fileSize = ftell(file);
    if (fileSize < 0)
    {
        fclose(file);
        vg_stdio_set_error(error, errorCapacity, "size probe failed for %s", path);
        return false;
    }
    if ((uint64_t)fileSize > (uint64_t)SIZE_MAX - 1)
    {
        fclose(file);
        vg_stdio_set_error(error, errorCapacity, "file too large: %s", path);
        return false;
    }
    if ((uint64_t)fileSize > (uint64_t)PTRDIFF_MAX)
    {
        fclose(file);
        vg_stdio_set_error(error, errorCapacity, "file too large: %s", path);
        return false;
    }
    rewind(file);
    *size = (size_t)fileSize;
    return true;
}

static bool vg_stdio_read_file(const char* path, VoicegroupFileBlob* blob, char* error, size_t errorCapacity)
{
    FILE* file = fopen(path, "rb");
    if (!file)
        return true;
    size_t size = 0;
    if (!vg_stdio_measure_file(file, path, &size, error, errorCapacity))
        return false;
    uint8_t* data = (uint8_t*)malloc(size + 1);
    if (!data)
    {
        fclose(file);
        vg_stdio_set_error(error, errorCapacity, "out of memory reading %s", path);
        return false;
    }
    size_t got = 0;
    if (size > 0)
        got = fread(data, 1, size, file);
    if (got != size)
    {
        free(data);
        fclose(file);
        vg_stdio_set_error(error, errorCapacity, "short read on %s", path);
        return false;
    }
    if (ferror(file))
    {
        free(data);
        fclose(file);
        vg_stdio_set_error(error, errorCapacity, "read error on %s", path);
        return false;
    }
    fclose(file);
    data[size] = 0;
    blob->data = data;
    blob->size = size;
    blob->found = 1;
    return true;
}

static bool vg_stdio_read_batch(
    void* user, const char* const* paths, size_t count, VoicegroupFileBlob* out, char* error, size_t errorCapacity)
{
    (void)user;
    if (!vg_stdio_prepare_batch(paths, count, out, error, errorCapacity))
        return false;
    for (size_t i = 0; i < count; i++)
    {
        if (!paths[i])
        {
            vg_stdio_set_error(error, errorCapacity, "vg_stdio: null path at %zu", i);
            return false;
        }
        if (!vg_stdio_read_file(paths[i], &out[i], error, errorCapacity))
            return false;
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
    int r = parse_voicegroup_file_session(project->projectRoot,
                                          filePath,
                                          startLabel,
                                          vg->voices,
                                          vg->voiceNames,
                                          vg,
                                          &session,
                                          &project->dsMap,
                                          &project->pwMap,
                                          &project->ksMap,
                                          project->disc,
                                          &waveCache,
                                          0,
                                          0,
                                          0);
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

typedef struct
{
    VoicegroupProject* project;
    LoadedSampleSet* set;
    VgLoadSession* session;
    WaveCache* waveCache;
} SampleSetLoadContext;

static size_t sample_set_allocation_count(int count)
{
    if (count > 0)
        return (size_t)count;
    return 1;
}

static LoadedSampleSet* allocate_sample_set(int sampleCount, int waveCount, int keysplitCount)
{
    LoadedSampleSet* set = (LoadedSampleSet*)calloc(1, sizeof(LoadedSampleSet));
    if (!set)
        return NULL;
    set->container = (LoadedVoiceGroup*)calloc(1, sizeof(LoadedVoiceGroup));
    set->waves = (WaveData**)calloc(sample_set_allocation_count(sampleCount), sizeof(WaveData*));
    set->progWaves = (uint32_t**)calloc(sample_set_allocation_count(waveCount), sizeof(uint32_t*));
    set->keysplits = (LoadedKeysplit*)calloc(sample_set_allocation_count(keysplitCount), sizeof(LoadedKeysplit));
    if (!set->container)
    {
        voicegroup_free_samples(set);
        return NULL;
    }
    if (!set->waves)
    {
        voicegroup_free_samples(set);
        return NULL;
    }
    if (!set->progWaves)
    {
        voicegroup_free_samples(set);
        return NULL;
    }
    if (!set->keysplits)
    {
        voicegroup_free_samples(set);
        return NULL;
    }
    set->count = sampleCount;
    set->progWaveCount = waveCount;
    set->keysplitCount = keysplitCount;
    return set;
}

static bool populate_sample_set_sample(const SampleSetLoadContext* context, int index, const char* symbol)
{
    const uint8_t* synthDesc = symbol_map_find_synth(&context->project->dsMap, symbol);
    if (synthDesc)
    {
        WaveData* wave = build_synth_wavedata(synthDesc, symbol, context->set->container, context->waveCache);
        if (!wave)
            return false;
        context->set->waves[index] = wave;
        return true;
    }
    const char* samplePath = symbol_map_find(&context->project->dsMap, symbol);
    if (samplePath)
    {
        char wavPath[VG_MAX_PATH_LEN];
        char aifPath[VG_MAX_PATH_LEN];
        char binPath[VG_MAX_PATH_LEN];
        if (!build_wave_abs_paths(context->project->projectRoot, samplePath, wavPath, aifPath, binPath))
            return false;
        const char* wav = NULL;
        const char* aif = NULL;
        if (wavPath[0])
            wav = wavPath;
        if (aifPath[0])
            aif = aifPath;
        return vg_load_session_add_wave(context->session, &context->set->waves[index], wav, aif, binPath);
    }
    WaveData* wave = NULL;
    int result = resolve_and_load_sample_serial(context->project->projectRoot,
                                                symbol,
                                                &context->project->dsMap,
                                                context->project->disc,
                                                context->set->container,
                                                context->waveCache,
                                                &wave);
    if (result == -1)
        return false;
    if (result == 1)
        context->set->waves[index] = wave;
    return true;
}

static bool populate_sample_set_samples(const SampleSetLoadContext* context, const char* const* symbols, int count)
{
    for (int i = 0; i < count; i++)
    {
        if (!populate_sample_set_sample(context, i, symbols[i]))
            return false;
    }
    return true;
}

static bool populate_sample_set_wave(const SampleSetLoadContext* context, int index, const char* symbol)
{
    const char* wavePath = symbol_map_find(&context->project->pwMap, symbol);
    if (!wavePath)
        return true;
    char absolutePath[VG_MAX_PATH_LEN];
    if (!build_path(absolutePath, sizeof(absolutePath), context->project->projectRoot, wavePath))
        return false;
    return vg_load_session_add_prog(context->session, &context->set->progWaves[index], absolutePath);
}

static bool populate_sample_set_waves(const SampleSetLoadContext* context, const char* const* symbols, int count)
{
    for (int i = 0; i < count; i++)
    {
        if (!populate_sample_set_wave(context, i, symbols[i]))
            return false;
    }
    return true;
}

static bool populate_sample_set_keysplit(const SampleSetLoadContext* context,
                                         int index,
                                         const char* subgroupSymbol,
                                         const char* tableSymbol)
{
    ToneData* subgroup = NULL;
    int subgroupResult = load_sub_voicegroup_session(context->project->projectRoot,
                                                     subgroupSymbol,
                                                     context->set->container,
                                                     context->session,
                                                     &context->project->dsMap,
                                                     &context->project->pwMap,
                                                     &context->project->ksMap,
                                                     context->project->disc,
                                                     context->waveCache,
                                                     &subgroup);
    if (subgroupResult == -1)
        return false;
    context->set->keysplits[index].subGroup = subgroup;
    KeySplitDef* definition = NULL;
    if (!keysplit_map_find_or_rescan_checked(
            &context->project->ksMap, tableSymbol, context->project->disc, &definition))
    {
        return false;
    }
    if (!definition)
        return true;
    uint8_t* table = (uint8_t*)malloc(128);
    if (!table)
        return false;
    memcpy(table, definition->table, 128);
    if (!vg_register_keysplittable(context->set->container, table))
    {
        free(table);
        return false;
    }
    context->set->keysplits[index].table = table;
    return true;
}

static bool populate_sample_set_keysplits(const SampleSetLoadContext* context,
                                          const char* const* subgroupSymbols,
                                          const char* const* tableSymbols,
                                          int count)
{
    for (int i = 0; i < count; i++)
    {
        if (!populate_sample_set_keysplit(context, i, subgroupSymbols[i], tableSymbols[i]))
            return false;
    }
    return true;
}

static LoadedSampleSet* project_load_samples(VoicegroupProject* project,
                                             const char* const* sampleSymbols,
                                             int sampleCount,
                                             const char* const* waveSymbols,
                                             int waveCount,
                                             const char* const* keysplitSymbols,
                                             const char* const* keysplitTableSymbols,
                                             int keysplitCount)
{
    LoadedSampleSet* set = allocate_sample_set(sampleCount, waveCount, keysplitCount);
    if (!set)
        return NULL;
    WaveCache waveCache;
    wave_cache_init(&waveCache);
    VgLoadSession session;
    vg_load_session_init(&session, &project->fileIo, set->container, &waveCache);
    SampleSetLoadContext context = {project, set, &session, &waveCache};
    if (!populate_sample_set_samples(&context, sampleSymbols, sampleCount))
    {
        vg_load_session_deinit(&session);
        voicegroup_free_samples(set);
        return NULL;
    }
    if (!populate_sample_set_waves(&context, waveSymbols, waveCount))
    {
        vg_load_session_deinit(&session);
        voicegroup_free_samples(set);
        return NULL;
    }
    if (!populate_sample_set_keysplits(&context, keysplitSymbols, keysplitTableSymbols, keysplitCount))
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
