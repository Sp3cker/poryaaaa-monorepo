#include "vg_discovery.h"
#include "vg_log.h"

#include <dirent.h>
#include <stdio.h>
#include <string.h>

/* ---- Lightweight content heuristics ---- */

/*
 * True if any of the first few .inc/.s files in dirPath mention a voice
 * macro. Quick heuristic — we peek at the first 50 lines of up to 5
 * files, not a full parse.
 */
static int dir_has_voice_macros(const char *dirPath)
{
    DIR *d = opendir(dirPath);
    if (!d) return 0;
    struct dirent *ent;
    int checked = 0;
    while ((ent = readdir(d)) != NULL && checked < 5) {
        if (ent->d_name[0] == '.') continue;
        if (!vg_str_ends_with_ci(ent->d_name, ".inc") && !vg_str_ends_with_ci(ent->d_name, ".s"))
            continue;
        char filePath[VG_MAX_PATH_LEN];
        snprintf(filePath, sizeof(filePath), "%s%c%s", dirPath, VG_PATH_SEP, ent->d_name);
        FILE *f = fopen(filePath, "r");
        if (!f) continue;
        char line[VG_MAX_LINE];
        int lineCount = 0;
        while (fgets(line, sizeof(line), f) && lineCount < 50) {
            if (strstr(line, "voice_directsound") || strstr(line, "voice_square") ||
                strstr(line, "voice_programmable_wave") || strstr(line, "voice_noise") ||
                strstr(line, "voice_keysplit") || strstr(line, "voice_group")) {
                fclose(f);
                closedir(d);
                return 1;
            }
            lineCount++;
        }
        fclose(f);
        checked++;
    }
    closedir(d);
    return 0;
}

/*
 * Heuristic for a "monolithic" voicegroup file — one that packs many
 * voicegroups in a single file with <label>:: delimiters. Used to
 * distinguish that shape from a hub file full of .include directives.
 */
static int is_monolithic_voicegroup_file(const char *filePath)
{
    FILE *f = fopen(filePath, "r");
    if (!f) return 0;

    char line[VG_MAX_LINE];
    int labelCount = 0;
    int voiceMacroCount = 0;
    int includeCount = 0;
    int lineCount = 0;

    while (fgets(line, sizeof(line), f) && lineCount < 500) {
        vg_strip_comment(line);
        vg_rtrim(line);
        char *trimmed = vg_ltrim(line);

        if (strstr(trimmed, "::") && trimmed[0] != '.' && trimmed[0] != '\0')
            labelCount++;
        if (strstr(trimmed, "voice_directsound") || strstr(trimmed, "voice_square") ||
            strstr(trimmed, "voice_programmable_wave") || strstr(trimmed, "voice_noise") ||
            strstr(trimmed, "voice_keysplit") || strstr(trimmed, "voice_group"))
            voiceMacroCount++;
        if (strstr(trimmed, ".include"))
            includeCount++;
        lineCount++;
    }
    fclose(f);

    return labelCount >= 2 && voiceMacroCount > 0 && voiceMacroCount > includeCount;
}

/* ---- Directory tree walk with a visitor callback ---- */

typedef void (*dir_visit_fn)(const char *dirPath, void *ctx);

static void scan_dirs_recursive(const char *basePath, int depth, int maxDepth,
                                dir_visit_fn visit, void *ctx)
{
    visit(basePath, ctx);
    if (depth >= maxDepth) return;

    DIR *d = opendir(basePath);
    if (!d) return;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] == '.') continue;
        char subPath[VG_MAX_PATH_LEN];
        snprintf(subPath, sizeof(subPath), "%s%c%s", basePath, VG_PATH_SEP, ent->d_name);
        if (vg_is_directory(subPath))
            scan_dirs_recursive(subPath, depth + 1, maxDepth, visit, ctx);
    }
    closedir(d);
}

static void visit_voicegroup_dirs(const char *dirPath, void *ctx)
{
    ProjectDiscovery *disc = (ProjectDiscovery *)ctx;
    if (dir_has_voice_macros(dirPath))
        pathlist_add(&disc->voicegroupDirs, dirPath);
}

/* ---- Config override application ---- */

/*
 * A config voicegroup path may be either a directory of per-voicegroup
 * .inc files or a single monolithic .inc file. We accept both.
 */
static void apply_voicegroup_path_override(const char *projectRoot,
                                           const char *relPath,
                                           ProjectDiscovery *out)
{
    char path[VG_MAX_PATH_LEN];
    vg_build_path(path, sizeof(path), projectRoot, relPath);

    if (vg_is_directory(path)) {
        pathlist_add(&out->voicegroupDirs, path);
        /* The dir may also contain monolithic files, so scan its contents. */
        DIR *d = opendir(path);
        if (!d) return;
        struct dirent *ent;
        while ((ent = readdir(d)) != NULL) {
            if (ent->d_name[0] == '.') continue;
            if (!vg_str_ends_with_ci(ent->d_name, ".inc") &&
                !vg_str_ends_with_ci(ent->d_name, ".s"))
                continue;
            char fpath[VG_MAX_PATH_LEN];
            snprintf(fpath, sizeof(fpath), "%s%c%s", path, VG_PATH_SEP, ent->d_name);
            if (is_monolithic_voicegroup_file(fpath))
                pathlist_add(&out->monolithicVGFiles, fpath);
        }
        closedir(d);
    } else if (vg_file_exists(path) && is_monolithic_voicegroup_file(path)) {
        pathlist_add(&out->monolithicVGFiles, path);
    }
}

static void apply_config_overrides(const char *projectRoot,
                                   const VoicegroupLoaderConfig *cfg,
                                   ProjectDiscovery *out)
{
    if (!cfg) return;

    char path[VG_MAX_PATH_LEN];
    for (int i = 0; i < cfg->soundDataPathCount && i < 8; i++) {
        vg_build_path(path, sizeof(path), projectRoot, cfg->soundDataPaths[i]);
        if (vg_file_exists(path))
            pathlist_add(&out->directSoundDataFiles, path);
    }
    for (int i = 0; i < cfg->voicegroupPathCount && i < 8; i++)
        apply_voicegroup_path_override(projectRoot, cfg->voicegroupPaths[i], out);
}

/* ---- Standard-path discovery ---- */

static void add_if_file(const char *projectRoot, const char *relPath, PathList *out)
{
    char path[VG_MAX_PATH_LEN];
    vg_build_path(path, sizeof(path), projectRoot, relPath);
    if (vg_file_exists(path))
        pathlist_add(out, path);
}

static void add_if_dir(const char *projectRoot, const char *relPath, PathList *out)
{
    char path[VG_MAX_PATH_LEN];
    vg_build_path(path, sizeof(path), projectRoot, relPath);
    if (vg_is_directory(path))
        pathlist_add(out, path);
}

static void add_standard_symbol_files(const char *projectRoot, ProjectDiscovery *out)
{
    add_if_file(projectRoot, "sound/direct_sound_data.inc",     &out->directSoundDataFiles);
    add_if_file(projectRoot, "sound/programmable_wave_data.inc", &out->progWaveDataFiles);
    add_if_file(projectRoot, "sound/keysplit_tables.inc",       &out->keySplitTableFiles);
}

static void add_standard_voicegroup_dirs(const char *projectRoot, ProjectDiscovery *out)
{
    add_if_dir(projectRoot, "sound/voicegroups",           &out->voicegroupDirs);
    add_if_dir(projectRoot, "sound/voicegroups/keysplits", &out->voicegroupDirs);
    add_if_dir(projectRoot, "sound/voicegroups/drumsets",  &out->voicegroupDirs);
}

static void check_monolithic_voice_groups_inc(const char *projectRoot, ProjectDiscovery *out)
{
    char path[VG_MAX_PATH_LEN];
    vg_build_path(path, sizeof(path), projectRoot, "sound/voice_groups.inc");
    vg_log("discover_project: checking monolithic '%s' exists=%d", path, vg_file_exists(path));
    if (vg_file_exists(path) && is_monolithic_voicegroup_file(path))
        pathlist_add(&out->monolithicVGFiles, path);
}

/* ---- Entry point ---- */

void vg_discover_project(const char *projectRoot,
                         const VoicegroupLoaderConfig *cfg,
                         ProjectDiscovery *out)
{
    memset(out, 0, sizeof(*out));

    char soundDir[VG_MAX_PATH_LEN];
    vg_build_path(soundDir, sizeof(soundDir), projectRoot, "sound");
    vg_log("discover_project: soundDir='%s' exists=%d", soundDir, vg_is_directory(soundDir));

    apply_config_overrides(projectRoot, cfg, out);
    add_standard_symbol_files(projectRoot, out);
    add_standard_voicegroup_dirs(projectRoot, out);

    vg_log("discover_project: scanning for voicegroup dirs under '%s'", soundDir);
    if (vg_is_directory(soundDir))
        scan_dirs_recursive(soundDir, 0, 3, visit_voicegroup_dirs, out);
    vg_log("discover_project: dir scan done, vgDirs=%d",
           out->voicegroupDirs.count);

    check_monolithic_voice_groups_inc(projectRoot, out);
}
