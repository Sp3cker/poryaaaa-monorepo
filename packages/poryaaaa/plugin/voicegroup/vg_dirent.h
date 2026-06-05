#ifndef VG_DIRENT_H
#define VG_DIRENT_H

#include "voicegroup_loader.h"

#ifdef _WIN32
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

struct dirent {
    char d_name[VG_MAX_PATH_LEN];
};

typedef struct {
    HANDLE handle;
    WIN32_FIND_DATAA data;
    struct dirent ent;
    int first;
} DIR;

static DIR *opendir(const char *path)
{
    char searchPath[VG_MAX_PATH_LEN];
    snprintf(searchPath, sizeof(searchPath), "%s\\*", path);

    DIR *dir = (DIR *)calloc(1, sizeof(DIR));
    if (!dir) return NULL;

    dir->handle = FindFirstFileA(searchPath, &dir->data);
    if (dir->handle == INVALID_HANDLE_VALUE) {
        free(dir);
        return NULL;
    }

    dir->first = 1;
    return dir;
}

static struct dirent *readdir(DIR *dir)
{
    if (!dir) return NULL;
    if (dir->first) {
        dir->first = 0;
    } else if (!FindNextFileA(dir->handle, &dir->data)) {
        return NULL;
    }

    strncpy(dir->ent.d_name, dir->data.cFileName, sizeof(dir->ent.d_name) - 1);
    dir->ent.d_name[sizeof(dir->ent.d_name) - 1] = '\0';
    return &dir->ent;
}

static int closedir(DIR *dir)
{
    if (!dir) return -1;
    FindClose(dir->handle);
    free(dir);
    return 0;
}
#else
#include <dirent.h>
#endif

#endif /* VG_DIRENT_H */
