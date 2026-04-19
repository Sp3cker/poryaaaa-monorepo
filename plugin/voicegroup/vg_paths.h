#ifndef VG_PATHS_H
#define VG_PATHS_H

#include <stddef.h>

#include "voicegroup_loader.h"  /* for VG_MAX_PATH_LEN */

#ifdef _WIN32
#define VG_PATH_SEP '\\'
#else
#define VG_PATH_SEP '/'
#endif

#define VG_MAX_DISCOVERED_PATHS 32
#define VG_MAX_LINE 1024

/*
 * Bounded, deduplicated list of paths used by project discovery.
 * Silently drops adds past VG_MAX_DISCOVERED_PATHS.
 */
typedef struct {
    char paths[VG_MAX_DISCOVERED_PATHS][VG_MAX_PATH_LEN];
    int count;
} PathList;

void pathlist_add(PathList *list, const char *path);

/* String utilities (mutate in place). */
char *vg_ltrim(char *s);
void  vg_rtrim(char *s);
void  vg_strip_comment(char *s);            /* strips '@...' and '//...' tails */
int   vg_str_ends_with_ci(const char *s, const char *suffix);

/* Path utilities. */
void vg_build_path(char *dest, size_t destSize, const char *base, const char *relative);
int  vg_file_exists(const char *path);
int  vg_is_directory(const char *path);

#endif /* VG_PATHS_H */
