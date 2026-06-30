#include "vg_source.h"

#include "vg_voice_macro.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

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
    return false;
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
