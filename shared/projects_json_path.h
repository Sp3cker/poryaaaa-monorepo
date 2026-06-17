#ifndef PORYAAAA_PROJECTS_JSON_PATH_H
#define PORYAAAA_PROJECTS_JSON_PATH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

static inline bool poryaaaa_projects_json_default_dir(char* out, size_t outSize)
{
    if (!out || outSize == 0)
        return false;
    int written = 0;
#ifdef _WIN32
    const char* appdata = getenv("APPDATA");
    if (appdata && *appdata)
        written = snprintf(out, outSize, "%s\\poryaaaa", appdata);
    else
    {
        const char* home = getenv("USERPROFILE");
        if (!home || !*home)
            home = getenv("HOME");
        if (!home || !*home)
            return false;
        written = snprintf(out, outSize, "%s\\AppData\\Roaming\\poryaaaa", home);
    }
#else
    const char* home = getenv("HOME");
    if (!home || !*home)
        return false;
#    ifdef __APPLE__
    written = snprintf(out, outSize, "%s/Library/Application Support/poryaaaa", home);
#    else
    const char* xdg = getenv("XDG_CONFIG_HOME");
    if (xdg && *xdg)
        written = snprintf(out, outSize, "%s/poryaaaa", xdg);
    else
        written = snprintf(out, outSize, "%s/.config/poryaaaa", home);
#    endif
#endif
    return written > 0 && (size_t)written < outSize;
}

static inline bool poryaaaa_projects_json_default_path(char* out, size_t outSize)
{
    char dir[600];
    if (!poryaaaa_projects_json_default_dir(dir, sizeof(dir)))
        return false;
#ifdef _WIN32
    int written = snprintf(out, outSize, "%s\\projects.json", dir);
#else
    int written = snprintf(out, outSize, "%s/projects.json", dir);
#endif
    return written > 0 && (size_t)written < outSize;
}

#endif /* PORYAAAA_PROJECTS_JSON_PATH_H */
