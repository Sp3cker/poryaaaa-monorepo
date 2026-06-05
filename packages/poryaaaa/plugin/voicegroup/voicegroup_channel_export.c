#include "voicegroup_loader.h"

#include "vg_discovery.h"
#include "vg_paths.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#endif

typedef struct {
    char filePath[VG_MAX_PATH_LEN];
    char label[VG_MAX_SYMBOL_LEN];
    int found;
} ChannelExportLocation;

static int set_if_exists(const char *path, ChannelExportLocation *out)
{
    if (!vg_file_exists(path)) return 0;
    strncpy(out->filePath, path, VG_MAX_PATH_LEN - 1);
    out->filePath[VG_MAX_PATH_LEN - 1] = '\0';
    out->found = 1;
    return 1;
}

static int try_file_in_dir(const char *dir, const char *name, ChannelExportLocation *out)
{
    char path[VG_MAX_PATH_LEN];
    snprintf(path, sizeof(path), "%s%c%s.inc", dir, VG_PATH_SEP, name);
    if (set_if_exists(path, out)) return 1;
    snprintf(path, sizeof(path), "%s%c%s.s", dir, VG_PATH_SEP, name);
    return set_if_exists(path, out);
}

static int dir_last_component_is(const char *dir, const char *name)
{
    size_t dirLen = strlen(dir);
    size_t nameLen = strlen(name);
    if (nameLen > dirLen) return 0;
    const char *tail = dir + dirLen - nameLen;
    if (strcmp(tail, name) != 0) return 0;
    if (tail == dir) return 1;
    return tail[-1] == '/' || tail[-1] == '\\';
}

static int try_file_in_subdir(const char *dir, const char *subdir, const char *name,
                              ChannelExportLocation *out)
{
    char path[VG_MAX_PATH_LEN];
    snprintf(path, sizeof(path), "%s%c%s%c%s.inc", dir, VG_PATH_SEP, subdir, VG_PATH_SEP, name);
    if (set_if_exists(path, out)) return 1;
    snprintf(path, sizeof(path), "%s%c%s%c%s.s", dir, VG_PATH_SEP, subdir, VG_PATH_SEP, name);
    return set_if_exists(path, out);
}

static int try_suffix_convention(const char *voicegroupName, const char *suffix,
                                 const char *subdir,
                                 const ProjectDiscovery *disc,
                                 ChannelExportLocation *out)
{
    const char *found = strstr(voicegroupName, suffix);
    if (!found) return 0;

    int baseLen = (int)(found - voicegroupName);
    if (baseLen <= 0 || baseLen >= VG_MAX_SYMBOL_LEN) return 0;

    char baseName[VG_MAX_SYMBOL_LEN];
    memcpy(baseName, voicegroupName, (size_t)baseLen);
    baseName[baseLen] = '\0';

    for (int i = 0; i < disc->voicegroupDirs.count; i++)
        if (try_file_in_subdir(disc->voicegroupDirs.paths[i], subdir, baseName, out))
            return 1;

    for (int i = 0; i < disc->voicegroupDirs.count; i++) {
        if (!dir_last_component_is(disc->voicegroupDirs.paths[i], subdir)) continue;
        if (try_file_in_dir(disc->voicegroupDirs.paths[i], baseName, out)) return 1;
    }
    return 0;
}

static int find_in_monolithic_files(const char *voicegroupName,
                                    const ProjectDiscovery *disc,
                                    ChannelExportLocation *out)
{
    char searchLabel[VG_MAX_SYMBOL_LEN + 4];
    snprintf(searchLabel, sizeof(searchLabel), "%s::", voicegroupName);

    for (int i = 0; i < disc->monolithicVGFiles.count; i++) {
        FILE *f = fopen(disc->monolithicVGFiles.paths[i], "r");
        if (!f) continue;

        char line[VG_MAX_LINE];
        while (fgets(line, sizeof(line), f)) {
            vg_strip_comment(line);
            char *trimmed = vg_ltrim(line);
            if (strstr(trimmed, searchLabel) == trimmed) {
                strncpy(out->filePath, disc->monolithicVGFiles.paths[i], VG_MAX_PATH_LEN - 1);
                out->filePath[VG_MAX_PATH_LEN - 1] = '\0';
                strncpy(out->label, voicegroupName, VG_MAX_SYMBOL_LEN - 1);
                out->label[VG_MAX_SYMBOL_LEN - 1] = '\0';
                out->found = 1;
                fclose(f);
                return 1;
            }
        }
        fclose(f);
    }
    return 0;
}

static ChannelExportLocation find_voicegroup_source(const char *projectRoot,
                                                    const char *voicegroupName,
                                                    const VoicegroupLoaderConfig *config)
{
    ProjectDiscovery disc;
    ChannelExportLocation loc;
    memset(&loc, 0, sizeof(loc));

    vg_discover_project(projectRoot, config, &disc);

    for (int i = 0; i < disc.voicegroupDirs.count; i++)
        if (try_file_in_dir(disc.voicegroupDirs.paths[i], voicegroupName, &loc))
            return loc;

    if (try_suffix_convention(voicegroupName, "_keysplit", "keysplits", &disc, &loc)) return loc;
    if (try_suffix_convention(voicegroupName, "_drumset", "drumsets", &disc, &loc)) return loc;

    {
        char prefixed[VG_MAX_SYMBOL_LEN];
        snprintf(prefixed, sizeof(prefixed), "vg_%s", voicegroupName);
        for (int i = 0; i < disc.voicegroupDirs.count; i++)
            if (try_file_in_dir(disc.voicegroupDirs.paths[i], prefixed, &loc))
                return loc;
    }

    find_in_monolithic_files(voicegroupName, &disc, &loc);
    return loc;
}

static int is_monolithic_boundary(const char *trimmed)
{
    if (strstr(trimmed, ".align") == trimmed) return 1;
    if (strstr(trimmed, "::") && trimmed[0] != '.') return 1;
    return 0;
}

static int is_voice_macro_line(const char *trimmed)
{
    static const char *const macros[] = {
        "voice_directsound_no_resample",
        "voice_directsound_alt",
        "voice_directsound",
        "voice_square_1_alt",
        "voice_square_1",
        "voice_square_2_alt",
        "voice_square_2",
        "voice_programmable_wave_alt",
        "voice_programmable_wave",
        "voice_noise_alt",
        "voice_noise",
        "voice_keysplit_all",
        "voice_keysplit",
        "cry_reverse",
        "cry",
        NULL,
    };

    for (int i = 0; macros[i]; i++) {
        size_t len = strlen(macros[i]);
        if (strncmp(trimmed, macros[i], len) == 0 &&
            (trimmed[len] == '\0' || trimmed[len] == ' ' || trimmed[len] == '\t')) {
            return 1;
        }
    }
    return 0;
}

static void strip_line_ending(char *text)
{
    size_t len = strlen(text);
    while (len > 0 && (text[len - 1] == '\n' || text[len - 1] == '\r'))
        text[--len] = '\0';
}

static bool read_voice_lines(const ChannelExportLocation *loc,
                             char voiceLines[VOICEGROUP_SIZE][VG_MAX_LINE])
{
    FILE *f = fopen(loc->filePath, "r");
    if (!f) return false;

    char searchLabel[VG_MAX_SYMBOL_LEN + 4];
    if (loc->label[0])
        snprintf(searchLabel, sizeof(searchLabel), "%s::", loc->label);

    int voiceIndex = 0;
    int inSection = (loc->label[0] == '\0');
    int voicesInSection = 0;

    char line[VG_MAX_LINE];
    while (fgets(line, sizeof(line), f) && voiceIndex < VOICEGROUP_SIZE) {
        char original[VG_MAX_LINE];
        char work[VG_MAX_LINE];
        strncpy(original, line, sizeof(original) - 1);
        original[sizeof(original) - 1] = '\0';
        strip_line_ending(original);

        strncpy(work, line, sizeof(work) - 1);
        work[sizeof(work) - 1] = '\0';
        vg_strip_comment(work);
        vg_rtrim(work);
        char *trimmed = vg_ltrim(work);
        if (trimmed[0] == '\0') continue;

        if (!inSection) {
            if (strstr(trimmed, searchLabel) == trimmed)
                inSection = 1;
            continue;
        }

        if (loc->label[0] && voicesInSection > 0 && is_monolithic_boundary(trimmed))
            break;

        if (strncmp(trimmed, "voice_group ", 12) == 0) {
            char ignored[VG_MAX_SYMBOL_LEN];
            int startingNote = 0;
            if (sscanf(trimmed + 12, "%[^,\n], %d", ignored, &startingNote) >= 2 &&
                startingNote > 0 && startingNote < VOICEGROUP_SIZE) {
                voiceIndex = startingNote;
            }
            continue;
        }

        if (is_voice_macro_line(trimmed)) {
            strncpy(voiceLines[voiceIndex], original, VG_MAX_LINE - 1);
            voiceLines[voiceIndex][VG_MAX_LINE - 1] = '\0';
            voiceIndex++;
            voicesInSection++;
        }
    }

    fclose(f);
    return true;
}

static bool replace_file(const char *tmpPath, const char *finalPath)
{
#ifdef _WIN32
    return MoveFileExA(tmpPath, finalPath,
                       MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    return rename(tmpPath, finalPath) == 0;
#endif
}

static void mkdir_one(const char *path)
{
#ifdef _WIN32
    _mkdir(path);
#else
    mkdir(path, 0755);
#endif
}

static void mkdir_parent_dirs(const char *path)
{
    char buf[VG_MAX_PATH_LEN];
    snprintf(buf, sizeof(buf), "%s", path);

    char *lastSep = NULL;
    for (char *scan = buf; *scan; scan++) {
        if (*scan == '/' || *scan == '\\')
            lastSep = scan;
    }
    if (!lastSep)
        return;
    *lastSep = '\0';

    char *scan = buf;
#ifdef _WIN32
    if (scan[0] && scan[1] == ':') scan += 2;
#endif
    if (*scan == '/' || *scan == '\\') scan++;

    for (; *scan; scan++) {
        if (*scan == '/' || *scan == '\\') {
            char saved = *scan;
            *scan = '\0';
            mkdir_one(buf);
            *scan = saved;
        }
    }
    mkdir_one(buf);
}

bool voicegroup_channel_export_default_path(const char *projectRoot,
                                            const char *voicegroupName,
                                            char *outPath,
                                            size_t outPathSize)
{
    if (!projectRoot || !projectRoot[0] || !voicegroupName || !voicegroupName[0] ||
        !outPath || outPathSize == 0) {
        return false;
    }

    int written = snprintf(outPath, outPathSize, "%s%csound%cvoicegroups%c%s_channel.inc",
                           projectRoot, VG_PATH_SEP, VG_PATH_SEP, VG_PATH_SEP, voicegroupName);
    return written > 0 && (size_t)written < outPathSize;
}

bool voicegroup_export_channel_remap(const char *projectRoot,
                                     const char *voicegroupName,
                                     const VoicegroupLoaderConfig *config,
                                     const uint8_t programs[12],
                                     const char *outputPath)
{
    if (!projectRoot || !voicegroupName || !programs || !outputPath || !outputPath[0])
        return false;

    ChannelExportLocation loc = find_voicegroup_source(projectRoot, voicegroupName, config);
    if (!loc.found)
        return false;

    char voiceLines[VOICEGROUP_SIZE][VG_MAX_LINE];
    memset(voiceLines, 0, sizeof(voiceLines));
    if (!read_voice_lines(&loc, voiceLines))
        return false;

    char tmpPath[VG_MAX_PATH_LEN + 16];
    int tmpWritten = snprintf(tmpPath, sizeof(tmpPath), "%s.tmp", outputPath);
    if (tmpWritten <= 0 || (size_t)tmpWritten >= sizeof(tmpPath))
        return false;

    mkdir_parent_dirs(outputPath);

    FILE *out = fopen(tmpPath, "w");
    if (!out)
        return false;

    bool writeOk = true;
    if (fprintf(out, "voice_group %s\n", voicegroupName) < 0)
        writeOk = false;

    for (int ch = 0; ch < 12; ch++) {
        uint8_t program = programs[ch];
        if (voiceLines[program][0]) {
            if (fprintf(out, "\t%s\n", vg_ltrim(voiceLines[program])) < 0)
                writeOk = false;
        } else {
            if (fprintf(out, "\tvoice_square_1 60, 0, 0, 0, 0, 0, 0, 0 @ unused\n") < 0)
                writeOk = false;
        }
    }

    if (ferror(out))
        writeOk = false;
    if (fclose(out) != 0)
        writeOk = false;

    if (!writeOk) {
        remove(tmpPath);
        return false;
    }

    if (!replace_file(tmpPath, outputPath)) {
        remove(tmpPath);
        return false;
    }
    return true;
}
