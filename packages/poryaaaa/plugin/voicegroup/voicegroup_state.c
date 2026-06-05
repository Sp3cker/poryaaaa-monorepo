#include "voicegroup_state.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#endif

static bool resolve_state_dir(char *out, size_t outSize)
{
#ifdef _WIN32
    const char *appdata = getenv("APPDATA");
    if (appdata && *appdata) {
        snprintf(out, outSize, "%s\\poryaaaa", appdata);
        return true;
    }
    const char *home = getenv("HOME");
    if (!home || !*home) return false;
    snprintf(out, outSize, "%s\\AppData\\Roaming\\poryaaaa", home);
    return true;
#else
    const char *home = getenv("HOME");
    if (!home || !*home) return false;
#ifdef __APPLE__
    snprintf(out, outSize, "%s/Library/Application Support/poryaaaa", home);
#else
    const char *xdg = getenv("XDG_CONFIG_HOME");
    if (xdg && *xdg)
        snprintf(out, outSize, "%s/poryaaaa", xdg);
    else
        snprintf(out, outSize, "%s/.config/poryaaaa", home);
#endif
    return true;
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

/*
 * mkdir -p semantics. Walks `path` and creates each segment in turn so
 * leaf-only mkdir doesn't fail when an intermediate (e.g. $HOME/.config)
 * is missing on a fresh user account.
 */
static void mkdir_p(const char *path)
{
    char buf[700];
    snprintf(buf, sizeof(buf), "%s", path);

    char *p = buf;
#ifdef _WIN32
    /* Skip drive letter prefix like "C:" so we don't try to mkdir it. */
    if (p[0] && p[1] == ':') p += 2;
#endif
    /* Skip leading separator so the first segment isn't an empty string. */
    if (*p == '/' || *p == '\\') p++;

    for (; *p; p++) {
        if (*p == '/' || *p == '\\') {
            char saved = *p;
            *p = '\0';
            mkdir_one(buf);
            *p = saved;
        }
    }
    mkdir_one(buf);
}

/*
 * Atomic replace. On POSIX rename() already replaces. On Windows, plain C
 * rename() fails when the target exists, so route through MoveFileEx with
 * MOVEFILE_REPLACE_EXISTING. Returns true on success.
 */
static bool atomic_replace(const char *tmpPath, const char *finalPath)
{
#ifdef _WIN32
    if (MoveFileExA(tmpPath, finalPath,
                    MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        return true;
    }
    return false;
#else
    return rename(tmpPath, finalPath) == 0;
#endif
}

static void write_escaped(FILE *f, const char *s)
{
    for (const char *p = s; *p; p++) {
        if (*p == '"' || *p == '\\') fputc('\\', f);
        fputc(*p, f);
    }
}

bool voicegroup_state_write_default(const char *projectRoot,
                                    const char *voicegroupName,
                                    const LoadedVoiceGroup *vg)
{
    if (!projectRoot || !voicegroupName || !vg) return false;

    char stateDir[600];
    if (!resolve_state_dir(stateDir, sizeof(stateDir))) return false;
    mkdir_p(stateDir);

    char tmpPath[700];
    char finalPath[700];
    snprintf(tmpPath,   sizeof(tmpPath),   "%s/state.json.tmp", stateDir);
    snprintf(finalPath, sizeof(finalPath), "%s/state.json",     stateDir);

    FILE *f = fopen(tmpPath, "w");
    if (!f) return false;

    fprintf(f, "{\n");
    fprintf(f, "  \"projectRoot\": \"");
    write_escaped(f, projectRoot);
    fprintf(f, "\",\n");
    fprintf(f, "  \"voicegroup\": \"");
    write_escaped(f, voicegroupName);
    fprintf(f, "\",\n");
    fprintf(f, "  \"slots\": [\n");

    int first = 1;
    for (int i = 0; i < VOICEGROUP_SIZE; i++) {
        const char *name = vg->voiceSampleNames[i];
        if (name[0] == '\0') continue;
        if (!first) fprintf(f, ",\n");
        first = 0;
        fprintf(f, "    {\"program\": %d, \"name\": \"", i);
        write_escaped(f, name);
        fprintf(f, "\"}");
    }
    fprintf(f, "\n  ]\n}\n");
    fclose(f);

    if (!atomic_replace(tmpPath, finalPath)) {
        remove(tmpPath);
        return false;
    }
    return true;
}
