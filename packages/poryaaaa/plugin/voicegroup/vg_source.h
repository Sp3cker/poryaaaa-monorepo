#ifndef VG_SOURCE_H
#define VG_SOURCE_H

#include <stdbool.h>

#include "vg_discovery.h"
#include "voicegroup_loader.h"

typedef struct
{
    char filePath[VG_MAX_PATH_LEN];
    char label[VG_MAX_SYMBOL_LEN];
    int found;
} VoicegroupSourceLocation;

VoicegroupSourceLocation vg_find_voicegroup_source(const char* voicegroupName, const ProjectDiscovery* disc);

bool vg_read_voicegroup_lines(const VoicegroupSourceLocation* loc, char voiceLines[VOICEGROUP_SIZE][VG_MAX_LINE]);

int vg_voicegroup_start_note(const char* trimmed);

bool vg_voicegroup_line_is_boundary(const char* trimmed);

#endif /* VG_SOURCE_H */
