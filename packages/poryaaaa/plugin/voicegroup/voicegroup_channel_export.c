#include "voicegroup_loader.h"

#include "vg_discovery.h"
#include "vg_paths.h"
#include "vg_source.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#    include <direct.h>
#    include <windows.h>
#endif

static bool replace_file(const char* tmpPath, const char* finalPath)
{
#ifdef _WIN32
    return MoveFileExA(tmpPath, finalPath, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    return rename(tmpPath, finalPath) == 0;
#endif
}

static void mkdir_one(const char* path)
{
#ifdef _WIN32
    _mkdir(path);
#else
    mkdir(path, 0755);
#endif
}

static void mkdir_parent_dirs(const char* path)
{
    char buf[VG_MAX_PATH_LEN];
    snprintf(buf, sizeof(buf), "%s", path);

    char* lastSep = NULL;
    for (char* scan = buf; *scan; scan++)
    {
        if (*scan == '/' || *scan == '\\')
            lastSep = scan;
    }
    if (!lastSep)
        return;
    *lastSep = '\0';

    char* scan = buf;
#ifdef _WIN32
    if (scan[0] && scan[1] == ':')
        scan += 2;
#endif
    if (*scan == '/' || *scan == '\\')
        scan++;

    for (; *scan; scan++)
    {
        if (*scan == '/' || *scan == '\\')
        {
            char saved = *scan;
            *scan = '\0';
            mkdir_one(buf);
            *scan = saved;
        }
    }
    mkdir_one(buf);
}

bool voicegroup_channel_export_default_path(const char* projectRoot,
                                            const char* voicegroupName,
                                            char* outPath,
                                            size_t outPathSize)
{
    if (!projectRoot || !projectRoot[0] || !voicegroupName || !voicegroupName[0] || !outPath || outPathSize == 0)
    {
        return false;
    }

    int written = snprintf(outPath,
                           outPathSize,
                           "%s%csound%cvoicegroups%c%s_channel.inc",
                           projectRoot,
                           VG_PATH_SEP,
                           VG_PATH_SEP,
                           VG_PATH_SEP,
                           voicegroupName);
    return written > 0 && (size_t)written < outPathSize;
}

bool voicegroup_export_channel_remap(const char* projectRoot,
                                     const char* voicegroupName,
                                     const VoicegroupLoaderConfig* config,
                                     const uint8_t programs[12],
                                     const char* outputPath)
{
    if (!projectRoot || !voicegroupName || !programs || !outputPath || !outputPath[0])
        return false;

    ProjectDiscovery disc;
    vg_discover_project(projectRoot, config, &disc);
    VoicegroupSourceLocation loc = vg_find_voicegroup_source(voicegroupName, &disc);
    if (!loc.found)
        return false;

    char voiceLines[VOICEGROUP_SIZE][VG_MAX_LINE];
    memset(voiceLines, 0, sizeof(voiceLines));
    if (!vg_read_voicegroup_lines(&loc, voiceLines))
        return false;

    char tmpPath[VG_MAX_PATH_LEN + 16];
    int tmpWritten = snprintf(tmpPath, sizeof(tmpPath), "%s.tmp", outputPath);
    if (tmpWritten <= 0 || (size_t)tmpWritten >= sizeof(tmpPath))
        return false;

    mkdir_parent_dirs(outputPath);

    FILE* out = fopen(tmpPath, "w");
    if (!out)
        return false;

    bool writeOk = true;
    if (fprintf(out, "voice_group %s\n", voicegroupName) < 0)
        writeOk = false;

    for (int ch = 0; ch < 12; ch++)
    {
        uint8_t program = programs[ch];
        if (voiceLines[program][0])
        {
            if (fprintf(out, "\t%s\n", vg_ltrim(voiceLines[program])) < 0)
                writeOk = false;
        }
        else
        {
            if (fprintf(out, "\tvoice_square_1 60, 0, 0, 0, 0, 0, 0, 0 @ unused\n") < 0)
                writeOk = false;
        }
    }

    if (ferror(out))
        writeOk = false;
    if (fclose(out) != 0)
        writeOk = false;

    if (!writeOk)
    {
        remove(tmpPath);
        return false;
    }

    if (!replace_file(tmpPath, outputPath))
    {
        remove(tmpPath);
        return false;
    }
    return true;
}
