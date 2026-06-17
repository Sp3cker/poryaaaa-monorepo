#include "vg_parser.h"
#include "vg_alloc.h"
#include "vg_log.h"
#include "vg_paths.h"
#include "vg_source.h"
#include "vg_voice_macro.h"
#include "vg_wav.h"

#include "voicegroup_types.h"

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INITIAL_CAPACITY 64

/* ---- Dynamic-array registration helpers ---- */
/* Each register_* call appends to an owned list on the
 * LoadedVoiceGroup; voicegroup_free() frees those lists. */

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

static bool register_progwave(LoadedVoiceGroup* vg, uint32_t* pw)
{
    if (vg->progWaveCount >= vg->progWaveCapacity)
    {
        int newCapacity;
        if (!next_capacity(vg->progWaveCapacity, vg->progWaveCount + 1, &newCapacity))
            return false;
        uint32_t** items = vg_realloc_array(vg->progWaves, (size_t)newCapacity, sizeof(*vg->progWaves));
        if (!items)
            return false;
        vg->progWaves = items;
        vg->progWaveCapacity = newCapacity;
    }
    vg->progWaves[vg->progWaveCount++] = pw;
    return true;
}

static bool register_subgroup(LoadedVoiceGroup* vg, ToneData* sg)
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
    vg->subGroups[vg->subGroupCount++] = sg;
    return true;
}

static bool register_keysplittable(LoadedVoiceGroup* vg, uint8_t* ks)
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
    vg->keySplitTables[vg->keySplitTableCount++] = ks;
    return true;
}

/* ---- Parse context ---- */
/* Every voice-macro handler needs access to the same small bundle of
 * inputs. Threading them through 15+ call sites as individual args
 * was error-prone; packaging into one struct makes handler
 * signatures uniform and short. */

typedef struct
{
    const char* projectRoot;
    const SymbolMap* dsMap;
    const SymbolMap* pwMap;
    const KeySplitMap* ksMap;
    const ProjectDiscovery* disc;
    LoadedVoiceGroup* vg;
    WaveCache* waveCache;
    bool allocationFailed;
    /* Trailing "@ ..." comment from the current voice macro's line, trimmed
     * of surrounding whitespace. NULL/empty if the line had no comment.
     * Valid only for the duration of the current dispatch; handlers that
     * recurse (keysplit -> load_sub_voicegroup) must capture it first. */
    const char* lineComment;
} ParseCtx;

/* ---- Sample resolution ---- */

/*
 * Resolve a DirectSound sample symbol to a WaveData via the symbol
 * map (looking up the .bin path pokeemerald records there, then
 * substituting .wav on disk). Returns NULL if the symbol isn't in
 * direct_sound_data.inc — matching real-ROM linking, where only
 * .incbin'd samples are part of the build. Cached within the
 * ParseCtx so the same .wav isn't loaded twice per voicegroup.
 */
static WaveData* resolve_sample(ParseCtx* ctx, const char* symbol)
{
    const char* samplePath = vg_symbol_map_find(ctx->dsMap, symbol);
    if (samplePath)
    {
        /* Cache key is the absolute .wav path (swap .bin extension). */
        char relWavPath[VG_MAX_PATH_LEN];
        strncpy(relWavPath, samplePath, VG_MAX_PATH_LEN - 1);
        relWavPath[VG_MAX_PATH_LEN - 1] = '\0';
        size_t pathLen = strlen(relWavPath);
        if (pathLen >= 4 && strcmp(relWavPath + pathLen - 4, ".bin") == 0)
        {
            relWavPath[pathLen - 3] = 'w';
            relWavPath[pathLen - 2] = 'a';
            relWavPath[pathLen - 1] = 'v';
        }
        char absWavPath[VG_MAX_PATH_LEN];
        vg_build_path(absWavPath, sizeof(absWavPath), ctx->projectRoot, relWavPath);

        WaveData* cached = vg_wave_cache_find(ctx->waveCache, absWavPath);
        if (cached)
            return cached;

        WaveData* wd = vg_load_sample(ctx->projectRoot, samplePath);
        if (wd)
        {
            if (!register_wavedata(ctx->vg, wd))
            {
                free(wd);
                ctx->allocationFailed = true;
                return NULL;
            }
            vg_wave_cache_insert(ctx->waveCache, absWavPath, wd);
            return wd;
        }
    }

    return NULL;
}

/*
 * Load a programmable-wave symbol and register it with the voicegroup.
 * Returns NULL if not found.
 */
static uint32_t* resolve_prog_wave(ParseCtx* ctx, const char* symbol)
{
    const char* wavePath = vg_symbol_map_find(ctx->pwMap, symbol);
    if (!wavePath)
        return NULL;
    uint32_t* pw = vg_load_prog_wave(ctx->projectRoot, wavePath);
    if (pw && !register_progwave(ctx->vg, pw))
    {
        free(pw);
        ctx->allocationFailed = true;
        return NULL;
    }
    return pw;
}

/*
 * Record a user-visible sample name on the voice slot. Called by any
 * handler that resolves a sample/wave/cry symbol so GUIs can show
 * the list of instruments without reloading the symbol map.
 *
 * If the symbol resolves in `map`, we store the basename of the
 * resolved path (e.g. "brass_1.bin"). If not (e.g. the wav-dirs
 * fallback path took over), we fall back to the raw symbol name.
 */
static void set_slot_name(ParseCtx* ctx, ToneData* td, const char* name)
{
    int slot = (int)(td - ctx->vg->voices);
    if (slot < 0 || slot >= VOICEGROUP_SIZE)
        return;
    strncpy(ctx->vg->voiceSampleNames[slot], name, VG_MAX_VOICE_SAMPLE_NAME - 1);
    ctx->vg->voiceSampleNames[slot][VG_MAX_VOICE_SAMPLE_NAME - 1] = '\0';
}

static void record_sample_name(ParseCtx* ctx, ToneData* td, const SymbolMap* map, const char* symbol)
{
    const char* path = vg_symbol_map_find(map, symbol);
    set_slot_name(ctx, td, path ? vg_path_basename(path) : symbol);
}

/*
 * Cry voices deliberately bypass the wav cache and sample dir fallback
 * — pokeemerald's cry samples are always referenced via the symbol map
 * and loaded once per voicegroup.
 */
static WaveData* resolve_cry_sample(ParseCtx* ctx, const char* symbol)
{
    const char* samplePath = vg_symbol_map_find(ctx->dsMap, symbol);
    if (!samplePath)
        return NULL;
    WaveData* wd = vg_load_bin_sample(ctx->projectRoot, samplePath);
    if (wd && !register_wavedata(ctx->vg, wd))
    {
        free(wd);
        ctx->allocationFailed = true;
        return NULL;
    }
    return wd;
}

/* Forward declaration: parser and sub-voicegroup loader are mutually
 * recursive (a voice_keysplit references another voicegroup). */
static int parse_voicegroup_file(const char* filePath, const char* startLabel, ParseCtx* ctx);

static ToneData* load_sub_voicegroup(const char* vgSymbol, ParseCtx* ctx)
{
    const char* name = vgSymbol;
    if (strncmp(name, "voicegroup_", 11) == 0)
        name += 11;

    VoicegroupSourceLocation loc = vg_find_voicegroup_source(name, ctx->disc);
    if (!loc.found)
    {
        vg_err("cannot find sub-voicegroup '%s'", vgSymbol);
        return NULL;
    }

    ToneData* subVg = vg_malloc_array(VOICEGROUP_SIZE, sizeof(ToneData));
    if (!subVg)
    {
        ctx->allocationFailed = true;
        return NULL;
    }
    memset(subVg, 0, sizeof(ToneData) * VOICEGROUP_SIZE);

    /* parse_voicegroup_file writes into vg->voices and, via
     * record_sample_name, into vg->voiceSampleNames. Save/restore both
     * so the recursion doesn't clobber the parent voicegroup's data. */
    ToneData savedVoices[VOICEGROUP_SIZE];
    char savedNames[VOICEGROUP_SIZE][VG_MAX_VOICE_SAMPLE_NAME];
    memcpy(savedVoices, ctx->vg->voices, sizeof(savedVoices));
    memcpy(savedNames, ctx->vg->voiceSampleNames, sizeof(savedNames));
    memset(ctx->vg->voices, 0, sizeof(ctx->vg->voices));
    memset(ctx->vg->voiceSampleNames, 0, sizeof(ctx->vg->voiceSampleNames));

    const char* startLabel = loc.label[0] ? loc.label : NULL;
    int rc = parse_voicegroup_file(loc.filePath, startLabel, ctx);
    if (rc != 0)
    {
        ctx->allocationFailed = true;
        free(subVg);
        memcpy(ctx->vg->voices, savedVoices, sizeof(savedVoices));
        memcpy(ctx->vg->voiceSampleNames, savedNames, sizeof(savedNames));
        return NULL;
    }

    memcpy(subVg, ctx->vg->voices, sizeof(ToneData) * VOICEGROUP_SIZE);
    memcpy(ctx->vg->voices, savedVoices, sizeof(savedVoices));
    memcpy(ctx->vg->voiceSampleNames, savedNames, sizeof(savedNames));

    if (!register_subgroup(ctx->vg, subVg))
    {
        ctx->allocationFailed = true;
        free(subVg);
        return NULL;
    }
    return subVg;
}

/* ---- Per-macro-kind handlers ---- */

static bool handle_directsound(ToneData* td, uint8_t voiceType, const char* args, ParseCtx* ctx)
{
    int key, pan, attack, decay, sustain, release;
    char sampleSymbol[VG_MAX_SYMBOL_LEN];
    if (sscanf(args, "%d, %d, %[^,], %d, %d, %d, %d", &key, &pan, sampleSymbol, &attack, &decay, &sustain, &release) !=
        7)
        return false;

    vg_rtrim(sampleSymbol);
    td->type = voiceType;
    td->key = (uint8_t)key;
    td->panSweep = pan ? (uint8_t)(0x80 | pan) : 0;
    td->attack = (uint8_t)attack;
    td->decay = (uint8_t)decay;
    td->sustain = (uint8_t)sustain;
    td->release = (uint8_t)release;

    WaveData* wd = resolve_sample(ctx, sampleSymbol);
    if (wd)
    {
        td->wav = wd;
        record_sample_name(ctx, td, ctx->dsMap, sampleSymbol);
    }
    return true;
}

static bool handle_square_1(ToneData* td, uint8_t voiceType, const char* args, ParseCtx* ctx)
{
    int key, pan, sweep, duty, attack, decay, sustain, release;
    if (sscanf(
            args, "%d, %d, %d, %d, %d, %d, %d, %d", &key, &pan, &sweep, &duty, &attack, &decay, &sustain, &release) !=
        8)
        return false;

    td->type = voiceType;
    td->key = (uint8_t)key;
    td->panSweep = (uint8_t)sweep;
    td->wavePointer = (uint32_t*)(uintptr_t)(duty & 0x03);
    td->attack = (uint8_t)(attack & 0x07);
    td->decay = (uint8_t)(decay & 0x07);
    td->sustain = (uint8_t)(sustain & 0x0F);
    td->release = (uint8_t)(release & 0x07);

    /* Hardware channels carry no sample symbol — record a stable
     * display name so state.json (and downstream UIs like ccomidi)
     * surface them instead of dropping empty-name slots. */
    set_slot_name(ctx, td, voiceType == VOICE_SQUARE_1_ALT ? "Square 1 (alt)" : "Square 1");
    return true;
}

static bool handle_square_2(ToneData* td, uint8_t voiceType, const char* args, ParseCtx* ctx)
{
    int key, pan, duty, attack, decay, sustain, release;
    if (sscanf(args, "%d, %d, %d, %d, %d, %d, %d", &key, &pan, &duty, &attack, &decay, &sustain, &release) != 7)
        return false;

    td->type = voiceType;
    td->key = (uint8_t)key;
    td->panSweep = 0;
    td->wavePointer = (uint32_t*)(uintptr_t)(duty & 0x03);
    td->attack = (uint8_t)(attack & 0x07);
    td->decay = (uint8_t)(decay & 0x07);
    td->sustain = (uint8_t)(sustain & 0x0F);
    td->release = (uint8_t)(release & 0x07);

    set_slot_name(ctx, td, voiceType == VOICE_SQUARE_2_ALT ? "Square 2 (alt)" : "Square 2");
    return true;
}

static bool handle_prog_wave(ToneData* td, uint8_t voiceType, const char* args, ParseCtx* ctx)
{
    int key, pan, attack, decay, sustain, release;
    char waveSymbol[VG_MAX_SYMBOL_LEN];
    if (sscanf(args, "%d, %d, %[^,], %d, %d, %d, %d", &key, &pan, waveSymbol, &attack, &decay, &sustain, &release) != 7)
        return false;

    vg_rtrim(waveSymbol);
    td->type = voiceType;
    td->key = (uint8_t)key;
    td->panSweep = pan ? (uint8_t)(0x80 | pan) : 0;
    td->attack = (uint8_t)(attack & 0x07);
    td->decay = (uint8_t)(decay & 0x07);
    td->sustain = (uint8_t)(sustain & 0x0F);
    td->release = (uint8_t)(release & 0x07);

    uint32_t* pw = resolve_prog_wave(ctx, waveSymbol);
    if (pw)
    {
        td->wavePointer = pw;
        record_sample_name(ctx, td, ctx->pwMap, waveSymbol);
    }
    else
    {
        /* Wave symbol didn't resolve — still record the slot so it shows
         * up in state.json as a Programmable Wave slot. */
        set_slot_name(ctx, td, voiceType == VOICE_PROGRAMMABLE_WAVE_ALT ? "ProgWave (alt)" : "ProgWave");
    }
    return true;
}

static bool handle_noise(ToneData* td, uint8_t voiceType, const char* args, ParseCtx* ctx)
{
    int key, pan, period, attack, decay, sustain, release;
    if (sscanf(args, "%d, %d, %d, %d, %d, %d, %d", &key, &pan, &period, &attack, &decay, &sustain, &release) != 7)
        return false;

    td->type = voiceType;
    td->key = (uint8_t)key;
    td->wavePointer = (uint32_t*)(uintptr_t)(period & 0x01);
    td->attack = (uint8_t)(attack & 0x07);
    td->decay = (uint8_t)(decay & 0x07);
    td->sustain = (uint8_t)(sustain & 0x0F);
    td->release = (uint8_t)(release & 0x07);

    set_slot_name(ctx, td, voiceType == VOICE_NOISE_ALT ? "Noise (alt)" : "Noise");
    return true;
}

static bool handle_keysplit(ToneData* td, uint8_t voiceType, const char* args, ParseCtx* ctx)
{
    char vgSymbol[VG_MAX_SYMBOL_LEN];
    char ksSymbol[VG_MAX_SYMBOL_LEN];
    if (sscanf(args, "%[^,], %s", vgSymbol, ksSymbol) != 2)
        return false;

    /* Capture before recursing — load_sub_voicegroup overwrites lineComment. */
    char displayName[VG_MAX_VOICE_SAMPLE_NAME];
    if (ctx->lineComment && ctx->lineComment[0])
        snprintf(displayName, sizeof(displayName), "%s", ctx->lineComment);
    else
        snprintf(displayName, sizeof(displayName), "%s", vgSymbol);

    vg_rtrim(vgSymbol);
    vg_rtrim(ksSymbol);
    td->type = voiceType;
    td->subGroup = load_sub_voicegroup(vgSymbol, ctx);
    set_slot_name(ctx, td, displayName);

    KeySplitDef* ksDef = vg_keysplit_map_find(ctx->ksMap, ksSymbol);
    if (ksDef)
    {
        uint8_t* table = vg_malloc_array(128, sizeof(*table));
        if (!table)
        {
            ctx->allocationFailed = true;
            return true;
        }
        memcpy(table, ksDef->table, 128);
        if (!register_keysplittable(ctx->vg, table))
        {
            ctx->allocationFailed = true;
            free(table);
            return true;
        }
        td->keySplitTable = table;
    }
    return true;
}

static bool handle_keysplit_all(ToneData* td, uint8_t voiceType, const char* args, ParseCtx* ctx)
{
    char vgSymbol[VG_MAX_SYMBOL_LEN];
    if (sscanf(args, "%s", vgSymbol) != 1)
        return false;

    char displayName[VG_MAX_VOICE_SAMPLE_NAME];
    if (ctx->lineComment && ctx->lineComment[0])
        snprintf(displayName, sizeof(displayName), "%s", ctx->lineComment);
    else
        snprintf(displayName, sizeof(displayName), "%s", vgSymbol);

    vg_rtrim(vgSymbol);
    td->type = voiceType;
    td->subGroup = load_sub_voicegroup(vgSymbol, ctx);
    set_slot_name(ctx, td, displayName);
    return true;
}

static bool handle_cry(ToneData* td, uint8_t voiceType, const char* args, ParseCtx* ctx)
{
    char sampleSymbol[VG_MAX_SYMBOL_LEN];
    if (sscanf(args, "%s", sampleSymbol) != 1)
        return false;
    vg_rtrim(sampleSymbol);

    td->type = voiceType;
    td->key = 60;
    td->attack = 0xFF;
    td->decay = 0;
    td->sustain = 0xFF;
    td->release = 0;

    WaveData* wd = resolve_cry_sample(ctx, sampleSymbol);
    if (wd)
    {
        td->wav = wd;
        record_sample_name(ctx, td, ctx->dsMap, sampleSymbol);
    }
    return true;
}

/* ---- Top-level file parser ---- */

static void capture_line_comment(const char* line, char out[VG_MAX_VOICE_SAMPLE_NAME])
{
    out[0] = '\0';
    const char* c = strchr(line, '@');
    if (!c)
        return;
    c++;
    while (*c && isspace((unsigned char)*c))
        c++;
    size_t n = 0;
    while (*c && n + 1 < VG_MAX_VOICE_SAMPLE_NAME)
        out[n++] = *c++;
    while (n > 0 && isspace((unsigned char)out[n - 1]))
        n--;
    out[n] = '\0';
}

static int consume_voice_line(char* trimmed, int* voiceIndex, int* voicesInSection, ParseCtx* ctx)
{
    int startingNote = vg_voicegroup_start_note(trimmed);
    if (startingNote >= 0)
    {
        *voiceIndex = startingNote;
        return 0;
    }

    ToneData* td = &ctx->vg->voices[*voiceIndex];
    const VoicegroupMacro* macro = NULL;
    const char* args = NULL;
    if (!vg_voice_macro_match(trimmed, &macro, &args))
    {
        if ((strncmp(trimmed, "voice_", 6) == 0 && strncmp(trimmed, "voice_group", 11) != 0) ||
            strncmp(trimmed, "cry", 3) == 0)
        {
            vg_err("malformed voice macro: %s", trimmed);
            return -1;
        }
        return 0;
    }

    bool ok = false;
    switch (macro->kind)
    {
    case VG_MACRO_DIRECTSOUND:
        ok = handle_directsound(td, macro->typeCode, args, ctx);
        break;
    case VG_MACRO_SQUARE_1:
        ok = handle_square_1(td, macro->typeCode, args, ctx);
        break;
    case VG_MACRO_SQUARE_2:
        ok = handle_square_2(td, macro->typeCode, args, ctx);
        break;
    case VG_MACRO_PROG_WAVE:
        ok = handle_prog_wave(td, macro->typeCode, args, ctx);
        break;
    case VG_MACRO_NOISE:
        ok = handle_noise(td, macro->typeCode, args, ctx);
        break;
    case VG_MACRO_KEYSPLIT:
        ok = handle_keysplit(td, macro->typeCode, args, ctx);
        break;
    case VG_MACRO_KEYSPLIT_ALL:
        ok = handle_keysplit_all(td, macro->typeCode, args, ctx);
        break;
    case VG_MACRO_CRY:
        ok = handle_cry(td, macro->typeCode, args, ctx);
        break;
    }
    if (!ok)
    {
        vg_err("malformed voice macro: %s", trimmed);
        return -1;
    }
    if (ctx->allocationFailed)
        return -1;
    (*voiceIndex)++;
    (*voicesInSection)++;
    return 0;
}

/*
 * Walk one .inc/.s file and populate ctx->vg->voices.
 *
 * If startLabel is NULL, we parse the whole file top to bottom
 * (per-voicegroup-file mode).
 *
 * If startLabel is non-NULL, we scan forward to a "<startLabel>::"
 * line and parse until the next label or .align directive.
 */
static int parse_voicegroup_file(const char* filePath, const char* startLabel, ParseCtx* ctx)
{
    vg_log("parse_voicegroup_file: '%s' label='%s'", filePath, startLabel ? startLabel : "(none)");

    FILE* f = fopen(filePath, "r");
    if (!f)
    {
        vg_err("cannot open %s", filePath);
        return -1;
    }

    char searchLabel[VG_MAX_SYMBOL_LEN + 4];
    if (startLabel)
        snprintf(searchLabel, sizeof(searchLabel), "%s::", startLabel);

    char line[VG_MAX_LINE];
    int voiceIndex = 0;
    int inSection = (startLabel == NULL);
    int voicesInSection = 0;

    while (fgets(line, sizeof(line), f) && voiceIndex < VOICEGROUP_SIZE)
    {
        char commentBuf[VG_MAX_VOICE_SAMPLE_NAME];
        capture_line_comment(line, commentBuf);
        ctx->lineComment = commentBuf[0] ? commentBuf : NULL;

        vg_strip_comment(line);
        vg_rtrim(line);
        char* trimmed = vg_ltrim(line);
        if (trimmed[0] == '\0')
            continue;

        if (!inSection)
        {
            /* Skip until the requested label. */
            if (strstr(trimmed, searchLabel) == trimmed)
                inSection = 1;
            continue;
        }

        /* Once we've consumed voices, the next label/.align is the
         * next voicegroup. Stop. */
        if (startLabel && voicesInSection > 0 && vg_voicegroup_line_is_boundary(trimmed))
            break;

        if (consume_voice_line(trimmed, &voiceIndex, &voicesInSection, ctx) != 0)
        {
            fclose(f);
            return -1;
        }
    }

    vg_log("parse_voicegroup_file: done, voiceIndex=%d", voiceIndex);
    fclose(f);
    return 0;
}

/* ---- Public entry ---- */

int vg_parse_voicegroup(const char* projectRoot,
                        const char* voicegroupName,
                        LoadedVoiceGroup* vg,
                        const SymbolMap* dsMap,
                        const SymbolMap* pwMap,
                        const KeySplitMap* ksMap,
                        const ProjectDiscovery* disc)
{
    vg_log("vg_parse_voicegroup: searching for '%s'", voicegroupName);
    VoicegroupSourceLocation loc = vg_find_voicegroup_source(voicegroupName, disc);
    if (!loc.found)
    {
        vg_err("cannot find voicegroup '%s'", voicegroupName);
        return -1;
    }
    vg_log("vg_parse_voicegroup: found at '%s' label='%s'", loc.filePath, loc.label);

    WaveCache waveCache;
    vg_wave_cache_init(&waveCache);

    ParseCtx ctx = {
        .projectRoot = projectRoot,
        .dsMap = dsMap,
        .pwMap = pwMap,
        .ksMap = ksMap,
        .disc = disc,
        .vg = vg,
        .waveCache = &waveCache,
    };

    const char* startLabel = loc.label[0] ? loc.label : NULL;
    return parse_voicegroup_file(loc.filePath, startLabel, &ctx);
}

/* ---- Public helpers declared in voicegroup_loader.h ---- */
/* These stand-alone sample loaders are consumed by the project asset
 * override path. They share the registration helpers above so the
 * LoadedVoiceGroup owns the allocation. */

WaveData* voicegroup_loader_load_sample(const char* projectRoot, const char* relPath, LoadedVoiceGroup* vg)
{
    WaveData* wd = vg_load_sample(projectRoot, relPath);
    if (!wd)
        wd = vg_load_bin_sample(projectRoot, relPath);
    if (wd && !register_wavedata(vg, wd))
    {
        free(wd);
        return NULL;
    }
    return wd;
}

uint32_t* voicegroup_loader_load_prog_wave(const char* projectRoot, const char* relPath, LoadedVoiceGroup* vg)
{
    uint32_t* pw = vg_load_prog_wave(projectRoot, relPath);
    if (pw && !register_progwave(vg, pw))
    {
        free(pw);
        return NULL;
    }
    return pw;
}
