#include "vg_source.h"

#include "vg_paths.h"
#include "vg_voice_macro.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

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
    char prev = *(tail - 1);
    return prev == '/' || prev == '\\';
}

static int set_if_exists(const char* path, VoicegroupSourceLocation* out)
{
    if (!vg_file_exists(path))
        return 0;
    strncpy(out->filePath, path, VG_MAX_PATH_LEN - 1);
    out->found = 1;
    return 1;
}

static int try_file_in_dir(const char* dir, const char* name, VoicegroupSourceLocation* out)
{
    char path[VG_MAX_PATH_LEN];
    snprintf(path, sizeof(path), "%s%c%s.inc", dir, VG_PATH_SEP, name);
    if (set_if_exists(path, out))
        return 1;
    snprintf(path, sizeof(path), "%s%c%s.s", dir, VG_PATH_SEP, name);
    return set_if_exists(path, out);
}

static int try_file_in_subdir(const char* dir, const char* subdir, const char* name, VoicegroupSourceLocation* out)
{
    char path[VG_MAX_PATH_LEN];
    snprintf(path, sizeof(path), "%s%c%s%c%s.inc", dir, VG_PATH_SEP, subdir, VG_PATH_SEP, name);
    if (set_if_exists(path, out))
        return 1;
    snprintf(path, sizeof(path), "%s%c%s%c%s.s", dir, VG_PATH_SEP, subdir, VG_PATH_SEP, name);
    return set_if_exists(path, out);
}

static int try_suffix_convention(const char* vgName,
                                 const char* suffix,
                                 const char* subdir,
                                 const ProjectDiscovery* disc,
                                 VoicegroupSourceLocation* out)
{
    const char* found = strstr(vgName, suffix);
    if (!found)
        return 0;

    char baseName[VG_MAX_SYMBOL_LEN];
    int baseLen = (int)(found - vgName);
    if (baseLen <= 0 || baseLen >= VG_MAX_SYMBOL_LEN)
        return 0;
    memcpy(baseName, vgName, baseLen);
    baseName[baseLen] = '\0';

    for (int i = 0; i < disc->voicegroupDirs.count; i++)
        if (try_file_in_subdir(disc->voicegroupDirs.paths[i], subdir, baseName, out))
            return 1;

    for (int i = 0; i < disc->voicegroupDirs.count; i++)
        if (dir_last_component_is(disc->voicegroupDirs.paths[i], subdir) &&
            try_file_in_dir(disc->voicegroupDirs.paths[i], baseName, out))
            return 1;
    return 0;
}

static int find_in_combined_files(const char* vgName, const ProjectDiscovery* disc, VoicegroupSourceLocation* out)
{
    char searchLabel[VG_MAX_SYMBOL_LEN + 4];
    snprintf(searchLabel, sizeof(searchLabel), "%s::", vgName);

    for (int i = 0; i < disc->combinedVGFiles.count; i++)
    {
        FILE* f = fopen(disc->combinedVGFiles.paths[i], "r");
        if (!f)
            continue;
        char line[VG_MAX_LINE];
        while (fgets(line, sizeof(line), f))
        {
            vg_strip_comment(line);
            char* trimmed = vg_ltrim(line);
            if (strstr(trimmed, searchLabel) != trimmed)
                continue;
            strncpy(out->filePath, disc->combinedVGFiles.paths[i], VG_MAX_PATH_LEN - 1);
            strncpy(out->label, vgName, VG_MAX_SYMBOL_LEN - 1);
            out->found = 1;
            fclose(f);
            return 1;
        }
        fclose(f);
    }
    return 0;
}

VoicegroupSourceLocation vg_find_voicegroup_source(const char* vgName, const ProjectDiscovery* disc)
{
    VoicegroupSourceLocation loc;
    memset(&loc, 0, sizeof(loc));

    for (int i = 0; i < disc->voicegroupDirs.count; i++)
        if (try_file_in_dir(disc->voicegroupDirs.paths[i], vgName, &loc))
            return loc;
    if (try_suffix_convention(vgName, "_keysplit", "keysplits", disc, &loc))
        return loc;
    if (try_suffix_convention(vgName, "_drumset", "drumsets", disc, &loc))
        return loc;

    char vgPrefixed[VG_MAX_SYMBOL_LEN];
    snprintf(vgPrefixed, sizeof(vgPrefixed), "vg_%s", vgName);
    for (int i = 0; i < disc->voicegroupDirs.count; i++)
        if (try_file_in_dir(disc->voicegroupDirs.paths[i], vgPrefixed, &loc))
            return loc;

    find_in_combined_files(vgName, disc, &loc);
    return loc;
}

bool vg_voicegroup_line_is_boundary(const char* trimmed)
{
    if (strncmp(trimmed, ".align", 6) == 0)
        return true;
    const char* cc = strstr(trimmed, "::");
    return cc && cc > trimmed && !isspace((unsigned char)trimmed[0]);
}

int vg_voicegroup_start_note(const char* trimmed)
{
    if (strncmp(trimmed, "voice_group ", 12) != 0)
        return -1;
    char ignored[VG_MAX_SYMBOL_LEN];
    int startingNote = 0;
    if (sscanf(trimmed + 12, "%[^,\n], %d", ignored, &startingNote) < 2)
        return -1;
    return startingNote > 0 && startingNote < VOICEGROUP_SIZE ? startingNote : -1;
}

static bool is_voice_macro_line(const char* trimmed)
{
    for (const VoicegroupMacro* macro = vg_voice_macros; macro->keyword; macro++)
    {
        size_t len = strlen(macro->keyword);
        if (strncmp(trimmed, macro->keyword, len) == 0 &&
            (trimmed[len] == '\0' || trimmed[len] == ' ' || trimmed[len] == '\t'))
            return true;
    }
    return (strncmp(trimmed, "voice_", 6) == 0 && strncmp(trimmed, "voice_group", 11) != 0) ||
           strncmp(trimmed, "cry", 3) == 0;
}

static void strip_line_ending(char* text)
{
    size_t len = strlen(text);
    while (len > 0 && (text[len - 1] == '\n' || text[len - 1] == '\r'))
        text[--len] = '\0';
}

bool vg_read_voicegroup_lines(const VoicegroupSourceLocation* loc, char voiceLines[VOICEGROUP_SIZE][VG_MAX_LINE])
{
    FILE* f = fopen(loc->filePath, "r");
    if (!f)
        return false;

    char searchLabel[VG_MAX_SYMBOL_LEN + 4];
    if (loc->label[0])
        snprintf(searchLabel, sizeof(searchLabel), "%s::", loc->label);

    char line[VG_MAX_LINE];
    int voiceIndex = 0;
    int inSection = (loc->label[0] == '\0');
    int voicesInSection = 0;

    while (fgets(line, sizeof(line), f) && voiceIndex < VOICEGROUP_SIZE)
    {
        char original[VG_MAX_LINE];
        char work[VG_MAX_LINE];
        snprintf(original, sizeof(original), "%s", line);
        strip_line_ending(original);
        snprintf(work, sizeof(work), "%s", line);
        vg_strip_comment(work);
        vg_rtrim(work);
        char* trimmed = vg_ltrim(work);
        if (trimmed[0] == '\0')
            continue;
        if (!inSection)
        {
            if (strstr(trimmed, searchLabel) == trimmed)
                inSection = 1;
            continue;
        }
        if (loc->label[0] && voicesInSection > 0 && vg_voicegroup_line_is_boundary(trimmed))
            break;
        int startNote = vg_voicegroup_start_note(trimmed);
        if (startNote >= 0)
        {
            voiceIndex = startNote;
            continue;
        }
        if (!is_voice_macro_line(trimmed))
            continue;
        snprintf(voiceLines[voiceIndex], VG_MAX_LINE, "%s", original);
        voiceIndex++;
        voicesInSection++;
    }

    fclose(f);
    return true;
}
