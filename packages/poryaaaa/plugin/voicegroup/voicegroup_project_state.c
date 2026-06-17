#include "voicegroup_project_state.h"

#include "vg_alloc.h"
#include "vg_discovery.h"
#include "vg_paths.h"
#include "vg_source.h"
#include "vg_symbols.h"
#include "vg_voice_macro.h"

#include "voicegroup_types.h"
#include "projects_json_path.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#    include <direct.h>
#    include <windows.h>
#endif

typedef struct
{
    const SymbolMap* dsMap;
    const SymbolMap* pwMap;
    const ProjectDiscovery* disc;
    VoicegroupProjectState* state;
    bool allocationFailed;
} ProjectStateRead;

static bool read_state_by_name(const char* voicegroupName, ProjectStateRead* read, VoicegroupProjectState* out);
static bool parse_keysplit_slot(
    VoicegroupProjectStateSlot* slot, uint8_t typeCode, const char* args, const char* comment, ProjectStateRead* read);
static bool parse_voice_line(const char* rawLine, VoicegroupProjectStateSlot* slot, ProjectStateRead* read);

static void set_slot(VoicegroupProjectStateSlot* slot, uint8_t typeCode, const char* name)
{
    slot->defined = true;
    slot->typeCode = typeCode;
    snprintf(slot->name, sizeof(slot->name), "%s", name);
}

static const char* basename_from_map(const SymbolMap* map, const char* symbol)
{
    const char* path = vg_symbol_map_find(map, symbol);
    return path ? vg_path_basename(path) : symbol;
}

static const char* cgb_name(uint8_t typeCode)
{
    if (typeCode == VOICE_SQUARE_1 || typeCode == VOICE_SQUARE_1_ALT)
        return typeCode == VOICE_SQUARE_1_ALT ? "Square 1 (alt)" : "Square 1";
    if (typeCode == VOICE_SQUARE_2 || typeCode == VOICE_SQUARE_2_ALT)
        return typeCode == VOICE_SQUARE_2_ALT ? "Square 2 (alt)" : "Square 2";
    if (typeCode == VOICE_NOISE || typeCode == VOICE_NOISE_ALT)
        return typeCode == VOICE_NOISE_ALT ? "Noise (alt)" : "Noise";
    return typeCode == VOICE_PROGRAMMABLE_WAVE_ALT ? "ProgWave (alt)" : "ProgWave";
}

static int arg_count(const char* args)
{
    int count = 0;
    const char* p = args;
    while (*p)
    {
        while (*p && (isspace((unsigned char)*p) || *p == ','))
            p++;
        if (!*p)
            break;
        count++;
        p += strcspn(p, ",");
    }
    return count;
}

static bool validate_macro_args(const VoicegroupMacro* macro, const char* args, ProjectStateRead* read)
{
    const int count = arg_count(args);
    bool ok = count >= macro->minArgs && count <= macro->maxArgs;
    if (!ok && read->state->diagnosticCount < VOICEGROUP_PROJECT_STATE_MAX_DIAGNOSTICS)
        snprintf(read->state->diagnostics[read->state->diagnosticCount++],
                 VOICEGROUP_PROJECT_STATE_DIAGNOSTIC_LEN,
                 "%s expects valid arguments, got %d field(s)",
                 macro->keyword,
                 count);
    return ok;
}

static void line_comment(const char* line, char out[VG_MAX_VOICE_SAMPLE_NAME])
{
    out[0] = '\0';
    const char* at = strchr(line, '@');
    if (!at)
        return;
    at++;
    while (*at && isspace((unsigned char)*at))
        at++;
    snprintf(out, VG_MAX_VOICE_SAMPLE_NAME, "%s", at);
    vg_rtrim(out);
}

static void parse_symbol_arg(const char* args, int prefixInts, char out[VG_MAX_SYMBOL_LEN])
{
    out[0] = '\0';
    const char* p = args;
    for (int i = 0; i < prefixInts; i++)
    {
        p = strchr(p, ',');
        if (!p)
            return;
        p++;
    }
    while (*p && isspace((unsigned char)*p))
        p++;
    size_t len = strcspn(p, ", \t\r\n");
    if (len >= VG_MAX_SYMBOL_LEN)
        len = VG_MAX_SYMBOL_LEN - 1;
    memcpy(out, p, len);
    out[len] = '\0';
}

static bool collect_drumset_pads(const char* vgSymbol, VoicegroupProjectStateSlot* slot, ProjectStateRead* read)
{
    const char* name = strncmp(vgSymbol, "voicegroup_", 11) == 0 ? vgSymbol + 11 : vgSymbol;
    VoicegroupProjectState nested;
    memset(&nested, 0, sizeof(nested));
    if (!read_state_by_name(name, read, &nested))
        return true;

    int count = 0;
    for (int note = 0; note < VOICEGROUP_SIZE; note++)
        if (nested.slots[note].defined && nested.slots[note].name[0])
            count++;
    if (count > 0)
        slot->drumset = vg_malloc_array((size_t)count, sizeof(*slot->drumset));
    if (count > 0 && !slot->drumset)
        read->allocationFailed = true;
    for (int note = 0, out = 0; slot->drumset && note < VOICEGROUP_SIZE; note++)
    {
        if (!nested.slots[note].defined || !nested.slots[note].name[0])
            continue;
        slot->drumset[out].note = (uint8_t)note;
        snprintf(slot->drumset[out].name, sizeof(slot->drumset[out].name), "%s", nested.slots[note].name);
        out++;
    }
    slot->drumsetCount = slot->drumset ? count : 0;
    voicegroup_project_state_free(&nested);
    return !read->allocationFailed;
}

static void set_sample_slot(
    VoicegroupProjectStateSlot* slot, uint8_t typeCode, const char* args, const SymbolMap* map, int prefixInts)
{
    char symbol[VG_MAX_SYMBOL_LEN];
    parse_symbol_arg(args, prefixInts, symbol);
    if (symbol[0])
        set_slot(slot, typeCode, basename_from_map(map, symbol));
}

static void
set_prog_wave_slot(VoicegroupProjectStateSlot* slot, uint8_t typeCode, const char* args, const SymbolMap* map)
{
    char symbol[VG_MAX_SYMBOL_LEN];
    parse_symbol_arg(args, 2, symbol);
    const char* path = symbol[0] ? vg_symbol_map_find(map, symbol) : NULL;
    set_slot(slot, typeCode, path ? vg_path_basename(path) : cgb_name(typeCode));
}

static bool parse_voice_line(const char* rawLine, VoicegroupProjectStateSlot* slot, ProjectStateRead* read)
{
    char comment[VG_MAX_VOICE_SAMPLE_NAME];
    char line[VG_MAX_LINE];
    line_comment(rawLine, comment);
    snprintf(line, sizeof(line), "%s", rawLine);
    vg_strip_comment(line);
    vg_rtrim(line);
    char* trimmed = vg_ltrim(line);

    const VoicegroupMacro* macro = NULL;
    const char* args = NULL;
    if (!vg_voice_macro_match(trimmed, &macro, &args))
    {
        if ((strncmp(trimmed, "voice_", 6) == 0 || strncmp(trimmed, "cry", 3) == 0) &&
            read->state->diagnosticCount < VOICEGROUP_PROJECT_STATE_MAX_DIAGNOSTICS)
            snprintf(read->state->diagnostics[read->state->diagnosticCount++],
                     VOICEGROUP_PROJECT_STATE_DIAGNOSTIC_LEN,
                     "unsupported voicegroup syntax: %s",
                     trimmed);
        return true;
    }
    uint8_t typeCode = macro->typeCode;
    if (!validate_macro_args(macro, args, read))
        return true;
    if (typeCode == VOICE_DIRECTSOUND || typeCode == VOICE_DIRECTSOUND_ALT || typeCode == VOICE_DIRECTSOUND_NO_RESAMPLE)
        set_sample_slot(slot, typeCode, args, read->dsMap, 2);
    else if (typeCode == VOICE_PROGRAMMABLE_WAVE || typeCode == VOICE_PROGRAMMABLE_WAVE_ALT)
        set_prog_wave_slot(slot, typeCode, args, read->pwMap);
    else if (typeCode == VOICE_CRY || typeCode == VOICE_CRY_REVERSE)
        set_sample_slot(slot, typeCode, args, read->dsMap, 0);
    else if (typeCode == VOICE_KEYSPLIT || typeCode == VOICE_KEYSPLIT_ALL)
        return parse_keysplit_slot(slot, typeCode, args, comment, read);
    else
        set_slot(slot, typeCode, cgb_name(typeCode));
    return true;
}

static bool parse_keysplit_slot(
    VoicegroupProjectStateSlot* slot, uint8_t typeCode, const char* args, const char* comment, ProjectStateRead* read)
{
    char vgSymbol[VG_MAX_SYMBOL_LEN];
    parse_symbol_arg(args, typeCode == VOICE_KEYSPLIT ? 1 : 0, vgSymbol);
    if (!vgSymbol[0])
        return true;
    set_slot(slot, typeCode, comment[0] ? comment : vgSymbol);
    return typeCode == VOICE_KEYSPLIT_ALL ? collect_drumset_pads(vgSymbol, slot, read) : true;
}

static bool read_state_by_name(const char* voicegroupName, ProjectStateRead* read, VoicegroupProjectState* out)
{
    VoicegroupSourceLocation loc = vg_find_voicegroup_source(voicegroupName, read->disc);
    if (!loc.found)
        return false;

    char lines[VOICEGROUP_SIZE][VG_MAX_LINE];
    memset(lines, 0, sizeof(lines));
    if (!vg_read_voicegroup_lines(&loc, lines))
        return false;

    for (int i = 0; i < VOICEGROUP_SIZE; i++)
        if (lines[i][0] && !parse_voice_line(lines[i], &out->slots[i], read))
            return false;

    return !read->allocationFailed && read->state->diagnosticCount == 0;
}

void voicegroup_project_state_free(VoicegroupProjectState* state)
{
    if (!state)
        return;
    for (int i = 0; i < VOICEGROUP_SIZE; i++)
        free(state->slots[i].drumset);
    memset(state, 0, sizeof(*state));
}

static void free_slots_only(VoicegroupProjectState* state)
{
    for (int i = 0; i < VOICEGROUP_SIZE; i++)
        free(state->slots[i].drumset);
    memset(state->slots, 0, sizeof(state->slots));
}

bool voicegroup_project_state_collect(const char* projectRoot,
                                      const char* voicegroupName,
                                      const VoicegroupLoaderConfig* config,
                                      VoicegroupProjectState* out)
{
    if (!projectRoot || !voicegroupName || !out)
        return false;
    memset(out, 0, sizeof(*out));

    ProjectDiscovery* disc = calloc(1, sizeof(*disc));
    if (!disc)
        return false;
    vg_discover_project(projectRoot, config, disc);

    SymbolMap dsMap, pwMap;
    vg_symbol_map_init(&dsMap);
    vg_symbol_map_init(&pwMap);
    bool ok = vg_parse_direct_sound_data(disc, &dsMap) && vg_parse_prog_wave_data(disc, &pwMap);
    ProjectStateRead read = {.dsMap = &dsMap, .pwMap = &pwMap, .disc = disc, .state = out};
    if (ok)
        ok = read_state_by_name(voicegroupName, &read, out);

    vg_symbol_map_free(&dsMap);
    vg_symbol_map_free(&pwMap);
    free(disc);
    if (!ok)
        free_slots_only(out);
    return ok;
}

bool voicegroup_project_state_default_path(char* out, size_t outSize)
{
    return poryaaaa_projects_json_default_path(out, outSize);
}

static void mkdir_one(const char* path)
{
#ifdef _WIN32
    _mkdir(path);
#else
    mkdir(path, 0755);
#endif
}

static void mkdir_p(const char* path)
{
    char buf[700];
    snprintf(buf, sizeof(buf), "%s", path);
    char* p = buf;
#ifdef _WIN32
    if (p[0] && p[1] == ':')
        p += 2;
#endif
    if (*p == '/' || *p == '\\')
        p++;
    for (; *p; p++)
    {
        if (*p != '/' && *p != '\\')
            continue;
        char saved = *p;
        *p = '\0';
        mkdir_one(buf);
        *p = saved;
    }
    mkdir_one(buf);
}

static void write_escaped(FILE* f, const char* s)
{
    for (const char* p = s; *p; p++)
    {
        if (*p == '"' || *p == '\\')
            fputc('\\', f);
        fputc(*p, f);
    }
}

static void write_drumset(FILE* f, const VoicegroupProjectStateSlot* slot)
{
    fprintf(f, ", \"drumset\": [");
    for (int i = 0; i < slot->drumsetCount; i++)
    {
        if (i)
            fprintf(f, ", ");
        fprintf(f, "{\"note\": %u, \"name\": \"", (unsigned)slot->drumset[i].note);
        write_escaped(f, slot->drumset[i].name);
        fprintf(f, "\"}");
    }
    fprintf(f, "]");
}

static bool finish_file(FILE* f, const char* tmpPath, const char* finalPath)
{
    bool ok = f && ferror(f) == 0 && fclose(f) == 0;
#ifdef _WIN32
    ok = ok && MoveFileExA(tmpPath, finalPath, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    ok = ok && rename(tmpPath, finalPath) == 0;
#endif
    if (!ok)
        remove(tmpPath);
    return ok;
}

static void write_slots(FILE* f, const VoicegroupProjectState* state)
{
    int first = 1;
    for (int i = 0; i < VOICEGROUP_SIZE; i++)
    {
        const VoicegroupProjectStateSlot* slot = &state->slots[i];
        if (!slot->defined || !slot->name[0])
            continue;
        fprintf(f, "%s    {\"program\": %d, \"name\": \"", first ? "" : ",\n", i);
        write_escaped(f, slot->name);
        fprintf(f, "\", \"typeCode\": %u", (unsigned)slot->typeCode);
        if ((slot->typeCode & VOICE_KEYSPLIT_ALL) && slot->drumsetCount > 0)
            write_drumset(f, slot);
        fprintf(f, "}");
        first = 0;
    }
}

bool voicegroup_project_state_write_default(const char* projectRoot,
                                            const char* voicegroupName,
                                            const VoicegroupProjectState* state)
{
    if (!projectRoot || !voicegroupName || !state)
        return false;

    char stateDir[600];
    if (!poryaaaa_projects_json_default_dir(stateDir, sizeof(stateDir)))
        return false;
    mkdir_p(stateDir);

    char tmpPath[700];
    char finalPath[700];
    snprintf(tmpPath, sizeof(tmpPath), "%s/projects.json.tmp", stateDir);
    if (!voicegroup_project_state_default_path(finalPath, sizeof(finalPath)))
        return false;

    FILE* f = fopen(tmpPath, "w");
    if (!f)
        return false;
    fprintf(f, "{\n  \"root\": \"");
    write_escaped(f, projectRoot);
    fprintf(f, "\",\n  \"bank\": \"");
    write_escaped(f, voicegroupName);
    fprintf(f, "\",\n  \"slots\": [\n");
    write_slots(f, state);
    fprintf(f, "\n  ]\n}\n");
    return finish_file(f, tmpPath, finalPath);
}
