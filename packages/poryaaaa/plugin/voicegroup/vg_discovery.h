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
typedef struct {
    PathList directSoundDataFiles;   /* direct_sound_data.inc */
    PathList progWaveDataFiles;      /* programmable_wave_data.inc */
    PathList keySplitTableFiles;     /* keysplit_tables.inc */
    PathList voicegroupDirs;         /* dirs containing per-voicegroup .inc/.s */
    PathList monolithicVGFiles;      /* .inc files packing many voicegroups with <label>:: */
} ProjectDiscovery;

/*
 * Walk the project tree rooted at projectRoot and populate *out.
 *
 * Discovery order (later entries append to, don't overwrite, earlier ones):
 *   1. Paths named in cfg->{soundData,voicegroup}Paths (if any)
 *   2. Standard sound/direct_sound_data.inc etc.
 *   3. Standard sound/voicegroups/ (plus keysplits/ and drumsets/ subdirs)
 *   4. Recursive scan under sound/ (depth 3) for voicegroup dirs
 *   5. Standard sound/voice_groups.inc (monolithic)
 *
 * Zeros *out before populating. cfg may be NULL for pure auto-discovery.
 */
void vg_discover_project(const char *projectRoot,
                         const VoicegroupLoaderConfig *cfg,
                         ProjectDiscovery *out);

#endif /* VG_DISCOVERY_H */
