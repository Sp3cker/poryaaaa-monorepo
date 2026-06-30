#ifndef VG_DISCOVERY_H
#define VG_DISCOVERY_H

#include "voicegroup_loader.h"
#include "vg_paths.h"

/*
 * Outcome of discovering the layout of a pokeemerald/pokefirered-style
 * project on disk. Every field is a (bounded, deduplicated) list of
 * absolute paths. Empty lists are legal — fork projects often only
 * populate a subset.
 */
typedef struct
{
    PathList directSoundDataFiles; /* direct_sound_data.inc */
    PathList progWaveDataFiles;    /* programmable_wave_data.inc */
    PathList keySplitTableFiles;   /* keysplit_tables.inc */
    PathList combinedVGFiles;      /* sound/voice_groups.inc */
} ProjectDiscovery;

/*
 * Populate standard project source files. Zeros *out before populating.
 */
void vg_discover_project(const char* projectRoot, ProjectDiscovery* out);

#endif /* VG_DISCOVERY_H */
