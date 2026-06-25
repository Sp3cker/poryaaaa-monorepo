#include "vg_discovery.h"
#include "vg_log.h"

#include <string.h>

/* ---- Standard-path discovery ---- */

static void add_if_file(const char* projectRoot, const char* relPath, PathList* out)
{
    char path[VG_MAX_PATH_LEN];
    vg_build_path(path, sizeof(path), projectRoot, relPath);
    if (vg_file_exists(path))
        pathlist_add(out, path);
}

static void add_standard_symbol_files(const char* projectRoot, ProjectDiscovery* out)
{
    add_if_file(projectRoot, "sound/direct_sound_data.inc", &out->directSoundDataFiles);
    add_if_file(projectRoot, "sound/programmable_wave_data.inc", &out->progWaveDataFiles);
    add_if_file(projectRoot, "sound/keysplit_tables.inc", &out->keySplitTableFiles);
}

static void check_combined_voice_groups_inc(const char* projectRoot, ProjectDiscovery* out)
{
    char path[VG_MAX_PATH_LEN];
    vg_build_path(path, sizeof(path), projectRoot, "sound/voice_groups.inc");
    vg_log("discover_project: checking combined '%s' exists=%d", path, vg_file_exists(path));
    if (vg_file_exists(path))
        pathlist_add(&out->combinedVGFiles, path);
}

/* ---- Entry point ---- */

void vg_discover_project(const char* projectRoot, ProjectDiscovery* out)
{
    memset(out, 0, sizeof(*out));
    add_standard_symbol_files(projectRoot, out);
    check_combined_voice_groups_inc(projectRoot, out);
}
